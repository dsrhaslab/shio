/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_PASSTHROUGH_CONTROL_APPLICATION_HPP
#define CHEFERD_PASSTHROUGH_CONTROL_APPLICATION_HPP

#include "_deps/grpc-src/include/grpcpp/grpcpp.h"
#include "cheferd/session/local_controller_session.hpp"
#include "control_application.hpp"

#include <regex>
#include <thread>

#ifdef BAZEL_BUILD
#include "examples/protos/controllers_grpc_interface.grpc.pb.h"
#else
#include "controllers_grpc_interface.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using controllers_grpc_interface::ACK;
using controllers_grpc_interface::ConnectReply;
using controllers_grpc_interface::ControllerToUpper;
using controllers_grpc_interface::StageInfoConnect;
using controllers_grpc_interface::StageReadyRaw;

namespace cheferd {

/**
 * PassthroughControlApplication class.
 * The PassthroughControlApplication runs in the cluster controller for stages of jobs without a
 * configured priority. It serves as liaison between upper and lower controllers: it forwards
 * stage registrations to the upper controller, and the upper controller's enforcement and
 * statistics requests to the local controllers.
 * Currently, the PassthroughControlApplication class contains the following variables:
 * - controller_address: address of this (cluster) controller.
 * - controller_sessions_: maps local controller addresses to their active sessions.
 * - stages_to_controller_address_lock_: mutex for concurrency control over
 * stages_to_controller_address and controller_address_to_stages.
 * - stages_to_controller_address: maps stage (name+env) to local controller address.
 * - controller_address_to_stages: maps local controller addresses to their associated stages.
 * - pending_stage_lock_: mutex for concurrency control over the pending containers.
 * - pending_stages_to_controller_address: maps pending stage (name+env) to local controller
 * address, until the upper controller marks the stage as ready.
 * - pending_controller_sessions_: maps pending stage (name+env) to its local controller session.
 * - session_lock_: serializes requests from the upper controller that use the sessions.
 * - stub_: unique_ptr of stub used to communicate with the upper controller.
 */
class PassthroughControlApplication : public ControlApplication {

private:
    std::string controller_address;
    std::unordered_map<std::string, std::shared_ptr<Session>> controller_sessions_;

    std::mutex stages_to_controller_address_lock_;
    std::unordered_map<std::string, std::string> stages_to_controller_address;
    std::unordered_map<std::string, std::vector<std::string>> controller_address_to_stages;

    std::mutex pending_stage_lock_;
    std::unordered_map<std::string, std::string> pending_stages_to_controller_address;
    std::unordered_map<std::string, std::shared_ptr<LocalControllerSession>>
        pending_controller_sessions_;

    // Should have instead one per session
    std::mutex session_lock_;

    std::unique_ptr<ControllerToUpper::Stub> stub_;

    /**
     * execute_feedback_loop: Executes feedback loop.
     * This method is responsible for keeping control application active until it is stopped or
     * the maximum number of rounds is reached.
     */
    void execute_feedback_loop ();

    /**
     * sleep: Used to make control application main thread wait for the next loop.
     */
    void sleep () override;

    /**
     * mark_stage_ready: Submits STAGE_READY to data plane stage.
     * @param stage_name_env Data plane stage identifier.
     * @return Returns PStatus::OK if successful, PStatus::Error otherwise.
     */
    PStatus mark_stage_ready (const std::string& stage_name_env) const;

public:
    /**
     * PassthroughControlApplication parameterized constructor.
     * @param controller_address Controller address.
     * @param cycle_sleep_time Amount of time that a feedback-loop cycle should take.
     */
    PassthroughControlApplication (const std::string& controller_address,
        const uint64_t& cycle_sleep_time);

    /**
     * PassthroughControlApplication default destructor.
     */
    ~PassthroughControlApplication () override;

    /**
     * operator: Used to initiate the control application feedback loop execution.
     */
    void operator() () override;

    /**
     * initialize: Starts thread to run control application.
     */
    void initialize ();

    /**
     * MarkStageReady: Mark stage ready from upper controller. Moves the stage from the pending
     * containers to the active ones and submits STAGE_READY to it.
     * @param context Server context.
     * @param request Defines if stage is ready.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status MarkStageReady (ServerContext* context,
        const controllers_grpc_interface::StageReadyRaw* request,
        controllers_grpc_interface::ACK* reply);

    /**
     * CreateEnforcementRule: Create enforcement rules from upper controller. Groups the rules per
     * local controller and submits them through the respective sessions.
     * @param context Server context.
     * @param request Rules to be enforced.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status CreateEnforcementRule (ServerContext* context,
        const controllers_grpc_interface::EnforcementRulesMult* request,
        controllers_grpc_interface::ACK* reply);

    /**
     * CollectGlobalAllStatistics: Collect Statistics request from upper controller for a non-fixed
     * number of operations. Stages that report no statistics are removed.
     * @param context Server context.
     * @param request Defines control operation.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status CollectGlobalAllStatistics (ServerContext* context,
        const controllers_grpc_interface::Empty* request,
        controllers_grpc_interface::StatsGlobalAllMap* reply);

    /**
     * register_stage_session: Register a new data plane. Normally the stage is kept pending and
     * forwarded to the upper controller; when rebuilding (after a failover) it is registered as
     * active directly.
     * @param request Info about stage.
     * @param controller_session Session of the local controller the stage is connected to.
     * @param rebuild Indicates if the stage session is being registered as part of a rebuild
     * process.
     */
    void register_stage_session (const StageInfoConnect* request,
        std::shared_ptr<LocalControllerSession> controller_session,
        bool rebuild = false);

    /**
     * register_communication_channel: Register communication channel to upper controller.
     * @param channel Communication channel.
     */
    void register_communication_channel (std::shared_ptr<grpc::Channel> channel);

    /**
     * log_stats: Log control application stats.
     */
    void log_stats ();

    /**
     * stop_feedback_loop: Stops the feedback loop from executing.
     */
    void stop_feedback_loop () override;
};

} // namespace cheferd

#endif // CHEFERD_PASSTHROUGH_CONTROL_APPLICATION_HPP
