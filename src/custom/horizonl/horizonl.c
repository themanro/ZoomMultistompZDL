/*
 * horizonl.c -- HorizonL: Horizon with its tank at half rate, MS-70CDR.
 *
 * The same design as Horizon (src/custom/horizon), but the diffusers, the
 * eight lines and their damping run once per pair of samples (22.05 kHz):
 * half the work and a quarter of the memory traffic, so it fits a chain
 * where Horizon crackles. In: a [1 2 1]/4 filter, every other sample; out:
 * linear upsampling. The top of Hi is 10 kHz instead of 16.
 *
 * Pre-delay, then four allpass diffusers smear the attack, then an 8-line
 * feedback delay network: each line read with a slowly moving fractional
 * delay, damped by a low cut and a high cut, the eight mixed by a Hadamard
 * matrix (orthogonal, so the loop keeps its energy) and fed back with the
 * gain that gives the reverb time asked for. The lines run from tens to
 * hundreds of milliseconds, so the space goes from a room to far past a
 * cathedral, and turning Size bends the tail's pitch as the lines stretch.
 * At the top of Decay the loop holds forever and takes no new sound.
 *
 * 9 knobs, all 0..100:
 *   Size  (params[5])  the space: the delay lines x0.35 .. x1.52
 *   Decay (params[6])  0.8 s .. about a minute; 100 holds the tail
 *   Pre   (params[7])  pre-delay 0 .. 370 ms
 *   Mod   (params[8])  slow movement of the lines: chorus in the tail
 *   Lo    (params[9])  low cut inside the tail, 20 .. 600 Hz
 *   Hi    (params[10]) high cut inside the tail, 1.5 .. 10 kHz
 *   Swell (params[11]) each note blooms into the reverb over up to 2 s
 *   Width (params[12]) stereo width of the tail
 *   Mix   (params[13]) dry/wet
 *
 * Screen (zvis, MS-70CDR 2.10 only; elsewhere the static cover stays): the
 * eight delay lines as eight orbits around the horizon, each as wide as its
 * line is long, each dot lit by the sound in its line; the header shows the
 * reverb time.
 *
 * Memory: about 165 KB of the ctx[3] arena (8 x 4096-sample lines at the
 * tank rate, an 8192-sample pre-delay), cleared a piece per callback.
 *
 * Safe-DSP (docs/SAFE-DSP-RULES.md): no switch, no math lib, no divide
 * (reciprocals: the C674x estimate and Newton steps), no float->unsigned
 * cast, no writable statics (state in the ctx[3] arena), magic shuttle
 * preserved. Tables are const data.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "horizonl_params.h"
#include "../common/zvis.h"
#include "horizonl_art.h"

#ifndef HORIZON_AUDIO_FUNC
#define HORIZON_AUDIO_FUNC Fx_DLY_HorizonL
#endif

#ifdef HORIZON_HOST
typedef uintptr_t hz_word;
#else
typedef unsigned int hz_word;
#endif

#define HZ_DO_PRAGMA(x) _Pragma(#x)
#define HZ_EXPAND_PRAGMA(x) HZ_DO_PRAGMA(x)
#define HZ_CODE_SECTION(f) HZ_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
#define HZ_INLINE(f) HZ_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(f))

HZ_CODE_SECTION(HORIZON_AUDIO_FUNC)

extern unsigned int _Fx_DLY_HorizonL_Info[];     /* imageInfo: the linker's symbol */

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define HZ_MAGIC   0x485A4C31u        /* 'HZL1' */
#define HZ_VERSION 1u
#define HZ_FXID    420u
#define HZ_FS      44100.0f
#define HZ_TFS     22050.0f           /* the tank's rate: every other sample */
#define HZ_NL      8                  /* delay lines */
#define HZ_LLEN    4096u              /* each line's ring (at the tank's rate: 186 ms) */
#define HZ_LMASK   (HZ_LLEN - 1u)
#define HZ_PLEN    8192u              /* pre-delay ring (371 ms) */
#define HZ_PMASK   (HZ_PLEN - 1u)
#define HZ_ALEN    256u               /* each diffuser's ring */
#define HZ_AMASK   (HZ_ALEN - 1u)
#define HZ_CLEAR   2048u              /* words cleared per callback at start */
#define HZ_IN      0.35f              /* input into each line */
#define HZ_OUT     0.42f              /* tank -> output */
#define HZ_RTCOMP  1.45f              /* the loop's own losses (damping at its widest, interpolation): measured time = asked */

/* the lines at Size x1 (samples): 38 .. 121 ms, no common factors */
static const float HZ_BASE[HZ_NL] = { 835.0f, 1019.0f, 1218.0f, 1425.0f, 1680.0f, 1983.0f, 2301.0f, 2661.0f };   /* tank samples */
/* the diffusers: 142, 107, 379, 277 samples (written out in the loop) */
/* each line's movement, Hz at Mod 50 */
static const float HZ_LFO[HZ_NL] = { 0.11f, 0.17f, 0.13f, 0.23f, 0.19f, 0.29f, 0.07f, 0.31f };

/* screen */
#define HZ_CX     64
#define HZ_CY     24
#define HZ_CELL_DECAY 38u             /* header: the time's cells (line 1, cell 6) */

typedef struct HzState {
    uint32_t magic, version;
    uint32_t clearPos, ready;
    uint32_t wp, pwp, awp;            /* write positions: lines, pre-delay, diffusers */
    uint32_t rng, step;
    float dl[HZ_NL];                  /* each line's length now (smoothed), samples */
    float g[HZ_NL];                   /* each line's feedback gain */
    float gk[HZ_NL], fk[HZ_NL];       /* this callback's gains and fractional delays */
    uint32_t ok[HZ_NL];               /* ... and whole-sample delays */
    float lp[HZ_NL], hp[HZ_NL];       /* damping states */
    float ph[HZ_NL];                  /* movement phases, 0..1 */
    float preD, inG;                  /* pre-delay (smoothed), input gain (the hold fades it) */
    float envF, envS, sw;             /* swell: fast and slow level, the bloom 0..1 */
    float xa, oL, oR;                 /* the last input sample; the last tank outputs (for the upsampling) */
    float lev[HZ_NL], ang[HZ_NL];     /* screen: each line's level, its dot's angle */
    float rt;                         /* the reverb time, for the header */
    unsigned char ln[64];
    ZvState z;
    float ap[4][HZ_ALEN];
    float pre[HZ_PLEN];
    float line[HZ_NL][HZ_LLEN];
} HzState;

HZ_INLINE(hz_abs)
static inline float hz_abs(float x) { return x < 0.0f ? -x : x; }

/* 1/x (x > 0) */
HZ_INLINE(hz_rcp)
static inline float hz_rcp(float x)
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

/* 2^x for x in -40..40 */
HZ_INLINE(hz_exp2)
static inline float hz_exp2(float x)
{
    int i = (int)x;
    if ((float)i > x) i--;
    float fr = (x - (float)i) * 0.69314718f;
    union { float f; uint32_t u; } c;
    c.u = (uint32_t)(i + 127) << 23;
    return c.f * (1.0f + fr * (1.0f + fr * (0.5f + fr * (0.16666667f + fr * (0.041666668f + fr * 0.0083333338f)))));
}

/* n / 10 for 0 <= n < 4096, without a divide */
HZ_INLINE(hz_div10)
static inline int hz_div10(int n) { return (n * 6554) >> 16; }

/* Screen drawing by bytes: a row run and a small block. */
HZ_INLINE(hz_px)
static inline void hz_px(ZvState *z, int x, int y)
{
    if ((unsigned)x >= (unsigned)ZV_W || (unsigned)y >= (unsigned)ZV_H) return;
    z->frame[(y >> 3) * ZV_W + x] |= (unsigned char)(1u << (y & 7));
}
HZ_INLINE(hz_hrun)
static inline void hz_hrun(ZvState *z, int x0, int x1, int y)
{
    int x;
    if ((unsigned)y >= (unsigned)ZV_H) return;
    if (x0 < 0) x0 = 0;
    if (x1 > ZV_W - 1) x1 = ZV_W - 1;
    {
        unsigned char m = (unsigned char)(1u << (y & 7));
        unsigned char *row = &z->frame[(y >> 3) * ZV_W];
        for (x = x0; x <= x1; x++) row[x] |= m;
    }
}

/* soft clip for the wet signal: x (27 + x^2) / (27 + 9 x^2), flat past +-3;
 * below about 0.3 it is all but transparent */
HZ_INLINE(hz_sat)
static inline float hz_sat(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) * hz_rcp(27.0f + 9.0f * x2);
}

/* sin(2 pi p) for p in 0..1, parabolic with a correction */
HZ_INLINE(hz_sin1)
static inline float hz_sin1(float p)
{
    float x = p < 0.5f ? p * 4.0f - 1.0f : 3.0f - p * 4.0f;   /* -1..1..-1 triangle */
    float y = x * (2.0f - hz_abs(x));                          /* parabola */
    return -(y * (0.775f + 0.225f * hz_abs(y)));
}

void HORIZON_AUDIO_FUNC(hz_word *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    hz_word *magicSrc = ZDL_PTR(hz_word *, ctx[12]);
    hz_word *magicDst = ZDL_PTR(hz_word *, *(hz_word *)ZDL_PTR(hz_word *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile hz_word *desc = ZDL_PTR(volatile hz_word *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u) return;
    if (base + sizeof(HzState) > end) return;

    HzState *st = (HzState *)base;
    int k;

    if (st->magic != HZ_MAGIC || st->version != HZ_VERSION) {
        st->magic = HZ_MAGIC;
        st->version = HZ_VERSION;
        st->clearPos = 0u; st->ready = 0u;
        st->wp = st->pwp = st->awp = 0u;
        st->rng = 0x9E3779B9u; st->step = 0u;
        for (k = 0; k < HZ_NL; k++) {
            st->dl[k] = HZ_BASE[k]; st->lp[k] = st->hp[k] = 0.0f; st->g[k] = 0.5f;
            st->ph[k] = (float)k * 0.125f; st->lev[k] = 0.0f; st->ang[k] = (float)k * 0.785f;
        }
        st->preD = 0.0f; st->inG = 1.0f;
        st->envF = st->envS = 0.0f; st->sw = 1.0f;
        st->xa = st->oL = st->oR = 0.0f;
        st->rt = 1.0f;
        zv_init(&st->z, HZ_FXID, _Fx_DLY_HorizonL_Info);
        return;
    }
    if (!st->ready) {                                 /* clear the big buffers, a piece a callback */
        float *w = &st->ap[0][0];
        uint32_t total = (uint32_t)((sizeof(st->ap) + sizeof(st->pre) + sizeof(st->line)) >> 2);
        uint32_t i, e = st->clearPos + HZ_CLEAR;
        if (e > total) e = total;
        for (i = st->clearPos; i < e; i++) w[i] = 0.0f;
        st->clearPos = e;
        if (e >= total) st->ready = 1u;
        return;
    }

    float size  = zoom_param_norm01(params[HORIZON_SIZE_SLOT],  HORIZON_SIZE_DEFAULT_NORM);
    float decay = zoom_param_norm01(params[HORIZON_DECAY_SLOT], HORIZON_DECAY_DEFAULT_NORM);
    float pre   = zoom_param_norm01(params[HORIZON_PRE_SLOT],   HORIZON_PRE_DEFAULT_NORM);
    float mod   = zoom_param_norm01(params[HORIZON_MOD_SLOT],   HORIZON_MOD_DEFAULT_NORM);
    float lo    = zoom_param_norm01(params[HORIZON_LO_SLOT],    HORIZON_LO_DEFAULT_NORM);
    float hi    = zoom_param_norm01(params[HORIZON_HI_SLOT],    HORIZON_HI_DEFAULT_NORM);
    float swell = zoom_param_norm01(params[HORIZON_SWELL_SLOT], HORIZON_SWELL_DEFAULT_NORM);
    float width = zoom_param_norm01(params[HORIZON_WIDTH_SLOT], HORIZON_WIDTH_DEFAULT_NORM);
    float mix   = zoom_param_norm01(params[HORIZON_MIX_SLOT],   HORIZON_MIX_DEFAULT_NORM);
    int hold = decay > 0.995f;

    /* --- per callback: lengths, gains, damping, movement --- */
    float scale = 0.35f * hz_exp2(size * 2.1186f);            /* x0.35 .. x1.52 (the longest line, 183 ms, fits its 4096 samples) */
    float rt = 0.8f * hz_exp2(decay * 6.2288f);                /* 0.8 s .. 60 s */
    float irt = hz_rcp(rt * HZ_TFS * HZ_RTCOMP);
    float depth = mod * mod * 24.0f;                           /* tank samples */
    float rate = 0.3f + 1.4f * mod;
    /* the high cut: Horizon's curve (1.5 kHz up), stopped at 10 kHz (the tank runs at 22 kHz) */
    float fHi = 1500.0f * hz_exp2(hi * 3.415f);
    if (fHi > 10000.0f) fHi = 10000.0f;
    float aHi = hold ? 1.0f : 1.0f - hz_exp2(-(2.0f * 3.14159265f / HZ_TFS) * fHi * 1.442695f);
    /* the low cut: 20 .. 600 Hz; held, still 10 Hz, so no offset can build up in the loop */
    float aLo = hold ? 0.0028453f : 1.0f - hz_exp2(-(2.0f * 3.14159265f / HZ_TFS) * 20.0f * hz_exp2(lo * 4.907f) * 1.442695f);
    /* per-line values for the callback live in the arena: the pedal's audio
     * stack is small (keep the frame near the effects proven on it) */
    float *gain = st->gk, *frac = st->fk;
    uint32_t *off = st->ok;
    for (k = 0; k < HZ_NL; k++) {
        float tgt = HZ_BASE[k] * scale;
        st->dl[k] += (tgt - st->dl[k]) * 0.0015f;              /* ~120 ms: a tape-like stretch */
        st->ph[k] += HZ_LFO[k] * rate * (8.0f / HZ_FS);
        if (st->ph[k] >= 1.0f) st->ph[k] -= 1.0f;
        float d = st->dl[k] + depth * hz_sin1(st->ph[k]) + depth + 2.0f;
        int di = (int)d;
        frac[k] = d - (float)di;
        off[k] = (uint32_t)di;
        if (((st->wp >> 3) & 3u) == ((uint32_t)k & 3u) || hold)          /* each gain every 4th callback */
            st->g[k] = hold ? 1.0f : hz_exp2(-9.9658f * st->dl[k] * irt);   /* 10^(-3 L / (RT fs)) */
        gain[k] = st->g[k];
    }
    st->rt = rt;
    float preT = pre * 0.37f * HZ_TFS;
    st->preD += (preT - st->preD) * 0.01f;
    uint32_t preI = (uint32_t)(int)st->preD + 1u;
    float swK = swell > 0.01f ? hz_rcp(swell * 2.0f * HZ_TFS) : 1.0f;
    float inT = hold ? 0.0f : 1.0f;
    float wetG = mix, dryG = 1.0f - mix;
    float wS = width * 1.3f;

    uint32_t wp = st->wp, pwp = st->pwp, awp = st->awp;
    float envF = st->envF, envS = st->envS, sw = st->sw, inG = st->inG;
    float xa = st->xa, oL = st->oL, oR = st->oR;
    float lpS[HZ_NL], hpS[HZ_NL];                 /* the damping states, local in the loop (64 bytes of stack) */
    for (k = 0; k < HZ_NL; k++) { lpS[k] = st->lp[k]; hpS[k] = st->hp[k]; }
    int fr;
    /* The tank runs at half rate: one tick per pair of samples. In: a
     * [1 2 1] / 4 filter (zero at 22 kHz) then every other sample. Out: the
     * tick's value and its average with the last one, linear upsampling. */
    for (fr = 0; fr < 8; fr += 2) {
        float x0 = 0.5f * (fxBuf[fr] + fxBuf[fr + 8]), x1 = 0.5f * (fxBuf[fr + 1] + fxBuf[fr + 9]);
        float x = 0.25f * (xa + x1) + 0.5f * x0;
        xa = x1;

        /* swell: a note's attack (fast level jumping over slow) restarts the bloom */
        {
            float a = hz_abs(x);
            envF += (a - envF) * (a > envF ? 0.0975f : 0.003f);
            envS += (envF - envS) * 0.0006f;
            if (swell > 0.01f) {
                if (envF > 2.0f * envS + 0.01f && sw > 0.6f) sw = 0.0f;
                sw += (1.0f - sw) * swK;
            } else sw = 1.0f;
        }
        inG += (inT - inG) * 0.001f;

        /* pre-delay */
        st->pre[pwp] = x * inG * sw;
        x = st->pre[(pwp - preI) & HZ_PMASK];
        pwp = (pwp + 1u) & HZ_PMASK;

        /* four diffusers (serial: each feeds the next) */
        {
            float b0 = st->ap[0][(awp - 71u) & HZ_AMASK], b1 = st->ap[1][(awp - 53u) & HZ_AMASK];
            float b2 = st->ap[2][(awp - 189u) & HZ_AMASK], b3 = st->ap[3][(awp - 139u) & HZ_AMASK];
            float v0 = x + 0.62f * b0;  x = b0 - 0.62f * v0;  st->ap[0][awp] = v0;
            float v1 = x + 0.62f * b1;  x = b1 - 0.62f * v1;  st->ap[1][awp] = v1;
            float v2 = x + 0.62f * b2;  x = b2 - 0.62f * v2;  st->ap[2][awp] = v2;
            float v3 = x + 0.62f * b3;  x = b3 - 0.62f * v3;  st->ap[3][awp] = v3;
            awp = (awp + 1u) & HZ_AMASK;
        }

        /* the eight lines: read (moving, fractional), damp */
        float o[HZ_NL];
#pragma UNROLL(8)
        for (k = 0; k < HZ_NL; k++) {
            uint32_t i0 = (wp - off[k]) & HZ_LMASK;
            float a = st->line[k][i0], b = st->line[k][(i0 - 1u) & HZ_LMASK];
            float y = a + (b - a) * frac[k];
            lpS[k] += (y - lpS[k]) * aHi;
            hpS[k] += (lpS[k] - hpS[k]) * aLo;
            o[k] = lpS[k] - hpS[k];
        }
        /* Hadamard 8 (orthogonal: x 1/sqrt 8) */
        float a0 = o[0] + o[1], a1 = o[0] - o[1], a2 = o[2] + o[3], a3 = o[2] - o[3];
        float a4 = o[4] + o[5], a5 = o[4] - o[5], a6 = o[6] + o[7], a7 = o[6] - o[7];
        float b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
        float b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;
        float m[HZ_NL];
        m[0] = b0 + b4; m[1] = b1 + b5; m[2] = b2 + b6; m[3] = b3 + b7;
        m[4] = b0 - b4; m[5] = b1 - b5; m[6] = b2 - b6; m[7] = b3 - b7;
        float xi = x * HZ_IN;
#pragma UNROLL(8)
        for (k = 0; k < HZ_NL; k++)
            st->line[k][wp] = gain[k] * m[k] * 0.35355339f + ((k & 1) ? -xi : xi);
        wp = (wp + 1u) & HZ_LMASK;

        /* out: two orthogonal sums of the lines, then width */
        /* two orthogonal rows of the Hadamard: left and right uncorrelated */
        float yL = (o[0] + o[1] + o[2] + o[3] - o[4] - o[5] - o[6] - o[7]) * HZ_OUT;
        float yR = (o[0] - o[1] + o[2] - o[3] + o[4] - o[5] + o[6] - o[7]) * HZ_OUT;
        float mid = 0.5f * (yL + yR), side = 0.5f * (yL - yR) * wS;
        float wL = mid + side, wR = mid - side;
        fxBuf[fr]      = dryG * fxBuf[fr]      + wetG * hz_sat(0.5f * (oL + wL));
        fxBuf[fr + 8]  = dryG * fxBuf[fr + 8]  + wetG * hz_sat(0.5f * (oR + wR));
        fxBuf[fr + 1]  = dryG * fxBuf[fr + 1]  + wetG * hz_sat(wL);
        fxBuf[fr + 9]  = dryG * fxBuf[fr + 9]  + wetG * hz_sat(wR);
        oL = wL; oR = wR;
        if (fr == 6) for (k = 0; k < HZ_NL; k++) {      /* the screen: once a callback */
            float pk = hz_abs(o[k]);
            st->lev[k] = pk > st->lev[k] ? pk : st->lev[k] * 0.99f;
        }
    }
    st->wp = wp; st->pwp = pwp; st->awp = awp;
    st->envF = envF; st->envS = envS; st->sw = sw; st->inG = inG;
    st->xa = xa; st->oL = oL; st->oR = oR;
    for (k = 0; k < HZ_NL; k++) {
        float l = lpS[k], h = hpS[k];
        if (l < 1e-20f && l > -1e-20f) l = 0.0f;               /* no denormals in the tail */
        if (h < 1e-20f && h > -1e-20f) h = 0.0f;
        st->lp[k] = l; st->hp[k] = h;
        st->ang[k] += 0.03f * HZ_BASE[0] * hz_rcp(st->dl[k]) * (0.4f + rate);
        if (st->ang[k] > 6.2831853f) st->ang[k] -= 6.2831853f;
    }

    /* --- the screen: one small piece of the frame per callback --- */
    {
        unsigned int *info = _Fx_DLY_HorizonL_Info;
        int visible = zv_visible(&st->z, (const uintptr_t *)ctx);
        zv_service(&st->z, info, visible);
        if (!visible) { st->step = 0u; return; }
        if (st->z.phase != ZV_IDLE) return;
        if (st->step == 0u) {                         /* background */
            uint32_t i;
            for (i = 0u; i < 512u; i++) {
                st->z.frame[i] = (unsigned char)((i >> 7) == 1u ? 0x10u : 0u);
                st->z.frame[512u + i] = HZN_BG[i];
            }
            st->step = 1u;
            return;
        }
        if (st->step == 1u) {                         /* the header: the reverb time */
            uint32_t i, c;
            unsigned char *ln = st->ln;
            for (i = 0u; i < 64u; i++) ln[i] = HZN_TAGS[i];
            if (hold) {
                for (i = 0u; i < 4u; i++) ln[HZ_CELL_DECAY + i] = HZN_HOLD[i];
            } else {                                  /* "12.4S" / " 0.8S" / "60S" */
                int t10 = (int)(st->rt * 10.0f + 0.5f);
                int s = t10 >= 100 ? 0 : 1;           /* tenths below 10 s */
                int whole = s ? hz_div10(t10) : hz_div10(t10 + 5);
                int tenth = t10 - hz_div10(t10) * 10;
                uint32_t cpos = HZ_CELL_DECAY;
                if (whole >= 10) ln[cpos++] = (unsigned char)(1 + hz_div10(whole));
                ln[cpos++] = (unsigned char)(1 + whole - hz_div10(whole) * 10);
                if (s) { ln[cpos++] = HZN_DOT; ln[cpos++] = (unsigned char)(1 + tenth); }
                ln[cpos] = HZN_S;
            }
            for (i = 0u; i < 64u; i++) {
                uint32_t gl = ln[i], x = 1u + 4u * (i & 31u), sh = (i >> 5) * 6u;
                if (!gl) continue;
                for (c = 0u; c < 3u; c++) {
                    uint32_t v = (uint32_t)HZN_FONT[3u * gl + c] << sh;
                    st->z.frame[x + c] |= (unsigned char)v;
                    st->z.frame[ZV_W + x + c] |= (unsigned char)(v >> 8);
                }
            }
            st->step = 2u;
            return;
        }
        if (st->step == 2u || st->step == 3u) {       /* the orbits' paths, dotted: two a callback */
            float maxL = HZ_BASE[HZ_NL - 1] * 1.52f;
            int kp = st->step == 2u ? 0 : 4;
            for (k = kp; k < kp + 4; k += 2) {
                float rr = st->dl[k] * hz_rcp(maxL);
                float rx = 10.0f + rr * 50.0f, ry = 3.0f + rr * 7.5f;
                int i;
                for (i = 0; i < 20; i++) {
                    float p = (float)i * (1.0f / 20.0f);
                    float cs = hz_sin1(p + 0.25f >= 1.0f ? p - 0.75f : p + 0.25f), sn = hz_sin1(p);
                    hz_px(&st->z, HZ_CX + (int)(cs * rx), HZ_CY + (int)(sn * ry));
                }
            }
            st->step++;
            return;
        }
        {                                             /* the horizon, then the orbits in two halves */
            float maxL = HZ_BASE[HZ_NL - 1] * 1.52f, m = 0.0005f;
            int k0 = st->step == 4u ? 0 : HZ_NL / 2;
            for (k = 0; k < HZ_NL; k++) if (st->lev[k] > m) m = st->lev[k];
            float sc = hz_rcp(m);
            if (k0 == 0) {
                int r = 2 + (int)(size * 4.0f);       /* the horizon: a disc seen edge-on */
                hz_hrun(&st->z, HZ_CX - r + 1, HZ_CX + r - 1, HZ_CY - 1);
                hz_hrun(&st->z, HZ_CX - r - 3, HZ_CX + r + 3, HZ_CY);
                hz_hrun(&st->z, HZ_CX - r + 1, HZ_CX + r - 1, HZ_CY + 1);
            }
            for (k = k0; k < k0 + HZ_NL / 2; k++) {
                float rr = st->dl[k] * hz_rcp(maxL);              /* 0..1 */
                float rx = 10.0f + rr * 50.0f, ry = 3.0f + rr * 7.5f;
                float p = st->ang[k] * 0.15915494f;               /* turns */
                float cs = hz_sin1(p + 0.25f >= 1.0f ? p - 0.75f : p + 0.25f), sn = hz_sin1(p);
                int x = HZ_CX + (int)(cs * rx), y = HZ_CY + (int)(sn * ry);
                float lv = st->lev[k] * sc;
                int sz = lv > 0.66f ? 2 : lv > 0.25f ? 1 : 0;
                hz_hrun(&st->z, x - sz, x + sz, y);
                if (sz > 1) { hz_hrun(&st->z, x - 1, x + 1, y - 1); hz_hrun(&st->z, x - 1, x + 1, y + 1); }
                /* a trail behind it: where it was */
                {
                    float p2 = p - 0.04f; if (p2 < 0.0f) p2 += 1.0f;
                    float c2 = hz_sin1(p2 + 0.25f >= 1.0f ? p2 - 0.75f : p2 + 0.25f), s2 = hz_sin1(p2);
                    hz_px(&st->z, HZ_CX + (int)(c2 * rx), HZ_CY + (int)(s2 * ry));
                    p2 -= 0.04f; if (p2 < 0.0f) p2 += 1.0f;
                    c2 = hz_sin1(p2 + 0.25f >= 1.0f ? p2 - 0.75f : p2 + 0.25f); s2 = hz_sin1(p2);
                    if (lv > 0.25f) hz_px(&st->z, HZ_CX + (int)(c2 * rx), HZ_CY + (int)(s2 * ry));
                }
            }
            if (k0 == 0) { st->step = 5u; return; }
#ifdef HORIZON_HOST
            { extern void hz_host_frame(const unsigned char *f); hz_host_frame(st->z.frame); }
#endif
            zv_submit(&st->z);
            st->step = 0u;
        }
    }
}
