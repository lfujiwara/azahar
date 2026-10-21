// Picks a ROM through the file input, then checks it boots from browser storage after a reload.
import { chromium } from "playwright";
const exe = `${process.env.HOME}/Library/Caches/ms-playwright/chromium_headless_shell-1223/chrome-headless-shell-mac-arm64/chrome-headless-shell`;
const browser = await chromium.launch({ headless: true, executablePath: process.env.CHROME || exe });
const page = await browser.newPage();
const lines = [];
page.on("console", (m) => lines.push(m.text()));
await page.goto(`http://127.0.0.1:${process.env.PORT || 8302}/index.html`);
await page.waitForSelector("#picker:not([hidden])");
await page.setInputFiles("#file", process.argv[2] || "roms/hello-world.3dsx");
await page.waitForURL(/rom=idb/);
await page.waitForTimeout(8000);
await page.locator("#canvas").screenshot({ path: "shots/picked.png" });
const booted = lines.some((l) => l.includes("Azahar Version"));
console.log(JSON.stringify({ url: page.url(), booted }));
await browser.close();
