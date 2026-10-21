// Headless smoke test: load a ROM, wait, dump log + canvas screenshot + pixel stats.
// usage: node smoke.mjs <rom> [seconds] [extra query string]
import { chromium } from "playwright";
import { mkdirSync, writeFileSync } from "fs";
import { basename } from "path";

const rom = process.argv[2] || "roms/hello-world.3dsx";
const seconds = Number(process.argv[3] || 15);
const extra = process.argv[4] || "";
const tag = process.env.TAG || basename(rom).replace(/\.[^.]+$/, "");
mkdirSync("shots", { recursive: true });

const browser = await chromium.launch({
  headless: true,
  executablePath:
    process.env.CHROME ||
    `${process.env.HOME}/Library/Caches/ms-playwright/chromium_headless_shell-1223/chrome-headless-shell-mac-arm64/chrome-headless-shell`,
  args: ["--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist", "--autoplay-policy=no-user-gesture-required"],
});
const page = await browser.newPage({ viewport: { width: 800, height: 1000 } });
const lines = [];
page.on("console", (m) => lines.push(m.text()));
page.on("pageerror", (e) => lines.push("[pageerror] " + e.message + "\n" + (e.stack || "").split("\n").slice(0, 12).join("\n")));
const t0 = Date.now();
await page.goto(`http://127.0.0.1:${process.env.PORT || 8302}/index.html?rom=${encodeURIComponent(rom)}&${extra}`);
// HOLD=KeyA,KeyM: hold these keys for the second half of the run (screenshot taken while held)
const hold = (process.env.HOLD || "").split(",").filter(Boolean);
await page.waitForTimeout((hold.length || process.env.TOUCH ? seconds / 2 : seconds) * 1000);
// TOUCH=x,y: hold the mouse down at canvas pixel (x, y) for the second half of the run
const touch = process.env.TOUCH?.split(",").map(Number);
if (touch && !hold.length) {
  await page.waitForTimeout((seconds / 2) * 1000);
  const box = await page.locator("#canvas").boundingBox();
  await page.mouse.move(box.x + touch[0], box.y + touch[1]);
  await page.mouse.down();
  await page.waitForTimeout((seconds / 2) * 1000);
}
if (hold.length) {
  await page.locator("#canvas").focus();
  for (const k of hold) await page.keyboard.down(k);
  await page.waitForTimeout((seconds / 2) * 1000);
}
// Tick probe: median wall ms per main-loop tick over 2 s (budget is ~16.7 ms at 60 Hz).
const tickMs = await page.evaluate(async () => {
  const xs = [];
  const id = setInterval(() => globalThis.azaharTickMs !== undefined && xs.push(globalThis.azaharTickMs), 7);
  await new Promise((r) => setTimeout(r, 2000));
  clearInterval(id);
  xs.sort((a, b) => a - b);
  return xs.length ? xs[xs.length >> 1] : null;
});
// Audio probe: wrap SDL's ScriptProcessor callback for 2 s and measure what it outputs.
const audio = await page.evaluate(async () => {
  const n = window.Module?.SDL2?.audio?.scriptProcessorNode;
  if (!n) return null;
  const fn = n.onaudioprocess;
  const a = { state: Module.SDL2.audioContext.state, blocks: 0, loud: 0, peak: 0 };
  n.onaudioprocess = (e) => {
    fn(e);
    a.blocks++;
    let m = 0;
    for (const v of e.outputBuffer.getChannelData(0)) m = Math.max(m, Math.abs(v));
    if (m > 0.01) a.loud++;
    a.peak = Math.max(a.peak, m);
  };
  await new Promise((r) => setTimeout(r, 2000));
  n.onaudioprocess = fn;
  return a;
});
const canvas = page.locator("#canvas");
const png = await canvas.screenshot();
writeFileSync(`shots/${tag}.png`, png);
const stats = await page.evaluate(async (b64) => {
  const img = new Image();
  img.src = "data:image/png;base64," + b64;
  await img.decode();
  const c = new OffscreenCanvas(img.width, img.height);
  const g = c.getContext("2d");
  g.drawImage(img, 0, 0);
  const d = g.getImageData(0, 0, img.width, img.height).data;
  let lit = 0;
  const colors = new Set();
  for (let i = 0; i < d.length; i += 4) {
    if (d[i] + d[i + 1] + d[i + 2] > 30) lit++;
    if (colors.size < 5000) colors.add((d[i] << 16) | (d[i + 1] << 8) | d[i + 2]);
  }
  return { w: img.width, h: img.height, litFraction: lit / (d.length / 4), colors: colors.size };
}, png.toString("base64"));
const fps = await page.evaluate(() => [window.azaharFps ?? null, window.azaharSpeed ?? null]);
writeFileSync(`shots/${tag}.log`, lines.join("\n"));
console.log(JSON.stringify({ tag, ms: Date.now() - t0, ...stats, fps, tickMs, audio, logLines: lines.length }));
console.log(lines.slice(-25).join("\n"));
await browser.close();
