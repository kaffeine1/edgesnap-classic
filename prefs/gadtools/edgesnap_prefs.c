/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * edgesnap_prefs.c - the preferences window on GadTools, for the
 * classic systems: AmigaOS 3.1 and later, and whatever comes after
 * them, until their GUI is known. Same rules as the MUI and ReAction
 * windows: it walks es_settings(), the table in core/config.c that
 * describes the vocabulary, so a setting added there shows up here
 * without a line of code; Save writes ENVARC: and ENV:, Use only ENV:,
 * Cancel nothing; and every value goes through es_setting_apply(), so
 * the window cannot accept what the file parser would refuse.
 *
 * GadTools lays out nothing by itself, so this file does: two columns
 * of framed groups, sized from the screen's font, split where the
 * columns come out most even, which on a PAL 640x256 screen with
 * topaz 8 is the only way the whole vocabulary fits. Values are taken
 * from the IDCMP messages as the gadgets change, with typed text read
 * from StringInfo before a page goes away: GT_GetGadgetAttrs() arrived
 * with V39, and nothing here needs more than V37. The page cycle shares
 * the buttons' row, so it costs none of the screen's scarce height.
 *
 * One row is not EdgeSnap's: "Windows can move off-screen" is
 * AmigaOS's own setting, the box of the same name in its IControl
 * editor, kept in its IControl file. It is here because a drag only
 * snaps where the pointer can reach the edge, and with that setting
 * off a dragged window stops at the edge and holds the pointer back.
 * It appears where the setting exists, from intuition.library 47.
 *
 * From a Shell, and from the installer:
 *   EdgeSnapPrefs OFFSCREEN QUERY|ON|OFF [BOOTONLY]
 * asks or sets that one thing without opening the window; BOOTONLY
 * leaves the live file alone, so that nothing happens until the
 * system starts again. QUERY returns 0 for on and 5 for off; every
 * form returns 10 where the setting does not exist.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <graphics/text.h>
#include <graphics/rastport.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "prefs_io.h"
#include "support.h"
#include "icontrol_offscreen.h"
#include "edgesnap_version.h"
#include "edgesnap.h"

static const char es_version_cookie[] __attribute__((used)) =
    "$VER: EdgeSnapPrefs " ES_VERSION " (" ES_VERSION_DATE ") "
    "Copyright (c) 2026 Michele Dipace";

#define ES_MAX_SETTINGS 48
#define ES_ZONE_COUNT   7
#define ES_MAX_GROUPS   8

/* The zones in the order a person reads them, as in the other two
 * windows: the halves, then the corners, then the whole screen. Two
 * columns, so the group stays narrower than its own labels. */
static const int es_zone_order[ES_ZONE_COUNT] = {
    ES_ZONE_LEFT, ES_ZONE_RIGHT,
    ES_ZONE_TOP_LEFT, ES_ZONE_TOP_RIGHT,
    ES_ZONE_BOTTOM_LEFT, ES_ZONE_BOTTOM_RIGHT,
    ES_ZONE_MAX
};
static const char *const es_zone_label[ES_ZONE_COUNT] = {
    "Left", "Right",
    "Top left", "Top right",
    "Bottom left", "Bottom right",
    "Maximize"
};
#define ES_ZONE_COLS 2
#define ES_ZONE_ROWS ((ES_ZONE_COUNT + ES_ZONE_COLS - 1) / ES_ZONE_COLS)

enum {
    ES_ID_SAVE = 1,
    ES_ID_USE,
    ES_ID_CANCEL,
    ES_ID_OFFSCREEN,          /* AmigaOS's setting, not ours */
    ES_ID_PAGE,
    ES_ID_SETTING = 100,      /* + index into the table  */
    ES_ID_ZONE = 200          /* + index into zone order */
};

/* The Project menu, by its items' user data, as in the other two
 * windows; About names this lane's repository. */
#define ES_MID_ABOUT   1
#define ES_MID_SUPPORT 2
#define ES_MID_QUIT    3
#define ES_CLASSIC_REPO_URL "https://github.com/kaffeine1/edgesnap-classic"

static struct NewMenu es_menu[] = {
    { NM_TITLE, (STRPTR)"Project",                NULL, 0, 0, NULL },
    { NM_ITEM,  (STRPTR)"About...",               (STRPTR)"?", 0, 0,
                (APTR)ES_MID_ABOUT },
    { NM_ITEM,  (STRPTR)"Support development...", NULL, 0, 0,
                (APTR)ES_MID_SUPPORT },
    { NM_ITEM,  NM_BARLABEL,                      NULL, 0, 0, NULL },
    { NM_ITEM,  (STRPTR)"Quit",                   (STRPTR)"Q", 0, 0,
                (APTR)ES_MID_QUIT },
    { NM_END,   NULL,                             NULL, 0, 0, NULL }
};

#define ES_PAD     4          /* inside a frame, and around the columns */
#define ES_GAP     2          /* between two frames                     */
#define ES_LGAP    8          /* GadTools' own gap, label to gadget     */
#define ES_SLIDER_W 80

struct ESFrame {
    int x, y, w, h;
    const char *title;
};

/* One group of the table: which rows it holds and how much room it
 * wants with this font. */
struct ESGroup {
    const char *name;
    int first, last;          /* indices into the table; -1: the row    */
                              /* that is AmigaOS's, not in the table    */
    int rows;
    int keys;                 /* checkboxes beside keys get a whole row */
    int label_w;              /* widest label among the labelled rows   */
    int w, h;                 /* the frame                              */
    int col;
};

struct ESPrefs {
    ESConfig cfg;
    ESHotkeys keys;
    /* Keep even refused text while its page has no gadgets. */
    char key_text[ES_MAX_SETTINGS][ES_HOTKEY_SIZE];
    int value[ES_MAX_SETTINGS];    /* what the gadgets say now        */
    struct Gadget *gadget[ES_MAX_SETTINGS];
    int zone_on[ES_ZONE_COUNT];
    int off_avail;                 /* AmigaOS has the setting         */
    int off_was, off_now;          /* windows can move off-screen     */
    const ESSetting *table;
    int count;
    char *page_name[ES_MAX_SETTINGS + 1];
    int page_first[ES_MAX_SETTINGS + 1];
    int pages, page, next_page;
    int action;                   /* acted on after messages are replied */
    int menu_about, menu_support; /* the same, for the Project menu      */
    struct Menu *menu;
    int focus;                    /* refused field, until activation works */
    struct Screen *scr;
    struct DrawInfo *dri;
    struct TextAttr ta;
    APTR vi;
    struct Gadget *glist;
    int gadgets;                  /* excludes Intuition's own gadgets */
    struct Window *win;
    int bl, bt;                    /* the window's borders, left, top */
    int fh;                        /* font height                     */
    int rowh;                      /* one row of gadgets              */
    int gadh;                      /* a text gadget's height          */
    int box_w, box_h;              /* a checkbox                      */
    struct ESFrame frame[ES_MAX_GROUPS];
    int frames;
    int page_w, button_y;
    int inner_w, inner_h;
};

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;

/* ------------------------------------------------------------ text */

static int es_text_w(struct ESPrefs *p, const char *s)
{
    return (int)TextLength(&p->scr->RastPort, (CONST_STRPTR)s,
                           (ULONG)strlen(s));
}

static int es_same_group(const char *a, const char *b)
{
    return a == b || strcmp(a, b) == 0;
}

#define ES_TITLE_H (p->fh + 4)

/* ---------------------------------------------------------- sizes */

static int es_is_string(const ESSetting *s)
{
    /* a range too long for a slider is typed instead */
    return s->kind == ES_SET_INT && s->max > 200;
}

/* The gadget alone, without its label: what the row needs to the
 * right of the label column. */
static int es_gadget_w(struct ESPrefs *p, const ESSetting *s)
{
    int w = 0, i;

    switch (s->kind) {
    case ES_SET_BOOL:
        return p->box_w;
    case ES_SET_CHOICE:
        for (i = 0; s->choices[i] != 0; i++) {
            int tw = es_text_w(p, s->choices[i]);

            if (tw > w) {
                w = tw;
            }
        }
        return w + 40;                       /* the arrow and margins */
    case ES_SET_INT:
        if (es_is_string(s)) {
            return es_text_w(p, "00000") + 12;
        }
        return ES_SLIDER_W + 4 + es_text_w(p, "000");  /* and its level */
    case ES_SET_KEY:
        return es_text_w(p, "ctrl alt right") + 12;
    default:
        return 0;
    }
}

/* The zone grid: each column as wide as its widest label. */
static void es_zone_cells(struct ESPrefs *p, int *cell)
{
    int k;

    for (k = 0; k < ES_ZONE_COLS; k++) {
        cell[k] = 0;
    }
    for (k = 0; k < ES_ZONE_COUNT; k++) {
        int w = p->box_w + ES_LGAP + es_text_w(p, es_zone_label[k]);

        if (w > cell[k % ES_ZONE_COLS]) {
            cell[k % ES_ZONE_COLS] = w;
        }
    }
}

static int es_zone_grid_w(struct ESPrefs *p)
{
    int cell[ES_ZONE_COLS], w = 0, k;

    es_zone_cells(p, cell);
    for (k = 0; k < ES_ZONE_COLS; k++) {
        w += cell[k];
    }
    return w + (ES_ZONE_COLS - 1) * ES_LGAP;
}

static const char es_off_group[] = "AmigaOS";
static const char es_off_label[] = "Windows can move off-screen:";
static const char es_off_after[] = "Sensitivity";   /* its place */

static int es_measure(struct ESPrefs *p, int page, struct ESGroup *g, int max)
{
    int n = 0, i, k;

    for (i = p->page_first[page]; i < p->page_first[page + 1]; i++) {
        const ESSetting *s = &p->table[i];

        if (n > 0 && es_same_group(g[n - 1].name, s->group)) {
            g[n - 1].last = i;
        } else {
            if (n >= max) {
                break;
            }
            g[n].name = s->group;
            g[n].first = g[n].last = i;
            g[n].rows = 0;
            g[n].keys = 0;
            g[n].label_w = 0;
            n++;
        }
        if (s->kind == ES_SET_ZONES) {
            g[n - 1].rows += ES_ZONE_ROWS;
        } else {
            g[n - 1].rows += 1;
            if (s->kind == ES_SET_KEY) {
                g[n - 1].keys = 1;
            }
        }
    }
    /* AmigaOS's row: a group of its own, after the one it belongs
     * next to, or last */
    if (page == 0 && p->off_avail && n < max) {
        int at = n;

        for (i = 0; i < n; i++) {
            if (es_same_group(g[i].name, es_off_after)) {
                at = i + 1;
                break;
            }
        }
        for (i = n; i > at; i--) {
            g[i] = g[i - 1];
        }
        g[at].name = es_off_group;
        g[at].first = g[at].last = -1;
        g[at].rows = 1;
        g[at].keys = 0;
        g[at].label_w = es_text_w(p, es_off_label);
        n++;
    }
    for (i = 0; i < n; i++) {
        int widest = 0;

        /* A long checkbox label must not widen every key's label
         * column. It uses its own row, with its label on the right. */
        for (k = g[i].first; g[i].first >= 0 && k <= g[i].last; k++) {
            const ESSetting *s = &p->table[k];

            if (s->kind != ES_SET_ZONES &&
                !(g[i].keys && s->kind == ES_SET_BOOL)) {
                int lw = es_text_w(p, s->label) + es_text_w(p, ":");

                if (lw > g[i].label_w) {
                    g[i].label_w = lw;
                }
            }
        }
        if (g[i].first < 0) {
            widest = g[i].label_w + ES_LGAP + p->box_w;
        }
        for (k = g[i].first; g[i].first >= 0 && k <= g[i].last; k++) {
            const ESSetting *s = &p->table[k];
            int rw;

            if (s->kind == ES_SET_ZONES) {
                rw = es_zone_grid_w(p);
            } else if (g[i].keys && s->kind == ES_SET_BOOL) {
                rw = p->box_w + ES_LGAP + es_text_w(p, s->label);
            } else {
                rw = g[i].label_w + ES_LGAP + es_gadget_w(p, s);
            }
            if (rw > widest) {
                widest = rw;
            }
        }
        g[i].w = ES_PAD * 2 + widest;
        g[i].h = ES_TITLE_H + 2 + g[i].rows * p->rowh + ES_PAD;
    }
    return n;
}

/* Two columns in table order, cut where the taller column comes out
 * shortest. */
static void es_columns(struct ESGroup *g, int n)
{
    int best = 1, best_h = 0, cut, i;

    for (cut = 1; cut < n; cut++) {
        int h0 = 0, h1 = 0, tall;

        for (i = 0; i < cut; i++) {
            h0 += g[i].h + ES_GAP;
        }
        for (i = cut; i < n; i++) {
            h1 += g[i].h + ES_GAP;
        }
        tall = h0 > h1 ? h0 : h1;
        if (cut == 1 || tall < best_h) {
            best = cut;
            best_h = tall;
        }
    }
    for (i = 0; i < n; i++) {
        g[i].col = (i < best) ? 0 : 1;
    }
}

static int es_layout(struct ESPrefs *p, int page, struct ESGroup *g,
                      int *col_w, int *height)
{
    int n = es_measure(p, page, g, ES_MAX_GROUPS);
    int col_h[2] = { ES_PAD, ES_PAD };
    int i;

    es_columns(g, n);
    col_w[0] = col_w[1] = 0;
    for (i = 0; i < n; i++) {
        int c = g[i].col;

        if (g[i].w > col_w[c]) {
            col_w[c] = g[i].w;
        }
        col_h[c] += g[i].h + ES_GAP;
    }
    *height = col_h[0] > col_h[1] ? col_h[0] : col_h[1];
    return n;
}

/* One size for all pages: no asynchronous resize, and the buttons do
 * not move under the pointer when the page changes. */
static void es_size(struct ESPrefs *p)
{
    struct ESGroup g[ES_MAX_GROUPS];
    int page, button_w = es_text_w(p, "Cancel") + 16;

    for (page = 0; page < p->pages; page++) {
        int col_w[2], height, width, cycle_w;

        es_layout(p, page, g, col_w, &height);
        width = col_w[0] + col_w[1] + ES_PAD * 3;
        if (width > p->inner_w) {
            p->inner_w = width;
        }
        if (height > p->button_y) {
            p->button_y = height;
        }
        cycle_w = es_text_w(p, p->page_name[page]) + 40;
        if (cycle_w > p->page_w) {
            p->page_w = cycle_w;
        }
    }
    if (p->inner_w < ES_PAD * 3 + p->page_w + button_w * 3 + ES_GAP * 2) {
        p->inner_w = ES_PAD * 3 + p->page_w + button_w * 3 + ES_GAP * 2;
    }
    p->inner_h = p->button_y + p->gadh + ES_PAD;
}

/* ---------------------------------------------------------- gadgets */

/* GadTools keeps the pointer to a cycle's label array, so each cycle
 * gadget gets its own, alive as long as the program. */
static char *es_cycle_entries[ES_MAX_SETTINGS][8];

static char **es_choices(int index, const char *const *choices)
{
    char **entries = es_cycle_entries[index];
    int n = 0;

    while (choices[n] != 0 && n < 7) {
        entries[n] = (char *)choices[n];
        n++;
    }
    entries[n] = 0;
    return entries;
}

static void es_newgadget(struct ESPrefs *p, struct NewGadget *ng, int id,
                         const char *text, int x, int y, int w, int h)
{
    memset(ng, 0, sizeof(*ng));
    ng->ng_TextAttr = &p->ta;
    ng->ng_VisualInfo = p->vi;
    ng->ng_GadgetID = (UWORD)id;
    ng->ng_GadgetText = (UBYTE *)text;
    ng->ng_LeftEdge = (WORD)(p->bl + x);
    ng->ng_TopEdge = (WORD)(p->bt + y);
    ng->ng_Width = (WORD)w;
    ng->ng_Height = (WORD)h;
}

static struct Gadget *es_checkbox(struct ESPrefs *p, struct Gadget *prev,
                                  int id, const char *text, int flags,
                                  int x, int y, int on)
{
    struct NewGadget ng;

    es_newgadget(p, &ng, id, text, x, y + (p->rowh - p->box_h) / 2,
                 p->box_w, p->box_h);
    ng.ng_Flags = (UWORD)flags;
    return CreateGadget(CHECKBOX_KIND, prev, &ng,
                        GTCB_Checked, on ? TRUE : FALSE,
                        GadToolsBase->lib_Version >= 39 ?
                            GTCB_Scaled : TAG_IGNORE, TRUE,
                        TAG_DONE);
}

/* One labelled row: the group's gadget column and a label to its left,
 * except for a checkbox beside keys, whose label follows its box. */
static struct Gadget *es_make(struct ESPrefs *p, struct Gadget *prev,
                              const ESSetting *s, int index,
                              int x, int y, int label_w, int keys)
{
    struct NewGadget ng;
    static char label[ES_MAX_SETTINGS][48];
    int value = p->value[index];
    int gx = x + label_w + ES_LGAP;
    int gy = y + (p->rowh - p->gadh) / 2;
    int id = ES_ID_SETTING + index;

    /* the table's labels have no colon: they feed the manual too */
    strncpy(label[index], s->label, sizeof(label[index]) - 2);
    label[index][sizeof(label[index]) - 2] = '\0';
    strcat(label[index], ":");

    switch (s->kind) {
    case ES_SET_BOOL:
        if (keys) {
            return es_checkbox(p, prev, id, s->label, PLACETEXT_RIGHT,
                               x, y, value);
        }
        return es_checkbox(p, prev, id, label[index], PLACETEXT_LEFT,
                           gx, y, value);
    case ES_SET_CHOICE:
        es_newgadget(p, &ng, id, label[index], gx, gy,
                     es_gadget_w(p, s), p->gadh);
        ng.ng_Flags = PLACETEXT_LEFT;
        return CreateGadget(CYCLE_KIND, prev, &ng,
                            GTCY_Labels, (ESTagData)es_choices(index, s->choices),
                            GTCY_Active, value,
                            TAG_DONE);
    case ES_SET_KEY:
        es_newgadget(p, &ng, id, label[index], gx, gy,
                     es_gadget_w(p, s), p->gadh);
        ng.ng_Flags = PLACETEXT_LEFT;
        return CreateGadget(STRING_KIND, prev, &ng,
                            GTST_String, (ESTagData)p->key_text[index],
                            GTST_MaxChars, ES_HOTKEY_SIZE - 1,
                            TAG_DONE);
    case ES_SET_INT:
        if (es_is_string(s)) {
            es_newgadget(p, &ng, id, label[index], gx, gy,
                         es_gadget_w(p, s), p->gadh);
            ng.ng_Flags = PLACETEXT_LEFT;
            return CreateGadget(INTEGER_KIND, prev, &ng,
                                GTIN_Number, value,
                                GTIN_MaxChars, 5,
                                TAG_DONE);
        }
        es_newgadget(p, &ng, id, label[index], gx, gy,
                     ES_SLIDER_W, p->gadh);
        ng.ng_Flags = PLACETEXT_LEFT;
        return CreateGadget(SLIDER_KIND, prev, &ng,
                            GTSL_Min, s->min,
                            GTSL_Max, s->max,
                            GTSL_Level, value,
                            GTSL_LevelFormat, (ESTagData)"%3ld",
                            GTSL_MaxLevelLen, 3,
                            GTSL_LevelPlace, PLACETEXT_RIGHT,
                            GA_RelVerify, TRUE,
                            GA_Immediate, TRUE,
                            TAG_DONE);
    default:
        return prev;
    }
}

static struct Gadget *es_make_zones(struct ESPrefs *p, struct Gadget *prev,
                                    int index, int x, int y)
{
    int mask = p->value[index];
    int cell[ES_ZONE_COLS], k;

    es_zone_cells(p, cell);
    for (k = 0; k < ES_ZONE_COUNT && prev != NULL; k++) {
        int col = k % ES_ZONE_COLS, row = k / ES_ZONE_COLS;
        int cx = x, c;

        for (c = 0; c < col; c++) {
            cx += cell[c] + ES_LGAP;
        }
        p->zone_on[k] = (mask & (int)ES_ZONEBIT(es_zone_order[k])) != 0;
        prev = es_checkbox(p, prev, ES_ID_ZONE + k, es_zone_label[k],
                           PLACETEXT_RIGHT, cx, y + row * p->rowh,
                           p->zone_on[k]);
    }
    return prev;
}

static struct Gadget *es_button(struct ESPrefs *p, struct Gadget *prev,
                                const char *text, int id, int x, int y,
                                int w)
{
    struct NewGadget ng;

    es_newgadget(p, &ng, id, text, x, y, w, p->gadh);
    ng.ng_Flags = PLACETEXT_IN;
    return CreateGadget(BUTTON_KIND, prev, &ng, TAG_DONE);
}

/*
 * Build every gadget at its place. Returns 0 when a gadget would not
 * be created, with the list freed by the caller.
 */
static int es_build(struct ESPrefs *p)
{
    struct ESGroup g[ES_MAX_GROUPS];
    struct Gadget *gad;
    struct NewGadget ng;
    int n, i, k;
    int col_w[2], col_y[2], col_x[2];
    int button_w, height;

    n = es_layout(p, p->page, g, col_w, &height);
    col_x[0] = ES_PAD;
    col_x[1] = ES_PAD + col_w[0] + ES_PAD;
    col_y[0] = col_y[1] = ES_PAD;

    gad = CreateContext(&p->glist);
    memset(p->gadget, 0, sizeof(p->gadget));
    p->frames = 0;
    for (i = 0; i < n && gad != NULL; i++) {
        int c = g[i].col;
        int x = col_x[c], y = col_y[c];
        int gy = y + ES_TITLE_H + 2;

        if (p->frames < ES_MAX_GROUPS) {
            struct ESFrame *f = &p->frame[p->frames++];

            f->x = x;
            f->y = y;
            f->w = col_w[c];
            f->h = g[i].h;
            f->title = g[i].name;
        }
        if (g[i].first < 0) {
            gad = es_checkbox(p, gad, ES_ID_OFFSCREEN, es_off_label,
                              PLACETEXT_LEFT,
                              x + ES_PAD + g[i].label_w + ES_LGAP, gy,
                              p->off_now);
        }
        for (k = g[i].first;
             g[i].first >= 0 && k <= g[i].last && gad != NULL; k++) {
            const ESSetting *s = &p->table[k];

            if (s->kind == ES_SET_ZONES) {
                gad = es_make_zones(p, gad, k, x + ES_PAD, gy);
                gy += p->rowh * ES_ZONE_ROWS;
            } else {
                gad = es_make(p, gad, s, k, x + ES_PAD, gy,
                               g[i].label_w, g[i].keys);
                p->gadget[k] = gad;
                gy += p->rowh;
            }
        }
        col_y[c] = y + g[i].h + ES_GAP;
    }
    /* The page cycle on the left, the three buttons on the right. */
    if (gad != NULL) {
        es_newgadget(p, &ng, ES_ID_PAGE, NULL, ES_PAD, p->button_y,
                     p->page_w, p->gadh);
        gad = CreateGadget(CYCLE_KIND, gad, &ng,
                           GTCY_Labels, (ESTagData)p->page_name,
                           GTCY_Active, p->page,
                           TAG_DONE);
    }
    button_w = es_text_w(p, "Cancel") + 16;
    if (gad != NULL) {
        int bx = p->inner_w - ES_PAD - button_w * 3 - ES_GAP * 2;

        gad = es_button(p, gad, "Save", ES_ID_SAVE, bx, p->button_y, button_w);
        if (gad != NULL) {
            gad = es_button(p, gad, "Use", ES_ID_USE,
                            bx + button_w + ES_GAP, p->button_y, button_w);
        }
        if (gad != NULL) {
            gad = es_button(p, gad, "Cancel", ES_ID_CANCEL,
                            bx + (button_w + ES_GAP) * 2,
                            p->button_y, button_w);
        }
    }
    p->gadgets = 0;
    if (gad == NULL) {
        return 0;
    }
    for (gad = p->glist; gad != NULL; gad = gad->NextGadget) {
        p->gadgets++;
    }
    return 1;
}

/* The frames and their titles: drawn, not gadgets, so they are
 * repainted by hand whenever the window asks. */
static void es_draw_frames(struct ESPrefs *p)
{
    struct RastPort *rp = p->win->RPort;
    int i;

    for (i = 0; i < p->frames; i++) {
        struct ESFrame *f = &p->frame[i];
        int x = p->bl + f->x;
        int y = p->bt + f->y;

        DrawBevelBox(rp, x, y, f->w, f->h,
                     GT_VisualInfo, (ESTagData)p->vi,
                     GTBB_Recessed, TRUE,
                     TAG_DONE);
        SetAPen(rp, p->dri->dri_Pens[HIGHLIGHTTEXTPEN]);
        SetDrMd(rp, JAM1);
        Move(rp, x + ES_PAD + 2, y + 2 + rp->TxBaseline);
        Text(rp, (CONST_STRPTR)f->title, (ULONG)strlen(f->title));
    }
}

/* ----------------------------------------------------------- values */

/* The number in a string gadget, as typed: Intuition parses it only on
 * Return, and Save is one click away. */
static int es_typed_number(struct Gadget *gad, int fallback)
{
    struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
    const char *s;
    int value = 0, neg = 0, digits = 0;

    if (si == NULL || si->Buffer == NULL) {
        return fallback;
    }
    s = (const char *)si->Buffer;
    while (*s == ' ') {
        s++;
    }
    if (*s == '-') {
        neg = 1;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        digits++;
        s++;
    }
    if (digits == 0) {
        return fallback;
    }
    return neg ? -value : value;
}

static void es_collect(struct ESPrefs *p)
{
    int i, k;

    for (i = p->page_first[p->page]; i < p->page_first[p->page + 1]; i++) {
        const ESSetting *s = &p->table[i];

        if (s->kind == ES_SET_KEY) {
            struct StringInfo *si = p->gadget[i] != NULL ?
                (struct StringInfo *)p->gadget[i]->SpecialInfo : NULL;

            if (si != NULL && si->Buffer != NULL) {
                strncpy(p->key_text[i], (const char *)si->Buffer,
                         ES_HOTKEY_SIZE - 1);
                p->key_text[i][ES_HOTKEY_SIZE - 1] = '\0';
            }
            continue;
        }
        if (es_is_string(s) && p->gadget[i] != NULL) {
            p->value[i] = es_typed_number(p->gadget[i], p->value[i]);
        }
        if (s->kind == ES_SET_ZONES) {
            int mask = 0;

            for (k = 0; k < ES_ZONE_COUNT; k++) {
                if (p->zone_on[k]) {
                    mask |= (int)ES_ZONEBIT(es_zone_order[k]);
                }
            }
            p->value[i] = mask;
        }
        es_setting_apply(&p->cfg, i, p->value[i]);
    }
}

/* A gadget spoke: remember what it says. */
static void es_changed(struct ESPrefs *p, struct Gadget *gad, UWORD code)
{
    int id = gad->GadgetID;

    if (id == ES_ID_OFFSCREEN) {
        p->off_now = (gad->Flags & GFLG_SELECTED) != 0;
    } else if (id >= ES_ID_ZONE && id < ES_ID_ZONE + ES_ZONE_COUNT) {
        p->zone_on[id - ES_ID_ZONE] = (gad->Flags & GFLG_SELECTED) != 0;
    } else if (id >= ES_ID_SETTING && id < ES_ID_SETTING + p->count) {
        int i = id - ES_ID_SETTING;

        switch (p->table[i].kind) {
        case ES_SET_BOOL:
            p->value[i] = (gad->Flags & GFLG_SELECTED) != 0;
            break;
        case ES_SET_CHOICE:
            p->value[i] = (int)code;
            break;
        case ES_SET_INT:
            if (es_is_string(&p->table[i])) {
                p->value[i] = es_typed_number(gad, p->value[i]);
            } else {
                p->value[i] = (int)(WORD)code;
            }
            break;
        default:
            break;
        }
    }
}

static void es_focus(struct ESPrefs *p)
{
    if (p->focus >= 0 && p->gadget[p->focus] != NULL &&
        ActivateGadget(p->gadget[p->focus], p->win, NULL)) {
        p->focus = -1;
    }
}

/* Every item picked in one go, as Intuition chains them; the
 * requesters wait until the messages are replied. */
static void es_menu_pick(struct ESPrefs *p, UWORD code)
{
    while (code != MENUNULL) {
        struct MenuItem *item = ItemAddress(p->menu, code);

        if (item == NULL) {
            break;
        }
        switch ((ULONG)GTMENUITEM_USERDATA(item)) {
        case ES_MID_ABOUT:
            p->menu_about = 1;
            break;
        case ES_MID_SUPPORT:
            p->menu_support = 1;
            break;
        case ES_MID_QUIT:
            p->action = ES_ID_CANCEL;
            break;
        default:
            break;
        }
        code = item->NextSelect;
    }
}

static void es_about(struct ESPrefs *p)
{
    struct EasyStruct es;

    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"About EdgeSnap Preferences";
    es.es_TextFormat = (UBYTE *)("EdgeSnap " ES_VERSION_LABEL " ("
                                 ES_VERSION_DATE ")\n"
                                 "(c) 2026 Michele Dipace, MIT licence\n\n"
                                 ES_CLASSIC_REPO_URL);
    es.es_GadgetFormat = (UBYTE *)"OK";
    EasyRequestArgs(p->win, &es, NULL, NULL);
}

/* The link, with a button that puts it on the clipboard. */
static void es_support(struct ESPrefs *p)
{
    struct EasyStruct es;

    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"Support development";
    es.es_TextFormat = (UBYTE *)(ESP_SUPPORT_TEXT "\n\n" ESP_SUPPORT_URL);
    es.es_GadgetFormat = (UBYTE *)"Copy link|Close";
    if (EasyRequestArgs(p->win, &es, NULL, NULL) == 1 &&
        !esp_clip_text(ESP_SUPPORT_URL)) {
        es.es_TextFormat = (UBYTE *)"The link could not be put on the "
                           "clipboard.";
        es.es_GadgetFormat = (UBYTE *)"OK";
        EasyRequestArgs(p->win, &es, NULL, NULL);
    }
}

/* Never replace gadgets while a message still refers to them. Actions
 * wait until this whole batch, including GadTools' context, is replied. */
static void es_messages(struct ESPrefs *p)
{
    struct IntuiMessage *msg;

    while ((msg = GT_GetIMsg(p->win->UserPort)) != NULL) {
        struct Gadget *gad = (struct Gadget *)msg->IAddress;

        switch (msg->Class) {
        case IDCMP_CLOSEWINDOW:
            p->action = ES_ID_CANCEL;
            break;
        case IDCMP_REFRESHWINDOW:
            GT_BeginRefresh(p->win);
            es_draw_frames(p);
            GT_EndRefresh(p->win, TRUE);
            break;
        case IDCMP_ACTIVEWINDOW:
            es_focus(p);
            break;
        case IDCMP_GADGETUP:
        case IDCMP_GADGETDOWN:
        case IDCMP_MOUSEMOVE:
            if (gad == NULL) {
                break;
            }
            if (gad->GadgetID == ES_ID_PAGE) {
                if (msg->Code < p->pages) {
                    p->next_page = (int)msg->Code;
                }
            } else if (gad->GadgetID == ES_ID_SAVE ||
                       gad->GadgetID == ES_ID_USE ||
                       gad->GadgetID == ES_ID_CANCEL) {
                if (p->action != ES_ID_CANCEL) {
                    p->action = gad->GadgetID;
                }
            } else {
                es_changed(p, gad, msg->Code);
            }
            break;
        case IDCMP_VANILLAKEY:
            if (msg->Code == 27) {             /* Esc: cancel */
                p->action = ES_ID_CANCEL;
            }
            break;
        case IDCMP_MENUPICK:
            es_menu_pick(p, msg->Code);
            break;
        default:
            break;
        }
        GT_ReplyIMsg(msg);
    }
}

static int es_show_page(struct ESPrefs *p, int page)
{
    struct RastPort *rp = p->win->RPort;

    if (page == p->page) {
        p->next_page = page;
        return 1;
    }
    p->focus = -1;
    es_collect(p);
    RemoveGList(p->win, p->glist, p->gadgets);
    /* Removal deactivates a string field. Catch its last edits and
     * any messages that arrived during removal before freeing it. */
    es_messages(p);
    es_collect(p);
    FreeGadgets(p->glist);
    p->glist = NULL;
    p->page = p->next_page = page;

    SetAPen(rp, p->dri->dri_Pens[BACKGROUNDPEN]);
    SetDrMd(rp, JAM1);
    RectFill(rp, p->bl, p->bt,
              p->bl + p->inner_w - 1, p->bt + p->inner_h - 1);
    if (!es_build(p)) {
        printf("EdgeSnapPrefs: the page's gadgets would not build\n");
        return 0;
    }
    AddGList(p->win, p->glist, (UWORD)-1, p->gadgets, NULL);
    RefreshGList(p->glist, p->win, NULL, p->gadgets);
    GT_RefreshWindow(p->win, NULL);
    es_draw_frames(p);
    return 1;
}

static int es_key_error(struct ESPrefs *p, int bad, int duplicate,
                         const char *text)
{
    struct EasyStruct es;
    ULONG args[3];
    int page;

    for (page = 0; page < p->pages; page++) {
        if (bad >= p->page_first[page] && bad < p->page_first[page + 1]) {
            if (!es_show_page(p, page)) {
                return 0;
            }
            break;
        }
    }
    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"EdgeSnap Preferences";
    es.es_GadgetFormat = (UBYTE *)"OK";
    args[0] = (ULONG)p->table[bad].label;
    args[1] = (ULONG)text;
    if (duplicate >= 0) {
        args[2] = (ULONG)p->table[duplicate].label;
        es.es_TextFormat = (UBYTE *)"The hotkey for %s:\n%s\n\nMatches %s. "
                                  "Choose another key or leave it empty.";
    } else {
        es.es_TextFormat = (UBYTE *)"Invalid hotkey for %s:\n%s\n\nUse an input "
                                  "description as Exchange writes it, or "
                                  "leave it empty.";
    }
    EasyRequestArgs(p->win, &es, NULL, args);
    p->focus = bad;
    /* The requester may return before our window is active again.
     * IDCMP_ACTIVEWINDOW retries if this first activation fails. */
    es_focus(p);
    return 1;
}

/* Raw text survives page changes, even when the parser refuses it.
 * Only Save and Use ask the parser, then ParseIX and the duplicate check.
 * -1 means the refused field's page could not be rebuilt. */
static int es_validate(struct ESPrefs *p)
{
    int i, bad, duplicate;

    for (i = 0; i < p->count; i++) {
        if (p->table[i].kind == ES_SET_KEY &&
            es_setting_apply_text(&p->keys, i, p->key_text[i]) != ES_OK) {
            return es_key_error(p, i, -1, p->key_text[i]) ? 0 : -1;
        }
    }
    if (!esp_validate_hotkeys(&p->keys, &bad, &duplicate)) {
        return es_key_error(p, bad, duplicate,
                             es_setting_text(&p->keys, bad)) ? 0 : -1;
    }
    return 1;
}

/* ------------------------------------------------------------- main */

/*
 * AmigaOS's setting, when the box was changed. Writing the live file
 * makes Intuition ask for the Workbench screen to be reset, with a
 * requester of its own that says nothing of why: so ours comes first
 * and says it. Save keeps the setting for good, Use for this session.
 */
static int es_store_offscreen(struct ESPrefs *p, int permanent)
{
    static struct EasyStruct es;

    if (!p->off_avail) {
        return 1;
    }
    if (p->off_now == p->off_was) {
        /* Save keeps for good what an earlier Use turned on. An
         * unchanged box never turns it off: the Installer's BOOTONLY
         * leaves it on in ENVARC: only, until the restart. */
        if (permanent && p->off_now && eso_query(1) == 0) {
            return eso_set(1, 0, 1);
        }
        return 1;
    }
    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"EdgeSnap Preferences";
    es.es_TextFormat = (UBYTE *)(permanent ?
        "\"Windows can move off-screen\" is a setting of AmigaOS.\n"
        "To change it, AmigaOS now asks to reset the Workbench\n"
        "screen and names the windows to close first.\n\n"
        "Close them and press Retry, or press Cancel:\n"
        "the setting is saved either way, and takes\n"
        "effect when the system starts again." :
        "\"Windows can move off-screen\" is a setting of AmigaOS.\n"
        "To change it, AmigaOS now asks to reset the Workbench\n"
        "screen and names the windows to close first.\n\n"
        "Close them and press Retry. With Cancel the\n"
        "setting stays as it was.");
    es.es_GadgetFormat = (UBYTE *)"OK";
    EasyRequestArgs(p->win, &es, NULL, NULL);
    return eso_set(p->off_now, 1, permanent);
}

static int es_run(struct ESPrefs *p)
{
    int rc = RETURN_OK, done = 0;
    ULONG winsig;

    p->win = OpenWindowTags(NULL,
                            WA_PubScreen, (ESTagData)p->scr,
                            WA_Title, (ESTagData)"EdgeSnap Preferences",
                            WA_InnerWidth, p->inner_w,
                            WA_InnerHeight, p->inner_h,
                            WA_Left, 20,
                            WA_Top, p->scr->BarHeight + 1,
                            WA_Gadgets, (ESTagData)p->glist,
                            WA_AutoAdjust, TRUE,
                            WA_DragBar, TRUE,
                            WA_DepthGadget, TRUE,
                            WA_CloseGadget, TRUE,
                            WA_Activate, TRUE,
                            WA_SmartRefresh, TRUE,
                            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                                      IDCMP_VANILLAKEY | IDCMP_ACTIVEWINDOW |
                                      BUTTONIDCMP | CHECKBOXIDCMP |
                                      CYCLEIDCMP | SLIDERIDCMP |
                                      INTEGERIDCMP | STRINGIDCMP |
                                      IDCMP_MENUPICK,
                            TAG_DONE);
    if (p->win == NULL) {
        printf("EdgeSnapPrefs: the window would not open (%dx%d inside)\n",
               p->inner_w, p->inner_h);
        return RETURN_FAIL;
    }
    /* Without its menu the window still does its job. */
    p->menu = CreateMenus(es_menu, TAG_DONE);
    if (p->menu != NULL &&
        (!LayoutMenus(p->menu, p->vi, GTMN_NewLookMenus, TRUE, TAG_DONE) ||
         !SetMenuStrip(p->win, p->menu))) {
        FreeMenus(p->menu);
        p->menu = NULL;
    }
    GT_RefreshWindow(p->win, NULL);
    es_draw_frames(p);
    winsig = 1UL << p->win->UserPort->mp_SigBit;

    while (!done) {
        int action, valid;

        if (p->action == 0 && p->next_page == p->page) {
            ULONG got = Wait(winsig | SIGBREAKF_CTRL_C);

            if (got & SIGBREAKF_CTRL_C) {
                break;
            }
        }
        es_messages(p);
        if (p->menu_about) {
            p->menu_about = 0;
            es_about(p);
        }
        if (p->menu_support) {
            p->menu_support = 0;
            es_support(p);
        }
        action = p->action;
        p->action = 0;
        if (action == ES_ID_CANCEL) {
            done = 1;
        } else if (action == ES_ID_SAVE || action == ES_ID_USE) {
            es_collect(p);
            valid = es_validate(p);
            if (valid < 0) {
                rc = RETURN_FAIL;
                done = 1;
            } else if (valid) {
                int permanent = action == ES_ID_SAVE;

                if (!esp_store(&p->cfg, &p->keys, permanent)) {
                    rc = RETURN_WARN;
                }
                if (!es_store_offscreen(p, permanent)) {
                    rc = RETURN_WARN;
                }
                done = 1;
            }
        } else if (!es_show_page(p, p->next_page)) {
            rc = RETURN_FAIL;
            done = 1;
        }
    }
    if (p->menu != NULL) {
        ClearMenuStrip(p->win);
        FreeMenus(p->menu);
        p->menu = NULL;
    }
    CloseWindow(p->win);
    p->win = NULL;
    return rc;
}

static int es_word(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        char ca = *a, cb = *b;

        if (ca >= 'a' && ca <= 'z') {
            ca = (char)(ca - 'a' + 'A');
        }
        if (cb >= 'a' && cb <= 'z') {
            cb = (char)(cb - 'a' + 'A');
        }
        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

/* AmigaOS has the setting from intuition.library 47, and the file is
 * one this program understands (or is not there at all). */
static int es_offscreen_available(void)
{
    struct Library *i47 = OpenLibrary((CONST_STRPTR)"intuition.library", 47);

    if (i47 == NULL) {
        return 0;
    }
    CloseLibrary(i47);
    return eso_query(0) >= 0 && eso_query(1) >= 0;
}

/* EdgeSnapPrefs OFFSCREEN QUERY|ON|OFF [BOOTONLY]: no window. */
static int es_offscreen_command(int argc, char **argv)
{
    int bootonly = argc >= 4 && es_word(argv[3], "BOOTONLY");
    int state;

    if (!es_offscreen_available()) {
        printf("EdgeSnapPrefs: this system has no such setting\n");
        return RETURN_ERROR;
    }
    if (es_word(argv[2], "QUERY")) {
        state = eso_query(bootonly);
        printf("windows can move off-screen: %s\n", state ? "yes" : "no");
        return state ? RETURN_OK : RETURN_WARN;
    }
    if (es_word(argv[2], "ON") || es_word(argv[2], "OFF")) {
        state = es_word(argv[2], "ON");
        if (!eso_set(state, !bootonly, 1)) {
            printf("EdgeSnapPrefs: the IControl preferences could not "
                   "be written\n");
            return RETURN_ERROR;
        }
        printf("windows can move off-screen: %s%s\n", state ? "yes" : "no",
               bootonly ? ", from the next start" : "");
        return RETURN_OK;
    }
    printf("EdgeSnapPrefs OFFSCREEN QUERY|ON|OFF [BOOTONLY]\n");
    return RETURN_ERROR;
}

int main(int argc, char **argv)
{
    static struct ESPrefs p;
    int rc = RETURN_FAIL, i;

    (void)es_version_cookie;
    if (argc >= 3 && es_word(argv[1], "OFFSCREEN")) {
        return es_offscreen_command(argc, argv);
    }
    IntuitionBase = (struct IntuitionBase *)
        OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 37);
    GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 37);
    if (IntuitionBase == NULL || GfxBase == NULL || GadToolsBase == NULL) {
        printf("EdgeSnapPrefs: AmigaOS 2.04 or later is needed\n");
        goto out;
    }
    esp_load(&p.cfg, &p.keys);
    p.off_avail = es_offscreen_available();
    p.off_was = p.off_now = p.off_avail ? eso_query(0) : 0;
    p.table = es_settings(&p.count);
    if (p.count < 1 || p.count > ES_MAX_SETTINGS) {
        printf("EdgeSnapPrefs: too many settings, or none\n");
        goto out;
    }
    for (i = 0; i < p.count; i++) {
        if (i == 0 || !es_same_group(p.table[i].page, p.table[i - 1].page)) {
            p.page_name[p.pages] = (char *)p.table[i].page;
            p.page_first[p.pages++] = i;
        }
        p.value[i] = es_setting_value(&p.cfg, i);
        if (p.table[i].kind == ES_SET_KEY) {
            strncpy(p.key_text[i], es_setting_text(&p.keys, i),
                     ES_HOTKEY_SIZE - 1);
            p.key_text[i][ES_HOTKEY_SIZE - 1] = '\0';
        }
    }
    p.page_name[p.pages] = NULL;
    p.page_first[p.pages] = p.count;
    p.focus = -1;
    p.scr = LockPubScreen(NULL);
    if (p.scr == NULL) {
        printf("EdgeSnapPrefs: no public screen\n");
        goto out;
    }
    p.dri = GetScreenDrawInfo(p.scr);
    p.vi = GetVisualInfo(p.scr, TAG_DONE);
    if (p.dri == NULL || p.vi == NULL) {
        printf("EdgeSnapPrefs: the screen would not tell its look\n");
        goto out;
    }
    p.ta.ta_Name = (STRPTR)p.scr->Font->ta_Name;
    p.ta.ta_YSize = p.scr->Font->ta_YSize;
    p.ta.ta_Style = 0;
    p.ta.ta_Flags = 0;
    p.fh = p.scr->Font->ta_YSize;
    p.rowh = p.fh + 7;
    p.gadh = p.fh + 6;
    p.box_w = 26;                  /* GadTools' checkbox, for topaz 8  */
    p.box_h = 11;
    /* Before V39 the checkbox image has a fixed size, whatever the
     * font: keep its layout at that size as well as omitting the tag. */
    if (p.fh > 8 && GadToolsBase->lib_Version >= 39) {
        p.box_w = 26 * p.fh / 8;
        p.box_h = 11 * p.fh / 8;
    }
    /* a window with a title bar has these borders */
    p.bl = p.scr->WBorLeft;
    p.bt = p.scr->WBorTop + p.scr->Font->ta_YSize + 1;

    es_size(&p);
    if (!es_build(&p)) {
        printf("EdgeSnapPrefs: the gadgets would not build\n");
        goto out;
    }
    rc = es_run(&p);

out:
    if (p.glist != NULL) {
        FreeGadgets(p.glist);
    }
    if (p.vi != NULL) {
        FreeVisualInfo(p.vi);
    }
    if (p.dri != NULL) {
        FreeScreenDrawInfo(p.scr, p.dri);
    }
    if (p.scr != NULL) {
        UnlockPubScreen(NULL, p.scr);
    }
    if (GadToolsBase != NULL) {
        CloseLibrary(GadToolsBase);
    }
    if (GfxBase != NULL) {
        CloseLibrary((struct Library *)GfxBase);
    }
    if (IntuitionBase != NULL) {
        CloseLibrary((struct Library *)IntuitionBase);
    }
    return rc;
}
