/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_LOCAL_CONTROL_APPLICATION_HPP
#define CHEFERD_LOCAL_CONTROL_APPLICATION_HPP

#include "_deps/grpc-src/include/grpcpp/grpcpp.h"
#include "cheferd/session/data_plane_session.hpp"
#include "cheferd/session/handshake_session.hpp"
#include "control_application.hpp"

#include <chrono>
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
using controllers_grpc_interface::StatsGlobalMap;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using controllers_grpc_interface::ControllerToLower;
using controllers_grpc_interface::ControllerToUpper;
using controllers_grpc_interface::SimplifiedHandshakeRaw;
using grpc::Channel;
using grpc::ClientContext;

namespace cheferd {

// Number of rounds for aggregated statistics.
#define AGGREGATE_COLLECT_ROUNDS 5

/**
 * LocalControlApplication class.
 * The LocalControlApplication represents the local controller that coordinates data plane stages.
 * It serves the ControllerToLower gRPC service for the upper controller, handshakes new stages,
 * forwards enforcement rules to them, and periodically collects their statistics. The feedback
 * loop starts after the handshake with the upper controller.
 * Currently, the LocalControlApplication class contains the following variables:
 * - local_address: address on which the local controller receives upper controller requests.
 * - data_sessions_: container used for mapping active data plane stages to its DataPlaneSession.
 * - preparing_data_sessions_: container used for mapping preparing data plane stages to its
 * DataPlaneSession.
 * - pending_data_sessions_: queue that holds pending data plane sessions.
 * - pending_data_plane_sessions_lock_: mutex for concurrency control over pending_data_sessions_.
 * - stub_: unique_ptr of stub used to communicate with the upper controller.
 * - operation_to_channel_object: container used for mapping an operation to its respective
 * (channel, object) pairs in the data plane stage context.
 * - server: unique_ptr of Server that receives requests from the upper controller.
 * - m_active_data_plane_sessions: atomic value that marks the number of active data
 * plane sessions.
 * - m_pending_data_plane_sessions: atomic value that marks the number of pending data
 * plane sessions.
 * - stage_info_detailed: detailed information about each stage.
 * - session_lock_: mutex for concurrency control over sessions.
 * - last_stats_global: last statistics collected by the feedback loop.
 * - stats_backlogged: whether last_stats_global was not yet sent to the upper controller.
 */
class LocalControlApplication : public ControllerToLower::Service, public ControlApplication {

private:
    std::string local_address;
    std::unordered_map<std::string, std::unique_ptr<DataPlaneSession>> data_sessions_;
    std::unordered_map<std::string, std::unique_ptr<DataPlaneSession>> preparing_data_sessions_;
    std::queue<std::unique_ptr<HandshakeSession>> pending_data_sessions_;
    std::mutex pending_data_plane_sessions_lock_;
    std::unique_ptr<ControllerToUpper::Stub> stub_;
    std::unordered_map<std::string, std::vector<std::pair<int, int>>> operation_to_channel_object;
    std::unique_ptr<Server> server;
    std::atomic<int> m_active_data_plane_sessions;
    std::atomic<int> m_pending_data_plane_sessions;
    std::unordered_map<std::string, std::unique_ptr<StageInfo>> stage_info_detailed;
    std::mutex session_lock_;
    controllers_grpc_interface::StatsGlobalAll last_stats_global;
    std::atomic<bool> stats_backlogged;

    /**
     * initialize: Initialize control application. Currently does nothing.
     */
    void initialize ();

    /**
     * execute_feedback_loop: Executes feedback loop.
     * This method is responsible to verify if there is any pending data plane trying to connect,
     * and to collect statistics from the stages every cycle. Exits the process at the end.
     */
    void execute_feedback_loop ();

    /**
     * handle_data_plane_sessions: Processes a pending data plane session: handshakes the stage,
     * creates its DataPlaneSession, and registers it in the upper controller.
     */
    void handle_data_plane_sessions ();

    /**
     * sleep: Used to make control application main thread wait for the next loop.
     */
    void sleep () override;

    /**
     * call_stage_handshake: Submits STAGE_HANDSHAKE rule and housekeeping rules to data plane
     * stage.
     * @param handshake_session Session to submit handshake rule to.
     * @return Returns unique_ptr holding data plane stage detailed information.
     */
    std::unique_ptr<StageInfo> call_stage_handshake (HandshakeSession* handshake_session);

    /**
     * mark_stage_ready: Submits STAGE_READY to data plane stage.
     * @param stage_name_env Data plane stage identifier.
     * @return Returns PStatus::OK if successful, PStatus::Error otherwise.
     */
    PStatus mark_stage_ready (const std::string& stage_name_env) const;

    /**
     * submit_housekeeping_rules: Submits housekeeping rules to data plane stage.
     * @param stage_name_env Data plane stage identifier.
     * @return Number of housekeeping rules successfully submitted.
     */
    int submit_housekeeping_rules (const std::string& stage_name_env) const;

    /**
     * fill_socket_info: Defines a new individual socket for data plane stage.
     * @param handshake_ptr Stores data plane stage information.
     * @param socket_info New socket for data plane stage.
     * @return Returns PStatus::OK if successful, PStatus::Error otherwise.
     */
    static PStatus fill_socket_info (StageResponseHandshake* handshake_ptr,
        std::string& socket_info);

    /**
     * RunUpperIncomingServer: Execute the server that receives requests from the upper
     * controller. Blocks until the server is shut down.
     */
    void RunUpperIncomingServer ();

    /**
     * ConnectToUpper: Connects to the upper controller.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status ConnectToUpper ();

    /**
     * ConnectStageToUpper: Connect data plane stage to upper controller.
     * @param stage_name Data plane stage job's name.
     * @param stage_env Data plane stage job's env.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status ConnectStageToUpper (const std::string& stage_name, const std::string& stage_env);

    /**
     * ControllerHandshake: Controller handshake from the upper controller. Stores the housekeeping
     * rules and starts the feedback loop.
     * @param context Server context.
     * @param request Container that stores housekeeping rules.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status ControllerHandshake (ServerContext* context,
        const controllers_grpc_interface::SimplifiedHandshakeRaw* request,
        controllers_grpc_interface::ACK* reply) override;

    /**
     * MarkStageReady: Mark stage ready from upper controller. Moves the stage from the preparing
     * sessions to the active ones.
     * @param context Server context.
     * @param request Defines if stage is ready.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status MarkStageReady (ServerContext* context,
        const controllers_grpc_interface::StageReadyRaw* request,
        controllers_grpc_interface::ACK* reply) override;

    /**
     * CreateEnforcementRule: Create enforcement rules from upper controller.
     * @param context Server context.
     * @param request Rules to be enforced.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status CreateEnforcementRule (ServerContext* context,
        const controllers_grpc_interface::EnforcementRulesMult* request,
        controllers_grpc_interface::ACK* reply) override;

    /**
     * CollectGlobalAllStatistics: Collect Statistics request from upper controller for a non-fixed
     * number of operations. Replies with the statistics last collected by the feedback loop (if
     * not sent yet).
     * @param context Server context.
     * @param request Defines control operation.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status CollectGlobalAllStatistics (ServerContext* context,
        const controllers_grpc_interface::Empty* request,
        controllers_grpc_interface::StatsGlobalAllMap* reply) override;

    /**
     * collect_global_all_statistics: Collects the statistics of all active data plane stages and
     * stores them in last_stats_global. Called by the feedback loop.
     */
    void collect_global_all_statistics ();

    /**
     * CollectGlobalStatistics: Collect Statistics request from upper controller.
     * @param context Server context.
     * @param request Defines control operation.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status CollectGlobalStatistics (ServerContext* context,
        const controllers_grpc_interface::Empty* request,
        controllers_grpc_interface::StatsGlobalMap* reply) override;

    /**
     * CollectGlobalStatisticsAggregated: Collect Statistics request from upper controller.
     * It aggregates the statistics from several rounds.
     * @param context Server context.
     * @param request Defines control operation.
     * @param reply Response.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status CollectGlobalStatisticsAggregated (ServerContext* context,
        const controllers_grpc_interface::Empty* request,
        StatsGlobalMap* reply) override;

    /**
     * LocalPassthru: General function to submit rules to data plane stages.
     * @param stage_name_env Data plane stage identifier.
     * @param rule Rule to be submitted.
     * @return Returns Status::OK if successful, Status::Error otherwise.
     */
    Status LocalPassthru (std::string stage_name_env, std::string rule);

public:
    /**
     * LocalControlApplication parameterized constructor.
     * @param core_address Core controller address.
     * @param local_address Local controller address.
     */
    LocalControlApplication (const std::string& core_address, const std::string& local_address);

    /**
     * LocalControlApplication parameterized constructor. Starts the server for the upper
     * controller and connects to it.
     * @param rules_ptr Pointer to the housekeeping rules container (filled by the handshake).
     * @param upper_address Address of the upper controller.
     * @param own_upper_address Address on which this controller receives upper controller
     * requests.
     * @param cycle_sleep_time Control feedback loop time (in microseconds).
     */
    LocalControlApplication (std::vector<std::string>* rules_ptr,
        const std::string& upper_address,
        const std::string& own_upper_address,
        const uint64_t& cycle_sleep_time);

    /**
     * LocalControlApplication default destructor.
     */
    ~LocalControlApplication () override;

    /**
     * operator: Used to initiate the control application feedback loop execution.
     */
    void operator() () override;

    /**
     * register_stage_session: Register a new data plane (queued for the handshake).
     * @param socket_t Socket id.
     */
    void register_stage_session (int socket_t);

    /**
     * log_stats: Log control application stats.
     */
    void log_stats ();

    /**
     * stop_feedback_loop: Stops the feedback loop from executing, shuts down the server, and
     * removes all data plane sessions.
     */
    void stop_feedback_loop () override;

    /**
     * parse_rule: parses a rule into tokens using char c as delimiter.
     * @param rule Rule to be parsed.
     * @param tokens Container to store parsed tokens.
     * @param c Delimiter.
     */
    void parse_rule (const std::string& rule, std::vector<std::string>* tokens, char c);
};

} // namespace cheferd

#endif // CHEFERD_LOCAL_CONTROL_APPLICATION_HPP
