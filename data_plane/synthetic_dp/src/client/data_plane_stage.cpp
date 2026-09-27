/**
 *   Copyright (c) 2026 INESC TEC.
 **/

/**
 * Synthetic data plane stage.
 * Emulates a PAIO-like data plane stage that registers itself with the control plane and answers
 * its control operations (housekeeping rules, enforcement rules, statistics collection) without
 * performing any real I/O. It is meant to evaluate the control plane at scale.
 *
 * Protocol overview:
 *  1. connect to the control plane's handshake socket and exchange the stage identification
 *  (StageSimplifiedHandshakeRaw) for the address of a dedicated socket (StageHandshakeRaw);
 *  2. reconnect to that dedicated socket;
 *  3. loop reading ControlOperation headers and dispatching them to the respective handler, until
 *  rounds_scalability enforcement rules have been received.
 */

#include "utils/interface_definitions.hpp"

#include <inttypes.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <string>

/**
 * Number of enforcement rules the stage expects to receive per registered channel before it
 * terminates.
 */
#define SCALABILITY_ROUNDS 10000

/**
 * Number of connection attempts to the control plane's handshake socket (1 second apart).
 */
#define CONNECTION_ATTEMPTS 3

/**
 * Operation subtype used by the control plane to request per-operation statistics
 * (see collect_global_all_stats). It has no counterpart in interface_definitions.hpp.
 */
#define COLLECT_GLOBAL_ALL_STATS 8

// Set to 1 once the control plane has collected statistics at least once.
int collect_ok = 0;

// Number of channels created through housekeeping rules.
int n_channels = 0;

// Remaining enforcement rules to be received before the stage terminates.
int rounds_scalability = SCALABILITY_ROUNDS;

/**
 * housekeeping_rule_channel: reads a HousekeepingCreateChannelRaw rule from the control plane and
 * acknowledges it. Each new channel increases the number of enforcement rules the stage waits for
 * (SCALABILITY_ROUNDS per channel).
 * @param sockfd Socket connected to the control plane.
 */
void housekeeping_rule_channel (int sockfd)
{
    HousekeepingCreateChannelRaw object = {};

    int n = ::read (sockfd, &object, sizeof (struct HousekeepingCreateChannelRaw));

    /*printf ("Housekeeping Rule Channel: channel_%ld:operation_type_%" PRIu32 "\n",
        object.m_channel_id,
        object.m_operation_context);*/

    n_channels++;
    rounds_scalability = SCALABILITY_ROUNDS * n_channels;

    ACK ack = {};
    ack.m_message = 1;

    n = ::write (sockfd, &ack, sizeof (struct ACK));
}

/**
 * housekeeping_rule_object: reads a HousekeepingCreateObjectRaw rule from the control plane, logs
 * it, and acknowledges it.
 * @param sockfd Socket connected to the control plane.
 */
void housekeeping_rule_object (int sockfd)
{
    HousekeepingCreateObjectRaw object = {};

    int n = ::read (sockfd, &object, sizeof (struct HousekeepingCreateObjectRaw));

    printf ("Housekeeping Rule Object: channel_%ld:enforcement_object_%ld:operation_type_%" PRIu32
            ": %ld\n",
        object.m_channel_id,
        object.m_enforcement_object_id,
        object.m_operation_context,
        object.m_property_first);

    ACK ack = {};
    ack.m_message = 1;

    n = ::write (sockfd, &ack, sizeof (struct ACK));
}

/**
 * mark_stage_ready: reads a StageReadyRaw message from the control plane and acknowledges it.
 * @param sockfd Socket connected to the control plane.
 */
void mark_stage_ready (int sockfd)
{
    StageReadyRaw object = {};

    int n = ::read (sockfd, &object, sizeof (struct StageReadyRaw));

    printf ("Stage Ready\n");

    ACK ack = {};
    ack.m_message = 1;

    n = ::write (sockfd, &ack, sizeof (struct ACK));
}

/**
 * create_enforcement_rule: reads an EnforcementRuleRaw from the control plane and acknowledges it.
 * The rule is not applied, since the stage does not serve any I/O.
 * @param sockfd Socket connected to the control plane.
 */
void create_enforcement_rule (int sockfd)
{
    EnforcementRuleRaw object = {};

    int n = ::read (sockfd, &object, sizeof (struct EnforcementRuleRaw));

    /*printf ("Enforcement Rule: channel_%ld:enforcement_object_%ld : %ld\n",
        object.m_channel_id,
        object.m_enforcement_object_id,
        object.m_property_first);*/

    ACK ack = {};
    ack.m_message = 1;

    n = ::write (sockfd, &ack, sizeof (struct ACK));
}

/**
 * collect_global_stats: replies to a data/metadata statistics request with fixed, synthetic rates
 * (150 MB/s of data and 10 MB/s of metadata).
 * @param sockfd Socket connected to the control plane.
 */
void collect_global_stats (int sockfd)
{
    StatsDataMetadataRaw object = {};

    object.m_total_data_rate = 150000000;
    object.m_total_metadata_rate = 10000000;

    int n = ::write (sockfd, &object, sizeof (struct StatsDataMetadataRaw));
}

/**
 * Synthetic operation names reported by collect_global_all_stats. Only the first op_number entries
 * are used.
 */
const char* operation_list[] = { "op1", "op2", "op3", "op4", "op5", "op6", "op7", "op8", "op9",
    "op10", "op11", "op12", "op13", "op14", "op15", "op16", "op17", "op18", "op19", "op20", "op21",
    "op22", "op23", "op24", "op25", "op26", "op27", "op28", "op29", "op30", "op31", "op32", "op33",
    "op34", "op35", "op36", "op37", "op38", "op39", "op40", "op41", "op42", "op43", "op44", "op45",
    "op46", "op47", "op48", "op49", "op50", "op51", "op52", "op53", "op54", "op55", "op56", "op57",
    "op58", "op59", "op60", "op61", "op62", "op63", "op64", "op65", "op66", "op67", "op68", "op69",
    "op70", "op71", "op72", "op73", "op74", "op75", "op76", "op77", "op78", "op79", "op80", "op81",
    "op82", "op83", "op84", "op85", "op86", "op87", "op88", "op89", "op90", "op91", "op92", "op93",
    "op94", "op95", "op96", "op97", "op98", "op99", "op100" };

/**
 * collect_global_all_stats: replies to a per-operation statistics request with op_number synthetic
 * entries, where operation i is named operation_list[i] and has a rate of i + 100.
 * @param sockfd Socket connected to the control plane.
 */
void collect_global_all_stats (int sockfd)
{
    StatsDataGlobal object = {};

    for (int i = 0; i < op_number; i++) {
        strcpy (object.op_name[i], operation_list[i]);
        object.op_rate[i] = i + 100;
    }

    int n = ::write (sockfd, &object, sizeof (struct StatsDataGlobal));
}

/**
 * DeployDataPlaneStage: registers the stage with the control plane and serves its control
 * operations until rounds_scalability enforcement rules have been received.
 * @param stage_name Stage name; ':' and '.' characters are stripped before it is sent.
 * @param stage_env Value of the stage's environment variable.
 * @param stage_user User that submitted the application.
 * @param m_pid Pid reported to the control plane.
 * @param m_ppid Parent pid reported to the control plane.
 * @param socket_name Path of the control plane's UNIX handshake socket.
 */
void DeployDataPlaneStage (char* stage_name,
    char* stage_env,
    char* stage_user,
    int m_pid,
    int m_ppid,
    char* socket_name)
{
    int sockfd, n;
    struct sockaddr_un serv_addr;

    sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf ("DeployDataPlaneStage: Error in Opening Socket 1!\n");
    }

    // Build the stage identification sent during the handshake.
    StageSimplifiedHandshakeRaw object = {};

    std::string s_stage_name = stage_name;
    s_stage_name.erase (std::remove (s_stage_name.begin (), s_stage_name.end (), ':'),
        s_stage_name.end ());
    s_stage_name.erase (std::remove (s_stage_name.begin (), s_stage_name.end (), '.'),
        s_stage_name.end ());

    strcpy (object.m_stage_name, s_stage_name.c_str ());
    strcpy (object.m_stage_env, stage_env);
    strcpy (object.m_stage_user, stage_user);

    object.m_pid = m_pid;
    object.m_ppid = m_ppid;

    // Connect to the control plane's handshake socket.
    bzero ((char*)&serv_addr, sizeof (serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy (serv_addr.sun_path, socket_name, sizeof (serv_addr.sun_path) - 1);

    for (int attempt = 1;
         connect (sockfd, (struct sockaddr*)&serv_addr, sizeof (serv_addr)) < 0;
         attempt++) {
        printf ("DeployDataPlaneStage: Error in Connection:%s \n", socket_name);
        if (attempt == CONNECTION_ATTEMPTS) {
            return;
        }
        sleep (1);
    }

    // Handshake: receive the handshake operation, send the stage identification, and receive the
    // address of the dedicated socket for this stage.
    ControlOperation operation = {};
    n = ::read (sockfd, &operation, sizeof (struct ControlOperation));

    n = ::write (sockfd, &object, sizeof (struct StageSimplifiedHandshakeRaw));
    if (n < 0) {
        printf ("DeployDataPlaneStage: Error writing to Socket!\n");
    }

    StageHandshakeRaw handshake_object = {};
    n = ::read (sockfd, &handshake_object, sizeof (struct StageHandshakeRaw));

    // Reconnect to the dedicated socket.
    close (sockfd);
    sockfd = socket (AF_UNIX, SOCK_STREAM, 0);

    bzero ((char*)&serv_addr, sizeof (serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy (serv_addr.sun_path, handshake_object.m_address, sizeof (serv_addr.sun_path) - 1);

    if (connect (sockfd, (struct sockaddr*)&serv_addr, sizeof (serv_addr)) < 0) {
        printf ("DeployDataPlaneStage: Error in Connection 2!\n");
    } else {
        printf ("DeployDataPlaneStage: Connection Successfulxx!\n");
    }

    // Serve control operations; each enforcement rule consumes one round.
    while (rounds_scalability > 0) {
        ControlOperation operation1 = {};

        n = ::read (sockfd, &operation1, sizeof (struct ControlOperation));

        switch (operation1.m_operation_type) {
            case CREATE_HSK_RULE:
                if (operation1.m_operation_subtype == HSK_CREATE_CHANNEL) {
                    housekeeping_rule_channel (sockfd);
                } else if (operation1.m_operation_subtype == HSK_CREATE_OBJECT) {
                    housekeeping_rule_object (sockfd);
                }
                break;

            case STAGE_READY:
                mark_stage_ready (sockfd);
                break;

            case COLLECT_DETAILED_STATS:
                if (operation1.m_operation_subtype == COLLECT_DATA_METADATA_STATS) {
                    collect_global_stats (sockfd);
                    collect_ok = 1;
                } else if (operation1.m_operation_subtype == COLLECT_GLOBAL_ALL_STATS) {
                    collect_global_all_stats (sockfd);
                    collect_ok = 1;
                }
                break;

            case CREATE_ENF_RULE:
                rounds_scalability--;
                create_enforcement_rule (sockfd);
                break;
        }
    }

    printf ("Destroy data plane stage!\n");
}

/**
 * Usage: data_plane_stage <stage_name> <stage_env> <stage_user> <socket_name>
 * The handshake socket used is /tmp/<socket_name>.socket. The stage_env argument is also parsed as
 * an integer and reported as the stage's pid; the ppid is fixed to 101.
 */
int main (int argc, char* argv[])
{
    std::string socket_path = "/tmp/" + std::string (argv[4]) + ".socket";

    DeployDataPlaneStage (argv[1],
        argv[2],
        argv[3],
        std::stoi (argv[2]),
        101,
        const_cast<char*> (socket_path.c_str ()));
}
