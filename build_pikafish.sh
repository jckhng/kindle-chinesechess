#!/bin/bash
set -euo pipefail

# Cross-compiles Pikafish for Kindle ARMv7 hard-float inside the existing
# exact-chinesechess-armhf-builder Docker container, then places the binary
# and the NNUE network file into bin/armhf/ for packaging.
#
# ARMv7 requires two source patches:
#   types.h   - add software uint128_soft; __uint128_t is x86/aarch64-only
#   bitboard.h - add explicit uint64_t() casts in lsb() for the soft type
#
# ARCH is set to armv7 (no NEON) because armv7-neon would enable
# USE_NEON=7, which pulls in vaddvq_u16 (an ARMv8-only horizontal-add
# intrinsic) from the NNUE sparse-input path.

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PIKAFISH_SRC="${EXACT_CHINESECHESS_PIKAFISH_SRC:-$ROOT/Pikafish}"
CONTAINER="${EXACT_CHINESECHESS_DOCKER_CONTAINER:-exact-chinesechess-armhf-builder}"
OUT_DIR="${EXACT_CHINESECHESS_PIKAFISH_OUT_DIR:-$ROOT/bin/armhf}"
ARCH="armv7"
JOBS="${EXACT_CHINESECHESS_PIKAFISH_JOBS:-$(nproc 2>/dev/null || echo 4)}"

if [ ! -d "$PIKAFISH_SRC/src" ]; then
    echo "Pikafish source not found at: $PIKAFISH_SRC" >&2
    echo "Set EXACT_CHINESECHESS_PIKAFISH_SRC to override." >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

NNUE_FILE="$OUT_DIR/pikafish.nnue"
if [ ! -f "$NNUE_FILE" ]; then
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
else
    echo "NNUE already present: $NNUE_FILE"
fi

BUILD_TEMP="$ROOT/_pikafish_build"
echo "Copying Pikafish source to build area ..."
rm -rf "$BUILD_TEMP"
cp -r "$PIKAFISH_SRC/." "$BUILD_TEMP"

# The net.sh script skips the download if pikafish.nnue is already in src/.
cp "$NNUE_FILE" "$BUILD_TEMP/src/pikafish.nnue"

echo "Applying ARM32 patches ..."
python3 - "$BUILD_TEMP" <<'PYEOF'
import sys, re, pathlib

build = pathlib.Path(sys.argv[1])

# ── types.h: insert uint128_soft and conditionalize Bitboard typedef ──────
types_h = build / "src" / "types.h"
src = types_h.read_text()

# The software uint128 struct inserted right before "namespace Stockfish {"
uint128_block = r'''
// Software 128-bit unsigned integer for ARM32 and other targets where GCC
// does not define __SIZEOF_INT128__ (i.e. no native __uint128_t).
// Provides the exact interface used by Pikafish's Bitboard type.
#ifndef __SIZEOF_INT128__
struct uint128_soft {
    uint64_t hi, lo;

    constexpr uint128_soft() noexcept : hi(0), lo(0) {}
    constexpr uint128_soft(uint64_t v) noexcept : hi(0), lo(v) {}
    constexpr uint128_soft(uint64_t h, uint64_t l) noexcept : hi(h), lo(l) {}

    constexpr operator bool()                        const noexcept { return hi | lo; }
    explicit constexpr operator unsigned()           const noexcept { return (unsigned)lo; }
    explicit constexpr operator unsigned long()      const noexcept { return (unsigned long)lo; }
    explicit constexpr operator unsigned long long() const noexcept { return (unsigned long long)lo; }

    friend constexpr uint128_soft operator~(uint128_soft a) noexcept { return {~a.hi, ~a.lo}; }
    friend constexpr uint128_soft operator&(uint128_soft a, uint128_soft b) noexcept { return {a.hi & b.hi, a.lo & b.lo}; }
    friend constexpr uint128_soft operator|(uint128_soft a, uint128_soft b) noexcept { return {a.hi | b.hi, a.lo | b.lo}; }
    friend constexpr uint128_soft operator^(uint128_soft a, uint128_soft b) noexcept { return {a.hi ^ b.hi, a.lo ^ b.lo}; }
    constexpr uint128_soft& operator&=(uint128_soft b) noexcept { hi &= b.hi; lo &= b.lo; return *this; }
    constexpr uint128_soft& operator|=(uint128_soft b) noexcept { hi |= b.hi; lo |= b.lo; return *this; }
    constexpr uint128_soft& operator^=(uint128_soft b) noexcept { hi ^= b.hi; lo ^= b.lo; return *this; }

    friend constexpr uint128_soft operator<<(uint128_soft a, int n) noexcept {
        if (n == 0)   return a;
        if (n >= 128) return {};
        if (n >= 64)  return {a.lo << (n - 64), 0};
        return {(a.hi << n) | (a.lo >> (64 - n)), a.lo << n};
    }
    friend constexpr uint128_soft operator>>(uint128_soft a, int n) noexcept {
        if (n == 0)   return a;
        if (n >= 128) return {};
        if (n >= 64)  return {0, a.hi >> (n - 64)};
        return {a.hi >> n, (a.lo >> n) | (a.hi << (64 - n))};
    }
    constexpr uint128_soft& operator<<=(int n) noexcept { return *this = *this << n; }
    constexpr uint128_soft& operator>>=(int n) noexcept { return *this = *this >> n; }
    // Extra overloads: avoid ambiguity when operator bool is non-explicit.
    // Without these, `x >> unsigned_val` is ambiguous between our operator>>(int)
    // and the built-in operator>>(unsigned long long, unsigned) via bool conversion.
    friend constexpr uint128_soft operator<<(uint128_soft a, unsigned int n) noexcept { return a << (int)n; }
    friend constexpr uint128_soft operator>>(uint128_soft a, unsigned int n) noexcept { return a >> (int)n; }

    friend constexpr uint128_soft operator+(uint128_soft a, uint128_soft b) noexcept {
        uint64_t new_lo = a.lo + b.lo;
        return {a.hi + b.hi + (new_lo < a.lo ? 1u : 0u), new_lo};
    }
    friend constexpr uint128_soft operator-(uint128_soft a, uint128_soft b) noexcept {
        return {a.hi - b.hi - (a.lo < b.lo ? 1u : 0u), a.lo - b.lo};
    }
    // Extra overloads: avoid ambiguity with built-in operator-(unsigned long long, int)
    // via the implicit bool conversion chain.
    friend constexpr uint128_soft operator-(uint128_soft a, int b) noexcept { return a - uint128_soft((uint64_t)b); }
    friend constexpr uint128_soft operator-(uint128_soft a, unsigned int b) noexcept { return a - uint128_soft((uint64_t)b); }
    friend constexpr uint128_soft operator-(int a, uint128_soft b) noexcept { return uint128_soft((uint64_t)a) - b; }
    friend constexpr uint128_soft operator-(uint128_soft a) noexcept {
        uint64_t new_lo = ~a.lo + 1;
        return {~a.hi + (new_lo == 0 ? 1u : 0u), new_lo};
    }
    friend constexpr uint128_soft operator*(uint128_soft a, uint128_soft b) noexcept {
        // Lower 128 bits of a 128x128 multiply (used for magic-bitboard indexing).
        const uint64_t a0 = a.lo & 0xFFFFFFFFu, a1 = a.lo >> 32;
        const uint64_t b0 = b.lo & 0xFFFFFFFFu, b1 = b.lo >> 32;
        const uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
        const uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFu) + (p10 & 0xFFFFFFFFu);
        return {p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32) + a.hi * b.lo + a.lo * b.hi,
                (p00 & 0xFFFFFFFFu) | ((mid & 0xFFFFFFFFu) << 32)};
    }

    friend constexpr bool operator==(uint128_soft a, uint128_soft b) noexcept { return a.lo == b.lo && a.hi == b.hi; }
    friend constexpr bool operator!=(uint128_soft a, uint128_soft b) noexcept { return !(a == b); }
    friend constexpr bool operator< (uint128_soft a, uint128_soft b) noexcept {
        return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo);
    }
};
#endif  // !__SIZEOF_INT128__

'''

# Insert the struct block before "namespace Stockfish {"
if "uint128_soft" not in src:
    src = src.replace("namespace Stockfish {", uint128_block + "namespace Stockfish {", 1)
    print("  types.h: inserted uint128_soft")
else:
    print("  types.h: uint128_soft already present, skipping")

# Conditionalize "using Bitboard = __uint128_t;"
old_bb = "using Bitboard = __uint128_t;"
new_bb = ("#ifdef __SIZEOF_INT128__\n"
          "using Bitboard = __uint128_t;\n"
          "#else\n"
          "using Bitboard = uint128_soft;\n"
          "#endif")
if old_bb in src:
    src = src.replace(old_bb, new_bb, 1)
    print("  types.h: conditionalized Bitboard typedef")
elif "using Bitboard = uint128_soft;" not in src:
    print("  types.h: WARNING - could not find 'using Bitboard = __uint128_t;'", file=sys.stderr)

types_h.write_text(src)

# ── bitboard.h: fix lsb() to use explicit uint64_t() casts ───────────────
bitboard_h = build / "src" / "bitboard.h"
src = bitboard_h.read_text()

old_lsb = (
    "    if (uint64_t(b))\n"
    "        return Square(__builtin_ctzll(b));\n"
    "    return Square(__builtin_ctzll(b >> 64) + 64);"
)
new_lsb = (
    "    if (uint64_t(b))\n"
    "        return Square(__builtin_ctzll(uint64_t(b)));\n"
    "    return Square(__builtin_ctzll(uint64_t(b >> 64)) + 64);"
)
if old_lsb in src:
    src = src.replace(old_lsb, new_lsb, 1)
    print("  bitboard.h: patched lsb() __builtin_ctzll casts")
elif "uint64_t(b >> 64)" in src:
    print("  bitboard.h: lsb() already patched, skipping")
else:
    print("  bitboard.h: WARNING - could not find lsb() pattern to patch", file=sys.stderr)

bitboard_h.write_text(src)
print("Patches applied.")
PYEOF

echo "Building Pikafish ARCH=$ARCH in Docker container $CONTAINER ..."
docker exec "$CONTAINER" /bin/bash -lc "
    set -e
    cd /src/exact-chinesechess/_pikafish_build/src
    make clean 2>/dev/null || true
    # EXTRALDFLAGS: statically embed libstdc++, libgcc, and libm so the binary
    # does not need GLIBC_2.29 (log/pow/exp) at runtime on the Kindle older glibc.
    make build ARCH=$ARCH COMP=gcc CXX=g++ -j$JOBS \
        'EXTRALDFLAGS=-static-libstdc++ -static-libgcc -Wl,-Bstatic,-lm,-Bdynamic'
"

BINARY="$BUILD_TEMP/src/pikafish"
if [ ! -f "$BINARY" ]; then
    echo "Build failed: pikafish binary not found at $BINARY" >&2
    rm -rf "$BUILD_TEMP"
    exit 1
fi

cp "$BINARY" "$OUT_DIR/pikafish"
chmod 755 "$OUT_DIR/pikafish"
rm -rf "$BUILD_TEMP"

echo ""
echo "Pikafish built successfully:"
file "$OUT_DIR/pikafish"
echo "  binary: $OUT_DIR/pikafish ($(du -sh "$OUT_DIR/pikafish" | cut -f1))"
echo "  nnue:   $NNUE_FILE ($(du -sh "$NNUE_FILE" | cut -f1))"
