/*
 * host.c -- run carillon.c on the desktop: WAV in, WAV out.
 *
 * Same C as the pedal build, fed through a fake ctx the way the firmware does
 * (8-frame planar blocks, params[5..13] normalized 0..1, a 64 KB arena).
 *
 *   cc -O2 -DCARILLON_HOST -DZV_HOST -I../../airwindows/common -I../common \
 *      -o /tmp/carillon_host host.c carillon.c
 *   CARILLON_FRAMES=dir carillon_host ...   also saves each screen frame
 *   CARILLON_SET=sec:knob:value carillon_host ...   turns one knob mid-run
 *   carillon_host in.wav out.wav [Cutoff Reso Decay Key Scale Slope Waves Noise Mix]
 *
 * Knob values are raw pedal values (0..100, or the step number for the named
 * knobs: Key 0..11, Scale 0..9);
 * omitted ones use the manifest default.
 * Reads 16-bit or float WAV, mono or stereo, 44.1 kHz assumed.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The header also defines the knob-name callbacks; carillon.c owns those. */
#define ZDL_GetLabel_3 host_label_3
#define ZDL_GetLabel_4 host_label_4
#include "carillon_params.h"

void Fx_DLY_Carillon(uintptr_t *ctx);

/* imageInfo stand-in: words 3/4 = 128 x 64 so the screen code draws. */
unsigned int _Fx_DLY_Carillon_Info[64] = { 0, 0, 0, 128, 64 };

/* Screen frames (CARILLON_FRAMES=dir): every finished frame as 1024 raw bytes. */
static const char *frame_dir;
static unsigned frame_n;
void cr_host_frame(const unsigned char *f)
{
    char path[512];
    FILE *o;
    if (!frame_dir) return;
    snprintf(path, sizeof path, "%s/f%05u.bin", frame_dir, frame_n++);
    if ((o = fopen(path, "wb"))) { fwrite(f, 1, 1024, o); fclose(o); }
}

#define ARENA 65536u

static int rd(FILE *f, void *p, size_t n) { return fread(p, 1, n, f) == n; }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s in.wav out.wav [9 knobs 0..100]\n", argv[0]); return 2; }
    FILE *fi = fopen(argv[1], "rb");
    if (!fi) { perror(argv[1]); return 1; }
    char id[4]; uint32_t sz;
    uint16_t fmt = 0, ch = 0, bits = 0; uint32_t rate = 0;
    float *L = NULL, *R = NULL; size_t frames = 0;
    if (!rd(fi, id, 4) || memcmp(id, "RIFF", 4) || !rd(fi, &sz, 4) || !rd(fi, id, 4)) { fprintf(stderr, "not a WAV\n"); return 1; }
    while (rd(fi, id, 4) && rd(fi, &sz, 4)) {
        if (!memcmp(id, "fmt ", 4)) {
            uint8_t b[64]; if (sz > sizeof b || !rd(fi, b, sz)) return 1;
            memcpy(&fmt, b, 2); memcpy(&ch, b + 2, 2); memcpy(&rate, b + 4, 4); memcpy(&bits, b + 14, 2);
            if (fmt == 0xFFFE) memcpy(&fmt, b + 24, 2);
        } else if (!memcmp(id, "data", 4)) {
            size_t bps = bits / 8u; frames = sz / (bps * ch);
            L = calloc(frames + 8, sizeof *L); R = calloc(frames + 8, sizeof *R);
            uint8_t *raw = malloc(sz); if (!rd(fi, raw, sz)) return 1;
            for (size_t i = 0; i < frames; i++) for (int c = 0; c < ch && c < 2; c++) {
                uint8_t *p = raw + (i * ch + c) * bps; float v;
                if (fmt == 3 && bits == 32) memcpy(&v, p, 4);
                else if (bits == 16) { int16_t s; memcpy(&s, p, 2); v = s / 32768.0f; }
                else if (bits == 24) { int32_t s = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8; v = s / 8388608.0f; }
                else { fprintf(stderr, "unsupported WAV: fmt %u, %u bits\n", fmt, bits); return 1; }
                (c ? R : L)[i] = v;
            }
            if (ch == 1) memcpy(R, L, frames * sizeof *L);
            free(raw);
            break;
        } else fseek(fi, sz + (sz & 1u), SEEK_CUR);
    }
    fclose(fi);
    if (!L) { fprintf(stderr, "no audio\n"); return 1; }
    if (rate != 44100) fprintf(stderr, "note: %u Hz input played as 44.1 kHz\n", rate);

    static const float defs[CARILLON_PARAM_COUNT] = {
        CARILLON_CUTOFF_UI_DEFAULT, CARILLON_RESO_UI_DEFAULT, CARILLON_DECAY_UI_DEFAULT,
        CARILLON_KEY_UI_DEFAULT, CARILLON_SCALE_UI_DEFAULT, CARILLON_SLOPE_UI_DEFAULT,
        CARILLON_WAVES_UI_DEFAULT, CARILLON_NOISE_UI_DEFAULT, CARILLON_MIX_UI_DEFAULT };
    float params[32] = {0};
    params[0] = 1.0f;
    for (int k = 0; k < CARILLON_PARAM_COUNT; k++)
        params[5 + k] = (argc > 3 + k ? (float)atof(argv[3 + k]) : defs[k]) * 0.01f;

    frame_dir = getenv("CARILLON_FRAMES");
    /* CARILLON_SET="sec:knob:value": turn knob (0 = Cutoff .. 8 = Mix) at sec,
     * e.g. "3:2:100" turns Decay to the top after 3 s */
    const char *se = getenv("CARILLON_SET");
    int set = 0, setK = 0; float setV = 0; size_t setAt = 0;
    if (se) { float sec; if (sscanf(se, "%f:%d:%f", &sec, &setK, &setV) == 3 && setK >= 0 && setK < 9) { set = 1; setAt = (size_t)(sec * 44100.0f); } }
    uint8_t *arena = aligned_alloc(64, ARENA);
    memset(arena, 0xA5, ARENA);       /* garbage, like the firmware's */
    uintptr_t desc[2] = { (uintptr_t)arena, (uintptr_t)arena + ARENA };
    float buf[16];
    uintptr_t magicSrc = 0x1234, magicDst = 0, magicPtr = (uintptr_t)&magicDst;
    uintptr_t ctx[16] = {0};
    ctx[1] = (uintptr_t)params; ctx[3] = (uintptr_t)desc; ctx[5] = (uintptr_t)buf;
    ctx[11] = (uintptr_t)&magicPtr; ctx[12] = (uintptr_t)&magicSrc;

    size_t tail = 44100u * 4u, total = frames + tail;
    int16_t *out = malloc(total * 2 * sizeof *out);
    float peak = 0;
    for (size_t i = 0; i < total; i += 8) {
        for (int f = 0; f < 8; f++) {
            size_t j = i + f;
            buf[f] = j < frames ? L[j] : 0.0f; buf[f + 8] = j < frames ? R[j] : 0.0f;
        }
        if (set && i >= setAt) { params[5 + setK] = setV * 0.01f; set = 0; }
        Fx_DLY_Carillon(ctx);
        for (int f = 0; f < 8 && i + f < total; f++) for (int c = 0; c < 2; c++) {
            float v = buf[f + 8 * c];
            if (v != v) { fprintf(stderr, "NaN at frame %zu\n", i + f); return 1; }
            if (v > peak) peak = v; if (-v > peak) peak = -v;
            if (v > 1) v = 1; if (v < -1) v = -1;
            out[(i + f) * 2 + c] = (int16_t)(v * 32767.0f);
        }
    }
    FILE *fo = fopen(argv[2], "wb");
    uint32_t dsz = (uint32_t)(total * 4), rsz = 36 + dsz, fsz = 16, sr = 44100, br = sr * 4;
    uint16_t pcm = 1, two = 2, ba = 4, b16 = 16;
    fwrite("RIFF", 1, 4, fo); fwrite(&rsz, 4, 1, fo); fwrite("WAVEfmt ", 1, 8, fo);
    fwrite(&fsz, 4, 1, fo); fwrite(&pcm, 2, 1, fo); fwrite(&two, 2, 1, fo); fwrite(&sr, 4, 1, fo);
    fwrite(&br, 4, 1, fo); fwrite(&ba, 2, 1, fo); fwrite(&b16, 2, 1, fo);
    fwrite("data", 1, 4, fo); fwrite(&dsz, 4, 1, fo); fwrite(out, 4, total, fo);
    fclose(fo);
    printf("%s: %.1f s, peak %.2f, magic %s\n", argv[2], total / 44100.0, peak, magicDst == 0x1234 ? "ok" : "MISSING");
    return 0;
}
