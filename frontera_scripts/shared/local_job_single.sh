#!/bin/bash
#Alternative code to launch data_plane_stages with synthetic workloads.

echo "module load gcc/9.1.0  python3/3.8.2 boost/1.72"
module load gcc/9.1.0
module load python3/3.8.2
module load boost/1.72

module list


echo "Running local controller"

cd cheferd/

export CC=`which gcc`
export CXX=`which g++`

killall -9 cheferd_exec
killall -9 data_plane_stage

rm -fr /tmp/*520*.socket

PER_NODE_JOBS=${1}
CLUSTER_NODE_ID=${2}
LOCAL_NODE_ID=${3}

LOCAL_ADDR=$(grep "$(hostname -s | sed 's/^c/i/')" /etc/hosts | awk '{print $1}')


echo "In local_job_single: Before prep config ${CLUSTER_NODE_ID}: $(date)"

#Prep configuration files
for i in $(seq 1 ${PER_NODE_JOBS})
do
    APP_NAME=N${LOCAL_NODE_ID}V${i}
    cp ../files/local_config_file_${CLUSTER_NODE_ID} /tmp/local_config_file:520${i}
    echo "own_upper_address: ${LOCAL_ADDR}:520${i}" >> /tmp/local_config_file:520${i}
done

echo "In local_job_single: After prep config ${CLUSTER_NODE_ID}: $(date)"


#Start local controllers
for i in $(seq 1 ${PER_NODE_JOBS})
do
    ./cheferd_exec --config_file /tmp/local_config_file:520${i} &
done

sleep 30


#Start data plane stages
for i in $(seq 1 ${PER_NODE_JOBS})
do
    APP_NAME=N${LOCAL_NODE_ID}V${i}
    echo ${APP_NAME}
    ./data_plane_stage ${APP_NAME} 1 user1 /tmp/${LOCAL_ADDR}:520${i}.socket &
done

wait

rm -fr /tmp/padll-* /tmp/local_config* /tmp/*520*.socket

echo "Exiting"