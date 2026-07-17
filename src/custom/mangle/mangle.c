/*
 * mangle.c -- Mangle: a delay whose repeats mutate every pass, MS-70CDR.
 *
 * A feedback delay where the feedback path runs a transform, so each repeat
 * is more processed than the last. A Mode knob picks the character; Mangle
 * sets the intensity (Mangle=0 = a clean delay). 5 knobs:
 *   Time     (params[5]) - delay length (~23 ms .. 1.3 s)
 *   Feedback (params[6]) - repeat count / regeneration
 *   Mangle   (params[7]) - transform intensity (0 = clean delay)
 *   Mode     (params[8]) - 0 pitch down · 1 pitch up · 2 tremolo · 3 crush
 *   Mix      (params[9]) - dry/wet
 *
 * Modes (applied to the wet before it feeds back, so they compound):
 *   pitch  - granular pitch shift; repeats spiral down or up an octave
 *   trem   - amplitude LFO; repeats pulse harder as they regen
 *   crush  - sample-rate reduce + bitcrush + darkening low-pass + soft sat;
 *            echoes erode into lo-fi dust
 *
 * Safe-DSP: no math lib (polynomial sine, cubic clip), no runtime divide
 * (crush quantiser uses power-of-two mul/inv chosen by a switch; reciprocals
 * are compile-time), no static arrays. Mono ring in the ctx[3] arena
 * (validated + lazily cleared). ctx[11]/ctx[12] magic shuttle preserved.
 * The feedback write is soft-clipped (unity slope) so no mode can run away.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "mangle_params.h"

#ifndef MANGLE_AUDIO_FUNC
#define MANGLE_AUDIO_FUNC Fx_DLY_Mangle
#endif

#define MANGLE_DO_PRAGMA(x) _Pragma(#x)
#define MANGLE_EXPAND_PRAGMA(x) MANGLE_DO_PRAGMA(x)
#define MANGLE_CODE_SECTION(f) MANGLE_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

MANGLE_CODE_SECTION(MANGLE_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define MG_MAGIC   0x4D414E47u        /* 'MANG' */
#define MG_VERSION 1u

#define MG_BUF        65536u          /* mono ring, ~1.49 s (power of 2) */
#define MG_MASK       (MG_BUF - 1u)
#define MG_CLEAR_STEP 4096u
#define MG_DMIN       1024.0f         /* ~23 ms */
#define MG_DMAX       58000.0f        /* ~1.31 s */
#define MG_GRAIN      4096.0f         /* pitch grain length */
#define MG_HALF       2048.0f
#define MG_INV_HALF   (1.0f / 2048.0f)
#define MG_TWO_PI     6.2831853f
#define MG_FB_MAX     0.92f

typedef struct MangleState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearIndex;

    uint32_t writePos;
    float grainPhase;                 /* pitch grain phase 0..MG_GRAIN */
    float lfo;                        /* tremolo phase */
    float shReg;                      /* crush sample-hold register */
    uint32_t shCnt;                   /* crush sample-hold counter */
    float lpZ;                        /* crush low-pass state */
    uint32_t pad;
} MangleState;

static inline float mg_abs(float x) { return x < 0.0f ? -x : x; }

static inline float mg_sin(float x)
{
    const float twoPi = 6.28318530718f, pi = 3.14159265359f, inv = 0.15915494309f;
    x = x - twoPi * (float)((int)(x * inv));
    if (x < -pi) x += twoPi;
    if (x > pi) x -= twoPi;
    float y = 1.2732395447f * x - 0.4052847346f * x * mg_abs(x);
    return y + 0.225f * (y * mg_abs(y) - y);
}

/* unity-slope soft clip: no small-signal gain (safe inside a feedback loop) */
static inline float mg_soft(float x)
{
    if (x > 1.5f) return 1.0f;
    if (x < -1.5f) return -1.0f;
    return x - (x * x * x) * 0.14814815f;   /* x - x^3/6.75 */
}

/* linear read, d samples behind the write head */
static inline float mg_read(const float *buf, uint32_t wp, float d)
{
    int di = (int)d;
    float fr = d - (float)di;
    uint32_t i0 = (wp - (uint32_t)di) & MG_MASK;
    uint32_t i1 = (i0 - 1u) & MG_MASK;
    return buf[i0] * (1.0f - fr) + buf[i1] * fr;
}

/* crush quantiser step from a "bits" value, as power-of-two mul + inv
 * (no runtime divide). bits 2..8 -> levels 4..256. */
static inline void mg_crush_scale(int bits, float *mul, float *inv)
{
    switch (bits) {
    case 2: *mul = 4.0f;   *inv = 0.25f;        break;
    case 3: *mul = 8.0f;   *inv = 0.125f;       break;
    case 4: *mul = 16.0f;  *inv = 0.0625f;      break;
    case 5: *mul = 32.0f;  *inv = 0.03125f;     break;
    case 6: *mul = 64.0f;  *inv = 0.015625f;    break;
    case 7: *mul = 128.0f; *inv = 0.0078125f;   break;
    default:*mul = 256.0f; *inv = 0.00390625f;  break;
    }
}

void MANGLE_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile unsigned int *desc = ZDL_PTR(volatile unsigned int *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u) return;

    uintptr_t bufBase = (base + sizeof(MangleState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)MG_BUF * 4u > end) return;

    MangleState *st = (MangleState *)base;
    float *buf = (float *)bufBase;

    if (st->magic != MG_MAGIC || st->version != MG_VERSION) {
        st->magic = MG_MAGIC;
        st->version = MG_VERSION;
        st->initialized = 0u;
        st->clearIndex = 0u;
        st->writePos = 0u;
        st->grainPhase = 0.0f;
        st->lfo = 0.0f;
        st->shReg = 0.0f;
        st->shCnt = 0u;
        st->lpZ = 0.0f;
    }

    if (!st->initialized) {
        uint32_t e = st->clearIndex + MG_CLEAR_STEP;
        if (e > MG_BUF) e = MG_BUF;
        uint32_t i;
        for (i = st->clearIndex; i < e; i++) buf[i] = 0.0f;
        st->clearIndex = e;
        if (e >= MG_BUF) st->initialized = 1u;
        return;
    }

    float time   = zoom_param_norm01(params[MANGLE_TIME_SLOT], MANGLE_TIME_DEFAULT_NORM);
    float fbk    = zoom_param_norm01(params[MANGLE_FEEDBK_SLOT], MANGLE_FEEDBK_DEFAULT_NORM);
    float mangle = zoom_param_norm01(params[MANGLE_MANGLE_SLOT], MANGLE_MANGLE_DEFAULT_NORM);
    float modeN  = zoom_param_norm01(params[MANGLE_MODE_SLOT], MANGLE_MODE_DEFAULT_NORM);
    float mix    = zoom_param_norm01(params[MANGLE_MIX_SLOT], MANGLE_MIX_DEFAULT_NORM);

    int mode = (int)(modeN * 3.999f);            /* 0..3 */
    if (mode < 0) mode = 0; else if (mode > 3) mode = 3;

    float delay = MG_DMIN + time * (MG_DMAX - MG_DMIN);
    float fb = fbk * MG_FB_MAX;
    float wet = mix, dry = 1.0f - mix;

    /* per-mode setup */
    float ratio = 1.0f;                           /* pitch modes */
    if (mode == 0) ratio = 1.0f - 0.5f * mangle;  /* down to 0.5x (oct down) */
    else if (mode == 1) ratio = 1.0f + mangle;    /* up to 2.0x (oct up) */
    float gInc = 1.0f - ratio;                    /* grain phase increment */

    float lfoInc = (2.0f + mangle * 8.0f) * (MG_TWO_PI / 44100.0f);   /* trem 2..10 Hz */
    float tremDepth = mangle;

    int bits = 8 - (int)(mangle * 6.0f);          /* crush 8..2 bits */
    if (bits < 2) bits = 2;
    float cMul = 256.0f, cInv = 0.00390625f;
    mg_crush_scale(bits, &cMul, &cInv);
    float lpCoef = 0.6f - mangle * 0.55f;         /* crush low-pass darkens */
    uint32_t shN = 1u + (uint32_t)(int)(mangle * 15.0f);   /* sample-hold 1..16x
        (via signed int: a direct float->unsigned cast pulls in __c6xabi_fixfu,
         which the loader can't resolve -> branch to 0 -> freeze) */

    uint32_t wp = st->writePos;
    float gph = st->grainPhase, lfo = st->lfo, lpZ = st->lpZ, shReg = st->shReg;
    uint32_t shCnt = st->shCnt;

    int f;
    for (f = 0; f < 8; f++) {
        float inL = fxBuf[f];
        float inR = fxBuf[f + 8];
        float in = 0.5f * (inL + inR);

        float m;
        if (mode <= 1) {
            /* granular pitch read at the delay tap */
            float d1 = delay + gph;
            float g2 = gph + MG_HALF;
            if (g2 >= MG_GRAIN) g2 -= MG_GRAIN;
            float d2 = delay + g2;
            float e1 = (MG_HALF - mg_abs(MG_HALF - gph)) * MG_INV_HALF;
            float e2 = (MG_HALF - mg_abs(MG_HALF - g2)) * MG_INV_HALF;
            m = mg_read(buf, wp, d1) * e1 + mg_read(buf, wp, d2) * e2;
            gph += gInc;
            if (gph >= MG_GRAIN) gph -= MG_GRAIN;
            else if (gph < 0.0f) gph += MG_GRAIN;
        } else {
            float base = mg_read(buf, wp, delay);
            if (mode == 2) {
                float trem = 1.0f - tremDepth * (0.5f - 0.5f * mg_sin(lfo));
                lfo += lfoInc;
                if (lfo > MG_TWO_PI) lfo -= MG_TWO_PI;
                m = base * trem;
            } else {
                /* crush: sample-hold -> bit quantise -> low-pass -> sat */
                shCnt++;
                if (shCnt >= shN) { shReg = base; shCnt = 0u; }
                float q = (float)((int)(shReg * cMul + (shReg >= 0.0f ? 0.5f : -0.5f))) * cInv;
                lpZ += lpCoef * (q - lpZ);
                m = mg_soft(lpZ * 1.1f);
            }
        }

        buf[wp] = mg_soft(in + fb * m);
        fxBuf[f]     = dry * inL + wet * m;
        fxBuf[f + 8] = dry * inR + wet * m;

        wp = (wp + 1u) & MG_MASK;
    }

    st->writePos = wp;
    st->grainPhase = gph;
    st->lfo = lfo;
    st->lpZ = lpZ;
    st->shReg = shReg;
    st->shCnt = shCnt;
}
