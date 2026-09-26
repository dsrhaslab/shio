/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#include <cheferd/session/handshake_session.hpp>

namespace cheferd {

// HandshakeSession default constructor.
HandshakeSession::HandshakeSession () : Session {}, socket_id_ { 0 }, interface_ {}
{ }

// HandshakeSession parameterized constructor.
HandshakeSession::HandshakeSession (long id) : Session {}, socket_id_ { id }, interface_ {}
{ }

// HandshakeSession default destructor.
HandshakeSession::~HandshakeSession () = default;

// StartSession call. Start session execution.
void HandshakeSession::StartSession ()
{
    // Logging::log_debug ("HandshakeSession: " + std::to_string (getSubmissionQueueSize ()));

    PStatus status;
    ControlOperation operation {};
    std::string rule {};

    working_session_ = true;

    status = DequeueRuleFromSubmissionQueue (rule);

    if (status.isOk ()) {
        status = SendRule (socket_id_, rule, &operation);
    }

    if (working_session_.load () && status.isOk ()) {
        /* Send info about address and port to connect to */
        status = DequeueRuleFromSubmissionQueue (rule);
        if (status.isOk ()) {
            status = SendRule (socket_id_, rule, &operation);
        }
    }
}

// SendRule call. Handle the rule to be submitted to the data plane stage.
PStatus
HandshakeSession::SendRule (int socket, const std::string& rule, ControlOperation* operation)
{
    // parse rule
    if (!rule.empty ()) {
        std::string token = rule.substr (0, rule.find ('|'));
        // std::cout << "Token: " << token << "\n";
        operation->m_operation_type = std::stoi (token);
    }

    PStatus status = PStatus::Error ();
    switch (operation->m_operation_type) {
        case STAGE_HANDSHAKE: {
            operation->m_size = sizeof (struct StageSimplifiedHandshakeRaw);
            // create temporary StageHandshakeRAW structure
            StageSimplifiedHandshakeRaw handshake_obj {};
            // invoke SouthboundInterface's StageHandshake call
            status = interface_.stage_handshake (socket, operation, handshake_obj);
            // enqueue response of data plane stage from StageHandshake request
            EnqueueResponseInCompletionQueue (
                std::make_unique<StageResponseHandshake> (STAGE_HANDSHAKE, handshake_obj));
            break;
        }

        case STAGE_HANDSHAKE_INFO: {
            // create temporary ACK structure
            ACK ack {};

            // send to the data plane stage the address and port that it should connect to.
            status = interface_.stage_handshake_address (socket, rule, ack);

            // enqueue response of data plane stage from StageHandshakeInfo
            EnqueueResponseInCompletionQueue (
                std::make_unique<StageResponseACK> (STAGE_HANDSHAKE_INFO, ack.m_message));
            break;
        }

        default:
            status = PStatus::NotSupported ();
            Logging::log_error ("HandshakeSession: SendRule -- rule not supported.");
            break;
    }

    return status;
}

} // namespace cheferd
