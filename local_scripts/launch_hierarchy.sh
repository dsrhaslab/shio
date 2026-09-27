#!/usr/bin/env bash
#
#   Copyright (c) 2026 INESC TEC.
#
# Launches a full shio control plane hierarchy on the local machine, where the global and each
# cluster controller run as a primary + backup pair, and then runs a failure scenario:
#
#   1. the hierarchy runs for RUN_WAIT seconds;
#   2. each primary controller is killed in sequence (global, then cluster 1, cluster 2, ...), so
#      that its backup takes over;
#   3. the jobs in EARLY_JOBS terminate;
#   4. the remaining jobs end in sequence;
#   5. all controllers are stopped.
#
# Steps are STEP_WAIT seconds apart. See hierarchy_common.sh for the hierarchy layout.
#
# Usage:
#   ./local_scripts/launch_hierarchy.sh
#
# Environment variables (in addition to the ones in hierarchy_common.sh):
#   RUN_WAIT     seconds the hierarchy runs before the first failure (default: 20)
#   STEP_WAIT    seconds between the steps of the scenario (default: 10)
#   EARLY_JOBS   space-separated ids (1..3) of the jobs terminated early (default: "2")
#
# Press Ctrl-C to stop every launched process at any time.

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/hierarchy_common.sh"

RUN_WAIT="${RUN_WAIT:-20}"
STEP_WAIT="${STEP_WAIT:-10}"
EARLY_JOBS="${EARLY_JOBS:-2}"

prepare

# 1. Global controller and its backup
write_global_config "$CONFIG_DIR/global_primary_config_file" primary \
    "$GLOBAL_PRIMARY_INTERNAL_PORT" "$GLOBAL_BACKUP_INTERNAL_PORT"
write_global_config "$CONFIG_DIR/global_backup_config_file" secondary \
    "$GLOBAL_BACKUP_INTERNAL_PORT" "$GLOBAL_PRIMARY_INTERNAL_PORT"

launch_controller global_primary
sleep "$STARTUP_WAIT"
launch_controller global_backup
sleep "$STARTUP_WAIT"

# 2. Cluster controllers and their backups
for i in "${!CLUSTER_IDS[@]}"; do
    id="${CLUSTER_IDS[$i]}"
    primary_internal="${CLUSTER_PRIMARY_INTERNAL_PORTS[$i]}"
    backup_internal="${CLUSTER_BACKUP_INTERNAL_PORTS[$i]}"
    upper="${CLUSTER_UPPER_PORTS[$i]}"
    down="${CLUSTER_DOWN_PORTS[$i]}"

    write_cluster_config "$CONFIG_DIR/cluster${id}_primary_config_file" primary \
        "$primary_internal" "$backup_internal" "$upper" "$down"
    write_cluster_config "$CONFIG_DIR/cluster${id}_backup_config_file" secondary \
        "$backup_internal" "$primary_internal" "$upper" "$down"

    launch_controller "cluster${id}_primary"
    sleep "$STARTUP_WAIT"
    launch_controller "cluster${id}_backup"
done
sleep "$STARTUP_WAIT"

# 3. Local controllers
launch_locals
sleep "$STARTUP_WAIT"

# 4. Jobs
launch_jobs

# 5. Failure scenario
log "Hierarchy running. Failure scenario starts in ${RUN_WAIT}s (Ctrl-C to stop)."
sleep "$RUN_WAIT"

# Primary controllers fail (SIGKILL, so they cannot shut down cleanly) and their backups take over.
log "Failing global primary controller"
stop KILL global_primary
sleep "$STEP_WAIT"

for id in "${CLUSTER_IDS[@]}"; do
    log "Failing cluster ${id} primary controller"
    stop KILL "cluster${id}_primary"
    sleep "$STEP_WAIT"
done

# Some jobs terminate while the others keep running.
for id in $EARLY_JOBS; do
    log "Terminating job ${id}"
    stop_job TERM "$id"
done
sleep "$STEP_WAIT"

# The remaining jobs end, one at a time.
for id in "${LOCAL_IDS[@]}"; do
    if [[ " $EARLY_JOBS " != *" $id "* ]]; then
        log "Ending job ${id}"
        stop_job TERM "$id"
        sleep "$STEP_WAIT"
    fi
done

# 6. All controllers are stopped (by cleanup, on exit).
log "Failure scenario finished. Stopping all controllers."
