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
RUNDIR="$GAMEDIR/exact_chinesechess"

mkdir -p "$CONFDIR"

cd "$RUNDIR" || exit 1

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

export XDG_DATA_HOME="$CONFDIR"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0
export LD_LIBRARY_PATH="$RUNDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"

BIN="$RUNDIR/exactcc.${DEVICE_ARCH}"

GPTK="$RUNDIR/exact_chinesechess.gptk"

if [ ! -x "$BIN" ]; then
  echo "Missing binary for DEVICE_ARCH=${DEVICE_ARCH}: $BIN"
  exit 1
fi

$GPTOKEYB "$(basename "$BIN")" -c "$GPTK" &
pm_platform_helper "$BIN"
"$BIN"

pm_finish
