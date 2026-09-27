#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: $0 <keyword>"
    exit 1
fi

PROCESS_TO_KILL="$1"

pkill -f "$PROCESS_TO_KILL"
