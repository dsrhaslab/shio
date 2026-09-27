#!/bin/bash


echo "module load gcc/9.1.0  python3/3.8.2 boost/1.72"
module load gcc/9.1.0
module load python3/3.8.2
module load boost/1.72

echo "Running local controller"

cd cheferd/

export CC=`which gcc`
export CXX=`which g++`

killall -9 cheferd_exec

rm -fr /tmp/*520*.socket

# local_job_single.sh ${PER_NODE} ${node_version}> output/local_output_app${app}.out &

PER_NODE_JOBS=${1}
CLUSTER_NODE_ID=${2}
#CLUSTER_NODE_ID=3
PHYSICAL_NODE_INDEX=${3}

LOCAL_ADDR=$(grep "$(hostname -s | sed 's/^c/i/')" /etc/hosts | awk '{print $1}')

for i in $(seq 1 ${PER_NODE_JOBS})
do
    cp ../files/local_config_file_${CLUSTER_NODE_ID} /tmp/local_config_file:520${i}
    echo "own_upper_address: ${LOCAL_ADDR}:520${i}" >> /tmp/local_config_file:520${i}
done



#export GRPC_VERBOSITY=DEBUG
#export GRPC_TRACE=call_error,connectivity_state,pick_first,round_robin,glb

for i in $(seq 1 ${PER_NODE_JOBS})
do
    ./cheferd_exec --config_file /tmp/local_config_file:520${i} &
done


sleep 30

cd ..

# from 1..50s
cluster_id=${CLUSTER_NODE_ID}
local_id_within_cluster=$(( (${PHYSICAL_NODE_INDEX} - 1) % 50 + 1 ))
jobs_source_file="files/distribution_jobs_c${cluster_id}_l${local_id_within_cluster}.csv"
#jobs_source_file="files/distribution_jobs_c3_l${local_id_within_cluster}.csv"
#jobs_source_file="files/distribution_jobs_c3_l7.csv"
max_execution_time=6600
deployments_per_compute_node=${PER_NODE_JOBS}
#per_cluster_connections=${PER_CLUSTER_NODES}
python3 execute_jobs.py --filename ${jobs_source_file} --time ${max_execution_time}  \
 --deployments_per_compute_node ${deployments_per_compute_node} --node_physical_node_index ${PHYSICAL_NODE_INDEX} --verbose

wait




echo "Exiting"
