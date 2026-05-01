#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
CONTAINER="$("$ROOT/docker_start_builder.sh" | tail -n 1)"
UID_HOST="$(id -u)"
GID_HOST="$(id -g)"
MAKE_TARGETS="${KINDLE_CHINESECHESS_MAKE_TARGETS:-kindle-chinesechess smoke-test}"
DO_PACKAGE="${KINDLE_CHINESECHESS_PACKAGE:-1}"

docker exec "$CONTAINER" chown -R "$UID_HOST:$GID_HOST" /src/kindle-chinesechess
docker exec --user "$UID_HOST:$GID_HOST" "$CONTAINER" /bin/sh -lc "make clean && make $MAKE_TARGETS && ./smoke-test"

if [ "$DO_PACKAGE" = "1" ]; then
    KINDLE_CHINESECHESS_DOCKER_CONTAINER="$CONTAINER" "$ROOT/package_extension.sh"
fi

echo "Builder container: $CONTAINER"
echo "Binary: $ROOT/kindle-chinesechess"
if [ "$DO_PACKAGE" = "1" ]; then
    echo "Package: $ROOT/dist/kindle-chinesechess-extension.zip"
fi
