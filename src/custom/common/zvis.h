/* zvis: live pictures on the effect's pedal screen (MS-70CDR SYSTEM 2.10, and
 * the G1on family: G1on / G1Xon 1.21 = B1on / B1Xon 1.30, one OS image).
 *
 * Firmware table (G1on support by Barsik-Barbosik, issue #12): each known OS is one entry of
 * zv_fws[] in .const (fingerprint words as address/value pairs, then its OS
 * addresses), read by one shared check loop and one shared screen test, so
 * another firmware costs a table entry, not code. zv_init copies the detected
 * OS's entry into the effect's own state (ZvState.fw, in the ctx[3] arena), so
 * the table is addressed once; the arena is lent out on patch switches, so
 * zv_visible re-checks the copy's fingerprint against the running OS every
 * callback before reading or calling anything from it. On the G1on "visible" = the selected effect byte
 * equals ctx[0] (works on a B1Xon running G1Xon 1.21 and running B1Xon 1.30).
 *
 * Used by Scope, Ring, Skull, Spectrum, Loop and the newer effects' animated
 * screens (Chimera, Horizon, HorizonL, Carillon, Droplets, Chopdly, Deform). The firmware re-decodes an effect's cover and
 * re-reads its imageInfo on every redraw, and the UI redraw can be woken from
 * the audio callback (our hardware tests: src/hardware_probes/coverrw, blink;
 * firmware facts and the visible-card predicate: Leemuzhko/ZOOM_development
 * SDK EFFECT_UI_WRITES.md and CARD_RENDERING.md -- credited, own code).
 *
 * A frame is drawn into `frame` (8 row blocks x 128 columns, one byte = 8
 * vertical pixels, bit 0 on top, 1 = black), then zv_submit() hands it to
 * zv_service(), which RLE-encodes ZV_ENC_STEP bytes per callback into the back
 * buffer, then -- if the effect's own main card is on screen -- points
 * imageInfo word 5 at it and wakes a redraw, at most every ZV_GAP callbacks.
 *
 * Rules kept by everything here: force-inlined (one .audio function: a call
 * into another section froze the pedal, see build/linker.py), no globals, no
 * RTS helpers, no jump tables. Firmware addresses are
 * read or called only after zv_fw_ok(); on any other firmware nothing is
 * published and the effect keeps its static cover. */
#ifndef ZVIS_H
#define ZVIS_H
#include <stdint.h>
#include "zoom_params.h"

#define ZV_INLINE(f)  ZOOM_ALWAYS_INLINE(f)
#define ZV_W          128
#define ZV_H          64
#define ZV_RLE_CAP    1544u                    /* 1536 worst case + padding */
#define ZV_ENC_STEP   32u                      /* raw bytes encoded per callback: 32 callbacks/frame */
#define ZV_GAP        184u                     /* callbacks between redraws: ~30 frames/s */
#define ZV_FW_MS70    1                        /* zv_fw_ok(): index into zv_fws[] + 1 */
#define ZV_FW_G1ON    2
#define ZV_NFW        2u
#define ZV_REDRAW     0x01u                    /* redraws the page and the cover */

/* One known OS. sig: fingerprint words (address, value), address 0 ends the
 * list; sig[0][0] is Event_post, so the function called is always one whose
 * code was checked. event: where the UI event object pointer is kept.
 * Screen test: the zero[] words must read 0 and `nonzero` must not (0 = unused);
 * page = selected page word (0..1, 0 = none); cursor = selected effect byte;
 * map = slot map, 24 B per page, 4 B per cursor position (0 = the cursor is the
 * slot); fxids = slot records, 44 B each, FXID in the low half of +4 (0 = not
 * checked); slots = chain slots. */
typedef struct {
    uint32_t sig[10][2];
    uint32_t event;
    uint32_t zero[3], nonzero, page, cursor, map, fxids, slots;
} ZvFw;

static const ZvFw zv_fws[ZV_NFW] = {
    /* MS-70CDR 2.10, from its Main.bin (firmware/extracted/main_os.dis):
     * Event_post's prologue, a caller posting B14[164] (the UI event), the B14
     * base and the UI task's wait mask. Event pointer = B14 (0xC00EF748) + 656.
     * Screen test = CARD_RENDERING's predicate. */
    { { { 0xC00CBF40u, 0x27773577u }, { 0xC00CBF44u, 0x85778677u }, { 0xC00CBF48u, 0xB64631F7u },
        { 0xC00CBF4Cu, 0x02E7442Bu }, { 0xC00CBF50u, 0x01E74228u }, { 0xC00BB168u, 0x1021BC13u },
        { 0xC00BB16Cu, 0x0200A46Du }, { 0xC00DD818u, 0x077BA42Au }, { 0xC00DD81Cu, 0x0760076Au },
        { 0xC00BADA8u, 0x033CFFA9u } },
      0xC00EF9D8u,
      { 0xC00EF888u, 0xC00EF9A8u, 0xC009D398u }, 0xC00EF894u, 0xC009D8F8u, 0xC009C064u,
      0xC00EE748u, 0xC009BF58u, 6u },
    /* G1on family (G1on / G1Xon 1.21, B1on / B1Xon 1.30, one OS image):
     * Event_post's prologue, the screen task's Event_pend and the effect-cursor
     * getter. Event pointer = B14 (0xC00DEA00) + 460 (screen task). */
    { { { 0xC00B05C0u, 0x27773577u }, { 0xC00B05C4u, 0x85778677u }, { 0xC00B05C8u, 0xB64631F7u },
        { 0xC00B05CCu, 0x02C53A2Bu }, { 0xC00B05D0u, 0x01C50628u }, { 0xC009B278u, 0x0200736Du },
        { 0xC009B280u, 0x10089013u }, { 0xC008B318u, 0x02C65A28u }, { 0xC008B320u, 0x02104CA0u },
        { 0u, 0u } },
      0xC00DEBCCu,
      { 0u, 0u, 0u }, 0u, 0u, 0xC0078CB4u, 0u, 0u, 5u },
};

enum { ZV_IDLE, ZV_ENCODE, ZV_SHOW };

typedef struct {
    unsigned phase, enc_pos, rle_n, rle_cur, rle_len, back, since, ok, fxid;
    ZvFw fw;                                    /* the detected OS's entry (zv_init) */
    unsigned char frame[1024];
    unsigned char buf[2][ZV_RLE_CAP];
} ZvState;

typedef void (*zv_post_fn)(void *, unsigned);
#define ZV_U32(a) (*(const volatile uint32_t *)(uintptr_t)(a))

ZV_INLINE(zv_in_ram)
static inline int zv_in_ram(unsigned a)
{
    return (a >= 0xC0000000u && a < 0xC8000000u) || (a >= 0x11800000u && a < 0x11840000u);
}

/* Do all fingerprint words (address, value; address 0 ends) match the running
 * OS? Addresses outside RAM or unaligned fail without being read, so a copy in
 * an overwritten arena can't make us read anywhere else. Pure reads. */
ZV_INLINE(zv_sig_ok)
static inline int zv_sig_ok(const uint32_t *sig)
{
    unsigned i;
    for (i = 0; i < 20u && sig[i]; i += 2u)
        if (!zv_in_ram(sig[i]) || (sig[i] & 3u) || ZV_U32(sig[i]) != sig[i + 1u]) return 0;
    return i > 0u;
}

/* Which known OS is this (ZV_FW_*, 0 = unknown)? Pure reads. */
ZV_INLINE(zv_fw_ok)
static inline int zv_fw_ok(void)
{
    unsigned k;
#ifdef ZV_HOST
    return 0;                                   /* host/emulator test: no pedal firmware */
#endif
    for (k = 0; k < ZV_NFW; ++k)
        if (zv_sig_ok(&zv_fws[k].sig[0][0])) return (int)k + 1;
    return 0;
}

/* The OS's UI event object, or 0 while it isn't a plausible pointer. */
ZV_INLINE(zv_event)
static inline unsigned zv_event(const ZvFw *t)
{
    unsigned ev;
    if (!zv_in_ram(t->event) || (t->event & 3u)) return 0u;
    ev = ZV_U32(t->event);
    return zv_in_ram(ev) && (ev & 3u) == 0u ? ev : 0u;
}

ZV_INLINE(zv_init)
static inline void zv_init(ZvState *z, unsigned fxid, const unsigned *info)
{
    unsigned i, f = info[3] == 128u && info[4] == 64u ? (unsigned)zv_fw_ok() : 0u;
    z->phase = ZV_IDLE; z->back = 0; z->since = ZV_GAP; z->fxid = fxid;
    if (f) for (i = 0; i < sizeof(ZvFw) / 4u; ++i) ((uint32_t *)&z->fw)[i] = ((const uint32_t *)&zv_fws[f - 1u])[i];
    z->ok = f && zv_event(&z->fw) ? 1u : 0u;
}

/* Is this instance's own main card on screen? (CARD_RENDERING's predicate:
 * main card, UI ready and unblocked, the selected mapped slot equals ctx[0]
 * and holds our FXID.) Reads only, and only on the qualified firmware. It
 * also vouches for z->fw: zv_service posts only in a callback where this said yes.
 * G1on: the selected effect byte equals ctx[0] (also true on its knob pages,
 * where the picture isn't shown: the redraws there cost only CPU). */
ZV_INLINE(zv_visible)
static inline int zv_visible(const ZvState *z, const uintptr_t *ctx)
{
    unsigned own = (unsigned)ctx[0], q, s, i;
    int c;
    const ZvFw *t = &z->fw;
#ifdef ZV_HOST
    (void)own; (void)q; (void)s; (void)i; (void)c; (void)z; (void)t;
    return 1;                                   /* host/emulator test: always draw */
#endif
    if (!z->ok || !zv_sig_ok(&t->sig[0][0]) || own >= t->slots) return 0;   /* the copy may be stale */
    for (i = 0; i < 3u; ++i)
        if (t->zero[i] && ZV_U32(t->zero[i]) != 0u) return 0;
    if (t->nonzero && ZV_U32(t->nonzero) == 0u) return 0;
    q = t->page ? ZV_U32(t->page) : 0u;
    c = (int)(*(const volatile signed char *)(uintptr_t)t->cursor);
    if (q > 1u || c < 0 || c >= (int)t->slots) return 0;
    s = t->map ? ZV_U32(t->map + 24u * q + 4u * (unsigned)c) : (unsigned)c;
    if (s != own) return 0;
    return !t->fxids || (ZV_U32(t->fxids + 44u * s + 4u) & 0xFFFFu) == z->fxid;
}

/* ------------------------------------------------------------ drawing */
ZV_INLINE(zv_copy)
static inline void zv_copy(ZvState *z, const unsigned char *bg)
{
    unsigned i;
    for (i = 0; i < 1024u; ++i) z->frame[i] = bg[i];
}

ZV_INLINE(zv_px)
static inline void zv_px(ZvState *z, int x, int y, int v)
{
    unsigned char *p;
    if ((unsigned)x >= (unsigned)ZV_W || (unsigned)y >= (unsigned)ZV_H) return;
    p = &z->frame[(unsigned)(y >> 3) * ZV_W + (unsigned)x];
    if (v) *p |= (unsigned char)(1u << (y & 7)); else *p &= (unsigned char)~(1u << (y & 7));
}

ZV_INLINE(zv_line)
static inline void zv_line(ZvState *z, int x0, int y0, int x1, int y1, int v)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, n = 0;
    for (;;) {
        int e2;
        zv_px(z, x0, y0, v);
        if ((x0 == x1 && y0 == y1) || ++n > 256) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

ZV_INLINE(zv_fill)
static inline void zv_fill(ZvState *z, int x0, int y0, int x1, int y1, int v)
{
    int x, y;
    for (y = y0; y <= y1; ++y) for (x = x0; x <= x1; ++x) zv_px(z, x, y, v);
}

/* Sprite: `w` x `h` bits, row-major, MSB first, each row padded to bytes.
 * Set bits are drawn with value v at (x, y); clear bits are left alone. */
ZV_INLINE(zv_blit)
static inline void zv_blit(ZvState *z, const unsigned char *bits, int w, int h, int x, int y, int v)
{
    int r, c, stride = (w + 7) >> 3;
    for (r = 0; r < h; ++r)
        for (c = 0; c < w; ++c)
            if (bits[r * stride + (c >> 3)] & (0x80u >> (c & 7))) zv_px(z, x + c, y + r, v);
}

/* ------------------------------------------------------------ publishing */
ZV_INLINE(zv_rle_flush)
static inline void zv_rle_flush(ZvState *z, unsigned char *out)
{
    if (z->rle_n == 1u) out[z->rle_len++] = (unsigned char)z->rle_cur;
    else if (z->rle_n > 1u) {
        out[z->rle_len++] = (unsigned char)z->rle_cur;
        out[z->rle_len++] = (unsigned char)z->rle_cur;
        out[z->rle_len++] = (unsigned char)(z->rle_n - 2u);
    }
    z->rle_n = 0;
}

ZV_INLINE(zv_submit)
static inline void zv_submit(ZvState *z)
{
    z->phase = ZV_ENCODE; z->enc_pos = 0; z->rle_n = 0; z->rle_len = 0;
}

/* Call once per callback. `visible` = zv_visible(). Returns 1 when a frame
 * was published (the effect may start drawing the next one). */
ZV_INLINE(zv_service)
static inline int zv_service(ZvState *z, unsigned *info, int visible)
{
    if (z->since < ZV_GAP) z->since++;
    if (z->phase == ZV_ENCODE) {
        /* Streaming Zoom RLE, byte-identical to build/screen_image.encode_zoom_rle. */
        unsigned char *out = z->buf[z->back];
        unsigned i, end = z->enc_pos + ZV_ENC_STEP;
        for (i = z->enc_pos; i < end; ++i) {
            unsigned b = z->frame[i];
            if (z->rle_n && b == z->rle_cur && z->rle_n < 257u) { z->rle_n++; continue; }
            zv_rle_flush(z, out);
            z->rle_cur = b; z->rle_n = 1;
        }
        z->enc_pos = end;
        if (end >= 1024u) {
            zv_rle_flush(z, out);
            while (z->rle_len & 3u) out[z->rle_len++] = 0;
            z->phase = ZV_SHOW;
        }
        return 0;
    }
    if (z->phase == ZV_SHOW && z->since >= ZV_GAP) {
        if (!visible) { z->phase = ZV_IDLE; return 0; }     /* screen changed: drop it */
        if (info[3] == 128u && info[4] == 64u) {
            info[5] = (unsigned)(uintptr_t)z->buf[z->back];
            z->back ^= 1u;
            unsigned ev = z->ok ? zv_event(&z->fw) : 0u;
            if (ev) ((zv_post_fn)z->fw.sig[0][0])((void *)ev, ZV_REDRAW);
            z->since = 0;
        }
        z->phase = ZV_IDLE;
        return 1;
    }
    return 0;
}

/* Full screen: stop the firmware drawing the knob boxes over the picture.
 * Each imageInfo coordinate block (words 9 + 4i) points at a knob-info record
 * {w, h, frames, ?, style, sprite} in the effect's own data; the stock box is
 * 20 x 15. Setting w = h = 0 leaves the firmware nothing to draw, while the
 * parameter count and the knob pages stay as they are. Only on the qualified
 * firmware, only if the record holds the expected 20 x 15. Experimental. */
ZV_INLINE(zv_hide_knob_boxes)
static inline void zv_hide_knob_boxes(const ZvState *z, const unsigned *info)
{
    unsigned i;
    if (!z->ok) return;
    for (i = 0; i < 3u; ++i) {
        unsigned a = info[9u + 4u * i + 3u];
        volatile unsigned *ki = (volatile unsigned *)(uintptr_t)a;
        if (!a || !(zv_in_ram(a) || (a >= 0x80000000u && a < 0x80100000u)) || (a & 3u)) continue;
        if (ki[0] == 20u && ki[1] == 15u) { ki[0] = 0u; ki[1] = 0u; }
    }
}

/* Fast-attack / slow-release level follower on |x|; coefficients per sample. */
ZV_INLINE(zv_follow)
static inline float zv_follow(float env, float x, float attack, float release)
{
    float a = x < 0.0f ? -x : x;
    if (!(a <= 4.0f)) a = 4.0f;
    return env + (a > env ? attack : release) * (a - env);
}

#endif
