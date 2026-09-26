/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#include "cheferd/controller/control_application/core_control_application.hpp"

#include "cheferd/utils/rules_file_parser.hpp"

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace cheferd {

// CoreControlApplication parameterized constructor.
CoreControlApplication::CoreControlApplication (const uint64_t& cycle_sleep_time,
    std::unordered_map<int, std::unordered_map<std::string, uint64_t>> policies,
    int total_jobs,
    std::unordered_map<std::string, int> jobs_name_to_priority) :
    ControlApplication { cycle_sleep_time },
    expected_total_jobs { total_jobs },
    jobs_name_to_priority { jobs_name_to_priority },
    priorities { policies },
    priority_counter_mutex {},
    priority_counter {},
    usage_per_priority_mutex {},
    usage_per_priority {},
    controller_sessions_ {},
    pending_controller_sessions_ {},
    controller_sessions_failure_counter {},
    active_ops {},
    ops_maximum_limit_mutex {},
    ops_maximum_limit {},
    op_job_demands_order {},
    job_rates {},
    job_rates_mutex {},
    job_previous_rates {},
    change_in_job {},
    controller_address_to_stages {},
    job_location_tracker {},
    stage_info_detailed {},
    local_to_data_queue_ {},
    pending_stage_lock_ {},
    m_active_data_plane_sessions { 0 },
    m_pending_data_plane_sessions { 0 },
    change_in_system { false },
    usage_per_operation_and_stage {}
{ }

// CoreControlApplication default destructor.
CoreControlApplication::~CoreControlApplication ()
{
    Logging::log_info ("CoreControlApplication: exiting ...\n");
}

////////////////////////////////////////////
///////// Register new sessions ////////////
////////////////////////////////////////////

// register_stage_session call. Registers a stage session with the control application.
void CoreControlApplication::register_stage_session (const StageInfoConnect* request,
    std::shared_ptr<Session> controller_session,
    bool rebuild)
{
    Logging::log_info ("CoreControlApplication: Registering stage session " + request->stage_name ()
        + " from local controller " + request->local_address ());

    auto stage_info = std::make_unique<StageInfo> ();
    stage_info->m_stage_name = request->stage_name ();
    stage_info->m_stage_env = request->stage_env ();
    stage_info->m_local_address = request->local_address ();

    std::string stage_name_env = stage_info->m_stage_name + "+" + stage_info->m_stage_env;
    std::string local_controller_address = stage_info->m_local_address;

    {
        std::lock_guard<std::mutex> lock (pending_stage_lock_);
        if (rebuild == false) {
            local_to_data_queue_.emplace (std::move (stage_info));
            pending_controller_sessions_.emplace (stage_name_env, std::move (controller_session));

            this->m_pending_data_plane_sessions.fetch_add (1);
        } else {
            change_in_system = true;

            auto& locations = job_location_tracker[stage_info->m_stage_name];
            try {
                if (locations.find (local_controller_address) == locations.end ()) {
                    locations[local_controller_address] = {};
                }
                locations[local_controller_address].push_back (std::stoi (stage_info->m_stage_env));
            } catch (const std::invalid_argument& e) {
                Logging::log_error ("Invalid stage environment value: " + stage_info->m_stage_env);
            } catch (const std::out_of_range& e) {
                Logging::log_error (
                    "Stage environment value out of range: " + stage_info->m_stage_env);
            }

            // Default priority 3
            auto priority = 3;
            if (jobs_name_to_priority.find (stage_info->m_stage_name)
                != jobs_name_to_priority.end ()) {
                priority = jobs_name_to_priority[stage_info->m_stage_name];
                {
                    std::lock_guard<std::mutex> lock (priority_counter_mutex);
                    priority_counter[priority]++;
                }
            } else {
                Logging::log_error (
                    "Stage name not found in jobs_name_to_priority when registering stage: "
                    + stage_info->m_stage_name);
            }

            if (locations.size () == 1 && locations[local_controller_address].size () == 1) {
                auto priority_values = priorities.find (priority);

                if (priority_values != priorities.end ()) {
                    for (const auto& op : active_ops) {
                        uint64_t priority_value = priority_values->second[op];
                        op_job_demands_order[op][priority_value].push_back (
                            stage_info->m_stage_name);
                    }
                } else {
                    Logging::log_debug ("CoreControlApplication::Priority not compatible.");
                }

                // No need to lock here, only one thread
                job_previous_rates[stage_info->m_stage_name] = {};
                job_rates[stage_info->m_stage_name] = {};
                for (const auto& op : active_ops) {
                    job_previous_rates[stage_info->m_stage_name][op] = 0;
                    job_rates[stage_info->m_stage_name][op] = 0;
                }
            }
            change_in_job[stage_info->m_stage_name] = true;

            controller_address_to_stages[local_controller_address].push_back (stage_name_env);
            controller_sessions_[local_controller_address] = std::move (controller_session);
            controller_sessions_failure_counter[local_controller_address] = 0;

            stage_info_detailed[stage_name_env] = std::move (stage_info);
            this->m_active_data_plane_sessions.fetch_add (1);
        }
    }
}

////////////////////////////////////////////
/////////////// Feedback Loop //////////////
////////////////////////////////////////////

// operator call. Used to initiate the control application feedback loop execution.
void CoreControlApplication::operator() ()
{
    this->execute_feedback_loop ();
}

// Benchmark counters (latencies in microseconds).
static uint64_t build_enforce_time_aggregated = 0;
static int rounds_scalability = max_rounds;
static int rounds_counter = 0;
static uint64_t collect_time_aggregated = 0;
static uint64_t compute_time_aggregated = 0;
static uint64_t enforce_time_aggregated = 0;
// Filled with default value
static auto start = std::chrono::system_clock::now ();
static auto end = std::chrono::system_clock::now ();

// log_stats call. Log control application stats.
void CoreControlApplication::log_stats ()
{
    Logging::log_info (
        "CoreControlApplication::Max Number of rounds: " + std::to_string (max_rounds) + " rounds");
    Logging::log_info (
        "CoreControlApplication::Number of rounds: " + std::to_string (rounds_counter) + " rounds");
    Logging::log_info ("CoreControlApplication::Total Latency: "
        + std::to_string (
            std::chrono::duration_cast<std::chrono::microseconds> (end - start).count ())
        + " [µs]");
    Logging::log_info ("CoreControlApplication::Collect Latency: "
        + std::to_string (collect_time_aggregated) + " [µs]");
    Logging::log_info ("CoreControlApplication::Compute Latency: "
        + std::to_string (compute_time_aggregated) + " [µs]");
    Logging::log_info ("CoreControlApplication::Build Enforce Latency: "
        + std::to_string (build_enforce_time_aggregated) + " [µs]");
    Logging::log_info ("CoreControlApplication::Local Network Enforce Latency: "
        + std::to_string (local_network_enforce_time) + " [µs]");
    Logging::log_info ("CoreControlApplication::Passthrough Network Enforce Latency: "
        + std::to_string (passthrough_network_enforce_time) + " [µs]");
    Logging::log_info ("CoreControlApplication::Enforce Latency: "
        + std::to_string (enforce_time_aggregated) + " [µs]");

    Logging::log_info ("CoreControlApplication::Exiting.");
}

// stop_feedback_loop call. Stops the feedback loop from executing.
void CoreControlApplication::stop_feedback_loop ()
{
    working_application_ = false;
    end = std::chrono::system_clock::now ();
}

// set_as_primary_controller call. Marks controller as primary.
void CoreControlApplication::set_as_primary_controller ()
{
    Logging::log_info ("CoreControlApplication: Setting as primary controller.");
    is_primary_ = true;
}

// execute_feedback_loop call. Executes feedback loop.
void CoreControlApplication::execute_feedback_loop ()
{
    Logging::log_debug ("CoreControlApplication::ExecuteFeedbackLoop");
    working_application_ = true;

    while (working_application_.load () && !is_primary_.load ()) {
        std::this_thread::sleep_for (std::chrono::milliseconds (1000));
    }

    // Sometimes nodes give-up, so let's decrease the number of instances.
    // If all launch correctly it will connect them all.
    // But it case one fails, would still work.
    int d_instances = std::max (1, expected_total_jobs) - 10;

    while (working_application_.load ()
        && (this->m_pending_data_plane_sessions.load () == 0
            || this->m_pending_data_plane_sessions.load () <= d_instances)
        && this->m_active_data_plane_sessions.load () == 0) {
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }

    while (this->m_pending_data_plane_sessions.load () > 0) {
        handle_data_plane_sessions ();
    }

    // this->collect_statistics_global ();

    while (
        working_application_.load () && this->m_active_data_plane_sessions.load () <= d_instances) {
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }

    // std::this_thread::sleep_for (milliseconds (10000));

    start = std::chrono::system_clock::now ();

    Logging::log_info ("CoreControlApplication::Execute feedback loop started.");

    while (ops_maximum_limit_set == false) {
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }

    while (working_application_.load () && rounds_scalability > 0) {
        /*while (working_application_.load () && rounds_scalability > 0
-        && this->m_active_data_plane_sessions.load () > 0) { */

        while (this->m_pending_data_plane_sessions.load () > 0) {
            handle_data_plane_sessions ();
        }

        rounds_scalability--;

        auto start_collect = std::chrono::system_clock::now ();
        this->collect_statistics_global ();
        auto end_collect = std::chrono::system_clock::now ();
        auto collect_time
            = std::chrono::duration_cast<std::chrono::microseconds> (end_collect - start_collect)
                  .count ();

        // If does not have a upper limit, there is no need to compute and enforce new rules.

        collect_time_aggregated += collect_time;

        auto start_compute = std::chrono::system_clock::now ();
        compute_psfa_rules ();
        auto end_compute = std::chrono::system_clock::now ();
        auto compute_time
            = std::chrono::duration_cast<std::chrono::microseconds> (end_compute - start_compute)
                  .count ();
        compute_time_aggregated += compute_time;

        auto start_enforce = std::chrono::system_clock::now ();
        this->enforce_psfa_rules ();
        auto end_enforce = std::chrono::system_clock::now ();
        auto enforce_time
            = std::chrono::duration_cast<std::chrono::microseconds> (end_enforce - start_enforce)
                  .count ();
        enforce_time_aggregated += enforce_time;

        change_in_system = false;

        rounds_counter++;

        auto round_duration
            = std::chrono::duration_cast<std::chrono::microseconds> (end_enforce - start_collect)
                  .count ();

        Logging::log_info ("CoreControlApplication::Round: " + std::to_string (rounds_counter)
            + " took " + std::to_string (round_duration) + ";" + std::to_string (collect_time) + ";"
            + std::to_string (compute_time) + ";" + std::to_string (enforce_time) + " [µs]");
        // Logging::log_info ("Number of rounds: " + std::to_string (rounds_counter) + " rounds");

        if (option_benchmark_ == true) {
            // Does not sleep
        } else {
            // Old version: the unsigned subtraction wraps around when the round takes longer
            // than the cycle, resulting in a (practically) endless sleep.
            // std::this_thread::sleep_for (
            //     microseconds (m_feedback_loop_sleep_time - round_duration));
            auto remaining_time
                = static_cast<int64_t> (m_feedback_loop_sleep_time) - round_duration;
            if (remaining_time > 0) {
                std::this_thread::sleep_for (microseconds (remaining_time));
            }
        }
    }

    // log message and end control loop
    Logging::log_info ("CoreControlApplication::Exiting.");

    if (working_application_.load ()) {
        stop_feedback_loop ();
    }
}

////////////////////////////////////////////
//////////////// Sleep /////////////////////
////////////////////////////////////////////

// sleep call. Used to make control application main thread wait for the next loop.
void CoreControlApplication::sleep ()
{
    std::this_thread::sleep_for (std::chrono::microseconds (this->m_feedback_loop_sleep_time));
}

//////////////////////////////////////////////
/// Supervisor to core controller communication ///
//////////////////////////////////////////////

// CollectCoreControllerStatistics call. Collects statistics from controller through gRPC.
Status CoreControlApplication::CollectCoreControllerStatistics (
    controllers_grpc_interface::StatsGlobalAggregatedMap* reply)
{
    Logging::log_debug (
        "CoreControlApplication::CollectCoreControllerStatistics: Collecting global "
        "controller statistics from supervisor controller.");

    auto* per_operation_usage = reply->mutable_per_operation_usage ();
    auto* counter_per_priority = reply->mutable_counter_per_priority ();

    auto collect_from_global_controller = std::chrono::system_clock::now ();

    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds> (
        collect_from_global_controller.time_since_epoch ())
                         .count ();

    {
        std::lock_guard<std::mutex> lock (usage_per_priority_mutex);
        for (const auto& [operation, priority_usage] : usage_per_priority) {
            auto& stats_rated = (*per_operation_usage)[operation];
            for (const auto& [priority, usage] : priority_usage) {
                (*stats_rated.mutable_usage_per_priority ())[priority] = usage;
                Logging::log_debug ("CoreControlApplication::CollectCoreControllerStatistics "
                    + operation + " " + std::to_string (priority) + " " + std::to_string (usage));
            }
        }
        std::string message = "collect_from_global-p1;" + std::to_string (timestamp) + ";"
            + std::to_string (rounds_counter) + ";";
        message += "meta_op:" + std::to_string (usage_per_priority["meta_op"][1]);
        message += ";data_op:" + std::to_string (usage_per_priority["data_op"][1]);
        Logging::log_info ("CoreControlApplication: " + message);

        message = "collect_from_global-p2;" + std::to_string (timestamp) + ";"
            + std::to_string (rounds_counter) + ";";
        message += "meta_op:" + std::to_string (usage_per_priority["meta_op"][2]);
        message += ";data_op:" + std::to_string (usage_per_priority["data_op"][2]);
        Logging::log_info ("CoreControlApplication: " + message);

        if (!usage_per_priority.empty ()) {
            {
                std::lock_guard<std::mutex> lock (priority_counter_mutex);
                for (const auto& [priority, counter] : priority_counter) {
                    (*counter_per_priority)[priority] = counter;
                }
            }
        }
    }

    return Status::OK;
}

// CollectCoreControllerStatistics call. Collects statistics from controller and returns them.
StageResponseStatController CoreControlApplication::CollectCoreControllerStatistics ()
{
    Logging::log_debug (
        "CoreControlApplication::CollectCoreControllerStatistics: Collecting global "
        "controller statistics.");

    auto collect_from_global_controller = std::chrono::system_clock::now ();

    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds> (
        collect_from_global_controller.time_since_epoch ())
                         .count ();

    StageResponseStatController reply;

    {
        std::lock_guard<std::mutex> lock (usage_per_priority_mutex);
        for (const auto& [operation, priority_usage] : usage_per_priority) {
            auto& stats_rated = reply.controller_stats.per_operation_usage[operation];
            for (const auto& [priority, usage] : priority_usage) {
                stats_rated.usage_per_priority[priority] = usage;
                Logging::log_debug ("CoreControlApplication::CollectCoreControllerStatistics "
                    + operation + " " + std::to_string (priority) + " " + std::to_string (usage));
            }
        }

        std::string message = "collect_from_global-p1;" + std::to_string (timestamp) + ";"
            + std::to_string (rounds_counter) + ";";
        message += "meta_op:" + std::to_string (usage_per_priority["meta_op"][1]);
        message += ";data_op:" + std::to_string (usage_per_priority["data_op"][1]);
        Logging::log_info ("CoreControlApplication: " + message);

        message = "collect_from_global-p2;" + std::to_string (timestamp) + ";"
            + std::to_string (rounds_counter) + ";";
        message += "meta_op:" + std::to_string (usage_per_priority["meta_op"][2]);
        message += ";data_op:" + std::to_string (usage_per_priority["data_op"][2]);
        Logging::log_info ("CoreControlApplication: " + message);

        if (!usage_per_priority.empty ()) {
            {
                std::lock_guard<std::mutex> lock (priority_counter_mutex);

                for (const auto& [priority, counter] : priority_counter) {
                    reply.controller_stats.counter_per_priority[priority] = counter;
                }
            }
        }
    }

    return reply;
}

// EnforceSystemLimit call. Enforces system limits received through gRPC.
Status CoreControlApplication::EnforceSystemLimit (ServerContext* context,
    const controllers_grpc_interface::OperationsLimits* request,
    controllers_grpc_interface::ACK* reply)
{
    // Lock the mutex to ensure thread safety
    std::lock_guard<std::mutex> lock (ops_maximum_limit_mutex);

    auto enforce_system_limit_time = std::chrono::system_clock::now ();

    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds> (
        enforce_system_limit_time.time_since_epoch ())
                         .count ();

    std::string message
        = "enforce;" + std::to_string (timestamp) + ";" + std::to_string (rounds_counter);

    // Iterate through the operations limits provided in the request
    for (const auto& [operation, max_limit] : request->limits_per_operation ()) {
        // Update the maximum limit for the operation
        ops_maximum_limit[operation] = max_limit;
        Logging::log_debug ("CoreControlApplication::EnforceSystemLimit: Operation " + operation
            + " has a maximum limit of " + std::to_string (max_limit));
    }

    ops_maximum_limit_set = true;

    message += ";meta_op:" + std::to_string (ops_maximum_limit["meta_op"]);
    message += ";data_op:" + std::to_string (ops_maximum_limit["data_op"]);

    Logging::log_info ("CoreControlApplication: " + message);

    // Set the reply status to acknowledge the request
    reply->set_m_message (1);

    return Status::OK;
}

// EnforceSystemLimit call. Enforces system limits received from the in-local supervisor.
void CoreControlApplication::EnforceSystemLimit (
    std::unordered_map<std::string, uint64_t>& limits_per_operation)
{
    auto enforce_system_limit_time = std::chrono::system_clock::now ();

    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds> (
        enforce_system_limit_time.time_since_epoch ())
                         .count ();

    std::string message
        = "enforce;" + std::to_string (timestamp) + ";" + std::to_string (rounds_counter);

    // Lock the mutex to ensure thread safety
    std::lock_guard<std::mutex> lock (ops_maximum_limit_mutex);

    // Iterate through the operations limits provided in the request
    for (const auto& [operation, max_limit] : limits_per_operation) {
        // Update the maximum limit for the operation
        ops_maximum_limit[operation] = max_limit;
        Logging::log_debug ("CoreControlApplication::EnforceSystemLimit: Operation " + operation
            + " has a maximum limit of " + std::to_string (max_limit));
    }

    ops_maximum_limit_set = true;

    message += ";meta_op:" + std::to_string (ops_maximum_limit["meta_op"]);
    message += ";data_op:" + std::to_string (ops_maximum_limit["data_op"]);

    Logging::log_info ("CoreControlApplication: " + message);
}

////////////////////////////////////////////
//////////// Auxiliary Functions ///////////
////////////////////////////////////////////

// initialize call. Sets the active operations and starts the control application thread.
void CoreControlApplication::initialize (std::unordered_set<std::string> original_active_ops)
{
    active_ops = std::move (original_active_ops);

    Logging::log_debug ("Current supported operations: ");
    for (const auto& op : active_ops) {
        Logging::log_debug (op + " ");
        op_job_demands_order[op] = {};
    }

    std::thread control_application_thread_t (&CoreControlApplication::operator(), this);
    control_application_thread_t.detach ();
}

// handle_data_plane_sessions call. Handles one pending data plane stage.
void CoreControlApplication::handle_data_plane_sessions ()
{
    std::lock_guard<std::mutex> lock (pending_stage_lock_);
    if (local_to_data_queue_.empty ()) {
        return;
    }

    auto stage_info = std::move (local_to_data_queue_.front ());
    local_to_data_queue_.pop ();

    this->m_pending_data_plane_sessions.fetch_sub (1);
    change_in_system = true;
    /*Logging::log_info ("CoreControlApplication:: Handling stage " + stage_info->m_stage_name
        + " from " + stage_info->m_local_address);*/
    auto& locations = job_location_tracker[stage_info->m_stage_name];
    try {
        if (locations.find (stage_info->m_local_address) == locations.end ()) {
            locations[stage_info->m_local_address] = {};
        }
        locations[stage_info->m_local_address].push_back (std::stoi (stage_info->m_stage_env));
    } catch (const std::invalid_argument& e) {
        Logging::log_error ("Invalid stage environment value: " + stage_info->m_stage_env);
    } catch (const std::out_of_range& e) {
        Logging::log_error ("Stage environment value out of range: " + stage_info->m_stage_env);
    }

    // Assign priotirity 3 if not found data
    auto priority = 3;
    if (jobs_name_to_priority.find (stage_info->m_stage_name) != jobs_name_to_priority.end ()) {
        priority = jobs_name_to_priority[stage_info->m_stage_name];
        {
            std::lock_guard<std::mutex> lock (priority_counter_mutex);
            priority_counter[priority]++;
        }
    } else {
        Logging::log_error ("Stage name not found in jobs_name_to_priority when registering: "
            + stage_info->m_stage_name);
    }

    if (locations.size () == 1 && locations[stage_info->m_local_address].size () == 1) {
        auto priority_values = priorities.find (priority);
        for (const auto& p : priorities) {
            Logging::log_debug (
                "Priority level: " + std::to_string (p.first) + " with operations:");
            for (const auto& op_pair : p.second) {
                Logging::log_debug ("  Operation: " + op_pair.first
                    + " with value: " + std::to_string (op_pair.second));
            }
        }

        if (priority_values != priorities.end ()) {
            for (const auto& op : active_ops) {
                uint64_t priority_value = priority_values->second[op];
                op_job_demands_order[op][priority_value].push_back (stage_info->m_stage_name);
            }
        } else {
            Logging::log_debug ("CoreControlApplication::Priority not compatible.");
        }

        // No need to lock here, only one thread
        job_previous_rates[stage_info->m_stage_name] = {};
        job_rates[stage_info->m_stage_name] = {};
        for (const auto& op : active_ops) {
            job_previous_rates[stage_info->m_stage_name][op] = 0;
            job_rates[stage_info->m_stage_name][op] = 0;
        }
    }

    change_in_job[stage_info->m_stage_name] = true;

    std::string local_controller_address = stage_info->m_local_address;
    std::string stage_name_env = stage_info->m_stage_name + "+" + stage_info->m_stage_env;
    controller_address_to_stages[local_controller_address].push_back (stage_name_env);

    auto it_session = pending_controller_sessions_.find (stage_name_env);

    if (it_session != pending_controller_sessions_.end ()) {
        controller_sessions_[local_controller_address] = std::move (it_session->second);
        controller_sessions_failure_counter[local_controller_address] = 0;
        pending_controller_sessions_.erase (it_session);
    } else {
        Logging::log_error ("CoreControlApplication: Stage name environment not found in "
                            "handle_data_plane_sessions.");
    }

    PStatus status = this->mark_data_plane_stage_ready (local_controller_address,
        stage_info->m_stage_name,
        stage_info->m_stage_env);

    stage_info_detailed[stage_name_env] = std::move (stage_info);

    if (status.isOk ()) {
        this->m_active_data_plane_sessions.fetch_add (1);
    } else {
        Logging::log_error ("DataPlaneSessionHandshake with Data Plane not established.");
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }
}

// mark_data_plane_stage_ready call. Submits STAGE_READY to a data plane stage.
PStatus CoreControlApplication::mark_data_plane_stage_ready (
    const std::string& local_controller_address,
    const std::string& stage_name,
    const std::string& stage_env)
{
    PStatus status = PStatus::Error ();
    std::string rule = std::to_string (STAGE_READY) + "|" + stage_name + "+" + stage_env + "|";

    if (controller_sessions_.find (local_controller_address) == controller_sessions_.end ()) {
        Logging::log_error (
            "Controller session not found for address: " + local_controller_address);
        return PStatus::Error ();
    }

    controller_sessions_.at (local_controller_address)->SubmitRule (rule);

    auto resp_t = controller_sessions_.at (local_controller_address)->GetResult ();
    auto* ack_ptr_t = dynamic_cast<StageResponseACK*> (resp_t.get ());

    if (ack_ptr_t && ack_ptr_t->ACKValue () == static_cast<int> (AckCode::ok)) {
        status = PStatus::OK ();
    }

    // FIX LATER: the ACK status is ignored.
    return PStatus::OK ();
}

////////////////////////////////////////////
//////////// Collect Statistics ////////////
////////////////////////////////////////////

// collect_statistics_global call. Collects statistics from the local controllers.
void CoreControlApplication::collect_statistics_global ()
{
    Logging::log_debug ("CoreControlApplication::CollectStatisticsGlobal");
    const std::string rule = std::to_string (COLLECT_DETAILED_STATS) + "|"
        + std::to_string (COLLECT_GLOBAL_ALL_STATS) + "|";

    for (const auto& [local_controller_address, session] : controller_sessions_) {
        Logging::log_debug ("CoreControlApplication: Submitting rule to controller session at "
            + local_controller_address);
        if (controller_address_to_stages.count (local_controller_address)
            && !controller_address_to_stages.at (local_controller_address).empty ()) {
            session->SubmitRule (rule);
        }
    }

    std::list<std::string> sessions_to_delete;

    for (const auto& [local_controller_address, session] : controller_sessions_) {
        if (controller_address_to_stages.count (local_controller_address)
            && !controller_address_to_stages.at (local_controller_address).empty ()) {
            collect_statistics_result_shared (session,
                local_controller_address,
                sessions_to_delete);
        }
    }

    for (const auto& local_controller_address : sessions_to_delete) {
        controller_sessions_.erase (local_controller_address);
    }
}

// Number of consecutive failed statistics collections after which a controller session is removed.
int stage_connect_attempt_tries = 4;

// collect_statistics_result_shared call. Collects the statistics of a local controller.
void CoreControlApplication::collect_statistics_result_shared (
    const std::shared_ptr<Session>& session,
    const std::string local_controller_address,
    std::list<std::string>& sessions_to_delete)
{
    auto stats_ptr = session->GetResult ();

    if (!stats_ptr) {
        // Failed to collect from controller.
        Logging::log_error ("CoreControlApplication: collect_statistics_result_shared: Failed to "
                            "connect to controller "
            + local_controller_address);
        controller_sessions_failure_counter[local_controller_address]++;
        if (controller_sessions_failure_counter[local_controller_address]
            >= stage_connect_attempt_tries) {
            handle_invalid_stats (local_controller_address, sessions_to_delete);
        }
        return;
    }

    auto* response_ptr = dynamic_cast<StageResponseStats*> (stats_ptr.get ());

    if (!response_ptr || !response_ptr->m_stats_ptr || response_ptr->m_stats_ptr->empty ()) {
        Logging::log_error (
            "CoreControlApplication: collect_statistics_result_shared: Stats return empty from "
            + local_controller_address);
        controller_sessions_failure_counter[local_controller_address]++;
        if (controller_sessions_failure_counter[local_controller_address]
            >= stage_connect_attempt_tries) {
            handle_invalid_stats (local_controller_address, sessions_to_delete);
        }
        return;
    }

    for (const auto& stage_info : (*response_ptr->m_stats_ptr)) {
        auto* global_stat_ptr = dynamic_cast<StageResponseStatAll*> (stage_info.second.get ());

        if (!global_stat_ptr || !global_stat_ptr->all_total_rates) {
            remove_stage (stage_info.first);
            auto& stages = controller_address_to_stages.at (local_controller_address);
            stages.erase (std::remove (stages.begin (), stages.end (), stage_info.first),
                stages.end ());

            if (stages.empty ()) {
                controller_address_to_stages.erase (local_controller_address);
                sessions_to_delete.push_back (local_controller_address);
            }
        } else {
            auto stage_name_env = stage_info.first;

            for (const auto& operation_rate : *global_stat_ptr->all_total_rates) {
                // Ensure the map is initialized
                auto operation = operation_rate.first;
                auto usage = operation_rate.second;
                if (usage_per_operation_and_stage.find (operation)
                    == usage_per_operation_and_stage.end ()) {
                    usage_per_operation_and_stage[operation] = {};
                }
                // Add the usage to the map
                usage_per_operation_and_stage[operation][stage_name_env] = usage;
            }
        }
    }

    controller_sessions_failure_counter[local_controller_address] = 0;
}

// handle_invalid_stats call. Removes the stages of a local controller that sent invalid stats.
void CoreControlApplication::handle_invalid_stats (const std::string& local_controller_address,
    std::list<std::string>& sessions_to_delete)
{
    Logging::log_error (
        "CoreControlApplication: Invalid stats received from controller session at address: "
        + local_controller_address);
    // working_application_ = false;
    if (controller_address_to_stages.find (local_controller_address)
        != controller_address_to_stages.end ()) {
        auto& stages = controller_address_to_stages.at (local_controller_address);

        for (const auto& stage : stages) {
            remove_stage (stage);
        }

        controller_address_to_stages.erase (local_controller_address);
        sessions_to_delete.push_back (local_controller_address);
    } else {
        Logging::log_error ("CoreControlApplication: Local controller address not found: "
            + local_controller_address);
    }
}

////////////////////////////////////////////
////// Compute and Enforce new rules ///////
////////////////////////////////////////////

// compute_psfa_rules call. Computes PSFA rules for all active operations.
void CoreControlApplication::compute_psfa_rules ()
{
    int current_jobs = static_cast<int> (job_location_tracker.size ());

    // Check if there are active jobs to control.
    if (current_jobs > 0) {
        const int ops_size = static_cast<int> (active_ops.size ());

        std::vector<std::thread> threads;
        threads.reserve (ops_size);

        for (const auto& operation : active_ops) {
            threads.emplace_back (&CoreControlApplication::compute_psfa_rules_per_op,
                this,
                operation);
        }

        for (auto& thread : threads) {
            thread.join ();
        }

        auto collect_in_core_controller = std::chrono::system_clock::now ();

        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds> (
            collect_in_core_controller.time_since_epoch ())
                             .count ();

        std::string message = "collect_core-p1;" + std::to_string (timestamp) + ";"
            + std::to_string (rounds_counter) + ";";
        message += "meta_op:" + std::to_string (usage_per_priority["meta_op"][1]);
        message += ";data_op:" + std::to_string (usage_per_priority["data_op"][1]);
        Logging::log_info ("CoreControlApplication: " + message);

        message = "collect_core-p2;" + std::to_string (timestamp) + ";"
            + std::to_string (rounds_counter) + ";";
        message += "meta_op:" + std::to_string (usage_per_priority["meta_op"][2]);
        message += ";data_op:" + std::to_string (usage_per_priority["data_op"][2]);
        Logging::log_info ("CoreControlApplication: " + message);

        /*std::lock_guard<std::mutex> lock (usage_per_priority_mutex);
        for (const auto& [operation, priority_usage] : usage_per_priority) {
            for (const auto& [priority, usage] : priority_usage) {
                Logging::log_info ("CoreControlApplication::Collect " + operation + " "
                    + std::to_string (priority) + " " + std::to_string (usage));
            }
        }*/
    } else {
        std::lock_guard<std::mutex> lock (usage_per_priority_mutex);
        usage_per_priority.clear ();

        Logging::log_debug ("CoreControlApplication: No active jobs to control.");
    }
}

// compute_psfa_rules_per_op call. Computes PSFA rules for an operation.
void CoreControlApplication::compute_psfa_rules_per_op (const std::string& operation)
{
    // Logging::log_debug("ControlApplication: Computing PSFA Rules for operation: " +
    // operation);

    // Assign all bandwidth to leftover_iops
    // Old version: read without ops_maximum_limit_mutex, while EnforceSystemLimit may write to it,
    // and operator[] may insert into a map shared by the per-operation threads (data race).
    // uint64_t left_iops_per_operation = ops_maximum_limit[operation];
    uint64_t left_iops_per_operation = 0;
    {
        std::lock_guard<std::mutex> lock (ops_maximum_limit_mutex);
        auto op_limit = ops_maximum_limit.find (operation);
        if (op_limit != ops_maximum_limit.end ()) {
            left_iops_per_operation = op_limit->second;
        }
    }

    std::unordered_map<std::string, uint64_t> app_usage;
    uint64_t system_total_rate = 0;

    compute_psfa_per_op_phase1 (operation, app_usage, system_total_rate, left_iops_per_operation);
    compute_psfa_per_op_phase2 (operation, app_usage, system_total_rate, left_iops_per_operation);

    uint64_t op_maximum_limit = 0;
    {
        std::lock_guard<std::mutex> lock (ops_maximum_limit_mutex);
        op_maximum_limit = ops_maximum_limit[operation];
    }
    compute_psfa_per_op_tune_rates (operation, system_total_rate, op_maximum_limit);
}

// aggregate_collected_stats_per_operation_and_job call. Aggregates the usage of a job.
uint64_t CoreControlApplication::aggregate_collected_stats_per_operation_and_job (
    const std::string& operation,
    const std::string& job)
{
    uint64_t total_app_rate = 0;

    for (const auto& [local_controller_address, envs] : job_location_tracker[job]) {

        for (int env : envs) {
            std::string stage_env = job + "+" + std::to_string (env);

            if (usage_per_operation_and_stage[operation].find (stage_env)
                == usage_per_operation_and_stage[operation].end ()) {
                // If the stage does not exist, we assume a default rate of 1
                usage_per_operation_and_stage[operation][stage_env] = 1;
            }

            if (usage_per_operation_and_stage[operation][stage_env] == 0) {
                // If the stage reports zero usage, we assume a default rate of 1
                usage_per_operation_and_stage[operation][stage_env] = 1;
            }

            total_app_rate += usage_per_operation_and_stage[operation][stage_env];
        }
    }

    return total_app_rate;
}

// compute_psfa_per_op_phase1 call. Computes phase 1 of PSFA's algorithm for an operation.
void CoreControlApplication::compute_psfa_per_op_phase1 (const std::string& operation,
    std::unordered_map<std::string, uint64_t>& app_usage,
    uint64_t& system_total_rate,
    uint64_t& left_iops_per_operation)
{
    int current_jobs = static_cast<int> (job_location_tracker.size ());

    // Get the job demands for the current operation, ordered by priority.
    // The structure is: operation -> value -> list of jobs
    // Assigns jobs with lower demand first, to ensure they get their fair share before higher
    // demand jobs are considered.
    // Old version: operator[] may insert into op_job_demands_order, which is shared by the
    // per-operation threads (data race).
    // auto op_value_jobs = op_job_demands_order[operation];
    std::map<uint64_t, std::vector<std::string>> op_value_jobs;
    auto op_demands = op_job_demands_order.find (operation);
    if (op_demands != op_job_demands_order.end ()) {
        op_value_jobs = op_demands->second;
    }

    {
        std::lock_guard<std::mutex> lock (usage_per_priority_mutex);
        usage_per_priority[operation].clear ();

        for (const auto& value_jobs : op_value_jobs) {
            uint64_t op_job_demand = value_jobs.first;

            // First phase: For each job assign either fair share or demand.
            for (const auto& job : value_jobs.second) {
                uint64_t op_job_rate
                    = aggregate_collected_stats_per_operation_and_job (operation, job);
                uint64_t fair_share = left_iops_per_operation / current_jobs;

                usage_per_priority[operation][jobs_name_to_priority[job]] += op_job_rate;

                auto demand_rate_difference = op_job_demand - op_job_rate;

                // First phase
                auto almost_demand_limit = static_cast<uint64_t> (round (op_job_demand * 0.95));
                if (op_job_rate <= almost_demand_limit) {
                    auto demand_rate_difference = op_job_demand - op_job_rate;
                    auto threshold_value
                        = static_cast<uint64_t> (round (demand_rate_difference * 0.05));

                    std::lock_guard<std::mutex> lock (job_rates_mutex);
                    if (op_job_rate + threshold_value < fair_share) {
                        job_rates[job][operation] = op_job_rate + threshold_value;
                    } else {
                        job_rates[job][operation] = fair_share;
                    }
                } else {
                    std::lock_guard<std::mutex> lock (job_rates_mutex);
                    if (op_job_demand < fair_share) {
                        job_rates[job][operation] = op_job_demand;
                    } else {
                        job_rates[job][operation] = fair_share;
                    }
                }
                app_usage[job] = op_job_rate;
                system_total_rate += op_job_rate;
                current_jobs--;
                std::lock_guard<std::mutex> lock (job_rates_mutex);
                left_iops_per_operation -= job_rates[job][operation];
            }
        }
    }
}

// compute_psfa_per_op_phase2 call. Computes phase 2 of PSFA's algorithm for an operation.
void CoreControlApplication::compute_psfa_per_op_phase2 (const std::string& operation,
    std::unordered_map<std::string, uint64_t>& app_usage,
    uint64_t system_total_rate,
    uint64_t left_iops_per_operation)
{
    /*Phase 2: Distribute remaining rate*/
    double left_iops_copy = static_cast<double> (left_iops_per_operation);

    if (left_iops_copy > 0) {
        for (const auto& app : app_usage) {
            auto job_name = app.first;

            double add_iops_perc
                = static_cast<double> (app.second) / static_cast<double> (system_total_rate);
            auto extra_rate = static_cast<uint64_t> (std::floor (add_iops_perc * left_iops_copy));
            std::lock_guard<std::mutex> lock (job_rates_mutex);
            job_rates[job_name][operation] += extra_rate;

            left_iops_per_operation -= extra_rate;
        }
    }
}

// compute_psfa_per_op_tune_rates call. Tunes the computed rates for an operation.
void CoreControlApplication::compute_psfa_per_op_tune_rates (const std::string& operation,
    uint64_t system_total_rate,
    uint64_t op_maximum_limit)
{
    uint64_t maintained_rate = 0;
    uint64_t updated_rate = 0;

    {
        std::lock_guard<std::mutex> lock (job_rates_mutex);
        for (const auto& app : job_rates) {
            auto job_name = app.first;

            // validate if assigned rate surpasses the changing bandwidth threshold
            auto rates_difference = static_cast<uint64_t> (std::abs (static_cast<int64_t> (
                job_rates[job_name][operation] - job_previous_rates[job_name][operation])));

            // validate if assigned rate surpasses the changing bandwidth threshold
            if (!change_in_job[job_name] && !change_in_system.load ()
                && (static_cast<double> (rates_difference) < (job_rates[job_name][operation] * 0.01)
                    || (static_cast<double> (system_total_rate) < 0.95 * op_maximum_limit
                        && static_cast<double> (rates_difference)
                            < (job_rates[job_name][operation] * 0.05)))) {
                maintained_rate += job_previous_rates[job_name][operation];
                job_rates[job_name][operation] = job_previous_rates[job_name][operation];
            } else {
                updated_rate += job_rates[job_name][operation];
            }
        }
    }

    double varied_perc = 1.0;

    if (static_cast<double> (system_total_rate) < 0.95 * op_maximum_limit
        && maintained_rate + updated_rate > op_maximum_limit) {
        uint64_t allowed_rate = op_maximum_limit - maintained_rate;
        varied_perc = static_cast<double> (allowed_rate) / static_cast<double> (updated_rate);
    }

    {
        std::lock_guard<std::mutex> lock (job_rates_mutex);
        if (varied_perc != 1.0) {
            for (const auto& app : job_rates) {
                auto job_name = app.first;
                if (job_rates[job_name][operation] == job_previous_rates[job_name][operation]) {
                    /*DO Nothing*/
                } else {
                    uint64_t cur_job_rate = job_rates[app.first][operation];
                    job_rates[app.first][operation] = static_cast<uint64_t> (
                        std::floor (static_cast<double> (cur_job_rate) * varied_perc));
                    job_previous_rates[app.first][operation] = job_rates[app.first][operation];
                }
            }
        }
    }
}

// enforce_psfa_rules call. Enforces the computed PSFA rules.
void CoreControlApplication::enforce_psfa_rules ()
{
    auto start_enforce = std::chrono::system_clock::now ();

    std::unordered_map<std::string, bool> address_updated;
    std::unordered_map<std::string, std::string> rules_per_local;

    for (const auto& app : job_location_tracker) {
        change_in_job[app.first] = false;
        build_enforcement_rule (app.first, app.second, address_updated, rules_per_local);
    }

    auto end_build_enforce = std::chrono::system_clock::now ();
    build_enforce_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (end_build_enforce - start_enforce)
               .count ();

    Logging::log_debug ("Enforcement rules built.");

    for (const auto& local_sessions : controller_sessions_) {
        const auto& local_controller_address = local_sessions.first;
        if (address_updated[local_controller_address]) {
            auto& controller = local_sessions.second;
            rules_per_local[local_controller_address] += "*.";
            // CHANGED HERE
            // Logging::log_info ("Enforcing rule " + rules_per_local[local_controller_address]
            //     + " to " + local_controller_address);
            controller->SubmitRule (rules_per_local[local_controller_address]);
        }
    }

    for (const auto& local_sessions : controller_sessions_) {
        const auto& local_controller_address = local_sessions.first;
        if (address_updated[local_controller_address]) {
            auto& controller = local_sessions.second;
            auto ack_ptr = controller->GetResult ();
            if (Logging::is_debug_enabled () && ack_ptr) {
                auto* response_ptr = dynamic_cast<StageResponseACK*> (ack_ptr.get ());
                if (response_ptr) {
                    Logging::log_debug (
                        "ACK response :: " + std::to_string (response_ptr->ResponseType ()) + " -- "
                        + response_ptr->toString ());
                }
            }
        }
    }
}

// build_enforcement_rule call. Builds the enforcement rules of an application.
void CoreControlApplication::build_enforcement_rule (const std::string& app_name,
    const std::unordered_map<std::string, std::vector<int>>& local_to_envs,
    std::unordered_map<std::string, bool>& address_updated,
    std::unordered_map<std::string, std::string>& rules_per_local)
{
    int total_stages = 0;
    for (const auto& [local_controller_address, envs] : local_to_envs) {
        total_stages += envs.size ();
    }

    std::string type_enforcement_rule = std::to_string (CREATE_ENF_RULE);

    for (const auto& [local_controller_address, envs] : local_to_envs) {
        if (rules_per_local.find (local_controller_address) == rules_per_local.end ()) {
            rules_per_local[local_controller_address] = type_enforcement_rule;
        }

        bool send_rule = false;

        for (const auto& operation : active_ops) {
            // No need for locks
            if (job_rates[app_name][operation]) {
                send_rule = true;
                rules_per_local[local_controller_address]
                    += "|.0|" + app_name + "|" + operation + "|";

                // ADDED
                // job_rates[app_name][operation] = 21474836;

                long limit_per_stage = std::floor (job_rates[app_name][operation] / total_stages);
                for (int env : envs) {
                    rules_per_local[local_controller_address]
                        += "*" + std::to_string (env) + ":" + std::to_string (limit_per_stage);

                    /*std::string stage_env = app_name + "+" + std::to_string (env);
                    std::cout << "stats:" << operation << ": " << app_name << "->"
                              << usage_per_operation_and_stage[operation][stage_env] << ":"
                              << limit_per_stage << "\n";*/
                }
            }
        }

        if (send_rule) {
            address_updated[local_controller_address] = true;
        }
    }
}

// remove_stage call. Removes stage from the control logic.
void CoreControlApplication::remove_stage (const std::string& stage_name_env)
{
    Logging::log_info ("CoreControlApplication:: Removing stage " + stage_name_env);
    if (stage_info_detailed.find (stage_name_env) == stage_info_detailed.end ()) {
        Logging::log_error ("Stage info not found for stage: " + stage_name_env);
        return;
    }
    m_active_data_plane_sessions.fetch_sub (1);

    auto stage_info = stage_info_detailed.at (stage_name_env).get ();
    auto& local_controller_address_to_envs = job_location_tracker.at (stage_info->m_stage_name);
    auto& envs = local_controller_address_to_envs.at (stage_info->m_local_address);

    envs.erase (std::remove (envs.begin (), envs.end (), std::stoi (stage_info->m_stage_env)),
        envs.end ());

    if (jobs_name_to_priority.find (stage_info->m_stage_name) != jobs_name_to_priority.end ()) {
        auto priority = jobs_name_to_priority[stage_info->m_stage_name];
        {
            std::lock_guard<std::mutex> lock (priority_counter_mutex);
            priority_counter[priority]--;
        }
    } else {
        Logging::log_error ("Stage name not found in jobs_name_to_priority when removing stage: "
            + stage_info->m_stage_name);
    }

    if (envs.empty ()) {
        local_controller_address_to_envs.erase (stage_info->m_local_address);
        if (local_controller_address_to_envs.empty ()) {
            job_location_tracker.erase (stage_info->m_stage_name);
            job_rates.erase (stage_info->m_stage_name);
            job_previous_rates.erase (stage_info->m_stage_name);
            change_in_job.erase (stage_info->m_stage_name);

            for (const auto& op : active_ops) {
                // uint64_t value = op_job_demands[op][stage_info->m_stage_name];
                uint64_t value = priorities[jobs_name_to_priority[stage_info->m_stage_name]][op];
                auto& job_list = op_job_demands_order[op][value];
                job_list.erase (
                    std::remove (job_list.begin (), job_list.end (), stage_info->m_stage_name),
                    job_list.end ());
            }
        }
    }

    change_in_system = true;
    change_in_job[stage_info->m_stage_name] = true;
    stage_info_detailed.erase (stage_name_env);
}

} // namespace cheferd
