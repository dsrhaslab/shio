#!/usr/bin/env bash
#
#   Copyright (c) 2026 INESC TEC.
#
# Launches a full shio control plane hierarchy on the local machine, where the global and each
# cluster controller run alone (without a backup), and keeps it running until Ctrl-C. See
# hierarchy_common.sh for the hierarchy layout.
#
# Controllers without a backup still run as primaries. Their twin address points to the backup's
# internal port, where nothing listens, so notifications to the (missing) backup simply fail.
#
# Usage:
#   ./local_scripts/launch_hierarchy_no_backup.sh
#
# Environment variables: see hierarchy_common.sh.
#
# Press Ctrl-C to stop every launched process.

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

# 4. Jobs
launch_jobs

log "Hierarchy running. Press Ctrl-C to stop."
wait
