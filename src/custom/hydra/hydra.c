/*
 * hydra.c -- Hydra: two extra playback heads on one delay ring, MS-70CDR.
 *
 * Pedal port of tools/audio_preview/renderers/hydra.py. The dry goes into a
 * mono ring; two more heads read that ring at 2x and 0.5x, both re-triggered
 * every Window samples from one window behind the write head:
 *
 *   2x head   gap closes at 1 sample/sample, so over one window it consumes
 *             TWO windows of history and lands flush on the write head
 *             -> double-time ghost, octave up.
 *   0.5x head falls behind, covering half a window per window, max lag
 *             1.5 windows -> half-time drag, octave down.
 *
 * Window is what makes this one effect rather than two: a few ms puts the
 * re-trigger rate up at audio frequency, which IS granular pitch shifting
 * (octave-up / octave-down layers in time with the dry); 100 ms .. 0.7 s puts
 * it at rhythmic rates, so the layers become double-time hits and a half-time
 * drag. Aimed at drums. 5 knobs:
 *   Window (params[5]) - re-trigger length, cubic 256 .. 32768 samples
 *   Fast   (params[6]) - level of the 2x head
 *   Slow   (params[7]) - level of the 0.5x head
 *   Tone   (params[8]) - low-pass on the two layers only
 *   Mix    (params[9]) - dry/wet
 *
 * Safe-DSP: no math lib (smoothstep seam fade instead of a cosine, no pow --
 * the Window curve is a cubic), no runtime divide (the fade length is a fixed
 * power of two so its reciprocal is a literal), no modulo (power-of-two ring
 * mask), no switch (jump tables are unreachable in a ZDL and hard-freeze the
 * DSP), no static arrays, no float->unsigned casts. Ring lives in the ctx[3]
 * arena, cleared once at init. ctx[11]/ctx[12] magic shuttle preserved.
 * There is no feedback path, so the layers cannot self-oscillate.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "hydra_params.h"

#ifndef HYDRA_AUDIO_FUNC
#define HYDRA_AUDIO_FUNC Fx_DLY_Hydra
#endif

#define HYDRA_DO_PRAGMA(x) _Pragma(#x)
#define HYDRA_EXPAND_PRAGMA(x) HYDRA_DO_PRAGMA(x)
#define HYDRA_CODE_SECTION(f) HYDRA_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

HYDRA_CODE_SECTION(HYDRA_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define HY_MAGIC   0x48594452u   /* 'HYDR' */
#define HY_VERSION 1u

/* 131072 frames = 512 KB, ~2.97 s. The 0.5x head lags up to 1.5 windows, so
 * the ring is 4x the largest window -- comfortable margin. */
#define HY_BUF      131072u
#define HY_MASK     (HY_BUF - 1u)
#define HY_WIN_MIN  256          /* ~5.8 ms */
#define HY_WIN_SPAN 32512        /* max window 32768 = ~743 ms */
#define HY_FADE     32           /* seam fade, fixed power of two ... */
#define HY_INV_FADE 0.03125f     /* ... so 1/HY_FADE is a literal, not a divide */
#define HY_DENORM   1.0e-18f

typedef struct HydraState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t writePos;
    float fastPos, slowPos;
    int32_t grainT;
    float lp;
} HydraState;

static inline float hy_soft(float x)
{
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return 1.5f * x - 0.5f * x * x * x;
}

/* Linear-interpolated ring read. pos is already wrapped into [0, HY_BUF). */
static inline float hy_read(const float *buf, float pos)
{
    int32_t i0 = (int32_t)pos;
    float fr = pos - (float)i0;
    uint32_t a = ((uint32_t)i0) & HY_MASK;
    uint32_t b = (a + 1u) & HY_MASK;
    return buf[a] * (1.0f - fr) + buf[b] * fr;
}

static inline float hy_wrap(float pos)
{
    if (pos >= (float)HY_BUF) pos -= (float)HY_BUF;
    if (pos < 0.0f) pos += (float)HY_BUF;
    return pos;
}

void HYDRA_AUDIO_FUNC(unsigned int *ctx)
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

    uintptr_t bufBase = (base + sizeof(HydraState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)HY_BUF * 4u > end) return;

    HydraState *st = (HydraState *)base;
    float *buf = (float *)bufBase;

    if (st->magic != HY_MAGIC || st->version != HY_VERSION || !st->initialized) {
        st->magic = HY_MAGIC;
        st->version = HY_VERSION;
        st->writePos = 0u;
        st->fastPos = 0.0f;
        st->slowPos = 0.0f;
        st->grainT = 0;
        st->lp = 0.0f;
        uint32_t i;
        for (i = 0u; i < HY_BUF; i++) buf[i] = 0.0f;
        st->initialized = 1u;
    }

    float wn   = zoom_param_norm01(params[HYDRA_WINDOW_SLOT], HYDRA_WINDOW_DEFAULT_NORM);
    float fast = zoom_param_norm01(params[HYDRA_FAST_SLOT],   HYDRA_FAST_DEFAULT_NORM);
    float slow = zoom_param_norm01(params[HYDRA_SLOW_SLOT],   HYDRA_SLOW_DEFAULT_NORM);
    float tone = zoom_param_norm01(params[HYDRA_TONE_SLOT],   HYDRA_TONE_DEFAULT_NORM);
    float mix  = zoom_param_norm01(params[HYDRA_MIX_SLOT],    HYDRA_MIX_DEFAULT_NORM);

    /* cubic Window curve: fine control down at the granular end, sweeping out
     * to rhythmic lengths at the top. Cheaper and safer than a pow(). */
    float wc = wn * wn * wn;
    int32_t W = HY_WIN_MIN + (int32_t)(wc * (float)HY_WIN_SPAN);
    if (W < HY_WIN_MIN) W = HY_WIN_MIN;

    float lpCoef = 0.02f + tone * 0.55f;

    uint32_t wp   = st->writePos;
    float fastPos = st->fastPos;
    float slowPos = st->slowPos;
    int32_t gt    = st->grainT;
    float lp      = st->lp;

    int i;
    for (i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];
        float dry = 0.5f * (inL + inR);

        buf[wp & HY_MASK] = dry;

        if (gt >= W) {                      /* re-trigger both heads one window back */
            gt = 0;
            float start = hy_wrap((float)(int32_t)wp - (float)W);
            fastPos = start;
            slowPos = start;
        }

        /* smoothstep seam fade -- no cosine, and the reciprocal is a literal */
        float env;
        if (gt < HY_FADE) {
            float t = (float)gt * HY_INV_FADE;
            env = t * t * (3.0f - 2.0f * t);
        } else if (gt > W - HY_FADE) {
            float t = (float)(W - gt) * HY_INV_FADE;
            env = t * t * (3.0f - 2.0f * t);
        } else {
            env = 1.0f;
        }

        float f = hy_read(buf, fastPos);
        float s = hy_read(buf, slowPos);
        fastPos = hy_wrap(fastPos + 2.0f);
        slowPos = hy_wrap(slowPos + 0.5f);
        gt++;
        wp++;

        float layers = (f * fast + s * slow) * env;
        lp += lpCoef * (layers - lp);
        if (lp < HY_DENORM && lp > -HY_DENORM) lp = 0.0f;   /* flush denormals */

        float wet = hy_soft(dry + lp);
        float out = dry + (wet - dry) * mix;

        fxBuf[i]     = out;
        fxBuf[i + 8] = out;
    }

    st->writePos = wp;
    st->fastPos = fastPos;
    st->slowPos = slowPos;
    st->grainT = gt;
    st->lp = lp;
}
