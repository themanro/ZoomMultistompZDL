/*
 * carillon.c -- Carillon: a resonant filter bank, MS-70CDR.
 *
 * 32 narrow band-pass filters from C2 to C8, snapped to a key and scale. At
 * low Decay they act together as one filter (Cutoff, Reso and Slope shape
 * it); as Decay rises each band rings longer, until the bank rings like a
 * set of tuned bells on everything you play. Waves sends a slow ripple
 * across the bands; Noise strikes random bands on its own.
 *
 * 9 knobs. Key and Scale are named steps (the pedal shows the names:
 * build/selector_metadata.py, which must match the tables here); the shared
 * edit handler stores raw/100, read back as round(norm * 100).
 *   Cutoff (params[5])  where the bank's emphasis sits, C2 .. above C8
 *   Reso   (params[6])  boost of the bands around Cutoff
 *   Decay  (params[7])  band width and ring: 0 a plain filter, 100 bells
 *                       (a few seconds; low bands ring longest)
 *   Key    (params[8])  C .. B: the scale's root (Free: moves the bank)
 *   Scale  (params[9])  Free, Chrom, Major, Minor, Dor, Pent, MinPt, Whole,
 *                       Fifth, Oct; bands snap to the nearest note in it, a
 *                       band landing on its neighbour's note is left out
 *   Slope  (params[10]) low-pass (0) .. band-pass (50) .. high-pass (100)
 *   Waves  (params[11]) 0..50 depth of a slow ripple across the bands;
 *                       50..100 full depth, faster (0.12 .. 2.5 Hz)
 *   Noise  (params[12]) 0..50 random bands struck on their own, more often;
 *                       50..100 adds a quiet hiss that makes the bank whistle
 *   Mix    (params[13]) dry/wet
 *
 * The filters are Chamberlin state-variable filters (low += f*band; high =
 * in - low - q*band; band += f*high): in tune down to C2, where a biquad's
 * coefficients run out of float precision. f = 2 sin(pi F / fs) comes from a
 * semitone table (build.py), interpolated for Free; the input is scaled by
 * q so every band peaks at unity. Coefficients update 8 bands a callback.
 *
 * Screen (zvis, MS-70CDR 2.10 only; elsewhere the static cover stays): a bar
 * per band, its level; a mark per band at the filter shape's height (Cutoff,
 * Reso, Slope and Waves together); header: Key and Scale.
 *
 * Safe-DSP (docs/SAFE-DSP-RULES.md): no switch, no math lib, no divide (a
 * Newton reciprocal instead), no float->unsigned cast, no writable statics
 * (all state lives in the ctx[3] arena), magic shuttle preserved. Tables are
 * const data.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "carillon_params.h"
#include "../common/zvis.h"
#include "carillon_art.h"

#ifndef CARILLON_AUDIO_FUNC
#define CARILLON_AUDIO_FUNC Fx_DLY_Carillon
#endif

#ifdef CARILLON_HOST
typedef uintptr_t cr_word;
#else
typedef unsigned int cr_word;
#endif

#define CR_DO_PRAGMA(x) _Pragma(#x)
#define CR_EXPAND_PRAGMA(x) CR_DO_PRAGMA(x)
#define CR_CODE_SECTION(f) CR_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
#define CR_INLINE(f) CR_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(f))

CR_CODE_SECTION(CARILLON_AUDIO_FUNC)

extern unsigned int _Fx_DLY_Carillon_Info[];   /* imageInfo: the linker's symbol */

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define CR_MAGIC   0x4341524Cu        /* 'CARL' */
#define CR_VERSION 1u
#define CR_FXID    497u

#define CR_NB      32u                /* bands */
#define CR_UPD     8u                 /* bands updated per callback */
#define CR_SPAN    72.0f              /* semitones the bands cover, C2 .. C8 */
#define CR_FS      44100.0f
#define CR_WET     1.0f               /* wet level at Mix 100 */
#define CR_BAR_BOT 34                 /* bars: rows 14..34 */
#define CR_BAR_H   20
/* header cells (x = 1 + 4 * cell; line 1 = cell + 32); build.py's TAGS put
 * KEY at line 1 cell 0 and SCALE at line 1 cell 8. */
#define CR_CELL_KEY   36u
#define CR_CELL_SCALE 46u

typedef struct {
    float low, band;                  /* filter state */
    float f, q;                       /* coefficients */
    float b;                          /* input gain: q x shape x waves (smoothed) */
    float w;                          /* shape x waves, for the screen */
    float semi;                       /* pitch, semitones above C2 */
    int   on;                         /* 0: snapped onto its neighbour's note */
} Band;

typedef struct CarState {
    uint32_t magic, version;
    uint32_t rng;
    uint32_t upd;                     /* next band to update */
    int      lastKey, lastScale;
    float    ph1, ph2;                /* the two waves' phases, 0..1 */
    uint32_t tPluck;                  /* samples to the next struck band */
    float    lim;                     /* wet limiter envelope */
    float    onGain;                  /* bands / bands in use */
    float    shAcc, shN;              /* shape (no Reso) summed over this pass */
    float    shGain, shG;             /* 1/sqrt(its mean), capped at +12 dB; smoothed */
    Band     bd[CR_NB];

    /* screen */
    uint32_t step;
    unsigned char ln[64];             /* header: font index per cell, 2 lines x 32 */
    ZvState  z;
} CarState;

CR_INLINE(cr_abs)
static inline float cr_abs(float x) { return x < 0.0f ? -x : x; }

CR_INLINE(cr_c01)
static inline float cr_c01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

/* 1/x without a divide: bit-trick seed + three Newton steps (x > 0). */
CR_INLINE(cr_rcp)
static inline float cr_rcp(float x)
{
    union { float f; uint32_t u; } c;
    c.f = x;
    c.u = 0x7EF311C7u - c.u;
    float y = c.f;
    y = y * (2.0f - x * y);
    y = y * (2.0f - x * y);
    y = y * (2.0f - x * y);
    return y;
}

/* 1/sqrt(x) (x > 0): bit-trick seed + two Newton steps. */
CR_INLINE(cr_rsqrt)
static inline float cr_rsqrt(float x)
{
    union { float f; uint32_t u; } c;
    c.f = x;
    c.u = 0x5F3759DFu - (c.u >> 1);
    float y = c.f;
    y = y * (1.5f - 0.5f * x * y * y);
    y = y * (1.5f - 0.5f * x * y * y);
    return y;
}

/* 2^x for x in -40..40: exponent bits for the integer part, a series for the
 * fraction. */
CR_INLINE(cr_exp2)
static inline float cr_exp2(float x)
{
    int i = (int)x;
    if ((float)i > x) i--;
    float fr = (x - (float)i) * 0.69314718f;
    union { float f; uint32_t u; } c;
    c.u = (uint32_t)(i + 127) << 23;
    return c.f * (1.0f + fr * (1.0f + fr * (0.5f + fr * (0.16666667f + fr * (0.041666668f + fr * 0.0083333338f)))));
}

/* sin(2 pi p), p >= 0: parabola plus one correction step. */
CR_INLINE(cr_sin1)
static inline float cr_sin1(float p)
{
    int i = (int)p;
    float v = 2.0f * (p - (float)i) - 1.0f;           /* -1..1 */
    float y = -4.0f * v * (1.0f - cr_abs(v));
    return y + 0.225f * (y * cr_abs(y) - y);
}

/* uniform 0..1 from an LCG step */
CR_INLINE(cr_u)
static inline float cr_u(uint32_t *r)
{
    *r = *r * 1664525u + 1013904223u;
    return (float)(int)(*r >> 8) * (1.0f / 16777216.0f);
}

/* 1 / (1 + x^4), x >= 0 */
CR_INLINE(cr_roll)
static inline float cr_roll(float x)
{
    float x2 = x * x;
    return cr_rcp(1.0f + x2 * x2);
}

/* Vertical span of rows y0..y1 in column x: set (black) or flipped. */
CR_INLINE(cr_vspan)
static inline void cr_vspan(ZvState *z, int x, int y0, int y1, int flip)
{
    int b;
    if ((unsigned)x >= (unsigned)ZV_W) return;
    for (b = y0 >> 3; b <= y1 >> 3; ++b) {
        int lo = b == y0 >> 3 ? y0 & 7 : 0, hi = b == y1 >> 3 ? y1 & 7 : 7;
        unsigned char m = (unsigned char)((0xFFu << lo) & (0xFFu >> (7 - hi)));
        unsigned char *p = &z->frame[b * ZV_W + x];
        *p = flip ? (unsigned char)(*p ^ m) : (unsigned char)(*p | m);
    }
}

void CARILLON_AUDIO_FUNC(cr_word *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    cr_word *magicSrc = ZDL_PTR(cr_word *, ctx[12]);
    cr_word *magicDst = ZDL_PTR(cr_word *, *(cr_word *)ZDL_PTR(cr_word *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile cr_word *desc = ZDL_PTR(volatile cr_word *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u) return;
    if (base + sizeof(CarState) > end) return;

    CarState *st = (CarState *)base;
    uint32_t i;

    if (st->magic != CR_MAGIC || st->version != CR_VERSION) {
        st->magic = CR_MAGIC;
        st->version = CR_VERSION;
        st->rng = 0x2545F491u;
        st->upd = 0u;
        st->lastKey = -1;
        st->lastScale = -1;
        st->ph1 = 0.0f;
        st->ph2 = 0.37f;
        st->tPluck = 4410u;
        st->lim = 0.0f;
        st->onGain = 1.0f;
        st->shAcc = st->shN = 0.0f;
        st->shGain = st->shG = 1.0f;
        for (i = 0u; i < CR_NB; i++) {
            Band *d = &st->bd[i];
            d->low = d->band = 0.0f;
            d->f = 0.01f;
            d->q = 1.0f;
            d->b = d->w = 0.0f;
            d->semi = 0.0f;
            d->on = 0;
        }
        st->step = 0u;
        zv_init(&st->z, CR_FXID, _Fx_DLY_Carillon_Info);
    }

    /* named steps: raw/100 from the edit handler, back to the step number */
    int keyI = (int)(zoom_param_norm01(params[CARILLON_KEY_SLOT],   CARILLON_KEY_UI_DEFAULT * 0.01f)   * 100.0f + 0.5f);
    int sclI = (int)(zoom_param_norm01(params[CARILLON_SCALE_SLOT], CARILLON_SCALE_UI_DEFAULT * 0.01f) * 100.0f + 0.5f);
    if (keyI > 11) keyI = 11;
    if (sclI > 9) sclI = 9;
    float cut   = zoom_param_norm01(params[CARILLON_CUTOFF_SLOT], CARILLON_CUTOFF_DEFAULT_NORM);
    float reso  = zoom_param_norm01(params[CARILLON_RESO_SLOT],   CARILLON_RESO_DEFAULT_NORM);
    float decay = zoom_param_norm01(params[CARILLON_DECAY_SLOT],  CARILLON_DECAY_DEFAULT_NORM);
    float slope = zoom_param_norm01(params[CARILLON_SLOPE_SLOT],  CARILLON_SLOPE_DEFAULT_NORM);
    float waves = zoom_param_norm01(params[CARILLON_WAVES_SLOT],  CARILLON_WAVES_DEFAULT_NORM);
    float noise = zoom_param_norm01(params[CARILLON_NOISE_SLOT],  CARILLON_NOISE_DEFAULT_NORM);
    float mix   = zoom_param_norm01(params[CARILLON_MIX_SLOT],    CARILLON_MIX_DEFAULT_NORM);

    /* --- band pitches: only when Key or Scale changes --- */
    if (keyI != st->lastKey || sclI != st->lastScale) {
        const signed char *snap = &CAR_SNAP[(uint32_t)sclI * 12u];
        int prev = -1000;
        for (i = 0u; i < CR_NB; i++) {
            Band *d = &st->bd[i];
            float s = (float)(int)i * (CR_SPAN / (float)(int)(CR_NB - 1u));
            if (sclI == 0) {          /* Free: as spaced, moved by Key */
                d->semi = s + (float)keyI;
                d->on = 1;
            } else {
                int n = (int)(s + 0.5f) - keyI;
                int pc = n + 12;
                while (pc >= 12) pc -= 12;    /* pitch class, no divide */
                n += snap[pc] + keyI;
                if (n < 0) n += 12;
                d->semi = (float)n;
                d->on = n != prev;
                prev = n;
            }
        }
        {
            uint32_t n = 0u;
            for (i = 0u; i < CR_NB; i++) n += st->bd[i].on ? 1u : 0u;
            st->onGain = 32.0f * cr_rcp((float)(int)(n ? n : 1u));   /* few bands catch little */
        }
        st->lastKey = keyI;
        st->lastScale = sclI;
    }

    /* --- per-callback values --- */
    /* Decay, eased (d' = d (1.5 - d/2)) so the middle already rings:
     * Q 3 .. 3072 (width; 1 kHz rings ~0.5 s at 50, ~4 s at 100) and a cap on
     * any band's ring, 0.15 .. 9.6 s (the low bands would ring for ever) */
    float de = decay * (1.5f - 0.5f * decay);
    float qInv = 0.33333334f * cr_exp2(-10.0f * de);
    float tTerm = (13.8155f / (0.15f * CR_FS)) * cr_exp2(-6.0f * de);    /* 2 ln(1000) / (T fs) */
#ifdef CR_NOMAKEUP
    float makeup = 1.0f;
#else
    /* makeup, measured with the bank wide open on DI guitar: at Decay 0 the
     * wide bands overlap, +9 dB; narrower bands catch less, about -16 dB by
     * the top (key barely matters: pick noise, vibrato and stretched
     * overtones fall between bands). So -9 dB, a saturating rise and a slope
     * on top; times bands / bands in use (Oct uses 7). Pure tones exactly on a band
     * come out louder at high Decay; the limiter holds them. */
    float makeup = cr_exp2(-1.495f + 1.827f * (1.0f - cr_exp2(-6.0f * de)) + de * (2.657f + 0.332f * de)) * st->onGain;
#endif
    float rsn = reso * reso * 3.0f;
    float wetG = mix * CR_WET * cr_rcp(1.0f + 0.35f * rsn), dryG = 1.0f - mix;
    float cutSemi = cut * 78.0f - 3.0f;

    float depth = cr_c01(2.0f * waves);
    float wx = cr_c01(2.0f * waves - 1.0f);
    float wRate = 0.12f + 2.38f * wx * wx;              /* Hz */
    st->ph1 += wRate * (8.0f / CR_FS);
    st->ph2 += wRate * (0.63f * 8.0f / CR_FS);
    if (st->ph1 >= 1.0f) st->ph1 -= 1.0f;
    if (st->ph2 >= 1.0f) st->ph2 -= 1.0f;

    /* --- coefficients, CR_UPD bands a callback --- */
    {
        uint32_t k, u = st->upd;
        for (k = 0u; k < CR_UPD; k++) {
            Band *d = &st->bd[u];
            /* f from the semitone table; Free interpolates */
            float s = d->semi;
            int si = (int)s;
            float fr = s - (float)si;
            float f = CAR_F[si] + (CAR_F[si + 1] - CAR_F[si]) * fr;
            float q = qInv + tTerm * cr_rcp(f);
            if (q > 1.2f) q = 1.2f;
            /* shape: low-pass .. band-pass .. high-pass, plus Reso's bump */
            float dl = (s - cutSemi) * (1.0f / 12.0f);        /* octaves from Cutoff */
            float up = dl > 0.0f ? dl : 0.0f, dn = dl < 0.0f ? -dl : 0.0f;
            float lp = cr_roll(up * 1.6f), hp = cr_roll(dn * 1.6f), bp = cr_roll(cr_abs(dl) * 1.1f);
            float sh = slope < 0.5f ? lp + (bp - lp) * (slope * 2.0f)
                                    : bp + (hp - bp) * (slope * 2.0f - 1.0f);
            float bump = cr_rcp(1.0f + dl * dl * 9.0f);
            if (d->on) { st->shAcc += sh; st->shN += 1.0f; }
            sh += rsn * bump;
            /* waves: two ripples across the bands, opposite ways */
            float fi = (float)(int)u;
            float wv = 0.5f + 0.25f * (cr_sin1(st->ph1 + fi * 0.131f) + cr_sin1(st->ph2 + 3.0f - fi * 0.049f));
            float w = d->on ? sh * (1.0f - depth * wv) * (1.0f + 0.5f * depth) : 0.0f;
            d->f = f;
            d->q = q;
            d->w = w;
            d->b += (q * w - d->b) * 0.5f;
            u = u + 1u >= CR_NB ? 0u : u + 1u;
            if (u == 0u) {            /* a pass done: what the shape lets through */
                float m = st->shAcc * cr_rcp(st->shN + 0.001f);
                if (m < 0.0625f) m = 0.0625f;
                st->shGain = cr_rsqrt(m);
                st->shAcc = st->shN = 0.0f;
            }
        }
        st->upd = u;
        st->shG += (st->shGain - st->shG) * 0.02f;
    }

    /* --- noise: struck bands, then hiss --- */
    float hiss = cr_c01(2.0f * noise - 1.0f) * 0.015f;
    uint32_t rng = st->rng;
    if (noise > 0.01f) {
        if (st->tPluck > 8u) st->tPluck -= 8u;
        else {
            float per = 0.5f + 20.0f * noise * noise;     /* strikes a second */
            uint32_t k = (rng >> 11) & (CR_NB - 1u);
            Band *d = &st->bd[k];
            float a = (0.3f + 0.7f * cr_u(&rng)) * 0.1f * cr_rcp(makeup);
            d->low += a * (d->w > 1.0f ? 1.0f : d->w);    /* kick low: the ring starts at 0, no step */
            st->tPluck = (uint32_t)(int)((0.3f + 1.4f * cr_u(&rng)) * CR_FS * cr_rcp(per));
        }
    }

    float lim = st->lim;
    float mkW = makeup * st->shG;

    int fr;
    for (fr = 0; fr < 8; fr++) {
        float inL = fxBuf[fr];
        float inR = fxBuf[fr + 8];
        float x = 0.5f * (inL + inR);
        if (hiss > 0.0f) x += hiss * (cr_u(&rng) - 0.5f);

        float accE = 0.0f, accO = 0.0f;
        uint32_t k;
        /* Eight bands a pass, each band's state loaded into locals first: the
         * bands are independent, so the compiler can overlap their (serial)
         * filter steps instead of waiting out each one's latency. The maths,
         * and the order the bands are summed in, are exactly as before. */
        for (k = 0u; k < CR_NB; k += 8u) {
            Band *d = &st->bd[k];
            float l0 = d[0].low, b0 = d[0].band, l1 = d[1].low, b1 = d[1].band;
            float l2 = d[2].low, b2 = d[2].band, l3 = d[3].low, b3 = d[3].band;
            float l4 = d[4].low, b4 = d[4].band, l5 = d[5].low, b5 = d[5].band;
            float l6 = d[6].low, b6 = d[6].band, l7 = d[7].low, b7 = d[7].band;
#define CR_SVF(i) \
            l##i = l##i + d[i].f * b##i; \
            { float h = d[i].b * x - l##i - d[i].q * b##i; b##i = b##i + d[i].f * h; }
            CR_SVF(0) CR_SVF(1) CR_SVF(2) CR_SVF(3) CR_SVF(4) CR_SVF(5) CR_SVF(6) CR_SVF(7)
#undef CR_SVF
            d[0].low = l0; d[0].band = b0; d[1].low = l1; d[1].band = b1;
            d[2].low = l2; d[2].band = b2; d[3].low = l3; d[3].band = b3;
            d[4].low = l4; d[4].band = b4; d[5].low = l5; d[5].band = b5;
            d[6].low = l6; d[6].band = b6; d[7].low = l7; d[7].band = b7;
            accE += b0; accO += b1; accE += b2; accO += b3;
            accE += b4; accO += b5; accE += b6; accO += b7;
        }
        /* even bands lean left, odd right */
        float wL = (accE + 0.45f * accO) * mkW;
        float wR = (accO + 0.45f * accE) * mkW;
        {
            float a = cr_abs(wL) > cr_abs(wR) ? cr_abs(wL) : cr_abs(wR);
            lim = a > lim ? a : lim * 0.9998f;
            if (lim > 0.9f) { float g = 0.9f * cr_rcp(lim); wL *= g; wR *= g; }
        }
        fxBuf[fr]     = dryG * inL + wetG * wL;
        fxBuf[fr + 8] = dryG * inR + wetG * wR;
    }
    st->rng = rng;
    st->lim = lim;

    /* --- the screen: one piece of the frame per callback --- */
    {
        unsigned int *info = _Fx_DLY_Carillon_Info;
        int visible = zv_visible(&st->z, (const uintptr_t *)ctx);
        zv_service(&st->z, info, visible);
        if (!visible) { st->step = 0u; return; }
        if (st->z.phase != ZV_IDLE) return;
        if (st->step == 0u) {         /* background, then the header values */
            uint32_t c;
            unsigned char *ln = st->ln;
            for (i = 0u; i < 512u; i++) {
                st->z.frame[i] = (unsigned char)((i >> 7) == 1u ? 0x10u : 0u);
                st->z.frame[512u + i] = CAR_BG[i];
            }
            for (i = 0u; i < 64u; i++) ln[i] = CAR_TAGS[i];
            for (i = 0u; i < 2u; i++) ln[CR_CELL_KEY + i] = CAR_KEY_N[(uint32_t)keyI * 2u + i];
            for (i = 0u; i < CAR_NAME_W; i++) ln[CR_CELL_SCALE + i] = CAR_SCALE_N[(uint32_t)sclI * CAR_NAME_W + i];
            for (i = 0u; i < 64u; i++) {  /* 3 columns a glyph, 5 rows: y 0 or 6 */
                uint32_t g = ln[i], x = 1u + 4u * (i & 31u), sh = (i >> 5) * 6u;
                if (!g) continue;
                for (c = 0u; c < 3u; c++) {
                    uint32_t v = (uint32_t)CAR_FONT[3u * g + c] << sh;
                    st->z.frame[x + c] |= (unsigned char)v;
                    st->z.frame[ZV_W + x + c] |= (unsigned char)(v >> 8);
                }
            }
            st->step = 1u;
            return;
        }
        {                             /* a bar per band, a mark at the shape's height: */
            uint32_t k;                /* half the bands a callback, so no callback runs long */
            uint32_t k0 = st->step == 1u ? 0u : CR_NB / 2u, k1 = k0 + CR_NB / 2u;
            float wMax = 1.0f + rsn;
            float sc = (float)CR_BAR_H * cr_rcp(wMax);
            for (k = k0; k < k1; k++) {
                Band *d = &st->bd[k];
                int x = 4 * (int)k;
                if (!d->on) { zv_px(&st->z, x + 1, CR_BAR_BOT, 1); continue; }
                float lo = cr_abs(d->low), bn = cr_abs(d->band);
                float m = (lo > bn ? lo + 0.4f * bn : bn + 0.4f * lo) * makeup;
                int h = (int)((float)CR_BAR_H * m * cr_rcp(m + 0.02f) + 0.5f);
                int y = CR_BAR_BOT - (int)(d->w * sc + 0.5f);
                if (y < CR_BAR_BOT - CR_BAR_H) y = CR_BAR_BOT - CR_BAR_H;
                if (h > 0) {
                    cr_vspan(&st->z, x, CR_BAR_BOT - h + 1, CR_BAR_BOT, 0);
                    cr_vspan(&st->z, x + 1, CR_BAR_BOT - h + 1, CR_BAR_BOT, 0);
                    cr_vspan(&st->z, x + 2, CR_BAR_BOT - h + 1, CR_BAR_BOT, 0);
                }
                cr_vspan(&st->z, x, y, y, 1);   /* the mark: flipped over the bar */
                cr_vspan(&st->z, x + 1, y, y, 1);
                cr_vspan(&st->z, x + 2, y, y, 1);
            }
            if (st->step == 1u) { st->step = 2u; return; }
#ifdef CARILLON_HOST
            { extern void cr_host_frame(const unsigned char *f); cr_host_frame(st->z.frame); }
#endif
            zv_submit(&st->z);
            st->step = 0u;
        }
    }
}
