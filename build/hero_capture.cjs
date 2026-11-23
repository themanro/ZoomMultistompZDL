// Record the README's console picture (build/hero/paint.html) as PNG frames, for
// build/make_paint_hero.py. Serves the repo on a local port, runs the page in headless
// Chromium with its clock driven by hand (like fx_capture.cjs), and writes one
// loop: frame i shows the console at i / fps seconds.
//
//   node build/hero_capture.cjs <out dir> <fps> [--page <page in build/hero/>] [only frame numbers...]
//
// Needs Playwright (npm i playwright); point NODE_PATH at its node_modules if
// it is installed elsewhere.
"use strict";
const fs = require("fs");
const path = require("path");
const http = require("http");
const { chromium } = require("playwright");

const ROOT = path.resolve(__dirname, "..");
const args = process.argv.slice(2);
let PAGE = "paint.html";
{ const i = args.indexOf("--page"); if (i >= 0) { PAGE = args[i + 1]; args.splice(i, 2); } }
const [out, fpsArg, ...only] = args;
const FPS = +fpsArg, STEP = 1000 / FPS, WARM = 3, BATCH = 12;
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".json": "application/json" };

const server = http.createServer((req, res) => {
  const p = path.join(ROOT, decodeURIComponent(new URL(req.url, "http://x").pathname));
  if (!p.startsWith(ROOT) || !fs.existsSync(p) || fs.statSync(p).isDirectory()) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { "Content-Type": TYPES[path.extname(p)] || "application/octet-stream" });
  fs.createReadStream(p).pipe(res);
});

(async () => {
  await new Promise(r => server.listen(0, "127.0.0.1", r));
  const port = server.address().port;
  let browser;
  try { browser = await chromium.launch(); } catch (err) { browser = await chromium.launch({ channel: "chrome" }); }
  const page = await browser.newPage({ viewport: { width: 1600, height: 800 }, deviceScaleFactor: 1 });
  await page.addInitScript(() => {
    window.__CAPTURE = true; window.__cbs = [];
    window.requestAnimationFrame = cb => { window.__cbs.push(cb); return 1; };
  });
  await page.goto(`http://127.0.0.1:${port}/build/hero/${PAGE}`);
  const { LOOP } = await page.evaluate(() => window.heroReady);
  const size = await page.evaluate(() => { const c = document.getElementById("stage"); return { width: c.width, height: c.height }; });
  await page.setViewportSize(size);
  const N = Math.round(LOOP * FPS);
  fs.mkdirSync(out, { recursive: true });
  // The effect pictures run on their own clock: warm them up, then step them
  // with the console, one frame at a time.
  await page.evaluate(({ STEP, WARM }) => {
    window.__now = 1000;
    window.__step = () => { window.__now += STEP; const cbs = window.__cbs; window.__cbs = []; cbs.forEach(cb => cb(window.__now)); };
    for (let i = 0; i < WARM * 1000 / STEP; i++) window.__step();
  }, { STEP, WARM });
  const want = only.length ? new Set(only.map(Number)) : null;
  for (let i0 = 0; i0 < N; i0 += BATCH) {
    const shots = await page.evaluate(({ i0, n, STEP, want }) => {
      const res = [];
      for (let i = i0; i < i0 + n; i++) {
        window.__step();
        if (want && !want.includes(i)) continue;
        window.frameAt(i * STEP / 1000);
        res.push([i, document.getElementById("stage").toDataURL("image/png")]);
      }
      return res;
    }, { i0, n: Math.min(BATCH, N - i0), STEP, want: want && [...want] });
    for (const [i, d] of shots) fs.writeFileSync(path.join(out, String(i).padStart(4, "0") + ".png"), Buffer.from(d.split(",")[1], "base64"));
  }
  console.log(`  ${want ? want.size : N} frames of ${N} (${LOOP} s at ${FPS} fps)`);
  await browser.close();
  server.close();
})().catch(err => { console.error(err); process.exit(1); });
