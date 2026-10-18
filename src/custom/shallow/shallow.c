/*
 * Shallow: Fairfield Circuitry Shallow Water-inspired random modulation.
 *
 * This is an original approximation, not a circuit/firmware clone. A filtered
 * random control signal moves a stereo fractional delay; the input envelope
 * makes modulation depth respond to playing dynamics. Optional low-level
 * noise and a dry/wet blend complete the control set.
 *
 * State and delay memory live in the validated ctx[3] arena and are cleared
 * incrementally. No math helpers, runtime division/modulo, jump tables, or
 * static audio state. ctx[11]/ctx[12] magic shuttle preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "shallow_params.h"

#ifndef SHALLOW_AUDIO_FUNC
#define SHALLOW_AUDIO_FUNC Fx_MOD_Shallow
#endif

#define SHALLOW_DO_PRAGMA(x) _Pragma(#x)
#define SHALLOW_EXPAND_PRAGMA(x) SHALLOW_DO_PRAGMA(x)
#define SHALLOW_CODE_SECTION(f) SHALLOW_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

SHALLOW_CODE_SECTION(SHALLOW_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define SW_MAGIC       0x53484C57u
#define SW_VERSION     1u
#define SW_BUF         2048u
#define SW_BUF_MASK    (SW_BUF - 1u)
#define SW_BUF_L       0u
#define SW_BUF_R       (SW_BUF_L + SW_BUF)
#define SW_TOTAL       (SW_BUF_R + SW_BUF)
#define SW_CLEAR_STEP  2048u
#define SW_DENORM      1.0e-18f

typedef struct ShallowState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearIndex;
    uint32_t writePos;
    uint32_t rngMod;
    uint32_t rngNoise;
    float modPhase;
    float modTarget;
    float modValue;
    float env;
} ShallowState;

static inline float sw_abs(float x)
{
    return x < 0.0f ? -x : x;
}

static inline float sw_flush(float x)
{
    if (x > -SW_DENORM && x < SW_DENORM) return 0.0f;
    return x;
}

static inline uint32_t sw_xorshift(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static inline float sw_random(uint32_t *state)
{
    uint32_t value = sw_xorshift(state) >> 8;
    return (float)(int32_t)value * (1.0f / 8388608.0f) - 1.0f;
}

static inline float sw_read(const float *buf, uint32_t writePos, float delay)
{
    int whole = (int)delay;
    float frac = delay - (float)whole;
    uint32_t idx = (writePos + SW_BUF - (uint32_t)whole) & SW_BUF_MASK;
    uint32_t prev = (idx - 1u) & SW_BUF_MASK;
    return buf[idx] * (1.0f - frac) + buf[prev] * frac;
}

void SHALLOW_AUDIO_FUNC(unsigned int *ctx)
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
    uintptr_t end = (uintptr_t)desc[1];
    if (base == 0u || end <= base || (base & 3u) != 0u) return;

    uintptr_t bufBase = (base + sizeof(ShallowState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)SW_TOTAL * sizeof(float) > end) return;

    ShallowState *st = (ShallowState *)base;
    float *buf = (float *)bufBase;
    if (st->magic != SW_MAGIC || st->version != SW_VERSION) {
        st->magic = SW_MAGIC;
        st->version = SW_VERSION;
        st->initialized = 0u;
        st->clearIndex = 0u;
        st->writePos = 0u;
        st->rngMod = 0x6D2B79F5u;
        st->rngNoise = 0x1F123BB5u;
        st->modPhase = 0.0f;
        st->modTarget = 0.0f;
        st->modValue = 0.0f;
        st->env = 0.0f;
    }

    if (!st->initialized) {
        uint32_t stop = st->clearIndex + SW_CLEAR_STEP;
        if (stop > SW_TOTAL) stop = SW_TOTAL;
        for (uint32_t i = st->clearIndex; i < stop; i++) buf[i] = 0.0f;
        st->clearIndex = stop;
        if (stop >= SW_TOTAL) st->initialized = 1u;
        return;
    }

    float rate = zoom_param_norm01(params[SHALLOW_RATE_SLOT], SHALLOW_RATE_DEFAULT_NORM);
    float depth = zoom_param_norm01(params[SHALLOW_DEPTH_SLOT], SHALLOW_DEPTH_DEFAULT_NORM);
    float sensitivity = zoom_param_norm01(params[SHALLOW_SENSITIVITY_SLOT],
                                          SHALLOW_SENSITIVITY_DEFAULT_NORM);
    float noise = zoom_param_norm01(params[SHALLOW_NOISE_SLOT], SHALLOW_NOISE_DEFAULT_NORM);
    float mix = zoom_param_norm01(params[SHALLOW_MIX_SLOT], SHALLOW_MIX_DEFAULT_NORM);

    float phaseInc = 0.0000027210884f + rate * 0.0001088435374f;
    float depthSamples = depth * 390.0f;
    float wetGain = 0.92f;
    float noiseGain = (noise >= 0.5f) ? 0.0012f : 0.0f;
    float dryGain = 1.0f - mix;
    float wetMix = mix * wetGain;

    uint32_t writePos = st->writePos;
    uint32_t rngMod = st->rngMod;
    uint32_t rngNoise = st->rngNoise;
    float modPhase = st->modPhase;
    float modTarget = st->modTarget;
    float modValue = st->modValue;
    float env = st->env;

    for (int i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];
        float mono = 0.5f * (sw_abs(inL) + sw_abs(inR));
        float envCoef = (mono > env) ? 0.012f : 0.0007f;
        env += envCoef * (mono - env);

        modPhase += phaseInc;
        if (modPhase >= 1.0f) {
            modPhase -= 1.0f;
            modTarget = sw_random(&rngMod);
        }
        modValue += 0.00028f * (modTarget - modValue);

        float envAmount = env * 4.0f;
        if (envAmount > 1.0f) envAmount = 1.0f;
        float dynamicDepth = 1.0f + sensitivity * (envAmount - 0.4f) * 0.75f;
        float modulation = modValue * depthSamples * dynamicDepth;
        float delayL = 640.0f + modulation;
        float delayR = 688.0f + modulation;

        buf[SW_BUF_L + writePos] = inL;
        buf[SW_BUF_R + writePos] = inR;
        float wetL = sw_read(buf + SW_BUF_L, writePos, delayL);
        float wetR = sw_read(buf + SW_BUF_R, writePos, delayR);
        float hissL = sw_random(&rngNoise) * noiseGain;
        float hissR = sw_random(&rngNoise) * noiseGain;

        fxBuf[i] = dryGain * inL + wetMix * (wetL + hissL);
        fxBuf[i + 8] = dryGain * inR + wetMix * (wetR + hissR);

        writePos = (writePos + 1u) & SW_BUF_MASK;
    }

    st->writePos = writePos;
    st->rngMod = rngMod;
    st->rngNoise = rngNoise;
    st->modPhase = sw_flush(modPhase);
    st->modTarget = sw_flush(modTarget);
    st->modValue = sw_flush(modValue);
    st->env = sw_flush(env);
}
