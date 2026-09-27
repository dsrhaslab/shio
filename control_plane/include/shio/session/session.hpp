/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_SESSION_HPP
#define SHIO_SESSION_HPP

#include "shio/networking/interface/local_interface.hpp"
#include "shio/networking/stage_response/stage_response.hpp"
#include "shio/networking/stage_response/stage_response_ack.hpp"
#include "shio/networking/stage_response/stage_response_handshake.hpp"

#include <shio/utils/logging.hpp>
#include <shio/utils/options.hpp>
#include <condition_variable>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <unistd.h>

namespace shio {

/**
 * Session class.
 * Base class for the sessions that connect a control application to a lower controller or data
 * plane stage. The control application submits rules to the submission queue; a session thread
 * (StartSession in the subclasses) dequeues them, sends them through its interface, and enqueues
 * the responses in the completion queue.
 * Currently, the Session class contains the following variables:
 * - session_id_: session Identifier.
 * - submission_queue_: queue that holds requests to submit to the lower controller/stage.
 * - submission_queue_lock_: mutex for concurrency control over submission_queue_.
 * - submission_queue_condition_: condition for submission_queue_.
 * - completion_queue_: queue that holds responses from the lower controller/stage.
 * - completion_queue_lock_: mutex for concurrency control over completion_queue_.
 * - completion_queue_condition_: condition for completion_queue_.
 * - working_session_: atomic bool that stores if session is active.
 */
class Session {

private:
    long session_id_;
    std::queue<std::string> submission_queue_;
    std::mutex submission_queue_lock_;
    std::condition_variable submission_queue_condition_;
    std::queue<std::unique_ptr<StageResponse>> completion_queue_;
    std::mutex completion_queue_lock_;
    std::condition_variable completion_queue_condition_;

    /**
     * EnqueueRuleInSubmissionQueue: Enqueue rule in the submission_queue_ in
     * string-based format.
     * @param rule Rule to be enqueued.
     */
    void EnqueueRuleInSubmissionQueue (const std::string& rule);

    /**
     * DequeueResponseFromCompletionQueue: Dequeue response from the
     * completion_queue_ in StageResponse format.
     * @return Smart pointer of a StageResponse object.
     */
    std::unique_ptr<StageResponse> DequeueResponseFromCompletionQueue ();

    /**
     * getSubmissionQueueSize: Get the total size of the submission_queue_ (not synchronized).
     * @return Return the size of the submission_queue_.
     */
    int getSubmissionQueueSize ();

protected:
    std::atomic<bool> working_session_;

    /**
     * DequeueRuleFromSubmissionQueue: Dequeue rule from the submission_queue_
     * in string-based format. Blocks until a rule is available or the session is removed.
     * @param rule Rule dequeued.
     * @return PStatus::OK() if the rule was successfully dequeued,
     * PStatus::Error() if the session was removed.
     */
    PStatus DequeueRuleFromSubmissionQueue (std::string& rule);

    /**
     * EnqueueResponseInCompletionQueue: Enqueue response in the completion_queue_
     * in StageResponse format.
     * @param response_object Smart pointer of a StageResponse object.
     */
    void EnqueueResponseInCompletionQueue (std::unique_ptr<StageResponse> response_object);

public:
    /**
     * Session default constructor (session identifier -1).
     */
    explicit Session ();

    /**
     * Session parameterized constructor.
     * @param id Session identifier.
     */
    Session (long id);

    /**
     * Session default destructor.
     */
    virtual ~Session ();

    /**
     * RemoveSession: Stop session execution. Wakes up the session thread if it is waiting for
     * a rule.
     */
    void RemoveSession ();

    /**
     * SessionIdentifier: Get session identifier.
     * @return Session identifier.
     */
    long SessionIdentifier () const;

    /**
     * SubmitRule: Emplace rules in the Session. This is the public
     * method that will be used by ControlApplication objects to submit
     * rules. Rules are enqueued in the submission_queue_ through the
     * EnqueueRuleInSubmissionQueue call. Concurrency control is already handled
     * in the EnqueueRuleInSubmissionQueue call (controlling concurrency here as
     * well could lead to a deadlock).
     * @param submission_rule Const value of the submission rule.
     * @return Returns PStatus::OK() (enqueueing cannot fail).
     */
    PStatus SubmitRule (const std::string& submission_rule);

    /**
     * GetResult: Pop result objects (StageResponse) from the Session. Blocks until a response is
     * available.
     * This is the public method that will be used by ControlApplication
     * objects to read received StageResponse of previously submitted requests.
     * StageResponses are dequeued from the completion_queue_ through the
     * DequeueResponseFromCompletionQueue call. Concurrency control is already
     * handled in the DequeueResponseFromCompletionQueue call (controlling
     * concurrency here as well could lead to a deadlock).
     * @return Returns smart pointer (std::unique_ptr) of a StageResponse
     * object, so the caller can unmarshall based on the Base or Derived class.
     */
    std::unique_ptr<StageResponse> GetResult ();
};

} // namespace shio
#endif // SHIO_SESSION_HPP
