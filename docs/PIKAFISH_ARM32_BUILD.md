# Building Pikafish for Kindle (ARMv7 hard-float)

This document explains how to cross-compile [Pikafish](https://github.com/official-pikafish/Pikafish)
for the Kindle's ARMv7 hard-float (armhf) target and bundle it into the
`exact-chinesechess` extension package. It also records the non-obvious
engineering problems solved along the way so future maintainers do not have to
rediscover them.

---

## Prerequisites

| Requirement | Notes |
|---|---|
| Docker | Any recent version that can run ARM containers |
| `exact-chinesechess-armhf-builder` container | Created by `./docker_rebuild.sh` |
| Pikafish source tree | The in-repo `Pikafish` Git submodule, or set `EXACT_CHINESECHESS_PIKAFISH_SRC` to another clone |

> **ARM binfmt** — the builder container runs 32-bit ARM code inside Docker.  
> On Linux, install the emulation layer once with:
> ```bash
> docker run --privileged --rm tonistiigi/binfmt --install arm
> ```

---

## Quick Start

```bash
# 1. Initialize the pinned Pikafish submodule
git submodule update --init --recursive

# 2. Make sure the builder container is running
./docker_rebuild.sh

# 3. Compile Pikafish, download the NNUE, and drop them into bin/armhf/
./build_pikafish.sh

# 4. Repackage the extension (bundles the binary + NNUE automatically)
./package_extension.sh
```

The finished package is at `release/exact-chinesechess-extension.zip`.

### Environment variables

| Variable | Default | Purpose |
|---|---|---|
| `EXACT_CHINESECHESS_PIKAFISH_SRC` | `Pikafish` | Path to the Pikafish source tree |
| `EXACT_CHINESECHESS_DOCKER_CONTAINER` | `exact-chinesechess-armhf-builder` | Container name |
| `EXACT_CHINESECHESS_PIKAFISH_JOBS` | `nproc` | Parallel make jobs |
| `EXACT_CHINESECHESS_PIKAFISH_OUT_DIR` | `bin/armhf` | Output directory for `pikafish` and `pikafish.nnue`; use a temporary directory for non-destructive rebuild checks |

---

## What `build_pikafish.sh` Does

1. Downloads `pikafish.nnue` from the official networks release (once; skipped
   if already present at `bin/armhf/pikafish.nnue`).
2. Copies the Pikafish source to a temporary build directory.
3. Applies two source patches (see [ARM32 patches](#arm32-source-patches)
   below) via an embedded Python script.
4. Runs `make build ARCH=armv7 COMP=gcc` inside the builder container with
   static linking flags to eliminate GLIBC_2.29 dependencies.
5. Copies the resulting binary to `bin/armhf/pikafish`.

---

## ARM32 Source Patches

Pikafish uses a 128-bit integer bitboard type (`__uint128_t`) that GCC does not
provide on 32-bit ARM targets. Two source files require patches.

### Patch 1 — `types.h`: software `uint128_soft` type

`__uint128_t` is only available on x86-64 and AArch64. On ARM32, GCC defines no
`__SIZEOF_INT128__`. The patch inserts a `uint128_soft` struct that provides the
exact same interface, guarded by `#ifndef __SIZEOF_INT128__`, and conditionalizes
the `Bitboard` typedef:

```cpp
#ifdef __SIZEOF_INT128__
using Bitboard = __uint128_t;
#else
using Bitboard = uint128_soft;
#endif
```

**Non-obvious gotchas worked out during development:**

- **Not an aggregate** — the struct has user-declared constructors, so
  `{hi, lo}` brace-initialization inside Pikafish source fails unless you add
  a two-argument constructor: `constexpr uint128_soft(uint64_t h, uint64_t l)`.

- **`constexpr` compound assignments** — `operator|=`, `operator&=`, etc. must
  be marked `constexpr` because Pikafish's `bitboard.h` calls them from
  `constexpr` lambdas.

- **`explicit operator bool()` causes copy-init failures** — Pikafish writes
  `bool flag = some_bitboard;` (copy-initialization). An `explicit` conversion
  operator is not considered for copy-init. The operator must be
  **non-explicit**.

- **Ambiguity from non-explicit bool** — removing `explicit` opens an implicit
  conversion chain (`uint128_soft → bool → unsigned long long`) that makes
  `x >> unsigned_val` and `x - int_val` ambiguous. Fix: add exact-match
  overloads for `unsigned int` shifts and `int`/`unsigned int` subtraction.

### Patch 2 — `bitboard.h`: explicit casts in `lsb()`

`__builtin_ctzll` takes `unsigned long long`. When `Bitboard` is `uint128_soft`
rather than a built-in type, implicit conversion is not available at the call
site. The patch adds explicit `uint64_t()` casts:

```cpp
// before
return Square(__builtin_ctzll(b));
// after
return Square(__builtin_ctzll(uint64_t(b)));
```

---

## Static Linking and GLIBC Compatibility

### The problem

Pikafish calls `log`, `pow`, and `exp` from `<cmath>`. In Debian Bullseye's
`libm`, these resolve to `log@@GLIBC_2.29` (the default versioned symbol).
Kindles run older firmware with a glibc predating 2.29, so the dynamic linker
rejects the binary on startup.

```
# confirmed by running inside the container:
objdump -x pikafish | grep GLIBC_2.29
# shows:  log  pow  exp  all at GLIBC_2.29
```

### The fix

Pass static-linking flags so the linker embeds libm's math routines directly
into the binary instead of importing them as versioned dynamic symbols:

```makefile
EXTRALDFLAGS=-static-libstdc++ -static-libgcc -Wl,-Bstatic,-lm,-Bdynamic
```

Pikafish's `Makefile` appends `$(EXTRALDFLAGS)` to `LDFLAGS`, so this is the
correct extension point.

**After the fix** — `objdump` shows `e_log.o`, `e_pow.o`, `e_exp.o` baked into
the binary (static archive object files), and the maximum GLIBC version
required drops from 2.29 to 2.17, well within what any modern Kindle has.

### Shell quoting pitfall

The `EXTRALDFLAGS` value contains spaces. Inside a `docker exec ... /bin/bash -lc "..."` call, **double-quoting** the value splits it:

```bash
# WRONG — inner " terminates the outer string; only -static-libstdc++ is applied
docker exec "$CONTAINER" /bin/bash -lc "
    make build ARCH=$ARCH "EXTRALDFLAGS=-static-libstdc++ -static-libgcc ..."
"

# CORRECT — single quotes are safe inside a double-quoted outer string
docker exec "$CONTAINER" /bin/bash -lc "
    make build ARCH=$ARCH \
        'EXTRALDFLAGS=-static-libstdc++ -static-libgcc -Wl,-Bstatic,-lm,-Bdynamic'
"
```

Evidence that the wrong quoting was silently applied: `make` printed
`LDFLAGS: -static-libstdc++ -latomic ...` — only the first token before the
space was treated as part of the variable value; the rest became separate
`make` command-line arguments that were silently ignored.

---

## Runtime: Why Pikafish Must NOT Inherit `LD_LIBRARY_PATH`

The main `exact-chinesechess` app bundles all of its shared libraries
(`libgtk`, `libgdk_pixbuf`, `libc`, `libm`, etc.) from the Docker container
into `lib/armhf/` so it can run without those system packages. The launch
script sets `LD_LIBRARY_PATH` to include that directory.

**Pikafish must not inherit this `LD_LIBRARY_PATH`.**

Reason: the bundled `libm.so.6` is built against Debian Bullseye's glibc 2.31.
If the Kindle's `ld-linux-armhf.so.3` (the system dynamic linker, as recorded
in Pikafish's ELF header) loads the Docker libm alongside the Kindle's libc,
two incompatible versions of glibc exist in the same process — an instant
crash.

Pikafish after the static-linking fix only needs the Kindle's own `libc`,
`libpthread`, and `librt` (all requiring at most GLIBC_2.17). `pikafish_uci.c`
explicitly unsets `LD_LIBRARY_PATH` in the child environment before spawning
the engine:

```c
envp = g_get_environ();
envp = g_environ_unsetenv(envp, "LD_LIBRARY_PATH");
```

The working directory is also set to `bin/armhf/` (the directory containing
the binary) so Pikafish's NNUE file loader finds `pikafish.nnue` next to the
executable.

---

## Verifying the Build

To test the exact Kindle-specific Pikafish build without overwriting the
package's current working engine files, build into a temporary output directory:

```bash
mkdir -p _pikafish_verify_out
cp bin/armhf/pikafish.nnue _pikafish_verify_out/

EXACT_CHINESECHESS_PIKAFISH_OUT_DIR="$PWD/_pikafish_verify_out" \
  ./build_pikafish.sh
```

That uses the same source patches and compiler invocation, but writes the test
binary to `_pikafish_verify_out/pikafish` instead of `bin/armhf/pikafish`.

After `build_pikafish.sh` completes, check inside the container:

```bash
docker exec exact-chinesechess-armhf-builder /bin/bash -lc "
  echo '=== GLIBC versions required ==='
  objdump -x /src/exact-chinesechess/_pikafish_verify_out/pikafish \
    | grep GLIBC | sed 's/.*GLIBC_//;s/ .*//' | sort -V | uniq

  echo '=== max GLIBC version ==='
  objdump -x /src/exact-chinesechess/_pikafish_verify_out/pikafish \
    | grep GLIBC | sed 's/.*GLIBC_//;s/ .*//' | sort -V | tail -1

  echo '=== dynamic deps (should be only glibc basics) ==='
  ldd /src/exact-chinesechess/_pikafish_verify_out/pikafish

  echo '=== math functions statically linked (should show e_log.o etc.) ==='
  objdump -x /src/exact-chinesechess/_pikafish_verify_out/pikafish \
    | grep -E '^[0-9a-f]+ l.*\.(text|bss).*e_(log|pow|exp)' | head
"
```

**Expected results:**

- GLIBC versions: `2.4 2.7 2.12 2.16 2.17 PRIVATE` — no `2.29`.
- `ldd` shows only `libpthread`, `librt`, `libm`, `libc` — no `libstdc++`.
- `objdump` shows `e_log.o`, `e_pow.o`, `e_exp.o` as local object files.

---

## Why `ARCH=armv7` (not `armv7-neon`)

Pikafish's `Makefile` maps `armv7-neon` → `USE_NEON=7`. The NEON sparse-input
path in the NNUE code then uses `vaddvq_u16`, a horizontal-add intrinsic that
only exists on **ARMv8** (AArch32 mode), not the 32-bit ARMv7 profile found in
Kindles. Using `armv7` (no NEON) avoids these intrinsics entirely.

---

## File Locations After a Full Build

```
exact-chinesechess/
  bin/armhf/
    pikafish           # ARMv7 hard-float binary (~1.8 MB)
    pikafish.nnue      # NNUE network weights (~49 MB)
  release/
    exact-chinesechess-extension.zip
    SHA256SUMS
```

The extension zip installs on the Kindle as:

```
/mnt/us/extensions/exact-chinesechess/
  bin/armhf/pikafish
  bin/armhf/pikafish.nnue
  ...
```

---

## Summary of Lessons Learned

| Problem | Root cause | Solution |
|---|---|---|
| `__uint128_t` not on ARM32 | GCC only provides it for 64-bit targets | Software `uint128_soft` struct with identical interface |
| Brace-init `{hi, lo}` fails | Struct with constructors is not an aggregate | Add two-argument constructor |
| Non-`constexpr` compound ops | Called from `constexpr` bitboard lambdas | Mark all `operator|=` etc. `constexpr` |
| `explicit operator bool()` copy-init error | `bool flag = x` is copy-init; `explicit` forbidden | Remove `explicit` from `operator bool()` |
| Ambiguous `>>` and `-` after non-explicit bool | `uint128_soft → bool → ull` implicit chain | Add overloads for `unsigned int` and `int` operands |
| `__builtin_ctzll` rejects soft type | No implicit conversion from struct | Add `uint64_t()` cast at call sites in `lsb()` |
| `log/pow/exp` need GLIBC_2.29 | Bullseye libm's default symbol version | `-Wl,-Bstatic,-lm,-Bdynamic` statically embeds libm |
| Only first flag applied | Space in `EXTRALDFLAGS` inside double-quoted `docker exec` | Use single quotes inside the outer double-quoted string |
| Pikafish crashes immediately on Kindle | `LD_LIBRARY_PATH` pointed at Docker libs → glibc mixing | Unset `LD_LIBRARY_PATH` in child process env before spawn |
| NNUE not found | Working dir was parent app's dir, not binary dir | Set `working_directory = bin/armhf/` in `g_spawn_async_with_pipes` |
