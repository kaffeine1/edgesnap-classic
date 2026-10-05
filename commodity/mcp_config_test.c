/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * mcp_config_test.c - host tests for mcp_config.c: the two lines turn
 * off and back on, nothing else in the file moves, turning back gives
 * the very same bytes, and a line that only looks like one is left be.
 * The text follows MCP 1.49's own file, as AmiKit writes it.
 */

#include <stdio.h>
#include <string.h>

#include "mcp_config.h"

static int failures;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

static const char sample[] =
    "MCP-CONFIG\n"
    "VERSION=1.48\n"
    "[INTERNAL=ON]\n"
    "CHECKJOY=NO\n"
    "\n"
    "[MOUSESPEEDER=OFF]\n"
    "THRESHOLD=2\n"
    "[MOVEWINDOW=ON]\n"
    "SIZE=2000000\n"
    "WBWINDOWS=NO\n"
    "\n"
    "[SIZEWINDOW=ON]\n"
    "WBWINDOWS=NO\n"
    "[NOGURU=ON]\n"
    "END-CONFIG\n";

static const char sample_off[] =
    "MCP-CONFIG\n"
    "VERSION=1.48\n"
    "[INTERNAL=ON]\n"
    "CHECKJOY=NO\n"
    "\n"
    "[MOUSESPEEDER=OFF]\n"
    "THRESHOLD=2\n"
    "[MOVEWINDOW=OFF]\n"
    "SIZE=2000000\n"
    "WBWINDOWS=NO\n"
    "\n"
    "[SIZEWINDOW=OFF]\n"
    "WBWINDOWS=NO\n"
    "[NOGURU=ON]\n"
    "END-CONFIG\n";

static char buf[512];

static long load(const char *text)
{
    long n = (long)strlen(text);

    memset(buf, 'x', sizeof(buf));
    memcpy(buf, text, (size_t)n + 1);
    return n;
}

static void test_off_and_back(void)
{
    long len = load(sample);

    CHECK(esm_is_config(buf, len));
    CHECK(esm_turn(buf, &len, ESM_BOTH, 1) == ESM_BOTH);
    CHECK(len == (long)strlen(sample_off));
    CHECK(strcmp(buf, sample_off) == 0);
    CHECK(buf[len] == '\0' && buf[len + 1] == 'x');   /* no spill */

    /* off again: nothing left to turn */
    CHECK(esm_turn(buf, &len, ESM_BOTH, 1) == 0);
    CHECK(strcmp(buf, sample_off) == 0);

    /* back on gives the very bytes we started from */
    CHECK(esm_turn(buf, &len, ESM_BOTH, 0) == ESM_BOTH);
    CHECK(len == (long)strlen(sample));
    CHECK(strcmp(buf, sample) == 0);
}

static void test_one_line_only(void)
{
    long len = load(sample);

    CHECK(esm_turn(buf, &len, ESM_SIZEWINDOW, 1) == ESM_SIZEWINDOW);
    CHECK(strstr(buf, "\n[MOVEWINDOW=ON]\n") != NULL);
    CHECK(strstr(buf, "\n[SIZEWINDOW=OFF]\n") != NULL);
    /* only what was turned goes back */
    CHECK(esm_turn(buf, &len, ESM_SIZEWINDOW, 0) == ESM_SIZEWINDOW);
    CHECK(strcmp(buf, sample) == 0);
}

static void test_lookalikes(void)
{
    static const char text[] =
        "MCP-CONFIG\n"
        "X[MOVEWINDOW=ON]\n"
        "[MOVEWINDOW=ON]X\n"
        "[SIZEWINDOW=ONE]\n"
        "END-CONFIG\n";
    long len = load(text);

    CHECK(esm_turn(buf, &len, ESM_BOTH, 1) == 0);
    CHECK(strcmp(buf, text) == 0);
}

static void test_line_ends(void)
{
    static const char crlf[] =
        "MCP-CONFIG\r\n[MOVEWINDOW=ON]\r\n[SIZEWINDOW=ON]";
    static const char crlf_off[] =
        "MCP-CONFIG\r\n[MOVEWINDOW=OFF]\r\n[SIZEWINDOW=OFF]";
    long len = load(crlf);

    /* a CR before the newline, and a last line with none at all */
    CHECK(esm_turn(buf, &len, ESM_BOTH, 1) == ESM_BOTH);
    CHECK(strcmp(buf, crlf_off) == 0);
}

static void test_not_mcp(void)
{
    CHECK(!esm_is_config("[MOVEWINDOW=ON]\n", 16));
    CHECK(!esm_is_config("MCP-CONF", 8));
    CHECK(!esm_is_config(NULL, 20));
    CHECK(esm_is_config(sample, (long)strlen(sample)));
}

int main(void)
{
    test_off_and_back();
    test_one_line_only();
    test_lookalikes();
    test_line_ends();
    test_not_mcp();
    if (failures == 0) {
        printf("mcp_config_test: all tests passed\n");
        return 0;
    }
    printf("mcp_config_test: %d failure(s)\n", failures);
    return 1;
}
