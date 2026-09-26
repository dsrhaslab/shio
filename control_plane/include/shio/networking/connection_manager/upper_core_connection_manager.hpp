/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_UPPER_CORE_CONNECTION_MANAGER_HPP
#define CHEFERD_UPPER_CORE_CONNECTION_MANAGER_HPP

#include "cheferd/controller/cluster_controller.hpp"
#include "cheferd/controller/control_application/control_application.hpp"
#include "cheferd/controller/control_application/passthrough_control_application.hpp"
#include "cheferd/session/local_controller_session.hpp"

#include <regex>
#include <thread>

#ifdef BAZEL_BUILD
#include "examples/protos/controllers_grpc_interface.grpc.pb.h"
#else
#include "controllers_grpc_interface.grpc.pb.h"
#endif

using controllers_grpc_interface::ACK;
using controllers_grpc_interface::ConnectReply;
using controllers_grpc_interface::ConnectRequest;
using controllers_grpc_interface::StageInfoConnect;
using controllers_grpc_interface::StageReadyRaw;
using controllers_grpc_interface::StatsGlobalMap;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using controllers_grpc_interface::ControllerToUpper;
using controllers_grpc_interface::SimplifiedHandshakeRaw;
using grpc::Channel;
using grpc::ClientContext;

namespace cheferd {

class ClusterController;

/**
 * UpperCoreConnectionManager class.
 * UpperCoreConnectionManager runs, on a detached thread, the gRPC server through which the upper
 * controller sends requests to the cluster controller. The cluster controller itself is
 * registered as the gRPC service.
 * Currently, the UpperCoreConnectionManager class contains the following variables:
 * - local_address: Address on which the server listens.
 * - server: unique_ptr of Server that receives requests from the upper controller.
 * - m_cluster_controller_ptr: ClusterController that implements the gRPC service.
 */
class UpperCoreConnectionManager {

private:
    std::string local_address;
    std::unique_ptr<Server> server;
    ClusterController* m_cluster_controller_ptr;

    /**
     * RunUpperIncomingServer: Execute the server that receives requests from the upper
     * controller. Blocks until stop_server () is called.
     */
    void RunUpperIncomingServer ();

public:
    /**
     * UpperCoreConnectionManager parameterized constructor.
     * Starts RunUpperIncomingServer on a detached thread.
     * @param local_address Address on which the server listens.
     * @param cluster_controller Cluster controller that implements the gRPC service.
     */
    UpperCoreConnectionManager (const std::string& local_address,
        ClusterController* cluster_controller);

    /**
     * UpperCoreConnectionManager default destructor.
     */
    ~UpperCoreConnectionManager ();

    /**
     * stop_server: Shut down the upper incoming server.
     */
    void stop_server ();
};

} // namespace cheferd

#endif // CHEFERD_UPPER_CORE_CONNECTION_MANAGER_HPP
