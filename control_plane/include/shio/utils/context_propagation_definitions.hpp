/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef PAIO_CONTEXT_PROPAGATION_DEFINITIONS_HPP
#define PAIO_CONTEXT_PROPAGATION_DEFINITIONS_HPP

/**
 * TODO:
 *  - currently, operation type and operation context can be used interchangeably; consider having
 *  different set of I/O classifiers for the operation context and operation type. With this change,
 *  we will need on the statistics class to collect statistics based on operation type, operation
 *  context, or both. Also, we will need to change the RulesParser and SouthboundConnectionHandler
 *  classes.
 */
namespace shio {

/**
 * ContextType: Defines the available operation context classifiers.
 */
enum class ContextType {
    PAIO_GENERAL = 0,
    POSIX = 1,
    POSIX_META = 2,
    LSM_KVS_SIMPLE = 3,
    LSM_KVS_DETAILED = 4,
    KVS = 5,
    OP_TEST = 6
};

// ------------------------------------------------------------------------------------

/**
 * GENERAL definitions.
 * This can be used as generic definitions for I/O requests to be classified.
 * Currently, it considers the main context of the operation (foreground or background) and its
 * priority (high or low). This can be later adjusted as the number of use cases increases.
 */
enum class PAIO_GENERAL {
    foreground = 1,
    background = 2,
    high_priority = 3,
    low_priority = 4,
    no_op = 0
};

const int paio_general_size = 5;

// ------------------------------------------------------------------------------------

/**
 * LSM_KVS_DETAILED Definitions.
 * Defines the context of LSM KVS operations.
 * This is a "detailed" version of the context of POSIX operations submitted from the LSM key-value
 * store, since it decouples compactions in different levels (namely, compactions from L0->L0,
 * L0->L1, L1->L2, L2->L3, and LN).
 * This is useful when intercepting and controlling the I/O operations of LSM-based key-values
 * stores, such as RocksDB, LevelDB, PebblesDB, ...
 */
enum class LSM_KVS_DETAILED {
    bg_flush = 1,
    bg_compaction = 2,
    bg_compaction_L0_L0 = 3,
    bg_compaction_L0_L1 = 4,
    bg_compaction_L1_L2 = 5,
    bg_compaction_L2_L3 = 6,
    bg_compaction_LN = 7,
    foreground = 8,
    no_op = 0
};

const int lsm_kvs_detailed_size = 9;

// ------------------------------------------------------------------------------------

/**
 * LSM_KVS_SIMPLE Definitions.
 * Defines the context of LSM KVS operations.
 * This is a "simplified" version of the context of POSIX operations submitted from the LSM
 * key-value store, since it aggregates compactions by priority. For instance, high-priority
 * compactions (i.e., compactions that directly impact client tail latency) are L0->L0 and L0->L1.
 * Low-priority compactions include L1->L2, L2->L3, and LN.
 * This is useful when intercepting and controlling the I/O operations of LSM-based key-values
 * stores, such as RocksDB, LevelDB, PebblesDB, ...
 */
enum class LSM_KVS_SIMPLE {
    bg_flush = 1,
    bg_compaction_high_priority = 2,
    bg_compaction_low_priority = 3,
    foreground = 4,
    background = 5,
    no_op = 0
};

const int lsm_kvs_simple_size = 6;

// ------------------------------------------------------------------------------------

/**
 * POSIX definitions.
 * Defines the operation type of POSIX applications. data, metadata, and total aggregate
 * operations by class.
 */
enum class POSIX {
    read = 1,
    write = 2,
    pread = 3,
    pwrite = 4,
    pread64 = 5,
    pwrite64 = 6,
    fread = 7,
    fwrite = 8,
    open = 9,
    open64 = 10,
    creat = 11,
    creat64 = 12,
    openat = 13,
    close = 14,
    fsync = 15,
    fdatasync = 16,
    sync = 17,
    syncfs = 18,
    truncate = 19,
    truncate64 = 20,
    ftruncate = 21,
    ftruncate64 = 22,
    xstat = 23,
    xstat64 = 24,
    lxstat = 25,
    lxstat64 = 26,
    fxstat = 27,
    fxstat64 = 28,
    fxstatat = 29,
    fxstatat64 = 30,
    statfs = 31,
    statfs64 = 32,
    fstatfs = 33,
    fstatfs64 = 34,
    link = 35,
    linkat = 36,
    unlink = 37,
    unlinkat = 38,
    rename = 39,
    renameat = 40,
    symlink = 41,
    symlinkat = 42,
    readlink = 43,
    readlinkat = 44,
    fopen = 45,
    fopen64 = 46,
    fdopen = 47,
    freopen = 48,
    freopen64 = 49,
    fclose = 50,
    fflush = 51,
    access = 52,
    faccessat = 53,
    lseek = 54,
    lseek64 = 55,
    fseek = 56,
    fseek64 = 57,
    ftell = 58,
    fseeko = 59,
    fseeko64 = 60,
    ftello = 61,
    ftello64 = 62,
    mkdir = 63,
    mkdirat = 64,
    readdir = 65,
    readdir64 = 66,
    opendir = 67,
    fdopendir = 68,
    closedir = 69,
    rmdir = 70,
    dirfd = 71,
    getxattr = 72,
    lgetxattr = 73,
    fgetxattr = 74,
    setxattr = 75,
    lsetxattr = 76,
    fsetxattr = 77,
    listxattr = 78,
    llistxattr = 79,
    flistxattr = 80,
    removexattr = 81,
    lremovexattr = 82,
    fremovexattr = 83,
    chmod = 84,
    fchmod = 85,
    fchmodat = 86,
    chown = 87,
    lchown = 88,
    fchown = 89,
    fchownat = 90,
    mknod = 91,
    mknodat = 92,
    mmap = 93,
    munmap = 94,
    data = 95,
    metadata = 96,
    total = 97,
    no_op = 0
};

const int posix_size = 100;

/**
 * POSIX_META definitions.
 * Defines the "meta" operations of POSIX applications.
 *  - foreground and background define the context of the operation;
 *  - high_priority, med_priority, and low_priority define the priority of the operation;
 *  - data_op, meta_op, dir_op, ext_attr_op, and file_mod_op define the class of the operation.
 */
enum class POSIX_META {
    foreground = 1,
    background = 2,
    high_priority = 3,
    med_priority = 4,
    low_priority = 5,
    data_op = 6,
    meta_op = 7,
    dir_op = 8,
    ext_attr_op = 9,
    file_mod_op = 10,
    op_test = 11,
    no_op = 0
};

const int posix_meta_size = 11;

/**
 * OP_TEST definitions.
 * Defines synthetic operation types (op1 to op100), used to test and benchmark the controller
 * with a large number of operation classes.
 */
enum class OP_TEST {
    op1 = 1,
    op2 = 2,
    op3 = 3,
    op4 = 4,
    op5 = 5,
    op6 = 6,
    op7 = 7,
    op8 = 8,
    op9 = 9,
    op10 = 10,
    op11 = 11,
    op12 = 12,
    op13 = 13,
    op14 = 14,
    op15 = 15,
    op16 = 16,
    op17 = 17,
    op18 = 18,
    op19 = 19,
    op20 = 20,
    op21 = 21,
    op22 = 22,
    op23 = 23,
    op24 = 24,
    op25 = 25,
    op26 = 26,
    op27 = 27,
    op28 = 28,
    op29 = 29,
    op30 = 30,
    op31 = 31,
    op32 = 32,
    op33 = 33,
    op34 = 34,
    op35 = 35,
    op36 = 36,
    op37 = 37,
    op38 = 38,
    op39 = 39,
    op40 = 40,
    op41 = 41,
    op42 = 42,
    op43 = 43,
    op44 = 44,
    op45 = 45,
    op46 = 46,
    op47 = 47,
    op48 = 48,
    op49 = 49,
    op50 = 50,
    op51 = 51,
    op52 = 52,
    op53 = 53,
    op54 = 54,
    op55 = 55,
    op56 = 56,
    op57 = 57,
    op58 = 58,
    op59 = 59,
    op60 = 60,
    op61 = 61,
    op62 = 62,
    op63 = 63,
    op64 = 64,
    op65 = 65,
    op66 = 66,
    op67 = 67,
    op68 = 68,
    op69 = 69,
    op70 = 70,
    op71 = 71,
    op72 = 72,
    op73 = 73,
    op74 = 74,
    op75 = 75,
    op76 = 76,
    op77 = 77,
    op78 = 78,
    op79 = 79,
    op80 = 80,
    op81 = 81,
    op82 = 82,
    op83 = 83,
    op84 = 84,
    op85 = 85,
    op86 = 86,
    op87 = 87,
    op88 = 88,
    op89 = 89,
    op90 = 90,
    op91 = 91,
    op92 = 92,
    op93 = 93,
    op94 = 94,
    op95 = 95,
    op96 = 96,
    op97 = 97,
    op98 = 98,
    op99 = 99,
    op100 = 100
};

const int op_test_size = 100;

// ------------------------------------------------------------------------------------

/**
 * KVS definitions.
 * Defines the operation type of LSM-based key-value stores, like LevelDB, RocksDB, and PebblesDB.
 */
enum class KVS {
    put = 1,
    get = 2,
    new_iterator = 3,
    delete_ = 4,
    write = 5,
    get_snapshot = 6,
    get_property = 7,
    get_approximate_size = 8,
    compact_range = 9,
    no_op = 0
};

const int kvs_size = 10;
// ------------------------------------------------------------------------------------

} // namespace shio

#endif // PAIO_CONTEXT_PROPAGATION_DEFINITIONS_HPP
