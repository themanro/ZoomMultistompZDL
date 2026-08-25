/*
 * rewire.c -- Rewire: four lo-fi blocks in a chain you can reorder, MS-70CDR.
 *
 * Alesis ModFX Bitrman-inspired (no Alesis branding). What makes that box worth
 * chasing is not any single block -- this pack already has bitcrush three times
 * over -- but that its 123B / B321 / 1B32 modes REORDER the chain. Crush before
 * a comb is a different pedal from a comb before crush, and distortion last
 * sounds nothing like distortion first. Slot order in a patch reorders whole
 * effects; nothing here could reorder stages INSIDE one.
 *
 * Four blocks, one knob each, and a Route knob that permutes them:
 *   B  Bits   crush + sample-rate decimate on one control
 *   D  Drive  asymmetric soft clip
 *   R  Ring   ring modulator; the knob sweeps the carrier and fades it in
 *   C  Comb   short feedback comb, the knob is depth
 *
 * 7 knobs. Route and Bits sit on knobs 1-2, the verbatim-stock LineSel edit
 * handlers -- the most reliable pair in the pack, and the two most often moved:
 *   Route (knob 1) block order: BDRC, CRDB, DBCR, RCBD, BRDC
 *   Bits  (knob 2) crush depth and decimation together
 *   Drive (knob 3) distortion amount
 *   Ring  (knob 4) carrier frequency, fading in from silent at 0
 *   Comb  (knob 5) comb depth
 *   Tone  (knob 6) low-pass on the wet path
 *   Mix   (knob 7) dry/wet
 *
 * How the routing is done, and why not the obvious ways. A permutation TABLE
 * would need an indirect call or an indexed jump, and both are freeze classes
 * here -- a ZDL's jump tables are unrelocated, and a CALL pulls in a helper.
 * Five straight-line arms with all four blocks expanded in each was the first
 * attempt, and it failed differently: twenty inline expansions made the compiler
 * give up inlining, emitting 256 bytes outside the .audio section and twelve
 * relocations with it, which is equally fatal. What works is packing the order
 * into one nibble per stage and looping four times, so each block appears ONCE
 * and stays inline. Costs a shift and a compare per stage.
 *
 * Safe-DSP: no switch (jump tables are unreachable in a ZDL and hard-freeze the
 * DSP), no CALLs or helper calls, no runtime divide (the decimator counter and
 * the comb length are integers; the crush step's reciprocal comes from an
 * if/else ladder of literals), no modulo (power-of-two ring mask), no static
 * arrays, no .fardata, no float->unsigned casts, denormals flushed. The comb is
 * the only feedback path and its gain is clamped below unity, so it cannot run
 * away whatever the routing.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "rewire_params.h"

#ifndef REWIRE_AUDIO_FUNC
#define REWIRE_AUDIO_FUNC Fx_DLY_Rewire
#endif

#define REWIRE_DO_PRAGMA(x) _Pragma(#x)
#define REWIRE_EXPAND_PRAGMA(x) REWIRE_DO_PRAGMA(x)
#define REWIRE_CODE_SECTION(f) REWIRE_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

REWIRE_CODE_SECTION(REWIRE_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define RW_MAGIC   0x52575231u   /* 'RWR1' */
#define RW_VERSION 1u

#define RW_BUF     4096u         /* 16 KB, ~93 ms -- plenty for a comb */
#define RW_MASK    (RW_BUF - 1u)
#define RW_DENORM  1.0e-18f
#define RW_TWO_PI  6.28318531f

typedef struct RewireState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearPos;
    uint32_t writePos;
    int32_t  decCount;          /* decimator sample-and-hold counter */
    float    decHold;
    float    oscPhase;
    float    lp;
    float    combLp;
} RewireState;

static inline float rw_flush(float x)
{
    if (x < RW_DENORM && x > -RW_DENORM) return 0.0f;
    return x;
}

/* Cubic soft clip. Same curve the rest of the pack uses -- no tanh, no math lib. */
static inline float rw_soft(float x)
{
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return 1.5f * x - 0.5f * x * x * x;
}

/* Parabolic sine, valid over -pi..pi. Cheaper than a table and needs no array,
 * which matters because static arrays are a freeze class here. */
static inline float rw_sin(float x)
{
    if (x > 3.14159265f) x -= RW_TWO_PI;
    const float B = 1.27323954f, C = -0.405284735f;
    float y = B * x + C * x * (x < 0.0f ? -x : x);
    return 0.225f * (y * (y < 0.0f ? -y : y) - y) + y;
}

/* ---- the four blocks. static inline: expanded in place, never CALLed ---- */

static inline float rw_bits(float x, float step, float invStep, int32_t hold,
                            RewireState *st)
{
    /* decimate first, then quantise -- the order inside the block matters as
     * much as the order between blocks, and this is the way a real sample-rate
     * reducer sits in front of a shorter word length. */
    st->decCount++;
    if (st->decCount >= hold) { st->decCount = 0; st->decHold = x; }
    float d = st->decHold;
    float q = d * step;
    int32_t qi = (int32_t)(q + (q >= 0.0f ? 0.5f : -0.5f));
    return (float)qi * invStep;
}

static inline float rw_drive(float x, float amt)
{
    return rw_soft(x * (1.0f + amt * 14.0f)) * (1.0f - amt * 0.45f);
}

static inline float rw_ring(float x, float depth, float osc)
{
    return x * (1.0f - depth) + (x * osc) * depth;
}

static inline float rw_comb(float x, float depth, float *buf, uint32_t wp,
                            int32_t len, RewireState *st)
{
    uint32_t rp = (wp - (uint32_t)len) & RW_MASK;
    float d = buf[rp];
    st->combLp = rw_flush(st->combLp + 0.35f * (d - st->combLp));
    float y = x + st->combLp * depth * 0.85f;
    buf[wp & RW_MASK] = rw_soft(y);
    return y;
}

void REWIRE_AUDIO_FUNC(unsigned int *ctx)
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
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u) return;

    uintptr_t bufBase = (base + sizeof(RewireState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)RW_BUF * 4u > end) return;

    RewireState *st = (RewireState *)base;
    float *buf = (float *)bufBase;

    if (st->magic != RW_MAGIC || st->version != RW_VERSION || !st->initialized) {
        st->magic = RW_MAGIC;
        st->version = RW_VERSION;
        st->writePos = 0u;
        st->decCount = 0;
        st->decHold = 0.0f;
        st->oscPhase = 0.0f;
        st->lp = 0.0f;
        st->combLp = 0.0f;
        st->clearPos = 0u;
        st->initialized = 1u;
    }

    if (st->clearPos < RW_BUF) {
        uint32_t n = st->clearPos, lim = n + 1024u;
        if (lim > RW_BUF) lim = RW_BUF;
        for (; n < lim; n++) buf[n] = 0.0f;
        st->clearPos = n;
    }

    float rt = zoom_param_norm01(params[REWIRE_ROUTE_SLOT], REWIRE_ROUTE_DEFAULT_NORM);
    float bi = zoom_param_norm01(params[REWIRE_BITS_SLOT],  REWIRE_BITS_DEFAULT_NORM);
    float dr = zoom_param_norm01(params[REWIRE_DRIVE_SLOT], REWIRE_DRIVE_DEFAULT_NORM);
    float rg = zoom_param_norm01(params[REWIRE_RING_SLOT],  REWIRE_RING_DEFAULT_NORM);
    float cb = zoom_param_norm01(params[REWIRE_COMB_SLOT],  REWIRE_COMB_DEFAULT_NORM);
    float tn = zoom_param_norm01(params[REWIRE_TONE_SLOT],  REWIRE_TONE_DEFAULT_NORM);
    float mx = zoom_param_norm01(params[REWIRE_MIX_SLOT],   REWIRE_MIX_DEFAULT_NORM);

    /* Bits: word length from a literal ladder so 1/step is a literal too, and
     * decimation hold rising with the same knob. */
    float step, invStep;
    if      (bi < 0.17f) { step = 4096.0f; invStep = 2.44140625e-4f; }
    else if (bi < 0.34f) { step = 1024.0f; invStep = 9.765625e-4f; }
    else if (bi < 0.51f) { step =  256.0f; invStep = 3.90625e-3f; }
    else if (bi < 0.68f) { step =   64.0f; invStep = 1.5625e-2f; }
    else if (bi < 0.85f) { step =   24.0f; invStep = 4.1666667e-2f; }
    else                 { step =    8.0f; invStep = 0.125f; }
    int32_t hold = 1 + (int32_t)(bi * bi * 23.0f);       /* 1 .. 24 samples */

    /* Ring: the knob is carrier frequency, and depth fades in from silence at 0
     * so the block is genuinely absent rather than quietly detuning things. */
    float carrier = 30.0f + rg * rg * 2200.0f;
    float oscInc = carrier * (RW_TWO_PI / 44100.0f);
    float ringDepth = rg < 0.03f ? 0.0f : (rg - 0.03f) * 1.03f;

    int32_t combLen = 24 + (int32_t)(cb * 900.0f);
    float combDepth = cb;

    float lpCoef = 0.03f + tn * 0.6f;

    /* Route: five orders. if/else, never switch. */
    /* Order packed one nibble per stage: 0=Bits 1=Drive 2=Ring 3=Comb. */
    int32_t packed;
    if      (rt < 0.20f) packed = 0x0123;   /* B D R C */
    else if (rt < 0.40f) packed = 0x3210;   /* C R D B */
    else if (rt < 0.60f) packed = 0x1032;   /* D B C R */
    else if (rt < 0.80f) packed = 0x2301;   /* R C B D */
    else                 packed = 0x0213;   /* B R D C */

    uint32_t wp = st->writePos;
    float ph = st->oscPhase;
    float lp = st->lp;

    int i;
    for (i = 0; i < 8; i++) {
        float dry = 0.5f * (fxBuf[i] + fxBuf[i + 8]);

        ph += oscInc;
        if (ph > RW_TWO_PI) ph -= RW_TWO_PI;
        float osc = rw_sin(ph);

        /* Apply the four blocks in the order this route packs. Each block
         * appears ONCE in the source and the stage loop picks between them.
         *
         * The first cut wrote five straight-line arms with all four blocks
         * expanded inside each -- twenty expansions -- and the compiler stopped
         * inlining, emitting 256 bytes of .text outside the .audio section and
         * twelve relocations with it. Both are freeze classes here. Packing the
         * order into a nibble field costs a shift and keeps every block inline. */
        int32_t stage;
        float x = dry;
        for (stage = 0; stage < 4; stage++) {
            int32_t which = (packed >> (12 - stage * 4)) & 0xF;
            if      (which == 0) x = rw_bits(x, step, invStep, hold, st);
            else if (which == 1) x = rw_drive(x, dr);
            else if (which == 2) x = rw_ring(x, ringDepth, osc);
            else                 x = rw_comb(x, combDepth, buf, wp, combLen, st);
        }
        wp++;

        lp += lpCoef * (x - lp);
        float wet = lp;

        float out = dry * (1.0f - mx) + rw_soft(wet) * mx;
        fxBuf[i]     = out;
        fxBuf[i + 8] = out;
    }

    st->writePos = wp;
    st->oscPhase = ph;
    st->lp = rw_flush(lp);
}
