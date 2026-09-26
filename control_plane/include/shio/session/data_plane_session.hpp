/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_DATA_PLANE_SESSION_HPP
#define CHEFERD_DATA_PLANE_SESSION_HPP

#include "cheferd/networking/interface/paio_interface.hpp"
#include "cheferd/networking/stage_response/stage_response.hpp"
#include "cheferd/networking/stage_response/stage_response_ack.hpp"
#include "cheferd/networking/stage_response/stage_response_handshake.hpp"
#include "cheferd/networking/stage_response/stage_response_stat.hpp"
#include "cheferd/networking/stage_response/stage_response_stat_all.hpp"
#include "cheferd/networking/stage_response/stage_response_stats.hpp"
#include "session.hpp"

#include <cheferd/utils/logging.hpp>
#include <cheferd/utils/options.hpp>
#include <condition_variable>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <queue>
#include <unistd.h>

namespace cheferd {

/**
 * DataPlaneSession class.
 * DataPlaneSession component serves as a liaison between the LocalControlApplication
 * and the data plane stage interface. After the handshake, the stage connects to the UNIX socket
 * created by this session, which then forwards rules and statistics requests to the stage.
 * Currently, the DataPlaneSession class contains the following variables:
 * - interface_: interface to submit requests.
 * - unix_socket_: UNIX socket.
 * - server_fd_: socket file descriptor.
 * - addrlen_: address length.
 */
class DataPlaneSession : public Session {

private:
    PAIOInterface interface_;
    struct sockaddr_un unix_socket_;
    int server_fd_;
    int addrlen_;

    /**
     * PrepareUnixConnection: Prepare UNIX Domain socket connections
     * between the control plane and the data plane stage (single).
     * @param socket_name UNIX Domain Socket name.
     */
    void PrepareUnixConnection (const char* socket_name);

    /**
     * SendRule: Handle the rule to be submitted to the data plane stage.
     * @param socket Socket identifier.
     * @param rule Rule to be submitted.
     * @param operation ControlOperation.
     * @return PStatus::OK() if the rule was successfully executed,
     * PStatus::Error() otherwise.
     */
    PStatus SendRule (int socket, const std::string& rule, ControlOperation* operation);

public:
    /**
     * DataPlaneSession parameterized constructor.
     * @param socket_name Socket name.
     */
    DataPlaneSession (const char* socket_name);

    /**
     * DataPlaneSession parameterized constructor.
     * @param id Session identifier.
     * @param socket_name Socket name.
     */
    explicit DataPlaneSession (long id, const char* socket_name);

    /**
     * DataPlaneSession default destructor.
     */
    ~DataPlaneSession ();

    /**
     * StartSession: Start session execution. Waits for the stage to connect and then handles
     * rules until the session is removed.
     */
    void StartSession ();
};
} // namespace cheferd

#endif // CHEFERD_DATA_PLANE_SESSION_HPP
