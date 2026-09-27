# Local scripts

Scripts to launch a full SHIO controller hierarchy on a single machine. See the [main README](../README.md) for how to build the Docker image and what each script does.

| Script | Controllers | Data plane |
|---|---|---|
| `launch_hierarchy.sh` | primary + backup, with a failure scenario | synthetic (or real, with `DATA_PLANE=real`) |
| `launch_hierarchy_no_backup.sh` | without backups, runs until `Ctrl-C` | synthetic (or real, with `DATA_PLANE=real`) |
| `launch_hierarchy_real_dp.sh` | without backups, runs until all jobs finish | real |

`hierarchy_common.sh` holds the settings and helpers shared by the three scripts, and is not meant to be run directly.

## Environment variables

The scripts accept the following environment variables:

| Variable | Description | Default |
|---|---|---|
| `DATA_PLANE` | data plane used by the jobs: `synthetic` or `real` | `synthetic` (`real` for `launch_hierarchy_real_dp.sh`) |
| `STAGES_PER_JOB` | data plane stages launched per job | `1` |
| `DATA_PLANE_BIN` | synthetic data plane stage binary | `data_plane/synthetic_dp/build/data_plane_stage` |
| `STAGE_USER` | user reported by the synthetic data plane stages | `$USER`, or `shio` |
| `JOB_APPS` | application replayed by each job (real data plane): `gromacs`, `resnet`, `openfoam`, or `shufflenet` | `gromacs resnet openfoam` |
| `JOB_DURATION` | seconds each job replays its trace (real data plane) | `60` |
| `PADLL_LIB`, `PAIO_LIB_DIR`, `TRACE_REPLAYER`, `TRACES_DIR` | real data plane binaries and traces | paths built by the Docker image |
| `JOB_CMD` | custom command that starts a job, instead of a data plane | — |
| `BUILD_DIR` | directory containing the controller binary (`shio_exec`) | `control_plane/build` |
| `RESULTS_DIR` | base directory for the results | `results` |
| `LOG_DIR` | directory for the logs of a run | `<RESULTS_DIR>/<timestamp>` |
| `STARTUP_WAIT` | seconds between launch steps | `2` |
| `RUN_WAIT` | seconds before the first failure (`launch_hierarchy.sh` only) | `20` |
| `STEP_WAIT` | seconds between failure/termination steps (`launch_hierarchy.sh` only) | `10` |
| `EARLY_JOBS` | jobs terminated early (`launch_hierarchy.sh` only) | `2` |

## Examples

A shorter failure scenario with 5 stages per job:

```bash
RUN_WAIT=10 STEP_WAIT=5 STAGES_PER_JOB=5 ./local_scripts/launch_hierarchy.sh
```

2 minutes of ShuffleNet and GROMACS traces through PADLL:

```bash
JOB_APPS="shufflenet gromacs" JOB_DURATION=120 ./local_scripts/launch_hierarchy_real_dp.sh
```

The failure scenario with the real data plane:

```bash
DATA_PLANE=real ./local_scripts/launch_hierarchy.sh
```
