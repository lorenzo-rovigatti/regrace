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
 * Main FLTK interface: the main window, its menus and tool bars.
 *
 * This is a port of xmgrace.c (and of the non-drawing parts of
 * xutil.c); menu entries, tool bar buttons and their actions are the
 * same as in the Motif GUI.
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <locale.h>

#include <string>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Flex.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Bitmap.H>
#include <FL/Fl_Pixmap.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Tooltip.H>
#include <FL/fl_ask.H>
#include <FL/platform.H>

extern "C" {
#include "defines.h"
#include "globals.h"
#include "utils.h"
#include "device.h"
#include "graphs.h"
#include "graphutils.h"
#include "plotone.h"
#include "events.h"
#include "x11drv.h"
#include "protos.h"
#include "undo.h"
#include "parser.h"
}
#include "fl_files.h"

#define HAVE_XPM
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#include "bitmaps.h"
#pragma GCC diagnostic pop

#include "fl_gui.h"
#include "fl_widgets.h"

extern int inpipe;
extern char batchfile[];

/* ------------------------------------------------------------------ */

static Fl_Double_Window *main_win = NULL;

/* set once initialize_gui() has opened the display */
static bool display_open = false;
static Fl_Flex *main_col = NULL;
static Fl_Flex *main_row = NULL;
static Fl_Menu_Bar *menu_bar = NULL;
static Fl_Box *loclab = NULL;          /* locator label */
static Fl_Box *statlab = NULL;         /* status line at the bottom */
static Fl_Flex *toolbar = NULL;
static Fl_Box *stack_depth_item = NULL;
static Fl_Box *curw_item = NULL;
static Fl_Widget *drawing_window = NULL;

static int toolbar_visible = 1;
static int statusbar_visible = 1;
static int locbar_visible = 1;

static int page_layout = PAGE_FIXED;

static int cur_cursor = -1;

#define MENU_EXIT	201
#define MENU_NEW	203
#define MENU_OPEN	204
#define MENU_SAVE	205
#define MENU_SAVEAS	206
#define MENU_REVERT	207
#define MENU_PRINT	208

#define WSTACK_PUSH         0
#define WSTACK_POP          1
#define WSTACK_CYCLE        2
#define WSTACK_PUSH_ZOOM    3

/* sizes of the bars, in FLTK units */
#define BAR_H   24
#define TOOL_W  76
#define TOOL_H  24

Fl_Window *fl_main_window(void)
{
    return main_win;
}

/* ------------------------------------------------------------------ */
/* generic callback glue: menu items and buttons call a C function */

typedef void (*CB_Proc)(void *data);

struct CBAction {
    CB_Proc proc;
    void *data;
};

static void action_cb(Fl_Widget *, void *p)
{
    CBAction *a = (CBAction *) p;
    a->proc(a->data);
}

static CBAction *new_action(CB_Proc proc, void *data)
{
    CBAction *a = new CBAction;
    a->proc = proc;
    a->data = data;
    return a;
}

/* escape menu path separators and add a keyboard mnemonic */
static std::string menu_label(const char *label, char mnemonic)
{
    std::string s;
    int done = (mnemonic == '\0');
    for (const char *p = label; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == '_') {
            s += '\\';
        } else if (*p == '&') {
            s += '&';
        }
        if (!done && *p == mnemonic) {
            s += '&';
            done = TRUE;
        }
        s += *p;
    }
    return s;
}

struct MenuBuilder {
    Fl_Menu_Bar *mb;
    std::vector<std::string> path;

    std::string prefix() const {
        std::string s;
        for (size_t i = 0; i < path.size(); i++) {
            s += path[i];
            s += '/';
        }
        return s;
    }
    void push(const char *label, char mnemonic) {
        path.push_back(menu_label(label, mnemonic));
        mb->add((prefix().substr(0, prefix().size() - 1)).c_str(), 0, 0, 0,
            FL_SUBMENU);
    }
    void pop() {
        path.pop_back();
    }
    int button(const char *label, char mnemonic, CB_Proc proc, void *data,
               int shortcut = 0) {
        std::string s = prefix() + menu_label(label, mnemonic);
        return mb->add(s.c_str(), shortcut, action_cb, new_action(proc, data));
    }
    int toggle(const char *label, char mnemonic, CB_Proc proc, void *data) {
        std::string s = prefix() + menu_label(label, mnemonic);
        return mb->add(s.c_str(), 0, action_cb, new_action(proc, data),
            FL_MENU_TOGGLE);
    }
    void separator() {
        /* flag the last item of the current submenu as divider */
        std::string p = prefix();
        int last = -1;
        for (int i = 0; i < mb->size(); i++) {
            char buf[1024];
            if (mb->item_pathname(buf, sizeof(buf), &mb->menu()[i]) == 0) {
                std::string ip(buf);
                if (ip.size() > 1 && ip[0] == '/') {
                    ip = ip.substr(1);
                }
                if (ip.compare(0, p.size(), p) == 0 &&
                    ip.find('/', p.size()) == std::string::npos) {
                    last = i;
                }
            }
        }
        if (last >= 0) {
            Fl_Menu_Item *mi = (Fl_Menu_Item *) &mb->menu()[last];
            mi->flags |= FL_MENU_DIVIDER;
        }
    }
};

/* ------------------------------------------------------------------ */
/* callbacks of the main window (as in xmgrace.c) */

static void do_drawgraph(void *data)
{
    drawgraph();
}

static void MenuCB(void *data)
{
    char *s;

    switch ((int) (long) data) {
    case MENU_EXIT:
	bailout();
	break;
    case MENU_NEW:
	new_project(NULL);

        xdrawgraph();
	break;
    case MENU_OPEN:
	create_openproject_popup();
	break;
    case MENU_SAVE:
	if (strcmp (get_docname(), NONAME) != 0) {
	    set_wait_cursor();

	    save_project(get_docname());

	    unset_wait_cursor();
	} else {
	    create_saveproject_popup();
	}
	break;
    case MENU_SAVEAS:
	create_saveproject_popup();
	break;
    case MENU_REVERT:
	set_wait_cursor();
	s = copy_string(NULL, get_docname());
	if (strcmp (s, NONAME) != 0) {
            load_project(s);
        } else {
	    new_project(NULL);
        }
        xfree(s);
        xdrawgraph();
	unset_wait_cursor();
	break;
    case MENU_PRINT:
	set_wait_cursor();
	do_hardcopy();
	unset_wait_cursor();
	break;
    default:
	break;
    }
}

/*
 * undo/redo
 */
static void undo_proc(void *data)
{
    int retval;
    
    set_wait_cursor();
    retval = data ? redo() : undo();
    if (retval == RETURN_SUCCESS) {
        update_all();
        xdrawgraph();
    }
    unset_wait_cursor();
}

static void set_menu_item_active(const char *path, int onoff)
{
    Fl_Menu_Item *mi = (Fl_Menu_Item *) menu_bar->find_item(path);
    if (mi) {
        if (onoff) {
            mi->activate();
        } else {
            mi->deactivate();
        }
    }
}

/* called when the GUI is idle: each user action becomes an undo step */
static void undo_idle_cb(void *)
{
    static int undo_state = -1, redo_state = -1;
    int u, r;
    
    if (!inwin) {
        return;
    }
    undo_checkpoint();
    
    u = undo_possible();
    r = redo_possible();
    if (u != undo_state || r != redo_state) {
        set_menu_item_active("&Edit/&Undo", u);
        set_menu_item_active("&Edit/&Redo", r);
        undo_state = u;
        redo_state = r;
    }
}

/*
 * service the autoscale buttons on the main panel
 */
static void autoscale_proc(void *data)
{
    int cg = get_cg();

    if (autoscale_graph(cg, (int) (long) data) == RETURN_SUCCESS) {
	update_ticks(cg);
        xdrawgraph();
    } else {
	errmsg("Can't autoscale (no active sets?)");
    }
}

extern "C" void autoon_proc(void *data)
{
    set_action(DO_NOTHING);
    set_action(AUTO_NEAREST);
}

/*
 * service the autoticks button on the main panel
 */
extern "C" void autoticks_proc(void *data)
{
    autotick_axis(get_cg(), ALL_AXES);
    update_ticks(get_cg());
    xdrawgraph();
}

static void graph_scroll_proc(void *data)
{
    graph_scroll((int) (long) data);
    xdrawgraph();
}

static void graph_zoom_proc(void *data)
{
    graph_zoom((int) (long) data);
    xdrawgraph();
}

static void world_stack_proc(void *data)
{
    switch ((int) (long) data) {
    case WSTACK_PUSH_ZOOM:
        push_and_zoom();
        break;
    case WSTACK_PUSH:
        push_world();
        break;
    case WSTACK_POP:
        pop_world();
        break;
    case WSTACK_CYCLE:
        cycle_world_stack();
        break;
    default:
        return;
    }
    update_all();
    xdrawgraph();
}

static void load_example(void *data)
{
    char *s, buf[128];

    set_wait_cursor();

    s = (char *) data;
    sprintf(buf, "examples/%s", s);
    load_project_file(buf, FALSE);

    xdrawgraph();

    unset_wait_cursor();
}

/*
 * clear the locator reference point
 */
extern "C" void do_clear_point(void *data)
{
    GLocator locator;

    get_graph_locator(get_cg(), &locator);
    locator.pointset = FALSE;
    set_graph_locator(get_cg(), &locator);
    xdrawgraph();
}

/*
 * set the message in the left footer
 */
extern "C" void set_left_footer(char *s)
{
    if (statlab == NULL) {
        return;
    }
    if (s == NULL) {
        char hbuf[64];
        char buf[GR_MAXPATHLEN + 100];
        gethostname(hbuf, 63);
        hbuf[63] = '\0';
        sprintf(buf, "%s, %s, %s", hbuf, display_name(), get_docname());
        statlab->copy_label(buf);
    } else {
        statlab->copy_label(s);
    }
    statlab->redraw();
}

void set_locator_text(const char *s)
{
    if (loclab) {
        loclab->copy_label(s);
        loclab->redraw();
    }
}

/*
 * for world stack
 */
extern "C" void set_stack_message(void)
{
    char buf[16];
    if (stack_depth_item) {
	sprintf(buf, " SD:%1d ", graph_world_stack_size(get_cg()));
	stack_depth_item->copy_label(buf);
        sprintf(buf, " CW:%1d ", get_world_stack_current(get_cg()));
	curw_item->copy_label(buf);
    }
}

/* ------------------------------------------------------------------ */
/* visibility of the tool bars */

static void set_view_items(void)
{
    const Fl_Menu_Item *mi;

    if (main_col == NULL) {
        return;
    }

    if (locbar_visible) {
        loclab->show();
    } else {
        loclab->hide();
    }
    if (toolbar_visible) {
        toolbar->show();
    } else {
        toolbar->hide();
    }
    if (statusbar_visible) {
        statlab->show();
    } else {
        statlab->hide();
    }

    if ((mi = menu_bar->find_item("&View/Show &locator bar"))) {
        if (locbar_visible) ((Fl_Menu_Item *) mi)->set(); else ((Fl_Menu_Item *) mi)->clear();
    }
    if ((mi = menu_bar->find_item("&View/Show &status bar"))) {
        if (statusbar_visible) ((Fl_Menu_Item *) mi)->set(); else ((Fl_Menu_Item *) mi)->clear();
    }
    if ((mi = menu_bar->find_item("&View/Show &tool bar"))) {
        if (toolbar_visible) ((Fl_Menu_Item *) mi)->set(); else ((Fl_Menu_Item *) mi)->clear();
    }

    main_row->layout();
    main_col->layout();
    main_win->redraw();
}

static int menu_toggle_state(const char *path)
{
    const Fl_Menu_Item *mi = menu_bar->find_item(path);
    return (mi && mi->value()) ? 1:0;
}

static void set_locbar_cb(void *data)
{
    locbar_visible = menu_toggle_state("&View/Show &locator bar");
    set_view_items();
}

static void set_statusbar_cb(void *data)
{
    statusbar_visible = menu_toggle_state("&View/Show &status bar");
    set_view_items();
}

static void set_toolbar_cb(void *data)
{
    toolbar_visible = menu_toggle_state("&View/Show &tool bar");
    set_view_items();
}

extern "C" void set_barebones(int onoff)
{
    if (onoff){
        locbar_visible = 0;
        toolbar_visible = 0;
        statusbar_visible = 0;
    }
}

/* ------------------------------------------------------------------ */
/* GUI zoom */

static void resync_after_zoom(void)
{
    if (inwin) {
        unsigned int w, h;
        if (get_pagelayout() == PAGE_FIXED) {
            sync_canvas_size(&w, &h, FALSE);
        }
        drawgraph();
    }
}

static void set_gui_scale(float f)
{
    int i;
    if (f < 0.5) {
        f = 0.5;
    }
    if (f > 4.0) {
        f = 4.0;
    }
    for (i = 0; i < Fl::screen_count(); i++) {
        Fl::screen_scale(i, f);
    }
    resync_after_zoom();
}

static void gui_zoom_proc(void *data)
{
    int dir = (int) (long) data;
    float f = Fl::screen_scale(main_win ? main_win->screen_num():0);

    if (dir > 0) {
        f *= 1.1;
    } else if (dir < 0) {
        f /= 1.1;
    } else {
        f = 1.0;
    }
    set_gui_scale(f);
}

static int zoom_event_handler(int event)
{
    if (event == FL_ZOOM_EVENT) {
        resync_after_zoom();
    }
    return 0;
}

/*
 * Undo/redo from any window: FLTK offers a shortcut nobody used only to
 * the window under the mouse, so the menu bar does not see Ctrl+Z typed
 * in a dialog (text fields handle it themselves before this). Ctrl+Y is
 * the other common shortcut of Redo (Ctrl+Shift+Z is in the menu).
 */
static int undo_key_handler(int event)
{
    int key = Fl::event_key();

    if (event != FL_SHORTCUT || !Fl::event_state(FL_CTRL) ||
        Fl::event_state(FL_ALT | FL_META)) {
        return 0;
    }
    if (key == 'z') {
        undo_proc(Fl::event_state(FL_SHIFT) ? (void *) 1 : NULL);
        return 1;
    }
    if (key == 'y' && !Fl::event_state(FL_SHIFT)) {
        undo_proc((void *) 1);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */

extern "C" void create_gui_prefs_frame(void *data);

/*
 * create the main menubar
 */
static Fl_Menu_Bar *CreateMainMenuBar(int W)
{
    static char buf[128];
    MenuBuilder m;

    m.mb = new Fl_Menu_Bar(0, 0, W, BAR_H);

/*
 * File menu
 */
    m.push("File", 'F');

    m.button("New", 'N', MenuCB, (void *) MENU_NEW, FL_CTRL + 'n');
    m.button("Open...", 'O', MenuCB, (void *) MENU_OPEN, FL_CTRL + 'o');
    m.button("Save", 'S', MenuCB, (void *) MENU_SAVE, FL_CTRL + 's');
    m.button("Save as...", 'a', MenuCB, (void *) MENU_SAVEAS);
    m.button("Revert to saved", 'v', MenuCB, (void *) MENU_REVERT);

    m.separator();

    m.button("Print setup...", 't', create_printer_setup, &hdevice);
    m.button("Print", 'P', MenuCB, (void *) MENU_PRINT, FL_CTRL + 'p');
    m.separator();
    m.button("Exit", 'x', MenuCB, (void *) MENU_EXIT, FL_CTRL + 'q');
    m.pop();

/*
 * Edit menu
 */
    m.push("Edit", 'E');

    /* new in the FLTK GUI */
    m.button("Undo", 'U', undo_proc, (void *) 0, FL_CTRL + 'z');
    m.button("Redo", 'R', undo_proc, (void *) 1, FL_CTRL + FL_SHIFT + 'z');
    m.separator();

    m.button("Data sets...", 'D', create_datasetprop_popup, NULL);
    m.button("Set operations...", 'o', create_setop_popup, NULL);
    m.separator();
    m.button("Arrange graphs...", 'r', create_arrange_frame, NULL);
    m.button("Overlay graphs...", 'O', create_overlay_frame, NULL);
    m.button("Autoscale graphs...", 'A', create_autos_frame, NULL);
    m.separator();

    m.push("Regions", 'i');
    m.button("Status...", 'S', define_status_popup, NULL);
    m.button("Define...", 'D', create_define_frame, NULL);
    m.button("Clear...", 'C', create_clear_frame, NULL);
    m.separator();
    m.button("Report on...", 'R', create_reporton_frame, NULL);
    m.pop();

    m.button("Hot links...", 'l', create_hotlinks_popup, NULL);

    m.separator();

    m.button("Set locator fixed point", 'f', set_actioncb, (void *) SEL_POINT);
    m.button("Clear locator fixed point", 'C', do_clear_point, NULL);
    m.button("Locator props...", 'p', create_locator_frame, NULL);

    m.separator();

    m.button("Preferences...", 'r', create_props_frame, NULL);
    m.pop();

/*
 * Data menu
 */
    m.push("Data", 'D');

    m.button("Data set operations...", 'o', create_datasetop_popup, NULL);

    m.push("Transformations", 'T');

    m.button("Evaluate expression...", 'E', create_eval_frame, NULL);
    m.separator();
    m.button("Histograms...", 'H', create_histo_frame, NULL);
    m.button("Fourier transforms...", 'u', create_fourier_frame, NULL);
    m.separator();
    m.button("Running averages...", 'a', create_run_frame, NULL);
    m.button("Differences...", 'D', create_diff_frame, NULL);
    m.button("Seasonal differences...", 'o', create_seasonal_frame, NULL);
    m.button("Integration...", 'I', create_int_frame, NULL);
    m.separator();
    m.button("Interpolation/splines...", 't', create_interp_frame, NULL);
    m.button("Regression...", 'R', create_reg_frame, NULL);
    m.button("Non-linear curve fitting...", 'N', create_nonl_frame, NULL);
    m.separator();
    m.button("Correlation/covariance...", 'C', create_xcor_frame, NULL);
    m.button("Digital filter...", 'f', create_digf_frame, NULL);
    m.button("Linear convolution...", 'v', create_lconv_frame, NULL);
    m.separator();
    m.button("Geometric transforms...", 'G', create_geom_frame, NULL);
    m.separator();
    m.button("Sample points...", 'm', create_samp_frame, NULL);
    m.button("Prune data...", 'P', create_prune_frame, NULL);
    m.pop();

    m.button("Feature extraction...", 'x', create_featext_frame, NULL);

    m.separator();

    m.push("Import", 'I');
    m.button("ASCII...", 'A', create_file_popup, NULL);
#ifdef HAVE_NETCDF
    m.button("NetCDF...", 'N', create_netcdfs_popup, NULL);
#endif
    m.pop();

    m.push("Export", 'E');
    m.button("ASCII...", 'A', create_write_popup, NULL);
    m.pop();
    m.pop();

/* Plot menu */
    m.push("Plot", 'P');

    m.button("Plot appearance...", 'p', create_plot_frame_cb, NULL);
    m.button("Graph appearance...", 'G', create_graphapp_frame_cb, (void *) -1);
    m.button("Set appearance...", 'S', define_symbols_popup, (void *) -1);
    m.button("Axis properties...", 'x', create_axes_dialog_cb, NULL);
    m.separator();
    m.button("Colors...", 'C', create_colors_frame, NULL);
    m.button("Color by value...", 'v', create_colorbyvalue_frame, NULL);
    m.separator();
    m.button("Load parameters...", 'L', create_rparams_popup, NULL);
    m.button("Save parameters...", 'S', create_wparam_frame, NULL);
    m.pop();

/* View menu */
    m.push("View", 'V');

    m.toggle("Show locator bar", 'l', set_locbar_cb, NULL);
    m.toggle("Show status bar", 's', set_statusbar_cb, NULL);
    m.toggle("Show tool bar", 't', set_toolbar_cb, NULL);

    m.separator();

    m.button("Page setup...", 'P', create_printer_setup, &tdevice);

    m.separator();

    m.button("Redraw", 'R', do_drawgraph, NULL);

    m.separator();

    m.button("Update all", 'U', update_all_cb, NULL);

    m.separator();

    /* new in the FLTK GUI */
    m.button("Larger GUI (Ctrl +)", 'L', gui_zoom_proc, (void *) 1);
    m.button("Smaller GUI (Ctrl -)", 'm', gui_zoom_proc, (void *) -1);
    m.button("Normal GUI size (Ctrl 0)", 'N', gui_zoom_proc, (void *) 0);
    m.button("GUI appearance...", 'G', create_gui_prefs_frame, NULL);
    m.pop();

/* Window menu */
    m.push("Window", 'W');

    m.button("Commands", 'C', open_command, NULL);
    m.button("Point explorer", 'P', create_points_frame, NULL);
    m.button("Drawing objects", 'o', define_objects_popup, NULL);
    m.button("Font tool", 'F', create_fonttool_cb, NULL);
    m.button("Console", 'l', create_monitor_frame_cb, NULL);
    m.pop();

/* help menu */

    m.push("Help", 'H');

    m.button("User's Guide", 'G', HelpCB, (void *) "doc/UsersGuide.html");
    m.button("Tutorial", 'T', HelpCB, (void *) "doc/Tutorial.html");
    m.button("FAQ", 'Q', HelpCB, (void *) "doc/FAQ.html");
    m.button("Changes", 'C', HelpCB, (void *) "doc/CHANGES.html");

    m.separator();

    m.push("Examples", 'E');
    m.push("General intro", 'i');
    m.button("Explain", '\0', load_example, (void *) "explain.agr");
    m.button("Properties", '\0', load_example, (void *) "props.agr");
    m.button("Axes", '\0',load_example, (void *) "axes.agr");
    m.button("Fonts", '\0', load_example, (void *) "tfonts.agr");
    m.button("Arrows", '\0', load_example, (void *) "arrows.agr");
    m.button("Symbols and lines", '\0', load_example, (void *) "symslines.agr");
    m.button("Fills", '\0', load_example, (void *) "fills.agr");
    m.button("World stack", '\0', load_example, (void *) "tstack.agr");
    m.button("Inset graphs", '\0', load_example, (void *) "tinset.agr");
    m.button("Many graphs", '\0', load_example, (void *) "manygraphs.agr");
    m.pop();

    m.push("XY graphs", 'g');
    m.button("Log scale", '\0', load_example, (void *) "tlog.agr");
    m.button("Log2 scale", '\0', load_example, (void *) "log2.agr");
    m.button("Log2/log scale", '\0', load_example, (void *) "log2log.agr");
    m.button("Logit scale", '\0', load_example, (void *) "logit.agr");
    m.button("Reciprocal scale", '\0', load_example, (void *) "reciprocal.agr");
    m.button("Error bars", '\0', load_example, (void *) "terr.agr");
    m.button("Date/time axis formats", '\0', load_example, (void *) "times.agr");
    m.button("Australia map", '\0', load_example, (void *) "au.agr");
    m.button("A CO2 analysis", '\0', load_example, (void *) "co2.agr");
    m.button("Motif statistics", '\0', load_example, (void *) "motif.agr");
    m.button("Spectrum", '\0', load_example, (void *) "spectrum.agr");
    m.pop();

    m.push("XY charts", 'c');
    m.button("Bar chart", '\0', load_example, (void *) "bar.agr");
    m.button("Stacked bar", '\0', load_example, (void *) "stackedb.agr");
    m.button("Bar chart with error bars", '\0', load_example, (void *) "chartebar.agr");
    m.button("Different charts", '\0', load_example, (void *) "charts.agr");
    m.pop();

    m.push("Polar graphs", 'P');
    m.button("Polar graph", '\0', load_example, (void *) "polar.agr");
    m.pop();

    m.push("Pie charts", 'i');
    m.button("Pie chart", '\0', load_example, (void *) "pie.agr");
    m.pop();

    m.push("Special set presentations", 'S');
    m.button("HILO", '\0', load_example, (void *) "hilo.agr");
    m.button("XY Radius", '\0', load_example, (void *) "txyr.agr");
    m.button("XYZ", '\0', load_example, (void *) "xyz.agr");
    m.button("Box plot", '\0', load_example, (void *) "boxplot.agr");
    m.button("Vector map", '\0', load_example, (void *) "vmap.agr");
    m.button("XY Size", '\0', load_example, (void *) "xysize.agr");
    m.button("XY Color", '\0', load_example, (void *) "xycolor.agr");
    m.pop();

    m.push("Type setting", 'T');
    m.button("Simple", '\0', load_example, (void *) "test2.agr");
    m.button("Text transforms", '\0', load_example, (void *) "txttrans.agr");
    m.button("Advanced", '\0', load_example, (void *) "typeset.agr");
    m.pop();

    m.push("Calculus", 'u');
    m.button("Non-linear fit", '\0', load_example, (void *) "logistic.agr");
    m.pop();
    m.pop();

    m.separator();

    sprintf(buf,
        "http://plasma-gate.weizmann.ac.il/Grace/comments.phtml?version_id=%ld",
        bi_version_id());
    m.button("Comments", 'm', HelpCB, buf);
    m.separator();
    m.button("License terms", 'L', HelpCB, (void *) "doc/GPL.html");
    m.button("About...", 'A', create_about_grtool, NULL);
    m.pop();

    return m.mb;
}

/* ------------------------------------------------------------------ */
/* tool bar */

static Fl_Button *tool_button(Fl_Flex *parent, const char *label,
    CB_Proc proc, void *data, const char *tip = NULL)
{
    Fl_Button *bt = new Fl_Button(0, 0, TOOL_W, TOOL_H);
    bt->copy_label(label);
    bt->callback(action_cb, new_action(proc, data));
    bt->visible_focus(0);
    if (tip) {
        bt->tooltip(tip);
    }
    if (parent) {
        parent->fixed(bt, TOOL_H);
    }
    return bt;
}

static Fl_Button *bitmap_button(const unsigned char *bits,
    CB_Proc proc, void *data, const char *tip)
{
    Fl_Button *bt = new Fl_Button(0, 0, TOOL_W/2, TOOL_H);
    bt->image(new Fl_Bitmap(bits, 16, 16));
    bt->callback(action_cb, new_action(proc, data));
    bt->visible_focus(0);
    bt->tooltip(tip);
    return bt;
}

static Fl_Flex *tool_row(Fl_Flex *parent)
{
    Fl_Flex *row = new Fl_Flex(0, 0, TOOL_W, TOOL_H, Fl_Flex::HORIZONTAL);
    row->gap(0);
    parent->fixed(row, TOOL_H);
    return row;
}

static Fl_Flex *CreateToolBar(int H)
{
    Fl_Flex *tb = new Fl_Flex(0, 0, TOOL_W, H, Fl_Flex::VERTICAL);
    Fl_Flex *row;
    Fl_Box *sep;

    tb->gap(1);
    tb->margin(2);
    tb->box(FL_THIN_UP_BOX);

    tool_button(tb, "Draw", do_drawgraph, NULL, "Redraw the canvas");

/* zoom and autoscale */
    row = tool_row(tb);
    bitmap_button(zoom_bits, set_actioncb, (void *) ZOOM_1ST, "Zoom");
    bitmap_button(auto_bits, autoscale_proc, (void *) AUTOSCALE_XY, "Autoscale");
    row->end();

/* expand/shrink */
    row = tool_row(tb);
    bitmap_button(expand_bits, graph_zoom_proc, (void *) GZOOM_EXPAND, "Expand");
    bitmap_button(shrink_bits, graph_zoom_proc, (void *) GZOOM_SHRINK, "Shrink");
    row->end();

/*
 * scrolling buttons
 */
    row = tool_row(tb);
    bitmap_button(left_bits, graph_scroll_proc, (void *) GSCROLL_LEFT, "Scroll left");
    bitmap_button(right_bits, graph_scroll_proc, (void *) GSCROLL_RIGHT, "Scroll right");
    row->end();

    row = tool_row(tb);
    bitmap_button(down_bits, graph_scroll_proc, (void *) GSCROLL_DOWN, "Scroll down");
    bitmap_button(up_bits, graph_scroll_proc, (void *) GSCROLL_UP, "Scroll up");
    row->end();

    sep = new Fl_Box(0, 0, TOOL_W, 6);
    tb->fixed(sep, 6);

    tool_button(tb, "AutoT", autoticks_proc, NULL, "Autotick axes");
    tool_button(tb, "AutoO", autoon_proc, NULL, "Autoscale on set");

    row = tool_row(tb);
    tool_button(NULL, "ZX", set_actioncb, (void *) ZOOMX_1ST, "Zoom along X");
    tool_button(NULL, "ZY", set_actioncb, (void *) ZOOMY_1ST, "Zoom along Y");
    row->end();

    row = tool_row(tb);
    tool_button(NULL, "AX", autoscale_proc, (void *) AUTOSCALE_X, "Autoscale X");
    tool_button(NULL, "AY", autoscale_proc, (void *) AUTOSCALE_Y, "Autoscale Y");
    row->end();

    row = tool_row(tb);
    tool_button(NULL, "PZ", world_stack_proc, (void *) WSTACK_PUSH_ZOOM, "Push and zoom");
    tool_button(NULL, "Pu", world_stack_proc, (void *) WSTACK_PUSH, "Push");
    row->end();

    row = tool_row(tb);
    tool_button(NULL, "Po", world_stack_proc, (void *) WSTACK_POP, "Pop");
    tool_button(NULL, "Cy", world_stack_proc, (void *) WSTACK_CYCLE, "Cycle");
    row->end();

    stack_depth_item = new Fl_Box(0, 0, TOOL_W, TOOL_H);
    tb->fixed(stack_depth_item, TOOL_H);
    stack_depth_item->tooltip("Stack depth");
    curw_item = new Fl_Box(0, 0, TOOL_W, TOOL_H);
    tb->fixed(curw_item, TOOL_H);
    curw_item->tooltip("Current world");

    tool_button(tb, "Exit", MenuCB, (void *) MENU_EXIT);

    /* filler */
    new Fl_Box(0, 0, TOOL_W, 1);

    tb->end();

    return tb;
}

/* ------------------------------------------------------------------ */

static void main_win_cb(Fl_Widget *w, void *)
{
    /* don't let Escape close the main window: it cancels canvas actions */
    if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) {
        set_action(DO_NOTHING);
        return;
    }
    bailout();
}

/*
 * initialize the toolkit
 */
extern "C" int initialize_gui(int *argc, char **argv)
{
    int i;
    char *dname = NULL;

    for (i = 1; i < *argc - 1; i++) {
        /* See if display name was specified in the args */
        const char *pattern = "-display";
        if (strlen(argv[i]) > 1 && strstr(pattern, argv[i]) == pattern) {
            dname = argv[i + 1];
        }
    }

#ifdef FLTK_USE_X11
    {
        Display *d = XOpenDisplay(dname);
        if (d == NULL) {
            errmsg("Can't open display");
            return RETURN_FAILURE;
        }
        fl_x11_use_display(d);
    }
#endif
    fl_open_display();
    display_open = true;

    /* numeric I/O must use the C locale */
    setlocale(LC_NUMERIC, "C");

    Fl::visual(FL_DOUBLE | FL_INDEX);

    fl_apply_gui_prefs();

    return RETURN_SUCCESS;
}

/*
 * GRACE_FLTK_OPEN="Plot/Plot appearance...;Edit/Data sets..." activates
 * the given menu entries at startup (mnemonics are ignored). Used to test
 * the dialogs without user interaction. Entries "cmd:..." are commands
 * of the Grace language.
 */
static std::string strip_amp(const char *s)
{
    std::string r;
    for (; *s; s++) {
        if (*s == '&' && s[1] != '&') {
            continue;
        }
        r += *s;
    }
    return r;
}

static void open_menu_items_from_env(void)
{
    const char *env = getenv("GRACE_FLTK_OPEN");
    std::string list, path;
    size_t start = 0, end;

    if (env == NULL) {
        return;
    }
    list = env;
    while (start <= list.size()) {
        end = list.find(';', start);
        if (end == std::string::npos) {
            end = list.size();
        }
        path = list.substr(start, end - start);
        start = end + 1;
        if (path.empty()) {
            continue;
        }
        /* let the GUI go idle between the steps, as between user actions */
        Fl::check();
        if (path.compare(0, 4, "cmd:") == 0) {
            /* a command of the Grace language */
            scanner((char *) path.substr(4).c_str());
            /* as the Enter key in the Commands window */
            undo_touch();
            Fl::check();
            continue;
        }
        for (int i = 0; i < menu_bar->size(); i++) {
            char buf[1024];
            const Fl_Menu_Item *mi = &menu_bar->menu()[i];
            if (mi->label() == NULL ||
                menu_bar->item_pathname(buf, sizeof(buf), mi) != 0) {
                continue;
            }
            std::string p = strip_amp(buf), q;
            if (!p.empty() && p[0] == '/') {
                p = p.substr(1);
            }
            /* item_pathname() keeps the escapes of '/' */
            for (size_t k = 0; k < p.size(); k++) {
                if (p[k] == '\\' && k + 1 < p.size()) {
                    k++;
                }
                q += p[k];
            }
            if (q == path) {
                menu_bar->picked(mi);
                break;
            }
        }
        Fl::flush();
    }
}

/*
 * build the GUI
 */
extern "C" void startup_gui(void)
{
    int W = 860, H = 720;
    int sx, sy, sw, sh;

    Fl::screen_work_area(sx, sy, sw, sh);
    W = MIN2(W, sw - 20);
    H = MIN2(H, sh - 40);

    main_win = new Fl_Double_Window(W, H, "Grace");
    main_win->xclass("XMgrace");
    main_win->callback(main_win_cb);

    main_col = new Fl_Flex(0, 0, W, H, Fl_Flex::VERTICAL);
    main_col->gap(0);

    menu_bar = CreateMainMenuBar(W);
    main_col->fixed(menu_bar, BAR_H);

    loclab = new Fl_Box(0, 0, W, BAR_H);
    loclab->box(FL_THIN_DOWN_BOX);
    loclab->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    main_col->fixed(loclab, BAR_H);

    main_row = new Fl_Flex(0, 0, W, H - 3*BAR_H, Fl_Flex::HORIZONTAL);
    main_row->gap(0);

    toolbar = CreateToolBar(H - 3*BAR_H);
    main_row->fixed(toolbar, TOOL_W + 4);

    if (get_pagelayout() == PAGE_FIXED) {
        Fl_Scroll *scroll = new Fl_Scroll(TOOL_W, BAR_H*2,
            W - TOOL_W, H - 3*BAR_H);
        scroll->box(FL_DOWN_BOX);
        scroll->color(fl_rgb_color(0x9a, 0x9a, 0x9a));
        fl_create_canvas(scroll->x() + 2, scroll->y() + 2, 100, 100);
        scroll->end();
        drawing_window = scroll;
    } else {
        drawing_window = fl_create_canvas(TOOL_W, BAR_H*2,
            W - TOOL_W, H - 3*BAR_H);
    }
    main_row->end();
    main_row->resizable(drawing_window);

    statlab = new Fl_Box(0, 0, W, BAR_H);
    statlab->box(FL_THIN_DOWN_BOX);
    statlab->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    main_col->fixed(statlab, BAR_H);

    main_col->end();
    main_win->end();
    main_win->resizable(main_col);
    main_win->size_range(300, 250);

    /* the icon */
    {
        Fl_Pixmap pm((const char * const *) grace_icon_xpm);
        Fl_RGB_Image icon(&pm, FL_GRAY);
        main_win->icon(&icon);
    }

    Fl::add_handler(zoom_event_handler);
    Fl::add_handler(undo_key_handler);
    
    fl_register_app_shell(main_win);
    fl_install_help_keys();
    init_option_menus();
    Fl::add_check(undo_idle_cb);

    if (get_pagelayout() == PAGE_FIXED) {
        unsigned int w, h;
        sync_canvas_size(&w, &h, FALSE);
    }

    set_view_items();

    set_locator_text("G0:[X, Y] = ");
    set_stack_message();
    set_left_footer(NULL);

    main_win->show();
    while (!main_win->shown() || !main_win->visible()) {
        Fl::wait(0.05);
    }
    Fl::flush();

    fl_canvas_widget()->take_focus();

    inwin = TRUE;

/*
 * set the title
 */
    update_app_title();

    /* what expose_resize() did on the first expose event */
    if (batchfile[0]) {
        getparms(batchfile);
    }

    if (inpipe == TRUE) {
        getdata(get_cg(), (char *) "stdin", SOURCE_DISK, LOAD_SINGLE);
        inpipe = FALSE;
    }

    update_all();
    drawgraph();

    /* for automated GUI tests: open menu entries given in the environment */
    open_menu_items_from_env();

    Fl::run();

    /* all windows closed */
    bailout();
}

extern "C" void sync_canvas_size(unsigned int *w, unsigned int *h, int inv)
{
    Page_geometry pg = get_page_geometry();
    if (inv) {
        canvas_get_pixel_size(w, h);
        set_page_dimensions((int) (*w*72.0/pg.dpi), (int) (*h*72.0/pg.dpi), TRUE);
    } else {
        *w = pg.width;
        *h = pg.height;
        canvas_set_pixel_size(*w, *h);
    }
}

extern "C" int get_pagelayout(void)
{
    return page_layout;
}

extern "C" void set_pagelayout(int layout)
{
    if (page_layout == layout) {
        return;
    }

    if (inwin) {
        errmsg("Can not change layout after initialization of GUI");
        return;
    } else {
        page_layout = layout;
    }
}

/* ------------------------------------------------------------------ */
/* window title, cursors */

/*
 * put a string in the title bar
 */
extern "C" void set_title(char *ts)
{
    static char *ts_save = NULL;
    static int dstate_save = 0;
    int dstate = is_dirtystate();

    if (!inwin || ts == NULL || main_win == NULL) {
        return;
    } else
    if (ts_save == NULL || strcmp(ts_save, ts) != 0 || dstate != dstate_save) {
        char *buf1, *buf2;
        ts_save = copy_string(ts_save, ts);
        dstate_save = dstate;
        buf1 = copy_string(NULL, "Grace: ");
        buf1 = concat_strings(buf1, ts);
        buf2 = copy_string(NULL, ts);
        if (dstate) {
            buf2 = concat_strings(buf2, "*");
            buf1 = concat_strings(buf1, " (modified)");
        }
        main_win->copy_label(buf1);
        main_win->iconlabel(NULL);
        xfree(buf1);
        xfree(buf2);
    }
}

extern "C" void set_wait_cursor(void)
{
    Fl_Window *w;

    if (main_win == NULL || !main_win->shown()) {
        return;
    }

    for (w = Fl::first_window(); w; w = Fl::next_window(w)) {
        w->cursor(FL_CURSOR_WAIT);
    }
    Fl::flush();
}

extern "C" void unset_wait_cursor(void)
{
    Fl_Window *w;

    if (main_win == NULL || !main_win->shown()) {
        return;
    }

    for (w = Fl::first_window(); w; w = Fl::next_window(w)) {
        w->cursor(FL_CURSOR_DEFAULT);
    }
    if (cur_cursor >= 0) {
        set_cursor(cur_cursor);
    }
    Fl::flush();
}

extern "C" void set_cursor(int c)
{
    cur_cursor = c;
    if (c < 0 || c > 4) {
        cur_cursor = -1;
    }
    canvas_set_cursor_shape(cur_cursor);
}

extern "C" void init_cursors(void)
{
    cur_cursor = -1;
}

/* ------------------------------------------------------------------ */
/* real time input */

static void xmonitor_rti(int fd, void *ib)
{
    set_wait_cursor();

    monitor_input((Input_buffer *) ib, 1, 1);
    undo_touch();

    unset_wait_cursor();
}

extern "C" void xunregister_rti(XtInputId id)
{
    if (main_win != NULL) {
        Fl::remove_fd((int) id);
    }
}

extern "C" void xregister_rti(Input_buffer *ib)
{
    if (main_win != NULL || display_open) {
        Fl::add_fd(ib->fd, FL_READ, xmonitor_rti, ib);
        ib->id = (unsigned long) ib->fd;
    }
}

extern "C" char *display_name(void)
{
#ifdef FLTK_USE_X11
    if (fl_display) {
        return DisplayString(fl_display);
    }
#endif
    return (char *) "";
}

/*
 * redraw all
 */
extern "C" void xdrawgraph(void)
{
    if (inwin && (auto_redraw)) {
	set_wait_cursor();
	drawgraph();
	unset_wait_cursor();
    }
}
