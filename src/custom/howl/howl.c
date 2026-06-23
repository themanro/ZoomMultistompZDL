/*
 * howl.c -- Howl: Death By Audio TSA-style self-oscillating feedback, MS-70CDR.
 *
 * Pedal port of tools/audio_preview/renderers/howl.py. 2 knobs:
 *   Tune    (params[5]) - resonant / oscillation frequency
 *   Annihil (params[6]) - feedback loop gain (low = resonant, high = scream)
 * Drive (in-loop fuzz), Tone, and Mix are baked.
 *
 * A damped resonant bandpass (Chamberlin SVF) in a feedback loop with a cubic
 * soft-clip fuzz; past unity loop gain it self-oscillates, bounded by the
 * clipper. Two slightly detuned resonators (L/R) for a beating stereo howl.
 *
 * Safe-DSP: no math lib (linear SVF coefficient, cubic soft-clip), no runtime
 * divide, no static arrays, no buffer. Tiny ctx[3] state. Magic shuttle kept.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "howl_params.h"

#ifndef HOWL_AUDIO_FUNC
#define HOWL_AUDIO_FUNC Fx_FLT_Howl
#endif

#define HOWL_DO_PRAGMA(x) _Pragma(#x)
#define HOWL_EXPAND_PRAGMA(x) HOWL_DO_PRAGMA(x)
#define HOWL_CODE_SECTION(f) HOWL_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

HOWL_CODE_SECTION(HOWL_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define HOWL_MAGIC   0x484F574Cu   /* 'HOWL' */
#define HOWL_VERSION 1u

#define HOWL_TWO_PI_SR (6.2831853f / 44100.0f)
#define HOWL_Q         0.45f
#define HOWL_FC_MIN    80.0f
#define HOWL_FC_SPAN   1870.0f      /* 80 .. 1950 Hz (linear) */
#define HOWL_DRIVE     5.0f         /* baked in-loop fuzz */
#define HOWL_LPC       0.25f        /* baked output tone */
#define HOWL_WET       0.75f
#define HOWL_DRY       0.25f
#define HOWL_DETUNE    1.012f

typedef struct HowlState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t pad;
    float lowL, bandL, fbL, lpL;
    float lowR, bandR, fbR, lpR;
} HowlState;

static inline float howl_soft(float x)
{
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return 1.5f * x - 0.5f * x * x * x;
}

void HOWL_AUDIO_FUNC(unsigned int *ctx)
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
    if (end - base < (uintptr_t)sizeof(HowlState)) return;

    HowlState *st = (HowlState *)base;
    if (st->magic != HOWL_MAGIC || st->version != HOWL_VERSION || !st->initialized) {
        st->magic = HOWL_MAGIC;
        st->version = HOWL_VERSION;
        st->lowL = st->bandL = st->fbL = st->lpL = 0.0f;
        st->lowR = st->bandR = st->fbR = st->lpR = 0.0f;
        st->initialized = 1u;
    }

    float tune  = zoom_param_norm01(params[HOWL_TUNE_SLOT], HOWL_TUNE_DEFAULT_NORM);
    float annih = zoom_param_norm01(params[HOWL_ANNIHIL_SLOT], HOWL_ANNIHIL_DEFAULT_NORM);

    float fc = HOWL_FC_MIN + tune * HOWL_FC_SPAN;
    float f1L = HOWL_TWO_PI_SR * fc;
    float f1R = HOWL_TWO_PI_SR * fc * HOWL_DETUNE;
    float fbgain = annih * annih * 2.2f;

    float lowL = st->lowL, bandL = st->bandL, fbL = st->fbL, lpL = st->lpL;
    float lowR = st->lowR, bandR = st->bandR, fbR = st->fbR, lpR = st->lpR;

    int i;
    for (i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];

        float xL = inL + fbgain * fbL;
        lowL += f1L * bandL;
        float hpL = xL - lowL - HOWL_Q * bandL;
        bandL += f1L * hpL;
        float yL = howl_soft(HOWL_DRIVE * bandL);
        fbL = yL;
        lpL += HOWL_LPC * (yL - lpL);
        float wetL = howl_soft(lpL * 1.1f);

        float xR = inR + fbgain * fbR;
        lowR += f1R * bandR;
        float hpR = xR - lowR - HOWL_Q * bandR;
        bandR += f1R * hpR;
        float yR = howl_soft(HOWL_DRIVE * bandR);
        fbR = yR;
        lpR += HOWL_LPC * (yR - lpR);
        float wetR = howl_soft(lpR * 1.1f);

        fxBuf[i]     = HOWL_DRY * inL + HOWL_WET * wetL;
        fxBuf[i + 8] = HOWL_DRY * inR + HOWL_WET * wetR;
    }

    st->lowL = lowL; st->bandL = bandL; st->fbL = fbL; st->lpL = lpL;
    st->lowR = lowR; st->bandR = bandR; st->fbR = fbR; st->lpR = lpR;
}
