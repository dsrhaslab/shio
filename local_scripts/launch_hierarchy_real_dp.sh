#!/usr/bin/env bash
#
#   Copyright (c) 2026 INESC TEC.
#
# Launches a full shio control plane hierarchy on the local machine with the real data plane:
# each job replays the collected I/O traces of an HPC application (GROMACS, ResNet, OpenFOAM, or
# ShuffleNet) through the trace replayer, with PADLL intercepting its I/O and enforcing the rules
# of the control plane. The global and each cluster controller run without a backup. The
# hierarchy runs until every job has replayed its trace for JOB_DURATION seconds, and then all
# controllers are stopped. See hierarchy_common.sh for the hierarchy layout.
#
# The trace replayer issues its I/O on /tmp/replay_files, which is the mount point controlled by
# PADLL.
#
# Usage:
#   ./local_scripts/launch_hierarchy_real_dp.sh
#
# Environment variables: see hierarchy_common.sh (JOB_APPS, JOB_DURATION, STAGES_PER_JOB, ...).
# The real data plane can also be used with the other launch scripts by setting DATA_PLANE=real.
#
# Press Ctrl-C to stop every launched process at any time.

DATA_PLANE=real
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/hierarchy_common.sh"

prepare

# 1. Global controller
write_global_config "$CONFIG_DIR/global_config_file" primary \
    "$GLOBAL_PRIMARY_INTERNAL_PORT" "$GLOBAL_BACKUP_INTERNAL_PORT"

launch_controller global
sleep "$STARTUP_WAIT"

# 2. Cluster controllers
for i in "${!CLUSTER_IDS[@]}"; do
    id="${CLUSTER_IDS[$i]}"

    write_cluster_config "$CONFIG_DIR/cluster${id}_config_file" primary \
        "${CLUSTER_PRIMARY_INTERNAL_PORTS[$i]}" "${CLUSTER_BACKUP_INTERNAL_PORTS[$i]}" \
        "${CLUSTER_UPPER_PORTS[$i]}" "${CLUSTER_DOWN_PORTS[$i]}"

    launch_controller "cluster${id}"
done
sleep "$STARTUP_WAIT"

# 3. Local controllers
launch_locals
sleep "$STARTUP_WAIT"

# 4. Jobs (real data plane)
launch_jobs

log "Hierarchy running. Jobs replay their traces for ${JOB_DURATION}s (Ctrl-C to stop)."
wait_jobs

# 5. All controllers are stopped (by cleanup, on exit).
log "All jobs finished. Stopping all controllers."
