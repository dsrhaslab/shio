#!/bin/bash

SERVICE=cheferd_exec

is_running() {
    pgrep "$SERVICE" > /dev/null
}

if ! is_running; then
    echo -e "${LIGHTRED}[!] ${WHITE}Please wait till process starts."
    while true; do
        sleep 1
        ! is_running || break
    done
else
    echo "Process '$SERVICE' is running"
fi
