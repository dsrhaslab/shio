/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#include "cheferd/controller/control_application/supervisor_control_application.hpp"

#include "cheferd/utils/rules_file_parser.hpp"

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
}

#include <algorithm> // for std::min

namespace cheferd {

// SupervisorControlApplication parameterized constructor.
SupervisorControlApplication::SupervisorControlApplication (std::vector<std::string>* rules_ptr,
    const uint64_t& cycle_sleep_time,
    std::unordered_map<std::string, uint64_t> op_system_limits,
    std::unordered_map<int, std::unordered_map<std::string, uint64_t>> policies,
    CoreControlApplication* core_control_application) :
    ControlApplication { rules_ptr, cycle_sleep_time },
    op_maximum_limit { op_system_limits },
    priorities { policies },
    controller_sessions_ {},
    controller_sessions_failure_counter {},
    m_core_control_application_ptr { core_control_application },
    controller_rates {},
    controller_rates_mutex {}
{
    for (const auto& pair : op_maximum_limit) {
        active_ops.insert (pair.first);
    }

    Logging::log_info ("SupervisorControlApplication parameterized constructor.");
}

// SupervisorControlApplication default destructor.
SupervisorControlApplication::~SupervisorControlApplication ()
{
    Logging::log_info ("SupervisorControlApplication: exiting ...\n");
}

////////////////////////////////////////////
///////// Register new sessions ////////////
////////////////////////////////////////////

// register_controller_session call. Register a new lower controller.
void SupervisorControlApplication::register_controller_session (
    const std::string& local_controller_address,
    std::shared_ptr<Session> controller_session)
{
    Logging::log_debug ("RegisterControllerSession -- ");
    controller_sessions_[local_controller_address] = controller_session;
    controller_sessions_failure_counter[local_controller_address] = 0;
}

////////////////////////////////////////////
/////////////// Feedback Loop //////////////
////////////////////////////////////////////

// operator call. Used to initiate the control application feedback loop execution.
void SupervisorControlApplication::operator() ()
{
    this->execute_feedback_loop ();
}

// Benchmark counters (latencies in microseconds).
static uint64_t build_enforce_time_aggregated = 0;
static int rounds_counter = 0;
static uint64_t collect_time_aggregated = 0;
static uint64_t compute_time_aggregated = 0;
static uint64_t enforce_time_aggregated = 0;
// Filled with default value
static auto start = std::chrono::system_clock::now ();
static auto end = std::chrono::system_clock::now ();

// log_stats call. Log control application stats.
void SupervisorControlApplication::log_stats ()
{
    Logging::log_info ("SupervisorControlApplication::Number of rounds: "
        + std::to_string (rounds_counter) + " rounds");
    Logging::log_info ("SupervisorControlApplication::Total Latency: "
        + std::to_string (
            std::chrono::duration_cast<std::chrono::microseconds> (end - start).count ())
        + " [µs]");
    Logging::log_info ("SupervisorControlApplication::Collect Latency: "
        + std::to_string (collect_time_aggregated) + " [µs]");
    Logging::log_info ("SupervisorControlApplication::Compute Latency: "
        + std::to_string (compute_time_aggregated) + " [µs]");
    Logging::log_info ("SupervisorControlApplication::Build Enforce Latency: "
        + std::to_string (build_enforce_time_aggregated) + " [µs]");
    Logging::log_info ("SupervisorControlApplication::Network Enforce Latency: "
        + std::to_string (core_network_enforce_time) + " [µs]");
    Logging::log_info ("SupervisorControlApplication::Enforce Latency: "
        + std::to_string (enforce_time_aggregated) + " [µs]");

    Logging::log_info ("SupervisorControlApplication::Exiting.");
}

// stop_feedback_loop call. Stops the feedback loop from executing.
void SupervisorControlApplication::stop_feedback_loop ()
{
    working_application_ = false;
    end = std::chrono::system_clock::now ();
}

// execute_feedback_loop call. Executes feedback loop.
void SupervisorControlApplication::execute_feedback_loop ()
{
    Logging::log_debug ("SupervisorControlApplication::ExecuteFeedbackLoop");
    PStatus status = PStatus::Error ();
    working_application_ = true;

    // While not primary do nothing
    while (working_application_.load () && !is_primary_.load ()) {
        std::this_thread::sleep_for (std::chrono::milliseconds (1000));
    }

    // ADD here waiting for all controllers to be connected

    start = std::chrono::system_clock::now ();

    Logging::log_info ("SupervisorControlApplication: Execute feedback loop started.");

    while (working_application_) {

        // No need to lock here either
        if (controller_rates.empty ()) {
            rounds_counter = 0;
            build_enforce_time_aggregated = 0;
            collect_time_aggregated = 0;
            compute_time_aggregated = 0;
            enforce_time_aggregated = 0;
            start = std::chrono::system_clock::now ();
        }

        // Logging::log_info ("Round: "+ std::to_string(rounds_scalability));
        usage_per_priority_and_controller.clear ();
        nr_jobs_per_priority_and_controller.clear ();
        controller_rates.clear ();

        auto start_collect = std::chrono::system_clock::now ();
        this->collect_statistics_global ();
        auto end_collect = std::chrono::system_clock::now ();
        auto collect_time
            = std::chrono::duration_cast<std::chrono::microseconds> (end_collect - start_collect)
                  .count ();
        collect_time_aggregated += collect_time;

        // Logging::log_info("Number of rounds: " + std::to_string(rounds_scalability) + "
        // rounds");

        auto start_compute = std::chrono::system_clock::now ();
        compute_controllers_limits ();
        auto end_compute = std::chrono::system_clock::now ();
        auto compute_time
            = std::chrono::duration_cast<std::chrono::microseconds> (end_compute - start_compute)
                  .count ();
        compute_time_aggregated += compute_time;

        auto start_enforce = std::chrono::system_clock::now ();
        this->enforce_controller_rules ();
        auto end_enforce = std::chrono::system_clock::now ();
        auto enforce_time
            = std::chrono::duration_cast<std::chrono::microseconds> (end_enforce - start_enforce)
                  .count ();
        enforce_time_aggregated += enforce_time;

        rounds_counter++;

        auto round_duration
            = std::chrono::duration_cast<std::chrono::microseconds> (end_enforce - start_collect)
                  .count ();

        Logging::log_info ("SupervisorControlApplication::Round: " + std::to_string (rounds_counter)
            + " took " + std::to_string (round_duration) + ";" + std::to_string (collect_time) + ";"
            + std::to_string (compute_time) + ";" + std::to_string (enforce_time) + " [µs]");

        // Make supervisor cycles be around 5 seconds
        // Old version: the unsigned subtraction wraps around when the round takes longer than
        // the cycle, resulting in a (practically) endless sleep.
        // std::this_thread::sleep_for (microseconds (m_feedback_loop_sleep_time -
        // round_duration));
        auto remaining_time = static_cast<int64_t> (m_feedback_loop_sleep_time) - round_duration;
        if (remaining_time > 0) {
            std::this_thread::sleep_for (microseconds (remaining_time));
        }
    }

    // log message and end control loop
    Logging::log_info ("SupervisorControlApplication::Exiting.");
    //_exit (EXIT_SUCCESS);

    stop_feedback_loop ();
}

////////////////////////////////////////////
//////////// Collect Statistics ////////////
////////////////////////////////////////////

// collect_statistics_global call. Sends the request and collects statistics from controllers.
void SupervisorControlApplication::collect_statistics_global ()
{
    // Logging::log_info ("SupervisorControlApplication: collect_statistics_global");

    // create COLLECT_DATA_METADATA_STATS request
    std::string rule = std::to_string (COLLECT_DETAILED_STATS) + "|"
        + std::to_string (COLLECT_GLOBAL_CONTROLLER_STATS) + "|";

    // submit requests to each controller session's submission_queue
    for (auto const& [controller_address, controller_session] : controller_sessions_) {
        // put request on the session's submission_queue
        controller_session->SubmitRule (rule);
    }

    std::list<std::string> sessions_to_delete;

    // collect responses from each controller session's completion_queue
    for (auto const& [controller_address, controller_session] : controller_sessions_) {
        collect_statistics_result_shared (controller_session,
            controller_address,
            sessions_to_delete);
    }

    // collect stats from local control application
    collect_statistics_local_control_application (sessions_to_delete);

    // Log nr_jobs_per_priority_and_controller
    for (const auto& [priority, controller_jobs] : nr_jobs_per_priority_and_controller) {
        for (const auto& [controller, nrjobs] : controller_jobs) {
            Logging::log_info (
                "SupervisorControlApplication: Priority: " + std::to_string (priority)
                + " Controller: " + controller + " Jobs: " + std::to_string (nrjobs));
        }
    }

    // Log usage_per_priority_and_controller
    for (const auto& [operation, priority_controller_usage] : usage_per_priority_and_controller) {
        for (const auto& [priority, controller_usage] : priority_controller_usage) {
            for (const auto& [controller, usage] : controller_usage) {
                Logging::log_info ("SupervisorControlApplication: Operation: " + operation
                    + " Priority: " + std::to_string (priority) + " Controller: " + controller
                    + " Usage: " + std::to_string (usage));
            }
        }
    }

    // Stop if a session is not available.
    // TO-DO: In a real case scenario, we should not stop the feedback loop. But instead just
    // eliminate the session.
    /*if (!sessions_to_delete.empty ()) {
        stop_feedback_loop ();
    }*/

    for (const auto& local_controller_address : sessions_to_delete) {
        controller_sessions_.erase (local_controller_address);
    }
}

////////////////////////////////////////////
//////////////// Sleep /////////////////////
////////////////////////////////////////////

// sleep call. Used to make control application main thread wait for the next loop.
void SupervisorControlApplication::sleep ()
{
    std::this_thread::sleep_for (microseconds (this->m_feedback_loop_sleep_time));
}

////////////////////////////////////////////
////// Compute and Enforce new rules ///////
////////////////////////////////////////////

// compute_controllers_limits call. Computes the limits of each controller for all operations.
void SupervisorControlApplication::compute_controllers_limits ()
{
    // Add +1 for core controller from supervisor controller.

    const int ops_size = static_cast<int> (active_ops.size ());

    std::vector<std::thread> threads;
    threads.reserve (ops_size);

    for (const auto& operation : active_ops) {
        threads.emplace_back (&SupervisorControlApplication::compute_controller_limits_per_op,
            this,
            operation);
    }

    for (auto& thread : threads) {
        thread.join ();
    }
}

// compute_controller_limits_per_op call. Computes the limit of each controller for an operation.
void SupervisorControlApplication::compute_controller_limits_per_op (const std::string& operation)
{
    // Logging::log_info ("ControlApplication: Computing PSFA Rules for operation: " + operation);

    // Assign all bandwidth to leftover_iops
    // Old version: operator[] may insert into op_maximum_limit, which is shared by the
    // per-operation threads (data race).
    // uint64_t left_iops_per_operation = op_maximum_limit[operation];
    uint64_t left_iops_per_operation = 0;
    auto op_limit = op_maximum_limit.find (operation);
    if (op_limit != op_maximum_limit.end ()) {
        left_iops_per_operation = op_limit->second;
    }

    std::unordered_map<std::string, uint64_t> controller_usage;
    uint64_t system_total_rate = 0;

    // compute_equal_controllers (operation, left_iops_per_operation);

    // compute_uniform_per_op (operation, left_iops_per_operation);

    // With psfa+
    compute_psfa_per_op_phase1 (operation,
        controller_usage,
        system_total_rate,
        left_iops_per_operation);
    compute_psfa_per_op_phase2 (operation,
        controller_usage,
        system_total_rate,
        left_iops_per_operation);

    // TO-DO: Tune if necessary. Like to avoid unnecessary enforce operations.
}

// compute_equal_controllers call. Assigns to all controllers the same equal rate.
void SupervisorControlApplication::compute_equal_controllers (const std::string& operation,
    uint64_t& left_iops_per_operation)
{
    // account for itself
    int total_controllers = controller_sessions_.size () + 1;

    std::lock_guard<std::mutex> lock (controller_rates_mutex);

    auto fair_share = left_iops_per_operation / total_controllers;

    for (auto const& [controller_address, _] : controller_sessions_) {
        controller_rates[controller_address][operation] = fair_share;
    }

    // Account for itself
    controller_rates["0.0.0.0"][operation] = fair_share;
}

// compute_uniform_per_op call. Assigns rates according to the number of jobs of each controller.
void SupervisorControlApplication::compute_uniform_per_op (const std::string& operation,
    uint64_t& left_iops_per_operation)
{
    // Extract total number of jobs
    int total_jobs = 0;
    std::unordered_map<std::string, int> controller_nrjobs;

    for (const auto& [priority, controller_jobs] : nr_jobs_per_priority_and_controller) {
        for (const auto& [controller_address, nrjobs] : controller_jobs) {
            total_jobs += nrjobs;
            controller_nrjobs[controller_address] += nrjobs;
        }
    }

    std::lock_guard<std::mutex> lock (controller_rates_mutex);

    // Initialize variables
    if (total_jobs > 0) {
        auto fair_share = left_iops_per_operation / total_jobs;
        for (auto const& [controller_address, _] : controller_sessions_) {
            uint64_t rate = 104857600;
            if (controller_nrjobs.find (controller_address) != controller_nrjobs.end ()) {
                rate = fair_share * controller_nrjobs[controller_address];
            }
            controller_rates[controller_address][operation] = rate;
        }
        // Account for itself
        if (controller_nrjobs.find ("0.0.0.0") != controller_nrjobs.end ()) {
            controller_rates["0.0.0.0"][operation] = fair_share * controller_nrjobs["0.0.0.0"];
        } else {
            controller_rates["0.0.0.0"][operation] = 104857600;
        }

    } else {
        for (auto const& [controller_address, _] : controller_sessions_) {
            controller_rates[controller_address][operation] = 104857600;
        }
        controller_rates["0.0.0.0"][operation] = 104857600;
    }
}

// compute_psfa_per_op_phase1 call. Computes phase 1 of PSFA's algorithm for an operation.
void SupervisorControlApplication::compute_psfa_per_op_phase1 (const std::string operation,
    std::unordered_map<std::string, uint64_t>& controller_usage,
    uint64_t& system_total_rate,
    uint64_t& left_iops_per_operation)
{
    // Extract total number of jobs
    int total_jobs = 0;
    for (const auto& [priority, controller_jobs] : nr_jobs_per_priority_and_controller) {
        for (const auto& [controller_address, nrjobs] : controller_jobs) {
            total_jobs += nrjobs;
        }
    }

    // Initialize variables
    if (total_jobs > 0) {
        for (const auto& [priority, controller_jobs] : nr_jobs_per_priority_and_controller) {
            for (const auto& [controller_address, nrjobs] : controller_jobs) {
                if (nrjobs > 0) {
                    auto fair_share = left_iops_per_operation / total_jobs;

                    // Old version: operator[] may insert into priorities and
                    // usage_per_priority_and_controller, which are shared by the per-operation
                    // threads (data race).
                    // uint64_t controller_total_demand_per_priority
                    //     = nrjobs * priorities[priority][operation];
                    // uint64_t controller_total_usage_per_priority
                    //     = usage_per_priority_and_controller[operation][priority]
                    //                                        [controller_address];
                    uint64_t priority_demand = 0;
                    auto priority_limits = priorities.find (priority);
                    if (priority_limits != priorities.end ()) {
                        auto op_demand = priority_limits->second.find (operation);
                        if (op_demand != priority_limits->second.end ()) {
                            priority_demand = op_demand->second;
                        }
                    }
                    uint64_t controller_total_demand_per_priority = nrjobs * priority_demand;

                    uint64_t controller_total_usage_per_priority = 0;
                    auto op_usage = usage_per_priority_and_controller.find (operation);
                    if (op_usage != usage_per_priority_and_controller.end ()) {
                        auto priority_usage = op_usage->second.find (priority);
                        if (priority_usage != op_usage->second.end ()) {
                            auto usage = priority_usage->second.find (controller_address);
                            if (usage != priority_usage->second.end ()) {
                                controller_total_usage_per_priority = usage->second;
                            }
                        }
                    }

                    // If usage is zero, we assume a default rate of 1
                    if (controller_total_usage_per_priority == 0) {
                        controller_total_usage_per_priority = 1;
                    }

                    uint64_t controller_rate;

                    uint64_t controller_fair_share = fair_share * nrjobs;

                    auto almost_demand_limit = static_cast<uint64_t> (
                        round (controller_total_demand_per_priority * 0.95));
                    if (controller_total_usage_per_priority <= almost_demand_limit) {
                        auto demand_rate_difference = controller_total_demand_per_priority
                            - controller_total_usage_per_priority;
                        uint64_t threshold_value
                            = static_cast<uint64_t> (round (demand_rate_difference * 0.05));

                        std::lock_guard<std::mutex> lock (controller_rates_mutex);
                        auto rate_value
                            = std::min (controller_total_usage_per_priority + threshold_value,
                                controller_fair_share);
                        // Assign at least 100mb to controller
                        uint64_t min_per_controller = 104857600;
                        controller_rate = std::max (rate_value, min_per_controller);
                        controller_rates[controller_address][operation] += controller_rate;

                    } else {
                        std::lock_guard<std::mutex> lock (controller_rates_mutex);
                        auto rate_value = std::min (controller_total_demand_per_priority,
                            controller_fair_share);
                        uint64_t min_per_controller = 104857600;
                        controller_rate = std::max (rate_value, min_per_controller);
                        controller_rates[controller_address][operation] += controller_rate;
                    }

                    controller_usage[controller_address] += controller_total_usage_per_priority;
                    system_total_rate += controller_total_usage_per_priority;
                    total_jobs -= nrjobs;
                    std::lock_guard<std::mutex> lock (controller_rates_mutex);
                    // Old version: controller_rate can exceed the remaining rate (because of the
                    // 100MiB minimum), making the unsigned subtraction wrap around.
                    // left_iops_per_operation -= controller_rate;
                    if (controller_rate >= left_iops_per_operation) {
                        left_iops_per_operation = 0;
                    } else {
                        left_iops_per_operation -= controller_rate;
                    }
                }
            }
        }
    } else {
        // Old version: the "0.0.0.0" entry was written outside of the lock.
        // for (auto const& [controller_address, _] : controller_sessions_) {
        //     std::lock_guard<std::mutex> lock (controller_rates_mutex);
        //     controller_rates[controller_address][operation] = 104857600;
        // }
        // controller_rates["0.0.0.0"][operation] = 104857600;
        std::lock_guard<std::mutex> lock (controller_rates_mutex);
        for (auto const& [controller_address, _] : controller_sessions_) {
            controller_rates[controller_address][operation] = 104857600;
        }
        controller_rates["0.0.0.0"][operation] = 104857600;
    }
}

// compute_psfa_per_op_phase2 call. Computes phase 2 of PSFA's algorithm for an operation.
void SupervisorControlApplication::compute_psfa_per_op_phase2 (const std::string operation,
    std::unordered_map<std::string, uint64_t>& controller_usage,
    uint64_t system_total_rate,
    uint64_t left_iops_per_operation)
{
    /*Phase 2: Distribute remaining rate*/
    double left_iops_copy = static_cast<double> (left_iops_per_operation);

    if (left_iops_copy > 0) {
        for (const auto& [controller_address, usage] : controller_usage) {
            double add_iops_perc
                = static_cast<double> (usage) / static_cast<double> (system_total_rate);
            uint64_t extra_rate
                = static_cast<uint64_t> (std::floor (add_iops_perc * left_iops_copy));
            std::lock_guard<std::mutex> lock (controller_rates_mutex);
            controller_rates[controller_address][operation] += extra_rate;
            left_iops_per_operation -= extra_rate;
        }
    }
}

// enforce_controller_rules call. Sends the computed limits to each controller.
void SupervisorControlApplication::enforce_controller_rules ()
{
    // Logging::log_info ("SupervisorControlApplication: Enforcing PSFA Controller Rules");

    auto start_enforce = std::chrono::system_clock::now ();

    std::unordered_map<std::string, bool> address_updated;

    std::unordered_map<std::string, std::string> rules_per_local;

    // No need to lock here, because it is only one thread, but lets do it anyway
    std::lock_guard<std::mutex> lock (controller_rates_mutex);
    for (auto const& [controller_address, rates] : controller_rates) {
        rules_per_local[controller_address] = std::to_string (CREATE_CONTROLLER_ENF_RULE);
        for (auto const& [operation, rate] : rates) {
            rules_per_local[controller_address] += "|" + operation + ":" + std::to_string (rate);
        }
        // TODO: Add further testing if enforcement is needed.
        address_updated[controller_address] = true;
    }

    auto end_enforce = std::chrono::system_clock::now ();
    build_enforce_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (end_enforce - start_enforce)
               .count ();

    // Send to own core control application
    if (controller_rates.find ("0.0.0.0") != controller_rates.end ()) {
        Logging::log_info ("SupervisorControlApplication: Enforcing rule "
            + rules_per_local["0.0.0.0"] + " to " + "0.0.0.0");
        m_core_control_application_ptr->EnforceSystemLimit ((controller_rates["0.0.0.0"]));
    }

    // Send to controller in lower layers.
    for (auto const& [controller_address, controller_session] : controller_sessions_) {
        if (address_updated[controller_address]) {
            rules_per_local[controller_address] += "|.";
            Logging::log_info ("SupervisorControlApplication: Enforcing rule "
                + rules_per_local[controller_address] + " to " + controller_address);
            controller_session->SubmitRule (rules_per_local[controller_address]);
        }
    }

    for (auto const& [controller_address, controller_session] : controller_sessions_) {
        if (address_updated[controller_address]) {
            std::unique_ptr<StageResponse> ack_ptr = controller_session->GetResult ();

            // TODO: Check if result is ok
            if (ack_ptr != nullptr) {
                auto* response_ptr = dynamic_cast<StageResponseACK*> (ack_ptr.get ());

                if (response_ptr->ACKValue () == 1) {
                    // reply->set_m_message(1);
                } else {
                    // reply->set_m_message(1);
                }
            }
        }
    }
}

////////////////////////////////////////////
//////////// Auxiliary Functions ///////////
////////////////////////////////////////////

// initialize call. Starts thread to run control application.
void SupervisorControlApplication::initialize ()
{
    std::thread control_application_thread_t = std::thread (std::ref ((*this)));
    control_application_thread_t.detach ();
}

// Number of consecutive failed statistics collections after which a controller session is removed.
int controller_connect_attempt_tries = 10;

// collect_statistics_result_shared call. Collects a local controller statistics.
void SupervisorControlApplication::collect_statistics_result_shared (
    const std::shared_ptr<Session>& controller_session,
    const std::string& controller_address,
    std::list<std::string>& sessions_to_delete)
{
    auto stats_ptr = controller_session->GetResult ();

    if (!stats_ptr) {
        Logging::log_info ("SupervisorControlApplication: Found empty statistics. Try: "
            + std::to_string (controller_sessions_failure_counter[controller_address]) + "\n");
        controller_sessions_failure_counter[controller_address]++;
        if (controller_sessions_failure_counter[controller_address]
            >= controller_connect_attempt_tries) {
            sessions_to_delete.push_back (controller_address);
        }
        return;
    }

    auto* response_ptr = dynamic_cast<StageResponseStatController*> (stats_ptr.get ());

    if (response_ptr) {
        auto controller_stats = response_ptr->controller_stats;

        for (const auto& [operation, stats_rated] : controller_stats.per_operation_usage) {
            for (const auto& [priority, usage] : stats_rated.usage_per_priority) {
                usage_per_priority_and_controller[operation][priority][controller_address] = usage;
            }
        }
        for (const auto& [priority, counter] : controller_stats.counter_per_priority) {
            nr_jobs_per_priority_and_controller[priority][controller_address] = counter;
        }
    }

    if (!response_ptr) {
        Logging::log_info ("SupervisorControlApplication: Found empty statistics. Try: "
            + std::to_string (controller_sessions_failure_counter[controller_address]) + "\n");
        controller_sessions_failure_counter[controller_address]++;
        if (controller_sessions_failure_counter[controller_address]
            >= controller_connect_attempt_tries) {
            sessions_to_delete.push_back (controller_address);
        }
        return;
    }

    controller_sessions_failure_counter[controller_address] = 0;
}

// collect_statistics_local_control_application call. Collects statistics from the in-local core
// control application.
void SupervisorControlApplication::collect_statistics_local_control_application (
    std::list<std::string>& sessions_to_delete)
{
    auto response_ptr = m_core_control_application_ptr->CollectCoreControllerStatistics ();

    auto controller_stats = response_ptr.controller_stats;

    for (const auto& [operation, stats_rated] : controller_stats.per_operation_usage) {
        for (const auto& [priority, usage] : stats_rated.usage_per_priority) {
            usage_per_priority_and_controller[operation][priority]["0.0.0.0"] = usage;
        }
    }
    for (const auto& [priority, counter] : controller_stats.counter_per_priority) {
        nr_jobs_per_priority_and_controller[priority]["0.0.0.0"] = counter;
    }
}

} // namespace cheferd
