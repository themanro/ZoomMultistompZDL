/*
 * BeatRpt: tempo-synced beat repeat for the Zoom MS-70CDR.
 *
 * Length selects a 1/32, 1/16, 1/8, or 1/4-note slice. The latest slice is
 * played while the next one is captured. Modes are continuous, one-shot on
 * effect activation, and one slice every four beats.
 *
 * The stereo ring lives in ctx[3]. Its 65536-frame power-of-two size leaves
 * room for a 32768-frame slice and prevents the writer overwriting the slice
 * while it is being played. The longest setting is capped to that safe size.
 * No math helpers, runtime division, modulo, switch/jump tables, or static
 * audio state. ctx[11]/ctx[12] magic shuttle preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "beatrep_params.h"

#ifndef BEATREP_AUDIO_FUNC
#define BEATREP_AUDIO_FUNC Fx_DLY_BeatRpt
#endif

#define BEATREP_DO_PRAGMA(x) _Pragma(#x)
#define BEATREP_EXPAND_PRAGMA(x) BEATREP_DO_PRAGMA(x)
#define BEATREP_CODE_SECTION(f) BEATREP_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

BEATREP_CODE_SECTION(BEATREP_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define BR_MAGIC       0x42525054u /* 'BRPT' */
#define BR_VERSION     1u
#define BR_BUF         65536u
#define BR_MASK        (BR_BUF - 1u)
#define BR_MAX_SLICE   32768u
#define BR_SR_X60      2646000.0f
#define BR_BPM_MIN     40.0f
#define BR_BPM_MAX     240.0f
#define BR_RSEED_A     0.03630952f
#define BR_RSEED_B     (-3.125e-4f)
#define BR_RSEED_C     7.440476e-7f

typedef struct BeatRepState {
    uint32_t magic;
    uint32_t version;
    uint32_t writePos;
    uint32_t available;
    uint32_t sliceLength;
    uint32_t phase;
    uint32_t autoCount;
    uint32_t autoRemain;
    uint32_t manualRemain;
    uint32_t manualPending;
    uint32_t wasEnabled;
    uint32_t mode;
} BeatRepState;

void BEATREP_AUDIO_FUNC(unsigned int *ctx)
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

    uintptr_t bufBase = (base + sizeof(BeatRepState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)BR_BUF * 2u * sizeof(float) > end) return;

    BeatRepState *st = (BeatRepState *)base;
    float *ring = (float *)bufBase;
    if (st->magic != BR_MAGIC || st->version != BR_VERSION) {
        st->magic = BR_MAGIC;
        st->version = BR_VERSION;
        st->writePos = 0u;
        st->available = 0u;
        st->sliceLength = 0u;
        st->phase = 0u;
        st->autoCount = 0u;
        st->autoRemain = 0u;
        st->manualRemain = 0u;
        st->manualPending = 0u;
        st->wasEnabled = 0u;
        st->mode = 0u;
    }

    float lengthNorm = zoom_param_norm01(params[BEATREP_LENGTH_SLOT], BEATREP_LENGTH_DEFAULT_NORM);
    float bpmNorm = zoom_param_norm01(params[BEATREP_BPM_SLOT], BEATREP_BPM_DEFAULT_NORM);
    float modeNorm = zoom_param_norm01(params[BEATREP_MODE_SLOT], BEATREP_MODE_DEFAULT_NORM);
    float mix = zoom_param_norm01(params[BEATREP_MIX_SLOT], BEATREP_MIX_DEFAULT_NORM);

    float bpm = bpmNorm * BR_BPM_MAX;
    if (bpm < BR_BPM_MIN) bpm = BR_BPM_MIN;
    else if (bpm > BR_BPM_MAX) bpm = BR_BPM_MAX;

    float reciprocal = BR_RSEED_A + BR_RSEED_B * bpm + BR_RSEED_C * bpm * bpm;
    reciprocal = reciprocal * (2.0f - bpm * reciprocal);
    reciprocal = reciprocal * (2.0f - bpm * reciprocal);
    reciprocal = reciprocal * (2.0f - bpm * reciprocal);
    reciprocal = reciprocal * (2.0f - bpm * reciprocal);
    float quarterSamples = BR_SR_X60 * reciprocal;

    float division;
    uint32_t autoEvery;
    if      (lengthNorm < 0.25f) { division = 0.125f; autoEvery = 32u; } /* 1/32 */
    else if (lengthNorm < 0.50f) { division = 0.25f;  autoEvery = 16u; } /* 1/16 */
    else if (lengthNorm < 0.75f) { division = 0.5f;   autoEvery = 8u; }  /* 1/8 */
    else                         { division = 1.0f;   autoEvery = 4u; }  /* 1/4 */

    int32_t sliceCount = (int32_t)(quarterSamples * division);
    if (sliceCount < 256) sliceCount = 256;
    else if (sliceCount > (int32_t)BR_MAX_SLICE) sliceCount = (int32_t)BR_MAX_SLICE;
    uint32_t sliceLength = (uint32_t)sliceCount;

    uint32_t mode;
    if      (modeNorm < 0.34f) mode = 0u; /* continuous */
    else if (modeNorm < 0.67f) mode = 1u; /* manual */
    else                       mode = 2u; /* automatic */

    if (sliceLength != st->sliceLength) {
        st->sliceLength = sliceLength;
        st->available = 0u;
        st->phase = 0u;
        st->autoCount = 0u;
        st->autoRemain = 0u;
        st->manualRemain = 0u;
        st->manualPending = 0u;
    }
    if (mode != st->mode) {
        st->mode = mode;
        st->phase = 0u;
        st->autoCount = 0u;
        st->autoRemain = 0u;
        st->manualRemain = 0u;
        st->manualPending = 0u;
    }

    int enabled = params[0] >= 0.5f;
    if (enabled && !st->wasEnabled && mode == 1u) st->manualPending = 1u;
    st->wasEnabled = enabled ? 1u : 0u;
    if (!enabled && mode == 1u) {
        st->manualRemain = 0u;
        st->manualPending = 0u;
    }

    uint32_t wp = st->writePos;
    uint32_t available = st->available;
    uint32_t phase = st->phase;
    uint32_t autoCount = st->autoCount;
    uint32_t autoRemain = st->autoRemain;
    uint32_t manualRemain = st->manualRemain;
    uint32_t manualPending = st->manualPending;
    int i;
    for (i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];
        int ready = available >= sliceLength;

        float repeatL = inL;
        float repeatR = inR;
        if (ready) {
            uint32_t readPos = (wp - sliceLength) & BR_MASK;
            repeatL = ring[2u * readPos];
            repeatR = ring[2u * readPos + 1u];
        }

        ring[2u * wp] = inL;
        ring[2u * wp + 1u] = inR;
        wp = (wp + 1u) & BR_MASK;
        if (available < sliceLength) available++;

        if (ready) {
            if (manualPending && enabled && mode == 1u) {
                manualRemain = sliceLength;
                manualPending = 0u;
            }

            if (phase >= sliceLength) {
                phase = 0u;
                if (autoCount >= autoEvery - 1u) {
                    autoCount = 0u;
                    autoRemain = sliceLength;
                } else {
                    autoCount++;
                }
            }

            int repeat = 0;
            if (enabled && mode == 0u) repeat = 1;
            else if (enabled && mode == 1u && manualRemain > 0u) {
                repeat = 1;
                manualRemain--;
            } else if (enabled && mode == 2u && autoRemain > 0u) {
                repeat = 1;
                autoRemain--;
            }

            if (repeat) {
                fxBuf[i] = (1.0f - mix) * inL + mix * repeatL;
                fxBuf[i + 8] = (1.0f - mix) * inR + mix * repeatR;
            }
            phase++;
        }
    }

    st->writePos = wp;
    st->available = available;
    st->phase = phase;
    st->autoCount = autoCount;
    st->autoRemain = autoRemain;
    st->manualRemain = manualRemain;
    st->manualPending = manualPending;
}
