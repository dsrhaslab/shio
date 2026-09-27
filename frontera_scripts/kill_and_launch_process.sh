#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Usage: $0 <keyword> <command> [args...]"
    exit 1
fi

PROCESS_TO_KILL="$1"
CONTROLLER_TYPE="$3"

pkill -9 -o "$PROCESS_TO_KILL"

sleep 30

PROCESS_TO_LAUNCH="$2"
echo "Starting: $PROCESS_TO_LAUNCH"

bash -c "$PROCESS_TO_LAUNCH"
