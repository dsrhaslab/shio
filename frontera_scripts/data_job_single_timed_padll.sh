#!/bin/bash


echo "module load gcc/9.1.0  python3/3.8.2 boost/1.72"
module load gcc/9.1.0
module load python3/3.8.2
module load boost/1.72

echo "Running data plane stage controller"

cd cheferd/


export CC=`which gcc`
export CXX=`which g++`

job_name=${1}
duration=${2}
physical_node_port=${3}
trace_compute_node=${4}

export CPATH="${WORK}/padll_paio/paio/include:${CPATH}"
export path_padll="${WORK}/padll_paio/padll/build"

NODE_STAGE=$(grep "$(hostname -s | sed 's/^c/i/')" /etc/hosts | awk '{print $1}')
echo "${NODE_STAGE}"

#Se if the compute node is ready for the new job, to account for 
#delays from the previous job.
PROCESS_NAME="attached_to_${NODE_STAGE}:520${physical_node_port}_process"

export paio_name=${job_name}
export paio_env=1
export padll_workflows=2
export paio_stage_opt=1
export cheferd_local_address=/tmp/${NODE_STAGE}:520${physical_node_port}.socket

letters_to_match=3  # number of characters to compare
#ADDED PROCESS_NAME TO CAN CONTROL IF THE NODE IS EMPTY

start=$(date +%s)


if [[ "${paio_name:0:$letters_to_match}" == "ope" ]]; then
    echo "Running openfoam"
    #LD_PRELOAD=$path_padll/libpadll.so ./trace_replayer_file_cropping $SCRATCH/datasets_final/openfoam/c1_merged/openfoam_c1_merged.csv 1738002343723313111 ${duration}
    flock -x /tmp/mylockfile_${PROCESS_NAME}  -c "
    sleep 10 && 
    LD_PRELOAD=$path_padll/libpadll.so ./trace_replayer_file_cropping $SCRATCH/datasets_final/openfoam/c1_merged/openfoam_c1_merged.csv -1 ${duration}"
elif [[ "${paio_name:0:$letters_to_match}" == "shu" ]]; then
    echo "Running shufflenet"
    #LD_PRELOAD=$path_padll/libpadll.so ./trace_replayer_file_cropping $SCRATCH/datasets_final/tensorflow_shufflenet_1_epoch_1_node/c1_merged/tensorflow_shufflenet_1_epoch_1_node_c1_merged_csv 1740588480869385130 ${duration}
    flock -x /tmp/mylockfile_${PROCESS_NAME}  -c "
    sleep 10 && 
    LD_PRELOAD=$path_padll/libpadll.so ./trace_replayer_file_cropping $SCRATCH/datasets_final/tensorflow_shufflenet_1_epoch_1_node/c1_merged/tensorflow_shufflenet_1_epoch_1_node_c1_merged_csv -1 ${duration}"
elif [[ "${paio_name:0:$letters_to_match}" == "gro" ]]; then
    echo "Running gromacs"
    #LD_PRELOAD=$path_padll/libpadll.so ./trace_replayer_file_cropping $SCRATCH/datasets_final/gromacs_3072/c${trace_compute_node}_merged/gromacs_3072_c${trace_compute_node}_merged.csv 1737999387114197662 ${duration}
    flock -x /tmp/mylockfile_${PROCESS_NAME}  -c "
    sleep 10 && 
    LD_PRELOAD=$path_padll/libpadll.so ./trace_replayer_file_cropping $SCRATCH/datasets_final/gromacs_3072/c${trace_compute_node}_merged/gromacs_3072_c${trace_compute_node}_merged.csv -1 ${duration}"
elif [[ "${paio_name:0:$letters_to_match}" == "res" ]]; then
    echo "Running resnet"
    #LD_PRELOAD=$path_padll/libpadll.so ./trace_replayer_file_cropping $SCRATCH/datasets_final/resnet50_4_nodes_4_gpus_4_epochs/c${trace_compute_node}_merged/resnet50_4_nodes_4_gpus_4_epochs_c${trace_compute_node}_merged.csv 1739270337883784530 ${duration}
    flock -x /tmp/mylockfile_${PROCESS_NAME}  -c "
    sleep 10 && 
    LD_PRELOAD=$path_padll/libpadll.so ./trace_replayer_file_cropping $SCRATCH/datasets_final/resnet50_4_nodes_4_gpus_4_epochs/c${trace_compute_node}_merged/resnet50_4_nodes_4_gpus_4_epochs_c${trace_compute_node}_merged.csv -1 ${duration}"
else
    echo "The first $n characters do not match."
fi

#LD_PRELOAD=$path_padll/libpadll.so ./data_plane_stage ${job_name} ${physical_node_port} user1 /tmp/${NODE_STAGE}:520${physical_node_port}.socket & 


#PID=$!

#echo  "Exiting '$job_name' with $PID"


#wait "$PID"

end=$(date +%s)

echo "${job_name};${duration};$((end - start));${start};${end}" >> ../output/job_time_differences.out


#rm -fr /tmp/${NODE_STAGE}:520${physical_node_port}.socket

echo "Exiting '$job_name' at $(date '+%Y-%m-%d %H:%M:%S')"