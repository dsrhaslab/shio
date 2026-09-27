/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/controller/cluster_controller.hpp"

#include "shio/utils/rules_file_parser.hpp"

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace shio {

// ClusterController parameterized constructor.
ClusterController::ClusterController (const std::string& upper_address,
    const std::string& own_upper_address,
    std::unordered_map<int, std::unordered_map<std::string, uint64_t>> policies,
    int total_jobs,
    std::unordered_map<std::string, int> jobs_name_to_priority,
    ControllerRole controller_role) :
    ControlApplication {},
    controller_sessions_ {},
    pending_controller_sessions_ {},
    pending_controller_sessions_lock_ {},
    m_active_controller_sessions { 0 },
    m_pending_controller_sessions { 0 },
    jobs_name_to_priority { jobs_name_to_priority },
    own_address { own_upper_address },
    controller_role { controller_role }
{
    auto channel = grpc::CreateChannel (upper_address, grpc::InsecureChannelCredentials ());
    auto stub_ = ControllerToUpper::NewStub (channel);

    if (controller_role == ControllerRole::PRIMARY) {
        Logging::log_info ("ClusterController: Initializing as primary controller.");
        ConnectToUpper (std::move (stub_));
        m_upper_core_connection_manager = new UpperCoreConnectionManager (own_address, this);
    } else if (controller_role == ControllerRole::SECONDARY) {
        Logging::log_info ("ClusterController: Initializing as secondary controller.");
        // ConnectToUpper (std::move(stub_));
        // The secondary controller does not need to connect to the upper controller.
    }

    m_core_control_application_ptr
        = new CoreControlApplication (option_default_core_control_application_sleep,
            policies,
            total_jobs,
            jobs_name_to_priority);

    m_passthru_control_application_ptr
        = new PassthroughControlApplication (own_address, option_default_control_application_sleep);

    m_passthru_control_application_ptr->register_communication_channel (channel);
}

// ClusterController default destructor.
ClusterController::~ClusterController ()
{
    Logging::log_info ("ClusterController: exiting ...\n");
}

// ConnectToUpper call. Connects to the upper controller.
Status ClusterController::ConnectToUpper (std::unique_ptr<ControllerToUpper::Stub> stub_)
{
    // Data we are sending to the server.
    ConnectRequest request;
    request.set_user_address (own_address);

    // Container for the data we expect from the server.
    ConnectReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->ConnectToUpper (&context, request, &reply);

    // Logging::log_info (
    //     "ClusterController: Connect local controller request sent to global controller");

    if (status.ok ()) {
        // Logging::log_info ("ClusterController: Connection successful");
    } else {
        Logging::log_info ("ClusterController: Connection failed");
        // std::cout << status.error_code () << ": " << status.error_message () << std::endl;
    }
    return status;
}

////////////////////////////////////////////
/////////////// Feedback Loop //////////////
////////////////////////////////////////////

// operator call. Used to initiate the controller feedback loop execution.
void ClusterController::operator() ()
{
    this->execute_feedback_loop ();
}

// set_as_primary_controller call. Marks controller as primary.
void ClusterController::set_as_primary_controller ()
{
    Logging::log_info ("ClusterController: Setting as primary controller.");

    for (auto const& [controller_address, _] : controller_sessions_) {
        Logging::log_info (
            "ClusterController: Registering new controller - " + controller_address + ".");

        this->controller_sessions_.insert_or_assign (controller_address,
            std::make_unique<LocalControllerSession> (controller_address));

        std::thread session_thread_t = std::thread (&LocalControllerSession::StartSession,
            controller_sessions_.at (controller_address).get (),
            controller_address);

        session_thread_t.detach ();

        m_active_controller_sessions.fetch_add (1);
    }

    collect_stages_info ();

    m_core_control_application_ptr->set_as_primary_controller ();
    m_passthru_control_application_ptr->set_as_primary_controller ();

    // Open for communication from upper controller.
    m_upper_core_connection_manager = new UpperCoreConnectionManager (own_address, this);

    controller_role = ControllerRole::PRIMARY;
}

// stop_feedback_loop call. Stops the feedback loop from executing.
void ClusterController::stop_feedback_loop ()
{
    working_application_ = false;
}

// execute_feedback_loop call. Executes feedback loop.
void ClusterController::execute_feedback_loop ()
{
    // Logging::log_debug ("ClusterController::ExecuteFeedbackLoop");
    PStatus status = PStatus::Error ();
    working_application_ = true;

    // wait for a local controller to connect
    while (working_application_.load () && this->m_pending_controller_sessions.load () == 0) {
        std::this_thread::sleep_for (milliseconds (100));
    }

    auto start_time = std::chrono::system_clock::now ();
    auto time_threshold
        = std::chrono::seconds (max_time_seconds); // Set your desired time threshold here

    // while existing controller connections (active or pending), execute the feedback loop
    while (working_application_.load ()
        && (this->m_pending_controller_sessions.load () > 0
            || this->m_active_controller_sessions.load () > 0)) {
        // if exists pending sessions, perform the Session Handshake
        while (this->m_pending_controller_sessions.load () > 0) {
            // execute session handshake
            handle_controller_sessions ();
            std::this_thread::sleep_for (milliseconds (1));
        }

        this->sleep ();

        if (!m_core_control_application_ptr->feedback_loop_is_active ()) {
            if (m_passthru_control_application_ptr->feedback_loop_is_active ()) {
                m_passthru_control_application_ptr->stop_feedback_loop ();
            }
            working_application_ = false;
            Logging::log_info ("ClusterController::Exiting.");
        } else if (!m_passthru_control_application_ptr->feedback_loop_is_active ()) {
            if (m_core_control_application_ptr->feedback_loop_is_active ()) {
                m_core_control_application_ptr->stop_feedback_loop ();
            }
            working_application_ = false;
            Logging::log_info ("ClusterController::Exiting.");
        }

        // Check if the time threshold has passed
        auto current_time = std::chrono::system_clock::now ();
        if (current_time - start_time > time_threshold) {
            Logging::log_info (
                "ClusterController::Time threshold exceeded. Stopping feedback loop.");
            working_application_ = false;
            if (m_core_control_application_ptr->feedback_loop_is_active ())
                m_core_control_application_ptr->stop_feedback_loop ();
            if (m_passthru_control_application_ptr->feedback_loop_is_active ())
                m_passthru_control_application_ptr->stop_feedback_loop ();
        }
    }

    m_core_control_application_ptr->log_stats ();
    m_passthru_control_application_ptr->log_stats ();

    for (auto const& controller_session : controller_sessions_) {
        controller_session.second->RemoveSession ();
    }

    working_application_ = false;

    // log message and end control loop
    Logging::log_info ("Exiting. No active connections.");
    _exit (EXIT_SUCCESS);
}

////////////////////////////////////////////
//////////////// Sleep /////////////////////
////////////////////////////////////////////

// sleep call. Used to make control application main thread wait for the next loop.
void ClusterController::sleep ()
{
    std::this_thread::sleep_for (microseconds (this->m_feedback_loop_sleep_time));
}

////////////////////////////////////////////
///////// Register new sessions ////////////
////////////////////////////////////////////

// register_controller_session call. Register a new controller.
void ClusterController::register_controller_session (const std::string& controller_address)
{
    Logging::log_debug ("ClusterController: RegisterControllerSession -- " + controller_address);

    if (controller_role == ControllerRole::PRIMARY) {
        std::lock_guard<std::mutex> lock (pending_controller_sessions_lock_);
        pending_controller_sessions_.emplace (controller_address);
        m_pending_controller_sessions.fetch_add (1, std::memory_order_relaxed);
    } else if (controller_role == ControllerRole::SECONDARY) {
        Logging::log_info ("ClusterController: Register controller as secondary controller.");
        std::lock_guard<std::mutex> lock (pending_controller_sessions_lock_);
        controller_sessions_.try_emplace (controller_address, nullptr);
    }
}

// handle_controller_sessions call. Processes a pending controller session.
void ClusterController::handle_controller_sessions ()
{
    Logging::log_debug ("ClusterController: Handle controller sessions");
    pending_controller_sessions_lock_.lock ();
    if (pending_controller_sessions_.empty ()) {
        pending_controller_sessions_lock_.unlock ();
        return;
    }
    std::string new_controller_address = pending_controller_sessions_.front ();
    pending_controller_sessions_.pop ();
    pending_controller_sessions_lock_.unlock ();

    m_pending_controller_sessions.fetch_sub (1);

    this->controller_sessions_.emplace (new_controller_address,
        std::make_unique<LocalControllerSession> (new_controller_address));

    // this->stages_to_leafs.emplace (leaf_controller_address, std::vector<std::string> {});

    std::thread session_thread_t = std::thread (&LocalControllerSession::StartSession,
        controller_sessions_.at (new_controller_address).get (),
        new_controller_address);
    session_thread_t.detach ();

    PStatus status = this->controller_handshake (new_controller_address);

    m_active_controller_sessions.fetch_add (1);
}

// controller_handshake call. Handles the handshake with a local controller.
PStatus ClusterController::controller_handshake (const std::string& controller_address)
{
    PStatus status = PStatus::Error ();

    // Only perform handshake if the controller is primary
    if (controller_sessions_[controller_address] != nullptr
        && controller_role == ControllerRole::PRIMARY) {
        // invoke CallStageHandshake routine to acknowledge the stage's identifier
        status = this->call_controller_handshake (controller_address);
    } else {
        Logging::log_info ("Controller Handshake: session (" + controller_address + ") is null.");
    }

    return status;
}

// call_controller_handshake call. Submits LOCAL_HANDSHAKE rule to a local controller.
PStatus ClusterController::call_controller_handshake (const std::string& controller_address)
{
    PStatus status = PStatus::Error ();
    // create LOCAL_HANDSHAKE request
    std::string rule = std::to_string (LOCAL_HANDSHAKE) + "|";
    // put request on LocalPlaneSession::submission_queue

    for (const auto& specific_rule : *housekeeping_rules_ptr_) {
        rule += ":" + specific_rule;
    }

    controller_sessions_[controller_address]->SubmitRule (rule);

    // wait for request to be on LocalPlaneSession::completion_queue
    std::unique_ptr<StageResponse> resp_t = controller_sessions_[controller_address]->GetResult ();
    // convert StageResponse to Handshake object
    auto* ack_ptr_t = dynamic_cast<StageResponseACK*> (resp_t.get ());

    if (ack_ptr_t != nullptr) {
        if (ack_ptr_t->ACKValue () == static_cast<int> (AckCode::ok)) {
            status = PStatus::OK ();
        }
    }

    return status;
}

// register_stage_session call. Register a new data plane stage.
void ClusterController::register_stage_session (const StageInfoConnect* request, bool rebuild)
{
    auto controller_session = controller_sessions_[request->local_address ()];

    // We don't need separate sessions because one local can only be associated with either passthru
    // or core controller.
    if (jobs_name_to_priority.find (request->stage_name ()) == jobs_name_to_priority.end ()) {
        m_passthru_control_application_ptr->register_stage_session (request,
            controller_session,
            rebuild);
    } else {
        m_core_control_application_ptr->register_stage_session (request,
            controller_session,
            rebuild);
    }
}

// collect_stages_info call. Collects and registers the stages of all local controllers.
void ClusterController::collect_stages_info ()
{
    for (const auto& [controller_address, session] : controller_sessions_) {
        Logging::log_info ("ClusterController: Collecting stage info from controller session at "
            + controller_address);
        session->SubmitRule (std::to_string (COLLECT_DETAILED_STATS) + "|"
            + std::to_string (COLLECT_GLOBAL_ALL_STATS) + "|");
    }

    for (const auto& [controller_address, session] : controller_sessions_) {

        auto stats_ptr = session->GetResult ();

        if (!stats_ptr) {
            Logging::log_error (
                "ClusterController: No statistics received from controller session at "
                + controller_address);
            continue;
        }

        auto* response_ptr = dynamic_cast<StageResponseStats*> (stats_ptr.get ());

        if (!response_ptr || response_ptr->m_stats_ptr->empty ()) {
            Logging::log_error (
                "ClusterController: No statistics found in response from controller session at "
                + controller_address);
            continue;
        }

        for (const auto& stage_info : (*response_ptr->m_stats_ptr)) {
            auto* global_stat_ptr = dynamic_cast<StageResponseStatAll*> (stage_info.second.get ());

            if (!global_stat_ptr || !global_stat_ptr->all_total_rates) {

            } else {
                auto stage_name_env = stage_info.first;
                StageInfoConnect stage_info_connect;
                stage_info_connect.set_stage_name (global_stat_ptr->stage_name);
                stage_info_connect.set_stage_env (global_stat_ptr->stage_env);
                stage_info_connect.set_local_address (controller_address);

                register_stage_session (&stage_info_connect, true);
                Logging::log_info ("ClusterController: Registered stage session for "
                    + global_stat_ptr->stage_name + " in environment " + global_stat_ptr->stage_env
                    + " from controller session at " + controller_address);
            }
        }
    }
}

//////////////////////////////////////////////
/// Upper to lower controller communication ///
//////////////////////////////////////////////

// ControllerHandshake call. Controller handshake from the upper controller.
Status ClusterController::ControllerHandshake (ServerContext* context,
    const controllers_grpc_interface::SimplifiedHandshakeRaw* request,
    controllers_grpc_interface::ACK* reply)
{
    Logging::log_info ("ClusterController: Received local handshake from core controller");

    set_housekeeping_rules (request);

    reply->set_m_message (1);
    return Status::OK;
}

// set_housekeeping_rules call. Stores housekeeping rules and initializes control applications.
void ClusterController::set_housekeeping_rules (
    const controllers_grpc_interface::SimplifiedHandshakeRaw* request)
{
    Logging::log_info ("ClusterController: Setting housekeeping rules");
    for (auto& rule : request->rules ()) {
        Logging::log_info ("ClusterController: " + rule);

        housekeeping_rules_ptr_->push_back (rule);
    }

    std::unordered_set<std::string> active_ops;

    for (const auto& specific_rule : *housekeeping_rules_ptr_) {
        std::vector<std::string> tokens {};

        parse_rule_with_break (specific_rule, &tokens);

        if (tokens[2] == "create_channel") {
            if (tokens[6] == "no_op") {
                active_ops.insert (tokens[7]);
            } else {
                active_ops.insert (tokens[6]);
            }
        }
    }

    m_passthru_control_application_ptr->initialize ();
    m_core_control_application_ptr->initialize (active_ops);
}

//////////////////////////////////////////////
/// Core to local controller communication ///
//////////////////////////////////////////////

// MarkStageReady call. Mark stage ready from upper controller.
Status ClusterController::MarkStageReady (ServerContext* context,
    const controllers_grpc_interface::StageReadyRaw* request,
    controllers_grpc_interface::ACK* reply)
{
    return m_passthru_control_application_ptr->MarkStageReady (context, request, reply);
}

// CreateEnforcementRule call. Create enforcement from core controller.
Status ClusterController::CreateEnforcementRule (ServerContext* context,
    const controllers_grpc_interface::EnforcementRulesMult* request,
    controllers_grpc_interface::ACK* reply)
{
    return m_passthru_control_application_ptr->CreateEnforcementRule (context, request, reply);
}

// EnforceSystemLimit call. Enforce system limit from core controller.
Status ClusterController::EnforceSystemLimit (grpc::ServerContext* context,
    const controllers_grpc_interface::OperationsLimits* request,
    controllers_grpc_interface::ACK* reply)
{
    return m_core_control_application_ptr->EnforceSystemLimit (context, request, reply);
}

// CollectGlobalAllStatistics call. Collect statistics request from upper controller.
Status ClusterController::CollectGlobalAllStatistics (ServerContext* context,
    const controllers_grpc_interface::Empty* request,
    controllers_grpc_interface::StatsGlobalAllMap* reply)
{
    return m_passthru_control_application_ptr->CollectGlobalAllStatistics (context, request, reply);
}

// CollectCoreControllerStatistics call. Collect aggregated statistics from core controller.
Status ClusterController::CollectCoreControllerStatistics (ServerContext* context,
    const controllers_grpc_interface::Empty* request,
    controllers_grpc_interface::StatsGlobalAggregatedMap* reply)
{
    // Logging::log_debug ("ClusterController::CollectAggregatedAllStatistics");
    return m_core_control_application_ptr->CollectCoreControllerStatistics (reply);
}

// Move to utils in latter version

// parse_rule_with_break call. Parses a rule into tokens using '|' as delimiter.
void ClusterController::parse_rule_with_break (const std::string& rule,
    std::vector<std::string>* tokens)
{
    size_t start;
    size_t end = 0;

    while ((start = rule.find_first_not_of ('|', end)) != std::string::npos) {
        end = rule.find ('|', start);
        tokens->push_back (rule.substr (start, end - start));
    }
}

} // namespace shio
