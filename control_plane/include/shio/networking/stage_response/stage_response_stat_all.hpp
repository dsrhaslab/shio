/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_STAGE_RESPONSE_STAT_ALL_HPP
#define CHEFERD_STAGE_RESPONSE_STAT_ALL_HPP

#include "stage_response.hpp"

#include <memory>
#include <unordered_map>

namespace cheferd {

/**
 * StageResponseStatAll class.
 * StageResponseStatAll is used for responses that hold all statistics of a single data plane
 * stage, mapped by operation type.
 * Currently, the StageResponseStatAll class contains the following variables:
 * - stage_name: name of the stage.
 * - stage_env: environment of the stage.
 * - all_total_rates: container used for mapping an operation type to its total rate.
 */
class StageResponseStatAll : public StageResponse {
public:
    std::string stage_name;
    std::string stage_env;
    std::shared_ptr<std::unordered_map<std::string, uint64_t>> all_total_rates;

    /**
     * StageResponseStatAll default constructor.
     */
    StageResponseStatAll ();

    /**
     * StageResponseStatAll parameterized constructor.
     * @param response_type Type of response.
     * @param allStats_ptr Container used for mapping a stat to its operation type (moved from).
     */
    StageResponseStatAll (const int& response_type,
        std::shared_ptr<std::unordered_map<std::string, uint64_t>>& allStats_ptr);

    /**
     * StageResponseStatAll parameterized constructor.
     * @param response_type Type of response.
     * @param stage_name Name of the stage.
     * @param stage_env Environment of the stage.
     * @param allStats_ptr Container used for mapping a stat to its operation type (moved from).
     */
    StageResponseStatAll (const int& response_type,
        const std::string& stage_name,
        const std::string& stage_env,
        std::shared_ptr<std::unordered_map<std::string, uint64_t>>& allStats_ptr);

    /**
     * StageResponseStatAll parameterized constructor (all_total_rates is left empty).
     * @param response_type Type of response.
     */
    StageResponseStatAll (const int& response_type);

    /**
     * StageResponseStatAll default destructor.
     */
    ~StageResponseStatAll () override;

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

#endif // CHEFERD_STAGE_RESPONSE_STAT_ALL_HPP
