/*
 * ceiling.c -- Ceiling: how many cycles can one effect use before the pedal
 * crackles? A probe, never in dist/.
 *
 * Each callback it passes the audio through (or plays a 1 kHz test sine) and
 * then burns kCyc x 1000 EMULATED cycles in one of three ways:
 *
 *   Mode 0  arithmetic only: a float multiply-add chain in registers. The
 *           C674x is statically scheduled, so emulated cycles = real cycles:
 *           the kCyc where it starts to crackle is the real capacity left to
 *           an effect, with the firmware's own work subtracted.
 *   Mode 1  a walk over 64 KB of the arena, one load per 128-byte line:
 *           L2-resident after the first pass, missing L1. Real cost =
 *           emulated + L2 stalls.
 *   Mode 2  the same walk over the whole arena (~690 KB, more than the
 *           128 KB L2): every load a miss to DDR.
 *
 * Turn kCyc up in a patch with Ceiling alone until the sine starts to click.
 * Mode 0's crackle point gives the budget; modes 1 and 2 against it give the
 * stall per access, which is what sizes a bigger NAM model (its weights and
 * history do not fit L2).
 *
 * The emulated cost per iteration of each loop is measured in the emulator
 * after a build (validate.py) and written into CEIL_ITER_* below, so kCyc
 * maps to cycles linearly.
 *
 * Safe-DSP: no switch (the mode is compared as a float), no math lib, no
 * divide, no writable statics (state in the ctx[3] arena), magic shuttle.
 */
#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "ceiling_params.h"

#ifndef CEIL_AUDIO_FUNC
#define CEIL_AUDIO_FUNC Fx_DLY_Ceiling
#endif
#ifdef CEIL_HOST
typedef uintptr_t ce_word;
#else
typedef unsigned int ce_word;
#endif
#define CE_DO_PRAGMA(x) _Pragma(#x)
#define CE_EXPAND_PRAGMA(x) CE_DO_PRAGMA(x)
CE_EXPAND_PRAGMA(CODE_SECTION(CEIL_AUDIO_FUNC, ".audio"))

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))
#define CE_MAGIC 0x4345494Cu          /* 'CEIL' */

/* emulated cycles per loop iteration, measured by validate.py (the build passes them in) */
#ifndef CEIL_ITER_ALU
#define CEIL_ITER_ALU 4.0f
#endif
#ifndef CEIL_ITER_MEM
#define CEIL_ITER_MEM 4.0f
#endif
#define CE_WIN    65536u              /* mode 1's window */
#define CE_LINE   128u                /* one L2 line */
#define CE_SKIP   4096u               /* the state lives at the arena's start */

typedef struct CeState {
    uint32_t magic, pos, span;
    float ph, acc, sink;
    uint32_t isink;
} CeState;

void CEIL_AUDIO_FUNC(ce_word *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    ce_word *magicSrc = ZDL_PTR(ce_word *, ctx[12]);
    ce_word *magicDst = ZDL_PTR(ce_word *, *(ce_word *)ZDL_PTR(ce_word *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile ce_word *desc = ZDL_PTR(volatile ce_word *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base || (base & 3u) != 0u) return;
    if (base + CE_SKIP + CE_WIN > end) return;

    CeState *st = (CeState *)base;
    if (st->magic != CE_MAGIC) {
        st->magic = CE_MAGIC; st->pos = 0u; st->ph = 0.0f; st->acc = 1.0f; st->sink = 0.0f; st->isink = 0u;
        st->span = (uint32_t)(end - base - CE_SKIP) & ~(CE_LINE - 1u);
        return;
    }

    /* params[] holds each knob's value / 100 whatever its range (Loop's Tempo
     * reads 2.5 for 250), so x 100 gives the value the pedal shows */
    float kcyc = params[CEILING_KCYC_SLOT] * 100.0f;
    float mode = params[CEILING_MODE_SLOT] * 100.0f;
    float tone = params[CEILING_TONE_SLOT] * 100.0f;
    if (!(kcyc >= 0.0f && kcyc <= 120.5f)) kcyc = 0.0f;      /* anything odd: burn nothing */
    int f;

    /* the audio: pass-through, or a 1 kHz sine at -12 dB (parabolic, no math lib) */
    if (tone > 0.5f) {
        float ph = st->ph;
        for (f = 0; f < 8; f++) {
            ph += 1000.0f / 44100.0f;
            if (ph >= 1.0f) ph -= 1.0f;
            float x = ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f;   /* triangle */
            float a = x < 0.0f ? -x : x;
            float y = x * (2.0f - a);                                    /* parabola */
            float ay = y < 0.0f ? -y : y;
            y = y * (0.775f + 0.225f * ay) * 0.25f;
            fxBuf[f] = y; fxBuf[f + 8] = y;
        }
        st->ph = ph;
    }

    /* the burn */
    {
        float cyc = kcyc * 1000.0f;
        if (mode < 0.5f) {                                   /* arithmetic: a dependent multiply-add chain */
            uint32_t n = (uint32_t)(int)(cyc * (1.0f / CEIL_ITER_ALU)), i;   /* a constant reciprocal: no divide */
            float a = st->acc;
            for (i = 0u; i < n; i++) a = a * 0.99999994f + 1.0e-7f;
            st->acc = a; st->sink += a;
        } else {
            uint32_t n = (uint32_t)(int)(cyc * (1.0f / CEIL_ITER_MEM)), i, pos = st->pos;
            uint32_t span = mode < 1.5f ? CE_WIN : st->span;
            uint32_t s = 0u;
            const volatile uint32_t *mem = (const volatile uint32_t *)(base + CE_SKIP);
            for (i = 0u; i < n; i++) {
                s += mem[pos >> 2];
                pos += CE_LINE;
                if (pos >= span) pos = 0u;
            }
            st->pos = pos; st->isink += s;
        }
    }
}
