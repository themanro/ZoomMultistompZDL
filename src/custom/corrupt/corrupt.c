/*
 * corrupt.c -- Data Corrupter-inspired PLL square synth for the MS-70CDR.
 *
 * The pedal version tracks pitch with a low-cost rising-zero-crossing
 * detector, then regenerates a square master, a divided subharmonic, and an
 * FM-modulated harmony voice. Ambiguous input can cause octave jumps; use
 * single-note lines for the most stable tracking.
 *
 * Safe-DSP: no math library, runtime divide, or modulo. Persistent state lives
 * in the ctx[3] arena; ctx[11]/ctx[12] magic shuttle is preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "corrupt_params.h"

#ifndef CORRUPT_AUDIO_FUNC
#define CORRUPT_AUDIO_FUNC Fx_DLY_Corrupt
#endif

#define CORRUPT_DO_PRAGMA(x) _Pragma(#x)
#define CORRUPT_EXPAND_PRAGMA(x) CORRUPT_DO_PRAGMA(x)
#define CORRUPT_CODE_SECTION(f) CORRUPT_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

CORRUPT_CODE_SECTION(CORRUPT_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define CR_MAGIC       0x43525054u   /* 'CRPT' */
#define CR_VERSION     3u
#define CR_LP_COEF     0.10f
#define CR_TONE_COEF   0.575f       /* one-pole equivalent of the 6 kHz preview LPF */
#define CR_GATE        0.01f
#define CR_ENV_ATK     0.05f
#define CR_ENV_REL     0.0008f
#define CR_MIN_P       40.0f
#define CR_MAX_P       1100.0f
#define CR_P_SMOOTH    0.30f
#define CR_MAKEUP      1.8f
#define CR_WAVE_LEVEL  0.55f
#define CR_CLAMP       1.5f
#define CR_SAMPLE_RATE_INV 0.000022675737f

typedef struct CorruptState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    int32_t  sinceCross;

    float lpIn;
    float prevLp;
    float period;
    float mphase;
    float sphase;
    float hphase;
    float fmPhase;
    float tlp;
    float env;
} CorruptState;

/* 1/x via a bit-trick seed + Newton steps (avoids __c6xabi_divf). */
static inline float cr_recip(float x)
{
    union { float f; uint32_t u; } v, y;
    v.f = x;
    y.u = 0x7EF311C3u - v.u;
    y.f = y.f * (2.0f - x * y.f);
    y.f = y.f * (2.0f - x * y.f);
    y.f = y.f * (2.0f - x * y.f);
    return y.f;
}

void CORRUPT_AUDIO_FUNC(unsigned int *ctx)
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
    if (end - base < (uintptr_t)sizeof(CorruptState)) return;

    CorruptState *st = (CorruptState *)base;
    if (st->magic != CR_MAGIC || st->version != CR_VERSION || !st->initialized) {
        st->magic = CR_MAGIC;
        st->version = CR_VERSION;
        st->sinceCross = 0;
        st->lpIn = 0.0f;
        st->prevLp = 0.0f;
        st->period = 200.0f;
        st->mphase = 0.0f;
        st->sphase = 0.0f;
        st->hphase = 0.0f;
        st->fmPhase = 0.0f;
        st->tlp = 0.0f;
        st->env = 0.0f;
        st->initialized = 1u;
    }

    float master = zoom_param_norm01(params[CORRUPT_MASTER_SLOT], CORRUPT_MASTER_DEFAULT_NORM);
    float subLvl = zoom_param_norm01(params[CORRUPT_SUB_SLOT], CORRUPT_SUB_DEFAULT_NORM);
    float harm = zoom_param_norm01(params[CORRUPT_HARM_SLOT], CORRUPT_HARM_DEFAULT_NORM);
    float rate = zoom_param_norm01(params[CORRUPT_RATE_SLOT], CORRUPT_RATE_DEFAULT_NORM);
    float depth = zoom_param_norm01(params[CORRUPT_DEPTH_SLOT], CORRUPT_DEPTH_DEFAULT_NORM);
    float mix = zoom_param_norm01(params[CORRUPT_MIX_SLOT], CORRUPT_MIX_DEFAULT_NORM);

    float masterRatio = 1.0f;
    if (master >= 0.25f) masterRatio = 0.5f;
    if (master >= 0.75f) masterRatio = 0.25f;

    float harmRatio;
    if (harm >= 0.84f) harmRatio = 2.996614f;       /* 12th */
    else if (harm >= 0.67f) harmRatio = 0.5f;       /* octave below */
    else if (harm >= 0.50f) harmRatio = 1.334840f;  /* fourth */
    else if (harm >= 0.34f) harmRatio = 2.0f;       /* octave */
    else if (harm >= 0.17f) harmRatio = 1.498307f;  /* fifth */
    else harmRatio = 1.0f;                         /* unison */

    float fmRate = 0.5f + rate * 11.5f;
    float fmInc = fmRate * CR_SAMPLE_RATE_INV;
    float fmDepth = depth * 0.519f;                /* approximately three semitones */
    float wet = mix;
    float dry = 1.0f - mix;

    float lpIn = st->lpIn, prevLp = st->prevLp, period = st->period;
    float mphase = st->mphase, sphase = st->sphase, hphase = st->hphase;
    float fmPhase = st->fmPhase, tlp = st->tlp, env = st->env;
    int sinceCross = st->sinceCross;

    int f;
    for (f = 0; f < 8; f++) {
        float inL = fxBuf[f];
        float inR = fxBuf[f + 8];
        float in = 0.5f * (inL + inR);

        float a = in < 0.0f ? -in : in;
        if (a > env) env += CR_ENV_ATK * (a - env);
        else env += CR_ENV_REL * (a - env);

        lpIn += CR_LP_COEF * (in - lpIn);
        if (sinceCross <= (int)CR_MAX_P) sinceCross++;
        if (prevLp < 0.0f && lpIn >= 0.0f && env > CR_GATE) {
            float p = (float)sinceCross;
            if (p >= CR_MIN_P && p <= CR_MAX_P) {
                period += CR_P_SMOOTH * (p - period);
            }
            sinceCross = 0;
        }
        prevLp = lpIn;

        float inc = cr_recip(period);
        mphase += inc * masterRatio;
        if (mphase >= 1.0f) mphase -= 1.0f;
        float masterVoice = mphase < 0.5f ? 1.0f : -1.0f;

        sphase += inc * masterRatio * 0.5f;
        if (sphase >= 1.0f) sphase -= 1.0f;
        float subVoice = sphase < 0.5f ? 1.0f : -1.0f;

        fmPhase += fmInc;
        if (fmPhase >= 1.0f) fmPhase -= 1.0f;
        float lfo;
        if (fmPhase < 0.5f) lfo = 16.0f * fmPhase * (0.5f - fmPhase);
        else lfo = -16.0f * (fmPhase - 0.5f) * (1.0f - fmPhase);
        float fmRatio = 1.0f + lfo * fmDepth;

        hphase += inc * masterRatio * harmRatio * fmRatio;
        if (hphase >= 1.0f) hphase -= 1.0f;
        float harmVoice = hphase < 0.5f ? 1.0f : -1.0f;

        float synth = env * CR_MAKEUP *
                      (masterVoice + subLvl * subVoice + CR_WAVE_LEVEL * harmVoice) * 0.45f;
        tlp += CR_TONE_COEF * (synth - tlp);
        float o = tlp;
        if (o > CR_CLAMP) o = CR_CLAMP;
        else if (o < -CR_CLAMP) o = -CR_CLAMP;

        fxBuf[f]     = dry * inL + wet * o;
        fxBuf[f + 8] = dry * inR + wet * o;
    }

    st->lpIn = lpIn;
    st->prevLp = prevLp;
    st->period = period;
    st->mphase = mphase;
    st->sphase = sphase;
    st->hphase = hphase;
    st->fmPhase = fmPhase;
    st->tlp = tlp;
    st->env = env;
    st->sinceCross = sinceCross;
}
