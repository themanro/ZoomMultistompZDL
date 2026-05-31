/*
 * TapeEcho4 -- custom Airwindows-inspired tape echo for Zoom Multistomp.
 *
 * This is not a 1:1 Airwindows port. It borrows safe, float32-friendly ideas
 * from TapeDelay/TapeDelay2, TapeHack, FromTape, and ToTape9:
 *   - feedback-path delay memory in the proven ctx[3] host arena
 *   - TapeHack-style polynomial soft clipping in the record path
 *   - Galaxy-derived tape/head FIR plus adjustable wear filtering
 *   - wow/flutter delay-time modulation
 *   - compact mono spring tank modeled from a Galaxy spring IR
 *   - BPM + musical division delay timing without libm/runtime helpers
 *
 * The first parameter descriptor is marked with the stock tempo flag pattern.
 * Whether the Zoom host can feed true tap-tempo values to custom ZDLs is still
 * unproven, so the DSP also exposes a direct BPM knob.
 */

#include <stdint.h>

#include "tapeecho4_params.h"

#ifndef TAPEECHO4_AUDIO_FUNC
#define TAPEECHO4_AUDIO_FUNC Fx_DLY_TapeEcho4
#endif

#define TAPEECHO4_DO_PRAGMA(x) _Pragma(#x)
#define TAPEECHO4_EXPAND_PRAGMA(x) TAPEECHO4_DO_PRAGMA(x)
#define TAPEECHO4_CODE_SECTION(func) TAPEECHO4_EXPAND_PRAGMA(CODE_SECTION(func, ".audio"))
#define TAPEECHO4_ALWAYS_INLINE(func) TAPEECHO4_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(func))
TAPEECHO4_CODE_SECTION(TAPEECHO4_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define TAPEECHO4_MAGIC 0x54453434u
#define TAPEECHO4_VERSION 4u
#define TAPEECHO4_CLEAR_STEP 1024u
#define TAPEECHO4_DELAY_SAMPLES 65536u
#define TAPEECHO4_DELAY_MASK (TAPEECHO4_DELAY_SAMPLES - 1u)
#define TAPEECHO4_MAX_DELAY_F 65500.0f
#define TAPEECHO4_MIN_DELAY_F 48.0f
#define TAPEECHO4_TWO_PI 6.2831853f
#define TAPEECHO4_PI 3.1415927f
#define TAPEECHO4_HALF_PI 1.5707963f
#define TAPEECHO4_CLIP_LIMIT 2.305929f
#define TAPEECHO4_RAW_MAX 0.14f
#define TAPEECHO4_RAW_TO_NORM 7.1428571f
#define TAPEECHO4_FIR_TAPS 64u
#define TAPEECHO4_SPRING_SAMPLES 8192u
#define TAPEECHO4_SPRING_MASK (TAPEECHO4_SPRING_SAMPLES - 1u)

/* Short causal FIR derived from tools/measure/Tape/Tape_IR.wav. The source IR
 * was captured from UAD Galaxy Tape Echo at 48 kHz, resampled to the pedal's
 * 44.1 kHz rate, truncated to 64 taps, and normalized at 1 kHz. This retains
 * the measured head/tape fingerprint while keeping the callback cost bounded. */
static const float te4_galaxy_ir[TAPEECHO4_FIR_TAPS] = {
    -0.579352892f, -0.357666011f, -0.035518767f,  0.091870312f,
     0.094054828f,  0.075074592f,  0.081397933f,  0.070719928f,
     0.049016628f,  0.038551856f,  0.043788628f,  0.047632244f,
     0.044524741f,  0.041661185f,  0.042265846f,  0.042692996f,
     0.040974830f,  0.039087825f,  0.038385600f,  0.037973640f,
     0.036995818f,  0.035863044f,  0.035069156f,  0.034404054f,
     0.033608670f,  0.032740797f,  0.031959075f,  0.031268058f,
     0.030557181f,  0.029828788f,  0.029136179f,  0.028463772f,
     0.027804472f,  0.027151666f,  0.026512750f,  0.025903448f,
     0.025289896f,  0.024680621f,  0.024102040f,  0.023536670f,
     0.022982173f,  0.022445717f,  0.021901044f,  0.021379827f,
     0.020861948f,  0.020359641f,  0.019861130f,  0.019366002f,
     0.018889874f,  0.018418834f,  0.017971537f,  0.017514992f,
     0.017078913f,  0.016650138f,  0.016238715f,  0.015835252f,
     0.015427290f,  0.015037479f,  0.014637418f,  0.014270834f,
     0.013888030f,  0.013531184f,  0.013165458f,  0.012817223f,
};

typedef struct TapeEcho4State {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearIndex;

    float delayL[TAPEECHO4_DELAY_SAMPLES];
    float delayR[TAPEECHO4_DELAY_SAMPLES];

    uint32_t writeIndex;
    float flutterPhaseL;
    float flutterPhaseR;
    float wowPhaseL;
    float wowPhaseR;
    float hpL;
    float hpR;
    float lpL;
    float lpR;
    float firL[TAPEECHO4_FIR_TAPS];
    float firR[TAPEECHO4_FIR_TAPS];
    uint32_t firIndex;
    float springDelay[TAPEECHO4_SPRING_SAMPLES];
    uint32_t springIndex;
    float springDamp;
    float prevDelay;
    uint32_t fpdL;
    uint32_t fpdR;
} TapeEcho4State;

TAPEECHO4_ALWAYS_INLINE(align4)
static inline uintptr_t align4(uintptr_t x)
{
    return (x + 3u) & ~(uintptr_t)3u;
}

TAPEECHO4_ALWAYS_INLINE(te4_clampf)
static inline float te4_clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

TAPEECHO4_ALWAYS_INLINE(te4_param_norm)
static inline float te4_param_norm(float raw, float fallback_norm, int group_empty)
{
    if (raw != raw) return te4_clampf(fallback_norm, 0.0f, 1.0f);
    if (raw < 0.0f) return te4_clampf(fallback_norm, 0.0f, 1.0f);
    if (raw <= 0.0001f) return group_empty ? te4_clampf(fallback_norm, 0.0f, 1.0f) : 0.0f;
    if (raw <= (TAPEECHO4_RAW_MAX * 1.1f)) return te4_clampf(raw * TAPEECHO4_RAW_TO_NORM, 0.0f, 1.0f);
    if (raw <= 1.0f) return te4_clampf(raw, 0.0f, 1.0f);
    if (raw <= 100.0f) return te4_clampf(raw * 0.01f, 0.0f, 1.0f);
    if (raw <= 240.0f) return te4_clampf(raw * 0.0041666667f, 0.0f, 1.0f);
    return te4_clampf(fallback_norm, 0.0f, 1.0f);
}

TAPEECHO4_ALWAYS_INLINE(recip_approx_pos)
static inline float recip_approx_pos(float x)
{
    union { float f; uint32_t u; } conv;
    conv.f = x;
    conv.u = 0x7EF311C3u - conv.u;
    float y = conv.f;
    y = y * (2.0f - x * y);
    y = y * (2.0f - x * y);
    return y * (2.0f - x * y);
}

TAPEECHO4_ALWAYS_INLINE(sin_approx)
static inline float sin_approx(float x)
{
    x = x - TAPEECHO4_TWO_PI * (float)((int)(x * 0.15915494f));
    if (x < 0.0f) x += TAPEECHO4_TWO_PI;
    float sign = 1.0f;
    if (x > TAPEECHO4_PI) {
        x -= TAPEECHO4_PI;
        sign = -1.0f;
    }
    if (x > TAPEECHO4_HALF_PI) x = TAPEECHO4_PI - x;

    float x2 = x * x;
    float y = x;
    y -= x * x2 * 0.16666667f;
    x *= x2;
    y += x * x2 * 0.0083333310f;
    x *= x2;
    y -= x * x2 * 0.0001984090f;
    return y * sign;
}

TAPEECHO4_ALWAYS_INLINE(tape_saturate)
static inline float tape_saturate(float x)
{
    x = te4_clampf(x, -TAPEECHO4_CLIP_LIMIT, TAPEECHO4_CLIP_LIMIT);
    float xx = x * x;
    float p = x * xx;
    float y = x;
    y -= p * 0.16666667f;
    p *= xx;
    y += p * 0.014492754f;
    p *= xx;
    y -= p * 0.000395244f;
    p *= xx;
    y += p * 0.00000444473f;
    p *= xx;
    y -= p * 0.000000100208f;
    return y;
}

TAPEECHO4_ALWAYS_INLINE(te4_reset_header)
static inline void te4_reset_header(TapeEcho4State *st)
{
    st->magic = TAPEECHO4_MAGIC;
    st->version = TAPEECHO4_VERSION;
    st->initialized = 0u;
    st->clearIndex = 16u;
}

TAPEECHO4_ALWAYS_INLINE(te4_finish_init)
static inline void te4_finish_init(TapeEcho4State *st)
{
    st->writeIndex = 0u;
    st->flutterPhaseL = 0.0f;
    st->flutterPhaseR = 2.0943951f;
    st->wowPhaseL = 0.0f;
    st->wowPhaseR = 3.1415927f;
    st->hpL = st->hpR = 0.0f;
    st->lpL = st->lpR = 0.0f;
    st->firIndex = 0u;
    st->springIndex = 0u;
    st->springDamp = 0.0f;
    st->prevDelay = 22050.0f;
    st->fpdL = 0x1234567u;
    st->fpdR = 0x89ABCDFu;
    st->initialized = 1u;
}

TAPEECHO4_ALWAYS_INLINE(te4_clear_chunk)
static inline void te4_clear_chunk(TapeEcho4State *st)
{
    uint32_t *w = (uint32_t *)(void *)st;
    uint32_t startWord = st->clearIndex >> 2;
    uint32_t endWord = startWord + (TAPEECHO4_CLEAR_STEP >> 2);
    uint32_t totalWords = (uint32_t)(sizeof(TapeEcho4State) >> 2);
    uint32_t i;

    if (endWord > totalWords) endWord = totalWords;
    for (i = startWord; i < endWord; i++) {
        w[i] = 0u;
    }
    st->clearIndex = endWord << 2;
    if (endWord >= totalWords) {
        te4_finish_init(st);
    }
}

TAPEECHO4_ALWAYS_INLINE(te4_division)
static inline float te4_division(float divNorm)
{
    if (divNorm < 0.16666667f) return 0.25f;
    if (divNorm < 0.33333334f) return 0.33333334f;
    if (divNorm < 0.5f) return 0.5f;
    if (divNorm < 0.6666667f) return 0.75f;
    if (divNorm < 0.8333333f) return 1.0f;
    return 1.5f;
}

TAPEECHO4_ALWAYS_INLINE(te4_read_delay)
static inline float te4_read_delay(const float *delay, uint32_t writeIndex, float delaySamples)
{
    int wholeInt = (int)delaySamples;
    uint32_t whole = (uint32_t)wholeInt;
    float frac = delaySamples - (float)wholeInt;
    uint32_t idx0 = (writeIndex - whole) & TAPEECHO4_DELAY_MASK;
    uint32_t idx1 = (idx0 - 1u) & TAPEECHO4_DELAY_MASK;
    return delay[idx0] * (1.0f - frac) + delay[idx1] * frac;
}

TAPEECHO4_ALWAYS_INLINE(te4_tape_filter)
static inline float te4_tape_filter(float x, float *hpState, float *lpState,
                                    float hpCoef, float lpCoef)
{
    *hpState += (x - *hpState) * hpCoef;
    x -= *hpState;
    *lpState += (x - *lpState) * lpCoef;
    return *lpState;
}

TAPEECHO4_ALWAYS_INLINE(te4_galaxy_filter)
static inline float te4_galaxy_filter(float x, float *history, uint32_t index)
{
    float y = 0.0f;
    uint32_t tap;

    history[index] = x;
    for (tap = 0u; tap < TAPEECHO4_FIR_TAPS; tap++) {
        y += history[(index - tap) & (TAPEECHO4_FIR_TAPS - 1u)] * te4_galaxy_ir[tap];
    }
    return y;
}

TAPEECHO4_ALWAYS_INLINE(te4_spring_tank)
static inline float te4_spring_tank(TapeEcho4State *st, float input)
{
    uint32_t index = st->springIndex;
    float *delay = st->springDelay;

    /* Compact mono tank fitted by ear around the measured Galaxy spring IR:
     * identical L/R capture, useful decay past 1 s, and dense resonances
     * concentrated around 0.5 .. 1.8 kHz. The incommensurate feedback taps
     * create the characteristic metallic diffusion without a long FIR. */
    float loop = delay[(index - 2111u) & TAPEECHO4_SPRING_MASK] * 0.68f;
    loop += delay[(index - 3067u) & TAPEECHO4_SPRING_MASK] * 0.19f;
    loop -= delay[(index - 4093u) & TAPEECHO4_SPRING_MASK] * 0.12f;
    st->springDamp += (loop + input * 0.28f - st->springDamp) * 0.42f;
    delay[index] = te4_clampf(st->springDamp, -1.4f, 1.4f);
    st->springIndex = (index + 1u) & TAPEECHO4_SPRING_MASK;

    return delay[(index - 1453u) & TAPEECHO4_SPRING_MASK] * 0.42f +
           delay[(index - 2591u) & TAPEECHO4_SPRING_MASK] * 0.34f +
           delay[(index - 3761u) & TAPEECHO4_SPRING_MASK] * 0.24f;
}

TAPEECHO4_ALWAYS_INLINE(te4_process_sample)
static inline void te4_process_sample(TapeEcho4State *st, float *sampleL, float *sampleR,
                                      float baseDelay, float feedback, float flutter,
                                      float wow, float wear, float drive,
                                      float spring, float mix)
{
    float inputL = *sampleL;
    float inputR = *sampleR;
    float dryL = inputL;
    float dryR = inputR;

    if (inputL > -1.18e-23f && inputL < 1.18e-23f) inputL = (float)st->fpdL * 1.18e-17f;
    if (inputR > -1.18e-23f && inputR < 1.18e-23f) inputR = (float)st->fpdR * 1.18e-17f;

    /* Rates and depths calibrated to UAD Galaxy Tape Echo measurements
     * (1 kHz tone, fb=0, 100% wet, delays 69 ms and 487 ms).
     *
     * Model one physical transport shared by both channels. The captures show
     * two useful wow components around 1.49 Hz and 3.83 Hz, plus a smaller
     * flutter texture around 6.03 Hz and 7.70 Hz. Wow displacement scales with
     * tape travel: about 3.6 samples at 69 ms and 25.5 samples at 487 ms. */
    st->flutterPhaseL += 0.001096f;  /* 7.70 Hz at fs=44.1k */
    st->flutterPhaseR += 0.000860f;  /* 6.03 Hz */
    st->wowPhaseL += 0.000213f;      /* 1.49 Hz */
    st->wowPhaseR += 0.000546f;      /* 3.83 Hz */
    if (st->flutterPhaseL > TAPEECHO4_TWO_PI) st->flutterPhaseL -= TAPEECHO4_TWO_PI;
    if (st->flutterPhaseR > TAPEECHO4_TWO_PI) st->flutterPhaseR -= TAPEECHO4_TWO_PI;
    if (st->wowPhaseL > TAPEECHO4_TWO_PI) st->wowPhaseL -= TAPEECHO4_TWO_PI;
    if (st->wowPhaseR > TAPEECHO4_TWO_PI) st->wowPhaseR -= TAPEECHO4_TWO_PI;

    float travelNorm = te4_clampf((baseDelay - 3043.0f) * 0.00005425f, 0.0f, 1.0f);
    float flutterDepth = flutter * flutter * (0.35f + te4_clampf(baseDelay, 0.0f, 21477.0f) * 0.000007f);
    float wowDepth = wow * wow * baseDelay * 0.00118f;
    float flutterA = sin_approx(st->flutterPhaseL);
    float flutterB = sin_approx(st->flutterPhaseR);
    float wowA = sin_approx(st->wowPhaseL);
    float wowB = sin_approx(st->wowPhaseR);
    float transportMod = (flutterA * 0.7f + flutterB * 0.3f) * flutterDepth;
    transportMod += (wowA * (0.35f + travelNorm * 0.65f) +
                     wowB * (0.75f - travelNorm * 0.55f)) * wowDepth;
    float modL = transportMod;
    float modR = transportMod;

    float delayL = te4_clampf(baseDelay + modL, TAPEECHO4_MIN_DELAY_F, TAPEECHO4_MAX_DELAY_F);
    float delayR = te4_clampf(baseDelay + modR, TAPEECHO4_MIN_DELAY_F, TAPEECHO4_MAX_DELAY_F);
    float wetL = te4_read_delay(st->delayL, st->writeIndex, delayL);
    float wetR = te4_read_delay(st->delayR, st->writeIndex, delayR);

    /* The FIR carries Galaxy's baseline head/tape response. Wear adds an
     * adjustable age layer: progressively more bass cut and treble loss. */
    float hpCoef = 0.0005f + wear * wear * 0.0065f;
    float lpCoef = 1.0f - wear * wear * 0.38f;

    float recordGain = 1.0f + drive * 2.4f;
    float recL = inputL + wetL * feedback;
    float recR = inputR + wetR * feedback;
    recL = tape_saturate(recL * recordGain) * recip_approx_pos(recordGain);
    recR = tape_saturate(recR * recordGain) * recip_approx_pos(recordGain);
    recL = te4_galaxy_filter(recL, st->firL, st->firIndex);
    recR = te4_galaxy_filter(recR, st->firR, st->firIndex);
    st->firIndex = (st->firIndex + 1u) & (TAPEECHO4_FIR_TAPS - 1u);
    recL = te4_tape_filter(recL, &st->hpL, &st->lpL, hpCoef, lpCoef);
    recR = te4_tape_filter(recR, &st->hpR, &st->lpR, hpCoef, lpCoef);

    st->delayL[st->writeIndex] = recL;
    st->delayR[st->writeIndex] = recR;
    st->writeIndex = (st->writeIndex + 1u) & TAPEECHO4_DELAY_MASK;

    float springReturn = te4_spring_tank(st, (inputL + inputR) * spring);
    *sampleL = dryL * (1.0f - mix) + (wetL + springReturn) * mix;
    *sampleR = dryR * (1.0f - mix) + (wetR + springReturn) * mix;

    st->fpdL ^= st->fpdL << 13;
    st->fpdL ^= st->fpdL >> 17;
    st->fpdL ^= st->fpdL << 5;
    st->fpdR ^= st->fpdR << 13;
    st->fpdR ^= st->fpdR >> 17;
    st->fpdR ^= st->fpdR << 5;
}

void TAPEECHO4_AUDIO_FUNC(unsigned int *ctx)
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
    unsigned int span = desc[2];
    uintptr_t stateBase = align4(base);
    uintptr_t requiredEnd = stateBase + sizeof(TapeEcho4State);
    uintptr_t bytes = end - base;

    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u || (end & 3u) != 0u || (span & 3u) != 0u) return;
    if (bytes < sizeof(TapeEcho4State) || span < bytes) return;
    if (requiredEnd > end) return;

    TapeEcho4State *st = (TapeEcho4State *)stateBase;
    if (st->magic != TAPEECHO4_MAGIC || st->version != TAPEECHO4_VERSION) {
        te4_reset_header(st);
        return;
    }
    if (!st->initialized) {
        te4_clear_chunk(st);
        return;
    }

    int page1Empty = (params[TAPEECHO4_TEMPO_SLOT] <= 0.0001f &&
                      params[TAPEECHO4_DIV_SLOT] <= 0.0001f &&
                      params[TAPEECHO4_FEED_SLOT] <= 0.0001f);
    int page2Empty = (params[TAPEECHO4_FLUTTER_SLOT] <= 0.0001f &&
                      params[TAPEECHO4_WOW_SLOT] <= 0.0001f &&
                      params[TAPEECHO4_WEAR_SLOT] <= 0.0001f);
    int page3Empty = (params[TAPEECHO4_DRIVE_SLOT] <= 0.0001f &&
                      params[TAPEECHO4_SPRING_SLOT] <= 0.0001f &&
                      params[TAPEECHO4_MIX_SLOT] <= 0.0001f);

    float tempoNorm = te4_param_norm(params[TAPEECHO4_TEMPO_SLOT], TAPEECHO4_TEMPO_DEFAULT_NORM, page1Empty);
    float divNorm = te4_param_norm(params[TAPEECHO4_DIV_SLOT], TAPEECHO4_DIV_DEFAULT_NORM, page1Empty);
    float feedNorm = te4_param_norm(params[TAPEECHO4_FEED_SLOT], TAPEECHO4_FEED_DEFAULT_NORM, page1Empty);
    float flutter = te4_param_norm(params[TAPEECHO4_FLUTTER_SLOT], TAPEECHO4_FLUTTER_DEFAULT_NORM, page2Empty);
    float wow = te4_param_norm(params[TAPEECHO4_WOW_SLOT], TAPEECHO4_WOW_DEFAULT_NORM, page2Empty);
    float wear = te4_param_norm(params[TAPEECHO4_WEAR_SLOT], TAPEECHO4_WEAR_DEFAULT_NORM, page2Empty);
    float drive = te4_param_norm(params[TAPEECHO4_DRIVE_SLOT], TAPEECHO4_DRIVE_DEFAULT_NORM, page3Empty);
    float spring = te4_param_norm(params[TAPEECHO4_SPRING_SLOT], TAPEECHO4_SPRING_DEFAULT_NORM, page3Empty);
    float mix = te4_param_norm(params[TAPEECHO4_MIX_SLOT], TAPEECHO4_MIX_DEFAULT_NORM, page3Empty);

    float bpm = 40.0f + tempoNorm * 160.0f;
    float beatSamples = 2646000.0f * recip_approx_pos(bpm);
    float baseDelay = beatSamples * te4_division(divNorm);
    baseDelay = te4_clampf(baseDelay, TAPEECHO4_MIN_DELAY_F, TAPEECHO4_MAX_DELAY_F);
    float feedback = feedNorm * feedNorm * 0.92f;

    int i;
    for (i = 0; i < 8; i++) {
        float sL = fxBuf[i];
        float sR = fxBuf[i + 8];
        te4_process_sample(st, &sL, &sR, baseDelay, feedback, flutter, wow, wear, drive, spring, mix);
        fxBuf[i] = sL;
        fxBuf[i + 8] = sR;
    }
}
