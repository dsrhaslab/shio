#!/bin/bash


echo "module load gcc/9.1.0  python3/3.8.2 boost/1.72"
module load gcc/9.1.0
module load python3/3.8.2
module load boost/1.72

#####

echo "Running core controller"

cd cheferd/

export CC=`which gcc`
export CXX=`which g++`

killall -9 cheferd_exec

#Extract global controller IP
NODE_GLOBAL=$(grep "$(hostname -s | sed 's/^c/i/')" /etc/hosts | awk '{print $1}')
echo "${NODE_GLOBAL}"


#Prep configuration files
echo "own_down_address: ${NODE_GLOBAL}:50051" >> ../files/global_config_file
echo "upper_address: ${NODE_GLOBAL}:50051" >> ../files/cluster_config_file
cp  ../files/global_config_file  ../files/global_config_file_bck
cp  ../files/global_config_file  ../files/global_config_file_bck_2

echo "role: primary" >> ../files/global_config_file
echo "own_internal_address: 0.0.0.0:50061" >> ../files/global_config_file
echo "twin_controller_address: 0.0.0.0:50062" >> ../files/global_config_file

echo "role: secondary" >> ../files/global_config_file_bck
echo "own_internal_address: 0.0.0.0:50062" >> ../files/global_config_file_bck
echo "twin_controller_address: 0.0.0.0:50061" >> ../files/global_config_file_bck

echo "role: secondary" >> ../files/global_config_file_bck_2
echo "own_internal_address: 0.0.0.0:50061" >> ../files/global_config_file_bck_2
echo "twin_controller_address: 0.0.0.0:50062" >> ../files/global_config_file_bck_2

#Start global controller
./cheferd_exec --config_file ../files/global_config_file cheferd_global_machine1 > ../output/global_output_machine1.out &

sleep 10

./cheferd_exec --config_file ../files/global_config_file_bck cheferd_global_machine2 > ../output/global_output_machine2.out &

wait

sleep 30

echo "Exiting"
