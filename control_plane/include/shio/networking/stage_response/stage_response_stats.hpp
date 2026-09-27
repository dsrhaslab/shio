/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_STAGE_RESPONSE_STATS_HPP
#define SHIO_STAGE_RESPONSE_STATS_HPP

#include "stage_response.hpp"

#include <memory>
#include <unordered_map>

namespace shio {

/**
 * StageResponseStats class.
 * StageResponseStats is used for responses that hold statistics from multiple data plane stages.
 * Currently, the StageResponseStats class contains the following variables:
 * - m_stats_ptr: container used for mapping a data plane stage to its collected statistics.
 */
class StageResponseStats : public StageResponse {

public:
    std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<StageResponse>>> m_stats_ptr;

    /**
     * StageResponseStats default constructor.
     */
    StageResponseStats ();

    /**
     * StageResponseStats parameterized constructor.
     * @param response_type Type of response.
     * @param mStats_ptr Container used for mapping a data plane stage to its collected statistics
     * (moved from).
     */
    StageResponseStats (const int& response_type,
        std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<StageResponse>>>&
            mStats_ptr);

    /**
     * StageResponseStats default destructor.
     */
    ~StageResponseStats () override;

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
} // namespace shio

#endif // SHIO_STAGE_RESPONSE_STATS_HPP
