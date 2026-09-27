/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/controller/control_application/passthrough_control_application.hpp"

#include "shio/utils/rules_file_parser.hpp"

#include <chrono>
#include <list>
#include <thread>
#include <unordered_map>

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace shio {

// PassthroughControlApplication parameterized constructor.
PassthroughControlApplication::PassthroughControlApplication (const std::string& local_address,
    const uint64_t& cycle_sleep_time) :
    ControlApplication { cycle_sleep_time },
    controller_address { local_address },
    controller_address_to_stages {}
{
    Logging::log_info ("PassthroughControlApplication: Controller address:" + controller_address);
}

// PassthroughControlApplication default destructor.
PassthroughControlApplication::~PassthroughControlApplication ()
{
    Logging::log_info ("PassthroughControlApplication: exiting ...\n");
}

////////////////////////////////////////////
///////// Register new sessions ////////////
////////////////////////////////////////////

// register_communication_channel call. Register communication channel to upper controller.
void PassthroughControlApplication::register_communication_channel (
    std::shared_ptr<grpc::Channel> channel)
{
    stub_ = ControllerToUpper::NewStub (channel);
}

// register_stage_session call. Register a new data plane.
void PassthroughControlApplication::register_stage_session (const StageInfoConnect* request,
    std::shared_ptr<LocalControllerSession> controller_session,
    bool rebuild)
{
    Logging::log_info ("PassthroughControlApplication: Registering stage session "
        + request->stage_name () + " from local controller");

    if (rebuild == false) {
        {
            std::lock_guard<std::mutex> lock (pending_stage_lock_);
            auto stage_name_env = request->stage_name () + "+" + request->stage_env ();
            pending_stages_to_controller_address.emplace (stage_name_env,
                request->local_address ());
            pending_controller_sessions_.emplace (stage_name_env, std::move (controller_session));
        }

        StageInfoConnect request_new;
        request_new.set_local_address (controller_address);
        request_new.set_stage_name (request->stage_name ());
        request_new.set_stage_env (request->stage_env ());

        ConnectReply reply;
        ClientContext context;
        Status status = stub_->ConnectStageToUpper (&context, request_new, &reply);

        int still_attempt = 20;
        while (still_attempt > 0 && !status.ok ()) {
            std::this_thread::sleep_for (microseconds (100000));
            ConnectReply reply_repeat;
            ClientContext context_repeat;
            status = stub_->ConnectStageToUpper (&context_repeat, request_new, &reply_repeat);
            still_attempt--;
        }

        Logging::log_info (
            "PassthroughControlApplication: Connect stage request sent to upper controller");

        if (!status.ok ()) {
            Logging::log_info ("PassthroughControlApplication: Connection failed");
        }
    } else {
        auto stage_name_env = request->stage_name () + "+" + request->stage_env ();
        {
            std::lock_guard<std::mutex> lock (stages_to_controller_address_lock_);
            auto local_controller_address = request->local_address ();
            controller_sessions_[local_controller_address] = std::move (controller_session);
            stages_to_controller_address.emplace (stage_name_env, local_controller_address);
            controller_address_to_stages[local_controller_address].push_back (stage_name_env);
        }

        // mark_stage_ready (stage_name_env);
    }
}

////////////////////////////////////////////
/////////////// Feedback Loop //////////////
////////////////////////////////////////////

// operator call. Used to initiate the control application feedback loop execution.
void PassthroughControlApplication::operator() ()
{
    this->execute_feedback_loop ();
}

// Benchmark counters (latencies in microseconds).
std::atomic<int> passthrough_all_collect_time_aggregated { 0 };
std::atomic<int> passthrough_all_enforce_time_aggregated { 0 };
std::atomic<int> passthrough_enforce_time_aggregated { 0 };

static int local_collect_time_aggregated = 0;
static int rounds_scalability = max_rounds;
static int rounds_counter = 0;

// log_stats call. Log control application stats.
void PassthroughControlApplication::log_stats ()
{
    Logging::log_info ("PassthroughControlApplication::Number of rounds: "
        + std::to_string (rounds_counter) + " rounds");
    Logging::log_info ("PassthroughControlApplication::Total Latency: "
        + std::to_string (
            passthrough_all_collect_time_aggregated + passthrough_all_enforce_time_aggregated)
        + " [µs]");
    Logging::log_info ("PassthroughControlApplication::All Collect Latency: "
        + std::to_string (passthrough_all_collect_time_aggregated) + " [µs]");
    Logging::log_info ("PassthroughControlApplication::Collect Latency: "
        + std::to_string (local_collect_time_aggregated) + " [µs]");
    Logging::log_info ("PassthroughControlApplication::All Enforce Latency: "
        + std::to_string (passthrough_all_enforce_time_aggregated) + " [µs]");
    Logging::log_info ("PassthroughControlApplication::Enforce Latency: "
        + std::to_string (passthrough_enforce_time_aggregated) + " [µs]");

    Logging::log_info ("PassthroughControlApplication::Exiting.");
}

// stop_feedback_loop call. Stops the feedback loop from executing.
void PassthroughControlApplication::stop_feedback_loop ()
{
    working_application_ = false;
}

// execute_feedback_loop call. Executes feedback loop.
void PassthroughControlApplication::execute_feedback_loop ()
{
    PStatus status = PStatus::Error ();
    working_application_ = true;

    while (working_application_.load () && rounds_scalability > 0) {
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }

    stop_feedback_loop ();

    //_exit (EXIT_SUCCESS);
}

////////////////////////////////////////////
//////////////// Sleep /////////////////////
////////////////////////////////////////////

// sleep call. Used to make control application main thread wait for the next loop.
void PassthroughControlApplication::sleep ()
{
    std::this_thread::sleep_for (std::chrono::microseconds (this->m_feedback_loop_sleep_time));
}

//////////////////////////////////////////////
/// Upper to local controller communication ///
//////////////////////////////////////////////

// MarkStageReady call. Mark stage ready from upper controller.
Status PassthroughControlApplication::MarkStageReady (ServerContext* context,
    const controllers_grpc_interface::StageReadyRaw* request,
    controllers_grpc_interface::ACK* reply)
{
    Logging::log_debug ("PassthroughControlApplication: Mark stage ready "
        + request->stage_name_env () + " from core controller");

    std::lock_guard<std::mutex> lock (session_lock_);

    std::string local_controller_address;
    {
        std::lock_guard<std::mutex> lock (pending_stage_lock_);
        auto it_address = pending_stages_to_controller_address.find (request->stage_name_env ());
        auto it_session = pending_controller_sessions_.find (request->stage_name_env ());

        if (it_address != pending_stages_to_controller_address.end ()
            && it_session != pending_controller_sessions_.end ()) {
            local_controller_address = it_address->second;
            controller_sessions_[local_controller_address] = std::move (it_session->second);

            pending_stages_to_controller_address.erase (it_address);
            pending_controller_sessions_.erase (it_session);
        } else {
            Logging::log_error ("PassthroughControlApplication: Stage name environment not found.");
            reply->set_m_message (0);
            return Status::CANCELLED;
        }
    }

    {
        std::lock_guard<std::mutex> lock (stages_to_controller_address_lock_);
        stages_to_controller_address.emplace (request->stage_name_env (), local_controller_address);
        controller_address_to_stages[local_controller_address].push_back (
            request->stage_name_env ());
    }

    mark_stage_ready (request->stage_name_env ());

    reply->set_m_message (1);
    return Status::OK;
}

// CreateEnforcementRule call. Create enforcement rules from upper controller.
Status PassthroughControlApplication::CreateEnforcementRule (ServerContext* context,
    const controllers_grpc_interface::EnforcementRulesMult* request,
    controllers_grpc_interface::ACK* reply)
{
    std::lock_guard<std::mutex> lock (session_lock_);

    auto start_enforce = std::chrono::system_clock::now ();
    Status status = Status::OK;
    std::unordered_map<std::string, std::string> rules_per_leaf;

    std::string intro_enforcement_rule = std::to_string (CREATE_ENF_RULE);
    std::string info_enforcement_rule = "";
    std::string rates_enforcement_rule = "";

    for (const auto& rule : request->rules ()) {
        info_enforcement_rule = "|.0|" + rule.m_stage_name () + "|" + rule.m_operation () + "|";

        for (const auto& env_rate : rule.env_rates ()) {
            rates_enforcement_rule
                = "*" + std::to_string (env_rate.first) + ":" + std::to_string (env_rate.second);

            {
                std::lock_guard<std::mutex> lock (stages_to_controller_address_lock_);

                std::string stage_name
                    = rule.m_stage_name () + "+" + std::to_string (env_rate.first);
                if (stages_to_controller_address.find (stage_name)
                    == stages_to_controller_address.end ()) {
                    continue;
                }

                auto controller_address = stages_to_controller_address.at (stage_name);
                auto tmp_rule = rules_per_leaf.find (controller_address);
                if (tmp_rule == rules_per_leaf.end ()) {
                    rules_per_leaf[controller_address] = intro_enforcement_rule;
                }
                rules_per_leaf[controller_address] += info_enforcement_rule;
                rules_per_leaf[controller_address] += rates_enforcement_rule;
            }
        }
    }

    auto s_enforce = std::chrono::system_clock::now ();

    for (auto& rules : rules_per_leaf) {
        rules.second += "*.";
        Logging::log_debug (
            "PassthroughControlApplication: Received create enforcement rule from core controller "
            + rules.second);

        std::string local_controller_address = rules.first;
        if (controller_sessions_.find (local_controller_address) == controller_sessions_.end ()) {
            continue;
        }
        this->controller_sessions_[local_controller_address]->SubmitRule (rules.second);
    }

    for (auto& rules : rules_per_leaf) {
        std::string local_controller_address = rules.first;
        if (controller_sessions_.find (local_controller_address) == controller_sessions_.end ()) {
            continue;
        }
        std::unique_ptr<StageResponse> ack_ptr
            = this->controller_sessions_[rules.first]->GetResult ();

        if (ack_ptr != nullptr) {
            auto* response_ptr = dynamic_cast<StageResponseACK*> (ack_ptr.get ());

            if (response_ptr->ACKValue () == 1) {
                reply->set_m_message (1);
            } else {
                reply->set_m_message (1);
            }
        }
    }

    auto e_enforce = std::chrono::system_clock::now ();
    passthrough_enforce_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (e_enforce - s_enforce).count ();

    auto all_end_enforce = std::chrono::system_clock::now ();
    passthrough_all_enforce_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (all_end_enforce - start_enforce)
               .count ();

    rounds_scalability--;
    rounds_counter++;

    return status;
}

// CollectGlobalAllStatistics call. Collect statistics request from upper controller.
Status PassthroughControlApplication::CollectGlobalAllStatistics (ServerContext* context,
    const controllers_grpc_interface::Empty* request,
    controllers_grpc_interface::StatsGlobalAllMap* reply)
{
    std::lock_guard<std::mutex> lock (session_lock_);

    Logging::log_debug (
        "PassthroughControlApplication: Received collect statistics request from core controller");

    auto start_collect = std::chrono::system_clock::now ();
    std::string rule = std::to_string (COLLECT_DETAILED_STATS) + "|"
        + std::to_string (COLLECT_GLOBAL_ALL_STATS) + "|";

    for (const auto& leaf_session : controller_sessions_) {
        leaf_session.second->SubmitRule (rule);
    }

    auto& stats_map = *reply->mutable_gl_stats ();
    std::list<std::string> sessions_to_delete;

    for (const auto& leaf_session : controller_sessions_) {
        std::unique_ptr<StageResponse> stats_ptr = leaf_session.second->GetResult ();

        if (stats_ptr != nullptr) {
            auto* response_ptr = dynamic_cast<StageResponseStats*> (stats_ptr.get ());

            if (response_ptr != nullptr && response_ptr->m_stats_ptr != nullptr
                && !response_ptr->m_stats_ptr.get ()->empty ()) {
                for (const auto& stats_value : (*response_ptr->m_stats_ptr.get ())) {
                    auto* global_stat_ptr
                        = dynamic_cast<StageResponseStatAll*> (stats_value.second.get ());

                    std::string name = stats_value.first;
                    controllers_grpc_interface::StatsGlobalAll stats_global;

                    if (global_stat_ptr == nullptr) {
                        continue;
                    }

                    if (global_stat_ptr->all_total_rates == nullptr) {
                        sessions_to_delete.push_back (name);
                    } else {
                        auto& data_plane_stats_map = *stats_global.mutable_all_rates ();
                        if (!global_stat_ptr->all_total_rates) {
                            sessions_to_delete.push_back (name);
                            continue;
                        }

                        for (const auto& op_rate : *global_stat_ptr->all_total_rates.get ()) {
                            data_plane_stats_map[op_rate.first] = op_rate.second;
                            Logging::log_debug (
                                "PassthroughControlApplication: CollectGlobalAllStatistics -> "
                                + name + " | " + op_rate.first + ": "
                                + std::to_string (op_rate.second));
                        }
                    }
                    stats_global.set_stage_name (global_stat_ptr->stage_name);
                    stats_global.set_stage_env (global_stat_ptr->stage_env);
                    stats_map[name] = stats_global;
                }
            }
        }
    }

    auto end_collect_b = std::chrono::system_clock::now ();
    local_collect_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (end_collect_b - start_collect)
               .count ();

    for (const auto& session : sessions_to_delete) {
        std::lock_guard<std::mutex> lock (stages_to_controller_address_lock_);
        if (stages_to_controller_address.find (session) != stages_to_controller_address.end ()) {
            auto session_controller_address = stages_to_controller_address.at (session);
            stages_to_controller_address.erase (session);

            if (controller_address_to_stages.find (session_controller_address)
                != controller_address_to_stages.end ()) {
                auto& stages = controller_address_to_stages.at (session_controller_address);
                stages.erase (std::remove (stages.begin (), stages.end (), session), stages.end ());

                if (stages.empty ()) {
                    controller_address_to_stages.erase (session_controller_address);
                    controller_sessions_.erase (session_controller_address);
                }
            }
        }
    }

    auto end_collect = std::chrono::system_clock::now ();
    passthrough_all_collect_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (end_collect - start_collect)
               .count ();

    return Status::OK;
}

////////////////////////////////////////////
//////////// Auxiliary Functions ///////////
////////////////////////////////////////////

// initialize call. Starts thread to run control application.
void PassthroughControlApplication::initialize ()
{
    std::thread control_application_thread_t = std::thread (std::ref (*this));
    control_application_thread_t.detach ();
}

// mark_stage_ready call. Submits STAGE_READY to data plane stage.
PStatus PassthroughControlApplication::mark_stage_ready (const std::string& stage_name_env) const
{
    // Logging::log_info ("PassthroughControlApplication: Mark stage ready " + stage_name_env);
    PStatus status = PStatus::Error ();
    std::string rule = std::to_string (STAGE_READY) + "|" + stage_name_env + "|";

    auto controller_address = stages_to_controller_address.at (stage_name_env);
    status = controller_sessions_.at (controller_address)->SubmitRule (rule);

    if (status.isOk ()) {
        std::unique_ptr<StageResponse> response
            = controller_sessions_.at (controller_address)->GetResult ();
        auto* ack_ptr = dynamic_cast<StageResponseACK*> (response.get ());

        if (ack_ptr != nullptr && ack_ptr->ACKValue () == static_cast<int> (AckCode::ok)) {
            status = PStatus::OK ();
        }
    }
    return status;
}

} // namespace shio
