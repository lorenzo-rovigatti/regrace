/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 *
 * Copyright (c) 1996-2000 Grace Development Team
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

/* apply the GUI look (scheme, colors, font, scale) from preferences */
void fl_apply_gui_prefs(void);

#endif /* __FL_WIDGETS_H_ */
