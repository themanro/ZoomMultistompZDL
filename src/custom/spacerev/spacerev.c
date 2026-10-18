/*
 * SpaceRev: DigiTech Space Station Reverse-mode-inspired reverse grains.
 *
 * Original approximation, not a firmware/algorithm clone. The Expression
 * control sets reverse-grain length; a triangle LFO modulates that expression
 * value using Rate and Depth. Two stereo banks allow one captured grain to
 * play backwards while the next is captured. Grain edges crossfade to dry.
 *
 * State and delay memory use the validated ctx[3] arena and clear
 * incrementally. No math helpers, runtime divide/modulo, jump tables, or
 * static audio state. ctx[11]/ctx[12] magic shuttle preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "spacerev_params.h"

#ifndef SPACEREV_AUDIO_FUNC
#define SPACEREV_AUDIO_FUNC Fx_DLY_SpaceRev
#endif

#define SPACEREV_DO_PRAGMA(x) _Pragma(#x)
#define SPACEREV_EXPAND_PRAGMA(x) SPACEREV_DO_PRAGMA(x)
#define SPACEREV_CODE_SECTION(f) SPACEREV_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

SPACEREV_CODE_SECTION(SPACEREV_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define SR_MAGIC          0x53505256u
#define SR_VERSION        1u
#define SR_MIN_GRAIN      4096u
#define SR_MAX_GRAIN      32768u
#define SR_GRAIN_RANGE    (SR_MAX_GRAIN - SR_MIN_GRAIN)
#define SR_BANK_FLOATS    (SR_MAX_GRAIN * 2u)
#define SR_TOTAL_FLOATS   (SR_BANK_FLOATS * 2u)
#define SR_CLEAR_STEP     2048u
#define SR_SAMPLE_RATE_INV 0.000022675737f
#define SR_FADE_SAMPLES   64u
#define SR_DENORM         1.0e-18f

typedef struct SpaceRevState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearIndex;
    uint32_t captureBank;
    uint32_t playbackBank;
    uint32_t captureCount;
    uint32_t captureLength;
    uint32_t captureReady;
    uint32_t playbackPos;
    uint32_t playbackRemain;
    uint32_t playbackLength;
    float lfoPhase;
} SpaceRevState;

static inline float sr_flush(float x)
{
    if (x > -SR_DENORM && x < SR_DENORM) return 0.0f;
    return x;
}

static inline float sr_lfo(float phase)
{
    if (phase < 0.5f) return phase * 4.0f - 1.0f;
    return 3.0f - phase * 4.0f;
}

static inline uint32_t sr_grain_length(float expression)
{
    if (expression < 0.0f) expression = 0.0f;
    if (expression > 1.0f) expression = 1.0f;
    int32_t range = (int32_t)(expression * (float)SR_GRAIN_RANGE);
    return SR_MIN_GRAIN + (uint32_t)range;
}

void SPACEREV_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf = ZDL_PTR(float *, ctx[5]);

    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *,
                                     *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile unsigned int *desc = ZDL_PTR(volatile unsigned int *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end = (uintptr_t)desc[1];
    if (base == 0u || end <= base || (base & 3u) != 0u) return;

    uintptr_t bufBase = (base + sizeof(SpaceRevState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)SR_TOTAL_FLOATS * sizeof(float) > end) return;

    SpaceRevState *st = (SpaceRevState *)base;
    float *buf = (float *)bufBase;
    if (st->magic != SR_MAGIC || st->version != SR_VERSION) {
        st->magic = SR_MAGIC;
        st->version = SR_VERSION;
        st->initialized = 0u;
        st->clearIndex = 0u;
        st->captureBank = 0u;
        st->playbackBank = 1u;
        st->captureCount = 0u;
        st->captureLength = SR_MIN_GRAIN;
        st->captureReady = 0u;
        st->playbackPos = 0u;
        st->playbackRemain = 0u;
        st->playbackLength = 0u;
        st->lfoPhase = 0.0f;
    }

    if (!st->initialized) {
        uint32_t stop = st->clearIndex + SR_CLEAR_STEP;
        if (stop > SR_TOTAL_FLOATS) stop = SR_TOTAL_FLOATS;
        for (uint32_t i = st->clearIndex; i < stop; i++) buf[i] = 0.0f;
        st->clearIndex = stop;
        if (stop >= SR_TOTAL_FLOATS) st->initialized = 1u;
        return;
    }

    float expression = zoom_param_norm01(params[SPACEREV_EXPRESSION_SLOT],
                                         SPACEREV_EXPRESSION_DEFAULT_NORM);
    float rate = zoom_param_norm01(params[SPACEREV_RATE_SLOT], SPACEREV_RATE_DEFAULT_NORM);
    float depth = zoom_param_norm01(params[SPACEREV_DEPTH_SLOT], SPACEREV_DEPTH_DEFAULT_NORM);
    float mix = zoom_param_norm01(params[SPACEREV_MIX_SLOT], SPACEREV_MIX_DEFAULT_NORM);
    float rateHz = 0.05f + rate * 7.95f;
    float phaseInc = rateHz * SR_SAMPLE_RATE_INV;

    uint32_t captureBank = st->captureBank;
    uint32_t playbackBank = st->playbackBank;
    uint32_t captureCount = st->captureCount;
    uint32_t captureLength = st->captureLength;
    uint32_t captureReady = st->captureReady;
    uint32_t playbackPos = st->playbackPos;
    uint32_t playbackRemain = st->playbackRemain;
    uint32_t playbackLength = st->playbackLength;
    float lfoPhase = st->lfoPhase;

    for (int i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];

        lfoPhase += phaseInc;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float expressionMod = expression + sr_lfo(lfoPhase) * depth * 0.5f;
        if (expressionMod < 0.0f) expressionMod = 0.0f;
        if (expressionMod > 1.0f) expressionMod = 1.0f;

        if (playbackRemain == 0u && captureReady) {
            playbackBank = captureBank;
            captureBank ^= 1u;
            playbackLength = captureLength;
            playbackPos = playbackLength - 1u;
            playbackRemain = playbackLength;
            captureReady = 0u;
            captureCount = 0u;
        }
        if (captureCount == 0u && !captureReady)
            captureLength = sr_grain_length(expressionMod);

        float outL = inL;
        float outR = inR;
        if (playbackRemain > 0u) {
            uint32_t bankOffset = playbackBank * SR_BANK_FLOATS;
            uint32_t readOffset = bankOffset + playbackPos * 2u;
            float revL = buf[readOffset];
            float revR = buf[readOffset + 1u];
            uint32_t elapsed = playbackLength - playbackRemain;
            float fadeIn = (elapsed < SR_FADE_SAMPLES)
                ? (float)elapsed * (1.0f / (float)SR_FADE_SAMPLES) : 1.0f;
            float fadeOut = (playbackRemain <= 1u) ? 0.0f
                : (playbackRemain < SR_FADE_SAMPLES)
                ? (float)playbackRemain * (1.0f / (float)SR_FADE_SAMPLES) : 1.0f;
            float wetMix = mix * ((fadeIn < fadeOut) ? fadeIn : fadeOut);
            outL = inL * (1.0f - wetMix) + revL * wetMix;
            outR = inR * (1.0f - wetMix) + revR * wetMix;
            if (playbackRemain > 1u) playbackPos--;
            else playbackPos = 0u;
            playbackRemain--;
        }

        if (!captureReady && captureCount < captureLength) {
            uint32_t writeOffset = captureBank * SR_BANK_FLOATS + captureCount * 2u;
            buf[writeOffset] = inL;
            buf[writeOffset + 1u] = inR;
            captureCount++;
            if (captureCount >= captureLength) captureReady = 1u;
        }

        fxBuf[i] = outL;
        fxBuf[i + 8] = outR;
    }

    st->captureBank = captureBank;
    st->playbackBank = playbackBank;
    st->captureCount = captureCount;
    st->captureLength = captureLength;
    st->captureReady = captureReady;
    st->playbackPos = playbackPos;
    st->playbackRemain = playbackRemain;
    st->playbackLength = playbackLength;
    st->lfoPhase = sr_flush(lfoPhase);
}
