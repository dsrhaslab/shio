#!/bin/bash

#SBATCH -J rt-distributed-single-job      # Job name
#SBATCH -p normal                 # Queue (partition) name
#SBATCH -N 208                  # Total # of nodes (must be 1 for serial)
#SBATCH -n 208             # Total # of mpi tasks (should be 1 for serial)
#SBATCH -t 02:00:00               # Run time (hh:mm:ss)


./call_multiple_op2_10000_try_50_per_node_all.sh 100nodes_10000nodes_50_100_50_100_50_v5_full_dependability_no_kill "10000nodes_50_100_50_100_50_v5_full"
sleep 60



