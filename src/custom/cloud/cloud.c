/*
 * cloud.c -- Cloud: a dense, diffuse ambient reverb, MS-70CDR.
 *
 * Built for density per byte, not per line: HorizonL's half-rate tank (22.05
 * kHz) with diffusion inside the loop. Pre-delay, then five allpass diffusers
 * smear the attack into a wash, then an 8-line feedback delay network in
 * which every line passes through its own allpass before the Hadamard mix.
 * Each trip round the loop multiplies the echoes instead of repeating them,
 * so the tail turns to fog within a few hundred milliseconds, with no flutter
 * even at a small Size. Diff sets how much smear: at 0 the echoes of the
 * lines can still be heard, at 100 there is no attack left at all.
 *
 * The tank runs once per pair of samples. In: a [1 2 1]/4 filter, every other
 * sample; out: linear upsampling. The dry signal is untouched.
 *
 * Damping keeps the reverb time: each line's filters are set from its own
 * gain (Jot's absorption filters), so the middle of the spectrum decays in
 * exactly the time asked and Tone and Lo only make the highs and the lows
 * decay faster or slower than that. A plain low and high cut in the loop
 * (Horizon's) shortens the whole tail through their skirts.
 *
 * 9 knobs, all 0..100:
 *   Size  (params[5])  the space: the lines x0.4 .. x2.0; turning it bends the tail's pitch
 *   Decay (params[6])  1 s .. about a minute and a half; 100 holds the tail
 *   Pre   (params[7])  pre-delay 0 .. 370 ms
 *   Diff  (params[8])  smear: the diffusers' and the loop allpasses' coefficient
 *   Mod   (params[9])  slow movement of the lines: chorus in the tail, detune at the top
 *   Tone  (params[10]) how long the highs last: 100 as long as the rest, 0 a fifth of that
 *   Lo    (params[11]) how long the lows (under ~250 Hz) last: 0 a little longer than the rest, 100 a sixth
 *   Swell (params[12]) each note blooms into the reverb over up to 2 s
 *   Mix   (params[13]) dry/wet
 *
 * Screen (zvis, MS-70CDR 2.10 and the G1on family; elsewhere the static cover
 * stays): the eight lines as eight puffs of one cloud, each puff as big as
 * the sound in its line, drifting with its line's movement; the more Diff,
 * the denser their fill. The header shows the reverb time.
 *
 * Memory: about 215 KB of the ctx[3] arena (8 x 4096-sample lines, 8 x 512
 * loop allpasses, 5 x 1024 diffusers, an 8192-sample pre-delay, all at the
 * tank rate), cleared a piece per callback.
 *
 * Safe-DSP (docs/SAFE-DSP-RULES.md): no switch, no math lib, no divide
 * (reciprocals: the C674x estimate and Newton steps), no float->unsigned
 * cast, no writable statics (state in the ctx[3] arena), magic shuttle
 * preserved. Tables are const data.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "cloud_params.h"
#include "../common/zvis.h"
#include "cloud_art.h"

#ifndef CLOUD_AUDIO_FUNC
#define CLOUD_AUDIO_FUNC Fx_DLY_Cloud
#endif

#ifdef CLOUD_HOST
typedef uintptr_t cl_word;
#else
typedef unsigned int cl_word;
#endif

#define CL_DO_PRAGMA(x) _Pragma(#x)
#define CL_EXPAND_PRAGMA(x) CL_DO_PRAGMA(x)
#define CL_CODE_SECTION(f) CL_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
#define CL_INLINE(f) CL_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(f))

CL_CODE_SECTION(CLOUD_AUDIO_FUNC)

extern unsigned int _Fx_DLY_Cloud_Info[];        /* imageInfo: the linker's symbol */

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define CL_MAGIC   0x434C4431u        /* 'CLD1' */
#define CL_VERSION 1u
#define CL_FXID    423u
#define CL_FS      44100.0f
#define CL_TFS     22050.0f           /* the tank's rate: every other sample */
#define CL_NL      8                  /* delay lines */
#define CL_LLEN    4096u              /* each line's ring (186 ms at the tank's rate) */
#define CL_LMASK   (CL_LLEN - 1u)
#define CL_PLEN    8192u              /* pre-delay ring (371 ms) */
#define CL_PMASK   (CL_PLEN - 1u)
#define CL_ALEN    512u               /* each loop allpass's ring */
#define CL_AMASK   (CL_ALEN - 1u)
#define CL_DLEN    1024u              /* each diffuser's ring */
#define CL_DMASK   (CL_DLEN - 1u)
#define CL_ND      5                  /* input diffusers */
#define CL_CLEAR   2048u              /* words cleared per callback at start */
#define CL_IN      0.35f              /* input into each line */
#define CL_OUT     0.42f              /* tank -> output */
#define CL_RTCOMP  1.00f              /* the loop's own losses (interpolation): measured time = asked */
#define CL_AX      0.0688f            /* the low shelf's crossover: 250 Hz at the tank's rate */

/* the lines at Size x1 (tank samples): 30 .. 84 ms, no common factors */
static const float CL_BASE[CL_NL] = { 661.0f, 797.0f, 937.0f, 1087.0f, 1259.0f, 1433.0f, 1627.0f, 1847.0f };
/* each line's loop allpass (tank samples, 9.6 .. 22.8 ms) */
static const uint32_t CL_LAP[CL_NL] = { 503u, 211u, 397u, 263u, 449u, 307u, 353u, 239u };
/* each line's movement, Hz at Mod 50 */
static const float CL_LFO[CL_NL] = { 0.11f, 0.17f, 0.13f, 0.23f, 0.19f, 0.29f, 0.07f, 0.31f };
/* sqrt(1 - (i/8)^2), i = 0..8: a puff's half-width per row */
static const float CL_ROUND[9] = { 1.0f, 0.9922f, 0.9682f, 0.9270f, 0.8660f, 0.7806f, 0.6614f, 0.4841f, 0.0f };

/* screen */
#define CL_CY     25
#define CL_CELL_DECAY 38u             /* header: the time's cells (line 1, cell 6) */

typedef struct ClState {
    uint32_t magic, version;
    uint32_t clearPos, ready;
    uint32_t wp, pwp, awp, dwp;       /* write positions: lines, pre-delay, loop allpasses, diffusers */
    uint32_t step;
    float dl[CL_NL];                  /* each line's length now (smoothed), samples */
    float g[CL_NL];                   /* each line's feedback gain */
    float gk[CL_NL], fk[CL_NL];       /* this callback's gains and fractional delays */
    uint32_t ok[CL_NL];               /* ... and whole-sample delays */
    float lp[CL_NL], hp[CL_NL];       /* damping states: the highs' filter, the lows' split */
    float pk[CL_NL], lk[CL_NL];       /* each line's highs pole and how much of its lows it loses */
    float ph[CL_NL];                  /* movement phases, 0..1 */
    float preD, inG;                  /* pre-delay (smoothed), input gain (the hold fades it) */
    float envF, envS, sw;             /* swell: fast and slow level, the bloom 0..1 */
    float xa, oL, oR;                 /* the last input sample; the last tank outputs (for the upsampling) */
    float lev[CL_NL];                 /* screen: each line's level */
    float rt, diff;                   /* the reverb time, for the header; Diff, for the fill */
    unsigned char ln[64];
    ZvState z;
    float dif[CL_ND][CL_DLEN];
    float lap[CL_NL][CL_ALEN];
    float pre[CL_PLEN];
    float line[CL_NL][CL_LLEN];
} ClState;

CL_INLINE(cl_abs)
static inline float cl_abs(float x) { return x < 0.0f ? -x : x; }

/* 1/x (x > 0) */
CL_INLINE(cl_rcp)
static inline float cl_rcp(float x)
{
#ifdef __TI_COMPILER_VERSION__
    float y = _rcpsp(x);
    y = y * (2.0f - x * y);
    return y * (2.0f - x * y);
#else
    union { float f; uint32_t u; } c;
    c.f = x;
    c.u = 0x7EF311C7u - c.u;
    float y = c.f;
    y = y * (2.0f - x * y);
    y = y * (2.0f - x * y);
    return y * (2.0f - x * y);
#endif
}

/* 2^x for x in -40..40 */
CL_INLINE(cl_exp2)
static inline float cl_exp2(float x)
{
    int i = (int)x;
    if ((float)i > x) i--;
    float fr = (x - (float)i) * 0.69314718f;
    union { float f; uint32_t u; } c;
    c.u = (uint32_t)(i + 127) << 23;
    return c.f * (1.0f + fr * (1.0f + fr * (0.5f + fr * (0.16666667f + fr * (0.041666668f + fr * 0.0083333338f)))));
}

/* n / 10 for 0 <= n < 4096, without a divide */
CL_INLINE(cl_div10)
static inline int cl_div10(int n) { return (n * 6554) >> 16; }

/* a row of a puff: both ends lit (its outline), the inside a dither,
 * every 2nd pixel (a checkerboard) or every 4th (sparse); every = 1 fills it */
CL_INLINE(cl_hrun)
static inline void cl_hrun(ZvState *z, int x0, int x1, int y, int every)
{
    int x;
    if ((unsigned)y >= (unsigned)ZV_H) return;
    if (x0 < 0) x0 = 0;
    if (x1 > ZV_W - 1) x1 = ZV_W - 1;
    if (x1 < x0) return;
    {
        unsigned char m = (unsigned char)(1u << (y & 7));
        unsigned char *row = &z->frame[(y >> 3) * ZV_W];
        row[x0] |= m; row[x1] |= m;
        if (every == 4 && (y & 1)) return;    /* sparse: dots on every other row */
        x = x0 + ((x0 + (every == 4 ? (y & 2) : y)) & (every - 1));    /* the dither moves along by row */
        for (; x <= x1; x += every) row[x] |= m;
    }
}

/* soft clip for the wet signal: x (27 + x^2) / (27 + 9 x^2), flat past +-3;
 * below about 0.3 it is all but transparent */
CL_INLINE(cl_sat)
static inline float cl_sat(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) * cl_rcp(27.0f + 9.0f * x2);
}

/* sin(2 pi p) for p in 0..1, parabolic with a correction */
CL_INLINE(cl_sin1)
static inline float cl_sin1(float p)
{
    float x = p < 0.5f ? p * 4.0f - 1.0f : 3.0f - p * 4.0f;   /* -1..1..-1 triangle */
    float y = x * (2.0f - cl_abs(x));                          /* parabola */
    return -(y * (0.775f + 0.225f * cl_abs(y)));
}

void CLOUD_AUDIO_FUNC(cl_word *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    cl_word *magicSrc = ZDL_PTR(cl_word *, ctx[12]);
    cl_word *magicDst = ZDL_PTR(cl_word *, *(cl_word *)ZDL_PTR(cl_word *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile cl_word *desc = ZDL_PTR(volatile cl_word *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u) return;
    if (base + sizeof(ClState) > end) return;

    ClState *st = (ClState *)base;
    int k;

    if (st->magic != CL_MAGIC || st->version != CL_VERSION) {
        st->magic = CL_MAGIC;
        st->version = CL_VERSION;
        st->clearPos = 0u; st->ready = 0u;
        st->wp = st->pwp = st->awp = st->dwp = 0u;
        st->step = 0u;
        for (k = 0; k < CL_NL; k++) {
            st->dl[k] = CL_BASE[k]; st->lp[k] = st->hp[k] = 0.0f; st->g[k] = 0.5f;
            st->pk[k] = 0.0f; st->lk[k] = 0.0f;
            st->ph[k] = (float)k * 0.125f; st->lev[k] = 0.0f;
        }
        st->preD = 0.0f; st->inG = 1.0f;
        st->envF = st->envS = 0.0f; st->sw = 1.0f;
        st->xa = st->oL = st->oR = 0.0f;
        st->rt = 1.0f; st->diff = 0.7f;
        zv_init(&st->z, CL_FXID, _Fx_DLY_Cloud_Info);
        return;
    }
    if (!st->ready) {                                 /* clear the big buffers, a piece a callback */
        float *w = &st->dif[0][0];
        uint32_t total = (uint32_t)((sizeof(st->dif) + sizeof(st->lap) + sizeof(st->pre) + sizeof(st->line)) >> 2);
        uint32_t i, e = st->clearPos + CL_CLEAR;
        if (e > total) e = total;
        for (i = st->clearPos; i < e; i++) w[i] = 0.0f;
        st->clearPos = e;
        if (e >= total) st->ready = 1u;
        return;
    }

    float size  = zoom_param_norm01(params[CLOUD_SIZE_SLOT],  CLOUD_SIZE_DEFAULT_NORM);
    float decay = zoom_param_norm01(params[CLOUD_DECAY_SLOT], CLOUD_DECAY_DEFAULT_NORM);
    float pre   = zoom_param_norm01(params[CLOUD_PRE_SLOT],   CLOUD_PRE_DEFAULT_NORM);
    float diff  = zoom_param_norm01(params[CLOUD_DIFF_SLOT],  CLOUD_DIFF_DEFAULT_NORM);
    float mod   = zoom_param_norm01(params[CLOUD_MOD_SLOT],   CLOUD_MOD_DEFAULT_NORM);
    float tone  = zoom_param_norm01(params[CLOUD_TONE_SLOT],  CLOUD_TONE_DEFAULT_NORM);
    float lo    = zoom_param_norm01(params[CLOUD_LO_SLOT],    CLOUD_LO_DEFAULT_NORM);
    float swell = zoom_param_norm01(params[CLOUD_SWELL_SLOT], CLOUD_SWELL_DEFAULT_NORM);
    float mix   = zoom_param_norm01(params[CLOUD_MIX_SLOT],   CLOUD_MIX_DEFAULT_NORM);
    int hold = decay > 0.995f;

    /* --- per callback: lengths, gains, damping, movement --- */
    float scale = 0.4f * cl_exp2(size * 2.3219f);             /* x0.4 .. x2.0 (the longest line, 168 ms, fits its 4096 samples) */
    float rt = cl_exp2(decay * 6.4919f);                       /* 1 s .. 90 s */
    float irt = cl_rcp(rt * CL_TFS * CL_RTCOMP);
    float depth = mod * mod * 24.0f;                           /* tank samples */
    float rate = 0.3f + 1.4f * mod;
    float gD = 0.4f + 0.35f * diff;                            /* the diffusers: a few echoes .. a wash */
    float gL = 0.15f + 0.5f * diff;                            /* the loop allpasses */
    /* the highs' and the lows' reverb time as a ratio of the rest's */
    float aHF = cl_exp2((tone - 1.0f) * 2.3219f);              /* 0.2 .. 1 */
    float hfK = 1.7269f * (cl_rcp(aHF * aHF) - 1.0f);          /* Jot: pole = ln10/4 log10(g) (1 - 1/a^2) */
    float lfE = cl_rcp(1.25f * cl_exp2(-lo * 3.059f)) - 1.0f;  /* lows: ratio 1.25 .. 0.15, as g^(1/ratio - 1) */
    /* per-line values for the callback live in the arena: the pedal's audio
     * stack is small (keep the frame near the effects proven on it) */
    float *gain = st->gk, *frac = st->fk;
    uint32_t *off = st->ok;
    for (k = 0; k < CL_NL; k++) {
        float tgt = CL_BASE[k] * scale;
        st->dl[k] += (tgt - st->dl[k]) * 0.0015f;              /* ~120 ms: a tape-like stretch */
        st->ph[k] += CL_LFO[k] * rate * (8.0f / CL_FS);
        if (st->ph[k] >= 1.0f) st->ph[k] -= 1.0f;
        float d = st->dl[k] + depth * cl_sin1(st->ph[k]) + depth + 2.0f;
        int di = (int)d;
        frac[k] = d - (float)di;
        off[k] = (uint32_t)di;
        if (((st->wp >> 3) & 3u) == ((uint32_t)k & 3u) || hold) {        /* each gain every 4th callback */
            float L = st->dl[k] + (float)CL_LAP[k];                      /* the loop's length, with its allpass */
            if (hold) { st->g[k] = 1.0f; st->pk[k] = 0.0f; st->lk[k] = 0.002f; }   /* still a little low loss: no offset builds up */
            else {
                float e = -9.9658f * L * irt;                            /* log2 g: 10^(-3 L / (RT fs)) */
                float p = hfK * L * irt;
                st->g[k] = cl_exp2(e);
                st->pk[k] = p > 0.85f ? 0.85f : p;
                st->lk[k] = 1.0f - cl_exp2(e * lfE);                     /* < 0: the lows ring longer */
            }
        }
        gain[k] = st->g[k];
    }
    st->rt = rt; st->diff = diff;
    float preT = pre * 0.37f * CL_TFS;
    st->preD += (preT - st->preD) * 0.01f;
    uint32_t preI = (uint32_t)(int)st->preD + 1u;
    float swK = swell > 0.01f ? cl_rcp(swell * 2.0f * CL_TFS) : 1.0f;
    float inT = hold ? 0.0f : 1.0f;
    float wetG = mix, dryG = 1.0f - mix;

    uint32_t wp = st->wp, pwp = st->pwp, awp = st->awp, dwp = st->dwp;
    float envF = st->envF, envS = st->envS, sw = st->sw, inG = st->inG;
    float xa = st->xa, oL = st->oL, oR = st->oR;
    float lpS[CL_NL], hpS[CL_NL];                 /* the damping states, local in the loop (64 bytes of stack) */
    for (k = 0; k < CL_NL; k++) { lpS[k] = st->lp[k]; hpS[k] = st->hp[k]; }
    int fr;
    /* The tank runs at half rate: one tick per pair of samples. In: a
     * [1 2 1] / 4 filter (zero at 22 kHz) then every other sample. Out: the
     * tick's value and its average with the last one, linear upsampling. */
    for (fr = 0; fr < 8; fr += 2) {
        float x0 = 0.5f * (fxBuf[fr] + fxBuf[fr + 8]), x1 = 0.5f * (fxBuf[fr + 1] + fxBuf[fr + 9]);
        float x = 0.25f * (xa + x1) + 0.5f * x0;
        xa = x1;

        /* swell: a note's attack (fast level jumping over slow) restarts the bloom */
        {
            float a = cl_abs(x);
            envF += (a - envF) * (a > envF ? 0.0975f : 0.003f);
            envS += (envF - envS) * 0.0006f;
            if (swell > 0.01f) {
                if (envF > 2.0f * envS + 0.01f && sw > 0.6f) sw = 0.0f;
                sw += (1.0f - sw) * swK;
            } else sw = 1.0f;
        }
        inG += (inT - inG) * 0.001f;

        /* pre-delay */
        st->pre[pwp] = x * inG * sw;
        x = st->pre[(pwp - preI) & CL_PMASK];
        pwp = (pwp + 1u) & CL_PMASK;

        /* five diffusers (serial: each feeds the next), 11 .. 43 ms, 130 ms in all:
         * the attack spreads into a bloom before it reaches the lines */
        {
            float b0 = st->dif[0][(dwp - 241u) & CL_DMASK], b1 = st->dif[1][(dwp - 383u) & CL_DMASK];
            float b2 = st->dif[2][(dwp - 557u) & CL_DMASK], b3 = st->dif[3][(dwp - 739u) & CL_DMASK];
            float b4 = st->dif[4][(dwp - 947u) & CL_DMASK];
            float v0 = x + gD * b0;  x = b0 - gD * v0;  st->dif[0][dwp] = v0;
            float v1 = x + gD * b1;  x = b1 - gD * v1;  st->dif[1][dwp] = v1;
            float v2 = x + gD * b2;  x = b2 - gD * v2;  st->dif[2][dwp] = v2;
            float v3 = x + gD * b3;  x = b3 - gD * v3;  st->dif[3][dwp] = v3;
            float v4 = x + gD * b4;  x = b4 - gD * v4;  st->dif[4][dwp] = v4;
            dwp = (dwp + 1u) & CL_DMASK;
        }

        /* the eight lines: read (moving, fractional), damp, then each line's allpass */
        float o[CL_NL];
#pragma UNROLL(8)
        for (k = 0; k < CL_NL; k++) {
            uint32_t i0 = (wp - off[k]) & CL_LMASK;
            float a = st->line[k][i0], b = st->line[k][(i0 - 1u) & CL_LMASK];
            float y = a + (b - a) * frac[k];
            lpS[k] = y + (lpS[k] - y) * st->pk[k];                /* highs: DC gain 1, the pole from the line's gain */
            hpS[k] += (lpS[k] - hpS[k]) * CL_AX;                  /* lows: under 250 Hz */
            y = lpS[k] - st->lk[k] * hpS[k];
            float c = st->lap[k][(awp - CL_LAP[k]) & CL_AMASK];
            float v = y + gL * c;
            st->lap[k][awp] = v;
            o[k] = c - gL * v;
        }
        awp = (awp + 1u) & CL_AMASK;
        /* Hadamard 8 (orthogonal: x 1/sqrt 8) */
        float a0 = o[0] + o[1], a1 = o[0] - o[1], a2 = o[2] + o[3], a3 = o[2] - o[3];
        float a4 = o[4] + o[5], a5 = o[4] - o[5], a6 = o[6] + o[7], a7 = o[6] - o[7];
        float b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
        float b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;
        float m[CL_NL];
        m[0] = b0 + b4; m[1] = b1 + b5; m[2] = b2 + b6; m[3] = b3 + b7;
        m[4] = b0 - b4; m[5] = b1 - b5; m[6] = b2 - b6; m[7] = b3 - b7;
        float xi = x * CL_IN;
#pragma UNROLL(8)
        for (k = 0; k < CL_NL; k++)
            st->line[k][wp] = gain[k] * m[k] * 0.35355339f + ((k & 1) ? -xi : xi);
        wp = (wp + 1u) & CL_LMASK;

        /* out: two orthogonal rows of the Hadamard, left and right uncorrelated */
        float wL = (o[0] + o[1] + o[2] + o[3] - o[4] - o[5] - o[6] - o[7]) * CL_OUT;
        float wR = (o[0] - o[1] + o[2] - o[3] + o[4] - o[5] + o[6] - o[7]) * CL_OUT;
        fxBuf[fr]      = dryG * fxBuf[fr]      + wetG * cl_sat(0.5f * (oL + wL));
        fxBuf[fr + 8]  = dryG * fxBuf[fr + 8]  + wetG * cl_sat(0.5f * (oR + wR));
        fxBuf[fr + 1]  = dryG * fxBuf[fr + 1]  + wetG * cl_sat(wL);
        fxBuf[fr + 9]  = dryG * fxBuf[fr + 9]  + wetG * cl_sat(wR);
        oL = wL; oR = wR;
        if (fr == 6) for (k = 0; k < CL_NL; k++) {      /* the screen: once a callback */
            float pk = cl_abs(o[k]);
            st->lev[k] = pk > st->lev[k] ? pk : st->lev[k] * 0.99f;
        }
    }
    st->wp = wp; st->pwp = pwp; st->awp = awp; st->dwp = dwp;
    st->envF = envF; st->envS = envS; st->sw = sw; st->inG = inG;
    st->xa = xa; st->oL = oL; st->oR = oR;
    for (k = 0; k < CL_NL; k++) {
        float l = lpS[k], h = hpS[k];
        if (l < 1e-20f && l > -1e-20f) l = 0.0f;               /* no denormals in the tail */
        if (h < 1e-20f && h > -1e-20f) h = 0.0f;
        st->lp[k] = l; st->hp[k] = h;
    }

    /* --- the screen: one small piece of the frame per callback --- */
    {
        unsigned int *info = _Fx_DLY_Cloud_Info;
        int visible = zv_visible(&st->z, (const uintptr_t *)ctx);
        zv_service(&st->z, info, visible);
        if (!visible) { st->step = 0u; return; }
        if (st->z.phase != ZV_IDLE) return;
        if (st->step == 0u) {                         /* background */
            uint32_t i;
            for (i = 0u; i < 512u; i++) {
                st->z.frame[i] = (unsigned char)((i >> 7) == 1u ? 0x10u : 0u);
                st->z.frame[512u + i] = CLD_BG[i];
            }
            st->step = 1u;
            return;
        }
        if (st->step == 1u) {                         /* the header: the reverb time */
            uint32_t i, c;
            unsigned char *ln = st->ln;
            for (i = 0u; i < 64u; i++) ln[i] = CLD_TAGS[i];
            if (hold) {
                for (i = 0u; i < 4u; i++) ln[CL_CELL_DECAY + i] = CLD_HOLD[i];
            } else {                                  /* "12.4S" / " 1.0S" / "90S" */
                int t10 = (int)(st->rt * 10.0f + 0.5f);
                int s = t10 >= 100 ? 0 : 1;           /* tenths below 10 s */
                int whole = s ? cl_div10(t10) : cl_div10(t10 + 5);
                int tenth = t10 - cl_div10(t10) * 10;
                uint32_t cpos = CL_CELL_DECAY;
                if (whole >= 10) ln[cpos++] = (unsigned char)(1 + cl_div10(whole));
                ln[cpos++] = (unsigned char)(1 + whole - cl_div10(whole) * 10);
                if (s) { ln[cpos++] = CLD_DOT; ln[cpos++] = (unsigned char)(1 + tenth); }
                ln[cpos] = CLD_S;
            }
            for (i = 0u; i < 64u; i++) {
                uint32_t gl = ln[i], x = 1u + 4u * (i & 31u), sh = (i >> 5) * 6u;
                if (!gl) continue;
                for (c = 0u; c < 3u; c++) {
                    uint32_t v = (uint32_t)CLD_FONT[3u * gl + c] << sh;
                    st->z.frame[x + c] |= (unsigned char)v;
                    st->z.frame[ZV_W + x + c] |= (unsigned char)(v >> 8);
                }
            }
            st->step = 2u;
            return;
        }
        {                                             /* the puffs, one a callback (steps 2..9): a big one is ~3,000 emulated cycles */
            float m = 0.0005f;
            int k0 = (int)(st->step - 2u);
            int every = st->diff > 0.5f ? 2 : 4;      /* little Diff: a sparser, grainier fill */
            for (k = 0; k < CL_NL; k++) if (st->lev[k] > m) m = st->lev[k];
            float sc = cl_rcp(m);
            for (k = k0; k < k0 + 1; k++) {
                float lv = st->lev[k] * sc;                         /* 0..1 */
                float rx = 5.0f + 8.0f * lv;                        /* 5 .. 13 px */
                int ry = 2 + (int)(5.0f * lv);                      /* 2 .. 7 rows */
                float iry = cl_rcp((float)ry + 0.5f) * 8.0f;
                int cx = 15 + 14 * k + (int)(3.0f * cl_sin1(st->ph[k]));
                int cy = CL_CY + ((k & 1) ? 3 : -2) + (int)(1.5f * cl_sin1(st->ph[k] + 0.25f >= 1.0f ? st->ph[k] - 0.75f : st->ph[k] + 0.25f));
                int dy;
                for (dy = -ry; dy <= ry; dy++) {
                    int ri = (int)((float)(dy < 0 ? -dy : dy) * iry);
                    int hw = (int)(rx * CL_ROUND[ri > 8 ? 8 : ri]);
                    int edge = dy == -ry || dy == ry;
                    cl_hrun(&st->z, cx - hw, cx + hw, cy + dy, edge ? 1 : every);
                }
            }
            if (st->step < 9u) { st->step++; return; }
#ifdef CLOUD_HOST
            { extern void cl_host_frame(const unsigned char *f); cl_host_frame(st->z.frame); }
#endif
            zv_submit(&st->z);
            st->step = 0u;
        }
    }
}
