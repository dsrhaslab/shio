#!/bin/bash

#jobs_config_file
#exemplo: 10000_G0_C10000
JOBS_CONFIG_PATH=$2


EXP=${JOBS_CONFIG_PATH}_deallocated_with_50_per_node_run$1
#total_cluster_physical_nodes
CLUSTER_NODES=1
#total_local_physical_nodes
LOCAL_NODES=50
#total_nodes_used_to_bootstrap_system (1 main and x auxiliary for local controllers)
BOOTSTRAP_NODES=2
#total_data_plane_stages_per_physical_node
PER_NODE_JOBS=50
#total_leaf_physical_nodes_per_local
PER_CLUSTER_NODES=50
#total_data_plane_stages
TOTAL_STAGES=2500
#total_rounds
ROUNDS=3000
#max_time
TIME=3000
#max_ops
OPS=2

#path to choose exec
EXEC_PATH=${ROUNDS}_${TIME}_sleep1
DATA_PLANE_EXEC_PATH=${OPS}_${ROUNDS}

#path to test launch
TEST_PATH=op${OPS}_c${CLUSTER_NODES}_l${LOCAL_NODES}_t${TOTAL_STAGES}
TEST_VERSION=${TEST_PATH}_exp${EXP}

./test_distributed_multi_deallocate_with_jobs_config_file.sh ${EXP} "concat_benchmark" ${CLUSTER_NODES} ${LOCAL_NODES} ${BOOTSTRAP_NODES} ${PER_CLUSTER_NODES} ${PER_NODE_JOBS} ${EXEC_PATH} ${DATA_PLANE_EXEC_PATH} ${TEST_PATH} ${TEST_VERSION} ${JOBS_CONFIG_PATH} &


sleep 10


export LC_NUMERIC="en_US.UTF-8"

module load intel
module load remora

export REMORA_TMPDIR=/tmp
export REMORA_PERIOD=1

ibrun -n 1 -o ${BOOTSTRAP_NODES} wait_process_exists.sh

remora ibrun -n 1 -o ${BOOTSTRAP_NODES} check_process_exists.sh 

sleep 10


rm -fr  outputs_sds/${LOCAL_NODES}_nodes_${PER_NODE}_stages_per_node_${PER_LOCAL}_per_local_${CLUSTER_NODES}_locals_${TOTAL_STAGES}_rounds_${ROUNDS}_run_${EXP}_op2_parallel_compute
mkdir outputs_sds/${LOCAL_NODES}_nodes_${PER_NODE}_stages_per_node_${PER_LOCAL}_per_local_${CLUSTER_NODES}_locals_${TOTAL_STAGES}_rounds_${ROUNDS}_run_${EXP}_op2_parallel_compute
cp ${SCRATCH}/test_scalability_${TEST_VERSION}/output/* outputs_sds/${LOCAL_NODES}_nodes_${PER_NODE}_stages_per_node_${PER_LOCAL}_per_local_${CLUSTER_NODES}_locals_${TOTAL_STAGES}_rounds_${ROUNDS}_run_${EXP}_op2_parallel_compute

rm -fr  outputs_remora/${LOCAL_NODES}_nodes_${PER_NODE}_stages_per_node_${PER_LOCAL}_per_local_${CLUSTER_NODES}_locals_${TOTAL_STAGES}_rounds_${ROUNDS}_run_${EXP}_op2_parallel_compute
mkdir outputs_remora/${LOCAL_NODES}_nodes_${PER_NODE}_stages_per_node_${PER_LOCAL}_per_local_${CLUSTER_NODES}_locals_${TOTAL_STAGES}_rounds_${ROUNDS}_run_${EXP}_op2_parallel_compute
mv remora_${SLURM_JOBID} outputs_remora/${LOCAL_NODES}_nodes_${PER_NODE}_stages_per_node_${PER_LOCAL}_per_local_${CLUSTER_NODES}_locals_${TOTAL_STAGES}_rounds_${ROUNDS}_run_${EXP}_op2_parallel_compute

sleep 10
