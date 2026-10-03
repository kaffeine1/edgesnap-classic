/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * frametest - the classic preview frame, without a drag, for a bench
 * that cannot see the screen. It finds the screen's memory the way the
 * commodity does (a planar bitmap's planes, or a graphics card's
 * linear memory through cybergraphics.library) and reports it, and on
 * request it puts the frame up, checks that every pixel of the strips
 * is the accent, takes it down and checks that every byte is back.
 *
 *   frametest INFO                        what the screen is; writes nothing
 *   frametest SHOW x y w h [ticks]        a frame around that rectangle of
 *                                         the default public screen, up for
 *                                         `ticks` fiftieths of a second
 *
 * The frame is commodity/frame_bits.c, the code the commodity uses.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/view.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <inline/macros.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frame_bits.h"

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *CyberGfxBase;

#define CGX_PIXFMT        0x80000004UL
#define CGX_ISCYBERGFX    0x80000008UL
#define CGX_ISLINEARMEM   0x80000009UL
#define LBMI_PIXFMT       0x84001004UL
#define LBMI_BYTESPERPIX  0x84001005UL
#define LBMI_BYTESPERROW  0x84001006UL
#define LBMI_BASEADDRESS  0x84001007UL
#define ft_GetCyberMapAttr(bm, attr) \
    LP2(0x60, ULONG, ft_GetCyberMapAttr, struct BitMap *, (bm), a0, \
        ULONG, (attr), d0, , CyberGfxBase)
#define ft_LockBitMapTagList(bm, tags) \
    LP2(0xa8, APTR, ft_LockBitMapTagList, struct BitMap *, (bm), a0, \
        struct TagItem *, (tags), a1, , CyberGfxBase)
#define ft_UnLockBitMap(handle) \
    LP1NR(0xae, ft_UnLockBitMap, APTR, (handle), a0, , CyberGfxBase)

static struct Screen *scr;
static struct BitMap *bm;
static int chunky;
static ULONG pixfmt;
static ESFPlanes planes;

static APTR lock_card(ESFChunky *out, ULONG *fmt_out)
{
    struct TagItem tags[5];
    ULONG base = 0, bpr = 0, fmt = 0, bpp = 0;
    APTR handle;

    tags[0].ti_Tag = LBMI_BASEADDRESS;  tags[0].ti_Data = (ULONG)&base;
    tags[1].ti_Tag = LBMI_BYTESPERROW;  tags[1].ti_Data = (ULONG)&bpr;
    tags[2].ti_Tag = LBMI_PIXFMT;       tags[2].ti_Data = (ULONG)&fmt;
    tags[3].ti_Tag = LBMI_BYTESPERPIX;  tags[3].ti_Data = (ULONG)&bpp;
    tags[4].ti_Tag = TAG_DONE;          tags[4].ti_Data = 0;
    handle = ft_LockBitMapTagList(bm, tags);
    out->base = (unsigned char *)base;
    out->stride = (long)bpr;
    out->bpp = (int)bpp;
    *fmt_out = fmt;
    return handle;
}

/* What the screen is. Returns 1 when the frame can go straight to its
 * memory. */
static int describe(int verbose)
{
    ULONG flags = GfxBase->LibNode.lib_Version >= 39 ?
                  GetBitMapAttr(bm, BMA_FLAGS) : BMF_STANDARD;
    int k;

    if (verbose) {
        printf("screen %dx%d, bitmap depth %d, rows %d, bytes per row %d, "
               "flags %08lx\n", (int)scr->Width, (int)scr->Height,
               (int)bm->Depth, (int)bm->Rows, (int)bm->BytesPerRow,
               (unsigned long)flags);
        printf("cybergraphics.library: %s\n",
               CyberGfxBase ? "open" : "not there");
    }
    if (CyberGfxBase != NULL && ft_GetCyberMapAttr(bm, CGX_ISCYBERGFX) != 0) {
        ESFChunky card;
        ULONG fmt = 0;
        APTR handle;

        chunky = 1;
        pixfmt = ft_GetCyberMapAttr(bm, CGX_PIXFMT);
        if (verbose) {
            printf("graphics card bitmap: pixel format %lu, linear %lu\n",
                   (unsigned long)pixfmt,
                   (unsigned long)ft_GetCyberMapAttr(bm, CGX_ISLINEARMEM));
        }
        Forbid();
        handle = lock_card(&card, &fmt);
        if (handle != NULL) {
            ft_UnLockBitMap(handle);
        }
        Permit();
        if (verbose) {
            printf("lock: %s, base %08lx, bytes per row %ld, pixel format "
                   "%lu, bytes per pixel %d\n", handle ? "ok" : "REFUSED",
                   (unsigned long)card.base, card.stride,
                   (unsigned long)fmt, card.bpp);
        }
        return handle != NULL && card.base != NULL &&
               card.stride >= (long)scr->Width * card.bpp && fmt == pixfmt;
    }
    if ((flags & BMF_STANDARD) == 0 || bm->Depth < 1 || bm->Depth > 8) {
        if (verbose) {
            printf("not a standard planar bitmap: no direct access\n");
        }
        return 0;
    }
    for (k = 0; k < bm->Depth; k++) {
        planes.plane[k] = (unsigned char *)bm->Planes[k];
    }
    planes.stride = bm->BytesPerRow;
    planes.depth = bm->Depth;
    if (verbose) {
        printf("planar bitmap: %d planes, interleaved %s\n", (int)bm->Depth,
               (flags & BMF_INTERLEAVED) ? "yes" : "no");
    }
    return 1;
}

/* The bytes of a planar strip, plane by plane and row by row, in the
 * order esf_planar_show() keeps them: to compare before and after. */
static void planar_copy(int x, int y, int w, int h, unsigned char *out)
{
    long first = (long)x >> 3, last = (long)(x + w - 1) >> 3;
    int k, row;

    for (k = 0; k < planes.depth; k++) {
        for (row = 0; row < h; row++) {
            memcpy(out, planes.plane[k] + (long)(y + row) * planes.stride + first,
                   (size_t)(last - first + 1));
            out += last - first + 1;
        }
    }
}

static unsigned int planar_pen(int x, int y)
{
    unsigned int v = 0;
    int k;

    for (k = 0; k < planes.depth; k++) {
        if (planes.plane[k][(long)y * planes.stride + (x >> 3)] &
            (0x80u >> (x & 7))) {
            v |= 1u << k;
        }
    }
    return v;
}

int main(int argc, char **argv)
{
    int rc = 0;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (IntuitionBase == NULL || GfxBase == NULL) {
        printf("frametest: needs AmigaOS 3.0\n");
        return 20;
    }
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 40);
    scr = LockPubScreen(NULL);
    if (scr == NULL) {
        printf("frametest: no public screen\n");
        return 20;
    }
    bm = scr->RastPort.BitMap;

    if (argc >= 2 && strcmp(argv[1], "INFO") == 0) {
        printf("direct access: %s\n", describe(1) ? "yes" : "no");
    } else if (argc >= 6 && strcmp(argv[1], "SHOW") == 0) {
        int x = atoi(argv[2]), y = atoi(argv[3]);
        int w = atoi(argv[4]), h = atoi(argv[5]);
        int ticks = argc >= 7 ? atoi(argv[6]) : 25;
        int t = 4, n, i, xx, yy, c, bad_up = 0, bad_back = 0;
        struct { int x, y, w, h; } st[4];
        LONG pen;
        ULONG rgb[3];
        unsigned char raw[4];
        int rawbpp = 0;
        unsigned long total = 0, at[4];
        unsigned char *keep, *before;
        ESFChunky card;
        ULONG fmt;
        APTR handle = NULL;

        if (!describe(0)) {
            printf("frametest: no direct access to this screen\n");
            UnlockPubScreen(NULL, scr);
            return 10;
        }
        if (scr->Height / 120 > t) {
            t = scr->Height / 120 > 8 ? 8 : scr->Height / 120;
        }
        st[0].x = x; st[0].y = y; st[0].w = w; st[0].h = t;
        st[1].x = x; st[1].y = y + h - t; st[1].w = w; st[1].h = t;
        st[2].x = x; st[2].y = y + t; st[2].w = t; st[2].h = h - 2 * t;
        st[3].x = x + w - t; st[3].y = y + t; st[3].w = t; st[3].h = h - 2 * t;
        n = 4;
        pen = ObtainBestPen(scr->ViewPort.ColorMap, 0x22222222UL,
                            0x88888888UL, 0xFFFFFFFFUL, OBP_Precision,
                            PRECISION_GUI, TAG_DONE);
        if (pen < 0) {
            pen = 3;
        }
        GetRGB32(scr->ViewPort.ColorMap, (ULONG)pen, 1, rgb);
        if (chunky) {
            rawbpp = esf_raw_colour(pixfmt, rgb[0] >> 24, rgb[1] >> 24,
                                    rgb[2] >> 24, (unsigned int)pen, raw);
            if (rawbpp == 0) {
                printf("frametest: pixel format %lu not known\n",
                       (unsigned long)pixfmt);
                rc = 10;
                goto done;
            }
        }
        for (i = 0; i < n; i++) {
            at[i] = total;
            total += chunky ? esf_chunky_bytes(st[i].w, st[i].h, rawbpp)
                            : esf_planar_bytes(st[i].x, st[i].w, st[i].h,
                                               planes.depth);
        }
        keep = (unsigned char *)AllocVec(total, MEMF_ANY);
        before = (unsigned char *)AllocVec(total, MEMF_ANY);
        if (keep == NULL || before == NULL) {
            printf("frametest: no memory\n");
            rc = 20;
            goto done;
        }
        printf("pen %ld = %02lx%02lx%02lx, %s", (long)pen,
               (unsigned long)(rgb[0] >> 24), (unsigned long)(rgb[1] >> 24),
               (unsigned long)(rgb[2] >> 24),
               chunky ? "raw" : "planar\n");
        if (chunky) {
            for (c = 0; c < rawbpp; c++) {
                printf(" %02x", raw[c]);
            }
            printf("\n");
        }

        /* up: show, then read every strip pixel back */
        Forbid();
        if (chunky) {
            handle = lock_card(&card, &fmt);
            if (handle == NULL || card.bpp != rawbpp) {
                if (handle != NULL) {
                    ft_UnLockBitMap(handle);
                }
                Permit();
                printf("frametest: lock refused\n");
                rc = 10;
                goto done;
            }
        } else {
            WaitBlit();
        }
        for (i = 0; i < n; i++) {
            /* a copy of what is there, to compare after the drop */
            if (!chunky) {
                planar_copy(st[i].x, st[i].y, st[i].w, st[i].h, before + at[i]);
            }
            for (yy = 0; yy < st[i].h; yy++) {
                if (chunky) {
                    memcpy(before + at[i] + (unsigned long)yy * st[i].w * rawbpp,
                           card.base + (long)(st[i].y + yy) * card.stride +
                           (long)st[i].x * rawbpp,
                           (size_t)(st[i].w * rawbpp));
                }
            }
            if (chunky) {
                esf_chunky_show(&card, st[i].x, st[i].y, st[i].w, st[i].h,
                                raw, keep + at[i]);
            } else {
                esf_planar_show(&planes, st[i].x, st[i].y, st[i].w, st[i].h,
                                (unsigned int)pen, keep + at[i]);
            }
        }
        for (i = 0; i < n; i++) {
            for (yy = 0; yy < st[i].h; yy++) {
                for (xx = 0; xx < st[i].w; xx++) {
                    if (chunky) {
                        unsigned char *p = card.base +
                            (long)(st[i].y + yy) * card.stride +
                            (long)(st[i].x + xx) * rawbpp;

                        for (c = 0; c < rawbpp; c++) {
                            if (p[c] != raw[c]) {
                                bad_up++;
                            }
                        }
                    } else if (planar_pen(st[i].x + xx, st[i].y + yy) !=
                               (unsigned int)pen) {
                        bad_up++;
                    }
                }
            }
        }
        if (chunky) {
            ft_UnLockBitMap(handle);
        }
        Permit();

        Delay(ticks > 0 ? ticks : 1);

        /* down: drop, then compare with the copy (chunky) */
        Forbid();
        if (chunky) {
            handle = lock_card(&card, &fmt);
        } else {
            WaitBlit();
        }
        for (i = 0; i < n; i++) {
            if (chunky) {
                if (handle != NULL) {
                    esf_chunky_drop(&card, st[i].x, st[i].y, st[i].w,
                                    st[i].h, keep + at[i]);
                }
            } else {
                esf_planar_drop(&planes, st[i].x, st[i].y, st[i].w, st[i].h,
                                keep + at[i]);
            }
        }
        if (!chunky) {
            /* same layout as the keep, so the same offsets */
            unsigned char *now = (unsigned char *)AllocVec(total, MEMF_ANY);

            if (now != NULL) {
                unsigned long b;

                for (i = 0; i < n; i++) {
                    planar_copy(st[i].x, st[i].y, st[i].w, st[i].h, now + at[i]);
                }
                for (b = 0; b < total; b++) {
                    if (now[b] != before[b]) {
                        bad_back++;
                    }
                }
                FreeVec(now);
            }
        }
        if (chunky && handle != NULL) {
            for (i = 0; i < n; i++) {
                for (yy = 0; yy < st[i].h; yy++) {
                    if (memcmp(before + at[i] + (unsigned long)yy * st[i].w * rawbpp,
                               card.base + (long)(st[i].y + yy) * card.stride +
                               (long)st[i].x * rawbpp,
                               (size_t)(st[i].w * rawbpp)) != 0) {
                        bad_back++;
                    }
                }
            }
            ft_UnLockBitMap(handle);
        }
        Permit();
        printf("frame %d,%d %dx%d, strips %d px: %d %s not the accent "
               "while up, %d %s not as before after the drop\n",
               x, y, w, h, t, bad_up, chunky ? "pixel bytes" : "pixels",
               bad_back, chunky ? "rows" : "plane bytes");
        if (bad_up || bad_back) {
            rc = 5;
        }
        FreeVec(keep);
        FreeVec(before);
done:
        ReleasePen(scr->ViewPort.ColorMap, (ULONG)pen);
    } else {
        printf("frametest INFO | SHOW x y w h [ticks]\n");
        rc = 5;
    }
    UnlockPubScreen(NULL, scr);
    if (CyberGfxBase != NULL) {
        CloseLibrary(CyberGfxBase);
    }
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
