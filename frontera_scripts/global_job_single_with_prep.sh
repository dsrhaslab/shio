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

#Start global controller
./cheferd_exec --config_file ../files/global_config_file > ../output/global_output.out 

sleep 30

echo "Exiting"
