/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 *
 * Written in 2026 for regrace; see README.md.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 */

/*
 * C++-only declarations shared between the FLTK GUI source files.
 */

#ifndef __FL_WIDGETS_H_
#define __FL_WIDGETS_H_

class Fl_Widget;
class Fl_Window;

/* the drawing canvas (fl_drv.cpp) */
Fl_Widget *fl_create_canvas(int X, int Y, int W, int H);
Fl_Widget *fl_canvas_widget(void);

/* the main application window (fl_mainwin.cpp) */
Fl_Window *fl_main_window(void);

/* the dialog widget layer (fl_motifutils.cpp) */
void fl_register_app_shell(Fl_Window *win);
void fl_install_help_keys(void);

/* putting custom FLTK widgets in dialogs made with the motifinc.h API */
typedef struct _GraceWidget *Widget;
void fl_begin_in(Widget parent);
void fl_end_in(void);
Widget fl_wrap_widget(Widget parent, Fl_Widget *w, int pref_w, int pref_h,
    int hexp, int vexp);
Fl_Widget *fl_widget_of(Widget w);
void fl_set_text_changed_cb(Widget w, void (*cb)(void *), void *data);

/* apply the GUI look (scheme, colors, font, scale) from preferences */
void fl_apply_gui_prefs(void);

#endif /* __FL_WIDGETS_H_ */
