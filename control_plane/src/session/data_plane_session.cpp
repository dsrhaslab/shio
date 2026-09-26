/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#include <cheferd/session/data_plane_session.hpp>

namespace cheferd {

// DataPlaneSession parameterized constructor.
DataPlaneSession::DataPlaneSession (const char* socket_name) : Session {}, interface_ {}
{
    PrepareUnixConnection (socket_name);
}

// DataPlaneSession parameterized constructor.
DataPlaneSession::DataPlaneSession (long id, const char* socket_name) :
    Session { id },
    interface_ {}
{
    PrepareUnixConnection (socket_name);
}

// DataPlaneSession default destructor.
DataPlaneSession::~DataPlaneSession () = default;

// PrepareUnixConnection call. Prepare UNIX Domain socket connections
// between the control plane and the data plane stage (single).
void DataPlaneSession::PrepareUnixConnection (const char* socket_name)
{
    unlink (socket_name);

    if ((server_fd_ = socket (AF_UNIX, SOCK_STREAM, 0)) == 0) {
        Logging::log_error ("DataPlaneSession: Socket creation error.");
        exit (EXIT_FAILURE);
    }

    unix_socket_.sun_family = AF_UNIX;
    strncpy (unix_socket_.sun_path, socket_name, sizeof (unix_socket_.sun_path) - 1);

    if (bind (server_fd_, (struct sockaddr*)&unix_socket_, sizeof (unix_socket_)) < 0) {
        Logging::log_error ("DataPlaneSession: Bind error.");
        working_session_ = false;
    }

    if (listen (server_fd_, 3) < 0) {
        Logging::log_error ("DataPlaneSession: Listen error.");
        working_session_ = false;
    }

    addrlen_ = sizeof (unix_socket_);
}

// StartSession call. Start session execution.
void DataPlaneSession::StartSession ()
{
    // Logging::log_debug ("DataPlaneSession::StartSession");

    int socket_t = accept (server_fd_, (struct sockaddr*)&unix_socket_, (socklen_t*)&addrlen_);

    // verify socket value
    /*socket_t == -1 ? Logging::log_error ("DataPlaneSession: failed to connect with "
                                         "data plane stage {UNIX}.")
                   : Logging::log_debug ("DataPlaneSession: New data plane stage connection "
                                         "established {UNIX}.");*/

    if (socket_t != -1) {
        working_session_ = true;
    }

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if (setsockopt (socket_t, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof (struct timeval))
        != 0) {
        Logging::log_error ("DataPlaneSession: Set socket options failed.");
        working_session_ = false;
        return;
    }

    PStatus status_submitted = PStatus::OK ();

    // after knowing the Stage identifier,
    // while (working_session_.load () && status_submitted.isOk ()) {
    while (working_session_.load ()) {

        PStatus status;
        ControlOperation operation {};
        std::string rule {};

        status = DequeueRuleFromSubmissionQueue (rule);

        if (status.isOk ()) {
            status_submitted = SendRule (socket_t, rule, &operation);
        }
    }

    working_session_ = false;

    // Logging::log_debug ("DataPlaneSession: Exiting data plane stage session.");
}

// SendRule call. Handle the rule to be submitted to the data plane stage.
PStatus
DataPlaneSession::SendRule (int socket, const std::string& rule, ControlOperation* operation)
{
    // parse rule
    if (!rule.empty ()) {
        std::string token = rule.substr (0, rule.find ('|'));
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

        case STAGE_READY: {
            operation->m_size = sizeof (struct StageReadyRaw);
            // create temporary StageReadyRaw struct
            StageReadyRaw stage_ready {};
            // create temporary ACK structure
            ACK ack {};
            // invoke ...
            status = interface_.mark_stage_ready (socket, operation, stage_ready, ack);
            // enqueue response of data plane stage from mar_stage_ready request
            EnqueueResponseInCompletionQueue (
                std::make_unique<StageResponseACK> (STAGE_READY, ack.m_message));
            break;
        }

        case CREATE_HSK_RULE: {
            // create temporary ACK structure
            ACK ack {};
            // invoke SouthboundInterface's CreateHousekeepingRule
            status = interface_.create_housekeeping_rule (socket, operation, rule, ack);
            // enqueue response of data plane stage from CreateHousekeepingRule request
            EnqueueResponseInCompletionQueue (
                std::make_unique<StageResponseACK> (CREATE_HSK_RULE, ack.m_message));
            break;
        }

        case CREATE_ENF_RULE: {
            // create temporary ACK structure
            ACK ack {};
            // invoke SouthboundInterface's CreateEnforcementRule
            status = interface_.create_enforcement_rule (socket, operation, rule, ack);

            // enqueue response of data plane stage from CreateEnforcementRule
            // request
            EnqueueResponseInCompletionQueue (
                std::make_unique<StageResponseACK> (CREATE_ENF_RULE, ack.m_message));

            break;
        }

        case REMOVE_RULE: {
            // create temporary ACK structure
            ACK ack {};
            // invoke SouthboundInterface's RemoveRule
            status = interface_.RemoveRule (socket, operation, operation->m_operation_id, ack);
            // enqueue response of data plane stage from RemoveRule request
            EnqueueResponseInCompletionQueue (
                std::make_unique<StageResponseACK> (REMOVE_RULE, ack.m_message));
            break;
        }

        case COLLECT_STATS:
            status = interface_.collect_statistics (socket, operation);
            break;

        case COLLECT_DETAILED_STATS: {
            std::vector<std::string> tokens {};

            size_t start;
            size_t end = 0;

            while ((start = rule.find_first_not_of ('|', end)) != std::string::npos) {
                end = rule.find ('|', start);
                tokens.push_back (rule.substr (start, end - start));
            }
            operation->m_operation_subtype = std::stoi (tokens[1]);

            switch (operation->m_operation_subtype) {
                case COLLECT_GLOBAL_ALL_STATS: {
                    // create temporary StatsDataMetadataRaw structure
                    // create temporary StatsKVSRaw structure
                    std::shared_ptr<std::unordered_map<std::string, uint64_t>> stats_tf_objects
                        = std::make_shared<std::unordered_map<std::string, uint64_t>> ();

                    // invoke SouthboundInterface's CollectStatisticsKVS
                    status = interface_.collect_global_all_statistics (socket,
                        operation,
                        stats_tf_objects);

                    if (status.isOk ()) {
                        // enqueue response of data plane stage from collect_tensorflow_statistics
                        EnqueueResponseInCompletionQueue (
                            std::make_unique<StageResponseStatAll> (COLLECT_GLOBAL_ALL_STATS,
                                stats_tf_objects));
                    } else {
                        // enqueue response of data plane stage from collect_tensorflow_statistics
                        EnqueueResponseInCompletionQueue (
                            std::make_unique<StageResponseStatAll> (COLLECT_GLOBAL_ALL_STATS));
                    }
                    break;
                }
                case COLLECT_GLOBAL_STATS_AGGREGATED: {
                    // create temporary StatsDataMetadataRaw structure
                    StatsDataMetadataRaw stats_global {};
                    // invoke SouthboundInterface's CollectStatisticsKVS
                    status = interface_.collect_global_statistics (socket, operation, stats_global);

                    if (status.isOk ()) {
                        // enqueue response of data plane stage from collect_tensorflow_statistics
                        EnqueueResponseInCompletionQueue (
                            std::make_unique<StageResponseStat> (COLLECT_DATA_METADATA_STATS,
                                stats_global.m_total_data_rate,
                                stats_global.m_total_metadata_rate));
                    } else {
                        // enqueue response of data plane stage from collect_tensorflow_statistics
                        EnqueueResponseInCompletionQueue (
                            std::make_unique<StageResponseStat> (COLLECT_DATA_METADATA_STATS,
                                -1,
                                -1));
                    }
                    break;
                }
                case COLLECT_DATA_METADATA_STATS: {
                    // create temporary StatsDataMetadataRaw structure
                    StatsDataMetadataRaw stats_global {};
                    // invoke SouthboundInterface's CollectStatisticsKVS
                    status = interface_.collect_global_statistics (socket, operation, stats_global);

                    if (status.isOk ()) {
                        // enqueue response of data plane stage from collect_tensorflow_statistics
                        EnqueueResponseInCompletionQueue (
                            std::make_unique<StageResponseStat> (COLLECT_DATA_METADATA_STATS,
                                stats_global.m_total_data_rate,
                                stats_global.m_total_metadata_rate));
                    } else {
                        // enqueue response of data plane stage from collect_tensorflow_statistics
                        EnqueueResponseInCompletionQueue (
                            std::make_unique<StageResponseStat> (COLLECT_DATA_METADATA_STATS,
                                -1,
                                -1));
                    }
                    break;
                }

                default:
                    Logging::log_error ("DataPlaneSession: After parsing -- other rule");
                    return PStatus::Error ();
            }
            break;
        }

        default:
            status = PStatus::NotSupported ();
            Logging::log_error ("DataPlaneSession: SendRule -- rule not supported.");
            break;
    }

    return status;
}

} // namespace cheferd
