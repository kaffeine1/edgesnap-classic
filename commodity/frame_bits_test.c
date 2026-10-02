/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * frame_bits_test.c - host tests for frame_bits.c: the strip is the
 * accent while it is up, nothing around it moves, dropping it gives
 * back what was there, and an outline inverted over it meanwhile is
 * still there afterwards.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frame_bits.h"

static int failures;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

static unsigned long seed = 12345;

static unsigned int rnd(void)
{
    seed = seed * 1103515245UL + 12345UL;
    return (unsigned int)((seed >> 16) & 0x7FFFu);
}

/* ------------------------------------------------------------ planar */

#define PW 64                         /* pixels per row      */
#define PH 12                         /* rows                */
#define PROW (PW / 8)                 /* bytes per plane row */

static unsigned int pixel(const ESFPlanes *bm, int x, int y)
{
    unsigned int v = 0;
    int k;

    for (k = 0; k < bm->depth; k++) {
        unsigned char byte = bm->plane[k][(long)y * bm->stride + (x >> 3)];

        if (byte & (0x80u >> (x & 7))) {
            v |= 1u << k;
        }
    }
    return v;
}

static void planar_case(int depth, int interleaved, int x, int y, int w,
                        int h, unsigned int pen)
{
    static unsigned char mem[PROW * PH * 8];
    static unsigned char was[PROW * PH * 8];
    static unsigned char outline[PROW * PH * 8];
    static unsigned char keep[PROW * PH * 8];
    ESFPlanes bm;
    unsigned long need = esf_planar_bytes(x, w, h, depth);
    int k, i, px, py, inside_ok = 1, outside_ok = 1;

    for (i = 0; i < (int)sizeof(mem); i++) {
        mem[i] = (unsigned char)rnd();
        outline[i] = (unsigned char)((rnd() & 3) == 0 ? rnd() : 0);
    }
    memcpy(was, mem, sizeof(mem));
    bm.depth = depth;
    for (k = 0; k < depth; k++) {
        bm.plane[k] = interleaved ? mem + k * PROW : mem + k * PROW * PH;
    }
    bm.stride = interleaved ? PROW * depth : PROW;

    CHECK(need <= sizeof(keep));
    esf_planar_show(&bm, x, y, w, h, pen, keep);
    for (py = 0; py < PH; py++) {
        for (px = 0; px < PW; px++) {
            int in = px >= x && px < x + w && py >= y && py < y + h;
            ESFPlanes old = bm;
            unsigned int now = pixel(&bm, px, py);
            unsigned int before;

            for (k = 0; k < depth; k++) {
                old.plane[k] = bm.plane[k] - mem + was;
            }
            before = pixel(&old, px, py);
            if (in && now != pen) {
                inside_ok = 0;
            }
            if (!in && now != before) {
                outside_ok = 0;
            }
        }
    }
    CHECK(inside_ok);
    CHECK(outside_ok);

    /* an outline inverted over everything while the strip is up */
    for (i = 0; i < (int)sizeof(mem); i++) {
        mem[i] ^= outline[i];
    }
    esf_planar_drop(&bm, x, y, w, h, keep);
    for (i = 0; i < (int)sizeof(mem); i++) {
        if (mem[i] != (unsigned char)(was[i] ^ outline[i])) {
            break;
        }
    }
    CHECK(i == (int)sizeof(mem));
}

static void test_planar(void)
{
    int depth, x, w;

    for (depth = 1; depth <= 8; depth++) {
        for (x = 0; x < 17; x += 3) {
            for (w = 1; w < 40; w += 5) {
                if (x + w > PW) {
                    continue;
                }
                planar_case(depth, 0, x, 2, w, 7, rnd() & ((1u << depth) - 1));
                planar_case(depth, 1, x, 1, w, 9, rnd() & ((1u << depth) - 1));
            }
        }
    }
    /* a strip one pixel wide on every bit of a byte, and a whole row */
    for (x = 0; x < 8; x++) {
        planar_case(2, 0, 8 + x, 0, 1, PH, 3);
    }
    planar_case(8, 1, 0, 0, PW, 1, 0xA5);
    CHECK(esf_planar_bytes(0, 0, 4, 2) == 0);
    CHECK(esf_planar_bytes(7, 2, 3, 2) == 2 * 3 * 2);    /* two bytes */
    CHECK(esf_planar_bytes(8, 8, 3, 2) == 1 * 3 * 2);    /* one byte  */
}

/* ------------------------------------------------------------ chunky */

#define CW 40
#define CH 10
#define CSTRIDE_PAD 3                       /* a row longer than its pixels */
#define CBYTES (CH * (CW * 4 + CSTRIDE_PAD) + 16)

static void chunky_case(int bpp, int x, int y, int w, int h)
{
    static unsigned char mem[CBYTES];
    static unsigned char was[CBYTES];
    static unsigned char outline[CBYTES];
    static unsigned char keep[CW * CH * 4];
    unsigned char accent[4];
    ESFChunky bm;
    int i, px, py, c, ok_in = 1, ok_out = 1;

    for (i = 0; i < 4; i++) {
        accent[i] = (unsigned char)rnd();
    }
    for (i = 0; i < (int)sizeof(mem); i++) {
        mem[i] = (unsigned char)rnd();
        outline[i] = (unsigned char)((rnd() & 1) ? 0xFF : 0x00);
    }
    memcpy(was, mem, sizeof(mem));
    bm.base = mem;
    bm.stride = CW * bpp + CSTRIDE_PAD;
    bm.bpp = bpp;
    CHECK(esf_chunky_bytes(w, h, bpp) <= sizeof(keep));
    esf_chunky_show(&bm, x, y, w, h, accent, keep);
    for (py = 0; py < CH; py++) {
        for (px = 0; px < CW; px++) {
            int in = px >= x && px < x + w && py >= y && py < y + h;

            for (c = 0; c < bpp; c++) {
                long at = (long)py * bm.stride + (long)px * bpp + c;

                if (in && mem[at] != accent[c]) {
                    ok_in = 0;
                }
                if (!in && mem[at] != was[at]) {
                    ok_out = 0;
                }
            }
        }
    }
    CHECK(ok_in);
    CHECK(ok_out);
    for (i = 0; i < (int)sizeof(mem); i++) {
        mem[i] ^= outline[i];
    }
    esf_chunky_drop(&bm, x, y, w, h, keep);
    for (i = 0; i < (int)sizeof(mem); i++) {
        if (mem[i] != (unsigned char)(was[i] ^ outline[i])) {
            break;
        }
    }
    CHECK(i == (int)sizeof(mem));
}

static void test_chunky(void)
{
    int bpp, x;

    for (bpp = 1; bpp <= 4; bpp++) {
        for (x = 0; x < 10; x += 3) {
            chunky_case(bpp, x, 1, 1, CH - 2);
            chunky_case(bpp, x, 0, CW - x, 3);
        }
    }
}

/* ------------------------------------------------------- the colours */

static void test_raw_colour(void)
{
    unsigned char o[4];

    /* the accent, 0x22 0x88 0xFF: r5 4, g5 17, g6 34, b5 31 */
    CHECK(esf_raw_colour(5, 0x22, 0x88, 0xFF, 0, o) == 2);       /* RGB16   */
    CHECK(o[0] == 0x24 && o[1] == 0x5F);
    CHECK(esf_raw_colour(7, 0x22, 0x88, 0xFF, 0, o) == 2);       /* RGB16PC */
    CHECK(o[0] == 0x5F && o[1] == 0x24);
    CHECK(esf_raw_colour(6, 0x22, 0x88, 0xFF, 0, o) == 2);       /* BGR16   */
    CHECK(o[0] == 0xFC && o[1] == 0x44);
    CHECK(esf_raw_colour(8, 0x22, 0x88, 0xFF, 0, o) == 2);       /* BGR16PC */
    CHECK(o[0] == 0x44 && o[1] == 0xFC);
    CHECK(esf_raw_colour(1, 0x22, 0x88, 0xFF, 0, o) == 2);       /* RGB15   */
    CHECK(o[0] == 0x12 && o[1] == 0x3F);
    CHECK(esf_raw_colour(3, 0x22, 0x88, 0xFF, 0, o) == 2);       /* RGB15PC */
    CHECK(o[0] == 0x3F && o[1] == 0x12);
    CHECK(esf_raw_colour(2, 0x22, 0x88, 0xFF, 0, o) == 2);       /* BGR15   */
    CHECK(o[0] == 0x7E && o[1] == 0x24);
    CHECK(esf_raw_colour(0, 0x22, 0x88, 0xFF, 52, o) == 1);      /* LUT8    */
    CHECK(o[0] == 52);
    CHECK(esf_raw_colour(9, 0x22, 0x88, 0xFF, 0, o) == 3);       /* RGB24   */
    CHECK(o[0] == 0x22 && o[1] == 0x88 && o[2] == 0xFF);
    CHECK(esf_raw_colour(10, 0x22, 0x88, 0xFF, 0, o) == 3);      /* BGR24   */
    CHECK(o[0] == 0xFF && o[1] == 0x88 && o[2] == 0x22);
    CHECK(esf_raw_colour(11, 0x22, 0x88, 0xFF, 0, o) == 4);      /* ARGB32  */
    CHECK(o[1] == 0x22 && o[2] == 0x88 && o[3] == 0xFF);
    CHECK(esf_raw_colour(12, 0x22, 0x88, 0xFF, 0, o) == 4);      /* BGRA32  */
    CHECK(o[0] == 0xFF && o[1] == 0x88 && o[2] == 0x22);
    CHECK(esf_raw_colour(13, 0x22, 0x88, 0xFF, 0, o) == 4);      /* RGBA32  */
    CHECK(o[0] == 0x22 && o[1] == 0x88 && o[2] == 0xFF);
    CHECK(esf_raw_colour(14, 0x22, 0x88, 0xFF, 0, o) == 0);      /* unknown */
    CHECK(esf_raw_colour(100, 0x22, 0x88, 0xFF, 0, o) == 0);
}

int main(void)
{
    test_planar();
    test_chunky();
    test_raw_colour();
    if (failures == 0) {
        printf("frame_bits_test: all tests passed\n");
        return 0;
    }
    printf("frame_bits_test: %d failure(s)\n", failures);
    return 1;
}
