/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/networking/connection_manager/supervisor_connection_manager.hpp"

#include "shio/utils/logging.hpp"

namespace shio {

// SupervisorConnectionManager parameterized constructor.
SupervisorConnectionManager::SupervisorConnectionManager (const std::string& controller_address,
    const std::string& own_internal_address,
    const std::string& twin_controller_address,
    ControlApplication* control_application_ptr,
    ControllerRole controller_role) :
    m_control_application_ptr { dynamic_cast<GlobalController*> (control_application_ptr) },
    controller_role { controller_role },
    controller_address { controller_address },
    own_internal_address { own_internal_address },
    twin_controller_address { twin_controller_address },
    lower_controller_addresses {},
    lower_controller_addresses_lock_ {},
    server_lock_ {},
    restart_server_ { false },
    server {}
{
    auto channel
        = grpc::CreateChannel (twin_controller_address, grpc::InsecureChannelCredentials ());
    secondary_stub_ = PrimaryToSecondary::NewStub (channel);
    primary_stub_ = SecondaryToPrimary::NewStub (channel);

    if (controller_role == ControllerRole::SECONDARY) {
        ClientContext context;
        controllers_grpc_interface::Empty request;
        controllers_grpc_interface::SecondaryBootstrapInfo reply;
        Status status = primary_stub_->ConnectToPrimary (&context, request, &reply);

        if (!status.ok ()) {
            // The ping thread below will fail as well and promote this controller to primary.
            Logging::log_error (
                "SupervisorConnectionManager: Failed to connect to primary controller at "
                + twin_controller_address + ": " + status.error_message ());
        } else {
            // Housekeeping rules from the primary are not applied by the global controller.
            // m_control_application_ptr->set_housekeeping_rules (&(reply.housekeeping_rules ()));

            for (const auto& address : reply.lower_controller_addresses ()) {
                lower_controller_addresses.push_back (address);
                if (m_control_application_ptr != nullptr) {
                    m_control_application_ptr->register_controller_session (address);
                }
            }
        }

        std::thread ping_primary_thread (&SupervisorConnectionManager::SendPingToPrimary,
            this,
            twin_controller_address);
        ping_primary_thread.detach ();
    }

    Logging::log_info ("SupervisorConnectionManager initialized with address: " + controller_address
        + " and twin controller address: " + twin_controller_address);

    Start ();
}

// SupervisorConnectionManager default destructor.
SupervisorConnectionManager::~SupervisorConnectionManager () = default;

// Start call. Execute a server that continuously accepts connections. The server is rebuilt
// when the secondary is promoted to primary, since a running gRPC server cannot add ports.
void SupervisorConnectionManager::Start ()
{
    std::unique_lock<std::mutex> lock (server_lock_);

    do {
        restart_server_ = false;
        ServerBuilder builder;

        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort (own_internal_address, grpc::InsecureServerCredentials ());
        std::string listening_addresses = own_internal_address;

        if (controller_role == ControllerRole::PRIMARY) {
            builder.AddListeningPort (controller_address, grpc::InsecureServerCredentials ());
            listening_addresses += " and " + controller_address;
        }

        // Register "service" as the instance through which we'll communicate with clients.
        // In this case it corresponds to an *synchronous* service.
        builder.RegisterService (static_cast<PrimaryToSecondary::Service*> (this));
        builder.RegisterService (static_cast<SecondaryToPrimary::Service*> (this));
        builder.RegisterService (static_cast<ControllerToUpper::Service*> (this));
        builder.SetMaxReceiveMessageSize (-1);

        // Finally assemble the server.
        server = builder.BuildAndStart ();
        if (server == nullptr) {
            Logging::log_error (
                "SupervisorConnectionManager: Failed to start server on " + listening_addresses);
            return;
        }
        Logging::log_info (
            "SupervisorConnectionManager: Server listening on " + listening_addresses);

        // Wait for the server to shutdown.
        // Note that some other thread must be responsible for shutting down the server for this
        // call to ever return.
        lock.unlock ();
        server->Wait ();
        lock.lock ();
    } while (restart_server_);
}

// ConnectToUpper call. Connect lower controller to global controller.
Status SupervisorConnectionManager::ConnectToUpper (ServerContext* context,
    const ConnectRequest* request,
    ConnectReply* reply)
{
    std::string prefix ("Hello local controller with address: ");

    PStatus status;

    if (!request->user_address ().empty ()) {
        // register data plane session

        if (m_control_application_ptr != nullptr) {
            m_control_application_ptr->register_controller_session (request->user_address ());
            {
                std::lock_guard<std::mutex> lock (lower_controller_addresses_lock_);
                lower_controller_addresses.push_back (request->user_address ());
            }

            ClientContext context;
            controllers_grpc_interface::ConnectRequest new_request;
            new_request.set_user_address (request->user_address ());
            controllers_grpc_interface::ACK new_reply;

            Status status = secondary_stub_->NewLowerController (&context, new_request, &new_reply);
        } else {
            reply->set_message (
                "SupervisorConnectionManager: Connection to Core Controller is not possible\n");
            return Status::CANCELLED;
        }
    }

    reply->set_message (prefix + request->user_address ());
    return Status::OK;
}

// ConnectStageToUpper call. Connect stage to global controller.
Status SupervisorConnectionManager::ConnectStageToUpper (ServerContext* context,
    const StageInfoConnect* request,
    ConnectReply* reply)
{
    std::string prefix ("Hello stage with index: ");

    PStatus status;

    if (!request->local_address ().empty ()) {
        // register data plane session
        if (m_control_application_ptr != nullptr) {
            m_control_application_ptr->register_stage_session (request);

            Logging::log_info ("SupervisorConnectionManager: Connecting stage ("
                + request->stage_name () + ") from " + request->local_address ()
                + " and going for the next one ...");
        } else {
            reply->set_message (
                "SupervisorConnectionManager: Connection to Core Controller is not possible\n");
            return Status::CANCELLED;
        }
    }

    reply->set_message (prefix + request->stage_name () + " " + request->stage_env ());
    return Status::OK;
}

// PingPrimary call. Answer a health check sent by the secondary controller.
Status SupervisorConnectionManager::PingPrimary (ServerContext* context,
    const controllers_grpc_interface::Empty* request,
    controllers_grpc_interface::ACK* reply)
{
    Logging::log_info ("SupervisorConnectionManager: Received ping from secondary controller");
    reply->set_m_message (1);
    return Status::OK;
}

// SendPingToPrimary call. Ping the primary controller every second; when a ping fails, promote
// this controller to primary.
void SupervisorConnectionManager::SendPingToPrimary (std::string primary_address)
{
    Logging::log_info (
        "SupervisorConnectionManager: Starting ping to primary controller at " + primary_address);
    Status status = Status::OK;

    while (status.ok ()) {
        // Sleep for a while before sending the next ping
        std::this_thread::sleep_for (std::chrono::seconds (1));

        Logging::log_info ("SupervisorConnectionManager: Sending ping to primary controller at "
            + primary_address);

        controllers_grpc_interface::Empty request;
        controllers_grpc_interface::ACK reply;

        ClientContext context;

        status = primary_stub_->PingPrimary (&context, request, &reply);
    }

    Logging::log_info ("SupervisorConnectionManager: Ping to primary controller failed. "
                       "Attempting to set as primary controller at "
        + controller_address);
    m_control_application_ptr->set_as_primary_controller ();

    // Shut down the server so that Start () rebuilds it listening on controller_address too.
    std::lock_guard<std::mutex> lock (server_lock_);
    controller_role = ControllerRole::PRIMARY;
    restart_server_ = true;
    if (server != nullptr) {
        server->Shutdown ();
    }
}

// NewLowerController call. Register a lower controller announced by the primary controller.
Status SupervisorConnectionManager::NewLowerController (ServerContext* context,
    const controllers_grpc_interface::ConnectRequest* request,
    controllers_grpc_interface::ACK* reply)
{
    Logging::log_info (
        "SupervisorConnectionManager: Received info about a new lower controller from "
        "primary controller");

    if (m_control_application_ptr != nullptr) {
        m_control_application_ptr->register_controller_session (request->user_address ());
        std::lock_guard<std::mutex> lock (lower_controller_addresses_lock_);
        lower_controller_addresses.push_back (request->user_address ());
    } else {
        reply->set_m_message (0);
        return Status::CANCELLED;
    }

    reply->set_m_message (1);

    return Status::OK;
}

// ConnectToPrimary call. Send the housekeeping rules and lower controller addresses to a
// connecting secondary controller.
Status SupervisorConnectionManager::ConnectToPrimary (ServerContext* context,
    const controllers_grpc_interface::Empty* request,
    controllers_grpc_interface::SecondaryBootstrapInfo* reply)
{
    Logging::log_info (
        "SupervisorConnectionManager: Received connection request from secondary controller");

    auto housekeeping_rules
        = m_control_application_ptr ? m_control_application_ptr->housekeeping_rules_ptr_ : nullptr;
    if (housekeeping_rules == nullptr) {
        Logging::log_info (
            "SupervisorConnectionManager: No housekeeping rules set, creating default ones");
    } else {
        for (const auto& specific_rule : *housekeeping_rules) {
            if (reply->mutable_housekeeping_rules () != nullptr) {
                reply->mutable_housekeeping_rules ()->add_rules (specific_rule);
            }
        }
    }

    std::lock_guard<std::mutex> lock (lower_controller_addresses_lock_);
    for (const auto& lower_controller_address : lower_controller_addresses) {
        reply->add_lower_controller_addresses (lower_controller_address);
    }

    return Status::OK;
}

// Stop call. Stop connection manager.
void SupervisorConnectionManager::Stop ()
{
    std::lock_guard<std::mutex> lock (server_lock_);
    restart_server_ = false;
    if (server != nullptr) {
        server->Shutdown ();
    }
    // m_control_application_ptr->stop_feedback_loop ();
}

} // namespace shio
