/*
 * syncprobv3.c
 *
 * Confirms TapeEcho3's tempo-sync algorithm can be replicated from a
 * custom effect by calling state[31] and state[24] FROM THE AUDIO
 * CONTEXT (no handler patching at all). The LineSel handler is
 * unpatched; all the sync logic lives here.
 *
 * Algorithm (from docs/TEMPO-SYNC.md §4 — verbatim TapeEcho3 DLY_EP3_Calc_DelayTime):
 *
 *   sync_value = state[31](slot=0, B4=6)        // 0=off, 1..15=division
 *   if (sync_value == 0):
 *     delay = state[31](slot=0, B4=4) + 10       // free-time path
 *   else:
 *     x = state[31]((slot-1)&0xff, B4=0x0f3c)    // secondary table at 0xc009fe90
 *     y = state[24](x, B4=100)                    // BPM-aware host math
 *     delay = y / 100
 *
 * Slot is hardcoded to 0 — the probe MUST be loaded in slot 0 of the
 * patch chain for the state[31] call to read this slot's SYNC value.
 *
 * The returned `delay` is the value TapeEcho3 stores at params[127]
 * and consumes in its audio function. We turn it into audible gain so
 * tap-tempo correlation is directly perceivable.
 *
 * Risks:
 *   - state[31] and state[24] are firmware functions at fixed addresses
 *     (c00b820c, c00d4b40). If they're not callable from audio context,
 *     this freezes on first audio block. The state[31] body is 5
 *     instructions and touches no globals, so it should be safe. state[24]
 *     does float math and writes one global (c00de524 conditional) — also
 *     should be safe.
 *   - If the audio context's caller-saved registers don't match what
 *     state[31]/state[24] preserve, returns may be corrupt. cl6x emits
 *     standard C6000 EABI calls so this should match.
 */

#include <stdint.h>

#include "syncprobv3_params.h"

#pragma CODE_SECTION(Fx_FLT_SyncPrV3, ".audio")

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

typedef int (*ti_state_fn)(int a4, int b4);

void Fx_FLT_SyncPrV3(unsigned int *ctx)
{
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);
    float *outBuf = ZDL_PTR(float *, ctx[6]);

    /* Preserve LineSel current-sample plumbing. */
    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    /* Fixed firmware function pointers. */
    ti_state_fn state31 = (ti_state_fn)0xc00b820cu;
    ti_state_fn state24 = (ti_state_fn)0xc00d4b40u;

    /* Replicate DLY_EP3_Calc_DelayTime. Slot hardcoded to 0. */
    int slot = 0;
    int sync_value = state31(slot, 6);

    int delay;
    if (sync_value == 0) {
        /* Free-time path: read TIME knob, add 10. */
        delay = state31(slot, 4) + 10;
    } else {
        /* Sync-on path: reach secondary table, run host math, /100. */
        int byte = (slot - 1) & 0xff;
        int x = state31(byte, 0x0f3c);
        int y = state24(x, 100);
        delay = y / 100;
    }

    /* Turn the delay into audible gain. Linear, 0..2000 -> 0..2, clipped.
     * Negative values are absolute-valued so we don't lose the bottom
     * half of state[24]'s range if it returns signed. */
    unsigned int abs_delay = (delay < 0) ? (unsigned int)(-delay) : (unsigned int)delay;
    float gain = (float)abs_delay * (2.0f / 2000.0f);
    if (gain > 2.0f) gain = 2.0f;

    int i;
    for (i = 0; i < 16; i++) {
        outBuf[i] += fxBuf[i] * gain;
    }
}
