/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_CONTROLLER_HPP
#define SHIO_CONTROLLER_HPP

#include "shio/controller/control_application/control_application.hpp"
#include "shio/networking/connection_manager/connection_manager.hpp"

#include <shio/controller/cluster_controller.hpp>
#include <shio/controller/global_controller.hpp>
#include <shio/utils/config_file_parser.hpp>
#include <shio/utils/options.hpp>

namespace shio {

/**
 * Controller class. Gateway component to start the whole system.
 * Provides the primitives to start the components: ControlApplication and ConnectionManager,
 * and to process inputted housekeeping rules.
 * Currently, the Controller class contains the following variables:
 * - m_controller_type: defines the type of controller (Global, Cluster or Local).
 * - m_control_application: defines control application orchestrating the system.
 * - m_connection_manager: handles connections made to the controller.
 * - m_housekeeping_rules: container used to store housekeeping rules that should be imposed at the
 * data plane stages.
 */
class Controller {

private:
    ControllerType m_controller_type;
    ControlApplication* m_control_application;
    ConnectionManager* m_connection_manager;

    /**
     * launchGlobalController, launchClusterController and launchLocalController calls.
     * These methods are responsible for creating the respective control application based on the
     * configuration provided in the ConfigFileParser (and, for the global controller, loading the
     * housekeeping rules).
     * @param config_file_parser Controller's configuration information.
     */
    void launchGlobalController (const ConfigFileParser config_file_parser);
    void launchClusterController (const ConfigFileParser config_file_parser);
    void launchLocalController (const ConfigFileParser config_file_parser);

    /**
     * handleControllerRole call.
     * Handles the role of the controller: marks the control application as primary when the
     * configured role is PRIMARY.
     * @param config_file_parser Controller's configuration information.
     */
    void handleControllerRole (const ConfigFileParser config_file_parser);

    /**
     * RegisterHousekeepingRules call. Processes housekeeping rules and stores them, in string
     * format, in m_housekeeping_rules.
     * @param path Path to the file that holds the housekeeping rules to be imposed.
     */
    void RegisterHousekeepingRules (const std::string& path);

    /**
     * SpawnControlApplication call.
     * Starts the control application that orchestrates the system, on a detached thread.
     */
    void SpawnControlApplication ();

    /**
     * SpawnConnectionManager call.
     * Spawn the connection manager to start to accept connections. Blocks until the connection
     * manager stops.
     * @param config_file_parser Controller's configuration information.
     */
    void SpawnConnectionManager (const ConfigFileParser config_file_parser);

    /**
     * StopController call.
     * Stops the controller, including the connection manager
     * and control application. Currently unused.
     */
    void StopController ();

public:
    std::vector<std::string> m_housekeeping_rules;

    /**
     * Controller constructor. Launches the controller described by config_file_parser; blocks
     * while the connection manager is running.
     * @param config_file_parser Controller's configuration information.
     */
    Controller (ConfigFileParser config_file_parser);

    /**
     * Controller default destructor.
     */
    ~Controller ();
};
} // namespace shio
#endif // SHIO_CONTROLLER_HPP
