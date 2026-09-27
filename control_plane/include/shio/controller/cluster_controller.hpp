/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_CLUSTER_CONTROLLER_HPP
#define SHIO_CLUSTER_CONTROLLER_HPP

#include "shio/controller/control_application/core_control_application.hpp"
#include "shio/controller/control_application/passthrough_control_application.hpp"
#include "shio/networking/connection_manager/upper_core_connection_manager.hpp"
#include "shio/session/local_controller_session.hpp"
#include "control_application/control_application.hpp"

#include <regex>
#include <thread>

#ifdef BAZEL_BUILD
#include "examples/protos/controllers_grpc_interface.grpc.pb.h"
#else
#include "controllers_grpc_interface.grpc.pb.h"
#endif

using controllers_grpc_interface::ACK;
using controllers_grpc_interface::ConnectReply;
using controllers_grpc_interface::ConnectRequest;
using controllers_grpc_interface::StageInfoConnect;
using controllers_grpc_interface::StageReadyRaw;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using controllers_grpc_interface::ControllerToUpper;
using controllers_grpc_interface::SimplifiedHandshakeRaw;
using controllers_grpc_interface::SupervisorControllerToLowerController;
using grpc::Channel;
using grpc::ClientContext;

namespace shio {

class UpperCoreConnectionManager;

/**
 * ClusterController class.
 * The ClusterController represents the mid-level controller that coordinates local controllers.
 * It serves the SupervisorControllerToLowerController gRPC service for the upper (global)
 * controller, and delegates requests to its core control application (stages of jobs with a
 * configured priority) or passthrough control application (all other stages).
 * Currently, the ClusterController class contains the following variables:
 * - m_core_control_application_ptr: pointer to the core control application instance.
 * - m_passthru_control_application_ptr: pointer to the passthrough control application
 * instance.
 * - m_upper_core_connection_manager: pointer to the upper core connection manager instance.
 * - controller_sessions_: maps controller addresses to their active sessions.
 * - pending_controller_sessions_: queue that holds pending controller sessions.
 * - pending_controller_sessions_lock_: mutex for concurrency control over
 * pending_controller_sessions_.
 * - m_active_controller_sessions: atomic value that tracks the number of active controller
 * sessions.
 * - m_pending_controller_sessions: atomic value that tracks the number of pending controller
 * sessions.
 * - jobs_name_to_priority: maps job names to their priority levels.
 * - own_address: address of the current controller.
 * - controller_role: role of the controller (primary or secondary).
 */
class ClusterController : public SupervisorControllerToLowerController::Service,
                          public ControlApplication {
private:
    CoreControlApplication* m_core_control_application_ptr;
    PassthroughControlApplication* m_passthru_control_application_ptr;
    UpperCoreConnectionManager* m_upper_core_connection_manager;
    std::unordered_map<std::string, std::shared_ptr<LocalControllerSession>> controller_sessions_;
    std::queue<std::string> pending_controller_sessions_;
    std::mutex pending_controller_sessions_lock_;
    std::atomic<int> m_active_controller_sessions;
    std::atomic<int> m_pending_controller_sessions;
    std::unordered_map<std::string, int> jobs_name_to_priority;
    std::string own_address;
    ControllerRole controller_role;

    /**
     * execute_feedback_loop: Executes feedback loop.
     * This method is responsible to verify if there is any pending controllers trying to connect.
     * Also, it handles if control application are still active, and terminates them when necessary.
     */
    void execute_feedback_loop ();

    /**
     * sleep: Used to make control application main thread wait for the next loop.
     */
    void sleep () override;

    /**
     * ControllerHandshake: Controller handshake from the upper controller. Stores the
     * housekeeping rules and initializes the control applications.
     * @param context Server context.
     * @param request Container that stores housekeeping rules.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status ControllerHandshake (ServerContext* context,
        const controllers_grpc_interface::SimplifiedHandshakeRaw* request,
        controllers_grpc_interface::ACK* reply) override;

    /**
     * MarkStageReady: Mark stage ready from upper controller.
     * @param context Server context.
     * @param request Defines if stage is ready.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status MarkStageReady (ServerContext* context,
        const controllers_grpc_interface::StageReadyRaw* request,
        controllers_grpc_interface::ACK* reply) override;

    /**
     * CreateEnforcementRule: Create enforcement from upper controller.
     * @param context Server context.
     * @param request Rules to be enforced.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status CreateEnforcementRule (ServerContext* context,
        const controllers_grpc_interface::EnforcementRulesMult* request,
        controllers_grpc_interface::ACK* reply) override;

    /**
     * EnforceSystemLimit: Enforce system limit from upper controller.
     * @param context Server context.
     * @param request Defines control operation.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status EnforceSystemLimit (ServerContext* context,
        const controllers_grpc_interface::OperationsLimits* request,
        controllers_grpc_interface::ACK* reply) override;

    /**
     * CollectGlobalAllStatistics: Collect Statistics request from upper controller for a non-fixed
     * number of operations.
     * @param context Server context.
     * @param request Defines control operation.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status CollectGlobalAllStatistics (ServerContext* context,
        const controllers_grpc_interface::Empty* request,
        controllers_grpc_interface::StatsGlobalAllMap* reply) override;

    /**
     * CollectCoreControllerStatistics: Collect core controller statistics request from upper
     * controller for a non-fixed number of operations.
     * @param context Server context.
     * @param request Defines control operation.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status CollectCoreControllerStatistics (ServerContext* context,
        const controllers_grpc_interface::Empty* request,
        controllers_grpc_interface::StatsGlobalAggregatedMap* reply) override;

    /**
     * handle_controller_sessions: Handles the controller sessions.
     * This method checks for pending controller sessions and processes them.
     */
    void handle_controller_sessions ();

    /**
     * controller_handshake: Handles the handshake with a local controller (primary only).
     * @param controller_address Address of the controller to handshake with.
     * @return Returns PStatus indicating the result of the handshake.
     */
    PStatus controller_handshake (const std::string& controller_address);

    /**
     * call_controller_handshake: Submits the LOCAL_HANDSHAKE rule (with the housekeeping rules) to
     * the local controller and waits for its ACK.
     * @param controller_address Address of the controller to handshake with.
     * @return Returns PStatus indicating the result of the handshake.
     */
    PStatus call_controller_handshake (const std::string& controller_address);

    /**
     * ConnectToUpper: Connects to the upper controller.
     * @param stub_ Unique pointer to the ControllerToUpper stub used for communication.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status ConnectToUpper (std::unique_ptr<ControllerToUpper::Stub> stub_);

    /**
     * parse_rule_with_break: parses a rule into tokens using '|' as delimiter.
     * @param rule Rule to be parsed.
     * @param tokens Container to store parsed tokens.
     */
    static void parse_rule_with_break (const std::string& rule, std::vector<std::string>* tokens);

public:
    /**
     * ClusterController parameterized constructor. A primary connects to the upper controller and
     * starts the server that receives its requests.
     * @param upper_address Address of the upper controller.
     * @param own_upper_address Address on which this controller receives upper controller requests.
     * @param policies Map of policies where each priority is defined by an integer key and a nested
     * map of operation names to limits.
     * @param total_jobs Total number of jobs expected to be managed by the application.
     * @param jobs_name_to_priority A map associating job names with their priority levels.
     * @param controller_role Role of controller (e.g. PRIMARY or SECONDARY).
     */
    ClusterController (const std::string& upper_address,
        const std::string& own_upper_address,
        std::unordered_map<int, std::unordered_map<std::string, uint64_t>> policies,
        int total_jobs,
        std::unordered_map<std::string, int> jobs_name_to_priority,
        ControllerRole controller_role);

    /**
     * ClusterController default destructor.
     */
    ~ClusterController () override;

    /**
     * operator: Used to initiate the controller feedback loop execution.
     */
    void operator() () override;

    /**
     * stop_feedback_loop: Stops the feedback loop from executing.
     */
    void stop_feedback_loop () override;

    /**
     * set_as_primary_controller: Marks controller as primary. Creates sessions for the known local
     * controllers, rebuilds the stage sessions, and starts the server for the upper controller.
     */
    void set_as_primary_controller () override;

    /**
     * register_controller_session: Register a new controller. The primary queues it for the
     * handshake; the secondary only records its address.
     * @param controller_address Controller identifier.
     */
    void register_controller_session (const std::string& controller_address);

    /**
     * register_stage_session: Register a new data plane. Stages of jobs with a configured priority
     * go to the core control application; the others go to the passthrough one.
     * @param request Info about stage.
     * @param rebuild Indicates if the stage session is being registered as part of a rebuild
     * process.
     */
    void register_stage_session (const StageInfoConnect* request, bool rebuild = false);

    /**
     * collect_stages_info: Collects detailed information about all stages connected to the local
     * controllers and registers them (with rebuild set).
     */
    void collect_stages_info ();

    /**
     * set_housekeeping_rules: Appends the received housekeeping rules and initializes the control
     * applications with the operations they define.
     * @param request Housekeeping rules received from the upper (or primary) controller.
     */
    void set_housekeeping_rules (const controllers_grpc_interface::SimplifiedHandshakeRaw* request);
};

} // namespace shio

#endif // SHIO_CLUSTER_CONTROLLER_HPP
