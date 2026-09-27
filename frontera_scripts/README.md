# 🧪 Running the experiments on Frontera

This directory contains the scripts used for the paper's experiments on the [Frontera](https://tacc.utexas.edu/systems/frontera/) supercomputer. They are submitted through Slurm and launch controllers and data plane stages across nodes with `ibrun`.

> ⚠️ These scripts assume Frontera's environment (Slurm, `ibrun`, Remora, and the `$WORK`/`$SCRATCH` file systems) and a copy of this directory and of the compiled binaries under `$WORK`. Adjust the paths at the top of `shared/test_distributed_multi_deallocate_with_jobs_config_file.sh` to your environment. To run SHIO on a single machine instead, see the [main README](../README.md).

## Running

The `test_*nodes.sh` scripts are the entry points, each targeting a different system size:

```bash
cd frontera_scripts

sbatch test_50_nodes.sh     # 50 stages
sbatch test_400nodes.sh     # 400 stages
sbatch test_2500nodes.sh    # 2,500 stages
sbatch test_10000nodes.sh   # 10,000 stages
```

Each entry point calls the matching `shared/call_multiple_op2_*.sh` script, which sets the experiment's topology (cluster controllers, physical nodes, stages per node). It then runs `shared/test_distributed_multi_deallocate_with_jobs_config_file.sh`, which deploys the global, cluster, and local controllers and the data plane stages, replaying the collected traces through PADLL. CPU, memory, and network usage are collected with [Remora](https://github.com/TACC/remora).

## Controller setup

The controller setup is selected by the third argument of `call_multiple_op2_*.sh`:
- `dependability_testing` (default): primary + backup global and cluster controllers, with failure injection (`dependability_testing/`), as in §4.3 of the paper;
- `normal_testing`: single global and cluster controllers (`normal_testing/`).

```bash
./shared/call_multiple_op2_400_try_50_per_node_all.sh test_name "10000nodes_50_100_50_100_50" normal_testing
```

## Directory structure

```
frontera_scripts/
├── test_*nodes.sh           # entry points (Slurm jobs), one per system size
├── shared/                  # scripts shared by both controller setups
├── dependability_testing/   # controllers with backups, and failure injection
├── normal_testing/          # controllers without backups
├── jobs_config_files/       # jobs deployed in each run (Configuration A, workload A1)
├── files/                   # controller and policy configuration files
└── processing_scripts/      # scripts to process the outputs
```
