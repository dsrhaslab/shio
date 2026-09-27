//
//

// Server side C/C++ program to demonstrate Socket programming
#include "utils/interface_definitions.hpp"

#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
// #include "../../include/utils/interface_definitions.hpp"
#include "string.h"

#include <algorithm>
#include <fstream> // std::ifstream
#include <inttypes.h>
#include <iostream> // std::cout
#include <iostream>
#define SCALABILITY_ROUNDS 10000

// const char* option_socket_name_tf_1_ = "/tmp/paiotensorflow01.socket";
// const char* option_socket_name_tf_1_client = "/tmp/paiotensorflow01_client.socket";

void collect_stats (int sockfd);
void collect_global_stats (int sockfd);
void collect_entity_stats (int sockfd);
void create_enforcement_rule (int sockfd);
void housekeeping_rule_channel (int sockfd);
void housekeeping_rule_object (int sockfd);
void mark_stage_ready (int sockfd);

int collect_ok = 0;

int n_channels = 0;

int rounds_scalability = SCALABILITY_ROUNDS;

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

void mark_stage_ready (int sockfd)
{
    StageReadyRaw object = {};

    int n = ::read (sockfd, &object, sizeof (struct StageReadyRaw));

    printf ("Stage Ready\n");

    ACK ack = {};
    ack.m_message = 1;

    n = ::write (sockfd, &ack, sizeof (struct ACK));
}

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

void collect_global_stats (int sockfd)
{
    // printf ("DataPlaneStage  collect_global_stats\n");

    StatsDataMetadataRaw object = {};

    object.m_total_data_rate = 150000000;
    object.m_total_metadata_rate = 10000000;

    int n = ::write (sockfd, &object, sizeof (struct StatsDataMetadataRaw));
}

char* operation_list[] = { "op1",
    "op2",
    "op3",
    "op4",
    "op5",
    "op6",
    "op7",
    "op8",
    "op9",
    "op10",
    "op11",
    "op12",
    "op13",
    "op14",
    "op15",
    "op16",
    "op17",
    "op18",
    "op19",
    "op20",
    "op21",
    "op22",
    "op23",
    "op24",
    "op25",
    "op26",
    "op27",
    "op28",
    "op29",
    "op30",
    "op31",
    "op32",
    "op33",
    "op34",
    "op35",
    "op36",
    "op37",
    "op38",
    "op39",
    "op40",
    "op41",
    "op42",
    "op43",
    "op44",
    "op45",
    "op46",
    "op47",
    "op48",
    "op49",
    "op50",
    "op51",
    "op52",
    "op53",
    "op54",
    "op55",
    "op56",
    "op57",
    "op58",
    "op59",
    "op60",
    "op61",
    "op62",
    "op63",
    "op64",
    "op65",
    "op66",
    "op67",
    "op68",
    "op69",
    "op70",
    "op71",
    "op72",
    "op73",
    "op74",
    "op75",
    "op76",
    "op77",
    "op78",
    "op79",
    "op80",
    "op81",
    "op82",
    "op83",
    "op84",
    "op85",
    "op86",
    "op87",
    "op88",
    "op89",
    "op90",
    "op91",
    "op92",
    "op93",
    "op94",
    "op95",
    "op96",
    "op97",
    "op98",
    "op99",
    "op100" };

void collect_global_all_stats (int sockfd)
{
    //printf ("DataPlaneStage  collect_global_stats: %d\n", op_number);

    StatsDataGlobal object = {};

    for (int i = 0; i < op_number; i++) {
        //printf ("%s, %d\n", operation_list[i], i + 100);
        strcpy (object.op_name[i], operation_list[i]);
        object.op_rate[i] = i + 100;
    }

    int n = ::write (sockfd, &object, sizeof (struct StatsDataGlobal));
}

const int m_nr_entities = 2;

void DeployDataPlaneStage (char* stage_name,
    char* stage_env,
    char* stage_user,
    int m_pid,
    int m_ppid,
    char* socket_name)
{
    int sockfd, n;
    struct sockaddr_un serv_addr;

    char buffer[256];

    sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf ("DeployDataPlaneStage: Error in Opening Socket 1!\n");
    }

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

    const char* option_socket_name = socket_name;

    bzero ((char*)&serv_addr, sizeof (serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy (serv_addr.sun_path, option_socket_name, sizeof (serv_addr.sun_path) - 1);

    if (connect (sockfd, (struct sockaddr*)&serv_addr, sizeof (serv_addr)) < 0) {
        printf ("DeployDataPlaneStage: Error in Connection:%s \n", socket_name);
        sleep(1);
        if (connect (sockfd, (struct sockaddr*)&serv_addr, sizeof (serv_addr)) < 0) {
            printf ("DeployDataPlaneStage: Error in Connection:%s \n", socket_name);
            sleep(1);
            if (connect (sockfd, (struct sockaddr*)&serv_addr, sizeof (serv_addr)) < 0) {
                printf ("DeployDataPlaneStage: Error in Connection:%s \n", socket_name);
                return;
            }
        }
    }

    bzero (buffer, 256);

    ControlOperation operation = {};

    n = ::read (sockfd, &operation, sizeof (struct ControlOperation));

    // printf(
    //     "DeployDataPlaneStage: Here is message 1: %d\n",operation.m_operation_type);

    n = ::write (sockfd, &object, sizeof (struct StageSimplifiedHandshakeRaw));

    if (n < 0) {
        printf ("DeployDataPlaneStage: Error writing to Socket!\n");
    }

    StageHandshakeRaw handshake_object = {};

    n = ::read (sockfd, &handshake_object, sizeof (struct StageHandshakeRaw));

    // printf(
    //     "DeployDataPlaneStage: Here is message 2: %s\n", handshake_object.m_address);

    const char* option_socket_name2 = handshake_object.m_address;

    close (sockfd);
    sockfd = socket (AF_UNIX, SOCK_STREAM, 0);

    bzero ((char*)&serv_addr, sizeof (serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy (serv_addr.sun_path, option_socket_name2, sizeof (serv_addr.sun_path) - 1);

    if (connect (sockfd, (struct sockaddr*)&serv_addr, sizeof (serv_addr)) < 0) {
        printf ("DeployDataPlaneStage: Error in Connection 2!\n");
    } else {
        printf ("DeployDataPlaneStage: Connection Successfulxx!\n");
    }

    while (rounds_scalability > 0) {

        ControlOperation operation1 = {};

        n = ::read (sockfd, &operation1, sizeof (struct ControlOperation));

        if (operation1.m_operation_type == 4) {
            if (operation1.m_operation_subtype == 1) {
                housekeeping_rule_channel (sockfd);
            } else if (operation1.m_operation_subtype == 2) {
                housekeeping_rule_object (sockfd);
            }
            //   printf("DeployDataPlaneStage: Housekeeping Rules Successful!\n");
        }

        else if (operation1.m_operation_type == 1) {
            mark_stage_ready (sockfd);
            //   printf("DeployDataPlaneStage: Mark stage ready!\n");
        } else if (operation1.m_operation_type == 3) {
            // printf("DeployDataPlaneStage: Collected Statistics!\n");
            if (operation1.m_operation_subtype == 6) {
                collect_global_stats (sockfd);
                collect_ok = 1;
            } else if (operation1.m_operation_subtype == 8) {
                collect_global_all_stats (sockfd);
                collect_ok = 1;
            }
        } else if (operation1.m_operation_type == 6) {
            // printf("DeployDataPlaneStage: Create Enforcement Rule!\n");
            rounds_scalability--;
            create_enforcement_rule (sockfd);
        }
    }
    printf ("Destroy data plane stage!\n");
}

int main (int argc, char* argv[])
{
    DeployDataPlaneStage (argv[1], argv[2], argv[3], std::stoi (argv[2]), 101, argv[4]);
}
