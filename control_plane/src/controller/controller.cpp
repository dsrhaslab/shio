/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/controller/control_application/local_control_application.hpp"
#include "shio/controller/control_application/supervisor_control_application.hpp"
#include "shio/networking/connection_manager/core_connection_manager.hpp"
#include "shio/networking/connection_manager/local_connection_manager.hpp"
#include "shio/networking/connection_manager/supervisor_connection_manager.hpp"

#include <shio/controller/controller.hpp>
#include <shio/utils/jobs_config_file_parser.hpp>
#include <shio/utils/logging.hpp>
#include <shio/utils/policy_file_parser.hpp>
#include <shio/utils/policy_generator.hpp>
#include <shio/utils/rules_file_parser.hpp>

namespace shio {

// Controller parameterized constructor.
Controller::Controller (ConfigFileParser config_file_parser) :
    m_controller_type { config_file_parser.controller_type },
    m_housekeeping_rules {}
{
    switch (m_controller_type) {
        case ControllerType::GLOBAL:
            launchGlobalController (config_file_parser);
            handleControllerRole (config_file_parser);
            break;
        case ControllerType::CLUSTER:
            launchClusterController (config_file_parser);
            handleControllerRole (config_file_parser);
            break;
        case ControllerType::LOCAL:
            launchLocalController (config_file_parser);
            break;
        default:
            Logging::log_error ("Controller:: Type of controller not supported.");
    }

    // spawn ControlAlgorithm to attach LocalControllerSessions and execute its
    // control algorithm ...
    SpawnControlApplication ();

    // start connection manager to receive connections
    SpawnConnectionManager (config_file_parser);
}

// Controller default destructor.
Controller::~Controller () = default;

// launchGlobalController call. Creates the GlobalController control application.
void Controller::launchGlobalController (const ConfigFileParser config_file_parser)
{
    std::string housekeeping_rules_file = config_file_parser.housekeeping_rules_file;
    RegisterHousekeepingRules (housekeeping_rules_file);
    ControllerRole controller_role = config_file_parser.controller_role;

    std::string policies_rules_file = config_file_parser.policies_rules_file;
    PolicyFileParser policy_parser { policies_rules_file };
    // ControlType control_type = policy_parser.control_type;
    auto m_system_limits = policy_parser.m_system_limits;
    auto m_policies_rules = policy_parser.m_priorities;

    JobsConfigFileParser jobsConfigFileParser (config_file_parser.jobs_config_file);
    auto m_total_jobs = jobsConfigFileParser.total_jobs;
    auto m_jobs_name_to_priority = jobsConfigFileParser.jobs_name_to_priority;

    m_control_application = new GlobalController (&m_housekeeping_rules,
        m_system_limits,
        m_policies_rules,
        m_total_jobs,
        m_jobs_name_to_priority,
        controller_role);
}

// launchClusterController call. Creates the ClusterController control application.
void Controller::launchClusterController (const ConfigFileParser config_file_parser)
{
    std::string upper_address = config_file_parser.upper_address;
    std::string own_upper_address = config_file_parser.own_upper_address;
    ControllerRole controller_role = config_file_parser.controller_role;

    std::string policies_rules_file = config_file_parser.policies_rules_file;
    PolicyFileParser policy_parser { policies_rules_file };
    auto m_policies_rules = policy_parser.m_priorities;

    JobsConfigFileParser jobsConfigFileParser (config_file_parser.jobs_config_file);
    auto m_total_jobs = jobsConfigFileParser.total_jobs;
    auto m_jobs_name_to_priority = jobsConfigFileParser.jobs_name_to_priority;

    Logging::log_info ("Launching Cluster Controller with upper address: " + upper_address
        + " and own upper address: " + own_upper_address);
    m_control_application = new ClusterController (upper_address,
        own_upper_address,
        m_policies_rules,
        m_total_jobs,
        m_jobs_name_to_priority,
        controller_role);
}

// launchLocalController call. Creates the LocalControlApplication.
void Controller::launchLocalController (const ConfigFileParser config_file_parser)
{
    std::string upper_address = config_file_parser.upper_address;
    std::string own_upper_address = config_file_parser.own_upper_address;

    m_control_application = new LocalControlApplication (&m_housekeeping_rules,
        upper_address,
        own_upper_address,
        option_default_local_control_application_sleep);
}

// handleControllerRole call. Marks the control application as primary if configured as such.
void Controller::handleControllerRole (const ConfigFileParser config_file_parser)
{
    ControllerRole controller_role = config_file_parser.controller_role;

    if (controller_role == ControllerRole::PRIMARY) {
        m_control_application->set_as_primary_controller ();
    }
}

// RegisterHousekeepingRules call. Processes housekeeping rules.
void Controller::RegisterHousekeepingRules (const std::string& path)
{
    // Logging::log_info ("Register Housekeeping Rules.");
    PolicyGenerator generator {};

    // create parser object. Rules are parsed at creation time.
    RulesFileParser file_parser { RuleType::housekeeping, path };
    int rules_size;

    // create, insert, and execute HousekeepingRules of type HSK_CREATE_CHANNEL
    std::vector<HousekeepingCreateChannelRaw> hsk_create_channel {};
    rules_size = file_parser.get_create_channel_rules (hsk_create_channel, -1);

    // convert HousekeepingRule EChannel objects to string and store them in
    // the this->m_housekeeping_rules container
    for (int i = 0; i < rules_size; i++) {
        std::string hsk_enf_channel;
        generator.convert_housekeeping_create_channel_string (hsk_create_channel.at (i),
            hsk_enf_channel);

        // store Housekeeping Rule of type HSK_CREATE_CHANNEL in string format
        this->m_housekeeping_rules.push_back (hsk_enf_channel);
    }

    // create, insert, and execute HousekeepingRules of type HSK_CREATE_OBJECT
    std::vector<HousekeepingCreateObjectRaw> hsk_create_object {};
    rules_size = file_parser.get_create_object_rules (hsk_create_object, -1);

    // convert HousekeepingRule EObject objects to string and store them in
    // the this->m_housekeeping_rules container
    for (int i = 0; i < rules_size; i++) {
        std::string hsk_enf_object;
        generator.convert_housekeeping_create_object_string (hsk_create_object.at (i),
            hsk_enf_object);

        // store Housekeeping Rule of type HSK_CREATE_OBJECT in string
        // format
        this->m_housekeeping_rules.push_back (hsk_enf_object);
    }
}

// SpawnControlApplication call. Starts the control application that orchestrates the system.
void Controller::SpawnControlApplication ()
{
    // Logging::log_info ("Spawning Control Algorithm -- ");
    std::thread control_application_thread_t = std::thread (std::ref (*m_control_application));
    control_application_thread_t.detach ();
}

// SpawnConnectionManager call. Spawn the connection manager to start to accept connections.
void Controller::SpawnConnectionManager (const ConfigFileParser config_file_parser)
{
    std::string controller_address = config_file_parser.own_down_address;
    std::string own_internal_address = config_file_parser.own_internal_address;
    std::string twin_controller_address = config_file_parser.twin_controller_address;
    ControllerRole controller_role = config_file_parser.controller_role;

    switch (m_controller_type) {
        case ControllerType::GLOBAL:
            m_connection_manager = new SupervisorConnectionManager (controller_address,
                own_internal_address,
                twin_controller_address,
                m_control_application,
                controller_role);
            break;
        case ControllerType::CLUSTER:
            m_connection_manager = new CoreConnectionManager (controller_address,
                own_internal_address,
                twin_controller_address,
                m_control_application,
                controller_role);
            break;
        case ControllerType::LOCAL:
            m_connection_manager
                = new LocalConnectionManager (controller_address, m_control_application);
            break;
        default:
            Logging::log_error ("Controller:: Type of controller not supported.");
    }
}

// StopController call. Stops the controller, including the connection manager and control
// application.
void Controller::StopController ()
{
    m_connection_manager->Stop ();
    m_control_application->stop_feedback_loop ();
}

} // namespace shio
