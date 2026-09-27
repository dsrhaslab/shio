#!/bin/bash

SERVICE=cheferd_exec

is_running() {
    pgrep "$SERVICE" || true > /dev/null
}

if is_running; then
    echo -e "${LIGHTRED}[!] ${WHITE}Please wait till process is finished."
    while true; do
        sleep 1
        is_running || break
    done
else
    echo "Process '$SERVICE' not running"
fi
