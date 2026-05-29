/*
 * bpmhunt.c — v3 (narrow scan on the 0xc009c0xx..0xc009c1xx gap)
 *
 * v1 covered 64 B at 4-byte step around 0xc009c1a0 (the state[31] table
 * base): no tap-tempo correlation. v2 widened to 16 KB at 1024-byte step
 * covering 0xc009c000..0xc009ffff: still no correlation, but the
 * positions louder/quieter pattern confirms the direct read mechanism
 * works. v2 sampled 0xc009c000 then jumped to 0xc009c400, leaving the
 * 0xc009c004..0xc009c19c region (≈410 bytes immediately BEFORE the
 * state[31] table) completely unscanned. Firmware globals tend to
 * cluster adjacent to related per-slot tables, so that gap is the
 * highest-probability remaining region.
 *
 * v3 narrows back down: 16 positions × 32-byte step covers
 * 0xc009c000..0xc009c1e0 — i.e. ALL of the previously-unscanned gap
 * plus the first 64 bytes of the state[31] table (re-tested as a
 * cross-check against v1).
 */

#include <stdint.h>

#include "bpmhunt_params.h"

#pragma CODE_SECTION(Fx_FLT_BpmHunt, ".audio")

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define FIRMWARE_RAM_BASE  0xc009c000u
#define FIRMWARE_RAM_STEP  32u

void Fx_FLT_BpmHunt(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);
    float *outBuf = ZDL_PTR(float *, ctx[6]);

    /* Preserve LineSel current-sample plumbing. */
    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    /* LineSel handler stores raw knob in roughly 0..0.14 float for a max=15
     * slot (each unit of knob travel ≈ 0.009 in raw). Map to a 0..15 idx,
     * clamping defensively in case the host adjusts the curve with the
     * pedal_flags=0x28 sync-style slot. */
    float raw = params[BPMHUNT_ADDR_SLOT];
    if (raw < 0.0f) raw = -raw;
    if (raw > 1.0f) raw = 1.0f;
    int idx = (int)(raw * (15.0f / 0.14f));
    if (idx < 0) idx = 0;
    if (idx > 15) idx = 15;

    /* Read the firmware-RAM word at base + step*idx. */
    volatile unsigned int *target = (volatile unsigned int *)(FIRMWARE_RAM_BASE + (unsigned int)idx * FIRMWARE_RAM_STEP);
    unsigned int value = *target;

    /* Sum all four bytes -> 0..1020. Sensitive to changes in ANY byte
     * position, so BPM stored as float, big-endian int, little-endian
     * int, packed half-words, etc. all produce visible gain swings.
     * Mapped to 0..2 by dividing by 510. */
    unsigned int byte_sum =
        ((value >> 24) & 0xFFu) +
        ((value >> 16) & 0xFFu) +
        ((value >>  8) & 0xFFu) +
        ( value        & 0xFFu);
    float gain = (float)byte_sum * (1.0f / 510.0f);
    if (gain > 2.0f) gain = 2.0f;

    int i;
    for (i = 0; i < 16; i++) {
        outBuf[i] += fxBuf[i] * gain;
    }
}
