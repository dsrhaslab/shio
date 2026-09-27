#!/bin/bash

#SBATCH -J rt-distributed-single-job      # Job name
#SBATCH -p normal                 # Queue (partition) name
#SBATCH -N 16                  # Total # of nodes (must be 1 for serial)
#SBATCH -n 16             # Total # of mpi tasks (should be 1 for serial)
#SBATCH -t 01:00:00               # Run time (hh:mm:ss)


./call_multiple_op2_400_try_50_per_node_all.sh 400nodes_10000nodes_50_100_50_100_50_v5_full_dependability_no_kill "10000nodes_50_100_50_100_50_v5_full"
sleep 60



