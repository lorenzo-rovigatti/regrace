/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 *
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 *
 * Copyright (c) 1996-2000 Grace Development Team
 * Modified in 2026 for regrace; see README.md.
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
 * Font tool (FLTK version of fontwin.c): a table of the glyphs of a
 * Type 1 font; clicking a glyph inserts it into the string.
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Bitmap.H>
#include <FL/Fl_Input.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>

#include <t1lib.h>

extern "C" {
#include "defines.h"
#include "t1fonts.h"
#include "utils.h"
#include "motifinc.h"
#include "protos.h"
}

#include "fl_gui.h"
#include "fl_widgets.h"

static Widget fonttool_frame = NULL;
static OptionStructure *font_select_item;
static TextStructure *string_item = NULL;

static Widget cstext_parent = NULL;

static int FontID = BAD_FONT_ID;
static BBox bbox;
static float Size = 16.8;

static int enable_edit_cb;

static T1_TMATRIX UNITY_MATRIX = {1.0, 0.0, 0.0, 1.0};

#define NCELLS  16

static void insert_into_string(char *s)
{
    int pos;

    pos = GetTextCursorPos(string_item);
    TextInsert(string_item, pos, s);
}

/* the glyph table (the Xbae matrix of the Motif version) */
class GlyphTable : public Fl_Widget {
public:
    Fl_Bitmap *glyphs[256];
    int hshift[256], vshift[256];
    bool valid[256];
    int selected;

    GlyphTable() : Fl_Widget(0, 0, 10, 10), selected(-1) {
        memset(glyphs, 0, sizeof(glyphs));
        memset(valid, 0, sizeof(valid));
        box(FL_DOWN_BOX);
        color(FL_BACKGROUND2_COLOR);
    }

    int cell_w() const { return (w() - 4)/NCELLS; }
    int cell_h() const { return (h() - 4)/NCELLS; }

    void clear_glyphs() {
        for (int i = 0; i < 256; i++) {
            delete glyphs[i];
            glyphs[i] = NULL;
            valid[i] = false;
        }
    }

    void load(void);
    void draw(void);
    int handle(int event);
};

void GlyphTable::load(void)
{
    int c;

    clear_glyphs();
    if (FontID == BAD_FONT_ID) {
        return;
    }

    for (c = 0; c < 256; c++) {
        GLYPH *glyph = T1_SetChar(FontID, (char) c, Size, &UNITY_MATRIX);
        if (glyph != NULL && glyph->bits != NULL) {
            int height = glyph->metrics.ascent - glyph->metrics.descent;
            int width = glyph->metrics.rightSideBearing -
                        glyph->metrics.leftSideBearing;
            int pad = T1_GetBitmapPad();
            int line_in = ((width + pad - 1)/pad)*pad/8;
            int line_out = (width + 7)/8;
            unsigned char *bits = new unsigned char[line_out*height + 1];
            for (int k = 0; k < height; k++) {
                memcpy(bits + k*line_out, glyph->bits + k*line_in, line_out);
            }
            glyphs[c] = new Fl_Bitmap(bits, width, height);
            glyphs[c]->alloc_array = 1;
            hshift[c] = MAX2(glyph->metrics.leftSideBearing - bbox.llx, 0);
            vshift[c] = MAX2(bbox.ury - glyph->metrics.ascent, 0);
            valid[c] = true;
        } else {
            valid[c] = (c == ' ');
        }
    }
}

void GlyphTable::draw(void)
{
    int i, j, cw = cell_w(), ch = cell_h();

    draw_box();
    fl_push_clip(x() + 2, y() + 2, w() - 4, h() - 4);
    for (j = 0; j < NCELLS; j++) {
        for (i = 0; i < NCELLS; i++) {
            int c = NCELLS*j + i;
            int cx = x() + 2 + i*cw, cy = y() + 2 + j*ch;
            fl_draw_box(FL_ENGRAVED_FRAME, cx, cy, cw, ch,
                (c == selected) ? FL_SELECTION_COLOR : color());
            if (c == selected) {
                fl_rectf(cx + 2, cy + 2, cw - 4, ch - 4, FL_SELECTION_COLOR);
            }
            if (glyphs[c]) {
                fl_color(c == selected ?
                    fl_contrast(FL_FOREGROUND_COLOR, FL_SELECTION_COLOR) :
                    FL_FOREGROUND_COLOR);
                glyphs[c]->draw(cx + 3 + hshift[c], cy + 3 + vshift[c]);
            }
        }
    }
    fl_pop_clip();
}

int GlyphTable::handle(int event)
{
    if (event == FL_PUSH) {
        int i = (Fl::event_x() - x() - 2)/cell_w();
        int j = (Fl::event_y() - y() - 2)/cell_h();
        if (i >= 0 && i < NCELLS && j >= 0 && j < NCELLS) {
            int c = NCELLS*j + i;
            selected = c;
            redraw();
            if (valid[c]) {
                char s[16];
                /* TODO: check for c being displayable in the GUI font */
                if (c > 31 && c < 127) {
                    s[0] = (char) c;
                    s[1] = '\0';
                } else {
                    sprintf(s, "\\#{%02x}", c);
                }
                insert_into_string(s);
            } else {
                fl_beep();
            }
        }
        return 1;
    }
    return Fl_Widget::handle(event);
}

static GlyphTable *font_table = NULL;

static void update_fonttool_cb(int value, void *data)
{
    char *buf;
    int csize, bsize;

    FontID = value;
    switch (T1_CheckForFontID(FontID)) {
    case 0:
        T1_LoadFont(FontID);
        break;
    case -1:
        errmsg("Couldn't load font");
        FontID = BAD_FONT_ID;
        return;
        break;
    default:
        break;
    }

    bbox = T1_GetFontBBox(FontID);
    /* check if bbox is zero or invalid and then calculate it ourselves */
    if (bbox.llx >= bbox.urx || bbox.lly >= bbox.ury) {
        int c;
        memset(&bbox, 0, sizeof(bbox));
        for (c = 0; c < 256; c++) {
            BBox bbox_tmp = T1_GetCharBBox(FontID, c);
            bbox.llx = MIN2(bbox.llx, bbox_tmp.llx);
            bbox.lly = MIN2(bbox.lly, bbox_tmp.lly);
            bbox.urx = MAX2(bbox.urx, bbox_tmp.urx);
            bbox.ury = MAX2(bbox.ury, bbox_tmp.ury);
        }
    }

    csize = MIN2(font_table->cell_w(), font_table->cell_h()) - 6;
    if (csize < 8) {
        csize = 22;
    }
    bsize = MAX2(bbox.urx - bbox.llx, bbox.ury - bbox.lly);
    Size  = floor(1000.0*csize/bsize);

    bbox.llx = (int) (bbox.llx*Size/1000);
    bbox.lly = (int) (bbox.lly*Size/1000);
    bbox.urx = (int) (bbox.urx*Size/1000);
    bbox.ury = (int) (bbox.ury*Size/1000);

    font_table->load();
    font_table->redraw();

    buf = copy_string(NULL, "\\f{");
    buf = concat_strings(buf, get_fontalias(FontID));
    buf = concat_strings(buf, "}");
    insert_into_string(buf);
    xfree(buf);
}

/* highlight the glyph of the character just typed */
static void EditStringCB(void *data)
{
    const char *s;
    int pos;
    Fl_Input *in;

    if (enable_edit_cb != TRUE) {
        return;
    }
    in = (Fl_Input *) fl_widget_of(string_item->text);
    s = in->value();
    pos = in->insert_position();
    if (pos > 0 && s && (int) strlen(s) >= pos) {
        font_table->selected = (unsigned char) s[pos - 1];
        font_table->redraw();
    }
}

static void fonttool_aac_cb(void *data)
{
    int aac_mode;

    aac_mode = (int) (long) data;

    if (aac_mode == AAC_CLOSE) {
        UnmanageChild(fonttool_frame);
        if (cstext_parent != NULL) {
            SetSensitive(cstext_parent, True);
        }
        return;
    }

    if (cstext_parent != NULL) {
        xv_setstr(cstext_parent, GetTextString(string_item));
    }

    if (aac_mode == AAC_ACCEPT) {
        UnmanageChild(fonttool_frame);
        if (cstext_parent != NULL) {
            SetSensitive(cstext_parent, True);
        }
    }
}

extern "C" void create_fonttool_cb(void *data)
{
    create_fonttool((Widget) data);
}

extern "C" void create_fonttool(Widget cstext)
{
    Widget fonttool_panel, aac_buts;
    int cell = FL_NORMAL_SIZE*2;

    if (string_item != NULL && cstext == string_item->text) {
        /* avoid recursion */
        return;
    }

    if (cstext_parent != NULL) {
        /* unlock previous parent */
        SetSensitive(cstext_parent, True);
    }

    cstext_parent = cstext;

    if (fonttool_frame == NULL) {
        fonttool_panel = CreateDialogForm(app_shell, (char *) "Font tool");
        fonttool_frame = GetParent(fonttool_panel);

        font_select_item = CreateFontChoice(fonttool_panel, (char *) "Font:");
        AddDialogFormChild(fonttool_panel, font_select_item->menu);

        fl_begin_in(fonttool_panel);
        font_table = new GlyphTable();
        fl_end_in();
        AddDialogFormChild(fonttool_panel,
            fl_wrap_widget(fonttool_panel, font_table,
                NCELLS*cell + 4, NCELLS*cell + 4, TRUE, TRUE));

        AddOptionChoiceCB(font_select_item, update_fonttool_cb, NULL);

        string_item = CreateCSText(fonttool_panel, (char *) "CString:");
        AddDialogFormChild(fonttool_panel, string_item->form);
        fl_set_text_changed_cb(string_item->text, EditStringCB, NULL);

        aac_buts = CreateAACButtons(fonttool_panel,
            fonttool_panel, fonttool_aac_cb);
        AddDialogFormChild(fonttool_panel, aac_buts);

        ManageChild(fonttool_panel);
        font_table->size(NCELLS*cell + 4, NCELLS*cell + 4);
        update_fonttool_cb(0, NULL);
    }

    enable_edit_cb = FALSE;
    if (cstext_parent == NULL) {
        SetTextString(string_item, (char *) "");
    } else {
        SetTextString(string_item, xv_getstr(cstext_parent));
        /* Lock editable text */
        SetSensitive(cstext_parent, False);
    }
    enable_edit_cb = TRUE;

    RaiseWindow(fonttool_frame);
}
