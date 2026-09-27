/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/networking/connection_manager/upper_core_connection_manager.hpp"

#include "shio/utils/logging.hpp"

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace shio {

// UpperCoreConnectionManager parameterized constructor.
UpperCoreConnectionManager::UpperCoreConnectionManager (const std::string& local_address,
    ClusterController* cluster_controller) :
    m_cluster_controller_ptr (cluster_controller)
{
    Logging::log_info ("UpperCoreConnectionManager initialized.");
    this->local_address = local_address;

    std::thread upper_connection_manager_thread_t
        = std::thread (&UpperCoreConnectionManager::RunUpperIncomingServer, this);
    upper_connection_manager_thread_t.detach ();
}

// UpperCoreConnectionManager default destructor.
UpperCoreConnectionManager::~UpperCoreConnectionManager ()
{
    Logging::log_info ("UpperCoreConnectionManager: exiting ...\n");
}

//////////////////////////////////////////////
/// Local to core controller communication ///
//////////////////////////////////////////////

// RunUpperIncomingServer call. Execute the server that receives requests from the upper
// controller and forwards them to the cluster controller.
void UpperCoreConnectionManager::RunUpperIncomingServer ()
{
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort (local_address, grpc::InsecureServerCredentials ());
    // Register "service" as the instance through which we'll communicate with clients.
    // In this case it corresponds to an *synchronous* service.
    builder.RegisterService (m_cluster_controller_ptr);
    builder.SetMaxReceiveMessageSize (-1);

    // Finally assemble the server.
    server = builder.BuildAndStart ();
    Logging::log_info ("UpperCoreConnectionManager: Server listening on " + local_address);

    // Wait for the server to shutdown.
    // Note that some other thread must be responsible for shutting down the server for this call to
    // ever return.
    server->Wait ();
}

// stop_server call. Shut down the upper incoming server.
void UpperCoreConnectionManager::stop_server ()
{
    server->Shutdown ();
}

} // namespace shio
