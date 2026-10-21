// Tiles the top screens (400x240) of several smoke screenshots side by side: node tile.mjs out.png a.png b.png ...
import { chromium } from "playwright";
import { readFileSync, writeFileSync } from "fs";
const [out, ...ins] = process.argv.slice(2);
const exe = `${process.env.HOME}/Library/Caches/ms-playwright/chromium_headless_shell-1223/chrome-headless-shell-mac-arm64/chrome-headless-shell`;
const b = await chromium.launch({ executablePath: process.env.CHROME || exe });
const p = await b.newPage();
const url = await p.evaluate(async (srcs) => {
  const c = document.createElement("canvas");
  c.width = 400 * srcs.length;
  c.height = 240;
  const g = c.getContext("2d");
  for (let i = 0; i < srcs.length; i++) {
    const im = new Image();
    im.src = srcs[i];
    await im.decode();
    g.drawImage(im, 0, 0, 400, 240, 400 * i, 0, 400, 240);
  }
  return c.toDataURL();
}, ins.map((f) => "data:image/png;base64," + readFileSync(f).toString("base64")));
writeFileSync(out, Buffer.from(url.split(",")[1], "base64"));
await b.close();
