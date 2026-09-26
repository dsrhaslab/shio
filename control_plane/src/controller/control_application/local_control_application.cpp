/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#include "cheferd/controller/control_application/local_control_application.hpp"

#include "cheferd/utils/rules_file_parser.hpp"

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace cheferd {

// LocalControlApplication parameterized constructor.
LocalControlApplication::LocalControlApplication (const std::string& core_address,
    const std::string& local_address) :
    ControlApplication {},
    data_sessions_ {},
    preparing_data_sessions_ {},
    pending_data_sessions_ {},
    operation_to_channel_object {},
    m_active_data_plane_sessions { 0 },
    m_pending_data_plane_sessions { 0 },
    last_stats_global {},
    stats_backlogged { false }
{
    auto channel = grpc::CreateChannel (core_address, grpc::InsecureChannelCredentials ());
    stub_ = ControllerToUpper::NewStub (channel);

    Logging::log_info ("LocalControlApplication initialized." + core_address);
    initialize ();
    working_application_ = false;
    this->local_address = local_address;

    std::thread control_application_thread_t
        = std::thread (&LocalControlApplication::RunUpperIncomingServer, this);
    control_application_thread_t.detach ();

    ConnectToUpper ();
}

// LocalControlApplication parameterized constructor.
LocalControlApplication::LocalControlApplication (std::vector<std::string>* rules_ptr,
    const std::string& upper_address,
    const std::string& own_upper_address,
    const uint64_t& cycle_sleep_time) :
    ControlApplication { rules_ptr, cycle_sleep_time },
    data_sessions_ {},
    preparing_data_sessions_ {},
    pending_data_sessions_ {},
    operation_to_channel_object {},
    m_active_data_plane_sessions { 0 },
    m_pending_data_plane_sessions { 0 }
{
    auto channel = grpc::CreateChannel (upper_address, grpc::InsecureChannelCredentials ());
    stub_ = ControllerToUpper::NewStub (channel);

    Logging::log_info ("LocalControlApplication parameterized constructor with upper address: "
        + own_upper_address);

    initialize ();
    working_application_ = false;
    this->local_address = own_upper_address;

    std::thread control_application_thread_t
        = std::thread (&LocalControlApplication::RunUpperIncomingServer, this);
    control_application_thread_t.detach ();

    ConnectToUpper ();
}

// LocalControlApplication default destructor.
LocalControlApplication::~LocalControlApplication ()
{
    Logging::log_info ("LocalControlApplication: exiting ...\n");
}

////////////////////////////////////////////
///////// Register new sessions ////////////
////////////////////////////////////////////

// register_stage_session call. Register a new data plane.
void LocalControlApplication::register_stage_session (int socket_t)
{
    // Logging::log_info ("RegisterDataPlaneSession -- DataPlaneStage-" + std::to_string
    // (socket_t));

    pending_data_plane_sessions_lock_.lock ();
    pending_data_sessions_.emplace (std::make_unique<HandshakeSession> (socket_t));

    pending_data_plane_sessions_lock_.unlock ();

    this->m_pending_data_plane_sessions.fetch_add (1);
}

////////////////////////////////////////////
/////////////// Feedback Loop //////////////
////////////////////////////////////////////

// operator call. Used to initiate the control application feedback loop execution.
void LocalControlApplication::operator() ()
{
    this->execute_feedback_loop ();
}

// stop_feedback_loop call. Stops the feedback loop from executing.
void LocalControlApplication::stop_feedback_loop ()
{
    server->Shutdown ();
    working_application_ = false;
    while (!pending_data_sessions_.empty ()) {
        pending_data_sessions_.front ()->RemoveSession ();
        pending_data_sessions_.pop ();
    }
    for (auto const& data_session : data_sessions_) {
        data_session.second->RemoveSession ();
    }
}

// Benchmark counters (latencies in microseconds).
auto leaf_all_collect_time_aggregated = 0;
auto leaf_collect_time_aggregated = 0;
auto leaf_all_enforce_time_aggregated = 0;
auto leaf_enforce_time_aggregated = 0;
static int rounds_scalability = max_rounds;
static int rounds_counter = 0;

// log_stats call. Log control application stats.
void LocalControlApplication::log_stats ()
{
    Logging::log_info ("LocalControlApplication::Max number of rounds: "
        + std::to_string (max_rounds) + " rounds");
    Logging::log_info ("LocalControlApplication::Number of rounds: "
        + std::to_string (rounds_counter) + " rounds");
    Logging::log_info ("LocalControlApplication::Total Latency: "
        + std::to_string (leaf_all_collect_time_aggregated + leaf_all_enforce_time_aggregated)
        + " [µs]");
    Logging::log_info ("LocalControlApplication::All Collect Latency: "
        + std::to_string (leaf_all_collect_time_aggregated) + " [µs]");
    Logging::log_info ("LocalControlApplication::Collect Latency: "
        + std::to_string (leaf_collect_time_aggregated) + " [µs]");
    Logging::log_info ("LocalControlApplication::All Enforce Latency: "
        + std::to_string (leaf_all_enforce_time_aggregated) + " [µs]");
    Logging::log_info ("LocalControlApplication::Enforce Latency: "
        + std::to_string (leaf_enforce_time_aggregated) + " [µs]");
}

// execute_feedback_loop call. Executes feedback loop.
void LocalControlApplication::execute_feedback_loop ()
{
    Logging::log_debug ("LocalControlApplication::ExecuteFeedbackLoop");
    PStatus status = PStatus::Error ();

    while (!working_application_.load ()) {
        std::this_thread::sleep_for (milliseconds (100));
    }

    // wait for a data plane stage to connect
    while (working_application_.load () && this->m_pending_data_plane_sessions.load () == 0) {
        std::this_thread::sleep_for (milliseconds (100));
    }

    auto start_time = std::chrono::system_clock::now ();
    auto time_threshold
        = std::chrono::seconds (max_time_seconds); // Set your desired time threshold here

    // while existing data plane stage connections (active or pending), execute the feedback loop
    while (working_application_.load () && rounds_scalability > 0)
    /*while (working_application_.load () && rounds_scalability > 0
-        && (this->m_pending_data_plane_sessions.load () > 0
-            || this->m_active_data_plane_sessions.load () > 0)) {*/
    {
        // if exists pending sessions, perform the Session Handshake
        while (this->m_pending_data_plane_sessions.load () > 0) {
            // execute session handshake
            handle_data_plane_sessions ();
            std::this_thread::sleep_for (milliseconds (100));
        }

        auto start_collect = std::chrono::system_clock::now ();
        collect_global_all_statistics ();
        auto end_collect = std::chrono::system_clock::now ();

        auto collect_time
            = std::chrono::duration_cast<std::chrono::microseconds> (end_collect - start_collect)
                  .count ();

        // Old version: the unsigned subtraction wraps around when collecting takes longer than
        // the cycle, resulting in a (practically) endless sleep.
        // std::this_thread::sleep_for (microseconds (m_feedback_loop_sleep_time - collect_time));
        auto remaining_time = static_cast<int64_t> (m_feedback_loop_sleep_time) - collect_time;
        if (remaining_time > 0) {
            std::this_thread::sleep_for (microseconds (remaining_time));
        }

        // Remove here
        // this->sleep ();

        auto current_time = std::chrono::system_clock::now ();
        if (current_time - start_time > time_threshold) {
            Logging::log_info (
                "LocalControlApplication::Time threshold exceeded. Stopping feedback loop.");
            working_application_ = false;
        }
    }

    log_stats ();

    working_application_ = false;

    // log message and end control loop
    Logging::log_info ("Exiting. No active connections.");
    _exit (EXIT_SUCCESS);
}

////////////////////////////////////////////
//////////////// Sleep /////////////////////
////////////////////////////////////////////

// sleep call. Used to make control application main thread wait for the next loop.
void LocalControlApplication::sleep ()
{
    std::this_thread::sleep_for (microseconds (this->m_feedback_loop_sleep_time));
}

//////////////////////////////////////////////
/// Local to upper controller communication ///
//////////////////////////////////////////////

// RunUpperIncomingServer call. Execute the server that receives requests from the upper
// controller.
void LocalControlApplication::RunUpperIncomingServer ()
{
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort (local_address, grpc::InsecureServerCredentials ());
    // Register "service" as the instance through which we'll communicate with clients.
    // In this case it corresponds to an *synchronous* service.
    builder.RegisterService (this);
    // Finally assemble the server.
    server = builder.BuildAndStart ();
    Logging::log_info ("LocalControlApplication: Server listening on " + local_address);

    // Wait for the server to shutdown.
    // Note that some other thread must be responsible for shutting down the server for this call to
    // ever return.
    server->Wait ();
}

// ConnectToUpper call. Connects to the upper controller.
Status LocalControlApplication::ConnectToUpper ()
{
    // Data we are sending to the server.
    ConnectRequest request;
    request.set_user_address (local_address);

    // Container for the data we expect from the server.
    ConnectReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->ConnectToUpper (&context, request, &reply);

    // Logging::log_info (
    //     "LocalControlApplication: Connect local controller request sent to global controller");

    if (status.ok ()) {
        // Logging::log_info ("LocalControlApplication: Connection successful");
    } else {
        Logging::log_info ("LocalControlApplication: Connection failed to connect controller");
        // std::cout << status.error_code () << ": " << status.error_message () << std::endl;
    }
    return status;
}

// ConnectStageToUpper call. Connect data plane stage to upper controller.
Status LocalControlApplication::ConnectStageToUpper (const std::string& stage_name,
    const std::string& stage_env)
{
    // Data we are sending to the server.
    StageInfoConnect request;
    request.set_local_address (local_address);
    request.set_stage_name (stage_name);
    request.set_stage_env (stage_env);

    // Container for the data we expect from the server.
    ConnectReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->ConnectStageToUpper (&context, request, &reply);

    Logging::log_info ("LocalControlApplication: Connect stage request sent to upper controller "
        + stage_name + " " + stage_env + ".");

    if (status.ok ()) {
        // Logging::log_info ("LocalControlApplication: Connection successful");
    } else {
        Logging::log_info (
            "LocalControlApplication: Connection failed to connect stage. Error code: "
            + std::to_string (status.error_code ()) + " : " + status.error_message ());

        int still_attempt = 50;
        while (still_attempt > 0 && !status.ok ()) {
            std::this_thread::sleep_for (microseconds (100000));
            ConnectReply reply_repeat;
            ClientContext context_repeat;
            status = stub_->ConnectStageToUpper (&context_repeat, request, &reply_repeat);
            still_attempt--;
        }

        if (!status.ok ()) {
            Logging::log_info (
                "LocalControlApplication: Final connection failed to connect stage. Error code: "
                + std::to_string (status.error_code ()) + " : " + status.error_message ());
        }

        // std::cout << status.error_code () << ": " << status.error_message () << std::endl;
    }

    return status;
}

//////////////////////////////////////////////
/// Upper to local controller communication ///
//////////////////////////////////////////////

// ControllerHandshake call. Controller handshake from the upper controller.
Status LocalControlApplication::ControllerHandshake (ServerContext* context,
    const controllers_grpc_interface::SimplifiedHandshakeRaw* request,
    controllers_grpc_interface::ACK* reply)
{
    Logging::log_info ("LocalControlApplication: Received local handshake from upper controller");

    for (auto& rule : request->rules ()) {
        Logging::log_info ("LocalControlApplication: " + rule);

        housekeeping_rules_ptr_->push_back (rule);

        std::vector<std::string> rule_tokens {};
        parse_rule (rule, &rule_tokens, '|');

        if (rule_tokens[2].compare ("create_channel") == 0) {
            auto channel_object = std::make_pair (std::stoi (rule_tokens[3]), 1);

            std::string operation = rule_tokens[6];
            if (rule_tokens[6] == "no_op") {
                operation = rule_tokens[7];
            }

            auto channels_objects = operation_to_channel_object.find (operation);

            if (channels_objects == operation_to_channel_object.end ()) {

                std::vector<std::pair<int, int>> new_channels_objects;
                new_channels_objects.push_back (channel_object);
                operation_to_channel_object.emplace (operation, new_channels_objects);
            } else {
                channels_objects->second.push_back (channel_object);
            }
        }
    }

    working_application_ = true;
    reply->set_m_message (1);
    return Status::OK;
}

// MarkStageReady call. Mark stage ready from upper controller.
Status LocalControlApplication::MarkStageReady (ServerContext* context,
    const controllers_grpc_interface::StageReadyRaw* request,
    controllers_grpc_interface::ACK* reply)
{
    std::lock_guard<std::mutex> lock (session_lock_);

    Logging::log_info ("LocalControlApplication: Mark stage ready " + request->stage_name_env ()
        + " from upper controller");

    auto stage = preparing_data_sessions_.extract (request->stage_name_env ());
    if (stage.empty ()) {
        reply->set_m_message (1);
        return Status::OK;
    }

    data_sessions_.insert (std::move (stage));

    preparing_data_sessions_.erase (request->stage_name_env ());

    reply->set_m_message (1);
    return Status::OK;
}

// CreateEnforcementRule call. Create enforcement rules from upper controller.
Status LocalControlApplication::CreateEnforcementRule (ServerContext* context,
    const controllers_grpc_interface::EnforcementRulesMult* request,
    controllers_grpc_interface::ACK* reply)
{
    Logging::log_debug (
        "LocalControlApplication: Received create enforcement rule from core controller to "
        + local_address + ".");

    std::lock_guard<std::mutex> lock (session_lock_);

    // TO-DO: Mudar para primeiro submeter para todos e só depois recolher resposta.
    auto start_enforce = std::chrono::system_clock::now ();

    Status status = Status::OK;

    auto timestamp
        = std::chrono::duration_cast<std::chrono::microseconds> (start_enforce.time_since_epoch ())
              .count ();

    std::string message = "enforce;" + std::to_string (timestamp) + ";"
        + std::to_string (rounds_counter) + ";" + local_address;

    for (auto& rule : request->rules ()) {

        for (auto& env_rate : rule.env_rates ()) {

            std::string individual_message = message + ";" + rule.m_stage_name () + "+"
                + std::to_string (env_rate.first) + ";" + rule.m_operation () + ":"
                + std::to_string (env_rate.second);

            // IF WANT ALL LOGGING ADD THIS BACK
            // Logging::log_info ("LocalControlApplication: " + individual_message);

            auto existing_channels = operation_to_channel_object.find (rule.m_operation ());

            if (existing_channels == operation_to_channel_object.end ()) {
                reply->set_m_message (0);
                status = Status::CANCELLED;
            } else {

                int total_channels = existing_channels->second.size ();
                long limit_per_channel = 0;
                if (total_channels > 0) {
                    limit_per_channel = std::floor (env_rate.second / total_channels);
                }

                for (auto& channel_objects : existing_channels->second) {

                    int channel_id = channel_objects.first;
                    int enforcement_object_id = channel_objects.second;

                    std::string enforcement_rule = std::to_string (CREATE_ENF_RULE) + "|"
                        + "0|" // rule-id
                        + std::to_string (channel_id) + "|" + std::to_string (enforcement_object_id)
                        + "|" + "drl" + "|" + "rate" + "|" + std::to_string (limit_per_channel);

                    Logging::log_debug ("LocalControlApplication: Received create enforcement rule "
                                        "from core controller to "
                        + local_address + " ->" + enforcement_rule);

                    auto s_enforce = std::chrono::system_clock::now ();
                    status = LocalPassthru (rule.m_stage_name () + "+"
                            + std::to_string (env_rate.first),
                        enforcement_rule);
                    auto e_enforce = std::chrono::system_clock::now ();
                    leaf_enforce_time_aggregated
                        += std::chrono::duration_cast<std::chrono::microseconds> (
                            e_enforce - s_enforce)
                               .count ();

                    if (status.ok ()) {
                        reply->set_m_message (1);
                    } else {
                        // reply->set_m_message(0);
                        // return status;
                        reply->set_m_message (1);
                        return Status::OK;
                    }
                }
            }
        }
    }

    auto all_end_enforce = std::chrono::system_clock::now ();
    leaf_all_enforce_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (all_end_enforce - start_enforce)
               .count ();

    rounds_scalability--;
    rounds_counter++;

    return status;
}

// CollectGlobalAllStatistics call. Collect Statistics request from upper controller.
Status LocalControlApplication::CollectGlobalAllStatistics (ServerContext* context,
    const controllers_grpc_interface::Empty* request,
    controllers_grpc_interface::StatsGlobalAllMap* reply)
{
    Logging::log_debug ("LocalControlApplication: Received collect statistics "
                        "request from core controller");

    std::lock_guard<std::mutex> lock (session_lock_);

    if (stats_backlogged.load () == false) {
        return Status::OK;
    } else {
        auto& stats_map = *reply->mutable_gl_stats ();
        std::string stage = last_stats_global.stage_name () + "+" + last_stats_global.stage_env ();
        stats_map[stage] = last_stats_global;
        stats_backlogged = false;
        return Status::OK;
    }
}

/*
Status LocalControlApplication::CollectGlobalAllStatistics (ServerContext* context,
    const controllers_grpc_interface::Empty* request,
    controllers_grpc_interface::StatsGlobalAllMap* reply)
{

    auto start_collect = std::chrono::system_clock::now ();

    auto timestamp
        = std::chrono::duration_cast<std::chrono::microseconds> (start_collect.time_since_epoch
()) .count ();

    std::lock_guard<std::mutex> lock (session_lock_);

    std::string rule = std::to_string (COLLECT_DETAILED_STATS) + "|"
        + std::to_string (COLLECT_GLOBAL_ALL_STATS) + "|";

    // submit requests to each DataPlaneSession's submission_queue
    for (auto const& data_session : data_sessions_) {
        // put request on DataPlaneSession::submission_queue
        data_session.second->SubmitRule (rule);
    }

    auto& stats_map = *reply->mutable_gl_stats ();

    std::list<std::string> sessions_to_delete;

    // collect requests from each DataPlaneSession's completion_queue
    for (auto const& data_session : data_sessions_) {

        // wait for request to be on DataPlaneSession::completion_queue
        std::unique_ptr<StageResponse> stats_ptr = data_session.second->GetResult ();

        // verify if pointer is valid
        if (stats_ptr != nullptr) {
            // convert StageResponse unique-ptr to StageResponseStatsKVS
            auto* response_ptr = dynamic_cast<StageResponseStatAll*> (stats_ptr.get ());

            std::string name = data_session.first;
            controllers_grpc_interface::StatsGlobalAll stats_global;

            if (response_ptr->all_total_rates == nullptr) {
                this->m_active_data_plane_sessions.fetch_sub (1);
                sessions_to_delete.push_back (name);
            } else {

                auto& data_plane_stats_map = *stats_global.mutable_all_rates ();

                for (auto const& op_rate : *response_ptr->all_total_rates.get ()) {
                    data_plane_stats_map[op_rate.first] = op_rate.second;
                }
            }
            auto stage_info = stage_info_detailed.find (name);
            if (stage_info == stage_info_detailed.end ()) {
                Logging::log_error (
                    "LocalControlApplication: Stage info not found for " + name + ".");
                continue;
            }
            auto stage_name = stage_info->second->m_stage_name;
            auto stage_env = stage_info->second->m_stage_env;

            stats_global.set_stage_name (stage_name);
            stats_global.set_stage_env (stage_env);

            stats_map[name] = stats_global;

        } else {

            //Should probably remove here
            // Logging::log_info ("LocalControlApplication: CollectGlobalStatistics ->"
            //                    "Connection error; disconnecting from instance-"
            //     + data_session.first);
            this->m_active_data_plane_sessions.fetch_sub (1);
        }
    }

    auto end_collect_b = std::chrono::system_clock::now ();
    leaf_collect_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (end_collect_b - start_collect)
               .count ();

    for (auto const& data_session : sessions_to_delete) {
        // Logging::log_info (
        //     "LocalControlApplication: Deleting session in data_sessions:" + data_session);
        stage_info_detailed.erase (data_session);
        data_sessions_.at (data_session)->RemoveSession ();
        data_sessions_.erase (data_session);
    }

    auto end_collect = std::chrono::system_clock::now ();
    leaf_all_collect_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (end_collect - start_collect)
               .count ();

    return Status::OK;
}*/

// collect_global_all_statistics call. Collects the statistics of all active data plane stages.
void LocalControlApplication::collect_global_all_statistics ()
{
    std::lock_guard<std::mutex> lock (session_lock_);

    auto start_collect = std::chrono::system_clock::now ();

    auto timestamp
        = std::chrono::duration_cast<std::chrono::microseconds> (start_collect.time_since_epoch ())
              .count ();

    std::string rule = std::to_string (COLLECT_DETAILED_STATS) + "|"
        + std::to_string (COLLECT_GLOBAL_ALL_STATS) + "|";

    // submit requests to each DataPlaneSession's submission_queue
    for (auto const& data_session : data_sessions_) {
        // put request on DataPlaneSession::submission_queue
        data_session.second->SubmitRule (rule);
    }

    std::list<std::string> sessions_to_delete;

    // collect requests from each DataPlaneSession's completion_queue
    for (auto const& data_session : data_sessions_) {

        // wait for request to be on DataPlaneSession::completion_queue
        std::unique_ptr<StageResponse> stats_ptr = data_session.second->GetResult ();

        // verify if pointer is valid
        if (stats_ptr != nullptr) {
            // convert StageResponse unique-ptr to StageResponseStatsKVS
            auto* response_ptr = dynamic_cast<StageResponseStatAll*> (stats_ptr.get ());

            std::string stage = data_session.first;
            controllers_grpc_interface::StatsGlobalAll stats_global;

            if (response_ptr->all_total_rates == nullptr) {
                this->m_active_data_plane_sessions.fetch_sub (1);
                sessions_to_delete.push_back (stage);
            } else {

                auto& data_plane_stats_map = *stats_global.mutable_all_rates ();

                for (auto const& op_rate : *response_ptr->all_total_rates.get ()) {
                    data_plane_stats_map[op_rate.first] = op_rate.second;
                }

                std::string message = "collect_local;" + stage + ";" + std::to_string (timestamp)
                    + ";" + std::to_string (rounds_counter) + ";";
                message += "meta_op:" + std::to_string (data_plane_stats_map["meta_op"]);
                message += ";data_op:" + std::to_string (data_plane_stats_map["data_op"]);
                Logging::log_info ("LocalControlApplication: " + message);
            }

            auto stage_info = stage_info_detailed.find (stage);
            if (stage_info == stage_info_detailed.end ()) {
                Logging::log_error (
                    "LocalControlApplication: Stage info not found for " + stage + ".");
                continue;
            }

            auto stage_name = stage_info->second->m_stage_name;
            auto stage_env = stage_info->second->m_stage_env;

            stats_global.set_stage_name (stage_name);
            stats_global.set_stage_env (stage_env);

            last_stats_global = stats_global;
            stats_backlogged = true;

        } else {
            // Should probably remove here
            Logging::log_info ("LocalControlApplication: Removing active data plane session.");
            this->m_active_data_plane_sessions.fetch_sub (1);
        }
    }

    for (auto const& data_session : sessions_to_delete) {
        Logging::log_debug (
            "LocalControlApplication: Deleting session in data_sessions:" + data_session);
        stage_info_detailed.erase (data_session);
        data_sessions_.at (data_session)->RemoveSession ();
        data_sessions_.erase (data_session);
    }
}

// CollectGlobalStatistics call. Collect Statistics request from upper controller.
Status LocalControlApplication::CollectGlobalStatistics (ServerContext* context,
    const controllers_grpc_interface::Empty* request,
    controllers_grpc_interface::StatsGlobalMap* reply)
{
    Logging::log_debug ("LocalControlApplication: Received collect statistics "
                        "request from core controller");

    auto start_collect = std::chrono::system_clock::now ();
    auto timestamp
        = std::chrono::duration_cast<std::chrono::microseconds> (start_collect.time_since_epoch ())
              .count ();

    std::string rule = std::to_string (COLLECT_DETAILED_STATS) + "|"
        + std::to_string (COLLECT_DATA_METADATA_STATS) + "|";

    // submit requests to each DataPlaneSession's submission_queue
    for (auto const& data_session : data_sessions_) {
        // put request on DataPlaneSession::submission_queue
        data_session.second->SubmitRule (rule);
    }

    auto& stats_map = *reply->mutable_gl_stats ();

    std::list<std::string> sessions_to_delete;

    // collect requests from each DataPlaneSession's completion_queue
    for (auto const& data_session : data_sessions_) {

        // wait for request to be on DataPlaneSession::completion_queue
        std::unique_ptr<StageResponse> stats_ptr = data_session.second->GetResult ();

        // verify if pointer is valid
        if (stats_ptr != nullptr) {
            // convert StageResponse unique-ptr to StageResponseStatsKVS
            auto* response_ptr = dynamic_cast<StageResponseStat*> (stats_ptr.get ());

            if (response_ptr->get_d_total_rate () == -1
                || response_ptr->get_m_total_rate () == -1) {
                // Logging::log_info ("LocalControlApplication: CollectGlobalStatistics ->"
                //                    "Connection error; disconnecting from instance-"
                //     + data_session.first);
                this->m_active_data_plane_sessions.fetch_sub (1);
                sessions_to_delete.push_back (data_session.first);
            }

            std::string name = data_session.first;

            controllers_grpc_interface::StatsGlobal stats_global;

            stats_global.set_m_data_total_rate (response_ptr->get_d_total_rate ());
            stats_global.set_m_metadata_total_rate (response_ptr->get_m_total_rate ());

            stats_map[name] = stats_global;

        } else {
            // Logging::log_info ("LocalControlApplication: CollectGlobalStatistics ->"
            //                    "Connection error; disconnecting from instance-"
            //     + data_session.first);
            this->m_active_data_plane_sessions.fetch_sub (1);
        }
    }

    auto end_collect_b = std::chrono::system_clock::now ();
    leaf_collect_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (end_collect_b - start_collect)
               .count ();

    for (auto const& data_session : sessions_to_delete) {
        // Logging::log_info (
        //     "LocalControlApplication: Deleting session in data_sessions:" + data_session);
        data_sessions_.at (data_session)->RemoveSession ();
        data_sessions_.erase (data_session);
    }

    auto end_collect = std::chrono::system_clock::now ();
    leaf_all_collect_time_aggregated
        += std::chrono::duration_cast<std::chrono::microseconds> (end_collect - start_collect)
               .count ();

    return Status::OK;
}

// CollectGlobalStatisticsAggregated call. Collect Statistics request from upper controller.
// It aggregates the statistics from several rounds.
Status LocalControlApplication::CollectGlobalStatisticsAggregated (ServerContext* context,
    const controllers_grpc_interface::Empty* request,
    controllers_grpc_interface::StatsGlobalMap* reply)
{
    Logging::log_debug ("LocalControlApplication: Received collect aggregated statistics "
                        "request from core controller");

    std::string rule = std::to_string (COLLECT_DETAILED_STATS) + "|"
        + std::to_string (COLLECT_DATA_METADATA_STATS) + "|";

    auto& stats_map = *reply->mutable_gl_stats ();

    for (int i = 0; i < AGGREGATE_COLLECT_ROUNDS; i++) {

        auto start = std::chrono::system_clock::now ();

        // submit requests to each DataPlaneSession's submission_queue
        for (auto const& data_session : data_sessions_) {
            // put request on DataPlaneSession::submission_queue
            data_session.second->SubmitRule (rule);
        }

        std::list<std::string> sessions_to_delete;

        // collect requests from each DataPlaneSession's completion_queue
        for (auto const& data_session : data_sessions_) {

            // wait for request to be on DataPlaneSession::completion_queue
            std::unique_ptr<StageResponse> stats_ptr = data_session.second->GetResult ();

            // verify if pointer is valid
            if (stats_ptr != nullptr) {
                // convert StageResponse unique-ptr to StageResponseStatsKVS
                auto* response_ptr = dynamic_cast<StageResponseStat*> (stats_ptr.get ());

                if (response_ptr->get_d_total_rate () == -1
                    || response_ptr->get_m_total_rate () == -1) {
                    Logging::log_error (
                        "LocalControlApplication: CollectGlobalStatisticsAggregated ->"
                        "Connection error; disconnecting from instance-"
                        + data_session.first);
                    this->m_active_data_plane_sessions.fetch_sub (1);
                    sessions_to_delete.push_back (data_session.first);
                } else {
                    std::string name = data_session.first;

                    if (!stats_map.contains (name)) {
                        controllers_grpc_interface::StatsGlobal stats_global;
                        stats_global.set_m_data_total_rate (response_ptr->get_d_total_rate ());
                        stats_global.set_m_metadata_total_rate (response_ptr->get_m_total_rate ());
                        stats_map[name] = stats_global;
                    } else {
                        auto stats_global = stats_map.at (name);
                        double previous_value = stats_global.m_data_total_rate ();
                        stats_global.set_m_data_total_rate (
                            (double)(response_ptr->get_d_total_rate () + previous_value * i)
                            / (i + 1));
                        previous_value = stats_global.m_metadata_total_rate ();
                        stats_global.set_m_metadata_total_rate (
                            (double)(response_ptr->get_m_total_rate () + previous_value * i)
                            / (i + 1));
                        stats_map[name] = stats_global;
                    }
                }
            } else {
                Logging::log_error ("LocalControlApplication: CollectGlobalStatisticsAggregated ->"
                                    "Connection error; disconnecting from instance-"
                    + data_session.first);
                this->m_active_data_plane_sessions.fetch_sub (1);
            }
        }

        for (auto const& data_session : sessions_to_delete) {
            controllers_grpc_interface::StatsGlobal stats_global;
            stats_global.set_m_data_total_rate (-1);
            stats_global.set_m_metadata_total_rate (-1);
            stats_map[data_session] = stats_global;
            // Logging::log_info (
            //     "LocalControlApplication: Deleting session in data_sessions_:" +
            //     data_session);
            data_sessions_.erase (data_session);
        }

        auto end = std::chrono::system_clock::now ();

        if (i < 4) {
            std::this_thread::sleep_for (microseconds (this->m_feedback_loop_sleep_time
                - std::chrono::duration_cast<std::chrono::microseconds> (end - start).count ()));
        }
    }

    return Status::OK;
}

////////////////////////////////////////////
//////////// Auxiliary Functions ///////////
////////////////////////////////////////////

// initialize call. Initialize control application (currently does nothing).
void LocalControlApplication::initialize ()
{ }

// handle_data_plane_sessions call. Processes pending data plane sessions.
void LocalControlApplication::handle_data_plane_sessions ()
{
    std::lock_guard<std::mutex> lock (session_lock_);

    if (!pending_data_sessions_.empty ()) {
        Logging::log_debug ("LocalControlApplication: Data Plane Session Handshake");

        PStatus status = PStatus::Error ();

        pending_data_plane_sessions_lock_.lock ();
        auto handshake_session = std::move (pending_data_sessions_.front ());
        pending_data_sessions_.pop ();
        pending_data_plane_sessions_lock_.unlock ();

        m_pending_data_plane_sessions.fetch_sub (1);

        /*Start Session*/
        std::thread session_thread_t
            = std::thread (&HandshakeSession::StartSession, handshake_session.get ());
        session_thread_t.detach ();

        // invoke CallStageHandshake routine to acknowledge the stage's identifier
        //<stage_name, stage_env>
        std::unique_ptr<StageInfo> stage_identifier
            = this->call_stage_handshake (handshake_session.get ());

        handshake_session->RemoveSession ();

        if (stage_identifier == nullptr) {
            Logging::log_error (
                "LocalControlApplication: Stage Handshake not established. No StageInfo.");

            return;
        }
        // Logging::log_info ("LocalControlApplication: Stage Handshake with "
        //     + stage_identifier->m_stage_name + "+" + stage_identifier->m_stage_env);

        if (!stage_identifier->m_stage_name.empty ()) {

            // submit housekeeping rules to the data plane stage
            std::string stage_env
                = stage_identifier->m_stage_name + "+" + stage_identifier->m_stage_env;

            int installed_rules = this->submit_housekeeping_rules (stage_env);

            // Logging::log_debug ("LocalControlApplication: installed rules ... ("
            //     + std::to_string (installed_rules) + ") in (" + stage_env + ")");

            if (installed_rules == housekeeping_rules_ptr_->size ()
                && mark_stage_ready (stage_env).isOk ()) {

                // Logging::log_debug ("LocalControlApplication: Connecting Stage to Global ("
                //     + stage_identifier->m_stage_name + ")");

                Status status = ConnectStageToUpper (stage_identifier->m_stage_name,
                    stage_identifier->m_stage_env);

                if (status.ok ()) {
                    m_active_data_plane_sessions.fetch_add (1);

                    stage_info_detailed.emplace (stage_env, std::move (stage_identifier));

                    // Logging::log_debug ("DataPlaneSessionHandshake with DataPlaneStage-"
                    //     + stage_identifier->m_stage_name + " successfully established.");
                } else {
                    Logging::log_error ("DataPlaneSessionHandshake with DataPlaneStage-"
                        + stage_identifier->m_stage_name + " not established.");

                    std::this_thread::sleep_for (milliseconds (100));
                }
            } else {
                Logging::log_error ("DataPlaneSessionHandshake with DataPlaneStage-"
                    + stage_identifier->m_stage_name + " not established.");

                preparing_data_sessions_.at (stage_env)->RemoveSession ();
                preparing_data_sessions_.erase (stage_env);
            }
        }
    }
}

// call_stage_handshake call. Submits STAGE_HANDSHAKE rule and housekeeping rules to data plane
// stage.
std::unique_ptr<StageInfo> LocalControlApplication::call_stage_handshake (
    HandshakeSession* handshake_session)
{
    // create STAGE_HANDSHAKE request
    std::string rule = std::to_string (STAGE_HANDSHAKE) + "|";
    // put request on DataPlaneSession::submission_queue

    handshake_session->SubmitRule (rule);

    // wait for request to be on DataPlaneSession::completion_queue
    std::unique_ptr<StageResponse> response_obj = handshake_session->GetResult ();
    // convert StageResponse to Handshake object

    auto* handshake_ptr = dynamic_cast<StageResponseHandshake*> (response_obj.get ());

    std::unique_ptr<StageInfo> all_stage_info = std::make_unique<StageInfo> ();

    if (handshake_ptr == nullptr || handshake_ptr->get_stage_name ().empty ()) {
        Logging::log_error (
            "LocalControlApplication: Stage Handshake not established. No stage name.");
        return nullptr;
    } else {

        // Logging::log_info ("LocalControlApplication: establishing UNIX connection with "
        //                    "data plane stage.");

        std::string socket_info;
        PStatus status = fill_socket_info (handshake_ptr, socket_info);

        std::string stage_name_env
            = handshake_ptr->get_stage_name () + "+" + handshake_ptr->get_stage_env ();

        // Logging::log_info ("LocalControlApplication: StageHandshake <" + socket_info + ">");

        int port = -1;

        this->preparing_data_sessions_[stage_name_env]
            = std::make_unique<DataPlaneSession> (socket_info.c_str ());

        std::thread session_thread_t = std::thread (&DataPlaneSession::StartSession,
            (this->preparing_data_sessions_[stage_name_env]).get ());
        session_thread_t.detach ();

        /*Send info about the address and port to connect to*/
        rule = std::to_string (STAGE_HANDSHAKE_INFO) + "|" + socket_info + "|"
            + std::to_string (port) + "|";
        handshake_session->SubmitRule (rule);

        std::unique_ptr<StageResponse> response_obj = handshake_session->GetResult ();
        // convert StageResponse to Handshake object
        // auto* handshake_ptr = dynamic_cast<StageResponseACK*> (response_obj.get ());

        all_stage_info->m_stage_name = handshake_ptr->get_stage_name ();
        all_stage_info->m_stage_env = handshake_ptr->get_stage_env ();

        // Logging message
        /*Logging::log_info ("LocalControlApplication: StageHandshake <"
            + handshake_ptr->get_stage_name () + ", "
            + std::to_string (handshake_ptr->get_stage_pid ()) + ", "
            + std::to_string (handshake_ptr->get_stage_ppid ()) + ">");*/
    }

    // return const value of the stage identifier's name
    return all_stage_info;
}

// submit_housekeeping_rules call. Submits housekeeping rules to data plane stage.
int LocalControlApplication::submit_housekeeping_rules (const std::string& stage_name_env) const
{
    PStatus status = PStatus::Error ();
    int rule_counter = 0;
    int valid_housekeeping_rules = 0;

    // read rules from housekeeping_rules_ptr and submit to the SubmissionQueue
    for (const auto& value : *housekeeping_rules_ptr_) {

        // submit rule to the SubmissionQueue
        status = preparing_data_sessions_.at (stage_name_env)->SubmitRule (value);

        // update the counter of submitted rules
        if (status.isOk ()) {
            rule_counter++;
        } else {
            break;
        }
    }

    // read responses from the CompletionQueue
    for (int i = 0; i < rule_counter; i++) {
        // get rules from CompletionQueue and cast them to a StageResponseACK object
        std::unique_ptr<StageResponse> response
            = preparing_data_sessions_.at (stage_name_env)->GetResult ();
        auto* ack_ptr = dynamic_cast<StageResponseACK*> (response.get ());

        // validate data plane stage response
        if (ack_ptr != nullptr) {
            if (ack_ptr->ACKValue () == static_cast<int> (AckCode::ok)) {
                valid_housekeeping_rules++;
            } else {
                return 0;
            }
        }
    }

    return valid_housekeeping_rules;
}

// mark_stage_ready call. Submits STAGE_READY to data plane stage.
PStatus LocalControlApplication::mark_stage_ready (const std::string& stage_name_env) const
{
    PStatus status = PStatus::Error ();

    std::string rule = std::to_string (STAGE_READY) + "|";

    status = preparing_data_sessions_.at (stage_name_env)->SubmitRule (rule);

    std::unique_ptr<StageResponse> response = nullptr;
    if (status.isOk ()) {
        std::unique_ptr<StageResponse> response
            = preparing_data_sessions_.at (stage_name_env)->GetResult ();
        auto* ack_ptr = dynamic_cast<StageResponseACK*> (response.get ());

        // validate data plane stage response
        if (ack_ptr != nullptr) {
            if (ack_ptr->ACKValue () == static_cast<int> (AckCode::ok)) {
                status = PStatus::OK ();
            }
        }
    }
    return status;
}

// LocalPassthru call. General function to submit rules to data plane stages.
Status LocalControlApplication::LocalPassthru (const std::string stage_name_env,
    const std::string rule)
{

    if (data_sessions_.find (stage_name_env) == data_sessions_.end ()) {
        return Status::CANCELLED;
    }

    this->data_sessions_[stage_name_env]->SubmitRule (rule);

    std::unique_ptr<StageResponse> ack_ptr = this->data_sessions_[stage_name_env]->GetResult ();

    // verify if pointer is valid
    if (ack_ptr != nullptr) {
        // convert StageResponse unique-ptr to StageResponseStatsKVS
        auto* response_ptr = dynamic_cast<StageResponseACK*> (ack_ptr.get ());

        if (response_ptr->ACKValue () == 1) {
            return Status::OK;
        } else {
            return Status::CANCELLED;
        }
    }

    return Status::CANCELLED;
}

// fill_socket_info call. Defines a new individual socket for data plane stage.
PStatus LocalControlApplication::fill_socket_info (StageResponseHandshake* handshake_ptr,
    std::string& socket_info)
{
    std::stringstream stream;

    PStatus status = PStatus::OK ();

    stream << "/tmp/";
    if (!handshake_ptr->get_stage_name ().empty ()) {
        stream << handshake_ptr->get_stage_name () << "_";
    } else {
        status = PStatus::Error ();
        Logging::log_error (
            "LocalControlApplication: StageResponseHandshake stage_name field is empty.");
    }

    if (!handshake_ptr->get_stage_env ().empty ()) {
        stream << handshake_ptr->get_stage_env () << "_";
    } else {
        status = PStatus::Error ();
        Logging::log_error (
            "LocalControlApplication: StageResponseHandshake stage_env field is empty.");
    }

    if (handshake_ptr->get_stage_pid () > 0) {
        stream << handshake_ptr->get_stage_pid () << "_";
    } else {
        status = PStatus::Error ();
        Logging::log_error (
            "LocalControlApplication: StageResponseHandshake stage_pid field is empty.");
    }

    if (handshake_ptr->get_stage_ppid () > 0) {
        stream << handshake_ptr->get_stage_ppid ();
    } else {
        status = PStatus::Error ();
        Logging::log_error (
            "LocalControlApplication: StageResponseHandshake stage_ppid field is empty.");
    }

    stream << ".socket";

    socket_info = stream.str ();

    return status;
}

// parse_rule call. Parses a rule into tokens using char c as delimiter.
void LocalControlApplication::parse_rule (const std::string& rule,
    std::vector<std::string>* tokens,
    const char c)
{
    size_t start;
    size_t end = 0;

    while ((start = rule.find_first_not_of (c, end)) != std::string::npos) {
        end = rule.find (c, start);
        tokens->push_back (rule.substr (start, end - start));
    }
}

} // namespace cheferd
