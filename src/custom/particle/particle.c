/*
 * particle.c -- Particle: real-time granular delay / pitch texture.
 *
 * A two-head, windowed grain reader plays from a mono delay ring. Mode
 * selects five delay textures and three pitch textures; the front-panel
 * approximation exposes Size, Density, Delay, Feedback, and Mix alongside it.
 * It is an original approximation, not a port of Red Panda firmware.
 *
 * Safe-DSP: no math library, runtime divide, modulo, switch/jump table, or
 * writable static data. The ring lives in the ctx[3] arena; ctx[11]/ctx[12]
 * magic shuttle is preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "particle_params.h"

#ifndef PARTICLE_AUDIO_FUNC
#define PARTICLE_AUDIO_FUNC Fx_DLY_Particle
#endif

#define PA_DO_PRAGMA(x) _Pragma(#x)
#define PA_EXPAND_PRAGMA(x) PA_DO_PRAGMA(x)
#define PA_CODE_SECTION(f) PA_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

PA_CODE_SECTION(PARTICLE_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define PA_MAGIC       0x50525443u   /* 'PRTC' */
#define PA_VERSION     1u
#define PA_RING_SIZE   131072u
#define PA_RING_MASK   (PA_RING_SIZE - 1u)
#define PA_RING_BYTES  (PA_RING_SIZE * 4u)
#define PA_CLEAR_CHUNK 1024u
#define PA_SR_INV      0.000022675737f
#define PA_TWO_PI      6.28318530718f
#define PA_DENORM      1.0e-18f

typedef struct ParticleState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearPos;
    uint32_t writePos;
    uint32_t rng;
    int32_t  timer;
    int32_t  ageA;
    int32_t  ageB;
    uint32_t nextHead;
    float    posA;
    float    posB;
    float    rateA;
    float    rateB;
    float    lfoPhase;
    float    lastWet;
} ParticleState;

static inline float pa_abs(float x)
{
    return x < 0.0f ? -x : x;
}

static inline uint32_t pa_random(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static inline float pa_unit_random(uint32_t *state)
{
    return (float)(pa_random(state) >> 8) * (1.0f / 16777216.0f);
}

static inline float pa_recip(float x)
{
    union { float f; uint32_t u; } v, y;
    v.f = x;
    y.u = 0x7EF311C3u - v.u;
    y.f = y.f * (2.0f - x * y.f);
    y.f = y.f * (2.0f - x * y.f);
    return y.f;
}

static inline float pa_wrap(float pos)
{
    if (pos >= (float)PA_RING_SIZE) pos -= (float)PA_RING_SIZE;
    if (pos < 0.0f) pos += (float)PA_RING_SIZE;
    return pos;
}

static inline float pa_read(const float *buf, float pos)
{
    int32_t i0 = (int32_t)pos;
    float frac = pos - (float)i0;
    uint32_t a = ((uint32_t)i0) & PA_RING_MASK;
    uint32_t b = (a + 1u) & PA_RING_MASK;
    return buf[a] * (1.0f - frac) + buf[b] * frac;
}

static inline float pa_clip(float x)
{
    if (x > 1.5f) return 1.0f;
    if (x < -1.5f) return -1.0f;
    return x - x * x * x * 0.1481481481f;
}

static inline float pa_triangle(float phase)
{
    if (phase < 0.5f) return 4.0f * phase - 1.0f;
    return 3.0f - 4.0f * phase;
}

static inline void pa_start_grain(
    ParticleState *st, int32_t grain, float delay,
    float density, int32_t mode, float lfo,
    uint32_t *rng)
{
    float offset = 0.0f;
    float rate = 1.0f;
    float unit = pa_unit_random(rng);

    if (mode == 0) {
        offset = (unit - 0.5f) * density * (float)grain;
    } else if (mode == 2) {
        offset = lfo * density * (float)grain * 0.5f;
    } else if (mode == 3) {
        if (unit < 0.2f) rate = 0.5f;
        else if (unit < 0.4f) rate = 0.66742f;
        else if (unit < 0.6f) rate = 1.0f;
        else if (unit < 0.8f) rate = 1.498307f;
        else rate = 2.0f;
    } else if (mode == 5) {
        rate = (st->nextHead & 1u) ? 1.02f : 0.98f;
    } else if (mode == 6) {
        if (unit < 0.33f) rate = 0.95f;
        else if (unit < 0.67f) rate = 1.0f;
        else rate = 1.05f;
    } else if (mode == 7) {
        rate = 1.0f + lfo * density * 0.12f;
    }

    float start = (float)st->writePos - delay + offset;
    if (mode == 4) {
        start += (float)grain;
        rate = -1.0f;
    }
    start = pa_wrap(start);

    if (st->nextHead == 0u) {
        st->posA = start;
        st->rateA = rate;
        st->ageA = 0;
    } else {
        st->posB = start;
        st->rateB = rate;
        st->ageB = 0;
    }
    st->nextHead ^= 1u;
}

void PARTICLE_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf = ZDL_PTR(float *, ctx[5]);

    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile unsigned int *desc = ZDL_PTR(volatile unsigned int *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end = (uintptr_t)desc[1];
    if (base == 0u || end <= base || (base & 3u) != 0u) return;

    uintptr_t bufBase = (base + sizeof(ParticleState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)PA_RING_BYTES > end) return;

    ParticleState *st = (ParticleState *)base;
    float *buf = (float *)bufBase;

    if (st->magic != PA_MAGIC || st->version != PA_VERSION || !st->initialized) {
        st->magic = PA_MAGIC;
        st->version = PA_VERSION;
        st->initialized = 1u;
        st->clearPos = 0u;
        st->writePos = 0u;
        st->rng = 0x9e3779b9u;
        st->timer = 0;
        st->ageA = 0x7fffffff;
        st->ageB = 0x7fffffff;
        st->nextHead = 0u;
        st->posA = 0.0f;
        st->posB = 0.0f;
        st->rateA = 1.0f;
        st->rateB = 1.0f;
        st->lfoPhase = 0.0f;
        st->lastWet = 0.0f;
    }

    if (st->clearPos < PA_RING_SIZE) {
        uint32_t stop = st->clearPos + PA_CLEAR_CHUNK;
        if (stop > PA_RING_SIZE) stop = PA_RING_SIZE;
        for (uint32_t i = st->clearPos; i < stop; ++i) buf[i] = 0.0f;
        st->clearPos = stop;
    }

    float mode = zoom_param_norm01(params[PARTICLE_MODE_SLOT], PARTICLE_MODE_DEFAULT_NORM);
    float size = zoom_param_norm01(params[PARTICLE_SIZE_SLOT], PARTICLE_SIZE_DEFAULT_NORM);
    float density = zoom_param_norm01(params[PARTICLE_DENSITY_SLOT], PARTICLE_DENSITY_DEFAULT_NORM);
    float delayN = zoom_param_norm01(params[PARTICLE_DELAY_SLOT], PARTICLE_DELAY_DEFAULT_NORM);
    float feedback = zoom_param_norm01(params[PARTICLE_FEEDBACK_SLOT], PARTICLE_FEEDBACK_DEFAULT_NORM);
    float mix = zoom_param_norm01(params[PARTICLE_MIX_SLOT], PARTICLE_MIX_DEFAULT_NORM);

    int32_t modeIndex = 0;
    if      (mode >= 0.125f) modeIndex = 1;
    if      (mode >= 0.250f) modeIndex = 2;
    if      (mode >= 0.375f) modeIndex = 3;
    if      (mode >= 0.500f) modeIndex = 4;
    if      (mode >= 0.625f) modeIndex = 5;
    if      (mode >= 0.750f) modeIndex = 6;
    if      (mode >= 0.875f) modeIndex = 7;

    float grainF = 256.0f + size * 7936.0f;
    float invGrain = pa_recip(grainF);
    int32_t grain = (int32_t)grainF;
    if (grain < 256) grain = 256;
    else if (grain > 8192) grain = 8192;
    int32_t hop = (int32_t)(grainF * (0.75f - density * 0.25f));
    if (hop < grain / 2) hop = grain / 2;
    if (hop < 1) hop = 1;

    float delay = (float)grain + 256.0f + delayN * 98304.0f;
    if (delay > (float)(PA_RING_SIZE - grain - 256u))
        delay = (float)(PA_RING_SIZE - grain - 256u);
    float lfoRate = 0.15f + density * 5.85f;
    float lfoInc = lfoRate * PA_SR_INV;
    float wetMix = mix;
    float dryMix = 1.0f - mix;
    float fbGain = feedback * feedback * 0.92f;

    uint32_t wp = st->writePos;
    uint32_t rng = st->rng;
    int32_t timer = st->timer;
    int32_t ageA = st->ageA, ageB = st->ageB;
    float posA = st->posA, posB = st->posB;
    float rateA = st->rateA, rateB = st->rateB;
    float lfoPhase = st->lfoPhase;
    float lastWet = st->lastWet;

    for (int32_t i = 0; i < 8; ++i) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];
        float input = 0.5f * (inL + inR);
        float lfo = pa_triangle(lfoPhase);
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        if (timer <= 0) {
            st->writePos = wp;
            st->posA = posA;
            st->posB = posB;
            st->rateA = rateA;
            st->rateB = rateB;
            st->ageA = ageA;
            st->ageB = ageB;
            pa_start_grain(st, grain, delay, density, modeIndex, lfo, &rng);
            posA = st->posA;
            posB = st->posB;
            rateA = st->rateA;
            rateB = st->rateB;
            ageA = st->ageA;
            ageB = st->ageB;
            timer = hop;
        }
        --timer;

        float sum = 0.0f, weightSum = 0.0f;
        if (ageA < grain) {
            float phaseA = (float)ageA * invGrain;
            float wA = phaseA < 0.5f ? phaseA * 2.0f : (1.0f - phaseA) * 2.0f;
            float readA = pa_wrap(posA + (modeIndex == 2
                                           ? lfo * density * (float)grain * 0.5f : 0.0f));
            sum += pa_read(buf, readA) * wA;
            weightSum += wA;
            posA = pa_wrap(posA + rateA);
            ++ageA;
        }
        if (ageB < grain) {
            float phaseB = (float)ageB * invGrain;
            float wB = phaseB < 0.5f ? phaseB * 2.0f : (1.0f - phaseB) * 2.0f;
            float readB = pa_wrap(posB + (modeIndex == 2
                                           ? lfo * density * (float)grain * 0.5f : 0.0f));
            sum += pa_read(buf, readB) * wB;
            weightSum += wB;
            posB = pa_wrap(posB + rateB);
            ++ageB;
        }
        float wet = weightSum > 1.0e-4f ? sum * pa_recip(weightSum) : 0.0f;
        if (pa_abs(wet) < PA_DENORM) wet = 0.0f;

        buf[wp] = pa_clip(input + lastWet * fbGain);
        wp = (wp + 1u) & PA_RING_MASK;
        lastWet = wet;

        fxBuf[i] = dryMix * inL + wetMix * wet;
        fxBuf[i + 8] = dryMix * inR + wetMix * wet;
    }

    st->writePos = wp;
    st->rng = rng;
    st->timer = timer;
    st->ageA = ageA;
    st->ageB = ageB;
    st->posA = posA;
    st->posB = posB;
    st->rateA = rateA;
    st->rateB = rateB;
    st->lfoPhase = lfoPhase;
    st->lastWet = lastWet;
}
