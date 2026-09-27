/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_LOCAL_INTERFACE_HPP
#define SHIO_LOCAL_INTERFACE_HPP

#include "_deps/grpc-src/include/grpcpp/grpcpp.h"
#include "shio/networking/stage_response/stage_response_stat.hpp"
#include "shio/networking/stage_response/stage_response_stat_all.hpp"
#include "shio/networking/stage_response/stage_response_stats.hpp"
#include "shio/utils/logging.hpp"
#include "southbound_interface.hpp"

#include <cstdio>
#include <netinet/in.h>
#include <random>
#include <sstream>
#include <sys/un.h>
#include <unistd.h>

#ifdef BAZEL_BUILD
#include "examples/protos/controllers_grpc_interface.grpc.pb.h"
#else
#include "controllers_grpc_interface.grpc.pb.h"
#endif

using controllers_grpc_interface::ACK;
using controllers_grpc_interface::StageReadyRaw;
using controllers_grpc_interface::StatsGlobalAllMap;
using controllers_grpc_interface::StatsGlobalMap;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using controllers_grpc_interface::ControllerToLower;
using controllers_grpc_interface::SimplifiedHandshakeRaw;
using grpc::Channel;
using grpc::ClientContext;

namespace shio {

// Accumulated time (in microseconds) spent in enforcement RPCs, reported by the control
// applications at the end of a benchmark.
extern int local_network_enforce_time;

/**
 * LocalInterface class.
 * gRPC client used by a cluster controller to communicate with a local controller (through the
 * ControllerToLower service).
 * Currently, the LocalInterface class contains the following variables:
 * - stub_: stub used to send requests to the local controller.
 */
class LocalInterface {

private:
    /**
     * parse_rule: Parse a rule into tokens using char c as delimiter.
     * @param rule Rule to be parsed.
     * @param tokens Container to store parsed tokens.
     * @param c Delimiter.
     */
    void parse_rule (const std::string& rule, std::vector<std::string>* tokens, char c);

    /**
     * fill_housekeeping_rules_grpc: Fill SimplifiedHandshakeRaw with rule data.
     * @param housekeeping_rules SimplifiedHandshakeRaw object to be filled.
     * @param rule ':'-separated rules; the first token (the operation header) is skipped.
     */
    void fill_housekeeping_rules_grpc (
        controllers_grpc_interface::SimplifiedHandshakeRaw* housekeeping_rules,
        const std::string& rule);

    std::unique_ptr<ControllerToLower::Stub> stub_;

public:
    /**
     * LocalInterface parameterized constructor. Creates the stub and resets the enforcement timer.
     * @param user_address Address of the local controller.
     */
    explicit LocalInterface (const std::string& user_address);

    /**
     * LocalInterface default destructor.
     */
    ~LocalInterface ();

    /**
     * local_handshake: Performs a handshake with the local controller. Informs local controller
     * of the housekeeping rules that should be imposed at the data plane stages.
     * @param user_address Corresponds to the local controller address.
     * @param operation ControlOperation.
     * @param rule Housekeeping rules.
     * @param response Response obtained.
     * @return PStatus value that defines if the operation was successful.
     */
    PStatus local_handshake (const std::string& user_address,
        ControlOperation* operation,
        const std::string& rule,
        ACK& response);

    /**
     * stage_handshake: Handshake a data plane stage.
     * Not implemented: stages handshake directly with the local controller.
     * @param user_address Corresponds to the data plane address.
     * @param operation ControlOperation.
     * @param stage_handshake_obj StageSimplifiedHandshakeRaw object stores data plane stage
     * detailed information.
     * @return PStatus value that defines if the operation was successful.
     */
    PStatus stage_handshake (const std::string& user_address,
        ControlOperation* operation,
        StageSimplifiedHandshakeRaw& stage_handshake_obj);

    /**
     * mark_stage_ready: Mark data plane stage as ready.
     * @param user_address Corresponds to the local controller address.
     * @param operation ControlOperation.
     * @param rule Rule to mark stage as ready.
     * @param response Response obtained.
     * @return PStatus value that defines if the operation was successful.
     */
    PStatus mark_stage_ready (const std::string& user_address,
        ControlOperation* operation,
        const std::string& rule,
        ACK& response);

    /**
     * create_enforcement_rule: Submit enforcement rules to the local controller to pass to its
     * data plane stages.
     * @param user_address Corresponds to the local controller address.
     * @param operation ControlOperation.
     * @param rule Enforcement rules, separated by '.' (the first element is a header and is
     * skipped). Each rule has the format <id>|<stage_name>|<operation>|<env>:<rate>*<env>:<rate>...
     * @param response Response obtained.
     * @return PStatus value that defines if the operation was successful.
     */
    PStatus create_enforcement_rule (const std::string& user_address,
        ControlOperation* operation,
        const std::string& rule,
        ACK& response);

    /**
     * collect_global_all_statistics: Collect all statistics from data plane stages.
     * @param user_address Corresponds to the local controller address.
     * @param operation ControlOperation.
     * @param stats_tf_objects Container to store responses.
     * @return  PStatus value that defines if the operation was successful.
     */
    PStatus collect_global_all_statistics (const std::string& user_address,
        ControlOperation* operation,
        std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<StageResponse>>>&
            stats_tf_objects);

    /**
     * collect_global_statistics: Collect statistics from data plane stages.
     * @param user_address Corresponds to the local controller address.
     * @param operation ControlOperation.
     * @param stats_tf_objects Container to store responses.
     * @return  PStatus value that defines if the operation was successful.
     */
    PStatus collect_global_statistics (const std::string& user_address,
        ControlOperation* operation,
        std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<StageResponse>>>&
            stats_tf_objects);

    /**
     * collect_global_statistics_aggregated: Collect aggregated statistics from data plane stages.
     * @param user_address Corresponds to the local controller address.
     * @param operation ControlOperation.
     * @param stats_tf_objects Container to store responses.
     * @return  PStatus value that defines if the operation was successful.
     */
    PStatus collect_global_statistics_aggregated (const std::string& user_address,
        ControlOperation* operation,
        std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<StageResponse>>>&
            stats_tf_objects);
};
} // namespace shio

#endif // SHIO_LOCAL_INTERFACE_HPP
