/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_SUPERVISOR_CONTROL_APPLICATION_HPP
#define CHEFERD_SUPERVISOR_CONTROL_APPLICATION_HPP

#include "cheferd/session/core_controller_session.hpp"
#include "control_application.hpp"
#include "core_control_application.hpp"

#include <regex>

namespace cheferd {

/**
 * SupervisorControlApplication class.
 * The SupervisorControlApplication runs in the global controller and coordinates the entire
 * system: it collects usage statistics from the lower controllers and from its own core control
 * application, computes a limit per controller and operation (PSFA), and enforces them. The
 * global controller's own core control application is identified by the address "0.0.0.0".
 * Only runs its loop while the controller is primary.
 * Currently, the SupervisorControlApplication class contains the following variables:
 * - op_maximum_limit: container used for mapping operations to their maximum limits.
 * - priorities: container used for mapping priorities to their respective operations and limits.
 * - controller_sessions_: container used for mapping controller addresses to their sessions.
 * - controller_sessions_failure_counter: Tracks failure counts for controller sessions.
 * - m_core_control_application_ptr: pointer to the global controller's core control application
 * instance.
 * - controller_rates: container used for mapping controller addresses to their operation limits.
 * - controller_rates_mutex: mutex for concurrency control over controller_rates.
 * - nr_jobs_per_priority_and_controller: container used for mapping priorities to controller
 * addresses and job counts.
 * - usage_per_priority_and_controller: container used for storing for each supported operations its
 * priorities, controller addresses, and usage statistics.
 * - active_ops: set of active operations currently supported by the controller.
 */
class SupervisorControlApplication : public ControlApplication {

private:
    //<operation, limit>
    std::unordered_map<std::string, uint64_t> op_maximum_limit;
    //<priority, <operation, limit>>
    std::unordered_map<int, std::unordered_map<std::string, uint64_t>> priorities;
    //<controller_address, session>
    std::unordered_map<std::string, std::shared_ptr<Session>> controller_sessions_;
    std::unordered_map<std::string, int> controller_sessions_failure_counter;
    // in-local core control application instance
    CoreControlApplication* m_core_control_application_ptr;
    //<controller_address, <operation, limit>>
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> controller_rates;
    std::mutex controller_rates_mutex;
    //<priority, <controller_address, nr_jobs>>
    std::map<int, std::unordered_map<std::string, int>> nr_jobs_per_priority_and_controller;
    //<operation, <priority, <controller_address, usage>>>
    std::map<std::string, std::map<int, std::unordered_map<std::string, uint64_t>>>
        usage_per_priority_and_controller;
    std::unordered_set<std::string> active_ops;

    /**
     * execute_feedback_loop: Executes feedback loop.
     * This method is responsible for collecting statistics about each controller, computing
     * each controller limits, enforcing PSFA policies.
     */
    void execute_feedback_loop ();

    /**
     * collect_statistics_global: Sends the request and collects statistics from controllers
     * (and from the in-local core control application). Removes sessions that failed too many
     * times.
     */
    void collect_statistics_global ();

    /**
     * compute_controllers_limits: Computes the limits of each controller, for all active
     * operations (one thread per operation).
     */
    void compute_controllers_limits ();

    /**
     * compute_controller_limits_per_op: Computes the limit of each controller for an operation,
     * using PSFA (phase 1 and phase 2).
     * @param operation Operation to compute limits for.
     */
    void compute_controller_limits_per_op (const std::string& operation);

    /**
     * compute_equal_controllers: Assigns to all controllers the same equal rate. Currently unused.
     * @param operation Operation to compute limits for.
     * @param left_iops_per_operation Rate available for the operation.
     */
    void compute_equal_controllers (const std::string& operation,
        uint64_t& left_iops_per_operation);

    /**
     * compute_uniform_per_op: Assigns rates according to the number of jobs that controller is
     * managing. Currently unused.
     * @param operation Operation to compute limits for.
     * @param left_iops_per_operation Rate available for the operation.
     */
    void compute_uniform_per_op (const std::string& operation, uint64_t& left_iops_per_operation);

    /**
     * compute_psfa_per_op_phase1: Computes phase 1 of PSFA's algorithm for an operation: each
     * controller gets, per priority, the minimum between its demand (or usage plus a margin) and
     * its fair share, with a minimum of 100 MiB.
     * @param operation Operation to compute limits for.
     * @param app_usage Output: usage of each controller.
     * @param system_total_rate Output: total usage of all controllers.
     * @param left_iops_per_operation Rate available for the operation; decremented by the
     * assigned rates.
     */
    void compute_psfa_per_op_phase1 (const std::string operation,
        std::unordered_map<std::string, uint64_t>& app_usage,
        uint64_t& system_total_rate,
        uint64_t& left_iops_per_operation);

    /**
     * compute_psfa_per_op_phase2: Computes phase 2 of PSFA's algorithm for an operation: the
     * remaining rate is distributed among controllers proportionally to their usage.
     * @param operation Operation to compute limits for.
     * @param controller_usage Usage of each controller (from phase 1).
     * @param system_total_rate Total usage of all controllers (from phase 1).
     * @param left_iops_per_operation Rate left after phase 1.
     */
    void compute_psfa_per_op_phase2 (const std::string operation,
        std::unordered_map<std::string, uint64_t>& controller_usage,
        uint64_t system_total_rate,
        uint64_t left_iops_per_operation);

    /**
     * enforce_controller_rules: Send the computed limits to each controller (and to the in-local
     * core control application) and wait for their ACKs.
     */
    void enforce_controller_rules ();

    /**
     * sleep: Used to make control application main thread wait for the next loop.
     */
    void sleep () override;

    /**
     * collect_statistics_result_shared: Collects statistics from a shared controller session.
     * @param controller_session Shared pointer to the controller session.
     * @param controller_address Address of the local controller.
     * @param sessions_to_delete Output: sessions that failed controller_connect_attempt_tries
     * times in a row.
     */
    void collect_statistics_result_shared (const std::shared_ptr<Session>& controller_session,
        const std::string& controller_address,
        std::list<std::string>& sessions_to_delete);

    /**
     * collect_statistics_local_control_application: Collects statistics from the in-local core
     * control application (stored under the address "0.0.0.0").
     * @param sessions_to_delete List of sessions to be deleted (unused).
     */
    void collect_statistics_local_control_application (std::list<std::string>& sessions_to_delete);

public:
    /**
     * SupervisorControlApplication parameterized constructor.
     * @param rules_ptr Pointer to the vector of housekeeping rules.
     * @param cycle_sleep_time Control feedback loop time (in microseconds).
     * @param op_system_limits Map of operations to their system limits.
     * @param policies Map of policies where each priority is defined by an integer key and a nested
     * map of operation names to limits.
     * @param core_control_application Pointer to the in-local core control application instance.
     */
    SupervisorControlApplication (std::vector<std::string>* rules_ptr,
        const uint64_t& cycle_sleep_time,
        std::unordered_map<std::string, uint64_t> op_system_limits,
        std::unordered_map<int, std::unordered_map<std::string, uint64_t>> policies,
        CoreControlApplication* core_control_application);

    /**
     * SupervisorControlApplication default destructor.
     */
    ~SupervisorControlApplication () override;

    /**
     * operator: Used to initiate the control application feedback loop execution.
     */
    void operator() () override;

    /**
     * initialize: Starts thread to run control application.
     */
    void initialize ();

    /**
     * register_controller_session: Register a new lower controller.
     * @param local_controller_address Local controller identifier.
     * @param controller_session Shared pointer to the controller session.
     */
    void register_controller_session (const std::string& local_controller_address,
        std::shared_ptr<Session> controller_session);

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

#endif // CHEFERD_SUPERVISOR_CONTROL_APPLICATION_HPP
