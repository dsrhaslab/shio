/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_JOBS_CONFIG_FILE_PARSER_HPP
#define SHIO_JOBS_CONFIG_FILE_PARSER_HPP

#include <shio/utils/options.hpp>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace shio {

/**
 * JobsConfigFileParser class.
 * JobsConfigFileParser processes the jobs configuration file (YAML with "total_jobs" and a
 * "jobs" list of {name, priority} entries).
 * Currently, the JobsConfigFileParser class contains the following variables:
 * - total_jobs: total number of jobs.
 * - jobs_name_to_priority: maps a job name to its priority.
 */
class JobsConfigFileParser {

public:
    int total_jobs;
    std::unordered_map<std::string, int> jobs_name_to_priority;

    /**
     * JobsConfigFileParser parameterized constructor. Loads and parses the jobs configuration file.
     * @param path Path to the jobs configuration file.
     */
    JobsConfigFileParser (const std::string& path);

    /**
     * JobsConfigFileParser default destructor.
     */
    ~JobsConfigFileParser ();

private:
    /**
     * process_jobs: Process jobs that a controller is responsible for.
     * @param jobs YAML "jobs" node.
     */
    void process_jobs (YAML::Node jobs);
};
} // namespace shio

#endif // SHIO_JOBS_CONFIG_FILE_PARSER_HPP
