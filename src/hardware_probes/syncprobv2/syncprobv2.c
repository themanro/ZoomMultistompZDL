/*
 * syncprobv2.c
 *
 * SyncProbe v2: bypass-state[7] probe.
 *
 * The v1 / v1.5 SyncProbe family answered the first half of the
 * tempo-sync question: state[24] is reachable from custom-handler
 * context. But the LineSel-cloned handler keeps the SHL-22 plus
 * state[7] tail-call after the call, so even when state[24] returned
 * something interesting it was sanitized to ~zero before reaching
 * params[5]. See docs/STATE-ABI-PROGRESS.md and build/ABI.md §5.2.
 *
 * This v2 patches the handler twice:
 *   - state[31] -> state[24] at blob offset +0x65 (same as v1)
 *   - +0x80..+0xab: SHL/state[7] tail-call REPLACED with a direct
 *     `STW.D1T1 A4, *+A0[5]` that writes state[24]'s raw return
 *     into params[5], then pops B3 and returns.
 *
 * The audio function below reads params[5] back as a raw 32-bit bit
 * pattern (NOT as a float) and turns its low 8 bits into a 0..2 gain.
 * Reading as uint32 avoids the "value lands as a tiny float because
 * state[24] returned a large integer" problem. With this layout:
 *
 *   state[24] returns 0       -> gain = 0          -> silent
 *   state[24] returns 0x80    -> gain = ~1         -> unity-ish
 *   state[24] returns 0x1FF   -> gain = ~2 then wraps -> louder, periodic
 *
 * As BPM changes, if state[24] returns a BPM-dependent value, the gain
 * should pulse / drift audibly on each tap-tempo press.
 */

#include <stdint.h>

#include "syncprobv2_params.h"

#pragma CODE_SECTION(Fx_FLT_SyncPrV2, ".audio")

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

void Fx_FLT_SyncPrV2(unsigned int *ctx)
{
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);
    float *outBuf = ZDL_PTR(float *, ctx[6]);
    unsigned int *paramsRaw = ZDL_PTR(unsigned int *, ctx[1]);

    /* Preserve LineSel current-sample plumbing.  ParamTap demonstrates this
     * keeps audio routing alive even when the effect does no DSP. */
    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    /* Read params[5] as raw 32-bit bits.  The v2 handler stored state[24]'s
     * return there with no float normalization, so the bit pattern is the
     * uint32 value the firmware handed us. */
    unsigned int raw = paramsRaw[SYNCPROBV2_SYNC_SLOT];
    float gain = (float)(raw & 0xFFu) * (2.0f / 255.0f);

    int i;
    for (i = 0; i < 16; i++) {
        outBuf[i] += fxBuf[i] * gain;
    }
}
