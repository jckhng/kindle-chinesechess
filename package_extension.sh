#!/bin/bash
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
WORK_ROOT="$ROOT/dist"
RELEASE_ROOT="$ROOT/release"
EXT_ROOT="$WORK_ROOT/extensions/exact-chinesechess"
DOC_ROOT="$WORK_ROOT/documents"
CONTAINER="${EXACT_CHINESECHESS_DOCKER_CONTAINER:-exact-chinesechess-armhf-builder}"
PIKAFISH_BIN="${EXACT_CHINESECHESS_PIKAFISH_BIN:-}"
PIKAFISH_NNUE="${EXACT_CHINESECHESS_PIKAFISH_NNUE:-}"
PIKAFISH_SRC="${EXACT_CHINESECHESS_PIKAFISH_SRC:-$ROOT/Pikafish}"

rm -rf "$WORK_ROOT"
mkdir -p "$EXT_ROOT/bin/armhf" \
         "$EXT_ROOT/lib/armhf" \
         "$EXT_ROOT/lib/armhf/gdk-pixbuf-loaders" \
         "$EXT_ROOT/assets/xiangqi" \
         "$EXT_ROOT/LICENSES" \
         "$DOC_ROOT"

cp "$ROOT/exact-chinesechess" "$EXT_ROOT/bin/armhf/exact-chinesechess"
cp "$ROOT/extension/config.xml" "$EXT_ROOT/config.xml"
cp "$ROOT/extension/menu.json" "$EXT_ROOT/menu.json"
cp "$ROOT/extension/launch_exactchinesechess.sh" "$EXT_ROOT/launch_exactchinesechess.sh"
cp "$ROOT/extension/stop_exactchinesechess.sh" "$EXT_ROOT/stop_exactchinesechess.sh"
cp "$ROOT/extension/tail_log_exactchinesechess.sh" "$EXT_ROOT/tail_log_exactchinesechess.sh"
cp "$ROOT/extension/shortcut_exactchinesechess.sh" "$DOC_ROOT/shortcut_exactchinesechess.sh"
cp "$ROOT/extension/NOTICE.txt" "$EXT_ROOT/NOTICE.txt"
cp "$ROOT/extension/README.md" "$EXT_ROOT/README-package.txt"
cp "$ROOT/extension/PIKAFISH.txt" "$EXT_ROOT/PIKAFISH.txt"
cp "$ROOT/extension/ASSETS.txt" "$EXT_ROOT/ASSETS.txt"
for asset in board.png \
             r_k.png r_a.png r_b.png r_n.png r_r.png r_c.png r_p.png \
             b_k.png b_a.png b_b.png b_n.png b_r.png b_c.png b_p.png
do
    cp "$ROOT/assets/xiangqi/$asset" "$EXT_ROOT/assets/xiangqi/$asset"
done

cp "$ROOT/licenses/COPYING" "$EXT_ROOT/LICENSES/COPYING"
cp "$ROOT/licenses/COPYING-DOCS" "$EXT_ROOT/LICENSES/COPYING-DOCS"
cp "$ROOT/licenses/COPYING.GPL3" "$EXT_ROOT/LICENSES/COPYING.GPL3"
cp "$ROOT/licenses/MIT" "$EXT_ROOT/LICENSES/MIT"

# Resolve pikafish binary from bin/armhf if not overridden
if [ -z "$PIKAFISH_BIN" ] && [ -f "$ROOT/bin/armhf/pikafish" ]; then
    PIKAFISH_BIN="$ROOT/bin/armhf/pikafish"
fi
if [ -z "$PIKAFISH_NNUE" ] && [ -f "$ROOT/bin/armhf/pikafish.nnue" ]; then
    PIKAFISH_NNUE="$ROOT/bin/armhf/pikafish.nnue"
fi

if [ -n "$PIKAFISH_BIN" ] && [ -f "$PIKAFISH_BIN" ]; then
    cp "$PIKAFISH_BIN" "$EXT_ROOT/bin/armhf/pikafish"
    chmod 755 "$EXT_ROOT/bin/armhf/pikafish"

    if [ -n "$PIKAFISH_NNUE" ] && [ -f "$PIKAFISH_NNUE" ]; then
        cp "$PIKAFISH_NNUE" "$EXT_ROOT/bin/armhf/pikafish.nnue"
        echo "Bundled pikafish.nnue ($(du -sh "$PIKAFISH_NNUE" | cut -f1))"
    else
        echo "Warning: pikafish.nnue not found; engine will not work without it." >&2
        echo "Run build_pikafish.sh first, or set EXACT_CHINESECHESS_PIKAFISH_NNUE." >&2
    fi

    if [ -f "$PIKAFISH_SRC/Copying.txt" ]; then
        cp "$PIKAFISH_SRC/Copying.txt" "$EXT_ROOT/LICENSES/PIKAFISH-COPYING.txt"
    fi
    if [ -f "$PIKAFISH_SRC/AUTHORS" ]; then
        cp "$PIKAFISH_SRC/AUTHORS" "$EXT_ROOT/LICENSES/PIKAFISH-AUTHORS.txt"
    fi
    {
        echo "Pikafish source"
        echo "==============="
        echo
        echo "Bundled binary: bin/armhf/pikafish"
        echo "Bundled NNUE:   bin/armhf/pikafish.nnue"
        if [ -d "$PIKAFISH_SRC/.git" ]; then
            echo "Upstream: $(git -C "$PIKAFISH_SRC" remote get-url origin 2>/dev/null || echo unknown)"
            echo "Commit:   $(git -C "$PIKAFISH_SRC" rev-parse HEAD 2>/dev/null || echo unknown)"
            echo "Describe: $(git -C "$PIKAFISH_SRC" describe --tags --always --dirty 2>/dev/null || echo unknown)"
        else
            echo "Source path at packaging time: $PIKAFISH_SRC"
        fi
        echo
        echo "Built for ARMv7 hard-float (armhf) using arm-linux-gnueabihf-g++ via"
        echo "the exact-chinesechess-armhf-builder Docker container."
        echo
        echo "Corresponding source model:"
        echo "- Clone the upstream repository at the commit above."
        echo "- Apply the ARM32 patches embedded verbatim in build_pikafish.sh."
        echo "- Rebuild with the exact invocation described in build_pikafish.sh and"
        echo "  docs/PIKAFISH_ARM32_BUILD.md."
    } > "$EXT_ROOT/LICENSES/PIKAFISH-SOURCE.txt"
else
    cat > "$EXT_ROOT/LICENSES/PIKAFISH-SOURCE.txt" <<EOF
No Pikafish binary was bundled.

The app will use its embedded fallback AI unless a Pikafish binary and its
NNUE network file are installed at:
  /mnt/us/extensions/exact-chinesechess/bin/armhf/pikafish
  /mnt/us/extensions/exact-chinesechess/bin/armhf/pikafish.nnue

Run build_pikafish.sh to compile Pikafish for ARM and download the NNUE, then
re-run package_extension.sh to bundle them.

Pikafish is GPLv3. If you distribute a package with Pikafish included, also
include the license, authors, and exact corresponding source/build details.
EOF
fi

if docker exec "$CONTAINER" /bin/bash -lc 'test -f /src/exact-chinesechess/exact-chinesechess' >/dev/null 2>&1; then
    docker exec "$CONTAINER" /bin/bash -lc '
        {
            echo /lib/arm-linux-gnueabihf/ld-linux-armhf.so.3
            ldd /src/exact-chinesechess/exact-chinesechess | grep -oE "/[^[:space:]]+"
            # Also include Pikafish C++ runtime deps (libstdc++, libgcc_s, libatomic)
            # so that when the main app spawns pikafish as a child process it finds them.
            if [ -f /src/exact-chinesechess/bin/armhf/pikafish ]; then
                ldd /src/exact-chinesechess/bin/armhf/pikafish | grep -oE "/[^[:space:]]+"
            fi
        } | sort -u
    ' > "$EXT_ROOT/LICENSES/RUNTIME-LIBS.txt"

    while IFS= read -r libpath; do
        [ -n "$libpath" ] || continue
        docker exec "$CONTAINER" /bin/bash -lc "cat '$libpath'" > "$EXT_ROOT/lib/armhf/$(basename "$libpath")"
    done < "$EXT_ROOT/LICENSES/RUNTIME-LIBS.txt"

    # Bundle the GDK pixbuf PNG loader so PNG piece images load on the Kindle
    docker exec "$CONTAINER" /bin/bash -lc \
        'cat /usr/lib/arm-linux-gnueabihf/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-png.so' \
        > "$EXT_ROOT/lib/armhf/gdk-pixbuf-loaders/libpixbufloader-png.so"

    cat > "$EXT_ROOT/LICENSES/THIRD-PARTY-NOTICE.txt" <<EOF
This package bundles ARM shared libraries copied from the Docker build
container ($CONTAINER) to reduce Kindle-side external dependencies.

The exact bundled files are listed in RUNTIME-LIBS.txt. These libraries remain
under their own upstream licenses.
EOF
else
    echo "No runtime library set was generated." > "$EXT_ROOT/LICENSES/RUNTIME-LIBS.txt"
    echo "No runtime libraries were bundled at packaging time." > "$EXT_ROOT/LICENSES/THIRD-PARTY-NOTICE.txt"
fi

chmod 755 "$EXT_ROOT/launch_exactchinesechess.sh" \
          "$EXT_ROOT/stop_exactchinesechess.sh" \
          "$EXT_ROOT/tail_log_exactchinesechess.sh" \
          "$DOC_ROOT/shortcut_exactchinesechess.sh" \
          "$EXT_ROOT/bin/armhf/exact-chinesechess"

if [ -f "$EXT_ROOT/lib/armhf/ld-linux-armhf.so.3" ]; then
    chmod 755 "$EXT_ROOT/lib/armhf/ld-linux-armhf.so.3"
fi

ZIP_NAME="exact-chinesechess-extension.zip"
(
    cd "$WORK_ROOT"
    if command -v zip >/dev/null 2>&1; then
        zip -qr "$ZIP_NAME" extensions documents
    else
        python3 - <<'PY'
import os, zipfile
with zipfile.ZipFile("exact-chinesechess-extension.zip", "w", zipfile.ZIP_DEFLATED) as zf:
    for top in ("extensions", "documents"):
        for root, _, files in os.walk(top):
            for name in files:
                path = os.path.join(root, name)
                zf.write(path, path)
PY
    fi
)

# Copy artifacts to release/
mkdir -p "$RELEASE_ROOT"
cp "$WORK_ROOT/$ZIP_NAME" "$RELEASE_ROOT/$ZIP_NAME"

# Generate SHA256SUMS
(
    cd "$RELEASE_ROOT"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$ZIP_NAME" > SHA256SUMS
    else
        python3 -c "
import hashlib, sys
data = open('$ZIP_NAME', 'rb').read()
print(hashlib.sha256(data).hexdigest() + '  $ZIP_NAME')
" > SHA256SUMS
    fi
)

echo ""
echo "Package created:"
echo "  $RELEASE_ROOT/$ZIP_NAME"
echo "  $RELEASE_ROOT/SHA256SUMS"
echo ""
echo "Contents:"
ls -lh "$RELEASE_ROOT/$ZIP_NAME"
cat "$RELEASE_ROOT/SHA256SUMS"
