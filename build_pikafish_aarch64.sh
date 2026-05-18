#!/bin/bash
set -euo pipefail

# Builds Pikafish for aarch64 PortMaster devices.
# This is separate from build_pikafish.sh, which keeps the older Kindle ARMv7
# hard-float build path.

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PIKAFISH_SRC="${EXACT_CHINESECHESS_PIKAFISH_SRC:-$ROOT/Pikafish}"
OUT_DIR="${EXACT_CHINESECHESS_PIKAFISH_OUT_DIR:-$ROOT/bin/aarch64}"
IMAGE="${EXACT_CHINESECHESS_PM_IMAGE:-ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest}"
JOBS="${EXACT_CHINESECHESS_PIKAFISH_JOBS:-$(nproc 2>/dev/null || echo 4)}"

if [ ! -d "$PIKAFISH_SRC/src" ]; then
    echo "Pikafish source not found at: $PIKAFISH_SRC" >&2
    echo "Set EXACT_CHINESECHESS_PIKAFISH_SRC to override." >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

NNUE_FILE="$OUT_DIR/pikafish.nnue"
if [ ! -f "$NNUE_FILE" ]; then
    if [ -f "$ROOT/bin/armhf/pikafish.nnue" ]; then
        cp "$ROOT/bin/armhf/pikafish.nnue" "$NNUE_FILE"
    else
        echo "Downloading pikafish.nnue ..."
        NNUE_URL="https://github.com/official-pikafish/Networks/releases/download/master-net/pikafish.nnue"
        if command -v curl >/dev/null 2>&1; then
            curl -L --progress-bar "$NNUE_URL" -o "$NNUE_FILE"
        elif command -v wget >/dev/null 2>&1; then
            wget -q --show-progress "$NNUE_URL" -O "$NNUE_FILE"
        else
            echo "Neither curl nor wget available. Download pikafish.nnue manually to: $NNUE_FILE" >&2
            exit 1
        fi
    fi
else
    echo "NNUE already present: $NNUE_FILE"
fi

BUILD_TEMP="$ROOT/_pikafish_aarch64_build"
rm -rf "$BUILD_TEMP"
cp -a "$PIKAFISH_SRC/." "$BUILD_TEMP"
cp "$NNUE_FILE" "$BUILD_TEMP/src/pikafish.nnue"

docker run --rm --platform=linux/arm64 \
    -v "$ROOT:/src/exact-chinesechess" \
    -w /src/exact-chinesechess/_pikafish_aarch64_build/src \
    "$IMAGE" \
    bash -lc "set -e
        make clean >/dev/null 2>&1 || true
        make build ARCH=armv8 COMP=gcc CXX=g++ -j$JOBS \
            'EXTRALDFLAGS=-static-libstdc++ -static-libgcc -Wl,-Bstatic,-lm,-Bdynamic'
        strip pikafish
    "

cp "$BUILD_TEMP/src/pikafish" "$OUT_DIR/pikafish"
chmod 755 "$OUT_DIR/pikafish"
rm -rf "$BUILD_TEMP"

echo "Pikafish aarch64 built successfully:"
file "$OUT_DIR/pikafish"
echo "  binary: $OUT_DIR/pikafish ($(du -sh "$OUT_DIR/pikafish" | cut -f1))"
echo "  nnue:   $NNUE_FILE ($(du -sh "$NNUE_FILE" | cut -f1))"
