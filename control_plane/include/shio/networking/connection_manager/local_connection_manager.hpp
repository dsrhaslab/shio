/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_LOCAL_CONNECTION_MANAGER_HPP
#define SHIO_LOCAL_CONNECTION_MANAGER_HPP

#include "shio/controller/control_application/local_control_application.hpp"
#include "shio/networking/connection_manager/connection_manager.hpp"
#include "shio/networking/interface/paio_interface.hpp"
#include "shio/session/data_plane_session.hpp"
#include "shio/utils/options.hpp"
#include "shio/utils/status.hpp"

#include <thread>

namespace shio {

// Backoff delays (in microseconds) used by Start () while no stage is connecting.
#define INITIAL_CONNECTION_DELAY 500000
#define MAX_CONNECTION_DELAY     INT_MAX

/**
 * LocalConnectionManager class.
 * LocalConnectionManager is used to manage the connections to the local controller from data plane
 * stages. Currently, the LocalConnectionManager class contains the following variables:
 * - inet_socket_: INET socket.
 * - unix_socket_: UNIX socket.
 * - server_fd_: socket file descriptor.
 * - addrlen_: address length.
 * - unix_socket_array_: container that holds sockets.
 * - server_fd_array_: container that holds file descriptors.
 * - addrlen_array_: container that holds address lengths.
 * - server_type_: connection type (e.g., UNIX, INET).
 * - index_t: data plane stage index.
 * - working_connection_: atomic value that holds if connection manager is operational.
 * - m_control_application_ptr: pointer to the ControlApplication component.
 */
class LocalConnectionManager : public ConnectionManager {

private:
    struct sockaddr_in inet_socket_;
    struct sockaddr_un unix_socket_;
    int server_fd_;
    int addrlen_;
    struct sockaddr_un unix_socket_array_[option_max_connections_];
    int server_fd_array_[option_max_connections_];
    int addrlen_array_[option_max_connections_];
    CommunicationType server_type_;
    int index_t;
    std::atomic<bool> working_connection_;
    LocalControlApplication* m_control_application_ptr;

    /**
     * Accept: Establish a new connection between the control plane and a data
     * plane stage (single).
     * @return Returns the assigned socket (file descriptor) of the established
     * communication.
     */
    int Accept ();

    /**
     * AcceptConnections: Establish a new connection between the control plane and a
     * data plane stage (multiple).
     * @param index Position in the socket arrays to accept on.
     * @return Returns the assigned socket (file descriptor) of the established
     * communication, or -1 on failure.
     */
    int AcceptConnections (int index);

    /**
     * Operator: Launch connection manager.
     * @param socket Socket identifier (file descriptor).
     * @param session Data plane session pointer.
     */
    void operator() (int socket, DataPlaneSession* session);

    /**
     * PrepareInetConnection: Prepare INET-based connections between the control
     * plane and the data plane stage.
     * @param port INET connection's port.
     * @return Returns 0 on success and -1 on failure.
     */
    int PrepareInetConnection (int port);

    /**
     * PrepareUnixConnection: Prepare UNIX Domain socket connections between the
     * control plane and the data plane stage (single).
     * @param socket_name UNIX Domain Socket name.
     * @param index Unused.
     * @return Returns 0 on success and -1 on failure.
     */
    int PrepareUnixConnection (const char* socket_name, int index);

    /**
     * PrepareUnixConnections: Prepare UNIX Domain socket connections
     * between the control plane and the data plane stage (multiple).
     * @param socket_name UNIX Domain Socket name.
     * @param index Position in the socket arrays to use.
     * @return Returns 0 on success and -1 on failure.
     */
    int PrepareUnixConnections (const char* socket_name, int index);

public:
    /**
     *  LocalConnectionManager parameterized constructor.
     *  Prepares the socket and calls Start (), so it blocks until Stop () is called.
     *  @param controller_address Name used for the UNIX socket (/tmp/<controller_address>.socket).
     *  @param control_application_ptr Pointer to the ControlApplication component.
     */
    LocalConnectionManager (const std::string& controller_address,
        ControlApplication* control_application_ptr);

    /**
     * LocalConnectionManager default destructor.
     */
    ~LocalConnectionManager ();

    /**
     * Start: Execute an endless loop that continuously accepts connections.
     */
    void Start () override;

    /**
     * Stop: Stop connection manager.
     */
    void Stop () override;
};
} // namespace shio

#endif // SHIO_LOCAL_CONNECTION_MANAGER_HPP
