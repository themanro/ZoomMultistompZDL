/*
 * BeatScat: automatic tempo-grid beat scatter for the Zoom MS-70CDR.
 *
 * Each selected grid cycle transforms the previous captured division as
 * stutter, reverse, or quarter-rotated playback. Pattern chooses every
 * division, alternating divisions, or the same alternation with opposite
 * phase. This is an original compact design, not a port of another device.
 *
 * The stereo ring lives in ctx[3]. A 65536-frame power-of-two ring can hold
 * both the previous and current 32768-frame slices without overwriting the
 * capture during playback. No math helpers, runtime division, modulo, switch,
 * or static audio state. ctx[11]/ctx[12] magic shuttle preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "beatscat_params.h"

#ifndef BEATSCAT_AUDIO_FUNC
#define BEATSCAT_AUDIO_FUNC Fx_DLY_BeatScat
#endif

#define BEATSCAT_DO_PRAGMA(x) _Pragma(#x)
#define BEATSCAT_EXPAND_PRAGMA(x) BEATSCAT_DO_PRAGMA(x)
#define BEATSCAT_CODE_SECTION(f) BEATSCAT_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

BEATSCAT_CODE_SECTION(BEATSCAT_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define BS_MAGIC       0x42534354u /* 'BSCT' */
#define BS_VERSION     1u
#define BS_BUF         65536u
#define BS_MASK        (BS_BUF - 1u)
#define BS_MAX_SLICE   32768u
#define BS_SR_X60      2646000.0f
#define BS_BPM_MIN     40.0f
#define BS_BPM_MAX     240.0f
#define BS_RSEED_A     0.03630952f
#define BS_RSEED_B     (-3.125e-4f)
#define BS_RSEED_C     7.440476e-7f

typedef struct BeatScatState {
    uint32_t magic;
    uint32_t version;
    uint32_t writePos;
    uint32_t available;
    uint32_t sliceLength;
    uint32_t phase;
    uint32_t patternStep;
    uint32_t captureStart;
    uint32_t playbackPos;
    uint32_t playbackRemain;
    uint32_t wasEnabled;
    uint32_t type;
    uint32_t pattern;
} BeatScatState;

void BEATSCAT_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    volatile unsigned int *desc = ZDL_PTR(volatile unsigned int *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base || (base & 3u) != 0u) return;

    uintptr_t bufBase = (base + sizeof(BeatScatState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)BS_BUF * 2u * sizeof(float) > end) return;

    BeatScatState *st = (BeatScatState *)base;
    float *ring = (float *)bufBase;
    if (st->magic != BS_MAGIC || st->version != BS_VERSION) {
        st->magic = BS_MAGIC;
        st->version = BS_VERSION;
        st->writePos = 0u;
        st->available = 0u;
        st->sliceLength = 0u;
        st->phase = 0u;
        st->patternStep = 0u;
        st->captureStart = 0u;
        st->playbackPos = 0u;
        st->playbackRemain = 0u;
        st->wasEnabled = 0u;
        st->type = 0u;
        st->pattern = 0u;
    }

    float divNorm = zoom_param_norm01(params[BEATSCAT_DIV_SLOT], BEATSCAT_DIV_DEFAULT_NORM);
    float bpmNorm = zoom_param_norm01(params[BEATSCAT_BPM_SLOT], BEATSCAT_BPM_DEFAULT_NORM);
    float typeNorm = zoom_param_norm01(params[BEATSCAT_TYPE_SLOT], BEATSCAT_TYPE_DEFAULT_NORM);
    float patternNorm = zoom_param_norm01(params[BEATSCAT_PATTERN_SLOT], BEATSCAT_PATTERN_DEFAULT_NORM);
    float mix = zoom_param_norm01(params[BEATSCAT_MIX_SLOT], BEATSCAT_MIX_DEFAULT_NORM);

    float bpm = bpmNorm * BS_BPM_MAX;
    if (bpm < BS_BPM_MIN) bpm = BS_BPM_MIN;
    else if (bpm > BS_BPM_MAX) bpm = BS_BPM_MAX;

    float reciprocal = BS_RSEED_A + BS_RSEED_B * bpm + BS_RSEED_C * bpm * bpm;
    reciprocal = reciprocal * (2.0f - bpm * reciprocal);
    reciprocal = reciprocal * (2.0f - bpm * reciprocal);
    reciprocal = reciprocal * (2.0f - bpm * reciprocal);
    reciprocal = reciprocal * (2.0f - bpm * reciprocal);
    float quarterSamples = BS_SR_X60 * reciprocal;

    float division;
    if      (divNorm < 0.25f) division = 0.125f; /* 1/32 */
    else if (divNorm < 0.50f) division = 0.25f;  /* 1/16 */
    else if (divNorm < 0.75f) division = 0.5f;   /* 1/8 */
    else                      division = 1.0f;   /* 1/4 */

    int32_t sliceCount = (int32_t)(quarterSamples * division);
    if (sliceCount < 256) sliceCount = 256;
    else if (sliceCount > (int32_t)BS_MAX_SLICE) sliceCount = (int32_t)BS_MAX_SLICE;
    uint32_t sliceLength = (uint32_t)sliceCount;

    uint32_t type;
    if      (typeNorm < 0.34f) type = 0u; /* stutter */
    else if (typeNorm < 0.67f) type = 1u; /* reverse */
    else                      type = 2u; /* shifted */

    uint32_t pattern;
    if      (patternNorm < 0.34f) pattern = 0u; /* every division */
    else if (patternNorm < 0.67f) pattern = 1u; /* alternating, phase 0 */
    else                          pattern = 2u; /* alternating, phase 1 */

    if (sliceLength != st->sliceLength) {
        st->sliceLength = sliceLength;
        st->available = 0u;
        st->phase = 0u;
        st->patternStep = 0u;
        st->playbackRemain = 0u;
    }
    if (type != st->type) {
        st->type = type;
    }
    if (pattern != st->pattern) {
        st->pattern = pattern;
        st->patternStep = 0u;
    }

    int enabled = params[0] >= 0.5f;
    if (enabled && !st->wasEnabled) {
        st->available = 0u;
        st->phase = 0u;
        st->patternStep = 0u;
        st->playbackRemain = 0u;
    }
    st->wasEnabled = enabled ? 1u : 0u;

    uint32_t wp = st->writePos;
    uint32_t available = st->available;
    uint32_t phase = st->phase;
    uint32_t patternStep = st->patternStep;
    uint32_t captureStart = st->captureStart;
    uint32_t playbackPos = st->playbackPos;
    uint32_t playbackRemain = st->playbackRemain;
    int i;
    for (i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];
        int ready = available >= sliceLength;
        int scatterActive = ready && enabled && playbackRemain > 0u;

        float scatterL = inL;
        float scatterR = inR;
        if (scatterActive) {
            uint32_t sourceOffset;
            if (type == 0u) {
                sourceOffset = playbackPos;
            } else if (type == 1u) {
                sourceOffset = sliceLength - 1u - playbackPos;
            } else {
                sourceOffset = playbackPos + (sliceLength >> 2);
                if (sourceOffset >= sliceLength) sourceOffset -= sliceLength;
            }
            uint32_t sourcePos = (captureStart + sourceOffset) & BS_MASK;
            scatterL = ring[2u * sourcePos];
            scatterR = ring[2u * sourcePos + 1u];
            playbackPos++;
            if (type == 0u) {
                uint32_t chunk = sliceLength >> 2;
                if (playbackPos >= chunk) playbackPos = 0u;
            }
            playbackRemain--;
        }

        ring[2u * wp] = inL;
        ring[2u * wp + 1u] = inR;
        wp = (wp + 1u) & BS_MASK;
        if (available < sliceLength) available++;

        if (ready) {
            if (phase >= sliceLength) {
                phase = 0u;
                captureStart = (wp - sliceLength) & BS_MASK;
                int trigger = pattern == 0u ||
                    (pattern == 1u && (patternStep & 1u) == 0u) ||
                    (pattern == 2u && (patternStep & 1u) != 0u);
                if (enabled && trigger) {
                    playbackPos = 0u;
                    playbackRemain = sliceLength;
                } else {
                    playbackRemain = 0u;
                }
                patternStep++;
                if (patternStep >= 4u) patternStep = 0u;
            }
            if (scatterActive) {
                fxBuf[i] = (1.0f - mix) * inL + mix * scatterL;
                fxBuf[i + 8] = (1.0f - mix) * inR + mix * scatterR;
            }
            phase++;
        }
    }

    st->writePos = wp;
    st->available = available;
    st->phase = phase;
    st->patternStep = patternStep;
    st->captureStart = captureStart;
    st->playbackPos = playbackPos;
    st->playbackRemain = playbackRemain;
}
