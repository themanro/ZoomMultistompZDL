/*
 * Bloom: diffuse stereo reverb with a transient-triggered swelling wet attack.
 *
 * This is an original Midiverb II Bloom-patch-inspired approximation, not a
 * recovered Alesis algorithm. A short predelay feeds four damped combs per
 * channel and three allpass diffusers. A phrase-onset envelope fades the wet
 * field in, while the feedback filters darken the long tail.
 *
 * All persistent state and delay memory live in the validated ctx[3] arena.
 * Large buffers clear incrementally. No math helpers, runtime divide/modulo,
 * switch/jump tables, or static audio state. ctx[11]/ctx[12] magic shuttle
 * preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "bloom_params.h"

#ifndef BLOOM_AUDIO_FUNC
#define BLOOM_AUDIO_FUNC Fx_REV_Bloom
#endif

#define BLOOM_DO_PRAGMA(x) _Pragma(#x)
#define BLOOM_EXPAND_PRAGMA(x) BLOOM_DO_PRAGMA(x)
#define BLOOM_CODE_SECTION(f) BLOOM_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

BLOOM_CODE_SECTION(BLOOM_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define BL_MAGIC       0x424C4F4Du /* 'BLOM' */
#define BL_VERSION     1u

#define BL_PRE         8192u
#define BL_PRE_MASK    (BL_PRE - 1u)

#define BL_C0          1116u
#define BL_C1          1277u
#define BL_C2          1422u
#define BL_C3          1491u
#define BL_C4          1188u
#define BL_C5          1356u
#define BL_C6          1491u
#define BL_C7          1557u

#define BL_A0          149u
#define BL_A1          211u
#define BL_A2          263u
#define BL_A3          173u
#define BL_A4          239u
#define BL_A5          293u

#define BL_PRE_L       0u
#define BL_PRE_R       (BL_PRE_L + BL_PRE)
#define BL_C0_OFF      (BL_PRE_R + BL_PRE)
#define BL_C1_OFF      (BL_C0_OFF + BL_C0)
#define BL_C2_OFF      (BL_C1_OFF + BL_C1)
#define BL_C3_OFF      (BL_C2_OFF + BL_C2)
#define BL_C4_OFF      (BL_C3_OFF + BL_C3)
#define BL_C5_OFF      (BL_C4_OFF + BL_C4)
#define BL_C6_OFF      (BL_C5_OFF + BL_C5)
#define BL_C7_OFF      (BL_C6_OFF + BL_C6)
#define BL_A0_OFF      (BL_C7_OFF + BL_C7)
#define BL_A1_OFF      (BL_A0_OFF + BL_A0)
#define BL_A2_OFF      (BL_A1_OFF + BL_A1)
#define BL_A3_OFF      (BL_A2_OFF + BL_A2)
#define BL_A4_OFF      (BL_A3_OFF + BL_A3)
#define BL_A5_OFF      (BL_A4_OFF + BL_A4)
#define BL_TOTAL       (BL_A5_OFF + BL_A5)

#define BL_CLEAR_STEP  4096u
#define BL_DENORM      1.0e-18f
#define BL_ONSET_FAST  0.02f
#define BL_ONSET_SLOW  0.0008f
#define BL_ONSET_MUL   1.7f
#define BL_ONSET_ADD   0.008f
#define BL_ARM_FRAC    0.08f
#define BL_PEAK_DEC    0.99995465f
#define BL_COOLDOWN    11025

typedef struct BloomState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearIndex;
    uint32_t prePos;
    uint32_t combPos[8];
    uint32_t allPos[6];
    float combLp[8];
    float envFast;
    float envSlow;
    float peak;
    float swell;
    uint32_t armed;
    int32_t cooldown;
} BloomState;

static inline float bl_abs(float x)
{
    return x < 0.0f ? -x : x;
}

static inline float bl_flush(float x)
{
    if (x > -BL_DENORM && x < BL_DENORM) return 0.0f;
    return x;
}

static inline float bl_allpass(float *buf, uint32_t off, uint32_t len,
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

void BLOOM_AUDIO_FUNC(unsigned int *ctx)
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
    if (base == 0u || end <= base || (base & 3u) != 0u) return;

    uintptr_t bufBase = (base + sizeof(BloomState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)BL_TOTAL * sizeof(float) > end) return;

    BloomState *st = (BloomState *)base;
    float *buf = (float *)bufBase;
    if (st->magic != BL_MAGIC || st->version != BL_VERSION) {
        st->magic = BL_MAGIC;
        st->version = BL_VERSION;
        st->initialized = 0u;
        st->clearIndex = 0u;
        st->prePos = 0u;
        for (int i = 0; i < 8; i++) {
            st->combPos[i] = 0u;
            st->combLp[i] = 0.0f;
        }
        for (int i = 0; i < 6; i++) st->allPos[i] = 0u;
        st->envFast = 0.0f;
        st->envSlow = 0.0f;
        st->peak = 0.0f;
        st->swell = 0.0f;
        st->armed = 1u;
        st->cooldown = 0;
    }

    if (!st->initialized) {
        uint32_t stop = st->clearIndex + BL_CLEAR_STEP;
        if (stop > BL_TOTAL) stop = BL_TOTAL;
        for (uint32_t i = st->clearIndex; i < stop; i++) buf[i] = 0.0f;
        st->clearIndex = stop;
        if (stop >= BL_TOTAL) st->initialized = 1u;
        return;
    }

    float bloom = zoom_param_norm01(params[BLOOM_BLOOM_SLOT], BLOOM_BLOOM_DEFAULT_NORM);
    float decay = zoom_param_norm01(params[BLOOM_DECAY_SLOT], BLOOM_DECAY_DEFAULT_NORM);
    float tone  = zoom_param_norm01(params[BLOOM_TONE_SLOT], BLOOM_TONE_DEFAULT_NORM);
    float mix   = zoom_param_norm01(params[BLOOM_MIX_SLOT], BLOOM_MIX_DEFAULT_NORM);

    /* Bloom's one-pole attack spans roughly 0.4 to 3.5 seconds. */
    float invBloom = 1.0f - bloom;
    float bloomK = 0.00065f + 0.0022f * invBloom * invBloom;

    /* Feedback stays below unity; upper decay settings produce long tails. */
    float feedback = 0.76f + 0.233f * decay;
    float damp = 0.08f + 0.78f * (1.0f - tone);
    float dampInv = 1.0f - damp;
    float wetGain = 1.10f;
    float dryGain = 1.0f - mix;

    uint32_t prePos = st->prePos;
    float envFast = st->envFast;
    float envSlow = st->envSlow;
    float peak = st->peak;
    float swell = st->swell;
    uint32_t armed = st->armed;
    int32_t cooldown = st->cooldown;

    for (int i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];
        float mono = 0.5f * (bl_abs(inL) + bl_abs(inR));

        envFast += BL_ONSET_FAST * (mono - envFast);
        envSlow += BL_ONSET_SLOW * (mono - envSlow);
        peak = (envFast > peak) ? envFast : peak * BL_PEAK_DEC;
        if (envFast < peak * BL_ARM_FRAC) armed = 1u;
        if (armed && cooldown <= 0 && envFast > envSlow * BL_ONSET_MUL + BL_ONSET_ADD) {
            swell = 0.0f;
            cooldown = BL_COOLDOWN;
            armed = 0u;
        }
        if (cooldown > 0) cooldown--;
        swell += bloomK * (1.0f - swell);

        uint32_t readPos = (prePos + 1u) & BL_PRE_MASK;
        float preL = buf[BL_PRE_L + readPos];
        float preR = buf[BL_PRE_R + readPos];
        buf[BL_PRE_L + prePos] = inL;
        buf[BL_PRE_R + prePos] = inR;
        prePos = (prePos + 1u) & BL_PRE_MASK;

        float sumL = 0.0f;
        float sumR = 0.0f;

        float y0 = buf[BL_C0_OFF + st->combPos[0]];
        float lp0 = st->combLp[0] * damp + y0 * dampInv;
        st->combLp[0] = lp0;
        buf[BL_C0_OFF + st->combPos[0]] = preL + lp0 * feedback;
        st->combPos[0] = (st->combPos[0] + 1u >= BL_C0) ? 0u : st->combPos[0] + 1u;
        sumL += y0;

        float y1 = buf[BL_C1_OFF + st->combPos[1]];
        float lp1 = st->combLp[1] * damp + y1 * dampInv;
        st->combLp[1] = lp1;
        buf[BL_C1_OFF + st->combPos[1]] = preL + lp1 * feedback;
        st->combPos[1] = (st->combPos[1] + 1u >= BL_C1) ? 0u : st->combPos[1] + 1u;
        sumL += y1;

        float y2 = buf[BL_C2_OFF + st->combPos[2]];
        float lp2 = st->combLp[2] * damp + y2 * dampInv;
        st->combLp[2] = lp2;
        buf[BL_C2_OFF + st->combPos[2]] = preL + lp2 * feedback;
        st->combPos[2] = (st->combPos[2] + 1u >= BL_C2) ? 0u : st->combPos[2] + 1u;
        sumL += y2;

        float y3 = buf[BL_C3_OFF + st->combPos[3]];
        float lp3 = st->combLp[3] * damp + y3 * dampInv;
        st->combLp[3] = lp3;
        buf[BL_C3_OFF + st->combPos[3]] = preL + lp3 * feedback;
        st->combPos[3] = (st->combPos[3] + 1u >= BL_C3) ? 0u : st->combPos[3] + 1u;
        sumL += y3;

        float y4 = buf[BL_C4_OFF + st->combPos[4]];
        float lp4 = st->combLp[4] * damp + y4 * dampInv;
        st->combLp[4] = lp4;
        buf[BL_C4_OFF + st->combPos[4]] = preR + lp4 * feedback;
        st->combPos[4] = (st->combPos[4] + 1u >= BL_C4) ? 0u : st->combPos[4] + 1u;
        sumR += y4;

        float y5 = buf[BL_C5_OFF + st->combPos[5]];
        float lp5 = st->combLp[5] * damp + y5 * dampInv;
        st->combLp[5] = lp5;
        buf[BL_C5_OFF + st->combPos[5]] = preR + lp5 * feedback;
        st->combPos[5] = (st->combPos[5] + 1u >= BL_C5) ? 0u : st->combPos[5] + 1u;
        sumR += y5;

        float y6 = buf[BL_C6_OFF + st->combPos[6]];
        float lp6 = st->combLp[6] * damp + y6 * dampInv;
        st->combLp[6] = lp6;
        buf[BL_C6_OFF + st->combPos[6]] = preR + lp6 * feedback;
        st->combPos[6] = (st->combPos[6] + 1u >= BL_C6) ? 0u : st->combPos[6] + 1u;
        sumR += y6;

        float y7 = buf[BL_C7_OFF + st->combPos[7]];
        float lp7 = st->combLp[7] * damp + y7 * dampInv;
        st->combLp[7] = lp7;
        buf[BL_C7_OFF + st->combPos[7]] = preR + lp7 * feedback;
        st->combPos[7] = (st->combPos[7] + 1u >= BL_C7) ? 0u : st->combPos[7] + 1u;
        sumR += y7;

        float wetL = sumL * 0.25f;
        wetL = bl_allpass(buf, BL_A0_OFF, BL_A0, &st->allPos[0], wetL);
        wetL = bl_allpass(buf, BL_A1_OFF, BL_A1, &st->allPos[1], wetL);
        wetL = bl_allpass(buf, BL_A2_OFF, BL_A2, &st->allPos[2], wetL);
        float wetR = sumR * 0.25f;
        wetR = bl_allpass(buf, BL_A3_OFF, BL_A3, &st->allPos[3], wetR);
        wetR = bl_allpass(buf, BL_A4_OFF, BL_A4, &st->allPos[4], wetR);
        wetR = bl_allpass(buf, BL_A5_OFF, BL_A5, &st->allPos[5], wetR);

        float bloomWet = wetGain * swell * mix;
        fxBuf[i] = dryGain * inL + bloomWet * wetL;
        fxBuf[i + 8] = dryGain * inR + bloomWet * wetR;
    }

    st->prePos = prePos;
    st->envFast = bl_flush(envFast);
    st->envSlow = bl_flush(envSlow);
    st->peak = bl_flush(peak);
    st->swell = bl_flush(swell);
    st->armed = armed;
    st->cooldown = cooldown;
}
