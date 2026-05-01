#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
CONTAINER="$("$ROOT/docker_start_builder.sh" | tail -n 1)"
UID_HOST="$(id -u)"
GID_HOST="$(id -g)"

docker exec "$CONTAINER" chown -R "$UID_HOST:$GID_HOST" /src/kindle-chinesechess
exec docker exec -it --user "$UID_HOST:$GID_HOST" "$CONTAINER" /bin/bash
