/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#include "cheferd/networking/stage_response/stage_response_stat_controller.hpp"

namespace cheferd {

// StageResponseStatController default constructor.
StageResponseStatController::StageResponseStatController () noexcept :
    StageResponse (),
    controller_stats ()
{ }

// StageResponseStatController parameterized constructor.
StageResponseStatController::StageResponseStatController (const int& response_type) noexcept :
    StageResponse (response_type),
    controller_stats ()
{ }

// StageResponseStatController default destructor.
StageResponseStatController::~StageResponseStatController () = default;

// ResponseType call. Get response's type.
int StageResponseStatController::ResponseType () const
{
    return response_type_;
}

// toString call. Converts response to string.
std::string StageResponseStatController::toString () const
{
    std::string result = "StageResponseStatController: \n";
    result += "Response Type: " + std::to_string (response_type_) + "\n";
    result += "Controller Stats: \n";
    for (const auto& [operation, stats_rated] : controller_stats.per_operation_usage) {
        result += "Operation: " + operation + "\n";
        for (const auto& [priority, usage] : stats_rated.usage_per_priority) {
            result += "Priority: " + std::to_string (priority)
                + ", Usage: " + std::to_string (usage) + "\n";
        }
    }
    for (const auto& [priority, counter] : controller_stats.counter_per_priority) {
        result += "Priority: " + std::to_string (priority)
            + ", Counter: " + std::to_string (counter) + "\n";
    }
    return result;
}

} // namespace cheferd