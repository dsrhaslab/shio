/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_HANDSHAKE_SESSION_HPP
#define SHIO_HANDSHAKE_SESSION_HPP

#include "shio/networking/interface/paio_interface.hpp"
#include "shio/networking/stage_response/stage_response.hpp"
#include "shio/networking/stage_response/stage_response_ack.hpp"
#include "shio/networking/stage_response/stage_response_handshake.hpp"
#include "session.hpp"

#include <shio/utils/logging.hpp>
#include <shio/utils/options.hpp>
#include <condition_variable>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <queue>
#include <unistd.h>

namespace shio {

/**
 * HandshakeSession class.
 * HandshakeSession component serves as a liaison between the LocalControlApplication
 * and the data plane stage interface. It is used for the handshake step: it sends the
 * STAGE_HANDSHAKE request and then the address (STAGE_HANDSHAKE_INFO) the stage should connect
 * to, and then finishes.
 * Currently, the HandshakeSession class contains the following variables:
 * - socket_id_: socket (file descriptor) of the connection with the data plane stage.
 * - interface_: interface to submit requests.
 */
class HandshakeSession : public Session {

private:
    long socket_id_;
    PAIOInterface interface_;

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
     * HandshakeSession default constructor.
     */
    HandshakeSession ();

    /**
     * HandshakeSession parameterized constructor.
     * @param id Socket (file descriptor) of the connection with the data plane stage.
     */
    explicit HandshakeSession (long id);

    /**
     * HandshakeSession default destructor.
     */
    ~HandshakeSession ();

    /**
     * StartSession: Start session execution. Handles the two handshake rules and returns.
     */
    void StartSession ();
};
} // namespace shio

#endif // SHIO_HANDSHAKE_SESSION_HPP
