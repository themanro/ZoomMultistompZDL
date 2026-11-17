/*
 * chopdly.c -- Chopdly: a delay that chops what it recorded and plays the
 * chops back rearranged, MS-70CDR.
 *
 * Every cycle (one delay time long) it plays back the last cycle's audio cut
 * into chops. Each chop keeps its place in time ("slot"), but what plays in it
 * is another chop's audio, optionally reversed and pitched. The chopped output
 * goes back into the recording through Feedbk, so repeats get re-chopped.
 *
 * 9 knobs. Chops, Order, Pitch, Reverse and Stretch are named steps (the
 * pedal shows the names: build/selector_metadata.py, which must match the
 * step numbers here); the shared edit handler stores raw/100, read back as
 * round(norm * 100) like Stasis.
 *   Time    (params[5])  cycle / delay length, 30 ms .. 2.5 s (0..100)
 *   Chops   (params[6])  0 Trans (cut at note attacks), 1..16 equal chops
 *   Order   (params[7])  0 Fwd, 1 Back, 2 Swap, 3 Skip, 4 Stutr, 5 Build,
 *                        6-13 Mix1-8 (fixed shuffles), 14-19 Rpt1-6 (fixed,
 *                        chops repeat or drop), 20 Rand, 21 Chaos (both new
 *                        every cycle)
 *   Pitch   (params[8])  0-6 all chops -12/-7/-5/0/+5/+7/+12; 7 Oct+, 8 Oct-,
 *                        9 Octs, 10 Fifth, 11 Some, 12 Wild (some chops jump)
 *   Reverse (params[9])  0 Off, 1 Few, 2 Alt, 3 Half, 4 Most, 5 All;
 *                        6 Ping (chops alternate L / R), 7 Scatr (each slot
 *                        a fixed pan); both play the chops forward
 *   Stretch (params[10]) 0-3 = 1/4, 1/3, 1/2, 3/4 speed; 4 Off (plain read);
 *                        5 1.5x, 6 2x. Pitch kept (two-grain crossfade);
 *                        Pitch still works on top of it.
 *   Glide   (params[11]) chop edges: 0 = a 32-sample anti-click fade, cut
 *                        where chopped; up = longer fades (up to half the
 *                        shorter neighbour at 100), and the chop that ends
 *                        rings on past its slot (a tail voice) so the two
 *                        crossfade instead of dipping
 *   Feedbk  (params[12]) chopped output back into the recording
 *   Mix     (params[13]) dry/wet
 *
 * Screen (zvis, MS-70CDR 2.10 only; elsewhere the static cover stays): the
 * recording being chopped as a waveform, dotted lines at the cuts, the chop
 * playing now inverted with its playhead (moving back when reversed), a cycle
 * marker under the waveform showing which slot is playing. Header, two lines
 * of 3x5 text: the chop count (TRANS when cut at attacks) and the Order step;
 * the Pitch, Reverse and Stretch steps. The step names come from the same
 * list as the knob names (build.py -> chopdly_art.h).
 *
 * A given knob setting always gives the same arrangement, so a patch recalls
 * what you dialled in (except Order Rand / Chaos, new every cycle). Reverse
 * uses fixed per-slot coins, so Few, Half and Most reverse nested sets.
 *
 * Safe-DSP (docs/SAFE-DSP-RULES.md): no switch, no math lib, no divide (a
 * Newton reciprocal instead), no float->unsigned cast, no static arrays (all
 * state and the ring live in the ctx[3] arena, validated and cleared lazily),
 * unity-slope soft clip inside the feedback loop, magic shuttle preserved.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "chopdly_params.h"
#include "../common/zvis.h"
#include "chopdly_art.h"

#ifdef CHOPDLY_TRACE
#include <stdio.h>
#endif

#ifndef CHOPDLY_AUDIO_FUNC
#define CHOPDLY_AUDIO_FUNC Fx_DLY_Chopdly
#endif

/* The pedal hands us 32-bit words; the desktop harness needs pointer-sized ones. */
#ifdef CHOPDLY_HOST
typedef uintptr_t cd_word;
#else
typedef unsigned int cd_word;
#endif

#define CD_DO_PRAGMA(x) _Pragma(#x)
#define CD_EXPAND_PRAGMA(x) CD_DO_PRAGMA(x)
#define CD_CODE_SECTION(f) CD_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
#define CD_INLINE(f) CD_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(f))

CD_CODE_SECTION(CHOPDLY_AUDIO_FUNC)

extern unsigned int _Fx_DLY_Chopdly_Info[];   /* imageInfo: the linker's symbol */

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define CD_MAGIC   0x43484F50u        /* 'CHOP' */
#define CD_VERSION 8u
#define CD_FXID    947u

#define CD_RING       262144u         /* int16 ring, 512 KB, ~5.9 s */
#define CD_MASK       (CD_RING - 1u)
#define CD_RING_WORDS (CD_RING / 2u)
#define CD_CLEAR_STEP 4096u           /* words cleared per call */
#define CD_TMIN       1323.0f         /* 30 ms */
#define CD_TMAX       110250.0f       /* 2.5 s; ring >= 2 * TMAX keeps a segment intact */
#define CD_MAXCH      16u
#define CD_MINCHOP    1024u           /* shortest transient chop, ~23 ms */
#define CD_MINEQ      512.0f          /* shortest equal chop */
#define CD_XMIN       32.0f           /* shortest chop edge fade (Glide 0) */
#define CD_ONSETS     32u
#define CD_HOLD       2205u           /* 50 ms between detected attacks */
#define CD_QWIN       1024u           /* look this far before an attack (~23 ms) for the cut */
#define CD_FB_MAX     0.95f
#define CD_S2F        (1.0f / 32768.0f)
#define CD_GRAIN      2048u           /* stretch grain, ~46 ms (power of 2) */
#define CD_GHALF      1024u
#define CD_GHALF_INV  (1.0f / 1024.0f)
#define CD_OV_MID     23              /* waveform rows 15..31 */
#define CD_OV_H       8
#define CD_OV_TOP     15
/* header cells (x = 1 + 4 * cell; line 1 = cell + 32). build.py's TAGS put
 * ORDER at 21, PITCH at 32, REV at 44, STR at 54. */
#define CD_COL_CHOPS_END 20u          /* chop count ends just before this cell */
#define CD_COL_ORDER  27u
#define CD_COL_PITCH  38u
#define CD_COL_REV    48u
#define CD_COL_STR    58u

typedef struct ChopState {
    uint32_t magic, version, initialized, clearIndex;

    uint32_t wp;                      /* ring write position */
    uint32_t cyc, T;                  /* position in cycle, cycle length (latched) */
    uint32_t seg;                     /* ring index of the segment being chopped */
    uint32_t n;                       /* chops this cycle */
    uint32_t k;                       /* current slot */
    uint32_t cycN;                    /* cycles played (Rand / Chaos seed) */
    uint32_t slotStart, slotEnd;
    float    srcBase, slotLen;        /* where the slot reads from, its length */
    uint32_t rev;                     /* current slot reversed */
    uint32_t revMask;
    float    xb[CD_MAXCH + 1u];       /* edge fade length at each slot's start */
    float    rx[CD_MAXCH + 1u];       /* its reciprocal */
    float    rIn, rOut, ovOut;        /* current slot: 1/fade in, 1/fade out, tail past its end */
    float    cA, cB, cWa, cWb, cStep; /* current chop: next read positions, weights, step */
    float    tA, tB, tWa, tWb, tStep, tGain, tDec;   /* tail of the chop before */
    uint32_t cSeg, tSeg, tLeft;
    float    gl;                      /* Glide, this block */
    float    rp;                      /* read position within the slot */
    float    gA, gB;                  /* stretch grain start positions */
    uint32_t gph;                     /* stretch grain phase, 0..CD_GRAIN-1 */
    uint32_t fresh;                   /* slot just started: seed the grains */

    float    envF, envS;              /* attack detector */
    uint32_t hold;
    uint32_t onW;                     /* attacks written (ring of CD_ONSETS) */

    float    lim;                     /* feedback limiter envelope */

    uint32_t b[CD_MAXCH + 1u];        /* slot boundaries, offsets into the segment */
    uint32_t perm[CD_MAXCH];          /* which chop plays in each slot */
    float    rateK[CD_MAXCH];         /* each slot's pitch, as a read rate */
    float    panL[CD_MAXCH], panR[CD_MAXCH];   /* each slot's wet gain per side */
    float    gL, gR;                  /* current slot's */
    uint32_t on[CD_ONSETS];           /* attack positions (ring indexes) */

    /* screen */
    uint32_t colk;                    /* column = (pos * colk) >> 16 */
    uint32_t ovcol;                   /* column being recorded */
    uint32_t showOff;                 /* playhead: segment offset being read */
    uint32_t step;                    /* frame-drawing step */
    uint32_t trans;                   /* this cycle is cut at attacks */
    unsigned char ovRec[ZV_W];        /* peak per column, recording now */
    unsigned char ovShow[ZV_W];       /* peak per column, the segment being chopped */
    unsigned char ln[64];             /* header: font index per cell, 2 lines x 32 */
    ZvState  z;
} ChopState;

CD_INLINE(cd_abs)
static inline float cd_abs(float x) { return x < 0.0f ? -x : x; }

/* 1/x without a divide: bit-trick seed + three Newton steps (x > 0). */
CD_INLINE(cd_rcp)
static inline float cd_rcp(float x)
{
    union { float f; uint32_t u; } c;
    c.f = x;
    c.u = 0x7EF311C7u - c.u;
    float y = c.f;
    y = y * (2.0f - x * y);
    y = y * (2.0f - x * y);
    y = y * (2.0f - x * y);
    return y;
}

/* unity-slope soft clip: no small-signal gain, safe inside feedback */
CD_INLINE(cd_soft)
static inline float cd_soft(float x)
{
    if (x > 1.5f) return 1.0f;
    if (x < -1.5f) return -1.0f;
    return x - (x * x * x) * 0.14814815f;
}

/* 2^(semi/12) for semi in -12..12, straight-line: 2^y = e^(y ln 2) as a
 * 7-term series (|y ln2| <= 0.694, error < 2e-6, ~0.003 cents). No loop: the
 * repeated-multiply version compiled to SPLOOPs and misbehaved in the emulator. */
CD_INLINE(cd_semi)
static inline float cd_semi(int semi)
{
    float x = (float)semi * (0.69314718f / 12.0f);
    return 1.0f + x * (1.0f + x * (0.5f + x * (0.16666667f + x * (0.041666668f
         + x * (0.0083333338f + x * 0.0013888889f)))));
}

/* Linear read at an offset from seg. A chop that runs past its own audio
 * (pitched up, stretched, or the tail) carries on into what was recorded
 * next, or before it when reversed. A position not recorded yet (past
 * avail, the samples since seg) bounces back off the record point and runs
 * backwards: no jump. Floor by hand: offsets can be negative. */
CD_INLINE(cd_read)
static inline float cd_read(const int16_t *ring, uint32_t seg, uint32_t wp, float off)
{
    float avail = (float)(int)((wp - seg) & CD_MASK) - 1.0f;
    if (off > avail) off = avail + avail - off;
    int oi = (int)off;
    if ((float)oi > off) oi--;
    float fr = off - (float)oi;
    uint32_t i0 = (seg + (uint32_t)oi) & CD_MASK;
    uint32_t i1 = (i0 + 1u) & CD_MASK;
    return ((float)ring[i0] * (1.0f - fr) + (float)ring[i1] * fr) * CD_S2F;
}

/* Vertical span of rows y0..y1 in column x: set (black) or flipped. */
CD_INLINE(cd_vspan)
static inline void cd_vspan(ZvState *z, int x, int y0, int y1, int flip)
{
    int b;
    if ((unsigned)x >= (unsigned)ZV_W) return;
    for (b = y0 >> 3; b <= y1 >> 3; ++b) {
        int lo = b == y0 >> 3 ? y0 & 7 : 0, hi = b == y1 >> 3 ? y1 & 7 : 7;
        unsigned char m = (unsigned char)((0xFFu << lo) & (0xFFu >> (7 - hi)));
        unsigned char *p = &z->frame[b * ZV_W + x];
        *p = flip ? (unsigned char)(*p ^ m) : (unsigned char)(*p | m);
    }
}

CD_INLINE(cd_hash)
static inline uint32_t cd_hash(uint32_t h)
{
    h *= 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

/* The chop that just ended keeps playing as the tail: from where it would
 * read next, at its step, fading from its gain at the slot edge to 0 over
 * ovOut samples (Glide 0: none). */
CD_INLINE(cd_tail)
static inline void cd_tail(ChopState *st)
{
    st->tSeg = st->cSeg;
    st->tA = st->cA; st->tB = st->cB; st->tWa = st->cWa; st->tWb = st->cWb;
    st->tStep = st->cStep;
    st->tGain = st->ovOut * st->rOut;
    st->tDec = st->rOut;
    st->tLeft = (uint32_t)(int)st->ovOut;
}

CD_INLINE(cd_slot)
static inline void cd_slot(ChopState *st)
{
    uint32_t k = st->k;
    st->slotStart = st->b[k];
    st->slotEnd = st->b[k + 1u];
    st->slotLen = (float)(int)(st->slotEnd - st->slotStart);
    st->srcBase = (float)(int)st->b[st->perm[k]];
    st->rev = (st->revMask >> k) & 1u;
    st->rIn = st->rx[k];
    st->rOut = st->rx[k + 1u];
    st->cSeg = st->seg;
    st->ovOut = st->gl * st->xb[k + 1u];
    st->gL = st->panL[k];
    st->gR = st->panR[k];
    st->rp = 0.0f;
    st->fresh = 1u;
}

void CHOPDLY_AUDIO_FUNC(cd_word *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    cd_word *magicSrc = ZDL_PTR(cd_word *, ctx[12]);
    cd_word *magicDst = ZDL_PTR(cd_word *, *(cd_word *)ZDL_PTR(cd_word *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile cd_word *desc = ZDL_PTR(volatile cd_word *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u) return;

    uintptr_t ringBase = (base + sizeof(ChopState) + 7u) & ~(uintptr_t)7u;
    if (ringBase + (uintptr_t)CD_RING * 2u > end) return;

    ChopState *st = (ChopState *)base;
    int16_t *ring = (int16_t *)ringBase;

    if (st->magic != CD_MAGIC || st->version != CD_VERSION) {
        st->magic = CD_MAGIC;
        st->version = CD_VERSION;
        st->initialized = 0u;
        st->clearIndex = 0u;
        st->wp = 0u;
        st->cyc = 0u;
        st->T = 0u;                   /* 0 = start a cycle on the first sample */
        st->seg = 0u;
        st->n = 1u;
        st->k = 0u;
        st->cycN = 0u;
        st->slotStart = 0u;
        st->slotEnd = 0u;
        st->srcBase = 0.0f;
        st->slotLen = 1.0f;
        st->rev = 0u;
        st->revMask = 0u;
        st->xb[0] = CD_XMIN;
        st->xb[1] = CD_XMIN;
        st->rx[0] = 1.0f / CD_XMIN;
        st->rx[1] = 1.0f / CD_XMIN;
        st->cSeg = 0u;
        st->rIn = 1.0f / CD_XMIN;
        st->rOut = 1.0f / CD_XMIN;
        st->ovOut = 0.0f;
        st->tLeft = 0u;
        st->gl = 0.0f;
        st->rp = 0.0f;
        st->rateK[0] = 1.0f;
        st->panL[0] = 1.0f;
        st->panR[0] = 1.0f;
        st->gL = 1.0f;
        st->gR = 1.0f;
        st->gA = 0.0f;
        st->gB = 0.0f;
        st->gph = 0u;
        st->fresh = 1u;
        st->envF = 0.0f;
        st->envS = 0.0f;
        st->hold = 0u;
        st->onW = 0u;
        st->lim = 0.0f;
        st->b[0] = 0u;
        st->b[1] = 0u;
        st->perm[0] = 0u;
        st->colk = 0u;
        st->ovcol = 0xFFFFu;
        st->showOff = 0u;
        st->step = 0u;
        st->trans = 0u;
        {
            uint32_t c;
            for (c = 0u; c < (uint32_t)ZV_W; c++) { st->ovRec[c] = 0u; st->ovShow[c] = 0u; }
        }
        zv_init(&st->z, CD_FXID, _Fx_DLY_Chopdly_Info);
    }

    if (!st->initialized) {           /* clear the ring, a chunk per call */
        uint32_t *w = (uint32_t *)ringBase;
        uint32_t e = st->clearIndex + CD_CLEAR_STEP;
        if (e > CD_RING_WORDS) e = CD_RING_WORDS;
        uint32_t i;
        for (i = st->clearIndex; i < e; i++) w[i] = 0u;
        st->clearIndex = e;
        if (e >= CD_RING_WORDS) st->initialized = 1u;
        return;
    }

    float time  = zoom_param_norm01(params[CHOPDLY_TIME_SLOT],    CHOPDLY_TIME_DEFAULT_NORM);
    /* named steps: raw/100 from the edit handler, back to the step number */
    int chopI = (int)(zoom_param_norm01(params[CHOPDLY_CHOPS_SLOT],   CHOPDLY_CHOPS_UI_DEFAULT * 0.01f)   * 100.0f + 0.5f);
    int ordI  = (int)(zoom_param_norm01(params[CHOPDLY_ORDER_SLOT],   CHOPDLY_ORDER_UI_DEFAULT * 0.01f)   * 100.0f + 0.5f);
    int pitI  = (int)(zoom_param_norm01(params[CHOPDLY_PITCH_SLOT],   CHOPDLY_PITCH_UI_DEFAULT * 0.01f)   * 100.0f + 0.5f);
    int revI  = (int)(zoom_param_norm01(params[CHOPDLY_REVERSE_SLOT], CHOPDLY_REVERSE_UI_DEFAULT * 0.01f) * 100.0f + 0.5f);
    int strI  = (int)(zoom_param_norm01(params[CHOPDLY_STRETCH_SLOT], CHOPDLY_STRETCH_UI_DEFAULT * 0.01f) * 100.0f + 0.5f);
    if (chopI > 16) chopI = 16;
    if (ordI > 21) ordI = 21;
    if (pitI > 12) pitI = 12;
    if (revI > 7) revI = 7;
    if (strI > 6) strI = 6;
    float glide = zoom_param_norm01(params[CHOPDLY_GLIDE_SLOT],   CHOPDLY_GLIDE_DEFAULT_NORM);
    float fbk   = zoom_param_norm01(params[CHOPDLY_FEEDBK_SLOT],  CHOPDLY_FEEDBK_DEFAULT_NORM);
    float mix   = zoom_param_norm01(params[CHOPDLY_MIX_SLOT],     CHOPDLY_MIX_DEFAULT_NORM);

    float fb = fbk * CD_FB_MAX;
    float wetG = mix, dryG = 1.0f - mix;

    st->gl = glide;

    /* Stretch: source speed S. Step 4 = off (plain read, no grains). */
    int stretchOn = strI != 4;
    float S = strI == 0 ? 0.25f : strI == 1 ? 0.33333334f : strI == 2 ? 0.5f
            : strI == 3 ? 0.75f : strI == 5 ? 1.5f : strI == 6 ? 2.0f : 1.0f;

    uint32_t wp = st->wp, cyc = st->cyc;
    float envF = st->envF, envS = st->envS, lim = st->lim;
    uint32_t hold = st->hold;

    int f;
    for (f = 0; f < 8; f++) {
        float inL = fxBuf[f];
        float inR = fxBuf[f + 8];
        float in = 0.5f * (inL + inR);

        /* --- attack detector: fast envelope jumping well above the slow one --- */
        float a = cd_abs(in);
        envF += (a - envF) * (a > envF ? 0.5f : 0.01f);
        envS += (a - envS) * 0.0005f;
        if (hold) hold--;
        else if (envF > envS * 2.5f + 0.005f) {
            /* cut at the quietest recorded sample in the ~23 ms before the
             * attack: the previous chop ends in a gap or at a zero crossing,
             * not mid-decay. A small penalty per sample back (up to ~-42 dBFS
             * at 23 ms) prefers a crossing close to the attack, keeping the
             * attack near the start of its chop. */
            uint32_t j, best = (wp - 8u) & CD_MASK;
            int bv = 0x10000;
            for (j = 8u; j < CD_QWIN; j++) {
                uint32_t ix = (wp - j) & CD_MASK;
                int sv = ring[ix];
                if (sv < 0) sv = -sv;
                sv += (int)(j >> 2);
                if (sv < bv) { bv = sv; best = ix; }
            }
            st->on[st->onW & (CD_ONSETS - 1u)] = best;
            st->onW++;
            hold = CD_HOLD;
        }

        /* --- new cycle: chop the segment just recorded --- */
        if (cyc >= st->T) {
            float Tf = CD_TMIN + time * time * (CD_TMAX - CD_TMIN);
            uint32_t T = (uint32_t)(int)Tf;
            Tf = (float)(int)T;
            uint32_t seg = (wp - T) & CD_MASK;
            uint32_t n = 0u, i;
            int trans = chopI == 0;

            if (trans) {              /* cut at the attacks that fall in this segment */
                uint32_t cnt = st->onW < CD_ONSETS ? st->onW : CD_ONSETS;
                uint32_t first = st->onW - cnt;
                n = 1u;
                st->b[0] = 0u;
                for (i = 0u; i < cnt && n < CD_MAXCH; i++) {
                    uint32_t off = (st->on[(first + i) & (CD_ONSETS - 1u)] - seg) & CD_MASK;
                    if (off >= T) continue;
                    if (off < st->b[n - 1u] + CD_MINCHOP) continue;
                    if (T - off < CD_MINCHOP) continue;
                    st->b[n++] = off;
                }
                if (n == 1u) trans = 0;   /* no attacks: fall back to equal chops */
            }
            if (!trans) {
                n = chopI == 0 ? 4u : (uint32_t)chopI;
                if (n < 1u) n = 1u;
                if (n > CD_MAXCH) n = CD_MAXCH;
                while (n > 1u && Tf * cd_rcp((float)(int)n) < CD_MINEQ) n--;
                float step = Tf * cd_rcp((float)(int)n);
                for (i = 0u; i < n; i++) st->b[i] = (uint32_t)(int)((float)(int)i * step);
            }
            st->b[n] = T;

            /* Glide: each slot edge fades over 32 + glide^2 / 2 of the shorter
             * chop on either side (the last chop neighbours the first). */
            {
                float gq = glide * glide * 0.5f;
                for (i = 0u; i < n; i++) {
                    uint32_t j = i ? i - 1u : n - 1u;
                    uint32_t la = st->b[i + 1u] - st->b[i], lb = st->b[j + 1u] - st->b[j];
                    float x = CD_XMIN + gq * (float)(int)(la < lb ? la : lb);
                    st->xb[i] = x;
                    st->rx[i] = cd_rcp(x);
                }
                st->xb[n] = st->xb[0];
                st->rx[n] = st->rx[0];
            }

            /* arrangement per Order step. Mix/Rpt are fixed (a patch recalls);
             * Rand/Chaos reseed every cycle. */
            st->cycN++;
            for (i = 0u; i < n; i++) st->perm[i] = i;
            if (ordI == 1) {          /* Back */
                for (i = 0u; i < n; i++) st->perm[i] = n - 1u - i;
            } else if (ordI == 2) {   /* Swap: 1 0 3 2 ... */
                for (i = 0u; i + 1u < n; i += 2u) { st->perm[i] = i + 1u; st->perm[i + 1u] = i; }
            } else if (ordI == 3) {   /* Skip: 0 2 4 .. 1 3 5 .. */
                uint32_t j = 0u;
                for (i = 0u; i < n; i += 2u) st->perm[j++] = i;
                for (i = 1u; i < n; i += 2u) st->perm[j++] = i;
            } else if (ordI == 4) {   /* Stutr: 0 0 2 2 4 4 .. */
                for (i = 0u; i < n; i++) st->perm[i] = i & ~1u;
            } else if (ordI == 5) {   /* Build: 0, 0 1, 0 1 2, .. */
                uint32_t c = 0u, top = 0u;
                for (i = 0u; i < n; i++) {
                    st->perm[i] = c;
                    if (c >= top) { top++; c = 0u; } else c++;
                }
            } else if (ordI >= 6) {
                int shuffle = ordI <= 13 || ordI == 20;
                uint32_t seed = ordI >= 20 ? st->cycN * 2654435761u : (uint32_t)ordI * 7919u;
                uint32_t rng = cd_hash(seed + n);
                if (shuffle) {        /* every chop once */
                    for (i = n - 1u; i > 0u; i--) {
                        rng = cd_hash(rng + i);
                        uint32_t j = ((rng >> 16) * (i + 1u)) >> 16;
                        uint32_t t = st->perm[i]; st->perm[i] = st->perm[j]; st->perm[j] = t;
                    }
                } else {              /* chops can repeat or drop out */
                    for (i = 0u; i < n; i++) {
                        rng = cd_hash(rng + i);
                        st->perm[i] = ((rng >> 16) * n) >> 16;
                    }
                }
            }

            /* reverse: fixed per-slot coins, so Few/Half/Most nest. The coins
             * step by the golden ratio (40503/65536): evenly spread at any chop
             * count, so Few/Half/Most really are ~1/4, 1/2, 3/4 of the chops. */
            uint32_t revMask = 0u;
            uint32_t thr = revI == 1 ? 16384u : revI == 3 ? 32768u : revI == 4 ? 49152u
                         : revI == 5 ? 65536u : 0u;
            for (i = 0u; i < n; i++)
                if ((((i + 1u) * 40503u) & 0xFFFFu) < thr) revMask |= 1u << i;
            if (revI == 2) revMask = 0xAAAAu;   /* Alt: every other slot */

            /* stereo (Reverse 6 Ping, 7 Scatr): the recording stays mono, each
             * slot gets a pan p (0 = L, 1 = R). Ping alternates hard L / R;
             * Scatr uses a fixed golden coin per slot (offset from the other
             * two), so neighbours land far apart and a patch recalls the same
             * placement. Gains 1 - p^2 / 1 - (1-p)^2: 0.75 each in the middle.
             * The chop fades hide the jump at each slot edge. */
            {
                int panOn = revI >= 6, ping = revI == 6;
                for (i = 0u; i < n; i++) {
                    uint32_t c = ((i + 1u) * 40503u + 43690u) & 0xFFFFu;
                    float pp = ping ? (float)(int)(i & 1u) : (float)(int)c * 1.5259022e-05f;
                    float q = 1.0f - pp;
                    st->panL[i] = panOn ? 1.0f - pp * pp : 1.0f;
                    st->panR[i] = panOn ? 1.0f - q * q : 1.0f;
                }
            }

            /* pitch per slot. Steps 0-6 move every chop (base); 7-12 use a fixed
             * golden coin per slot (c, offset from Reverse's): below thrA ->
             * semiA (or a hashed random jump when jA), below thrB -> semiB, else
             * base. Picked once here so the loop is two compares: an if-chain
             * per slot made cg6x run for minutes. Rate from cd_semi: no powf. */
            int base = pitI == 0 ? -12 : pitI == 1 ? -7 : pitI == 2 ? -5
                     : pitI == 4 ? 5 : pitI == 5 ? 7 : pitI == 6 ? 12 : 0;
            uint32_t thrA = 0u, thrB = 0u;
            int semiA = 0, semiB = 0, jA = 0;
            if (pitI == 7)       { thrA = 32768u; semiA = 12; }
            else if (pitI == 8)  { thrA = 32768u; semiA = -12; }
            else if (pitI == 9)  { thrA = 21845u; semiA = 12; thrB = 43690u; semiB = -12; }
            else if (pitI == 10) { thrA = 21845u; semiA = 7;  thrB = 43690u; semiB = -5; }
            else if (pitI == 11) { thrA = 32768u; jA = 1; }
            else if (pitI == 12) { thrA = 65536u; jA = 1; }
            for (i = 0u; i < n; i++) {
                uint32_t h = cd_hash(i + 211u);
                uint32_t c = ((i + 1u) * 40503u + 21845u) & 0xFFFFu;   /* golden, as Reverse */
                uint32_t pick = ((h & 0xFFFFu) * 6u) >> 16;              /* 0..5 */
                int down = pick < 3u;
                int mm = (int)(down ? pick : pick - 3u);                 /* 0..2 */
                int mag = 12 - 5 * (mm >= 1) - 2 * (mm >= 2);            /* 12, 7, 5 */
                int a = jA ? (down ? -mag : mag) : semiA;
                int semi = c < thrA ? a : c < thrB ? semiB : base;
                st->rateK[i] = cd_semi(semi);
            }

            st->T = T;
            st->seg = seg;
            st->n = n;
            st->revMask = revMask;
            st->k = 0xFFFFFFFFu;      /* the slot loop below enters slot 0, and */
            st->slotEnd = 0u;         /* hands the last chop over as the tail */
            st->trans = (uint32_t)trans;
            cyc = 0u;
            {                         /* the segment just recorded is the one shown */
                uint32_t c;
                for (c = 0u; c < (uint32_t)ZV_W; c++) { st->ovShow[c] = st->ovRec[c]; st->ovRec[c] = 0u; }
                st->colk = (uint32_t)(int)(8388608.0f * cd_rcp(Tf));   /* 128 << 16 / T */
                st->ovcol = 0xFFFFu;
            }
#ifdef CHOPDLY_TRACE
            fprintf(stderr, "cycle T=%u n=%u%s rev=%04x |", T, n, trans ? " trans" : "", revMask);
            for (i = 0u; i < n; i++) fprintf(stderr, " %u<-%u@%.2f", st->b[i], st->perm[i], st->rateK[i]);
            fprintf(stderr, "\n");
#endif
        }
        while (cyc >= st->slotEnd && st->k + 1u < st->n) { cd_tail(st); st->k++; cd_slot(st); }

        /* --- read the chop that plays in this slot --- */
        float o = (float)(int)(cyc - st->slotStart);
        float L = st->slotLen;
        float tgt = st->rateK[st->k];
        float rr = tgt;
        float dir = st->rev ? -1.0f : 1.0f;
        float step = dir * rr;
        float pA, pB, wa, wb;         /* two reads, so the tail can carry on either */
        if (!stretchOn) {             /* plain read: pitch = speed, like a sampler */
            pA = st->rev ? st->srcBase + (L - 1.0f) * tgt - st->rp : st->srcBase + st->rp;
            pB = pA;
            wa = 1.0f;
            wb = 0.0f;
            st->rp += rr;
            st->showOff = (uint32_t)(int)(pA >= 0.0f ? pA : 0.0f);
        } else {                      /* two grains, half a grain apart, triangle windows
                                       * summing to 1. Each grain plays at the pitch rate;
                                       * the source position moves at rate * S. */
            float start = st->rev ? st->srcBase + (L - 1.0f) * tgt * S : st->srcBase;
            float pos = start + dir * st->rp;
            st->showOff = (uint32_t)(int)(pos >= 0.0f ? pos : 0.0f);
            if (st->fresh) {
                st->gA = pos;
                st->gB = pos - step * (float)(int)CD_GHALF;
                st->gph = 0u;
                st->fresh = 0u;
            }
            uint32_t pa = st->gph, pb = (pa + CD_GHALF) & (CD_GRAIN - 1u);
            float fa = (float)(int)pa, fpb = (float)(int)pb;
            wa = 1.0f - cd_abs(fa * CD_GHALF_INV - 1.0f);
            wb = 1.0f - cd_abs(fpb * CD_GHALF_INV - 1.0f);
            pA = st->gA + step * fa;
            pB = st->gB + step * fpb;
            st->rp += rr * S;
            pa = (pa + 1u) & (CD_GRAIN - 1u);
            if (pa == 0u) st->gA = pos;
            if (pa == CD_GHALF) st->gB = pos;
            st->gph = pa;
        }
        float smp = cd_read(ring, st->seg, wp, pA) * wa + cd_read(ring, st->seg, wp, pB) * wb;
        st->cStep = step;             /* for the tail: both reads run on, weights frozen */
        st->cA = pA + step;
        st->cB = pB + step;
        st->cWa = wa;
        st->cWb = wb;

        float g1 = o * st->rIn, g2 = (L + st->ovOut - o) * st->rOut;
        float g = g1 < g2 ? g1 : g2;
        if (g > 1.0f) g = 1.0f;
        float wet = smp * g;
        if (st->tLeft) {              /* the chop before, fading out under this one */
            st->tLeft--;
            wet += (cd_read(ring, st->tSeg, wp, st->tA) * st->tWa + cd_read(ring, st->tSeg, wp, st->tB) * st->tWb) * st->tGain;
            st->tA += st->tStep;
            st->tB += st->tStep;
            st->tGain -= st->tDec;
        }

        /* --- feedback: record input + chopped output, limited --- */
        float v = in + fb * wet;
        float av = cd_abs(v);
        lim = av > lim ? av : lim * 0.9997f;
        if (lim > 0.8f) v *= 0.8f * cd_rcp(lim);
        v = cd_soft(v);
        int q = (int)(v * 32767.0f);
        if (q > 32767) q = 32767; else if (q < -32767) q = -32767;
        ring[wp] = (int16_t)q;
        {                             /* screen: this sample's peak into its column */
            uint32_t col = (cyc * st->colk) >> 16;
            float av2 = cd_abs(v);   /* soft scale: quiet stays visible, loud doesn't fill */
            uint32_t h = (uint32_t)(int)(10.0f * av2 * cd_rcp(av2 + 0.3f) + 0.5f);
            if (h > (uint32_t)CD_OV_H) h = CD_OV_H;
            if (col >= (uint32_t)ZV_W) col = ZV_W - 1;
            if (col != st->ovcol) { st->ovcol = col; st->ovRec[col] = 0u; }
            if (h > st->ovRec[col]) st->ovRec[col] = (unsigned char)h;
        }

        fxBuf[f]     = dryG * inL + wetG * st->gL * wet;   /* feedback stays mono */
        fxBuf[f + 8] = dryG * inR + wetG * st->gR * wet;

        wp = (wp + 1u) & CD_MASK;
        cyc++;
    }

    st->wp = wp;
    st->cyc = cyc;
    st->envF = envF;
    st->envS = envS;
    st->hold = hold;
    st->lim = lim;

    /* --- the screen: one piece of the frame per callback --- */
    {
        unsigned int *info = _Fx_DLY_Chopdly_Info;
        int visible = zv_visible(&st->z, (const uintptr_t *)ctx);
        zv_service(&st->z, info, visible);
        if (!visible) { st->step = 0u; return; }
        if (st->z.phase != ZV_IDLE) return;
        if (st->step == 0u) {         /* background, then the header values */
            uint32_t i, c, n = st->n, nd = n > 9u ? 2u : 1u;
            unsigned char *ln = st->ln;
            /* top half: blank but the line under the header (y 12 = block 1,
             * bit 4); the tags come in as text with the values. Bottom half
             * (dials, knob names) from build.py. */
            for (i = 0u; i < 512u; i++) {
                st->z.frame[i] = (unsigned char)((i >> 7) == 1u ? 0x10u : 0u);
                st->z.frame[512u + i] = CHOP_BG[i];
            }
            for (i = 0u; i < 64u; i++) ln[i] = CHOP_TAGS[i];
            /* "TRANS 12" or "12 CHOPS" (n <= 16) ending at CD_COL_CHOPS_END;
             * font index 1 + d is digit d */
            c = CD_COL_CHOPS_END - 6u - nd;
            if (st->trans) { for (i = 0u; i < 5u; i++) ln[c + i] = CHOP_W_TRANS[i]; c += 6u; }
            if (nd == 2u) ln[c++] = 2u;
            ln[c++] = (unsigned char)(1u + (nd == 2u ? n - 10u : n));
            if (!st->trans) for (i = 0u; i < 5u; i++) ln[c + 1u + i] = CHOP_W_CHOPS[i];
            for (i = 0u; i < CHOP_NAME_W; i++) {
                ln[CD_COL_ORDER + i] = CHOP_ORDER_N[(uint32_t)ordI * CHOP_NAME_W + i];
                ln[CD_COL_PITCH + i] = CHOP_PITCH_N[(uint32_t)pitI * CHOP_NAME_W + i];
                ln[CD_COL_REV + i]   = CHOP_REV_N[(uint32_t)revI * CHOP_NAME_W + i];
                ln[CD_COL_STR + i]   = CHOP_STR_N[(uint32_t)strI * CHOP_NAME_W + i];
            }
            for (i = 0u; i < 64u; i++) {  /* 3 columns a glyph, 5 rows: y 0 or 6 */
                uint32_t g = ln[i], x = 1u + 4u * (i & 31u), sh = (i >> 5) * 6u;
                if (!g) continue;
                for (c = 0u; c < 3u; c++) {
                    uint32_t v = (uint32_t)CHOP_FONT[3u * g + c] << sh;
                    st->z.frame[x + c] |= (unsigned char)v;
                    st->z.frame[ZV_W + x + c] |= (unsigned char)(v >> 8);
                }
            }
            st->step = 1u;
            return;
        }
        if (st->step < 5u) {          /* waveform, 32 columns a callback */
            uint32_t c, c0 = (st->step - 1u) * 32u;
            for (c = c0; c < c0 + 32u; c++) {
                int h = st->ovShow[c];
                if (h) cd_vspan(&st->z, (int)c, CD_OV_MID - h, CD_OV_MID + h, 0);
                else zv_px(&st->z, (int)c, CD_OV_MID, 1);
            }
            st->step++;
            return;
        }
        {                             /* cuts, the chop playing, playhead, cycle marker */
            uint32_t i, n = st->n, colk = st->colk, s = st->perm[st->k];
            int y;
            for (i = 1u; i < n; i++) {
                int x = (int)((st->b[i] * colk) >> 16);
                for (y = CD_OV_TOP; y <= 31; y += 2) zv_px(&st->z, x, y, 1);
            }
            {
                int x0 = (int)((st->b[s] * colk) >> 16), x1 = (int)((st->b[s + 1u] * colk) >> 16), x;
                for (x = x0; x < x1; x++) cd_vspan(&st->z, x, CD_OV_TOP, 31, 1);
                x = (int)((st->showOff * colk) >> 16);
                cd_vspan(&st->z, x, CD_OV_TOP, 31, 1);
            }
            {
                int x = (int)((st->cyc * colk) >> 16), xs = (int)((st->slotStart * colk) >> 16);
                int xe = (int)((st->slotEnd * colk) >> 16);
                for (y = xs; y < xe; y++) cd_vspan(&st->z, y, 33, 34, 0);   /* the slot playing */
                cd_vspan(&st->z, x, 32, 35, 1);                               /* now */
            }
#ifdef CHOPDLY_HOST
            { extern void cd_host_frame(const unsigned char *f); cd_host_frame(st->z.frame); }
#endif
            zv_submit(&st->z);
            st->step = 0u;
        }
    }
}
