/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_STAGE_RESPONSE_HPP
#define CHEFERD_STAGE_RESPONSE_HPP

#include <string>

namespace cheferd {

/**
 * StageResponse class.
 * Base class for all responses from both data plane stages and local controllers.
 * Currently, the StageResponse class contains the following variables:
 * - response_type_: type of response (e.g., STAGE_HANDSHAKE,
 * CREATE_ENF_RULE, COLLECT_DATA_METADATA_STATS, ...).
 */
class StageResponse {
public:
    int response_type_;

    /**
     * StageResponse default constructor.
     */
    StageResponse ();

    /**
     * StageResponse parameterized constructor.
     * @param response_type Type of response.
     */
    StageResponse (const int& response_type);

    /**
     * StageResponse default destructor.
     */
    virtual ~StageResponse ();

    /**
     * ResponseType: Get response's type.
     * @return Type of response (-1 for the base class; subclasses return response_type_).
     */
    virtual int ResponseType () const;

    /**
     * toString: Converts response to string.
     * @return Response in string format.
     */
    virtual std::string toString () const;
};
} // namespace cheferd

#endif // CHEFERD_STAGE_RESPONSE_HPP
