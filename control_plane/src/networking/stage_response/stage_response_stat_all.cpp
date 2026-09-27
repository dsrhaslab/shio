/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/networking/stage_response/stage_response_stat_all.hpp"

namespace shio {

// StageResponseStatAll default constructor.
StageResponseStatAll::StageResponseStatAll ()
{ }

// StageResponseStatAll parameterized constructor.
StageResponseStatAll::StageResponseStatAll (const int& response_type,
    std::shared_ptr<std::unordered_map<std::string, uint64_t>>& allStats_ptr) :
    StageResponse { response_type },
    all_total_rates { std::move (allStats_ptr) }
{ }

// StageResponseStatAll parameterized constructor.
StageResponseStatAll::StageResponseStatAll (const int& response_type,
    const std::string& stage_name,
    const std::string& stage_env,
    std::shared_ptr<std::unordered_map<std::string, uint64_t>>& allStats_ptr) :
    StageResponse { response_type },
    stage_name { stage_name },
    stage_env { stage_env },
    all_total_rates { std::move (allStats_ptr) }
{ }

// StageResponseStatAll parameterized constructor.
StageResponseStatAll::StageResponseStatAll (const int& response_type) :
    StageResponse { response_type },
    all_total_rates { nullptr }
{ }

// StageResponseStatAll default destructor.
StageResponseStatAll::~StageResponseStatAll () = default;

// ResponseType call. Get response's type.
int StageResponseStatAll::ResponseType () const
{
    return response_type_;
}

// toString call. Converts response to string.
std::string StageResponseStatAll::toString () const
{
    std::string return_value_t = "Stats {";
    for (auto& stat : *all_total_rates) {
        return_value_t += "[" + stat.first + ": " + std::to_string (stat.second) + "]";
    }
    return_value_t += "}";

    return return_value_t;
}

} // namespace shio