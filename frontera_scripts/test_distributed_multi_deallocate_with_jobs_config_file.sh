cd $SCRATCH

EXP=${1}
VERSION=${2}
CLUSTER_NODES=${3}
LOCAL_NODES=${4}
BOOTSTRAP_NODES=${5}
PER_CLUSTER_NODES=${6}
PER_NODE_JOBS=${7}
EXEC_PATH=${8}
DATA_PLANE_EXEC_PATH=${9}
TEST_PATH=${10}
TEST_PATH=test_dynamic_deployment_traces_deallocated
TEST_VERSION=${11}
JOBS_CONFIG_PATH=${12}

rm -r test_scalability_${TEST_VERSION}
mkdir test_scalability_${TEST_VERSION}

#Copy to SCRATCH necessary files
BASE_PATH=$WORK/dependability_evaluation/${TEST_PATH}

cp ${BASE_PATH}/global_job_single_with_prep.sh test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/global_job_single_with_prep_and_backup.sh test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/cluster_job_single.sh test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/cluster_job_single_and_backup.sh test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/launch_local_controllers.sh test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/local_job_single.sh test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/local_job_single_trace.sh test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/data_job_single_timed_padll.sh test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/execute_jobs.py test_scalability_${TEST_VERSION}

cp ${BASE_PATH}/split_nodelist.py test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/fail_and_launch_controllers.sh test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/kill_and_launch_process.sh test_scalability_${TEST_VERSION}
cp ${BASE_PATH}/kill_process.sh test_scalability_${TEST_VERSION}

cd test_scalability_${TEST_VERSION}/

#Copy executables
mkdir cheferd/
cp $WORK/dependability_cheferd_version/cheferd_scalability/cheferd_${VERSION}/build/cheferd_exec_${EXEC_PATH} cheferd
cp cheferd/cheferd_exec_${EXEC_PATH} cheferd/cheferd_exec
cp $WORK/trace_replayer_cheferd_version/cheferd_scalability/cheferd_${VERSION}/build/cheferd_exec_${EXEC_PATH} cheferd
cp cheferd/cheferd_exec_${EXEC_PATH} cheferd/cheferd_exec
cp $WORK/datasets_final/collected_traces_replayer/trace_replayer_file_cropping cheferd

#Copy initial configuration files
cp -r ${BASE_PATH}/files/ .
cp -r ${BASE_PATH}/jobs_config_files/${JOBS_CONFIG_PATH}/* files/

#Create configuration files
#cp ${BASE_PATH}/split_distribution_jobs_data_into_local_and_cluster_files_and_assign_priority.py files
#cp ${BASE_PATH}/split_distribution_jobs_data_into_local_and_cluster_files_and_assign_ai_priority.py files
#cd files
#python3 split_distribution_jobs_data_into_local_and_cluster_files_and_assign_ai_priority.py --filename distribution_jobs.csv --deployments_per_compute_node 50
#cd ..

#Prep output directories
mkdir output/

#####

sleep 60

GLOBAL_INDEX=${BOOTSTRAP_NODES}

#Launch global controller
ibrun -n 1 -o ${GLOBAL_INDEX} global_job_single_with_prep_and_backup.sh &

#####

sleep 30

#Launch cluster controllers
for CLUSTER_NODE_ID in $(seq 1 ${CLUSTER_NODES}); do
    #offset one for the commander node
    beforenodes=$((CLUSTER_NODE_ID + BOOTSTRAP_NODES))

    ibrun -n 1 -o ${beforenodes} cluster_job_single_and_backup.sh ${CLUSTER_NODE_ID} &
    #ibrun -n 1 -o ${beforenodes} cluster_job_single.sh ${CLUSTER_NODE_ID} &

    sleep 1
done

#####

sleep 60

GLOBAL_NODES=1
BASE_NODES=$((BOOTSTRAP_NODES + GLOBAL_NODES + CLUSTER_NODES))
TOTAL_NODES=$((BASE_NODES + LOCAL_NODES))

echo "Before launch"

LOCAL_BOOTSTRAP_NODES=$((BOOTSTRAP_NODES - 1))
PER_BOOTSTRAP_NODE=$((CLUSTER_NODES / LOCAL_BOOTSTRAP_NODES))

#Launch bootstrap controller to start local controllers
for BOOTSTRAP_NODE in $(seq 1 ${LOCAL_BOOTSTRAP_NODES}); do
    ibrun -n 1 -o ${BOOTSTRAP_NODE} launch_local_controllers.sh ${BOOTSTRAP_NODE} ${PER_BOOTSTRAP_NODE} ${PER_CLUSTER_NODES} ${PER_NODE_JOBS} ${BASE_NODES} ${TOTAL_NODES} &

done

######
#Dependability Section

./fail_and_launch_controllers.sh ${GLOBAL_INDEX} ${CLUSTER_NODES} ${LOCAL_NODES} >output/fail_and_launch_output.out &

wait

sleep 5

rm -fr /tmp/padll-* /tmp/*520*.socket /tmp/app*.socket
