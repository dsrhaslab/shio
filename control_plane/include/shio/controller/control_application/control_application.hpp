/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_CONTROL_APPLICATION_HPP
#define CHEFERD_CONTROL_APPLICATION_HPP

#include "cheferd/networking/stage_response/stage_response_ack.hpp"
#include "cheferd/networking/stage_response/stage_response_handshake.hpp"
#include "cheferd/networking/stage_response/stage_response_stat.hpp"
#include "cheferd/networking/stage_response/stage_response_stat_all.hpp"
#include "cheferd/utils/logging.hpp"

#include <array>
#include <atomic>
#include <memory>

using namespace std::chrono;

namespace cheferd {

/**
 * Struct StageInfo.
 * Container that holds detailed information related to a data plane stage.
 * - m_stage_name: name of the stage (job).
 * - m_stage_env: environment of the stage.
 * - m_local_address: address of the local controller the stage is connected to.
 */
struct StageInfo {
    std::string m_stage_name;
    std::string m_stage_env;
    std::string m_local_address;
};

/**
 * ControlApplication class.
 * The ControlApplication class serves as base class for the intelligence/control component of both
 * core and local control applications. It defines core variables and functions that are required
 * for the control component. Currently, the ControlApplication class contains the following
 * variables:
 * - m_feedback_loop_sleep_time: defines how long a feedback loop should take (in microseconds).
 * - working_application_: atomic value that marks if the control application is currently active.
 * - is_primary_: atomic value that marks if the control application is the primary one.
 * - housekeeping_rules_ptr_: container used to store housekeeping rules that should be imposed at
 * the data plane stages.
 */
class ControlApplication {

protected:
    const uint64_t m_feedback_loop_sleep_time;
    std::atomic<bool> working_application_;
    std::atomic<bool> is_primary_;

    /**
     * sleep: Used to make control application main thread wait for the next loop.
     */
    virtual void sleep () = 0;

public:
    std::vector<std::string>* housekeeping_rules_ptr_;

    /**
     * ControlApplication default constructor.
     */
    ControlApplication () :
        m_feedback_loop_sleep_time { option_default_control_application_sleep },
        working_application_ { true },
        is_primary_ { false },
        housekeeping_rules_ptr_ { new std::vector<std::string> () }
    { }

    /**
     * ControlApplication parameterized constructor.
     * @param rules_ptr Pointer to the housekeeping rules container (not owned).
     * @param cycle_sleep_time Control feedback loop time (in microseconds).
     */
    explicit ControlApplication (std::vector<std::string>* rules_ptr,
        const uint64_t& cycle_sleep_time) :
        m_feedback_loop_sleep_time { cycle_sleep_time },
        working_application_ { true },
        is_primary_ { false },
        housekeeping_rules_ptr_ { rules_ptr }
    { }

    /**
     * ControlApplication parameterized constructor.
     * @param rules_ptr Pointer to the housekeeping rules container (not owned).
     */
    explicit ControlApplication (std::vector<std::string>* rules_ptr) :
        m_feedback_loop_sleep_time { option_default_control_application_sleep },
        working_application_ { true },
        is_primary_ { false },
        housekeeping_rules_ptr_ { rules_ptr }
    { }

    /**
     * ControlApplication parameterized constructor.
     * @param cycle_sleep_time Control feedback loop time (in microseconds).
     */
    explicit ControlApplication (const uint64_t& cycle_sleep_time) :
        m_feedback_loop_sleep_time { cycle_sleep_time },
        working_application_ { true },
        is_primary_ { false },
        housekeeping_rules_ptr_ { new std::vector<std::string> () }
    { }

    /**
     * ControlApplication default destructor.
     */
    virtual ~ControlApplication () = default;

    /**
     * operator: Used to initiate the control application feedback loop execution.
     */
    virtual void operator() () = 0;

    /**
     * feedback_loop_is_active: Checks if the feedback loop is active.
     * @return True if the feedback loop is active, false otherwise.
     */
    virtual bool feedback_loop_is_active ()
    {
        return working_application_.load ();
    }

    /**
     * stop_feedback_loop: Stops the feedback loop from executing.
     */
    virtual void stop_feedback_loop () = 0;

    /**
     * set_as_primary_controller: Marks the control application as primary.
     * This method is used to indicate that this control application instance is the primary one.
     */
    virtual void set_as_primary_controller ()
    {
        Logging::log_info ("ControlApplication: Setting as primary controller.");
        is_primary_ = true;
    }
};

} // namespace cheferd

#endif // CHEFERD_CONTROL_APPLICATION_HPP
