#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
IMAGE="${EXACT_CHINESECHESS_DOCKER_IMAGE:-exact-chinesechess-armhf-build:bullseye}"

docker build \
    --platform linux/arm/v7 \
    -t "$IMAGE" \
    -f "$ROOT/docker/Dockerfile.armhf-bullseye" \
    "$ROOT/docker"

echo "Built Docker image: $IMAGE"
