/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_CORE_CONTROL_APPLICATION_HPP
#define SHIO_CORE_CONTROL_APPLICATION_HPP

#include "_deps/grpc-src/include/grpcpp/grpcpp.h"
#include "shio/networking/stage_response/stage_response_stat_controller.hpp"
#include "shio/session/core_controller_session.hpp"
#include "shio/session/local_controller_session.hpp"
#include "shio/utils/options.hpp"
#include "control_application.hpp"

#include <regex>
#include <thread>

#ifdef BAZEL_BUILD
#include "examples/protos/controllers_grpc_interface.grpc.pb.h"
#else
#include "controllers_grpc_interface.grpc.pb.h"
#endif

using controllers_grpc_interface::ACK;
using controllers_grpc_interface::OperationsLimits;
using controllers_grpc_interface::StageInfoConnect;
using controllers_grpc_interface::StatsGlobalAggregatedMap;
using grpc::Server;
using grpc::ServerContext;
using grpc::Status;

namespace shio {

/**
 * CoreControlApplication class.
 *
 * The CoreControlApplication class is responsible for coordinating data plane stages
 * and managing various aspects of job priorities, operations, and controller sessions.
 * It provides mechanisms for tracking job demands, usage statistics, and system changes.
 * Each round it collects stage statistics, computes per-job rates with PSFA (within the limits
 * set by the upper controller through EnforceSystemLimit), and enforces them. Only runs its loop
 * while the controller is primary.
 *
 * Key Features:
 * - **Job and Priority Management**:
 *   - `expected_total_jobs`: Expected total number of jobs.
 *   - `jobs_name_to_priority`: Maps job names to their priority levels.
 *   - `priorities`: Maps priority levels to job demands.
 *   - `priority_counter`: Tracks the count of jobs for each priority level.
 *   - `priority_counter_mutex`: Mutex for concurrency control over priority_counter.
 *   - `usage_per_priority`: Tracks usage statistics for each operation and priority level.
 *   - `usage_per_priority_mutex`: Mutex for concurrency control over usage_per_priority.
 *
 * - **Controller Sessions**:
 *   - `controller_sessions_`: Maps controller addresses to their active sessions.
 *   - `pending_controller_sessions_`: Maps pending controller addresses to their active sessions.
 *   - `controller_sessions_failure_counter`: Tracks failure counts for controller sessions.
 *
 * - **Operation and Job Demands**:
 *   - `active_ops`: Set of currently active operations.
 *   - `ops_maximum_limit`: Maximum limits for operations (set by the upper controller).
 *   - `ops_maximum_limit_mutex`: Mutex for concurrency control over ops_maximum_limit.
 *   - `ops_maximum_limit_set`: Whether the upper controller already set the limits.
 *   - `op_job_demands_order`: Jobs per operation, ordered by demand.
 *   - `job_rates`: Current job rates.
 *   - `job_rates_mutex`: Mutex for concurrency control over job_rates.
 *   - `job_previous_rates`: Previous job rates.
 *   - `change_in_job`: Flags indicating changes in job rates.
 *   - `controller_address_to_stages`: Maps controller addresses to their associated stages.
 *
 * - **Job Location Tracking**:
 *   - `job_location_tracker`: Tracks job locations across different stages and local controllers.
 *   - `stage_info_detailed`: Detailed information about stages.
 *
 * - **Data Queue and Locks**:
 *   - `local_to_data_queue_`: Queue of stages waiting to be handled (registered but not ready).
 *   - `pending_stage_lock_`: Mutex for concurrency control during stage registration.
 *
 * - **Data Plane Session Counters**:
 *   - `m_active_data_plane_sessions`: Atomic counter for active data plane sessions.
 *   - `m_pending_data_plane_sessions`: Atomic counter for pending data plane sessions.
 *
 * - **System Change Flag**:
 *   - `change_in_system`: Atomic flag indicating changes in the system state.
 *
 * - **Usage Statistics**:
 *   - `usage_per_operation_and_stage`: Tracks usage statistics for operations and stages.
 */
class CoreControlApplication : public ControlApplication {

private:
    // Job and priority mappings
    int expected_total_jobs;
    std::unordered_map<std::string, int> jobs_name_to_priority;
    std::unordered_map<int, std::unordered_map<std::string, uint64_t>> priorities;
    std::mutex priority_counter_mutex;
    std::unordered_map<int, int> priority_counter;
    //<operation,<priority, usage>>
    std::mutex usage_per_priority_mutex;
    std::unordered_map<std::string, std::unordered_map<int, uint64_t>> usage_per_priority;

    // Controller sessions
    std::unordered_map<std::string, std::shared_ptr<Session>> controller_sessions_;
    std::unordered_map<std::string, std::shared_ptr<Session>> pending_controller_sessions_;
    std::unordered_map<std::string, int> controller_sessions_failure_counter;

    // Operation and job demands
    std::unordered_set<std::string> active_ops;
    std::mutex ops_maximum_limit_mutex;
    std::unordered_map<std::string, uint64_t> ops_maximum_limit;
    bool ops_maximum_limit_set;
    std::unordered_map<std::string, std::map<uint64_t, std::vector<std::string>>>
        op_job_demands_order;
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> job_rates;
    std::mutex job_rates_mutex;
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> job_previous_rates;
    std::unordered_map<std::string, std::atomic<bool>> change_in_job;
    std::unordered_map<std::string, std::vector<std::string>> controller_address_to_stages;

    // Job location tracking
    std::map<std::string, std::unordered_map<std::string, std::vector<int>>> job_location_tracker;
    std::unordered_map<std::string, std::unique_ptr<StageInfo>> stage_info_detailed;

    // Data queue and locks
    std::queue<std::unique_ptr<StageInfo>> local_to_data_queue_;
    std::mutex pending_stage_lock_;

    // Data plane session counters
    std::atomic<int> m_active_data_plane_sessions;
    std::atomic<int> m_pending_data_plane_sessions;

    // System change flag
    std::atomic<bool> change_in_system;

    // Usage statistics per operation and stage
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>>
        usage_per_operation_and_stage;

    // Private methods
    /**
     * @brief Executes the feedback loop for processing and decision-making.
     */
    void execute_feedback_loop ();

    /**
     * @brief Handles one pending data plane stage: registers it and marks it as ready.
     */
    void handle_data_plane_sessions ();

    /**
     * @brief Used to make control application main thread wait for the next loop.
     */
    void sleep () override;

    /**
     * @brief Marks the data plane stage as ready.
     *
     * @param local_controller_address The address of the local controller.
     * @param stage_name The name of the stage to mark as ready.
     * @param stage_env The environment of the stage.
     * @return PStatus::Error() if there is no session for the local controller; PStatus::OK()
     * otherwise (the stage's ACK is currently not checked).
     */
    PStatus mark_data_plane_stage_ready (const std::string& local_controller_address,
        const std::string& stage_name,
        const std::string& stage_env);

    /**
     * @brief Collects statistics from the local controllers that have stages.
     */
    void collect_statistics_global ();

    /**
     * @brief Collects shared statistics for a specific session and manages session cleanup.
     *
     * @param session The shared pointer to the session object.
     * @param local_controller_address The local address associated with the session.
     * @param sessions_to_delete A list of sessions to be deleted.
     */
    void collect_statistics_result_shared (const std::shared_ptr<Session>& session,
        const std::string local_controller_address,
        std::list<std::string>& sessions_to_delete);

    /**
     * @brief Aggregates collected statistics per operation and job, over all stages of the job.
     * Stages with no (or zero) usage count as 1.
     *
     * @param operation The name of the operation.
     * @param job The name of the job.
     * @return uint64_t The aggregated statistics value.
     */
    uint64_t aggregate_collected_stats_per_operation_and_job (const std::string& operation,
        const std::string& job);

    /**
     * @brief Computes PSFA rules for the system (one thread per active operation).
     */
    void compute_psfa_rules ();

    /**
     * @brief Computes PSFA rules for a specific operation.
     *
     * @param operation The name of the operation.
     */
    void compute_psfa_rules_per_op (const std::string& operation);

    /**
     * @brief Computes PSFA rules for a specific operation in phase 1.
     *
     * @param operation The name of the operation.
     * @param app_usage A map containing application usage statistics.
     * @param system_total_rate The total system rate.
     * @param left_iops_per_operation The remaining IOPS per operation.
     */
    void compute_psfa_per_op_phase1 (const std::string& operation,
        std::unordered_map<std::string, uint64_t>& app_usage,
        uint64_t& system_total_rate,
        uint64_t& left_iops_per_operation);

    /**
     * @brief Computes PSFA rules for a specific operation in phase 2.
     *
     * @param operation The name of the operation.
     * @param app_usage A map containing application usage statistics.
     * @param system_total_rate The total system rate.
     * @param left_iops_per_operation The remaining IOPS per operation.
     */
    void compute_psfa_per_op_phase2 (const std::string& operation,
        std::unordered_map<std::string, uint64_t>& app_usage,
        uint64_t system_total_rate,
        uint64_t left_iops_per_operation);

    /**
     * @brief Tunes rates for PSFA rules for a specific operation: keeps the previous rate of jobs
     * whose rate barely changed, and scales the others so the total does not exceed the limit.
     *
     * @param operation The name of the operation.
     * @param system_total_rate The total system rate.
     * @param op_maximum_limit The maximum limit for the operation.
     */
    void compute_psfa_per_op_tune_rates (const std::string& operation,
        uint64_t system_total_rate,
        uint64_t op_maximum_limit);

    /**
     * @brief Enforces the computed PSFA rules, sending one rule per local controller.
     */
    void enforce_psfa_rules ();

    /**
     * @brief Builds enforcement rules for the given application.
     * @param app_name The name of the application for which enforcement rules are being built.
     * @param local_to_envs A mapping of local identifiers to their corresponding environment
     * identifiers. Each local identifier maps to a vector of environment IDs.
     * @param address_updated A reference to a map that tracks whether each local
     * identifier has been updated. The map's keys are local address identifiers, and the values are
     * boolean flags indicating update status.
     * @param rules_per_local A reference to a map that stores the enforcement rules for each local
     * address identifier. The map's keys are local identifiers, and the values are the
     * corresponding rules as strings.
     */
    void build_enforcement_rule (const std::string& app_name,
        const std::unordered_map<std::string, std::vector<int>>& local_to_envs,
        std::unordered_map<std::string, bool>& address_updated,
        std::unordered_map<std::string, std::string>& rules_per_local);

    /**
     * @brief Handles invalid statistics for the given local address and updates the list of
     * sessions to delete.
     *
     * This function processes invalid statistics associated with the specified local address
     * and determines which sessions need to be removed. The sessions to be deleted are added
     * to the provided list.
     *
     * @param local_controller_address The local address associated with the invalid statistics.
     * @param sessions_to_delete A reference to a list where the sessions to be deleted will be
     * stored.
     */
    void handle_invalid_stats (const std::string& local_controller_address,
        std::list<std::string>& sessions_to_delete);

    /**
     * @brief Removes stage from the control logic.
     *
     * @param stage_name_env The name and env of the stage to be removed.
     */
    void remove_stage (const std::string& stage_name_env);

public:
    /**
     * @brief Constructor for CoreControlApplication.
     * @param cycle_sleep_time Amount of time (in microseconds) that a feedback-loop cycle should
     * take.
     * @param policies A map of policies where each policy is defined by an integer key and a nested
     * map of operation names to limits.
     * @param total_jobs Total number of jobs expected to be managed by the application.
     * @param jobs_name_to_priority A map associating job names with their priority levels.
     */
    CoreControlApplication (const uint64_t& cycle_sleep_time,
        std::unordered_map<int, std::unordered_map<std::string, uint64_t>> policies,
        int total_jobs,
        std::unordered_map<std::string, int> jobs_name_to_priority);

    /**
     * @brief Destructor for CoreControlApplication.
     */
    ~CoreControlApplication () override;

    /**
     * @brief Function call operator to execute the control application logic.
     */
    void operator() () override;

    /**
     * @brief Initializes the control application with a set of original active operations, and
     * starts its thread.
     * @param original_active_ops A set of strings representing the names of original active
     * operations.
     */
    void initialize (std::unordered_set<std::string> original_active_ops);

    /**
     * @brief Registers a stage session with the control application.
     * @param request Pointer to the StageInfoConnect object containing session details.
     * @param controller_session Shared pointer to the Session object representing the controller
     * session.
     * @param rebuild Indicates if the stage session is being registered as part of a rebuild
     * process (after a failover): the stage is registered as active directly, instead of being
     * queued.
     */
    void register_stage_session (const StageInfoConnect* request,
        std::shared_ptr<Session> controller_session,
        bool rebuild = false);

    /**
     * @brief Logs the statistics of the control application.
     */
    void log_stats ();

    /**
     * @brief Stops the feedback loop of the control application.
     */
    void stop_feedback_loop () override;

    /**
     * @brief Marks controller as primary, which starts the feedback loop.
     */
    void set_as_primary_controller () override;

    /**
     * @brief Collects statistics from controller through gRPC.
     * @param reply Pointer to a StatsGlobalAggregatedMap object to store the aggregated statistics.
     * @return Status indicating the success or failure of the operation.
     */
    Status CollectCoreControllerStatistics (
        controllers_grpc_interface::StatsGlobalAggregatedMap* reply);

    /**
     * @brief Collects statistics from controller and returns them.
     * @return StageResponseStatController object containing the aggregated statistics.
     */
    StageResponseStatController CollectCoreControllerStatistics ();

    /**
     * @brief Enforces system limits based on the provided operations limits through gRPC.
     * @param context Pointer to the server context for the request.
     * @param request Pointer to the OperationsLimits object containing the limits to enforce.
     * @param reply Pointer to the ACK object to store the acknowledgment of the operation.
     * @return Status indicating the success or failure of the operation.
     */
    Status EnforceSystemLimit (ServerContext* context,
        const controllers_grpc_interface::OperationsLimits* request,
        controllers_grpc_interface::ACK* reply);

    /**
     * @brief Enforces system limits based on the provided map of limits per operation.
     * @param limits_per_operation A map of operation names to their respective limits.
     */
    void EnforceSystemLimit (std::unordered_map<std::string, uint64_t>& limits_per_operation);
};

} // namespace shio

#endif // SHIO_CORE_CONTROL_APPLICATION_HPP
