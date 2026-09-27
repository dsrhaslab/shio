#!/usr/bin/env bash
#
#   Copyright (c) 2026 INESC TEC.
#
# Launches a full shio control plane hierarchy on the local machine:
#
#                         global (primary + backup)
#                          /                     \
#     cluster 1 (primary + backup)        cluster 2 (primary + backup)
#          /            \                          |
#      local 1        local 2                   local 3
#         |              |                         |
#       job 1          job 2                     job 3
#         |              |                         |
#     stage(s)       stage(s)                  stage(s)
#
# Each controller is started from the build directory as:
#   ./shio_exec --config_file ../files/hierarchy/<controller>_config_file
#
# and each job's data plane stages as:
#   data_plane_stage <job_name> <stage_env> <stage_user> /tmp/<local_controller_address>.socket
#
# Usage:
#   ./scripts/launch_hierarchy.sh
#
# Environment variables:
#   BUILD_DIR       directory containing shio_exec (default: <control_plane>/build)
#   DATA_PLANE_BIN  data plane stage binary
#                   (default: <repo>/data_plane/synthetic_dp/build/data_plane_stage)
#   STAGES_PER_JOB  number of data plane stages launched per job (default: 1). Stages of the
#                   same job are told apart by their stage_env (1..STAGES_PER_JOB).
#   STAGE_USER      user reported by the data plane stages (default: $USER, or "shio")
#   JOB_CMD         custom command that starts a job, used instead of DATA_PLANE_BIN. It is
#                   launched with SHIO_JOB_NAME and SHIO_SOCKET (the local controller UNIX
#                   socket) exported.
#   LOG_DIR      directory for the logs (default: <build>/logs/<timestamp>)
#   STARTUP_WAIT seconds to wait between launch stages (default: 2)
#
# Press Ctrl-C to stop every launched process.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
CONTROL_PLANE_DIR="$REPO_DIR/control_plane"

BUILD_DIR="${BUILD_DIR:-$CONTROL_PLANE_DIR/build}"
DATA_PLANE_BIN="${DATA_PLANE_BIN:-$REPO_DIR/data_plane/synthetic_dp/build/data_plane_stage}"
STAGES_PER_JOB="${STAGES_PER_JOB:-1}"
STAGE_USER="${STAGE_USER:-${USER:-shio}}"
JOB_CMD="${JOB_CMD:-}"
LOG_DIR="${LOG_DIR:-$BUILD_DIR/logs/$(date +%Y%m%d_%H%M%S)}"
STARTUP_WAIT="${STARTUP_WAIT:-2}"

# Config files are generated here and referenced relative to the build directory.
CONFIG_DIR="$CONTROL_PLANE_DIR/files/hierarchy"
CONFIG_REL_DIR="../files/hierarchy"

HOST="0.0.0.0"

# Global controller
GLOBAL_DOWN_PORT=50051
GLOBAL_PRIMARY_INTERNAL_PORT=50061
GLOBAL_BACKUP_INTERNAL_PORT=50062

# Cluster controllers: primary and backup share the upper/down ports (only the active primary
# binds them) and use distinct internal ports for the primary/backup heartbeat.
CLUSTER_IDS=(1 2)
CLUSTER_UPPER_PORTS=(50052 50054)
CLUSTER_DOWN_PORTS=(50053 50055)
CLUSTER_PRIMARY_INTERNAL_PORTS=(50071 50073)
CLUSTER_BACKUP_INTERNAL_PORTS=(50072 50074)

# Local controllers: index into CLUSTER_IDS for the parent cluster, own port, and job name.
LOCAL_IDS=(1 2 3)
LOCAL_PARENT_CLUSTER=(0 0 1)
LOCAL_PORTS=(50057 50058 50059)
LOCAL_JOB_NAMES=(N1V1 N1V2 N1V3)

PIDS=()

log () {
    echo "[$(date +%H:%M:%S)] $*"
}

cleanup () {
    trap - INT TERM EXIT
    log "Stopping ${#PIDS[@]} processes ..."
    for pid in ${PIDS[@]+"${PIDS[@]}"}; do
        # also stop children (e.g., the process started by a job's bash -c wrapper)
        pkill -TERM -P "$pid" 2>/dev/null || true
        kill "$pid" 2>/dev/null || true
    done
    wait 2>/dev/null || true
    log "All processes stopped. Logs are in $LOG_DIR"
}

# launch <name> <command...>. Runs a command in the background, logging to $LOG_DIR/<name>.log.
launch () {
    local name="$1"
    shift
    "$@" > "$LOG_DIR/$name.log" 2>&1 &
    local pid=$!
    PIDS+=("$pid")
    log "Started $name (pid $pid, log $LOG_DIR/$name.log)"
}

# launch_controller <name>. Runs ./shio_exec with ../files/hierarchy/<name>_config_file.
launch_controller () {
    local name="$1"
    launch "$name" ./shio_exec --config_file "$CONFIG_REL_DIR/${name}_config_file"
}

write_global_config () {
    local file="$1" role="$2" own_internal="$3" twin="$4"
    cat > "$file" <<EOF
controller: global
role: $role
own_internal_address: $HOST:$own_internal
twin_controller_address: $HOST:$twin
housekeeping_rules_file: ../files/posix_layer_housekeeping_rules_meta_data
policies_rules_file: ../files/policy_4_rules_file_meta_data
jobs_config_file: ../files/global_jobs_config_file
own_down_address: $HOST:$GLOBAL_DOWN_PORT
EOF
}

write_cluster_config () {
    local file="$1" role="$2" own_internal="$3" twin="$4" own_upper="$5" own_down="$6"
    cat > "$file" <<EOF
controller: cluster
role: $role
own_internal_address: $HOST:$own_internal
twin_controller_address: $HOST:$twin
jobs_config_file: ../files/cluster_jobs_config_file
policies_rules_file: ../files/policy_4_rules_file_meta_data
upper_address: $HOST:$GLOBAL_DOWN_PORT
own_upper_address: $HOST:$own_upper
own_down_address: $HOST:$own_down
EOF
}

write_local_config () {
    local file="$1" upper="$2" own_upper="$3"
    cat > "$file" <<EOF
controller: local
upper_address: $HOST:$upper
own_upper_address: $HOST:$own_upper
EOF
}

if [[ ! -x "$BUILD_DIR/shio_exec" ]]; then
    echo "Controller binary not found or not executable: $BUILD_DIR/shio_exec" >&2
    echo "Build it first (mkdir -p build && cd build && cmake .. && cmake --build .) or set BUILD_DIR." >&2
    exit 1
fi

mkdir -p "$CONFIG_DIR" "$LOG_DIR"
cd "$BUILD_DIR"
trap cleanup INT TERM EXIT
log "Configs: $CONFIG_DIR | Logs: $LOG_DIR"

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

# 3. Local controllers and their jobs
for i in "${!LOCAL_IDS[@]}"; do
    id="${LOCAL_IDS[$i]}"
    parent="${LOCAL_PARENT_CLUSTER[$i]}"

    write_local_config "$CONFIG_DIR/local${id}_config_file" \
        "${CLUSTER_DOWN_PORTS[$parent]}" "${LOCAL_PORTS[$i]}"
    launch_controller "local${id}"
done
sleep "$STARTUP_WAIT"

# 4. Jobs (data plane stages). The local controller accepts data plane stages on
# /tmp/<own_upper_address>.socket
if [[ -n "$JOB_CMD" ]]; then
    for i in "${!LOCAL_IDS[@]}"; do
        id="${LOCAL_IDS[$i]}"
        socket="/tmp/$HOST:${LOCAL_PORTS[$i]}.socket"
        SHIO_JOB_NAME="${LOCAL_JOB_NAMES[$i]}" SHIO_SOCKET="$socket" \
            launch "job${id}" bash -c "$JOB_CMD"
    done
elif [[ -x "$DATA_PLANE_BIN" ]]; then
    for i in "${!LOCAL_IDS[@]}"; do
        id="${LOCAL_IDS[$i]}"
        for env in $(seq 1 "$STAGES_PER_JOB"); do
            launch "job${id}_stage${env}" "$DATA_PLANE_BIN" \
                "${LOCAL_JOB_NAMES[$i]}" "$env" "$STAGE_USER" "/tmp/$HOST:${LOCAL_PORTS[$i]}.socket"
        done
    done
else
    log "Data plane stage binary not found: $DATA_PLANE_BIN; skipping job launch."
    log "Build it (cd data_plane/synthetic_dp && mkdir -p build && cd build && cmake .. && cmake --build .) or set DATA_PLANE_BIN."
fi

log "Hierarchy running. Press Ctrl-C to stop."
wait
