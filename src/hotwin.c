/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-1995 Paul J Turner, Portland, OR
 * Copyright (c) 1996-2002 Grace Development Team
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
 *
 * hot links
 *
 */

#include <config.h>

#include <stdio.h>


#include "globals.h"
#include "graphs.h"
#include "parser.h"
#include "utils.h"
#include "motifinc.h"
#include "protos.h"

static Widget hotlink_frame = (Widget) NULL;
static SetChoiceItem hotlink_set_item;
static ListStructure *hotlink_list_item;
static Widget hotlink_file_item;
static Widget *hotlink_source_item;

void create_hotfiles_popup(void *client_data);

/*
 * establish hotlinks by associating a set with a column in a file
 * or the stdout of a process
 */
static void do_hotlink_proc(void *client_data)
{
    int i, numset, src, *sets;
    char fname[256];
    char buf[256];

    set_wait_cursor();

    numset = GetSelectedSets(hotlink_set_item, &sets);
    src = GetChoice(hotlink_source_item);
    strcpy(fname, xv_getstr(hotlink_file_item));

    if (numset == SET_SELECT_ERROR) {
        errwin("No set selected");
        unset_wait_cursor();
        return;
    }
    if (fname[0] == '\0') {
        errwin("No source selected");
        unset_wait_cursor();
        return;
    }

	for( i=0; i<numset; i++ ) {
		set_hotlink(get_cg(), sets[i], i+1, fname, src==0?SOURCE_DISK:SOURCE_PIPE);
		if( numset == 1 )
			setcomment( get_cg(), sets[i], fname );
		else {
			sprintf( buf, "%s:%d", fname, i+1 );
			setcomment( get_cg(), sets[i], buf );
		}
	}
    update_hotlinks();

    unset_wait_cursor();
}

/*
 * unlink sets
 */
static void do_hotunlink_proc(void *client_data)
{
    int  i, cnt, setno, *values;

    set_wait_cursor();

    cnt = GetListChoices(hotlink_list_item, &values);
    if( cnt ) {
		for (i = 0; i < cnt; i++) {
			setno = values[i];
			if (setno >= 0 && setno < number_of_sets(get_cg())) {
				set_hotlink(get_cg(), setno, FALSE, NULL, 0);
			}
		}
		xfree(values);
		update_hotlinks();
	}
    unset_wait_cursor();
}

/*
 * update hot links displayed in scrolled list
 */
void update_hotlinks(void)
{
    int j, n = 0;
    char buf[256];
    OptionItem *items;

    if (hotlink_frame != NULL) {
		set_wait_cursor();
		items = xmalloc((number_of_sets(get_cg()) + 1)*sizeof(OptionItem));
		for (j = 0; j < number_of_sets(get_cg()); j++) {
			if (is_hotlinked(get_cg(), j)) {
				sprintf(buf, "G%d.S%d -> %s -> %s:%d", get_cg(), j, 
					get_hotlink_src(get_cg(), j) == SOURCE_DISK ? "DISK" : "PIPE", 
					get_hotlink_file(get_cg(), j), is_hotlinked(get_cg(),j) );
				items[n].value = j;
				items[n].label = copy_string(NULL, buf);
				n++;
			}
		}
		UpdateListChoice(hotlink_list_item, n, items);
		for (j = 0; j < n; j++) {
			xfree(items[j].label);
		}
		xfree(items);
		unset_wait_cursor();
    }
}

/*
 * update the sets in the current graph
 */
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

/*
 * create the big hot links widget
 */
void create_hotlinks_popup(void *data)
{
    static Widget top, dialog;
    set_wait_cursor();
    if (top == NULL) {
	char *label1[5];
	Widget but1[5];
	label1[0] = "Link";
	label1[1] = "Files...";
	label1[2] = "Unlink";
	label1[3] = "Update";
	label1[4] = "Close";
	top = CreateDialogShell(app_shell, "Hot links");
	handle_close(top);
	dialog = CreateDialogRC(top);

	hotlink_list_item = CreateListChoice(dialog, "Hot links:",
            LIST_TYPE_MULTIPLE, 5, 0, NULL);

        hotlink_set_item = CreateSetSelector(dialog, "Link set:",
                SET_SELECT_ACTIVE,
                FILTER_SELECT_ALL,
                GRAPH_SELECT_CURRENT,
                SELECTION_TYPE_MULTIPLE);

	hotlink_file_item = CreateTextItem2(dialog, 30, "To file or SOURCE_PIPE:");
	hotlink_source_item = CreatePanelChoice(dialog, "Source: ", 3,
						"Disk file",
						"Pipe",
						NULL);

	CreateSeparator(dialog);

	CreateCommandButtons(dialog, 5, but1, label1);
	AddButtonCB(but1[0], do_hotlink_proc, (void *) NULL);
	AddButtonCB(but1[1], create_hotfiles_popup, (void *) NULL);
	AddButtonCB(but1[2], do_hotunlink_proc, (void *) NULL);
	AddButtonCB(but1[3], do_hotupdate_proc, (void *) NULL);
	AddButtonCB(but1[4], destroy_dialog_cb, (void *) top);

	ManageChild(dialog);
	hotlink_frame = top;
    }
    RaiseWindow(top);
    update_hotlinks();
    unset_wait_cursor();
}


/*
 * callback for the file selection widget
 * this routine copies the selected file into the text widget on the main
 * hot links widget
 */
static int do_hotlinkfile_proc(char *filename, void *data)
{
    xv_setstr(hotlink_file_item, filename);
    return TRUE;
}

/*
 * create file selection pop up to choose the file to hot link
 */
void create_hotfiles_popup(void *client_data)
{
    static FSBStructure *fsb = NULL;

    set_wait_cursor();

    if (fsb == NULL) {
        fsb = CreateFileSelectionBox(app_shell, "Select hot link file");
	AddFileSelectionBoxCB(fsb, do_hotlinkfile_proc, NULL);
        ManageChild(fsb->FSB);
    }
    
    RaiseWindow(fsb->dialog);

    unset_wait_cursor();
}
