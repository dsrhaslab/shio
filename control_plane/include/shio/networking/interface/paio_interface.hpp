/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_PAIO_INTERFACE_HPP
#define SHIO_PAIO_INTERFACE_HPP

#include "shio/networking/stage_response/stage_response.hpp"
#include "shio/utils/logging.hpp"
#include "southbound_interface.hpp"

#include <cstdio>
#include <netinet/in.h>
#include <random>
#include <signal.h>
#include <sstream>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace shio {

/**
 * PAIOInterface class.
 * Interface to communicate with a PAIO data plane stage over a socket. Each request writes a
 * ControlOperation header followed by the request object, and reads back the response.
 */
class PAIOInterface : public SouthboundInterface {

private:
    /**
     * parse_rule: Parses a rule into tokens ('|'-separated).
     * @param rule Rule to be parsed.
     * @param tokens Container to store parsed tokens.
     */
    void parse_rule (const std::string& rule, std::vector<std::string>* tokens);

    /**
     * fill_create_channel_rule: Fill HousekeepingCreateChannelRaw with tokens data.
     * @param hsk_channel_obj HousekeepingCreateChannelRaw object to be filled.
     * @param tokens Data to fill object.
     */
    void fill_create_channel_rule (HousekeepingCreateChannelRaw* hsk_channel_obj,
        const std::vector<std::string>& tokens);

    /**
     * fill_create_object_rule: Fill HousekeepingCreateObjectRaw with tokens data.
     * @param hsk_object_obj HousekeepingCreateObjectRaw object to be filled.
     * @param tokens Data to fill object.
     */
    void fill_create_object_rule (HousekeepingCreateObjectRaw* hsk_object_obj,
        const std::vector<std::string>& tokens);

    /**
     * fill_enforcement_rule: Fill EnforcementRuleRaw with tokens data.
     * @param enf_object EnforcementRuleRaw object to be filled.
     * @param tokens Data to fill object.
     */
    void fill_enforcement_rule (EnforcementRuleRaw* enf_object,
        const std::vector<std::string>& tokens);

public:
    /**
     * PAIOInterface default constructor.
     */
    PAIOInterface ();

    /**
     * PAIOInterface default destructor.
     */
    ~PAIOInterface ();

    /**
     * stage_handshake: Handshake a data plane stage.
     * Submit a handshake request to collect data about the data plane stage.
     * @param socket Corresponds to the open file descriptor/socket of a
     * specific controller-data plane communication.
     * @param operation ControlOperation object that contains the type of rule that will
     * be sent, its size, and the id.
     * @param stage_handshake_obj StageSimplifiedHandshakeRaw object stores data plane stage
     * detailed information.
     * @return PStatus::OK() if the rule was successfully executed,
     * PStatus::Error() otherwise.
     */
    PStatus stage_handshake (int socket,
        ControlOperation* operation,
        StageSimplifiedHandshakeRaw& stage_handshake_obj) override;

    /**
     * stage_handshake_address: Informs a data plane stage about the new socket to connect to.
     * @param socket Corresponds to the open file descriptor/socket of a
     * specific controller-data plane communication.
     * @param rule New socket to connect to.
     * @param response Response obtained.
     * @return PStatus::OK() if the rule was successfully executed,
     * PStatus::Error() otherwise.
     */
    PStatus stage_handshake_address (int socket, const std::string& rule, ACK& response);

    /**
     * create_housekeeping_rule: Creates a housekeeping rule at the data plane stage.
     * @param socket Corresponds to the open file descriptor/socket of a
     * specific controller-data plane communication.
     * @param operation ControlOperation object that contains the type of rule that will
     * be sent, its size, and the id.
     * @param rule Housekeeping rule to be created.
     * @param response Response obtained.
     * @return PStatus::OK() if the rule was successfully executed,
     * PStatus::Error() otherwise.
     */
    PStatus create_housekeeping_rule (int socket,
        ControlOperation* operation,
        const std::string& rule,
        ACK& response) override;

    /**
     * mark_stage_ready: Mark data plane stage as ready.
     * @param socket Corresponds to the open file descriptor/socket of a
     * specific controller-data plane communication.
     * @param operation ControlOperation object that contains the type of rule that will
     * be sent, its size, and the id.
     * @param stage_ready_obj StageReadyRaw object stores if a data plane stage
     * is ready
     * @param response Response obtained.
     * @return PStatus::OK() if the rule was successfully executed,
     * PStatus::Error() otherwise.
     */
    PStatus mark_stage_ready (int socket,
        ControlOperation* operation,
        StageReadyRaw& stage_ready_obj,
        ACK& response) override;

    /**
     * create_enforcement_rule: Creates an enforcement rule at the data plane stage.
     * @param socket Corresponds to the open file descriptor/socket of a
     * specific controller-data plane communication.
     * @param operation ControlOperation object that contains the type of rule that will
     * be sent, its size, and the id.
     * @param rule Enforcement rule to be created.
     * @param response Response obtained.
     * @return PStatus::OK() if the rule was successfully executed,
     * PStatus::Error() otherwise.
     */
    PStatus create_enforcement_rule (int socket,
        ControlOperation* operation,
        const std::string& rule,
        ACK& response) override;

    /**
     * RemoveRule: Remove a HousekeepingRule from a specific data plane stage.
     * @param socket Corresponds to the open file descriptor/socket of a
     * specific controller-data plane communication.
     * @param operation ControlOperation object that contains the type of rule that will
     * be sent, its size, and the id.
     * @param rule_id HousekeepingRule identifier.
     * @param response Response obtained.
     * @return PStatus::OK() if the rule was successfully executed,
     * PStatus::Error() otherwise.
     */
    PStatus
    RemoveRule (int socket, ControlOperation* operation, int rule_id, ACK& response) override;

    /**
     * collect_statistics: Get the statistics of a current data plane stage. Not implemented.
     * @param socket Corresponds to the open file descriptor/socket of a
     * specific controller-data plane communication.
     * @param operation ControlOperation object that contains the type of rule that will
     * be sent, its size, and the id.
     * @return PStatus::NotSupported().
     */
    PStatus collect_statistics (int socket, ControlOperation* operation) override;

    /**
     * collect_global_all_statistics: Get the data and metadata rates of a current data plane
     * stage, stored as "data_op" and "meta_op".
     * @param socket Corresponds to the open file descriptor/socket of a
     * specific controller-data plane communication.
     * @param operation ControlOperation object to be filled and sent.
     * @param stats_all_ops Map where the collected rates are stored, per operation.
     * @return PStatus value that defines if the operation was successful.
     */
    PStatus collect_global_all_statistics (int socket,
        ControlOperation* operation,
        std::shared_ptr<std::unordered_map<std::string, uint64_t>>& stats_all_ops);

    /**
     * collect_global_statistics: Get the statistics of a current data plane stage.
     * @param socket Corresponds to the open file descriptor/socket of a
     * specific controller-data plane communication.
     * @param operation ControlOperation object to be filled and sent.
     * @param stats_tf_object StatsDataMetadataRaw object that contains the type of statistics
     * collected.
     * @return PStatus value that defines if the operation was successful.
     */
    PStatus collect_global_statistics (int socket,
        ControlOperation* operation,
        StatsDataMetadataRaw& stats_tf_object);
};
} // namespace shio

#endif // SHIO_PAIO_INTERFACE_HPP
