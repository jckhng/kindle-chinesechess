#!/bin/sh

EXT_DIR="/mnt/us/extensions/exact-chinesechess"
LAUNCHER="$EXT_DIR/launch_exactchinesechess.sh"
LOG_FILE="/mnt/us/exact-chinesechess-shortcut.log"

{
    echo "----- Exact Chinese Chess shortcut $(date) -----"
    echo "cwd=$(pwd)"
    echo "launcher=$LAUNCHER"
} >>"$LOG_FILE" 2>&1

if [ ! -f "$LAUNCHER" ]; then
    echo "launcher not found: $LAUNCHER" >>"$LOG_FILE" 2>&1
    exit 1
fi

chmod 755 "$EXT_DIR"/*.sh 2>/dev/null || true
cd "$EXT_DIR" || exit 1

export DISPLAY="${DISPLAY:-:0}"
export HOME="${HOME:-/mnt/us}"
export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

nohup /bin/sh "$LAUNCHER" --restart >>"$LOG_FILE" 2>&1 &
exit 0
