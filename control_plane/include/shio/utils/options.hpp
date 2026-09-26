/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_OPTIONS_HPP
#define CHEFERD_OPTIONS_HPP

#include <string>

namespace cheferd {

/**
 * Transport used between a local controller and its data plane stages.
 */
enum class CommunicationType { UNIX = 0, INET = 1, gRPC = 2 };

/**
 * Level of a controller in the hierarchy (global > cluster > local).
 */
enum class ControllerType { GLOBAL = 0, CLUSTER = 1, LOCAL = 2 };

/**
 * Replication role of a controller. The secondary monitors the primary and takes over when it
 * fails.
 */
enum class ControllerRole { PRIMARY = 0, SECONDARY = 1 };

/**
 * Control algorithm applied by the control application (selected by the "policy" field of the
 * policy file).
 */
enum class ControlType { STATIC = 1, EQUISHARE = 2, PRIORISHARE = 3, PSFA = 4, NOOP = 0 };

/**
 * Type of enforcement object installed in data plane stages (DRL: dynamic rate limiter).
 */
enum class EnforcementObjectType { DRL = 1, NOOP = 0 };

/**
 * Maximum number of feedback-loop rounds for the benchmark.
 */
const int max_rounds = 4000;

/**
 * Maximum time in seconds for the benchmark.
 */
const int max_time_seconds = 4000;

/*
 * ****************************************************************
 * General Options: Default configurations.
 * ****************************************************************
 */

/**
 * Default HousekeepingRules file.
 * This parameter points to the path of the default rules to insert and enforce
 * at system creation.
 */
const std::string option_housekeeping_rules_default
    = "../files/posix_layer_housekeeping_rules_noop";

/**
 * Default communication option.
 * This parameter is defined "a priori", as Sysadmins are not able to change it
 * at runtime.
 */
const CommunicationType option_communication_ = CommunicationType::UNIX;

/**
 * Default PORT for TCP-based communications.
 */
const int option_port_ = 12345;

/**
 * Maximum connections with data plane stages (size of the LocalConnectionManager socket arrays).
 */
const size_t option_max_connections_ = 4;

/**
 * Default logging option.
 * This parameter defines if debug logging is enabled (true) or disabled (false).
 */
const bool option_option_logging_ = false;

/**
 * Default benchmark option.
 * This parameter defines if the benchmark is enabled (true) or disabled (false).
 */
const bool option_benchmark_ = false;

/**
 * Default Control Application cycle time.
 * This parameter defines the amount of time (in microseconds) that the control application sleeps
 * at each feedback-loop cycle.
 */
const uint64_t option_default_control_application_sleep = 1000000;

/**
 * Default Core Control Application cycle time.
 * This parameter defines the amount of time (in microseconds) that the control application sleeps
 * at each feedback-loop cycle.
 */
const uint64_t option_default_core_control_application_sleep = 1000000;

/**
 * Default Supervisor Control Application cycle time.
 * This parameter defines the amount of time (in microseconds) that the control application sleeps
 * at each feedback-loop cycle.
 */
const uint64_t option_default_supervisor_control_application_sleep = 5000000;

/**
 * Default Local Control Application cycle time.
 * This parameter defines the amount of time (in microseconds) that the control application sleeps
 * at each feedback-loop cycle.
 */
const uint64_t option_default_local_control_application_sleep = 1000000;

} // namespace cheferd

#endif // CHEFERD_OPTIONS_HPP
