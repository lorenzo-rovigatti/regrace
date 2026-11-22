/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
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
 * Internal interface of the FLTK GUI.
 *
 * Everything declared here is callable from both C (the code ported
 * from the Motif GUI, e.g. fl_events.c) and C++ (the FLTK widgets).
 */

#ifndef __FL_GUI_H_
#define __FL_GUI_H_

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- canvas events (fl_events.c) ---- */
#define CANVAS_EV_MOTION    0
#define CANVAS_EV_PRESS     1
#define CANVAS_EV_KEY       2

#define CANVAS_BUTTON1      1
#define CANVAS_BUTTON2      2
#define CANVAS_BUTTON3      3

/* x, y are in canvas pixels */
void canvas_event(int type, int x, int y, int button, int dbl_click, int key);

/* canvas keyboard shortcuts (the old Xt translation table) */
int canvas_shortcut(int ctrl, int alt, int key);

/* ---- canvas / driver (fl_drv.cpp) ---- */
void canvas_get_pixel_size(unsigned int *w, unsigned int *h);
void canvas_set_pixel_size(unsigned int w, unsigned int h);
void canvas_redisplay(void);
double canvas_scale(void);
void overlay_clear(void);

/* ---- main window (fl_mainwin.cpp) ---- */
void set_locator_text(const char *s);
void set_stack_labels(const char *sd, const char *cw);
void canvas_set_cursor_shape(int c);
void gui_open_project(void);
void gui_save_project_as(void);
void gui_fatal_no_display(void);

/* ---- dialogs (fl_dialogs.cpp) ---- */
void gui_not_ported(const char *what);
void create_monitor_frame(int force, char *msg);

/* ---- from the Motif event code, kept for fl_events.c ---- */
void set_stack_message(void);
void do_clear_point(void *data);
void autoon_proc(void *data);
void autoticks_proc(void *data);

#ifdef __cplusplus
}
#endif

#endif /* __FL_GUI_H_ */
