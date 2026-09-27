#!/bin/bash

echo "Launching local controllers in bootstrap node ${BOOTSTRAP_NODE}"

BOOTSTRAP_NODE=${1}
PER_BOOTSTRAP_NODE=${2}
PER_CLUSTER_NODES=${3}
PER_NODE_JOBS=${4}
BASE_NODES=${5}
TOTAL_NODES=${6}

echo "BOOTSTRAP_NODE: ${BOOTSTRAP_NODE}"
echo "per_cluster_nodes: ${PER_CLUSTER_NODES}"
echo "per_node: ${PER_NODE_JOBS}"
echo "base_nodes: ${BASE_NODES}"
echo "total_nodes: ${TOTAL_NODES}"

ADDR=$(grep "$(hostname -s | sed 's/^c/i/')" /etc/hosts | awk '{print $1}')

#Compute how many local controllers bootstrap node is responsible for launching
TOTAL_LOCALS_TO_LAUNCH=$((PER_BOOTSTRAP_NODE * PER_CLUSTER_NODES))
#Compute offset of local controllers index
OFFSET_NODES=$(((BOOTSTRAP_NODE - 1) * ${TOTAL_LOCALS_TO_LAUNCH}))

#Extracts nodes where the system is being tested
HOSTS=$(squeue --me -j $SLURM_JOBID | awk 'NR > 1 {print $8}')
echo "$HOSTS"
NODE_LIST=$(python3 split_nodelist.py $HOSTS)

for local_node in $(seq 1 ${TOTAL_LOCALS_TO_LAUNCH}); do

    echo "${ADDR} Before local launch ${BOOTSTRAP_NODE} -> ${local_node}: $(date)"

    #Define IDs for local and cluster node
    LOCAL_NODE_ID=$((local_node + OFFSET_NODES))
    CLUSTER_NODE_ID=$(((LOCAL_NODE_ID - 1) / PER_CLUSTER_NODES + 1))

    #Remove 1 because index starts at zero
    beforenodes=$((BASE_NODES + LOCAL_NODE_ID - 1))
    afternodes=$((TOTAL_NODES - beforenodes - 1))

    #Extract local node to launch
    NODE_TGT=$(echo $NODE_LIST | sed "s/\(.* \)\{$beforenodes\}\(.*\)\( .*\)\{$afternodes\}/\2/")

    #echo "local_job_single.sh with data: ${PER_NODE_JOBS} ${CLUSTER_NODE_ID} ${LOCAL_NODE_ID}"

    #Start local controller in remote node
    ssh ${NODE_TGT} -f "cd $PWD; pwd; exec bash local_job_single_trace.sh ${PER_NODE_JOBS} ${CLUSTER_NODE_ID} ${LOCAL_NODE_ID} > output/local_output_app${LOCAL_NODE_ID}.out"

    sleep 1
done

#wait
