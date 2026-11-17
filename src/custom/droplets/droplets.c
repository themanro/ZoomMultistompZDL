/*
 * droplets.c -- Droplets: a granular delay, MS-70CDR.
 *
 * Records what you play into a ring and sprays short grains (droplets) from
 * it: up to 8 at once, each with its own start point, pitch, length,
 * envelope, level, direction and place in the stereo field. Feedback writes
 * the grains back into the ring; Feedbk at the top (Lock) stops recording,
 * so the grains keep playing from a frozen buffer that Scan can move through.
 *
 * 9 knobs. Pitch and Pattern are named steps (the pedal shows the names:
 * build/selector_metadata.py, which must match the tables here); the shared
 * edit handler stores raw/100, read back as round(norm * 100).
 *   Pitch   (params[5])  -24 -12 -7 -5 -3 Unity +3 +5 +7 +12 +24, every grain
 *   Rate    (params[6])  grains per second, ~2 .. 60, free running
 *   Size    (params[7])  grain length 10 .. 250 ms
 *   Contour (params[8])  envelope peak position: 0 sharp hit .. 100 swell
 *   Scan    (params[9])  0..49 where grains start, 30 ms .. 2.5 s behind the
 *                        input (or the frozen buffer's end); 50..100 the start
 *                        point travels through the last 2.5 s, 0 .. 2x speed
 *   Pattern (params[10]) pitch sequence per grain on top of Pitch: Off, Oct,
 *                        Oct-, Fifth, Pwr, Maj, Min, Down, Sus (cycles);
 *                        Pent, Wide (random picks)
 *   Random  (params[11]) 0..33 spray (start scatter, up to 0.5 s) and stereo
 *                        spread; to 66 adds pitch jumps, length, timing and
 *                        level; to 100 adds reversed grains (up to half)
 *   Feedbk  (params[12]) grains back into the ring; 100 = Lock (freeze)
 *   Mix     (params[13]) dry/wet
 *
 * Screen (zvis, MS-70CDR 2.10 only; elsewhere the static cover stays): the
 * last 2.5 s as a waveform (oldest left), a line per grain where it is
 * reading (taller as its envelope opens), the scan point under it; header:
 * Pitch and Pattern steps, LOCK while frozen.
 *
 * Safe-DSP (docs/SAFE-DSP-RULES.md): no switch, no math lib, no divide (a
 * Newton reciprocal instead), no float->unsigned cast, no writable statics
 * (all state and the ring live in the ctx[3] arena, validated and cleared
 * lazily), unity-slope soft clip inside the feedback loop, magic shuttle
 * preserved. Tables are const data.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "droplets_params.h"
#include "../common/zvis.h"
#include "droplets_art.h"

#ifndef DROPLETS_AUDIO_FUNC
#define DROPLETS_AUDIO_FUNC Fx_DLY_Droplets
#endif

/* The pedal hands us 32-bit words; the desktop harness needs pointer-sized ones. */
#ifdef DROPLETS_HOST
typedef uintptr_t dr_word;
#else
typedef unsigned int dr_word;
#endif

#define DR_DO_PRAGMA(x) _Pragma(#x)
#define DR_EXPAND_PRAGMA(x) DR_DO_PRAGMA(x)
#define DR_CODE_SECTION(f) DR_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
#define DR_INLINE(f) DR_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(f))

DR_CODE_SECTION(DROPLETS_AUDIO_FUNC)

extern unsigned int _Fx_DLY_Droplets_Info[];   /* imageInfo: the linker's symbol */

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define DR_MAGIC   0x44524F50u        /* 'DROP' */
#define DR_VERSION 1u
#define DR_FXID    492u

#define DR_RING       262144u         /* int16 ring, 512 KB, ~5.9 s */
#define DR_MASK       (DR_RING - 1u)
#define DR_RING_WORDS (DR_RING / 2u)
#define DR_CLEAR_STEP 4096u           /* words cleared per call */
#define DR_NG         8u              /* grains at once */
#define DR_W          110250.0f       /* 2.5 s: scan range, the screen's window */
#define DR_DMIN       1323.0f         /* 30 ms: nearest a grain starts to the input */
#define DR_SPRAY      22050.0f        /* widest spray, 0.5 s */
#define DR_FB_MAX     0.9f
#define DR_S2F        (1.0f / 32768.0f)
#define DR_COL        862u            /* samples per screen column: 128 x 862 ~ 2.5 s */
#define DR_COL_INV    (1.0f / 862.0f)
#define DR_OV_MID     23              /* waveform rows 15..31 */
#define DR_OV_H       8
#define DR_OV_TOP     15
/* header cells (x = 1 + 4 * cell; line 1 = cell + 32); build.py's TAGS put
 * PITCH at line 1 cell 0 and PAT at line 1 cell 13. */
#define DR_CELL_PITCH 38u
#define DR_CELL_PAT   49u
#define DR_CELL_LOCK  28u

/* step tables: must match build/selector_metadata.py 'Droplets' */
static const signed char DR_PITCH[11] = { -24, -12, -7, -5, -3, 0, 3, 5, 7, 12, 24 };
/* Pattern: 8 entries a row; rows 0-8 cycle through their first DR_PATN, rows
 * 9-10 (Pent, Wide) pick any of the 8 at random. */
static const signed char DR_PAT[88] = {
    0, 0, 0, 0, 0, 0, 0, 0,            /* Off */
    0, 12, 0, 0, 0, 0, 0, 0,           /* Oct */
    0, -12, 0, 0, 0, 0, 0, 0,          /* Oct- */
    0, 7, 0, 0, 0, 0, 0, 0,            /* Fifth */
    0, 7, 12, 0, 0, 0, 0, 0,           /* Pwr */
    0, 4, 7, 12, 0, 0, 0, 0,           /* Maj */
    0, 3, 7, 12, 0, 0, 0, 0,           /* Min */
    12, 7, 4, 0, 0, 0, 0, 0,           /* Down */
    0, 5, 7, 12, 0, 0, 0, 0,           /* Sus */
    0, 2, 4, 7, 9, 12, 14, 16,         /* Pent */
    -12, 0, 0, 7, 12, 12, 19, 24 };    /* Wide */
static const unsigned char DR_PATN[11] = { 1, 2, 2, 2, 3, 4, 4, 4, 4, 8, 8 };
static const signed char DR_JUMP[4] = { -12, -5, 7, 12 };   /* Random's pitch jumps */

typedef struct {
    uint32_t base;                    /* ring index the grain reads from */
    uint32_t age, left, aLen;         /* samples played, to go, of rise */
    float    pos, step;               /* read offset from base, per sample */
    float    e, up, dn;               /* envelope, its rise / fall per sample */
    float    amp, gl, gr;             /* level, pan gains */
} Grain;

typedef struct DropState {
    uint32_t magic, version, initialized, clearIndex;
    uint32_t wp;                      /* ring write position */
    uint32_t rng;
    uint32_t tn;                      /* samples to the next grain */
    uint32_t pi;                      /* pattern step */
    float    sd;                      /* scan: distance behind wp */
    float    lim;                     /* feedback limiter envelope */
    Grain    g[DR_NG];

    /* screen */
    uint32_t colN, colAcc;            /* columns finished, samples in this one */
    float    colMax;
    uint32_t step;                    /* frame-drawing step */
    unsigned char colPk[128];         /* peak per column, ring of 128 */
    unsigned char ln[64];             /* header: font index per cell, 2 lines x 32 */
    ZvState  z;
} DropState;

DR_INLINE(dr_abs)
static inline float dr_abs(float x) { return x < 0.0f ? -x : x; }

DR_INLINE(dr_c01)
static inline float dr_c01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

/* 1/x without a divide: bit-trick seed + three Newton steps (x > 0). */
DR_INLINE(dr_rcp)
static inline float dr_rcp(float x)
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
DR_INLINE(dr_rsqrt)
static inline float dr_rsqrt(float x)
{
    union { float f; uint32_t u; } c;
    c.f = x;
    c.u = 0x5F3759DFu - (c.u >> 1);
    float y = c.f;
    y = y * (1.5f - 0.5f * x * y * y);
    y = y * (1.5f - 0.5f * x * y * y);
    return y;
}

/* unity-slope soft clip: no small-signal gain, safe inside feedback */
DR_INLINE(dr_soft)
static inline float dr_soft(float x)
{
    if (x > 1.5f) return 1.0f;
    if (x < -1.5f) return -1.0f;
    return x - (x * x * x) * 0.14814815f;
}

/* 2^(semi/12) for semi in -24..24: an octave factor, then a 7-term series
 * for the rest (|y ln2| <= 0.694). No loop, no powf. */
DR_INLINE(dr_ratio)
static inline float dr_ratio(int semi)
{
    float k = semi > 12 ? 2.0f : semi < -12 ? 0.5f : 1.0f;
    int s = semi > 12 ? semi - 12 : semi < -12 ? semi + 12 : semi;
    float x = (float)s * (0.69314718f / 12.0f);
    return k * (1.0f + x * (1.0f + x * (0.5f + x * (0.16666667f + x * (0.041666668f
         + x * (0.0083333338f + x * 0.0013888889f))))));
}

/* uniform 0..1 from an LCG step */
DR_INLINE(dr_u)
static inline float dr_u(uint32_t *r)
{
    *r = *r * 1664525u + 1013904223u;
    return (float)(int)(*r >> 8) * (1.0f / 16777216.0f);
}

/* Linear read at an offset from base. A position not recorded yet (past
 * avail, the samples since base) bounces back off the record point and runs
 * backwards: no jump. Floor by hand: offsets can be negative. */
DR_INLINE(dr_read)
static inline float dr_read(const int16_t *ring, uint32_t base, uint32_t wp, float off)
{
    float avail = (float)(int)((wp - base) & DR_MASK) - 1.0f;
    if (off > avail) off = avail + avail - off;
    int oi = (int)off;
    if ((float)oi > off) oi--;
    float fr = off - (float)oi;
    uint32_t i0 = (base + (uint32_t)oi) & DR_MASK;
    uint32_t i1 = (i0 + 1u) & DR_MASK;
    return ((float)ring[i0] * (1.0f - fr) + (float)ring[i1] * fr) * DR_S2F;
}

/* Vertical span of rows y0..y1 in column x: set (black) or flipped. */
DR_INLINE(dr_vspan)
static inline void dr_vspan(ZvState *z, int x, int y0, int y1, int flip)
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

void DROPLETS_AUDIO_FUNC(dr_word *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    dr_word *magicSrc = ZDL_PTR(dr_word *, ctx[12]);
    dr_word *magicDst = ZDL_PTR(dr_word *, *(dr_word *)ZDL_PTR(dr_word *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile dr_word *desc = ZDL_PTR(volatile dr_word *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u) return;

    uintptr_t ringBase = (base + sizeof(DropState) + 7u) & ~(uintptr_t)7u;
    if (ringBase + (uintptr_t)DR_RING * 2u > end) return;

    DropState *st = (DropState *)base;
    int16_t *ring = (int16_t *)ringBase;
    uint32_t i;

    if (st->magic != DR_MAGIC || st->version != DR_VERSION) {
        st->magic = DR_MAGIC;
        st->version = DR_VERSION;
        st->initialized = 0u;
        st->clearIndex = 0u;
        st->wp = 0u;
        st->rng = 0x2545F491u;
        st->tn = 1u;
        st->pi = 0u;
        st->sd = 22050.0f;
        st->lim = 0.0f;
        for (i = 0u; i < DR_NG; i++) st->g[i].left = 0u;
        st->colN = 0u;
        st->colAcc = 0u;
        st->colMax = 0.0f;
        st->step = 0u;
        for (i = 0u; i < 128u; i++) st->colPk[i] = 0u;
        zv_init(&st->z, DR_FXID, _Fx_DLY_Droplets_Info);
    }

    if (!st->initialized) {           /* clear the ring, a chunk per call */
        uint32_t *w = (uint32_t *)ringBase;
        uint32_t e = st->clearIndex + DR_CLEAR_STEP;
        if (e > DR_RING_WORDS) e = DR_RING_WORDS;
        for (i = st->clearIndex; i < e; i++) w[i] = 0u;
        st->clearIndex = e;
        if (e >= DR_RING_WORDS) st->initialized = 1u;
        return;
    }

    /* named steps: raw/100 from the edit handler, back to the step number */
    int pitI = (int)(zoom_param_norm01(params[DROPLETS_PITCH_SLOT],   DROPLETS_PITCH_UI_DEFAULT * 0.01f)   * 100.0f + 0.5f);
    int patI = (int)(zoom_param_norm01(params[DROPLETS_PATTERN_SLOT], DROPLETS_PATTERN_UI_DEFAULT * 0.01f) * 100.0f + 0.5f);
    if (pitI > 10) pitI = 10;
    if (patI > 10) patI = 10;
    float rate = zoom_param_norm01(params[DROPLETS_RATE_SLOT],    DROPLETS_RATE_DEFAULT_NORM);
    float size = zoom_param_norm01(params[DROPLETS_SIZE_SLOT],    DROPLETS_SIZE_DEFAULT_NORM);
    float cont = zoom_param_norm01(params[DROPLETS_CONTOUR_SLOT], DROPLETS_CONTOUR_DEFAULT_NORM);
    float scan = zoom_param_norm01(params[DROPLETS_SCAN_SLOT],    DROPLETS_SCAN_DEFAULT_NORM);
    float rnd  = zoom_param_norm01(params[DROPLETS_RANDOM_SLOT],  DROPLETS_RANDOM_DEFAULT_NORM);
    float fbk  = zoom_param_norm01(params[DROPLETS_FEEDBK_SLOT],  DROPLETS_FEEDBK_DEFAULT_NORM);
    float mix  = zoom_param_norm01(params[DROPLETS_MIX_SLOT],     DROPLETS_MIX_DEFAULT_NORM);

    int lock = fbk > 0.995f;          /* Feedbk 100 */
    float fb = lock ? 0.0f : fbk * DR_FB_MAX;
    float wetG = mix, dryG = 1.0f - mix;

    float ri = 1.0f - rate;
    float ival = 735.0f + ri * ri * ri * 21315.0f;        /* 60 .. ~2 grains/s */
    float glen = 441.0f + size * size * 10584.0f;        /* 10 .. 250 ms */
    float pk = 0.02f + 0.96f * cont;                     /* envelope peak, 0..1 of the grain */
    float spray = dr_c01(3.0f * rnd);
    float jit = dr_c01(3.0f * rnd - 1.0f);
    float revP = dr_c01(3.0f * rnd - 2.0f) * 0.5f;
    float width = 0.4f + 0.6f * spray;
    int base0 = DR_PITCH[pitI];
    uint32_t patN = DR_PATN[patI];
    int patRand = patI >= 9;
    const signed char *pat = &DR_PAT[(uint32_t)patI * 8u];

    /* level: overlapping grains add up, ~sqrt(overlap) for unrelated ones */
    float ov = glen * dr_rcp(ival);
    if (ov < 1.0f) ov = 1.0f;
    if (ov > (float)(int)DR_NG) ov = (float)(int)DR_NG;
    float gn = 1.3f * dr_rsqrt(ov);

    /* scan: below the middle a place (distance behind the input or the frozen
     * end); above it the place travels at v x the audio's speed, wrapping
     * inside the last 2.5 s */
    {
        float sd = st->sd;
        if (scan < 0.5f) {
            float x = scan * 2.0f;
            sd = DR_DMIN + x * x * (DR_W - DR_DMIN);
        } else {
            float v = (scan - 0.5f) * 4.0f;
            sd += 8.0f * ((lock ? 0.0f : 1.0f) - v);
            if (sd > DR_W) sd -= DR_W - DR_DMIN;
            if (sd < DR_DMIN) sd += DR_W - DR_DMIN;
        }
        st->sd = sd;
    }

    uint32_t wp = st->wp, rng = st->rng, tn = st->tn;
    float lim = st->lim;

    int f;
    for (f = 0; f < 8; f++) {
        float inL = fxBuf[f];
        float inR = fxBuf[f + 8];
        float in = 0.5f * (inL + inR);

        /* --- a new grain every ival samples (jittered), if a voice is free --- */
        if (tn > 1u) tn--;
        else {
            uint32_t k, fr = DR_NG;
            for (k = 0u; k < DR_NG; k++) if (!st->g[k].left) fr = k;
            float iv = ival * (1.0f + jit * (dr_u(&rng) - 0.5f));
            tn = (uint32_t)(int)iv;
            if (fr < DR_NG) {
                Grain *q = &st->g[fr];
                uint32_t pi = st->pi;
                uint32_t pick = patRand ? (rng >> 21) & 7u : pi;
                int semi = base0 + pat[pick];
                st->pi = pi + 1u >= patN ? 0u : pi + 1u;
                if (dr_u(&rng) < jit * 0.6f) {   /* a jump: -12, -5, +7 or +12 */
                    semi += DR_JUMP[(rng >> 22) & 3u];
                }
                if (semi > 24) semi = 24;
                if (semi < -24) semi = -24;
                float ratio = dr_ratio(semi);
                float L = glen * (1.0f + jit * (dr_u(&rng) - 0.5f));
                float off = st->sd + spray * spray * DR_SPRAY * dr_u(&rng);
                int rev = dr_u(&rng) < revP;
                q->base = (wp - (uint32_t)(int)off) & DR_MASK;
                q->step = rev ? -ratio : ratio;
                q->pos = rev ? L * ratio : 0.0f;
                q->age = 0u;
                q->left = (uint32_t)(int)L;
                q->aLen = (uint32_t)(int)(pk * L);
                q->e = 0.0f;
                q->up = dr_rcp(pk * L + 1.0f);
                q->dn = dr_rcp((1.0f - pk) * L + 1.0f);
                q->amp = 1.0f - jit * 0.7f * dr_u(&rng);
                {
                    float p = 0.5f + (dr_u(&rng) - 0.5f) * width, p1 = 1.0f - p;
                    q->gl = 1.0f - p * p;
                    q->gr = 1.0f - p1 * p1;
                }
            }
        }

        /* --- play the grains --- */
        float accL = 0.0f, accR = 0.0f, accM = 0.0f;
        uint32_t nOn = (st->g[0].left != 0u) + (st->g[1].left != 0u) + (st->g[2].left != 0u) + (st->g[3].left != 0u)
                     + (st->g[4].left != 0u) + (st->g[5].left != 0u) + (st->g[6].left != 0u) + (st->g[7].left != 0u);
        if (nOn <= 6u) {
            /* a few voices: skip the idle ones */
            uint32_t k;
            for (k = 0u; k < DR_NG; k++) {
                Grain *q = &st->g[k];
                if (!q->left) continue;
                float s = dr_read(ring, q->base, wp, q->pos);
                q->pos += q->step;
                float e = q->e + (q->age < q->aLen ? q->up : -q->dn);
                e = dr_c01(e);
                q->e = e;
                s *= e * e * (3.0f - 2.0f * e) * q->amp;
                accM += s;
                accL += s * q->gl;
                accR += s * q->gr;
                q->age++;
                q->left--;
            }
        } else {
        /* Seven or eight: all eight voices straight through, no branch: an idle voice is
         * worked out too (its read is in bounds) but keeps its state and adds
         * exactly 0. With no branch the compiler can overlap the voices'
         * reads and envelopes. Each playing grain's maths, and the order the
         * grains are summed in, are exactly as before. */
#define DR_VOICE(k, sv) { \
            Grain *q = &st->g[k]; \
            int on = q->left != 0u; \
            float s = dr_read(ring, q->base, wp, q->pos); \
            float e = q->e + (q->age < q->aLen ? q->up : -q->dn); \
            e = dr_c01(e); \
            s *= e * e * (3.0f - 2.0f * e) * q->amp; \
            sv = on ? s : 0.0f; \
            if (on) { q->pos += q->step; q->e = e; q->age++; q->left--; } }
        float s0, s1, s2, s3, s4, s5, s6, s7;
        DR_VOICE(0u, s0) DR_VOICE(1u, s1) DR_VOICE(2u, s2) DR_VOICE(3u, s3)
        DR_VOICE(4u, s4) DR_VOICE(5u, s5) DR_VOICE(6u, s6) DR_VOICE(7u, s7)
#undef DR_VOICE
#define DR_ADD(k, sv) accM += sv; accL += sv * st->g[k].gl; accR += sv * st->g[k].gr;
        DR_ADD(0u, s0) DR_ADD(1u, s1) DR_ADD(2u, s2) DR_ADD(3u, s3)
        DR_ADD(4u, s4) DR_ADD(5u, s5) DR_ADD(6u, s6) DR_ADD(7u, s7)
#undef DR_ADD
        }
        float wetL = accL * gn, wetR = accR * gn;

        /* --- record: input + grains, limited; nothing while locked --- */
        if (!lock) {
            float v = in + fb * accM * gn * 0.75f;
            float av = dr_abs(v);
            lim = av > lim ? av : lim * 0.9997f;
            if (lim > 0.8f) v *= 0.8f * dr_rcp(lim);
            v = dr_soft(v);
            int q = (int)(v * 32767.0f);
            if (q > 32767) q = 32767; else if (q < -32767) q = -32767;
            ring[wp] = (int16_t)q;
            wp = (wp + 1u) & DR_MASK;
            {                         /* screen: the column's peak */
                float a = dr_abs(v);
                if (a > st->colMax) st->colMax = a;
                if (++st->colAcc >= DR_COL) {
                    float m = st->colMax;
                    uint32_t h = (uint32_t)(int)(7.0f * m * dr_rcp(m + 0.4f) + 0.5f);   /* soft: loud doesn't fill */
                    if (h > (uint32_t)DR_OV_H) h = DR_OV_H;
                    st->colPk[st->colN & 127u] = (unsigned char)h;
                    st->colN++;
                    st->colAcc = 0u;
                    st->colMax = 0.0f;
                }
            }
        }

        fxBuf[f]     = dryG * inL + wetG * wetL;
        fxBuf[f + 8] = dryG * inR + wetG * wetR;
    }

    st->wp = wp;
    st->rng = rng;
    st->tn = tn;
    st->lim = lim;

    /* --- the screen: one piece of the frame per callback --- */
    {
        unsigned int *info = _Fx_DLY_Droplets_Info;
        int visible = zv_visible(&st->z, (const uintptr_t *)ctx);
        zv_service(&st->z, info, visible);
        if (!visible) { st->step = 0u; return; }
        if (st->z.phase != ZV_IDLE) return;
        if (st->step == 0u) {         /* background, then the header values */
            uint32_t c;
            unsigned char *ln = st->ln;
            /* top half: blank but the line under the header (y 12 = block 1,
             * bit 4); bottom half (frame line, dials, knob names) from build.py */
            for (i = 0u; i < 512u; i++) {
                st->z.frame[i] = (unsigned char)((i >> 7) == 1u ? 0x10u : 0u);
                st->z.frame[512u + i] = DROP_BG[i];
            }
            for (i = 0u; i < 64u; i++) ln[i] = DROP_TAGS[i];
            for (i = 0u; i < DROP_NAME_W; i++) {
                ln[DR_CELL_PITCH + i] = DROP_PITCH_N[(uint32_t)pitI * DROP_NAME_W + i];
                ln[DR_CELL_PAT + i]   = DROP_PAT_N[(uint32_t)patI * DROP_NAME_W + i];
            }
            if (lock) for (i = 0u; i < 4u; i++) ln[DR_CELL_LOCK + i] = DROP_W_LOCK[i];
            for (i = 0u; i < 64u; i++) {  /* 3 columns a glyph, 5 rows: y 0 or 6 */
                uint32_t g = ln[i], x = 1u + 4u * (i & 31u), sh = (i >> 5) * 6u;
                if (!g) continue;
                for (c = 0u; c < 3u; c++) {
                    uint32_t v = (uint32_t)DROP_FONT[3u * g + c] << sh;
                    st->z.frame[x + c] |= (unsigned char)v;
                    st->z.frame[ZV_W + x + c] |= (unsigned char)(v >> 8);
                }
            }
            st->step = 1u;
            return;
        }
        if (st->step < 5u) {          /* waveform, 32 columns a callback, oldest left */
            uint32_t c, c0 = (st->step - 1u) * 32u;
            for (c = c0; c < c0 + 32u; c++) {
                int h = st->colPk[(st->colN + c) & 127u];
                if (h) dr_vspan(&st->z, (int)c, DR_OV_MID - h, DR_OV_MID + h, 0);
                else zv_px(&st->z, (int)c, DR_OV_MID, 1);
            }
            st->step++;
            return;
        }
        {                             /* a line per grain where it reads, the scan point */
            uint32_t k;
            for (k = 0u; k < DR_NG; k++) {
                Grain *q = &st->g[k];
                if (!q->left) continue;
                uint32_t at = (q->base + (uint32_t)(int)(q->pos + 0.5f)) & DR_MASK;
                int x = 127 - (int)((float)(int)((wp - at) & DR_MASK) * DR_COL_INV);
                int h = 3 + (int)(q->e * 5.0f);   /* taller as it opens */
                dr_vspan(&st->z, x, DR_OV_MID - h, DR_OV_MID + h, 1);
            }
            {
                int x = 127 - (int)(st->sd * DR_COL_INV);
                dr_vspan(&st->z, x, 32, 35, 0);
            }
#ifdef DROPLETS_HOST
            { extern void dr_host_frame(const unsigned char *f); dr_host_frame(st->z.frame); }
#endif
            zv_submit(&st->z);
            st->step = 0u;
        }
    }
}
