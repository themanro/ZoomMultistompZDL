// Record the MultiStomp Editor (tools/editor.html) as PNG frames, for
// build/make_editor_preview.py. Loads the real page from the local server,
// fills it with an example bank and patch (no pedal: MIDI output is a stub
// that sends nothing), and steps the frame clock by hand so the effect
// pictures move a fixed step per frame.
//
//   node build/editor_capture.cjs <url> <out dir> <frames> <fps> <width> <height>
//
// Needs Playwright (see fx_capture.cjs) and the repo served over http, e.g.
// python3 -m http.server 8000 from the repo root.
"use strict";
const fs = require("fs");
const path = require("path");
const { chromium } = require("playwright");

const [url, out, frames, fps, width, height] = process.argv.slice(2);
const N = +frames, STEP = 1000 / +fps, WARM = 3;   // seconds run before recording

// The bank on the left, and the patch in the editor (index CUR).
const BANK = [
  "Clean Amp", "Night Ride", "Tape Haze", "Cathedral", "Chop Shop", "Bell Tower", "Glass Tide", "Dune Sub",
  "Frozen", "Skull Gig", "Lo-Fi Keys", "Sub Oct", "Dub Siren", "Swell", "Rust Fuzz", "Talkbox",
  "Spring", "Shimmer", "Reverse", "Glitch Kit", "Wah Steps", "VHS Ghost", "Big Sky", "Loop Jam",
  "Ring Mod", "Bit Storm", "Drum Bus", "Slow Rise", "Wavefold", "Ambient 1", "Ambient 2", "Scope",
  "Grain Fog", "Chorale", "Tremolo", "Long Hall", "Plate", "Octave Up", "Stutter", "Oxide",
  "Bass Grit", "Feedback", "Klang", "Spectrum", "Drum Echo", "Tonic", "Freeze", "Dry Amp",
  "Rehearsal", "Live Set",
];
const CUR = 6;
const CHAIN = ["Chopdly", "Deform", "Carillon", "Hydra", "Spool", "Horizon"];

(async () => {
  let browser;                                   // Playwright's own Chromium, else the installed Chrome
  try { browser = await chromium.launch(); } catch (err) { browser = await chromium.launch({ channel: "chrome" }); }
  const page = await browser.newPage({ viewport: { width: +width, height: +height }, deviceScaleFactor: 1, colorScheme: "dark" });
  await page.addInitScript(() => { try { localStorage.setItem("pe_fxview", "anim"); } catch (e) {} });
  await page.goto(url, { waitUntil: "load" });
  await page.waitForFunction(() => window.FxVis && document.getElementById("slots") && typeof render === "function");
  // Take over the frame clock once the page has loaded (it waits on real frames while it sets up).
  let now = await page.evaluate(() => {
    window.__cbs = []; let id = 0;
    window.requestAnimationFrame = cb => { window.__cbs.push(cb); return ++id; };
    return performance.now() + 1000;
  });
  await page.evaluate(({ BANK, CUR, CHAIN }) => {
    /* global library, curProg, patch, midiOut, PEDAL_ID, DB, emptyPatch, render, hasLocalEdits */
    const byName = n => DB.custom.find(e => e.name === n);
    midiOut = { state: "connected", send() {} };
    PEDAL_ID = { model: "MS-70CDR", fw: "2.10" };
    library = BANK.map(name => ({ ...emptyPatch(), name }));
    curProg = CUR;
    const fx = CHAIN.map(n => { const e = byName(n); const f = [1, e.id, ...e.params.map(p => p.default ?? 0)]; while (f.length < 11) f.push(0); return f; });
    patch = { name: BANK[CUR], fx, maxfx: 6, curfx: 0, dsp: 0 };
    hasLocalEdits = false;
    FxVis.setMotion(true);
    render();
    document.getElementById("noticeBar").hidden = true;
    const m = document.getElementById("mwModel"); if (m) m.textContent = "MS-70CDR · 2.10";
    document.activeElement && document.activeElement.blur();
  }, { BANK, CUR, CHAIN });
  await page.evaluate(() => document.fonts && document.fonts.ready);
  const step = async () => { now += STEP; await page.evaluate(t => { const c = window.__cbs; window.__cbs = []; c.forEach(f => f(t)); }, now); };
  for (let i = 0; i < WARM * 1000 / STEP; i++) await step();
  fs.mkdirSync(out, { recursive: true });
  for (let i = 0; i < N; i++) {
    await step();
    await page.screenshot({ path: path.join(out, String(i).padStart(3, "0") + ".png") });
  }
  console.log(`  editor: ${N} frames`);
  await browser.close();
})();
