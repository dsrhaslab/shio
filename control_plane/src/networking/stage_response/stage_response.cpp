/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/networking/stage_response/stage_response.hpp"

namespace shio {

// StageResponse default constructor.
StageResponse::StageResponse () = default;

// StageResponse parameterized constructor.
StageResponse::StageResponse (const int& response_type) : response_type_ { response_type }
{ }

// StageResponse default destructor.
StageResponse::~StageResponse () = default;

// ResponseType call. Get response's type.
int StageResponse::ResponseType () const
{
    return -1;
}

// toString call. Converts response to string.
std::string StageResponse::toString () const
{
    return "StageResponse::";
}

} // namespace shio
