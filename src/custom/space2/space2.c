/*
 * Space2: DigiTech XP-300 Space Station patch 2-inspired shimmer reverb.
 *
 * Original approximation, not a firmware/algorithm clone. A dual-head
 * fractional-delay shifter creates an upward perfect fifth; a damped,
 * cross-coupled comb network and short allpasses diffuse it into a reverb.
 *
 * All state and delay memory live in the validated ctx[3] arena and clear
 * incrementally. No math helpers, runtime division/modulo, jump tables, or
 * static audio state. ctx[11]/ctx[12] magic shuttle preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "space2_params.h"

#ifndef SPACE2_AUDIO_FUNC
#define SPACE2_AUDIO_FUNC Fx_REV_Space2
#endif

#define SPACE2_DO_PRAGMA(x) _Pragma(#x)
#define SPACE2_EXPAND_PRAGMA(x) SPACE2_DO_PRAGMA(x)
#define SPACE2_CODE_SECTION(f) SPACE2_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

SPACE2_CODE_SECTION(SPACE2_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define S2_MAGIC       0x53503252u
#define S2_VERSION     1u
#define S2_PITCH_LEN   4096u
#define S2_PITCH_MASK  (S2_PITCH_LEN - 1u)
#define S2_GRAIN       2048.0f
#define S2_SPAN        1024.0f
#define S2_BASE_DELAY  512.0f
#define S2_C0          997u
#define S2_C1          1193u
#define S2_C2          1031u
#define S2_C3          1277u
#define S2_A0          149u
#define S2_A1          211u

#define S2_PITCH_L     0u
#define S2_PITCH_R     (S2_PITCH_L + S2_PITCH_LEN)
#define S2_C0_OFF      (S2_PITCH_R + S2_PITCH_LEN)
#define S2_C1_OFF      (S2_C0_OFF + S2_C0)
#define S2_C2_OFF      (S2_C1_OFF + S2_C1)
#define S2_C3_OFF      (S2_C2_OFF + S2_C2)
#define S2_A0_OFF      (S2_C3_OFF + S2_C3)
#define S2_A1_OFF      (S2_A0_OFF + S2_A0)
#define S2_TOTAL       (S2_A1_OFF + S2_A1)
#define S2_CLEAR_STEP  2048u
#define S2_DENORM      1.0e-18f

typedef struct Space2State {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearIndex;
    uint32_t pitchWrite;
    uint32_t combPos[4];
    uint32_t allPos[2];
    float pitchPhase;
    float combLp[4];
} Space2State;

static inline float s2_flush(float x)
{
    if (x > -S2_DENORM && x < S2_DENORM) return 0.0f;
    return x;
}

static inline float s2_window(float phase)
{
    return phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
}

static inline float s2_read(const float *buf, uint32_t writePos, float delay)
{
    int whole = (int)delay;
    float frac = delay - (float)whole;
    uint32_t idx = (writePos + S2_PITCH_LEN - (uint32_t)whole) & S2_PITCH_MASK;
    uint32_t prev = (idx - 1u) & S2_PITCH_MASK;
    return buf[idx] * (1.0f - frac) + buf[prev] * frac;
}

static inline float s2_shift(const float *buf, uint32_t writePos,
                             float phase, float offset)
{
    float p0 = phase + offset;
    if (p0 >= 1.0f) p0 -= 1.0f;
    float p1 = p0 + 0.5f;
    if (p1 >= 1.0f) p1 -= 1.0f;
    float d0 = S2_BASE_DELAY + (1.0f - p0) * S2_SPAN;
    float d1 = S2_BASE_DELAY + (1.0f - p1) * S2_SPAN;
    return s2_read(buf, writePos, d0) * s2_window(p0)
         + s2_read(buf, writePos, d1) * s2_window(p1);
}

static inline float s2_allpass(float *buf, uint32_t off, uint32_t len,
                               uint32_t *pos, float x)
{
    uint32_t idx = off + *pos;
    float delayed = buf[idx];
    float out = delayed - x * 0.65f;
    buf[idx] = x + delayed * 0.65f;
    uint32_t next = *pos + 1u;
    if (next >= len) next = 0u;
    *pos = next;
    return out;
}

void SPACE2_AUDIO_FUNC(unsigned int *ctx)
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

    uintptr_t bufBase = (base + sizeof(Space2State) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)S2_TOTAL * sizeof(float) > end) return;

    Space2State *st = (Space2State *)base;
    float *buf = (float *)bufBase;
    if (st->magic != S2_MAGIC || st->version != S2_VERSION) {
        st->magic = S2_MAGIC;
        st->version = S2_VERSION;
        st->initialized = 0u;
        st->clearIndex = 0u;
        st->pitchWrite = 0u;
        for (int i = 0; i < 4; i++) {
            st->combPos[i] = 0u;
            st->combLp[i] = 0.0f;
        }
        st->allPos[0] = 0u;
        st->allPos[1] = 0u;
        st->pitchPhase = 0.0f;
    }

    if (!st->initialized) {
        uint32_t stop = st->clearIndex + S2_CLEAR_STEP;
        if (stop > S2_TOTAL) stop = S2_TOTAL;
        for (uint32_t i = st->clearIndex; i < stop; i++) buf[i] = 0.0f;
        st->clearIndex = stop;
        if (stop >= S2_TOTAL) st->initialized = 1u;
        return;
    }

    float fifth = zoom_param_norm01(params[SPACE2_FIFTH_SLOT], SPACE2_FIFTH_DEFAULT_NORM);
    float decay = zoom_param_norm01(params[SPACE2_DECAY_SLOT], SPACE2_DECAY_DEFAULT_NORM);
    float tone = zoom_param_norm01(params[SPACE2_TONE_SLOT], SPACE2_TONE_DEFAULT_NORM);
    float mix = zoom_param_norm01(params[SPACE2_MIX_SLOT], SPACE2_MIX_DEFAULT_NORM);

    float feedback = 0.72f + 0.265f * decay;
    float damp = 0.08f + 0.76f * (1.0f - tone);
    float dampInv = 1.0f - damp;
    float fifthGain = fifth * 0.78f;
    float dryGain = 1.0f - mix;
    float wetGain = mix;
    float combInputGain = 0.22f + fifth * 0.48f;

    uint32_t pitchWrite = st->pitchWrite;
    uint32_t combPos0 = st->combPos[0];
    uint32_t combPos1 = st->combPos[1];
    uint32_t combPos2 = st->combPos[2];
    uint32_t combPos3 = st->combPos[3];
    uint32_t allPos0 = st->allPos[0];
    uint32_t allPos1 = st->allPos[1];
    float pitchPhase = st->pitchPhase;
    float lp0 = st->combLp[0], lp1 = st->combLp[1];
    float lp2 = st->combLp[2], lp3 = st->combLp[3];

    for (int i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];
        buf[S2_PITCH_L + pitchWrite] = inL;
        buf[S2_PITCH_R + pitchWrite] = inR;
        float shiftedL = s2_shift(buf + S2_PITCH_L, pitchWrite, pitchPhase, 0.0f);
        float shiftedR = s2_shift(buf + S2_PITCH_R, pitchWrite, pitchPhase, 0.5f);
        pitchPhase += 1.0f / S2_GRAIN;
        if (pitchPhase >= 1.0f) pitchPhase -= 1.0f;
        pitchWrite = (pitchWrite + 1u) & S2_PITCH_MASK;

        float excitationL = inL * 0.25f + shiftedL * combInputGain;
        float excitationR = inR * 0.25f + shiftedR * combInputGain;

        float c0 = buf[S2_C0_OFF + combPos0];
        lp0 = lp0 * damp + c0 * dampInv;
        buf[S2_C0_OFF + combPos0] = excitationL + lp0 * feedback;
        combPos0 = (combPos0 + 1u >= S2_C0) ? 0u : combPos0 + 1u;

        float c1 = buf[S2_C1_OFF + combPos1];
        lp1 = lp1 * damp + c1 * dampInv;
        buf[S2_C1_OFF + combPos1] = excitationL + lp1 * feedback;
        combPos1 = (combPos1 + 1u >= S2_C1) ? 0u : combPos1 + 1u;

        float c2 = buf[S2_C2_OFF + combPos2];
        lp2 = lp2 * damp + c2 * dampInv;
        buf[S2_C2_OFF + combPos2] = excitationR + lp2 * feedback;
        combPos2 = (combPos2 + 1u >= S2_C2) ? 0u : combPos2 + 1u;

        float c3 = buf[S2_C3_OFF + combPos3];
        lp3 = lp3 * damp + c3 * dampInv;
        buf[S2_C3_OFF + combPos3] = excitationR + lp3 * feedback;
        combPos3 = (combPos3 + 1u >= S2_C3) ? 0u : combPos3 + 1u;

        float revL = s2_allpass(buf, S2_A0_OFF, S2_A0, &allPos0, (c0 + c1) * 0.5f);
        float revR = s2_allpass(buf, S2_A1_OFF, S2_A1, &allPos1, (c2 + c3) * 0.5f);
        float wetL = shiftedL * fifthGain * 0.5f + revL * 0.62f;
        float wetR = shiftedR * fifthGain * 0.5f + revR * 0.62f;
        fxBuf[i] = dryGain * inL + wetGain * wetL;
        fxBuf[i + 8] = dryGain * inR + wetGain * wetR;
    }

    st->pitchWrite = pitchWrite;
    st->combPos[0] = combPos0;
    st->combPos[1] = combPos1;
    st->combPos[2] = combPos2;
    st->combPos[3] = combPos3;
    st->allPos[0] = allPos0;
    st->allPos[1] = allPos1;
    st->pitchPhase = s2_flush(pitchPhase);
    st->combLp[0] = s2_flush(lp0);
    st->combLp[1] = s2_flush(lp1);
    st->combLp[2] = s2_flush(lp2);
    st->combLp[3] = s2_flush(lp3);
}
