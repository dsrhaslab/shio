#!/bin/bash

#SBATCH -J rt-distributed-single-job      # Job name
#SBATCH -p normal                 # Queue (partition) name
#SBATCH -N 54                  # Total # of nodes (must be 1 for serial)
#SBATCH -n 54             # Total # of mpi tasks (should be 1 for serial)
#SBATCH -t 01:00:00               # Run time (hh:mm:ss)


./call_multiple_op2_2500_try_50_per_node_all.sh try8_4096_with_limit_cutting_try_copping_priorities 10000nodes_50_100_v2_full
sleep 60





