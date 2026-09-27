/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_POLICY_FILE_PARSER_HPP
#define SHIO_POLICY_FILE_PARSER_HPP

#include <shio/utils/options.hpp>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace shio {

/**
 * PolicyFileParser class.
 * PolicyFileParser processes policy file.
 * Currently, the PolicyFileParser class contains the following variables:
 * - control_type: type of control that control application imposes (e.g., STATIC,
 * EQUISHARE, PRIORISHARE, PSFA).
 * - m_system_limits: maximum limit of the system per operation class (e.g., data_op, meta_op).
 * - m_priorities: limits per operation class, indexed by priority id.
 */
class PolicyFileParser {

public:
    ControlType control_type;
    std::unordered_map<std::string, uint64_t> m_system_limits;
    std::unordered_map<int, std::unordered_map<std::string, uint64_t>> m_priorities;

    /**
     * PolicyFileParser parameterized constructor. Loads and parses the policy file.
     * @param path Path to the policy file.
     */
    PolicyFileParser (const std::string& path);

    /**
     * PolicyFileParser default destructor.
     */
    ~PolicyFileParser ();

    /**
     * process_system_limits: Process system limits into m_system_limits.
     * @param system_limit YAML "system_limit" node (operation class -> limit).
     */
    void process_system_limits (YAML::Node system_limit);

    /**
     * process_priorities: Process priorities into m_priorities.
     * @param priorities YAML "priority" node (list of entries with an "id" and per-operation
     * class limits).
     */
    void process_priorities (YAML::Node priorities);
};
} // namespace shio

#endif // SHIO_POLICY_FILE_PARSER_HPP
