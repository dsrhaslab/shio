#!/bin/bash

#SBATCH -J rt-distributed-single-job      # Job name
#SBATCH -p normal                 # Queue (partition) name
#SBATCH -N 54                  # Total # of nodes (must be 1 for serial)
#SBATCH -n 54             # Total # of mpi tasks (should be 1 for serial)
#SBATCH -t 01:00:00               # Run time (hh:mm:ss)


./call_multiple_op2_2500_try_50_per_node_all.sh 1 "2500nodes_with_resnet"
sleep 60







