/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_STAGE_RESPONSE_STAT_HPP
#define CHEFERD_STAGE_RESPONSE_STAT_HPP

#include "stage_response.hpp"

namespace cheferd {

/**
 * StageResponseStat class.
 * StageResponseStat is used for responses that hold the data and metadata rates of a single
 * data plane stage.
 * Currently, the StageResponseStat class contains the following variables:
 * - d_total_rate: data plane stage data total rate.
 * - m_total_rate: data plane stage metadata total rate.
 */
class StageResponseStat : public StageResponse {
private:
    double d_total_rate;
    double m_total_rate;

public:
    /**
     * StageResponseStat default constructor.
     */
    StageResponseStat ();

    /**
     * StageResponseStat parameterized constructor.
     * @param response_type Type of response.
     * @param d_total_rate Data plane stage data total rate.
     * @param m_total_rate Data plane stage metadata total rate.
     */
    StageResponseStat (const int& response_type,
        const double& d_total_rate,
        const double& m_total_rate);

    /**
     * StageResponseStat default destructor.
     */
    ~StageResponseStat () override;

    /**
     * ResponseType: Get response's type.
     * @return Type of response.
     */
    int ResponseType () const override;

    /**
     * get_d_total_rate: Get data plane stage data total rate.
     * @return Data plane stage's data total rate.
     */
    double get_d_total_rate () const;

    /**
     * get_m_total_rate: Get data plane stage metadata total rate.
     * @return Data plane stage's metadata total rate.
     */
    double get_m_total_rate () const;

    /**
     * toString: Converts response to string.
     * @return Response in string format (metadata rate in MiB).
     */
    std::string toString () const override;
};
} // namespace cheferd

#endif // CHEFERD_STAGE_RESPONSE_STAT_HPP
