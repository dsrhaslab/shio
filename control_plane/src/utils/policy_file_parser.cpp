/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/utils/policy_file_parser.hpp"

#include "shio/utils/logging.hpp"

namespace shio {

// PolicyFileParser parameterized constructor.
PolicyFileParser::PolicyFileParser (const std::string& path) : m_system_limits {}, m_priorities {}
{
    YAML::Node root_node = YAML::LoadFile (path);

    if (root_node["policy"]) {
        int policy = root_node["policy"].as<int> ();
        if (policy == 1) {
            control_type = ControlType::STATIC;
            process_priorities (root_node["priority"]);
        } else if (policy == 2) {
            control_type = ControlType::EQUISHARE;
            process_system_limits (root_node["system_limit"]);
        } else if (policy == 3) {
            control_type = ControlType::PRIORISHARE;
            process_system_limits (root_node["system_limit"]);
            process_priorities (root_node["priority"]);
        } else if (policy == 4) {
            control_type = ControlType::PSFA;
            process_system_limits (root_node["system_limit"]);
            process_priorities (root_node["priority"]);
        } else {
            Logging::log_error ("Controller in policy option not supported!");
        }
    }

    // Logging::log_debug ("PolicyFileParser default constructor.");
}

// PolicyFileParser default destructor.
PolicyFileParser::~PolicyFileParser ()
{
    Logging::log_debug ("PolicyFileParser default destructor.");
}

// process_system_limits call. Process system limits.
void PolicyFileParser::process_system_limits (YAML::Node system_limit)
{
    for (YAML::const_iterator it = system_limit.begin (); it != system_limit.end (); ++it) {
        m_system_limits.emplace (it->first.as<std::string> (), it->second.as<uint64_t> ());
    }

    // m_system_limits.emplace("data_op", system_limit["data_op"].as<long>());
    // m_system_limits.emplace("meta_op", system_limit["meta_op"].as<long>());
};

// process_priorities call. Process priorities.
void PolicyFileParser::process_priorities (YAML::Node priorities)
{
    for (YAML::iterator it = priorities.begin (); it != priorities.end (); ++it) {
        YAML::Node priority = *it;

        std::unordered_map<std::string, uint64_t> priorities_per_id;
        int priority_id = 0;
        for (YAML::const_iterator it = priority.begin (); it != priority.end (); ++it) {
            if (std::strcmp (it->first.as<std::string> ().c_str (), "id") == 0) {
                priority_id = it->second.as<uint64_t> ();
            } else {
                priorities_per_id.emplace (it->first.as<std::string> (),
                    it->second.as<uint64_t> ());
            }
        }

        /*std::unordered_map<std::string, long> priorities_per_id;
        priorities_per_id.emplace("data_op", priority["data_op"].as<long>());
        priorities_per_id.emplace("meta_op", priority["meta_op"].as<long>());
        */
        m_priorities.emplace (priority_id, priorities_per_id);
    }
};

} // namespace shio
