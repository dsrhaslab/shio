/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_RULES_FILE_PARSER_HPP
#define CHEFERD_RULES_FILE_PARSER_HPP

#include "cheferd/networking/interface/interface_definitions.hpp"

#include <cheferd/utils/options.hpp>
#include <fstream>
#include <iostream>
#include <vector>

namespace cheferd {

/**
 * RuleType enum class.
 * Defines the type of rules to be parsed, submitted, received, and handled.
 * Currently, it supports the following type:
 *  - housekeeping: respects to HousekeepingRules, which are used for general data plane management,
 *  including creation and configuration of Channels and EnforcementObjects;
 *  - differentiation: respects to DifferentiationRules, which are used to classify and
 *  differentiate I/O requests;
 *  - enforcement: respects to EnforcementRules, which are used to dynamically adjust the data plane
 *  state elements at execution time.
 */
enum class RuleType { housekeeping = 1, differentiation = 2, enforcement = 3, noop = 0 };

/**
 * RulesFileParser class.
 * RulesFileParser processes rules file. Each line of the file is a rule whose fields are separated
 * by spaces; lines are staged as token vectors and later converted into Raw structures.
 * Currently, the RulesFileParser class contains the following variables:
 * - m_rules_type: type of the rules in the file.
 * - m_staged_rules: tokens of each rule read from the file.
 * - m_create_channel_rules_min_elements: minimum number of tokens of a create_channel rule.
 * - m_create_object_rules_min_elements: minimum number of tokens of a create_object rule.
 */
class RulesFileParser {

private:
    RuleType m_rules_type { RuleType::noop };
    std::vector<std::vector<std::string>> m_staged_rules {};
    const int m_create_channel_rules_min_elements { 7 };
    const int m_create_object_rules_min_elements { 8 };

    /**
     * read_rules_from_file: Read rules from a given file and store them in the m_staged_rules
     * container.
     * @param path Path to the file that contains the rules.
     * @return Returns the number of rules stored.
     */
    int read_rules_from_file (const std::string& path);

    /**
     * parse_rule: Split line (rule in string) into tokens.
     * @param rule Rule in string format.
     * @param tokens Vector to store each token of the string-based rule.
     */
    void parse_rule (const std::string& rule, std::vector<std::string>* tokens);

public:
    /**
     * convert_housekeeping_operation: Convert string-based operation into the respective
     * HousekeepingOperation type.
     * @param operation String-based operation.
     * @return Returns the respective HousekeepingOperation; returns no-op for unlisted operations.
     */
    static HousekeepingOperation convert_housekeeping_operation (const std::string& operation);

    /**
     * convert_object_type: Convert string-based object type into the respective
     * EnforcementObjectType classifier and vice-versa.
     * @param object_type String-based enforcement object type.
     * @return Returns the respective EnforcementObjectType; returns ::NOOP for unlisted types.
     */
    static EnforcementObjectType convert_object_type (const std::string& object_type);

    /**
     * convert_object_type: Convert an EnforcementObjectType into its string-based format.
     * @param object_type EnforcementObjectType to convert.
     * @return Returns "drl" for DRL objects and "noop" otherwise.
     */
    static std::string convert_object_type (const EnforcementObjectType& object_type);

    /**
     * convert_enforcement_operation: Convert string-based enforcement operations into a listed
     * integer, based on the respective EnforcementObjectType and vice-versa.
     * @param object_type EnforcementObjectType of the operation to be executed.
     * @param operation String-based operation type.
     * @return Returns an integer that corresponds to the respective enforcement operation
     * (configuration).
     */
    static int convert_enforcement_operation (const EnforcementObjectType& object_type,
        const std::string& operation);

    /**
     * convert_enforcement_operation: Convert an enforcement operation into its string-based format.
     * @param operation Integer-based enforcement operation.
     * @return Returns the string-based operation ("init", "rate", "refill"), or "noop" for
     * unlisted operations.
     */
    static std::string convert_enforcement_operation (const int& operation);

    /**
     * convert_context_type_definition: Convert a string-based ContextType object to the
     * corresponding long value and vice-versa.
     * @param context_type String-based ContextType object.
     * @return Returns the corresponding long value of the ContextType; if the object is not
     * recognized, it returns -1.
     */
    static int convert_context_type_definition (const std::string& context_type);

    /**
     * convert_context_type_definition: Convert a ContextType into its string-based format.
     * @param context_type ContextType to convert.
     * @return Returns the string-based ContextType, or "noop" if not recognized.
     */
    static std::string convert_context_type_definition (const ContextType& context_type);

    /**
     * convert_differentiation_definitions: Convert I/O classification and differentiation
     * definitions from string-based format to the corresponding long value and vice-versa.
     * @param context_type String-based ContextType object, to select the correct conversion
     * method to use.
     * @param definition String-based definition for the I/O differentiation.
     * @return Returns the corresponding long value of the I/O definition.
     */
    static long convert_differentiation_definitions (const std::string& context_type,
        const std::string& definition);

    /**
     * convert_differentiation_definitions: Convert an I/O differentiation definition into its
     * string-based format.
     * @param context_type ContextType of the definition, to select the correct conversion method.
     * @param definition Integer-based definition for the I/O differentiation.
     * @return Returns the string-based definition, or an empty string for unknown context types.
     */
    static std::string convert_differentiation_definitions (const ContextType& context_type,
        const int& definition);

    /**
     * convert_paio_general_definitions: Convert PAIO_GENERAL differentiation definitions from a
     * string-based format to the corresponding long value and vice-versa.
     * @param general_definitions String-based definition of a PAIO_GENERAL element for the I/O
     * differentiation.
     * @return Returns the corresponding long value of the I/O definition.
     */
    static long convert_paio_general_definitions (const std::string& general_definitions);

    static std::string convert_paio_general_definitions (const PAIO_GENERAL& general_definitions);

    /**
     * convert_posix_lsm_simple_definitions: Convert LSM_KVS_SIMPLE differentiation
     * definitions from a string-based format to the corresponding long value and vice-versa.
     * @param posix_lsm_definitions String-based definition of a LSM_KVS_SIMPLE element for
     * the I/O differentiation.
     * @return Returns the corresponding long value of the I/O definition.
     */
    static long convert_posix_lsm_simple_definitions (const std::string& posix_lsm_definitions);

    static std::string convert_posix_lsm_simple_definitions (
        const LSM_KVS_SIMPLE& posix_lsm_definitions);

    /**
     * convert_posix_lsm_detailed_definitions: Convert LSM_KVS_DETAILED differentiation
     * definitions from a string-based format to the corresponding long value and vice-versa.
     * @param posix_lsm_definitions String-based definition of a LSM_KVS_DETAILED element for
     * the I/O differentiation.
     * @return Returns the corresponding long value of the I/O definition.
     */
    static long convert_posix_lsm_detailed_definitions (const std::string& posix_lsm_definitions);

    static std::string convert_posix_lsm_detailed_definitions (
        const LSM_KVS_DETAILED& posix_lsm_definitions);

    /**
     * convert_posix_definitions: Convert POSIX differentiation definitions from a string-based
     * format to the corresponding long value and vice-versa.
     * @param posix_definitions String-based definition of a POSIX element for the I/O
     * differentiation.
     * @return Returns the corresponding long value of the I/O definition.
     */
    static long convert_posix_definitions (const std::string& posix_definitions);

    static std::string convert_posix_definitions (const POSIX& posix_definitions);

    /**
     * convert_posix_meta_definitions: Convert POSIX_META differentiation definitions from a
     * string-based format to the corresponding long value and vice-versa.
     * @param posix_meta_definitions String-based definition of a POSIX_META element for the I/O
     * differentiation.
     * @return Returns the corresponding long value of the I/O definition.
     */
    static long convert_posix_meta_definitions (const std::string& posix_meta_definitions);

    static std::string convert_posix_meta_definitions (const POSIX_META& posix_meta_definitions);

    /**
     * convert_op_test_definitions: Convert OP_TEST differentiation definitions from a
     * string-based format to the corresponding long value and vice-versa.
     * @param op_test_definitions String-based definition of a OP_TEST element for the I/O
     * differentiation.
     * @return Returns the corresponding long value of the I/O definition.
     */
    static long convert_op_test_definitions (const std::string& op_test_definitions);

    static std::string convert_op_test_definitions (const OP_TEST& op_test_definitions);

    /**
     * convert_kvs_definitions: Convert KVS differentiation definitions from a string-based format
     * to the corresponding long value and vice-versa.
     * @param kvs_definitions String-based definition of a KVS element for the I/O differentiation.
     * @return Returns the corresponding long value of the I/O definition.
     */
    static long convert_kvs_definitions (const std::string& kvs_definitions);

    static std::string convert_kvs_definitions (const KVS& kvs_definitions);

    /**
     * RulesFileParser default constructor.
     */
    RulesFileParser ();

    /**
     * RulesFileParser parameterized constructor.
     * @param type type of rules the file comprises. Rules can be of type: HSK
     * (Housekeeping), DIF (Differentiation), and ENF (Enforcement).
     * @param path Path to the file that contains the rules.
     */
    RulesFileParser (RuleType type, const std::string& path);

    /**
     * RulesFileParser default destructor.
     */
    ~RulesFileParser ();

    /**
     * get_rule_type: Get the type of the rules in the file.
     * @return Returns the RuleType of the file.
     */
    RuleType get_rule_type () const;

    /**
     * get_create_channel_rules: read raw housekeeping rules of type
     * HSK_CREATE_CHANNEL from vector and store them in hsk_rules.
     * @param hsk_rules reference to a container that stores the RAW structure.
     * @param total_rules number of rules to store in the container (-1
     * indicates to pass all rules).
     * @return returns the number of rules stored in the container.
     */
    int get_create_channel_rules (std::vector<HousekeepingCreateChannelRaw>& hsk_rules,
        int total_rules);

    /**
     * get_create_object_rules: read raw housekeeping rules of type
     * HSK_CREATE_OBJECT from vector and store them in hsk_rules.
     * @param hsk_rules reference to a container that stores the RAW structure.
     * @param total_rules number of rules to store in the container (-1
     * indicates to pass all rules).
     * @return returns the number of rules stored in the container.
     */
    int get_create_object_rules (std::vector<HousekeepingCreateObjectRaw>& hsk_rules,
        int total_rules);

    /**
     * erase_rules: Remove all rules stored in the m_staged_rules container.
     * @return Returns the number of rules removed.
     */
    int erase_rules ();

    /**
     * print_rules: Write to stdout the rules stored in the m_staged_rules
     * container.
     */
    void print_rules () const;
};
} // namespace cheferd

#endif // CHEFERD_RULES_FILE_PARSER_HPP
