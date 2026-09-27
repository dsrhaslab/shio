#!/bin/bash

echo "module load gcc/9.1.0  python3/3.8.2 boost/1.72"
module load gcc/9.1.0
module load python3/3.8.2
module load boost/1.72

echo "Running cluster controller"

cd cheferd/

export CC=$(which gcc)
export CXX=$(which g++)

killall -9 cheferd_exec

rm -fr /tmp/*520*.socket

#Prep configuration files
NODE_CLUSTER=$(grep "$(hostname -s | sed 's/^c/i/')" /etc/hosts | awk '{print $1}')
CLUSTER_NODE_ID=${1}

cp ../files/cluster_config_file ../files/cluster_config_file_${NODE_CLUSTER}
echo "jobs_config_file: ../files/cluster_jobs_config_c${CLUSTER_NODE_ID}_file" >>../files/cluster_config_file_${NODE_CLUSTER}
echo "own_upper_address: ${NODE_CLUSTER}:50052" >>../files/cluster_config_file_${NODE_CLUSTER}
echo "own_down_address: ${NODE_CLUSTER}:50053" >>../files/cluster_config_file_${NODE_CLUSTER}

cp ../files/local_config_file ../files/local_config_file_${CLUSTER_NODE_ID}
echo "upper_address: ${NODE_CLUSTER}:50053" >>../files/local_config_file_${CLUSTER_NODE_ID}

#Start cluster controller
./cheferd_exec --config_file ../files/cluster_config_file_${NODE_CLUSTER} >../output/cluster_output_app${CLUSTER_NODE_ID}.out &

wait

echo "Exiting"
