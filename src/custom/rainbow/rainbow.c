/*
 * Rainbow: EarthQuaker Devices Rainbow Machine-inspired pitch-warping effect.
 *
 * This is an original approximation, not a circuit or firmware clone. Two
 * dual-head delay voices create continuously warped harmonies; Tracking sets
 * the grain/delay character, Tone damps the voices, Secondary adds an octave
 * voice, and Magic feeds the pitch field back into itself. Pedal footswitch
 * behavior is represented only by the host effect enable control.
 *
 * Delay memory and state live in the validated ctx[3] arena and clear
 * incrementally. No math helpers, runtime division/modulo, jump tables, or
 * static audio state. ctx[11]/ctx[12] magic shuttle preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "rainbow_params.h"

#ifndef RAINBOW_AUDIO_FUNC
#define RAINBOW_AUDIO_FUNC Fx_MOD_Rainbow
#endif

#define RAINBOW_DO_PRAGMA(x) _Pragma(#x)
#define RAINBOW_EXPAND_PRAGMA(x) RAINBOW_DO_PRAGMA(x)
#define RAINBOW_CODE_SECTION(f) RAINBOW_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

RAINBOW_CODE_SECTION(RAINBOW_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define RW_MAGIC       0x52424F57u
#define RW_VERSION     1u
#define RW_BUF         4096u
#define RW_BUF_MASK    (RW_BUF - 1u)
#define RW_BUF_L       0u
#define RW_BUF_R       (RW_BUF_L + RW_BUF)
#define RW_TOTAL       (RW_BUF_R + RW_BUF)
#define RW_CLEAR_STEP  2048u
#define RW_DENORM      1.0e-18f
typedef struct RainbowState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearIndex;
    uint32_t writePos;
    float primaryPhase;
    float secondaryPhase;
    float toneL;
    float toneR;
    float toneSL;
    float toneSR;
    float feedbackL;
    float feedbackR;
} RainbowState;

static inline float rw_flush(float x)
{
    if (x > -RW_DENORM && x < RW_DENORM) return 0.0f;
    return x;
}

static inline float rw_window(float phase)
{
    return phase < 0.5f ? phase * 2.0f : (2.0f - phase * 2.0f);
}

static inline float rw_read(const float *buf, uint32_t writePos, float delay)
{
    int whole = (int)delay;
    float frac = delay - (float)whole;
    uint32_t idx = (writePos + RW_BUF - (uint32_t)whole) & RW_BUF_MASK;
    uint32_t prev = (idx - 1u) & RW_BUF_MASK;
    return buf[idx] * (1.0f - frac) + buf[prev] * frac;
}

static inline float rw_voice(const float *buf, uint32_t writePos,
                             float delayBase, float delayRange, float phase)
{
    float phaseA = phase;
    float phaseB = phase + 0.5f;
    if (phaseB >= 1.0f) phaseB -= 1.0f;
    float delayA = delayBase + phaseA * delayRange;
    float delayB = delayBase + phaseB * delayRange;
    float a = rw_read(buf, writePos, delayA);
    float b = rw_read(buf, writePos, delayB);
    float weightA = rw_window(phaseA);
    float weightB = rw_window(phaseB);
    return a * weightA + b * weightB;
}

static inline float rw_soft_feedback(float x)
{
    if (x > 2.0f) return 2.0f;
    if (x < -2.0f) return -2.0f;
    return x - 0.0625f * x * x * x;
}

void RAINBOW_AUDIO_FUNC(unsigned int *ctx)
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

    uintptr_t bufBase = (base + sizeof(RainbowState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)RW_TOTAL * sizeof(float) > end) return;

    RainbowState *st = (RainbowState *)base;
    float *buf = (float *)bufBase;
    if (st->magic != RW_MAGIC || st->version != RW_VERSION) {
        st->magic = RW_MAGIC;
        st->version = RW_VERSION;
        st->initialized = 0u;
        st->clearIndex = 0u;
        st->writePos = 0u;
        st->primaryPhase = 0.0f;
        st->secondaryPhase = 0.25f;
        st->toneL = 0.0f;
        st->toneR = 0.0f;
        st->toneSL = 0.0f;
        st->toneSR = 0.0f;
        st->feedbackL = 0.0f;
        st->feedbackR = 0.0f;
    }

    if (!st->initialized) {
        uint32_t stop = st->clearIndex + RW_CLEAR_STEP;
        if (stop > RW_TOTAL) stop = RW_TOTAL;
        for (uint32_t i = st->clearIndex; i < stop; i++) buf[i] = 0.0f;
        st->clearIndex = stop;
        if (stop >= RW_TOTAL) st->initialized = 1u;
        return;
    }

    float pitch = zoom_param_norm01(params[RAINBOW_PITCH_SLOT], RAINBOW_PITCH_DEFAULT_NORM);
    float primary = zoom_param_norm01(params[RAINBOW_PRIMARY_SLOT], RAINBOW_PRIMARY_DEFAULT_NORM);
    float tracking = zoom_param_norm01(params[RAINBOW_TRACKING_SLOT], RAINBOW_TRACKING_DEFAULT_NORM);
    float tone = zoom_param_norm01(params[RAINBOW_TONE_SLOT], RAINBOW_TONE_DEFAULT_NORM);
    float secondary = zoom_param_norm01(params[RAINBOW_SECONDARY_SLOT],
                                       RAINBOW_SECONDARY_DEFAULT_NORM);
    float magic = zoom_param_norm01(params[RAINBOW_MAGIC_SLOT], RAINBOW_MAGIC_DEFAULT_NORM);

    float primaryRatio = pitch < 0.5f
        ? 1.0f - (0.5f - pitch) * 0.5f
        : 1.0f + (pitch - 0.5f) * 0.5f;
    float secondaryRatio = pitch < 0.5f ? primaryRatio * 0.5f : primaryRatio * 2.0f;
    if (pitch > 0.49f && pitch < 0.51f) secondaryRatio = 1.008f;

    float delayRange = 512.0f + tracking * 768.0f;
    float delayBase = 256.0f + (1.0f - tracking) * 1024.0f;
    float primaryInc = (1.0f - primaryRatio) / delayRange - 0.000003f;
    float secondaryInc = (1.0f - secondaryRatio) / delayRange;
    float toneCoef = 0.035f + tone * 0.82f;
    float feedbackGain = magic * 0.82f;
    float secondaryGain = secondary;
    float feedbackSecondary = secondaryGain * 0.5f;
    float feedbackPrimary = 1.0f - feedbackSecondary;

    uint32_t writePos = st->writePos;
    float primaryPhase = st->primaryPhase;
    float secondaryPhase = st->secondaryPhase;
    float toneL = st->toneL;
    float toneR = st->toneR;
    float toneSL = st->toneSL;
    float toneSR = st->toneSR;
    float feedbackL = st->feedbackL;
    float feedbackR = st->feedbackR;

    for (int i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];
        float writeL = inL + feedbackL * feedbackGain;
        float writeR = inR + feedbackR * feedbackGain;
        buf[RW_BUF_L + writePos] = rw_soft_feedback(writeL);
        buf[RW_BUF_R + writePos] = rw_soft_feedback(writeR);

        float pL = rw_voice(buf + RW_BUF_L, writePos, delayBase, delayRange,
                            primaryPhase);
        float pR = rw_voice(buf + RW_BUF_R, writePos, delayBase, delayRange,
                            primaryPhase + 0.5f >= 1.0f
                                ? primaryPhase - 0.5f : primaryPhase + 0.5f);
        float sL = rw_voice(buf + RW_BUF_L, writePos, delayBase, delayRange,
                            secondaryPhase);
        float sR = rw_voice(buf + RW_BUF_R, writePos, delayBase, delayRange,
                            secondaryPhase + 0.5f >= 1.0f
                                ? secondaryPhase - 0.5f : secondaryPhase + 0.5f);

        toneL += toneCoef * (pL - toneL);
        toneR += toneCoef * (pR - toneR);
        toneSL += toneCoef * (sL - toneSL);
        toneSR += toneCoef * (sR - toneSR);
        float wetL = primary * toneL + secondaryGain * toneSL;
        float wetR = primary * toneR + secondaryGain * toneSR;
        feedbackL = feedbackPrimary * toneL + feedbackSecondary * toneSL;
        feedbackR = feedbackPrimary * toneR + feedbackSecondary * toneSR;

        fxBuf[i] = inL + wetL;
        fxBuf[i + 8] = inR + wetR;

        primaryPhase += primaryInc;
        if (primaryPhase >= 1.0f) primaryPhase -= 1.0f;
        if (primaryPhase < 0.0f) primaryPhase += 1.0f;
        secondaryPhase += secondaryInc;
        if (secondaryPhase >= 1.0f) secondaryPhase -= 1.0f;
        if (secondaryPhase < 0.0f) secondaryPhase += 1.0f;
        writePos = (writePos + 1u) & RW_BUF_MASK;
    }

    st->writePos = writePos;
    st->primaryPhase = rw_flush(primaryPhase);
    st->secondaryPhase = rw_flush(secondaryPhase);
    st->toneL = rw_flush(toneL);
    st->toneR = rw_flush(toneR);
    st->toneSL = rw_flush(toneSL);
    st->toneSR = rw_flush(toneSR);
    st->feedbackL = rw_flush(feedbackL);
    st->feedbackR = rw_flush(feedbackR);
}
