/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#include "cheferd/utils/config_file_parser.hpp"

#include "cheferd/utils/logging.hpp"

namespace cheferd {

// ConfigFileParser default constructor.
ConfigFileParser::ConfigFileParser ()
{
    // Logging::log_debug ("ConfigFileParser default constructor.");
}

// ConfigFileParser default destructor.
ConfigFileParser::~ConfigFileParser ()
{
    Logging::log_debug ("ConfigFileParser default destructor.");
}

// select_default_housekeeping_rule call. Selects default housekeeping file if not defined
// in config file.
void ConfigFileParser::select_default_housekeeping_rule ()
{
    housekeeping_rules_file = cheferd::option_housekeeping_rules_default;
}

// process_l1_controller_config call. Process l1 (global) controller configuration.
void ConfigFileParser::process_l1_controller_config (YAML::Node root_node)
{
    if (root_node["own_down_address"]) {
        own_down_address = root_node["own_down_address"].as<std::string> ();
    } else {
        Logging::log_error ("Own controller down address needs to be provided!");
    }

    if (root_node["housekeeping_rules_file"]) {
        housekeeping_rules_file = root_node["housekeeping_rules_file"].as<std::string> ();
    } else {
        select_default_housekeeping_rule ();
        Logging::log_info ("Using default housekeeping rules!");
    }

    if (root_node["policies_rules_file"]) {
        policies_rules_file = root_node["policies_rules_file"].as<std::string> ();
    } else {
        Logging::log_error ("Policies rules file path needs to be provided!");
    }

    if (root_node["system_admin_rules_file"]) {
        system_admin_rules_file = root_node["system_admin_rules_file"].as<std::string> ();
    } else {
        Logging::log_info ("System admin rules file path not provided!");
    }

    if (root_node["jobs_config_file"]) {
        jobs_config_file = root_node["jobs_config_file"].as<std::string> ();
    }
}

// process_l2_controller_config call. Process l2 (cluster) controller configuration.
void ConfigFileParser::process_l2_controller_config (YAML::Node root_node)
{
    if (root_node["upper_address"]) {
        upper_address = root_node["upper_address"].as<std::string> ();
    } else {
        Logging::log_error ("Upper controller address needs to be provided!");
    }

    if (root_node["own_upper_address"]) {
        own_upper_address = root_node["own_upper_address"].as<std::string> ();
    } else {
        Logging::log_error ("Own controller upper address needs to be provided!");
    }

    if (root_node["own_down_address"]) {
        own_down_address = root_node["own_down_address"].as<std::string> ();
    } else {
        Logging::log_error ("Own controller down address needs to be provided!");
    }

    if (root_node["policies_rules_file"]) {
        policies_rules_file = root_node["policies_rules_file"].as<std::string> ();
    } else {
        Logging::log_error ("Policies rules file path needs to be provided!");
    }

    if (root_node["jobs_config_file"]) {
        jobs_config_file = root_node["jobs_config_file"].as<std::string> ();
    }
}

// process_l3_controller_config call. Process l3 (local) controller configuration.
void ConfigFileParser::process_l3_controller_config (YAML::Node root_node)
{
    if (root_node["upper_address"]) {
        upper_address = root_node["upper_address"].as<std::string> ();
    } else {
        Logging::log_error ("Upper controller address needs to be provided!");
    }

    if (root_node["own_upper_address"]) {
        own_upper_address = root_node["own_upper_address"].as<std::string> ();
    } else {
        Logging::log_error ("Own controller address needs to be provided!");
    }
}

// process_config_file call. Process configuration file.
void ConfigFileParser::process_config_file (const std::string& path)
{
    YAML::Node root_node = YAML::LoadFile (path);

    if (root_node["controller"]) {
        std::string controller = root_node["controller"].as<std::string> ();
        if (controller == "global") {
            controller_type = ControllerType::GLOBAL;
            process_l1_controller_config (root_node);
        } else if (controller == "cluster") {
            controller_type = ControllerType::CLUSTER;
            process_l2_controller_config (root_node);
        } else if (controller == "local") {
            controller_type = ControllerType::LOCAL;
            process_l3_controller_config (root_node);
            own_down_address = own_upper_address;
        } else {
            Logging::log_error (
                "Controller in config option not supported (choose global, cluster or local)!");
        }
    }

    if (root_node["role"]) {
        std::string role = root_node["role"].as<std::string> ();
        if (role == "primary") {
            controller_role = ControllerRole::PRIMARY;
        } else if (role == "secondary") {
            controller_role = ControllerRole::SECONDARY;
        } else {
            Logging::log_error (
                "Controller role in config option not supported (choose primary or secondary)!");
        }
        if (!root_node["twin_controller_address"]) {
            Logging::log_error ("Twin controller address needs to be provided!");
        } else {
            twin_controller_address = root_node["twin_controller_address"].as<std::string> ();
        }

        if (!root_node["own_internal_address"]) {
            Logging::log_error ("Own controller internal address needs to be provided!");
        } else {
            own_internal_address = root_node["own_internal_address"].as<std::string> ();
        }
    }
}

} // namespace cheferd
