/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_STAGE_RESPONSE_STAT_CONTROLLER_HPP
#define CHEFERD_STAGE_RESPONSE_STAT_CONTROLLER_HPP

#include "stage_response.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace cheferd {

/**
 * StageResponseStatController class.
 * StageResponseStatController is used for responses that hold the aggregated statistics of a
 * lower controller.
 * Currently, the StageResponseStatController class contains the following variables:
 * - controller_stats: usage per operation and priority, and number of jobs per priority.
 */
class StageResponseStatController : public StageResponse {

public:
    // Usage of an operation, per priority.
    struct StatsRated {
        std::unordered_map<int32_t, uint64_t> usage_per_priority;
    };

    // Aggregated statistics: usage per operation, and counter per priority.
    struct StatsGlobalAggregatedMap {
        std::unordered_map<std::string, StatsRated> per_operation_usage;
        std::unordered_map<int32_t, uint64_t> counter_per_priority;
    };

    StatsGlobalAggregatedMap controller_stats;

    /**
     * StageResponseStatController default constructor.
     */
    StageResponseStatController () noexcept;

    /**
     * StageResponseStatController parameterized constructor.
     * @param response_type Type of response.
     */
    StageResponseStatController (const int& response_type) noexcept;

    /**
     * StageResponseStatController default destructor.
     */
    ~StageResponseStatController () override;

    /**
     * ResponseType: Get response's type.
     * @return Type of response.
     */
    int ResponseType () const override;

    /**
     * toString: Converts response to string.
     * @return Response in string format.
     */
    std::string toString () const override;
};

} // namespace cheferd

#endif // CHEFERD_STAGE_RESPONSE_STAT_CONTROLLER_HPP
