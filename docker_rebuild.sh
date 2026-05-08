#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
CONTAINER="$("$ROOT/docker_start_builder.sh" | tail -n 1)"
UID_HOST="$(id -u)"
GID_HOST="$(id -g)"
MAKE_TARGETS="${EXACT_CHINESECHESS_MAKE_TARGETS:-exact-chinesechess smoke-test}"
DO_PACKAGE="${EXACT_CHINESECHESS_PACKAGE:-1}"

docker exec "$CONTAINER" chown -R "$UID_HOST:$GID_HOST" /src/exact-chinesechess
docker exec --user "$UID_HOST:$GID_HOST" "$CONTAINER" /bin/sh -lc "make clean && make $MAKE_TARGETS && ./smoke-test"

if [ "$DO_PACKAGE" = "1" ]; then
    EXACT_CHINESECHESS_DOCKER_CONTAINER="$CONTAINER" "$ROOT/package_extension.sh"
fi

echo "Builder container: $CONTAINER"
echo "Binary: $ROOT/exact-chinesechess"
if [ "$DO_PACKAGE" = "1" ]; then
    echo "Package: $ROOT/release/exact-chinesechess-extension.zip"
fi
