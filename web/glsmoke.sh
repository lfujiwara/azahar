#!/bin/sh
# usage: glsmoke.sh <rom-name> [api=1] [seconds=15] [extra settings]
# Runs smoke.mjs on port 8303 and prints the stats line plus distinct error lines.
cd "$(dirname "$0")"
rom=$1; api=${2:-1}; secs=${3:-15}; extra=$(printf "%s" "$4" | tr "," "\n" | sed "/^$/d; /\./!s/^/Renderer./; s/^/,/" | tr -d "\n")
tag=${TAG:-$rom-api$api}
TAG=$tag PORT=${PORT:-8303} node smoke.mjs roms/$rom.3dsx $secs "set=Renderer.graphics_api=$api$extra" | head -1
grep -E "Error|Critical|pageerror|WebGL:|Abort|ERROR:" shots/$tag.log | grep -v "Service\.\|WebSocket\|savedata" | sed 's/\x1b\[[0-9;]*m//g; s/^\[ *[0-9.]*\] //' | cut -c1-220 | sort | uniq -c | sort -rn | head -${N:-15}
