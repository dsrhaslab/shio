/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/networking/stage_response/stage_response_stat.hpp"

#include <shio/session/session.hpp>

namespace shio {

// Session default constructor.
Session::Session () : session_id_ { -1 }
{ }

// Session parameterized constructor.
Session::Session (long id) : session_id_ { id }
{ }

// Session default destructor.
Session::~Session () = default;

// SessionIdentifier call. Get session identifier.
long Session::SessionIdentifier () const
{
    return session_id_;
}

// RemoveSession call. Stop session execution.
void Session::RemoveSession ()
{
    working_session_ = false;
    EnqueueRuleInSubmissionQueue ("");
}

// EnqueueRuleInSubmissionQueue call. Enqueue rule in the submission_queue_ in
// string-based format.
void Session::EnqueueRuleInSubmissionQueue (const std::string& rule)
{
    std::unique_lock<std::mutex> lock_t { submission_queue_lock_ };
    submission_queue_.emplace (rule);
    submission_queue_condition_.notify_one ();
}

// DequeueRuleFromSubmissionQueue call. Dequeue rule from the submission_queue_
// in string-based format.
PStatus Session::DequeueRuleFromSubmissionQueue (std::string& rule)
{
    std::unique_lock<std::mutex> lock_t { submission_queue_lock_ };
    PStatus status_t = PStatus::Error ();

    while (working_session_.load () && submission_queue_.empty ()) {
        submission_queue_condition_.wait (lock_t);
    }

    if (working_session_.load ()) {
        rule = submission_queue_.front ();
        submission_queue_.pop ();
        status_t = PStatus::OK ();
    }

    return status_t;
}

// EnqueueResponseInCompletionQueue call. Enqueue response in the completion_queue_
// in StageResponse format.
void Session::EnqueueResponseInCompletionQueue (std::unique_ptr<StageResponse> response_object)
{
    std::unique_lock<std::mutex> lock_t { completion_queue_lock_ };
    completion_queue_.emplace (std::move (response_object));
    completion_queue_condition_.notify_one ();
}

// DequeueResponseFromCompletionQueue call. Dequeue response from the
// completion_queue_ in StageResponse format.
std::unique_ptr<StageResponse> Session::DequeueResponseFromCompletionQueue ()
{
    std::unique_lock<std::mutex> lock_t { completion_queue_lock_ };

    while (completion_queue_.empty ()) {
        completion_queue_condition_.wait (lock_t);
    }

    std::unique_ptr<StageResponse> response_t = std::move (completion_queue_.front ());
    completion_queue_.pop ();

    return response_t;
}

// getSubmissionQueueSize call. Get the total size of the submission_queue.
int Session::getSubmissionQueueSize ()
{
    return submission_queue_.size ();
}

// SubmitRule call. Submit rules to the Session.
PStatus Session::SubmitRule (const std::string& submission_rule)
{
    PStatus status_t = PStatus::Error ();

    EnqueueRuleInSubmissionQueue (submission_rule);
    status_t = PStatus::OK ();

    return status_t;
}

// GetResult call. Pop result objects (StageResponse) from the Session.
std::unique_ptr<StageResponse> Session::GetResult ()
{
    return DequeueResponseFromCompletionQueue ();
}

} // namespace shio
