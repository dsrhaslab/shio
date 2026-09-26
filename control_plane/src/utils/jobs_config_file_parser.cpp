/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#include "cheferd/utils/jobs_config_file_parser.hpp"

#include "cheferd/utils/logging.hpp"

namespace cheferd {

// JobsConfigFileParser parameterized constructor.
JobsConfigFileParser::JobsConfigFileParser (const std::string& path) : jobs_name_to_priority {}
{
    YAML::Node root_node = YAML::LoadFile (path);

    if (root_node["total_jobs"]) {
        total_jobs = root_node["total_jobs"].as<int> ();
    }

    if (root_node["jobs"]) {
        process_jobs (root_node["jobs"]);
    }
}

// JobsConfigFileParser default destructor.
JobsConfigFileParser::~JobsConfigFileParser ()
{
    Logging::log_debug ("JobsConfigFileParser default destructor.");
}

// process_jobs call. Process jobs that a controller is responsible for.
void JobsConfigFileParser::process_jobs (YAML::Node jobs)
{
    for (YAML::const_iterator it = jobs.begin (); it != jobs.end (); ++it) {
        jobs_name_to_priority.emplace ((*it)["name"].as<std::string> (),
            (*it)["priority"].as<int> ());
    }
}

} // namespace cheferd
