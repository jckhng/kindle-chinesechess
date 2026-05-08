#!/bin/sh

LOG_FILE="/mnt/us/exact-chinesechess.log"

if [ ! -f "$LOG_FILE" ]; then
    echo "No log yet: $LOG_FILE" >&2
    exit 0
fi

tail -n 80 "$LOG_FILE" >&2
exit 0
