/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-1995 Paul J Turner, Portland, OR
 * Copyright (c) 1996-2000 Grace Development Team
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
 * GUI utilities shared by all toolkits (see guiutils.c).
 * Internal to the GUI; motifinc.h has the public API.
 */

#ifndef __GUIUTILS_H_
#define __GUIUTILS_H_

#include "motifinc.h"

typedef struct {
    Widget popup;
    Widget label_item;
    Widget focus_item;
    Widget hide_item;
    Widget show_item;
    Widget duplicate_item;
    Widget kill_item;
    Widget copy12_item;
    Widget copy21_item;
    Widget move12_item;
    Widget move21_item;
    Widget swap_item;
} GraphPopupMenu;

typedef enum {
    GraphMenuFocusCB,
    GraphMenuHideCB,
    GraphMenuShowCB,
    GraphMenuDuplicateCB,
    GraphMenuKillCB,
    GraphMenuCopy12CB,
    GraphMenuCopy21CB,
    GraphMenuMove12CB,
    GraphMenuMove21CB,
    GraphMenuSwapCB,
    GraphMenuNewCB
} GraphMenuCBtype;

/* selectors kept up to date by update_*_selectors() */
extern OptionItem *color_option_items;
extern int ncolor_option_items;
extern OptionStructure **color_selectors;
extern int ncolor_selectors;

extern OptionItem *graph_select_items;
extern int ngraph_select_items;
extern ListStructure **graph_selectors;
extern int ngraph_selectors;

extern ListStructure **set_selectors;
extern int nset_selectors;

extern SetChoiceItem *plist;
extern int nplist;

void graph_menu_cb(ListStructure *listp, GraphMenuCBtype type);
void set_menu_cb(ListStructure *listp, SetMenuCBtype type);

void bringf_set_proc(void *data);
void sendb_set_proc(void *data);
void pack_set_proc(void *data);
void shownd_set_proc(int onoff, void *data);
void showh_set_proc(int onoff, void *data);
void view_comments_set_proc(int onoff, void *data);
void update_set_proc(void *data);

void update_graph_selectors(void);
void update_set_selectors(int gno);
void update_color_selectors(void);

/* toolkit-specific */
void paint_color_selector(OptionStructure *optp);

#endif /* __GUIUTILS_H_ */
