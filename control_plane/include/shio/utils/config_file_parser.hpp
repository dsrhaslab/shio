/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_CONFIG_FILE_PARSER_HPP
#define SHIO_CONFIG_FILE_PARSER_HPP

#include <shio/utils/options.hpp>
#include <yaml-cpp/yaml.h>

namespace shio {

/**
 * ConfigFileParser class.
 * ConfigFileParser processes configuration file.
 * Currently, the ConfigFileParser class contains the following variables:
 * - controller_type: type of controller (e.g., GLOBAL, CLUSTER, LOCAL).
 * - controller_role: role of the controller (e.g., PRIMARY, SECONDARY).
 * - own_down_address: address on which the controller accepts lower controllers/stages.
 * - twin_controller_address: address of the twin (primary/secondary) controller.
 * - own_internal_address: internal address of the controller, used by its twin.
 * - own_upper_address: address on which the controller receives requests from the upper
 * controller.
 * - upper_address: address of the upper controller.
 * - housekeeping_rules_file: path to file that contains housekeeping rules.
 * - policies_rules_file:  path to file that contains policy rules.
 * - system_admin_rules_file:  path to file that contains system admin rules.
 * - jobs_config_file: path to file that contains jobs configuration.
 */
class ConfigFileParser {

public:
    ControllerType controller_type;
    ControllerRole controller_role;

    std::string own_down_address;
    std::string twin_controller_address;
    std::string own_internal_address;
    std::string own_upper_address;
    std::string upper_address;
    std::string housekeeping_rules_file;
    std::string policies_rules_file;
    std::string system_admin_rules_file;
    std::string jobs_config_file;

    /**
     * process_config_file. Process configuration file.
     * @param path Path to configuration file.
     */
    void process_config_file (const std::string& path);

    /**
     * ConfigFileParser default constructor.
     */
    ConfigFileParser ();

    /**
     * ConfigFileParser default destructor.
     */
    ~ConfigFileParser ();

private:
    /**
     * select_default_housekeeping_rule: Selects default housekeeping file if not defined
     * in config file.
     */
    void select_default_housekeeping_rule ();

    /**
     * process_l1_controller_config: Process l1 (global) controller configuration.
     * @param root_node YAML root node.
     */
    void process_l1_controller_config (YAML::Node root_node);

    /**
     * process_l2_controller_config: Process l2 (cluster) controller configuration.
     * @param root_node YAML root node.
     */
    void process_l2_controller_config (YAML::Node root_node);

    /**
     * process_l3_controller_config: Process l3 (local) controller configuration.
     * @param root_node YAML root node.
     */
    void process_l3_controller_config (YAML::Node root_node);
};
} // namespace shio

#endif // SHIO_CONFIG_FILE_PARSER_HPP
