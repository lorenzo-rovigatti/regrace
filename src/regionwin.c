/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-1995 Paul J Turner, Portland, OR
 * Copyright (c) 1996-2000 Grace Development Team
 * Modified in 2026 for regrace; see README.md.
 * 
 * Maintained by Evgeny Stambulchik
 * 
 * 
 *                           All Rights Reserved
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
 * 
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * define regions and operate on regions
 */

#include <config.h>

#include <stdio.h>


#include "globals.h"
#include "graphs.h"
#include "utils.h"
#include "events.h"
#include "motifinc.h"
#include "protos.h"

static Widget but1[2];
static Widget but2[2];
static Widget but3[3];

extern int regiontype;	/* in regionutils.c */

static void do_define_region(void *client_data);
static void do_clear_region(void *client_data);
static void select_area_cb(void *client_data);
static void select_peri_cb(void *client_data);


static Widget *define_region_item;
static Widget *define_type_item;

static char buf[256];

static Widget status_frame;
static Widget status_panel;


static Widget header_w;
static Widget *labx;

static void do_define_region(void *client_data)
{
    int rtype = GetChoice(define_type_item);

    nr = GetChoice(define_region_item);
    define_region(nr, rtype);
}

void create_define_frame(void *data)
{
    static Widget top, dialog;

    set_wait_cursor();
    if (top == NULL) {
	char *label2[2];
	label2[0] = "Define";
	label2[1] = "Close";
	top = CreateDialogShell(app_shell, "Define region");
	handle_close(top);
	dialog = CreateDialogRC(top);

	define_region_item = CreatePanelChoice(dialog,
					       "Define region:",
					       6,
					       "0", "1", "2", "3", "4",
					       NULL);

	define_type_item = CreatePanelChoice(dialog,
					     "Region type:",
					     11,
					     "Inside polygon",
					     "Outside polygon",
					     "Above line",
					     "Below line",
					     "Left of line",
					     "Right of line",
					     "In Horiz. Range",
					     "In Vert. Range",
					     "Out of Horiz. Range",
					     "Out of Vert. Range",
					     NULL);

	CreateSeparator(dialog);

	CreateCommandButtons(dialog, 2, but2, label2);
	AddButtonCB(but2[0], do_define_region, (void *) NULL);
	AddButtonCB(but2[1], destroy_dialog_cb, (void *) top);

	ManageChild(dialog);
    }
    RaiseWindow(top);
    unset_wait_cursor();
}

static Widget *clear_region_item;

static void do_clear_region(void *client_data)
{
    int i;

    set_wait_cursor();
    if (GetChoice(clear_region_item) == MAXREGION) {
	for (i = 0; i < MAXREGION; i++) {
	    kill_region(i);
	}
    } else {
	kill_region(GetChoice(clear_region_item));
    }
    unset_wait_cursor();
    xdrawgraph();
}

void create_clear_frame(void *data)
{
    static Widget top, dialog;

    set_wait_cursor();
    if (top == NULL) {
	char *label1[2];
	label1[0] = "Accept";
	label1[1] = "Close";
	top = CreateDialogShell(app_shell, "Clear region");
	handle_close(top);
	dialog = CreateDialogRC(top);

	clear_region_item = CreatePanelChoice(dialog,
					      "Clear region:",
					      7,
					      "0", "1", "2", "3", "4", "All",
					      NULL);

	CreateSeparator(dialog);

	CreateCommandButtons(dialog, 2, but1, label1);
	AddButtonCB(but1[0], do_clear_region, (void *) NULL);
	AddButtonCB(but1[1], destroy_dialog_cb, (void *) top);

	ManageChild(dialog);
    }
    RaiseWindow(top);
    unset_wait_cursor();
}

Widget arealab, perimlab;

static void select_area_cb(void *client_data)
{
    do_select_area();
}

static void select_peri_cb(void *client_data)
{
    do_select_peri();
}

void create_area_frame(void *data)
{
    static Widget top, dialog;

    set_wait_cursor();
    if (top == NULL) {
	char *label3[3];
	label3[0] = "Area";
	label3[1] = "Perimeter";
	label3[2] = "Close";
	top = CreateDialogShell(app_shell, "Area/perimeter");
	handle_close(top);
	dialog = CreateDialogRC(top);

        arealab = CreateLabel(dialog, "[    Area    ]");
	perimlab = CreateLabel(dialog, "[    Perim    ]");

	CreateSeparator(dialog);

	CreateCommandButtons(dialog, 3, but3, label3);
	AddButtonCB(but3[0], select_area_cb, (void *) NULL);
	AddButtonCB(but3[1], select_peri_cb, (void *) NULL);
	AddButtonCB(but3[2], destroy_dialog_cb, (void *) top);

	ManageChild(dialog);
    }
    RaiseWindow(top);
    unset_wait_cursor();
}

static Widget *reporton_region_item;
static Widget *reporton_type_item;

static void do_reporton_region(void *client_data)
{
    int regno = (int) GetChoice(reporton_region_item);
    int type = (int) GetChoice(reporton_type_item);
    set_wait_cursor();
    reporton_region(get_cg(), regno, type);
    unset_wait_cursor();
}

void create_reporton_frame(void *data)
{
    static Widget top, dialog;

    set_wait_cursor();
    if (top == NULL) {
	char *label1[2];
	label1[0] = "Accept";
	label1[1] = "Close";
	top = CreateDialogShell(app_shell, "Report on sets in region");
	handle_close(top);
	dialog = CreateDialogRC(top);

	reporton_region_item = CreatePanelChoice(dialog,
						   "Report on sets in region:",
						   8,
		   "0", "1", "2", "3", "4", "Inside world", "Outside world",
						   NULL);

	reporton_type_item = CreatePanelChoice(dialog,
						   "Report type:",
						   3,
		   				"Sets", "Points",
						   NULL);

	CreateSeparator(dialog);

	CreateCommandButtons(dialog, 2, but1, label1);
	AddButtonCB(but1[0], do_reporton_region, (void *) NULL);
	AddButtonCB(but1[1], destroy_dialog_cb, (void *) top);

	ManageChild(dialog);
    }
    RaiseWindow(top);
    unset_wait_cursor();
}

static void set_status_label(Widget w, char *buf)
{
    SetLabel(w, buf);
}


void clear_status(void)
{
    int i;
    for (i = 0; i < MAXREGION; i++) {
	set_status_label(labx[i], " ");
    }
}

void update_status_popup(void *client_data)
{
    int rno;
    if (status_frame) {
        clear_status();
        sprintf(buf, " Region # Active  Type");
	set_status_label(header_w, buf);

	for (rno = 0; rno < MAXREGION; rno++) {
	    sprintf(buf, "  %2d    %3s   %6s", rno, on_or_off(rg[rno].active),
		    region_types(rg[rno].type, 0));
	    set_status_label(labx[rno], buf);
	}

    }
}


void define_status_popup(void *data)
{
    set_wait_cursor();
    
    if (status_frame == NULL) {
        int i;
        Widget wbut, rc, rc3, fr2;

	status_frame = CreateDialogShell(app_shell, "Status");
	handle_close(status_frame);

	status_panel = CreateDialogRC(status_frame);

	rc3 = CreateScrolledVContainer(status_panel, 200);
	header_w = CreateLabel(rc3, "");
	SetFixedFont(header_w);
        labx = (Widget *)xmalloc( MAXREGION*sizeof(Widget) );
	for (i = 0; i < MAXREGION; i++) {
            labx[i] = CreateLabel(rc3, "");
            SetFixedFont(labx[i]);
        }

	fr2 = CreateFrame(status_panel, NULL);
	rc = CreateHContainer(fr2);

	wbut = CreateButton(rc, "Close");
	AddButtonCB(wbut, destroy_dialog_cb, (void *) status_frame);

	wbut = CreateButton(rc, "Update");
	AddButtonCB(wbut, update_status_popup, (void *) NULL);

	ManageChild(status_panel);

    }
    RaiseWindow(status_frame);
    update_status_popup(NULL);
    unset_wait_cursor();
}

void define_region(int nr, int rtype)
{
    kill_region(nr);
    xdrawgraph();
    switch (rtype) {
    case 0:
	regiontype = REGION_POLYI;
	do_select_region();
	break;
    case 1:
	regiontype = REGION_POLYO;
	do_select_region();
	break;
    case 2:
	regiontype = REGION_ABOVE;
	set_action(DO_NOTHING);
	set_action(DEF_REGION1ST);
	break;
    case 3:
	regiontype = REGION_BELOW;
	set_action(DO_NOTHING);
	set_action(DEF_REGION1ST);
	break;
    case 4:
	regiontype = REGION_TOLEFT;
	set_action(DO_NOTHING);
	set_action(DEF_REGION1ST);
	break;
    case 5:
	regiontype = REGION_TORIGHT;
	set_action(DO_NOTHING);
	set_action(DEF_REGION1ST);
	break;
    case 6:
        regiontype= REGION_HORIZI;
	set_action(DO_NOTHING);
	set_action(DEF_REGION1ST);
	break;
    case 7:
        regiontype= REGION_VERTI;
	set_action(DO_NOTHING);
	set_action(DEF_REGION1ST);
	break;
    case 8:
        regiontype= REGION_HORIZO;
	set_action(DO_NOTHING);
	set_action(DEF_REGION1ST);
	break;
    case 9:
        regiontype= REGION_VERTO;
	set_action(DO_NOTHING);
	set_action(DEF_REGION1ST);
	break;
    }
}
