/*
 * stasis.c -- Stasis: footswitch-triggered freeze, MS-70CDR.
 *
 * Stomp the slot ON and whatever you just played is held indefinitely while the
 * dry signal keeps passing, so you play over your own sustained chord. Stomp it
 * OFF and the hold stops.
 *
 * The trigger is the slot's own bypass bit. That is only possible because of two
 * things EdgeWatch established on hardware (2026-08-18), both of which had to be
 * measured rather than assumed:
 *
 *   1. The effect keeps running while the slot is bypassed, its output is still
 *      audible, and it SEES params[0] change -- from the footswitch and from an
 *      editor's patch write alike, on slots 1-3 and 4-6 equally. So the
 *      footswitch is a real, performable trigger.
 *   2. Input audio still reaches the effect while bypassed. That is what makes
 *      the capture instantaneous: the ring is ALREADY FULL when you stomp, so
 *      Stasis holds the note you just played rather than starting to record
 *      after you ask it to.
 *
 * Recording stops while frozen, which is what protects the captured region from
 * being overwritten by the playing you do on top of it.
 *
 * Sustain is granular, not looped. A loop of any length betrays itself as a
 * period; overlapping windowed grains taken from random offsets inside the
 * capture do not. Two voices, half a grain apart, so one is always fading in
 * while the other fades out and the seam never lands in silence.
 *
 * 5 knobs. Length and Blur sit on knobs 1-2, the verbatim-stock LineSel edit
 * handlers -- the most reliable pair in the pack, and the two that shape the
 * sound most:
 *   Length (knob 1) how much of the past is captured, ~60 ms .. ~1.4 s. Short
 *                   is a single grain of a chord; long is a whole phrase.
 *   Blur   (knob 2) grain size, 1024..16384 samples. Small grains smear into
 *                   texture, large grains keep the chord recognisable.
 *   Decay  (knob 3) how the hold fades. Fully up is genuinely infinite.
 *   Tone   (knob 4) low-pass on the held layer only; the dry stays open.
 *   Mix    (knob 5) LEVEL of the held layer. Not a dry/wet crossfade -- the one
 *                   deliberate exception to the pack's convention. As a
 *                   crossfade it ducked the dry by (1 - Mix) even with nothing
 *                   frozen, so adding Stasis to a patch cost 6 dB at the default.
 *
 * Which knobs work AFTER the freeze: Blur, Decay, Tone and Mix are read every
 * block, so they shape a hold that is already running. Length is latched at the
 * moment of capture and cannot change it afterwards. Note that on slots 4-6 the
 * editor reaches knobs by bypass-bouncing the slot, which Stasis sees as a fresh
 * trigger -- so live knob control of a running hold wants slots 1-3.
 *
 * Safe-DSP: no switch (jump tables are unreachable in a ZDL and hard-freeze the
 * DSP), no CALLs or helper calls, no runtime divide (grain lengths are powers of
 * two chosen through an if/else ladder so each reciprocal is a literal), no
 * modulo (power-of-two ring mask), no static arrays, no .fardata, no
 * float->unsigned casts, denormals flushed. There is no feedback path: grains
 * are read from a frozen buffer and never written back, so the hold cannot run
 * away however long it is held.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "stasis_params.h"

#ifndef STASIS_AUDIO_FUNC
#define STASIS_AUDIO_FUNC Fx_DLY_Stasis
#endif

#define STASIS_DO_PRAGMA(x) _Pragma(#x)
#define STASIS_EXPAND_PRAGMA(x) STASIS_DO_PRAGMA(x)
#define STASIS_CODE_SECTION(f) STASIS_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

STASIS_CODE_SECTION(STASIS_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define SS_MAGIC   0x53545341u   /* 'STSA' */
#define SS_VERSION 1u

#define SS_BUF      65536u       /* 256 KB, ~1.49 s -- the capture ceiling */
#define SS_MASK     (SS_BUF - 1u)
#define SS_CAP_MIN  2646         /* ~60 ms */
#define SS_CAP_SPAN 60000        /* max capture 62646, inside the ring */
#define SS_VOICES   2
#define SS_DENORM   1.0e-18f
#define SS_WET_TRIM 0.62f        /* two overlapping windowed voices sum above
                                  * unity; by-ear match against the pack */

typedef struct StasisState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearPos;          /* lazy clear cursor */
    uint32_t writePos;
    uint32_t prevOn;
    uint32_t frozen;
    uint32_t rng;
    float    capBase;           /* absolute ring position the capture starts at */
    int32_t  capLen;
    float    gpos[SS_VOICES];
    int32_t  gph[SS_VOICES];
    float    lp;
    float    level;
} StasisState;

static inline float ss_soft(float x)
{
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return 1.5f * x - 0.5f * x * x * x;
}

static inline float ss_flush(float x)
{
    if (x < SS_DENORM && x > -SS_DENORM) return 0.0f;
    return x;
}

/* xorshift; returns 0..1. No library call, no divide (the scale is a literal). */
static inline float ss_rand(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return (float)(int32_t)(x & 0x00FFFFFFu) * 5.9604645e-8f;
}

static inline float ss_read(const float *buf, float pos)
{
    int32_t i0 = (int32_t)pos;
    float fr = pos - (float)i0;
    uint32_t a = ((uint32_t)i0) & SS_MASK;
    uint32_t b = (a + 1u) & SS_MASK;
    return buf[a] * (1.0f - fr) + buf[b] * fr;
}

void STASIS_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    /* Deliberately NO early return on params[0]. The bypass bit is this
     * effect's TRIGGER, and the ring has to stay full while bypassed so a stomp
     * captures the past instead of the future. */

    volatile unsigned int *desc = ZDL_PTR(volatile unsigned int *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u) return;

    uintptr_t bufBase = (base + sizeof(StasisState) + 7u) & ~(uintptr_t)7u;
    if (bufBase + (uintptr_t)SS_BUF * 4u > end) return;

    StasisState *st = (StasisState *)base;
    float *buf = (float *)bufBase;

    if (st->magic != SS_MAGIC || st->version != SS_VERSION || !st->initialized) {
        st->magic = SS_MAGIC;
        st->version = SS_VERSION;
        st->writePos = 0u;
        st->prevOn = (params[0] >= 0.5f) ? 1u : 0u;   /* don't fire on load */
        st->frozen = 0u;
        st->rng = 0x9E3779B9u;
        st->capBase = 0.0f;
        st->capLen = SS_CAP_MIN;
        int v;
        for (v = 0; v < SS_VOICES; v++) { st->gpos[v] = 0.0f; st->gph[v] = 0; }
        st->lp = 0.0f;
        st->level = 0.0f;
        st->clearPos = 0u;
        st->initialized = 1u;
    }

    /* Clear the ring in chunks rather than all at once: a 64K single-shot clear
     * at instantiation is long enough to matter on this DSP. */
    if (st->clearPos < SS_BUF) {
        uint32_t n = st->clearPos, lim = n + 2048u;
        if (lim > SS_BUF) lim = SS_BUF;
        for (; n < lim; n++) buf[n] = 0.0f;
        st->clearPos = n;
    }

    float ln  = zoom_param_norm01(params[STASIS_LENGTH_SLOT], STASIS_LENGTH_DEFAULT_NORM);
    float bl  = zoom_param_norm01(params[STASIS_BLUR_SLOT],   STASIS_BLUR_DEFAULT_NORM);
    float dc  = zoom_param_norm01(params[STASIS_DECAY_SLOT],  STASIS_DECAY_DEFAULT_NORM);
    float tn  = zoom_param_norm01(params[STASIS_TONE_SLOT],   STASIS_TONE_DEFAULT_NORM);
    float mix = zoom_param_norm01(params[STASIS_MIX_SLOT],    STASIS_MIX_DEFAULT_NORM);

    /* Grain length ladder. Powers of two so the window reciprocal is a literal;
     * if/else, never switch. */
    int32_t G; float invG;
    if      (bl < 0.20f) { G = 1024;  invG = 9.765625e-4f; }
    else if (bl < 0.40f) { G = 2048;  invG = 4.8828125e-4f; }
    else if (bl < 0.60f) { G = 4096;  invG = 2.44140625e-4f; }
    else if (bl < 0.80f) { G = 8192;  invG = 1.220703125e-4f; }
    else                 { G = 16384; invG = 6.103515625e-5f; }

    /* Decay: 1.0 is genuinely infinite. Below that, a per-sample multiplier
     * shaped so the knob's top half stays usefully long. */
    float decay;
    if (dc > 0.98f) decay = 1.0f;
    else {
        float k = 1.0f - dc;
        decay = 1.0f - (2.0e-6f + k * k * k * 9.0e-5f);
    }

    float lpCoef = 0.02f + tn * 0.55f;
    /* Mix is a HOLD LEVEL here, not a dry/wet crossfade -- the one deliberate
     * exception to the pack's convention.
     *
     * As a crossfade it attenuated the dry by (1 - Mix) at all times, so simply
     * adding Stasis to a patch cost 6 dB at the default Mix of 50 even with
     * nothing frozen, because the wet side is silent until you stomp. It is also
     * wrong musically: the whole point is to play over the held chord at full
     * strength, so the dry must not duck to make room for it. */
    float wetLvl = mix;

    uint32_t nowOn = (params[0] >= 0.5f) ? 1u : 0u;

    /* --- the trigger --- */
    if (nowOn && !st->prevOn) {
        int32_t cap = SS_CAP_MIN + (int32_t)(ln * ln * (float)SS_CAP_SPAN);
        if (cap < SS_CAP_MIN) cap = SS_CAP_MIN;
        if (cap > (int32_t)(SS_BUF - 1024u)) cap = (int32_t)(SS_BUF - 1024u);
        st->capLen = cap;
        st->capBase = (float)((int32_t)st->writePos - cap);
        st->frozen = 1u;
        st->level = 1.0f;
        int v;
        for (v = 0; v < SS_VOICES; v++) {
            st->gph[v] = (v * G) >> 1;               /* half a grain apart */
            st->gpos[v] = st->capBase;
        }
        st->lp = 0.0f;
    } else if (!nowOn && st->prevOn) {
        st->frozen = 0u;
    }
    st->prevOn = nowOn;

    int32_t maxOff = st->capLen - G;
    if (maxOff < 0) maxOff = 0;

    uint32_t wp = st->writePos;
    uint32_t frozen = st->frozen;
    uint32_t rng = st->rng;
    float lp = st->lp, level = st->level;

    int i;
    for (i = 0; i < 8; i++) {
        float dry = 0.5f * (fxBuf[i] + fxBuf[i + 8]);

        /* Record only while NOT frozen: this is what stops the audio you play
         * over the hold from overwriting the hold itself. */
        if (!frozen) {
            buf[wp & SS_MASK] = dry;
            wp++;
        }

        float wet = 0.0f;
        if (frozen) {
            int v;
            for (v = 0; v < SS_VOICES; v++) {
                int32_t ph = st->gph[v];
                /* triangular window, smoothstepped -- no cosine, no pow */
                float e;
                if (ph < (G >> 1)) e = (float)ph * (2.0f * invG);
                else               e = (float)(G - ph) * (2.0f * invG);
                if (e < 0.0f) e = 0.0f;
                else if (e > 1.0f) e = 1.0f;
                e = e * e * (3.0f - 2.0f * e);

                wet += ss_read(buf, st->gpos[v]) * e;

                st->gpos[v] += 1.0f;
                ph++;
                if (ph >= G) {
                    ph = 0;
                    /* new offset inside the capture: multiply, never modulo */
                    st->gpos[v] = st->capBase + (float)maxOff * ss_rand(&rng);
                }
                st->gph[v] = ph;
            }
            wet *= SS_WET_TRIM;

            lp += lpCoef * (wet - lp);
            wet = lp;

            level *= decay;
            wet *= level;
        }

        /* dry at unity, hold added on top; soft-clipped so a loud hold under
         * loud playing cannot fold over. */
        float out = dry + wetLvl * wet;
        out = ss_soft(out);
        fxBuf[i]     = out;
        fxBuf[i + 8] = out;
    }

    st->writePos = wp;
    st->rng = rng;
    st->lp = ss_flush(lp);
    st->level = ss_flush(level);
}
