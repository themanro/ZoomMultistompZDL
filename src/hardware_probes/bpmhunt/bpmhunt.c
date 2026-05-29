/*
 * bpmhunt.c — v4 (per-byte isolation around 0xc009c080)
 *
 * v3 found the hit: at base+0x80 (= 0xc009c080) the byte-sum changes
 * with tap tempo — at BPM 75 and BPM 250 the gain reads "a bit louder"
 * than at other BPMs. That's our first BPM-correlated word. The
 * non-monotonic 75/250 response suggests the gain is sensing a 32-bit
 * value (probably period-in-samples or a fixed-point ratio) where
 * different BPM ranges activate different bytes.
 *
 * v4 confirms-and-narrows: the knob spans 4 candidate words around
 * the hit (0xc009c080, 0xc009c084, 0xc009c088, 0xc009c08c) AND
 * isolates ONE BYTE per knob position. So:
 *
 *   idx  byte_idx  word_base    field read
 *   ---  --------  -----------  ----------
 *    0   byte 0    0xc009c080   bits [ 7: 0]
 *    1   byte 1    0xc009c080   bits [15: 8]
 *    2   byte 2    0xc009c080   bits [23:16]
 *    3   byte 3    0xc009c080   bits [31:24]
 *    4   byte 0    0xc009c084   bits [ 7: 0]
 *    5   byte 1    0xc009c084   bits [15: 8]
 *    ...
 *   15   byte 3    0xc009c08c   bits [31:24]
 *
 * Each knob position turns one isolated byte into a 0..2 gain. A byte
 * that holds raw BPM (~40..240) will show a HUGE swing between BPM 75
 * and BPM 250 (gain ~0.6 vs ~2.0). A byte that holds a stable upper
 * half of a pointer (0xc0) will be saturated-constant. A byte from a
 * fixed-point period will vary in some predictable way.
 *
 * After sweeping, the user reports which knob positions track BPM,
 * and we identify the exact (word, byte) pair holding BPM info.
 */

#include <stdint.h>

#include "bpmhunt_params.h"

#pragma CODE_SECTION(Fx_FLT_BpmHunt, ".audio")

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define FIRMWARE_RAM_BASE  0xc009c080u

void Fx_FLT_BpmHunt(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);
    float *outBuf = ZDL_PTR(float *, ctx[6]);

    /* Preserve LineSel current-sample plumbing. */
    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    /* LineSel handler stores raw knob in roughly 0..0.14 float for a
     * max=15 slot. Map to 0..15. */
    float raw = params[BPMHUNT_ADDR_SLOT];
    if (raw < 0.0f) raw = -raw;
    if (raw > 1.0f) raw = 1.0f;
    int idx = (int)(raw * (15.0f / 0.14f));
    if (idx < 0) idx = 0;
    if (idx > 15) idx = 15;

    /* idx 0..15 → (word 0..3, byte 0..3). */
    int word_idx = idx >> 2;       /* 0..3 -> 0xc009c080, +4, +8, +c */
    int byte_idx = idx & 3;        /* 0..3 -> low byte .. high byte */

    volatile unsigned int *target =
        (volatile unsigned int *)(FIRMWARE_RAM_BASE + (unsigned int)word_idx * 4u);
    unsigned int value = *target;
    unsigned int byte = (value >> ((unsigned int)byte_idx * 8u)) & 0xFFu;

    /* Single-byte gain: 0..255 → 0..2. A BPM-shaped byte (75..250)
     * fills most of the range. */
    float gain = (float)byte * (2.0f / 255.0f);

    int i;
    for (i = 0; i < 16; i++) {
        outBuf[i] += fxBuf[i] * gain;
    }
}
