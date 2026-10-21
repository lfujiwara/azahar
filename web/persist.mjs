// Boots twice in one browser profile; the second boot must see files the first one wrote.
import { chromium } from "playwright";
const exe = `${process.env.HOME}/Library/Caches/ms-playwright/chromium_headless_shell-1223/chrome-headless-shell-mac-arm64/chrome-headless-shell`;
const browser = await chromium.launch({ headless: true, executablePath: process.env.CHROME || exe });
const ctx = await browser.newContext();
const url = `http://127.0.0.1:${process.env.PORT || 8302}/index.html?rom=roms/hello-world.3dsx&set=Renderer.graphics_api=0`;
const persisted = async () => {
  const page = await ctx.newPage();
  const lines = [];
  page.on("console", (m) => lines.push(m.text()));
  await page.goto(url);
  await page.waitForTimeout(6000);
  await page.evaluate(() => window.azaharSync());
  await page.close();
  return Number(lines.find((l) => l.startsWith("[web] persisted files:")).split(":")[1]);
};
const first = await persisted();
const second = await persisted();
console.log(JSON.stringify({ first, second, ok: first === 0 && second > 0 }));
await browser.close();
