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
LOCAL_NODE_ID=${3}

LOCAL_ADDR=$(grep "$(hostname -s | sed 's/^c/i/')" /etc/hosts | awk '{print $1}')

for i in $(seq 1 ${PER_NODE_JOBS})
do
    cp ../files/local_config_file_${CLUSTER_NODE_ID} /tmp/local_config_file:520${i}
    echo "own_upper_address: ${LOCAL_ADDR}:520${i}" >> /tmp/local_config_file:520${i}
done

cp ../files/read_25mbs_30min /tmp

#export GRPC_VERBOSITY=DEBUG
#export GRPC_TRACE=call_error,connectivity_state,pick_first,round_robin,glb

for i in $(seq 1 ${PER_NODE_JOBS})
do
    ./cheferd_exec --config_file /tmp/local_config_file:520${i} &
done


sleep 30

export CPATH="${WORK}/padll_paio/paio/include:${CPATH}"
export path_padll="${WORK}/padll_paio/padll/build"

NODE_STAGE=$(grep "$(hostname -s | sed 's/^c/i/')" /etc/hosts | awk '{print $1}')
echo "${NODE_STAGE}"


#Start data plane stages
for i in $(seq 1 ${PER_NODE_JOBS})
do
    APP_NAME=N${LOCAL_NODE_ID}V${i}
    echo ${APP_NAME}
    export paio_name=${APP_NAME}
    export paio_env=1
    export padll_workflows=2
    export paio_stage_opt=1
    export cheferd_local_address=/tmp/${NODE_STAGE}:520${i}.socket
    LD_PRELOAD=$path_padll/libpadll.so ./trace_replayer_file_cropping /tmp/read_25mbs_30min -1 1200 &
    sleep 1
done


wait


echo "Exiting local jobs ${NODE_STAGE}"
