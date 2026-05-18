#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"

[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"

get_controls

GAMEDIR=/$directory/ports/exact_chinesechess/
CONFDIR="$GAMEDIR/conf/"
BINDIR="$GAMEDIR/exact_chinesechess"
GPTOKEYB_PID=0
FRONTEND_KILLED=0
CLEANED_UP=0

mkdir -p "$CONFDIR"
cd "$BINDIR" || exit 1

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

restart_muos_frontend() {
  if [ "${CFW_NAME:-}" != "muOS" ]; then
    return
  fi

  echo "Restoring muOS frontend after SSH test launch."
  if command -v systemctl >/dev/null 2>&1; then
    systemctl restart oga_events >/dev/null 2>&1 &
  fi
  [ -w /dev/tty0 ] && printf "\033c" > /dev/tty0 || true
  [ -w /dev/tty1 ] && printf "\033c" > /dev/tty1 || true

  [ -e /run/muos/system/foreground_process ] && printf "muxlaunch" > /run/muos/system/foreground_process
  [ -e /opt/muos/config/system/foreground_process ] && printf "muxlaunch" > /opt/muos/config/system/foreground_process

  FRONTEND=""
  for candidate in \
    /opt/muos/script/mux/frontend.sh \
    "$(command -v frontend.sh 2>/dev/null)"
  do
    if [ -n "$candidate" ] && [ -x "$candidate" ]; then
      FRONTEND="$candidate"
      break
    fi
  done

  if [ -n "$FRONTEND" ]; then
    echo "Starting muOS frontend: $FRONTEND"
    nohup "$FRONTEND" >/dev/null 2>&1 &
  elif [ -x /opt/muos/extra/muxlaunch ]; then
    echo "Starting muOS muxlaunch fallback."
    LD_LIBRARY_PATH="/opt/muos/extra/lib:$LD_LIBRARY_PATH" nohup /opt/muos/extra/muxlaunch muxlaunch >/dev/null 2>&1 &
  elif command -v muxlaunch >/dev/null 2>&1; then
    nohup muxlaunch >/dev/null 2>&1 &
  else
    echo "Searching for muxlaunch/frontend.sh:"
    find /opt /usr /mnt/mmc /mnt/sdcard -maxdepth 5 \( -name muxlaunch -o -name frontend.sh \) 2>/dev/null | head -20 || true
    echo "Could not find muxlaunch/frontend.sh; run 'muxlaunch &' or reboot from SSH."
  fi
}

log_muos_processes() {
  if [ "${CFW_NAME:-}" = "muOS" ]; then
    echo "$1"
    ps | grep -E 'mux|frontend|oga_events|gptokeyb|exactcc' | grep -v grep || true
  fi
}

kill_muos_frontend_pids() {
  PIDS="$(
    ps w | awk '
      /\/opt\/muos\/extra\/mux/ ||
      /\/opt\/muos\/frontend\/mux/ ||
      /\/opt\/muos\/script\/mux\/frontend/ ||
      /frontend\.sh/ ||
      / mux[a-z]/ { print $1 }
    '
  )"
  if [ -n "$PIDS" ]; then
    echo "Killing muOS frontend PIDs: $PIDS"
    kill $PIDS 2>/dev/null || true
    sleep 1
    PIDS="$(
      ps w | awk '
        /\/opt\/muos\/extra\/mux/ ||
        /\/opt\/muos\/frontend\/mux/ ||
        /\/opt\/muos\/script\/mux\/frontend/ ||
        /frontend\.sh/ ||
        / mux[a-z]/ { print $1 }
      '
    )"
    if [ -n "$PIDS" ]; then
      echo "Force killing muOS frontend PIDs: $PIDS"
      kill -9 $PIDS 2>/dev/null || true
      sleep 1
    fi
  fi
}

stop_muos_frontend_for_ssh() {
  echo "Stopping muOS frontend for direct SSH testing."
  log_muos_processes "muOS processes before stop:"

  killall -q frontend.sh muxlaunch muxapp muxarchive muxassign muxbackup muxcharge muxcollect muxconfig muxconnect muxcontrol muxcredits muxcustom muxdownload muxgov muxhdmi muxhistory muxinfo muxkiosk muxlanguage muxnetinfo muxnetprofile muxnetscan muxnetwork muxoption muxpass muxpicker muxplore muxpower muxrtc muxsearch muxshare muxshot muxspace muxsplash muxstorage muxsysinfo muxtag muxtask muxtester muxtext muxthemedown muxthemefilter muxtimezone muxtweakadv muxtweakgen muxvisual muxwebserv 2>/dev/null || true

  if command -v pkill >/dev/null 2>&1; then
    pkill -x 'mux.*' 2>/dev/null || true
  fi
  kill_muos_frontend_pids
  log_muos_processes "muOS processes after stop:"
}

cleanup() {
  if [ "$CLEANED_UP" = "1" ]; then
    return
  fi
  CLEANED_UP=1
  if [ "$GPTOKEYB_PID" != "0" ]; then
    kill "$GPTOKEYB_PID" >/dev/null 2>&1 || true
    wait "$GPTOKEYB_PID" >/dev/null 2>&1 || true
  fi
  killall -q gptokeyb 2>/dev/null || true
  pm_finish
  if [ "$FRONTEND_KILLED" = "1" ]; then
    restart_muos_frontend
  fi
}

trap cleanup EXIT INT TERM

export XDG_DATA_HOME="$CONFDIR"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0

if [ -z "${SDL_RENDER_DRIVER:-}" ]; then
  export SDL_RENDER_DRIVER="software"
fi

if [ -z "${SDL_VIDEODRIVER:-}" ] && [ "${CFW_NAME:-}" = "muOS" ]; then
  export SDL_VIDEODRIVER="mali"
fi

if [ "$DEVICE_ARCH" = "armhf" ]; then
  export PORT_32BIT="Y"
fi

BIN="$BINDIR/exactcc.${DEVICE_ARCH}"
GPTK="$BINDIR/exact_chinesechess.gptk.${ANALOG_STICKS:-0}"

if [ ! -x "$BIN" ]; then
  echo "Missing binary for DEVICE_ARCH=${DEVICE_ARCH}: $BIN"
  echo "Available binaries:"
  ls -l "$BINDIR"/exactcc.* "$BINDIR"/exact_chinesechess.* 2>/dev/null || true
  exit 1
fi

if [ ! -f "$GPTK" ]; then
  GPTK="$BINDIR/exact_chinesechess.gptk.0"
fi

set_muos_foreground() {
  if [ "${CFW_NAME:-}" = "muOS" ]; then
    echo "Setting muOS foreground process to $(basename "$BIN")"
    [ -e /run/muos/system/foreground_process ] && printf "%s" "$(basename "$BIN")" > /run/muos/system/foreground_process
    [ -e /opt/muos/config/system/foreground_process ] && printf "%s" "$(basename "$BIN")" > /opt/muos/config/system/foreground_process
  fi
}

if [ -n "${ESUDO:-}" ]; then
  $ESUDO chmod 666 /dev/uinput >/dev/null 2>&1 || true
else
  chmod 666 /dev/uinput >/dev/null 2>&1 || true
fi

if [ "${EXACT_CC_KILL_FRONTEND:-0}" = "1" ]; then
  stop_muos_frontend_for_ssh
  FRONTEND_KILLED=1
fi

set_muos_foreground
if [ "${EXACT_CC_USE_GPTOKEYB:-0}" = "1" ]; then
  echo "Starting gptokeyb; set EXACT_CC_USE_GPTOKEYB=0 to use native SDL controller input."
  $GPTOKEYB "$(basename "$BIN")" -c "$GPTK" &
  GPTOKEYB_PID=$!
else
  echo "Using native SDL controller input; gptokeyb disabled to avoid uinput events leaking to muOS."
fi

pm_platform_helper "$BIN"
set_muos_foreground

if [ "${EXACT_CC_USE_BUNDLED_SDL:-0}" = "1" ]; then
  export LD_LIBRARY_PATH="$BINDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"
else
  echo "Using firmware SDL2; set EXACT_CC_USE_BUNDLED_SDL=1 to prefer bundled SDL2."
fi

echo "DEVICE_ARCH=$DEVICE_ARCH"
echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-}"
echo "SDL_RENDER_DRIVER=${SDL_RENDER_DRIVER:-}"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "Running $BIN"
echo "GPTK=$GPTK"

"$BIN"
