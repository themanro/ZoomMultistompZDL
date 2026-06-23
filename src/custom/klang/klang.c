/*
 * klang.c -- Klang: ring modulator, MS-70CDR.
 *
 * Pedal port (v1) of tools/audio_preview/renderers/klang.py, RING MOD only.
 * The frequency-shifter modes need a Hilbert allpass network and are deferred
 * to v2. 2 knobs:
 *   Freq (params[5]) - carrier frequency (linear ~0.5..2000 Hz)
 *   Mix  (params[6]) - dry/wet
 * Stereo spread (a slight L/R carrier detune) is baked.
 *
 * Safe-DSP: no math lib (carrier is a polynomial sine), no runtime divide
 * (carrier increment uses the compile-time 2*pi/SR constant), no buffer.
 * Tiny persistent state (two carrier phases) in the ctx[3] arena.
 * ctx[11]/ctx[12] magic shuttle preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "klang_params.h"

#ifndef KLANG_AUDIO_FUNC
#define KLANG_AUDIO_FUNC Fx_MOD_Klang
#endif

#define KLANG_DO_PRAGMA(x) _Pragma(#x)
#define KLANG_EXPAND_PRAGMA(x) KLANG_DO_PRAGMA(x)
#define KLANG_CODE_SECTION(f) KLANG_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

KLANG_CODE_SECTION(KLANG_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define KLANG_MAGIC   0x4B4C4E47u   /* 'KLNG' */
#define KLANG_VERSION 1u

#define KLANG_TWO_PI    6.28318530718f
#define KLANG_TWO_PI_SR (6.28318530718f / 44100.0f)
#define KLANG_FREQ_MIN  0.5f
#define KLANG_FREQ_SPAN 1999.5f       /* 0.5 .. 2000 Hz (linear) */
#define KLANG_SPREAD    1.006f        /* R carrier slightly detuned */

typedef struct KlangState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t pad;
    float phaseL;
    float phaseR;
} KlangState;

static inline float kl_abs(float x) { return x < 0.0f ? -x : x; }

static inline float kl_sin(float x)
{
    const float twoPi = 6.28318530718f, pi = 3.14159265359f, inv = 0.15915494309f;
    x = x - twoPi * (float)((int)(x * inv));
    if (x < -pi) x += twoPi;
    if (x > pi) x -= twoPi;
    float y = 1.2732395447f * x - 0.4052847346f * x * kl_abs(x);
    return y + 0.225f * (y * kl_abs(y) - y);
}

void KLANG_AUDIO_FUNC(unsigned int *ctx)
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
    if (end - base < (uintptr_t)sizeof(KlangState)) return;

    KlangState *st = (KlangState *)base;
    if (st->magic != KLANG_MAGIC || st->version != KLANG_VERSION || !st->initialized) {
        st->magic = KLANG_MAGIC;
        st->version = KLANG_VERSION;
        st->phaseL = 0.0f;
        st->phaseR = 0.0f;
        st->initialized = 1u;
    }

    float freq = zoom_param_norm01(params[KLANG_FREQ_SLOT], KLANG_FREQ_DEFAULT_NORM);
    float mix  = zoom_param_norm01(params[KLANG_MIX_SLOT], KLANG_MIX_DEFAULT_NORM);

    float carrierHz = KLANG_FREQ_MIN + freq * KLANG_FREQ_SPAN;
    float incL = carrierHz * KLANG_TWO_PI_SR;
    float incR = incL * KLANG_SPREAD;
    float wet = mix;
    float dry = 1.0f - mix;

    float phL = st->phaseL, phR = st->phaseR;
    int i;
    for (i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];

        float cL = kl_sin(phL);
        float cR = kl_sin(phR);
        fxBuf[i]     = dry * inL + wet * (inL * cL);
        fxBuf[i + 8] = dry * inR + wet * (inR * cR);

        phL += incL;
        if (phL > KLANG_TWO_PI) phL -= KLANG_TWO_PI;
        phR += incR;
        if (phR > KLANG_TWO_PI) phR -= KLANG_TWO_PI;
    }

    st->phaseL = phL;
    st->phaseR = phR;
}
