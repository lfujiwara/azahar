#!/bin/sh
# Regression pass over every homebrew ROM plus the persistence and picker flows.
# usage: web/check.sh [set-overrides]   e.g. web/check.sh Renderer.graphics_api=0 (software)
cd "$(dirname "$0")"
SET=${1:-Renderer.graphics_api=1}
for rom in roms/*.3dsx; do
  TAG="check-$(basename "$rom" .3dsx)" node smoke.mjs "$rom" 8 "set=$SET" | head -1 |
    node -e 'const r=JSON.parse(require("fs").readFileSync(0));console.log([r.tag,"lit="+r.litFraction.toFixed(3),"colors="+r.colors,"fps="+(r.fps[0]??0).toFixed(1),"speed="+(r.fps[1]??0).toFixed(0)+"%","audioPeak="+(r.audio?.peak??0).toFixed(2)].join("\t"))'
done
# Open-source games (roms/games, roms/sdmc; see NOTES.md for sources)
game() {
  TAG="check-$1" node smoke.mjs "$2" "$3" "$4" | head -1 |
    node -e 'const r=JSON.parse(require("fs").readFileSync(0));console.log([r.tag,"lit="+r.litFraction.toFixed(3),"colors="+r.colors,"speed="+(r.fps[1]??0).toFixed(0)+"%","tick="+(r.tickMs??0).toFixed(1)+"ms","audioPeak="+(r.audio?.peak??0).toFixed(2)].join("\t"))'
}
if [ -d games ]; then
  SEQ=8000:KeyA:150,11000:KeyA:150 game haxagon games/SuperHaxagon.3dsx 16 "set=$SET"
  SEQ=8000:KeyA:150,11000:KeyA:150 game haxagon-cia games/SuperHaxagon.cia 16 "set=$SET"
  game quake games/ctrQuake.3dsx 25 "set=$SET&sdmc=3ds/ctrQuake/id1/pak0.pak,3ds/ctrQuake/keyboardOverlay.bin,3ds/ctrQuake/touchOverlay.bin"
  game hbmenu games/boot.3dsx 12 "set=$SET&sdmc=3ds/ctrQuake/ctrQuake.3dsx"
fi
node persist.mjs
node pick.mjs
