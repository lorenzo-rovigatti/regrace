/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 *
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 *
 * Copyright (c) 1991-1995 Paul J Turner, Portland, OR
 * Copyright (c) 1996-2000 Grace Development Team
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 */

/*
 *
 * FLTK implementation of the widget API of motifinc.h
 * (the toolkit-specific half of motifutils.c).
 *
 * A "Widget" is a node of a tree (struct _GraceWidget) that mirrors the
 * Motif widget hierarchy the dialogs build: shells, forms, row-columns,
 * frames, grids, tabs, and leaves wrapping one FLTK widget. Motif lays
 * dialogs out automatically from the natural sizes of the widgets; the
 * tree is used to do the same here (measure() and place() below), so
 * the dialog code written for Motif works unchanged.
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

#include <string>
#include <vector>
#include <algorithm>
#include <map>

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_File_Browser.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Bitmap.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <FL/filename.H>

extern "C" {
#include "defines.h"
#include "globals.h"
#include "draw.h"
#include "graphs.h"
#include "utils.h"
#include "parser.h"
#include "protos.h"
#include "guiutils.h"
}

#include "fl_gui.h"
#include "fl_widgets.h"

/* ------------------------------------------------------------------ */
/* the widget tree */

enum GKind {
    GK_SHELL,       /* top-level window */
    GK_FORM,        /* dialog form: children stacked, last one stretches */
    GK_VBOX,        /* vertical row-column */
    GK_HBOX,        /* horizontal row-column */
    GK_FRAME,       /* titled frame */
    GK_GRID,        /* grid of equal cells */
    GK_RCGRID,      /* row-column packed in equal cells (row-major) */
    GK_TABS,        /* tab widget */
    GK_TABPAGE,     /* a page of it */
    GK_SCROLL,      /* scrolled window */
    GK_LEAF,        /* one FLTK widget */
    GK_MENU,        /* pulldown pane (submenu) */
    GK_POPUP,       /* popup menu */
    GK_MENUITEM,    /* menu push button */
    GK_MENUTOGGLE,  /* menu toggle */
    GK_MENULABEL,   /* menu label (inactive item) */
    GK_MENUSEP,     /* menu separator */
    GK_CHOICEITEM   /* an entry of an option menu */
};

enum LeafType {
    LT_LABEL, LT_BUTTON, LT_TOGGLE, LT_TEXT, LT_SCROLLTEXT, LT_CHOICE,
    LT_LIST, LT_SCALE, LT_SEPARATOR, LT_ARROWS, LT_MENUBAR, LT_FILEBROWSER,
    LT_CUSTOM
};

struct ButtonCB {
    Button_CBProc proc;
    void *data;
};

struct ToggleCB {
    TB_CBProc proc;
    void *data;
};

struct _GraceWidget {
    GKind kind;
    int ltype;
    _GraceWidget *parent;
    std::vector<_GraceWidget *> children;

    Fl_Widget *w;

    bool managed;
    bool sensitive;

    /* layout */
    int cols;               /* text width in characters */
    int rows;               /* visible lines */
    int min_w;              /* forced minimum width */
    int fixed_h;            /* scrolled window height */
    bool equal;             /* equal-width horizontal box */
    int ncols, nrows;       /* grid */
    int gcol, grow;         /* position in the grid */
    int form_order;         /* order of AddDialogFormChild(), -1 = none */
    bool form_bottom;       /* stuck to the bottom of a form */
    int form_counter;       /* for forms: next order */
    int custom_w, custom_h; /* custom leaves: natural size */
    bool custom_hexp, custom_vexp;

    /* menus */
    std::string label;
    char mnemonic;
    bool value;             /* toggles */
    std::string disp_label; /* label shown, with the mnemonic */
    std::vector<Fl_Menu_Item> items;    /* menubar/popup: generated menu */
    std::vector<std::string> item_labels; /* choices: storage of the labels */
    bool menu_dirty;
    int index;              /* choice items: position in the choice */

    /* shells */
    bool layout_pending;
    bool shown_once;

    /* callbacks */
    std::vector<ButtonCB> button_cbs;
    std::vector<ToggleCB> toggle_cbs;
    std::vector<ButtonCB> text_cbs;

    char *help;

    void *owner;            /* OptionStructure, ListStructure, ... */
    _GraceWidget *popup;    /* list popup menu */
    void (*popup_proc)(_GraceWidget *list);
    void (*default_proc)(_GraceWidget *list, int item);

    _GraceWidget(GKind k, _GraceWidget *p) : kind(k), ltype(-1), parent(p),
        w(NULL), managed(true), sensitive(true), cols(0), rows(0), min_w(0),
        fixed_h(0), equal(false), ncols(1), nrows(1), gcol(0), grow(0),
        form_order(-1), form_bottom(false), form_counter(0), custom_w(0),
        custom_h(0), custom_hexp(false), custom_vexp(false), mnemonic('\0'),
        value(false), menu_dirty(true), index(0), layout_pending(false),
        shown_once(false), help(NULL), owner(NULL), popup(NULL),
        popup_proc(NULL), default_proc(NULL) {
        if (p) {
            p->children.push_back(this);
        }
    }
};

typedef struct _GraceWidget GW;

Widget app_shell = NULL;

/* FLTK widget -> node, for help lookups */
static std::map<Fl_Widget *, _GraceWidget *> widget_map;

static void register_widget(Fl_Widget *w, _GraceWidget *g)
{
    widget_map[w] = g;
}

static GW *shell_of(GW *g)
{
    while (g && g->kind != GK_SHELL) {
        g = g->parent;
    }
    return g;
}

static bool is_menu_kind(GW *g)
{
    switch (g->kind) {
    case GK_MENU:
    case GK_POPUP:
    case GK_MENUITEM:
    case GK_MENUTOGGLE:
    case GK_MENULABEL:
    case GK_MENUSEP:
    case GK_CHOICEITEM:
        return true;
    default:
        return false;
    }
}

/* the FLTK group new FLTK widgets of children of g go into */
static Fl_Group *fl_group_for(GW *g)
{
    for (; g; g = g->parent) {
        switch (g->kind) {
        case GK_SHELL:
        case GK_FRAME:
        case GK_TABS:
        case GK_TABPAGE:
        case GK_SCROLL:
            return (Fl_Group *) g->w;
        default:
            break;
        }
    }
    return NULL;
}

/* labels of widgets: '&' is not a shortcut marker in Motif */
static void set_label(Fl_Widget *w, const char *s)
{
    std::string t;
    for (; s && *s; s++) {
        if (*s == '&') {
            t += '&';
        }
        t += *s;
    }
    w->copy_label(t.c_str());
}

static void begin_in(GW *parent)
{
    Fl_Group::current(fl_group_for(parent));
}

static void end_in(void)
{
    Fl_Group::current(NULL);
}

/* ------------------------------------------------------------------ */
/* dimensions */

#define SP  3       /* spacing between children */
#define MG  3       /* margin of containers */
#define FB  6       /* frame border */

static int row_h(void)
{
    return FL_NORMAL_SIZE + 10;
}

static int text_h(void)
{
    return FL_NORMAL_SIZE + 4;
}

static int label_width(Fl_Widget *w, const char *s)
{
    int lw = 0, lh = 0;
    if (s == NULL || *s == '\0') {
        return 0;
    }
    fl_font(w->labelfont(), w->labelsize());
    fl_measure(s, lw, lh, 0);
    return lw;
}

static int char_width(void)
{
    fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
    return (int) ceil(fl_width("0"));
}

/* ------------------------------------------------------------------ */
/* custom menu labels: color swatches and bitmaps */

static Fl_Labeltype LT_SWATCH = (Fl_Labeltype) (FL_FREE_LABELTYPE);
static Fl_Labeltype LT_BITMAP = (Fl_Labeltype) (FL_FREE_LABELTYPE + 1);
static std::vector<Fl_Bitmap *> bitmap_registry;

/* swatch labels are "\033RRGGBBname" */
static void swatch_draw(const Fl_Label *l, int X, int Y, int W, int H,
    Fl_Align a)
{
    unsigned int rgb = 0;
    const char *s = l->value;
    int sz = H - 6;
    if (s && s[0] == '\033' && strlen(s) >= 7) {
        char hex[7];
        memcpy(hex, s + 1, 6);
        hex[6] = '\0';
        rgb = strtoul(hex, NULL, 16);
        s += 7;
    }
    if (sz > 16) {
        sz = 16;
    }
    fl_color(fl_rgb_color((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff));
    fl_rectf(X + 3, Y + (H - sz)/2, sz, sz);
    fl_color(FL_FOREGROUND_COLOR);
    fl_rect(X + 3, Y + (H - sz)/2, sz, sz);
    fl_font(l->font, l->size);
    fl_color((Fl_Color) l->color);
    fl_draw(s, X + sz + 8, Y, W - sz - 8, H,
        (Fl_Align) (FL_ALIGN_LEFT | FL_ALIGN_CLIP));
}

static void swatch_measure(const Fl_Label *l, int &W, int &H)
{
    const char *s = l->value;
    if (s && s[0] == '\033' && strlen(s) >= 7) {
        s += 7;
    }
    fl_font(l->font, l->size);
    W = (int) fl_width(s) + l->size + 14;
    H = l->size + 4;
}

/* bitmap labels are "\034<index in bitmap_registry>" */
static void bitmap_draw(const Fl_Label *l, int X, int Y, int W, int H,
    Fl_Align a)
{
    const char *s = l->value;
    if (s && s[0] == '\034') {
        size_t i = atoi(s + 1);
        if (i < bitmap_registry.size()) {
            Fl_Bitmap *bm = bitmap_registry[i];
            fl_color((Fl_Color) l->color);
            bm->draw(X + 3, Y + (H - bm->h())/2);
        }
    }
}

static void bitmap_measure(const Fl_Label *l, int &W, int &H)
{
    const char *s = l->value;
    W = H = 0;
    if (s && s[0] == '\034') {
        size_t i = atoi(s + 1);
        if (i < bitmap_registry.size()) {
            W = bitmap_registry[i]->w() + 6;
            H = bitmap_registry[i]->h() + 2;
        }
    }
}

static void init_labeltypes(void)
{
    static bool done = false;
    if (!done) {
        Fl::set_labeltype(LT_SWATCH, swatch_draw, swatch_measure);
        Fl::set_labeltype(LT_BITMAP, bitmap_draw, bitmap_measure);
        done = true;
    }
}

/* ------------------------------------------------------------------ */
/* layout */

static void measure(GW *g, int &w, int &h);
static void place(GW *g, int x, int y, int w, int h);
static void apply_visibility(GW *g, bool vis);
static void schedule_layout(GW *g);
static void rebuild_menubar(GW *mb);

static bool lays_out(GW *g)
{
    return g->managed && !is_menu_kind(g);
}

static void layout_children(GW *g, std::vector<GW *> &out)
{
    out.clear();
    for (size_t i = 0; i < g->children.size(); i++) {
        if (lays_out(g->children[i])) {
            out.push_back(g->children[i]);
        }
    }
}

static bool hexpand(GW *g)
{
    if (g->kind == GK_LEAF && g->ltype == LT_CUSTOM) {
        return g->custom_hexp;
    }
    if (g->kind == GK_LEAF) {
        switch (g->ltype) {
        case LT_TEXT:
        case LT_SCROLLTEXT:
        case LT_LIST:
        case LT_SCALE:
        case LT_FILEBROWSER:
        case LT_MENUBAR:
            return true;
        default:
            return false;
        }
    } else {
        std::vector<GW *> ch;
        layout_children(g, ch);
        for (size_t i = 0; i < ch.size(); i++) {
            if (hexpand(ch[i])) {
                return true;
            }
        }
        return false;
    }
}

static bool vexpand(GW *g)
{
    if (g->kind == GK_LEAF && g->ltype == LT_CUSTOM) {
        return g->custom_vexp;
    }
    if (g->kind == GK_LEAF) {
        switch (g->ltype) {
        case LT_SCROLLTEXT:
        case LT_LIST:
        case LT_FILEBROWSER:
            return true;
        default:
            return false;
        }
    } else if (g->kind == GK_SCROLL) {
        return true;
    } else {
        std::vector<GW *> ch;
        layout_children(g, ch);
        for (size_t i = 0; i < ch.size(); i++) {
            if (vexpand(ch[i])) {
                return true;
            }
        }
        return false;
    }
}

static void leaf_size(GW *g, int &w, int &h)
{
    Fl_Widget *fw = g->w;
    int rh = row_h();

    switch (g->ltype) {
    case LT_LABEL:
        {
            int lw = 0, lh = 0;
            fl_font(fw->labelfont(), fw->labelsize());
            if (fw->label() && fw->label()[0]) {
                fl_measure(fw->label(), lw, lh, 0);
            }
            w = lw + 8;
            h = MAX2(lh + 6, rh);
        }
        break;
    case LT_BUTTON:
        if (fw->image()) {
            w = fw->image()->w() + 10;
            h = MAX2(fw->image()->h() + 8, rh);
        } else {
            w = MAX2(label_width(fw, fw->label()) + 20, 50);
            h = rh + 2;
        }
        break;
    case LT_TOGGLE:
        w = label_width(fw, fw->label()) + FL_NORMAL_SIZE + 16;
        h = rh;
        break;
    case LT_TEXT:
        w = MAX2(g->cols, 1)*char_width() + 10;
        h = rh;
        break;
    case LT_SCROLLTEXT:
        w = (g->cols > 0 ? g->cols:40)*char_width() + 24;
        h = MAX2(g->rows, 2)*text_h() + 10;
        break;
    case LT_CHOICE:
        {
            Fl_Choice *ch = (Fl_Choice *) fw;
            int i, mw = 30;
            for (i = 0; i < ch->size() - 1; i++) {
                const Fl_Menu_Item *it = &ch->menu()[i];
                if (it->label()) {
                    int iw = it->measure(NULL, ch);
                    mw = MAX2(mw, iw);
                }
            }
            w = mw + 34;
            h = rh;
        }
        break;
    case LT_LIST:
        w = 180;
        h = MAX2(g->rows, 1)*text_h() + 8;
        break;
    case LT_FILEBROWSER:
        w = 380;
        h = 12*text_h();
        break;
    case LT_SCALE:
        w = 200;
        h = rh + text_h();
        break;
    case LT_SEPARATOR:
        if (g->parent && g->parent->kind == GK_HBOX) {
            w = 6;
            h = 2;
        } else {
            w = 2;
            h = 6;
        }
        break;
    case LT_ARROWS:
        w = rh - 4;
        h = rh;
        break;
    case LT_CUSTOM:
        w = g->custom_w;
        h = g->custom_h;
        break;
    case LT_MENUBAR:
        {
            Fl_Menu_Bar *mb = (Fl_Menu_Bar *) fw;
            const Fl_Menu_Item *m = mb->menu();
            w = 10;
            if (m) {
                for (; m->text; m = m->next()) {
                    w += m->measure(NULL, mb) + 16;
                }
            }
            h = rh + 2;
        }
        break;
    default:
        w = 50;
        h = rh;
        break;
    }
    w = MAX2(w, g->min_w);
}

static void vbox_measure(std::vector<GW *> &ch, int &w, int &h)
{
    int i, cw, chh;
    w = 0;
    h = 0;
    for (i = 0; i < (int) ch.size(); i++) {
        measure(ch[i], cw, chh);
        w = MAX2(w, cw);
        h += chh;
        if (i) {
            h += SP;
        }
    }
}

static void form_children(GW *g, std::vector<GW *> &ordered,
    std::vector<GW *> &bottom)
{
    std::vector<GW *> ch, added, rest;
    size_t i;
    layout_children(g, ch);
    for (i = 0; i < ch.size(); i++) {
        if (ch[i]->form_bottom) {
            bottom.push_back(ch[i]);
        } else if (ch[i]->form_order >= 0) {
            added.push_back(ch[i]);
        } else {
            rest.push_back(ch[i]);
        }
    }
    std::stable_sort(added.begin(), added.end(),
        [](GW *a, GW *b) { return a->form_order < b->form_order; });
    ordered = rest;
    ordered.insert(ordered.end(), added.begin(), added.end());
}

static void measure(GW *g, int &w, int &h)
{
    std::vector<GW *> ch;
    int i, cw, chh;

    w = h = 0;

    switch (g->kind) {
    case GK_LEAF:
        leaf_size(g, w, h);
        return;
    case GK_SHELL:
    case GK_VBOX:
    case GK_TABPAGE:
        layout_children(g, ch);
        vbox_measure(ch, w, h);
        w += 2*MG;
        h += 2*MG;
        break;
    case GK_FORM:
        {
            std::vector<GW *> ordered, bottom;
            form_children(g, ordered, bottom);
            ordered.insert(ordered.end(), bottom.begin(), bottom.end());
            vbox_measure(ordered, w, h);
            w += 2*MG;
            h += 2*MG;
        }
        break;
    case GK_HBOX:
        layout_children(g, ch);
        for (i = 0; i < (int) ch.size(); i++) {
            measure(ch[i], cw, chh);
            if (g->equal) {
                w = MAX2(w, cw);
            } else {
                w += cw;
            }
            h = MAX2(h, chh);
        }
        if (g->equal) {
            w *= ch.size();
        }
        if (ch.size() > 1) {
            w += SP*(ch.size() - 1);
        }
        w += 2*MG;
        h += 2*MG;
        break;
    case GK_FRAME:
        layout_children(g, ch);
        vbox_measure(ch, w, h);
        {
            int th = (g->w->label() && g->w->label()[0]) ? text_h():FB;
            int lw = label_width(g->w, g->w->label()) + 4*FB;
            w = MAX2(w + 2*FB, lw);
            h += th + FB;
        }
        break;
    case GK_GRID:
        {
            int cellw = 0, cellh = 0;
            layout_children(g, ch);
            for (i = 0; i < (int) ch.size(); i++) {
                measure(ch[i], cw, chh);
                cellw = MAX2(cellw, cw);
                cellh = MAX2(cellh, chh);
            }
            w = g->ncols*cellw + (g->ncols - 1)*SP;
            h = g->nrows*cellh + (g->nrows - 1)*SP;
        }
        break;
    case GK_RCGRID:
        {
            int cellw = 0, cellh = 0, ncols;
            layout_children(g, ch);
            ncols = (ch.size() + g->nrows - 1)/g->nrows;
            for (i = 0; i < (int) ch.size(); i++) {
                measure(ch[i], cw, chh);
                cellw = MAX2(cellw, cw);
                cellh = MAX2(cellh, chh);
            }
            w = ncols*cellw + MAX2(ncols - 1, 0)*SP + 2*MG;
            h = g->nrows*cellh + (g->nrows - 1)*SP + 2*MG;
        }
        break;
    case GK_TABS:
        layout_children(g, ch);
        for (i = 0; i < (int) ch.size(); i++) {
            measure(ch[i], cw, chh);
            w = MAX2(w, cw);
            h = MAX2(h, chh);
        }
        {
            /* room for the tab labels */
            int lw = 0;
            for (i = 0; i < (int) ch.size(); i++) {
                lw += label_width(ch[i]->w, ch[i]->w->label()) + 24;
            }
            w = MAX2(w, lw) + 4;
        }
        h += row_h() + 6;
        break;
    case GK_SCROLL:
        layout_children(g, ch);
        vbox_measure(ch, w, h);
        w += Fl::scrollbar_size() + 6;
        if (g->fixed_h > 0) {
            h = g->fixed_h;
        }
        h += 4;
        break;
    default:
        break;
    }
}

static void vbox_place(std::vector<GW *> &ch, int x, int y, int w, int h,
    GW *stretch)
{
    int i, n = ch.size(), tot = 0, nexp = 0, extra;
    std::vector<int> ph(n);
    std::vector<bool> exp(n);

    for (i = 0; i < n; i++) {
        int cw;
        measure(ch[i], cw, ph[i]);
        tot += ph[i];
        if (stretch) {
            exp[i] = (ch[i] == stretch);
        } else {
            exp[i] = vexpand(ch[i]);
        }
        if (exp[i]) {
            nexp++;
        }
    }
    if (n > 1) {
        tot += SP*(n - 1);
    }
    extra = h - tot;
    if (extra < 0) {
        extra = 0;
    }
    for (i = 0; i < n; i++) {
        int hh = ph[i];
        if (exp[i] && nexp > 0) {
            int add = extra/nexp;
            hh += add;
            extra -= add;
            nexp--;
        }
        place(ch[i], x, y, w, hh);
        y += hh + SP;
    }
}

static bool is_container(GW *g)
{
    return g->kind != GK_LEAF;
}

static void place(GW *g, int x, int y, int w, int h)
{
    std::vector<GW *> ch;
    int i;

    switch (g->kind) {
    case GK_LEAF:
        if (g->ltype == LT_SCALE) {
            int th = text_h();
            g->w->resize(x, y + th, w, h - th);
        } else {
            g->w->resize(x, y, w, h);
        }
        return;
    case GK_SHELL:
    case GK_VBOX:
        layout_children(g, ch);
        vbox_place(ch, x + MG, y + MG, w - 2*MG, h - 2*MG, NULL);
        break;
    case GK_TABPAGE:
        g->w->resize(x, y, w, h);
        layout_children(g, ch);
        vbox_place(ch, x + MG, y + MG, w - 2*MG, h - 2*MG, NULL);
        break;
    case GK_FORM:
        {
            std::vector<GW *> ordered, bottom;
            GW *stretch = NULL;
            int bh = 0, cw, chh;
            form_children(g, ordered, bottom);
            for (i = 0; i < (int) ordered.size(); i++) {
                if (ordered[i]->form_order >= 0) {
                    stretch = ordered[i];
                }
            }
            for (i = 0; i < (int) bottom.size(); i++) {
                measure(bottom[i], cw, chh);
                bh += chh + SP;
            }
            vbox_place(ordered, x + MG, y + MG, w - 2*MG, h - 2*MG - bh,
                stretch);
            {
                int by = y + h - MG - bh + SP;
                for (i = 0; i < (int) bottom.size(); i++) {
                    measure(bottom[i], cw, chh);
                    place(bottom[i], x + MG, by, w - 2*MG, chh);
                    by += chh + SP;
                }
            }
        }
        break;
    case GK_HBOX:
        {
            int n, tot = 0, nexp = 0, extra, cx;
            layout_children(g, ch);
            n = ch.size();
            std::vector<int> pw(n), ph(n);
            std::vector<bool> exp(n);
            for (i = 0; i < n; i++) {
                measure(ch[i], pw[i], ph[i]);
                exp[i] = hexpand(ch[i]);
                if (exp[i]) {
                    nexp++;
                }
                tot += pw[i];
            }
            if (n > 1) {
                tot += SP*(n - 1);
            }
            extra = w - 2*MG - tot;
            if (extra < 0) {
                extra = 0;
            }
            cx = x + MG;
            for (i = 0; i < n; i++) {
                int ww = pw[i], hh, yy;
                if (g->equal) {
                    ww = (w - 2*MG - SP*(n - 1))/n;
                } else if (exp[i] && nexp > 0) {
                    int add = extra/nexp;
                    ww += add;
                    extra -= add;
                    nexp--;
                }
                if (is_container(ch[i]) || vexpand(ch[i])) {
                    hh = h - 2*MG;
                    yy = y + MG;
                } else {
                    hh = MIN2(ph[i], h - 2*MG);
                    yy = y + MG + (h - 2*MG - hh)/2;
                }
                place(ch[i], cx, yy, ww, hh);
                cx += ww + SP;
            }
        }
        break;
    case GK_FRAME:
        {
            int th = (g->w->label() && g->w->label()[0]) ? text_h():FB;
            g->w->resize(x, y, w, h);
            layout_children(g, ch);
            vbox_place(ch, x + FB, y + th, w - 2*FB, h - th - FB, NULL);
        }
        break;
    case GK_GRID:
        {
            int cellw, cellh;
            layout_children(g, ch);
            cellw = (w - (g->ncols - 1)*SP)/MAX2(g->ncols, 1);
            cellh = (h - (g->nrows - 1)*SP)/MAX2(g->nrows, 1);
            for (i = 0; i < (int) ch.size(); i++) {
                place(ch[i], x + ch[i]->gcol*(cellw + SP),
                    y + ch[i]->grow*(cellh + SP), cellw, cellh);
            }
        }
        break;
    case GK_RCGRID:
        {
            int cellw, cellh, ncols;
            layout_children(g, ch);
            ncols = MAX2((int) (ch.size() + g->nrows - 1)/g->nrows, 1);
            cellw = (w - 2*MG - (ncols - 1)*SP)/ncols;
            cellh = (h - 2*MG - (g->nrows - 1)*SP)/g->nrows;
            for (i = 0; i < (int) ch.size(); i++) {
                int col = i % ncols, row = i/ncols;
                place(ch[i], x + MG + col*(cellw + SP),
                    y + MG + row*(cellh + SP), cellw, cellh);
            }
        }
        break;
    case GK_TABS:
        {
            int th = row_h() + 6;
            g->w->resize(x, y, w, h);
            layout_children(g, ch);
            for (i = 0; i < (int) ch.size(); i++) {
                place(ch[i], x, y + th, w, h - th);
            }
        }
        break;
    case GK_SCROLL:
        {
            Fl_Scroll *sc = (Fl_Scroll *) g->w;
            int cw, chh, sb = Fl::scrollbar_size();
            sc->scroll_to(0, 0);
            sc->resize(x, y, w, h);
            layout_children(g, ch);
            vbox_measure(ch, cw, chh);
            vbox_place(ch, x + 2, y + 2, w - sb - 6, chh, NULL);
            sc->redraw();
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* shells (dialog windows) */

class GWindow : public Fl_Double_Window {
public:
    GW *gw;
    bool in_layout;
    GWindow(int W, int H, GW *g) : Fl_Double_Window(W, H), gw(g),
        in_layout(false) {
        xclass("XMgrace");
    }
    void resize(int X, int Y, int W, int H) {
        Fl_Double_Window::resize(X, Y, W, H);
        if (gw && !in_layout) {
            in_layout = true;
            place(gw, 0, 0, w(), h());
            in_layout = false;
        }
    }
};

static void shell_close_cb(Fl_Widget *w, void *)
{
    w->hide();
}

static void flush_menubars(GW *g)
{
    if (g->kind == GK_LEAF && g->ltype == LT_MENUBAR && g->menu_dirty) {
        rebuild_menubar(g);
    }
    for (size_t i = 0; i < g->children.size(); i++) {
        if (!is_menu_kind(g->children[i])) {
            flush_menubars(g->children[i]);
        }
    }
}

static void layout_shell(GW *s, bool initial)
{
    GWindow *win = (GWindow *) s->w;
    int pw, ph, W, H;

    if (s == app_shell || win == NULL) {
        return;
    }

    flush_menubars(s);
    measure(s, pw, ph);
    if (initial && !s->shown_once) {
        W = pw;
        H = ph;
    } else {
        W = MAX2(win->w(), pw);
        H = MAX2(win->h(), ph);
    }
    win->in_layout = true;
    if (W != win->w() || H != win->h()) {
        win->size(W, H);
    }
    win->size_range(pw, ph);
    place(s, 0, 0, win->w(), win->h());
    win->in_layout = false;
    win->redraw();
}

static void layout_timeout_cb(void *data)
{
    GW *s = (GW *) data;
    s->layout_pending = false;
    if (s->w && s->w->visible()) {
        layout_shell(s, false);
    }
}

static void schedule_layout(GW *g)
{
    GW *s = shell_of(g);
    if (s == NULL || s == app_shell || s->w == NULL) {
        return;
    }
    if (!s->layout_pending) {
        s->layout_pending = true;
        Fl::add_timeout(0.0, layout_timeout_cb, s);
    }
}

static void show_shell(GW *s)
{
    if (s == NULL || s->w == NULL) {
        return;
    }
    if (s == app_shell) {
        s->w->show();
        return;
    }
    for (size_t i = 0; i < s->children.size(); i++) {
        s->children[i]->managed = true;
        apply_visibility(s->children[i], true);
    }
    if (!s->w->visible()) {
        layout_shell(s, true);
        s->shown_once = true;
    }
    ((Fl_Window *) s->w)->show();
}

static void hide_shell(GW *s)
{
    if (s && s->w && s != app_shell) {
        s->w->hide();
    }
}

void fl_register_app_shell(Fl_Window *win)
{
    if (app_shell == NULL) {
        app_shell = new GW(GK_SHELL, NULL);
    }
    app_shell->w = win;
}

/* ------------------------------------------------------------------ */
/* visibility */

static void apply_visibility(GW *g, bool vis)
{
    bool me = vis && g->managed;

    if (is_menu_kind(g) || g->kind == GK_SHELL) {
        return;
    }
    if (g->w && g->kind != GK_TABPAGE) {
        if (me) {
            g->w->show();
        } else {
            g->w->hide();
        }
    }
    for (size_t i = 0; i < g->children.size(); i++) {
        apply_visibility(g->children[i], me);
    }
}

static bool parent_visible(GW *g)
{
    for (GW *p = g->parent; p; p = p->parent) {
        if (p->kind == GK_SHELL) {
            return true;
        }
        if (!p->managed) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* menus */

static void rebuild_menubar(GW *mb);
static void build_items(GW *menu, std::vector<Fl_Menu_Item> &out);

static void menu_item_activate(GW *it, int onoff_known, int onoff)
{
    size_t i;
    if (it->kind == GK_MENUTOGGLE) {
        it->value = onoff_known ? (onoff != 0) : !it->value;
        for (i = 0; i < it->toggle_cbs.size(); i++) {
            it->toggle_cbs[i].proc(it->value ? TRUE:FALSE,
                it->toggle_cbs[i].data);
        }
    } else {
        for (i = 0; i < it->button_cbs.size(); i++) {
            it->button_cbs[i].proc(it->button_cbs[i].data);
        }
    }
}

static void menubar_item_cb(Fl_Widget *w, void *data)
{
    GW *it = (GW *) data;
    Fl_Menu_ *m = (Fl_Menu_ *) w;
    const Fl_Menu_Item *mi = m->mvalue();
    if (it->kind == GK_MENUTOGGLE && mi) {
        menu_item_activate(it, TRUE, mi->value());
    } else {
        menu_item_activate(it, FALSE, 0);
    }
}

static std::string mnemonic_label(const std::string &label, char mn)
{
    std::string s;
    bool done = (mn == '\0');
    for (size_t i = 0; i < label.size(); i++) {
        if (label[i] == '&') {
            s += '&';
        }
        if (!done && label[i] == mn) {
            s += '&';
            done = true;
        }
        s += label[i];
    }
    return s;
}

static Fl_Menu_Item blank_item(void)
{
    Fl_Menu_Item it;
    memset(&it, 0, sizeof(it));
    return it;
}

static void build_items(GW *menu, std::vector<Fl_Menu_Item> &out)
{
    int last = -1;
    for (size_t i = 0; i < menu->children.size(); i++) {
        GW *c = menu->children[i];
        Fl_Menu_Item it = blank_item();

        if (!c->managed) {
            continue;
        }
        if (c->kind == GK_MENUSEP) {
            /* flag the previous item of this level */
            if (last >= 0) {
                out[last].flags |= FL_MENU_DIVIDER;
            }
            continue;
        }
        c->disp_label = mnemonic_label(c->label, c->mnemonic);
        it.text = c->disp_label.c_str();
        it.labeltype_ = FL_NORMAL_LABEL;
        it.user_data_ = c;
        if (!c->sensitive) {
            it.flags |= FL_MENU_INACTIVE;
        }
        switch (c->kind) {
        case GK_MENU:
            it.flags |= FL_SUBMENU;
            last = out.size();
            out.push_back(it);
            build_items(c, out);
            out.push_back(blank_item());
            break;
        case GK_MENUITEM:
            it.callback_ = menubar_item_cb;
            last = out.size();
            out.push_back(it);
            break;
        case GK_MENUTOGGLE:
            it.callback_ = menubar_item_cb;
            it.flags |= FL_MENU_TOGGLE;
            if (c->value) {
                it.flags |= FL_MENU_VALUE;
            }
            last = out.size();
            out.push_back(it);
            break;
        case GK_MENULABEL:
            it.flags |= FL_MENU_INACTIVE;
            last = out.size();
            out.push_back(it);
            break;
        default:
            break;
        }
    }
}

static void rebuild_menubar(GW *mb)
{
    Fl_Menu_Bar *bar = (Fl_Menu_Bar *) mb->w;
    std::vector<Fl_Menu_Item> fresh;

    build_items(mb, fresh);
    fresh.push_back(blank_item());
    mb->items.swap(fresh);
    bar->menu(&mb->items[0]);
    bar->redraw();
    mb->menu_dirty = false;
}

static GW *menu_root(GW *g)
{
    while (g && g->kind != GK_LEAF && g->kind != GK_POPUP) {
        g = g->parent;
    }
    return g;
}

static void menubar_timeout_cb(void *data)
{
    GW *mb = (GW *) data;
    if (mb->menu_dirty) {
        rebuild_menubar(mb);
        schedule_layout(mb);
    }
}

/* menu structure or labels changed */
static void menu_changed(GW *g)
{
    GW *root = menu_root(g);
    if (root && root->kind == GK_LEAF && root->ltype == LT_MENUBAR) {
        if (!root->menu_dirty) {
            root->menu_dirty = true;
            Fl::add_timeout(0.0, menubar_timeout_cb, root);
        }
    }
}

static void show_popup(GW *popup, Fl_Widget *w)
{
    std::vector<Fl_Menu_Item> items;
    const Fl_Menu_Item *m;

    build_items(popup, items);
    items.push_back(blank_item());
    if (items.size() <= 1) {
        return;
    }
    m = items[0].popup(Fl::event_x(), Fl::event_y());
    if (m && m->user_data()) {
        GW *it = (GW *) m->user_data();
        if (it->kind == GK_MENUITEM || it->kind == GK_MENUTOGGLE) {
            menu_item_activate(it, FALSE, 0);
        }
    }
}

/* ------------------------------------------------------------------ */
/* basic API */

void ManageChild(Widget w)
{
    if (w == NULL) {
        return;
    }
    if (w->kind == GK_SHELL) {
        show_shell(w);
        return;
    }
    if (w->parent && w->parent->kind == GK_SHELL) {
        w->managed = true;
        show_shell(w->parent);
        return;
    }
    if (w->managed) {
        return;
    }
    w->managed = true;
    if (is_menu_kind(w)) {
        menu_changed(w);
        return;
    }
    apply_visibility(w, parent_visible(w));
    schedule_layout(w);
}

void UnmanageChild(Widget w)
{
    if (w == NULL) {
        return;
    }
    if (w->kind == GK_SHELL) {
        hide_shell(w);
        return;
    }
    if (w->parent && w->parent->kind == GK_SHELL) {
        hide_shell(w->parent);
        return;
    }
    if (!w->managed) {
        return;
    }
    w->managed = false;
    if (is_menu_kind(w)) {
        menu_changed(w);
        return;
    }
    apply_visibility(w, false);
    schedule_layout(w);
}

int IsManaged(Widget w)
{
    if (w == NULL) {
        return FALSE;
    }
    if (w->kind == GK_SHELL) {
        return (w->w && w->w->visible()) ? TRUE:FALSE;
    }
    if (w->parent && w->parent->kind == GK_SHELL) {
        return (w->parent->w && w->parent->w->visible()) ? TRUE:FALSE;
    }
    return w->managed ? TRUE:FALSE;
}

static void set_sensitive_rec(GW *g, bool onoff)
{
    if (g->w) {
        if (onoff) {
            g->w->activate();
        } else {
            g->w->deactivate();
        }
    }
    /* layout-only containers have no FLTK widget: recurse */
    if (g->w == NULL || g->kind == GK_LEAF) {
        for (size_t i = 0; i < g->children.size(); i++) {
            if (!is_menu_kind(g->children[i])) {
                set_sensitive_rec(g->children[i], onoff);
            }
        }
    }
}

void SetSensitive(Widget w, int onoff)
{
    if (w == NULL) {
        return;
    }
    w->sensitive = onoff ? true:false;
    if (w->kind == GK_CHOICEITEM) {
        Fl_Choice *ch = (Fl_Choice *) w->parent->w;
        if (w->index < ch->size() - 1) {
            Fl_Menu_Item *mi = (Fl_Menu_Item *) &ch->menu()[w->index];
            if (onoff) {
                mi->activate();
            } else {
                mi->deactivate();
            }
        }
        return;
    }
    if (is_menu_kind(w)) {
        menu_changed(w);
        return;
    }
    set_sensitive_rec(w, w->sensitive);
}

Widget GetParent(Widget w)
{
    if (w) {
        return w->parent;
    } else {
        errmsg("Internal error: GetParent() called with NULL widget");
        return NULL;
    }
}

void RegisterEditRes(Widget shell)
{
}

void SetDimensions(Widget w, unsigned int width, unsigned int height)
{
    if (w && w->w) {
        w->w->size(width, height);
    }
}

void GetDimensions(Widget w, unsigned int *width, unsigned int *height)
{
    if (w && w->w) {
        *width = w->w->w();
        *height = w->w->h();
    } else {
        *width = *height = 0;
    }
}

/* ------------------------------------------------------------------ */
/* leaves */

static GW *new_leaf(GW *parent, int ltype, Fl_Widget *w)
{
    GW *g = new GW(GK_LEAF, parent);
    g->ltype = ltype;
    g->w = w;
    w->user_data(g);
    register_widget(w, g);
    schedule_layout(g);
    return g;
}

static void button_cb(Fl_Widget *w, void *data)
{
    GW *g = (GW *) data;
    for (size_t i = 0; i < g->button_cbs.size(); i++) {
        g->button_cbs[i].proc(g->button_cbs[i].data);
    }
}

static void toggle_cb(Fl_Widget *w, void *data)
{
    GW *g = (GW *) data;
    int onoff = ((Fl_Button *) w)->value();
    for (size_t i = 0; i < g->toggle_cbs.size(); i++) {
        g->toggle_cbs[i].proc(onoff, g->toggle_cbs[i].data);
    }
}

static void text_cb(Fl_Widget *w, void *data)
{
    GW *g = (GW *) data;
    for (size_t i = 0; i < g->text_cbs.size(); i++) {
        g->text_cbs[i].proc(g->text_cbs[i].data);
    }
}

Widget CreateLabel(Widget parent, char *s)
{
    Fl_Box *b;
    begin_in(parent);
    b = new Fl_Box(0, 0, 10, 10);
    end_in();
    set_label(b, s);
    if (parent && parent->kind == GK_RCGRID) {
        b->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    } else {
        b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    }
    return new_leaf(parent, LT_LABEL, b);
}

void AlignLabel(Widget w, int alignment)
{
    Fl_Align a;

    if (w == NULL || w->w == NULL) {
        return;
    }
    switch (alignment) {
    case ALIGN_BEGINNING:
        a = FL_ALIGN_LEFT;
        break;
    case ALIGN_CENTER:
        a = FL_ALIGN_CENTER;
        break;
    case ALIGN_END:
        a = FL_ALIGN_RIGHT;
        break;
    default:
        errmsg("Internal error in AlignLabel()");
        return;
    }
    if (w->ltype == LT_LABEL) {
        w->w->align(a | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    } else {
        w->w->align(a | FL_ALIGN_INSIDE);
    }
    w->w->redraw();
}

void SetLabel(Widget w, char *s)
{
    if (w == NULL) {
        return;
    }
    if (is_menu_kind(w)) {
        w->label = s ? s:"";
        menu_changed(w);
        return;
    }
    if (w->w) {
        set_label(w->w, s ? s:"");
        w->w->redraw();
        if (w->parent) {
            w->parent->w ? w->parent->w->redraw() : (void) 0;
        }
        schedule_layout(w);
    }
}

void SetFixedFont(Widget w)
{
    if (w == NULL || w->w == NULL) {
        return;
    }
    if (w->ltype == LT_TEXT) {
        ((Fl_Input *) w->w)->textfont(FL_COURIER);
    } else if (w->ltype == LT_SCROLLTEXT) {
        ((Fl_Text_Editor *) w->w)->textfont(FL_COURIER);
    } else if (w->ltype == LT_LIST) {
        ((Fl_Browser *) w->w)->textfont(FL_COURIER);
    } else {
        w->w->labelfont(FL_COURIER);
    }
}

Widget CreateButton(Widget parent, char *label)
{
    Fl_Button *b;
    GW *g;
    begin_in(parent);
    b = new Fl_Button(0, 0, 10, 10);
    end_in();
    set_label(b, label);
    g = new_leaf(parent, LT_BUTTON, b);
    b->callback(button_cb, g);
    return g;
}

Widget CreateBitmapButton(Widget parent,
    int width, int height, const unsigned char *bits)
{
    Fl_Button *b;
    GW *g;
    begin_in(parent);
    b = new Fl_Button(0, 0, 10, 10);
    end_in();
    b->image(new Fl_Bitmap(bits, width, height));
    g = new_leaf(parent, LT_BUTTON, b);
    b->callback(button_cb, g);
    return g;
}

void AddButtonCB(Widget button, Button_CBProc cbproc, void *data)
{
    ButtonCB cb = {cbproc, data};
    if (button == NULL) {
        return;
    }
    button->button_cbs.push_back(cb);
}

Widget CreateToggleButton(Widget parent, char *s)
{
    Fl_Check_Button *b;
    GW *g;
    begin_in(parent);
    b = new Fl_Check_Button(0, 0, 10, 10);
    end_in();
    set_label(b, s);
    g = new_leaf(parent, LT_TOGGLE, b);
    b->callback(toggle_cb, g);
    return g;
}

int GetToggleButtonState(Widget w)
{
    if (w == NULL) {
        return FALSE;
    }
    if (w->kind == GK_MENUTOGGLE) {
        return w->value ? TRUE:FALSE;
    }
    return ((Fl_Button *) w->w)->value() ? TRUE:FALSE;
}

void SetToggleButtonState(Widget w, int value)
{
    if (w == NULL) {
        return;
    }
    if (w->kind == GK_MENUTOGGLE) {
        GW *root = menu_root(w);
        w->value = value ? true:false;
        /* update the live menu without rebuilding it */
        if (root && root->kind == GK_LEAF && !root->menu_dirty) {
            for (size_t i = 0; i < root->items.size(); i++) {
                if (root->items[i].user_data_ == w) {
                    if (value) {
                        root->items[i].set();
                    } else {
                        root->items[i].clear();
                    }
                }
            }
        }
        return;
    }
    ((Fl_Button *) w->w)->value(value ? 1:0);
}

void AddToggleButtonCB(Widget w, TB_CBProc cbproc, void *anydata)
{
    ToggleCB cb = {cbproc, anydata};
    if (w == NULL) {
        return;
    }
    w->toggle_cbs.push_back(cb);
}

/* ------------------------------------------------------------------ */
/* containers */

Widget CreateDialogForm(Widget parent, char *s)
{
    GW *shell, *form;
    GWindow *win;
    char *bufp;

    shell = new GW(GK_SHELL, NULL);
    win = new GWindow(300, 200, shell);
    win->end();
    bufp = copy_string(NULL, "Grace: ");
    bufp = concat_strings(bufp, s);
    win->copy_label(bufp);
    xfree(bufp);
    win->callback(shell_close_cb);
    win->resizable(win);
    shell->w = win;
    shell->managed = false;
    register_widget(win, shell);

    form = new GW(GK_FORM, shell);
    form->managed = false;

    return form;
}

void SetDialogFormResizable(Widget form, int onoff)
{
}

void AddDialogFormChild(Widget form, Widget child)
{
    if (form == NULL || child == NULL) {
        return;
    }
    child->form_order = form->form_counter++;
    schedule_layout(form);
}

Widget CreateVContainer(Widget parent)
{
    GW *g = new GW(GK_VBOX, parent);
    schedule_layout(g);
    return g;
}

Widget CreateHContainer(Widget parent)
{
    GW *g = new GW(GK_HBOX, parent);
    schedule_layout(g);
    return g;
}

/* a frame with the title in the border, as in Motif */
class GFrame : public Fl_Group {
public:
    GFrame() : Fl_Group(0, 0, 10, 10) {
        box(FL_NO_BOX);
        align(FL_ALIGN_TOP_LEFT | FL_ALIGN_INSIDE);
    }
    void draw() {
        int th = (label() && label()[0]) ? text_h():FB;
        int y0 = y() + th/2;
        fl_draw_box(FL_ENGRAVED_FRAME, x(), y0, w(), h() - th/2, color());
        if (label() && label()[0]) {
            int lw = label_width(this, label());
            fl_color(color());
            fl_rectf(x() + 8, y(), lw + 6, th);
            fl_font(labelfont(), labelsize());
            fl_color(active_r() ? labelcolor() : fl_inactive(labelcolor()));
            fl_draw(label(), x() + 11, y(), lw, th, FL_ALIGN_LEFT);
        }
        draw_children();
    }
};

Widget CreateFrame(Widget parent, char *s)
{
    GW *g;
    GFrame *fr;
    begin_in(parent);
    fr = new GFrame();
    fr->end();
    end_in();
    if (s) {
        set_label(fr, s);
    }
    g = new GW(GK_FRAME, parent);
    g->w = fr;
    fr->user_data(g);
    register_widget(fr, g);
    schedule_layout(g);
    return g;
}

Widget CreateSeparator(Widget parent)
{
    Fl_Box *b;
    if (parent && (parent->kind == GK_MENU || parent->kind == GK_POPUP)) {
        GW *g = new GW(GK_MENUSEP, parent);
        menu_changed(g);
        return g;
    }
    begin_in(parent);
    b = new Fl_Box(0, 0, 10, 10);
    end_in();
    b->box(FL_THIN_DOWN_BOX);
    return new_leaf(parent, LT_SEPARATOR, b);
}

Widget CreateGrid(Widget parent, int ncols, int nrows)
{
    GW *g;
    if (ncols <= 0 || nrows <= 0) {
        errmsg("Wrong call to CreateGrid()");
        ncols = 1;
        nrows = 1;
    }
    g = new GW(GK_GRID, parent);
    g->ncols = ncols;
    g->nrows = nrows;
    schedule_layout(g);
    return g;
}

void PlaceGridChild(Widget grid, Widget w, int col, int row)
{
    if (grid == NULL || grid->kind != GK_GRID) {
        errmsg("PlaceGridChild() called with a non-grid widget");
        return;
    }
    if (col < 0 || col >= grid->ncols) {
        errmsg("PlaceGridChild() called with wrong `col' argument");
        return;
    }
    if (row < 0 || row >= grid->nrows) {
        errmsg("PlaceGridChild() called with wrong `row' argument");
        return;
    }
    w->gcol = col;
    w->grow = row;
    schedule_layout(grid);
}

static void tabs_cb(Fl_Widget *w, void *)
{
    w->redraw();
}

Widget CreateTab(Widget parent)
{
    GW *g;
    Fl_Tabs *t;
    begin_in(parent);
    t = new Fl_Tabs(0, 0, 100, 100);
    t->end();
    end_in();
    t->callback(tabs_cb);
    g = new GW(GK_TABS, parent);
    g->w = t;
    t->user_data(g);
    register_widget(t, g);
    schedule_layout(g);
    return g;
}

Widget CreateTabPage(Widget parent, char *s)
{
    GW *g;
    Fl_Group *p;
    Fl_Tabs *t = (Fl_Tabs *) parent->w;
    int th = row_h() + 6;

    Fl_Group::current(t);
    p = new Fl_Group(t->x(), t->y() + th, t->w(), t->h() - th);
    p->end();
    end_in();
    set_label(p, s);
    p->box(FL_THIN_UP_BOX);
    if (t->children() > 1) {
        p->hide();
    }
    g = new GW(GK_TABPAGE, parent);
    g->w = p;
    p->user_data(g);
    register_widget(p, g);
    schedule_layout(g);
    return g;
}

void SelectTabPage(Widget tab, Widget w)
{
    if (tab && w && tab->w && w->w) {
        ((Fl_Tabs *) tab->w)->value(w->w);
    }
}

Widget CreateScrolledVContainer(Widget parent, int height)
{
    GW *g;
    Fl_Scroll *sc;
    begin_in(parent);
    sc = new Fl_Scroll(0, 0, 100, 100);
    sc->end();
    sc->type(Fl_Scroll::VERTICAL_ALWAYS);
    sc->box(FL_DOWN_BOX);
    end_in();
    g = new GW(GK_SCROLL, parent);
    g->w = sc;
    g->fixed_h = height;
    sc->user_data(g);
    schedule_layout(g);
    return CreateVContainer(g);
}

/* ------------------------------------------------------------------ */
/* text */

static GW *new_text_leaf(GW *parent, int len)
{
    Fl_Input *in;
    GW *g;
    begin_in(parent);
    in = new Fl_Input(0, 0, 10, 10);
    end_in();
    g = new_leaf(parent, LT_TEXT, in);
    g->cols = len;
    in->callback(text_cb, g);
    in->when(FL_WHEN_ENTER_KEY_ALWAYS);
    return g;
}

Widget CreateTextItem2(Widget parent, int len, char *s)
{
    GW *rc = (GW *) CreateHContainer(parent);
    CreateLabel(rc, s);
    return new_text_leaf(rc, len);
}

Widget CreateTextItem4(Widget parent, int len, char *label)
{
    CreateLabel(parent, label);
    return new_text_leaf(parent, len);
}

Widget CreateScrollTextItem2(Widget parent, int hgt, char *s)
{
    GW *form, *g;
    Fl_Text_Editor *ed;

    form = (GW *) CreateVContainer(parent);
    CreateLabel(form, s);
    begin_in(form);
    ed = new Fl_Text_Editor(0, 0, 10, 10);
    end_in();
    ed->buffer(new Fl_Text_Buffer());
    ed->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    g = new_leaf(form, LT_SCROLLTEXT, ed);
    g->rows = (hgt > 0) ? hgt : 4;
    return g;
}

static std::string get_text(GW *w)
{
    if (w == NULL || w->w == NULL) {
        return std::string();
    }
    if (w->ltype == LT_SCROLLTEXT) {
        char *t = ((Fl_Text_Editor *) w->w)->buffer()->text();
        std::string s(t ? t:"");
        free(t);
        return s;
    } else if (w->ltype == LT_TEXT) {
        const char *t = ((Fl_Input *) w->w)->value();
        return std::string(t ? t:"");
    } else {
        const char *t = w->w->label();
        return std::string(t ? t:"");
    }
}

static void set_text(GW *w, const char *s)
{
    if (w == NULL || w->w == NULL) {
        return;
    }
    if (s == NULL) {
        s = "";
    }
    if (w->ltype == LT_SCROLLTEXT) {
        ((Fl_Text_Editor *) w->w)->buffer()->text(s);
    } else if (w->ltype == LT_TEXT) {
        ((Fl_Input *) w->w)->value(s);
    } else {
        w->w->copy_label(s);
    }
}

char *xv_getstr(Widget w)
{
    static char buf[MAX_STRING_LENGTH];
    std::string s = get_text(w);
    int i;

    strncpy(buf, s.c_str(), MAX_STRING_LENGTH - 1);
    buf[MAX_STRING_LENGTH - 1] = '\0';

    for (i = strlen(buf) - 1; i >= 0; i--) {
        if (buf[i] == '\n') {
            buf[i] = ' ';
        }
    }
    return buf;
}

void xv_setstr(Widget w, char *s)
{
    if (w != NULL) {
        set_text(w, s);
    }
}

void SetTextCursorPos(Widget w, int pos)
{
    if (w && w->ltype == LT_TEXT) {
        ((Fl_Input *) w->w)->insert_position(pos);
    }
}

/* lookup table to determine if character is a floating point digit
 * only allowable char's [0-9.eE]
 */
static int fpdigit(int c)
{
    return (c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E';
}

static int eval_text(GW *w, double *answer)
{
    std::string s = get_text(w);
    static char *buf = NULL;
    int i, len, ier;
    double result;

    buf = copy_string(buf, (char *) s.c_str());
    if (!(len = strlen(buf))) {
        *answer = 0;
        return -1;
    }
    /* first character may be a sign */
    if (!fpdigit((unsigned char) buf[0]) && buf[0] != '-' && buf[0] != '+') {
        i = len + 1;
    } else {
        i = 1;
    }
    for (; i < len; i++) {
        if (!fpdigit((unsigned char) buf[i])) {
            break;
        }
    }
    if (i == len) {
        *answer = atof(buf);
        return 1;
    } else {
        ier = s_scanner(buf, &result);
        if (!ier) {
            *answer = result;
            return 0;
        } else {
            *answer = 0;
            return -1;
        }
    }
}

Boolean xv_evalexpr(Widget w, double *answer)
{
    return (eval_text(w, answer) >= 0) ? RETURN_SUCCESS:RETURN_FAILURE;
}

Boolean xv_evalexpri(Widget w, int *answer)
{
    double d;
    int r = eval_text(w, &d);
    if (r == 1) {
        std::string s = get_text(w);
        *answer = atoi(s.c_str());
    } else {
        *answer = (int) d;
    }
    return (r >= 0) ? RETURN_SUCCESS:RETURN_FAILURE;
}

TextStructure *CreateTextInput(Widget parent, char *s)
{
    TextStructure *retval;

    retval = (TextStructure *) xmalloc(sizeof(TextStructure));
    retval->form = CreateHContainer(parent);
    retval->label = CreateLabel(retval->form, s);
    retval->text = new_text_leaf(retval->form, 20);

    return retval;
}

/* Ctrl+E in compound string inputs opens the font tool */
class CSInput : public Fl_Input {
public:
    CSInput() : Fl_Input(0, 0, 10, 10) {}
    int handle(int event) {
        if (event == FL_KEYBOARD && Fl::event_state(FL_CTRL) &&
            Fl::event_key() == 'e') {
            create_fonttool((Widget) user_data());
            return 1;
        }
        return Fl_Input::handle(event);
    }
};

TextStructure *CreateCSText(Widget parent, char *s)
{
    TextStructure *retval;
    CSInput *in;
    GW *g;

    retval = (TextStructure *) xmalloc(sizeof(TextStructure));
    retval->form = CreateHContainer(parent);
    retval->label = CreateLabel(retval->form, s);
    begin_in(retval->form);
    in = new CSInput();
    end_in();
    g = new_leaf(retval->form, LT_TEXT, in);
    g->cols = 20;
    in->callback(text_cb, g);
    in->when(FL_WHEN_ENTER_KEY_ALWAYS);
    in->tooltip("Ctrl+E opens the font tool");
    retval->text = g;

    return retval;
}

char *GetTextString(TextStructure *cst)
{
    static char *buf = NULL;
    std::string s = get_text(cst->text);
    buf = copy_string(buf, (char *) s.c_str());
    return buf;
}

void SetTextString(TextStructure *cst, char *s)
{
    set_text(cst->text, s ? s:"");
}

void AddTextInputCB(TextStructure *cst, Text_CBProc cbproc, void *data)
{
    ButtonCB cb = {(Button_CBProc) cbproc, data};
    cst->text->text_cbs.push_back(cb);
}

int GetTextCursorPos(TextStructure *cst)
{
    return ((Fl_Input *) cst->text->w)->insert_position();
}

void TextInsert(TextStructure *cst, int pos, char *s)
{
    Fl_Input *in = (Fl_Input *) cst->text->w;
    in->replace(pos, pos, s);
}

/* ------------------------------------------------------------------ */
/* option menus */

static void choice_cb(Fl_Widget *w, void *data)
{
    GW *g = (GW *) data;
    Fl_Choice *ch = (Fl_Choice *) w;
    int i = ch->value();
    size_t k;

    /* callbacks attached to the whole menu */
    for (k = 0; k < g->toggle_cbs.size(); k++) {
        OptionStructure *opt = (OptionStructure *) g->owner;
        int value = (opt && i >= 0 && i < opt->nchoices) ?
            opt->options[i].value : i;
        g->toggle_cbs[k].proc(value, g->toggle_cbs[k].data);
    }
    /* callbacks attached to the selected entry (CreatePanelChoice) */
    for (k = 0; k < g->children.size(); k++) {
        GW *it = g->children[k];
        if (it->kind == GK_CHOICEITEM && it->index == i) {
            for (size_t j = 0; j < it->button_cbs.size(); j++) {
                it->button_cbs[j].proc(it->button_cbs[j].data);
            }
        }
    }
}

static GW *new_choice(GW *parent, const char *labelstr, GW **rc_out)
{
    GW *rc, *g;
    Fl_Choice *ch;

    rc = (GW *) CreateHContainer(parent);
    if (labelstr && labelstr[0]) {
        CreateLabel(rc, (char *) labelstr);
    }
    begin_in(rc);
    ch = new Fl_Choice(0, 0, 10, 10);
    end_in();
    g = new_leaf(rc, LT_CHOICE, ch);
    ch->callback(choice_cb, g);
    *rc_out = rc;
    return g;
}

/* set the entries of a choice; labels as given (no menu path parsing) */
static void set_choice_items(GW *g, std::vector<std::string> &labels,
    Fl_Labeltype lt)
{
    Fl_Choice *ch = (Fl_Choice *) g->w;
    std::vector<Fl_Menu_Item> items;
    int old = ch->value();
    size_t i;

    /* Fl_Menu_::copy() doesn't copy the strings: keep them here */
    std::vector<std::string> keep(labels);

    init_labeltypes();
    for (i = 0; i < keep.size(); i++) {
        Fl_Menu_Item it = blank_item();
        it.text = keep[i].c_str();
        it.labeltype_ = (lt == LT_BITMAP && keep[i][0] != '\034') ?
            FL_NORMAL_LABEL : lt;
        items.push_back(it);
    }
    items.push_back(blank_item());
    ch->copy(&items[0]);
    g->item_labels.swap(keep);
    if (old >= 0 && old < (int) labels.size()) {
        ch->value(old);
    } else if (labels.size() > 0) {
        ch->value(0);
    }
    /* entry handles (for SetSensitive()) */
    for (i = 0; i < g->children.size(); i++) {
        g->children[i]->index = i;
    }
    while (g->children.size() < labels.size()) {
        GW *it = new GW(GK_CHOICEITEM, g);
        it->index = g->children.size() - 1;
    }
    ch->redraw();
    schedule_layout(g);
}

OptionStructure *CreateOptionChoice(Widget parent, char *labelstr, int ncols,
                                                int nchoices, OptionItem *items)
{
    OptionStructure *retval;
    GW *rc, *g;

    retval = (OptionStructure *) xmalloc(sizeof(OptionStructure));
    g = new_choice(parent, labelstr, &rc);
    g->owner = retval;
    retval->menu = rc;
    retval->pulldown = g;
    retval->ncols = ncols;
    retval->nchoices = 0;
    retval->options = NULL;

    UpdateOptionChoice(retval, nchoices, items);

    return retval;
}

void UpdateOptionChoice(OptionStructure *optp, int nchoices, OptionItem *items)
{
    GW *g = (GW *) optp->pulldown;
    std::vector<std::string> labels;
    int i;

    optp->options = (OptionWidgetItem *) xrealloc(optp->options,
        nchoices*sizeof(OptionWidgetItem));
    optp->nchoices = nchoices;

    for (i = 0; i < nchoices; i++) {
        optp->options[i].value = items[i].value;
        labels.push_back(items[i].label ? items[i].label : "");
    }
    set_choice_items(g, labels, FL_NORMAL_LABEL);
    for (i = 0; i < nchoices; i++) {
        optp->options[i].widget = g->children[i];
    }
}

void paint_color_selector(OptionStructure *optp)
{
    GW *g = (GW *) optp->pulldown;
    std::vector<std::string> labels;
    int i;

    for (i = 0; i < optp->nchoices; i++) {
        int color = optp->options[i].value;
        RGB *rgb = get_rgb(color);
        char buf[16];
        sprintf(buf, "\033%02x%02x%02x",
            rgb ? rgb->red >> (GRACE_BPP - 8) : 0,
            rgb ? rgb->green >> (GRACE_BPP - 8) : 0,
            rgb ? rgb->blue >> (GRACE_BPP - 8) : 0);
        labels.push_back(std::string(buf) + get_colorname(color));
    }
    set_choice_items(g, labels, LT_SWATCH);
}

OptionStructure *CreateBitmapOptionChoice(Widget parent, char *labelstr, int ncols,
                int nchoices, int width, int height, BitmapOptionItem *items)
{
    OptionStructure *retval;
    GW *rc, *g;
    std::vector<std::string> labels;
    int i;

    retval = (OptionStructure *) xmalloc(sizeof(OptionStructure));
    g = new_choice(parent, labelstr, &rc);
    g->owner = retval;
    retval->menu = rc;
    retval->pulldown = g;
    retval->ncols = ncols;
    retval->nchoices = nchoices;
    retval->options = (OptionWidgetItem *)
        xmalloc(nchoices*sizeof(OptionWidgetItem));

    for (i = 0; i < nchoices; i++) {
        retval->options[i].value = items[i].value;
        if (items[i].bitmap != NULL) {
            char buf[16];
            bitmap_registry.push_back(
                new Fl_Bitmap(items[i].bitmap, width, height));
            sprintf(buf, "\034%d", (int) bitmap_registry.size() - 1);
            labels.push_back(buf);
        } else {
            labels.push_back("None");
        }
    }

    set_choice_items(g, labels, LT_BITMAP);
    for (i = 0; i < nchoices; i++) {
        retval->options[i].widget = g->children[i];
    }
    schedule_layout(g);

    return retval;
}

void SetOptionChoice(OptionStructure *opt, int value)
{
    int i;

    if (opt->options == NULL || opt->nchoices <= 0) {
        return;
    }

    for (i = 0; i < opt->nchoices; i++) {
        if (opt->options[i].value == value) {
            ((Fl_Choice *) ((GW *) opt->pulldown)->w)->value(i);
            return;
        }
    }
}

int GetOptionChoice(OptionStructure *opt)
{
    int i;

    if (opt->options == NULL || opt->nchoices <= 0) {
        errmsg("Internal error in GetOptionChoice()");
        return 0;
    }

    i = ((Fl_Choice *) ((GW *) opt->pulldown)->w)->value();
    if (i >= 0 && i < opt->nchoices) {
        return opt->options[i].value;
    }
    errmsg("Internal error in GetOptionChoice()");
    return 0;
}

void AddOptionChoiceCB(OptionStructure *opt, OC_CBProc cbproc, void *anydata)
{
    ToggleCB cb = {(TB_CBProc) cbproc, anydata};
    ((GW *) opt->pulldown)->toggle_cbs.push_back(cb);
}

Widget *CreatePanelChoice(Widget parent, char *labelstr, int nchoices, ...)
{
    va_list var;
    int i;
    char *s;
    Widget *retval;
    GW *rc, *g;
    std::vector<std::string> labels;

    nchoices--;

    retval = (Widget *) xmalloc((nchoices + 2)*sizeof(Widget));
    g = new_choice(parent, labelstr, &rc);

    va_start(var, nchoices);
    while ((s = va_arg(var, char *)) != NULL) {
        labels.push_back(s);
    }
    va_end(var);
    if ((int) labels.size() != nchoices) {
        errmsg("Incorrect number of selections in CreatePanelChoice()");
    }
    set_choice_items(g, labels, FL_NORMAL_LABEL);

    retval[0] = rc;
    retval[1] = g;
    for (i = 0; i < nchoices; i++) {
        retval[i + 2] = (i < (int) g->children.size()) ? g->children[i] : NULL;
    }

    return retval;
}

void SetChoice(Widget *w, int value)
{
    Fl_Choice *ch;
    if (w == NULL) {
        errwin("Internal error, SetChoice: Attempt to set NULL Widget");
        return;
    }
    ch = (Fl_Choice *) w[1]->w;
    if (value < 0 || value >= ch->size() - 1) {
        errwin("Value not found in SetChoice()");
        return;
    }
    ch->value(value);
}

int GetChoice(Widget *w)
{
    if (w == NULL) {
        errwin("Internal error, GetChoice called with NULL argument");
        return 0;
    }
    return ((Fl_Choice *) w[1]->w)->value();
}

/* ------------------------------------------------------------------ */
/* lists */

class GBrowser : public Fl_Browser {
public:
    GW *gw;
    GBrowser() : Fl_Browser(0, 0, 10, 10), gw(NULL) {
        format_char(0);
    }
    int handle(int event);
};

static void list_selection(GW *g, std::vector<int> &pos)
{
    Fl_Browser *b = (Fl_Browser *) g->w;
    pos.clear();
    for (int i = 1; i <= b->size(); i++) {
        if (b->selected(i)) {
            pos.push_back(i);
        }
    }
}

static void call_list_cbs(GW *g)
{
    ListStructure *listp = (ListStructure *) g->owner;
    int n, *values;

    if (listp == NULL || g->toggle_cbs.empty()) {
        return;
    }
    n = GetListChoices(listp, &values);
    for (size_t k = 0; k < g->toggle_cbs.size(); k++) {
        ((List_CBProc) (void *) g->toggle_cbs[k].proc)(n, values,
            g->toggle_cbs[k].data);
    }
    if (n > 0) {
        xfree(values);
    }
}

static void list_select_all(GW *g, int how)
{
    Fl_Browser *b = (Fl_Browser *) g->w;
    if (b->type() != FL_MULTI_BROWSER) {
        fl_beep();
        return;
    }
    for (int i = 1; i <= b->size(); i++) {
        switch (how) {
        case 0:
            b->select(i, 1);
            break;
        case 1:
            b->select(i, 0);
            break;
        default:
            b->select(i, !b->selected(i));
            break;
        }
    }
    call_list_cbs(g);
}

int GBrowser::handle(int event)
{
    if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE) {
        if (gw && gw->popup && gw->popup_proc) {
            gw->popup_proc(gw);
            return 1;
        }
    }
    if (event == FL_KEYBOARD && Fl::event_state(FL_CTRL) && gw) {
        switch (Fl::event_key()) {
        case 'a':
            list_select_all(gw, 0);
            return 1;
        case 'u':
            list_select_all(gw, 1);
            return 1;
        case 'i':
            list_select_all(gw, 2);
            return 1;
        }
    }
    return Fl_Browser::handle(event);
}

static void list_cb(Fl_Widget *w, void *data)
{
    GW *g = (GW *) data;
    Fl_Browser *b = (Fl_Browser *) w;

    if (Fl::event() == FL_PUSH && Fl::event_clicks() > 0 &&
        Fl::event_button() == FL_LEFT_MOUSE) {
        int line = b->value();
        Fl::event_clicks(0);
        if (line > 0 && g->default_proc) {
            g->default_proc(g, line - 1);
        }
        return;
    }
    call_list_cbs(g);
}

ListStructure *CreateListChoice(Widget parent, char *labelstr, int type,
                                int nvisible, int nchoices, OptionItem *items)
{
    ListStructure *retval;
    GBrowser *b;
    GW *g;

    retval = (ListStructure *) xmalloc(sizeof(ListStructure));
    retval->rc = CreateVContainer(parent);
    AddHelpCB(retval->rc, (char *) "doc/UsersGuide.html#list-selector");
    CreateLabel(retval->rc, labelstr);

    begin_in(retval->rc);
    b = new GBrowser();
    end_in();
    b->type(type == LIST_TYPE_SINGLE ? FL_HOLD_BROWSER : FL_MULTI_BROWSER);
    g = new_leaf(retval->rc, LT_LIST, b);
    b->gw = g;
    g->rows = nvisible;
    g->owner = retval;
    b->callback(list_cb, g);
    b->when(FL_WHEN_CHANGED | FL_WHEN_NOT_CHANGED);

    retval->list = g;
    retval->values = NULL;
    retval->nchoices = 0;
    retval->anydata = NULL;

    UpdateListChoice(retval, nchoices, items);

    return retval;
}

void UpdateListChoice(ListStructure *listp, int nchoices, OptionItem *items)
{
    int i, nsel, *selvalues;
    Fl_Browser *b;

    if (listp == NULL) {
        return;
    }
    b = (Fl_Browser *) ((GW *) listp->list)->w;

    nsel = GetListChoices(listp, &selvalues);

    listp->nchoices = nchoices;
    listp->values = (int *) xrealloc(listp->values, nchoices*SIZEOF_INT);
    for (i = 0; i < nchoices; i++) {
        listp->values[i] = items[i].value;
    }

    b->clear();
    for (i = 0; i < nchoices; i++) {
        b->add(items[i].label);
    }
    SelectListChoices(listp, nsel, selvalues);
    if (nsel > 0) {
        xfree(selvalues);
    }
}

int SelectListChoice(ListStructure *listp, int choice)
{
    Fl_Browser *b = (Fl_Browser *) ((GW *) listp->list)->w;
    int i = 0;

    while (i < listp->nchoices && listp->values[i] != choice) {
        i++;
    }
    if (i < listp->nchoices) {
        i++;
        b->deselect();
        b->select(i, 1);
        b->make_visible(i);
        call_list_cbs((GW *) listp->list);
        return RETURN_SUCCESS;
    } else {
        return RETURN_FAILURE;
    }
}

void SelectListChoices(ListStructure *listp, int nchoices, int *choices)
{
    Fl_Browser *b = (Fl_Browser *) ((GW *) listp->list)->w;
    int i = 0, j, last = 0;

    b->deselect();
    for (j = 0; j < nchoices; j++) {
        i = 0;
        while (i < listp->nchoices && listp->values[i] != choices[j]) {
            i++;
        }
        if (i < listp->nchoices) {
            i++;
            b->select(i, 1);
            last = i;
        }
    }
    if (last > 0) {
        b->make_visible(last);
    }
    if (nchoices > 0) {
        call_list_cbs((GW *) listp->list);
    }
}

int GetListChoices(ListStructure *listp, int **values)
{
    std::vector<int> pos;
    int i, n;

    list_selection((GW *) listp->list, pos);
    n = pos.size();
    if (n == 0) {
        return 0;
    }
    *values = (int *) xmalloc(n*SIZEOF_INT);
    for (i = 0; i < n; i++) {
        (*values)[i] = listp->values[pos[i] - 1];
    }
    return n;
}

void AddListChoiceCB(ListStructure *listp, List_CBProc cbproc, void *anydata)
{
    ToggleCB cb = {(TB_CBProc) (void *) cbproc, anydata};
    ((GW *) listp->list)->toggle_cbs.push_back(cb);
}

/* the graph and set selectors with their popup menus */

static void list_selectall_cb(void *data)
{
    list_select_all((GW *) ((ListStructure *) data)->list, 0);
}

static void list_unselectall_cb(void *data)
{
    list_select_all((GW *) ((ListStructure *) data)->list, 1);
}

static void list_invertselection_cb(void *data)
{
    list_select_all((GW *) ((ListStructure *) data)->list, 2);
}

static Widget CreatePopupMenu(GW *list)
{
    GW *g = new GW(GK_POPUP, NULL);
    list->popup = g;
    return g;
}

static void graph_popup(GW *list);

static GraphPopupMenu *CreateGraphPopupEntries(ListStructure *listp)
{
    GraphPopupMenu *graph_popup_menu;
    Widget popup;

    graph_popup_menu = (GraphPopupMenu *) xmalloc(sizeof(GraphPopupMenu));

    popup = CreatePopupMenu((GW *) listp->list);
    graph_popup_menu->popup = popup;

    graph_popup_menu->label_item = CreateMenuLabel(popup, (char *) "Selection:");
    CreateMenuSeparator(popup);
    graph_popup_menu->focus_item = CreateMenuButton(popup, (char *) "Focus to", 'F',
    	switch_focus_proc, (void *) listp);
    CreateMenuSeparator(popup);
    graph_popup_menu->hide_item = CreateMenuButton(popup, (char *) "Hide", 'H',
    	hide_graph_proc, (void *) listp);
    graph_popup_menu->show_item = CreateMenuButton(popup, (char *) "Show", 'S',
    	show_graph_proc, (void *) listp);
    graph_popup_menu->duplicate_item = CreateMenuButton(popup, (char *) "Duplicate", 'D',
    	duplicate_graph_proc, (void *) listp);
    graph_popup_menu->kill_item = CreateMenuButton(popup, (char *) "Kill", 'K',
    	kill_graph_proc, (void *) listp);
    CreateMenuSeparator(popup);
    graph_popup_menu->copy12_item = CreateMenuButton(popup, (char *) "Copy 1 to 2", '\0',
    	copy12_graph_proc, (void *) listp);
    graph_popup_menu->copy21_item = CreateMenuButton(popup, (char *) "Copy 2 to 1", '\0',
    	copy21_graph_proc, (void *) listp);
    graph_popup_menu->move12_item = CreateMenuButton(popup, (char *) "Move 1 to 2", '\0',
    	move12_graph_proc, (void *) listp);
    graph_popup_menu->move21_item = CreateMenuButton(popup, (char *) "Move 2 to 1", '\0',
    	move21_graph_proc, (void *) listp);
    graph_popup_menu->swap_item = CreateMenuButton(popup, (char *) "Swap", 'w',
    	swap_graph_proc, (void *) listp);
    CreateMenuSeparator(popup);
    CreateMenuButton(popup, (char *) "Create new", 'C',
    	create_new_graph_proc, (void *) listp);

    ((GW *) listp->list)->popup_proc = graph_popup;

    return graph_popup_menu;
}

static void graph_popup(GW *list)
{
    ListStructure *listp = (ListStructure *) list->owner;
    int i, n;
    int *values;
    char buf[64];
    GraphPopupMenu *graph_popup_menu;

    graph_popup_menu = (GraphPopupMenu *) listp->anydata;

    n = GetListChoices(listp, &values);
    if (n > 0) {
        sprintf(buf, "G%d", values[0]);
        for (i = 1; i < n; i++) {
            if (strlen(buf) > 30) {
                strcat(buf, "...");
                break;
            }
            sprintf(buf + strlen(buf), ", G%d", values[i]);
        }
    } else {
        strcpy(buf, "None");
    }

    SetLabel(graph_popup_menu->label_item, buf);

    SetSensitive(graph_popup_menu->hide_item, n != 0);
    SetSensitive(graph_popup_menu->show_item, n != 0);
    SetSensitive(graph_popup_menu->duplicate_item, n != 0);
    SetSensitive(graph_popup_menu->kill_item, n != 0);
    SetSensitive(graph_popup_menu->focus_item, n == 1);
    if (n == 2) {
        sprintf(buf, "Copy G%d to G%d", values[0], values[1]);
        SetLabel(graph_popup_menu->copy12_item, buf);
        ManageChild(graph_popup_menu->copy12_item);
        sprintf(buf, "Copy G%d to G%d", values[1], values[0]);
        SetLabel(graph_popup_menu->copy21_item, buf);
        ManageChild(graph_popup_menu->copy21_item);
        sprintf(buf, "Move G%d to G%d", values[0], values[1]);
        SetLabel(graph_popup_menu->move12_item, buf);
        ManageChild(graph_popup_menu->move12_item);
        sprintf(buf, "Move G%d to G%d", values[1], values[0]);
        SetLabel(graph_popup_menu->move21_item, buf);
        ManageChild(graph_popup_menu->move21_item);
        SetSensitive(graph_popup_menu->swap_item, TRUE);
    } else {
        UnmanageChild(graph_popup_menu->copy12_item);
        UnmanageChild(graph_popup_menu->copy21_item);
        UnmanageChild(graph_popup_menu->move12_item);
        UnmanageChild(graph_popup_menu->move21_item);
        SetSensitive(graph_popup_menu->swap_item, FALSE);
    }

    if (n > 0) {
        xfree(values);
    }
    show_popup((GW *) graph_popup_menu->popup, list->w);
}

static void graph_select_default(GW *list, int item)
{
    ListStructure *plist = (ListStructure *) list->owner;
    switch_current_graph(plist->values[item]);
}

ListStructure *CreateGraphChoice(Widget parent, char *labelstr, int type)
{
    ListStructure *retvalp;
    int nvisible;

    ngraph_selectors++;
    graph_selectors = (ListStructure **) xrealloc(graph_selectors,
                                    ngraph_selectors*sizeof(ListStructure *));

    nvisible = (type == LIST_TYPE_SINGLE) ? 2 : 4;
    retvalp = CreateListChoice(parent, labelstr, type, nvisible,
                               ngraph_select_items, graph_select_items);
    if (retvalp == NULL) {
        return NULL;
    }
    AddHelpCB(retvalp->rc, (char *) "doc/UsersGuide.html#graph-selector");
    graph_selectors[ngraph_selectors - 1] = retvalp;

    ((GW *) retvalp->list)->default_proc = graph_select_default;
    retvalp->anydata = CreateGraphPopupEntries(retvalp);

    if (ngraph_select_items == 0) {
        update_graph_selectors();
    } else {
        UpdateListChoice(retvalp, ngraph_select_items, graph_select_items);
    }

    SelectListChoice(retvalp, get_cg());

    return retvalp;
}

static void set_popup(GW *list);

static SetPopupMenu *CreateSetPopupEntries(ListStructure *listp)
{
    SetPopupMenu *set_popup_menu;
    Widget popup, submenupane;

    set_popup_menu = (SetPopupMenu *) xmalloc(sizeof(SetPopupMenu));
    popup = CreatePopupMenu((GW *) listp->list);
    set_popup_menu->popup = popup;

    set_popup_menu->label_item = CreateMenuLabel(popup, (char *) "Selection:");

    CreateMenuSeparator(popup);

    set_popup_menu->hide_item = CreateMenuButton(popup, (char *) "Hide", '\0',
    	hide_set_proc, (void *) listp);
    set_popup_menu->show_item = CreateMenuButton(popup, (char *) "Show", '\0',
    	show_set_proc, (void *) listp);
    set_popup_menu->bringf_item = CreateMenuButton(popup, (char *) "Bring to front", '\0',
    	bringf_set_proc, (void *) listp);
    set_popup_menu->sendb_item = CreateMenuButton(popup, (char *) "Send to back", '\0',
    	sendb_set_proc, (void *) listp);
    CreateMenuSeparator(popup);
    set_popup_menu->duplicate_item = CreateMenuButton(popup, (char *) "Duplicate", '\0',
    	duplicate_set_proc, (void *) listp);
    set_popup_menu->kill_item = CreateMenuButton(popup, (char *) "Kill", '\0',
    	kill_set_proc, (void *) listp);
    set_popup_menu->killd_item = CreateMenuButton(popup, (char *) "Kill data", '\0',
    	killd_set_proc, (void *) listp);
    CreateMenuSeparator(popup);
    set_popup_menu->copy12_item = CreateMenuButton(popup, (char *) "Copy 1 to 2", '\0',
    	copy12_set_proc, (void *) listp);
    set_popup_menu->copy21_item = CreateMenuButton(popup, (char *) "Copy 2 to 1", '\0',
    	copy21_set_proc, (void *) listp);
    set_popup_menu->move12_item = CreateMenuButton(popup, (char *) "Move 1 to 2", '\0',
    	move12_set_proc, (void *) listp);
    set_popup_menu->move21_item = CreateMenuButton(popup, (char *) "Move 2 to 1", '\0',
    	move21_set_proc, (void *) listp);
    set_popup_menu->swap_item = CreateMenuButton(popup, (char *) "Swap", '\0',
    	swap_set_proc, (void *) listp);
    CreateMenuSeparator(popup);
    set_popup_menu->edit_item = CreateMenu(popup, (char *) "Edit", 'E', FALSE);
    CreateMenuButton(set_popup_menu->edit_item, (char *) "In spreadsheet", '\0',
    	editS_set_proc, (void *) listp);
    CreateMenuButton(set_popup_menu->edit_item, (char *) "In text editor", '\0',
    	editE_set_proc, (void *) listp);
    submenupane = CreateMenu(popup, (char *) "Create new", '\0', FALSE);
    CreateMenuButton(submenupane, (char *) "By formula", '\0',
    	newF_set_proc, (void *) listp);
    CreateMenuButton(submenupane, (char *) "In spreadsheet", '\0',
    	newS_set_proc, (void *) listp);
    CreateMenuButton(submenupane, (char *) "In text editor", '\0',
    	newE_set_proc, (void *) listp);
    CreateMenuButton(submenupane, (char *) "From block data", '\0',
    	newB_set_proc, (void *) listp);

    CreateMenuSeparator(popup);

    CreateMenuButton(popup, (char *) "Pack all sets", '\0',
    	pack_set_proc, (void *) listp);

    CreateMenuSeparator(popup);

    submenupane = CreateMenu(popup, (char *) "Selector operations", 'o', FALSE);
    CreateMenuToggle(submenupane,
        (char *) "View set comments", '\0', view_comments_set_proc, (void *) listp);
    CreateMenuSeparator(submenupane);
    set_popup_menu->shownd_item = CreateMenuToggle(submenupane,
        (char *) "Show data-less", '\0', shownd_set_proc, (void *) listp);
    set_popup_menu->showh_item = CreateMenuToggle(submenupane,
        (char *) "Show hidden", '\0', showh_set_proc, (void *) listp);
    CreateMenuSeparator(submenupane);
    CreateMenuButton(submenupane, (char *) "Select all", '\0',
    	list_selectall_cb, (void *) listp);
    CreateMenuButton(submenupane, (char *) "Unselect all", '\0',
    	list_unselectall_cb, (void *) listp);
    CreateMenuButton(submenupane, (char *) "Invert selection", '\0',
    	list_invertselection_cb, (void *) listp);
    CreateMenuSeparator(submenupane);
    CreateMenuButton(submenupane, (char *) "Update", '\0',
    	update_set_proc, (void *) listp);

    ((GW *) listp->list)->popup_proc = set_popup;

    return set_popup_menu;
}

static void set_popup(GW *list)
{
    ListStructure *listp = (ListStructure *) list->owner;
    SetChoiceData *sdata;
    int i, n;
    int *values;
    char buf[64];
    SetPopupMenu *set_popup_menu;

    sdata = (SetChoiceData *) listp->anydata;
    set_popup_menu = sdata->menu;

    n = GetListChoices(listp, &values);
    if (n > 0) {
        sprintf(buf, "S%d", values[0]);
        for (i = 1; i < n; i++) {
            if (strlen(buf) > 30) {
                strcat(buf, "...");
                break;
            }
            sprintf(buf + strlen(buf), ", S%d", values[i]);
        }
    } else {
        strcpy(buf, "None");
    }

    SetLabel(set_popup_menu->label_item, buf);

    SetToggleButtonState(set_popup_menu->shownd_item, sdata->show_nodata);
    SetToggleButtonState(set_popup_menu->showh_item, sdata->show_hidden);

    SetSensitive(set_popup_menu->hide_item, n != 0);
    SetSensitive(set_popup_menu->show_item, n != 0);
    SetSensitive(set_popup_menu->duplicate_item, n != 0);
    SetSensitive(set_popup_menu->kill_item, n != 0);
    SetSensitive(set_popup_menu->killd_item, n != 0);
    SetSensitive(set_popup_menu->bringf_item, n == 1);
    SetSensitive(set_popup_menu->sendb_item, n == 1);
    SetSensitive(set_popup_menu->edit_item, n == 1);
    if (n == 2) {
        sprintf(buf, "Copy S%d to S%d", values[0], values[1]);
        SetLabel(set_popup_menu->copy12_item, buf);
        ManageChild(set_popup_menu->copy12_item);
        sprintf(buf, "Copy S%d to S%d", values[1], values[0]);
        SetLabel(set_popup_menu->copy21_item, buf);
        ManageChild(set_popup_menu->copy21_item);
        sprintf(buf, "Move S%d to S%d", values[0], values[1]);
        SetLabel(set_popup_menu->move12_item, buf);
        ManageChild(set_popup_menu->move12_item);
        sprintf(buf, "Move S%d to S%d", values[1], values[0]);
        SetLabel(set_popup_menu->move21_item, buf);
        ManageChild(set_popup_menu->move21_item);
        SetSensitive(set_popup_menu->swap_item, TRUE);
    } else {
        UnmanageChild(set_popup_menu->copy12_item);
        UnmanageChild(set_popup_menu->copy21_item);
        UnmanageChild(set_popup_menu->move12_item);
        UnmanageChild(set_popup_menu->move21_item);
        SetSensitive(set_popup_menu->swap_item, FALSE);
    }

    if (n > 0) {
        xfree(values);
    }
    show_popup((GW *) set_popup_menu->popup, list->w);
}

static void ss_edit_default(GW *list, int item)
{
    ListStructure *plist = (ListStructure *) list->owner;
    SetChoiceData *sdata = (SetChoiceData *) plist->anydata;
    create_ss_frame(sdata->gno, plist->values[item]);
}

ListStructure *CreateSetChoice(Widget parent, char *labelstr,
                                        int type, int standalone)
{
    ListStructure *retvalp;
    SetChoiceData *sdata;
    int nvisible;

    nvisible = (type == LIST_TYPE_SINGLE) ? 4 : 8;
    retvalp = CreateListChoice(parent, labelstr, type, nvisible, 0, NULL);
    if (retvalp == NULL) {
        return NULL;
    }
    AddHelpCB(retvalp->rc, (char *) "doc/UsersGuide.html#set-selector");

    sdata = (SetChoiceData *) xmalloc(sizeof(SetChoiceData));
    if (sdata == NULL) {
        XCFREE(retvalp);
        return NULL;
    }

    sdata->standalone = standalone;
    sdata->view_comments = FALSE;
    sdata->show_hidden = TRUE;
    sdata->show_nodata = FALSE;
    sdata->menu = CreateSetPopupEntries(retvalp);

    ((GW *) retvalp->list)->default_proc = ss_edit_default;

    retvalp->anydata = sdata;

    if (standalone == TRUE) {
        UpdateSetChoice(retvalp, get_cg());
    }

    nset_selectors++;
    set_selectors = (ListStructure **) xrealloc(set_selectors,
                                nset_selectors*sizeof(ListStructure *));
    set_selectors[nset_selectors - 1] = retvalp;

    return retvalp;
}

static void update_sets_cb(int n, int *values, void *data)
{
    int gno;
    ListStructure *set_listp = (ListStructure *) data;

    if (n == 1) {
        gno = values[0];
    } else {
        gno = -1;
    }
    UpdateSetChoice(set_listp, gno);
}

GraphSetStructure *CreateGraphSetSelector(Widget parent, char *s, int sel_type)
{
    GraphSetStructure *retval;
    Widget rc;

    retval = (GraphSetStructure *) xmalloc(sizeof(GraphSetStructure));
    retval->frame = CreateFrame(parent, s);
    rc = CreateVContainer(retval->frame);
    retval->graph_sel = CreateGraphChoice(rc, (char *) "Graph:", LIST_TYPE_SINGLE);
    retval->set_sel = CreateSetChoice(rc, (char *) "Set:", sel_type, FALSE);
    AddListChoiceCB(retval->graph_sel,
        update_sets_cb, (void *) retval->set_sel);
    UpdateSetChoice(retval->set_sel, get_cg());

    return retval;
}

SrcDestStructure *CreateSrcDestSelector(Widget parent, int sel_type)
{
    SrcDestStructure *retval;

    retval = (SrcDestStructure *) xmalloc(sizeof(SrcDestStructure));

    retval->form = CreateHContainer(parent);
    ((GW *) retval->form)->equal = true;
    retval->src  = CreateGraphSetSelector(retval->form, (char *) "Source", sel_type);
    retval->dest = CreateGraphSetSelector(retval->form, (char *) "Destination", sel_type);

    return retval;
}

/* the old-style set selector */

static void set_selector_text(GW *list, int line, std::string &s)
{
    Fl_Browser *b = (Fl_Browser *) list->w;
    const char *t = b->text(line);
    s = t ? t:"";
}

SetChoiceItem CreateSetSelector(Widget parent,
				char *label,
				int type,
				int ff,
				int gtype,
				int stype)
{
    SetChoiceItem sel;
    GW *rc2, *g;
    GBrowser *b;

    rc2 = (GW *) CreateVContainer(parent);
    CreateLabel(rc2, label);
    begin_in(rc2);
    b = new GBrowser();
    end_in();
    b->type(stype == SELECTION_TYPE_MULTIPLE ? FL_MULTI_BROWSER:FL_HOLD_BROWSER);
    g = new_leaf(rc2, LT_LIST, b);
    b->gw = g;
    g->rows = 6;

    sel.list = g;
    sel.type = type;
    sel.gno = gtype;
    sel.indx = save_set_list(sel);
    update_set_list(gtype == GRAPH_SELECT_CURRENT ? get_cg():sel.gno, sel);

    return sel;
}

static int parse_set_item(const std::string &s)
{
    int retval = SET_SELECT_ERROR;
    if (s == "New set") {
        retval = SET_SELECT_NEXT;
    } else if (s == "All sets") {
        retval = SET_SELECT_ALL;
    } else if (s == "Nearest set") {
        retval = SET_SELECT_NEAREST;
    } else {
        sscanf(s.c_str(), "S%d", &retval);
    }
    return retval;
}

int GetSelectedSet(SetChoiceItem l)
{
    std::vector<int> pos;
    std::string s;

    list_selection(l.list, pos);
    if (pos.empty()) {
        return SET_SELECT_ERROR;
    }
    set_selector_text(l.list, pos[0], s);
    return parse_set_item(s);
}

int GetSelectedSets(SetChoiceItem l, int **sets)
{
    std::vector<int> pos;
    int i, cnt;

    list_selection(l.list, pos);
    cnt = pos.size();
    if (cnt == 0) {
        return SET_SELECT_ERROR;
    }
    *sets = (int *) xmalloc(cnt*SIZEOF_INT);
    for (i = 0; i < cnt; i++) {
        std::string s;
        int v;
        set_selector_text(l.list, pos[i], s);
        v = parse_set_item(s);
        if (v == SET_SELECT_NEXT) {
            return SET_SELECT_NEXT;
        } else if (v == SET_SELECT_ALL) {
            int j, gno, nsets = 0, retval;
            gno = (l.gno == GRAPH_SELECT_CURRENT) ? get_cg():l.gno;
            retval = nactive(gno);
            *sets = (int *) xrealloc(*sets, retval*SIZEOF_INT);
            for (j = 0; j < number_of_sets(gno); j++) {
                if (is_set_active(gno, j)) {
                    (*sets)[nsets++] = j;
                }
            }
            return retval;
        }
        (*sets)[i] = v;
    }
    return cnt;
}

void update_set_list(int gno, SetChoiceItem l)
{
    Fl_Browser *b = (Fl_Browser *) l.list->w;
    int i, scnt = 0;
    char buf[1024];

    b->clear();
    for (i = 0; i < number_of_sets(gno); i++) {
	if (is_set_active(gno, i)) {
	    scnt++;
	}
    }
    switch (l.type) {
    case SET_SELECT_ALL:
	b->add("All sets");
	break;
    case SET_SELECT_NEXT:
	b->add("New set");
	break;
    case SET_SELECT_NEAREST:
	b->add("Nearest set");
	break;
    default:
	break;
    }
    for (i = 0; i < number_of_sets(gno); i++) {
        if (is_set_active(gno, i)) {
            sprintf(buf, "S%d (N=%d, %s)", i, getsetlength(gno, i), getcomment(gno, i));
            b->add(buf);
        }
    }
    /* automatically highlight if only 1 selection */
    if (scnt == 1) {
        b->select(b->size(), 1);
    }
}

/* ------------------------------------------------------------------ */
/* spin buttons */

static void spin_arrow_cb(Fl_Widget *w, void *data)
{
    SpinStructure *spinp = (SpinStructure *) data;
    double value = GetSpinChoice(spinp);

    if (w == ((GW *) spinp->arrow_up)->w) {
        value += spinp->incr;
    } else {
        value -= spinp->incr;
    }
    SetSpinChoice(spinp, value);
}

SpinStructure *CreateSpinChoice(Widget parent, char *s, int len,
                        int type, double min, double max, double incr)
{
    SpinStructure *retval;
    Fl_Button *b;
    GW *rc;

    if (min >= max) {
        errmsg("min >= max in CreateSpinChoice()!");
        return NULL;
    }

    retval = (SpinStructure *) xmalloc(sizeof(SpinStructure));

    retval->type = type;
    retval->min = min;
    retval->max = max;
    retval->incr = incr;

    retval->rc = rc = (GW *) CreateHContainer(parent);
    CreateLabel(rc, s);
    retval->text = new_text_leaf(rc, len);

    begin_in(rc);
    b = new Fl_Button(0, 0, 10, 10, "@-22>");
    end_in();
    b->callback(spin_arrow_cb, retval);
    b->visible_focus(0);
    retval->arrow_down = new_leaf(rc, LT_ARROWS, b);
    ((GW *) retval->arrow_down)->min_w = row_h() - 4;
    b->user_data(retval);

    begin_in(rc);
    b = new Fl_Button(0, 0, 10, 10, "@-28>");
    end_in();
    b->callback(spin_arrow_cb, retval);
    b->visible_focus(0);
    retval->arrow_up = new_leaf(rc, LT_ARROWS, b);
    ((GW *) retval->arrow_up)->min_w = row_h() - 4;
    b->user_data(retval);

    return retval;
}

void SetSpinChoice(SpinStructure *spinp, double value)
{
    char buf[64];

    if (value < spinp->min) {
        fl_beep();
        value = spinp->min;
    } else if (value > spinp->max) {
        fl_beep();
        value = spinp->max;
    }

    if (spinp->type == SPIN_TYPE_FLOAT) {
        sprintf(buf, "%g", value);
    } else {
        sprintf(buf, "%d", (int) rint(value));
    }
    xv_setstr(spinp->text, buf);
}

/* ------------------------------------------------------------------ */
/* scales */

static void scale_cb(Fl_Widget *w, void *data)
{
}

Widget CreateScale(Widget parent, char *s, int min, int max, int delta)
{
    Fl_Value_Slider *sl;
    GW *g;

    begin_in(parent);
    sl = new Fl_Value_Slider(0, 0, 10, 10);
    end_in();
    sl->type(FL_HOR_NICE_SLIDER);
    sl->bounds(min, max);
    sl->step(1);
    sl->value(0);
    set_label(sl, s);
    sl->align(FL_ALIGN_TOP_LEFT);
    sl->textsize(FL_NORMAL_SIZE - 1);
    sl->callback(scale_cb);
    g = new_leaf(parent, LT_SCALE, sl);
    return g;
}

void SetScaleValue(Widget w, int value)
{
    ((Fl_Value_Slider *) w->w)->value(value);
}

int GetScaleValue(Widget w)
{
    return (int) rint(((Fl_Value_Slider *) w->w)->value());
}

void SetScaleWidth(Widget w, int width)
{
    w->min_w = width;
    schedule_layout(w);
}

/* ------------------------------------------------------------------ */
/* dialogs */

typedef struct {
    Widget form;
    int close;
    AACDialog_CBProc cbproc;
    void *anydata;
} AACDialog_CBdata;

static void aacdialog_int_cb_proc(void *data)
{
    AACDialog_CBdata *cbdata;
    int retval;

    set_wait_cursor();

    cbdata = (AACDialog_CBdata *) data;

    retval = cbdata->cbproc(cbdata->anydata);

    if (cbdata->close && retval == RETURN_SUCCESS) {
        UnmanageChild(GetParent(cbdata->form));
    }

    unset_wait_cursor();
}

void CreateAACDialog(Widget form,
    Widget container, AACDialog_CBProc cbproc, void *data)
{
    Widget fr, aacbut[3];
    AACDialog_CBdata *cbdata_accept, *cbdata_apply;
    char *aaclab[3] = {(char *) "Apply", (char *) "Accept", (char *) "Close"};

    fr = CreateFrame(form, NULL);
    fr->form_bottom = true;
    CreateCommandButtons(fr, 3, aacbut, aaclab);

    AddDialogFormChild(form, container);

    cbdata_accept = (AACDialog_CBdata *) xmalloc(sizeof(AACDialog_CBdata));
    cbdata_accept->form    = form;
    cbdata_accept->anydata = data;
    cbdata_accept->cbproc  = cbproc;
    cbdata_accept->close   = TRUE;

    cbdata_apply  = (AACDialog_CBdata *) xmalloc(sizeof(AACDialog_CBdata));
    cbdata_apply->form     = form;
    cbdata_apply->anydata  = data;
    cbdata_apply->cbproc   = cbproc;
    cbdata_apply->close    = FALSE;

    AddButtonCB(aacbut[0], aacdialog_int_cb_proc, cbdata_apply);
    AddButtonCB(aacbut[1], aacdialog_int_cb_proc, cbdata_accept);
    AddButtonCB(aacbut[2], destroy_dialog_cb, GetParent(form));

    ManageChild(container);
    ManageChild(form);
}

Widget CreateAACButtons(Widget parent, Widget form, Button_CBProc aac_cb)
{
    Widget w;
    Widget aacbut[3];
    static char *aaclab[3] = {(char *) "Apply", (char *) "Accept", (char *) "Close"};

    w = CreateCommandButtons(parent, 3, aacbut, aaclab);
    AddButtonCB(aacbut[0], aac_cb, (void *) AAC_APPLY);
    AddButtonCB(aacbut[1], aac_cb, (void *) AAC_ACCEPT);
    AddButtonCB(aacbut[2], aac_cb, (void *) AAC_CLOSE);

    return w;
}

Widget CreateCommandButtonsNoDefault(Widget parent, int n, Widget *buts, char **l)
{
    int i;
    GW *form;

    form = (GW *) CreateHContainer(parent);
    form->equal = true;
    for (i = 0; i < n; i++) {
        buts[i] = CreateButton(form, l[i]);
    }
    return form;
}

Widget CreateCommandButtons(Widget parent, int n, Widget *buts, char **l)
{
    return CreateCommandButtonsNoDefault(parent, n, buts, l);
}

/*
 * generic unmanage popup routine, used elswhere
 */
void destroy_dialog(Widget w, XtPointer client_data, XtPointer call_data)
{
    UnmanageChild((Widget) client_data);
}

/*
 * same for AddButtonCB
 */
void destroy_dialog_cb(void *data)
{
    UnmanageChild((Widget) data);
}

void handle_close(Widget w)
{
}

/*
 * Manage and raise
 */
void RaiseWindow(Widget w)
{
    GW *s = shell_of(w);
    if (s) {
        show_shell(s);
    }
}

void savewidget(Widget w)
{
}

void deletewidget(Widget w)
{
    GW *s = shell_of(w);
    if (s && s != app_shell && s->w) {
        s->w->hide();
        Fl::delete_widget(s->w);
        s->w = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* menus */

class GMenuBar : public Fl_Menu_Bar {
public:
    GMenuBar() : Fl_Menu_Bar(0, 0, 10, 10) {}
};

Widget CreateMenuBar(Widget parent)
{
    GMenuBar *mb;
    GW *g;
    begin_in(parent);
    mb = new GMenuBar();
    end_in();
    mb->box(FL_FLAT_BOX);
    g = new_leaf(parent, LT_MENUBAR, mb);
    g->managed = false;
    mb->hide();
    rebuild_menubar(g);
    return g;
}

Widget CreateMenu(Widget parent, char *label, char mnemonic, int help)
{
    GW *g = new GW(GK_MENU, parent);
    g->label = label;
    g->mnemonic = mnemonic;
    menu_changed(g);

    if (help) {
        CreateMenuButton(g, (char *) "On context", 'x', ContextHelpCB, NULL);
        CreateSeparator(g);
    }

    return g;
}

Widget CreateMenuButton(Widget parent, char *label, char mnemonic,
	Button_CBProc cb, void *data)
{
    GW *g = new GW(GK_MENUITEM, parent);
    g->label = label;
    g->mnemonic = mnemonic;
    AddButtonCB(g, cb, data);
    menu_changed(g);
    return g;
}

Widget CreateMenuCloseButton(Widget parent, Widget form)
{
    return CreateMenuButton(parent,
        (char *) "Close", 'C', destroy_dialog_cb, GetParent(form));
}

Widget CreateMenuToggle(Widget parent, char *label, char mnemonic,
	TB_CBProc cb, void *data)
{
    GW *g = new GW(GK_MENUTOGGLE, parent);
    g->label = label;
    g->mnemonic = mnemonic;
    if (cb) {
        AddToggleButtonCB(g, cb, data);
    }
    menu_changed(g);
    return g;
}

Widget CreateMenuLabel(Widget parent, char *name)
{
    GW *g = new GW(GK_MENULABEL, parent);
    g->label = name;
    menu_changed(g);
    return g;
}

/* ------------------------------------------------------------------ */
/* help */

void AddHelpCB(Widget w, char *ha)
{
    if (w) {
        w->help = ha;
    }
}

static void help_for(GW *g)
{
    for (; g; g = g->parent) {
        if (g->help) {
            HelpCB(g->help);
            return;
        }
    }
    HelpCB(NULL);
}

static GW *gw_of(Fl_Widget *w)
{
    for (; w; w = w->parent()) {
        std::map<Fl_Widget *, GW *>::iterator it = widget_map.find(w);
        if (it != widget_map.end()) {
            return it->second;
        }
    }
    return NULL;
}

static bool context_help_mode = false;

static Fl_Widget *widget_at(Fl_Group *g, int x, int y)
{
    for (int i = g->children() - 1; i >= 0; i--) {
        Fl_Widget *c = g->child(i);
        if (c->visible() && x >= c->x() && x < c->x() + c->w() &&
            y >= c->y() && y < c->y() + c->h()) {
            if (c->as_group() && !c->as_window()) {
                Fl_Widget *d = widget_at(c->as_group(), x, y);
                return d ? d : c;
            }
            return c;
        }
    }
    return NULL;
}

static int help_dispatch(int event, Fl_Window *win)
{
    if (context_help_mode && event == FL_PUSH) {
        Fl_Widget *w;
        context_help_mode = false;
        for (Fl_Window *ww = Fl::first_window(); ww; ww = Fl::next_window(ww)) {
            ww->cursor(FL_CURSOR_DEFAULT);
        }
        w = widget_at(win, Fl::event_x(), Fl::event_y());
        help_for(gw_of(w ? w : win));
        return 1;
    }
    if (event == FL_KEYBOARD && Fl::event_key() == FL_F + 1 &&
        dynamic_cast<GWindow *>(win)) {
        help_for(gw_of(Fl::focus() ? Fl::focus() : win));
        return 1;
    }
    return Fl::handle_(event, win);
}

void ContextHelpCB(void *data)
{
    static bool installed = false;
    if (!installed) {
        Fl::event_dispatch(help_dispatch);
        installed = true;
    }
    context_help_mode = true;
    for (Fl_Window *w = Fl::first_window(); w; w = Fl::next_window(w)) {
        w->cursor(FL_CURSOR_HELP);
    }
}

void fl_install_help_keys(void)
{
    Fl::event_dispatch(help_dispatch);
}

/* ------------------------------------------------------------------ */
/* file selection box */

struct FSBData {
    FSBStructure *fsb;
    GW *filter;         /* text: directory + pattern */
    GW *browser;
    GW *selection;      /* text */
    std::string dir;
    std::string pattern;
    FSB_CBProc cbproc;
    void *anydata;
};

static const struct {
    const char *title;
    const char *pattern;
} fsb_patterns[] = {
    {"Read history",            "*.com"},
    {"Open project",            "*.agr"},
    {"Save project",            "*.agr"},
    {"Read sets",               "*.dat"},
    {"Write sets",              "*.dat"},
    {"Read parameters",         "*.par"},
    {"Write parameters",        "*.par"},
    {"Select netCDF file",      "*.nc"},
    {"Select hot link file",    "*.dat"},
    {"Open fit parameter file", "*.fit"},
    {"Save fit parameter file", "*.fit"}
};

static void fsb_reload(FSBData *d)
{
    Fl_File_Browser *b = (Fl_File_Browser *) d->browser->w;
    std::string f;

    if (d->dir.empty() || d->dir[d->dir.size() - 1] != '/') {
        d->dir += '/';
    }
    b->filter(d->pattern.empty() ? "*" : d->pattern.c_str());
    b->load(d->dir.c_str());
    /* don't show "./" */
    for (int i = 1; i <= b->size(); i++) {
        if (b->text(i) && strcmp(b->text(i), "./") == 0) {
            b->remove(i);
            break;
        }
    }
    f = d->dir + (d->pattern.empty() ? "*" : d->pattern);
    xv_setstr(d->filter, (char *) f.c_str());
}

static void fsb_apply_filter(FSBData *d)
{
    std::string f = get_text(d->filter);
    size_t slash = f.rfind('/');
    if (slash == std::string::npos) {
        d->pattern = f;
    } else {
        d->dir = f.substr(0, slash + 1);
        d->pattern = f.substr(slash + 1);
    }
    fsb_reload(d);
}

static void fsb_filter_cb(void *data)
{
    fsb_apply_filter((FSBData *) data);
}

static void fsb_ok_cb(void *data)
{
    FSBData *d = (FSBData *) data;
    std::string s = get_text(d->selection);
    int ok;

    if (s.empty()) {
        return;
    }
    if (fl_filename_isdir(s.c_str())) {
        d->dir = s;
        fsb_reload(d);
        return;
    }
    if (d->cbproc == NULL) {
        return;
    }
    set_wait_cursor();
    ok = d->cbproc((char *) s.c_str(), d->anydata);
    if (ok) {
        UnmanageChild(d->fsb->dialog);
    }
    unset_wait_cursor();
}

static void fsb_browser_cb(Fl_Widget *w, void *data)
{
    FSBData *d = (FSBData *) data;
    Fl_File_Browser *b = (Fl_File_Browser *) w;
    int line = b->value();
    const char *t;
    std::string path;

    if (line <= 0 || (t = b->text(line)) == NULL) {
        return;
    }
    if (strcmp(t, "../") == 0) {
        /* go up */
        std::string dir = d->dir;
        if (dir.size() > 1) {
            size_t p = dir.rfind('/', dir.size() - 2);
            if (p != std::string::npos) {
                dir = dir.substr(0, p + 1);
            }
        }
        path = dir;
    } else {
        path = d->dir + t;
    }
    xv_setstr(d->selection, (char *) path.c_str());
    if (Fl::event_clicks() > 0) {
        Fl::event_clicks(0);
        if (fl_filename_isdir(path.c_str())) {
            d->dir = path;
            Fl::add_timeout(0.0, [](void *p) { fsb_reload((FSBData *) p); }, d);
        } else {
            fsb_ok_cb(d);
        }
    }
}

static void fsb_browse_cb(void *data)
{
    FSBData *d = (FSBData *) data;
    Fl_Native_File_Chooser fc;

    fc.title(((Fl_Window *) d->fsb->dialog->w)->label());
    fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
    if (!d->pattern.empty() && d->pattern != "*") {
        std::string f = std::string(d->pattern) + "\nAll files\t*";
        fc.filter(f.c_str());
    }
    fc.directory(d->dir.c_str());
    if (fc.show() == 0) {
        std::string fn = fc.filename();
        size_t slash = fn.rfind('/');
        xv_setstr(d->selection, (char *) fn.c_str());
        if (slash != std::string::npos) {
            d->dir = fn.substr(0, slash + 1);
            fsb_reload(d);
        }
        fsb_ok_cb(d);
    }
}

#define FSB_CWD     0
#define FSB_HOME    1
#define FSB_ROOT    2

static void fsb_cd_cb(int value, void *data)
{
    FSBData *d = (FSBData *) data;
    switch (value) {
    case FSB_CWD:
        d->dir = get_workingdir();
        break;
    case FSB_HOME:
        d->dir = get_userhome();
        break;
    case FSB_ROOT:
        d->dir = "/";
        break;
    default:
        return;
    }
    fsb_reload(d);
}

static void fsb_setcwd_cb(void *data)
{
    FSBData *d = (FSBData *) data;
    set_workingdir(d->dir.c_str());
}

static void fsb_cancel_cb(void *data)
{
    FSBData *d = (FSBData *) data;
    UnmanageChild(d->fsb->dialog);
}

FSBStructure *CreateFileSelectionBox(Widget parent, char *s)
{
    FSBStructure *retval;
    FSBData *d;
    GW *form, *rc, *b;
    Fl_File_Browser *fb;
    OptionStructure *opt;
    Widget button, buts[5];
    static OptionItem fsb_items[] = {
        {FSB_CWD,  (char *) "Cwd"},
        {FSB_HOME, (char *) "Home"},
        {FSB_ROOT, (char *) "/"}
    };
    char *labels[5] = {(char *) "OK", (char *) "Filter", (char *) "Browse...",
                       (char *) "Cancel", (char *) "Help"};
    size_t i;

    retval = (FSBStructure *) xmalloc(sizeof(FSBStructure));
    d = new FSBData;
    d->fsb = retval;
    d->cbproc = NULL;
    d->anydata = NULL;
    d->dir = get_workingdir();
    d->pattern = "*";
    for (i = 0; i < sizeof(fsb_patterns)/sizeof(fsb_patterns[0]); i++) {
        if (strcmp(s, fsb_patterns[i].title) == 0) {
            d->pattern = fsb_patterns[i].pattern;
        }
    }

    form = (GW *) CreateDialogForm(parent, s);
    retval->FSB = form;
    retval->dialog = form->parent;
    AddHelpCB(form, (char *) "doc/UsersGuide.html#FS-dialog");

    rc = (GW *) CreateHContainer(form);
    CreateLabel(rc, (char *) "Filter:");
    d->filter = new_text_leaf(rc, 30);
    d->filter->text_cbs.push_back(ButtonCB{fsb_filter_cb, d});
    AddDialogFormChild(form, rc);

    begin_in(form);
    fb = new Fl_File_Browser(0, 0, 10, 10);
    end_in();
    fb->type(FL_HOLD_BROWSER);
    fb->filetype(Fl_File_Browser::FILES);
    fb->iconsize((uchar) (FL_NORMAL_SIZE + 2));
    b = new_leaf(form, LT_FILEBROWSER, fb);
    fb->callback(fsb_browser_cb, d);
    fb->when(FL_WHEN_CHANGED | FL_WHEN_NOT_CHANGED);
    d->browser = b;
    AddDialogFormChild(form, b);

    rc = (GW *) CreateHContainer(form);
    CreateLabel(rc, (char *) "Selection:");
    d->selection = new_text_leaf(rc, 30);
    d->selection->text_cbs.push_back(ButtonCB{fsb_ok_cb, d});
    rc->form_bottom = true;

    /* the work area (user additions go here) */
    retval->rc = CreateVContainer(form);
    retval->rc->form_bottom = true;
    rc = (GW *) CreateHContainer(retval->rc);
    opt = CreateOptionChoice(rc, (char *) "Chdir to:", 1, 3, fsb_items);
    AddOptionChoiceCB(opt, fsb_cd_cb, d);
    button = CreateButton(rc, (char *) "Set as cwd");
    AddButtonCB(button, fsb_setcwd_cb, d);

    rc = (GW *) CreateCommandButtons(form, 5, buts, labels);
    rc->form_bottom = true;
    AddButtonCB(buts[0], fsb_ok_cb, d);
    AddButtonCB(buts[1], fsb_filter_cb, d);
    AddButtonCB(buts[2], fsb_browse_cb, d);
    AddButtonCB(buts[3], fsb_cancel_cb, d);
    AddButtonCB(buts[4], HelpCB, (void *) "doc/UsersGuide.html#FS-dialog");

    form->owner = d;
    fsb_reload(d);

    return retval;
}

void AddFileSelectionBoxCB(FSBStructure *fsb, FSB_CBProc cbproc, void *anydata)
{
    FSBData *d = (FSBData *) ((GW *) fsb->FSB)->owner;
    d->cbproc = cbproc;
    d->anydata = anydata;
}

void SetFileSelectionBoxPattern(FSBStructure *fsb, char *pattern)
{
    FSBData *d = (FSBData *) ((GW *) fsb->FSB)->owner;
    if (pattern != NULL) {
        d->pattern = pattern;
        fsb_reload(d);
    }
}

/* ------------------------------------------------------------------ */
/* restriction choice */

RestrictionStructure *CreateRestrictionChoice(Widget parent, char *s)
{
    RestrictionStructure *retval;
    Widget rc;
    OptionItem restr_items[7];

    restr_items[0].value = RESTRICT_NONE;
    restr_items[0].label = (char *) "None";
    restr_items[1].value = RESTRICT_REG0;
    restr_items[1].label = (char *) "Region 0";
    restr_items[2].value = RESTRICT_REG1;
    restr_items[2].label = (char *) "Region 1";
    restr_items[3].value = RESTRICT_REG2;
    restr_items[3].label = (char *) "Region 2";
    restr_items[4].value = RESTRICT_REG3;
    restr_items[4].label = (char *) "Region 3";
    restr_items[5].value = RESTRICT_REG4;
    restr_items[5].label = (char *) "Region 4";
    restr_items[6].value = RESTRICT_WORLD;
    restr_items[6].label = (char *) "Inside graph";

    retval = (RestrictionStructure *) xmalloc(sizeof(RestrictionStructure));

    retval->frame = CreateFrame(parent, s);
    rc = CreateHContainer(retval->frame);

    retval->r_sel = CreateOptionChoice(rc,
        (char *) "Restriction:", 1, 7, restr_items);
    retval->negate = CreateToggleButton(rc, (char *) "Negated");

    return retval;
}

/* ------------------------------------------------------------------ */
/* custom FLTK widgets in dialogs (C++ only, see fl_widgets.h) */

void fl_begin_in(Widget parent)
{
    begin_in(parent);
}

void fl_end_in(void)
{
    end_in();
}

Widget fl_wrap_widget(Widget parent, Fl_Widget *w, int pref_w, int pref_h,
    int hexp, int vexp)
{
    GW *g = new_leaf(parent, LT_CUSTOM, w);
    g->custom_w = pref_w;
    g->custom_h = pref_h;
    g->custom_hexp = hexp ? true:false;
    g->custom_vexp = vexp ? true:false;
    return g;
}

Fl_Widget *fl_widget_of(Widget w)
{
    return w ? w->w : NULL;
}

void fl_set_text_changed_cb(Widget w, void (*cb)(void *), void *data)
{
    if (w && w->ltype == LT_TEXT) {
        Fl_Input *in = (Fl_Input *) w->w;
        ButtonCB c = {cb, data};
        w->text_cbs.clear();
        w->text_cbs.push_back(c);
        in->when(FL_WHEN_CHANGED);
    }
}

/* ------------------------------------------------------------------ */
/* more helpers (see motifinc.h) */

int IsSensitive(Widget w)
{
    return (w && w->sensitive) ? TRUE:FALSE;
}

Widget CreateDialogShell(Widget parent, char *s)
{
    GW *shell;
    GWindow *win;
    
    shell = new GW(GK_SHELL, NULL);
    win = new GWindow(300, 200, shell);
    win->end();
    win->copy_label(s);
    win->callback(shell_close_cb);
    win->resizable(win);
    shell->w = win;
    shell->managed = false;
    register_widget(win, shell);
    
    return shell;
}

Widget CreateDialogRC(Widget shell)
{
    GW *g = new GW(GK_VBOX, shell);
    g->managed = false;
    return g;
}

Widget CreateColumnGrid(Widget parent, int nrows)
{
    GW *g = new GW(GK_RCGRID, parent);
    g->nrows = MAX2(nrows, 1);
    schedule_layout(g);
    return g;
}

void AddDialogFormBottomChild(Widget form, Widget child)
{
    if (form && child) {
        child->form_bottom = true;
        schedule_layout(form);
    }
}

/* ------------------------------------------------------------------ */
/* simple tables of strings */

class GTable : public Fl_Table {
public:
    std::vector<std::vector<std::string> > cells;
    std::vector<std::string> collabels, rowlabels;
    bool editable;
    Fl_Input *editor;
    int erow, ecol;
    int visrows, viscols;
    
    GTable(int nr, int nc, int vr, int vc, bool ed);
    void set_size(int nr, int nc);
    void draw_cell(TableContext context, int R, int C, int X, int Y,
        int W, int H);
    void start_edit(int R, int C);
    void done_edit(void);
    int handle(int event);
    void resize(int X, int Y, int W, int H) {
        done_edit();
        Fl_Table::resize(X, Y, W, H);
    }
};

static void table_editor_cb(Fl_Widget *w, void *data)
{
    GTable *t = (GTable *) data;
    if (t->erow >= 0) {
        t->cells[t->erow][t->ecol] = ((Fl_Input *) w)->value();
    }
    if (Fl::event() == FL_KEYBOARD && (Fl::event_key() == FL_Enter ||
        Fl::event_key() == FL_KP_Enter)) {
        int R = t->erow, C = t->ecol;
        t->done_edit();
        if (R + 1 < t->rows()) {
            t->start_edit(R + 1, C);
        }
    }
}

static void table_cb(Fl_Widget *w, void *)
{
    GTable *t = (GTable *) w;
    if (t->editable && t->callback_context() == Fl_Table::CONTEXT_CELL &&
        Fl::event() == FL_PUSH) {
        t->start_edit(t->callback_row(), t->callback_col());
    }
}

GTable::GTable(int nr, int nc, int vr, int vc, bool ed) :
    Fl_Table(0, 0, 10, 10), editable(ed), erow(-1), ecol(-1),
    visrows(vr), viscols(vc)
{
    end();
    begin();
    editor = new Fl_Input(0, 0, 10, 10);
    editor->hide();
    editor->box(FL_FLAT_BOX);
    editor->callback(table_editor_cb, this);
    editor->when(FL_WHEN_CHANGED | FL_WHEN_ENTER_KEY_ALWAYS);
    end();
    row_header(1);
    col_header(1);
    row_height_all(text_h() + 6);
    col_header_height(text_h() + 6);
    row_header_width(4*char_width() + 8);
    col_width_all(10*char_width() + 8);
    color(FL_BACKGROUND2_COLOR);
    callback(table_cb);
    when(FL_WHEN_CHANGED | FL_WHEN_RELEASE);
    set_size(nr, nc);
}

void GTable::set_size(int nr, int nc)
{
    cells.resize(nr);
    for (int i = 0; i < nr; i++) {
        cells[i].resize(nc);
    }
    collabels.resize(nc);
    rowlabels.resize(nr);
    rows(nr);
    cols(nc);
}

void GTable::start_edit(int R, int C)
{
    int X, Y, W, H;
    done_edit();
    if (R < 0 || C < 0 || R >= rows() || C >= cols()) {
        return;
    }
    erow = R;
    ecol = C;
    find_cell(CONTEXT_CELL, R, C, X, Y, W, H);
    editor->resize(X, Y, W, H);
    editor->value(cells[R][C].c_str());
    editor->insert_position(0, editor->size());
    editor->show();
    editor->take_focus();
    redraw();
}

void GTable::done_edit(void)
{
    if (erow >= 0) {
        cells[erow][ecol] = editor->value();
        erow = ecol = -1;
        editor->hide();
        redraw();
    }
}

int GTable::handle(int event)
{
    if (event == FL_PUSH && erow >= 0 && !Fl::event_inside(editor)) {
        done_edit();
    }
    return Fl_Table::handle(event);
}

void GTable::draw_cell(TableContext context, int R, int C, int X, int Y,
    int W, int H)
{
    switch (context) {
    case CONTEXT_STARTPAGE:
        fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
        return;
    case CONTEXT_COL_HEADER:
        fl_push_clip(X, Y, W, H);
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_BACKGROUND_COLOR);
        fl_color(FL_FOREGROUND_COLOR);
        if (C < (int) collabels.size()) {
            fl_draw(collabels[C].c_str(), X, Y, W, H, FL_ALIGN_CENTER);
        }
        fl_pop_clip();
        return;
    case CONTEXT_ROW_HEADER:
        fl_push_clip(X, Y, W, H);
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_BACKGROUND_COLOR);
        fl_color(FL_FOREGROUND_COLOR);
        if (R < (int) rowlabels.size()) {
            fl_draw(rowlabels[R].c_str(), X, Y, W, H, FL_ALIGN_CENTER);
        }
        fl_pop_clip();
        return;
    case CONTEXT_CELL:
        if (R == erow && C == ecol) {
            return;
        }
        fl_push_clip(X, Y, W, H);
        fl_color((!editable && (R % 2)) ?
            fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, 0.6) :
            FL_BACKGROUND2_COLOR);
        fl_rectf(X, Y, W, H);
        fl_color(FL_FOREGROUND_COLOR);
        fl_draw(cells[R][C].c_str(), X + 3, Y, W - 6, H,
            editable ? FL_ALIGN_LEFT : FL_ALIGN_RIGHT);
        fl_color(FL_LIGHT2);
        fl_rect(X, Y, W, H);
        fl_pop_clip();
        return;
    default:
        return;
    }
}

static GTable *table_of(Widget w)
{
    return (w && w->w) ? dynamic_cast<GTable *>(w->w) : NULL;
}

static void table_update_size(GW *g)
{
    GTable *t = (GTable *) g->w;
    int i, W = t->row_header_width() + Fl::scrollbar_size() + 4;
    for (i = 0; i < MIN2(t->viscols, t->cols()); i++) {
        W += t->col_width(i);
    }
    g->custom_w = W;
    g->custom_h = t->col_header_height() +
        MAX2(t->visrows, 1)*t->row_height(0) + Fl::scrollbar_size() + 4;
    schedule_layout(g);
}

Widget CreateTable(Widget parent, int nrows, int ncols,
    int visrows, int viscols, int editable)
{
    GTable *t;
    GW *g;
    
    begin_in(parent);
    t = new GTable(nrows, ncols, visrows, viscols, editable ? true:false);
    end_in();
    g = new_leaf(parent, LT_CUSTOM, t);
    t->callback(table_cb);
    g->custom_hexp = true;
    g->custom_vexp = true;
    table_update_size(g);
    
    return g;
}

void TableSetColLabels(Widget w, char **labels)
{
    GTable *t = table_of(w);
    if (t) {
        for (int i = 0; i < t->cols(); i++) {
            t->collabels[i] = labels[i] ? labels[i] : "";
        }
        t->redraw();
    }
}

void TableSetRowLabels(Widget w, char **labels)
{
    GTable *t = table_of(w);
    if (t) {
        int len = 1;
        for (int i = 0; i < t->rows(); i++) {
            t->rowlabels[i] = labels[i] ? labels[i] : "";
            len = MAX2(len, (int) t->rowlabels[i].size());
        }
        t->row_header_width((len + 1)*char_width() + 8);
        table_update_size(w);
        t->redraw();
    }
}

void TableSetColWidth(Widget w, int col, int width)
{
    GTable *t = table_of(w);
    if (t && col >= 0 && col < t->cols()) {
        t->col_width(col, width*char_width() + 8);
        table_update_size(w);
    }
}

void TableSetCell(Widget w, int row, int col, char *s)
{
    GTable *t = table_of(w);
    if (t && row >= 0 && row < t->rows() && col >= 0 && col < t->cols()) {
        t->cells[row][col] = s ? s:"";
        if (row == t->erow && col == t->ecol) {
            t->editor->value(s ? s:"");
        }
        t->redraw();
    }
}

char *TableGetCell(Widget w, int row, int col)
{
    static char *buf = NULL;
    GTable *t = table_of(w);
    if (t && row >= 0 && row < t->rows() && col >= 0 && col < t->cols()) {
        buf = copy_string(buf, (char *) t->cells[row][col].c_str());
        return buf;
    }
    return (char *) "";
}

int TableGetNumRows(Widget w)
{
    GTable *t = table_of(w);
    return t ? t->rows() : 0;
}

void TableSetNumRows(Widget w, int nrows, char **rowlabels)
{
    GTable *t = table_of(w);
    if (t) {
        int old = t->rows();
        t->done_edit();
        t->set_size(nrows, t->cols());
        for (int i = old; i < nrows; i++) {
            t->rowlabels[i] = rowlabels[i] ? rowlabels[i] : "";
        }
        t->redraw();
    }
}

void TableCommitEdit(Widget w)
{
    GTable *t = table_of(w);
    if (t) {
        t->done_edit();
    }
}
