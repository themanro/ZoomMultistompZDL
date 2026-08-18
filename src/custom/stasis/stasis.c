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
 * Sustain is a crossfade loop: the capture is read straight through, and a short
 * crossfade at the wrap fades into the same point one loop earlier so the join
 * is continuous rather than a click.
 *
 * The first version played overlapping grains from RANDOM offsets inside the
 * capture, on the theory that a loop betrays itself as a period while scattered
 * grains do not. That was the wrong trade. Consecutive grains were uncorrelated,
 * so every grain boundary was a phase discontinuity, and a sustained note -- the
 * material a freeze should hold most cleanly -- came out chopped. Audible
 * periodicity is a far smaller sin than chop.
 *
 * 5 knobs. Length and Blur sit on knobs 1-2, the verbatim-stock LineSel edit
 * handlers -- the most reliable pair in the pack, and the two that shape the
 * sound most:
 *   Length (knob 1) how much of the past is captured, ~60 ms .. ~1.4 s. Short
 *                   is a single grain of a chord; long is a whole phrase.
 *   Blur   (knob 2) crossfade length at the loop seam, 5.8..93 ms. Short is a
 *                   tighter, more defined hold; long smears the join away
 *                   entirely. Clamped so it always fits twice inside the loop.
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
#define SS_VERSION 2u   /* playback rewritten: crossfade loop, not random grains */

#define SS_BUF      65536u       /* 256 KB, ~1.49 s -- the capture ceiling */
#define SS_MASK     (SS_BUF - 1u)
#define SS_CAP_MIN  2646         /* ~60 ms */
#define SS_CAP_SPAN 60000        /* max capture 62646, inside the ring */
#define SS_DENORM   1.0e-18f
#define SS_WET_TRIM 0.9f

typedef struct StasisState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearPos;          /* lazy clear cursor */
    uint32_t writePos;
    uint32_t prevOn;
    uint32_t frozen;
    float    capBase;           /* absolute ring position the capture starts at */
    int32_t  capLen;
    float    pos;               /* single read head; the seam is crossfaded */
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
        st->capBase = 0.0f;
        st->capLen = SS_CAP_MIN;
        st->pos = 0.0f;
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

    /* Blur is the CROSSFADE length at the loop seam, not a grain size.
     *
     * v1 played overlapping grains that each jumped to a RANDOM offset inside
     * the capture. That is fine for smearing noise and wrong for everything
     * else: consecutive grains were uncorrelated, so every grain boundary was a
     * phase discontinuity, and a sustained note -- the material that should hold
     * most cleanly -- came out chopped. Playback is now a plain loop read with a
     * short crossfade hiding the wrap, which is what a freeze actually wants.
     * Powers of two so each reciprocal is a literal; if/else, never switch. */
    int32_t F; float invF;
    if      (bl < 0.20f) { F = 256;  invF = 3.90625e-3f; }
    else if (bl < 0.40f) { F = 512;  invF = 1.953125e-3f; }
    else if (bl < 0.60f) { F = 1024; invF = 9.765625e-4f; }
    else if (bl < 0.80f) { F = 2048; invF = 4.8828125e-4f; }
    else                 { F = 4096; invF = 2.44140625e-4f; }

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
        {   /* keep the base positive so the seam read never goes negative */
            float cb = (float)((int32_t)st->writePos - cap);
            if (cb < 0.0f) cb += (float)SS_BUF;
            st->capBase = cb;
        }
        st->frozen = 1u;
        st->level = 1.0f;
        st->pos = st->capBase;
        st->lp = 0.0f;
    } else if (!nowOn && st->prevOn) {
        st->frozen = 0u;
    }
    st->prevOn = nowOn;

    /* The crossfade has to fit twice inside the loop or the seam never resolves. */
    int32_t L = st->capLen;
    while (F * 2 > L && F > 64) { F >>= 1; invF *= 2.0f; }

    uint32_t wp = st->writePos;
    uint32_t frozen = st->frozen;
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
            float pos = st->pos;
            float endp = st->capBase + (float)L;
            float dist = endp - pos;
            wet = ss_read(buf, pos);
            if (dist < (float)F) {
                /* approaching the seam: fade into the same point one loop back,
                 * which is exactly where the head is about to jump to, so the
                 * join is continuous instead of a click */
                float t = ((float)F - dist) * invF;
                if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
                t = t * t * (3.0f - 2.0f * t);
                float q = pos - (float)L;
                if (q < 0.0f) q += (float)SS_BUF;
                wet = wet * (1.0f - t) + ss_read(buf, q) * t;
            }
            pos += 1.0f;
            if (pos >= endp) pos -= (float)L;
            st->pos = pos;
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
    st->lp = ss_flush(lp);
    st->level = ss_flush(level);
}
