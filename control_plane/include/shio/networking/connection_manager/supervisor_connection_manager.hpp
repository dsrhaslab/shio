/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_SUPERVISOR_CONNECTION_MANAGER_HPP
#define SHIO_SUPERVISOR_CONNECTION_MANAGER_HPP

#include "shio/controller/global_controller.hpp"
#include "shio/networking/connection_manager/connection_manager.hpp"
#include "shio/session/data_plane_session.hpp"
#include "shio/utils/options.hpp"
#include "shio/utils/status.hpp"

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
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using controllers_grpc_interface::ControllerToUpper;
using controllers_grpc_interface::PrimaryToSecondary;
using controllers_grpc_interface::SecondaryToPrimary;
using grpc::Channel;
using grpc::ClientContext;

namespace shio {

/**
 * SupervisorConnectionManager class.
 * SupervisorConnectionManager is used to manage the connections to the global controller.
 * Currently, the SupervisorConnectionManager class contains the following variables:
 * - m_control_application_ptr: GlobalController component.
 * - controller_role: Role of the controller (primary or secondary).
 * - controller_address: Public controller address (only served by the primary).
 * - own_internal_address: Internal address of the controller.
 * - twin_controller_address: Address of the primary/secondary controller (if any).
 * - lower_controller_addresses: Addresses of the connected lower controllers.
 * - lower_controller_addresses_lock_: Mutex that protects lower_controller_addresses.
 * - server_lock_: Mutex that protects server, controller_role and restart_server_.
 * - restart_server_: Whether Start () should rebuild the server after it shuts down (set when
 * the secondary is promoted to primary).
 * - server: unique_ptr of Server that receives requests from lower controllers, stages and the
 * twin controller.
 * - secondary_stub_: unique_ptr of PrimaryToSecondary::Stub to send requests to secondary
 * controller.
 * - primary_stub_: unique_ptr of SecondaryToPrimary::Stub to send requests to primary controller.
 */
class SupervisorConnectionManager : public ControllerToUpper::Service,
                                    public PrimaryToSecondary::Service,
                                    public SecondaryToPrimary::Service,
                                    public ConnectionManager {

private:
    GlobalController* m_control_application_ptr;
    ControllerRole controller_role;
    std::string controller_address;
    std::string own_internal_address;
    std::string twin_controller_address;
    std::vector<std::string> lower_controller_addresses;
    std::mutex lower_controller_addresses_lock_;
    std::mutex server_lock_;
    bool restart_server_;
    std::unique_ptr<Server> server;
    std::unique_ptr<PrimaryToSecondary::Stub> secondary_stub_;
    std::unique_ptr<SecondaryToPrimary::Stub> primary_stub_;

    /**
     * ConnectToUpper: Connect lower controller to controller and forward its address to the
     * secondary controller.
     * @param context Server context.
     * @param request ConnectRequest containing the lower controller address.
     * @param reply Greeting message, or error message on failure.
     * @return Returns Status value that defines if the operation was successful.
     */
    Status ConnectToUpper (ServerContext* context,
        const ConnectRequest* request,
        ConnectReply* reply) override;

    /**
     * ConnectStageToUpper: Connect stage to core controller.
     * @param context Server context.
     * @param request StageInfoConnect describing the stage.
     * @param reply Greeting message, or error message on failure.
     * @return Returns Status value that defines if the operation was successful.
     */
    Status ConnectStageToUpper (ServerContext* context,
        const StageInfoConnect* request,
        ConnectReply* reply) override;

    /**
     * PingPrimary: Response to primary controller health check.
     * @param context Server context.
     * @param request Empty request.
     * @param reply ACK response (always 1).
     * @return Returns Status value that defines if the operation was successful.
     */
    Status PingPrimary (ServerContext* context,
        const controllers_grpc_interface::Empty* request,
        controllers_grpc_interface::ACK* reply) override;

    /**
     * NewLowerController: Register a lower controller announced by the primary controller.
     * @param context Server context.
     * @param request ConnectRequest containing the controller address.
     * @param reply ACK response (1 on success, 0 on failure).
     * @return Returns Status value that defines if the operation was successful.
     */
    Status NewLowerController (ServerContext* context,
        const controllers_grpc_interface::ConnectRequest* request,
        controllers_grpc_interface::ACK* reply) override;

    /**
     * ConnectToPrimary: Handles connection request from a secondary controller.
     * @param context Server context.
     * @param request Empty request.
     * @param reply SecondaryBootstrapInfo with the housekeeping rules and lower controller
     * addresses known by the primary.
     * @return Returns Status value that defines if the operation was successful.
     */
    Status ConnectToPrimary (ServerContext* context,
        const controllers_grpc_interface::Empty* request,
        controllers_grpc_interface::SecondaryBootstrapInfo* reply) override;

public:
    /**
     *  SupervisorConnectionManager parameterized constructor.
     *  @param controller_address Core controller address (always primary address)
     *  @param own_internal_address Address of the local controller.
     *  @param twin_controller_address Address of the primary/secondary controller (if any).
     *  @param control_application_ptr Pointer to the ControlApplication component.
     *  @param controller_role Role of the controller (primary or secondary).
     *  A secondary bootstraps its state from the primary and starts pinging it. The constructor
     *  calls Start (), so it blocks until the server is shut down.
     */
    SupervisorConnectionManager (const std::string& controller_address,
        const std::string& own_internal_address,
        const std::string& twin_controller_address,
        ControlApplication* control_application_ptr,
        ControllerRole controller_role = ControllerRole::PRIMARY);

    /**
     * SupervisorConnectionManager default destructor.
     */
    ~SupervisorConnectionManager ();

    /**
     * Start: Execute a server that continuously accepts connections.
     */
    void Start () override;

    /**
     * Stop: Stop connection manager.
     */
    void Stop () override;

    /**
     * SendPingToPrimary: Ping the primary controller every second until a ping fails, then
     * promote this controller to primary. Runs on a detached thread in the secondary.
     * @param primary_address Address of the primary controller.
     */
    void SendPingToPrimary (std::string primary_address);
};
} // namespace shio

#endif // SHIO_SUPERVISOR_CONNECTION_MANAGER_HPP
