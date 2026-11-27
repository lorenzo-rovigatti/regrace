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
 * Palettes, colour maps and colouring of data by value
 *
 * Everything here works with the ordinary colour table of the project,
 * so the results are saved as "map color" lines and projects remain
 * readable by any Grace 5.1.
 *
 *  - a palette replaces the main colours 2, 3, ... (0 and 1 stay white
 *    and black);
 *  - a colour map is sampled into n colours, added to the colour table
 *    with names like "viridis_64_07" (so that applying it again reuses
 *    them);
 *  - colouring by value turns a set into an XYCOLOR one, whose third
 *    column holds the colour of each symbol, and can add a colour bar:
 *    a narrow graph next to the set's graph, whose Y axis gives the
 *    scale, filled with one box per colour.
 *
 */

#include <config.h>
#include <cmath.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "defines.h"
#include "globals.h"
#include "utils.h"
#include "draw.h"
#include "device.h"
#include "graphs.h"
#include "graphutils.h"
#include "protos.h"

#include "colormaps.h"

/* ------------------------------------------------------------------ */
/* palettes */

typedef struct {
    char *name;
    unsigned int rgb;
} NamedColor;

typedef struct {
    char *name;
    int n;
    NamedColor colors[16];
} Palette;

static Palette palettes[] = {
    {"Grace", 14, {
        {"red",       0xff0000}, {"green",     0x00ff00},
        {"blue",      0x0000ff}, {"yellow",    0xffff00},
        {"brown",     0xbc8f8f}, {"grey",      0xdcdcdc},
        {"violet",    0x9400d3}, {"cyan",      0x00ffff},
        {"magenta",   0xff00ff}, {"orange",    0xffa500},
        {"indigo",    0x7221bc}, {"maroon",    0x670748},
        {"turquoise", 0x40e0d0}, {"green4",    0x008b00}}},
    /* Okabe & Ito, "Color Universal Design" (colour-blind safe) */
    {"Okabe-Ito (colour-blind safe)", 8, {
        {"oi:orange",         0xe69f00}, {"oi:sky blue",      0x56b4e9},
        {"oi:bluish green",   0x009e73}, {"oi:yellow",        0xf0e442},
        {"oi:blue",           0x0072b2}, {"oi:vermillion",    0xd55e00},
        {"oi:reddish purple", 0xcc79a7}, {"oi:grey",          0x999999}}},
    /* Paul Tol's "bright" scheme (colour-blind safe) */
    {"Tol bright (colour-blind safe)", 7, {
        {"tol:blue",   0x4477aa}, {"tol:cyan",   0x66ccee},
        {"tol:green",  0x228833}, {"tol:yellow", 0xccbb44},
        {"tol:red",    0xee6677}, {"tol:purple", 0xaa3377},
        {"tol:grey",   0xbbbbbb}}},
    /* Paul Tol's "muted" scheme (colour-blind safe) */
    {"Tol muted (colour-blind safe)", 9, {
        {"tol:indigo", 0x332288}, {"tol:cyan",   0x88ccee},
        {"tol:teal",   0x44aa99}, {"tol:green",  0x117733},
        {"tol:olive",  0x999933}, {"tol:sand",   0xddcc77},
        {"tol:rose",   0xcc6677}, {"tol:wine",   0x882255},
        {"tol:purple", 0xaa4499}}},
    /* Tableau 10 (also matplotlib's default cycle) */
    {"Tableau 10", 10, {
        {"tab:blue",  0x1f77b4}, {"tab:orange", 0xff7f0e},
        {"tab:green", 0x2ca02c}, {"tab:red",    0xd62728},
        {"tab:purple",0x9467bd}, {"tab:brown",  0x8c564b},
        {"tab:pink",  0xe377c2}, {"tab:grey",   0x7f7f7f},
        {"tab:olive", 0xbcbd22}, {"tab:cyan",   0x17becf}}}
};

#define NPALETTES ((int) (sizeof(palettes)/sizeof(Palette)))

int number_of_palettes(void)
{
    return NPALETTES;
}

char *palette_name(int i)
{
    return (i >= 0 && i < NPALETTES) ? palettes[i].name : NULL;
}

static RGB rgb_of(unsigned int c)
{
    RGB rgb;
    rgb.red   = (c >> 16) & 0xff;
    rgb.green = (c >> 8) & 0xff;
    rgb.blue  = c & 0xff;
    return rgb;
}

int apply_palette(int i)
{
    int k;

    if (i < 0 || i >= NPALETTES) {
        return RETURN_FAILURE;
    }
    for (k = 0; k < palettes[i].n; k++) {
        CMap_entry cmap;
        cmap.rgb = rgb_of(palettes[i].colors[k].rgb);
        cmap.cname = palettes[i].colors[k].name;
        cmap.ctype = COLOR_MAIN;
        if (store_color(2 + k, cmap) != RETURN_SUCCESS) {
            return RETURN_FAILURE;
        }
    }
    set_dirtystate();

    return RETURN_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* colour maps */

typedef struct {
    char *name;
    int n;
    unsigned int rgb[11];
} ColorMap;

/* viridis, magma, inferno, plasma and cividis: Smith & van der Walt,
   and Nunez et al. (as sampled by viridisLite); RdBu: ColorBrewer;
   coolwarm: Moreland */
static ColorMap colormaps[] = {
    {"viridis", 10, {0x440154, 0x482878, 0x3e4a89, 0x31688e, 0x26828e,
                     0x1f9e89, 0x35b779, 0x6dcd59, 0xb4de2c, 0xfde725}},
    {"magma",   10, {0x000004, 0x180f3e, 0x451077, 0x721f81, 0x9f2f7f,
                     0xcd4071, 0xf1605d, 0xfd9567, 0xfec98d, 0xfcfdbf}},
    {"inferno", 10, {0x000004, 0x1b0c42, 0x4b0c6b, 0x781c6d, 0xa52c60,
                     0xcf4446, 0xed6925, 0xfb9a06, 0xf7d03c, 0xfcffa4}},
    {"plasma",  10, {0x0d0887, 0x47039f, 0x7301a8, 0x9c179e, 0xbd3786,
                     0xd8576b, 0xed7953, 0xfa9e3b, 0xfdc926, 0xf0f921}},
    {"cividis", 10, {0x00204d, 0x00336f, 0x39486b, 0x575c6d, 0x707173,
                     0x8a8779, 0xa69d75, 0xc4b56c, 0xe4cf5b, 0xffea46}},
    {"coolwarm", 5, {0x3b4cc0, 0x7c9ff9, 0xdddddd, 0xf49a7b, 0xb40426}},
    {"RdBu",    11, {0x67001f, 0xb2182b, 0xd6604d, 0xf4a582, 0xfddbc7,
                     0xf7f7f7, 0xd1e5f0, 0x92c5de, 0x4393c3, 0x2166ac,
                     0x053061}},
    {"gray",     2, {0x000000, 0xffffff}},
    {"jet",      9, {0x00007f, 0x0000ff, 0x007fff, 0x00ffff, 0x7fff7f,
                     0xffff00, 0xff7f00, 0xff0000, 0x7f0000}}
};

#define NCOLORMAPS ((int) (sizeof(colormaps)/sizeof(ColorMap)))

int number_of_colormaps(void)
{
    return NCOLORMAPS;
}

char *colormap_name(int i)
{
    return (i >= 0 && i < NCOLORMAPS) ? colormaps[i].name : NULL;
}

/* the colour at t in [0, 1] (linear interpolation between the nodes) */
static RGB colormap_rgb(ColorMap *cm, double t)
{
    RGB rgb, a, b;
    double s;
    int i;

    if (t <= 0.0 || cm->n == 1) {
        return rgb_of(cm->rgb[0]);
    }
    if (t >= 1.0) {
        return rgb_of(cm->rgb[cm->n - 1]);
    }
    s = t*(cm->n - 1);
    i = (int) floor(s);
    s -= i;
    a = rgb_of(cm->rgb[i]);
    b = rgb_of(cm->rgb[i + 1]);
    rgb.red   = (int) rint(a.red   + s*(b.red   - a.red));
    rgb.green = (int) rint(a.green + s*(b.green - a.green));
    rgb.blue  = (int) rint(a.blue  + s*(b.blue  - a.blue));
    return rgb;
}

static void colormap_entry_name(int map, int n, int k, char *buf)
{
    char *s;
    sprintf(buf, "%s_%d_%02d", colormaps[map].name, n, k);
    for (s = buf; *s; s++) {
        *s = tolower(*s);
    }
}

int colormap_colors(int map, int n, int reverse, int *indices)
{
    int k, missing = 0;
    char buf[64];

    if (map < 0 || map >= NCOLORMAPS || n < 2) {
        errmsg("Wrong colour map");
        return RETURN_FAILURE;
    }

    for (k = 0; k < n; k++) {
        colormap_entry_name(map, n, k, buf);
        if (get_color_by_name(buf) == BAD_COLOR) {
            missing++;
        }
    }
    if (number_of_colors() + missing > MAXCOLORS) {
        char ebuf[128];
        sprintf(ebuf, "Not enough room in the color table for %d more colors",
            missing);
        errmsg(ebuf);
        return RETURN_FAILURE;
    }

    for (k = 0; k < n; k++) {
        int cindex;
        colormap_entry_name(map, n, k, buf);
        cindex = get_color_by_name(buf);
        if (cindex == BAD_COLOR) {
            CMap_entry cmap;
            cmap.rgb = colormap_rgb(&colormaps[map], (double) k/(n - 1));
            cmap.cname = buf;
            cmap.ctype = COLOR_MAIN;
            cindex = number_of_colors();
            if (store_color(cindex, cmap) != RETURN_SUCCESS) {
                errmsg("Failed adding a color");
                return RETURN_FAILURE;
            }
        }
        indices[reverse ? (n - 1 - k) : k] = cindex;
    }

    return RETURN_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* colouring by value */

static int usable_value(double v, int logscale)
{
    return finite(v) && (!logscale || v > 0.0);
}

int get_column_range(int gno, int setno, int col, int logscale,
    double *vmin, double *vmax)
{
    double *x;
    int i, n, found = FALSE;

    if (!is_valid_setno(gno, setno) || col < 0 ||
        col >= dataset_cols(gno, setno)) {
        return RETURN_FAILURE;
    }
    x = getcol(gno, setno, col);
    n = getsetlength(gno, setno);
    for (i = 0; i < n; i++) {
        if (!usable_value(x[i], logscale)) {
            continue;
        }
        if (!found) {
            *vmin = *vmax = x[i];
            found = TRUE;
        } else {
            *vmin = MIN2(*vmin, x[i]);
            *vmax = MAX2(*vmax, x[i]);
        }
    }
    return found ? RETURN_SUCCESS:RETURN_FAILURE;
}

/* position in [0, 1] of a value in the scale */
static double scale_position(double v, ColorByValue *cbv)
{
    if (cbv->logscale) {
        return (log10(v) - log10(cbv->vmin))/
               (log10(cbv->vmax) - log10(cbv->vmin));
    } else {
        return (v - cbv->vmin)/(cbv->vmax - cbv->vmin);
    }
}

static double scale_value(double t, ColorByValue *cbv)
{
    if (cbv->logscale) {
        return pow(10.0, log10(cbv->vmin) +
                         t*(log10(cbv->vmax) - log10(cbv->vmin)));
    } else {
        return cbv->vmin + t*(cbv->vmax - cbv->vmin);
    }
}

/* colour bars made in this session, to replace them when re-applying */
typedef struct {
    int gno, setno;     /* the coloured set */
    int bar;            /* its colour bar graph */
} ColorBar;

static ColorBar *colorbars = NULL;
static int ncolorbars = 0;

static int find_colorbar(int gno, int setno)
{
    int i;
    for (i = 0; i < ncolorbars; i++) {
        if (colorbars[i].gno == gno && colorbars[i].setno == setno &&
            is_valid_gno(colorbars[i].bar)) {
            return colorbars[i].bar;
        }
    }
    return -1;
}

static void remember_colorbar(int gno, int setno, int bar)
{
    colorbars = xrealloc(colorbars, (ncolorbars + 1)*sizeof(ColorBar));
    colorbars[ncolorbars].gno = gno;
    colorbars[ncolorbars].setno = setno;
    colorbars[ncolorbars].bar = bar;
    ncolorbars++;
}

static int make_colorbar(int gno, int setno, ColorByValue *cbv, int *indices)
{
    int bar, k, i;
    view v, bv;
    world w;
    double px, py, width, gap;
    tickmarks *t;

    if (get_graph_viewport(gno, &v) != RETURN_SUCCESS) {
        return RETURN_FAILURE;
    }

    bar = find_colorbar(gno, setno);
    if (bar < 0) {
        bar = number_of_graphs();
        if (set_graph_active(bar) != RETURN_SUCCESS) {
            return RETURN_FAILURE;
        }
        remember_colorbar(gno, setno, bar);
    } else {
        /* remove the old boxes */
        for (i = 0; i < number_of_boxes(); i++) {
            boxtype b;
            get_graph_box(i, &b);
            if (isactive_box(i) && b.loctype == COORD_WORLD && b.gno == bar) {
                kill_box(i);
            }
        }
    }

    /* to the right of the graph, making room for it if needed */
    get_page_viewport(&px, &py);
    width = 0.03;
    gap = 0.03;
    if (v.xv2 + gap + width + 0.1 > px) {
        double shift = v.xv2 + gap + width + 0.1 - px;
        if (v.xv2 - shift > v.xv1 + 0.2) {
            v.xv2 -= shift;
            set_graph_viewport(gno, v);
        }
    }
    bv.xv1 = v.xv2 + gap;
    bv.xv2 = bv.xv1 + width;
    bv.yv1 = v.yv1;
    bv.yv2 = v.yv2;
    set_graph_viewport(bar, bv);

    set_graph_xscale(bar, SCALE_NORMAL);
    set_graph_yscale(bar, cbv->logscale ? SCALE_LOG:SCALE_NORMAL);
    w.xg1 = 0.0;
    w.xg2 = 1.0;
    w.yg1 = cbv->vmin;
    w.yg2 = cbv->vmax;
    set_graph_world(bar, w);
    set_graph_legend_active(bar, FALSE);
    set_graph_hidden(bar, FALSE);

    /* no X axis; Y ticks and labels on the right */
    t = get_graph_tickmarks(bar, X_AXIS);
    if (t) {
        t->active = FALSE;
    }
    t = get_graph_tickmarks(bar, Y_AXIS);
    if (t) {
        t->active = TRUE;
        t->t_op = PLACEMENT_OPPOSITE;
        t->tl_op = PLACEMENT_OPPOSITE;
        t->label_op = PLACEMENT_OPPOSITE;
    }
    autotick_axis(bar, Y_AXIS);

    /* the colours */
    for (k = 0; k < cbv->ncolors; k++) {
        boxtype b;
        int i = next_box();
        if (i < 0) {
            return RETURN_FAILURE;
        }
        set_default_box(&b);
        b.active = TRUE;
        b.loctype = COORD_WORLD;
        b.gno = bar;
        b.x1 = 0.0;
        b.x2 = 1.0;
        b.y1 = scale_value((double) k/cbv->ncolors, cbv);
        b.y2 = scale_value((double) (k + 1)/cbv->ncolors, cbv);
        b.lines = 0;
        b.color = indices[k];
        b.fillcolor = indices[k];
        b.fillpattern = 1;
        set_graph_box(i, &b);
    }

    return RETURN_SUCCESS;
}

int color_by_value(int gno, int setno, ColorByValue *cbv)
{
    int *indices, i, n, dest;
    double *values, *c;
    plotarr p;
    char buf[64];

    if (!is_valid_setno(gno, setno) || !is_set_active(gno, setno)) {
        errmsg("No valid set selected");
        return -1;
    }
    if (cbv->col < 0 || cbv->col >= dataset_cols(gno, setno)) {
        errmsg("The set has no such column");
        return -1;
    }
    if (cbv->ncolors < 2 || cbv->ncolors > MAXCOLORS/2) {
        errmsg("Wrong number of colors");
        return -1;
    }
    if (cbv->logscale && (cbv->vmin <= 0.0 || cbv->vmax <= 0.0)) {
        errmsg("The range must be positive for a logarithmic scale");
        return -1;
    }
    if (!(cbv->vmax > cbv->vmin)) {
        errmsg("Empty range of values");
        return -1;
    }

    indices = xmalloc(cbv->ncolors*sizeof(int));
    if (indices == NULL ||
        colormap_colors(cbv->map, cbv->ncolors, cbv->reverse, indices)
            != RETURN_SUCCESS) {
        xfree(indices);
        return -1;
    }

    /* the values, before the set is changed */
    n = getsetlength(gno, setno);
    values = xmalloc(MAX2(n, 1)*SIZEOF_DOUBLE);
    memcpy(values, getcol(gno, setno, cbv->col), n*SIZEOF_DOUBLE);

    if (cbv->new_set) {
        /* reuse the set made previously from the same source */
        sprintf(buf, "Colored by value: G%d.S%d", gno, setno);
        dest = -1;
        for (i = 0; i < number_of_sets(gno); i++) {
            if (i != setno && is_set_active(gno, i) &&
                compare_strings(getcomment(gno, i), buf)) {
                dest = i;
                break;
            }
        }
        if (dest < 0) {
            dest = nextset(gno);
        }
        if (dest < 0 || do_copyset(gno, setno, gno, dest) != RETURN_SUCCESS) {
            xfree(values);
            xfree(indices);
            return -1;
        }
        setcomment(gno, dest, buf);
        if (cbv->hide_source) {
            set_set_hidden(gno, setno, TRUE);
        }
    } else {
        dest = setno;
    }

    set_dataset_type(gno, dest, SET_XYCOLOR);
    c = getcol(gno, dest, 2);
    for (i = 0; i < n; i++) {
        if (usable_value(values[i], cbv->logscale)) {
            int k = (int) floor(scale_position(values[i], cbv)*cbv->ncolors);
            k = MAX2(0, MIN2(k, cbv->ncolors - 1));
            c[i] = indices[k];
        } else {
            /* not representable: black */
            c[i] = 1;
        }
    }

    /* the symbols must be visible and filled with the colours */
    get_graph_plotarr(gno, dest, &p);
    if (p.sym == 0) {
        p.sym = SYM_CIRCLE;
    }
    p.symfillpen.pattern = 1;
    if (cbv->new_set) {
        p.linet = LINE_TYPE_NONE;
    }
    set_graph_plotarr(gno, dest, &p);

    if (cbv->colorbar) {
        make_colorbar(gno, dest, cbv, indices);
    }

    xfree(values);
    xfree(indices);
    set_dirtystate();

    return dest;
}
