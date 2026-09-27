/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/networking/stage_response/stage_response_stat.hpp"

#include <shio/session/core_controller_session.hpp>

namespace shio {

// CoreControllerSession parameterized constructor.
CoreControllerSession::CoreControllerSession (const std::string& user_address) :
    Session {},
    interface_ { user_address }
{ }

// CoreControllerSession parameterized constructor.
CoreControllerSession::CoreControllerSession (long id, const std::string& user_address) :
    Session { id },
    interface_ { user_address }
{ }

// CoreControllerSession parameterized constructor.
CoreControllerSession::CoreControllerSession (const std::string& user_address,
    CoreInterface interface) :
    Session {}
{
    interface_ = interface;
}

// CoreControllerSession default destructor.
CoreControllerSession::~CoreControllerSession () = default;

// ShareInterface call. Share the interface.
CoreInterface CoreControllerSession::ShareInterface ()
{
    return interface_;
}

// StartSession call. Start session execution.
void CoreControllerSession::StartSession (const std::string& user_address)
{
    // Logging::log_debug ("CoreControllerSession::StartSession");

    working_session_ = true;

    // after knowing the Stage identifier,
    while (working_session_.load ()) {
        PStatus status;
        ControlOperation operation {};
        std::string rule {};

        status = DequeueRuleFromSubmissionQueue (rule);
        if (status.isOk ()) {
            status = SendRule (user_address, rule, &operation);
        }
    }
}

// SendRule call. Handle the rule to be submitted to the local controller.
PStatus CoreControllerSession::SendRule (const std::string& user_address,
    const std::string& rule,
    ControlOperation* operation)
{
    // parse rule
    if (!rule.empty ()) {
        std::string token = rule.substr (0, rule.find ('|'));
        operation->m_operation_type = std::stoi (token);
    }

    PStatus status = PStatus::Error ();
    switch (operation->m_operation_type) {
        case LOCAL_HANDSHAKE: {
            operation->m_size = sizeof (struct StageSimplifiedHandshakeRaw);
            // create temporary ACK structure
            ACK ack {};
            // invoke SouthboundInterface's StageHandshake call
            status = interface_.local_handshake (user_address, operation, rule, ack);
            // enqueue response of data plane stage from StageHandshake request
            EnqueueResponseInCompletionQueue (
                std::make_unique<StageResponseACK> (LOCAL_HANDSHAKE, ack.m_message));

            break;
        }

        case STAGE_READY: {
            operation->m_size = sizeof (struct StageReadyRaw);
            // create temporary ACK structure
            ACK ack {};
            // invoke ...
            status = interface_.mark_stage_ready (user_address, operation, rule, ack);
            // enqueue response of data plane stage from mar_stage_ready request
            EnqueueResponseInCompletionQueue (
                std::make_unique<StageResponseACK> (STAGE_READY, ack.m_message));

            break;
        }

        case CREATE_ENF_RULE: {
            // create temporary ACK structure
            ACK ack {};
            // invoke SouthboundInterface's CreateEnforcementRule
            status = interface_.create_enforcement_rule (user_address, operation, rule, ack);

            // enqueue response of data plane stage from CreateEnforcementRule
            // request
            EnqueueResponseInCompletionQueue (
                std::make_unique<StageResponseACK> (CREATE_ENF_RULE, ack.m_message));

            break;
        }

        case CREATE_CONTROLLER_ENF_RULE: {
            // create temporary ACK structure
            ACK ack {};
            // invoke SouthboundInterface's CreateEnforcementRule
            status = interface_.create_controller_enforcement_rule (user_address,
                operation,
                rule,
                ack);

            // enqueue response of data plane stage from CreateEnforcementRule
            // request
            EnqueueResponseInCompletionQueue (
                std::make_unique<StageResponseACK> (CREATE_CONTROLLER_ENF_RULE, ack.m_message));

            break;
        }

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
                    std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<StageResponse>>>
                        stats_tf_objects = std::make_unique<
                            std::unordered_map<std::string, std::unique_ptr<StageResponse>>> ();

                    status = interface_.collect_global_all_statistics (user_address,
                        operation,
                        stats_tf_objects);

                    if (status.isOk ()) {
                        EnqueueResponseInCompletionQueue (
                            std::make_unique<StageResponseStats> (COLLECT_GLOBAL_ALL_STATS,
                                stats_tf_objects));

                    } else {
                        EnqueueResponseInCompletionQueue (NULL);
                    }
                    break;
                }
                case COLLECT_GLOBAL_CONTROLLER_STATS: {

                    StageResponseStatController stats_object { COLLECT_GLOBAL_CONTROLLER_STATS };

                    // invoke SouthboundInterface's CollectStatisticsKVS
                    status = interface_.collect_global_controller_statistics (user_address,
                        operation,
                        stats_object);

                    if (status.isOk ()) {
                        EnqueueResponseInCompletionQueue (
                            std::make_unique<StageResponseStatController> (stats_object));

                    } else {
                        EnqueueResponseInCompletionQueue (NULL);
                    }
                    break;
                }

                default:
                    Logging::log_error ("CoreControllerSession: After parsing -- other rule");
                    return PStatus::Error ();
            }
            break;
        }
        default:
            status = PStatus::NotSupported ();
            Logging::log_error ("CoreControllerSession: SendRule -- rule not supported.");
            break;
    }

    return status;
}

} // namespace shio
