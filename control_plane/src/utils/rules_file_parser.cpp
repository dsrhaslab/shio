/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/utils/logging.hpp"

#include <shio/utils/rules_file_parser.hpp>
#include <limits>

namespace shio {

// RulesFileParser default constructor.
RulesFileParser::RulesFileParser ()
{
    // Logging::log_debug ("RulesFileParser default constructor.");
}

// RulesFileParser parameterized constructor.
RulesFileParser::RulesFileParser (RuleType type, const std::string& path) :
    m_rules_type { type },
    m_staged_rules {}
{
    Logging::log_debug ("RulesFileParser parameterized constructor.");
    this->read_rules_from_file (path);
}

// RulesFileParser default destructor.
RulesFileParser::~RulesFileParser ()
{
    Logging::log_debug ("RulesFileParser default destructor.");
}

/**
 * hash: Compute a hash based on a string value. The method uses an offset (defaults to 0), and
 * a salt/tweak (defaults to 10242048).
 * @param s String-based value to be computed.
 * @param off Offset value.
 * @param salt Salt to generate pseudo-random hash values.
 * @return Returns an unsigned int value that corresponds to the original string.
 */
constexpr unsigned int hash (const char* s, int off = 0, int salt = 10242048)
{
    return !s[off] ? salt : (hash (s, off + 1) * 33) ^ s[off];
}

/**
 * operator ""_: String operator that converts a string to the corresponding hash value.
 * @param string_value String to be computed.
 * @return Returns an unsigned int value that corresponds to the original string.
 */
constexpr inline unsigned int operator""_ (char const* string_value, size_t)
{
    return hash (string_value);
}

// get_rule_type call. Get the type of the rules in the file.
RuleType RulesFileParser::get_rule_type () const
{
    return this->m_rules_type;
}

// parse_rule call. Auxiliary method for parsing a line and storing its tokens in a container.
void RulesFileParser::parse_rule (const std::string& rule, std::vector<std::string>* tokens)
{
    size_t start;
    size_t end = 0;

    while ((start = rule.find_first_not_of (' ', end)) != std::string::npos) {
        end = rule.find (' ', start);
        tokens->push_back (rule.substr (start, end - start));
    }
}

// read_rules_from_file call. Read the rules from file at path.
int RulesFileParser::read_rules_from_file (const std::string& path)
{
    std::string line;
    std::ifstream input_stream;

    int total_rules = 0;
    // open file stream
    input_stream.open (path);

    // verify if stream is open
    if (input_stream.is_open ()) {
        // read line and store in line_t
        while (std::getline (input_stream, line)) {
            // send to parser and store in the respective structure
            std::vector<std::string> tokens {};
            // parse line
            this->parse_rule (line, &tokens);

            // store parsed tokens
            this->m_staged_rules.push_back (tokens);
            total_rules++;
        }

        // close file stream
        input_stream.close ();
    } else {
        Logging::log_error ("RulesFileParser: cannot open file " + path + ".");
    }

    return total_rules;
}

// convert_housekeeping_operation call. Convert a string to a HousekeepingOperation.
HousekeepingOperation RulesFileParser::convert_housekeeping_operation (const std::string& operation)
{
    switch (shio::hash (operation.data ())) {
        case "create_channel"_:
            return HousekeepingOperation::create_channel;
        case "create_object"_:
            return HousekeepingOperation::create_object;
        default:
            return HousekeepingOperation::no_op;
    }
}

// convert_object_type call. Convert a string to the respective EnforcementObjectType and
// vice-versa.
EnforcementObjectType RulesFileParser::convert_object_type (const std::string& object_type)
{
    return (object_type == "drl") ? EnforcementObjectType::DRL : EnforcementObjectType::NOOP;
}

std::string RulesFileParser::convert_object_type (const EnforcementObjectType& object_type)
{
    return (object_type == EnforcementObjectType::DRL) ? "drl" : "noop";
}

// convert_enforcement_operation call. Convert a string to an enforcement operation and vice-versa.
int RulesFileParser::convert_enforcement_operation (const EnforcementObjectType& object_type,
    const std::string& operation)
{
    switch (object_type) {
        case EnforcementObjectType::DRL:
            switch (shio::hash (operation.data ())) {
                case "init"_:
                    return 1;
                case "rate"_:
                    return 2;
                case "refill"_:
                    return 4;
                default:
                    return 0;
            }

        case EnforcementObjectType::NOOP:
            return 0;
    }
}

std::string RulesFileParser::convert_enforcement_operation (const int& operation)
{
    switch (operation) {
        case 1:
            return "init";
        case 2:
            return "rate";
        case 4:
            return "refill";
        default:
            return "noop";
    }
}

// convert_context_type_definition call. Convert string-based ContextType value to long and
// vice-versa.
int RulesFileParser::convert_context_type_definition (const std::string& context_type)
{
    switch (shio::hash (context_type.data ())) {
        case "general"_:
            return static_cast<long> (ContextType::PAIO_GENERAL);
        case "posix"_:
            return static_cast<long> (ContextType::POSIX);
        case "posix_meta"_:
            return static_cast<long> (ContextType::POSIX_META);
        case "lsm_kvs_simple"_:
            return static_cast<long> (ContextType::LSM_KVS_SIMPLE);
        case "lsm_kvs_detailed"_:
            return static_cast<long> (ContextType::LSM_KVS_DETAILED);
        case "kvs"_:
            return static_cast<long> (ContextType::KVS);
        case "op_test"_:
            return static_cast<long> (ContextType::OP_TEST);
        default:
            return -1;
    }
}

// convert_context_type_definition call. Convert ContextType value to string.
std::string RulesFileParser::convert_context_type_definition (const ContextType& context_type)
{
    switch (context_type) {
        case ContextType::PAIO_GENERAL:
            return "general";
        case ContextType::POSIX:
            return "posix";
        case ContextType::POSIX_META:
            return "posix_meta";
        case ContextType::LSM_KVS_SIMPLE:
            return "lsm_kvs_simple";
        case ContextType::LSM_KVS_DETAILED:
            return "lsm_kvs_detailed";
        case ContextType::KVS:
            return "kvs";
        default:
            return "noop";
    }
}

// convert_differentiation_definitions call. Convert I/O differentiation and classification
// definitions from string to long and vice-versa.
long RulesFileParser::convert_differentiation_definitions (const std::string& context_type,
    const std::string& definition)
{
    switch (shio::hash (context_type.data ())) {
        case "general"_:
            return convert_paio_general_definitions (definition);
        case "posix"_:
            return convert_posix_definitions (definition);
        case "posix_meta"_:
            return convert_posix_meta_definitions (definition);
        case "lsm_kvs_simple"_:
            return convert_posix_lsm_simple_definitions (definition);
        case "lsm_kvs_detailed"_:
            return convert_posix_lsm_detailed_definitions (definition);
        case "kvs"_:
            return convert_kvs_definitions (definition);
        case "op_test"_:
            return convert_op_test_definitions (definition);
        default:
            return -1;
    }
}

std::string RulesFileParser::convert_differentiation_definitions (const ContextType& context_type,
    const int& definition)
{
    switch (context_type) {
        case ContextType::PAIO_GENERAL:
            return convert_paio_general_definitions (static_cast<PAIO_GENERAL> (definition));
        case ContextType::POSIX:
            return convert_posix_definitions (static_cast<POSIX> (definition));
        case ContextType::POSIX_META:
            return convert_posix_meta_definitions (static_cast<POSIX_META> (definition));
        case ContextType::LSM_KVS_SIMPLE:
            return convert_posix_lsm_simple_definitions (static_cast<LSM_KVS_SIMPLE> (definition));
        case ContextType::LSM_KVS_DETAILED:
            return convert_posix_lsm_detailed_definitions (
                static_cast<LSM_KVS_DETAILED> (definition));
        case ContextType::KVS:
            return convert_kvs_definitions (static_cast<KVS> (definition));
        case ContextType::OP_TEST:
            return convert_op_test_definitions (static_cast<OP_TEST> (definition));
        default:
            return "";
    }
}

// convert_paio_general_definitions call. Convert PAIO_GENERAL differentiation definitions from
// string to long and vice-versa.
long RulesFileParser::convert_paio_general_definitions (const std::string& general_definitions)
{
    switch (shio::hash (general_definitions.data ())) {
        case "foreground"_:
            return static_cast<long> (PAIO_GENERAL::foreground);
        case "background"_:
            return static_cast<long> (PAIO_GENERAL::background);
        case "high_priority"_:
            return static_cast<long> (PAIO_GENERAL::high_priority);
        case "low_priority"_:
            return static_cast<long> (PAIO_GENERAL::low_priority);
        default:
            return static_cast<long> (PAIO_GENERAL::no_op);
    }
}

std::string RulesFileParser::convert_paio_general_definitions (
    const PAIO_GENERAL& general_definitions)
{
    switch (general_definitions) {
        case PAIO_GENERAL::foreground:
            return "foreground";
        case PAIO_GENERAL::background:
            return "background";
        case PAIO_GENERAL::high_priority:
            return "high_priority";
        case PAIO_GENERAL::low_priority:
            return "low_priority";
        default:
            return "no_op";
    }
}

// convert_posix_lsm_simple_definitions call. Convert LSM_KVS_SIMPLE differentiation
// definitions from string to long and vice-versa.
long RulesFileParser::convert_posix_lsm_simple_definitions (
    const std::string& posix_lsm_definitions)
{
    switch (shio::hash (posix_lsm_definitions.data ())) {
        case "bg_flush"_:
            return static_cast<long> (LSM_KVS_SIMPLE::bg_flush);
        case "bg_compaction_high_priority"_:
            return static_cast<long> (LSM_KVS_SIMPLE::bg_compaction_high_priority);
        case "bg_compaction_low_priority"_:
            return static_cast<long> (LSM_KVS_SIMPLE::bg_compaction_low_priority);
        case "foreground"_:
            return static_cast<long> (LSM_KVS_SIMPLE::foreground);
        default:
            return static_cast<long> (LSM_KVS_SIMPLE::no_op);
    }
}

std::string RulesFileParser::convert_posix_lsm_simple_definitions (
    const LSM_KVS_SIMPLE& posix_lsm_definitions)
{
    switch (posix_lsm_definitions) {
        case LSM_KVS_SIMPLE::bg_flush:
            return "bg_flush";
        case LSM_KVS_SIMPLE::bg_compaction_high_priority:
            return "bg_compaction_high_priority";
        case LSM_KVS_SIMPLE::bg_compaction_low_priority:
            return "bg_compaction_low_priority";
        case LSM_KVS_SIMPLE::foreground:
            return "foreground";
        default:
            return "no_op";
    }
}

// convert_posix_lsm_detailed_definitions call. Convert LSM_KVS_DETAILED differentiation
// definitions from string to long and vice-versa.
long RulesFileParser::convert_posix_lsm_detailed_definitions (
    const std::string& posix_lsm_definitions)
{
    switch (shio::hash (posix_lsm_definitions.data ())) {
        case "bg_flush"_:
            return static_cast<long> (LSM_KVS_DETAILED::bg_flush);
        case "bg_compaction"_:
            return static_cast<long> (LSM_KVS_DETAILED::bg_compaction);
        case "bg_compaction_L0_L0"_:
            return static_cast<long> (LSM_KVS_DETAILED::bg_compaction_L0_L0);
        case "bg_compaction_L0_L1"_:
            return static_cast<long> (LSM_KVS_DETAILED::bg_compaction_L0_L1);
        case "bg_compaction_L1_L2"_:
            return static_cast<long> (LSM_KVS_DETAILED::bg_compaction_L1_L2);
        case "bg_compaction_L2_L3"_:
            return static_cast<long> (LSM_KVS_DETAILED::bg_compaction_L2_L3);
        case "bg_compaction_LN"_:
            return static_cast<long> (LSM_KVS_DETAILED::bg_compaction_LN);
        case "foreground"_:
            return static_cast<long> (LSM_KVS_DETAILED::foreground);
        default:
            return static_cast<long> (LSM_KVS_DETAILED::no_op);
    }
}

std::string RulesFileParser::convert_posix_lsm_detailed_definitions (
    const LSM_KVS_DETAILED& posix_lsm_definitions)
{
    switch (posix_lsm_definitions) {
        case LSM_KVS_DETAILED::bg_flush:
            return "bg_flush";
        case LSM_KVS_DETAILED::bg_compaction:
            return "bg_compaction";
        case LSM_KVS_DETAILED::bg_compaction_L0_L0:
            return "bg_compaction_L0_L0";
        case LSM_KVS_DETAILED::bg_compaction_L0_L1:
            return "bg_compaction_L0_L1";
        case LSM_KVS_DETAILED::bg_compaction_L1_L2:
            return "bg_compaction_L1_L2";
        case LSM_KVS_DETAILED::bg_compaction_L2_L3:
            return "bg_compaction_L2_L3";
        case LSM_KVS_DETAILED::bg_compaction_LN:
            return "bg_compaction_LN";
        case LSM_KVS_DETAILED::foreground:
            return "foreground";
        default:
            return "no_op";
    }
}

// convert_posix_definitions call. Convert POSIX differentiation definitions from string to long and
// vice-versa.
long RulesFileParser::convert_posix_definitions (const std::string& posix_definitions)
{
    switch (shio::hash (posix_definitions.data ())) {
        case "read"_:
            return static_cast<long> (POSIX::read);
        case "write"_:
            return static_cast<long> (POSIX::write);
        case "pread"_:
            return static_cast<long> (POSIX::pread);
        case "pwrite"_:
            return static_cast<long> (POSIX::pwrite);
        case "pread64"_:
            return static_cast<long> (POSIX::pread64);
        case "pwrite64"_:
            return static_cast<long> (POSIX::pwrite64);
        case "fread"_:
            return static_cast<long> (POSIX::fread);
        case "fwrite"_:
            return static_cast<long> (POSIX::fwrite);
        case "open"_:
            return static_cast<long> (POSIX::open);
        case "open64"_:
            return static_cast<long> (POSIX::open64);
        case "creat"_:
            return static_cast<long> (POSIX::creat);
        case "creat64"_:
            return static_cast<long> (POSIX::creat64);
        case "openat"_:
            return static_cast<long> (POSIX::openat);
        case "close"_:
            return static_cast<long> (POSIX::close);
        case "fsync"_:
            return static_cast<long> (POSIX::fsync);
        case "fdatasync"_:
            return static_cast<long> (POSIX::fdatasync);
        case "sync"_:
            return static_cast<long> (POSIX::sync);
        case "syncfs"_:
            return static_cast<long> (POSIX::syncfs);
        case "truncate"_:
            return static_cast<long> (POSIX::truncate);
        case "ftruncate"_:
            return static_cast<long> (POSIX::ftruncate);
        case "truncate64"_:
            return static_cast<long> (POSIX::truncate64);
        case "ftruncate64"_:
            return static_cast<long> (POSIX::ftruncate64);
        case "xstat"_:
            return static_cast<long> (POSIX::xstat);
        case "lxstat"_:
            return static_cast<long> (POSIX::lxstat);
        case "fxstat"_:
            return static_cast<long> (POSIX::fxstat);
        case "xstat64"_:
            return static_cast<long> (POSIX::xstat64);
        case "lxstat64"_:
            return static_cast<long> (POSIX::lxstat64);
        case "fxstat64"_:
            return static_cast<long> (POSIX::fxstat64);
        case "fxstatat"_:
            return static_cast<long> (POSIX::fxstatat);
        case "fxstatat64"_:
            return static_cast<long> (POSIX::fxstatat64);
        case "statfs"_:
            return static_cast<long> (POSIX::statfs);
        case "fstatfs"_:
            return static_cast<long> (POSIX::fstatfs);
        case "statfs64"_:
            return static_cast<long> (POSIX::statfs64);
        case "fstatfs64"_:
            return static_cast<long> (POSIX::fstatfs64);
        case "link"_:
            return static_cast<long> (POSIX::link);
        case "linkat"_:
            return static_cast<long> (POSIX::linkat);
        case "unlink"_:
            return static_cast<long> (POSIX::unlink);
        case "unlinkat"_:
            return static_cast<long> (POSIX::unlinkat);
        case "rename"_:
            return static_cast<long> (POSIX::rename);
        case "renameat"_:
            return static_cast<long> (POSIX::renameat);
        case "symlink"_:
            return static_cast<long> (POSIX::symlink);
        case "symlinkat"_:
            return static_cast<long> (POSIX::symlinkat);
        case "readlink"_:
            return static_cast<long> (POSIX::readlink);
        case "readlinkat"_:
            return static_cast<long> (POSIX::readlinkat);
        case "fopen"_:
            return static_cast<long> (POSIX::fopen);
        case "fopen64"_:
            return static_cast<long> (POSIX::fopen64);
        case "freopen"_:
            return static_cast<long> (POSIX::freopen);
        case "freopen64"_:
            return static_cast<long> (POSIX::freopen64);
        case "fclose"_:
            return static_cast<long> (POSIX::fclose);
        case "fflush"_:
            return static_cast<long> (POSIX::fflush);
        case "access"_:
            return static_cast<long> (POSIX::access);
        case "faccessat"_:
            return static_cast<long> (POSIX::faccessat);
        case "lseek"_:
            return static_cast<long> (POSIX::lseek);
        case "lseek64"_:
            return static_cast<long> (POSIX::lseek64);
        case "fseek"_:
            return static_cast<long> (POSIX::fseek);
        case "fseek64"_:
            return static_cast<long> (POSIX::fseek64);
        case "ftell"_:
            return static_cast<long> (POSIX::ftell);
        case "fseeko"_:
            return static_cast<long> (POSIX::fseeko);
        case "fseeko64"_:
            return static_cast<long> (POSIX::fseeko64);
        case "ftello"_:
            return static_cast<long> (POSIX::ftello);
        case "ftello64"_:
            return static_cast<long> (POSIX::ftello64);
        case "mkdir"_:
            return static_cast<long> (POSIX::mkdir);
        case "mkdirat"_:
            return static_cast<long> (POSIX::mkdirat);
        case "rmdir"_:
            return static_cast<long> (POSIX::rmdir);
        case "opendir"_:
            return static_cast<long> (POSIX::opendir);
        case "readdir"_:
            return static_cast<long> (POSIX::readdir);
        case "readdir64"_:
            return static_cast<long> (POSIX::readdir64);
        case "fdopendir"_:
            return static_cast<long> (POSIX::fdopendir);
        case "closedir"_:
            return static_cast<long> (POSIX::closedir);
        case "dirfd"_:
            return static_cast<long> (POSIX::dirfd);
        case "getxattr"_:
            return static_cast<long> (POSIX::getxattr);
        case "lgetxattr"_:
            return static_cast<long> (POSIX::lgetxattr);
        case "fgetxattr"_:
            return static_cast<long> (POSIX::fgetxattr);
        case "setxattr"_:
            return static_cast<long> (POSIX::setxattr);
        case "lsetxattr"_:
            return static_cast<long> (POSIX::lsetxattr);
        case "fsetxattr"_:
            return static_cast<long> (POSIX::fsetxattr);
        case "removexattr"_:
            return static_cast<long> (POSIX::removexattr);
        case "lremovexattr"_:
            return static_cast<long> (POSIX::lremovexattr);
        case "fremovexattr"_:
            return static_cast<long> (POSIX::fremovexattr);
        case "listxattr"_:
            return static_cast<long> (POSIX::listxattr);
        case "llistxattr"_:
            return static_cast<long> (POSIX::llistxattr);
        case "flistxattr"_:
            return static_cast<long> (POSIX::flistxattr);
        case "chmod"_:
            return static_cast<long> (POSIX::chmod);
        case "fchmod"_:
            return static_cast<long> (POSIX::fchmod);
        case "fchmodat"_:
            return static_cast<long> (POSIX::fchmodat);
        case "chown"_:
            return static_cast<long> (POSIX::chown);
        case "fchown"_:
            return static_cast<long> (POSIX::fchown);
        case "fchownat"_:
            return static_cast<long> (POSIX::fchownat);
        case "lchown"_:
            return static_cast<long> (POSIX::lchown);
        case "data"_:
            return static_cast<long> (POSIX::data);
        case "metadata"_:
            return static_cast<long> (POSIX::metadata);
        case "total"_:
            return static_cast<long> (POSIX::total);
        default:
            return static_cast<long> (POSIX::no_op);
    }
}

std::string RulesFileParser::convert_posix_definitions (const POSIX& posix_definitions)
{
    switch (posix_definitions) {
        case POSIX::read:
            return "read";
        case POSIX::write:
            return "write";
        case POSIX::pread:
            return "pread";
        case POSIX::pwrite:
            return "pwrite";
        case POSIX::pread64:
            return "pread64";
        case POSIX::pwrite64:
            return "pwrite64";
        case POSIX::open:
            return "open";
        case POSIX::close:
            return "close";
        case POSIX::getxattr:
            return "getxattr";
        case POSIX::rename:
            return "rename";
        case POSIX::setxattr:
            return "setxattr";
        case POSIX::mkdir:
            return "mkdir";
        case POSIX::mknod:
            return "mknod";
        case POSIX::rmdir:
            return "rmdir";
        case POSIX::statfs:
            return "statfs";
        case POSIX::sync:
            return "sync";
        case POSIX::unlink:
            return "unlink";
        case POSIX::data:
            return "data";
        case POSIX::metadata:
            return "metadata";
        case POSIX::total:
            return "total";
        default:
            return "no_op";
    }
}

// convert_posix_meta_definitions call. Convert POSIX_META differentiation definitions from string
// to long and vice-versa.
long RulesFileParser::convert_posix_meta_definitions (const std::string& posix_meta_definitions)
{
    switch (shio::hash (posix_meta_definitions.data ())) {
        case "foreground"_:
            return static_cast<long> (POSIX_META::foreground);
        case "background"_:
            return static_cast<long> (POSIX_META::background);
        case "high_priority"_:
            return static_cast<long> (POSIX_META::high_priority);
        case "med_priority"_:
            return static_cast<long> (POSIX_META::med_priority);
        case "low_priority"_:
            return static_cast<long> (POSIX_META::low_priority);
        case "data_op"_:
            return static_cast<long> (POSIX_META::data_op);
        case "meta_op"_:
            return static_cast<long> (POSIX_META::meta_op);
        case "dir_op"_:
            return static_cast<long> (POSIX_META::dir_op);
        case "ext_attr_op"_:
            return static_cast<long> (POSIX_META::ext_attr_op);
        case "file_mod_op"_:
            return static_cast<long> (POSIX_META::file_mod_op);
        default:
            return static_cast<long> (POSIX::no_op);
    }
}

std::string RulesFileParser::convert_posix_meta_definitions (
    const POSIX_META& posix_meta_definitions)
{
    switch (posix_meta_definitions) {
        case POSIX_META::foreground:
            return "foreground";
        case POSIX_META::background:
            return "background";
        case POSIX_META::high_priority:
            return "high_priority";
        case POSIX_META::med_priority:
            return "med_priority";
        case POSIX_META::low_priority:
            return "low_priority";
        case POSIX_META::data_op:
            return "data_op";
        case POSIX_META::meta_op:
            return "meta_op";
        case POSIX_META::dir_op:
            return "dir_op";
        case POSIX_META::ext_attr_op:
            return "ext_attr_op";
        case POSIX_META::file_mod_op:
            return "file_mod_op";
        default:
            return "no_op";
    }
}

// convert_op_test_definitions call. Convert OP_TEST differentiation definitions from string to
// long and vice-versa.
long RulesFileParser::convert_op_test_definitions (const std::string& op_test_definitions)
{
    switch (shio::hash (op_test_definitions.data ())) {
        case "op1"_:
            return static_cast<long> (OP_TEST::op1);
        case "op2"_:
            return static_cast<long> (OP_TEST::op2);
        case "op3"_:
            return static_cast<long> (OP_TEST::op3);
        case "op4"_:
            return static_cast<long> (OP_TEST::op4);
        case "op5"_:
            return static_cast<long> (OP_TEST::op5);
        case "op6"_:
            return static_cast<long> (OP_TEST::op6);
        case "op7"_:
            return static_cast<long> (OP_TEST::op7);
        case "op8"_:
            return static_cast<long> (OP_TEST::op8);
        case "op9"_:
            return static_cast<long> (OP_TEST::op9);
        case "op10"_:
            return static_cast<long> (OP_TEST::op10);
        case "op11"_:
            return static_cast<long> (OP_TEST::op11);
        case "op12"_:
            return static_cast<long> (OP_TEST::op12);
        case "op13"_:
            return static_cast<long> (OP_TEST::op13);
        case "op14"_:
            return static_cast<long> (OP_TEST::op14);
        case "op15"_:
            return static_cast<long> (OP_TEST::op15);
        case "op16"_:
            return static_cast<long> (OP_TEST::op16);
        case "op17"_:
            return static_cast<long> (OP_TEST::op17);
        case "op18"_:
            return static_cast<long> (OP_TEST::op18);
        case "op19"_:
            return static_cast<long> (OP_TEST::op19);
        case "op20"_:
            return static_cast<long> (OP_TEST::op20);
        case "op21"_:
            return static_cast<long> (OP_TEST::op21);
        case "op22"_:
            return static_cast<long> (OP_TEST::op22);
        case "op23"_:
            return static_cast<long> (OP_TEST::op23);
        case "op24"_:
            return static_cast<long> (OP_TEST::op24);
        case "op25"_:
            return static_cast<long> (OP_TEST::op25);
        case "op26"_:
            return static_cast<long> (OP_TEST::op26);
        case "op27"_:
            return static_cast<long> (OP_TEST::op27);
        case "op28"_:
            return static_cast<long> (OP_TEST::op28);
        case "op29"_:
            return static_cast<long> (OP_TEST::op29);
        case "op30"_:
            return static_cast<long> (OP_TEST::op30);
        case "op31"_:
            return static_cast<long> (OP_TEST::op31);
        case "op32"_:
            return static_cast<long> (OP_TEST::op32);
        case "op33"_:
            return static_cast<long> (OP_TEST::op33);
        case "op34"_:
            return static_cast<long> (OP_TEST::op34);
        case "op35"_:
            return static_cast<long> (OP_TEST::op35);
        case "op36"_:
            return static_cast<long> (OP_TEST::op36);
        case "op37"_:
            return static_cast<long> (OP_TEST::op37);
        case "op38"_:
            return static_cast<long> (OP_TEST::op38);
        case "op39"_:
            return static_cast<long> (OP_TEST::op39);
        case "op40"_:
            return static_cast<long> (OP_TEST::op40);
        case "op41"_:
            return static_cast<long> (OP_TEST::op41);
        case "op42"_:
            return static_cast<long> (OP_TEST::op42);
        case "op43"_:
            return static_cast<long> (OP_TEST::op43);
        case "op44"_:
            return static_cast<long> (OP_TEST::op44);
        case "op45"_:
            return static_cast<long> (OP_TEST::op45);
        case "op46"_:
            return static_cast<long> (OP_TEST::op46);
        case "op47"_:
            return static_cast<long> (OP_TEST::op47);
        case "op48"_:
            return static_cast<long> (OP_TEST::op48);
        case "op49"_:
            return static_cast<long> (OP_TEST::op49);
        case "op50"_:
            return static_cast<long> (OP_TEST::op50);
        case "op51"_:
            return static_cast<long> (OP_TEST::op51);
        case "op52"_:
            return static_cast<long> (OP_TEST::op52);
        case "op53"_:
            return static_cast<long> (OP_TEST::op53);
        case "op54"_:
            return static_cast<long> (OP_TEST::op54);
        case "op55"_:
            return static_cast<long> (OP_TEST::op55);
        case "op56"_:
            return static_cast<long> (OP_TEST::op56);
        case "op57"_:
            return static_cast<long> (OP_TEST::op57);
        case "op58"_:
            return static_cast<long> (OP_TEST::op58);
        case "op59"_:
            return static_cast<long> (OP_TEST::op59);
        case "op60"_:
            return static_cast<long> (OP_TEST::op60);
        case "op61"_:
            return static_cast<long> (OP_TEST::op61);
        case "op62"_:
            return static_cast<long> (OP_TEST::op62);
        case "op63"_:
            return static_cast<long> (OP_TEST::op63);
        case "op64"_:
            return static_cast<long> (OP_TEST::op64);
        case "op65"_:
            return static_cast<long> (OP_TEST::op65);
        case "op66"_:
            return static_cast<long> (OP_TEST::op66);
        case "op67"_:
            return static_cast<long> (OP_TEST::op67);
        case "op68"_:
            return static_cast<long> (OP_TEST::op68);
        case "op69"_:
            return static_cast<long> (OP_TEST::op69);
        case "op70"_:
            return static_cast<long> (OP_TEST::op70);
        case "op71"_:
            return static_cast<long> (OP_TEST::op71);
        case "op72"_:
            return static_cast<long> (OP_TEST::op72);
        case "op73"_:
            return static_cast<long> (OP_TEST::op73);
        case "op74"_:
            return static_cast<long> (OP_TEST::op74);
        case "op75"_:
            return static_cast<long> (OP_TEST::op75);
        case "op76"_:
            return static_cast<long> (OP_TEST::op76);
        case "op77"_:
            return static_cast<long> (OP_TEST::op77);
        case "op78"_:
            return static_cast<long> (OP_TEST::op78);
        case "op79"_:
            return static_cast<long> (OP_TEST::op79);
        case "op80"_:
            return static_cast<long> (OP_TEST::op80);
        case "op81"_:
            return static_cast<long> (OP_TEST::op81);
        case "op82"_:
            return static_cast<long> (OP_TEST::op82);
        case "op83"_:
            return static_cast<long> (OP_TEST::op83);
        case "op84"_:
            return static_cast<long> (OP_TEST::op84);
        case "op85"_:
            return static_cast<long> (OP_TEST::op85);
        case "op86"_:
            return static_cast<long> (OP_TEST::op86);
        case "op87"_:
            return static_cast<long> (OP_TEST::op87);
        case "op88"_:
            return static_cast<long> (OP_TEST::op88);
        case "op89"_:
            return static_cast<long> (OP_TEST::op89);
        case "op90"_:
            return static_cast<long> (OP_TEST::op90);
        case "op91"_:
            return static_cast<long> (OP_TEST::op91);
        case "op92"_:
            return static_cast<long> (OP_TEST::op92);
        case "op93"_:
            return static_cast<long> (OP_TEST::op93);
        case "op94"_:
            return static_cast<long> (OP_TEST::op94);
        case "op95"_:
            return static_cast<long> (OP_TEST::op95);
        case "op96"_:
            return static_cast<long> (OP_TEST::op96);
        case "op97"_:
            return static_cast<long> (OP_TEST::op97);
        case "op98"_:
            return static_cast<long> (OP_TEST::op98);
        case "op99"_:
            return static_cast<long> (OP_TEST::op99);
        case "op100"_:
            return static_cast<long> (OP_TEST::op100);
        default:
            return static_cast<long> (POSIX::no_op);
    }
}

std::string RulesFileParser::convert_op_test_definitions (const OP_TEST& op_test_definitions)
{
    switch (op_test_definitions) {
        case OP_TEST::op1:
            return "op1";
        case OP_TEST::op2:
            return "op2";
        case OP_TEST::op3:
            return "op3";
        case OP_TEST::op4:
            return "op4";
        case OP_TEST::op5:
            return "op5";
        case OP_TEST::op6:
            return "op6";
        case OP_TEST::op7:
            return "op7";
        case OP_TEST::op8:
            return "op8";
        case OP_TEST::op9:
            return "op9";
        case OP_TEST::op10:
            return "op10";
        case OP_TEST::op11:
            return "op11";
        case OP_TEST::op12:
            return "op12";
        case OP_TEST::op13:
            return "op13";
        case OP_TEST::op14:
            return "op14";
        case OP_TEST::op15:
            return "op15";
        case OP_TEST::op16:
            return "op16";
        case OP_TEST::op17:
            return "op17";
        case OP_TEST::op18:
            return "op18";
        case OP_TEST::op19:
            return "op19";
        case OP_TEST::op20:
            return "op20";
        case OP_TEST::op21:
            return "op21";
        case OP_TEST::op22:
            return "op22";
        case OP_TEST::op23:
            return "op23";
        case OP_TEST::op24:
            return "op24";
        case OP_TEST::op25:
            return "op25";
        case OP_TEST::op26:
            return "op26";
        case OP_TEST::op27:
            return "op27";
        case OP_TEST::op28:
            return "op28";
        case OP_TEST::op29:
            return "op29";
        case OP_TEST::op30:
            return "op30";
        case OP_TEST::op31:
            return "op31";
        case OP_TEST::op32:
            return "op32";
        case OP_TEST::op33:
            return "op33";
        case OP_TEST::op34:
            return "op34";
        case OP_TEST::op35:
            return "op35";
        case OP_TEST::op36:
            return "op36";
        case OP_TEST::op37:
            return "op37";
        case OP_TEST::op38:
            return "op38";
        case OP_TEST::op39:
            return "op39";
        case OP_TEST::op40:
            return "op40";
        case OP_TEST::op41:
            return "op41";
        case OP_TEST::op42:
            return "op42";
        case OP_TEST::op43:
            return "op43";
        case OP_TEST::op44:
            return "op44";
        case OP_TEST::op45:
            return "op45";
        case OP_TEST::op46:
            return "op46";
        case OP_TEST::op47:
            return "op47";
        case OP_TEST::op48:
            return "op48";
        case OP_TEST::op49:
            return "op49";
        case OP_TEST::op50:
            return "op50";
        case OP_TEST::op51:
            return "op51";
        case OP_TEST::op52:
            return "op52";
        case OP_TEST::op53:
            return "op53";
        case OP_TEST::op54:
            return "op54";
        case OP_TEST::op55:
            return "op55";
        case OP_TEST::op56:
            return "op56";
        case OP_TEST::op57:
            return "op57";
        case OP_TEST::op58:
            return "op58";
        case OP_TEST::op59:
            return "op59";
        case OP_TEST::op60:
            return "op60";
        case OP_TEST::op61:
            return "op61";
        case OP_TEST::op62:
            return "op62";
        case OP_TEST::op63:
            return "op63";
        case OP_TEST::op64:
            return "op64";
        case OP_TEST::op65:
            return "op65";
        case OP_TEST::op66:
            return "op66";
        case OP_TEST::op67:
            return "op67";
        case OP_TEST::op68:
            return "op68";
        case OP_TEST::op69:
            return "op69";
        case OP_TEST::op70:
            return "op70";
        case OP_TEST::op71:
            return "op71";
        case OP_TEST::op72:
            return "op72";
        case OP_TEST::op73:
            return "op73";
        case OP_TEST::op74:
            return "op74";
        case OP_TEST::op75:
            return "op75";
        case OP_TEST::op76:
            return "op76";
        case OP_TEST::op77:
            return "op77";
        case OP_TEST::op78:
            return "op78";
        case OP_TEST::op79:
            return "op79";
        case OP_TEST::op80:
            return "op80";
        case OP_TEST::op81:
            return "op81";
        case OP_TEST::op82:
            return "op82";
        case OP_TEST::op83:
            return "op83";
        case OP_TEST::op84:
            return "op84";
        case OP_TEST::op85:
            return "op85";
        case OP_TEST::op86:
            return "op86";
        case OP_TEST::op87:
            return "op87";
        case OP_TEST::op88:
            return "op88";
        case OP_TEST::op89:
            return "op89";
        case OP_TEST::op90:
            return "op90";
        case OP_TEST::op91:
            return "op91";
        case OP_TEST::op92:
            return "op92";
        case OP_TEST::op93:
            return "op93";
        case OP_TEST::op94:
            return "op94";
        case OP_TEST::op95:
            return "op95";
        case OP_TEST::op96:
            return "op96";
        case OP_TEST::op97:
            return "op97";
        case OP_TEST::op98:
            return "op98";
        case OP_TEST::op99:
            return "op99";
        case OP_TEST::op100:
            return "op100";
        default:
            return "no_op";
    }
}

// convert_kvs_definitions call. Convert KVS differentiation definitions from string to long and
// vice-versa.
long RulesFileParser::convert_kvs_definitions (const std::string& kvs_definitions)
{
    switch (shio::hash (kvs_definitions.data ())) {
        case "put"_:
            return static_cast<long> (KVS::put);
        case "get"_:
            return static_cast<long> (KVS::get);
        case "new_iterator"_:
            return static_cast<long> (KVS::new_iterator);
        case "delete"_:
            return static_cast<long> (KVS::delete_);
        case "write"_:
            return static_cast<long> (KVS::write);
        case "get_snapshot"_:
            return static_cast<long> (KVS::get_snapshot);
        case "get_property"_:
            return static_cast<long> (KVS::get_property);
        case "get_approximate_size"_:
            return static_cast<long> (KVS::get_approximate_size);
        case "compact_range"_:
            return static_cast<long> (KVS::compact_range);
        default:
            return static_cast<long> (KVS::no_op);
    }
}

std::string RulesFileParser::convert_kvs_definitions (const KVS& kvs_definitions)
{
    switch (kvs_definitions) {
        case KVS::put:
            return "put";
        case KVS::get:
            return "get";
        case KVS::new_iterator:
            return "new_iterator";
        case KVS::delete_:
            return "delete_";
        case KVS::write:
            return "write";
        case KVS::get_snapshot:
            return "get_snapshot";
        case KVS::get_property:
            return "get_property";
        case KVS::get_approximate_size:
            return "get_approximate_size";
        case KVS::compact_range:
            return "compact_range";
        default:
            return "no_op";
    }
}

// get_create_channel_rules call. Convert string-based rules of create_channel type to the
// respective HousekeepingRule.
int RulesFileParser::get_create_channel_rules (std::vector<HousekeepingCreateChannelRaw>& hsk_rules,
    int total_rules)
{
    int rules_passed = 0;
    if (total_rules == -1) {
        total_rules = std::numeric_limits<int>::max ();
    }

    for (auto& staged_rule : this->m_staged_rules) {
        // convert staged rule element to HousekeepingOperation type
        HousekeepingOperation operation = this->convert_housekeeping_operation (staged_rule[1]);

        // validate if current operation is of type 'create_channel'
        if (operation == HousekeepingOperation::create_channel) {
            // verify if total of elements in staged rule are complete
            if (staged_rule.size () < this->m_create_channel_rules_min_elements) {
                Logging::log_error ("RulesFileParser: Error while reading staged rule and creating "
                                    "HousekeepingRule object (missing elements)");
            } else {
                // emplace the enum ContextType of which the operation definitions respect to, and
                // the respective classifiers, namely workflow-id (staged_rule[4]), operation type
                // (staged_rule[5]), and operation context (staged_rule[6])
                HousekeepingCreateChannelRaw channel_rule {};
                channel_rule.m_rule_id = std::stoll (staged_rule[0]);
                channel_rule.m_rule_type = static_cast<int> (HousekeepingOperation::create_channel);
                channel_rule.m_channel_id = std::stol (staged_rule[2]);
                channel_rule.m_context_definition
                    = this->convert_context_type_definition (staged_rule[3]);
                channel_rule.m_workflow_id = std::stol (staged_rule[4]);
                channel_rule.m_operation_type
                    = this->convert_differentiation_definitions (staged_rule[3], staged_rule[5]);
                channel_rule.m_operation_context
                    = this->convert_differentiation_definitions (staged_rule[3], staged_rule[6]);

                hsk_rules.push_back (channel_rule);
                rules_passed++;

                if (rules_passed == total_rules) {
                    break;
                }
            }
        }
    }

    return rules_passed;
}

// get_create_object_rules call. Convert string-based rules of create_object type to the respective
// HousekeepingRule.
int RulesFileParser::get_create_object_rules (std::vector<HousekeepingCreateObjectRaw>& hsk_rules,
    int total_rules)
{
    int rules_passed = 0;
    if (total_rules == -1) {
        total_rules = std::numeric_limits<int>::max ();
    }

    for (auto& staged_rule : this->m_staged_rules) {
        // convert staged rule element to HousekeepingOperation type
        HousekeepingOperation operation = this->convert_housekeeping_operation (staged_rule[1]);

        // validate if current operation is of type 'create_object'
        if (operation == HousekeepingOperation::create_object) {
            // verify if total of elements in staged rule are complete
            if (staged_rule.size () < this->m_create_object_rules_min_elements) {
                Logging::log_error ("RulesFileParser: Error while reading staged rule and creating "
                                    "HousekeepingRule object (missing elements)");
            } else {
                HousekeepingCreateObjectRaw object_rule {};
                object_rule.m_rule_id = std::stoll (staged_rule[0]);
                object_rule.m_rule_type = static_cast<int> (HousekeepingOperation::create_object);
                object_rule.m_channel_id = std::stol (staged_rule[2]);
                object_rule.m_enforcement_object_id = std::stol (staged_rule[3]);
                object_rule.m_context_definition
                    = this->convert_context_type_definition (staged_rule[4]);
                object_rule.m_operation_type
                    = this->convert_differentiation_definitions (staged_rule[4], staged_rule[5]);
                object_rule.m_operation_context
                    = this->convert_differentiation_definitions (staged_rule[4], staged_rule[6]);
                object_rule.m_enforcement_object_type
                    = static_cast<long> (this->convert_object_type (staged_rule[7]));
                object_rule.m_property_first = std::stol (staged_rule[8]);
                object_rule.m_property_second = std::stol (staged_rule[9]);

                hsk_rules.push_back (object_rule);
                rules_passed++;

                if (rules_passed == total_rules) {
                    break;
                }
            }
        }
    }

    return rules_passed;
}

// erase_rules call. Remove all rules from the m_staged_rules container.
int RulesFileParser::erase_rules ()
{
    int removed_rules = 0;
    for (auto iterator = this->m_staged_rules.begin (); iterator != this->m_staged_rules.end ();) {
        this->m_staged_rules.erase (iterator);
        removed_rules++;
    }

    // return total of rules removed
    return removed_rules;
}

// print_rules call. Write to stdout all staged rules in the m_staged_rules container.
void RulesFileParser::print_rules () const
{
    for (const auto& m_staged_rule : this->m_staged_rules) {
        for (const auto& parameter : m_staged_rule) {
            std::cout << parameter << " ";
        }
        std::cout << "\n";
    }
}

} // namespace shio
