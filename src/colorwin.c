/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 *
 * Written in 2026 for regrace; see README.md.
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
 * Colors: the color editor (with palettes) and the "color by value"
 * dialog. Written on the toolkit-neutral API of motifinc.h.
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "globals.h"
#include "utils.h"
#include "draw.h"
#include "graphs.h"
#include "colormaps.h"
#include "motifinc.h"
#include "protos.h"

/* ------------------------------------------------------------------ */
/* color editor */

typedef struct {
    Widget top;
    OptionStructure *color;
    Widget name;
    Widget red, green, blue;
    Widget swatch;
    OptionStructure *palette;
    int selected;       /* the color shown (the choice keeps a position) */
} ColorsUI;

static ColorsUI cui;

static void update_swatch(void)
{
    SetColorSwatch(cui.swatch, GetScaleValue(cui.red),
        GetScaleValue(cui.green), GetScaleValue(cui.blue));
}

static void show_color(int cindex)
{
    RGB *rgb = get_rgb(cindex);

    if (rgb == NULL) {
        return;
    }
    SetScaleValue(cui.red, rgb->red);
    SetScaleValue(cui.green, rgb->green);
    SetScaleValue(cui.blue, rgb->blue);
    xv_setstr(cui.name, get_colorname(cindex));
    update_swatch();
}

/* select a color and show it; one no longer in the table selects black */
static void select_color(int cindex)
{
    CMap_entry *pcmap = get_cmap_entry(cindex);

    if (pcmap == NULL || pcmap->ctype != COLOR_MAIN) {
        cindex = 1;
    }
    cui.selected = cindex;
    SetOptionChoice(cui.color, cindex);
    show_color(cindex);
}

static void color_selected_cb(int value, void *data)
{
    cui.selected = value;
    show_color(value);
}

static void rgb_changed_cb(int value, void *data)
{
    update_swatch();
}

static CMap_entry entry_from_dialog(int cindex)
{
    CMap_entry cmap;
    char *name = xv_getstr(cui.name);
    static char buf[32];

    cmap.rgb.red   = GetScaleValue(cui.red);
    cmap.rgb.green = GetScaleValue(cui.green);
    cmap.rgb.blue  = GetScaleValue(cui.blue);
    cmap.ctype = COLOR_MAIN;
    if (name == NULL || name[0] == '\0') {
        sprintf(buf, "color%d", cindex);
        cmap.cname = buf;
    } else {
        cmap.cname = name;
    }
    return cmap;
}

static void colors_changed(int cindex)
{
    set_dirtystate();
    update_all();
    select_color(cindex);
    xdrawgraph();
}

/* Apply/Accept: change the selected color */
static int colors_aac_cb(void *data)
{
    int cindex = GetOptionChoice(cui.color);

    if (cindex == 0 || cindex == 1) {
        if (!yesno("Change the background/foreground color?",
            NULL, NULL, NULL)) {
            return RETURN_FAILURE;
        }
    }
    if (store_color(cindex, entry_from_dialog(cindex)) != RETURN_SUCCESS) {
        errmsg("Failed changing the color");
        return RETURN_FAILURE;
    }
    colors_changed(cindex);

    return RETURN_SUCCESS;
}

static void add_color_cb(void *data)
{
    int cindex = number_of_colors();
    CMap_entry cmap;

    if (cindex >= MAXCOLORS) {
        errmsg("The color table is full");
        return;
    }
    cmap = entry_from_dialog(cindex);
    if (get_color_by_name(cmap.cname) != BAD_COLOR) {
        /* the name of the color it was made from: don't duplicate it */
        static char buf[32];
        sprintf(buf, "color%d", cindex);
        cmap.cname = buf;
    }
    if (store_color(cindex, cmap) != RETURN_SUCCESS) {
        errmsg("Failed adding the color");
        return;
    }
    colors_changed(cindex);
}

/* the color table changed elsewhere (e.g. undo): show the selection again */
void update_colors_frame(void)
{
    if (cui.top != NULL) {
        select_color(cui.selected);
    }
}

static void apply_palette_cb(void *data)
{
    int cindex = GetOptionChoice(cui.color);

    if (apply_palette(GetOptionChoice(cui.palette)) != RETURN_SUCCESS) {
        errmsg("Failed applying the palette");
        return;
    }
    colors_changed(cindex);
}

void create_colors_frame(void *data)
{
    set_wait_cursor();

    if (cui.top == NULL) {
        Widget panel, fr, rc, rc2, rc3, button;
        OptionItem *items;
        int i, n;

        cui.top = CreateDialogForm(app_shell, "Colors");
        panel = CreateVContainer(cui.top);

        fr = CreateFrame(panel, "Color");
        rc = CreateVContainer(fr);
        cui.color = CreateColorChoice(rc, "Color:");
        AddOptionChoiceCB(cui.color, color_selected_cb, NULL);
        cui.name = CreateTextItem2(rc, 20, "Name:");
        rc2 = CreateHContainer(rc);
        cui.swatch = CreateColorSwatch(rc2, 60, 60);
        rc3 = CreateVContainer(rc2);
        cui.red = CreateScale(rc3, "Red", 0, 255, 16);
        cui.green = CreateScale(rc3, "Green", 0, 255, 16);
        cui.blue = CreateScale(rc3, "Blue", 0, 255, 16);
        AddScaleCB(cui.red, rgb_changed_cb, NULL);
        AddScaleCB(cui.green, rgb_changed_cb, NULL);
        AddScaleCB(cui.blue, rgb_changed_cb, NULL);
        button = CreateButton(rc, "Add as a new color");
        AddButtonCB(button, add_color_cb, NULL);

        fr = CreateFrame(panel, "Palette");
        rc = CreateHContainer(fr);
        n = number_of_palettes();
        items = xmalloc(n*sizeof(OptionItem));
        for (i = 0; i < n; i++) {
            items[i].value = i;
            items[i].label = palette_name(i);
        }
        cui.palette = CreateOptionChoice(rc, "Palette:", 1, n, items);
        xfree(items);
        button = CreateButton(rc, "Apply palette");
        AddButtonCB(button, apply_palette_cb, NULL);

        CreateAACDialog(cui.top, panel, colors_aac_cb, NULL);

        cui.selected = 2;
    }
    select_color(cui.selected);

    RaiseWindow(GetParent(cui.top));
    unset_wait_cursor();
}

/* ------------------------------------------------------------------ */
/* color by value */

typedef struct {
    Widget top;
    ListStructure *set;
    OptionStructure *col;
    OptionStructure *map;
    Widget reverse;
    SpinStructure *ncolors;
    Widget vmin, vmax;
    OptionStructure *scale;
    OptionStructure *result;
    Widget hide;
    Widget colorbar;
} CbvUI;

static CbvUI vui;

static int selected_set(void)
{
    int setno;
    if (GetSingleListChoice(vui.set, &setno) == RETURN_SUCCESS) {
        return setno;
    } else {
        return -1;
    }
}

/* fill the range from the data */
static void auto_range_cb(void *data)
{
    double vmin, vmax;
    char buf[64];
    int setno = selected_set();

    if (setno < 0) {
        return;
    }
    if (get_column_range(get_cg(), setno, GetOptionChoice(vui.col),
            GetOptionChoice(vui.scale), &vmin, &vmax) == RETURN_SUCCESS) {
        sprintf(buf, "%g", vmin);
        xv_setstr(vui.vmin, buf);
        sprintf(buf, "%g", vmax);
        xv_setstr(vui.vmax, buf);
    }
}

static void set_selected_cb(int n, int *values, void *data)
{
    int setno;

    if (n != 1) {
        return;
    }
    setno = values[0];
    /* by default, the column after X and Y */
    if (is_valid_setno(get_cg(), setno) &&
        GetOptionChoice(vui.col) >= dataset_cols(get_cg(), setno)) {
        SetOptionChoice(vui.col, dataset_cols(get_cg(), setno) - 1);
    }
    auto_range_cb(NULL);
}

static void range_changed_cb(int value, void *data)
{
    auto_range_cb(NULL);
}

static void result_changed_cb(int value, void *data)
{
    SetSensitive(vui.hide, value);
}

static int cbv_aac_cb(void *data)
{
    ColorByValue cbv;
    int setno = selected_set();

    if (setno < 0) {
        errmsg("Please select a single set");
        return RETURN_FAILURE;
    }

    cbv.col = GetOptionChoice(vui.col);
    cbv.map = GetOptionChoice(vui.map);
    cbv.reverse = GetToggleButtonState(vui.reverse);
    cbv.ncolors = (int) GetSpinChoice(vui.ncolors);
    cbv.logscale = GetOptionChoice(vui.scale);
    cbv.new_set = GetOptionChoice(vui.result);
    cbv.hide_source = GetToggleButtonState(vui.hide);
    cbv.colorbar = GetToggleButtonState(vui.colorbar);
    if (xv_evalexpr(vui.vmin, &cbv.vmin) != RETURN_SUCCESS ||
        xv_evalexpr(vui.vmax, &cbv.vmax) != RETURN_SUCCESS) {
        errmsg("Can't parse the range");
        return RETURN_FAILURE;
    }

    if (color_by_value(get_cg(), setno, &cbv) < 0) {
        return RETURN_FAILURE;
    }

    update_all();
    xdrawgraph();

    return RETURN_SUCCESS;
}

void create_colorbyvalue_frame(void *data)
{
    set_wait_cursor();

    if (vui.top == NULL) {
        Widget panel, fr, rc, rc2, button;
        OptionItem items[MAX_SET_COLS];
        OptionItem *mitems;
        int i, n;

        vui.top = CreateDialogForm(app_shell, "Color by value");
        panel = CreateVContainer(vui.top);

        vui.set = CreateSetChoice(panel, "Set:", LIST_TYPE_SINGLE, TRUE);
        AddListChoiceCB(vui.set, set_selected_cb, NULL);

        for (i = 0; i < MAX_SET_COLS; i++) {
            items[i].value = i;
            items[i].label = dataset_colname(i);
        }
        vui.col = CreateOptionChoice(panel, "Values from column:",
            1, MAX_SET_COLS, items);
        SetOptionChoice(vui.col, 2);
        AddOptionChoiceCB(vui.col, range_changed_cb, NULL);

        fr = CreateFrame(panel, "Color map");
        rc = CreateVContainer(fr);
        rc2 = CreateHContainer(rc);
        n = number_of_colormaps();
        mitems = xmalloc(n*sizeof(OptionItem));
        for (i = 0; i < n; i++) {
            mitems[i].value = i;
            mitems[i].label = colormap_name(i);
        }
        vui.map = CreateOptionChoice(rc2, "Map:", 1, n, mitems);
        xfree(mitems);
        vui.reverse = CreateToggleButton(rc2, "Reversed");
        vui.ncolors = CreateSpinChoice(rc, "Number of colors:", 4,
            SPIN_TYPE_INT, 2.0, (double) (MAXCOLORS/2), 8.0);
        SetSpinChoice(vui.ncolors, 64.0);

        fr = CreateFrame(panel, "Range of values");
        rc = CreateVContainer(fr);
        rc2 = CreateHContainer(rc);
        vui.vmin = CreateTextItem2(rc2, 10, "Min:");
        vui.vmax = CreateTextItem2(rc2, 10, "Max:");
        button = CreateButton(rc2, "From data");
        AddButtonCB(button, auto_range_cb, NULL);
        items[0].value = FALSE;
        items[0].label = "Linear";
        items[1].value = TRUE;
        items[1].label = "Logarithmic";
        vui.scale = CreateOptionChoice(rc, "Scale:", 1, 2, items);
        AddOptionChoiceCB(vui.scale, range_changed_cb, NULL);

        fr = CreateFrame(panel, "Result");
        rc = CreateVContainer(fr);
        items[0].value = TRUE;
        items[0].label = "A new set (a copy of the set)";
        items[1].value = FALSE;
        items[1].label = "The set itself (becomes XYCOLOR)";
        vui.result = CreateOptionChoice(rc, "Colored set:", 1, 2, items);
        AddOptionChoiceCB(vui.result, result_changed_cb, NULL);
        vui.hide = CreateToggleButton(rc, "Hide the original set");
        SetToggleButtonState(vui.hide, TRUE);
        vui.colorbar = CreateToggleButton(rc, "Add a color bar");
        SetToggleButtonState(vui.colorbar, TRUE);

        CreateAACDialog(vui.top, panel, cbv_aac_cb, NULL);
    }

    RaiseWindow(GetParent(vui.top));
    unset_wait_cursor();
}
