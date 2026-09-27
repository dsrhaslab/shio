#!/bin/bash

GLOBAL_INDEX=${1}
NR_CLUSTER_NODES=${2}
NR_LOCAL_NODES=${3}
START_CLUSTER_NODE_INDEX=$((GLOBAL_INDEX + 1))
CLUSTER_1=${START_CLUSTER_NODE_INDEX}
CLUSTER_2=$((START_CLUSTER_NODE_INDEX + 1))
CLUSTER_3=$((START_CLUSTER_NODE_INDEX + 2))
CLUSTER_4=$((START_CLUSTER_NODE_INDEX + 3))
START_LOCAL_NODE_INDEX=$((START_CLUSTER_NODE_INDEX + NR_CLUSTER_NODES))
END_LOCAL_NODE_INDEX=$((START_LOCAL_NODE_INDEX + NR_LOCAL_NODES - 1))

sleep 1100

echo "FAILING GLOBAL CONTROLLER"

#Kill cluster 1
#ibrun -n 1 -o ${CLUSTER_1} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/cluster_config_file_c1_bck_2 cheferd_cluster_machine3 > ../output/cluster_output_node1_machine3.out" &

sleep 180

#Kill global
#ibrun -n 1 -o ${GLOBAL_INDEX} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/global_config_file_bck_2 cheferd_global_machine3 > ../output/global_output_machine3.out" &

sleep 180

#Kill cluster 1,2,3,4
#ibrun -n 1 -o ${CLUSTER_1} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/cluster_config_file_c1_bck cheferd_cluster_machine4 > ../output/cluster_output_node1_machine4.out" &
#ibrun -n 1 -o ${CLUSTER_2} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/cluster_config_file_c2_bck_2 cheferd_cluster_machine3 > ../output/cluster_output_node2_machine3.out" &
#ibrun -n 1 -o ${CLUSTER_3} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/cluster_config_file_c3_bck_2 cheferd_cluster_machine3 > ../output/cluster_output_node3_machine3.out" &
#ibrun -n 1 -o ${CLUSTER_4} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/cluster_config_file_c4_bck_2 cheferd_cluster_machine3 > ../output/cluster_output_node4_machine3.out" &

sleep 180

#Kill global and cluster 1
#ibrun -n 1 -o ${GLOBAL_INDEX} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/global_config_file_bck cheferd_global_machine4 > ../output/global_output_machine4.out" &
#ibrun -n 1 -o ${CLUSTER_1} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/cluster_config_file_c1_bck_2 cheferd_cluster_machine5 > ../output/cluster_output_node1_machine5.out" &

sleep 180

#Kill all -> global and cluster 1,2,3,4
#ibrun -n 1 -o ${GLOBAL_INDEX} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/global_config_file_bck_2 cheferd_global_machine5 > ../output/global_output_machine5.out" &
#ibrun -n 1 -o ${CLUSTER_1} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/cluster_config_file_c1_bck cheferd_cluster_machine6 > ../output/cluster_output_node1_machine6.out" &
#ibrun -n 1 -o ${CLUSTER_2} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/cluster_config_file_c2_bck cheferd_cluster_machine4 > ../output/cluster_output_node2_machine4.out" &
#ibrun -n 1 -o ${CLUSTER_3} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/cluster_config_file_c3_bck cheferd_cluster_machine4 > ../output/cluster_output_node3_machine4.out" &
#ibrun -n 1 -o ${CLUSTER_4} ./kill_and_launch_process.sh cheferd_exec "cd $PWD;cd cheferd/ ;./cheferd_exec --config_file ../files/cluster_config_file_c4_bck cheferd_cluster_machine4 > ../output/cluster_output_node4_machine4.out" &

sleep 1040

#for LOCAL_NODE in $(seq ${LOCAL_CLUSTER_NODE_INDEX} ${END_LOCAL_NODE_INDEX})
#do
#    #ibrun -n 1 -o ${LOCAL_NODE} bash -c 'cp /tmp/local_output_app* output/'
#done

#ibrun -n 1 -o ${GLOBAL_INDEX} ./kill_process.sh cheferd_exec
