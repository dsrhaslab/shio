/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/networking/interface/core_interface.hpp"

#include "shio/utils/rules_file_parser.hpp"

namespace shio {

int core_network_enforce_time = 0;
int passthrough_network_enforce_time = 0;

// CoreInterface parameterized constructor.
CoreInterface::CoreInterface (const std::string& user_address)
{
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize (-1);

    controller_stub_ = SupervisorControllerToLowerController::NewStub (
        grpc::CreateCustomChannel (user_address, grpc::InsecureChannelCredentials (), args));
    core_network_enforce_time = 0;
    passthrough_network_enforce_time = 0;
}

// CoreInterface default constructor.
CoreInterface::CoreInterface () = default;

// CoreInterface default destructor.
CoreInterface::~CoreInterface () = default;

// local_handshake call: Performs a handshake with the local controller. Informs local controller
// of the housekeeping rules that should be imposed at the data plane stages.
PStatus CoreInterface::local_handshake (const std::string& user_address,
    ControlOperation* operation,
    const std::string& rule,
    ACK& response)
{
    controllers_grpc_interface::ACK reply;
    // parsing phase
    controllers_grpc_interface::SimplifiedHandshakeRaw housekeeping_rules;
    fill_housekeeping_rules_grpc (&housekeeping_rules, rule);

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = controller_stub_->ControllerHandshake (&context, housekeeping_rules, &reply);

    if (!status.ok ()) {
        Logging::log_error (
            "CoreInterface: local_handshake: Error while handshake control operation ("
            + status.error_message () + ").");
        return PStatus::Error ();
    } else {
        response.m_message = reply.m_message ();

        return PStatus::OK ();
    }
}

// mark_stage_ready call. Mark data plane stage as ready.
PStatus CoreInterface::mark_stage_ready (const std::string& user_address,
    ControlOperation* operation,
    const std::string& rule,
    ACK& response)
{
    controllers_grpc_interface::ACK reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // parsing phase
    std::vector<std::string> rule_tokens {};
    this->parse_rule (rule, &rule_tokens, '|');

    controllers_grpc_interface::StageReadyRaw stage_ready_raw;
    stage_ready_raw.set_m_mark_stage (true);
    stage_ready_raw.set_stage_name_env (rule_tokens[1]);

    Status status = controller_stub_->MarkStageReady (&context, stage_ready_raw, &reply);

    response.m_message = reply.m_message ();

    if (!status.ok ()) {
        Logging::log_error ("CoreInterface: mark_stage_ready (channel): Error while writing "
            + rule_tokens[1] + " stage ready (" + status.error_message () + ").");
        return PStatus::Error ();
    } else if (reply.m_message () == static_cast<int> (AckCode::ok)) {
        // Logging::log_debug ("CoreInterface: mark_stage_ready: ACK message received ("
        //     + std::to_string (response.m_message) + ").");
        return PStatus::OK ();
    } else {
        return PStatus::Error ();
    }
}

// create_enforcement_rule call. Submit enforcement rules to the lower controller to pass to its
// data plane stages.
PStatus CoreInterface::create_enforcement_rule (const std::string& user_address,
    ControlOperation* operation,
    const std::string& rule,
    ACK& response)
{
    // validate if logging is enabled and log debug message
    if (Logging::is_debug_enabled ()) {
        // Logging::log_debug ("LocalInterface: create_enforcement_rule: " + rule);
    }

    size_t start0;
    size_t end0 = 0;
    bool first = true;

    controllers_grpc_interface::EnforcementRulesMult create_enforcement_rule;

    while ((start0 = rule.find_first_not_of ('.', end0)) != std::string::npos) {
        end0 = rule.find ('.', start0);

        if (first) {
            first = false;
            continue;
        }

        std::string cur_rule = rule.substr (start0, end0 - start0);

        std::vector<std::string> rule_tokens {};
        this->parse_rule (cur_rule, &rule_tokens, '|');

        auto create_enforcement_stage_rule = create_enforcement_rule.add_rules ();

        create_enforcement_stage_rule->set_m_operation (rule_tokens[2]);
        create_enforcement_stage_rule->set_m_stage_name (rule_tokens[1]);

        auto& rules_map = *create_enforcement_stage_rule->mutable_env_rates ();

        size_t start1;
        size_t end1 = 0;

        while ((start1 = rule_tokens[3].find_first_not_of ('*', end1)) != std::string::npos) {
            end1 = rule_tokens[3].find ('*', start1);

            std::string token_rule = rule_tokens[3].substr (start1, end1 - start1);

            size_t start2;
            size_t end2 = 0;

            std::vector<std::string> tokens = {};
            while ((start2 = token_rule.find_first_not_of (':', end2)) != std::string::npos) {
                end2 = token_rule.find (':', start2);
                tokens.push_back (token_rule.substr (start2, end2 - start2));
            }

            auto env = std::stoll (tokens[0]);
            rules_map[env] = std::stoll (tokens[1]);
        }
    }

    controllers_grpc_interface::ACK reply;
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    auto begin = std::chrono::system_clock::now ();

    // write EnforcementRule object through user_address
    Status status
        = controller_stub_->CreateEnforcementRule (&context, create_enforcement_rule, &reply);

    auto end = std::chrono::system_clock::now ();

    passthrough_network_enforce_time
        += std::chrono::duration_cast<std::chrono::microseconds> (end - begin).count ();

    // write EnforcementRule object through user_address

    response.m_message = reply.m_message ();
    if (!status.ok ()) {
        Logging::log_error (
            "CoreInterface: create_enforcement_rule: Error while writing enforcement rule object "
            "to the local controller ("
            + status.error_message () + ").");
        return PStatus::Error ();
    } else if (reply.m_message () == static_cast<int> (AckCode::ok)) {
        // Logging::log_debug ("LocalInterface: create_enforcement_rule: ACK message received ("
        //     + std::to_string (response.m_message) + ").");
        return PStatus::OK ();
    } else {
        return PStatus::Error ();
    }
}

// create_controller_enforcement_rule call. Submit system limits to the lower controller.
PStatus CoreInterface::create_controller_enforcement_rule (const std::string& user_address,
    ControlOperation* operation,
    const std::string& rule,
    ACK& response)
{
    // validate if logging is enabled and log debug message
    if (Logging::is_debug_enabled ()) {
        // Logging::log_debug ("CoreInterface: create_controller_enforcement_rule: " + rule);
    }

    size_t start0;
    size_t end0 = 0;
    bool first = true;

    controllers_grpc_interface::OperationsLimits operation_limits;
    auto& rules_map = *operation_limits.mutable_limits_per_operation ();

    while ((start0 = rule.find_first_not_of ('|', end0)) != std::string::npos) {
        end0 = rule.find ('|', start0);

        if (first) {
            first = false;
            continue;
        }

        std::string cur_rule = rule.substr (start0, end0 - start0);

        Logging::log_debug ("CoreInterface: create_controller_enforcement_rule: " + cur_rule);

        // Extracting the operation and the rules

        size_t colon_pos = cur_rule.find (':');
        if (colon_pos != std::string::npos) {
            std::string operation = cur_rule.substr (0, colon_pos);
            auto limit = std::stoll (cur_rule.substr (colon_pos + 1));

            rules_map[operation] = limit;

            Logging::log_debug (
                "Extracted operation: " + operation + ", limit: " + std::to_string (limit));
        }
    }

    controllers_grpc_interface::ACK reply;
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    auto begin = std::chrono::system_clock::now ();

    // write EnforcementRule object through user_address
    Status status = controller_stub_->EnforceSystemLimit (&context, operation_limits, &reply);

    auto end = std::chrono::system_clock::now ();

    core_network_enforce_time
        += std::chrono::duration_cast<std::chrono::microseconds> (end - begin).count ();

    response.m_message = reply.m_message ();
    if (!status.ok ()) {
        Logging::log_error ("CoreInterface: create_controller_enforcement_rule: Error while "
                            "writing enforcement rule object "
                            "to the lower controller ("
            + status.error_message () + ").");
        return PStatus::Error ();
    } else if (reply.m_message () == static_cast<int> (AckCode::ok)) {
        // Logging::log_debug ("CoreInterface: create_controller_enforcement_rule: ACK message
        // received ("
        //     + std::to_string (response.m_message) + ").");
        return PStatus::OK ();
    } else {
        return PStatus::Error ();
    }
}

// collect_global_all_statistics call. Collect all statistics from data plane stages.
PStatus CoreInterface::collect_global_all_statistics (const std::string& user_address,
    ControlOperation* operation,
    std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<StageResponse>>>&
        stats_tf_objects)
{
    controllers_grpc_interface::Empty empty_place_holder;

    controllers_grpc_interface::StatsGlobalAllMap reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    Status status
        = controller_stub_->CollectGlobalAllStatistics (&context, empty_place_holder, &reply);

    if (!status.ok ()) {
        Logging::log_error (
            "CoreInterface: collect_global_all_statistics: Error while writing control operation ("
            + status.error_message () + ").");
        return PStatus::Error ();
    } else {

        for (auto stats : reply.gl_stats ()) {

            auto stage_data = stats.second;

            if (stage_data.all_rates ().empty ()) {
                stats_tf_objects->emplace (stats.first,
                    std::make_unique<StageResponseStatAll> (COLLECT_GLOBAL_ALL_STATS));
            } else {
                std::shared_ptr<std::unordered_map<std::string, uint64_t>> stats_all_ops
                    = std::make_shared<std::unordered_map<std::string, uint64_t>> ();

                for (auto stat : stage_data.all_rates ()) {
                    stats_all_ops->emplace (stat.first, stat.second);
                }

                stats_tf_objects->emplace (stats.first,
                    std::make_unique<StageResponseStatAll> (COLLECT_GLOBAL_ALL_STATS,
                        stage_data.stage_name (),
                        stage_data.stage_env (),
                        stats_all_ops));
            }
        }

        return PStatus::OK ();
    }
}

// collect_global_controller_statistics call. Collect aggregated statistics from the controller.
PStatus CoreInterface::collect_global_controller_statistics (const std::string& user_address,
    ControlOperation* operation,
    StageResponseStatController& stats)
{
    Logging::log_debug ("CoreInterface::collect_global_controller_statistics: Collecting global "
                        "controller statistics.");

    controllers_grpc_interface::Empty empty_place_holder;

    controllers_grpc_interface::StatsGlobalAggregatedMap reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    Status status
        = controller_stub_->CollectCoreControllerStatistics (&context, empty_place_holder, &reply);

    if (!status.ok ()) {
        Logging::log_error ("CoreInterface: collect_global_controller_statistics: Error while "
                            "writing control operation ("
            + status.error_message () + ").");
        return PStatus::Error ();
    } else {

        for (const auto& [operation, grpc_stats_rated] : reply.per_operation_usage ()) {

            StageResponseStatController::StatsRated stats_rated;
            for (const auto& [priority, usage] : grpc_stats_rated.usage_per_priority ()) {
                stats_rated.usage_per_priority[priority] = usage;
                Logging::log_debug ("CoreInterface::collect_global_controller_statistics "
                    + operation + " " + std::to_string (priority) + " " + std::to_string (usage));
            }
            stats.controller_stats.per_operation_usage[operation] = stats_rated;
        }

        // Convert counter_per_priority
        for (const auto& [priority, counter] : reply.counter_per_priority ()) {
            stats.controller_stats.counter_per_priority[priority] = counter;
            Logging::log_debug ("CoreInterface::collect_global_controller_statistics "
                + std::to_string (priority) + " " + std::to_string (counter));
        }

        return PStatus::OK ();
    }
}

////////////////////////////////////////////
//////////// Auxiliary Functions ///////////
////////////////////////////////////////////

// parse_rule call. Parse a rule into tokens using char c as delimiter.
void CoreInterface::parse_rule (const std::string& rule,
    std::vector<std::string>* tokens,
    const char c)
{
    size_t start;
    size_t end = 0;

    while ((start = rule.find_first_not_of (c, end)) != std::string::npos) {
        end = rule.find (c, start);
        tokens->push_back (rule.substr (start, end - start));
    }
}

// fill_housekeeping_rules_grpc call. Fill SimplifiedHandshakeRaw with rule data.
void CoreInterface::fill_housekeeping_rules_grpc (
    controllers_grpc_interface::SimplifiedHandshakeRaw* housekeeping_rules,
    const std::string& rule)
{
    size_t start;
    size_t end = 0;

    bool first = true;

    while ((start = rule.find_first_not_of (':', end)) != std::string::npos) {
        end = rule.find (':', start);

        // Exclude LOCAL_HANDSHAKE |
        if (first) {
            first = false;
            continue;
        }

        std::string token_rule = rule.substr (start, end - start);
        housekeeping_rules->add_rules (token_rule);
    }
}

} // namespace shio
