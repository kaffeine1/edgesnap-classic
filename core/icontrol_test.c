/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * icontrol_test.c - host tests for core/icontrol.c, against the files
 * two real systems wrote.
 */

#include <stdio.h>
#include <string.h>

#include "icontrol.h"

static int failures;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

/* What the IControl editor of AmigaOS 3.2.3 saved on the bench with
 * "Can Move Off-screen" ticked, and what an AmiKit on real hardware
 * had in ENVARC:, which was the same file (2026-09-27 and 28). */
static const unsigned char saved_on[72] = {
    0x46, 0x4F, 0x52, 0x4D, 0x00, 0x00, 0x00, 0x40,
    0x50, 0x52, 0x45, 0x46, 0x50, 0x52, 0x48, 0x44,
    0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x49, 0x43, 0x54, 0x4C, 0x00, 0x00,
    0x00, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x32, 0x00, 0x40, 0x80, 0x00,
    0x80, 0x1E, 0x4E, 0x4D, 0x56, 0x42, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void test_the_real_file(void)
{
    unsigned char buf[72];

    memcpy(buf, saved_on, sizeof(buf));
    CHECK(es_ictl_flags_at(buf, 72) == 0x36);
    CHECK(es_ictl_offscreen(buf, 72) == 1);

    /* already on: nothing changes */
    CHECK(es_ictl_set_offscreen(buf, 72, 1) == 0);
    CHECK(memcmp(buf, saved_on, 72) == 0);

    /* off: one byte differs, and it is the one the editor changes
     * (8000801E became 8000001E on the bench) */
    CHECK(es_ictl_set_offscreen(buf, 72, 0) == 1);
    CHECK(es_ictl_offscreen(buf, 72) == 0);
    CHECK(buf[0x36] == 0x80 && buf[0x37] == 0x00 &&
          buf[0x38] == 0x00 && buf[0x39] == 0x1E);
    buf[0x38] = 0x80;
    CHECK(memcmp(buf, saved_on, 72) == 0);
}

static void test_the_default_file(void)
{
    unsigned char buf[ES_ICTL_DEFAULT_LEN];

    CHECK(es_ictl_default(buf, 1) == 72);
    CHECK(memcmp(buf, saved_on, 72) == 0);   /* what the editor writes */
    CHECK(es_ictl_default(buf, 0) == 72);
    CHECK(es_ictl_offscreen(buf, 72) == 0);
    CHECK(buf[0x38] == 0x00);
}

static void test_not_a_file(void)
{
    unsigned char buf[72];

    memcpy(buf, saved_on, sizeof(buf));
    CHECK(es_ictl_flags_at(buf, 11) == -1);          /* too short       */
    CHECK(es_ictl_flags_at(buf, 0x30) == -1);        /* cut in the chunk */
    CHECK(es_ictl_flags_at(0, 72) == -1);
    buf[0] = 'X';
    CHECK(es_ictl_offscreen(buf, 72) == -1);
    CHECK(es_ictl_set_offscreen(buf, 72, 1) == -1);
    buf[0] = 'F';
    buf[8] = 'I';                                    /* FORM ILBM, say  */
    CHECK(es_ictl_flags_at(buf, 72) == -1);
    memcpy(buf, saved_on, sizeof(buf));
    buf[0x1A] = 'X';                                 /* no ICTL chunk   */
    CHECK(es_ictl_flags_at(buf, 72) == -1);
    memcpy(buf, saved_on, sizeof(buf));
    buf[0x21] = 0x10;                                /* ICTL of 16 bytes */
    CHECK(es_ictl_flags_at(buf, 72) == -1);
    memcpy(buf, saved_on, sizeof(buf));
    buf[0x1F] = 0x7F;                                /* a chunk past the end */
    CHECK(es_ictl_flags_at(buf, 72) == -1);
}

/* A chunk of odd length in front of ICTL: chunks are padded to even. */
static void test_a_chunk_before(void)
{
    unsigned char buf[128];
    int n = 0, at;

    memcpy(buf, saved_on, 0x1A);                     /* FORM, PREF, PRHD */
    n = 0x1A;
    memcpy(buf + n, "XTRA", 4);
    buf[n + 4] = 0; buf[n + 5] = 0; buf[n + 6] = 0; buf[n + 7] = 3;
    buf[n + 8] = 1; buf[n + 9] = 2; buf[n + 10] = 3; buf[n + 11] = 0;
    n += 12;
    memcpy(buf + n, saved_on + 0x1A, 72 - 0x1A);     /* ICTL as it was   */
    at = n + 8 + 20;
    n += 72 - 0x1A;
    CHECK(es_ictl_flags_at(buf, n) == at);
    CHECK(es_ictl_offscreen(buf, n) == 1);
    CHECK(es_ictl_set_offscreen(buf, n, 0) == 1);
    CHECK(es_ictl_offscreen(buf, n) == 0);
}

int main(void)
{
    test_the_real_file();
    test_the_default_file();
    test_not_a_file();
    test_a_chunk_before();
    if (failures == 0) {
        printf("icontrol_test: all tests passed\n");
        return 0;
    }
    printf("icontrol_test: %d failure(s)\n", failures);
    return 1;
}
