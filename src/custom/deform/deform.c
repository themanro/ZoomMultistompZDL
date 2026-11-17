/*
 * deform.c -- Deform: drive, crush and compression in one, MS-70CDR.
 *
 * After the Torso S-4's Deform, in its order: noise that follows your
 * playing, then a two-band drive, then the crush, then a one-knob
 * compressor. The crush is built to stay soft: rounded steps instead of
 * hard ones, with the smallest signals given more of them, its error fed
 * back so the grit sits low instead of fizzing, and the sample-rate
 * reduction drawn as straight lines between the held samples with a filter
 * after, so no harsh mirror images.
 *
 * 9 knobs. Split and Mode are named steps (the pedal shows the names:
 * build/selector_metadata.py, which must match the order here); the shared
 * edit handler stores raw/100, read back as round(norm * 100).
 *   Drive (params[5])  0..+36 dB into a soft, slightly uneven clipper
 *   Crush (params[6])  how far the crush goes; 0 is off
 *   Comp  (params[7])  one-knob compressor: threshold and ratio together,
 *                      level made up
 *   Tilt  (params[8])  50 flat; above, highs pushed into the drive and lows
 *                      held back (and the other way below). Half of it is
 *                      undone after the drive, so it moves where the drive
 *                      bites more than the tone
 *   Split (params[9])  Low, Mid, High: Tilt's crossover, 80 Hz, 650 Hz,
 *                      2.5 kHz
 *   Mode  (params[10]) Bit (fewer levels), Redux (lower sample rate, with a
 *                      little jitter), Both
 *   Noise (params[11]) pink noise that follows your playing, before the
 *                      drive, so it's driven and crushed with the guitar
 *   Decay (params[12]) how long the noise hangs on after you stop,
 *                      20 ms .. 2 s
 *   Mix   (params[13]) dry/wet
 *
 * Screen (zvis, MS-70CDR 2.10 only; elsewhere the static cover stays): on
 * the left the drive and crush curve (what comes out for each input level),
 * then the compressor's gain reduction hanging from the top, then the wet
 * signal, one cycle caught at a rising zero crossing.
 *
 * Safe-DSP (docs/SAFE-DSP-RULES.md): no switch, no math lib, no divide (a
 * Newton reciprocal instead), no float->unsigned cast, no writable statics
 * (all state lives in the ctx[3] arena), magic shuttle preserved. Tables are
 * const data.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "deform_params.h"
#include "../common/zvis.h"
#include "deform_art.h"

#ifndef DEFORM_AUDIO_FUNC
#define DEFORM_AUDIO_FUNC Fx_DLY_Deform
#endif

#ifdef DEFORM_HOST
typedef uintptr_t df_word;
#else
typedef unsigned int df_word;
#endif

#define DF_DO_PRAGMA(x) _Pragma(#x)
#define DF_EXPAND_PRAGMA(x) DF_DO_PRAGMA(x)
#define DF_CODE_SECTION(f) DF_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
#define DF_INLINE(f) DF_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(f))

DF_CODE_SECTION(DEFORM_AUDIO_FUNC)

extern unsigned int _Fx_DLY_Deform_Info[];     /* imageInfo: the linker's symbol */

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define DF_MAGIC   0x44464F52u        /* 'DFOR' */
#define DF_VERSION 2u                 /* 2: the screen drawn over more callbacks */
#define DF_FXID    417u
#define DF_FS      44100.0f
#define DF_BIAS    0.1f               /* the clipper's unevenness: even harmonics */
#define DF_CREF    0.15f              /* the compressor holds this level */
#define DF_NOISE   0.6f               /* noise level at Noise 100, x the input's level */
/* level after the drive: 2^-(d (A + B d)), fitted on DI guitar */
#define DF_POST_A  5.5f
#define DF_POST_B  (-3.04f)

/* Tilt's crossover per Split step (Low, Mid, High: 80 Hz, 650 Hz, 2.5 kHz):
 * 1 - e^(-2 pi F / fs), a one-pole low band; the high band is the rest. */
static const float DF_SPLIT_C[3] = { 0.011333f, 0.08845f, 0.29966f };

/* screen: rows 14..34, centre 24 */
#define DF_MID     24
#define DF_AMP     10.0f
#define DF_CURVE_W 31                 /* curve: x 0..30 */
#define DF_GR_X    34                 /* gain reduction: x 34..35 */
#define DF_SC_X    38                 /* scope: x 38..127 */
#define DF_SC_N    90
/* header cells (x = 1 + 4 * cell; line 1 = cell + 32); build.py's TAGS put
 * SPLIT at line 1 cell 0 and MODE at line 1 cell 12. */
#define DF_CELL_SPLIT 38u
#define DF_CELL_MODE  49u

typedef struct {
    float tPre, tPost;                /* tilt crossovers: low band, before / after the drive */
    float dcX, dcY;                   /* DC blocker after the clipper */
    float h0, h1;                     /* redux: the last two held samples */
    float rLp;                        /* redux: filter after */
    float e;                          /* bit crush: the last error, fed back */
    float p0, p1, p2;                 /* pink noise */
} Chan;

typedef struct DefState {
    uint32_t magic, version;
    uint32_t rng;
    float    env;                     /* input level, for the noise */
    float    cEnv;                    /* compressor detector */
    float    gr;                      /* last gain, for the screen */
    float    rPh;                     /* redux clock, 0..1 */
    float    lim;                     /* wet limiter envelope */
    Chan     ch[2];

    /* screen */
    uint32_t step;
    uint32_t capState, capN, capWait;  /* scope: 0 armed, 1 catching, 2 caught */
    float    capPrev;
    float    cap[DF_SC_N];
    float    yv[DF_CURVE_W];          /* the curve, between its two drawing steps */
    float    yvSc;
    int      py;                      /* last point drawn, across steps */
    unsigned char ln[64];             /* header: font index per cell, 2 lines x 32 */
    ZvState  z;
} DefState;

DF_INLINE(df_abs)
static inline float df_abs(float x) { return x < 0.0f ? -x : x; }

/* 1/x without a divide (x > 0). On the pedal: the C674x's own estimate
 * (RCPSP, 8 bits) and two Newton steps, full precision. On a computer: a
 * bit-trick seed and three steps, the same result. */
DF_INLINE(df_rcp)
static inline float df_rcp(float x)
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
    y = y * (2.0f - x * y);
    return y;
#endif
}

/* The same, to about 16 bits (one step): for the per-sample curves (clipper,
 * squeeze, compressor gain), where that is far below anything audible. */
DF_INLINE(df_rcpf)
static inline float df_rcpf(float x)
{
#ifdef __TI_COMPILER_VERSION__
    float y = _rcpsp(x);
    return y * (2.0f - x * y);
#else
    return df_rcp(x);
#endif
}

/* 2^x for x in -40..40: exponent bits for the integer part, a series for the
 * fraction. */
DF_INLINE(df_exp2)
static inline float df_exp2(float x)
{
    int i = (int)x;
    if ((float)i > x) i--;
    float fr = (x - (float)i) * 0.69314718f;
    union { float f; uint32_t u; } c;
    c.u = (uint32_t)(i + 127) << 23;
    return c.f * (1.0f + fr * (1.0f + fr * (0.5f + fr * (0.16666667f + fr * (0.041666668f + fr * 0.0083333338f)))));
}

/* uniform 0..1 from an LCG step */
DF_INLINE(df_u)
static inline float df_u(uint32_t *r)
{
    *r = *r * 1664525u + 1013904223u;
    return (float)(int)(*r >> 8) * (1.0f / 16777216.0f);
}

/* Soft clip, odd and smooth: x (27 + x^2) / (27 + 9 x^2), flat at +-1 past
 * +-3 (the tanh-like curve that bends gradually). */
DF_INLINE(df_sat)
static inline float df_sat(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) * df_rcpf(27.0f + 9.0f * x2);
}

/* The drive: gain, then the clipper made slightly uneven by a bias (removed
 * again, so silence stays at 0). */
DF_INLINE(df_drive)
static inline float df_drive(float x, float g, float sb)
{
    return df_sat(g * x + DF_BIAS) - sb;
}

/* Bit crush, softly. The signal is squeezed first (small signals get more
 * of the levels, so quiet notes crumble instead of cutting out), then put on
 * the levels with rounded steps (n + 4 fr^3: flat on each level, a smooth
 * slope between, no corner), then expanded back. lv = levels per side. */
DF_INLINE(df_bits)
static inline float df_bits(float u, float lv, float lvInv)
{
    float v = u * lv + 0.5f;
    int n = (int)v;
    if ((float)n > v) n--;                       /* floor */
    float fr = v - (float)n - 0.5f;               /* -0.5 .. 0.5 */
    return ((float)n + 4.0f * fr * fr * fr) * lvInv;
}

/* squeeze: y = 5x / (1 + 4|x|), |x| 1 -> 1 */
DF_INLINE(df_sq)
static inline float df_sq(float x) { return 5.0f * x * df_rcpf(1.0f + 4.0f * df_abs(x)); }

/* and back: x = y / (5 - 4|y|), |y| held under 1.2 */
DF_INLINE(df_unsq)
static inline float df_unsq(float y)
{
    float a = df_abs(y);
    if (a > 1.2f) { y = y > 0.0f ? 1.2f : -1.2f; a = 1.2f; }
    return y * df_rcpf(5.0f - 4.0f * a);
}

void DEFORM_AUDIO_FUNC(df_word *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    df_word *magicSrc = ZDL_PTR(df_word *, ctx[12]);
    df_word *magicDst = ZDL_PTR(df_word *, *(df_word *)ZDL_PTR(df_word *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile df_word *desc = ZDL_PTR(volatile df_word *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u) return;
    if (base + sizeof(DefState) > end) return;

    DefState *st = (DefState *)base;
    uint32_t i;

    if (st->magic != DF_MAGIC || st->version != DF_VERSION) {
        st->magic = DF_MAGIC;
        st->version = DF_VERSION;
        st->rng = 0x2545F491u;
        st->env = st->cEnv = 0.0f;
        st->gr = 1.0f;
        st->rPh = 0.0f;
        st->lim = 0.0f;
        for (i = 0u; i < 2u; i++) {
            Chan *c = &st->ch[i];
            c->tPre = c->tPost = 0.0f;
            c->dcX = c->dcY = 0.0f;
            c->h0 = c->h1 = c->rLp = 0.0f;
            c->e = 0.0f;
            c->p0 = c->p1 = c->p2 = 0.0f;
        }
        st->step = 0u;
        st->capState = 0u; st->capN = 0u; st->capWait = 0u;
        st->capPrev = 0.0f;
        for (i = 0u; i < DF_SC_N; i++) st->cap[i] = 0.0f;
        zv_init(&st->z, DF_FXID, _Fx_DLY_Deform_Info);
    }

    /* named steps: raw/100 from the edit handler, back to the step number */
    int splI = (int)(zoom_param_norm01(params[DEFORM_SPLIT_SLOT], DEFORM_SPLIT_UI_DEFAULT * 0.01f) * 100.0f + 0.5f);
    int modI = (int)(zoom_param_norm01(params[DEFORM_MODE_SLOT],  DEFORM_MODE_UI_DEFAULT * 0.01f)  * 100.0f + 0.5f);
    if (splI > 2) splI = 2;
    if (modI > 2) modI = 2;
    float drive = zoom_param_norm01(params[DEFORM_DRIVE_SLOT], DEFORM_DRIVE_DEFAULT_NORM);
    float crush = zoom_param_norm01(params[DEFORM_CRUSH_SLOT], DEFORM_CRUSH_DEFAULT_NORM);
    float comp  = zoom_param_norm01(params[DEFORM_COMP_SLOT],  DEFORM_COMP_DEFAULT_NORM);
    float tilt  = zoom_param_norm01(params[DEFORM_TILT_SLOT],  DEFORM_TILT_DEFAULT_NORM);
    float noise = zoom_param_norm01(params[DEFORM_NOISE_SLOT], DEFORM_NOISE_DEFAULT_NORM);
    float decay = zoom_param_norm01(params[DEFORM_DECAY_SLOT], DEFORM_DECAY_DEFAULT_NORM);
    float mix   = zoom_param_norm01(params[DEFORM_MIX_SLOT],   DEFORM_MIX_DEFAULT_NORM);

    /* --- per-callback values --- */
    float t = 2.0f * tilt - 1.0f;
    float gHi = df_exp2(2.0f * t), gLo = df_exp2(-2.0f * t);   /* +-12 dB into the drive */
    float gHi2 = df_exp2(-t), gLo2 = df_exp2(t);                /* half of it undone after */
    float xo = DF_SPLIT_C[splI];

    float gD = df_exp2(6.0f * drive);                           /* 0 .. +36 dB */
    float sb = df_sat(DF_BIAS);
#ifdef DF_NOLEVEL
    float post = 1.0f;
#else
    /* level after the drive, measured on DI guitar so Drive changes the
     * sound more than the volume (a little louder at the top) */
    float post = df_exp2(-drive * (DF_POST_A + DF_POST_B * drive));
#endif

    int doBit = modI != 1 && crush > 0.004f;
    int doRdx = modI != 0 && crush > 0.004f;
    float bitsN = modI == 2 ? 8.5f - 5.0f * crush : 8.5f - 6.7f * crush;     /* bits a side, squeezed */
    float lv = df_exp2(bitsN - 1.0f), lvInv = df_rcp(lv);
    float rInc = df_exp2(modI == 2 ? -4.0f * crush : -5.0f * crush);          /* rate, x 44.1 kHz */
    float rK = 1.5708f * rInc;                                   /* filter after: a quarter of the rate */
    if (rK > 1.0f) rK = 1.0f;
    float jit = 0.3f * crush;

    float kc = 40.0f * comp * comp;
    float mk = 1.0f + kc * DF_CREF;
    float cAtk = 0.0076f, cRel = 0.00015f;                       /* 3 ms / 150 ms */

    float nAmt = noise * noise * DF_NOISE;
    float eRel = df_rcp(0.02f * df_exp2(6.64f * decay) * DF_FS); /* 20 ms .. 2 s */
    float eAtk = 0.0113f;                                        /* 2 ms */

    float wetG = mix, dryG = 1.0f - mix;

    uint32_t rng = st->rng;
    float env = st->env, cEnv = st->cEnv, rPh = st->rPh, lim = st->lim;
    float g = st->gr;
    int fr;
    for (fr = 0; fr < 8; fr++) {
        float inL = fxBuf[fr];
        float inR = fxBuf[fr + 8];
        float w[2];
        uint32_t c;

        /* the input's level, for the noise */
        {
            float a = df_abs(inL) > df_abs(inR) ? df_abs(inL) : df_abs(inR);
            env += (a - env) * (a > env ? eAtk : eRel);
        }
        /* the redux clock: a new sample when it wraps, a little early or late */
        int take = 0;
        if (doRdx) {
            rPh += rInc;
            if (rPh >= 1.0f) { rPh -= 1.0f + jit * (df_u(&rng) - 0.5f); take = 1; if (rPh < 0.0f) rPh = 0.0f; }
        }

        for (c = 0u; c < 2u; c++) {
            Chan *ch = &st->ch[c];
            float x = c ? inR : inL;

            /* noise that follows your playing (pink: three filtered whites) */
            if (nAmt > 0.0f) {
                float wn = df_u(&rng) - 0.5f;
                ch->p0 = 0.99765f * ch->p0 + wn * 0.0990460f;
                ch->p1 = 0.96300f * ch->p1 + wn * 0.2965164f;
                ch->p2 = 0.57000f * ch->p2 + wn * 1.0526913f;
                x += (ch->p0 + ch->p1 + ch->p2 + wn * 0.1848f) * 0.5f * nAmt * env;
            }

            /* tilt into the drive */
            ch->tPre += (x - ch->tPre) * xo;
            x = ch->tPre * gLo + (x - ch->tPre) * gHi;

            /* drive, then DC out */
            x = df_drive(x, gD, sb);
            ch->dcY = x - ch->dcX + 0.9995f * ch->dcY;
            ch->dcX = x;
            x = ch->dcY;

            /* half the tilt back */
            ch->tPost += (x - ch->tPost) * xo;
            x = (ch->tPost * gLo2 + (x - ch->tPost) * gHi2) * post;

            /* redux: straight lines between held samples, then a filter */
            if (doRdx) {
                if (take) { ch->h0 = ch->h1; ch->h1 = x; }
                float r = ch->h0 + (ch->h1 - ch->h0) * (rPh > 1.0f ? 1.0f : rPh);
                ch->rLp += (r - ch->rLp) * rK;
                x = ch->rLp;
            }
            /* bit crush: squeezed, rounded steps, error fed back (noise kept low) */
            if (doBit) {
                float u = df_sq(x) + 0.5f * ch->e;
                float q = df_bits(u, lv, lvInv);
                ch->e = q - u;
                x = df_unsq(q);
            }
            w[c] = x;
        }

        /* compressor: one detector for both sides, soft knee (1 / (1 + k env)) */
        {
            float a = df_abs(w[0]) > df_abs(w[1]) ? df_abs(w[0]) : df_abs(w[1]);
            cEnv += (a - cEnv) * (a > cEnv ? cAtk : cRel);
            g = mk * df_rcpf(1.0f + kc * cEnv);
        }
        float wL = w[0] * g, wR = w[1] * g;
        {
            float a = df_abs(wL) > df_abs(wR) ? df_abs(wL) : df_abs(wR);
            lim = a > lim ? a : lim * 0.9998f;
            if (lim > 0.9f) { float lg = 0.9f * df_rcp(lim); wL *= lg; wR *= lg; }
        }

        /* scope: catch one rising zero crossing, then 90 points every 2nd sample */
        if (st->capState == 0u) {
            if ((st->capPrev < 0.0f && wL >= 0.0f) || ++st->capWait > 4096u) {
                st->capState = 1u; st->capN = 0u; st->capWait = 0u;
            }
        }
        if (st->capState == 1u && (fr & 1) == 0) {
            st->cap[st->capN++] = wL;
            if (st->capN >= DF_SC_N) st->capState = 2u;
        }
        st->capPrev = wL;

        fxBuf[fr]     = dryG * inL + wetG * wL;
        fxBuf[fr + 8] = dryG * inR + wetG * wR;
    }
    st->rng = rng;
    st->env = env; st->cEnv = cEnv; st->rPh = rPh; st->lim = lim;
    st->gr = g * df_rcp(mk);

    /* --- the screen: one piece of the frame per callback --- */
    {
        unsigned int *info = _Fx_DLY_Deform_Info;
        int visible = zv_visible(&st->z, (const uintptr_t *)ctx);
        zv_service(&st->z, info, visible);
        if (!visible) { st->step = 0u; return; }
        if (st->z.phase != ZV_IDLE) return;
        if (st->step == 0u) {         /* background, then the header values */
            uint32_t c;
            unsigned char *ln = st->ln;
            for (i = 0u; i < 512u; i++) {
                st->z.frame[i] = (unsigned char)((i >> 7) == 1u ? 0x10u : 0u);
                st->z.frame[512u + i] = DEF_BG[i];
            }
            for (i = 0u; i < 64u; i++) ln[i] = DEF_TAGS[i];
            for (i = 0u; i < DEF_NAME_W; i++) {
                ln[DF_CELL_SPLIT + i] = DEF_SPLIT_N[(uint32_t)splI * DEF_NAME_W + i];
                ln[DF_CELL_MODE + i]  = DEF_MODE_N[(uint32_t)modI * DEF_NAME_W + i];
            }
            for (i = 0u; i < 64u; i++) {  /* 3 columns a glyph, 5 rows: y 0 or 6 */
                uint32_t gl = ln[i], x = 1u + 4u * (i & 31u), sh = (i >> 5) * 6u;
                if (!gl) continue;
                for (c = 0u; c < 3u; c++) {
                    uint32_t v = (uint32_t)DEF_FONT[3u * gl + c] << sh;
                    st->z.frame[x + c] |= (unsigned char)v;
                    st->z.frame[ZV_W + x + c] |= (unsigned char)(v >> 8);
                }
            }
            st->step = 1u;
            return;
        }
        /* The rest of the frame over four more callbacks, so no single one
         * carries the whole drawing (a callback that runs long is heard as a
         * click when the chain is near the DSP limit). */
        if (st->step == 1u || st->step == 2u) {   /* the curve: drive then crush, inputs -0.5 .. 0.5, in two halves */
            int k, k0 = st->step == 1u ? 0 : DF_CURVE_W / 2, k1 = st->step == 1u ? DF_CURVE_W / 2 : DF_CURVE_W;
            for (k = k0; k < k1; k++) {
                float x = (float)(k - DF_CURVE_W / 2) * (0.5f / (float)(DF_CURVE_W / 2));
                x = df_drive(x, gD, sb);
                if (doBit) x = df_unsq(df_bits(df_sq(x * post), lv, lvInv));
                st->yv[k] = x;
            }
            if (st->step == 1u) { st->step = 2u; return; }
            {
                float m = 0.0001f;
                for (k = 0; k < DF_CURVE_W; k++) if (df_abs(st->yv[k]) > m) m = df_abs(st->yv[k]);
                st->yvSc = DF_AMP * df_rcp(m);
            }
            st->step = 5u;
            return;
        }
        if (st->step == 5u) {         /* draw it, and the gain reduction (0 .. 20 dB, from the top) */
            int k, py = 0;
            float sc = st->yvSc;
            for (k = 0; k < DF_CURVE_W; k++) {
                float yk = st->yv[k];
                int y = DF_MID - (int)(yk * sc + (yk >= 0.0f ? 0.5f : -0.5f));
                if (k) zv_line(&st->z, k - 1, py, k, y, 1); else zv_px(&st->z, k, y, 1);
                py = y;
            }
            {
                float gr = st->gr, d = 0.0f;
                while (gr < 0.891f && d < 20.0f) { gr *= 1.122f; d += 1.0f; }   /* 1 dB steps, no log */
                if (d > 0.0f) zv_fill(&st->z, DF_GR_X, 14, DF_GR_X + 1, 14 + (int)d, 1);
            }
            st->step = 6u;
            return;
        }
        {                             /* the wet signal: one caught cycle, scaled to fit, in three parts */
            int part = (int)st->step - 6, k, k0 = part * (DF_SC_N / 3), k1 = k0 + DF_SC_N / 3;
            float pk = 0.02f;
            for (k = 0; k < DF_SC_N; k++) if (df_abs(st->cap[k]) > pk) pk = df_abs(st->cap[k]);
            float s2 = DF_AMP * df_rcp(pk);
            int py = st->py;
            for (k = k0; k < k1; k++) {
                float v = st->cap[k];
                int y = DF_MID - (int)(v * s2 + (v >= 0.0f ? 0.5f : -0.5f));
                if (k) zv_line(&st->z, DF_SC_X + k - 1, py, DF_SC_X + k, y, 1);
                py = y;
            }
            st->py = py;
            if (part < 2) { st->step++; return; }
            if (st->capState == 2u) st->capState = 0u;     /* catch the next one */
#ifdef DEFORM_HOST
            { extern void df_host_frame(const unsigned char *f); df_host_frame(st->z.frame); }
#endif
            zv_submit(&st->z);
            st->step = 0u;
        }
    }
}
