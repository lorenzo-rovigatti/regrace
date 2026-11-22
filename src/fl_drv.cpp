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
 * Screen driver and drawing canvas for the FLTK GUI.
 *
 * This replaces x11drv.c (the "X11" device) and the canvas-related
 * parts of xutil.c of the Motif GUI. The device keeps the name "X11",
 * so that parameter files and the command language are unaffected.
 *
 * Grace draws in device pixels. With FLTK, widget coordinates are in
 * "FLTK units" which differ from pixels when the GUI is scaled
 * (Ctrl +/-, or HiDPI screens), so every coordinate passed to FLTK is
 * divided by the current scale factor. The off-screen buffer has the
 * full pixel resolution, so the plot stays sharp at any GUI scale.
 *
 * The rubber-band lines/boxes, the crosshair and the graph focus
 * markers were XOR-drawn on top of the plot in the Motif version.
 * Here they are kept in an overlay list and re-drawn after the plot
 * is copied to the screen; drawing the same item twice removes it,
 * which reproduces the XOR semantics the event code relies on.
 *
 */

#include <config.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>
#include <FL/platform.H>

extern "C" {
#include "defines.h"
#include "globals.h"
#include "utils.h"
#include "device.h"
#include "devlist.h"
#include "draw.h"
#include "graphs.h"
#include "x11drv.h"
#include "plotone.h"
#include "protos.h"
}

#include "patterns.h"

#ifdef FLTK_USE_X11
#  include <X11/cursorfont.h>
#endif

#include "fl_gui.h"
#include "fl_widgets.h"

#if defined(FLTK_USE_X11) && FLTK_USE_X11 && !(defined(FLTK_USE_CAIRO) && FLTK_USE_CAIRO)
#  define GRACE_XLIB_EXTRAS 1
#endif

/* kept for compatibility with the command line options (no-op) */
extern "C" {
int install_cmap = CMAP_INSTALL_AUTO;
}

/* canvas size in device pixels */
static unsigned int win_w = 0, win_h = 0;
#define win_scale ((win_h < win_w) ? win_h:win_w)

/* scale factor (pixels per FLTK unit) used while drawing */
static double dscale = 1.0;

static Fl_Offscreen offscr = 0;
static int offscr_w = 0, offscr_h = 0;    /* in FLTK units */
static double offscr_scale = 0.0;         /* scale it was created at */
static int drawing = FALSE;

static class GraceCanvas *canvas = NULL;

static Device_entry dev_x11 = {DEVICE_TERM,
          (char *) "X11",
          xlibinitgraphics,
          NULL,
          NULL,
          (char *) "",
          FALSE,
          TRUE,
          {DEFAULT_PAGE_WIDTH, DEFAULT_PAGE_HEIGHT, 72.0},
          NULL
         };

/* ------------------------------------------------------------------ */
/* overlay (XOR emulation) */

#define OVL_LINE        0
#define OVL_RECT        1
#define OVL_FILLRECT    2

struct OverlayItem {
    int type;
    int x1, y1, x2, y2;
    bool operator==(const OverlayItem &o) const {
        return type == o.type && x1 == o.x1 && y1 == o.y1 &&
               x2 == o.x2 && y2 == o.y2;
    }
};

static std::vector<OverlayItem> overlay;

static void overlay_toggle(int type, int x1, int y1, int x2, int y2)
{
    OverlayItem it = {type, x1, y1, x2, y2};
    for (size_t i = 0; i < overlay.size(); i++) {
        if (overlay[i] == it) {
            overlay.erase(overlay.begin() + i);
            canvas_redisplay();
            return;
        }
    }
    overlay.push_back(it);
    canvas_redisplay();
}

void overlay_clear(void)
{
    overlay.clear();
}

/* ------------------------------------------------------------------ */
/* the canvas widget */

/* set the pointer shape for Grace cursor number c (see set_cursor()) */
static void apply_cursor(Fl_Window *win, int c)
{
    Fl_Cursor shape;
    
    if (win == NULL) {
        return;
    }
    
    switch (c) {
    case 0:
        shape = FL_CURSOR_CROSS;
        break;
    case 1:
        shape = FL_CURSOR_HAND;
        break;
    case 2:
        shape = FL_CURSOR_INSERT;
        break;
    case 3:
#if defined(FLTK_USE_X11) && FLTK_USE_X11
        /* the "kill" cursor: FLTK has no equivalent of the X11 pirate */
        if (fl_xid(win)) {
            static Cursor kill_cursor = 0;
            if (!kill_cursor) {
                kill_cursor = XCreateFontCursor(fl_display, XC_pirate);
            }
            XDefineCursor(fl_display, fl_xid(win), kill_cursor);
            return;
        }
#endif
        shape = FL_CURSOR_CROSS;
        break;
    case 4:
        shape = FL_CURSOR_MOVE;
        break;
    default:
        shape = FL_CURSOR_DEFAULT;
        break;
    }
    win->cursor(shape);
}

class GraceCanvas : public Fl_Widget {
public:
    GraceCanvas(int X, int Y, int W, int H) : Fl_Widget(X, Y, W, H) {
        cursor_id = -1;
    }

    void draw();
    int handle(int event);
    void resize(int X, int Y, int W, int H);

    int cursor_id;          /* Grace cursor number, -1 = default */

    int to_px(int ex) const { return (int) lround(ex*canvas_scale()); }
};

static void draw_overlay(int ox, int oy, double s)
{
    if (overlay.empty()) {
        return;
    }

    fl_push_clip(ox, oy, canvas->w(), canvas->h());
#ifdef GRACE_XLIB_EXTRAS
    XSetFunction(fl_display, fl_gc, invert ? GXinvert:GXxor);
#endif
    fl_color(FL_BLACK);
    fl_line_style(FL_SOLID, 0);
    for (size_t i = 0; i < overlay.size(); i++) {
        const OverlayItem &it = overlay[i];
        double x1 = ox + it.x1/s, y1 = oy + it.y1/s;
        double x2 = ox + it.x2/s, y2 = oy + it.y2/s;
        switch (it.type) {
        case OVL_LINE:
            fl_begin_line();
            fl_vertex(x1, y1);
            fl_vertex(x2, y2);
            fl_end_line();
            break;
        case OVL_RECT:
            fl_begin_loop();
            fl_vertex(x1, y1);
            fl_vertex(x2, y1);
            fl_vertex(x2, y2);
            fl_vertex(x1, y2);
            fl_end_loop();
            break;
        case OVL_FILLRECT:
            fl_begin_polygon();
            fl_vertex(x1, y1);
            fl_vertex(x2, y1);
            fl_vertex(x2, y2);
            fl_vertex(x1, y2);
            fl_end_polygon();
            break;
        }
    }
#ifdef GRACE_XLIB_EXTRAS
    XSetFunction(fl_display, fl_gc, GXcopy);
#endif
    fl_pop_clip();
}

void GraceCanvas::draw()
{
    if (offscr) {
        int cw = MIN2(w(), offscr_w), ch = MIN2(h(), offscr_h);
        fl_copy_offscreen(x(), y(), cw, ch, offscr, 0, 0);
        if (cw < w()) {
            fl_rectf(x() + cw, y(), w() - cw, h(), FL_WHITE);
        }
        if (ch < h()) {
            fl_rectf(x(), y() + ch, cw, h() - ch, FL_WHITE);
        }
    } else {
        fl_rectf(x(), y(), w(), h(), FL_WHITE);
    }
    draw_overlay(x(), y(), canvas_scale());
}

static void resize_timeout_cb(void *)
{
    if (inwin && get_pagelayout() == PAGE_FREE) {
        unsigned int w, h;
        sync_canvas_size(&w, &h, TRUE);
        drawgraph();
    }
}

void GraceCanvas::resize(int X, int Y, int W, int H)
{
    int changed = (W != w() || H != h());
    Fl_Widget::resize(X, Y, W, H);
    if (changed && get_pagelayout() == PAGE_FREE) {
        /* collapse a burst of resize events into a single redraw */
        Fl::remove_timeout(resize_timeout_cb);
        Fl::add_timeout(0.1, resize_timeout_cb);
    }
}

int GraceCanvas::handle(int event)
{
    int px, py;

    switch (event) {
    case FL_ENTER:
        apply_cursor(window(), cursor_id);
        return 1;
    case FL_LEAVE:
        window()->cursor(FL_CURSOR_DEFAULT);
        return 1;
    case FL_FOCUS:
    case FL_UNFOCUS:
        return 1;
    case FL_MOVE:
    case FL_DRAG:
        if (!inwin) {
            return 1;
        }
        px = to_px(Fl::event_x() - x());
        py = to_px(Fl::event_y() - y());
        canvas_event(CANVAS_EV_MOTION, px, py, 0, FALSE, 0);
        return 1;
    case FL_PUSH:
        if (!inwin) {
            return 1;
        }
        if (Fl::focus() != this) {
            take_focus();
        }
        px = to_px(Fl::event_x() - x());
        py = to_px(Fl::event_y() - y());
        {
            int button;
            switch (Fl::event_button()) {
            case FL_LEFT_MOUSE:
                button = CANVAS_BUTTON1;
                break;
            case FL_MIDDLE_MOUSE:
                button = CANVAS_BUTTON2;
                break;
            case FL_RIGHT_MOUSE:
                button = CANVAS_BUTTON3;
                break;
            default:
                return 0;
            }
            canvas_event(CANVAS_EV_PRESS, px, py, button,
                Fl::event_clicks() > 0, 0);
            if (button == CANVAS_BUTTON1 && Fl::event_clicks() > 0) {
                /* don't let a third click count as another double click */
                Fl::event_clicks(0);
            }
        }
        return 1;
    case FL_RELEASE:
        return 1;
    case FL_KEYBOARD:
    case FL_SHORTCUT:
        if (Fl::event_key() == FL_Escape) {
            canvas_event(CANVAS_EV_KEY, 0, 0, 0, FALSE, 27);
            return 1;
        } else {
            int key = Fl::event_key();
            if (key >= 'a' && key <= 'z' &&
                canvas_shortcut(Fl::event_state(FL_CTRL) ? TRUE:FALSE,
                                Fl::event_state(FL_ALT) ? TRUE:FALSE,
                                key) == TRUE) {
                return 1;
            }
        }
        return 0;
    default:
        return Fl_Widget::handle(event);
    }
}

Fl_Widget *fl_create_canvas(int X, int Y, int W, int H)
{
    canvas = new GraceCanvas(X, Y, W, H);
    return canvas;
}

Fl_Widget *fl_canvas_widget(void)
{
    return canvas;
}

double canvas_scale(void)
{
    Fl_Window *win = canvas ? canvas->window():NULL;
    if (win) {
        return Fl::screen_scale(win->screen_num());
    } else {
        return Fl::screen_scale(0);
    }
}

void canvas_redisplay(void)
{
    if (canvas) {
        canvas->redraw();
    }
}

void canvas_get_pixel_size(unsigned int *w, unsigned int *h)
{
    double s = canvas_scale();
    if (canvas) {
        *w = (unsigned int) lround(canvas->w()*s);
        *h = (unsigned int) lround(canvas->h()*s);
    } else {
        *w = 0;
        *h = 0;
    }
}

void canvas_set_pixel_size(unsigned int w, unsigned int h)
{
    double s = canvas_scale();
    if (canvas) {
        int W = (int) lround(w/s), H = (int) lround(h/s);
        if (W != canvas->w() || H != canvas->h()) {
            canvas->size(W, H);
            if (canvas->parent()) {
                canvas->parent()->init_sizes();
                canvas->parent()->redraw();
            }
        }
    }
}

void canvas_set_cursor_shape(int c)
{
    if (canvas) {
        canvas->cursor_id = c;
        if (canvas->window() && Fl::belowmouse() == canvas) {
            apply_cursor(canvas->window(), c);
        }
    }
}

/* ------------------------------------------------------------------ */
/* the device driver */

extern "C" int register_x11_drv(void)
{
    float hdpi, vdpi;

#ifdef GRACE_XLIB_EXTRAS
    {
        long mrsize;
        int max_path_limit;

        mrsize = XExtendedMaxRequestSize(fl_display);
        if (mrsize <= 0) {
            mrsize = XMaxRequestSize(fl_display);
        }
        max_path_limit = (mrsize - 3)/2;
        if (max_path_limit < get_max_path_limit()) {
            char buf[128];
            sprintf(buf,
                "Setting max drawing path length to %d (limited by the X server)",
                max_path_limit);
            errmsg(buf);
            set_max_path_limit(max_path_limit);
        }
    }
#endif

    /* the physical resolution of the screen, i.e. in device pixels */
    Fl::screen_dpi(hdpi, vdpi, 0);
    hdpi *= Fl::screen_scale(0);
    if (hdpi > 0) {
        dev_x11.pg.dpi = rint(hdpi);
    }

    return register_device(dev_x11);
}

extern "C" int xlibinit(void)
{
    return RETURN_SUCCESS;
}

extern "C" int xconvxlib(double x)
{
    return ((int) rint(win_scale * x));
}

extern "C" int yconvxlib(double y)
{
    return ((int) rint(win_h - win_scale * y));
}

extern "C" void xlibVPoint2dev(VPoint vp, int *x, int *y)
{
    *x = xconvxlib(vp.x);
    *y = yconvxlib(vp.y);
}

/* sub-pixel device coordinates, in FLTK units */
static inline double ux(double x)
{
    return win_scale*x/dscale;
}

static inline double uy(double y)
{
    return (win_h - win_scale*y)/dscale;
}

/*
 * xlibdev2VPoint - given (x,y) in screen coordinates, return the
 * viewport coordinates
 */
extern "C" VPoint xlibdev2VPoint(int x, int y)
{
    VPoint vp;

    if (win_scale == 0) {
        vp.x = (double) 0.0;
        vp.y = (double) 0.0;
    } else {
        vp.x = (double) x / win_scale;
        vp.y = (double) (win_h - y) / win_scale;
    }

    return (vp);
}

extern "C" void xlibupdatecmap(void)
{
    /* nothing to do on TrueColor displays */
}

extern "C" void xlibinitcmap(void)
{
}

static Fl_Color grace_color(int cindex)
{
    RGB *prgb = get_rgb(cindex);
    if (prgb == NULL) {
        return FL_BLACK;
    }
    return fl_rgb_color(prgb->red >> (GRACE_BPP - 8),
                        prgb->green >> (GRACE_BPP - 8),
                        prgb->blue >> (GRACE_BPP - 8));
}

extern "C" void xlibleavegraphics(void);
extern "C" void xlibdrawpixel(VPoint vp);
extern "C" void xlibdrawpolyline(VPoint *vps, int n, int mode);
extern "C" void xlibfillpolygon(VPoint *vps, int npoints);
extern "C" void xlibdrawarc(VPoint vp1, VPoint vp2, int angle1, int angle2);
extern "C" void xlibfillarc(VPoint vp1, VPoint vp2, int angle1, int angle2, int mode);
extern "C" void xlibputpixmap(VPoint vp, int width, int height,
     char *databits, int pixmap_bpp, int bitmap_pad, int pixmap_type);

extern "C" int xlibinitgraphics(void)
{
    int i, j;
    double step;

    if (inwin == FALSE || canvas == NULL || canvas->window() == NULL) {
        return RETURN_FAILURE;
    }

    /* device-dependent routines */
    devupdatecmap = xlibupdatecmap;

    devdrawpixel = xlibdrawpixel;
    devdrawpolyline = xlibdrawpolyline;
    devfillpolygon = xlibfillpolygon;
    devdrawarc = xlibdrawarc;
    devfillarc = xlibfillarc;
    devputpixmap = xlibputpixmap;

    devleavegraphics = xlibleavegraphics;

    /* init settings specific to X11 driver */

    if (get_pagelayout() == PAGE_FIXED) {
        sync_canvas_size(&win_w, &win_h, FALSE);
    } else {
        sync_canvas_size(&win_w, &win_h, TRUE);
    }

    if (win_w == 0 || win_h == 0) {
        return RETURN_FAILURE;
    }

    dscale = canvas_scale();

    /* (re)allocate the off-screen buffer */
    if (offscr && (offscr_w != canvas->w() || offscr_h != canvas->h() ||
                   offscr_scale != dscale)) {
        fl_delete_offscreen(offscr);
        offscr = 0;
    }
    if (!offscr) {
        Fl_Window *win = canvas->window();
        win->make_current();
        offscr_w = canvas->w();
        offscr_h = canvas->h();
        offscr_scale = dscale;
        offscr = fl_create_offscreen(offscr_w, offscr_h);
        if (!offscr) {
            errmsg("Can't allocate buffer pixmap");
            return RETURN_FAILURE;
        }
    }

    /* the previous plot (and everything XOR'ed on it) is gone */
    overlay_clear();

    fl_begin_offscreen(offscr);
    drawing = TRUE;

    fl_rectf(0, 0, offscr_w, offscr_h, grace_color(0));

    /* the dotted grid in the background */
    fl_color(grace_color(1));
    step = (double) win_scale/10;
    for (i = 0; i < win_w/step; i++) {
        for (j = 0; j < win_h/step; j++) {
            fl_point((int) (rint(i*step)/dscale),
                     (int) ((win_h - rint(j*step))/dscale));
        }
    }

    fl_line_style(FL_SOLID, 0);
    fl_rect(0, 0, offscr_w, offscr_h);

    return RETURN_SUCCESS;
}

static void xlib_setpen(void)
{
    fl_color(grace_color(getcolor()));
}

static void xlib_setdrawbrush(void)
{
    unsigned int iw;
    int style, lc, lj, w;
    char dashes[16];

    xlib_setpen();

    iw = (unsigned int) rint(getlinewidth()*win_scale);
    if (iw == 1) {
        iw = 0;
    }
    style = getlinestyle();

    switch (getlinecap()) {
    case LINECAP_ROUND:
        lc = FL_CAP_ROUND;
        break;
    case LINECAP_PROJ:
        lc = FL_CAP_SQUARE;
        break;
    case LINECAP_BUTT:
    default:
        lc = FL_CAP_FLAT;
        break;
    }

    switch (getlinejoin()) {
    case LINEJOIN_ROUND:
        lj = FL_JOIN_ROUND;
        break;
    case LINEJOIN_BEVEL:
        lj = FL_JOIN_BEVEL;
        break;
    case LINEJOIN_MITER:
    default:
        lj = FL_JOIN_MITER;
        break;
    }

    if (iw == 0) {
        w = 0;
    } else {
        w = MAX2(1, (int) lround(iw/dscale));
    }

    if (style > 1 && dash_array_length[style] < (int) sizeof(dashes)) {
        int i, darr_len = dash_array_length[style];
        int scale = MAX2(1, iw);
        for (i = 0; i < darr_len; i++) {
            int d = (int) lround(scale*dash_array[style][i]/dscale);
            dashes[i] = (char) MAX2(1, MIN2(d, 127));
        }
        dashes[darr_len] = '\0';
        fl_line_style(FL_SOLID | lc | lj, w, dashes);
    } else {
        fl_line_style(FL_SOLID | lc | lj, w);
    }
}

extern "C" void xlibdrawpixel(VPoint vp)
{
    if (!drawing) {
        return;
    }
    xlib_setpen();
    fl_point((int) floor(ux(vp.x)), (int) floor(uy(vp.y)));
}

extern "C" void xlibdrawpolyline(VPoint *vps, int n, int mode)
{
    int i;

    if (!drawing || n <= 1 || getlinestyle() == 0 || getpattern() == 0) {
        return;
    }

    xlib_setdrawbrush();

    if (mode == POLYLINE_CLOSED) {
        fl_begin_loop();
    } else {
        fl_begin_line();
    }
    for (i = 0; i < n; i++) {
        fl_vertex(ux(vps[i].x), uy(vps[i].y));
    }
    if (mode == POLYLINE_CLOSED) {
        fl_end_loop();
    } else {
        fl_end_line();
    }
}

/*
 * Set up a fill pattern; returns the pixmap to be freed afterwards.
 * Only the Xlib backend supports tiled fills, elsewhere patterns
 * are rendered as a solid fill.
 */
#ifdef GRACE_XLIB_EXTRAS
static Pixmap set_fill_pattern(void)
{
    int p = getpattern();

    if (p <= 1 || p >= number_of_patterns()) {
        return 0;
    } else {
        Pixmap ptmp = XCreatePixmapFromBitmapData(fl_display, fl_window,
            (char *) pat_bits[p], 16, 16,
            fl_xpixel(grace_color(getcolor())),
            fl_xpixel(grace_color(getbgcolor())),
            fl_visual->depth);
        XSetFillStyle(fl_display, fl_gc, FillTiled);
        XSetTile(fl_display, fl_gc, ptmp);
        XSetTSOrigin(fl_display, fl_gc, 0, 0);
        return ptmp;
    }
}

static void unset_fill_pattern(Pixmap ptmp)
{
    if (ptmp) {
        XSetFillStyle(fl_display, fl_gc, FillSolid);
        XFreePixmap(fl_display, ptmp);
    }
}
#endif

extern "C" void xlibfillpolygon(VPoint *vps, int npoints)
{
    int i;

    if (!drawing || npoints < 3 || getpattern() == 0) {
        return;
    }

    xlib_setpen();

#ifdef GRACE_XLIB_EXTRAS
    Pixmap ptmp = set_fill_pattern();
    if (getfillrule() == FILLRULE_WINDING) {
        XSetFillRule(fl_display, fl_gc, WindingRule);
    }
#endif

    fl_begin_complex_polygon();
    for (i = 0; i < npoints; i++) {
        fl_vertex(ux(vps[i].x), uy(vps[i].y));
    }
    fl_end_complex_polygon();

#ifdef GRACE_XLIB_EXTRAS
    if (getfillrule() == FILLRULE_WINDING) {
        XSetFillRule(fl_display, fl_gc, EvenOddRule);
    }
    unset_fill_pattern(ptmp);
#endif
}

/* add an elliptic arc inscribed in the box (vp1, vp2) to the current path */
static void arc_path(VPoint vp1, VPoint vp2, int angle1, int angle2)
{
    double cx = (ux(vp1.x) + ux(vp2.x))/2, cy = (uy(vp1.y) + uy(vp2.y))/2;
    double rx = fabs(ux(vp2.x) - ux(vp1.x))/2;
    double ry = fabs(uy(vp2.y) - uy(vp1.y))/2;

    fl_push_matrix();
    fl_translate(cx, cy);
    fl_scale(rx, ry);
    fl_arc(0.0, 0.0, 1.0, (double) angle1, (double) angle2);
    fl_pop_matrix();
}

/*
 *  xlibdrawarc
 */
extern "C" void xlibdrawarc(VPoint vp1, VPoint vp2, int angle1, int angle2)
{
    int x1, y1, x2, y2;

    if (!drawing || getlinestyle() == 0 || getpattern() == 0) {
        return;
    }

    xlibVPoint2dev(vp1, &x1, &y2);
    xlibVPoint2dev(vp2, &x2, &y1);

    xlib_setdrawbrush();

    if (x1 != x2 || y1 != y2) {
        if (angle2 - angle1 >= 360) {
            fl_begin_loop();
            arc_path(vp1, vp2, angle1, angle2);
            fl_end_loop();
        } else {
            fl_begin_line();
            arc_path(vp1, vp2, angle1, angle2);
            fl_end_line();
        }
    } else { /* zero radius */
        fl_point((int) (x1/dscale), (int) (y1/dscale));
    }
}

/*
 *  xlibfillarc
 */
extern "C" void xlibfillarc(VPoint vp1, VPoint vp2, int angle1, int angle2, int mode)
{
    int x1, y1, x2, y2;

    if (!drawing || getpattern() == 0) {
        return;
    }

    xlibVPoint2dev(vp1, &x1, &y2);
    xlibVPoint2dev(vp2, &x2, &y1);

    xlib_setpen();
    if (x1 != x2 || y1 != y2) {
#ifdef GRACE_XLIB_EXTRAS
        Pixmap ptmp = set_fill_pattern();
#endif
        fl_begin_complex_polygon();
        if (mode != ARCFILL_CHORD && angle2 - angle1 < 360) {
            fl_vertex((ux(vp1.x) + ux(vp2.x))/2, (uy(vp1.y) + uy(vp2.y))/2);
        }
        arc_path(vp1, vp2, angle1, angle2);
        fl_end_complex_polygon();
#ifdef GRACE_XLIB_EXTRAS
        unset_fill_pattern(ptmp);
#endif
    } else { /* zero radius */
        fl_point((int) (x1/dscale), (int) (y1/dscale));
    }
}


extern "C" void xlibputpixmap(VPoint vp, int width, int height,
     char *databits, int pixmap_bpp, int bitmap_pad, int pixmap_type)
{
    int j, k, x, y;
    int transparent = (pixmap_type == PIXMAP_TRANSPARENT);
    unsigned char *buf, *p;

    if (!drawing || width <= 0 || height <= 0) {
        return;
    }

    xlibVPoint2dev(vp, &x, &y);

    buf = (unsigned char *) xmalloc(4*width*height);
    if (buf == NULL) {
        errmsg("xmalloc failed in xlibputpixmap()");
        return;
    }

    p = buf;
    if (pixmap_bpp != 1) {
        int bg = getbgcolor();
        for (k = 0; k < height; k++) {
            for (j = 0; j < width; j++) {
                int cindex = (unsigned char) databits[k*width + j];
                RGB *prgb = get_rgb(cindex);
                if (prgb) {
                    p[0] = prgb->red   >> (GRACE_BPP - 8);
                    p[1] = prgb->green >> (GRACE_BPP - 8);
                    p[2] = prgb->blue  >> (GRACE_BPP - 8);
                } else {
                    p[0] = p[1] = p[2] = 0;
                }
                p[3] = (transparent && cindex == bg) ? 0:255;
                p += 4;
            }
        }
    } else {
        int line_len = PAD(width, bitmap_pad) >> 3;
        unsigned char fr, fg, fb, br, bgr, bb;
        Fl::get_color(grace_color(getcolor()), fr, fg, fb);
        Fl::get_color(grace_color(getbgcolor()), br, bgr, bb);
        for (k = 0; k < height; k++) {
            for (j = 0; j < width; j++) {
                int bit = (databits[k*line_len + (j >> 3)] >> (j % 8)) & 0x01;
                if (bit) {
                    p[0] = fr; p[1] = fg; p[2] = fb; p[3] = 255;
                } else {
                    p[0] = br; p[1] = bgr; p[2] = bb;
                    p[3] = transparent ? 0:255;
                }
                p += 4;
            }
        }
    }

    {
        Fl_RGB_Image img(buf, width, height, 4);
        int W = (int) lround(width/dscale), H = (int) lround(height/dscale);
        if (W != width || H != height) {
            img.scale(MAX2(1, W), MAX2(1, H), 0, 1);
        }
        img.draw((int) lround(x/dscale), (int) lround(y/dscale));
    }

    xfree(buf);
}

extern "C" void xlibleavegraphics(void)
{
    int cg = get_cg();

    if (drawing) {
        fl_line_style(0);
        fl_end_offscreen();
        drawing = FALSE;
    }

    if (is_graph_hidden(cg) == FALSE) {
        draw_focus(cg);
    }
    reset_crosshair();
    canvas_redisplay();
    Fl::flush();
}

/* ------------------------------------------------------------------ */
/* XOR-drawn items (from xutil.c) */

/*
 * draw the graph focus indicators
 */
extern "C" void draw_focus(int gno)
{
    int ix1, iy1, ix2, iy2;
    view v;
    VPoint vp;

    if (draw_focus_flag == TRUE) {
        get_graph_viewport(gno, &v);
        vp.x = v.xv1;
        vp.y = v.yv1;
        xlibVPoint2dev(vp, &ix1, &iy1);
        vp.x = v.xv2;
        vp.y = v.yv2;
        xlibVPoint2dev(vp, &ix2, &iy2);
        overlay_toggle(OVL_FILLRECT, ix1 - 5, iy1 - 5, ix1 + 5, iy1 + 5);
        overlay_toggle(OVL_FILLRECT, ix1 - 5, iy2 - 5, ix1 + 5, iy2 + 5);
        overlay_toggle(OVL_FILLRECT, ix2 - 5, iy2 - 5, ix2 + 5, iy2 + 5);
        overlay_toggle(OVL_FILLRECT, ix2 - 5, iy1 - 5, ix2 + 5, iy1 + 5);
    }
}

/*
 * rubber band line (optionally erasing previous one)
 */
extern "C" void select_line(int x1, int y1, int x2, int y2, int erase)
{
    static int x1_old, y1_old, x2_old, y2_old;

    if (erase) {
        overlay_toggle(OVL_LINE, x1_old, y1_old, x2_old, y2_old);
    }
    x1_old = x1;
    y1_old = y1;
    x2_old = x2;
    y2_old = y2;
    overlay_toggle(OVL_LINE, x1, y1, x2, y2);
}


/*
 * draw an xor'ed box (optionally erasing previous one)
 */
extern "C" void select_region(int x1, int y1, int x2, int y2, int erase)
{
    static int x1_old, y1_old, dx_old, dy_old;
    int dx = x2 - x1;
    int dy = y2 - y1;

    if (dx < 0) {
	iswap(&x1, &x2);
	dx = -dx;
    }
    if (dy < 0) {
	iswap(&y1, &y2);
	dy = -dy;
    }
    if (erase) {
        overlay_toggle(OVL_RECT, x1_old, y1_old,
            x1_old + dx_old, y1_old + dy_old);
    }
    x1_old = x1;
    y1_old = y1;
    dx_old = dx;
    dy_old = dy;
    overlay_toggle(OVL_RECT, x1, y1, x1 + dx, y1 + dy);
}

/*
 * slide an xor'ed bbox shifted by shift_*, (optionally erasing previous one)
 */
extern "C" void slide_region(view bb, int shift_x, int shift_y, int erase)
{
    int x1, x2;
    int y1, y2;
    VPoint vp;

    vp.x = bb.xv1;
    vp.y = bb.yv1;
    xlibVPoint2dev(vp, &x1, &y1);
    x1 += shift_x;
    y1 += shift_y;

    vp.x = bb.xv2;
    vp.y = bb.yv2;
    xlibVPoint2dev(vp, &x2, &y2);
    x2 += shift_x;
    y2 += shift_y;

    select_region(x1, y1, x2, y2, erase);
}

static int crosshair_erase = FALSE;

extern "C" void reset_crosshair(void)
{
    crosshair_erase = FALSE;
}

/*
 * draw a crosshair cursor
 */
extern "C" void crosshair_motion(int x, int y)
{
    static int cursor_oldx, cursor_oldy;

    /* Erase the previous crosshair */
    if (crosshair_erase == TRUE) {
        overlay_toggle(OVL_LINE, 0, cursor_oldy, win_w, cursor_oldy);
        overlay_toggle(OVL_LINE, cursor_oldx, 0, cursor_oldx, win_h);
    }

    /* Draw the new crosshair */
    overlay_toggle(OVL_LINE, 0, y, win_w, y);
    overlay_toggle(OVL_LINE, x, 0, x, win_h);
    crosshair_erase = TRUE;
    cursor_oldx = x;
    cursor_oldy = y;
}

/*
 * for the goto point feature
 */
extern "C" void setpointer(VPoint vp)
{
    int x, y;

    xlibVPoint2dev(vp, &x, &y);

    /* Make sure we remain inside the canvas */
    x = MAX2(x, 0);
    x = MIN2(x, (int) win_w);
    y = MAX2(y, 0);
    y = MIN2(y, (int) win_h);

#ifdef GRACE_XLIB_EXTRAS
    if (canvas && canvas->window() && fl_xid(canvas->window())) {
        double s = canvas_scale();
        XWarpPointer(fl_display, None, fl_xid(canvas->window()), 0, 0, 0, 0,
            (int) lround(canvas->x()*s) + x, (int) lround(canvas->y()*s) + y);
    }
#endif
}
