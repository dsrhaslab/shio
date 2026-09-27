/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_GLOBAL_CONTROLLER_HPP
#define SHIO_GLOBAL_CONTROLLER_HPP

#include "shio/session/local_controller_session.hpp"
#include "control_application/control_application.hpp"
#include "control_application/core_control_application.hpp"
#include "control_application/supervisor_control_application.hpp"

#include <regex>
#include <thread>

#ifdef BAZEL_BUILD
#include "examples/protos/controllers_grpc_interface.grpc.pb.h"
#else
#include "controllers_grpc_interface.grpc.pb.h"
#endif

using controllers_grpc_interface::StageInfoConnect;
using grpc::Status;

namespace shio {

/**
 * GlobalController class.
 * The GlobalController represents the top-level controller that coordinates cluster controllers.
 * It is composed by a core control application and a supervisor control application; each lower
 * controller gets one session for each of them (sharing the same gRPC interface).
 * Currently, the GlobalController class contains the following variables:
 * - m_core_control_application_ptr: pointer to the core control application instance.
 * - m_supervisor_control_application_ptr: pointer to the supervisor control application
 * instance.
 * - core_controllers_sessions_: maps controller addresses to the sessions used by the core
 * control application.
 * - supervisor_controllers_session_: maps controller addresses to the sessions used by the
 * supervisor control application.
 * - pending_controller_sessions_: queue that holds pending controller sessions.
 * - pending_controller_sessions_lock_: mutex for concurrency control over
 * pending_controller_sessions_.
 * - m_active_controller_sessions: atomic value that tracks the number of active controller
 * sessions.
 * - m_pending_controller_sessions: atomic value that tracks the number of pending controller
 * sessions.
 * - controller_role: role of the controller (primary or secondary).
 */
class GlobalController : public ControlApplication {

private:
    CoreControlApplication* m_core_control_application_ptr;
    SupervisorControlApplication* m_supervisor_control_application_ptr;

    std::unordered_map<std::string, std::shared_ptr<CoreControllerSession>>
        core_controllers_sessions_;
    std::unordered_map<std::string, std::shared_ptr<CoreControllerSession>>
        supervisor_controllers_session_;
    std::queue<std::string> pending_controller_sessions_;
    std::mutex pending_controller_sessions_lock_;
    std::atomic<int> m_active_controller_sessions;
    std::atomic<int> m_pending_controller_sessions;
    ControllerRole controller_role;

    /**
     * execute_feedback_loop: Executes feedback loop.
     * This method is responsible to verify if there is any pending controller trying to connect,
     * and to stop both control applications when one of them finishes (or the time threshold is
     * exceeded). Exits the process at the end.
     */
    void execute_feedback_loop ();

    /**
     * sleep: Used to make control application main thread wait for the next loop.
     */
    void sleep () override;

    /**
     * handle_controller_sessions: Handles the controller sessions.
     * This method checks for pending controller sessions and processes them.
     */
    void handle_controller_sessions ();

    /**
     * controller_handshake: Handles the handshake with a lower controller.
     * @param controller_address Address of the controller to handshake with.
     * @return Returns PStatus indicating the result of the handshake.
     */
    PStatus controller_handshake (const std::string& controller_address);

    /**
     * call_controller_handshake: Submits the LOCAL_HANDSHAKE rule (with the housekeeping rules) to
     * the lower controller and waits for its ACK.
     * @param controller_address Address of the controller to handshake with.
     * @return Returns PStatus indicating the result of the handshake.
     */
    PStatus call_controller_handshake (const std::string& controller_address);

    /**
     * parse_rule_with_break: parses a rule into tokens using '|' as delimiter.
     * @param rule Rule to be parsed.
     * @param tokens Container to store parsed tokens.
     */
    static void parse_rule_with_break (const std::string& rule, std::vector<std::string>* tokens);

public:
    /**
     * GlobalController parameterized constructor.
     * @param rules_ptr Pointer to the vector of housekeeping rules.
     * @param op_system_limits Map of operations to their system limits.
     * @param policies Map of policies where each priority is defined by an integer key and a nested
     * map of operation names to limits.
     * @param total_jobs Total number of jobs expected to be managed by the application.
     * @param jobs_name_to_priority A map associating job names with their priority levels.
     * @param controller_role Role of controller (e.g. PRIMARY or SECONDARY)
     */
    GlobalController (std::vector<std::string>* rules_ptr,
        std::unordered_map<std::string, uint64_t> op_system_limits,
        std::unordered_map<int, std::unordered_map<std::string, uint64_t>> policies,
        int total_jobs,
        std::unordered_map<std::string, int> jobs_name_to_priority,
        ControllerRole controller_role);

    /**
     * GlobalController default destructor.
     */
    ~GlobalController () override;

    /**
     * operator: Used to initiate the control application feedback loop execution.
     */
    void operator() () override;

    /**
     * stop_feedback_loop: Stops the feedback loop from executing.
     */
    void stop_feedback_loop () override;

    /**
     * set_as_primary_controller: Marks controller as primary. Rebuilds the stage sessions from the
     * lower controllers' statistics and marks both control applications as primary.
     */
    void set_as_primary_controller () override;

    /**
     * register_controller_session: Register a new controller. The primary queues it for the
     * handshake; the secondary creates its sessions directly (the handshake was done by the
     * primary).
     * @param controller_address Controller identifier.
     */
    void register_controller_session (const std::string& controller_address);

    /**
     * register_stage_session: Register a new data plane.
     * @param request Info about stage.
     * @param rebuild Indicates if the stage session is being registered as part of a rebuild
     * process (after a failover).
     */
    void register_stage_session (const StageInfoConnect* request, bool rebuild = false);

    /**
     * collect_stages_info: Collects detailed information about all stages connected to the lower
     * controllers and registers them (with rebuild set).
     * @return PStatus::OK().
     */
    PStatus collect_stages_info ();
};

} // namespace shio

#endif // SHIO_GLOBAL_CONTROLLER_HPP
