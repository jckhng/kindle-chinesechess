#!/bin/sh

EXT_DIR="/mnt/us/extensions/kindle-chinesechess"
APP_BIN="$EXT_DIR/bin/armhf/kindle-chinesechess"
APP_LOG="/mnt/us/kindle-chinesechess.log"
APP_TITLE="Kindle ChineseChess"
APP_LOADER="$EXT_DIR/lib/armhf/ld-linux-armhf.so.3"
RUNTIME_MODE="${KINDLE_CHINESECHESS_RUNTIME:-auto}"

if [ ! -x "$APP_BIN" ]; then
    echo "$APP_TITLE binary not found: $APP_BIN" >"$APP_LOG"
    exit 1
fi

if [ "$1" = "--restart" ]; then
    pkill -f "$APP_BIN" 2>/dev/null
    pkill -f "$APP_LOADER.*$APP_BIN" 2>/dev/null
    sleep 1
fi

if pgrep -f "$APP_BIN" >/dev/null 2>&1; then
    exit 0
fi

LIB_PATHS=""
for dir in "$EXT_DIR/lib/armhf" "$EXT_DIR/bin/armhf"
do
    if [ -d "$dir" ]; then
        if [ -n "$LIB_PATHS" ]; then
            LIB_PATHS="$LIB_PATHS:$dir"
        else
            LIB_PATHS="$dir"
        fi
    fi
done

cd "$EXT_DIR" || exit 1

# Set up GDK pixbuf PNG loader so piece images render as graphics, not text.
# Generate the loaders.cache at launch time with the Kindle-local absolute path.
PIXBUF_LOADERS_DIR="$EXT_DIR/lib/armhf/gdk-pixbuf-loaders"
PIXBUF_CACHE="$PIXBUF_LOADERS_DIR/loaders.cache"
PNG_LOADER="$PIXBUF_LOADERS_DIR/libpixbufloader-png.so"
if [ -f "$PNG_LOADER" ]; then
    if [ ! -f "$PIXBUF_CACHE" ]; then
        printf '"%s"\n"png" 5 "gdk-pixbuf" "PNG" "LGPL"\n"image/png" ""\n"png" ""\n"\\211PNG\\r\\n\\032\\n" "" 100\n\n' \
            "$PNG_LOADER" > "$PIXBUF_CACHE"
    fi
    export GDK_PIXBUF_MODULE_FILE="$PIXBUF_CACHE"
fi

{
    echo "----- $APP_TITLE launch $(date) -----"
    echo "app=$APP_BIN"
    echo "runtime_mode=$RUNTIME_MODE"
    echo "lib_paths=$LIB_PATHS"
    echo "pixbuf_cache=$GDK_PIXBUF_MODULE_FILE"
} >>"$APP_LOG"

if [ -f "$APP_LOADER" ] && [ ! -x "$APP_LOADER" ]; then
    chmod 755 "$APP_LOADER" 2>/dev/null || true
fi

try_launch() {
    mode="$1"
    shift

    echo "Trying runtime: $mode" >>"$APP_LOG"
    DISPLAY=:0 GDK_NATIVE_WINDOWS=1 "$@" >>"$APP_LOG" 2>&1 &
    pid="$!"
    sleep 1

    if kill -0 "$pid" 2>/dev/null || pgrep -f "$APP_BIN" >/dev/null 2>&1; then
        echo "Started runtime: $mode pid=$pid" >>"$APP_LOG"
        if command -v xwininfo >/dev/null 2>&1; then
            DISPLAY=:0 xwininfo -root -tree 2>/dev/null | grep -i "chinesechess\\|kindlechinesechess\\|kindle" >>"$APP_LOG" 2>&1 || true
        fi
        exit 0
    fi

    wait "$pid" 2>/dev/null
    code="$?"
    echo "Runtime failed: $mode exit=$code" >>"$APP_LOG"
}

case "$RUNTIME_MODE" in
    direct)
        try_launch direct "$APP_BIN"
        ;;
    loader)
        if [ -x "$APP_LOADER" ]; then
            try_launch loader "$APP_LOADER" --library-path "$LIB_PATHS" "$APP_BIN"
        fi
        ;;
    ldpath)
        LD_LIBRARY_PATH="${LIB_PATHS}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" try_launch ldpath "$APP_BIN"
        ;;
    auto|*)
        try_launch direct "$APP_BIN"
        if [ -x "$APP_LOADER" ]; then
            try_launch loader "$APP_LOADER" --library-path "$LIB_PATHS" "$APP_BIN"
        fi
        LD_LIBRARY_PATH="${LIB_PATHS}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" try_launch ldpath "$APP_BIN"
        ;;
esac

echo "All launch methods failed." >>"$APP_LOG"
exit 1
