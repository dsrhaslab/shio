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
#CLUSTER_NODE_ID=3

cp ../files/cluster_config_file ../files/cluster_config_file_c${CLUSTER_NODE_ID}
echo "jobs_config_file: ../files/cluster_jobs_config_c${CLUSTER_NODE_ID}_file" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}
echo "own_upper_address: ${NODE_CLUSTER}:50052" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}
echo "own_down_address: ${NODE_CLUSTER}:50053" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}

cp ../files/local_config_file ../files/local_config_file_${CLUSTER_NODE_ID}
echo "upper_address: ${NODE_CLUSTER}:50053" >>../files/local_config_file_${CLUSTER_NODE_ID}

cp ../files/cluster_config_file_c${CLUSTER_NODE_ID} ../files/cluster_config_file_c${CLUSTER_NODE_ID}_bck
cp ../files/cluster_config_file_c${CLUSTER_NODE_ID} ../files/cluster_config_file_c${CLUSTER_NODE_ID}_bck_2

echo "role: primary" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}
echo "own_internal_address: 0.0.0.0:50061" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}
echo "twin_controller_address: 0.0.0.0:50062" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}

echo "role: secondary" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}_bck
echo "own_internal_address: 0.0.0.0:50062" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}_bck
echo "twin_controller_address: 0.0.0.0:50061" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}_bck

echo "role: secondary" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}_bck_2
echo "own_internal_address: 0.0.0.0:50061" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}_bck_2
echo "twin_controller_address: 0.0.0.0:50062" >>../files/cluster_config_file_c${CLUSTER_NODE_ID}_bck_2

#Start cluster controller
./cheferd_exec --config_file ../files/cluster_config_file_c${CLUSTER_NODE_ID} cheferd_cluster_machine1 >../output/cluster_output_node${CLUSTER_NODE_ID}_machine1.out &

sleep 10

./cheferd_exec --config_file ../files/cluster_config_file_c${CLUSTER_NODE_ID}_bck cheferd_cluster_machine2 >../output/cluster_output_node${CLUSTER_NODE_ID}_machine2.out &

wait

echo "Exiting"
