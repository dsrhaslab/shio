#!/bin/bash

SERVICE=cheferd_exec

is_running() {
    pgrep "$SERVICE" >/dev/null
}

if is_running; then
    echo -e "${LIGHTRED}[!] ${WHITE}Please wait till process is finished."
    while true; do
        sleep 1
        is_running || break
    done
    echo "Process '$SERVICE' not running"
else
    echo "Process '$SERVICE' not running"
fi
