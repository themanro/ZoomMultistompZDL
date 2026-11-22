/* fx_visuals.js -- an animated picture per effect in the Patch Editor's slots.
 *
 * Each slot shows a small scene for its effect that moves on its own and
 * reshapes as its knobs turn (from the editor or from the pedal: it reads the
 * slot's values every frame and glides to them). Scenes are shared by kind
 * (waveform, bars, hatched drive, delay rings / reverb box, modulation waves,
 * filter curve, steps, pitch lines, dynamics curve); each of this project's
 * effects picks its scene and wires its own knobs to it (FX below), Zoom's
 * own effects get one by category. No audio is involved: the editor never
 * hears the pedal, so this is the knobs, drawn.
 *
 *   FxVis.attach(canvas, effect, () => slotValues)   effect: a DB entry
 *
 * One animation loop for all slots, up to 60 frames a second, only for
 * canvases on screen, and none for a slot that is off. When the system asks for reduced motion the scenes hold
 * still and only follow the knobs, until the Play button (setMotion) says to
 * move anyway; that choice is remembered and wins either way.
 */
(function (root) {
  "use strict";
  const LW = 256, LH = 128;                 // logical drawing size (2:1, as the cover)
  const views = new Set();
  let raf = 0, last = 0;
  const osMotion = () => !(root.matchMedia && root.matchMedia("(prefers-reduced-motion: reduce)").matches);
  let userMotion = null;                    // "on" / "off": the Play/Pause choice, over the system's
  try { userMotion = root.localStorage.getItem("pe_fxmotion"); } catch (e) {}
  const motion = () => userMotion === "on" ? true : userMotion === "off" ? false : osMotion();
  function setMotion(on) {
    userMotion = on ? "on" : "off";
    try { root.localStorage.setItem("pe_fxmotion", userMotion); } catch (e) {}
  }

  function colours() {
    const cs = getComputedStyle(document.documentElement);
    const v = (n, d) => (cs.getPropertyValue(n) || "").trim() || d;
    return { bg: v("--screen-bg", "#101218"), fg: v("--screen-fg", "#e8e8e8") };
  }

  // Seeded random: the same effect draws the same shapes every time.
  function rng(seed) {
    let a = (seed >>> 0) || 1;
    return () => { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a);
      t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
  }
  const clamp = (x, a = 0, b = 1) => x < a ? a : x > b ? b : x;
  const TAU = Math.PI * 2;

  /* ---- knob access: by name, smoothed so changes glide ---------------- */
  function knobs(view) {
    const ps = view.e.params || [], f = view.get() || [];
    if (!view.sm) view.sm = ps.map((p, k) => clamp((f[k + 2] ?? p.default ?? 0) / (p.max || 100)));
    ps.forEach((p, k) => { const t = clamp((f[k + 2] ?? 0) / (p.max || 100)); view.sm[k] += (t - view.sm[k]) * 0.22; });
    const find = re => { const key = re.source; if (!(key in view.ix)) view.ix[key] = ps.findIndex(p => re.test(p.name)); return view.ix[key]; };
    const q = (re, d) => { const i = find(re); return i < 0 ? d : view.sm[i]; };
    q.raw = (re, d) => { const i = find(re); return i < 0 ? d : (f[i + 2] | 0); };
    q.on = !!f[0];
    return q;
  }
  const R = {                               // knob roles, by name
    drive: /^(drive|gain|input|annihil|dust|power|amount|amnt)$/i,
    crush: /^(crush|bits|loss)$/i, comp: /^(comp|compress)$/i,
    tilt: /^(tilt|tone|treb|bright|filter|filt)$/i, noise: /^(noise|hiss|znr)$/i,
    mix: /^(mix|wet|level|output|vol)$/i, time: /^(time|dly1|div|length|size)$/i,
    fb: /^(feedbk|fdbk|feed|regen|fb|tail)$/i, decay: /^(decay|fall|blur)$/i,
    rate: /^(rate|speed|pace|spin|motor|wow|flutter|freq)$/i, depth: /^(depth|detune|sweep|span|choir)$/i,
    size: /^(size|bigness|room)$/i, cut: /^(cutoff|cntr|cut|freq|tune|midf)$/i, reso: /^(reso|q)$/i,
  };

  /* ---- shared drawing bits ------------------------------------------- */
  function waveTable(r, n) {                 // a loop of decaying hits, per column (top, bottom)
    const top = new Float32Array(n), bot = new Float32Array(n);
    let x = 0;
    while (x < n) {
      const len = 10 + r() * 26, amp = 0.55 + r() * 0.45, gap = 14 + r() * 26;
      for (let i = 0; i < gap && x < n; i++, x++) {
        const e = amp * Math.exp(-i / len) + 0.04;
        top[x] = e * (0.55 + 0.45 * r()); bot[x] = e * (0.55 + 0.45 * r());
      }
    }
    return { top, bot, n };
  }
  function drawWave(g, wt, x0, x1, cy, h, off, dens, rev) {
    for (let x = Math.ceil(x0); x < x1; x++) {
      let u = (x - x0) * dens + off; if (rev) u = (x1 - x) * dens + off;
      const i = ((Math.floor(u) % wt.n) + wt.n) % wt.n;
      g.moveTo(x + 0.5, cy - wt.top[i] * h); g.lineTo(x + 0.5, cy + wt.bot[i] * h);
    }
  }
  function polyBlob(cx, cy, rx, ry, n, pts) {  // superellipse outline
    const out = [];
    const e = 2 / n;
    for (let k = 0; k < 96; k++) {
      const a = k / 96 * TAU, c = Math.cos(a), s = Math.sin(a);
      out.push([cx + rx * Math.sign(c) * Math.abs(c) ** e, cy + ry * Math.sign(s) * Math.abs(s) ** e]);
    }
    return pts ? pts(out) : out;
  }
  function pathOf(g, pts) { g.beginPath(); pts.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.closePath(); }
  function hatch(g, angle, gap, off, jit, r) {  // parallel lines over the whole canvas (clip first)
    const c = Math.cos(angle), s = Math.sin(angle), L = 320;
    g.beginPath();
    for (let d = -L + (off % gap); d < L; d += gap) {
      const j = jit ? (r() - 0.5) * jit : 0;
      const mx = LW / 2 - s * (d + j), my = LH / 2 + c * (d + j);
      g.moveTo(mx - c * L, my - s * L); g.lineTo(mx + c * L, my + s * L);
    }
    g.stroke();
  }


  /* ---- pixel scenes: a noise texture and colours as numbers ------------ */
  const NS = 256;
  let NOISE = null;
  function noiseTex() {                      // tileable value noise, equalised to 0..1, made once
    if (NOISE) return NOISE;
    const r = rng(0x5eed), raw = new Float32Array(NS * NS);
    for (const [cell, amp] of [[32, 0.5], [16, 0.3], [8, 0.2]]) {
      const n = NS / cell, grid = Float32Array.from({ length: n * n }, () => r());
      const at = (x, y) => grid[(y % n) * n + (x % n)];
      for (let y = 0; y < NS; y++) for (let x = 0; x < NS; x++) {
        const gx = x / cell, gy = y / cell, x0 = gx | 0, y0 = gy | 0, fx = gx - x0, fy = gy - y0;
        const sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
        const a = at(x0, y0), b = at(x0 + 1, y0), c = at(x0, y0 + 1), d = at(x0 + 1, y0 + 1);
        raw[y * NS + x] += amp * (a + (b - a) * sx + (c - a) * sy + (a - b - c + d) * sx * sy);
      }
    }
    const order = Array.from(raw.keys()).sort((i, j) => raw[i] - raw[j]);
    NOISE = new Float32Array(NS * NS);
    order.forEach((i, k) => { NOISE[i] = k / (NS * NS); });   // flat: a threshold is a share
    return NOISE;
  }
  const rgbCache = new Map();
  function rgbOf(css) {
    if (rgbCache.has(css)) return rgbCache.get(css);
    const c = document.createElement("canvas"); c.width = c.height = 1;
    const x = c.getContext("2d"); x.fillStyle = css; x.fillRect(0, 0, 1, 1);
    const d = x.getImageData(0, 0, 1, 1).data, out = [d[0], d[1], d[2]];
    rgbCache.set(css, out); return out;
  }
  const pack = ([r, g, b]) => (0xff000000 | (b << 16) | (g << 8) | r) >>> 0;   // ImageData, little-endian
  const lerp3 = (a, b, k) => [0, 1, 2].map(i => Math.round(a[i] + (b[i] - a[i]) * k));
  function vivid([r, g, b]) {                // the same hue, more colour and a little darker
    const mx = Math.max(r, g, b), mn = Math.min(r, g, b), l = (mx + mn) / 510;
    if (mx === mn) return [r, g, b];
    const L = Math.min(l, 0.7), S = 0.85, d = mx - mn;
    let h = mx === r ? ((g - b) / d) % 6 : mx === g ? (b - r) / d + 2 : (r - g) / d + 4; h = (h * 60 + 360) % 360;
    const c = (1 - Math.abs(2 * L - 1)) * S, x = c * (1 - Math.abs((h / 60) % 2 - 1)), m = L - c / 2;
    const [a, bb, cc] = h < 60 ? [c, x, 0] : h < 120 ? [x, c, 0] : h < 180 ? [0, c, x] : h < 240 ? [0, x, c] : h < 300 ? [x, 0, c] : [c, 0, x];
    return [a, bb, cc].map(v => Math.round((v + m) * 255));
  }
  // Two colours, as Deform has them: the screen colour made vivid, and a pale one.
  const tones = C => { const bg = rgbOf(C.bg), fg = rgbOf(C.fg); return { bg, fg, hi: vivid(fg), lo: lerp3(fg, [255, 255, 255], 0.55) }; };
  const css = c => `rgb(${c.join(",")})`;

  // Chimera's vowel shapes (src/custom/chimera/chimera_vowels.h): log area per section, u o a e i
  const THROAT_VOWELS = [[0.470, 1.669, 1.776, 1.540, 0.997, 0.510, 0.489, 1.038, 1.843, 2.412, 2.465, 2.147, 1.879, 1.983, 2.405, -0.706],
      [-0.577, -0.241, 0.054, -0.082, -0.652, -1.208, -1.203, -0.449, 0.709, 1.659, 2.010, 1.877, 1.728, 1.946, 2.485, 0.358],
      [0.071, 0.021, -0.064, -0.155, -0.209, -0.172, -0.003, 0.315, 0.760, 1.271, 1.772, 2.189, 2.483, 2.485, 2.485, 1.590],
      [-0.605, -0.380, 0.005, 0.442, 0.814, 1.032, 1.057, 0.916, 0.683, 0.458, 0.331, 0.355, 0.524, 0.780, 1.029, 1.122],
      [-0.980, -0.531, 0.143, 0.729, 0.996, 0.893, 0.514, -0.002, -0.555, -1.079, -1.504, -1.730, -1.674, -1.358, -0.937, 0.674]];

  // A pixel canvas for the scenes: shapes into a mask (bits), then coloured
  // by the scene's own renderer (shade: Deform's hatch; cells: a dot matrix;
  // flat: plain fills and rings) and blown up.
  //   bit 1: pale, hatched the other way round (paths, scenery)   bit 2: the colour, hatched (what is
  //   sounding now)   bit 4: the colour, dimmer, hatched (what has sounded)
  const N = (a, b) => noiseTex()[((b | 0) & 255) * NS + ((a | 0) & 255)];
  function pixels(v) {
    const s = v.s, cv = v.canvas;
    const RW = Math.max(200, Math.min(480, Math.round(cv.width))) & ~1, RH = RW >> 1, k = RW / LW;
    if (!s.px || s.px.RW !== RW) {
      const off = document.createElement("canvas"); off.width = RW; off.height = RH;
      const ctx = off.getContext("2d"), img = ctx.createImageData(RW, RH);
      s.px = { RW, RH, k, off, ctx, img, out: new Uint32Array(img.data.buffer), M: new Uint8Array(RW * RH), seed: 0 };
    }
    const P = s.px, M = P.M, NT = noiseTex();
    P.disc = (cx, cy, rx, ry, bit, rough = 1, dome = false) => {   // an ellipse (dome: its top half), edges roughened by noise
      if (rx <= 0 || ry <= 0) return;
      const sd = (P.seed = (P.seed + 37) & 255), rr = rough * 0.9 * k;
      const y0 = Math.max(0, Math.floor((cy - ry) * k)), y1 = Math.min(RH, Math.ceil((dome ? cy : cy + ry) * k));
      for (let y = y0; y < y1; y++) {
        const dy = (y / k - cy) / ry; if (dy <= -1 || dy >= 1) continue;
        const half = rx * Math.sqrt(1 - dy * dy) * k;
        const nA = NT[((y * 2) & 255) * NS + sd] - 0.5, nB = NT[((y * 2 + 97) & 255) * NS + ((sd * 3) & 255)] - 0.5;
        const xa = Math.max(0, Math.round(cx * k - half + nA * 2 * rr)), xb = Math.min(RW, Math.round(cx * k + half + nB * 2 * rr));
        for (let x = xa, i = y * RW + xa; x < xb; x++, i++) M[i] |= bit;
      }
    };
    P.rect = (x0, y0, x1, y1, bit) => {
      const ya = Math.max(0, Math.floor(y0 * k)), yb = Math.min(RH, Math.ceil(y1 * k));
      const xa = Math.max(0, Math.floor(x0 * k)), xb = Math.min(RW, Math.ceil(x1 * k));
      for (let y = ya; y < yb; y++) for (let x = xa, i = y * RW + xa; x < xb; x++, i++) M[i] |= bit;
    };
    // wet: brightness (Mix); period/duty: the hatch (periodDim: bit 4's); holes: noise eating the fill
    P.shade = (C, t, o = {}) => {
      const T = tones(C), wet = o.wet ?? 1, per = (o.period ?? 5) * k, perR = (o.period ?? 5) / (o.periodDim ?? o.period ?? 5), duty = o.duty ?? 0.5, holes = o.holes ?? 0.05;
      const cBg = pack(T.bg), cPale = pack(lerp3(T.bg, T.lo, (o.pale ?? 0.5) * wet));
      const cHi = pack(lerp3(T.bg, T.hi, wet)), cDim = pack(lerp3(T.bg, T.hi, (o.dim ?? 0.5) * wet)), cLo = pack(lerp3(T.bg, T.lo, wet));
      const ph = t * (o.drift ?? 0.15), oB = Math.floor(t * 9), oC = Math.floor(t * 6), sc = 1 / k, out = P.out;
      for (let y = 0, i = 0; y < RH; y++) for (let x = 0; x < RW; x++, i++) {
        const m = M[i];
        if (!m) { out[i] = cBg; continue; }
        const pr = (x * 0.77 + y * 0.64) / per, nh = NT[(((y * sc * 3.5) + oC) & 255) * NS + (((x * sc * 3.5) + oB) & 255)];
        if (nh < holes) { out[i] = cBg; continue; }
        if (m & 2) { const f = pr + ph - Math.floor(pr + ph); if (f < duty) { out[i] = (m & 8) ? cLo : cHi; continue; } }
        else if (m & 4) { const p2 = pr * perR + ph, f = p2 - Math.floor(p2); if (f < duty) { out[i] = cDim; continue; } }
        const f2 = pr - ph * 0.6 + 0.5 - Math.floor(pr - ph * 0.6 + 0.5);
        out[i] = (m & 1) && (o.paleSolid || f2 < duty * 0.9) ? cPale : cBg;
      }
    };
    // Dot-matrix instead of hatch: each bit drawn as square cells of its own
    // size (o.cell[bit], logical px), lit where the mask covers the cell's
    // centre, dimming toward the left (o.fade). Bit 16: thin lines, as drawn.
    P.cells = (C, o = {}) => {
      const T = tones(C), wet = o.wet ?? 1, out = P.out, cBg = pack(T.bg), fade = o.fade ?? 0;
      const tone = { 1: lerp3(T.bg, T.lo, (o.pale ?? 0.8) * wet), 2: lerp3(T.bg, T.hi, wet), 4: lerp3(T.bg, T.hi, (o.dim ?? 0.8) * wet) };
      out.fill(cBg);
      const cLine = pack(lerp3(T.bg, T.lo, 0.35));
      for (let i = 0; i < M.length; i++) if (M[i] & 16) out[i] = cLine;
      for (const bit of [4, 1, 2]) {
        const cs = (o.cell && o.cell[bit] || 4) * k, gap = Math.max(1, Math.round(cs * 0.22)), base = tone[bit];
        for (let cy = 0; cy < RH; cy += cs) for (let cx = 0; cx < RW; cx += cs) {
          const mx = Math.min(RW - 1, (cx + cs / 2) | 0), my = Math.min(RH - 1, (cy + cs / 2) | 0);
          if (!(M[my * RW + mx] & bit)) continue;
          const col = pack(lerp3(T.bg, base, 1 - fade * (1 - mx / RW)));
          const x0 = Math.round(cx), y0 = Math.round(cy), x1 = Math.min(RW, Math.round(cx + cs) - gap), y1 = Math.min(RH, Math.round(cy + cs) - gap);
          for (let y = y0; y < y1; y++) out.fill(col, y * RW + x0, y * RW + x1);
        }
      }
    };
    // Flat colours, no texture: bit 2 the colour, bit 4 the colour dimmer
    // (bit 8 cuts it: rings), bit 1 pale.
    P.flat = (C, o = {}) => {
      const T = tones(C), wet = o.wet ?? 1, out = P.out;
      const cBg = pack(T.bg), cHi = pack(lerp3(T.bg, T.hi, wet)), cDim = pack(lerp3(T.bg, T.hi, (o.dim ?? 0.6) * wet)), cPale = pack(lerp3(T.bg, T.lo, (o.pale ?? 0.6) * wet));
      for (let i = 0; i < M.length; i++) { const m = M[i]; out[i] = (m & 2) ? cHi : (m & 4) && !(m & 8) ? cDim : (m & 1) ? cPale : cBg; }
    };
    P.blit = g => {
      P.ctx.putImageData(P.img, 0, 0);
      g.imageSmoothingEnabled = false; g.drawImage(P.off, 0, 0, LW, LH); g.imageSmoothingEnabled = true;
    };
    return P;
  }

  /* SKULL_FRAMES:BEGIN generated by src/custom/skull/pe_frames.py: the pedal's own skull,
     128 x 64, 1 bit, rows of 16 bytes; jaw shut to open, and the eyes */
  const SKULL_EYES = [[47, 31], [64, 30]];
  const SKULL_JAW = [
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAH//gAAAAAAAAAAAAAAAAAf///AAAAAAAAAAAAAAAAAf///+AAAAAAAAAAAAAAAAf///1UAAAAAAAAAAAAAAAf////qgAAAAAAAAAAAAAAf///9VUAAAAAAAAAAAAAAP////7qoAAAAAAAAAAAAAH////VVEAAAAAAAAAAAAAD////+qqoAAAAAAAAAAAAB////1UREAAAAAAAAAAAAAf////uqqgAAAAAAAAAAAAP///9VUVEAAAAAAAAAAAAH////6qqqgAAAAAAAAAAAB////VREREAAAAAAAAAAAAf///+6qqqAAAAAAAAAAAAP///1VRERAAAAAAAAAAAAD////qqqqqAAAAAAAAAAAA///dVERERAAAAAAAAAAAAP//+qqqqqoAAAAAAAAAAAD///ABFREUAAAAAAAAAAAA8B/AAKqqqgAAAAAAAAAAAHANAAAERAQAAAAAAAAAAABgB4AAKqqqAAAAAAAAAAAAYAMAAAEREAAAAAAAAAAAACADgAAqqqgAAAAAAAAAAAAwA4AAFAREAAAAAAAAAAAAAAPAAH6qqAAAAAAAAAAAACAEYAH1ERAAAAAAAAAAAADgCBgGqqqoAAAAAAAAAAAA+Dgf0AREQAAAAAAAAAAAAP/wD/qqqqgAAAAAAAAAAAAH8A/wAQEQAAAAAAAAAAAAD/AP6qqqoAAAAAAAAAAAAAf4D9QAAEAAAAAAAAAAAAAH+B/6qqqgAAAAAAAAAAAAB/w/VRERAAAAAAAAAAAAAAf+6qqqqoAAAAAAAAAAAAABR+gERAQAAAAAAAAAAAAAAL/roq6oAAAAAAAAAAAAAAA/+xEXAAAAAAAAAAAAAAAAP6qqvqAAAAAAAAAAAAAAAD3UABxAAAAAAAAAAAAAAAA///5+oAAAAAAAAAAAAAAA/1/1FQAAAAAAAAAAAAAAAf//+qqgAAAAAAAAAAAAAAH//9RAQAAAAAAAAAAAAAAB/7qqqgAAAAAAAAAAAAAAABERERAAAAAAAAAAAAAAAAAKoAAoAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAH//gAAAAAAAAAAAAAAAAAf///AAAAAAAAAAAAAAAAAf///+AAAAAAAAAAAAAAAAf///1UAAAAAAAAAAAAAAAf////qgAAAAAAAAAAAAAAf///9VUAAAAAAAAAAAAAAP////7qoAAAAAAAAAAAAAH////VVEAAAAAAAAAAAAAD////+qqoAAAAAAAAAAAAB////1UREAAAAAAAAAAAAAf////uqqgAAAAAAAAAAAAP///9VUVEAAAAAAAAAAAAH////6qqqgAAAAAAAAAAAB////VREREAAAAAAAAAAAAf///+6qqqAAAAAAAAAAAAP///1VRERAAAAAAAAAAAAD////qqqqqAAAAAAAAAAAA///dVERERAAAAAAAAAAAAP//+qqqqqoAAAAAAAAAAAD///ABFREUAAAAAAAAAAAA8B/AAKqqqgAAAAAAAAAAAHANAAAERAQAAAAAAAAAAABgB4AAKqqqAAAAAAAAAAAAYAMAAAEREAAAAAAAAAAAACADgAAqqqgAAAAAAAAAAAAwA4AAFAREAAAAAAAAAAAAAAPAAH6qqAAAAAAAAAAAACAEYAH1ERAAAAAAAAAAAADgCBgGqqqoAAAAAAAAAAAA+Dgf0AREQAAAAAAAAAAAAP/wD/qqqqgAAAAAAAAAAAAH8A/wAQEQAAAAAAAAAAAAD/AP6qqqoAAAAAAAAAAAAAf4D9QAAEAAAAAAAAAAAAAH+B/6qiqgAAAAAAAAAAAAB/w/VRERAAAAAAAAAAAAAAf+6qqqqoAAAAAAAAAAAAABR+gERAQAAAAAAAAAAAAAAL/rqq6oAAAAAAAAAAAAAAA/+xEXAAAAAAAAAAAAAAAAP6qqrqAAAAAAAAAAAAAAADwAAAxAAAAAAAAAAAAAAAA//wAeoAAAAAAAAAAAAAAAF/X9VQAAAAAAAAAAAAAAAH///qqgAAAAAAAAAAAAAAD///RAQAAAAAAAAAAAAAAA///6qoAAAAAAAAAAAAAAAPVVURAAAAAAAAAAAAAAAAAqqqqoAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAH//gAAAAAAAAAAAAAAAAAf///AAAAAAAAAAAAAAAAAf///+AAAAAAAAAAAAAAAAf///1UAAAAAAAAAAAAAAAf////qgAAAAAAAAAAAAAAf///9VUAAAAAAAAAAAAAAP////7qoAAAAAAAAAAAAAH////VVEAAAAAAAAAAAAAD////+qqoAAAAAAAAAAAAB////1UREAAAAAAAAAAAAAf////uqqgAAAAAAAAAAAAP///9VUVEAAAAAAAAAAAAH////6qqqgAAAAAAAAAAAB////VREREAAAAAAAAAAAAf///+6qqqAAAAAAAAAAAAP///1VRERAAAAAAAAAAAAD////qqqqqAAAAAAAAAAAA///dVERERAAAAAAAAAAAAP//+qqqqqoAAAAAAAAAAAD///ABFREUAAAAAAAAAAAA8B/AAKqqqgAAAAAAAAAAAHANAAAERAQAAAAAAAAAAABgB4AAKqqqAAAAAAAAAAAAYAMAAAEREAAAAAAAAAAAACADgAAqqqgAAAAAAAAAAAAwA4AAFAREAAAAAAAAAAAAAAPAAH6qqAAAAAAAAAAAACAEYAH1ERAAAAAAAAAAAADgCBgGqqqoAAAAAAAAAAAA+Dgf0AREQAAAAAAAAAAAAP/wD/qqqqgAAAAAAAAAAAAH8A/wAQEQAAAAAAAAAAAAD/AP6qqqoAAAAAAAAAAAAAf4D9QAAEAAAAAAAAAAAAAH+B/6qqqgAAAAAAAAAAAAB/w/VRERAAAAAAAAAAAAAAf+6qqqqoAAAAAAAAAAAAABR+gERAQAAAAAAAAAAAAAAL/rqq6oAAAAAAAAAAAAAAA/+xEXAAAAAAAAAAAAAAAAP6qqrqAAAAAAAAAAAAAAABwAAA1AAAAAAAAAAAAAAAAegAAfoAAAAAAAAAAAAAAAH/0APxAAAAAAAAAAAAAAAB///q6gAAAAAAAAAAAAAAB//fxAQAAAAAAAAAAAAAAA///6qoAAAAAAAAAAAAAAAP//8REAAAAAAAAAAAAAAAB+6qqqAAAAAAAAAAAAAAAARERAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAH//gAAAAAAAAAAAAAAAAAf///AAAAAAAAAAAAAAAAAf///+AAAAAAAAAAAAAAAAf///1UAAAAAAAAAAAAAAAf////qgAAAAAAAAAAAAAAf///9VUAAAAAAAAAAAAAAP////7qoAAAAAAAAAAAAAH////VVEAAAAAAAAAAAAAD////+qqoAAAAAAAAAAAAB////1UREAAAAAAAAAAAAAf////uqqgAAAAAAAAAAAAP///9VUVEAAAAAAAAAAAAH////6qqqgAAAAAAAAAAAB////VREREAAAAAAAAAAAAf///+6qqqAAAAAAAAAAAAP///1VRERAAAAAAAAAAAAD////qqqqqAAAAAAAAAAAA///dVERERAAAAAAAAAAAAP//+qqqqqoAAAAAAAAAAAD///ABFREUAAAAAAAAAAAA8B/AAKqqqgAAAAAAAAAAAHANAAAERAQAAAAAAAAAAABgB4AAKqqqAAAAAAAAAAAAYAMAAAEREAAAAAAAAAAAACADgAAqqqgAAAAAAAAAAAAwA4AAFAREAAAAAAAAAAAAAAPAAH6qqAAAAAAAAAAAACAEYAH1ERAAAAAAAAAAAADgCBgGqqqoAAAAAAAAAAAA+Dgf0AREQAAAAAAAAAAAAP/wD/qqqqgAAAAAAAAAAAAH8A/wAQEQAAAAAAAAAAAAD/AP6qqqoAAAAAAAAAAAAAf4D9QAAEAAAAAAAAAAAAAH+B/6qqqgAAAAAAAAAAAAB/w/VRERAAAAAAAAAAAAAAf+6qqqqoAAAAAAAAAAAAABR+gERAQAAAAAAAAAAAAAAL/rqqqoAAAAAAAAAAAAAAA/+wEHAAAAAAAAAAAAAAAAP6qqrqAAAAAAAAAAAAAAABwAAA1AAAAAAAAAAAAAAAAegAAPoAAAAAAAAAAAAAAAHQAAHxAAAAAAAAAAAAAAAB/6n+6gAAAAAAAAAAAAAAAf/n1AQAAAAAAAAAAAAAAAP//+qoAAAAAAAAAAAAAAAH///REAAAAAAAAAAAAAAAB///qqAAAAAAAAAAAAAAAAVVREQAAAAAAAAAAAAAAAACqqgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAH//gAAAAAAAAAAAAAAAAAf///AAAAAAAAAAAAAAAAAf///+AAAAAAAAAAAAAAAAf///1UAAAAAAAAAAAAAAAf////qgAAAAAAAAAAAAAAf///9VUAAAAAAAAAAAAAAP////7qoAAAAAAAAAAAAAH////VVEAAAAAAAAAAAAAD////+qqoAAAAAAAAAAAAB////1UREAAAAAAAAAAAAAf////uqqgAAAAAAAAAAAAP///9VUVEAAAAAAAAAAAAH////6qqqgAAAAAAAAAAAB////VREREAAAAAAAAAAAAf///+6qqqAAAAAAAAAAAAP///1VRERAAAAAAAAAAAAD////qqqqqAAAAAAAAAAAA///dVERERAAAAAAAAAAAAP//+qqqqqoAAAAAAAAAAAD///ABFREUAAAAAAAAAAAA8B/AAKqqqgAAAAAAAAAAAHANAAAERAQAAAAAAAAAAABgB4AAKqqqAAAAAAAAAAAAYAMAAAEREAAAAAAAAAAAACADgAAqqqgAAAAAAAAAAAAwA4AAFAREAAAAAAAAAAAAAAPAAH6qqAAAAAAAAAAAACAEYAH1ERAAAAAAAAAAAADgCBgGqqqoAAAAAAAAAAAA+Dgf0AREQAAAAAAAAAAAAP/wD/qqqqgAAAAAAAAAAAAH8A/wAQEQAAAAAAAAAAAAD/AP6qqqoAAAAAAAAAAAAAf4D9QAAEAAAAAAAAAAAAAH+B/6qqqgAAAAAAAAAAAAB/w/VRERAAAAAAAAAAAAAAf+6qqqqoAAAAAAAAAAAAABR+gERAQAAAAAAAAAAAAAAL/rqqqoAAAAAAAAAAAAAAA/+xEHAAAAAAAAAAAAAAAAP6qqrqAAAAAAAAAAAAAAABwAAAVAAAAAAAAAAAAAAAAeoAAHoAAAAAAAAAAAAAAADQAADxAAAAAAAAAAAAAAAA/gAD6oAAAAAAAAAAAAAAAP/5/FQAAAAAAAAAAAAAAAH/+/qqAAAAAAAAAAAAAAAD/9/REAAAAAAAAAAAAAAAA///6qAAAAAAAAAAAAAAAAP/3URAAAAAAAAAAAAAAAAD+qqoAAAAAAAAAAAAAAAAAVFAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAH//gAAAAAAAAAAAAAAAAAf///AAAAAAAAAAAAAAAAAf///+AAAAAAAAAAAAAAAAf///1UAAAAAAAAAAAAAAAf////qgAAAAAAAAAAAAAAf///9VUAAAAAAAAAAAAAAP////7qoAAAAAAAAAAAAAH////VVEAAAAAAAAAAAAAD////+qqoAAAAAAAAAAAAB////1UREAAAAAAAAAAAAAf////uqqgAAAAAAAAAAAAP///9VUVEAAAAAAAAAAAAH////6qqqgAAAAAAAAAAAB////VREREAAAAAAAAAAAAf///+6qqqAAAAAAAAAAAAP///1VRERAAAAAAAAAAAAD////qqqqqAAAAAAAAAAAA///dVERERAAAAAAAAAAAAP//+qqqqqoAAAAAAAAAAAD///ABFREUAAAAAAAAAAAA8B/AAKqqqgAAAAAAAAAAAHANAAAERAQAAAAAAAAAAABgB4AAKqqqAAAAAAAAAAAAYAMAAAEREAAAAAAAAAAAACADgAAqqqgAAAAAAAAAAAAwA4AAFAREAAAAAAAAAAAAAAPAAH6qqAAAAAAAAAAAACAEYAH1ERAAAAAAAAAAAADgCBgGqqqoAAAAAAAAAAAA+Dgf0AREQAAAAAAAAAAAAP/wD/qqqqgAAAAAAAAAAAAH8A/wAQEQAAAAAAAAAAAAD/AP6qqqoAAAAAAAAAAAAAf4D9QAAEAAAAAAAAAAAAAH+B/6qqqgAAAAAAAAAAAAB/w/VRERAAAAAAAAAAAAAAf+6qqqqoAAAAAAAAAAAAABR+gERAQAAAAAAAAAAAAAAL/rqqqoAAAAAAAAAAAAAAA/+xEHAAAAAAAAAAAAAAAAP6qqr6AAAAAAAAAAAAAAABwAAAVAAAAAAAAAAAAAAAAOoAAHqAAAAAAAAAAAAAAADQAADxAAAAAAAAAAAAAAAA/oAB6oAAAAAAAAAAAAAAAPxEDVQAAAAAAAAAAAAAAAD///qqAAAAAAAAAAAAAAAA//fxEAAAAAAAAAAAAAAAAf//6qgAAAAAAAAAAAAAAAP/3cRAAAAAAAAAAAAAAAAD//+qAAAAAAAAAAAAAAAAAVFRUAAAAAAAAAAAAAAAAACqAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==",
  ];
  /* SKULL_FRAMES:END */
  let SKULL_PX = null;

  /* ---- scenes ---------------------------------------------------------- */
  const SCENES = {
    // Drive: two hatched shapes, the low band and the high band (Tilt sizes
    // them), bulging toward square as Drive clips, squashed by Comp, edges
    // stepped by Crush (Mode: steps across, down, or both), lines shaken by Noise.
    hatch(g, q, v, t, C) {
      const drive = q(R.drive, 0.5), crush = q(R.crush, 0), comp = q(R.comp, 0);
      const tilt = q(R.tilt, 0.5), noise = q(R.noise, 0), mix = q(R.mix, 1);
      const mode = q.raw(/^mode$/i, 0), fold = /fold/i.test(v.e.name) ? q(/^symm$/i, 0.5) : null;
      const n = 2 + drive * 6;
      const step = crush > 0.02 ? 2 + crush * 12 : 0;
      const snap = pts => !step ? pts : pts.map(([x, y]) => [
        mode === 0 ? x : Math.round(x / step) * step, mode === 1 ? y : Math.round(y / step) * step]);
      const ry = 1 - 0.38 * comp;
      const r1 = 30 + 22 * (1 - tilt), r2 = 30 + 22 * tilt;
      const wob = q(/^(flutter|wow|motor)$/i, 0);     // tape and motors: the outline wavers
      const wv = (pts, cx, cy, ph) => !wob ? pts : pts.map(([x, y], k) => {
        const m = 1 + wob * 0.14 * Math.sin(k / 96 * TAU * 5 + t * (2 + wob * 6) + ph);
        return [cx + (x - cx) * m, cy + (y - cy) * m]; });
      const bx = 158 + (fold !== null ? (fold - 0.5) * 30 : 0);
      const pa = snap(wv(polyBlob(98, 66, r1 * 1.25, r1 * ry, n), 98, 66, 0));
      const pb = snap(wv(polyBlob(bx, 62, r2 * 1.25, r2 * ry, n), bx, 62, 2));
      const a = pathOf.bind(null, g, pa), b = pathOf.bind(null, g, pb);
      const rr = rng(Math.floor(t * 12) + 1);
      g.globalAlpha = 0.35 + 0.65 * mix;
      g.lineWidth = 1.6 + drive * 1.6;
      const gap = 9 - drive * 4.5, off = t * (6 + drive * 18);
      g.save(); a(); g.clip(); hatch(g, -0.75 - drive * 0.35, gap, off, noise * 6, rr); g.restore();
      g.save(); b(); g.clip(); hatch(g, -0.75 - drive * 0.35, gap, -off * 0.6, noise * 6, rr); g.restore();
      g.save(); a(); g.clip(); b(); g.globalAlpha *= 0.85; g.fill(); g.restore();
      g.globalAlpha = 0.5; g.lineWidth = 1; a(); g.stroke(); b(); g.stroke();
      if (fold !== null) {                           // wavefolding: the shape folds in on itself
        const nf = 1 + Math.round(drive * 4);
        for (let i = 1; i <= nf; i++) {
          const k = 1 - i / (nf + 1), ph = Math.sin(t * 1.5 + i) * 0.04;
          g.globalAlpha = 0.7 - i * 0.1; pathOf(g, polyBlob(bx, 62, r2 * 1.25 * (k + ph), r2 * ry * (k - ph), n)); g.stroke();
        }
      }
      g.globalAlpha = 1;
    },

    // Deform, after the S-4's screen: two big hatched circles sitting on the
    // floor, the low band (left) and the high band (right), pulsing with the
    // playing. Tilt sizes them; Split slides them; Comp evens the pulse,
    // flattens them and pulls them together. Drive thickens the lines and eats
    // holes and dashes into them. Noise throws off hatched blobs that fade as
    // Decay says, roughens the edges and dusts the background. Crush holds the
    // picture in steps: Redux across (time), Bit down (level), Both in blocks.
    deform(g, q, v, t, C, dt) {
      const s = v.s, cv = v.canvas;
      const RW = Math.max(200, Math.min(480, Math.round(cv.width))) & ~1, RH = RW >> 1, k = RW / LW;
      if (!s.off || s.off.width !== RW) {
        s.off = document.createElement("canvas"); s.off.width = RW; s.off.height = RH;
        s.ctx = s.off.getContext("2d"); s.img = s.ctx.createImageData(RW, RH);
        s.out = new Uint32Array(s.img.data.buffer); s.fb = new Uint32Array(RW * RH);
        s.mask = new Uint8Array(RW * RH);
        if (!s.r) { s.r = rng(v.e.id || 9); s.envL = 0; s.envR = 0; s.next = 0; s.blobs = []; s.jit = 0; s.jt = 0; }
      }
      const N = noiseTex();
      const drive = q(R.drive, 0.35), crush = q(R.crush, 0), comp = q(R.comp, 0), tilt = q(R.tilt, 0.5);
      const split = q(/^split$/i, 0.5), noise = q(R.noise, 0), decay = q(R.decay, 0.4), mix = q(R.mix, 1);
      const mode = q.raw(/^mode$/i, 0);

      // The playing: hits every so often; the high band jumps, the low swells.
      if (v.live && t >= s.next) {
        s.envR = 1; s.envL = Math.max(s.envL, 0.8);
        s.next = t + 0.3 + s.r() * 0.9;
        for (let n = Math.round(noise * (2 + s.r() * 3)); n > 0; n--) spawn();
      }
      if (v.live && noise > 0.02 && s.r() < noise * 5 * dt) spawn();
      s.envR *= Math.exp(-dt / 0.28); s.envL *= Math.exp(-dt / 0.7);
      function spawn() {
        if (s.blobs.length > 26) s.blobs.shift();
        const r0 = 6 + s.r() * (10 + 24 * noise);
        s.blobs.push({ x: 14 + s.r() * 228, y: 22 + s.r() * 86, rx: r0 * (0.8 + s.r() * 0.5), ry: r0 * (0.7 + s.r() * 0.5),
          born: t, life: 0.08 + 2 * decay * decay * (0.6 + s.r() * 0.8), vy: -(2 + s.r() * 8), ph: s.r() });
      }
      s.blobs = s.blobs.filter(b => t - b.born < b.life || !v.live);

      // Shapes (logical units, 256 x 128; the floor at 120).
      const ev = e => Math.pow(e, 1 / (1 + 3 * comp));            // compressed: more even
      const pumpL = 0.88 + 0.12 * ev(s.envL) * (1 - 0.4 * comp) + 0.05 * comp;
      const pumpR = 0.86 + 0.14 * ev(s.envR) * (1 - 0.4 * comp) + 0.05 * comp;
      const r0 = 68, floor = 120, flat = 1 - 0.2 * comp, gapC = 88 * (1 - 0.3 * comp);
      const mid = 128 + (split - 0.5) * 22;
      const rL = r0 * (1.16 - 0.32 * tilt) * pumpL, rR = r0 * (0.84 + 0.32 * tilt) * pumpR;
      const shapes = [
        { cx: mid - gapC / 2, rx: rL, ry: rL * flat, bit: 1 },
        { cx: mid + gapC / 2, rx: rR, ry: rR * flat, bit: 2 },
      ];
      shapes.forEach(sh => { sh.cy = floor - 0.3 * sh.ry; });

      // Fill the mask, row by row, with rough edges.
      const M = s.mask; M.fill(0);
      const rough = (0.25 + drive * 2.2 + noise * 4.5) * k, tN = Math.floor(t * (8 + 30 * noise + 10 * drive));
      const yFloor = Math.round(floor * k);
      const span = (cx, cy, rx, ry, bit, seed, lim) => {
        const y0 = Math.max(0, Math.floor((cy - ry) * k)), y1 = Math.min(lim, Math.ceil((cy + ry) * k));
        for (let y = y0; y < y1; y++) {
          const dy = (y / k - cy) / ry; if (dy <= -1 || dy >= 1) continue;
          const half = rx * Math.sqrt(1 - dy * dy) * k;
          const nA = N[((y * 2) & 255) * NS + ((seed + tN) & 255)] - 0.5, nB = N[((y * 2 + 97) & 255) * NS + ((seed * 3 + tN) & 255)] - 0.5;
          const xa = Math.max(0, Math.round(cx * k - half + nA * 2 * rough)), xb = Math.min(RW, Math.round(cx * k + half + nB * 2 * rough));
          for (let x = xa, i = y * RW + xa; x < xb; x++, i++) M[i] |= bit;
        }
      };
      shapes.forEach((sh, j) => span(sh.cx, sh.cy, sh.rx, sh.ry, sh.bit, 31 + j * 77, yFloor));
      for (const b of s.blobs) {
        const age = clamp((t - b.born) / b.life), sz = Math.sqrt(1 - age);
        if (sz <= 0.05) continue;
        const by = b.y + b.vy * (t - b.born), sd = (b.ph * 255) | 0;
        span(b.x, by, b.rx * sz, b.ry * sz, 4, sd, RH);
        span(b.x, by, b.rx * sz - 1.6, b.ry * sz - 1.6, 8, sd, RH);
      }

      // Colours: the high band in the screen colour, the low band paler; Mix fades the lot.
      const bg = rgbOf(C.bg), fg = rgbOf(C.fg), wet = 0.35 + 0.65 * mix;
      const hi = vivid(fg), lo = lerp3(fg, [255, 255, 255], 0.55);
      const cR = pack(lerp3(bg, hi, wet)), cL = pack(lerp3(bg, lo, wet));
      const cLd = pack(lerp3(bg, lo, wet * 0.45)), cBg = pack(bg);
      const cDust = pack(lerp3(bg, fg, 0.55 * wet));

      // Shade: diagonal hatch, eaten by Drive and Noise.
      const cs = clamp((crush - 0.08) / 0.92);
      const period = (5 + 12 * cs * cs) * k;
      const duty = mode === 2 ? 0.46 + 0.22 * drive + (0.54 - 0.22 * drive) * cs : 0.46 + 0.22 * drive;   // Both: fills in
      const phL = t * 0.18, phR = 0.5 - t * 0.12;
      const eat = clamp((drive - 0.25) / 0.75), holes = eat * 0.2, dash = Math.max(eat * 0.25, noise * 0.2);
      const fillIn = eat * 0.22, sc = 1 / k;
      const oA = Math.floor(t * (14 + 50 * drive)), oB = Math.floor(t * (9 + 30 * drive)), oC = Math.floor(t * 6);
      const fb = s.fb;
      for (let y = 0, i = 0; y < RH; y++) {
        for (let x = 0; x < RW; x++, i++) {
          const m = M[i];
          if (!m) { fb[i] = cBg; continue; }
          const pr = (x * 0.77 + y * 0.64) / period;
          const fL = pr + phL - Math.floor(pr + phL), fR = pr + phR - Math.floor(pr + phR);
          let c = 0;
          if ((m & 12) === 4) { fb[i] = cBg; continue; }             // a fragment's edge: cut
          if (m & 8) { const fB = fL + 0.33 - Math.floor(fL + 0.33); c = fB < duty ? ((m & 2) ? cR : cL) : 0; }
          else if (m & 2) c = fR < duty ? cR : (m & 1) && fL < duty * 0.7 ? cLd : 0;
          else c = fL < duty ? cL : 0;
          // along the lines (dashes) and across the field (holes)
          const al = (x * 0.64 - y * 0.77) * sc, ac = pr * period * sc;
          const nd = N[(((ac * 3) | 0) & 255) * NS + (((al * 0.45) + oA) & 255)];
          const nh = N[(((y * sc * 3.5) + oC) & 255) * NS + (((x * sc * 3.5) + oB) & 255)];
          const nf = N[(((y * sc * 6) + oA) & 255) * NS + (((x * sc * 6) - oC) & 255)];
          if (c) { if (nh < holes || nd < ((m & 3) ? dash : dash + 0.3)) c = 0; }
          else if (nf > 1 - fillIn && nh > holes) c = (m & 2) ? cR : cL;
          fb[i] = c || cBg;
        }
      }
      if (noise > 0.01 && v.live) {               // dust
        for (let n = Math.round(noise * RW * RH * 0.0025); n > 0; n--) fb[(s.r() * RW * yFloor) | 0] = cDust;
      }

      // Crush: hold. Redux holds across (time, with a little jitter), Bit holds
      // down (level), Both holds in blocks.
      const out = s.out;
      const hxB = mode === 0 ? 1 : mode === 1 ? 1 + cs * 0.3 * period : 1 + cs ** 1.3 * 18 * k;
      const hyB = mode === 1 ? 1 : mode === 0 ? 1 + cs * 0.3 * period : hxB;
      if (hxB < 1.5 && hyB < 1.5) out.set(fb);
      else {
        if (v.live && t - s.jt > 0.09) { s.jt = t; s.jit = s.r(); }
        const jx = mode === 0 ? 0 : s.jit * hxB * 0.6;
        for (let y = 0; y < RH; y++) {
          const sy = hyB > 1 && y < yFloor ? Math.min(yFloor - 1, Math.floor(y / hyB) * hyB | 0) : y;
          const row = sy * RW, o = y * RW;
          for (let x = 0; x < RW; x++) {
            const sx = hxB > 1 ? Math.max(0, Math.min(RW - 1, (Math.floor((x + jx) / hxB) * hxB - jx) | 0)) : x;
            out[o + x] = fb[row + sx];
          }
        }
      }
      s.ctx.putImageData(s.img, 0, 0);
      g.imageSmoothingEnabled = false;
      g.drawImage(s.off, 0, 0, LW, LH);
      g.imageSmoothingEnabled = true;
    },

    // Filter bank (Carillon): a bar per band at the filter shape's height
    // (Cutoff, Reso, Slope, Waves), lit as the bands ring (Decay); Noise
    // strikes more; Scale leaves bands out, as the pedal does.
    bars(g, q, v, t, C, dt) {
      const s = v.s, N = 32;
      if (!s.e) { s.e = new Float32Array(N); s.next = 0; s.r = rng(v.e.id || 7); }
      const cut = q(R.cut, 0.7), reso = q(R.reso, 0.3), decay = q(R.decay, 0.5);
      const slope = q(/^slope$/i, v.kind === "eq" ? 0.5 : 0.4), waves = q(/^waves$/i, 0.2), noise = q(R.noise, 0);
      const on = bandsOn(q.raw(/^scale$/i, 1), q.raw(/^key$/i, 0), N);
      const rsn = reso * reso * 3, cutSemi = cut * 78 - 3, depth = clamp(2 * waves);
      const ph = t * (0.12 + 2.38 * clamp(2 * waves - 1) ** 2);
      const tau = 0.06 + 3 * decay * decay;
      for (let i = 0; i < N; i++) s.e[i] *= Math.exp(-dt / tau);
      if (v.live && t >= s.next) {                 // a "note": a few bands, then struck ones
        const k = Math.floor(s.r() * N); s.e[k] = Math.max(s.e[k], 0.6 + 0.4 * s.r());
        if (s.r() < 0.4) { const j = (k + 7) % N; s.e[j] = Math.max(s.e[j], 0.5); }
        s.next = t + 1 / (4 + 18 * noise) * (0.4 + s.r());
      }
      const bw = LW / N;
      for (let i = 0; i < N; i++) {
        const x = i * bw + bw * 0.22, w = bw * 0.56;
        if (!on[i]) { g.globalAlpha = 0.5; g.fillRect(x, LH - 10, w, 2); continue; }
        let sh;
        if (v.kind === "eq") sh = eqShape(q, i / (N - 1));
        else {
          const dl = (i * 72 / 31 - cutSemi) / 12;
          const roll = z => 1 / (1 + z ** 4);
          const lp = roll(Math.max(dl, 0) * 1.6), hp = roll(Math.max(-dl, 0) * 1.6), bp = roll(Math.abs(dl) * 1.1);
          sh = slope < 0.5 ? lp + (bp - lp) * slope * 2 : bp + (hp - bp) * (slope * 2 - 1);
          sh += rsn / (1 + 9 * dl * dl);
          const wv = 0.5 + 0.25 * (Math.sin(TAU * (ph + i * 0.131)) + Math.sin(TAU * (ph * 0.63 + 3 - i * 0.049)));
          sh = sh * (1 - depth * wv) * (1 + 0.5 * depth) / (1 + rsn);
        }
        const hMax = (LH - 22) * clamp(sh, 0.03, 1.1);
        g.globalAlpha = 0.28; g.fillRect(x, LH - 8 - hMax, w, hMax);
        const lit = hMax * (0.18 + 0.82 * s.e[i]);
        g.globalAlpha = 1; g.fillRect(x, LH - 8 - lit, w, lit);
      }
      g.globalAlpha = 1;
    },

    // Spectrum (and other bars without a shape): a falling, tilted spectrum.
    spectrum(g, q, v, t, C, dt) {
      const s = v.s, N = 40;
      if (!s.e) { s.e = new Float32Array(N); s.r = rng(5); }
      const fall = q(R.decay, 0.5), tilt = q(R.tilt, 0.5);
      for (let i = 0; i < N; i++) {
        const target = (0.35 + 0.65 * s.r()) * (1 - 0.6 * (i / N) * (1 - tilt) * 1.4) * (0.6 + 0.4 * Math.sin(t * 2 + i * 0.4));
        s.e[i] = v.live ? Math.max(target * (s.r() < 0.15 ? 1 : 0.7), s.e[i] * Math.exp(-dt * (1 + 6 * (1 - fall)))) : (s.e[i] || target);
        const h = (LH - 20) * clamp(s.e[i]);
        g.globalAlpha = 1; g.fillRect(i * LW / N + 1.5, LH - 8 - h, LW / N - 3, h);
      }
    },

    // Waveform scenes: grains (Droplets), chops (Chopdly), a loop, a scrub, a freeze.
    wave(g, q, v, t, C, dt) {
      const s = v.s;
      if (!s.w) { s.w = waveTable(rng(v.e.id || 3), 512); s.off = 0; s.grains = []; s.r = rng(11); s.nextG = 0; }
      const cy = LH / 2, h = LH * 0.42, k = v.kind;
      g.lineWidth = 1;
      if (k === "grains") {
        const rate = q(/^rate$/i, 0.5), size = q(/^size$/i, 0.4), contour = q(/^contour$/i, 0.5);
        const scan = q(/^scan$/i, 0.3), rnd = q(/^random$/i, 0.3), fb = q(R.fb, 0.3);
        const lock = fb > 0.985;
        if (v.live && !lock) s.off += dt * 22;
        g.globalAlpha = 0.32; g.beginPath(); drawWave(g, s.w, 0, LW, cy, h, s.off, 1, false); g.stroke();
        let pos = scan < 0.5 ? LW * (0.96 - scan * 1.8) : (LW * 0.95 - ((t * (scan - 0.5) * 2 * 60) % (LW * 0.9)));
        if (v.live && t >= s.nextG) {
          s.grains.push({ x: pos + (s.r() - 0.5) * LW * 0.55 * clamp(rnd * 3), b: t, life: 0.08 + size * 0.7, rev: s.r() < clamp(rnd * 3 - 2) * 0.5 });
          s.nextG = t + 1 / (2 + 40 * rate * rate);
          if (s.grains.length > 40) s.grains.shift();
        }
        g.globalAlpha = 1; g.lineWidth = 1.4;
        s.grains = s.grains.filter(gr => t - gr.b < gr.life);
        for (const gr of s.grains) {
          const u = (t - gr.b) / gr.life;
          const env = contour < 0.5 ? (1 - u) ** (1 + (0.5 - contour) * 6) : Math.sin(Math.PI * Math.min(1, u * (1 + (contour - 0.5) * 2))) ;
          const i = ((Math.floor(gr.x + s.off) % 512) + 512) % 512;
          const hh = h * (0.35 + 0.65 * Math.max(s.w.top[i], 0.3)) * env;
          g.beginPath(); g.setLineDash(gr.rev ? [2, 2] : []);
          g.moveTo(gr.x + 0.5, cy - hh); g.lineTo(gr.x + 0.5, cy + hh); g.stroke();
        }
        g.setLineDash([]);
        g.globalAlpha = 0.9; g.fillRect(pos - 0.5, LH - 6, 2, 5);
        if (lock) { g.globalAlpha = 1; g.font = "9px ui-monospace,monospace"; g.fillText("LOCK", 4, LH - 4); }
      } else if (k === "chops") {
        const time = q(R.time, 0.4), chops = q(/^chops$/i, 0.3), order = q.raw(/^order$/i, 0);
        const rev = q(/^reverse$/i, 0), stretch = q(/^stretch$/i, 0.5), pitch = q(/^pitch$/i, 0.5);
        const n = 2 + Math.round(chops * 14), r = rng(order * 97 + 13), rr = rng(Math.round(rev * 8) * 31 + order + 5);
        const idx = [...Array(n).keys()];
        if (order) for (let i = n - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); [idx[i], idx[j]] = [idx[j], idx[i]]; }
        const period = 0.5 + time * 2.5, ph = v.live ? (t / period) % 1 : 0.3, cur = Math.floor(ph * n);
        const segW = LW / n;
        for (let c = 0; c < n; c++) {
          const x0 = c * segW, back = rr() < rev;
          g.globalAlpha = c === cur ? 1 : 0.38;
          g.beginPath(); drawWave(g, s.w, x0 + 1, x0 + segW - 1, cy, h, idx[c] * 512 / n, 0.5 + pitch * 1.2 * (0.6 + stretch * 0.8), back); g.stroke();
          if (back) { g.globalAlpha = 0.8; g.beginPath(); g.moveTo(x0 + segW - 4, 6); g.lineTo(x0 + 4, 6); g.lineTo(x0 + 7, 3); g.stroke(); }
        }
        g.globalAlpha = 0.6;
        for (let c = 1; c < n; c++) g.fillRect(c * segW - 0.5, LH - 8, 1, 6);
        g.globalAlpha = 1; g.fillRect(ph * LW, 2, 1.2, LH - 4);
      } else {                                      // loop / scrub / freeze
        const pos = q(/^pos$/i, null), blur = q(/^blur$/i, 0), len = q(/^length$/i, 1), fade = q(/^fade$/i, 0);
        const frozen = k === "freeze";
        if (v.live && !frozen && k !== "loop") s.off += dt * 18;
        const span = frozen ? 0.3 + 0.7 * len : 1;
        g.globalAlpha = frozen ? 0.85 : 0.95;
        g.lineWidth = 1 + blur * 2.5;
        g.beginPath(); drawWave(g, s.w, 0, LW * span, cy, h * (1 - blur * 0.3), s.off, 1 - blur * 0.5, false); g.stroke();
        if (fade > 0) { const gr = g.createLinearGradient(LW * 0.4, 0, LW, 0); gr.addColorStop(0, "rgba(0,0,0,0)"); gr.addColorStop(1, C.bg); g.globalAlpha = fade; g.fillStyle = gr; g.fillRect(0, 0, LW, LH); g.fillStyle = C.fg; }
        g.globalAlpha = 1;
        const px = pos !== null ? pos * LW : ((t * 0.25) % 1) * LW * span;
        g.fillRect(px, 2, 1.4, LH - 4);
        if (pos !== null) { const sp = q(/^spray$/i, 0); g.globalAlpha = 0.25; g.fillRect(px - sp * 40, 2, sp * 80, LH - 4); }
        for (let i = 0; i <= 8; i++) { g.globalAlpha = 0.6; g.fillRect(i * LW * span / 8, LH - 5, 1, 4); }
      }
      g.globalAlpha = 1;
    },

    // Delays: rings sent out at the delay time, fading by Feedback. Reverbs:
    // a turning box with layers inside (Size, Decay). Both when an effect has both.
    space(g, q, v, t, C) {
      const time = q(R.time, 0.4), fb = q(R.fb, 0.4), mix = q(R.mix, 0.6), tone = q(R.tilt, 0.6);
      const size = q(R.size, 0.5), decay = q(R.decay, 0.5);
      const delay = v.kind !== "reverb", verb = v.kind !== "delay";
      const cxR = verb && delay ? 74 : LW / 2;
      if (delay) {
        // Each effect's own knobs shape its echoes: Crush makes them polygons,
        // Rise lifts them, Spin turns them dashed, Fast/Slow are two sources,
        // a grid delay's are square.
        const crushR = q(/^crush$/i, 0), rise = q(/^rise$/i, 0), spin = q(/^spin$/i, null);
        const fast = q(/^fast$/i, null), slow = q(/^slow$/i, null), square = /grid/i.test(v.e.name);
        const keep = clamp(0.15 + fb * 0.84, 0, 0.985);
        const srcs = fast !== null && slow !== null
          ? [[cxR - 48, 0.2 + (1 - fast) * 0.5], [cxR + 48, 0.6 + (1 - slow) * 1.4]]
          : [[cxR, 0.3 + time * 1.4]];
        const sides = crushR > 0.05 ? Math.max(3, Math.round(20 - crushR * 17)) : 0;
        g.lineWidth = 0.8 + tone * 1.4;
        if (spin !== null) { g.setLineDash([7, 5]); g.lineDashOffset = -(v.live ? t : 0) * (10 + spin * 60); }
        for (const [cx0, T] of srcs) {
          const tt = v.live ? t : 2.2 * T, age0 = tt % T;
          for (let k = 0; k < 24; k++) {
            const age = age0 + k * T, a = keep ** (age / T) * (0.35 + 0.65 * mix);
            if (a < 0.04) break;
            const r = 4 + age / T * (8 + time * 10);
            if (r > LW) break;
            const cy = LH / 2 - rise * (age / T) * 9;
            g.globalAlpha = a; g.beginPath();
            if (square) g.rect(cx0 - r, cy - r, 2 * r, 2 * r);
            else if (sides) for (let j = 0; j <= sides; j++) { const an = j / sides * TAU - Math.PI / 2; j ? g.lineTo(cx0 + Math.cos(an) * r, cy + Math.sin(an) * r) : g.moveTo(cx0 + Math.cos(an) * r, cy + Math.sin(an) * r); }
            else g.arc(cx0, cy, r, 0, TAU);
            g.stroke();
          }
        }
        g.setLineDash([]);
      }
      if (verb) {
        const cx = delay ? 182 : LW / 2, cy = LH / 2 + 4, sc = 22 + size * 22;
        const ang = (v.live ? t * 0.25 : 0.6) + 0.6;
        const P = (x, y, z) => { const c = Math.cos(ang), s = Math.sin(ang), X = x * c - z * s, Z = x * s + z * c;
          return [cx + X * sc, cy + (y * 0.85 + Z * 0.38) * sc]; };
        const E = [[-1,-1,-1,1,-1,-1],[1,-1,-1,1,-1,1],[1,-1,1,-1,-1,1],[-1,-1,1,-1,-1,-1],[-1,1,-1,1,1,-1],[1,1,-1,1,1,1],
                   [1,1,1,-1,1,1],[-1,1,1,-1,1,-1],[-1,-1,-1,-1,1,-1],[1,-1,-1,1,1,-1],[1,-1,1,1,1,1],[-1,-1,1,-1,1,1]];
        g.globalAlpha = 0.9; g.lineWidth = 1.2; g.beginPath();
        for (const e of E) { const a = P(e[0], e[1], e[2]), b = P(e[3], e[4], e[5]); g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); }
        g.stroke();
        const L = 1 + Math.round(decay * 5);
        for (let k = 0; k < L; k++) {
          const y = -1 + (((v.live ? t * (0.08 + (1 - decay) * 0.2) : 0) + k / L) % 1) * 2;
          const pts = [P(-1, y, -1), P(1, y, -1), P(1, y, 1), P(-1, y, 1)];
          g.globalAlpha = (0.1 + 0.25 * tone) * (1 - Math.abs(y) * 0.6); pathOf(g, pts); g.fill();
        }
      }
      g.globalAlpha = 1;
    },

    // Modulation: a few waves, apart by Depth, moving at Rate.
    lfo(g, q, v, t) {
      const rate = q(R.rate, 0.4), depth = q(R.depth, 0.5), mix = q(R.mix, 0.6);
      const tt = v.live ? t : 0;
      if (v.kind === "rose") {                       // Flower: petals turning at Rate
        const k = 3 + Math.round(mix * 3), rot = tt * (0.15 + rate * 1.5);
        for (let L = 0; L < 3; L++) {
          g.globalAlpha = L ? 0.3 : 1; g.lineWidth = L ? 1 : 1.6; g.beginPath();
          for (let i = 0; i <= 360; i++) {
            const th = i / 360 * TAU, r = (50 - L * 9) * Math.abs(Math.cos(k * th + L * 0.25 * Math.sin(tt)));
            const x = LW / 2 + r * Math.cos(th + rot) * 1.5, y = LH / 2 + r * Math.sin(th + rot);
            i ? g.lineTo(x, y) : g.moveTo(x, y);
          }
          g.stroke();
        }
        g.globalAlpha = 1; return;
      }
      if (v.kind === "siren" || v.kind === "scope") {  // one trace: a siren's sweep, or a scope's
        const pitch = q(/^(pitch|gain)$/i, 0.5), tm = q(/^time$/i, 0.5);
        if (v.kind === "scope") { g.globalAlpha = 0.18; g.lineWidth = 1; g.beginPath();
          for (let x = 32; x < LW; x += 32) { g.moveTo(x, 0); g.lineTo(x, LH); } for (let y = 32; y < LH; y += 32) { g.moveTo(0, y); g.lineTo(LW, y); } g.stroke(); }
        g.globalAlpha = 1; g.lineWidth = 1.6; g.beginPath();
        let ph = 0;
        for (let x = 0; x <= LW; x += 1.5) {
          const f = v.kind === "siren" ? (0.02 + pitch * 0.06) * (1 + depth * 0.9 * Math.sin(tt * (0.5 + rate * 5) + x / LW * Math.PI))
                                        : (0.02 + (1 - tm) * 0.12);
          ph += f * 1.5;
          const amp = v.kind === "siren" ? 40 : 10 + pitch * 40;
          const y = LH / 2 + amp * (Math.sin(ph - tt * 4) + (v.kind === "scope" ? 0.3 * Math.sin(ph * 3 - tt * 4) : 0)) / (v.kind === "scope" ? 1.3 : 1);
          x ? g.lineTo(x, y) : g.moveTo(x, y);
        }
        g.stroke(); return;
      }
      const n = 2 + Math.round(mix * 3), sp = (v.live ? t : 0) * (0.2 + rate * 2.4);
      g.lineWidth = 1.4;
      for (let k = 0; k < n; k++) {
        g.globalAlpha = k ? 0.35 + 0.4 * (1 - k / n) : 1;
        g.beginPath();
        for (let x = 0; x <= LW; x += 2) {
          const u = x / LW;
          const y = LH / 2 + (10 + depth * 36) * Math.sin(TAU * (u * (1.5 + k * depth * 0.3) - sp - k * 0.12 * (0.3 + depth)))
            * (0.7 + 0.3 * Math.sin(TAU * (u * 0.5 + sp * 0.3)));
          x ? g.lineTo(x, y) : g.moveTo(x, y);
        }
        g.stroke();
      }
      g.globalAlpha = 1;
    },

    // Filters: the response, its peak at Cutoff (swept by Rate/Depth), Reso's bump.
    filter(g, q, v, t) {
      const cut = q(R.cut, 0.5), reso = q(R.reso, 0.3), rate = q(R.rate, 0), depth = q(R.depth, 0);
      const c = clamp(cut + depth * 0.35 * Math.sin((v.live ? t : 0) * (0.3 + rate * 4) * TAU) , 0.02, 0.98);
      const pts = [];
      for (let x = 0; x <= LW; x += 2) {
        const d = (x / LW - c) * 6;
        const lp = 1 / Math.sqrt(1 + Math.max(d, 0) ** 4), peak = reso * 1.4 / (1 + d * d * (8 + reso * 30));
        pts.push([x, LH - 12 - (LH - 34) * clamp((lp + peak) / 1.8 * 1.4, 0, 1.2)]);
      }
      g.save(); g.beginPath(); g.moveTo(0, LH); pts.forEach(([x, y]) => g.lineTo(x, y)); g.lineTo(LW, LH); g.closePath(); g.clip();
      g.lineWidth = 1; g.globalAlpha = 0.45; hatch(g, -0.8, 5, (v.live ? t : 0) * 8, 0, null); g.restore();
      g.globalAlpha = 1; g.lineWidth = 2; g.beginPath(); pts.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.stroke();
    },

    // Steps: a euclidean ring (EuGate) or flickering glitch blocks.
    steps(g, q, v, t, C, dt) {
      const s = v.s;
      if (v.kind === "euclid") {
        // the real counts: Steps around the ring, Notes spread evenly, Shift turns it, Tempo moves it
        const N = clamp(q.raw(/^steps$/i, 16), 1, 32), K = clamp(q.raw(/^notes$/i, 4), 0, N);
        const shift = q.raw(/^shift$/i, 0), gap = q(/^gap$/i, 0.3), bpm = clamp(q.raw(/^tempo$/i, 120), 40, 300);
        const pos = Math.floor((v.live ? t : 0) * bpm / 60 * 4) % N;
        g.globalAlpha = 0.25; g.lineWidth = 1; g.beginPath(); g.ellipse(LW / 2, LH / 2, 74, 46, 0, 0, TAU); g.stroke();
        for (let i = 0; i < N; i++) {
          const j = ((i - shift) % N + N) % N;
          const hit = K > 0 && Math.floor(j * K / N) !== Math.floor((j + N - 1) % N * K / N) || (K === N && K > 0);
          const a = i / N * TAU - Math.PI / 2, x = LW / 2 + Math.cos(a) * 74, y = LH / 2 + Math.sin(a) * 46;
          g.globalAlpha = i === pos ? 1 : hit ? 0.8 : 0.35; g.lineWidth = 1.2;
          g.beginPath(); g.arc(x, y, hit ? 6.5 - gap * 4 : 2.2, 0, TAU); hit ? g.fill() : g.stroke();
          if (i === pos) { g.beginPath(); g.arc(x, y, 9, 0, TAU); g.stroke(); }
        }
      } else {
        if (!s.r) { s.r = rng(v.e.id || 9); s.cells = new Float32Array(16 * 8); s.next = 0; }
        const chance = q(/^(chance|loss|burst|glitch)$/i, 0.4), smear = q(/^(smear|size|fill)$/i, 0.4);
        if (v.live && t >= s.next) { for (let i = 0; i < 128; i++) if (s.r() < chance * 0.35) s.cells[i] = 1; s.next = t + 0.08; }
        for (let i = 0; i < 128; i++) {
          s.cells[i] *= Math.exp(-dt * (2 + (1 - smear) * 10));
          const x = (i % 16) * 16, y = Math.floor(i / 16) * 16;
          g.globalAlpha = 0.05 + 0.95 * s.cells[i];
          g.fillRect(x + 2, y + 2, 12 + (s.cells[i] > 0.5 ? smear * 14 : 0), 12);
        }
      }
      g.globalAlpha = 1;
    },

    // Pitch: a line per voice at its interval, with sparks rising (shimmer) or falling.
    pitch(g, q, v, t, C, dt) {
      const s = v.s;
      if (!s.p) { s.p = []; s.r = rng(v.e.id || 4); s.next = 0; }
      const vals = (v.e.params || []).map((p, k) => /ptch|pitch|tune|chord|detune/i.test(p.name) ? q(new RegExp("^" + p.name.replace(/\W/g, "\\$&") + "$", "i"), 0.5) : null).filter(x => x !== null);
      // one knob: the dry note plus the voices it makes, spread by it
      const voices = vals.length > 1 ? vals : [0.3, clamp(0.5 + 0.3 * (vals[0] ?? 0.5)), clamp(0.62 + 0.33 * (vals[0] ?? 0.5))];
      const mix = q(R.mix, 0.6), wob = q(/^(detune|glide|feel|regen|annihil)$/i, 0.2), tt = v.live ? t : 0;
      voices.forEach((p, i) => {
        const y = LH - 16 - p * (LH - 32);
        for (let L = -1; L <= 1; L++) {                 // a band of three: the voice and its shimmer
          g.globalAlpha = L ? 0.3 : 0.95; g.lineWidth = L ? 1 : 1.8; g.beginPath();
          for (let x = 0; x <= LW; x += 3) {
            const yy = y + L * (2 + wob * 5) + (2 + wob * 6) * Math.sin(x * (0.03 + i * 0.01) + tt * (1.5 + i) + L);
            x ? g.lineTo(x, yy) : g.moveTo(x, yy);
          }
          g.stroke();
        }
        if (v.live && t >= s.next) { s.p.push({ x: s.r() * LW, y, vy: 12 + (p - 0.3) * 50 + s.r() * 10, b: t }); s.next = t + 0.05 / (0.3 + mix); }
      });
      s.p = s.p.filter(o => t - o.b < 1.4);
      for (const o of s.p) { const a = 1 - (t - o.b) / 1.4; g.globalAlpha = a; g.fillRect(o.x, o.y - o.vy * (t - o.b), 3, 3); }
      g.globalAlpha = 1;
    },

    // Dynamics: the transfer curve with its knee, and a level bouncing on it.
    dyn(g, q, v, t) {
      const thr = q(/^(thrsh|threshold|sense|sens)$/i, 0.5), ratio = q(/^(ratio|attack|comp)$/i, 0.5);
      const x0 = 60, y0 = LH - 10, S = 100, k = 1 - thr * 0.8, rr = 1 + ratio * 9;
      g.globalAlpha = 0.35; g.lineWidth = 1; g.strokeRect(x0, y0 - S, S, S);
      g.globalAlpha = 1; g.lineWidth = 2; g.beginPath();
      for (let i = 0; i <= S; i += 2) { const u = i / S, o = u < k ? u : k + (u - k) / rr; i ? g.lineTo(x0 + i, y0 - o * S) : g.moveTo(x0, y0); }
      g.stroke();
      const lvl = 0.5 + 0.45 * Math.abs(Math.sin((v.live ? t : 0.4) * 2.2)) ;
      const o = lvl < k ? lvl : k + (lvl - k) / rr;
      g.beginPath(); g.arc(x0 + lvl * S, y0 - o * S, 3.5, 0, TAU); g.fill();
      g.fillRect(180, y0 - o * S, 12, o * S);
    },

    // Carillon, after the S-4's filter-bank screen: a tall dim bar per band
    // is the filter's shape (Cutoff, Reso, Slope; Waves rolls through it),
    // and a bright bar inside it is what comes through: the playing, filtered,
    // plus the bands it strikes ringing like bells (a shimmer) for as long as
    // Decay says. Noise strikes more. Bands the Scale leaves out sit as dots.
    bells(g, q, v, t, C, dt) {
      const s = v.s, N = 32, NZ = noiseTex();
      if (!s.e) { s.e = new Float32Array(N); s.lv = new Float32Array(N); s.r = rng(v.e.id || 7); s.next = 0; s.env = 0; }
      const cut = q(R.cut, 0.7), reso = q(R.reso, 0.3), decay = q(R.decay, 0.55);
      const slope = q(/^slope$/i, 0.4), waves = q(/^waves$/i, 0.2), noise = q(R.noise, 0), mix = q(R.mix, 0.6);
      const on = bandsOn(q.raw(/^scale$/i, 2), q.raw(/^key$/i, 0), N);
      const strike = (i, a) => { if (i >= 0 && i < N && on[i]) s.e[i] = Math.max(s.e[i], a); };
      if (v.live && t >= s.next) {                 // a note: its bands, and a few above
        s.env = 1; s.next = t + 0.3 + s.r() * 0.9;
        const k = 4 + Math.floor(s.r() * 22);
        strike(k, 0.7 + 0.3 * s.r()); strike(k + 7, 0.55); strike(k + 12, 0.4); if (s.r() < 0.5) strike(k + 4, 0.45);
      }
      if (v.live && s.r() < noise * 14 * dt) strike(Math.floor(s.r() * N), 0.4 + 0.6 * s.r() * noise);
      s.env *= Math.exp(-dt / 0.35);
      const tau = 0.06 + 3 * decay * decay;
      for (let i = 0; i < N; i++) s.e[i] *= Math.exp(-dt / tau);
      const rsn = reso * reso * 3, cutSemi = cut * 78 - 3, depth = clamp(2 * waves);
      const ph = t * (0.12 + 2.38 * clamp(2 * waves - 1) ** 2);
      const fall = 1 - Math.exp(-dt / (0.04 + 0.35 * decay));
      const x0 = 6, bw = (LW - 12) / N, floor = 122, top = 10, bg = rgbOf(C.bg), fg = rgbOf(C.fg);
      const T = tones(C), wet = 0.45 + 0.55 * mix;
      const dim = css(lerp3(bg, T.hi, 0.36)), dot = css(lerp3(bg, T.lo, 0.3)); void fg;
      const tn = Math.floor(t * 24);
      for (let i = 0; i < N; i++) {
        const x = Math.round(x0 + i * bw + bw * 0.18), w = Math.max(1, Math.round(bw * 0.64));
        if (!on[i]) { g.fillStyle = dot; g.fillRect(x, floor - 2, w, 2); continue; }
        const dl = (i * 72 / 31 - cutSemi) / 12, roll = z => 1 / (1 + z ** 4);
        const lp = roll(Math.max(dl, 0) * 1.6), hp = roll(Math.max(-dl, 0) * 1.6), bp = roll(Math.abs(dl) * 1.1);
        let sh = slope < 0.5 ? lp + (bp - lp) * slope * 2 : bp + (hp - bp) * (slope * 2 - 1);
        sh += rsn / (1 + 9 * dl * dl);
        const wv = 0.5 + 0.25 * (Math.sin(TAU * (ph + i * 0.131)) + Math.sin(TAU * (ph * 0.63 + 3 - i * 0.049)));
        sh = clamp(sh * (1 - depth * wv) * (1 + 0.5 * depth) / (1 + rsn), 0.03, 1.05);
        const hMax = (floor - top) * sh;
        // what comes through: a moving input spectrum, through the filter, plus the ringing
        const inp = NZ[((i * 9) & 255) * NS + ((tn + i * 31) & 255)];
        const thru = (0.12 + 0.5 * inp * (0.35 + 0.65 * s.env));
        const ring = s.e[i] * (0.8 + 0.2 * Math.sin(t * (9 + i * 0.7) + i));
        const want = Math.min(1, Math.max(thru, ring));
        s.lv[i] += (want - s.lv[i]) * (want > s.lv[i] ? 0.55 : fall);
        g.fillStyle = dim; g.fillRect(x, Math.round(floor - hMax), w, Math.round(hMax));
        const h = Math.max(2, Math.round(hMax * s.lv[i]));
        // what comes through is pale; a struck band ringing glows in the colour
        const rs = clamp((ring - thru * 0.8) / (want + 1e-3) * 2);
        g.fillStyle = css(lerp3(bg, lerp3(T.lo, T.hi, rs), wet)); g.fillRect(x, floor - h, w, h);
      }
    },

    // Droplets: the last 2.5 s along the bottom (now at the right; it stops
    // when Feedbk locks it), and the grains above it at the place they read
    // and the pitch they play: each one drawn as its envelope (Contour), as
    // wide as its Size, hatched when reversed. Rate is how many; Scan where
    // they start or how the start travels; Random sprays and jumps them;
    // Pattern steps their pitch. Grains are written back into the strip as
    // Feedbk rises.
    grains(g, q, v, t, C, dt) {
      const s = v.s;
      if (!s.wt) { s.r = rng(v.e.id || 11); s.wt = waveTable(s.r, 1024); s.gr = []; s.off = 0; s.next = 0; s.step = 0; s.sx = 200; }
      const SEMI = [-24, -12, -7, -5, -3, 0, 3, 5, 7, 12, 24];
      const PAT = [[0], [0, 12], [0, -12], [0, 7], [0, 7, 12], [0, 4, 7, 12], [0, 3, 7, 12], [0, -5, -12], [0, 5, 7, 12],
        "pent", "wide"];
      const PENT = [0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24], WIDE = [-12, -5, 0, 7, 12, 19, 24];
      const pitch = SEMI[clamp(q.raw(/^pitch$/i, 5), 0, 10)] ?? 0, pat = PAT[clamp(q.raw(/^pattern$/i, 0), 0, 10)] || [0];
      const rate = q(R.rate, 0.45), size = q(/^size$/i, 0.4), cont = q(/^contour$/i, 0.5), scan = q(/^scan$/i, 0.25);
      const rnd = q(/^random$/i, 0.2), fb = q(R.fb, 0.3), mix = q(R.mix, 0.5);
      const lock = q.raw(R.fb, 30) >= 100;
      const L = 8, Rx = 248, PX = (Rx - L) / 2.5;            // 2.5 s across
      const T = tones(C), bg = T.bg, col = (a, c = T.lo) => css(lerp3(bg, c, a));
      // the buffer scrolls left as you play (not when locked); the scan point
      if (v.live && !lock) s.off += dt * PX;
      if (scan < 0.5) { const d = 0.03 + (scan / 0.5) ** 2 * 2.47; s.sx = Rx - d * PX; }
      else if (v.live) {
        s.sx += ((scan - 0.5) * 4 - (lock ? 0 : 1)) * PX * dt;
        if (s.sx < L) s.sx += Rx - L; if (s.sx > Rx) s.sx -= Rx - L;
      }
      // grains
      if (v.live && t >= s.next) {
        const spray = Math.min(rnd, 0.33) / 0.33 * 0.5 * PX * (s.r() * 2 - 1);
        let semi = pitch;
        if (pat === "pent") semi += PENT[Math.floor(s.r() * PENT.length)] - 12;
        else if (pat === "wide") semi += WIDE[Math.floor(s.r() * WIDE.length)];
        else semi += pat[s.step++ % pat.length];
        if (rnd > 0.33 && s.r() < (rnd - 0.33) * 1.5) semi += [-12, -5, 7, 12][Math.floor(s.r() * 4)];
        const len = (0.01 + 0.24 * size * size) * (1 + (rnd > 0.33 ? (s.r() - 0.5) * rnd : 0));
        const rev = rnd > 0.66 && s.r() < (rnd - 0.66) / 0.34 * 0.5;
        let x = s.sx + spray; if (x < L) x += Rx - L; if (x > Rx) x -= Rx - L;
        s.gr.push({ x, semi: clamp(semi, -36, 36), w: Math.max(5, len * PX * 1.8), a: 0.55 + 0.45 * (rnd > 0.33 ? s.r() : 1), born: t, rev,
          life: 0.45 + len * 4 });
        if (s.gr.length > 60) s.gr.shift();
        const gps = 2 * Math.pow(30, rate);
        s.next = t + (1 / gps) * (1 + (rnd > 0.33 ? (s.r() - 0.5) * rnd : 0));
      }
      if (v.live) s.gr = s.gr.filter(gr => {
        const alive = t - gr.born < gr.life;
        if (!alive && fb > 0.02 && !lock) {                 // written back into the buffer, at now
          const at = Math.floor(s.off + Rx - L) % s.wt.n;
          for (let k = 0; k < gr.w; k += 2) {
            const i = (at - Math.round(gr.w) + k + s.wt.n) % s.wt.n, e = Math.sin(Math.PI * k / gr.w) * gr.a * fb * (0.55 + 0.45 * s.r());
            s.wt.top[i] = Math.max(s.wt.top[i], e); s.wt.bot[i] = Math.max(s.wt.bot[i], e * (0.6 + 0.4 * s.r()));
          }
        }
        return alive;
      });
      // the strip
      const cy = 110, hh = 12;
      g.fillStyle = col(0.42 + 0.25 * mix);
      for (let x = L; x < Rx; x += 2) {
        const i = ((Math.floor(x - L + s.off) % s.wt.n) + s.wt.n) % s.wt.n;
        const a = s.wt.top[i] * hh, b = s.wt.bot[i] * hh;
        g.fillRect(x, Math.round(cy - a), 1, Math.max(1, Math.round(a + b)));
      }
      if (lock) {                                           // locked: a bracket round the frozen strip
        g.strokeStyle = col(0.8); g.lineWidth = 1; g.setLineDash([3, 3]);
        g.strokeRect(L - 3.5, cy - hh - 3.5, Rx - L + 7, hh * 2 + 7); g.setLineDash([]);
        g.fillStyle = col(0.8); g.font = "bold 8px ui-monospace, monospace"; g.fillText("LOCK", L - 2, cy - hh - 6);
      }
      // pitch guides, the scan head
      const py = semi => 52 - semi * 1.25;
      g.fillStyle = col(0.22);
      for (const sm of [-12, 0, 12]) for (let x = L; x < Rx; x += 4) g.fillRect(x, Math.round(py(pitch + sm)), 1, 1);
      g.fillStyle = col(0.9);
      g.fillRect(Math.round(s.sx), cy - hh - 4, 1, hh * 2 + 8);
      // the grains, each as its envelope
      const envAt = u => {
        const hit = Math.exp(-u * 5) * (1 - Math.exp(-u * 60)) * 1.15, hann = Math.sin(Math.PI * u) ** 2;
        const swell = u < 0.9 ? (u / 0.9) ** 2.5 : (1 - u) / 0.1;
        return cont < 0.5 ? hit + (hann - hit) * cont * 2 : hann + (swell - hann) * (cont - 0.5) * 2;
      };
      for (const gr of s.gr) {
        const age = clamp((t - gr.born) / gr.life), fade = (1 - age) ** 1.5;
        if (fade < 0.02) continue;
        // each grain is a little waveform: its envelope (Contour) as the bars'
        // heights, as wide as its Size; reversed ones run backwards, pale
        const y = Math.round(py(gr.semi)), H = 2 + 6 * gr.a, x0 = Math.round(gr.x - gr.w / 2);
        g.fillStyle = col((0.3 + 0.7 * fade) * (0.5 + 0.5 * mix), gr.rev ? T.lo : T.hi);
        const seed = (gr.born * 1000) | 0;
        for (let k = 0; k < gr.w; k += 2) {
          const u = k / gr.w, e = envAt(gr.rev ? 1 - u : u) * (0.7 + 0.3 * Math.abs(Math.sin(seed + k * 1.7)));
          const hgt = Math.max(1, Math.round(e * H));
          g.fillRect(x0 + k, y - hgt, 1, hgt * 2);
        }
        if (age < 0.2) { g.fillStyle = col(0.3 * (1 - age * 5), T.hi); g.fillRect(Math.round(gr.x), y + H + 2, 1, Math.max(0, cy - hh - y - H - 2)); }   // where it reads
      }
    },

    // Galactic: a galaxy, tilted, turning. Its size is Bigness; each note
    // sends a bright ring out through it (the tail building); Replace sets how
    // long the trails hang on (low: a long smeared tail, high: quick
    // turnover); Detune makes the arms waver; Bright makes the stars sharp and

    // Chimera (the pedal effect): the airway from the glottis (left) to the lips (right), as
    // wide as each section is (the same fitted vowel shapes as chimera.c),
    // morphing with Vowel and Open, as long as Body says. Sound fronts run
    // from the glottis to the lips and leave as arcs; the nose branches off
    // the soft palate when Nasal is up. Move moves the mouth as the pedal
    // does: each note (Pick), a sway (Slow, Fast) or babble (Chat).
    throat(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.r) { s.r = rng(v.e.id || 17); s.env = 0; s.next = 0; s.ph = 0; s.vt = 2; s.vc = 2; s.fronts = []; s.lna = null; }
      const vowel = q(/^vowel$/i, 0.5), open = q(/^open$/i, 0.55), body = q(/^body$/i, 0.3);
      const nasal = q(/^nasal$/i, 0), talk = q(/^talk$/i, 0.35), mix = q(R.mix, 1);
      const move = q.raw(/^move$/i, 0), buzz = q(/^buzz$/i, 0.3), breath = q(/^breath$/i, 0);
      // the playing: notes now and then
      if (v.live && t >= s.next) { s.env = 1; s.next = t + 0.35 + s.r() * 0.9; s.fronts.push({ x: 0, a: 1 }); }
      s.env *= Math.exp(-dt / 0.35);
      let vp = vowel * 4, op = open;
      if (move === 0) { vp += talk * 2.4 * s.env; op += talk * 0.6 * s.env; }
      else if (move < 3) { s.ph = (s.ph + dt * (move === 1 ? 0.35 : 2.2)) % 1; let x = s.ph * 4 - 1; if (x > 1) x = 2 - x; vp += talk * 2 * x * (1.5 - 0.5 * x * x); }
      else {
        s.ph += dt * 4.5;
        if (s.ph >= 1) { s.ph -= 1; s.vt = vp + talk * 4 * (s.r() * 2 - 1); }
        s.vc += (s.vt - s.vc) * Math.min(1, dt * 30);
        vp = s.vc; op = open * (1 - talk * (1 - 4 * s.ph * (1 - s.ph)));
      }
      vp = clamp(vp, 0, 3.999); op = clamp(op);
      const VT = THROAT_VOWELS, vi = Math.floor(vp), fr = vp - vi, N = 16;
      const tgt = VT[vi].map((a, k) => { const w = k / (N - 1); return Math.max(-3, a + (VT[vi + 1][k] - a) * fr + (op - 0.55) * 2.6 * w * w * w); });
      if (!s.lna) s.lna = tgt.slice();
      for (let k = 0; k < N; k++) s.lna[k] += (tgt[k] - s.lna[k]) * Math.min(1, dt * 18);
      // geometry
      const len = 19 - 5.5 * body, x0 = 18, cy = 70, sx = (LW - 64) / 19 * len / N;
      const rad = s.lna.map(l => Math.min(30, 2 + Math.sqrt(Math.exp(l)) * 8));
      const xs = k => x0 + k * sx;
      const top = [], bot = [];
      for (let k = 0; k < N; k++) { top.push([xs(k), cy - rad[k]], [xs(k + 1), cy - rad[k]]); bot.push([xs(k), cy + rad[k]], [xs(k + 1), cy + rad[k]]); }
      const smooth = pts => { g.moveTo(pts[0][0], pts[0][1]); for (let i = 1; i < pts.length - 1; i++) { const mx = (pts[i][0] + pts[i + 1][0]) / 2, my = (pts[i][1] + pts[i + 1][1]) / 2; g.quadraticCurveTo(pts[i][0], pts[i][1], mx, my); } g.lineTo(pts[pts.length - 1][0], pts[pts.length - 1][1]); };
      const xl = xs(N), kv = 8, xv = xs(kv) + sx / 2;
      // the nose: a branch up from the soft palate to nostrils at the top right
      if (nasal > 0.02) {
        const wv = 2 + nasal * 7, ny = 16;
        g.strokeStyle = col(0.45 + 0.5 * mix, T.hi); g.lineWidth = 1.6;
        g.beginPath(); g.moveTo(xv - wv, cy - rad[kv]); g.quadraticCurveTo(xv - wv, ny, xv + 30, ny - 4); g.lineTo(xl - 6, ny - 4); g.stroke();
        g.beginPath(); g.moveTo(xv + wv, cy - rad[kv]); g.quadraticCurveTo(xv + wv, ny + 12, xv + 30, ny + 8); g.lineTo(xl - 6, ny + 8); g.stroke();
        for (let i = 0; i < 3; i++) {                  // sound out of the nostrils
          const ph = ((t * 1.3 + i / 3) % 1), r = 4 + ph * 18;
          g.strokeStyle = col((1 - ph) * nasal * (0.3 + 0.7 * s.env), T.lo); g.lineWidth = 1;
          g.beginPath(); g.arc(xl - 6, ny + 2, r, -0.7, 0.7); g.stroke();
        }
      }
      // the sound inside: fronts running glottis -> lips, as tall as the tube
      const speed = 0.9 + 0.6 * buzz;
      for (const f of s.fronts) f.x += dt * speed;
      s.fronts = s.fronts.filter(f => f.x < 1.4);
      const steady = 0.18 + 0.5 * buzz;               // the guitar keeps sounding between notes
      for (let k = 0; k < N; k++) {
        let lit = steady * (0.5 + 0.5 * Math.sin(t * 9 - k * 0.8)) * (0.3 + 0.7 * s.env);
        for (const f of s.fronts) { const d = f.x * N - k; lit += Math.exp(-d * d / 2.5) * f.a; }
        const h = rad[k] * Math.min(1, lit) * 0.85;
        if (h < 0.6) continue;
        g.fillStyle = col(0.35 + 0.65 * mix, T.lo);
        g.fillRect(xs(k) + sx * 0.3, cy - h, Math.max(1.5, sx * 0.4), 2 * h);
      }
      if (breath > 0.02) {                            // air: specks drifting through
        g.fillStyle = col(0.6 * breath, T.lo);
        for (let i = 0; i < 26 * breath; i++) { const u = (s.r() + t * 0.6) % 1, k = Math.floor(u * N); g.fillRect(xs(0) + u * N * sx, cy + (s.r() - 0.5) * rad[k] * 1.6, 1, 1); }
      }
      // the walls
      g.strokeStyle = col(0.55 + 0.45 * mix, T.hi); g.lineWidth = 2;
      g.beginPath(); smooth(top); g.stroke(); g.beginPath(); smooth(bot); g.stroke();
      g.beginPath(); g.moveTo(x0, cy - rad[0]); g.lineTo(x0, cy + rad[0]); g.stroke();   // the glottis
      if (nasal > 0.02) { g.strokeStyle = C.bg; g.lineWidth = 3; g.beginPath(); g.moveTo(xv - 1, cy - rad[kv]); g.lineTo(xv + 1, cy - rad[kv]); g.stroke(); }
      // sound leaving the lips
      for (let i = 0; i < 4; i++) {
        const ph = ((t * 1.6 + i / 4) % 1), r = rad[N - 1] * 0.6 + ph * 34;
        g.strokeStyle = col((1 - ph) * (0.25 + 0.75 * s.env) * (0.4 + 0.6 * mix), T.lo); g.lineWidth = 1.3;
        g.beginPath(); g.arc(xl, cy, r, -0.9, 0.9); g.stroke();
      }
    },

    // Rewire: its five blocks as tiles, in the order Route picks (they slide
    // to their new places when Route changes). Each tile shows a test
    // waveform as it is at that point in the chain, so the order shows: crush
    // then drive is not drive then crush. A bar under each tile is its knob;
    // a block at 0 is dim (off). Tone closes the chain; the dry signal runs
    // over the top, stronger as Mix goes down.
    rewire(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const ORDERS = [[0, 1, 2, 3, 4], [4, 3, 2, 1, 0], [1, 4, 0, 3, 2], [2, 3, 4, 0, 1], [4, 0, 2, 1, 3]];
      const NAMES = ["BITS", "DRIVE", "RING", "COMB", "SHIFT"], LET = "BDRCS";
      const rt = q(/^route$/i, 0.1), ri = Math.min(4, Math.floor(rt * 5 + 1e-6));
      const amt = [q(/^bits$/i, 0.55), q(/^drive$/i, 0.35), q(/^ring$/i, 0.2), q(/^comb$/i, 0.25), Math.abs(q(/^shift$/i, 0.5) - 0.5) * 2];
      const shiftSign = q(/^shift$/i, 0.5) >= 0.5 ? 1 : -1;
      const tone = q(/^tone$/i, 0.55), mix = q(R.mix, 0.5);
      const order = ORDERS[ri];
      if (!s.pos) { s.pos = [0, 1, 2, 3, 4]; order.forEach((b, i) => { s.pos[b] = i; }); s.ph = 0; s.sh = 0; }
      order.forEach((b, i) => { s.pos[b] += (i - s.pos[b]) * Math.min(1, dt * 7); });
      if (v.live) { s.ph += dt * 0.6; s.sh += dt * shiftSign * amt[4] * 3; }
      // layout
      const n = 5, tw = 34, gap = 5, x0 = 18, ty = 40, th = 44, cy = ty + th / 2;
      const tx = i => x0 + i * (tw + gap);
      const xT = tx(n) + 2, xOut = xT + 30;
      // the test signal through the chain, in route order: 64 points per stage
      const M = 64;
      let sig = new Float32Array(M);
      for (let i = 0; i < M; i++) { const u = i / M * 2 * Math.PI * 2 + s.ph * 6; sig[i] = 0.75 * Math.sin(u) + 0.18 * Math.sin(3 * u + 0.4); }
      const stage = (b, x) => {
        const a = amt[b], y = new Float32Array(M);
        if (b === 0) {                                 // Bits: fewer levels, held samples
          const lv = [4096, 1024, 256, 64, 24, 8][Math.min(5, Math.floor(a / 0.17))], hold = 1 + Math.floor(a * a * 7);
          for (let i = 0; i < M; i++) { const j = i - (i % hold); y[i] = Math.round(x[j] * lv) / lv; }   // lv: levels per unit, as rewire.c
        } else if (b === 1) {                          // Drive: an uneven soft clip
          const k = 1 + a * 9; for (let i = 0; i < M; i++) { const z = k * x[i] + 0.15 * a; y[i] = Math.tanh(z) - Math.tanh(0.15 * a); }
        } else if (b === 2) {                          // Ring: times a carrier, fading in
          const f = 1 + a * a * 14, d = a < 0.03 ? 0 : a; for (let i = 0; i < M; i++) y[i] = x[i] * (1 - d + d * Math.sin(i / M * 2 * Math.PI * f + s.ph * 9));
        } else if (b === 3) {                          // Comb: a short echo fed back
          const L = 3 + Math.floor(a * 20); for (let i = 0; i < M; i++) y[i] = x[i] + (i >= L ? a * 0.8 * y[i - L] : 0); for (let i = 0; i < M; i++) y[i] /= 1 + a * 0.8;
        } else {                                       // Shift: the waveform slides, an extra partial joins
          for (let i = 0; i < M; i++) { const j = (i + Math.round(s.sh * 8) % M + M) % M; y[i] = x[j] * (1 - a * 0.3) + a * 0.3 * Math.sin(i / M * 2 * Math.PI * 7 + s.sh * 4); }
        }
        return y;
      };
      const out = {};
      order.forEach(b => { sig = stage(b, sig); out[b] = sig; });
      // wiring: in -> tiles -> tone -> out, with the dry path over the top
      g.lineWidth = 1.2;
      g.strokeStyle = col(0.25 + 0.6 * (1 - mix), T.lo);
      g.beginPath(); g.moveTo(6, cy); g.lineTo(10, cy); g.lineTo(10, 22); g.lineTo(xOut, 22); g.lineTo(xOut, cy); g.stroke();
      g.strokeStyle = col(0.3 + 0.6 * mix, T.hi);
      g.beginPath(); g.moveTo(6, cy); g.lineTo(xT, cy); g.moveTo(xT + 26, cy); g.lineTo(xOut + 6, cy); g.stroke();
      // signal dots running along the chain
      g.fillStyle = col(0.7 * mix + 0.2, T.lo);
      for (let k = 0; k < 6; k++) { const u = ((t * 0.35 + k / 6) % 1); g.fillRect(6 + u * (xOut - 6), cy - 1, 2, 2); }
      // the tiles
      g.font = "bold 7px ui-monospace, monospace"; g.textAlign = "center";
      for (let b = 0; b < 5; b++) {
        const x = tx(s.pos[b]), on = amt[b] > 0.03, a = on ? 1 : 0.35;
        g.fillStyle = C.bg; g.fillRect(x, ty, tw, th);
        g.strokeStyle = col(on ? 0.9 : 0.35, T.hi); g.lineWidth = on ? 1.5 : 1; g.strokeRect(x + 0.5, ty + 0.5, tw - 1, th - 1);
        const y = out[b];
        g.strokeStyle = col(a * (0.5 + 0.5 * mix), T.lo); g.lineWidth = 1.2; g.beginPath();
        for (let i = 0; i < M; i++) { const px = x + 3 + i / (M - 1) * (tw - 6), py = cy + 4 - clamp(y[i], -1.2, 1.2) * 12; i ? g.lineTo(px, py) : g.moveTo(px, py); }
        g.stroke();
        g.fillStyle = col(on ? 0.95 : 0.4, T.hi); g.fillText(LET[b], x + tw / 2, ty + 9);
        g.fillStyle = col(0.25, T.lo); g.fillRect(x, ty + th + 4, tw, 2);              // amount
        g.fillStyle = col(on ? 0.9 : 0.4, T.hi); g.fillRect(x, ty + th + 4, tw * amt[b], 2);
        g.fillStyle = col(0.55, T.lo); g.font = "6px ui-monospace, monospace"; g.fillText(NAMES[b], x + tw / 2, ty + th + 13); g.font = "bold 7px ui-monospace, monospace";
      }
      // Tone: a low-pass curve
      g.strokeStyle = col(0.9, T.hi); g.lineWidth = 1.5; g.strokeRect(xT + 0.5, ty + 8.5, 25, th - 16);
      g.strokeStyle = col(0.9, T.lo); g.lineWidth = 1.2; g.beginPath();
      for (let i = 0; i <= 20; i++) { const f = i / 20, h = 1 / Math.sqrt(1 + Math.pow(f / (0.08 + tone * 0.9), 4)); const px = xT + 3 + f * 20, py = ty + th - 11 - h * 18; i ? g.lineTo(px, py) : g.moveTo(px, py); }
      g.stroke();
      g.fillStyle = col(0.55, T.lo); g.font = "6px ui-monospace, monospace"; g.fillText("TONE", xT + 13, ty + th + 13);
      // the route's name, and the five to choose from
      g.textAlign = "left"; g.font = "bold 9px ui-monospace, monospace";
      g.fillStyle = col(0.95, T.hi); g.fillText("ROUTE " + order.map(b => LET[b]).join(""), 6, 12);
      for (let i = 0; i < 5; i++) { g.fillStyle = col(i === ri ? 0.95 : 0.3, i === ri ? T.hi : T.lo); g.fillRect(118 + i * 8, 7, 5, 5); }
      g.textAlign = "start";
    },

    // Horizon: its eight delay lines as eight orbits around the horizon (as
    // on the pedal), as wide as Size makes them, stretched by Width, wobbling
    // with Mod. A note lights them after the pre-delay; the light dies away
    // as Decay says (or stays, held, at 100), blooming in slowly with Swell.
    // Hi brightens the outer orbits, Lo thins the inner ones.
    horizon(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const size = q(/^size$/i, 0.7), decay = q(R.decay, 0.6), pre = q(/^pre$/i, 0.15), mod = q(/^mod$/i, 0.35);
      const lo = q(/^lo$/i, 0.2), hi = q(/^hi$/i, 0.55), swell = q(/^swell$/i, 0), width = q(/^width$/i, 0.8), mix = q(R.mix, 0.45);
      const hold = q.raw(R.decay, 60) >= 100;
      const BASE = [1671, 2039, 2437, 2851, 3361, 3967, 4603, 5323];
      if (!s.init) { s.init = 1; s.e = 0; s.next = 0; s.pend = []; s.ang = BASE.map((_, k) => k * 0.785); s.bl = 1; s.r = rng(v.e.id || 23); }
      if (v.live && t >= s.next && !hold) { s.pend.push(t + pre * 0.37); s.next = t + 0.8 + s.r() * 1.6; }
      while (s.pend.length && s.pend[0] <= t) { s.pend.shift(); s.e = Math.min(1.4, s.e + 0.7); s.bl = swell > 0.02 ? 0 : 1; }
      const rt = 0.8 * Math.pow(2, decay * 6.2288);
      if (!hold) s.e *= Math.exp(-dt * 6.9 / rt);
      else s.e = Math.max(s.e, 0.75);                  // held: the tail keeps glowing
      s.bl += (1 - s.bl) * Math.min(1, dt / (0.05 + swell * 2));
      const scale = 0.35 * Math.pow(2, size * 2.1186), maxL = BASE[7] * 1.52;
      const cx = LW / 2, cy = LH / 2 + 4;
      // the horizon: a dark disc with a thin bright rim, an accretion line through it
      const hr = 6 + size * 8;
      g.fillStyle = col(0.9 * mix + 0.1, T.hi); g.fillRect(cx - hr * 2.4, cy - 0.6, hr * 4.8, 1.2);
      g.fillStyle = C.bg; g.beginPath(); g.arc(cx, cy, hr, 0, TAU); g.fill();
      g.strokeStyle = col(0.5 + 0.5 * Math.min(1, s.e * s.bl), T.hi); g.lineWidth = 1.4; g.beginPath(); g.arc(cx, cy, hr, 0, TAU); g.stroke();
      for (let k = 0; k < 8; k++) {
        const rr = BASE[k] * scale / maxL, rx = (hr + 8 + rr * 92) * (0.75 + 0.35 * width), ry = 4 + rr * 30;
        const wob = 1 + mod * 0.06 * Math.sin(t * (0.6 + k * 0.17) + k);
        // light in this orbit: the tail's energy, the cuts thinning the ends
        const cut = (k < 3 ? 1 - lo * (3 - k) * 0.28 : 1) * (k > 4 ? 0.45 + 0.55 * hi : 1);
        const lit = clamp(s.e * s.bl * cut);
        g.strokeStyle = col(0.22 + 0.4 * lit, T.lo); g.lineWidth = 1;
        g.setLineDash([1.5, 3]); g.beginPath(); g.ellipse(cx, cy, rx * wob, ry * wob, 0, 0, TAU); g.stroke(); g.setLineDash([]);
        if (v.live) s.ang[k] += dt * 0.9 * BASE[0] / (BASE[k] * scale) * (0.4 + mod);
        for (let j = 0; j < 4; j++) {                   // the dot and its trail
          const a = s.ang[k] - j * 0.09;
          const x = cx + Math.cos(a) * rx * wob, y = cy + Math.sin(a) * ry * wob;
          if (Math.sin(a) < 0 && Math.abs(Math.cos(a)) * rx < hr) continue;   // behind the horizon
          g.fillStyle = col((0.45 + 0.55 * lit) * (1 - j * 0.22), j ? T.lo : T.hi);
          const sz = (j ? 1.4 : 2.6) * (0.6 + 0.6 * lit);
          g.beginPath(); g.arc(x, y, sz, 0, TAU); g.fill();
        }
      }
      g.fillStyle = col(0.85, T.hi); g.font = "bold 9px ui-monospace, monospace";
      g.fillText(hold ? "HOLD" : (rt < 10 ? rt.toFixed(1) : Math.round(rt)) + " s", 6, 12);
    },

    // Cloud: its eight delay lines as eight puffs of one cloud (as on the
    // pedal), spread wider with Size, drifting with Mod. A note arrives after
    // the pre-delay as a spray of drops that diffuse into the puffs: with
    // little Diff a few big drops you can follow (the echoes), with a lot a
    // fine mist and soft-edged puffs. The glow dies away as Decay says (or
    // stays, held, at 100), blooming in with Swell; Tone lights the tops,
    // Lo the shadow under the cloud.
    cloud(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const size = q(/^size$/i, 0.65), decay = q(R.decay, 0.6), pre = q(/^pre$/i, 0.1), diff = q(/^diff$/i, 0.75);
      const mod = q(/^mod$/i, 0.4), tone = q(/^tone$/i, 0.5), lo = q(/^lo$/i, 0.2), swell = q(/^swell$/i, 0), mix = q(R.mix, 0.45);
      const hold = q.raw(R.decay, 60) >= 100;
      if (!s.init) { s.init = 1; s.e = Array(8).fill(0.15); s.next = 0; s.pend = []; s.drops = []; s.bl = 1; s.r = rng(v.e.id || 31); }
      const spread = 13 + 11 * size, cy = LH / 2 + 6, base = cy + 12;
      const px = k => LW / 2 + (k - 3.5) * spread + mod * 3 * Math.sin(t * (0.25 + 0.5 * mod) + k * 1.7);
      const py = k => base - 4 - 14 * Math.sin(Math.PI * (k + 0.5) / 8) + (k & 1 ? 3 : -2) + mod * 2.5 * Math.sin(t * (0.2 + 0.4 * mod) + k);
      if (v.live && t >= s.next && !hold) { s.pend.push(t + pre * 0.37); s.next = t + 0.9 + s.r() * 1.6; }
      while (s.pend.length && s.pend[0] <= t) {        // a note: a spray of drops, fewer and bigger with less Diff
        s.pend.shift(); s.bl = swell > 0.02 ? 0 : 1;
        const n = Math.round(5 + 30 * diff), life = 0.25 + 0.45 * diff;
        for (let i = 0; i < n; i++) s.drops.push({ k: (s.r() * 8) | 0, t0: t + s.r() * 0.15 * diff, life: life * (0.6 + 0.8 * s.r()),
          x: 4 + s.r() * 14, y: base - 6 - s.r() * 22, sz: 3 - 2.2 * diff + s.r() * 0.6 });
      }
      const rt = Math.pow(2, decay * 6.4919);
      for (let k = 0; k < 8; k++) {
        if (!hold) s.e[k] *= Math.exp(-dt * 6.9 / rt); else s.e[k] = Math.max(s.e[k], 0.7);
      }
      s.bl += (1 - s.bl) * Math.min(1, dt / (0.05 + swell * 2));
      const amb = 0.35 + 0.65 * mix, rgba = (c, a) => `rgba(${c.join(",")},${clamp(a).toFixed(3)})`;
      // the shadow under the cloud: the lows, longer the lower Lo
      const sh = clamp(s.e.reduce((a, b) => a + b, 0) / 8) * (1 - 0.75 * lo);
      g.fillStyle = col(0.1 + 0.3 * sh * amb, T.lo);
      g.beginPath(); g.ellipse(LW / 2, base + 12, 4 * spread + 14, 3 + 3 * sh, 0, 0, TAU); g.fill();
      // the puffs, one cloud with a flat base: soft-edged with Diff, as big and bright as their line's sound
      g.save(); g.beginPath(); g.rect(0, 0, LW, base); g.clip();
      for (let pass = 0; pass < 2; pass++) for (let k = 0; k < 8; k++) {
        const lit = clamp(s.e[k] * s.bl), x = px(k), y = py(k), r = (13 + 9 * size) * (0.75 + 0.4 * lit);
        const hard = 0.75 - 0.55 * diff;               // where the edge starts to fade
        const gr = g.createRadialGradient(x, y, 0, x, y, r);
        if (!pass) {                                   // the body
          gr.addColorStop(0, rgba(lerp3(T.lo, T.hi, 0.35 + 0.5 * lit), (0.4 + 0.55 * lit) * amb));
          gr.addColorStop(hard, rgba(T.lo, (0.3 + 0.45 * lit) * amb));
          gr.addColorStop(1, rgba(T.lo, 0));
        } else {                                       // Tone: light on the tops
          const hy = y - r * 0.35, hg = g.createRadialGradient(x, hy, 0, x, hy, r * 0.7);
          hg.addColorStop(0, rgba(T.hi, (0.1 + 0.55 * tone) * (0.3 + 0.7 * lit) * amb));
          hg.addColorStop(1, rgba(T.hi, 0));
          g.fillStyle = hg; g.beginPath(); g.arc(x, hy, r * 0.7, 0, TAU); g.fill();
          continue;
        }
        g.fillStyle = gr; g.beginPath(); g.arc(x, y, r, 0, TAU); g.fill();
      }
      g.restore();
      // the drops: from the left into their puff, landing as glow
      s.drops = s.drops.filter(d => {
        const u = (t - d.t0) / d.life;
        if (u < 0) return true;
        if (u >= 1) { s.e[d.k] = Math.min(1.3, s.e[d.k] + 5.4 / (5 + 30 * diff)); return false; }
        const x1 = px(d.k), e = u * u * (3 - 2 * u);
        const x = d.x + (x1 - d.x) * e, y = d.y + (py(d.k) - d.y) * e;
        g.fillStyle = col((1 - u * 0.7) * amb, T.hi);
        g.beginPath(); g.arc(x, y, d.sz * (1 - 0.5 * u), 0, TAU); g.fill();
        return true;
      });
      g.fillStyle = col(0.85, T.hi); g.font = "bold 9px ui-monospace, monospace";
      g.fillText(hold ? "HOLD" : (rt < 10 ? rt.toFixed(1) : Math.round(rt)) + " s", 6, 12);
    },
    // twinkling, or soft and dim.
    galaxy(g, q, v, t, C, dt) {
      const s = v.s, cv = v.canvas;
      const RW = Math.max(200, Math.min(480, Math.round(cv.width))) & ~1, RH = RW >> 1, k = RW / LW;
      if (!s.off || s.off.width !== RW) {
        s.off = document.createElement("canvas"); s.off.width = RW; s.off.height = RH;
        s.ctx = s.off.getContext("2d"); s.img = s.ctx.createImageData(RW, RH);
        s.out = new Uint32Array(s.img.data.buffer); s.I = new Float32Array(RW * RH);
        if (!s.st) {
          s.r = rng(v.e.id || 13); s.hits = []; s.next = 0;
          s.st = Array.from({ length: 520 }, () => {
            const r = 0.04 + 0.96 * Math.pow(s.r(), 0.7), arm = s.r() < 0.5 ? 0 : Math.PI;
            return { r, a: arm + Math.log(r) * 2.2 + (s.r() - 0.5) * 0.9, z: s.r(), b: 0.35 + 0.65 * s.r(), p: s.r() * TAU };
          });
        }
      }
      const rep = q(/^replace$/i, 0.5), bright = q(/^bright$/i, 0.5), det = q(R.depth, 0.5);
      const big = q(R.size, 1), mix = q(R.mix, 0.5);
      if (v.live && t >= s.next) { s.hits.push(t); s.next = t + 0.6 + s.r() * 1.4; if (s.hits.length > 6) s.hits.shift(); }
      const I = s.I, decay = Math.exp(-dt / (0.04 + 1.6 * (1 - rep) ** 2));
      for (let i = 0; i < I.length; i++) I[i] *= decay;
      const cx = 128 * k, cy = 62 * k, Rr = (34 + 74 * big) * k, sq = 0.4, tilt = -0.22, ct = Math.cos(tilt), stl = Math.sin(tilt);
      const soft = bright < 0.5, gain = (0.5 + 0.9 * mix) * (0.55 + 0.6 * bright) * (1 + 2.2 * rep * rep);   // short trails: brighter stars
      const plot = (x, y, a) => {
        const xi = x | 0, yi = y | 0;
        if (xi < 1 || yi < 1 || xi >= RW - 1 || yi >= RH - 1) return;
        const i = yi * RW + xi; I[i] += a;
        if (soft) { const h = a * (0.5 - bright) * 0.9; I[i - 1] += h; I[i + 1] += h; I[i - RW] += h; I[i + RW] += h; }
      };
      for (const st of s.st) {
        if (v.live) st.a += dt * (0.18 + 0.5 * (1 - big * 0.4)) / (0.25 + st.r);
        const rr = st.r * (1 + det * 0.09 * Math.sin(t * (1.2 + 3 * st.z) + st.p));
        const ex = Math.cos(st.a) * rr * Rr, ey = Math.sin(st.a) * rr * Rr * sq;
        const x = cx + ex * ct - ey * stl, y = cy + ex * stl + ey * ct;
        let a = st.b * 0.16;
        for (const h of s.hits) { const d = (t - h) * 0.7 - st.r; a += 0.9 * Math.exp(-d * d / 0.006) * (1 - Math.min(1, (t - h) / 3)); }
        if (!soft && st.z > 0.92) a *= 0.6 + 0.8 * Math.abs(Math.sin(t * 7 + st.p * 9));   // twinkle
        plot(x, y, a * gain);
      }
      // the core: hatched, swelling with the hits
      let ce = 0; for (const h of s.hits) ce += Math.exp(-(t - h) * 3);
      const cr = (5 + 3 * big + 3 * Math.min(1, ce)) * k;
      for (let y = Math.floor(cy - cr * sq * 1.6); y < cy + cr * sq * 1.6; y++)
        for (let x = Math.floor(cx - cr); x < cx + cr; x++) {
          const dx = (x - cx) / cr, dy = (y - cy) / (cr * sq * 1.6);
          if (dx * dx + dy * dy < 1 && ((x * 0.77 + y * 0.64) / (2.6 * k)) % 1 < 0.55) plot(x, y, 0.12 * gain);
        }
      // intensity to colour
      const bg = rgbOf(C.bg), fg = rgbOf(C.fg);
      if (!s.lut || s.lutKey !== C.bg + C.fg) {
        s.lutKey = C.bg + C.fg; s.lut = new Uint32Array(64);
        const T = tones(C);                       // faint stars in the colour, bright ones pale
        for (let i = 0; i < 64; i++) { const u = i / 63; s.lut[i] = pack(u < 0.55 ? lerp3(bg, T.hi, u / 0.55) : lerp3(T.hi, T.lo, (u - 0.55) / 0.45)); }
      }
      const out = s.out, lut = s.lut;
      for (let i = 0; i < I.length; i++) { const u = I[i]; out[i] = lut[u >= 1 ? 63 : (u * 63) | 0]; }
      s.ctx.putImageData(s.img, 0, 0);
      g.imageSmoothingEnabled = false; g.drawImage(s.off, 0, 0, LW, LH); g.imageSmoothingEnabled = true;
    },

    // Spiral: the repeats climb (or sink) as they go round. Each note starts
    // the path (in the middle when they climb, at the rim when they sink); every repeat (Div at Tempo) steps one turn-slice
    // further round, Rise steps it outward (up) or inward (down), and Glide
    // unwinds the whole staircase over Span after each note. Feedbk sets how
    // many repeats there are, Tone how much each loses (and how thick the
    // rings), Mix how bright the lot. The path is a string of beads drifting
    // outward; the repeat sounding is solid, the ones before it rings.
    spiral(g, q, v, t, C, dt) {
      const P = pixels(v), s = v.s, { RW, RH, k, M } = P;
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 31); s.note = -9; s.next = 0.2; }
      const ps = v.e.params || [], di = ps.findIndex(p => /^div$/i.test(p.name));
      const ch = di >= 0 && ps[di].choices, raw = q.raw(/^div$/i, 30);
      const idx = ch ? Math.max(0, ch.findIndex(c => raw >= c.from && raw <= c.to)) : Math.min(7, (raw / 12.5) | 0);
      const beats = [0.125, 1 / 6, 0.25, 1 / 3, 0.5, 0.75, 1, 1.5][idx] ?? 0.25;
      const tempo = Math.max(40, Math.min(240, q.raw(/^tempo$/i, 120) || 120));
      const period = beats * 60 / tempo;
      const fb = q(R.fb, 0.6) * 0.965, rise = q(/^rise$/i, 0.62), glide = q(/^glide$/i, 0.45);
      const span = q(/^span$/i, 0.45), tone = q(R.tilt, 0.5), mix = q(R.mix, 0.5);
      const loss = fb * (0.7 + 0.3 * tone);                          // what a repeat keeps
      const nMax = Math.min(40, loss < 0.05 ? 1 : Math.ceil(Math.log(0.06) / Math.log(loss)));
      if (v.live && t >= s.next) { s.note = t; s.next = t + Math.max(1.6, Math.min(6, nMax * period + 0.6)) + s.r() * 0.8; }
      const since = t - s.note;
      const spanS = 0.19 + 7.8 * span ** 3, ramp = glide * (1 - Math.exp(-Math.max(0, since) / spanS));
      const turn = 0.45 + 0.8 * Math.min(1, period / 0.6);            // angle per repeat
      const uMax = Math.max(nMax, 2.4 * TAU / turn);                  // the path: at least 2.4 turns
      const rs = (rise - 0.5) * 2, step = (Math.sign(rs) * Math.sqrt(Math.abs(rs)) * 46 + ramp * 14) / uMax;   // outward per repeat
      const cx = 128, cy = 64, sx = 1.8, r0 = 30 - Math.sign(rs) * Math.sqrt(Math.abs(rs)) * 24, rot = t * 0.15;   // up: out from the middle; down: in from the rim
      const at = n => { const r = Math.max(3, Math.min(55, r0 + step * n)), a = rot + n * turn; return [cx + Math.cos(a) * r * sx, cy + Math.sin(a) * r * 1.05]; };
      // the path the repeats follow: a rough band, pale
      // the path, as beads drifting outward along it (the way the repeats go)
      M.fill(0);
      const gap = 6.5, ph = (t * 9) % gap;
      let [px, py] = at(0), acc = gap - ph;
      for (let u = 0.01; u <= uMax; u += 0.01) {
        const [x, y] = at(u); acc += Math.hypot(x - px, y - py); px = x; py = y;
        if (acc >= gap) { acc -= gap; const b = 1.3 + 0.9 * N(u * 50, 11); P.disc(x, y, b * 1.2, b, 1, 0.3); }
      }
      // the note (where the path starts) and its repeats: the one sounding is
      // solid with a halo, the ones that have sounded hollow rings
      const ring = (x, y, rx, ry, w) => { P.disc(x, y, rx, ry, 4, 0.4); P.disc(x, y, rx - w * 1.2, ry - w, 8, 0.4); };
      const rw = 1.2 + 1.8 * (1 - tone);
      const hit = Math.exp(-Math.max(0, since) * 5);
      const cr = 8 + 7 * hit;
      { const [x, y] = at(0); P.disc(x, y, cr * 1.25, cr, 2, 0.6); if (hit > 0.05) ring(x, y, (cr + 3 + 6 * (1 - hit)) * 1.25, cr + 3 + 6 * (1 - hit), rw); }
      for (let n = 1; n <= nMax; n++) {
        const age = since - n * period;
        if (age < 0) break;
        const a = Math.pow(loss, n - 1), sz = (4 + 10 * a) * (1 + 0.4 * Math.exp(-age * 8));
        const [x, y] = at(n);
        if (age < period * 1.5) { P.disc(x, y, sz * 1.25, sz, 2, 0.6); ring(x, y, (sz + 3.5) * 1.25, sz + 3.5, rw); }
        else ring(x, y, sz * 1.25, sz, rw);
      }
      P.flat(C, { wet: 0.55 + 0.45 * mix, pale: 0.7, dim: 0.75 });
      P.blit(g);
    },

    // Hydra: three heads on one tape, scrolling past like a drum machine's
    // lanes. The dry hits run in the middle; above, the 2x head plays two
    // windows of them in one (each hit twice, half as wide: double time,
    // octave up); below, the 0.5x head plays half a window stretched over a
    // whole one (twice as wide, some hits never reached: half time, octave
    // down). Ticks mark where the heads jump back (Div at Tempo). At Grain the
    // window is a few ms: the layers sit on the dry, fine dots over it
    // (up) and big blocks under it (down). Fast and Slow size their lanes, Tone softens them, Mix
    // sets how bright. Drawn as a dot matrix: small dots up, big blocks down.
    hydra(g, q, v, t, C, dt) {
      const P = pixels(v), s = v.s;
      if (!s.init) { s.init = 1; const r = rng(v.e.id || 41);
        s.pat = [1, 0, 0.45, 0, 0.8, 0, 0.45, 0.3, 1, 0, 0.45, 0, 0.8, 0.35, 0.5, 0].map(a => a && a * (0.8 + 0.2 * r())); }
      const ps = v.e.params || [], di = ps.findIndex(p => /^div$/i.test(p.name));
      const ch = di >= 0 && ps[di].choices, raw = q.raw(/^div$/i, 42);
      const idx = ch ? Math.max(0, ch.findIndex(c => raw >= c.from && raw <= c.to)) : Math.min(7, (raw / 12.5) | 0);
      const tempo = Math.max(40, Math.min(240, q.raw(/^tempo$/i, 120) || 120));
      const grain = idx === 0, W = grain ? 256 / 44100 : [0, 0.125, 1 / 6, 0.25, 1 / 3, 0.5, 0.75, 1][idx] * 60 / tempo;
      const fast = q(/^fast$/i, 0.7), slow = q(/^slow$/i, 0.55), tone = q(R.tilt, 0.45), mix = q(R.mix, 0.5);
      const step = 30 / tempo, V = Math.max(1.2, Math.min(3.2, grain ? 2 : W * 7)), X1 = 246, pps = (X1 - 6) / V;
      const lanes = { fast: 41, dry: 82, slow: 123 }, H = 40;
      P.M.fill(0);
      const hump = (tau, len, amp, base, bit) => {     // a hit: a hump on its lane
        const x = X1 - (t - tau) * pps; if (x < -20 || tau > t || amp < 0.02) return;
        const rx = Math.max(2.5, len * pps), ry = amp * H;
        P.disc(x + rx * 0.6, base, rx, ry, bit, 0.4 + 1.6 * (1 - tone), true);
      };
      const LEN = 0.1;                                  // a hit's length (s)
      for (let k = Math.floor((t - V - 2 * W) / step) - 1; k * step <= t; k++) {
        const s0 = k * step, a = s.pat[((k % 16) + 16) % 16]; if (!a) continue;
        hump(s0, LEN, a, lanes.dry, 1);
        if (grain) { hump(s0, LEN, a * fast, lanes.fast, 2); hump(s0, LEN, a * slow, lanes.slow, 4); continue; }
        const n0 = Math.floor(s0 / W);
        for (const n of [n0, n0 + 1]) hump(n * W + (s0 - (n - 1) * W) / 2, LEN / 2, a * fast, lanes.fast, 2);
        const n = n0 + 1, off = s0 - (n - 1) * W;
        if (off < W / 2) hump(n * W + 2 * off, LEN * 2, a * slow, lanes.slow, 4);
      }
      // where the heads jump back, and the three heads at the right
      if (!grain && W * pps > 5) for (let n = Math.ceil((t - V) / W); n * W <= t; n++) {
        const x = X1 - (t - n * W) * pps;
        P.rect(x - 0.7, lanes.fast, x + 0.7, lanes.fast + 5, 16); P.rect(x - 0.7, lanes.slow - 44, x + 0.7, lanes.slow - 40, 16);
      }
      for (const b of [lanes.fast, lanes.dry, lanes.slow]) { P.rect(4, b + 0.5, X1 + 2, b + 1.3, 16); P.rect(X1 + 3, b - H, X1 + 5, b + 1.3, 16); }
      P.cells(C, { wet: 0.5 + 0.5 * mix, cell: { 2: 3, 1: 4.5, 4: 7.5 }, pale: 0.85, dim: 0.75, fade: 0.55 });
      P.blit(g);
    },

    // Mangle: a note and its repeats left to right, each a slab of waveform,
    // each worse than the last. Crush (in the loop, so it compounds) cuts
    // every later repeat into bigger, coarser steps; Tremolo wobbles each one
    // harder; Pitch puts an octave-down ghost behind them all, twice as long
    // and combed. Time spaces them, Feedbk sets how many and how fast they
    // fade, Mix how bright. The teeth top and bottom are the pedal's own,
    // sharper as Crush goes up.
    mangle(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 53); s.note = -9; s.next = 0.3; s.wav = Array.from({ length: 64 }, () => 0.75 + 0.25 * s.r()); }
      const time = q(R.time, 0.4), fb = q(R.fb, 0.45) * 0.92, crush = q(R.crush, 0), trem = q(/^trem/i, 0), pitch = q(/^pitch$/i, 0), mix = q(R.mix, 0.5);
      const secs = 0.023 + 1.28 * time * time, gapX = 16 + 46 * time, len = Math.min(gapX * 0.85, 46);
      const nMax = Math.min(14, fb < 0.06 ? 0 : Math.floor(Math.log(0.07) / Math.log(fb)));
      if (v.live && t >= s.next) { s.note = t; s.next = t + Math.max(2.6, Math.min(6, (nMax + 1) * secs + 1.4)); }
      const since = t - s.note, cy = 64, H = 44, wet = 0.45 + 0.55 * mix;
      // the teeth
      const tooth = 10 - 4 * crush, th = 3 + 5 * crush;
      g.fillStyle = col(0.35 + 0.3 * crush, T.lo);
      for (const [y0, dir] of [[0, 1], [128, -1]]) {
        g.beginPath(); g.moveTo(0, y0);
        for (let x = 0; x <= 256; x += tooth) { g.lineTo(x, y0); g.lineTo(x + tooth / 2, y0 + dir * th); }
        g.lineTo(256, y0); g.closePath(); g.fill();
      }
      const slab = (x0, n, a, c, comb, stretch) => {  // one repeat, drawn in columns
        const cr = 1 - Math.pow(1 - crush * 0.55, n), blk = 1 + Math.round(cr * 7), lv = Math.max(2, Math.round(14 - 12 * cr));
        const td = 1 - Math.pow(1 - trem * 0.6, n + 1), L = len * stretch;
        g.fillStyle = c;
        for (let x = 0; x < L; x += blk) {
          if (comb && (x / blk | 0) % 2) continue;
          const u = x / L, env = Math.min(1, u * 10) * Math.exp(-u * 1.3) * s.wav[(x * 2 | 0) & 63];
          const tr = 1 - td * (0.5 + 0.5 * Math.sin(u * TAU * 4.5 * secs * 3 * stretch + n));
          const h = Math.round(env * tr * a * lv) / lv * H;
          if (h < 0.5) continue;
          g.fillRect(x0 + x, cy - h, Math.max(1, blk - (blk > 2 ? 1 : 0)), h * 2);
        }
      };
      const show = n => since >= n * secs;
      if (pitch > 0.02) for (let n = 0; n <= nMax; n++) if (show(n) && n) slab(8 + n * gapX, n, Math.pow(fb, n * 0.6) * pitch * 0.8, col(0.45 * wet, T.hi), true, 2);
      for (let n = nMax; n >= 0; n--) {
        if (!show(n)) continue;
        const a = Math.pow(fb, n * 0.6), fresh = Math.exp(-(since - n * secs) * 6);
        slab(8 + n * gapX, n, a, n ? col((0.4 + 0.6 * a + 0.3 * fresh) * wet, T.hi) : col(0.85, T.lo), false, 1);
      }
      g.fillStyle = col(0.3, T.lo); g.fillRect(4, cy - 0.4, 248, 0.8);
    },

    // Stasis: what you play runs along the bottom as a pale trace; a capture
    // lifts the last Length of it off the trace and wraps it into a ring of
    // spikes that turns once per loop, the read point lit. Blur is the wedge
    // where the loop's end fades into its start; Decay fades the ring (fully
    // up, never); Tone smooths its spikes; Mix weighs the ring against the
    // trace. Capture: Stomp takes a new one now and then, Release lets it go,
    // Hold keeps it.
    stasis(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const W = 248, SPD = 120;                              // trace: logical px, px per second
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 61); s.tr = new Float32Array(W); s.env = 0; s.next = 0; s.acc = 0;
        s.cap = null; s.capT = -9; s.nextCap = 1.2; s.held = 0; s.lastNote = -9;
        for (let x = 0; x < W; x++) { if (x % 70 === 20) s.env = 0.8; s.env *= 0.965; s.tr[x] = s.env * (0.35 + 0.65 * s.r()) + 0.03 * s.r(); } }
      const len = q(R.time, 0.35), blur = q(/^blur$/i, 0.5), decay = q(/^decay$/i, 1);
      const tone = q(R.tilt, 0.55), mix = q(R.mix, 0.5), mode = q.raw(/^capture$/i, 0);
      const lenS = 0.06 + 1.34 * len, L = Math.max(8, Math.min(W - 8, lenS * SPD));
      // the playing: notes into an envelope, the trace scrolls left
      if (v.live) {
        if (t >= s.next) { s.env = 0.6 + 0.4 * s.r(); s.next = t + 0.35 + s.r() * 0.9; s.lastNote = t; }
        s.acc += dt * SPD;
        while (s.acc >= 1) { s.acc -= 1; s.tr.copyWithin(0, 1); s.env *= 0.965;
          s.tr[W - 1] = s.env * (0.35 + 0.65 * s.r()) + 0.03 * s.r(); }
      }
      const take = () => {                                   // the last L px, smoothed by Tone, 96 spikes
        const raw = Array.from({ length: 96 }, (_, k) => s.tr[Math.max(0, Math.min(W - 1, Math.round(W - L + k * L / 96)))]);
        const w = Math.round((1 - tone) * 4);
        const sm = raw.map((_, k) => { let a = 0, n = 0; for (let j = -w; j <= w; j++) { const i = k + j; if (i >= 0 && i < 96) { a += raw[i]; n++; } } return a / n; });
        const pk = Math.max(0.15, ...sm); s.cap = sm.map(a => a / pk);
        s.capT = t; s.capL = L;
      };
      if (v.live) {
        if (mode === 1) s.held = Math.max(0, s.held - dt * 2.5);
        else if (!s.cap || (mode === 0 && t >= s.nextCap && t - s.lastNote > L / SPD * 0.5)) { take(); s.held = 1; s.nextCap = t + 5 + s.r() * 3; }
        else if (mode === 2 && s.held < 1) { take(); s.held = 1; }
      }
      const life = decay >= 0.99 ? 1 : Math.exp(-(t - s.capT) / (0.5 + 30 * decay ** 3));
      const fresh = Math.exp(-(t - s.capT) * 4), wet = (0.4 + 0.6 * mix) * s.held * life;
      // the trace (and, just after a capture, the stretch it took)
      const ty = 114, tw = 0.75 - 0.35 * mix * s.held;
      g.strokeStyle = col(tw, T.lo); g.lineWidth = 1; g.beginPath();
      for (let x = 0; x < W; x++) { const y = ty - s.tr[x] * 12 * (x % 2 ? 1 : -1); x ? g.lineTo(4 + x, y) : g.moveTo(4, y); }
      g.stroke();
      if (fresh > 0.05 && s.cap) {                         // brackets round the stretch taken
        const x0 = 4 + W - s.capL - (t - s.capT) * SPD, x1 = x0 + s.capL;
        g.strokeStyle = col(fresh, T.hi); g.lineWidth = 1.4; g.beginPath();
        g.moveTo(x0 + 4, ty - 16); g.lineTo(x0, ty - 16); g.lineTo(x0, ty + 16); g.lineTo(x0 + 4, ty + 16);
        g.moveTo(x1 - 4, ty - 16); g.lineTo(x1, ty - 16); g.lineTo(x1, ty + 16); g.lineTo(x1 - 4, ty + 16); g.stroke();
      }
      if (!s.cap || wet < 0.01) return;
      // the ring
      const cx = 128, cy = 54, r0 = 8 + 20 * len, sx = 1.5, grow = 1 - fresh * fresh;
      const spin = Math.min(TAU / lenS, 3.2) * (t - s.capT), seam = 0.02 + 0.38 * blur;
      g.lineWidth = 2;
      for (let k = 0; k < 96; k++) {
        const u = k / 96, a = u * TAU + spin;
        let amp = s.cap[k];
        const inSeam = u > 1 - seam;
        if (inSeam) { const f = (u - (1 - seam)) / seam; amp = amp * (1 - f) + s.cap[Math.round(f * seam * 96) % 96] * f; }
        const read = Math.abs(((u - 0.0) % 1 + 1) % 1);   // the read point sits at angle `spin` (u = 0)
        const lit = Math.max(Math.exp(-(read) * 30), Math.exp(-(1 - read) * 30));
        const r1 = r0 + 2, r2 = r0 + 2 + (2 + 30 * amp) * grow;
        const c = Math.cos(a), sn = Math.sin(a);
        g.strokeStyle = col(wet * (inSeam ? 0.45 : 0.75) + 0.25 * lit * wet, lit > 0.3 ? T.lo : T.hi);
        g.beginPath(); g.moveTo(cx + c * r1 * sx, cy + sn * r1); g.lineTo(cx + c * r2 * sx, cy + sn * r2 * 0.8); g.stroke();
      }
      g.strokeStyle = col(wet * 0.5, T.lo); g.lineWidth = 1;
      g.beginPath(); g.ellipse(cx, cy, r0 * sx, r0, 0, 0, TAU); g.stroke();
      if (decay >= 0.99) { g.fillStyle = col(wet * 0.8, T.lo); g.font = "bold 9px ui-monospace, monospace"; g.textAlign = "center"; g.fillText("∞", cx, cy + 3); g.textAlign = "start"; }
    },

    // EuGate: the sound runs left through a row of Steps gates; the Notes
    // hits, spread as evenly as they go (and turned by Shift), open and let
    // it through, each window as long as Gap leaves it and as rounded as
    // Soft; the rest is shut, the sound dim behind. Swing pushes every other
    // step late; the playhead walks at Tempo. Above: the count and the
    // grouping, as the pedal writes it (5/16 3.3.3.3.4). Mix: how bright.
    eugate(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; const r = rng(v.e.id || 71); s.w = Float32Array.from({ length: 512 }, () => r()); }
      const N = Math.max(1, Math.min(64, q.raw(/^steps$/i, 15) + 1)), K = Math.max(0, Math.min(N, q.raw(/^notes$/i, 4) + 1));
      const shift = q.raw(/^shift$/i, 0) % N, swing = q(/^swing$/i, 0), gap = q(/^gap$/i, 0.3), soft = q(/^soft$/i, 0.35), mix = q(R.mix, 1);
      const bpm = Math.max(30, Math.min(441, q.raw(/^tempo$/i, 120) || 120)), stepS = 15 / bpm;
      const base = new Set(Array.from({ length: K }, (_, m) => Math.floor(m * N / K)));   // 5/16: 0 3 6 9 12, as the pedal groups it
      const hits = Array.from({ length: N }, (_, i) => base.has(((i - shift) % N + N) % N));
      const X0 = 6, X1 = 250, cw = (X1 - X0) / N, cy = 74, H = 32;
      const swingOf = i => (i % 2 ? swing * cw * 0.33 : 0);
      const pos = ((v.live || t ? t : 0) / stepS) % N, ph = Math.floor(pos);
      // the sound: a waveform drifting left, dim where it's shut
      const scroll = Math.floor(t * 40), wave = x => { const i = (x * 1.6 + scroll) | 0; return 0.35 + 0.65 * s.w[i & 511] * (0.6 + 0.4 * Math.sin(x * 0.11 + t)); };
      const winAt = x => {                                  // the gate's opening at x, 0..1
        const i = Math.min(N - 1, Math.floor((x - X0) / cw)); if (!hits[i]) return 0;
        const a = X0 + i * cw + swingOf(i), open = cw * (1 - gap * 0.9), u = (x - a) / open;
        if (u < 0 || u > 1) return 0;
        const e = Math.max(0.02, soft * 0.5);
        return Math.min(1, u / e, (1 - u) / e);
      };
      for (let x = X0; x < X1; x++) {
        const h = wave(x) * H, w = winAt(x), i = Math.min(N - 1, Math.floor((x - X0) / cw));
        g.fillStyle = col(0.1 + 0.25 * (1 - mix), T.lo); g.fillRect(x, cy - h, 1, h * 2);
        if (w > 0) {
          const near = i === ph ? 1 : Math.max(0, 1 - ((ph - i + N) % N) / N);
          g.fillStyle = col((0.35 + 0.65 * near) * (0.45 + 0.55 * mix), i === ph ? T.lo : T.hi);
          g.fillRect(x, cy - h * w, 1, h * w * 2);
        }
      }
      // the gates: a frame per step, the hits' tops marked, the playhead
      g.fillStyle = col(0.4, T.lo);
      for (let i = 0; i <= N; i++) if (cw > 3 || i % 4 === 0) g.fillRect(X0 + i * cw - 0.4, cy + H + 4, 0.8, 4);
      for (let i = 0; i < N; i++) if (hits[i]) { g.fillStyle = col(0.9, T.hi); g.fillRect(X0 + i * cw + swingOf(i), cy - H - 7, Math.max(1.5, cw * (1 - gap * 0.9)), 2.5); }
      g.fillStyle = col(0.9, T.lo); g.fillRect(X0 + (pos - ph + ph) * cw, cy + H + 2, 1.6, 9);
      // the count and grouping
      const on = [...base].sort((x, y) => x - y), grp = on.map((a, k) => ((on[(k + 1) % on.length] - a + N - 1) % N) + 1);
      g.fillStyle = col(0.85, T.lo); g.font = "bold 9px ui-monospace, monospace";
      g.fillText(K + "/" + N + (K > 1 && K < N ? "  " + grp.join(".") : ""), 6, 14);
    },

    // Flower: a resonant filter whose peak jumps to a new random place every
    // step (Rate), left in the colour and right pale, each on its own
    // sequence. Every jump blooms a six-petal flower where the peak lands;
    // along the bottom the two sample-and-hold staircases. Range spreads the
    // jumps, Manual centres them, Reso sharpens the peaks and widens the
    // petals, Smooth glides between steps, Mix sets how bright.
    flower(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const rate = q(R.rate, 0.45), range = q(/^range$/i, 0.7), man = q(/^manual$/i, 0.45), reso = q(R.reso, 0.75), smooth = q(/^smooth$/i, 0.15), mix = q(R.mix, 0.5);
      const hz = 0.5 + 13.5 * rate * rate, X0 = 8, X1 = 248;
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 83); s.next = 0; s.ch = [0, 1].map(() => ({ tgt: 0.5, cur: 0.5, hist: [], blooms: [] })); }
      if (v.live && t >= s.next) {
        s.next = t + 1 / hz;
        for (const c of s.ch) { c.tgt = s.r(); c.hist.push({ t, u: c.tgt }); if (c.hist.length > 40) c.hist.shift(); c.blooms.push({ t, u: c.tgt }); if (c.blooms.length > 8) c.blooms.shift(); }
      }
      const glide = smooth < 0.02 ? 1 : 1 - Math.exp(-dt / (0.01 + smooth * 0.6 / hz * 3));
      const xOf = u => { const c = clamp(man + (u - 0.5) * range * 0.75); return X0 + c * (X1 - X0); };
      for (const c of s.ch) c.cur += (c.tgt - c.cur) * glide;
      // the two filter curves
      const base = 86, peakH = 18 + 46 * reso, bw = 0.03 + 0.12 * (1 - reso);
      s.ch.forEach((c, k) => {
        const px = xOf(c.cur), cc = k ? T.lo : T.hi;
        g.strokeStyle = col((k ? 0.55 : 0.85) * (0.5 + 0.5 * mix), cc); g.lineWidth = k ? 1 : 1.6; g.beginPath();
        for (let x = X0; x <= X1; x += 2) {
          const d = (x - px) / (X1 - X0), pk = Math.exp(-(d * d) / (2 * bw * bw)), lp = d > 0 ? Math.exp(-d * 9) : 1;
          const y = base - (lp * 14 + pk * peakH) + 14;
          x === X0 ? g.moveTo(x, y) : g.lineTo(x, y);
        }
        g.stroke();
        // blooms: six petals where the peak landed, opening then fading
        for (const b of c.blooms) {
          const age = t - b.t, life = 0.25 + 1.6 / hz; if (age > life) continue;
          const o = clamp(age / 0.12), f = 1 - age / life, bx = xOf(b.u), by = base - peakH - 1, pr = (2.5 + 5 * reso) * (0.5 + 0.5 * o);
          g.strokeStyle = col(f * (k ? 0.6 : 0.95), cc); g.lineWidth = 1;
          for (let p = 0; p < 6; p++) {
            const a = p / 6 * TAU + (k ? 0.5 : 0) + age * 0.8;
            g.beginPath(); g.ellipse(bx + Math.cos(a) * pr * 0.9, by + Math.sin(a) * pr * 0.9 * 0.8, pr * 0.65, pr * 0.38, a, 0, TAU); g.stroke();
          }
          g.fillStyle = col(f, cc); g.fillRect(bx - 1, by - 1, 2, 2);
        }
      });
      // the staircases: recent steps, newest at the right
      const span = Math.min(40, Math.max(6, hz * 3)), sy = 118, sh = 14;
      s.ch.forEach((c, k) => {
        g.strokeStyle = col(k ? 0.45 : 0.75, k ? T.lo : T.hi); g.lineWidth = 1; g.beginPath(); let first = true;
        for (const h of c.hist) {
          const x = X1 - (t - h.t) * hz / span * (X1 - X0); if (x < X0 - 10) continue;
          const y = sy - h.u * sh - (k ? 0 : 1);
          if (first) { g.moveTo(Math.max(X0, x), y); first = false; } else { g.lineTo(x, g._ly ?? y); g.lineTo(x, y); }
          g._ly = y;
        }
        if (!first) g.lineTo(X1, g._ly); g.stroke(); g._ly = undefined;
      });
    },

    // Scope: a phosphor screen. The signal (a played note, its harmonics
    // drifting) drawn as a glowing trace that leaves an afterglow on a dotted
    // graticule; Time is how much of it fits across, Gain how tall (flat-
    // topped where it runs off the screen), Mode 1 plots left against right.
    scope(g, q, v, t, C, dt) {
      const s = v.s, cv = v.canvas;
      const RW = Math.max(200, Math.min(480, Math.round(cv.width))) & ~1, RH = RW >> 1, k = RW / LW;
      if (!s.off || s.off.width !== RW) {
        s.off = document.createElement("canvas"); s.off.width = RW; s.off.height = RH;
        s.ctx = s.off.getContext("2d"); s.img = s.ctx.createImageData(RW, RH);
        s.out = new Uint32Array(s.img.data.buffer); s.I = new Float32Array(RW * RH);
        if (!s.r) { s.r = rng(v.e.id || 89); s.env = 0; s.next = 0; s.f = 1; }
      }
      const time = q(R.time, 0.3), gain = q(/^gain$/i, 0.25), xy = q.raw(/^mode$/i, 0) === 1;
      if (v.live && t >= s.next) { s.env = 0.7 + 0.3 * s.r(); s.f = [1, 1.25, 1.5, 0.75, 2][(s.r() * 5) | 0]; s.next = t + 0.8 + s.r() * 1.6; }
      s.env *= Math.exp(-dt / 2.2);
      const I = s.I, fade = Math.exp(-dt / 0.09);
      for (let i = 0; i < I.length; i++) I[i] *= fade;
      const win = 0.0029 * Math.pow(2, time * 6), f0 = 196 * s.f, gn = 0.5 * Math.pow(2, gain * 6) * 0.45;
      const h2 = 0.45 + 0.3 * Math.sin(t * 0.7), h3 = 0.25 + 0.2 * Math.sin(t * 1.1 + 1);
      const sig = (u, ph) => { const a = TAU * f0 * u + ph; return Math.sin(a) + h2 * Math.sin(2 * a + 0.4) + h3 * Math.sin(3 * a + 1.3); };
      const lvl = (s.env + 0.04) * gn, clip = y => Math.max(-1, Math.min(1, y));
      const plot = (x, y, a) => { const xi = x | 0, yi = y | 0; if (xi < 0 || yi < 0 || xi >= RW || yi >= RH) return; I[yi * RW + xi] += a; };
      const M = 900, cx = RW / 2, cy = RH / 2, A = RH * 0.42;
      let lx = 0, ly = 0;
      for (let j = 0; j <= M; j++) {
        const u = j / M;
        let x, y;
        if (xy) { const tt = u * 2 / f0; const lx2 = Math.min(lvl, 0.8); x = cx + clip(sig(tt, 0) * lx2 / 1.7) * A * 1.4; y = cy - clip(sig(tt, 1.1 + 0.3 * Math.sin(t * 0.5)) * lx2 / 1.7) * A; }
        else { x = u * RW; y = cy - clip(sig(u * win, 0) * lvl / 1.7) * A; }
        if (j) { const n = Math.max(1, Math.ceil(Math.hypot(x - lx, y - ly))); for (let m = 0; m < n; m++) plot(lx + (x - lx) * m / n, ly + (y - ly) * m / n, 0.5 / Math.sqrt(n)); }
        lx = x; ly = y;
      }
      // graticule
      const gr = 0.09;
      for (let gx = 0; gx <= 10; gx++) for (let y = 0; y < RH; y += Math.round(3 * k)) plot(gx * (RW - 1) / 10, y, gr);
      for (let gy = 0; gy <= 8; gy++) for (let x = 0; x < RW; x += Math.round(3 * k)) plot(x, gy * (RH - 1) / 8, gr);
      if (!s.lut || s.lutKey !== C.bg + C.fg) {
        s.lutKey = C.bg + C.fg; s.lut = new Uint32Array(64); const T = tones(C), bg = rgbOf(C.bg);
        for (let i = 0; i < 64; i++) { const u = i / 63; s.lut[i] = pack(u < 0.5 ? lerp3(bg, T.hi, u / 0.5) : lerp3(T.hi, T.lo, (u - 0.5) / 0.5)); }
      }
      const out = s.out, lut = s.lut;
      for (let i = 0; i < I.length; i++) { const u = I[i]; out[i] = lut[u >= 1 ? 63 : (u * 63) | 0]; }
      s.ctx.putImageData(s.img, 0, 0);
      g.imageSmoothingEnabled = true; g.drawImage(s.off, 0, 0, LW, LH);
    },

    // Skull: the pedal's own skull (SKULL_JAW below, from skull3d.py), drawn
    // as the pedal draws it. With each note the jaw opens with the level
    // (Sense), the eye lights grow and flicker, a sharp attack jolts it
    // sideways (Shake), and it relaxes over Decay; the hardest notes flash
    // it, blow it apart and it pulls itself back together (Boom).
    skull(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), GW = 128, GH = 64;
      if (!SKULL_PX) SKULL_PX = SKULL_JAW.map(b64 => { const b = atob(b64), f = new Uint8Array(GW * GH);
        for (let i = 0; i < f.length; i++) f[i] = (b.charCodeAt(i >> 3) >> (7 - (i & 7))) & 1; return f; });
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 97); s.env = 0; s.next = 0.4; s.boomT = -9; s.parts = null; s.shk = 0; s.tick = 0; }
      const sense = q(/^sense$/i, 0.5), decay = q(R.decay, 0.4), shake = q(/^shake$/i, 0.5), boom = q(/^boom$/i, 0.5);
      if (v.live && t >= s.next) {
        const hit = 0.35 + 0.65 * s.r(); s.env = Math.max(s.env, hit); s.next = t + 0.5 + s.r() * 1.3;
        s.shk = Math.round(shake * 8);
        if (boom > 0.02 && t - s.boomT > 2.4 && hit * (0.6 + 0.8 * sense) > 1.32 - boom * 0.7) s.boomT = t;
      }
      s.env *= Math.exp(-dt / (0.08 + 1.2 * decay));
      const lvl = Math.min(1.5, s.env * (0.4 + 1.6 * sense));
      const f = SKULL_PX[Math.max(0, Math.min(5, Math.floor(lvl * 6)))];
      let dx = 0;
      if (v.live && s.shk > 0) { s.tick ^= 1; dx = s.tick ? -2 : 2; s.shk--; }
      const bone = css(T.lo), eye = css(T.hi);
      const age = t - s.boomT, OUT = 0.5, BACK = 0.9;
      if (age >= 0 && age < OUT + BACK) {
        if (age < 0.06) {                                   // the flash, then the negative
          g.fillStyle = bone; g.fillRect(0, 0, LW, LH);
          if (age > 0.03) { g.fillStyle = C.bg; for (let i = 0; i < f.length; i++) if (f[i]) g.fillRect((i % GW) * 2, ((i / GW) | 0) * 2, 2, 2); }
          return;
        }
        if (!s.parts || s.parts.t !== s.boomT) {
          const P = []; for (let i = 0; i < f.length; i++) if (f[i]) { const x = i % GW, y = (i / GW) | 0, a = Math.atan2(y - 30, x - 62) + (s.r() - 0.5) * 0.7, sp = 40 + s.r() * 110;
            P.push({ x, y, vx: Math.cos(a) * sp, vy: Math.sin(a) * sp - 25 }); }
          s.parts = P; s.parts.t = s.boomT;
        }
        const a2 = age - 0.06, sx = a2 < 0.25 ? (Math.floor(a2 * 40) % 2 ? 1 : -1) * (1 - a2 / 0.25) * 3 : 0;
        g.fillStyle = bone;
        for (const p of s.parts) {
          let x, y;
          if (a2 < OUT) { x = p.x + p.vx * a2; y = p.y + p.vy * a2 + 70 * a2 * a2; }
          else { const e = 1 - Math.pow(1 - Math.min(1, (a2 - OUT) / BACK), 3), fx = p.x + p.vx * OUT, fy = p.y + p.vy * OUT + 70 * OUT * OUT; x = fx + (p.x - fx) * e; y = fy + (p.y - fy) * e; }
          g.fillRect(Math.round(x + sx) * 2, Math.round(y) * 2, 2, 2);
        }
        return;
      }
      g.fillStyle = bone;
      for (let y = 0; y < GH; y++) { let run = -1;                 // runs of bone, row by row
        for (let x = 0; x <= GW; x++) { const on = x < GW && f[y * GW + x];
          if (on && run < 0) run = x; else if (!on && run >= 0) { g.fillRect((run + dx) * 2, y * 2, (x - run) * 2, 2); run = -1; } } }
      // eye lights: diamonds growing with the level, flickering
      let r = lvl > 1 ? 3 : lvl > 0.6 ? 2 : lvl > 0.3 ? 1 : lvl > 0.12 ? 0 : -1;
      if (r >= 1 && v.live && Math.floor(t * 30) % 4 === 0) r--;
      if (r >= 0) { g.fillStyle = eye;
        for (const [ex, ey] of SKULL_EYES) for (let yy = -r; yy <= r; yy++) { const w = r - Math.abs(yy); g.fillRect((ex + dx - w) * 2, (ey + yy) * 2, (2 * w + 1) * 2, 2); } }
    },

    // Chopdly: the last cycle on top, cut into chops (equal, or at the note
    // attacks at Trans); below, the repeat as it plays them: in Order's
    // order, reversed ones flipped, pitched ones raised or lowered and
    // squeezed, run faster or slower by Stretch, overlapping by Glide. Wires
    // join each chop to where it went; the one playing now is lit. Feedbk
    // keeps fainter older repeats underneath.
    chopdly(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; const r = rng(v.e.id || 101); s.w = Float32Array.from({ length: 240 }, (_, i) => { const ph = i % 60; return (ph < 3 ? 1 : Math.exp(-ph / 18)) * (0.5 + 0.5 * r()); });
        s.cyc = 0; s.rs = 1; }
      const time = q(R.time, 0.45), nRaw = q.raw(/^chops$/i, 4), order = q.raw(/^order$/i, 6), pitch = q.raw(/^pitch$/i, 3), rev = q.raw(/^reverse$/i, 0);
      const str = q.raw(/^stretch$/i, 4), glide = q(/^glide$/i, 0.2), fb = q(R.fb, 0.35), mix = q(R.mix, 0.5);
      const cycS = 0.03 + 2.47 * time * time, vis = Math.max(1.2, cycS * 2), X0 = 8, X1 = 248, W = X1 - X0;
      const cyc = Math.floor(t / vis), u = (t % vis) / vis;
      if (cyc !== s.cyc) { s.cyc = cyc; s.rs = (s.rs * 16807) % 2147483647; }
      const rr = (k, salt) => { let a = (k + 1) * 9301 + salt * 49297 + (order >= 20 ? s.rs : 7); a = (a * 233280 + 12345) % 2147483647; return (a % 1000) / 1000; };
      // the cuts: equal, or at the attacks (every 60 samples of the table)
      let cuts;
      if (nRaw === 0) cuts = [0, 0.25, 0.5, 0.75, 1];
      else cuts = Array.from({ length: nRaw + 1 }, (_, i) => i / nRaw);
      const N = cuts.length - 1;
      // the order
      let seq = [...Array(N).keys()];
      const name = ["Fwd", "Back", "Swap", "Skip", "Stutr", "Build"][order];
      if (name === "Back") seq.reverse();
      else if (name === "Swap") seq = seq.map(i => (i % 2 ? i - 1 : Math.min(N - 1, i + 1)));
      else if (name === "Skip") seq = [...seq.filter(i => i % 2 === 0), ...seq.filter(i => i % 2)];
      else if (name === "Stutr") { const o = []; for (let i = 0; o.length < N; i++) { o.push(i); if (i % 2 === 0 && o.length < N) o.push(i); } seq = o; }
      else if (name === "Build") { const o = []; for (let k = 1; o.length < N; k++) for (let i = 0; i < k && o.length < N; i++) o.push(i); seq = o; }
      else if (order >= 6) seq = seq.map((_, k) => Math.floor(rr(k, order) * N));
      // per chop: reversed? pitch (semitones)? 
      const revOf = k => [0, rr(k, 31) < 0.25, k % 2 === 1, rr(k, 32) < 0.5, rr(k, 33) < 0.75, 1, 0, rr(k, 34) < 0.5][rev] ? 1 : 0;
      const pOf = k => { const fixed = [-12, -7, -5, 0, 5, 7, 12][pitch]; if (fixed !== undefined) return fixed;
        const r = rr(k, 41); return pitch === 7 ? (k % 2 ? 12 : 0) : pitch === 8 ? (k % 2 ? -12 : 0) : pitch === 9 ? (r < 0.33 ? 12 : r < 0.66 ? -12 : 0)
          : pitch === 10 ? (r < 0.33 ? 7 : 0) : pitch === 11 ? (r < 0.4 ? [5, 7, 12, -5][(r * 10 | 0) % 4] : 0) : Math.round((r - 0.5) * 24); };
      const speed = [0.25, 1 / 3, 0.5, 0.75, 1, 1.5, 2][str] ?? 1;
      const topY = 30, botY = 92, H = 16;
      const wave = (x0, x1, y, h, a0, a1, flip, sq, c) => {     // table a0..a1 drawn over x0..x1
        g.fillStyle = c;
        for (let x = x0; x < x1; x += 1.5) {
          let f = (x - x0) / Math.max(1, x1 - x0); if (flip) f = 1 - f;
          const a = s.w[Math.min(239, Math.floor((a0 + (a1 - a0) * ((f * sq) % 1)) * 240))];
          g.fillRect(x, y - a * h, 1.2, a * h * 2);
        }
      };
      // the recording, with its cuts
      wave(X0, X1, topY, H, 0, 1, 0, 1, col(0.55, T.lo));
      g.fillStyle = col(0.8, T.lo);
      for (const c of cuts) g.fillRect(X0 + c * W - 0.5, topY - H - 3, 1, H * 2 + 6);
      // older repeats (Feedbk), faint
      for (let k = 1; k <= 2; k++) if (fb > 0.05) { g.globalAlpha = Math.pow(fb, k) * 0.5; g.fillStyle = col(0.5, T.hi); g.fillRect(X0, botY + H + 4 + k * 5, W * Math.pow(fb, k * 0.3), 2); }
      g.globalAlpha = 1;
      // the repeat, chop by chop; wires from where each came from
      const slot = W / N, ov = glide * slot * 0.4, play = Math.floor(u * N);
      for (let k = 0; k < N; k++) {
        const src = seq[k], a0 = cuts[src], a1 = cuts[src + 1], rv = revOf(k), p = pOf(k);
        const x0 = X0 + k * slot - (k ? ov / 2 : 0), x1 = X0 + (k + 1) * slot + (k < N - 1 ? ov / 2 : 0);
        const y = botY - p * 0.9, h = H * (1 - Math.abs(p) / 40), lit = k === play;
        const cc = lit ? col(1, T.lo) : col((0.55 + 0.4 * mix) * (rv ? 0.75 : 1), T.hi);
        wave(x0 + 1, x1 - 1, y, h, a0, a1, rv, speed, cc);
        if (rv) { g.fillStyle = cc; g.beginPath(); g.moveTo(x0 + 3, y - h - 5); g.lineTo(x0 + 7, y - h - 7.5); g.lineTo(x0 + 7, y - h - 2.5); g.fill(); }
        const sx = X0 + (a0 + a1) / 2 * W, dx = (x0 + x1) / 2;
        g.strokeStyle = col(lit ? 0.9 : 0.28, lit ? T.lo : T.hi); g.lineWidth = lit ? 1.4 : 1;
        g.beginPath(); g.moveTo(sx, topY + H + 4); g.bezierCurveTo(sx, topY + H + 22, dx, y - h - 22, dx, y - h - 3); g.stroke();
      }
      g.fillStyle = col(0.75, T.lo); g.font = "bold 8px ui-monospace, monospace";
      const lab = ["Fwd", "Back", "Swap", "Skip", "Stutr", "Build", "Mix1", "Mix2", "Mix3", "Mix4", "Mix5", "Mix6", "Mix7", "Mix8", "Rpt1", "Rpt2", "Rpt3", "Rpt4", "Rpt5", "Rpt6", "Rand", "Chaos"][order] || "";
      g.fillText((nRaw ? N : "Trans") + "  " + lab, X0, 9);
    },

    // Loop: the loop as layers of sound stacked like strata, the newest on
    // top, each overdub fading the ones beneath (Fade). The looper runs its
    // cycle: record (the first layer drawn in as you play), play, overdub,
    // play, stop. Bar and beat lines when Beats is set, a flash on each
    // click; the live sound runs underneath at Dry; Loop sets how bright
    // the loop plays.
    loop(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; const r = rng(v.e.id || 103); s.L = Array.from({ length: 5 }, () => Float32Array.from({ length: 128 }, (_, i) => { const ph = (i + (r() * 128 | 0)) % 32; return (ph < 2 ? 1 : Math.exp(-ph / 10)) * (0.4 + 0.6 * r()); })); }
      const loopL = q(/^loop$/i, 1), dry = q(/^dry$/i, 1), fade = q(/^fade$/i, 1), beats = q.raw(/^beats$/i, 0), click = q(/^click$/i, 0.5);
      const bpm = Math.max(40, q.raw(/^tempo$/i, 120)), long = q.raw(/^long$/i, 0);
      const lenS = beats > 0 ? Math.max(1, beats) * 60 / bpm * (beats === 1 ? 4 : 1) * 2 : 3.0;
      const plan = [["REC", 1], ["PLAY", 2], ["DUB", 1], ["PLAY", 1], ["DUB", 1], ["PLAY", 2], ["STOP", 1]];
      const total = plan.reduce((a, p) => a + p[1], 0), lap = Math.floor(t / lenS) % total;
      let acc = 0, st = "PLAY", layers = 0;
      for (const [nm, n] of plan) { if (lap < acc + n) { st = nm; break; } acc += n; if (nm === "REC" || nm === "DUB") layers++; }
      const ph = (t % lenS) / lenS, X0 = 8, X1 = 248, W = X1 - X0, base = 98;
      // the strata
      const shown = st === "STOP" ? 0 : layers + (st === "REC" || st === "DUB" ? 1 : 0);
      const top = new Float32Array(128);
      for (let k = 0; k < shown; k++) {
        const fresh = k === shown - 1 && (st === "REC" || st === "DUB");
        const age = shown - 1 - k, keep = Math.pow(0.35 + 0.65 * fade, age);
        g.fillStyle = col((0.35 + 0.55 * (1 - age / 5)) * (0.4 + 0.6 * loopL), k === shown - 1 ? T.hi : T.lo);
        for (let i = 0; i < 128; i++) {
          const x = X0 + i / 128 * W; if (fresh && i / 128 > ph) break;
          const h = s.L[k][i] * 22 * keep;
          g.fillRect(x, base - top[i] - h, W / 128 + 0.3, h); top[i] += h;
        }
      }
      // beats and bars
      if (beats > 0) {
        const nb = beats === 1 ? 8 : beats * 2;
        for (let b = 0; b <= nb; b++) { const x = X0 + b / nb * W, bar = beats === 1 || b % beats === 0;
          g.fillStyle = col(bar ? 0.55 : 0.22, T.lo); g.fillRect(x - 0.5, 22, 1, base - 22 + (bar ? 4 : 0)); }
        const bi = Math.floor(ph * nb), bf = ph * nb - bi;
        if (click > 0.02 && st !== "STOP" && bf < 0.18) { g.fillStyle = col(click * (1 - bf / 0.18), T.lo); g.fillRect(X0 + bi / nb * W - 2, 16, 4, 4); }
      }
      // the playhead, the live sound, the state
      if (st !== "STOP") { g.fillStyle = col(0.95, T.lo); g.fillRect(X0 + ph * W - 0.8, 20, 1.6, base - 18); }
      g.fillStyle = col(0.15 + 0.45 * dry, T.lo);
      for (let i = 0; i < 128; i += 1) { const h = s.L[0][(i + Math.floor(t * 40)) & 127] * 7 * dry; g.fillRect(X0 + i / 128 * W, 116 - h, W / 128, h * 2); }
      g.fillStyle = col(0.9, st === "REC" || st === "DUB" ? T.hi : T.lo); g.font = "bold 9px ui-monospace, monospace";
      g.fillText((st === "REC" ? "● " : "") + st, X0, 12);
      g.textAlign = "right"; g.fillText(beats > 0 ? (beats === 1 ? "BEAT" : beats + "/4") + " " + bpm : (lenS.toFixed(1) + "S" + (long ? " LONG" : "")), X1, 12); g.textAlign = "start";
    },

    // Scrub: the last six seconds laid out as a strip, now at the right. The
    // head sits at Pos (gliding there over Glide) and loops a grain of
    // Grain's length; the lens above shows that grain, magnified, playing
    // the way Dir says. Spray scatters where each grain starts. LIVE: the
    // strip keeps moving; HOLD: it stops, frozen; STOMP: frozen and lit.
    scrub(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 107); s.w = Float32Array.from({ length: 600 }, (_, i) => { const ph = i % 47; return (ph < 2 ? 1 : Math.exp(-ph / 14)) * (0.3 + 0.7 * s.r()); });
        s.head = 1; s.off = 0; s.sp = []; s.gr = 0; }
      const pos = q(/^pos$/i, 1), grainRaw = q.raw(/^grain$/i, 50), rec = q.raw(/^rec$/i, 0), glide = q(/^glide$/i, 0.4), dir = q.raw(/^dir$/i, 0), spray = q(/^spray$/i, 0), mix = q(R.mix, 0.5);
      const grainS = grainRaw <= 100 ? 0.01 * Math.pow(100, grainRaw / 100) : [1 / 16, 1 / 8, 1 / 6, 1 / 4, 1 / 3, 3 / 8, 1 / 2, 2 / 3, 3 / 4, 1, 1.5, 2][Math.min(11, grainRaw - 101)] * 2;
      if (rec === 0 && v.live) s.off += dt * 100;               // LIVE: the recording moves on
      s.head += (pos - s.head) * (glide < 0.02 ? 1 : 1 - Math.exp(-dt / (0.02 + 3 * glide * glide)));
      const X0 = 8, X1 = 248, W = X1 - X0, sy = 104, hx = X0 + s.head * W, gw = Math.max(2, grainS / 6 * W);
      // the strip
      for (let x = X0; x < X1; x += 1.5) {
        const a = s.w[Math.floor((x - X0) / W * 600 + s.off) % 600], near = Math.abs(x - hx) < gw / 2;
        g.fillStyle = col(near ? 0.9 : rec ? 0.4 : 0.3, near ? T.hi : T.lo); g.fillRect(x, sy - a * 14, 1.2, a * 28);
      }
      g.fillStyle = col(0.5, T.lo); g.font = "bold 8px ui-monospace, monospace";
      g.fillText(["LIVE", "HOLD", "STOMP"][rec] || "", X0, 124);
      g.textAlign = "right"; g.fillText(Math.round((1 - s.head) * 6000) + "ms", X1, 124); g.textAlign = "start";
      // spray: where grains have started
      if (v.live && t - s.gr > Math.max(0.05, grainS)) { s.gr = t; s.sp.push({ x: hx + (s.r() - 0.5) * spray * 0.0833 * W, t }); if (s.sp.length > 12) s.sp.shift(); }
      for (const p of s.sp) { const f = 1 - (t - p.t) / 1.2; if (f <= 0) continue; g.fillStyle = col(f * 0.8, T.hi); g.fillRect(p.x - 0.7, sy - 20, 1.4, 4); }
      // the head and the lens
      g.fillStyle = col(1, T.lo); g.fillRect(hx - 0.8, sy - 22, 1.6, 44);
      const lx = Math.max(52, Math.min(204, hx)), ly = 40, lw = 92, lh = 28;
      g.strokeStyle = col(0.6, T.lo); g.lineWidth = 1;
      g.beginPath(); g.moveTo(hx - gw / 2, sy - 22); g.lineTo(lx - lw / 2, ly + lh); g.moveTo(hx + gw / 2, sy - 22); g.lineTo(lx + lw / 2, ly + lh); g.stroke();
      g.strokeRect(lx - lw / 2, ly - lh, lw, lh * 2);
      const gph = (t / Math.max(0.02, grainS)) % 1, gi = Math.floor(t / Math.max(0.02, grainS));
      const back = dir === 1 || (dir === 2 && gi % 2) || (dir === 3 && ((gi * 7919) % 3 === 0));
      const c0 = Math.floor((s.head * 600 - grainS * 50 + s.off) + 600) % 600, nS = Math.max(4, Math.round(grainS * 100));
      for (let i = 0; i < lw - 4; i += 1.5) {
        const f = i / (lw - 4), a = s.w[(c0 + Math.floor(f * nS)) % 600];
        const lit = Math.abs(f - (back ? 1 - gph : gph)) < 0.04;
        g.fillStyle = col(lit ? 1 : 0.4 + 0.5 * mix, lit ? T.lo : T.hi); g.fillRect(lx - lw / 2 + 2 + i, ly - a * (lh - 3), 1.2, a * (lh - 3) * 2);
      }
      g.fillStyle = col(0.8, T.lo); g.font = "bold 8px ui-monospace, monospace"; g.textAlign = "center";
      g.fillText(["→", "←", "↔", "?"][dir] || "→", lx, ly - lh - 3); g.textAlign = "start";
    },

    // GridDly: the echoes on grid paper, a column per sixteenth at Tempo.
    // Each repeat is a mark Time apart, smaller as Fdbk lets it go, drawn the
    // engine's way: DIGI clean squares, TAPE soft and wavering, DUB thick and
    // dark, REVRS ramps that swell backwards, TAPS three in each Time, LOFI
    // coarser blocks each time. Duck dips the marks while you play.
    griddly(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.note = 0; s.next = 0.2; }
      const type = q.raw(/^type$/i, 0), traw = q.raw(R.time, 108), fd = q.raw(/^fdbk$/i, 45) / 100, tone = q(R.tilt, 0.6), char = q(/^char$/i, 0.3);
      const duck = q(/^duck$/i, 0), mix = q(R.mix, 0.5), bpm = Math.max(40, Math.min(240, (q.raw(/^tempo$/i, 120) - 1) % 241 + 1 || 120));
      const beat = 60 / bpm, timeS = traw <= 100 ? 0.012 + 0.988 * (traw / 100) ** 2 : [1 / 8, 1 / 6, 1 / 4, 1 / 3, 3 / 8, 1 / 2, 2 / 3, 3 / 4, 1, 1.5, 2, 4, 8][Math.min(12, traw - 101)] * beat;
      const cols = 16, step = beat / 4, X0 = 8, X1 = 248, cw = (X1 - X0) / cols, cy = 70;
      const barS = cols * step;
      if (v.live && t >= s.next) { s.note = t; s.next = t + barS; }
      const since = t - s.note, play = since < 0.15;
      // the paper
      for (let i = 0; i <= cols; i++) { g.fillStyle = col(i % 4 ? 0.12 : 0.3, T.lo); g.fillRect(X0 + i * cw - 0.4, 22, 0.8, 96); }
      for (let j = 0; j <= 6; j++) { g.fillStyle = col(0.12, T.lo); g.fillRect(X0, 22 + j * 16 - 0.4, X1 - X0, 0.8); }
      // the dry hit, then the repeats
      const amp0 = 1, dk = 1 - duck * 0.7 * (play ? 1 : 0);
      const taps = type === 4 ? [[0.25, 0.5, 1], [3 / 8, 3 / 4, 1], [1 / 3, 2 / 3, 1], [0.5, 0.75, 1]][Math.min(3, Math.floor(char * 4))] : [1];
      const marks = [{ x: 0, a: amp0, n: 0 }];
      for (let n = 1, a = 1; n < 40; n++) {
        a *= Math.min(1.05, fd) * (type === 2 ? 0.88 : 0.97);
        if (a < 0.04 || n * timeS > barS) break;
        taps.forEach((f, k) => marks.push({ x: (n - 1 + f) * timeS, a: a * (k === taps.length - 1 ? 1 : 0.55 + 0.2 * k), n }));
      }
      for (const m of marks) {
        const x = X0 + m.x / barS * (X1 - X0); if (x > X1) continue;
        const shown = since >= m.x, a = clamp(m.a) * (m.n ? dk : 1), lit = shown && since - m.x < 0.12;
        const sz = 6 + 26 * Math.sqrt(a), c = m.n ? (lit ? T.lo : T.hi) : T.lo, y = cy;
        if (!shown) { g.strokeStyle = col(0.3, T.hi); g.lineWidth = 1; g.strokeRect(x + 0.5, y - sz / 2, Math.max(2, cw * 0.6), sz); continue; }
        g.fillStyle = col((0.45 + 0.55 * (m.n ? (0.5 + 0.5 * mix) : 1)) * (0.6 + 0.4 * tone), c);
        if (type === 0 || m.n === 0) g.fillRect(x, y - sz / 2, Math.max(2, cw * 0.6), sz);
        else if (type === 1) { for (let i = 0; i < 6; i++) { const w = Math.sin(t * 7 + m.n + i) * 1.5 * (0.3 + char); g.fillRect(x + i * 1.2, y - sz / 2 + w, 1.2, sz); } }
        else if (type === 2) { g.fillRect(x - 1, y - sz / 3, Math.max(3, cw * 0.8), sz * 0.66); }
        else if (type === 3) { g.beginPath(); g.moveTo(x, y + sz / 2); g.lineTo(x + cw * 0.8, y + sz / 2); g.lineTo(x + cw * 0.8, y - sz / 2); g.closePath(); g.fill(); }
        else if (type === 4) g.fillRect(x, y - sz / 2, Math.max(1.5, cw * 0.35), sz);
        else { const b = 1 + Math.min(6, m.n * (0.5 + char * 1.5)); for (let yy = -sz / 2; yy < sz / 2; yy += b + 1) g.fillRect(x, y + yy, Math.max(2, cw * 0.6), b); }
      }
      g.fillStyle = col(0.9, T.lo); g.fillRect(X0 + (since % barS) / barS * (X1 - X0) - 0.6, 18, 1.2, 104);
      g.fillStyle = col(0.8, T.lo); g.font = "bold 8px ui-monospace, monospace";
      g.fillText(["DIGI", "TAPE", "DUB", "REVRS", "TAPS", "LOFI"][type] + "  " + (traw <= 100 ? Math.round(timeS * 1000) + "ms" : ["1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/4.", "1/2", "1bar", "2bar"][traw - 101]), X0, 13);
    },

    // Ring: the signal wrapped round a circle that spins, as on the pedal:
    // Time how much of it goes round, Size how far it pushes the ring in and
    // out, Spin which way it turns (50 still). A phosphor afterglow.
    ring(g, q, v, t, C, dt) {
      const s = v.s, cv = v.canvas;
      const RW = Math.max(200, Math.min(480, Math.round(cv.width))) & ~1, RH = RW >> 1, k = RW / LW;
      if (!s.off || s.off.width !== RW) {
        s.off = document.createElement("canvas"); s.off.width = RW; s.off.height = RH;
        s.ctx = s.off.getContext("2d"); s.img = s.ctx.createImageData(RW, RH);
        s.out = new Uint32Array(s.img.data.buffer); s.I = new Float32Array(RW * RH);
        if (!s.r) { s.r = rng(v.e.id || 109); s.env = 0.5; s.next = 0; s.ang = 0; s.f = 1; }
      }
      const time = q(R.time, 0.25), size = q(R.size, 0.35), spin = q(/^spin$/i, 0.6);
      if (v.live && t >= s.next) { s.env = 0.6 + 0.4 * s.r(); s.f = [1, 1.5, 2, 3][(s.r() * 4) | 0]; s.next = t + 0.9 + s.r() * 1.5; }
      s.env *= Math.exp(-dt / 1.6); s.ang += dt * (spin - 0.5) * 2 * 4;
      const I = s.I, fade = Math.exp(-dt / 0.1);
      for (let i = 0; i < I.length; i++) I[i] *= fade;
      const cx = RW / 2, cy = RH / 2, R0 = RH * 0.3, cyc = 3 + time * 40;
      const amp = (0.08 + 0.6 * size) * (0.15 + s.env) * R0;
      let lx = 0, ly = 0;
      for (let j = 0; j <= 720; j++) {
        const u = j / 720, a = u * TAU + s.ang, w = u * cyc * TAU * s.f;
        const r = R0 + amp * (Math.sin(w + t * 3) + 0.4 * Math.sin(2 * w + 1));
        const x = cx + Math.cos(a) * r * 1.7, y = cy + Math.sin(a) * r;
        if (j) { const n = Math.max(1, Math.ceil(Math.hypot(x - lx, y - ly))); for (let m = 0; m < n; m++) { const xi = (lx + (x - lx) * m / n) | 0, yi = (ly + (y - ly) * m / n) | 0;
          if (xi >= 0 && yi >= 0 && xi < RW && yi < RH) I[yi * RW + xi] += 0.55 / Math.sqrt(n); } }
        lx = x; ly = y;
      }
      if (!s.lut || s.lutKey !== C.bg + C.fg) {
        s.lutKey = C.bg + C.fg; s.lut = new Uint32Array(64); const T = tones(C), bg = rgbOf(C.bg);
        for (let i = 0; i < 64; i++) { const u = i / 63; s.lut[i] = pack(u < 0.5 ? lerp3(bg, T.hi, u / 0.5) : lerp3(T.hi, T.lo, (u - 0.5) / 0.5)); }
      }
      const out = s.out, lut = s.lut;
      for (let i = 0; i < I.length; i++) { const u = I[i]; out[i] = lut[u >= 1 ? 63 : (u * 63) | 0]; }
      s.ctx.putImageData(s.img, 0, 0);
      g.drawImage(s.off, 0, 0, LW, LH);
    },

    // Spool: the tape machine. Two reels turning; between them the tape runs
    // past the record head and the play head (Div at Tempo apart), carrying
    // the echoes as marks that come round again, fading as Feed lets them.
    // Flutter and Wow shake the tape and the reels' speed, Wear drops specks
    // out of it, Drive thickens the marks, Spring sets a spring shivering
    // under the deck. Mix: how bright.
    spool(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 113); s.marks = []; s.next = 0.2; s.rot = 0; s.spr = 0; }
      const bpm = Math.max(40, Math.min(240, q.raw(/^tempo$/i, 120) || 120)), divi = Math.min(5, Math.floor(q.raw(/^div$/i, 67) / 16.7));
      const delay = [0.25, 1 / 3, 0.5, 0.75, 1, 1.5][divi] * 60 / bpm, feed = q(/^feed$/i, 0.35), flut = q(/^flutter$/i, 0.35), wow = q(/^wow$/i, 0.25);
      const wear = q(/^wear$/i, 0.55), drive = q(R.drive, 0.35), spring = q(/^spring$/i, 0), mix = q(R.mix, 0.35);
      const RL = [52, 50], RR = [204, 50], rad = 26, recX = 92, playX = 92 + 40 + 60 * Math.min(1, delay / 0.75), tapeY = 96;
      const speed = 60 * (1 + wow * 0.06 * Math.sin(t * 1.3) + flut * 0.02 * Math.sin(t * 23));
      if (v.live) { s.rot += dt * speed / rad; for (const m of s.marks) m.x += dt * speed * (playX - recX) / (delay * 60) / (speed / 60); }
      if (v.live && t >= s.next) { s.marks.push({ x: recX, a: 1, n: 0 }); s.next = t + 0.9 + s.r() * 1.2; s.spr = 1; }
      s.spr *= Math.exp(-dt * 4);
      const keep = [];
      for (const m of s.marks) {
        if (m.x >= playX) { if (m.a * feed * 0.98 > 0.06) keep.push({ x: recX, a: m.a * feed * 0.98, n: m.n + 1 }); s.spr = Math.max(s.spr, m.a); continue; }
        keep.push(m);
      }
      s.marks = keep.slice(-14);
      // reels
      for (const [cx, cy] of [RL, RR]) {
        g.strokeStyle = col(0.7, T.lo); g.lineWidth = 1.5; g.beginPath(); g.arc(cx, cy, rad, 0, TAU); g.stroke();
        g.beginPath(); g.arc(cx, cy, 6, 0, TAU); g.stroke();
        for (let k = 0; k < 3; k++) { const a = s.rot + k * TAU / 3; g.beginPath(); g.moveTo(cx + Math.cos(a) * 8, cy + Math.sin(a) * 8); g.lineTo(cx + Math.cos(a) * (rad - 4), cy + Math.sin(a) * (rad - 4)); g.stroke(); }
      }
      // the tape path, shaking with flutter
      const wob = x => tapeY + Math.sin(x * 0.12 + t * 18) * flut * 1.6 + Math.sin(x * 0.03 + t * 1.3) * wow * 1.5;
      g.strokeStyle = col(0.55, T.lo); g.lineWidth = 2; g.beginPath(); g.moveTo(RL[0] - rad, RL[1]); g.lineTo(RL[0] - rad + 2, tapeY);
      for (let x = RL[0] - rad + 2; x <= RR[0] + rad - 2; x += 3) g.lineTo(x, wob(x));
      g.lineTo(RR[0] + rad, RR[1]); g.stroke();
      // wear: dropouts
      if (wear > 0.02) { g.fillStyle = C.bg; for (let i = 0; i < Math.round(wear * 10); i++) { const x = RL[0] - rad + 4 + ((i * 37 + Math.floor(t * 60 * (1 + i % 3))) % (RR[0] - RL[0] + 2 * rad - 8)); g.fillRect(x, wob(x) - 1.5, 2, 3); } }
      // heads
      for (const [x, lab] of [[recX, "R"], [playX, "P"]]) { g.fillStyle = col(0.85, T.lo); g.fillRect(x - 4, tapeY + 4, 8, 7); g.fillStyle = C.bg; g.font = "bold 6px ui-monospace, monospace"; g.fillText(lab, x - 2, tapeY + 10); }
      // the echoes on the tape
      for (const m of s.marks) { const h = (3 + 7 * m.a) * (1 + drive * 0.6); g.fillStyle = col((0.4 + 0.6 * m.a) * (0.5 + 0.5 * mix), m.n ? T.hi : T.lo); g.fillRect(m.x - 1.5 - drive * 1.5, wob(m.x) - h, 3 + drive * 3, h * 2); }
      // the spring
      if (spring > 0.02) { g.strokeStyle = col(0.3 + 0.5 * spring, T.hi); g.lineWidth = 1; g.beginPath();
        for (let x = 30; x <= 226; x += 2) { const y = 120 + Math.sin(x * 0.9) * 2.5 + Math.sin(x * 0.2 + t * 30) * s.spr * spring * 3; x === 30 ? g.moveTo(x, y) : g.lineTo(x, y); } g.stroke(); }
    },

    // Gyre: a held loop as a vortex. The loop's sound circulates as a swarm
    // of particles; two grain windows, half a turn apart, sweep round and
    // light what they pass (they sum to one, so it holds). Rise moves every
    // pass outward (up) or inward (down), Glide adds a drift after each
    // phrase over Span, Feed pulls new sound in from the edge, Decay lets
    // the swarm fade (0 holds), Tone dims it each pass. Size: the loop.
    gyre(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 127); s.p = Array.from({ length: 150 }, (_, i) => ({ a: s.r() * TAU, r: [0.4, 0.6, 0.8][i % 3] + (s.r() - 0.5) * 0.06, e: 0.4 + 0.6 * s.r() })); s.phr = 0; s.next = 0; }
      const size = q(R.size, 0.45), feed = q(/^feed$/i, 0.6), decay = q(/^decay$/i, 0), rise = q(/^rise$/i, 0.5), glide = q(/^glide$/i, 0), span = q(/^span$/i, 0.5);
      const tone = q(R.tilt, 0.55), mix = q(R.mix, 0.5);
      const lenS = 0.04 + 2.86 * size * size, w = TAU / Math.max(0.6, lenS * 1.5);
      if (v.live && t >= s.next) { s.phr = t; s.next = t + 3 + s.r() * 2; }
      const ramp = glide * (1 - Math.exp(-(t - s.phr) / (0.19 + 7.8 * span ** 3)));
      const drift = ((rise - 0.5) * 0.25 + ramp * 0.12) * dt / Math.max(0.3, lenS);
      const life = decay < 0.01 ? 1 : Math.exp(-dt * (0.2 + 3 * decay * decay));
      const cx = 128, cy = 64;
      if (v.live) for (const p of s.p) {
        p.a += w * dt * (0.9 + 0.2 * p.e); p.r += drift; p.e *= life * (1 - dt * (1 - tone) * 0.05);
        if (p.r > 1.05 || p.r < 0.08 || p.e < 0.03) { if (s.r() < 0.3 + feed) { p.r = [0.4, 0.6, 0.8][(s.r() * 3) | 0]; p.a = s.r() * TAU; p.e = 0.5 + 0.5 * s.r(); } else p.e = 0; }
        if (feed > 0.02 && s.r() < feed * dt * 0.5) { p.r = 1; p.a = s.r() * TAU; p.e = 0.8; }
      }
      const g1 = (t * w) % TAU;
      for (const [ga, gc] of [[g1, T.hi], [g1 + Math.PI, T.lo]]) { g.strokeStyle = col(0.25, gc); g.lineWidth = 6; g.beginPath(); g.ellipse(cx, cy, 104, 56, 0, ga - 0.5, ga + 0.5); g.stroke(); }
      for (const p of s.p) {
        if (p.e <= 0.02) continue;
        const d1 = Math.abs(((p.a - g1) % TAU + TAU + Math.PI) % TAU - Math.PI), win = Math.cos(Math.min(Math.PI / 2, d1 / 2)) ** 2;
        const lit = 0.3 + 0.7 * Math.max(win, 1 - win);
        const x = cx + Math.cos(p.a) * p.r * 100, y = cy + Math.sin(p.a) * p.r * 54;
        g.fillStyle = col(p.e * lit * (0.45 + 0.55 * mix), win > 0.5 ? T.hi : T.lo);
        const sz = 1.2 + 2.2 * p.e;
        for (let k = 0; k < 7; k++) { const aa = p.a - k * 0.035 * (1 + w); g.globalAlpha = 1 - k * 0.13;
          g.fillRect(cx + Math.cos(aa) * p.r * 100 - sz / 2, cy + Math.sin(aa) * p.r * 54 - sz / 2, sz, sz); }
        g.globalAlpha = 1;
      }
      g.fillStyle = col(0.7, T.lo); g.font = "bold 8px ui-monospace, monospace";
      g.fillText(decay < 0.01 ? "HOLD" : "FADE", 6, 12);
    },

    // DubSiren: the siren's pitch drawn as a thick neon sweep across time
    // (Mode's shape: Wail, Fast, Slow, Laser; Rate how often, Depth how far,
    // Pitch where), and its tape echoes as fainter copies trailing behind,
    // Time apart, as many as Fdbk keeps. Trig Pulse fires it in bursts.
    dubsiren(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const trig = q.raw(/^trig$/i, 1), mode = q.raw(/^mode$/i, 1), pitch = q(/^pitch$/i, 0.48), rraw = q.raw(R.rate, 67), depth = q(R.depth, 0.34);
      const vol = q(/^vol$/i, 0.5), time = q(R.time, 0.4), fd = q.raw(/^fdbk$/i, 70) / 100, bpm = Math.max(40, Math.min(240, (q.raw(/^tempo$/i, 120) - 1) % 241 + 1 || 120));
      const hz = rraw <= 0 ? 0.3 : rraw <= 100 ? 0.15 * Math.pow(100, rraw / 100) : 1 / ([16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 1 / 16, 1 / 32, 1 / 24, 1 / 12][Math.min(11, rraw - 101)] * 60 / bpm * 4) / 4;
      const f = hz * [1, 2, 0.5, 1][mode] || 1, echoS = 0.05 + 0.95 * time, V = 2.6, X0 = 6, X1 = 250;
      const on = tt => trig % 2 === 0 ? 1 : ((tt % 2.2) < 0.45 ? 1 : 0);
      const lfo = tt => { const ph = (tt * f) % 1; return mode === 3 ? 1 - ph : 0.5 + 0.5 * Math.sin(ph * TAU); };
      const yOf = tt => { const base = 104 - pitch * 70; return base - (lfo(tt) - 0.5) * depth * 70; };
      const drawSweep = (lag, a, wdt, c) => {
        g.strokeStyle = col(a, c); g.lineWidth = wdt; g.beginPath(); let pen = false;
        for (let x = X0; x <= X1; x += 1.5) {
          const tt = t - lag - (X1 - x) / (X1 - X0) * V;
          if (tt < 0 || !on(tt)) { pen = false; continue; }
          const y = yOf(tt) + lag * 9;
          pen ? g.lineTo(x, y) : g.moveTo(x, y); pen = true;
        }
        g.stroke();
      };
      for (let n = 6; n >= 1; n--) { const a = Math.pow(Math.min(1.1, fd), n); if (a < 0.05) continue; drawSweep(n * echoS, Math.min(1, a) * 0.5 * (0.4 + 0.6 * vol), 1, T.lo); }
      drawSweep(0, 0.25 * (0.4 + 0.6 * vol), 6, T.hi);
      drawSweep(0, 0.6 + 0.4 * vol, 2.2, T.hi);
      g.fillStyle = col(0.8, T.lo); g.font = "bold 8px ui-monospace, monospace";
      g.fillText(["WAIL", "FAST", "SLOW", "LASER"][mode] + "  " + ["HOLD", "PULSE", "SHOLD", "SPULS"][trig], X0, 12);
    },

    // Scorch: the amp as fire. The sound feeds a row of flame along the
    // bottom, shaped like its waveform; Gain sets how hot it burns and how
    // high the flames climb, each note a flare-up; Mix how bright.
    scorch(g, q, v, t, C, dt) {
      const s = v.s, GW = 128, GH = 64;
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 131); s.H = new Float32Array(GW * (GH + 2)); s.env = 0.5; s.next = 0; s.img = null; }
      const gain = q(R.drive, 0.22), mix = q(R.mix, 0.5);
      if (v.live && t >= s.next) { s.env = 1; s.next = t + 0.6 + s.r() * 1.2; }
      s.env *= Math.exp(-dt / 0.7);
      const H = s.H, src = (0.7 + 0.5 * gain) * (0.6 + 0.4 * s.env), cool = 0.008 + (1 - gain) * 0.03;
      if (v.live) {
        for (let x = 0; x < GW; x++) { const w = 0.6 + 0.4 * Math.sin(x * 0.22 + t * 3) * Math.sin(x * 0.07 - t); H[(GH + 1) * GW + x] = H[GH * GW + x] = src * w * (0.6 + 0.4 * s.r()); }
        for (let y = 0; y < GH; y++) for (let x = 0; x < GW; x++) {
          const xl = x ? x - 1 : x, xr = x < GW - 1 ? x + 1 : x;
          const a = (H[(y + 1) * GW + xl] + H[(y + 1) * GW + x] + H[(y + 1) * GW + xr] + H[(y + 2) * GW + x]) / 4;
          H[y * GW + x] = Math.max(0, a - cool * (0.5 + s.r()) * 0.35);
        }
      }
      if (!s.lut || s.lutKey !== C.bg + C.fg + mix.toFixed(2)) {
        s.lutKey = C.bg + C.fg + mix.toFixed(2); const T = tones(C); s.lut = [];
        for (let i = 0; i < 32; i++) { const u = i / 31 * (0.6 + 0.4 * mix); s.lut.push(css(u < 0.15 ? T.bg : u < 0.6 ? lerp3(T.bg, T.hi, (u - 0.15) / 0.45) : lerp3(T.hi, T.lo, (u - 0.6) / 0.4))); }
      }
      for (let y = 0; y < GH; y++) for (let x = 0; x < GW; x++) {
        const h = H[y * GW + x]; if (h < 0.08) continue;
        g.fillStyle = s.lut[Math.min(31, (h * 1.3 * 31) | 0)]; g.fillRect(x * 2, y * 2, 2, 2);
      }
    },

    // DirtBox: the chosen pedal's clipping curve (input across, output up),
    // a ball riding it as an acid line plays, and the result traced to the
    // right; Drive steepens the curve, ZNR cuts a quiet band in the middle,
    // Tone and the three-band EQ (the sliders) colour it, Level and Mix how
    // bright. Model: TS9, DIST+, DS-1, RAT2, MUFF, SFUZZ, MT-2.
    dirtbox(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.tr = new Float32Array(100); s.k = 0; }
      const model = Math.min(6, q.raw(/^model$/i, 3)), drive = q(R.drive, 0.5), tone = q(R.tilt, 0.8), znr = q(R.noise, 0.5);
      const eq = ["low", "mid", "high"].map(n => (q.raw(new RegExp("^" + n + "$", "i"), 12) - 12) / 12), level = q(/^level$/i, 0.5), mix = q(/^mix$/i, 1);
      const gn = 1 + drive * 24, th = Math.tanh;
      const f = x => { const y = gn * x;
        return [th(y * 0.5) * 0.7 + x * 0.3, y > 0 ? th(y) : th(0.7 * y) * 0.85, y / Math.pow(1 + y ** 4, 0.25), 2 / Math.PI * Math.atan(y * 1.5),
          th(th(y) * 3), Math.abs(th(y)) * 1.4 - 0.7 + 0.3 * th(y), Math.sign(y) * Math.min(1, Math.sqrt(Math.abs(y) * 0.6))][model]; };
      const gate = x => Math.abs(x) < znr * 0.08 ? 0 : 1;
      // acid line: steps of a note with an accent and a decaying envelope
      const acid = tt => { const st = Math.floor(tt * 8), u = tt * 8 - st, acc = ((st * 7) % 5 + 5) % 5 === 0 ? 1 : 0.6;
        return Math.sin(tt * TAU * (2 + (((st % 4) + 4) % 4) * 0.5)) * acc * (0.3 + 0.7 * Math.exp(-u * 3)); };
      const inp = acid(t), out = f(inp) * gate(inp);
      for (let i = 0; i < 100; i++) { const x = acid(t - (99 - i) * 0.012); s.tr[i] = f(x) * gate(x); }
      // the curve
      const PX = 14, PY = 22, PW = 92, PH = 92, cx = PX + PW / 2, cy = PY + PH / 2;
      g.strokeStyle = col(0.25, T.lo); g.lineWidth = 1; g.strokeRect(PX, PY, PW, PH);
      g.beginPath(); g.moveTo(PX, cy); g.lineTo(PX + PW, cy); g.moveTo(cx, PY); g.lineTo(cx, PY + PH); g.stroke();
      if (znr > 0.02) { g.fillStyle = col(0.15, T.hi); const w = znr * 0.08 * PW / 2; g.fillRect(cx - w, PY, w * 2, PH); }
      g.strokeStyle = col(0.9, T.hi); g.lineWidth = 2; g.beginPath();
      for (let i = 0; i <= 92; i++) { const x = i / 46 - 1, y = f(x) * gate(x); i ? g.lineTo(PX + i, cy - y * PH / 2 * 0.9) : g.moveTo(PX, cy - y * PH / 2 * 0.9); }
      g.stroke();
      g.fillStyle = col(1, T.lo); g.beginPath(); g.arc(cx + inp * PW / 2, cy - out * PH / 2 * 0.9, 3, 0, TAU); g.fill();
      // the output, traced
      g.strokeStyle = col((0.4 + 0.6 * level) * (0.4 + 0.6 * mix), T.lo); g.lineWidth = 1.4; g.beginPath();
      for (let i = 0; i < 100; i++) { const x = 116 + i * 1.05, y = cy - s.tr[i] * PH / 2 * 0.9 * (0.6 + 0.4 * tone); i ? g.lineTo(x, y) : g.moveTo(x, y); }
      g.stroke();
      // EQ sliders
      ["L", "M", "H"].forEach((nm, k) => { const x = 228 + k * 9; g.fillStyle = col(0.25, T.lo); g.fillRect(x, PY + 10, 1.2, PH - 20);
        g.fillStyle = col(0.9, T.hi); g.fillRect(x - 2.5, cy - eq[k] * (PH / 2 - 12) - 1.5, 6, 3); g.fillStyle = col(0.6, T.lo); g.font = "bold 7px ui-monospace, monospace"; g.fillText(nm, x - 1.5, PY + PH + 2); });
      g.fillStyle = col(0.85, T.lo); g.font = "bold 9px ui-monospace, monospace";
      g.fillText(["TS9", "DIST+", "DS-1", "RAT2", "MUFF", "SFUZZ", "MT-2"][model], PX, 14);
    },

    // WaveFold: a wave pushed past two mirrors and folded back between them.
    // The faint line is the wave as it would go (Drive pushes it further),
    // the bright one what comes out; Symm slides it off centre, Tone rounds
    // the folds (dark) or leaves them sharp (bright). Level, Mix: brightness.
    wavefold(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.env = 0.6; s.next = 0; s.r = rng(v.e.id || 137); }
      const drive = q(R.drive, 0.4), symm = q(/^symm$/i, 0.5), tone = q(R.tilt, 0.6), level = q(/^level$/i, 0.5), mix = q(R.mix, 1);
      if (v.live && t >= s.next) { s.env = 1; s.next = t + 1 + s.r() * 1.5; }
      s.env += ((0.45) - s.env) * Math.min(1, dt * 0.8);
      const gn = (1 + drive * 5) * s.env * 1.6, off = (symm - 0.5) * 1.4, cy = 66, A = 30, X0 = 6, X1 = 250;
      const tri = x => { let y = ((x + 1) % 4 + 4) % 4; return y < 2 ? y - 1 : 3 - y; };
      const fold = x => tone * tri(x) + (1 - tone) * Math.sin(Math.PI / 2 * x);
      // mirrors
      g.fillStyle = col(0.45, T.lo);
      for (let x = X0; x < X1; x += 6) { g.fillRect(x, cy - A - 0.6, 3, 1.2); g.fillRect(x, cy + A - 0.6, 3, 1.2); }
      // the wave as it would go
      g.strokeStyle = col(0.25, T.lo); g.lineWidth = 1; g.beginPath();
      for (let x = X0; x <= X1; x += 1.5) { const a = (x - X0) / 60 * TAU + t * 1.6, y = cy - (Math.sin(a) * gn + off) * A; x === X0 ? g.moveTo(x, y) : g.lineTo(x, y); }
      g.stroke();
      // folded
      g.strokeStyle = col((0.5 + 0.5 * level) * (0.45 + 0.55 * mix), T.hi); g.lineWidth = 2; g.beginPath();
      for (let x = X0; x <= X1; x += 1) { const a = (x - X0) / 60 * TAU + t * 1.6, y = cy - fold(Math.sin(a) * gn + off) * A; x === X0 ? g.moveTo(x, y) : g.lineTo(x, y); }
      g.stroke();
      g.fillStyle = col(0.75, T.lo); g.font = "bold 8px ui-monospace, monospace";
      g.fillText(Math.max(0, Math.floor(gn + Math.abs(off) - 1) + (gn + Math.abs(off) > 1 ? 1 : 0)) + " FOLDS", X0, 14);
    },

    // Dustbox: a vacuum cleaner. Dust drifts in from the left and is sucked
    // into the nozzle; Motor is the pull (faster, more turbulent), Dust grows
    // big chunky clumps among the specks that lurch at half the pace (the
    // sub-octave), Filter dulls (dark) or sharpens (bright) them, Power how
    // much there is.
    dustbox(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 139); s.p = []; }
      const dust = q(/^dust$/i, 0.5), motor = q(/^motor$/i, 0.6), filt = q(/^filter$/i, 0.5), power = q(/^power$/i, 0.5);
      const NX = 226, NY = 64, n = Math.round(30 + 110 * power);
      while (s.p.length < n) s.p.push({ x: s.r() * 200, y: 10 + s.r() * 108, big: s.r() < dust * 0.45, ph: s.r() * TAU, sz: 0.6 + s.r() });
      if (s.p.length > n) s.p.length = n;
      const pull = 25 + 140 * motor;
      for (const p of s.p) {
        if (v.live) {
          const dx = NX - p.x, dy = NY - p.y, d = Math.hypot(dx, dy) + 1, k = pull * (p.big ? 0.5 * (Math.sin(t * 6 + p.ph) > 0 ? 1.6 : 0.2) : 1) / Math.sqrt(d / 40);
          p.x += (dx / d * k + Math.sin(t * 9 + p.ph) * motor * 18) * dt; p.y += (dy / d * k + Math.cos(t * 7 + p.ph) * motor * 14) * dt;
          if (d < 8) { p.x = -4 - s.r() * 30; p.y = 10 + s.r() * 108; p.big = s.r() < dust * 0.45; }
        }
        const sz = p.big ? 3 + 4 * dust * p.sz : 1 + 0.8 * p.sz;
        g.fillStyle = col((p.big ? 0.75 : 0.5) * (0.5 + 0.5 * filt), p.big ? T.hi : T.lo);
        if (filt < 0.35) { g.globalAlpha = 0.5; g.fillRect(p.x - sz, p.y - sz, sz * 2, sz * 2); g.globalAlpha = 1; }
        g.fillRect(p.x - sz / 2, p.y - sz / 2, sz, sz);
      }
      // the nozzle
      g.fillStyle = col(0.85, T.lo);
      g.beginPath(); g.moveTo(NX - 6, NY - 16); g.lineTo(NX + 4, NY - 8); g.lineTo(256, NY - 8); g.lineTo(256, NY + 8); g.lineTo(NX + 4, NY + 8); g.lineTo(NX - 6, NY + 16); g.closePath(); g.fill();
      g.fillStyle = C.bg; g.fillRect(NX - 5, NY - 3, 3, 6);
    },

    // Corrupt: three square waves, the played note (middle), its octave down
    // (Sub) and its octave up (Wave), in thin lanes along the top, summed into
    // the big stepped wave below; Tone rounds its corners (dark) or leaves
    // them raw. Each note glitches it: bands of the picture jump sideways.
    corrupt(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 149); s.next = 0; s.gl = []; }
      const sub = q(/^sub$/i, 0.82), tone = q(R.tilt, 0.45), wave = q(/^wave$/i, 0.5), mix = q(R.mix, 0.5);
      if (v.live && t >= s.next) { s.next = t + 0.7 + s.r() * 1.2; s.gl = Array.from({ length: 4 + (s.r() * 4 | 0) }, () => ({ y: 40 + s.r() * 80, h: 3 + s.r() * 10, dx: (s.r() - 0.5) * 40, t })); }
      const P = 48, X0 = 6, X1 = 250, ph = t * 30;
      const sq = (x, per) => ((((x + ph) / per) % 1 + 1) % 1) < 0.5 ? 1 : -1;
      const lanes = [[sub, P * 2, 12], [1, P, 22], [wave, P / 2, 32]];
      lanes.forEach(([a, per, y]) => { g.strokeStyle = col(0.25 + 0.6 * a, T.lo); g.lineWidth = 1; g.beginPath();
        for (let x = X0; x <= X1; x++) { const yy = y - sq(x, per) * 3; x === X0 ? g.moveTo(x, yy) : g.lineTo(x, yy); } g.stroke(); });
      const cy = 86, A = 26 / (1 + sub + wave) * 1.6, sm = (1 - tone) * 6;
      const val = x => { let a = 0, n = 0; for (let k = -sm; k <= sm; k++) { a += sq(x + k, P) + sub * sq(x + k, P * 2) + wave * sq(x + k, P / 2); n++; } return a / n; };
      for (const gl of s.gl) if (t - gl.t < 0.25) { g.save(); g.beginPath(); g.rect(0, gl.y, 256, gl.h); g.clip(); g.translate(gl.dx * (1 - (t - gl.t) / 0.25), 0); drawBig(); g.restore(); }
      drawBig();
      function drawBig() {
        g.fillStyle = col(0.35 + 0.6 * mix, T.hi);
        for (let x = X0; x < X1; x += 1) { const y = val(x) * A; g.fillRect(x, y > 0 ? cy - y : cy, 1, Math.abs(y) + 0.6); }
      }
    },

    // Tonic: its three processors as a chain of boxes, in Order (Fwd: Drive,
    // Phase, then the Mode's; Rev the other way), each showing what it does
    // to the wave passing through: Drive flattens its tops, Phase sweeps
    // notches through it, the Mode box decimates, crushes, wobbles (FM) or
    // ring-modulates. A box at zero just passes it on. Mix: brightness.
    tonic(g, q, v, t, C, dt) {
      const T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const ch = (re, raw) => { const p = (v.e.params || []).find(p => re.test(p.name)), c = p && p.choices; return c ? Math.max(0, c.findIndex(x => raw >= x.from && raw <= x.to)) : 0; };
      const mode = ch(/^mode$/i, q.raw(/^mode$/i, 12)), amt = q(/^amount$/i, 0.25), drive = q(/^drive$/i, 0), phase = q(/^phase$/i, 0);
      const rev = ch(/^order$/i, q.raw(/^order$/i, 0)) === 1, mix = q(R.mix, 0.5);
      const boxes = [["DRIVE", drive], ["PHASE", phase], [["DECIM", "CRUSH", "FM", "RING"][mode], amt]];
      if (rev) boxes.reverse();
      const proc = (name, a, x, u) => {
        if (a < 0.01) return x;
        if (name === "DRIVE") { const gn = 1 + a * 8; return Math.tanh(gn * x) / Math.tanh(gn); }
        if (name === "PHASE") return x * (0.55 + 0.45 * Math.cos(u * (6 + a * 10) - t * (1 + a * 6)));
        if (name === "DECIM") { const st = 0.02 + a * 0.12; const uu = Math.floor(u / st) * st; return Math.sin(uu * TAU * 3 + t * 4) * (Math.abs(x) > 0 ? 1 : 1); }
        if (name === "CRUSH") { const lv = Math.max(1, Math.round(10 - a * 9)); return Math.round(x * lv) / lv; }
        if (name === "FM") return Math.sin(u * TAU * 3 + t * 4 + Math.sin(u * TAU * 1.5 + t * 2) * a * 6);
        return x * Math.sin(u * TAU * (8 + a * 20) + t * 5);
      };
      const bw = 70, gap = 13, X0 = 8, cy = 70, A = 22;
      boxes.forEach(([name, a], k) => {
        const bx = X0 + k * (bw + gap), on = a >= 0.01;
        g.strokeStyle = col(on ? 0.65 : 0.25, T.lo); g.lineWidth = 1; g.strokeRect(bx, cy - 36, bw, 72);
        g.fillStyle = col(on ? 0.85 : 0.35, T.lo); g.font = "bold 8px ui-monospace, monospace"; g.fillText(name, bx + 3, cy - 40);
        if (k < 2) { g.fillStyle = col(0.5, T.lo); g.beginPath(); g.moveTo(bx + bw + 3, cy - 3); g.lineTo(bx + bw + gap - 3, cy); g.lineTo(bx + bw + 3, cy + 3); g.fill(); }
        g.strokeStyle = col(on ? (0.45 + 0.55 * mix) : 0.3, on ? T.hi : T.lo); g.lineWidth = 1.6; g.beginPath();
        for (let i = 0; i <= bw - 6; i++) {
          const u = i / (bw - 6); let x = Math.sin(u * TAU * 3 + t * 4);
          for (let j = 0; j <= k; j++) x = proc(boxes[j][0], boxes[j][1], x, u);
          const y = cy - x * A; i ? g.lineTo(bx + 3 + i, y) : g.moveTo(bx + 3, y);
        }
        g.stroke();
      });
    },

    // Oxide: tape's magnetic hysteresis loop, field across, magnetisation up.
    // The sound drives a point round the loop (Input: how far), leaving a
    // trail; Bias narrows the loop (straighter), Shape sharpens its knee,
    // Tilt leans it; Flutter shakes the trace at FlutSpd. Below, the head
    // bump in the lows (HeadBmp). Output, Mix: brightness.
    oxide(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.tr = []; s.ph = 0; s.env = 0.7; s.next = 0; s.r = rng(v.e.id || 151); }
      const inp = q(/^input$/i, 0.5), tilt = q(R.tilt, 0.5), shape = q(/^shape$/i, 0.5), flut = q(/^flutter$/i, 0.3), fsp = q(/^flutspd$/i, 0.5);
      const bias = q(/^bias$/i, 0.5), bump = q(/^headbmp$/i, 0.25), outp = q(/^output$/i, 0.5), mix = q(R.mix, 0.5);
      if (v.live && t >= s.next) { s.env = 1; s.next = t + 1.2 + s.r() * 1.5; }
      s.env += (0.5 - s.env) * Math.min(1, dt * 0.6);
      const hc = 0.38 * (1 - bias * 0.85), kk = 1.4 + 4 * shape, lean = (tilt - 0.5) * 0.5;
      const B = (H, up) => Math.tanh((H + (up ? -hc : hc)) * kk) / Math.tanh(kk) * 0.85 + lean * H;
      const cx = 128, cy = 54, SX = 70, SY = 40, Amax = 0.3 + 0.9 * inp;
      g.strokeStyle = col(0.2, T.lo); g.lineWidth = 1; g.beginPath(); g.moveTo(cx - SX - 6, cy); g.lineTo(cx + SX + 6, cy); g.moveTo(cx, cy - SY - 6); g.lineTo(cx, cy + SY + 6); g.stroke();
      g.fillStyle = col(0.16 * (0.5 + 0.5 * mix), T.hi); g.beginPath();                   // the loop's area: what the tape holds on to
      for (let i = 0; i <= 80; i++) { const H = -Amax + 2 * Amax * i / 80; g[i ? "lineTo" : "moveTo"](cx + H / 1.2 * SX, cy - B(H, true) * SY); }
      for (let i = 80; i >= 0; i--) { const H = -Amax + 2 * Amax * i / 80; g.lineTo(cx + H / 1.2 * SX, cy - B(H, false) * SY); }
      g.fill();
      for (const up of [true, false]) { g.strokeStyle = col(0.7, T.lo); g.lineWidth = 1.6; g.beginPath();
        for (let i = 0; i <= 80; i++) { const H = -Amax + 2 * Amax * i / 80, x = cx + H / 1.2 * SX, y = cy - B(H, up) * SY; i ? g.lineTo(x, y) : g.moveTo(x, y); } g.stroke(); }
      if (v.live) s.ph += dt * TAU * 0.7 * (1 + flut * 0.15 * Math.sin(t * TAU * (1 + fsp * 8)));
      const H = Math.sin(s.ph) * Amax * s.env, up = Math.cos(s.ph) > 0;
      const jit = flut * 2 * Math.sin(t * TAU * (1 + fsp * 8)), px = cx + H / 1.2 * SX + jit, py = cy - B(H, up) * SY;
      s.tr.push([px, py]); if (s.tr.length > 40) s.tr.shift();
      s.tr.forEach(([x, y], i) => { const k = i / 40, w = 1.5 + 2.5 * k; g.fillStyle = col((0.3 + 0.7 * k) * (0.5 + 0.5 * outp) * (0.6 + 0.4 * mix), T.hi); g.fillRect(x - w / 2, y - w / 2, w, w); });
      g.fillStyle = col(1, T.lo); g.beginPath(); g.arc(px, py, 3, 0, TAU); g.fill();
      // the head bump
      g.strokeStyle = col(0.55, T.hi); g.lineWidth = 1.2; g.beginPath();
      for (let i = 0; i <= 120; i++) { const u = i / 120, y = 118 - bump * 10 * Math.exp(-(((u - 0.12) / 0.06) ** 2)) + (tilt - 0.5) * 8 * (u - 0.5); i ? g.lineTo(68 + i, y) : g.moveTo(68, y); }
      g.stroke();
      g.fillStyle = col(0.5, T.lo); g.font = "bold 7px ui-monospace, monospace"; g.fillText("HEAD BUMP", 6, 121);
    },

    // Rooms: a room in perspective, a sound source in it, rays of sound
    // bouncing off the walls and dimming with every bounce until they die.
    // Time is how long they last, Depth (Room) how big the room is, Freq how
    // much each wall swallows. The Mode changes the room: Digit snaps the
    // rays to a coarse grid, Peak lets only a few angles ring, Gate cuts them
    // all off at once, Wave makes the walls breathe, Gong turns it into a
    // ringing round plate.
    rooms(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const ch = (re, raw) => { const p = (v.e.params || []).find(p => re.test(p.name)), c = p && p.choices; return c ? Math.max(0, c.findIndex(x => raw >= x.from && raw <= x.to)) : 0; };
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 157); s.rays = []; s.next = 0; s.hit = -9; }
      const mode = ch(/^mode$/i, q.raw(/^mode$/i, 0)), time = q(R.time, 0.55), freq = q(/^freq$/i, 0.45), depth = q(R.depth, 0.45), mix = q(R.mix, 0.5);
      const life = 0.3 + 3.5 * time * time, absorb = 0.5 + 0.45 * (1 - freq);
      const sc = mode === 0 ? 0.55 + 0.45 * depth : 0.8, cx = 128, cy = 64, hw = 110 * sc, hh = 52 * sc;
      const wob = mode === 4 ? 1 + 0.08 * depth * Math.sin(t * (1 + freq * 6)) : 1;
      const X0 = cx - hw * wob, X1 = cx + hw * wob, Y0 = cy - hh, Y1 = cy + hh;
      // the room: front frame, back wall, the edges between
      const bk = 0.55;
      g.strokeStyle = col(0.3, T.lo); g.lineWidth = 1;
      if (mode === 5) { g.beginPath(); g.ellipse(cx, cy, hw * 0.95, hh * 1.1, 0, 0, TAU); g.stroke(); g.beginPath(); g.ellipse(cx, cy, hw * 0.95 * bk, hh * 1.1 * bk, 0, 0, TAU); g.stroke(); }
      else {
        g.strokeRect(X0, Y0, X1 - X0, Y1 - Y0); const bx0 = cx - (cx - X0) * bk, by0 = cy - hh * bk, bx1 = cx + (X1 - cx) * bk, by1 = cy + hh * bk;
        g.strokeRect(bx0, by0, bx1 - bx0, by1 - by0);
        g.beginPath(); g.moveTo(X0, Y0); g.lineTo(bx0, by0); g.moveTo(X1, Y0); g.lineTo(bx1, by0); g.moveTo(X0, Y1); g.lineTo(bx0, by1); g.moveTo(X1, Y1); g.lineTo(bx1, by1); g.stroke();
      }
      // a note: a burst of rays from the source
      const sx = cx - hw * 0.35, sy = cy + hh * 0.2;
      if (v.live && t >= s.next) {
        s.next = t + Math.max(1.2, life + 0.4); s.hit = t;
        for (let i = 0; i < 26; i++) { let a = s.r() * TAU; if (mode === 2) a = Math.round(a / (TAU / (3 + Math.round(freq * 5)))) * (TAU / (3 + Math.round(freq * 5))) + 0.2;
          s.rays.push({ x: sx, y: sy, vx: Math.cos(a) * 90, vy: Math.sin(a) * 60, e: 1, pts: [[sx, sy]], t }); }
      }
      const gate = mode === 3 ? 0.15 + 0.85 * freq * life : 1e9;
      for (const r of s.rays) {
        if (v.live) {
          r.x += r.vx * dt; r.y += r.vy * dt;
          let bounce = false;
          if (mode === 5) { const nx = (r.x - cx) / (hw * 0.95), ny = (r.y - cy) / (hh * 1.1); if (nx * nx + ny * ny > 1) { const n = Math.hypot(nx, ny), ux = nx / n, uy = ny / n, d = r.vx * ux + r.vy * uy; r.vx -= 2 * d * ux; r.vy -= 2 * d * uy; r.x -= r.vx * dt; r.y -= r.vy * dt; bounce = true; } }
          else { if (r.x < X0 || r.x > X1) { r.vx = -r.vx; r.x = Math.max(X0, Math.min(X1, r.x)); bounce = true; } if (r.y < Y0 || r.y > Y1) { r.vy = -r.vy; r.y = Math.max(Y0, Math.min(Y1, r.y)); bounce = true; } }
          if (bounce) { r.e *= mode === 5 ? 0.97 : absorb; r.pts.push([r.x, r.y]); if (r.pts.length > 8) r.pts.shift(); }
          r.e *= Math.exp(-dt / life);
        }
        if (t - r.t > gate) r.e = 0;
      }
      s.rays = s.rays.filter(r => r.e > 0.03);
      g.lineWidth = 1.2;
      for (const r of s.rays) {
        g.strokeStyle = col(r.e * (0.45 + 0.55 * mix), T.hi); g.beginPath();
        const P = [...r.pts, [r.x, r.y]].map(([x, y]) => mode === 1 ? [Math.round(x / (4 + depth * 10)) * (4 + depth * 10), Math.round(y / (4 + depth * 10)) * (4 + depth * 10)] : [x, y]);
        P.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.stroke();
        g.fillStyle = col(r.e, T.lo); g.fillRect(P[P.length - 1][0] - 1.2, P[P.length - 1][1] - 1.2, 2.4, 2.4);
      }
      const fl = Math.exp(-(t - s.hit) * 8);
      g.fillStyle = col(0.5 + 0.5 * fl, T.lo); g.beginPath(); g.arc(sx, sy, 2.5 + 2 * fl, 0, TAU); g.fill();
      g.fillStyle = col(0.75, T.lo); g.font = "bold 8px ui-monospace, monospace";
      g.fillText(["ROOM", "DIGIT", "PEAK", "GATE", "WAVE", "GONG"][mode], 6, 11);
    },

    // Breather: the sound as an orb and the reverb as a cloud around it,
    // breathing on the beat at Tempo: Shape is how (Duck drops and comes
    // back, Gate cuts and holds for Curve, Rise grows into the beat), Div how
    // often, Depth how far, Shift how late. Targt says what breathes: the orb
    // (DRY), the cloud (VERB), both, or what feeds the cloud (SEND). Verb is
    // how much cloud, Size how wide. Along the bottom, the level.
    breather(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.cl = 0; s.r = rng(v.e.id || 163); s.pts = Array.from({ length: 90 }, () => ({ a: s.r() * TAU, d: 0.5 + 0.5 * s.r(), p: s.r() * TAU })); }
      const tgt = q.raw(/^targt$/i, 2), shape = q.raw(/^shape$/i, 0), depth = q(R.depth, 0.8), div = q.raw(/^div$/i, 2), shift = q(/^shift$/i, 0);
      const curve = q(/^curve$/i, 0.5), verb = q(/^verb$/i, 0.35), size = q(R.size, 0.5), bpm = Math.max(40, Math.min(240, (q.raw(/^tempo$/i, 160) - 1) % 241 + 1 || 160));
      const divS = [0.25, 0.5, 1, 2, 4][div] * 60 / bpm, cv = 0.05 + 0.95 * curve;
      const env = tt => { const u = (((tt / divS - shift * (60 / bpm) / divS) % 1) + 1) % 1;
        const m = shape === 0 ? (u < cv ? u / cv : 1) : shape === 1 ? (u < cv ? 0 : 1) : (u < 1 - cv ? 0 : (u - (1 - cv)) / cv);
        return shape === 2 ? 1 - depth + depth * (u < 0.02 ? 0 : m) : 1 - depth * (1 - m); };
      const e = env(t), dryE = tgt === 0 || tgt === 2 ? e : 1, verbE = tgt === 1 || tgt === 2 ? e : 1, sendE = tgt === 3 ? e : 1;
      s.cl += (dryE * sendE * verb - s.cl) * Math.min(1, dt / (0.2 + size * 1.2));
      const cx = 128, cy = 58, R0 = 20;
      // the cloud
      const cloud = (tgt === 3 ? s.cl / Math.max(0.01, verb) : verbE) * verb;
      for (const p of s.pts) { const r = (R0 + 8 + p.d * (16 + 40 * size)) * (0.85 + 0.15 * Math.sin(t + p.p)), a = p.a + t * 0.15;
        g.fillStyle = col(cloud * (1 - p.d * 0.5) * 0.9, T.lo); g.fillRect(cx + Math.cos(a) * r * 1.6 - 1.5, cy + Math.sin(a) * r * 0.7 - 1.5, 3, 3); }
      // the orb
      const rr = R0 * (0.35 + 0.65 * dryE);
      g.fillStyle = col(0.35 + 0.65 * dryE, T.hi); g.beginPath(); g.ellipse(cx, cy, rr * 1.3, rr, 0, 0, TAU); g.fill();
      // the level along the bottom, beats marked
      const X0 = 8, X1 = 248, span = divS * 4;
      g.strokeStyle = col(0.75, T.hi); g.lineWidth = 1.4; g.beginPath();
      for (let x = X0; x <= X1; x += 1) { const tt = t - (X1 - x) / (X1 - X0) * span, y = 122 - env(tt) * 14; x === X0 ? g.moveTo(x, y) : g.lineTo(x, y); }
      g.stroke();
      g.fillStyle = col(0.75, T.lo); g.font = "bold 8px ui-monospace, monospace";
      g.fillText(["DRY", "VERB", "BOTH", "SEND"][tgt] + "  " + ["DUCK", "GATE", "RISE"][shape] + "  " + ["1/16", "1/8", "1/4", "1/2", "BAR"][div], 6, 11);
    },

    // Taffy: the tape as a ribbon of sound pulled through the head in the
    // middle, stretched thin when it runs fast, bunched up slow, flowing
    // backwards in reverse, standing still at stop. Speed is where the motor
    // heads, Chance how often it slips, Depth how far a slip goes, Glide how
    // smoothly it gets there. The dial above reads the speed now.
    taffy(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 167); s.sp = 1; s.tgt = 1; s.pos = 0; s.next = 0; s.w = Float32Array.from({ length: 512 }, (_, i) => { const p = i % 64; return (p < 2 ? 1 : Math.exp(-p / 16)) * (0.4 + 0.6 * s.r()); }); }
      const speed = q(/^speed$/i, 0.5), chance = q(/^chance$/i, 0.35), depth = q(R.depth, 0.6), glide = q(/^glide$/i, 0.6), mix = q(R.mix, 0.5);
      const home = speed < 0.25 ? -1 + speed * 4 : speed < 0.5 ? (speed - 0.25) * 4 : 1 + (speed - 0.5) * 2;
      if (v.live && t >= s.next) { s.next = t + 0.3 + (1 - chance) * 3 * s.r(); s.tgt = s.r() < chance ? home + (s.r() * 3 - 1.5) * depth : home; }
      if (v.live) { s.tgt += (home - s.tgt) * Math.min(1, dt * 0.6); s.sp += (s.tgt - s.sp) * Math.min(1, dt / (0.02 + glide * 0.8)); s.pos += s.sp * dt * 60; }
      const cy = 82, X0 = 6, X1 = 250, cx = 128;
      for (let x = X0; x < X1; x += 1.5) {
        const d = (x - cx) / (X1 - cx), str = Math.max(0.15, Math.min(2.5, Math.abs(s.sp)));
        const thin = 1 / (1 + Math.abs(d) * (str - 0.5) * 1.5);
        const a = s.w[(((Math.floor((x - cx) / str + s.pos)) % 512) + 512) % 512];
        const h = a * 26 * thin * (0.6 + 0.4 * (1 - Math.abs(d)));
        g.fillStyle = col((0.35 + 0.55 * mix) * (1 - Math.abs(d) * 0.5), s.sp < 0 ? T.lo : T.hi); g.fillRect(x, cy - h, 1.2, h * 2);
      }
      g.fillStyle = col(0.9, T.lo); g.fillRect(cx - 1, cy - 34, 2, 68);
      // the dial: -1x .. 2x
      const dx = 128, dy = 40, dr = 26, ang = u => Math.PI + clamp((u + 1) / 3) * Math.PI;
      g.strokeStyle = col(0.4, T.lo); g.lineWidth = 1; g.beginPath(); g.arc(dx, dy, dr, Math.PI, TAU); g.stroke();
      for (const [u, lab] of [[-1, "-1"], [0, "0"], [1, "1"], [2, "2"]]) { const a = ang(u); g.fillStyle = col(0.6, T.lo); g.fillRect(dx + Math.cos(a) * dr - 0.6, dy + Math.sin(a) * dr - 0.6, 1.2, 1.2); g.font = "bold 7px ui-monospace, monospace"; g.fillText(lab, dx + Math.cos(a) * (dr + 7) - 3, dy + Math.sin(a) * (dr + 7) + 3); }
      const a = ang(s.sp); g.strokeStyle = col(1, T.hi); g.lineWidth = 2; g.beginPath(); g.moveTo(dx, dy); g.lineTo(dx + Math.cos(a) * (dr - 3), dy + Math.sin(a) * (dr - 3)); g.stroke();
      g.fillStyle = col(0.8, T.lo); g.font = "bold 8px ui-monospace, monospace"; g.fillText((s.sp >= 0 ? "" : "") + s.sp.toFixed(2) + "x", 6, 11);
    },

    // GenLoss: a worn video tape on a screen. The sound as a band of light
    // across the scanlines that wavers sideways with Wow, smears soft as
    // Tone goes dark, with Hiss as snow and a tracking band rolling through.
    // Mix: how bright.
    genloss(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C);
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 173); s.w = Float32Array.from({ length: 256 }, (_, i) => { const p = i % 40; return (p < 2 ? 1 : Math.exp(-p / 12)) * (0.4 + 0.6 * s.r()); }); }
      const wow = q(/^wow$/i, 0.72), tone = q(R.tilt, 0.52), hiss = q(R.noise, 0.02), mix = q(R.mix, 0.5);
      const roll = ((t * 18) % 160) - 16, blur = Math.round((1 - tone) * 5);
      for (let y = 0; y < 128; y += 2) {
        const sh = Math.sin(y * 0.05 + t * 1.3) * wow * 6 + Math.sin(y * 0.4 + t * 9) * wow * 1.2 + (Math.abs(y - roll) < 8 ? (s.r() - 0.5) * 14 : 0);
        const amp = 1 - Math.abs(y - 64) / 64;
        for (let x = 0; x < 256; x += 2) {
          let a = 0; for (let k = -blur; k <= blur; k++) a += s.w[(((x + k * 2 + sh + t * 40) >> 1) % 256 + 256) % 256]; a /= 2 * blur + 1;
          let lum = a > (1 - amp) * 0.9 ? (0.3 + 0.7 * a) * (0.45 + 0.55 * mix) : 0;
          if (hiss > 0.005 && s.r() < hiss * 0.5) lum = Math.max(lum, s.r() * 0.8);
          if (Math.abs(y - roll) < 3) lum = Math.max(lum, 0.25);
          if (lum < 0.05) continue;
          g.fillStyle = css(lerp3(T.bg, lum > 0.6 ? T.lo : T.hi, Math.min(1, lum * 1.3))); g.fillRect(x, y, 2, 1.4);
        }
      }
    },

    // Klang: the sound as a spectrum (pale lines, its harmonics), the
    // carrier (Freq) as the bright marker, and what the ring modulator makes:
    // every partial reappears on both sides of the carrier, sum and
    // difference, in the colour. Sweep swings the carrier back and forth,
    // and the sidebands with it. Mix: how bright.
    klang(g, q, v, t, C, dt) {
      const T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const freq = q(/^freq$/i, 0.4), sweep = q(/^sweep$/i, 0.3), mix = q(R.mix, 0.5);
      const X0 = 8, X1 = 248, base = 112, lo = Math.log(20), hi = Math.log(8000);
      const xOf = f => X0 + (Math.log(Math.max(20, f)) - lo) / (hi - lo) * (X1 - X0);
      const car = Math.max(1, (0.5 + 1999.5 * freq) * (1 + sweep * 0.6 * Math.sin(t * TAU * sweep * 12 * 0.3)));
      g.fillStyle = col(0.2, T.lo); g.fillRect(X0, base, X1 - X0, 1);
      for (const f of [100, 1000]) { g.fillRect(xOf(f) - 0.5, base, 1, 4); }
      g.font = "bold 7px ui-monospace, monospace"; g.fillText("100", xOf(100) - 6, base + 12); g.fillText("1k", xOf(1000) - 4, base + 12);
      const f0 = 147;
      for (let h = 1; h <= 8; h++) {
        const f = f0 * h, a = 1 / h ** 0.7;
        g.fillStyle = col(0.45, T.lo); g.fillRect(xOf(f) - 0.6, base - a * 30, 1.2, a * 30);
        for (const sb of [car + f, Math.abs(car - f)]) { if (sb < 20) continue; const x = xOf(sb);
          g.fillStyle = col((0.45 + 0.55 * mix) * a, T.hi); g.fillRect(x - 1.2, base - a * 70, 2.4, a * 70); }
      }
      g.fillStyle = col(1, T.lo); const cx = xOf(car); g.fillRect(cx - 0.8, 18, 1.6, base - 18);
      g.beginPath(); g.moveTo(cx - 4, 14); g.lineTo(cx + 4, 14); g.lineTo(cx, 20); g.fill();
      g.fillStyle = col(0.8, T.lo); g.font = "bold 8px ui-monospace, monospace"; g.fillText(Math.round(car) + " HZ", 6, 11);
    },

    // Sweep: a waterfall. Each row is the effect's response at one moment,
    // the newest at the bottom, frequency across, so the LFO's sweep draws
    // itself as it goes: phaser notches (PH 4, PH 8), a flanger's comb (FL +,
    // FL -), or a filter's band (LP, BP, HP, NTCH). Rate how fast, Depth how
    // far, Cntr where, Reso how sharp, Shape the LFO's shape.
    sweep(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C);
      const type = q.raw(/^type$/i, 0), rraw = q.raw(R.rate, 109), depth = q(R.depth, 0.7), cntr = q(R.cut, 0.5), reso = q(R.reso, 0.4), shp = q.raw(/^shape$/i, 0), mix = q(R.mix, 1);
      const bpm = Math.max(40, Math.min(240, (q.raw(/^tempo$/i, 120) - 1) % 241 + 1 || 120));
      const hz = rraw <= 100 ? 0.05 * Math.pow(160, rraw / 100) : 1 / ([32, 28, 24, 20, 16, 12, 8, 6, 4, 3, 2, 1, 0.5, 0.25, 0.125][Math.min(14, rraw - 101)] * 60 / bpm);
      if (!s.rows) { s.rows = []; s.acc = 0; s.ph = 0; s.r = rng(v.e.id || 179); s.rv = 0; s.rt = 0; }
      const lfo = ph => { const u = ph % 1; return [Math.abs(u * 2 - 1) * 2 - 1, Math.sin(u * TAU), u * 2 - 1, 1 - u * 2, u < 0.5 ? 1 : -1, s.rv][shp] ?? 0; };
      const N = 64;
      if (v.live) { s.ph += dt * hz; if (shp === 5 && s.ph * 2 - s.rt >= 1) { s.rt = Math.floor(s.ph * 2); s.rv = s.r() * 2 - 1; }
        s.acc += dt; while (s.acc > 0.04) { s.acc -= 0.04;
          const c = cntr + lfo(s.ph) * depth * 0.35, row = new Float32Array(N);
          for (let i = 0; i < N; i++) { const u = i / (N - 1), d = u - c; let r;
            if (type <= 1) { const n = type ? 4 : 2; r = 1; for (let k = 0; k < n; k++) { const dd = u - (c + (k - (n - 1) / 2) * 0.12); r *= 1 - (0.75 + 0.25 * reso) * Math.exp(-(dd * dd) / 0.0006); } }
            else if (type <= 3) { const sp = 0.04 + 0.2 * (1 - c); r = 0.5 + 0.5 * (type === 2 ? 1 : -1) * Math.cos(u / sp * TAU) * (0.4 + 0.6 * reso); }
            else if (type === 4) r = (1 / (1 + Math.exp((d) * 30))) + reso * 0.8 * Math.exp(-(d * d) / 0.001);
            else if (type === 5) r = Math.exp(-(d * d) / (0.004 + 0.02 * (1 - reso)));
            else if (type === 6) r = (1 / (1 + Math.exp(-(d) * 30))) + reso * 0.8 * Math.exp(-(d * d) / 0.001);
            else r = 1 - Math.exp(-(d * d) / (0.001 + 0.004 * (1 - reso)));
            row[i] = Math.min(1.8, Math.max(0, r)); }
          s.rows.push(row); if (s.rows.length > 30) s.rows.shift(); } }
      const cw = 256 / N, rh = 128 / 30;
      s.rows.forEach((row, j) => { const age = (s.rows.length - 1 - j) / 30;
        for (let i = 0; i < N; i++) { const a = Math.min(1, row[i]) * (1 - age * 0.6) * (0.45 + 0.55 * mix); if (a < 0.06) continue;
          g.fillStyle = css(row[i] > 1.08 ? lerp3(T.bg, T.lo, a) : lerp3(T.bg, T.hi, a * 0.62)); g.fillRect(i * cw, 128 - (s.rows.length - j) * rh, cw - 0.4, rh - 0.4); } });
      g.fillStyle = css(T.lo); g.font = "bold 8px ui-monospace, monospace";
      g.fillText(["PH 4", "PH 8", "FL +", "FL -", "LP", "BP", "HP", "NTCH"][type] + "  " + ["TRI", "SINE", "RISE", "FALL", "SQR", "RAND"][shp], 6, 11);
    },

    // SyncEQ: a level meter of LED segments, a column per band, a little
    // music running through it, shaped by the EQ (the bright curve: LoCut,
    // the Low shelf, the Mid bell at MidF, the High shelf, HiCut) with peak
    // dots; Drive lights the tops hot. Top right, the bar lamp: it blinks on
    // each bar it passes on at Tempo, and the beat dots count to four.
    synceq(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 181); s.lv = new Float32Array(24); s.pk = new Float32Array(24); }
      const lc = q.raw(/^locut$/i, 0), low = (q.raw(/^low$/i, 12) - 12) / 12, mid = (q.raw(/^mid$/i, 12) - 12) / 12, mf = q(/^midf$/i, 0.5);
      const high = (q.raw(/^high$/i, 12) - 12) / 12, hc = q.raw(/^hicut$/i, 50), drive = q(R.drive, 0), level = (q.raw(/^level$/i, 12) - 12) / 12;
      const bpm = Math.max(40, Math.min(240, (q.raw(/^tempo$/i, 120) - 1) % 241 + 1 || 120));
      const NB = 24, eq = u => { let gdb = low * 12 / (1 + Math.exp((u - 0.18) * 25)) + high * 12 / (1 + Math.exp(-(u - 0.85) * 25)) + mid * 12 * Math.exp(-(((u - (0.25 + 0.55 * mf)) / 0.07) ** 2)) + level * 12;
        if (lc > 0) gdb -= 24 * Math.max(0, (0.05 + lc / 50 * 0.25) - u) / 0.1; if (hc < 50) gdb -= 24 * Math.max(0, u - (0.6 + hc / 50 * 0.4)) / 0.1; return gdb; };
      const beat = 60 / bpm, b = Math.floor(t / beat), bf = t / beat - b;
      if (v.live) for (let i = 0; i < NB; i++) { const u = i / (NB - 1), music = (0.55 - u * 0.25) * (0.7 + 0.3 * s.r()) * (bf < 0.15 && i < 6 ? 1.3 : 1);
        const tgt = clamp(music * Math.pow(10, eq(u) / 40) * (1 + drive * 0.3)); s.lv[i] += (tgt - s.lv[i]) * 0.3; s.pk[i] = Math.max(s.pk[i] - dt * 0.4, s.lv[i]); }
      const X0 = 10, W = 236, cw = W / NB, SEG = 14, Y1 = 118, sh = 6;
      for (let i = 0; i < NB; i++) { const n = Math.round(s.lv[i] * SEG);
        for (let k = 0; k < SEG; k++) { const lit = k < n, hot = k >= SEG - 3 && drive > 0.05;
          g.fillStyle = lit ? col(hot ? 1 : 0.55 + 0.4 * (k / SEG), hot ? T.lo : T.hi) : col(0.08, T.lo); g.fillRect(X0 + i * cw + 1, Y1 - (k + 1) * sh, cw - 2, sh - 1.5); }
        g.fillStyle = col(0.9, T.lo); g.fillRect(X0 + i * cw + 1, Y1 - Math.round(s.pk[i] * SEG) * sh - 2, cw - 2, 1.5); }
      g.strokeStyle = col(0.9, T.lo); g.lineWidth = 1.5; g.beginPath();
      for (let x = 0; x <= W; x += 2) { const y = 76 - eq(x / W) * 2.2; x ? g.lineTo(X0 + x, y) : g.moveTo(X0, y); } g.stroke();
      const lamp = b % 4 === 0 && bf < 0.25;
      g.fillStyle = col(lamp ? 1 : 0.15, T.hi); g.beginPath(); g.arc(238, 10, 4, 0, TAU); g.fill();
      for (let k = 0; k < 4; k++) { g.fillStyle = col(k === b % 4 ? 0.9 : 0.2, T.lo); g.fillRect(200 + k * 7, 8, 4, 4); }
      g.fillStyle = col(0.75, T.lo); g.font = "bold 8px ui-monospace, monospace"; g.fillText(bpm + " BPM", 6, 11);
    },

    // Howl: the resonator as its two poles on the unit circle (a moon): Tune
    // turns them round (higher, further from the right), Annihil pulls them
    // out toward the rim, where the ring gets longer, until it howls. Each
    // note rings it; the ring is drawn running out to the right.
    howl(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.hit = 0; s.next = 0; s.r = rng(v.e.id || 191); }
      const tune = q(/^tune$/i, 0.17), ann = q(/^annihil$/i, 0.34), mix = q(R.mix, 0.5);
      if (v.live && t >= s.next) { s.hit = t; s.next = t + 1.4 + s.r() * 1.5; }
      const th = 0.15 + tune * 2.6, rad = 0.6 + 0.395 * Math.sqrt(ann), cx = 62, cy = 64, R0 = 44;
      g.strokeStyle = col(0.45, T.lo); g.lineWidth = 1.2; g.beginPath(); g.arc(cx, cy, R0, 0, TAU); g.stroke();
      g.fillStyle = col(0.06, T.lo); g.beginPath(); g.arc(cx, cy, R0, 0, TAU); g.fill();
      g.strokeStyle = col(0.15, T.lo); g.lineWidth = 1; g.beginPath(); g.moveTo(cx - R0 - 6, cy); g.lineTo(cx + R0 + 6, cy); g.moveTo(cx, cy - R0 - 6); g.lineTo(cx, cy + R0 + 6); g.stroke();
      const age = t - s.hit, decay = -Math.log(rad) * 18, env = Math.exp(-age * decay * 0.6), glow = 0.4 + 0.6 * env;
      for (const sg of [1, -1]) { const px = cx + Math.cos(th) * rad * R0, py = cy - sg * Math.sin(th) * rad * R0;
        g.strokeStyle = col(glow, T.hi); g.lineWidth = 2; g.beginPath(); g.moveTo(px - 4, py - 4); g.lineTo(px + 4, py + 4); g.moveTo(px + 4, py - 4); g.lineTo(px - 4, py + 4); g.stroke();
        if (env > 0.05) { g.strokeStyle = col(env * 0.5, T.hi); g.lineWidth = 1; g.beginPath(); g.arc(px, py, 6 + (1 - env) * 10, 0, TAU); g.stroke(); } }
      g.fillStyle = col(0.6, T.lo); g.beginPath(); g.arc(cx, cy, 2.5, 0, TAU); g.fill();
      // the ring, out to the right
      g.strokeStyle = col(0.45 + 0.55 * mix, T.hi); g.lineWidth = 1.4; g.beginPath();
      for (let x = 0; x <= 140; x++) { const tt = Math.max(0, age - (140 - x) * 0.01), e = Math.exp(-tt * decay * 0.6) * (age - (140 - x) * 0.01 >= 0 ? 1 : 0);
        const y = cy - Math.sin(tt * th * 60) * e * 34; x ? g.lineTo(112 + x, y) : g.moveTo(112, y); }
      g.stroke();
      g.fillStyle = col(0.75, T.lo); g.font = "bold 8px ui-monospace, monospace"; g.fillText(rad > 0.985 ? "HOWL" : "RING", 6, 11);
    },

    // Arrakis: the two detuned voices beat against each other, and their
    // beating is drawn as dunes rolling past: Detune is how close the crests
    // come (faster beats, tighter dunes); behind, the slow big dune of the
    // sub-octave. Mix: how bright the sand.
    arrakis(g, q, v, t, C, dt) {
      const T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const det = q(/^detune$/i, 0.63), mix = q(R.mix, 0.5), sp = t * 18;
      const beat = 0.012 + det * 0.06;
      const layers = [[0.004, 40, 46, 0.25, T.lo, 0.4], [beat, 26, 76, 0.55, T.hi, 1], [beat * 1.7, 16, 100, 0.85, T.hi, 1.6]];
      for (const [f, A, base, a, c, spd] of layers) {
        g.fillStyle = col(a * (0.5 + 0.5 * mix), c); g.beginPath(); g.moveTo(0, 128);
        for (let x = 0; x <= 256; x += 2) { const u = x + sp * spd, y = base - Math.abs(Math.sin(u * f * Math.PI)) ** 1.6 * A - Math.sin(u * 0.05) * 2; g.lineTo(x, y); }
        g.lineTo(256, 128); g.closePath(); g.fill();
      }
      // grains blowing off the crests
      for (let i = 0; i < 40; i++) { const x = ((i * 53 + t * 60 * (1 + i % 3)) % 280) - 12, y = 60 + ((i * 37) % 50) - Math.sin(x * 0.03 + i) * 6;
        g.fillStyle = col(0.5, T.lo); g.fillRect(x, y, 1.2, 1.2); }
      g.fillStyle = col(0.8, T.lo); g.beginPath(); g.arc(214, 22, 7, 0, TAU); g.fill();
    },

    // DualShft: a piano roll. The note you play (pale, at 0), and the two
    // voices on their own pitch lanes (Ptch1, Ptch2), each starting its echo
    // time later (Dly1, Dly2), bent in opposite directions by the shared
    // LFO (Depth, Shape, Div at Tempo). Keys on the left.
    dualshft(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 193); s.notes = []; s.next = 0; s.rnd = 0; s.rt = -1; }
      const semi = r => { const d = r - 33; return Math.abs(d) <= 10 ? d * 0.1 : Math.sign(d) * (1 + (Math.abs(d) - 10)); };
      const p1 = semi(q.raw(/^ptch1$/i, 49)), p2 = semi(q.raw(/^ptch2$/i, 19));
      const bpm = Math.max(40, Math.min(240, (q.raw(/^tempo$/i, 120) - 1) % 241 + 1 || 120)), beat = 60 / bpm;
      const dl = r => r <= 100 ? 0.012 + 0.988 * (r / 100) ** 2 : [1 / 8, 1 / 6, 1 / 4, 1 / 3, 3 / 8, 1 / 2, 2 / 3, 3 / 4, 1, 1.5, 2, 4][Math.min(11, r - 101)] * beat;
      const d1 = dl(q.raw(/^dly1$/i, 70)), d2 = dl(q.raw(/^dly2$/i, 50));
      const cyc = [16, 8, 6, 4, 3, 2, 1.5, 1, 0.75, 2 / 3, 0.5, 1 / 3, 0.25, 1 / 6, 0.125, 1 / 12, 1 / 16][Math.min(16, q.raw(/^div$/i, 8))] * beat;
      const depth = q.raw(/^depth$/i, 29) / 80 * 12, shp = q.raw(/^shape$/i, 0), mix = q(R.mix, 0.5);
      const lfo = tt => { const u = ((tt / cyc) % 1 + 1) % 1, k = Math.floor(tt / cyc);
        const rr = x => { const a = Math.sin((k + x) * 12.9898) * 43758.5453; return (a - Math.floor(a)) * 2 - 1; };
        return [Math.abs(u * 2 - 1) * 2 - 1, u < 0.5 ? 1 : -1, rr(0), Math.round(rr(0) * 3) / 3, Math.sin(u * TAU), u * 2 - 1, 1 - u * 2][shp] ?? 0; };
      if (v.live && t >= s.next) { s.notes.push({ t, len: 0.25 + s.r() * 0.5, p: [0, 3, 5, 7, -2][(s.r() * 5) | 0] }); s.next = t + 0.6 + s.r() * 0.6; if (s.notes.length > 8) s.notes.shift(); }
      const X0 = 22, X1 = 252, span = 3, cy = 64, ps = 2.1;
      const yOf = semis => cy - semis * ps;
      for (let k = -26; k <= 26; k++) { const y = yOf(k), black = [1, 3, 6, 8, 10].includes(((k % 12) + 12) % 12);
        g.fillStyle = col(black ? 0.12 : 0.5, T.lo); g.fillRect(4, y - ps / 2, black ? 10 : 15, ps - 0.4);
        if (((k % 12) + 12) % 12 === 0) { g.fillStyle = col(0.12, T.lo); g.fillRect(X0, y - 0.3, X1 - X0, 0.6); } }
      const xOf = tt => X1 - (t - tt) / span * (X1 - X0);
      const bar = (t0, len, semis, c, a, bend) => { for (let tt = t0; tt < t0 + len; tt += 0.02) { const x = xOf(tt); if (x < X0 || tt > t) continue;
        g.fillStyle = col(a, c); g.fillRect(x, yOf(semis + (bend ? bend * lfo(tt) * depth : 0)) - ps * 0.8, (X1 - X0) / span * 0.02 + 0.5, ps * 1.6); } };
      for (const n of s.notes) { bar(n.t, n.len, n.p, T.lo, 0.7, 0); bar(n.t + d1, n.len, n.p + p1, T.hi, 0.5 + 0.5 * mix, 1); bar(n.t + d2, n.len, n.p + p2, T.hi, 0.35 + 0.4 * mix, -1); }
      g.fillStyle = col(0.75, T.lo); g.font = "bold 8px ui-monospace, monospace";
      const fm = x => (x > 0 ? "+" : "") + (Math.abs(x) < 1 ? x.toFixed(1) : Math.round(x));
      g.fillText(fm(p1) + " / " + fm(p2), X0, 11);
    },

    // Microlm: a loom. The sound's grains are threads woven across the warp;
    // every pass round the cloud (Regen) weaves them again a step higher or
    // lower (Pitch: an octave down at 0, unison at a third, an octave up at
    // the top), so the cloth climbs or sinks; Tone dims the highest threads.
    microlm(g, q, v, t, C, dt) {
      const T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const pitch = q(/^pitch$/i, 1), regen = q(/^regen$/i, 0.72), tone = q(R.tilt, 0.55), mix = q(R.mix, 0.5);
      const semis = pitch < 0.33 ? -12 + pitch / 0.33 * 12 : (pitch - 0.33) / 0.67 * 12, rise = semis / 12;
      for (let x = 8; x < 250; x += 8) { g.fillStyle = col(0.18, T.lo); g.fillRect(x, 8, 1, 112); }       // the warp
      const passes = 1 + Math.round(regen * 9);
      for (let k = passes - 1; k >= 0; k--) {                 // each pass a thread, over and under the warp
        const a = Math.pow(0.84, k) * (0.5 + 0.5 * mix), y0 = 112 - k * 10;
        let px = null, py = 0;
        for (let x = 4; x <= 252; x += 2) {
          const y = y0 - (x / 248) * rise * 22 + Math.sin(x * 0.06 + t * 2 + k) * 2.2;
          if (px !== null) { const over = (Math.floor(x / 8) + k) % 2 === 0, dim = y < 50 ? 0.35 + 0.65 * tone : 1;
            g.strokeStyle = col(a * dim * (over ? 1 : 0.35), k ? T.hi : T.lo); g.lineWidth = over ? 2.4 : 1.2;
            g.beginPath(); g.moveTo(px, py); g.lineTo(x, y); g.stroke(); }
          px = x; py = y;
        }
        const sx = ((t * 40 + k * 37) % 260) - 4, sy = y0 - (sx / 248) * rise * 22;   // the shuttle on the newest
        if (k === 0) { g.fillStyle = col(0.95, T.lo); g.fillRect(sx - 4, sy - 2, 8, 4); }
      }
      g.fillStyle = col(0.75, T.lo); g.font = "bold 8px ui-monospace, monospace"; g.fillText((semis >= 0 ? "+" : "") + Math.round(semis) + " ST", 6, 11);
    },

    // Choral: the choir itself. Singers in a row, sized by section (giants
    // big, kids small; Choir), as many as Size, standing higher for higher
    // notes of the Chord, their mouths shaped by what they Sing (aah, eh,
    // ee, oh, oo, mm), moving with Pace and quivering with vibrato (Feel:
    // soft wide, punchy tight). Mix: how bright.
    choral(g, q, v, t, C, dt) {
      const T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      const choir = q.raw(/^choir$/i, 4), size = q.raw(R.size, 2), chord = q.raw(/^chord$/i, 0), sing = q.raw(/^sing$/i, 6);
      const pace = q.raw(/^pace$/i, 4), feel = q(/^feel$/i, 0.4), mix = q(R.mix, 0.7), bpm = Math.max(40, Math.min(240, (q.raw(/^tempo$/i, 120) - 1) % 241 + 1 || 120));
      const SECT = [[0], [1], [2], [3], [0, 1], [0, 2], [1, 2], [3, 0], [3, 1], [3, 2], [0, 1, 2], [3, 0, 1], [3, 0, 2], [3, 1, 2], [3, 0, 1, 2], [3, 0], [0, 1]][Math.min(16, choir)];
      const n = Math.min(6, size + 1), cycS = [16, 8, 6, 4, 3, 2, 1.5, 1, 0.75, 2 / 3, 0.5, 1 / 3, 0.25, 1 / 6, 0.125, 1 / 12, 1 / 16][Math.min(16, pace)] * 60 / bpm;
      const CH = [[0], [0, 1], [0, 2], [0, 3], [0, 4], [0, 5], [0, 6], [0, 7], [0, 8], [0, 9], [0, 10], [0, 11], [0, 12], [0, 4, 7, 12], [0, 3, 7, 12], [0, 2, 7, 12], [0, 5, 7, 12], [0, 3, 6, 12], [0, 4, 8, 12], [0, 4, 7, 11], [0, 3, 7, 10], [0, 4, 7, 10], [0, 4, 7, 14], [0, 7, 16], [0, 0]][Math.min(24, chord)];
      const ph = (t / cycS) % 1, open0 = [1, 0.7, 0.45, 0.8, 0.5, 0.05][Math.min(5, sing)] ?? 0.6;
      const vib = (1 - feel) * 1.4;
      g.save(); g.translate(128, 124); g.scale(1.35, 1.35); g.translate(-128, -124);
      for (let i = 0; i < n; i++) {
        const sec = SECT[i % SECT.length], semis = (i % 2 ? CH[Math.min(CH.length - 1, (i + 1) >> 1)] : CH[Math.min(CH.length - 1, i >> 1)]) || 0;
        const sc = [1, 0.85, 0.65, 1.3][sec], x = 128 + (i - (n - 1) / 2) * (160 / Math.max(3, n)), base = 104 - semis * 2.2 - [0, 6, 12, -6][sec];
        let open = open0;
        if (sing === 6) open = 0.2 + 0.8 * Math.abs(Math.sin(ph * Math.PI));
        if (sing === 7) open = [1, 0.7, 0.45, 0.8, 0.5][Math.floor(ph * 5)];
        if (sing === 10) open = 0.3 + 0.7 * Math.sin(ph * Math.PI);
        if (sing === 11 && ph * SECT.length < (sec % SECT.length)) open = 0.05;
        if (sing === 8 || sing === 9) open = 0.2 + 0.8 * Math.min(1, ph * 4);
        const wob = Math.sin(t * 6 + i) * vib, hr = 11 * sc, cyH = base - 22 * sc + wob;
        g.fillStyle = col(0.35, T.lo); g.fillRect(x - 8 * sc, base - 10 * sc, 16 * sc, 12 * sc + 20);      // robe
        g.fillStyle = col(0.45 + 0.5 * mix, T.hi); g.beginPath(); g.arc(x, cyH, hr, 0, TAU); g.fill();    // head
        g.fillStyle = C.bg; g.fillRect(x - 4 * sc, cyH - 3 * sc, 2 * sc, 2 * sc); g.fillRect(x + 2 * sc, cyH - 3 * sc, 2 * sc, 2 * sc);
        const mw = (sing === 2 ? 6 : sing === 4 || sing === 3 ? 3 : 4.5) * sc, mh = Math.max(0.8, 6 * open * sc);
        g.beginPath(); g.ellipse(x, cyH + 4 * sc, mw, mh / 2, 0, 0, TAU); g.fill();
      }
      g.restore();
      g.fillStyle = col(0.75, T.lo); g.font = "bold 8px ui-monospace, monospace";
      g.fillText(["MEN", "WOMEN", "KIDS", "GIANT", "M+W", "M+KID", "W+KID", "G+M", "G+W", "G+KID", "M+W+K", "G+M+W", "G+M+K", "G+W+K", "ALL", "MONKS", "GOSPL"][Math.min(16, choir)] + "  " + ["AAH", "EHH", "EEE", "OHH", "OOH", "MMM", "OOAH", "VOWL", "LA", "DOO", "SWELL", "CANON"][Math.min(11, sing)], 6, 11);
    },

    // Dissolve: the sound as a row of chunks going into the delay; some are
    // dropped before they get there (Chance: gone), and every chunk crumbles
    // into dust that drifts up and away, further and softer as Smear goes up.
    // Glitch flips some chunks backwards (marked) or drops them an octave
    // (wider, lower). Mix: how bright.
    dissolve(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 197); s.ch = []; s.dust = []; s.next = 0; s.k = 0; }
      const chance = q(/^chance$/i, 0.4), smear = q(/^smear$/i, 0.55), glitch = q(/^glitch$/i, 0.35), mix = q(R.mix, 0.5);
      const cw = 18, cy = 92;
      if (v.live && t >= s.next) { s.next = t + 0.22; s.k++; s.ch.push({ x: 256, a: 0.5 + 0.5 * s.r(), drop: s.r() < chance, rev: s.r() < glitch * 0.5, oct: s.r() < glitch * 0.5 }); }
      if (v.live) { for (const c of s.ch) c.x -= dt * 80; s.ch = s.ch.filter(c => c.x > -40); }
      for (const c of s.ch) {
        if (c.drop) { g.strokeStyle = col(0.25, T.lo); g.lineWidth = 1; g.setLineDash([2, 2]); g.strokeRect(c.x, cy - 12, cw - 2, 24); g.setLineDash([]); continue; }
        const w = c.oct ? cw * 1.6 : cw - 2, h = (c.oct ? 14 : 24) * c.a, crumble = clamp((200 - c.x) / 200);
        g.fillStyle = col((0.45 + 0.55 * mix) * (1 - crumble * 0.7), T.hi);
        for (let yy = 0; yy < h; yy += 2) { const keep = (yy / h) > crumble * 0.9 || ((yy * 7 + c.x | 0) % 5) > crumble * 5;
          if (keep) g.fillRect(c.x, cy + (c.oct ? 6 : 0) - h / 2 + yy, w, 1.6); }
        if (c.rev) { g.fillStyle = col(0.9, T.lo); g.beginPath(); g.moveTo(c.x + 2, cy - h / 2 - 5); g.lineTo(c.x + 7, cy - h / 2 - 8); g.lineTo(c.x + 7, cy - h / 2 - 2); g.fill(); }
        if (v.live && s.r() < crumble * (0.4 + smear) * 0.8) s.dust.push({ x: c.x + s.r() * w, y: cy - h / 2 + s.r() * h, vx: -10 - s.r() * 30 * smear, vy: -8 - s.r() * 40 * smear, life: 0.6 + 2.5 * smear, t });
      }
      if (s.dust.length > 400) s.dust.splice(0, s.dust.length - 400);
      for (const d of s.dust) { const age = t - d.t; if (age > d.life) continue; if (v.live) { d.x += d.vx * dt; d.y += d.vy * dt; }
        const sz = 1 + smear * 1.5 * (age / d.life); g.fillStyle = col((1 - age / d.life) * 0.7, T.lo); g.fillRect(d.x, d.y, sz, sz); }
      s.dust = s.dust.filter(d => t - d.t < d.life);
    },

    // Shatter: the sound as a pane of glass, slice by slice; each slice
    // breaks with Chance into shards that jolt out of place, stutter (the
    // same shard repeated) or play backwards (flipped, marked). Mix: bright.
    shatter(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 199); s.w = Float32Array.from({ length: 64 }, (_, i) => (i % 16 < 1 ? 1 : Math.exp(-(i % 16) / 6)) * (0.4 + 0.6 * s.r())); s.sl = []; s.k = -1; }
      const chance = q(/^chance$/i, 0.55), mix = q(R.mix, 0.5), N = 8, sw = 30, X0 = 8, cy = 66;
      const k = Math.floor(t / 0.5);
      if (k !== s.k) { s.k = k; s.sl = Array.from({ length: N }, () => { const br = s.r() < chance; return { br, kind: br ? (s.r() < 0.5 ? 0 : 1) : -1, dx: (s.r() - 0.5) * 6, dy: (s.r() - 0.5) * 10, rot: (s.r() - 0.5) * 0.25, cut: 0.3 + 0.4 * s.r() }; }); }
      const age = t - k * 0.5, jolt = Math.exp(-age * 10);
      for (let i = 0; i < N; i++) {
        const sl = s.sl[i] || { br: false }, x0 = X0 + i * sw;
        const draw = (ox, oy, rot, flip, a) => { g.save(); g.translate(x0 + sw / 2 + ox, cy + oy); g.rotate(rot); g.fillStyle = col(a, sl.br ? T.hi : T.lo);
          for (let j = 0; j < sw - 2; j++) { const u = flip ? sw - 3 - j : j, h = s.w[(i * 8 + (u >> 2)) & 63] * 30; g.fillRect(j - sw / 2, -h, 1, h * 2); } g.restore(); };
        if (!sl.br) { draw(0, 0, 0, false, 0.45 + 0.3 * mix); continue; }
        if (sl.kind === 0) { for (let r = 0; r < 3; r++) draw(sl.dx * jolt + r * 1.5, sl.dy * jolt - r * 1.5, sl.rot * jolt, false, (0.9 - r * 0.25) * (0.5 + 0.5 * mix)); }
        else { draw(sl.dx * jolt, sl.dy * jolt, sl.rot * jolt, true, 0.5 + 0.5 * mix);
          g.fillStyle = col(0.9, T.lo); g.beginPath(); g.moveTo(x0 + 3, cy - 38); g.lineTo(x0 + 9, cy - 41); g.lineTo(x0 + 9, cy - 35); g.fill(); }
        g.strokeStyle = col(0.85, T.lo); g.lineWidth = 1; g.beginPath();
        g.moveTo(x0 + sw * sl.cut, cy - 34); g.lineTo(x0 + sw * 0.5, cy); g.lineTo(x0 + sw * (1 - sl.cut), cy + 34); g.moveTo(x0 + sw * 0.5, cy); g.lineTo(x0 + sw - 2, cy - 8); g.stroke();
      }
    },

    // S.GN_L: a broken line, packet by packet, from the mast on the left to
    // the one on the right. Lost packets (Loss, bunched up by Burst) leave a
    // hole filled the Fill way: nothing (GAP), the last one again (REPT), its
    // ghost fading (FADE), hiss (NOISE), backwards (REVRS), garbage (GARBL),
    // an old one out of order (LATE). Codec and Jump coarsen them, Edge
    // rounds their corners, Size how long they are; the bars show the line.
    sgnl(g, q, v, t, C, dt) {
      const s = v.s, T = tones(C), col = (a, c) => css(lerp3(T.bg, c, clamp(a)));
      if (!s.init) { s.init = 1; s.r = rng(v.e.id || 211); s.p = []; s.acc = 0; s.lost = false; s.seq = 0; s.q = 0; }
      const loss = q(/^loss$/i, 0.3), size = q(R.size, 0.4), codec = q(/^codec$/i, 0.3), fill = q.raw(/^fill$/i, 1), burst = q(/^burst$/i, 0.3);
      const jump = q(/^jump$/i, 0.2), line = q.raw(/^line$/i, 0), edge = q(/^edge$/i, 0.3), mix = q(R.mix, 1);
      const pw = 4 + size * 22, speed = 70, cy = 70, X0 = 30, X1 = 226;
      if (v.live) { s.acc += dt * speed; while (s.acc >= pw + 2) { s.acc -= pw + 2;
        const pL = loss * 0.85, stay = burst * 0.9, lost = s.lost ? s.r() < stay + (1 - stay) * pL : s.r() < pL * (1 - stay) / Math.max(0.05, 1 - pL * stay);
        s.lost = lost; if (s.r() < jump * 0.08) s.q = 4; else s.q = Math.max(0, s.q - 1);
        s.p.push({ x: X0, seq: s.seq++, lost, f: fill === 7 ? [0, 1, 2, 3, 4, 6][(s.r() * 6) | 0] : fill, a: 0.4 + 0.6 * s.r(), bad: s.q > 0 }); }
        for (const p of s.p) p.x += dt * speed; s.p = s.p.filter(p => p.x < X1); }
      // masts
      for (const mx of [16, 240]) { g.strokeStyle = col(0.7, T.lo); g.lineWidth = 1.4; g.beginPath(); g.moveTo(mx, cy + 30); g.lineTo(mx, cy - 14); g.moveTo(mx - 6, cy + 30); g.lineTo(mx, cy - 4); g.lineTo(mx + 6, cy + 30); g.stroke();
        for (let r = 1; r <= 2; r++) { g.beginPath(); g.arc(mx, cy - 14, r * 5, -2.4, -0.7); g.stroke(); } }
      let prev = null;
      for (const p of s.p) {
        const h = 24 * p.a, crush = Math.round(1 + (codec + (p.bad ? 0.5 : 0)) * 4), x = p.x, rr = edge * pw * 0.4;
        const box = (a, c, dash) => { g.fillStyle = col(a, c); if (dash) { for (let yy = -h; yy < h; yy += crush + 1) g.fillRect(x, cy + yy, pw, crush); } else { g.beginPath(); g.roundRect ? g.roundRect(x, cy - h, pw, h * 2, rr) : g.rect(x, cy - h, pw, h * 2); g.fill(); } };
        if (!p.lost) { box((0.45 + 0.55 * mix) * (p.bad ? 0.6 : 1), T.hi, crush > 1); prev = p; continue; }
        const f = p.f;
        if (f === 0) { g.strokeStyle = col(0.3, T.lo); g.lineWidth = 1; g.strokeRect(x + 0.5, cy - 4, pw - 1, 8); }
        else if (f === 1 && prev) { g.fillStyle = col(0.55, T.lo); g.fillRect(x, cy - prev.a * 24, pw, prev.a * 48); }
        else if (f === 2 && prev) { g.fillStyle = col(0.3, T.lo); g.fillRect(x, cy - prev.a * 12, pw, prev.a * 24); }
        else if (f === 3) { for (let i = 0; i < 12; i++) { g.fillStyle = col(0.5, T.lo); g.fillRect(x + s.r() * pw, cy + (s.r() - 0.5) * 30, 1.2, 1.2); } }
        else if (f === 4 && prev) { g.fillStyle = col(0.55, T.lo); g.fillRect(x, cy - prev.a * 24, pw, prev.a * 48); g.fillStyle = C.bg; g.beginPath(); g.moveTo(x + pw - 2, cy - 4); g.lineTo(x + 2, cy); g.lineTo(x + pw - 2, cy + 4); g.fill(); }
        else if (f === 5) { for (let yy = -26; yy < 26; yy += 3) { g.fillStyle = col(s.r(), s.r() < 0.5 ? T.hi : T.lo); g.fillRect(x, cy + yy, pw * s.r(), 2); } }
        else { g.fillStyle = col(0.45, T.lo); g.fillRect(x, cy - 10, pw, 20); g.fillStyle = C.bg; g.font = "bold 6px ui-monospace, monospace"; g.fillText(String((p.seq - 3) % 10), x + pw / 2 - 2, cy + 2); }
      }
      // signal bars and line
      const bars = Math.max(0, Math.round(4 - loss * 4));
      for (let b = 0; b < 4; b++) { g.fillStyle = col(b < bars ? 0.9 : 0.15, T.lo); g.fillRect(222 + b * 7, 14 - b * 3, 4, 4 + b * 3); }
      g.fillStyle = col(0.75, T.lo); g.font = "bold 8px ui-monospace, monospace";
      g.fillText(["HIFI", "VOIP", "PHONE", "WALKY"][line] + "  " + ["GAP", "REPT", "FADE", "NOISE", "REVRS", "GARBL", "LATE", "RND"][fill], 6, 11);
    },
  };

  // Carillon's scales: which of 32 bands survive the snap (build.py's SCALES).
  const SCALES = [[], [...Array(12).keys()], [0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 9, 10],
    [0, 2, 4, 7, 9], [0, 3, 5, 7, 10], [0, 2, 4, 6, 8, 10], [0, 7], [0]];
  function bandsOn(scale, key, N) {
    const sc = SCALES[scale] || SCALES[1];
    if (!sc.length) return Array(N).fill(true);
    let prev = -1000; const out = [];
    for (let i = 0; i < N; i++) {
      const s = Math.round(i * 72 / (N - 1)) - key, pc = ((s % 12) + 12) % 12;
      let best = 99; for (const n of sc) for (const o of [-12, 0, 12]) { const d = n + o - pc; if (Math.abs(d) < Math.abs(best) || (Math.abs(d) === Math.abs(best) && d < best)) best = d; }
      const n = s + best; out.push(n !== prev); prev = n;
    }
    return out;
  }
  function eqShape(q, u) {                  // SyncEQ-ish: shelves, a mid bump, the cuts
    const lo = q(/^low$/i, 0.5), mid = q(/^mid$/i, 0.5), mf = q(/^midf$/i, 0.5), hi = q(/^high$/i, 0.5);
    const lc = q(/^locut$/i, 0), hc = q(/^hicut$/i, 1);
    let g = 0.45 + (lo - 0.5) * 0.6 / (1 + Math.exp((u - 0.25) * 18)) + (hi - 0.5) * 0.6 / (1 + Math.exp(-(u - 0.75) * 18))
      + (mid - 0.5) * 0.7 * Math.exp(-(((u - mf) * 5) ** 2));
    g *= 1 / (1 + Math.exp(-(u - lc * 0.5) * 30)) * (1 / (1 + Math.exp((u - (0.5 + hc * 0.5)) * 30)));
    return g;
  }

  /* ---- which scene for which effect ----------------------------------- */
  const FX = {
    Deform: ["deform"], Chimera: ["throat"], Scorch: ["scorch"], DirtBox: ["dirtbox"], WaveFold: ["wavefold"], Dustbox: ["dustbox"],
    Corrupt: ["corrupt"], Tonic: ["tonic"], Rewire: ["rewire"], Oxide: ["oxide"], Howl: ["howl"], Skull: ["skull"],
    Carillon: ["bells"], SyncEQ: ["synceq"], Spectrum: ["spectrum"], Klang: ["klang"],
    Droplets: ["grains"], Chopdly: ["chopdly"], Loop: ["loop"], Scrub: ["scrub"], Stasis: ["stasis"],
    GridDly: ["griddly"], Mangle: ["mangle"], Spiral: ["spiral"], Hydra: ["hydra"],
    Spool: ["spool"], Ring: ["ring"], DubSiren: ["dubsiren"], Gyre: ["gyre"],
    Rooms: ["rooms"], Horizon: ["horizon"], HorizonL: ["horizon"], Cloud: ["cloud"], Galactic: ["galaxy"], Breather: ["breather"],
    Flower: ["flower"], Taffy: ["taffy"], GenLoss: ["genloss"], Scope: ["scope"], Sweep: ["sweep"],
    Arrakis: ["arrakis"], DualShft: ["dualshft"], Microlm: ["microlm"], Choral: ["choral"],
    EuGate: ["eugate"], Dissolve: ["dissolve"], Shatter: ["shatter"], "S.GN_L": ["sgnl"],
  };
  const BY_CATEGORY = { Modulation: ["lfo"], Reverb: ["space", "reverb"], Delay: ["space", "delay"], Filter: ["filter"],
    Amp: ["hatch"], Drive: ["hatch"], Dynamics: ["dyn"], SFX: ["steps"], Pedal: ["filter"] };
  function sceneFor(e) {
    const n = e.name || "";
    if (FX[n]) return FX[n];
    if (/^NAMLite/.test(n)) return ["hatch"];
    if (/^CabIR/.test(n)) return ["filter"];
    return BY_CATEGORY[e.category] || ["lfo"];
  }

  /* ---- the loop -------------------------------------------------------- */
  let io = null;
  function attach(canvas, effect, get) {
    const e = effect || { name: "?", params: [] };
    const [scene, kind] = sceneFor(e);
    const v = { canvas, e, get, scene, kind, s: {}, ix: {}, visible: true, live: motion() };
    views.add(v);
    if (root.IntersectionObserver) {
      if (!io) io = new IntersectionObserver(es => es.forEach(en => { for (const w of views) if (w.canvas === en.target) w.visible = en.isIntersecting; }));
      io.observe(canvas);
    }
    if (!raf) raf = requestAnimationFrame(tick);
    return v;
  }
  function tick(now) {
    raf = 0;
    if (now - last < 15) { raf = requestAnimationFrame(tick); return; }   // up to ~60 frames a second
    const dt = Math.min(0.1, (now - (last || now)) / 1000); last = now;
    const C = colours(), live = motion();
    for (const v of [...views]) {
      if (!v.canvas.isConnected) { views.delete(v); if (io) io.unobserve(v.canvas); continue; }
      if (!v.visible || document.hidden || !v.canvas.offsetParent) continue;
      draw(v, now / 1000, dt, C, live);
    }
    if (views.size) raf = requestAnimationFrame(tick);
  }
  function draw(v, t, dt, C, live) {
    const cv = v.canvas, dpr = root.devicePixelRatio || 1;
    const box = cv.parentElement || cv;            // the container's width: the canvas's own would feed back
    const w = Math.min(1600, Math.max(64, Math.round((box.clientWidth || 256) * dpr))), h = Math.round(w / 2);
    if (cv.width !== w || cv.height !== h) { cv.width = w; cv.height = h; }
    const g = cv.getContext("2d");
    g.setTransform(1, 0, 0, 1, 0, 0);
    g.fillStyle = C.bg; g.fillRect(0, 0, w, h);
    g.setTransform(w / LW, 0, 0, h / LH, 0, 0);
    g.fillStyle = C.fg; g.strokeStyle = C.fg; g.lineCap = "butt";
    const q = knobs(v);
    // A slot that is off holds its picture where it was (its own clock stops),
    // dimmed; knob moves still reshape it.
    const run = live && q.on;
    v.live = run;
    if (run) v.clock = (v.clock || 0) + dt;
    try { SCENES[v.scene](g, q, v, run ? v.clock : (live ? v.clock || 0 : 0), C, run ? dt : (q.on ? 0.05 : 0)); }
    catch (err) { v.scene = "lfo"; }
    if (!q.on) { g.setTransform(1, 0, 0, 1, 0, 0); g.globalAlpha = 0.55; g.fillStyle = C.bg; g.fillRect(0, 0, w, h); g.globalAlpha = 1; }
  }

  root.FxVis = { attach, sceneFor, motion, setMotion, SCENES: Object.keys(SCENES) };
})(typeof window !== "undefined" ? window : globalThis);
