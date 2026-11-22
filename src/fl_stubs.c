/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 *
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 *
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
 * Placeholders for the dialogs of the Motif GUI which have not been
 * ported to FLTK yet. Opening one of them tells the user so; the
 * "update" hooks (called by the core to refresh open dialogs) do
 * nothing.
 *
 * When a dialog gets ported, remove its entry from here.
 *
 */

#include <config.h>

#include "defines.h"
#include "globals.h"
#include "utils.h"
#include "graphs.h"
#include "events.h"
#include "protos.h"

#include "fl_gui.h"

#define NOT_PORTED(func, name) \
void func(void *data) \
{ \
    gui_not_ported(name); \
}

/* File menu */
NOT_PORTED(create_printer_setup, "Print/Page setup")

/* Edit menu */
NOT_PORTED(create_datasetprop_popup, "Data sets")
NOT_PORTED(create_setop_popup, "Set operations")
NOT_PORTED(create_arrange_frame, "Arrange graphs")
NOT_PORTED(create_overlay_frame, "Overlay graphs")
NOT_PORTED(create_autos_frame, "Autoscale graphs")
NOT_PORTED(define_status_popup, "Region status")
NOT_PORTED(create_define_frame, "Define region")
NOT_PORTED(create_clear_frame, "Clear region")
NOT_PORTED(create_reporton_frame, "Report on region")
NOT_PORTED(create_area_frame, "Area/perimeter")
NOT_PORTED(create_region_frame, "Regions")
NOT_PORTED(create_hotlinks_popup, "Hot links")
NOT_PORTED(create_locator_frame, "Locator props")
NOT_PORTED(create_props_frame, "Preferences")

/* Data menu */
NOT_PORTED(create_datasetop_popup, "Data set operations")
NOT_PORTED(create_eval_frame, "Evaluate expression")
NOT_PORTED(create_load_frame, "Load values")
NOT_PORTED(create_histo_frame, "Histograms")
NOT_PORTED(create_fourier_frame, "Fourier transforms")
NOT_PORTED(create_run_frame, "Running averages")
NOT_PORTED(create_diff_frame, "Differences")
NOT_PORTED(create_seasonal_frame, "Seasonal differences")
NOT_PORTED(create_int_frame, "Integration")
NOT_PORTED(create_interp_frame, "Interpolation/splines")
NOT_PORTED(create_reg_frame, "Regression")
NOT_PORTED(create_nonl_frame, "Non-linear curve fitting")
NOT_PORTED(create_xcor_frame, "Correlation/covariance")
NOT_PORTED(create_digf_frame, "Digital filter")
NOT_PORTED(create_lconv_frame, "Linear convolution")
NOT_PORTED(create_geom_frame, "Geometric transforms")
NOT_PORTED(create_samp_frame, "Sample points")
NOT_PORTED(create_prune_frame, "Prune data")
NOT_PORTED(create_featext_frame, "Feature extraction")
NOT_PORTED(create_netcdfs_popup, "Import NetCDF")
NOT_PORTED(create_write_popup, "Export ASCII")
NOT_PORTED(create_saveall_popup, "Save all")

/* Plot menu */
NOT_PORTED(create_plot_frame_cb, "Plot appearance")
NOT_PORTED(create_graphapp_frame_cb, "Graph appearance")
NOT_PORTED(define_symbols_popup, "Set appearance")
NOT_PORTED(create_axes_dialog_cb, "Axis properties")
NOT_PORTED(create_rparams_popup, "Load parameters")
NOT_PORTED(create_wparam_frame, "Save parameters")
NOT_PORTED(create_graph_frame, "Graphs")
NOT_PORTED(create_world_frame, "World")

/* Window menu */
NOT_PORTED(create_points_frame, "Point explorer")
NOT_PORTED(define_objects_popup, "Drawing objects")
NOT_PORTED(create_fonttool_cb, "Font tool")

void create_plot_frame(void)
{
    gui_not_ported("Plot appearance");
}

void create_graphapp_frame(int gno)
{
    gui_not_ported("Graph appearance");
}

void create_axes_dialog(int axisno)
{
    gui_not_ported("Axis properties");
}

void create_eblock_frame(int gno)
{
    gui_not_ported("Edit block data");
}

void create_ss_frame(int gno, int setno)
{
    gui_not_ported("Spreadsheet data set editor");
}

void do_ext_editor(int gno, int setno)
{
    gui_not_ported("External editor");
}

void box_edit_popup(int no)
{
    gui_not_ported("Edit box");
}

void ellipse_edit_popup(int no)
{
    gui_not_ported("Edit ellipse");
}

void line_edit_popup(int no)
{
    gui_not_ported("Edit line");
}

void string_edit_popup(int no)
{
    gui_not_ported("Edit string");
}

int object_edit_popup(int type, int id)
{
    switch (type) {
    case OBJECT_BOX:
        box_edit_popup(id);
        break;
    case OBJECT_ELLIPSE:
        ellipse_edit_popup(id);
        break;
    case OBJECT_LINE:
        line_edit_popup(id);
        break;
    case OBJECT_STRING:
        string_edit_popup(id);
        break;
    default:
        return RETURN_FAILURE;
    }

    return RETURN_SUCCESS;
}

void define_region(int nr, int rtype)
{
    gui_not_ported("Define region");
}

/* hot links: the update itself does not need a dialog */
void do_hotupdate_proc(void *data)
{
    int gno, setno;

    set_wait_cursor();

    /* do links */
    for (gno = 0; gno < number_of_graphs(); gno++) {
        for (setno = 0; setno < number_of_sets(gno); setno++) {
            do_update_hotlink(gno, setno);
        }
    }

    unset_wait_cursor();
    xdrawgraph();
}

/* point explorer state (ptswin.c) */
static int track_setno = -1;
static int track_add_at = ADD_POINT_END;
static int track_move_dir = MOVE_POINT_XY;

void update_point_locator(int gno, int setno, int loc)
{
}

void get_tracking_props(int *setno, int *move_dir, int *add_at)
{
    *setno = track_setno;
    *move_dir = track_move_dir;
    *add_at = track_add_at;
}

/* hooks used to refresh open dialogs: nothing to refresh yet */
void update_set_lists(int gno) {}
void updatesymbols(int gno, int value) {}
void updatelegends(int gno) {}
void update_view(int gno) {}
void update_ticks(int gno) {}
void update_locator_items(int gno) {}
void update_hotlinks(void) {}
void update_nonl_frame(void) {}
void update_prune_frame(void) {}
void update_misc_items(void) {}
void update_props_items(void) {}
void update_ss_editors(int gno) {}
void set_graph_selectors(int gno) {}
