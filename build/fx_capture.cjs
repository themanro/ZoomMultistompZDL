// Record the editor's effect pictures (tools/fx_visuals.js) as PNG frames, for
// build/make_fx_previews.py. Runs the real scene code in headless Chromium with
// the frame clock driven by hand, so every frame is a fixed step apart and the
// result is the same each run.
//
//   node build/fx_capture.cjs <out dir> <frames> <fps> <width> Name1 Name2 ...
//
// Needs Playwright (npm i playwright, then npx playwright install chromium);
// point NODE_PATH at its node_modules if it is installed elsewhere.
"use strict";
const fs = require("fs");
const path = require("path");
const { chromium } = require("playwright");

const ROOT = path.resolve(__dirname, "..");
const [out, frames, fps, width, ...names] = process.argv.slice(2);
const N = +frames, STEP = 1000 / +fps, W = +width, WARM = 3;   // seconds run before recording

(async () => {
  const db = JSON.parse(fs.readFileSync(path.join(ROOT, "tools/effects_db.json"), "utf8"));
  const all = [...db.custom, ...db.stock];
  let browser;                                   // Playwright's own Chromium, else the installed Chrome
  try { browser = await chromium.launch(); } catch (err) { browser = await chromium.launch({ channel: "chrome" }); }
  const page = await browser.newPage({ viewport: { width: W + 40, height: W }, deviceScaleFactor: 1 });
  await page.setContent(`<!doctype html><html><head><style>
    :root{--screen-bg:#111114;--screen-fg:#dfe2ff}
    body{margin:0;background:#111114} #box{width:${W}px} canvas{width:100%;display:block}
  </style></head><body><div id="box"><canvas id="cv"></canvas></div></body></html>`);
  await page.evaluate(() => { window.__cb = null; window.requestAnimationFrame = cb => { window.__cb = cb; return 1; }; });
  await page.addScriptTag({ content: fs.readFileSync(path.join(ROOT, "tools/fx_visuals.js"), "utf8") });
  for (const name of names) {
    const e = all.find(x => x.name === name);
    if (!e) { console.error(`${name}: not in tools/effects_db.json`); process.exitCode = 1; continue; }
    const dir = path.join(out, name);
    fs.mkdirSync(dir, { recursive: true });
    const shots = await page.evaluate(async ({ e, N, STEP, WARM }) => {
      const box = document.getElementById("box");
      box.innerHTML = '<canvas id="cv"></canvas>';
      const cv = document.getElementById("cv");
      const vals = [1, e.id, ...(e.params || []).map(p => p.default ?? 0)];
      FxVis.setMotion(true);
      FxVis.attach(cv, e, () => vals);
      let now = (window.__now || 1000);
      const step = () => { now += STEP; const cb = window.__cb; window.__cb = null; if (cb) cb(now); };
      for (let i = 0; i < WARM * 1000 / STEP; i++) step();
      const out = [];
      for (let i = 0; i < N; i++) { step(); out.push(cv.toDataURL("image/png")); }
      window.__now = now + 1000;
      return out;
    }, { e, N, STEP, WARM });
    shots.forEach((d, i) => fs.writeFileSync(path.join(dir, String(i).padStart(3, "0") + ".png"), Buffer.from(d.split(",")[1], "base64")));
    console.log(`  ${name}: ${shots.length} frames`);
  }
  await browser.close();
})();
