#!/bin/sh
# Regression pass over every homebrew ROM plus the persistence and picker flows.
# usage: web/check.sh [set-overrides]   e.g. web/check.sh Renderer.graphics_api=1
cd "$(dirname "$0")"
SET=${1:-Renderer.graphics_api=0}
for rom in roms/*.3dsx; do
  TAG="check-$(basename "$rom" .3dsx)" node smoke.mjs "$rom" 8 "set=$SET" | head -1 |
    node -e 'const r=JSON.parse(require("fs").readFileSync(0));console.log([r.tag,"lit="+r.litFraction.toFixed(3),"colors="+r.colors,"fps="+(r.fps[0]??0).toFixed(1),"speed="+(r.fps[1]??0).toFixed(0)+"%","audioPeak="+(r.audio?.peak??0).toFixed(2)].join("\t"))'
done
node persist.mjs
node pick.mjs
