#!/usr/bin/env bash
#
#   Copyright (c) 2026 INESC TEC.
#
# Shared settings and helpers for launch_hierarchy.sh and launch_hierarchy_no_backup.sh. Meant to
# be sourced, not run. Both scripts launch the same hierarchy on the local machine:
#
#                              global
#                          /            \
#                   cluster 1          cluster 2
#                   /       \              |
#              local 1    local 2       local 3
#                 |          |             |
#               job 1      job 2         job 3
#                 |          |             |
#             stage(s)   stage(s)      stage(s)
#
# Each controller is started from the build directory as:
#   ./shio_exec --config_file <LOG_DIR>/configs/<controller>_config_file
#
# and each job's data plane stages, depending on DATA_PLANE, as:
#   synthetic: data_plane_stage <job_name> <stage_env> <stage_user> /tmp/<local_controller_address>.socket
#   real:      LD_PRELOAD=libpadll.so trace_replayer <trace.csv> -1 <JOB_DURATION>
#              (PADLL connects to the local controller at /tmp/<local_controller_address>.socket)
#
# Environment variables:
#   BUILD_DIR       directory containing shio_exec (default: <control_plane>/build)
#   DATA_PLANE      data plane used by the jobs: "synthetic" (synthetic data plane stage, randomly
#                   generated metrics) or "real" (I/O traces replayed through PADLL)
#                   (default: synthetic)
#   DATA_PLANE_BIN  synthetic data plane stage binary
#                   (default: <repo>/data_plane/synthetic_dp/build/data_plane_stage)
#   STAGES_PER_JOB  number of data plane stages launched per job (default: 1). Stages of the
#                   same job are told apart by their stage_env (1..STAGES_PER_JOB).
#   STAGE_USER      user reported by the data plane stages (default: $USER, or "shio")
#   JOB_CMD         custom command that starts a job, used instead of DATA_PLANE_BIN. It is
#                   launched with SHIO_JOB_NAME and SHIO_SOCKET (the local controller UNIX
#                   socket) exported.
#   PADLL_LIB       PADLL library preloaded by real stages
#                   (default: <repo>/data_plane/realistic_dp/paio_padll_dp/padll/build/libpadll.so)
#   PAIO_LIB_DIR    directory containing libpaio
#                   (default: <repo>/data_plane/realistic_dp/paio_padll_dp/paio/build)
#   TRACE_REPLAYER  trace replayer binary
#                   (default: <repo>/data_plane/realistic_dp/trace_replayer/trace_replayer)
#   TRACES_DIR      directory with the collected traces
#                   (default: <repo>/data_plane/realistic_dp/trace_replayer/traces_collected)
#   JOB_APPS        space-separated application replayed by each job (in local controller order;
#                   reused cyclically): gromacs, resnet, openfoam, or shufflenet
#                   (default: "gromacs resnet openfoam")
#   JOB_DURATION    seconds each real job replays its trace (default: 60)
#   RESULTS_DIR     base directory for the results of each run (default: <repo>/results)
#   LOG_DIR         directory for the logs of this run (default: <RESULTS_DIR>/<timestamp>).
#                   The run's generated config files are written to <LOG_DIR>/configs.
#   STARTUP_WAIT    seconds to wait between launch stages (default: 2)

set -euo pipefail

COMMON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$COMMON_DIR")"
CONTROL_PLANE_DIR="$REPO_DIR/control_plane"

BUILD_DIR="${BUILD_DIR:-$CONTROL_PLANE_DIR/build}"
DATA_PLANE="${DATA_PLANE:-synthetic}"
DATA_PLANE_BIN="${DATA_PLANE_BIN:-$REPO_DIR/data_plane/synthetic_dp/build/data_plane_stage}"
REAL_DP_DIR="$REPO_DIR/data_plane/realistic_dp"
PADLL_LIB="${PADLL_LIB:-$REAL_DP_DIR/paio_padll_dp/padll/build/libpadll.so}"
PAIO_LIB_DIR="${PAIO_LIB_DIR:-$REAL_DP_DIR/paio_padll_dp/paio/build}"
TRACE_REPLAYER="${TRACE_REPLAYER:-$REAL_DP_DIR/trace_replayer/trace_replayer}"
TRACES_DIR="${TRACES_DIR:-$REAL_DP_DIR/trace_replayer/traces_collected}"
JOB_APPS="${JOB_APPS:-gromacs resnet openfoam}"
JOB_DURATION="${JOB_DURATION:-60}"
STAGES_PER_JOB="${STAGES_PER_JOB:-1}"
STAGE_USER="${STAGE_USER:-${USER:-shio}}"
JOB_CMD="${JOB_CMD:-}"
RESULTS_DIR="${RESULTS_DIR:-$REPO_DIR/results}"
LOG_DIR="${LOG_DIR:-$RESULTS_DIR/$(date +%Y%m%d_%H%M%S)}"
STARTUP_WAIT="${STARTUP_WAIT:-2}"

# Config files are generated for each run, next to its logs. Paths inside them (e.g., the policy
# files) are relative to the build directory, where the controllers run.
CONFIG_DIR="$LOG_DIR/configs"

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

# Launched processes: NAMES[i] is the name (and log file) of the process with pid PIDS[i].
NAMES=()
PIDS=()
# Pids of the launched jobs (their stages), waited on by wait_jobs.
JOB_PIDS=()

log () {
    echo "[$(date +%H:%M:%S)] $*"
}

cleanup () {
    trap - INT TERM EXIT
    log "Stopping remaining processes ..."
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
    NAMES+=("$name")
    PIDS+=("$pid")
    log "Started $name (pid $pid, log $LOG_DIR/$name.log)"
}

# launch_controller <name>. Runs ./shio_exec with $CONFIG_DIR/<name>_config_file.
launch_controller () {
    local name="$1"
    launch "$name" ./shio_exec --config_file "$CONFIG_DIR/${name}_config_file"
}

# stop <signal> <name>. Sends a signal to a launched process (and its children) and waits for it
# to exit.
stop () {
    local signal="$1" name="$2" i
    for i in "${!NAMES[@]}"; do
        if [[ "${NAMES[$i]}" == "$name" ]]; then
            local pid="${PIDS[$i]}"
            pkill "-$signal" -P "$pid" 2>/dev/null || true
            kill "-$signal" "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
            log "Stopped $name (pid $pid, SIG$signal)"
            return
        fi
    done
    log "No process named $name"
}

# stop_job <signal> <local id>. Stops every stage of the job attached to that local controller.
stop_job () {
    local signal="$1" id="$2" name
    for name in ${NAMES[@]+"${NAMES[@]}"}; do
        if [[ "$name" == "job${id}" || "$name" == "job${id}_stage"* ]]; then
            stop "$signal" "$name"
        fi
    done
}

# write_global_config <file> <role> <own internal port> <twin internal port>
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

# write_cluster_config <file> <role> <own internal port> <twin internal port> <upper port>
#                      <down port>
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

# write_local_config <file> <upper port> <own port>
write_local_config () {
    local file="$1" upper="$2" own_upper="$3"
    cat > "$file" <<EOF
controller: local
upper_address: $HOST:$upper
own_upper_address: $HOST:$own_upper
EOF
}

# prepare. Checks the controller binary, creates the config and log directories, moves to the
# build directory and installs the cleanup handler.
prepare () {
    if [[ ! -x "$BUILD_DIR/shio_exec" ]]; then
        echo "Controller binary not found or not executable: $BUILD_DIR/shio_exec" >&2
        echo "Build it first (mkdir -p build && cd build && cmake .. && cmake --build .) or set BUILD_DIR." >&2
        exit 1
    fi

    mkdir -p "$CONFIG_DIR" "$LOG_DIR"
    cd "$BUILD_DIR"
    trap cleanup INT TERM EXIT
    log "Configs: $CONFIG_DIR | Logs: $LOG_DIR"
}

# launch_locals. Launches the local controllers, each connected to its parent cluster.
launch_locals () {
    local i id parent
    for i in "${!LOCAL_IDS[@]}"; do
        id="${LOCAL_IDS[$i]}"
        parent="${LOCAL_PARENT_CLUSTER[$i]}"

        write_local_config "$CONFIG_DIR/local${id}_config_file" \
            "${CLUSTER_DOWN_PORTS[$parent]}" "${LOCAL_PORTS[$i]}"
        launch_controller "local${id}"
    done
}

# trace_file <app> <stage_env>. Prints the trace replayed by a stage of an application. Traces
# collected on several compute nodes (c1, c2, ...) are assigned to stages cyclically, and are
# extracted from their zip on first use.
trace_file () {
    local app="$1" env="$2" dir zips count node
    case "$app" in
        gromacs) dir="$TRACES_DIR/gromacs_3072" ;;
        resnet) dir="$TRACES_DIR/resnet50_4_nodes_4_gpus_4_epochs" ;;
        openfoam) dir="$TRACES_DIR/openfoam" ;;
        shufflenet) dir="$TRACES_DIR/tensorflow_shufflenet_1_epoch_1_node" ;;
        *)
            echo "Unknown application: $app (expected gromacs, resnet, openfoam, or shufflenet)" >&2
            return 1
            ;;
    esac

    count=$(ls "$dir"/c*_merged.zip 2>/dev/null | wc -l | tr -d ' ')
    if [[ "$count" -eq 0 ]]; then
        echo "No traces found in $dir" >&2
        return 1
    fi
    node=$(((env - 1) % count + 1))

    if [[ ! -d "$dir/c${node}_merged" ]]; then
        (cd "$dir" && unzip -o -q "c${node}_merged.zip")
    fi
    ls "$dir/c${node}_merged/"* | head -n 1
}

# launch_real_stage <name> <job name> <stage_env> <app> <socket>. Launches a stage of the real data
# plane: the trace replayer, with PADLL preloaded and connected to the local controller.
launch_real_stage () {
    local name="$1" job_name="$2" env="$3" app="$4" socket="$5" trace
    trace=$(trace_file "$app" "$env")
    launch "$name" env \
        paio_name="$job_name" \
        paio_env="$env" \
        padll_workflows=2 \
        paio_stage_opt=1 \
        cheferd_local_address="$socket" \
        LD_LIBRARY_PATH="$PAIO_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        LD_PRELOAD="$PADLL_LIB" \
        "$TRACE_REPLAYER" "$trace" -1 "$JOB_DURATION"
    log "  $name replays $app ($trace) for ${JOB_DURATION}s"
}

# check_data_plane. Checks that the binaries of the selected data plane exist.
check_data_plane () {
    case "$DATA_PLANE" in
        synthetic)
            if [[ ! -x "$DATA_PLANE_BIN" ]]; then
                log "Data plane stage binary not found: $DATA_PLANE_BIN"
                log "Build it (cd data_plane/synthetic_dp && mkdir -p build && cd build && cmake .. && cmake --build .) or set DATA_PLANE_BIN."
                return 1
            fi
            ;;
        real)
            if [[ ! -f "$PADLL_LIB" || ! -x "$TRACE_REPLAYER" ]]; then
                log "Real data plane not found (PADLL_LIB=$PADLL_LIB, TRACE_REPLAYER=$TRACE_REPLAYER)."
                log "Build PAIO, PADLL, and the trace replayer (see the Dockerfile) or set these variables."
                return 1
            fi
            ;;
        *)
            log "Unknown DATA_PLANE: $DATA_PLANE (expected synthetic or real)"
            return 1
            ;;
    esac
}

# launch_jobs. Launches each job: JOB_CMD, or STAGES_PER_JOB stages of the selected data plane. The
# local controller accepts data plane stages on /tmp/<own_upper_address>.socket
launch_jobs () {
    local i id env socket apps app
    if [[ -z "$JOB_CMD" ]] && ! check_data_plane; then
        log "Skipping job launch."
        return
    fi

    apps=($JOB_APPS)
    for i in "${!LOCAL_IDS[@]}"; do
        id="${LOCAL_IDS[$i]}"
        socket="/tmp/$HOST:${LOCAL_PORTS[$i]}.socket"

        if [[ -n "$JOB_CMD" ]]; then
            SHIO_JOB_NAME="${LOCAL_JOB_NAMES[$i]}" SHIO_SOCKET="$socket" \
                launch "job${id}" bash -c "$JOB_CMD"
            JOB_PIDS+=("${PIDS[${#PIDS[@]} - 1]}")
            continue
        fi

        app="${apps[$((i % ${#apps[@]}))]}"
        for env in $(seq 1 "$STAGES_PER_JOB"); do
            if [[ "$DATA_PLANE" == "real" ]]; then
                launch_real_stage "job${id}_stage${env}" "${LOCAL_JOB_NAMES[$i]}" "$env" "$app" "$socket"
            else
                launch "job${id}_stage${env}" "$DATA_PLANE_BIN" \
                    "${LOCAL_JOB_NAMES[$i]}" "$env" "$STAGE_USER" "$socket"
            fi
            JOB_PIDS+=("${PIDS[${#PIDS[@]} - 1]}")
        done
    done
}

# wait_jobs. Waits for every launched job to finish.
wait_jobs () {
    local pid
    for pid in ${JOB_PIDS[@]+"${JOB_PIDS[@]}"}; do
        wait "$pid" 2>/dev/null || true
    done
}
