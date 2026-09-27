/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/controller/global_controller.hpp"

#include "shio/utils/rules_file_parser.hpp"

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace shio {

// GlobalController parameterized constructor.
GlobalController::GlobalController (std::vector<std::string>* rules_ptr,
    std::unordered_map<std::string, uint64_t> op_system_limits,
    std::unordered_map<int, std::unordered_map<std::string, uint64_t>> policies,
    int total_jobs,
    std::unordered_map<std::string, int> jobs_name_to_priority,
    ControllerRole controller_role) :
    ControlApplication { rules_ptr },
    core_controllers_sessions_ {},
    supervisor_controllers_session_ {},
    pending_controller_sessions_ {},
    pending_controller_sessions_lock_ {},
    m_active_controller_sessions { 0 },
    m_pending_controller_sessions { 0 },
    controller_role { controller_role }
{
    m_core_control_application_ptr
        = new CoreControlApplication (option_default_core_control_application_sleep,
            policies,
            total_jobs,
            jobs_name_to_priority);

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

    m_core_control_application_ptr->initialize (active_ops);

    m_supervisor_control_application_ptr
        = new SupervisorControlApplication (housekeeping_rules_ptr_,
            option_default_supervisor_control_application_sleep,
            op_system_limits,
            policies,
            m_core_control_application_ptr);
    m_supervisor_control_application_ptr->initialize ();
}

// GlobalController default destructor.
GlobalController::~GlobalController ()
{
    Logging::log_info ("GlobalController: exiting ...\n");
}

////////////////////////////////////////////
/////////////// Feedback Loop //////////////
////////////////////////////////////////////

// operator call. Used to initiate the control application feedback loop execution.
void GlobalController::operator() ()
{
    this->execute_feedback_loop ();
}

// set_as_primary_controller call. Marks controller as primary.
void GlobalController::set_as_primary_controller ()
{
    Logging::log_info ("GlobalController: Setting as primary controller.");

    collect_stages_info ();
    m_core_control_application_ptr->set_as_primary_controller ();
    m_supervisor_control_application_ptr->set_as_primary_controller ();

    controller_role = ControllerRole::PRIMARY;
}

// stop_feedback_loop call. Stops the feedback loop from executing.
void GlobalController::stop_feedback_loop ()
{
    working_application_ = false;
}

// execute_feedback_loop call. Executes feedback loop.
void GlobalController::execute_feedback_loop ()
{
    // Logging::log_debug ("GlobalController::ExecuteFeedbackLoop");
    PStatus status = PStatus::Error ();
    working_application_ = true;

    int l_instances = 1;

    while (
        working_application_.load () && this->m_pending_controller_sessions.load () < l_instances) {
        std::this_thread::sleep_for (milliseconds (100));
    }

    // while existing data plane stage connections (active or pending), execute the feedback loop
    auto start_time = std::chrono::system_clock::now ();
    auto time_threshold
        = std::chrono::seconds (max_time_seconds); // Set your desired time threshold here

    while (working_application_.load ()
        && (this->m_pending_controller_sessions.load () > 0
            || this->m_active_controller_sessions.load () > 0)) {
        // if exists pending sessions, perform the Session Handshake
        while (this->m_pending_controller_sessions.load () > 0) {
            // execute session handshake
            handle_controller_sessions ();
            std::this_thread::sleep_for (milliseconds (100));
        }

        this->sleep ();

        if (!m_core_control_application_ptr->feedback_loop_is_active ()) {
            if (m_supervisor_control_application_ptr->feedback_loop_is_active ()) {
                m_supervisor_control_application_ptr->stop_feedback_loop ();
            }
            working_application_ = false;
            Logging::log_info (
                "GlobalController::Exiting. Core controller application finished first.");
        } else if (!m_supervisor_control_application_ptr->feedback_loop_is_active ()) {
            if (m_core_control_application_ptr->feedback_loop_is_active ()) {
                m_core_control_application_ptr->stop_feedback_loop ();
            }
            working_application_ = false;
            Logging::log_info (
                "GlobalController::Exiting. Supervisor controller application finished first.");
        }

        // Check if the time threshold has passed
        auto current_time = std::chrono::system_clock::now ();
        if (current_time - start_time > time_threshold) {
            Logging::log_info (
                "GlobalController::Time threshold exceeded. Stopping feedback loop.");
            working_application_ = false;
            m_supervisor_control_application_ptr->stop_feedback_loop ();
            m_core_control_application_ptr->stop_feedback_loop ();
        }
    }

    m_supervisor_control_application_ptr->log_stats ();
    m_core_control_application_ptr->log_stats ();

    for (auto const& controller_session : supervisor_controllers_session_) {
        controller_session.second->RemoveSession ();
    }

    for (auto const& controller_session : core_controllers_sessions_) {
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
void GlobalController::sleep ()
{
    std::this_thread::sleep_for (microseconds (this->m_feedback_loop_sleep_time));
}

////////////////////////////////////////////
///////// Register new sessions ////////////
////////////////////////////////////////////

// register_controller_session call. Register a new controller.
void GlobalController::register_controller_session (const std::string& controller_address)
{
    if (controller_role == ControllerRole::PRIMARY) {
        Logging::log_info ("GlobalController: RegisterControllerSession -- " + controller_address);

        pending_controller_sessions_lock_.lock ();
        pending_controller_sessions_.emplace (controller_address);
        pending_controller_sessions_lock_.unlock ();

        this->m_pending_controller_sessions.fetch_add (1);
    } else if (controller_role == ControllerRole::SECONDARY) {
        // Check if active?
        Logging::log_info ("GlobalController: Register controller as secondary controller.");

        // FOR SUPERVISOR
        this->supervisor_controllers_session_.emplace (controller_address,
            std::make_unique<CoreControllerSession> (controller_address));

        std::thread session_thread_t = std::thread (&CoreControllerSession::StartSession,
            supervisor_controllers_session_.at (controller_address).get (),
            controller_address);

        session_thread_t.detach ();

        m_supervisor_control_application_ptr->register_controller_session (controller_address,
            supervisor_controllers_session_[controller_address]);

        auto core_interface
            = supervisor_controllers_session_[controller_address]->ShareInterface ();

        // FOR CORE
        // Create dedicated session for core with shared interface

        this->core_controllers_sessions_.emplace (controller_address,
            std::make_unique<CoreControllerSession> (controller_address, core_interface));

        std::thread session_thread_t_core = std::thread (&CoreControllerSession::StartSession,
            core_controllers_sessions_.at (controller_address).get (),
            controller_address);
        session_thread_t_core.detach ();

        m_active_controller_sessions.fetch_add (1);
    }
}

// handle_controller_sessions call. Processes a pending controller session.
void GlobalController::handle_controller_sessions ()
{
    Logging::log_info ("GlobalController: Handle controller sessions");

    pending_controller_sessions_lock_.lock ();
    if (pending_controller_sessions_.empty ()) {
        pending_controller_sessions_lock_.unlock ();
        return;
    }
    std::string new_controller_address = pending_controller_sessions_.front ();
    pending_controller_sessions_.pop ();
    pending_controller_sessions_lock_.unlock ();

    m_pending_controller_sessions.fetch_sub (1);

    Logging::log_info ("GlobalController: Handling (" + new_controller_address + ")");

    this->supervisor_controllers_session_.emplace (new_controller_address,
        std::make_unique<CoreControllerSession> (new_controller_address));

    m_supervisor_control_application_ptr->register_controller_session (new_controller_address,
        supervisor_controllers_session_[new_controller_address]);

    std::thread session_thread_t = std::thread (&CoreControllerSession::StartSession,
        supervisor_controllers_session_.at (new_controller_address).get (),
        new_controller_address);
    session_thread_t.detach ();

    // Create dedicated session for core with shared interface
    auto core_interface
        = supervisor_controllers_session_[new_controller_address]->ShareInterface ();

    this->core_controllers_sessions_.emplace (new_controller_address,
        std::make_unique<CoreControllerSession> (new_controller_address, core_interface));

    std::thread session_thread_t_core = std::thread (&CoreControllerSession::StartSession,
        core_controllers_sessions_.at (new_controller_address).get (),
        new_controller_address);
    session_thread_t_core.detach ();

    PStatus status = this->controller_handshake (new_controller_address);

    m_active_controller_sessions.fetch_add (1);
}

// controller_handshake call. Handles the handshake with a lower controller.
PStatus GlobalController::controller_handshake (const std::string& controller_address)
{
    Logging::log_info ("GlobalController: Controller Handshake (" + controller_address + ")");
    PStatus status = PStatus::Error ();

    if (supervisor_controllers_session_[controller_address] != nullptr) {
        // invoke CallStageHandshake routine to acknowledge the stage's identifier
        status = this->call_controller_handshake (controller_address);

    } else {
        Logging::log_info ("Controller Handshake: session (" + controller_address + ") is null.");
    }

    return status;
}

// call_controller_handshake call. Submits LOCAL_HANDSHAKE rule to a lower controller.
PStatus GlobalController::call_controller_handshake (const std::string& controller_address)
{
    PStatus status = PStatus::Error ();
    // create LOCAL_HANDSHAKE request
    std::string rule = std::to_string (LOCAL_HANDSHAKE) + "|";
    // put request on LocalPlaneSession::submission_queue

    for (const auto& specific_rule : *housekeeping_rules_ptr_) {
        rule += ":" + specific_rule;
    }

    supervisor_controllers_session_[controller_address]->SubmitRule (rule);

    // wait for request to be on LocalPlaneSession::completion_queue
    std::unique_ptr<StageResponse> resp_t
        = supervisor_controllers_session_[controller_address]->GetResult ();
    // convert StageResponse to Handshake object
    auto* ack_ptr_t = dynamic_cast<StageResponseACK*> (resp_t.get ());

    if (ack_ptr_t != nullptr) {
        if (ack_ptr_t->ACKValue () == static_cast<int> (AckCode::ok)) {
            status = PStatus::OK ();
        }
    }

    return status;
}

// register_stage_session call. Register a new data plane stage in the core control application.
void GlobalController::register_stage_session (const StageInfoConnect* request, bool rebuild)
{
    auto controller_session = core_controllers_sessions_[request->local_address ()];
    m_core_control_application_ptr->register_stage_session (request, controller_session, rebuild);
}

// collect_stages_info call. Collects and registers the stages of all lower controllers.
PStatus GlobalController::collect_stages_info ()
{
    for (const auto& [controller_address, session] : core_controllers_sessions_) {
        Logging::log_debug ("GlobalController: Collecting stage info from controller session at "
            + controller_address);
        session->SubmitRule (std::to_string (COLLECT_DETAILED_STATS) + "|"
            + std::to_string (COLLECT_GLOBAL_ALL_STATS) + "|");
    }

    for (const auto& [controller_address, session] : core_controllers_sessions_) {

        auto stats_ptr = session->GetResult ();

        if (!stats_ptr) {
            Logging::log_error (
                "GlobalController: No statistics received from controller session at "
                + controller_address + ". Trying again.");
            int still_attempt = 50;
            while (still_attempt > 0 && !stats_ptr) {
                std::this_thread::sleep_for (microseconds (100000));
                session->SubmitRule (std::to_string (COLLECT_DETAILED_STATS) + "|"
                    + std::to_string (COLLECT_GLOBAL_ALL_STATS) + "|");
                stats_ptr = session->GetResult ();
                still_attempt--;
            }
        }

        auto* response_ptr = dynamic_cast<StageResponseStats*> (stats_ptr.get ());

        if (!response_ptr || response_ptr->m_stats_ptr->empty ()) {
            Logging::log_error (
                "GlobalController: No statistics found in response from controller session at "
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
                Logging::log_info ("GlobalController: Registered stage session for "
                    + global_stat_ptr->stage_name + " in environment " + global_stat_ptr->stage_env
                    + " from controller session at " + controller_address);
            }
        }
    }
    return PStatus::OK ();
}

// TO-DO:Move to utils in latter version

// parse_rule_with_break call. Parses a rule into tokens using '|' as delimiter.
void GlobalController::parse_rule_with_break (const std::string& rule,
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
