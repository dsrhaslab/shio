#!/bin/bash

#SBATCH -J rt-distributed-single-job      # Job name
#SBATCH -p normal                 # Queue (partition) name
#SBATCH -N 5                  # Total # of nodes (must be 1 for serial)
#SBATCH -n 5             # Total # of mpi tasks (should be 1 for serial)
#SBATCH -t 01:00:00               # Run time (hh:mm:ss)

./call_multiple_op2_50_try_50_per_node_all.sh test_name "10000nodes_50_100_50_100_50"
sleep 60
