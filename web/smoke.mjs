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
  args: ["--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"],
});
const page = await browser.newPage({ viewport: { width: 800, height: 1000 } });
const lines = [];
page.on("console", (m) => lines.push(m.text()));
page.on("pageerror", (e) => lines.push("[pageerror] " + e.message));
const t0 = Date.now();
await page.goto(`http://127.0.0.1:8302/index.html?rom=${encodeURIComponent(rom)}&${extra}`);
await page.waitForTimeout(seconds * 1000);
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
const fps = await page.evaluate(() => window.azaharFps ?? null);
writeFileSync(`shots/${tag}.log`, lines.join("\n"));
console.log(JSON.stringify({ tag, ms: Date.now() - t0, ...stats, fps, logLines: lines.length }));
console.log(lines.slice(-25).join("\n"));
await browser.close();
