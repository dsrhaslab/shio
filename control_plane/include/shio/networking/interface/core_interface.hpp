/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_CORE_INTERFACE_HPP
#define CHEFERD_CORE_INTERFACE_HPP

#include "_deps/grpc-src/include/grpcpp/grpcpp.h"
#include "cheferd/networking/interface/southbound_interface.hpp"
#include "cheferd/networking/stage_response/stage_response_stat.hpp"
#include "cheferd/networking/stage_response/stage_response_stat_all.hpp"
#include "cheferd/networking/stage_response/stage_response_stat_controller.hpp"
#include "cheferd/networking/stage_response/stage_response_stats.hpp"
#include "cheferd/utils/logging.hpp"

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
using controllers_grpc_interface::EnforcementRulesMult;
using controllers_grpc_interface::SimplifiedHandshakeRaw;
using controllers_grpc_interface::StageReadyRaw;
using controllers_grpc_interface::StatsGlobalAllMap;
using controllers_grpc_interface::SupervisorControllerToLowerController;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

namespace cheferd {

// Accumulated time (in microseconds) spent in enforcement RPCs, reported by the control
// applications at the end of a benchmark.
extern int core_network_enforce_time;
extern int passthrough_network_enforce_time;

/**
 * CoreInterface class.
 * gRPC client used by an upper controller to communicate with a lower controller (through the
 * SupervisorControllerToLowerController service).
 * Currently, the CoreInterface class contains the following variables:
 * - controller_stub_: stub used to send requests to the lower controller.
 */
class CoreInterface {

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

    std::shared_ptr<SupervisorControllerToLowerController::Stub> controller_stub_;

public:
    /**
     * CoreInterface parameterized constructor. Creates the stub and resets the enforcement
     * timers.
     * @param user_address Address of the lower controller.
     */
    explicit CoreInterface (const std::string& user_address);

    /**
     * CoreInterface default constructor.
     */
    CoreInterface ();

    /**
     * CoreInterface default destructor.
     */
    ~CoreInterface ();

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
     * create_enforcement_rule: Submit enforcement rules to the lower controller to pass to
     * its data plane stages.
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
     * create_controller_enforcement_rule: Submit system limits to the lower controller, which
     * distributes them among its data plane stages.
     * @param user_address Corresponds to the local controller address.
     * @param operation ControlOperation.
     * @param rule Enforcement rules.
     * @param response Response obtained.
     * @return PStatus value that defines if the operation was successful.
     */
    PStatus create_controller_enforcement_rule (const std::string& user_address,
        ControlOperation* operation,
        const std::string& rule,
        ACK& response);

    /**
     * collect_global_all_statistics: Collect all statistics from data plane stages.
     * @param user_address Corresponds to the local controller address.
     * @param operation ControlOperation.
     * @param stats_tf_objects Container to store responses.
     * @return PStatus value that defines if the operation was successful.
     */
    PStatus collect_global_all_statistics (const std::string& user_address,
        ControlOperation* operation,
        std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<StageResponse>>>&
            stats_tf_objects);

    /**
     * collect_global_controller_statistics: Collect statistics from the controller.
     * @param user_address Corresponds to the local controller address.
     * @param operation ControlOperation.
     * @param stats Container to store responses.
     * @return PStatus value that defines if the operation was successful.
     */
    PStatus collect_global_controller_statistics (const std::string& user_address,
        ControlOperation* operation,
        StageResponseStatController& stats);
};
} // namespace cheferd

#endif // CHEFERD_CORE_INTERFACE_HPP
