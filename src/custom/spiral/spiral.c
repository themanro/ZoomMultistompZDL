/*
 * spiral.c -- Spiral: a delay whose repeats climb (or sink), MS-70CDR.
 *
 * Pedal port of tools/audio_preview/renderers/spiral.py. A grain-re-triggered
 * tap is read at `ratio` INSIDE the feedback loop, so one reader both delays
 * and pitch-shifts (Hydra's trick) and the shift COMPOUNDS: repeat 1 is one
 * step away, repeat 2 two steps, an endless staircase fading with the feedback.
 *
 * Two controls sit on that ratio, covering both readings of "the delay rises":
 *   Rise  a constant ratio != 1 -> discrete per-repeat staircase. Bipolar,
 *         unity at centre; left of centre the repeats sink instead.
 *   Glide+Span  an onset-triggered ramp on the same ratio. A note resets the
 *         ramp to ZERO and it climbs toward Glide's depth over Span, so the
 *         repeats begin at pitch and drift away over a while -- a riser, NOT a
 *         swoop that settles back.
 * Glide 0 = pure staircase; Rise centred + Glide up = pure glide, no net
 * transposition; both = a climb that also swoops.
 *
 * 7 knobs:
 *   Time   (params[5])  delay length, squared, 4096..52920 samples (~93ms..1.2s)
 *   Feedbk (params[6])  repeats, scaled to 0.965 and soft-clipped in the loop
 *   Rise   (params[7])  per-repeat ratio: 0 = oct down, 50 = unity, 100 = oct up
 *   Glide  (params[8])  depth of the onset ramp on that ratio
 *   Span   (params[9])  how long the rise takes, ~0.19s .. ~8s
 *   Tone   (params[10]) low-pass inside the loop
 *   Mix    (params[11]) dry/wet
 *
 * Safe-DSP: no math lib -- the ratio map is piecewise LINEAR rather than
 * 2^(semis/12), and Span's one-pole coefficient is a CUBIC in the knob instead
 * of 1-exp(-1/(t*sr)), because both exp2 and the divide would pull in helper
 * calls. No runtime divide (grain length is a fixed power of two so 1/G is a
 * literal), no modulo (power-of-two ring mask), no switch (jump tables are
 * unreachable in a ZDL and hard-freeze the DSP), no static arrays, no
 * float->unsigned casts. Ring lives in the ctx[3] arena, cleared once at init.
 * ctx[11]/ctx[12] magic shuttle preserved.
 *
 * Tap-overtake guard: the tap starts D behind the write head and advances at
 * `ratio`, so over one grain it gains G*(ratio-1) on the writer. With G=1024,
 * ratio clamped to 4.0 and D >= 4096, the worst case gain is 3072 < 4096 -- the
 * tap can never read past the write head into unwritten samples.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "spiral_params.h"

#ifndef SPIRAL_AUDIO_FUNC
#define SPIRAL_AUDIO_FUNC Fx_DLY_Spiral
#endif

#define SPIRAL_DO_PRAGMA(x) _Pragma(#x)
#define SPIRAL_EXPAND_PRAGMA(x) SPIRAL_DO_PRAGMA(x)
#define SPIRAL_CODE_SECTION(f) SPIRAL_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

SPIRAL_CODE_SECTION(SPIRAL_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define SP_MAGIC   0x53505241u   /* 'SPRA' */
#define SP_VERSION 1u

#define SP_BUF       131072u     /* 512 KB, ~2.97 s */
#define SP_MASK      (SP_BUF - 1u)
#define SP_D_MIN     4096        /* ~93 ms; also the tap-overtake guard floor */
#define SP_D_SPAN    48824       /* max delay 52920 = ~1.2 s */
#define SP_GRAIN     1024        /* fixed power of two ... */
#define SP_INV_GRAIN 9.765625e-4f /* ... so 1/G is a literal, not a divide */
#define SP_RATIO_MIN 0.25f
#define SP_RATIO_MAX 4.0f
#define SP_ENV_FAST  0.02f
#define SP_ENV_SLOW  0.0008f
#define SP_ONSET_MUL 1.7f
#define SP_ONSET_ADD 0.008f
#define SP_COOLDOWN  2205        /* ~50 ms between onset triggers */
#define SP_K_MAX     1.13e-4f    /* Span short end (~0.19 s) */
#define SP_K_MIN     2.8e-6f     /* Span long end  (~8.1 s) */
#define SP_DENORM    1.0e-18f

typedef struct SpiralState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t writePos;
    float tap;
    int32_t grainT;
    float lp;
    float envFast, envSlow, ramp;
    int32_t cool;
} SpiralState;

static inline float sp_soft(float x)
{
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return 1.5f * x - 0.5f * x * x * x;
}

static inline float sp_abs(float x) { return x < 0.0f ? -x : x; }

static inline float sp_read(const float *buf, float pos)
{
    int32_t i0 = (int32_t)pos;
    float fr = pos - (float)i0;
    uint32_t a = ((uint32_t)i0) & SP_MASK;
    uint32_t b = (a + 1u) & SP_MASK;
    return buf[a] * (1.0f - fr) + buf[b] * fr;
}

static inline float sp_wrap(float pos)
{
    if (pos >= (float)SP_BUF) pos -= (float)SP_BUF;
    if (pos < 0.0f) pos += (float)SP_BUF;
    return pos;
}

void SPIRAL_AUDIO_FUNC(unsigned int *ctx)
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

    uintptr_t bufBase = (base + sizeof(SpiralState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)SP_BUF * 4u > end) return;

    SpiralState *st = (SpiralState *)base;
    float *buf = (float *)bufBase;

    if (st->magic != SP_MAGIC || st->version != SP_VERSION || !st->initialized) {
        st->magic = SP_MAGIC;
        st->version = SP_VERSION;
        st->writePos = 0u;
        st->tap = 0.0f;
        st->grainT = 0;
        st->lp = 0.0f;
        st->envFast = st->envSlow = 0.0f;
        st->ramp = 1.0f;
        st->cool = 0;
        uint32_t i;
        for (i = 0u; i < SP_BUF; i++) buf[i] = 0.0f;
        st->initialized = 1u;
    }

    float tn = zoom_param_norm01(params[SPIRAL_TIME_SLOT],   SPIRAL_TIME_DEFAULT_NORM);
    float fn = zoom_param_norm01(params[SPIRAL_FEEDBK_SLOT], SPIRAL_FEEDBK_DEFAULT_NORM);
    float rn = zoom_param_norm01(params[SPIRAL_RISE_SLOT],   SPIRAL_RISE_DEFAULT_NORM);
    float gn = zoom_param_norm01(params[SPIRAL_GLIDE_SLOT],  SPIRAL_GLIDE_DEFAULT_NORM);
    float sn = zoom_param_norm01(params[SPIRAL_SPAN_SLOT],   SPIRAL_SPAN_DEFAULT_NORM);
    float on = zoom_param_norm01(params[SPIRAL_TONE_SLOT],   SPIRAL_TONE_DEFAULT_NORM);
    float mix = zoom_param_norm01(params[SPIRAL_MIX_SLOT],   SPIRAL_MIX_DEFAULT_NORM);

    int32_t D = SP_D_MIN + (int32_t)(tn * tn * (float)SP_D_SPAN);
    if (D < SP_D_MIN) D = SP_D_MIN;
    float fb = fn * 0.965f;

    /* piecewise linear, unity at centre -- no exp2 */
    float baseRatio;
    if (rn < 0.5f) baseRatio = 0.5f + rn;
    else           baseRatio = 1.0f + (rn - 0.5f) * 2.0f;
    float glideDir = (rn >= 0.5f) ? 1.0f : -1.0f;
    float glideAmt = gn * 0.6f;

    /* Span -> one-pole coefficient. Cubic in the knob so the long end gets the
     * resolution, and it avoids both exp() and a divide. */
    float inv = 1.0f - sn;
    float rampK = SP_K_MIN + SP_K_MAX * inv * inv * inv;

    float lpCoef = 0.03f + on * 0.5f;

    uint32_t wp  = st->writePos;
    float tap    = st->tap;
    int32_t gt   = st->grainT;
    float lp     = st->lp;
    float envF   = st->envFast;
    float envS   = st->envSlow;
    float ramp   = st->ramp;
    int32_t cool = st->cool;

    int i;
    for (i = 0; i < 8; i++) {
        float dry = 0.5f * (fxBuf[i] + fxBuf[i + 8]);

        float ax = sp_abs(dry);
        envF += SP_ENV_FAST * (ax - envF);
        envS += SP_ENV_SLOW * (ax - envS);
        if (cool <= 0 && envF > envS * SP_ONSET_MUL + SP_ONSET_ADD) {
            ramp = 0.0f;                 /* a note restarts the climb from pitch */
            cool = SP_COOLDOWN;
        }
        if (cool > 0) cool--;
        ramp += rampK * (1.0f - ramp);   /* ... and it rises from there over Span */

        float ratio = baseRatio * (1.0f + glideAmt * ramp * glideDir);
        if (ratio < SP_RATIO_MIN) ratio = SP_RATIO_MIN;
        else if (ratio > SP_RATIO_MAX) ratio = SP_RATIO_MAX;

        if (gt >= SP_GRAIN) {            /* re-trigger the tap D behind */
            gt = 0;
            tap = sp_wrap((float)(int32_t)wp - (float)D);
        }

        /* triangular grain window, smoothstepped */
        float e;
        if (gt < (SP_GRAIN >> 1)) e = (float)gt * (2.0f * SP_INV_GRAIN);
        else                      e = (float)(SP_GRAIN - gt) * (2.0f * SP_INV_GRAIN);
        if (e > 1.0f) e = 1.0f;
        e = e * e * (3.0f - 2.0f * e);

        float echo = sp_read(buf, tap) * e;
        tap = sp_wrap(tap + ratio);
        gt++;

        lp += lpCoef * (echo - lp);      /* Tone inside the loop */
        if (lp < SP_DENORM && lp > -SP_DENORM) lp = 0.0f;

        /* soft-clip the feedback: a compounding shift must not run away */
        buf[wp & SP_MASK] = sp_soft(dry + lp * fb);
        wp++;

        float out = dry + (sp_soft(dry + lp) - dry) * mix;
        fxBuf[i]     = out;
        fxBuf[i + 8] = out;
    }

    st->writePos = wp;
    st->tap = tap;
    st->grainT = gt;
    st->lp = lp;
    st->envFast = envF;
    st->envSlow = envS;
    st->ramp = ramp;
    st->cool = cool;
}
