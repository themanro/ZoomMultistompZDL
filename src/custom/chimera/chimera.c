/*
 * chimera.c -- Chimera: your guitar as the voice of a simulated throat and
 * mouth, MS-70CDR. Talkbox-like: the guitar takes the vocal folds' place.
 *
 * Named after, and built on, Chimera, the physical-modelling voice synth
 * (github.com/jamesondh/chimera-vocal-synth, MIT licence, copyright (c) 2026
 * Jameson Hodge; its docs/architecture.md section 1), not affiliated with it.
 * Its airway, cut down to fit: a staggered-grid transmission line in cgs
 * units, pressure at the nodes and volume velocity on the branches between
 * them, each branch's inertance built from the two half-sections either side
 * (stable at any area ratio), viscous loss on the walls, and a Flanagan
 * parallel R-L radiation load at the lips and the nostrils, the output being
 * the derivative of the radiated flow. The synth runs ~100 nodes at 96 kHz
 * with yielding walls, turbulence and moving-wall sources; here it is 16
 * sections of throat and mouth plus 6 of nose at the pedal's 44.1 kHz, the
 * guitar injected as the flow at the glottis.
 *
 * The synth builds its areas from a 2D head; the vowel shapes here are fitted
 * instead (design.py -> chimera_vowels.h): log-area curves whose first three
 * resonances sit on measured formants. The Vowel knob morphs between them.
 *
 * 9 knobs. Move is named steps (build/selector_metadata.py, same order); the
 * shared edit handler stores raw/100, read back as round(norm * 100).
 *   Vowel  (params[5])  the mouth's shape, u o a e i from 0 to 100
 *   Open   (params[6])  jaw and lips, closed (a hum with Nasal) to wide
 *   Body   (params[7])  the throat's length, 19 cm (big) to 13.5 cm (small)
 *   Nasal  (params[8])  opens the soft palate into the nose
 *   Talk   (params[9])  how far Move moves the mouth
 *   Move   (params[10]) Pick (the mouth follows each note), Slow, Fast
 *                       (sways), Chat (babbles syllables)
 *   Buzz   (params[11]) drives the guitar into a soft clipper first
 *   Breath (params[12]) air noise through the throat, following your playing
 *   Mix    (params[13]) dry/wet
 *
 * Screen (zvis, MS-70CDR 2.10 only; elsewhere the static cover stays): the
 * airway from the glottis (left) to the lips (right), as wide as it is, lit
 * inside by the sound in each section; the nose's opening above when Nasal
 * is up.
 *
 * Safe-DSP (docs/SAFE-DSP-RULES.md): no switch, no math lib, no divide
 * (reciprocals: the C674x estimate and Newton steps), no float->unsigned
 * cast, no writable statics (state in the ctx[3] arena), magic shuttle
 * preserved. Tables are const data.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "chimera_params.h"
#include "../common/zvis.h"
#include "chimera_art.h"
#include "chimera_vowels.h"

#ifndef CHIMERA_AUDIO_FUNC
#define CHIMERA_AUDIO_FUNC Fx_DLY_Chimera
#endif

#ifdef CHIMERA_HOST
typedef uintptr_t th_word;
#else
typedef unsigned int th_word;
#endif

#define TH_DO_PRAGMA(x) _Pragma(#x)
#define TH_EXPAND_PRAGMA(x) TH_DO_PRAGMA(x)
#define TH_CODE_SECTION(f) TH_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
#define TH_INLINE(f) TH_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(f))

TH_CODE_SECTION(CHIMERA_AUDIO_FUNC)

extern unsigned int _Fx_DLY_Chimera_Info[];     /* imageInfo: the linker's symbol */

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define TH_MAGIC   0x54485254u        /* 'THRT' */
#define TH_VERSION 2u                 /* 2: coefficient scratch in the state */
#define TH_FXID    418u
#define TH_FS      44100.0f

/* the airway, cgs */
#define TH_RHO     1.14e-3f           /* g/cm^3 */
#define TH_C       35000.0f           /* cm/s */
#define TH_RC2T    31.6627f           /* rho c^2 / fs */
#define TH_T       (1.0f / 44100.0f)
#define TH_VISC    0.03235f           /* sqrt(rho mu w / 2) x 2 sqrt(pi) / 2, w = 2 pi 500 */
#define TH_LOSS    2.5f               /* x the synth's viscous loss: stands in for its yielding walls */
#define TH_NT      TH_NSEC            /* throat and mouth sections, glottis 0 .. lips 15 */
#define TH_KV      8                  /* the soft palate sits over this section */
#define TH_NN      6                  /* nose sections, palate .. nostrils */
#define TH_NLEN    11.0f              /* cm */
#define TH_LMAX    19.0f              /* Body 0 */
#define TH_LMIN    13.5f              /* Body 100: still at CFL < 1 with 16 sections */
#define TH_RADMIN  0.4f               /* cm^2: see th_rad */
#define TH_OPEN    1.3f               /* Open's reach, in log area at the lips */
#define TH_GIN     60.0f              /* guitar -> glottal flow, cm^3/s */
#define TH_GOUT    1.6e-6f            /* radiated flow derivative -> output, before TH_VGAIN */

/* Output gain per vowel (u o a e i), from each shape's level with pink noise
 * in (a guitar's spectrum, roughly): evens out most of the 13 dB between a
 * closed 'u' and an open 'e' (exponent 0.85: 'u' stays a little softer). */
static const float TH_VGAIN[TH_NVOW] = { 0.659f, 0.286f, 0.217f, 0.185f, 0.190f };

/* the nose: areas palate -> nostrils (cm^2), a lossier passage */
static const float TH_NOSE_A[TH_NN] = { 1.2f, 2.4f, 3.2f, 2.6f, 1.6f, 0.9f };

/* screen */
#define TH_MID     24                 /* the airway's centre line */
#define TH_HMAX    10                 /* its half-height at the widest */
#define TH_X0      8                  /* glottis */
#define TH_SW      7                  /* pixels a section */
/* header cells (x = 1 + 4 * cell; line 1 = cell + 32); build.py's TAGS put
 * MOVE at line 1 cell 0 and VOWEL at line 1 cell 12. */
#define TH_CELL_MOVE  37u
#define TH_CELL_VOWEL 50u

typedef struct ThState {
    uint32_t magic, version;
    uint32_t rng;
    /* the airway */
    float p[TH_NT];                   /* pressure at each section */
    float u[TH_NT];                   /* flow between section k and k+1 (u[15] unused) */
    float pn[TH_NN], un[TH_NN];       /* the nose; un[5] unused */
    float uv;                         /* through the soft palate */
    float uL, uLn;                    /* radiation loads' inductor flows: lips, nostrils */
    float yPrev;                      /* last radiated flow, for its derivative */
    float lna[TH_NT];                 /* the shape now, log area (smoothed) */
    float av;                         /* palate opening now, cm^2 */
    float kN[TH_NT], kB[TH_NT], kD[TH_NT];   /* per-section steps: node; branch u' = kD u + kB dp */
    float kNn[TH_NN], kBn[TH_NN], kDn[TH_NN];
    float kBv, kDv, kR, kL, kRn, kLn, kI, kIn;   /* kI: 1 / (1 + kN (kR + kL)) at the lips / nostrils */
    uint32_t updHalf, nasLeft;
    /* the source and the mouth's motion */
    float env, pk;                    /* playing level, its slow peak */
    float nLp, dcX, dcY;              /* breath filter, output DC blocker */
    float mPh, mV, mVt, mO;           /* motion: phase, vowel now / target, opening */
    /* screen */
    uint32_t step;
    float lev[TH_NT];                 /* sound level per section, for the screen */
    float shA[TH_NT];                 /* the shape as drawn */
    float levSc;                      /* 1 / this frame's loudest section */
    float ca[TH_NT], cia[TH_NT], cia15[TH_NT];   /* scratch for the coefficients (kept off the stack) */
    unsigned char ln[64];
    ZvState z;
} ThState;

TH_INLINE(th_abs)
static inline float th_abs(float x) { return x < 0.0f ? -x : x; }

/* 1/x (x > 0): on the pedal the C674x estimate and two Newton steps; on a
 * computer a bit-trick seed and three. */
TH_INLINE(th_rcp)
static inline float th_rcp(float x)
{
#ifdef __TI_COMPILER_VERSION__
    float y = _rcpsp(x);
    y = y * (2.0f - x * y);
    return y * (2.0f - x * y);
#else
    union { float f; uint32_t u; } c;
    c.f = x;
    c.u = 0x7EF311C7u - c.u;
    float y = c.f;
    y = y * (2.0f - x * y);
    y = y * (2.0f - x * y);
    return y * (2.0f - x * y);
#endif
}

/* 1/sqrt(x) (x > 0) */
TH_INLINE(th_rsqrt)
static inline float th_rsqrt(float x)
{
#ifdef __TI_COMPILER_VERSION__
    float y = _rsqrsp(x);
#else
    union { float f; uint32_t u; } c;
    c.f = x;
    c.u = 0x5F3759DFu - (c.u >> 1);
    float y = c.f;
    y = y * (1.5f - 0.5f * x * y * y);
#endif
    y = y * (1.5f - 0.5f * x * y * y);
    return y * (1.5f - 0.5f * x * y * y);
}

/* e^x for x in -20..20 */
TH_INLINE(th_exp)
static inline float th_exp(float x)
{
    float t = x * 1.44269504f;
    int i = (int)t;
    if ((float)i > t) i--;
    float fr = (t - (float)i) * 0.69314718f;
    union { float f; uint32_t u; } c;
    c.u = (uint32_t)(i + 127) << 23;
    return c.f * (1.0f + fr * (1.0f + fr * (0.5f + fr * (0.16666667f + fr * (0.041666668f + fr * 0.0083333338f)))));
}

TH_INLINE(th_u)
static inline float th_u(uint32_t *r)
{
    *r = *r * 1664525u + 1013904223u;
    return (float)(int)(*r >> 8) * (1.0f / 16777216.0f);
}

/* soft clip, odd: x (27 + x^2) / (27 + 9 x^2), flat past +-3 */
TH_INLINE(th_sat)
static inline float th_sat(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) * th_rcp(27.0f + 9.0f * x2);
}

/* Screen drawing by bytes (a column span, a row run), not pixel by pixel. */
TH_INLINE(th_vspan)
static inline void th_vspan(ZvState *z, int x, int y0, int y1)
{
    int b;
    if ((unsigned)x >= (unsigned)ZV_W || y1 < y0) return;
    for (b = y0 >> 3; b <= y1 >> 3; ++b) {
        int lo = b == y0 >> 3 ? y0 & 7 : 0, hi = b == y1 >> 3 ? y1 & 7 : 7;
        z->frame[b * ZV_W + x] |= (unsigned char)((0xFFu << lo) & (0xFFu >> (7 - hi)));
    }
}
TH_INLINE(th_hrun)
static inline void th_hrun(ZvState *z, int x0, int x1, int y, int v)
{
    int x;
    unsigned char m = (unsigned char)(1u << (y & 7));
    unsigned char *row = &z->frame[(y >> 3) * ZV_W];
    for (x = x0; x <= x1; x++) row[x] = v ? (unsigned char)(row[x] | m) : (unsigned char)(row[x] & ~m);
}
/* half-height of a section on screen, ~ its radius */
TH_INLINE(th_hpx)
static inline int th_hpx(float a)
{
    int h = (int)(a * th_rsqrt(a) * 3.6f + 0.5f);
    return h < 1 ? 1 : h > TH_HMAX ? TH_HMAX : h;
}

/* A Flanagan radiation load for an opening of area a: 1/R and T/L. The
 * load's inductance and the last section's air form a resonance that the
 * 44.1 kHz step can only follow while dx sqrt(a) > ~0.33 (cm^1.5); the synth
 * steps at 96 kHz. So the load is computed for an opening of at least
 * TH_RADMIN: the lips can still close (the section itself narrows), only the
 * load stops shrinking with them. */
TH_INLINE(th_rad)
static inline void th_rad(float a, float *kR, float *kL)
{
    if (a < TH_RADMIN) a = TH_RADMIN;
    /* R = 128 rho c / (9 pi^2 a), L = 8 rho / (3 pi sqrt(pi a)) */
    *kR = a * (9.0f * 9.8696044f / (128.0f * TH_RHO * TH_C));
    *kL = TH_T * (3.0f * 3.14159265f * 1.77245385f / (8.0f * TH_RHO)) * th_rsqrt(th_rcp(a));
}

void CHIMERA_AUDIO_FUNC(th_word *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    th_word *magicSrc = ZDL_PTR(th_word *, ctx[12]);
    th_word *magicDst = ZDL_PTR(th_word *, *(th_word *)ZDL_PTR(th_word *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile th_word *desc = ZDL_PTR(volatile th_word *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u) return;
    if (base + sizeof(ThState) > end) return;

    ThState *st = (ThState *)base;
    int k;

    if (st->magic != TH_MAGIC || st->version != TH_VERSION) {
        st->magic = TH_MAGIC;
        st->version = TH_VERSION;
        st->rng = 0x6A09E667u;
        for (k = 0; k < TH_NT; k++) {
            st->p[k] = st->u[k] = 0.0f;
            st->lna[k] = TH_VOWEL_LNA[2][k];        /* start on 'a' */
            st->lev[k] = 0.0f; st->shA[k] = 1.0f;
            st->levSc = 1.0f;
            st->kN[k] = st->kB[k] = st->kD[k] = 0.0f;
        }
        for (k = 0; k < TH_NN; k++) st->pn[k] = st->un[k] = 0.0f;
        st->uv = st->uL = st->uLn = st->yPrev = 0.0f;
        st->av = 0.0f;
        st->updHalf = 0u; st->nasLeft = 0u;
        st->env = 0.0f; st->pk = 0.05f;
        st->nLp = st->dcX = st->dcY = 0.0f;
        st->mPh = 0.0f; st->mV = st->mVt = 2.0f; st->mO = 1.0f;
        st->step = 0u;
        zv_init(&st->z, TH_FXID, _Fx_DLY_Chimera_Info);
        st->kR = st->kL = st->kRn = st->kLn = 0.0f;
        st->kBv = st->kDv = 0.0f;
        st->kI = st->kIn = 1.0f;
        for (k = 0; k < TH_NN; k++) st->kNn[k] = st->kBn[k] = st->kDn[k] = 0.0f;
        st->updHalf = 4u;                            /* compute every coefficient this callback */
    }

    float vowel  = zoom_param_norm01(params[CHIMERA_VOWEL_SLOT],  CHIMERA_VOWEL_DEFAULT_NORM);
    float open   = zoom_param_norm01(params[CHIMERA_OPEN_SLOT],   CHIMERA_OPEN_DEFAULT_NORM);
    float body   = zoom_param_norm01(params[CHIMERA_BODY_SLOT],   CHIMERA_BODY_DEFAULT_NORM);
    float nasal  = zoom_param_norm01(params[CHIMERA_NASAL_SLOT],  CHIMERA_NASAL_DEFAULT_NORM);
    float talk   = zoom_param_norm01(params[CHIMERA_TALK_SLOT],   CHIMERA_TALK_DEFAULT_NORM);
    int   moveI  = (int)(zoom_param_norm01(params[CHIMERA_MOVE_SLOT], CHIMERA_MOVE_UI_DEFAULT * 0.01f) * 100.0f + 0.5f);
    float buzz   = zoom_param_norm01(params[CHIMERA_BUZZ_SLOT],   CHIMERA_BUZZ_DEFAULT_NORM);
    float breath = zoom_param_norm01(params[CHIMERA_BREATH_SLOT], CHIMERA_BREATH_DEFAULT_NORM);
    float mix    = zoom_param_norm01(params[CHIMERA_MIX_SLOT],    CHIMERA_MIX_DEFAULT_NORM);
    if (moveI > 3) moveI = 3;

    /* --- the mouth's motion: a vowel position (0..4) and an opening --- */
    float vPos = vowel * 4.0f, oPos = open;
    {
        float m = st->env * th_rcp(st->pk);           /* this note's level, 0..~1 */
        if (m > 1.0f) m = 1.0f;
        if (moveI == 0) {                             /* Pick: each note opens the mouth and moves it on */
            vPos += talk * 2.4f * m;
            oPos += talk * 0.6f * m;
        } else if (moveI < 3) {                       /* Slow / Fast: a sway, sine-ish */
            st->mPh += (moveI == 1 ? 0.35f : 2.2f) * (8.0f / TH_FS);
            if (st->mPh >= 1.0f) st->mPh -= 1.0f;
            float x = st->mPh * 4.0f - 1.0f;          /* triangle -1..1..-1, then rounded */
            if (x > 1.0f) x = 2.0f - x;
            x = x * (1.5f - 0.5f * x * x);
            vPos += talk * 2.0f * x;
        } else {                                      /* Chat: syllables, the mouth closing between */
            st->mPh += 4.5f * (8.0f / TH_FS);           /* ~4.5 syllables a second */
            if (st->mPh >= 1.0f) {
                st->mPh -= 1.0f;
                st->mVt = vPos + talk * 4.0f * (th_u(&st->rng) * 2.0f - 1.0f);
            }
            st->mV += (st->mVt - st->mV) * 0.03f;
            float s = st->mPh * (1.0f - st->mPh) * 4.0f;    /* 0 at the boundaries, 1 mid-syllable */
            vPos = st->mV;
            oPos = open * (1.0f - talk * (1.0f - s));
        }
        if (vPos < 0.0f) vPos = 0.0f;
        if (vPos > 3.999f) vPos = 3.999f;
        if (oPos < 0.0f) oPos = 0.0f;
        if (oPos > 1.0f) oPos = 1.0f;
    }

    /* --- the shape: morph the vowels, open or close the front, glide --- */
    {
        int vi = (int)vPos;
        float fr = vPos - (float)vi;
        float oL = (oPos - 0.55f) * (2.0f * TH_OPEN);
        for (k = 0; k < TH_NT; k++) {
            float w = (float)k * (1.0f / (float)(TH_NT - 1));
            float t = TH_VOWEL_LNA[vi][k] + (TH_VOWEL_LNA[vi + 1][k] - TH_VOWEL_LNA[vi][k]) * fr;
            t += oL * w * w * w;                      /* the jaw and lips: mostly the front */
            if (t < -3.0f) t = -3.0f;                 /* 0.05 cm^2: nearly closed */
            st->lna[k] += (t - st->lna[k]) * 0.08f;
        }
        float avT = nasal * nasal * 1.6f;
        st->av += (avT - st->av) * 0.08f;
    }

    /* --- per-section steps from the shape; half the airway a callback --- */
    {
        float len = TH_LMAX - (TH_LMAX - TH_LMIN) * body;
        float dx = len * (1.0f / (float)TH_NT), idx = th_rcp(dx);
        int k0 = 0, k1 = TH_NT;                   /* a quarter of the airway a callback */
        if (st->updHalf < 4u) { k0 = (int)st->updHalf * (TH_NT / 4); k1 = k0 + TH_NT / 4; }
        st->updHalf = st->updHalf >= 3u ? 0u : st->updHalf + 1u;
        float *a = st->ca, *ia = st->cia, *ia15 = st->cia15;   /* in the arena, not on the stack */
        int lo = k0 > 0 ? k0 - 1 : 0, hi = k1 < TH_NT ? k1 + 1 : TH_NT;
        for (k = lo; k < hi; k++) {
            a[k] = th_exp(st->lna[k]);
            ia[k] = th_rcp(a[k]);
            ia15[k] = ia[k] * th_rsqrt(a[k]);         /* a^-1.5 */
        }
        for (k = k0; k < k1; k++) {
            /* the palate's section joins three ways: a floor on its volume keeps
             * the step stable there (the synth's junction-node floor) */
            float iak = k == TH_KV && ia[k] > 1.25f ? 1.25f : ia[k];
            st->kN[k] = TH_RC2T * iak * idx;
            st->shA[k] = a[k];
            if (k < TH_NT - 1) {
                /* L = rho/2 (dx/a_k + dx/a_k+1); R = visc dx (a_k^-1.5 + a_k+1^-1.5) */
                float iL = th_rcp(0.5f * TH_RHO * dx * (ia[k] + ia[k + 1]));
                float r = TH_LOSS * TH_VISC * dx * (ia15[k] + ia15[k + 1]);
                float d = th_rcp(1.0f + TH_T * iL * r);
                st->kD[k] = d;
                st->kB[k] = TH_T * iL * d;            /* u' = kD u + kB (p_k - p_k+1) */
            }
        }
        if (k1 == TH_NT) {
            th_rad(a[TH_NT - 1], &st->kR, &st->kL);
            st->kI = th_rcp(1.0f + st->kN[TH_NT - 1] * (st->kR + st->kL));
        }
        if (k0 <= TH_KV && TH_KV < k1) {            /* the nose and the palate (needs section KV) */
            float dn = TH_NLEN * (1.0f / (float)TH_NN), idn = th_rcp(dn);
            for (k = 0; k < TH_NN; k++) {
                float an = TH_NOSE_A[k], ian = th_rcp(an);
                st->kNn[k] = TH_RC2T * ian * idn;
                if (k < TH_NN - 1) {
                    float ian1 = th_rcp(TH_NOSE_A[k + 1]);
                    float iL = th_rcp(0.5f * TH_RHO * dn * (ian + ian1));
                    float r = 3.0f * TH_LOSS * TH_VISC * dn * (ian * th_rsqrt(an) + ian1 * th_rsqrt(TH_NOSE_A[k + 1]));
                    float d = th_rcp(1.0f + TH_T * iL * r);
                    st->kDn[k] = d;
                    st->kBn[k] = TH_T * iL * d;
                }
            }
            th_rad(TH_NOSE_A[TH_NN - 1], &st->kRn, &st->kLn);
            st->kIn = th_rcp(1.0f + st->kNn[TH_NN - 1] * (st->kRn + st->kLn));
            if (st->av > 0.004f) {
                /* a 1.2 cm duct of area av, plus the half-sections it joins */
                float iav = th_rcp(st->av);
                float lv = TH_RHO * (1.2f * iav + 0.5f * dx * ia[TH_KV] + 0.5f * dn * th_rcp(TH_NOSE_A[0]));
                float iL = th_rcp(lv);
                float r = TH_LOSS * TH_VISC * 1.2f * iav * th_rsqrt(st->av) * 2.0f;
                float d = th_rcp(1.0f + TH_T * iL * r);
                st->kDv = d;
                st->kBv = TH_T * iL * d;
                st->nasLeft = 22050u;                 /* keep the nose running half a second after it shuts */
            } else {
                st->kBv = 0.0f;
            }
        }
    }

    /* --- the source, and the output gain for this shape --- */
    float outG;
    {
        int vi = (int)vPos;
        float fr2 = vPos - (float)vi;
        outG = (TH_VGAIN[vi] + (TH_VGAIN[vi + 1] - TH_VGAIN[vi]) * fr2) * (TH_GOUT * TH_FS);
    }
    float g = 1.0f + 24.0f * buzz * buzz;             /* drive into the clipper, x1 .. x25 */
    float gComp;                                       /* g^-0.875: quiet playing comes out near unity */
    {
        float s1 = g * th_rsqrt(g), s2 = s1 * th_rsqrt(s1), s3 = s2 * th_rsqrt(s2);   /* g^0.5, ^0.25, ^0.125 */
        gComp = th_rcp(g) * s3;
    }
    float nAmt = breath * breath * 0.6f;
    float wetG = mix, dryG = 1.0f - mix;
    int nose = st->kBv > 0.0f || st->nasLeft > 0u;
    if (st->kBv == 0.0f) st->uv = 0.0f;

    float env = st->env, pk = st->pk;
    float uL = st->uL, uLn = st->uLn, yPrev = st->yPrev, dcX = st->dcX, dcY = st->dcY, nLp = st->nLp;
    uint32_t rng = st->rng;
    /* The throat in locals for the callback: its sections are independent
     * within each half-step, and out of the arena (no aliasing) the compiler
     * can overlap them. */
    /* Only the state goes on the stack (the pedal's audio stack is small:
     * keep the frame near the effects proven on it, < 300 bytes); the
     * coefficients are read from the arena. */
    float P[TH_NT], U[TH_NT];
    const float *cN = st->kN, *cD = st->kD, *cE = st->kB;
    for (k = 0; k < TH_NT; k++) { P[k] = st->p[k]; U[k] = st->u[k]; }
    float kI = st->kI, kLl = st->kL, kRl = st->kR, cNl = cN[TH_NT - 1], cNv = cN[TH_KV];
    float PN[TH_NN], UN[TH_NN];
    const float *nN = st->kNn, *nD = st->kDn, *nE = st->kBn;
    float uv = st->uv, kDv = st->kDv, kBv = st->kBv, kIn = st->kIn, kLn = st->kLn, kRn = st->kRn;
    if (nose) for (k = 0; k < TH_NN; k++) { PN[k] = st->pn[k]; UN[k] = st->un[k]; }
    int fr;
    for (fr = 0; fr < 8; fr++) {
        float inL = fxBuf[fr], inR = fxBuf[fr + 8];
        float x = 0.5f * (inL + inR);
        float ax = th_abs(x);
        env += (ax - env) * (ax > env ? 0.15f : 0.0004f);

        float s = th_sat(g * x) * gComp;
        if (nAmt > 0.0f) {
            nLp += ((th_u(&rng) - 0.5f) - nLp) * 0.5f;
            s += nLp * nAmt * env * 2.2f;
        }
        float ug = s * TH_GIN;

        /* branches: L du/dt = p_a - p_b - R u, implicit in R */
#pragma UNROLL(15)
        for (k = 0; k < TH_NT - 1; k++) U[k] = cD[k] * U[k] + cE[k] * (P[k] - P[k + 1]);
        float urn = 0.0f;
        if (nose) {
            uv = kDv * uv + kBv * (P[TH_KV] - PN[0]);
#pragma UNROLL(5)
            for (k = 0; k < TH_NN - 1; k++) UN[k] = nD[k] * UN[k] + nE[k] * (PN[k] - PN[k + 1]);
            PN[0] += nN[0] * (uv - UN[0]);
#pragma UNROLL(4)
            for (k = 1; k < TH_NN - 1; k++) PN[k] += nN[k] * (UN[k - 1] - UN[k]);
            {   /* the nostrils, solved together with their load (see the lips) */
                float pv = (PN[TH_NN - 1] + nN[TH_NN - 1] * (UN[TH_NN - 2] - uLn)) * kIn;
                uLn += kLn * pv;
                urn = uLn + kRn * pv;
                PN[TH_NN - 1] = pv;
            }
        }
        /* nodes: dp/dt = rho c^2 / V (flow in - flow out) */
        P[0] += cN[0] * (ug - U[0]);
#pragma UNROLL(14)
        for (k = 1; k < TH_NT - 1; k++) P[k] += cN[k] * (U[k - 1] - U[k]);
        if (nose) P[TH_KV] -= cNv * uv;
        float ur;
        {   /* the lips, solved together with the radiation load (backward Euler
             * for the load: p' = p + kN (u_in - uL - kL p' - kR p')), which
             * keeps the last section stable whatever the step size */
            float pv = (P[TH_NT - 1] + cNl * (U[TH_NT - 2] - uL)) * kI;
            uL += kLl * pv;
            ur = uL + kRl * pv;
            P[TH_NT - 1] = pv;
        }

        float yr = ur + urn;
        float y = (yr - yPrev) * outG;
        yPrev = yr;
        dcY = y - dcX + 0.9993f * dcY;               /* DC out */
        dcX = y;
        y = th_sat(dcY);
        fxBuf[fr]     = dryG * inL + wetG * y;
        fxBuf[fr + 8] = dryG * inR + wetG * y;
    }
    for (k = 0; k < TH_NT; k++) { st->p[k] = P[k]; st->u[k] = U[k]; }
    if (nose) { for (k = 0; k < TH_NN; k++) { st->pn[k] = PN[k]; st->un[k] = UN[k]; } st->uv = uv; }
    if (env > pk) pk = env; else pk += (env - pk) * 0.0015f;
    if (pk < 0.02f) pk = 0.02f;
    st->env = env; st->pk = pk;
    st->uL = uL; st->uLn = uLn; st->yPrev = yPrev; st->dcX = dcX; st->dcY = dcY; st->nLp = nLp;
    st->rng = rng;
    if (st->nasLeft > 8u) st->nasLeft -= 8u; else st->nasLeft = 0u;
    if (!nose) { for (k = 0; k < TH_NN; k++) st->pn[k] = st->un[k] = 0.0f; uLn = 0.0f; st->uLn = 0.0f; }
    for (k = 0; k < TH_NT; k++) {                     /* the screen's levels: peak, falling */
        float a = th_abs(st->p[k]);
        st->lev[k] = a > st->lev[k] ? a : st->lev[k] * 0.97f;
    }

    /* --- the screen: one small piece of the frame per callback --- */
    {
        unsigned int *info = _Fx_DLY_Chimera_Info;
        int visible = zv_visible(&st->z, (const uintptr_t *)ctx);
        zv_service(&st->z, info, visible);
        if (!visible) { st->step = 0u; return; }
        if (st->z.phase != ZV_IDLE) return;
        if (st->step == 0u) {                         /* background */
            uint32_t i;
            for (i = 0u; i < 512u; i++) {
                st->z.frame[i] = (unsigned char)((i >> 7) == 1u ? 0x10u : 0u);
                st->z.frame[512u + i] = THR_BG[i];
            }
            st->step = 1u;
            return;
        }
        if (st->step == 1u) {                         /* the header values */
            uint32_t i, c;
            unsigned char *ln = st->ln;
            int vn = (int)(vPos + 0.5f);
            for (i = 0u; i < 64u; i++) ln[i] = THR_TAGS[i];
            for (i = 0u; i < THR_NAME_W; i++) ln[TH_CELL_MOVE + i] = THR_MOVE_N[(uint32_t)moveI * THR_NAME_W + i];
            ln[TH_CELL_VOWEL] = THR_VOWEL_N[vn];
            for (i = 0u; i < 64u; i++) {
                uint32_t gl = ln[i], x = 1u + 4u * (i & 31u), sh = (i >> 5) * 6u;
                if (!gl) continue;
                for (c = 0u; c < 3u; c++) {
                    uint32_t v = (uint32_t)THR_FONT[3u * gl + c] << sh;
                    st->z.frame[x + c] |= (unsigned char)v;
                    st->z.frame[ZV_W + x + c] |= (unsigned char)(v >> 8);
                }
            }
            st->step = 2u;
            return;
        }
        if (st->step == 2u || st->step == 3u) {       /* the airway's walls, half a callback */
            int k0 = st->step == 2u ? 0 : TH_NT / 2;
            int prev = k0 ? th_hpx(st->shA[k0 - 1]) : 0;
            for (k = k0; k < k0 + TH_NT / 2; k++) {
                int h = th_hpx(st->shA[k]);
                int x0 = TH_X0 + TH_SW * k, x1 = x0 + TH_SW - 1;
                if (k) {
                    int lo = h < prev ? h : prev, hi = h < prev ? prev : h;
                    th_vspan(&st->z, x0, TH_MID - hi, TH_MID - lo);
                    th_vspan(&st->z, x0, TH_MID + lo, TH_MID + hi);
                }
                th_hrun(&st->z, x0, x1, TH_MID - h, 1);
                th_hrun(&st->z, x0, x1, TH_MID + h, 1);
                prev = h;
            }
            if (k0 == 0) th_vspan(&st->z, TH_X0 - 1, TH_MID - 2, TH_MID + 2);   /* the glottis */
            st->step++;
            return;
        }
        if (st->step == 4u) {                         /* the sound inside: this frame's scale */
            float m = 0.001f;
            for (k = 0; k < TH_NT; k++) if (st->lev[k] > m) m = st->lev[k];
            st->levSc = th_rcp(m);
        }
        {                                             /* ... drawn in two halves */
            int k0 = st->step == 4u ? 0 : TH_NT / 2;
            for (k = k0; k < k0 + TH_NT / 2; k++) {
                int h = th_hpx(st->shA[k]);
                float lr = st->lev[k] * st->levSc;                /* 0..1, shown as its square root */
                int f = (int)((float)(h - 1) * lr * th_rsqrt(lr + 1e-6f) + 0.5f);
                int x0 = TH_X0 + TH_SW * k + 2;
                if (f > 0) {
                    th_vspan(&st->z, x0, TH_MID - f + 1, TH_MID + f - 1);
                    th_vspan(&st->z, x0 + 1, TH_MID - f + 1, TH_MID + f - 1);
                    th_vspan(&st->z, x0 + 2, TH_MID - f + 1, TH_MID + f - 1);
                }
            }
            if (st->step == 4u) { st->step = 5u; return; }
            if (st->av > 0.02f) {                     /* the palate open: a gap and a way up */
                int x0 = TH_X0 + TH_SW * TH_KV;
                int h = th_hpx(st->shA[TH_KV]);
                int w = 1 + (int)(st->av * 2.5f);
                th_hrun(&st->z, x0 + 1, x0 + 1 + w, TH_MID - h, 0);
                th_vspan(&st->z, x0, 13, TH_MID - h);            /* the way up to the nose */
                th_vspan(&st->z, x0 + 2 + w, 13, TH_MID - h);
            }
#ifdef CHIMERA_HOST
            { extern void th_host_frame(const unsigned char *f); th_host_frame(st->z.frame); }
#endif
            zv_submit(&st->z);
            st->step = 0u;
        }
    }
}
