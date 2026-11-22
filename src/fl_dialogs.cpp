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
 * Dialogs of the FLTK GUI that have been ported so far:
 *   - console (monwin.c), yes/no questions (motifutils.c)
 *   - commands window (comwin.c)
 *   - open/save project (fileswin.c), import ASCII data (fileswin.c)
 *   - about, help (helpwin.c)
 *   - GUI appearance (new)
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Flex.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Preferences.H>
#include <FL/Fl_Tooltip.H>
#include <FL/fl_ask.H>
#include <FL/fl_utf8.h>
#include <FL/filename.H>

extern "C" {
#include "defines.h"
#include "globals.h"
#include "utils.h"
#include "graphs.h"
#include "graphutils.h"
#include "plotone.h"
#include "parser.h"
#include "protos.h"
}
#include "fl_files.h"

#include "fl_gui.h"
#include "fl_widgets.h"

#define DLG_ROW 26

/* ------------------------------------------------------------------ */
/* helpers */

/* standard dialog window, closed (hidden) by the window manager */
static Fl_Double_Window *dialog_window(int W, int H, const char *title)
{
    Fl_Double_Window *w = new Fl_Double_Window(W, H);
    w->copy_label(title);
    w->xclass("XMgrace");
    return w;
}

static void hide_window_cb(Fl_Widget *, void *data)
{
    ((Fl_Window *) data)->hide();
}

static void raise_window(Fl_Window *w)
{
    w->show();
}

static void help_button_cb(Fl_Widget *, void *data)
{
    HelpCB(data);
}

static int choose_file(int save, const char *title, const char *filter,
    std::string &result)
{
    Fl_Native_File_Chooser fc;

    fc.title(title);
    fc.type(save ? Fl_Native_File_Chooser::BROWSE_SAVE_FILE :
                   Fl_Native_File_Chooser::BROWSE_FILE);
    if (save) {
        fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM |
                   Fl_Native_File_Chooser::NEW_FOLDER);
    }
    if (filter) {
        fc.filter(filter);
    }
    fc.directory(get_workingdir());

    switch (fc.show()) {
    case 0:
        result = fc.filename();
        return TRUE;
    case -1:
        errmsg(fc.errmsg());
        return FALSE;
    default:
        return FALSE;
    }
}

extern "C" void gui_not_ported(const char *what)
{
    fl_message_title("Grace: not yet ported");
    fl_message("The \"%s\" dialog has not been ported to the FLTK interface yet.\n\n"
        "Meanwhile, you can use the equivalent commands of the Grace command\n"
        "language in Window/Commands, or load a parameter file.", what);
}

/* ------------------------------------------------------------------ */
/* console */

struct ConsoleUI {
    Fl_Double_Window *win;
    Fl_Text_Display *text;
    Fl_Text_Buffer *buf;
    int popup_only_on_errors;
};

static ConsoleUI *console = NULL;

static void console_save_cb(Fl_Widget *, void *)
{
    std::string fn;
    if (choose_file(TRUE, "Save logs", NULL, fn)) {
        FILE *pp = grace_openw((char *) fn.c_str());
        if (pp != NULL) {
            char *text = console->buf->text();
            fwrite(text, SIZEOF_CHAR, strlen(text), pp);
            free(text);
            grace_close(pp);
        }
    }
}

static void console_clear_cb(Fl_Widget *, void *)
{
    console->buf->text("");
}

static void console_popup_cb(Fl_Widget *w, void *)
{
    Fl_Menu_Bar *mb = (Fl_Menu_Bar *) w;
    const Fl_Menu_Item *mi = mb->mvalue();
    console->popup_only_on_errors = (mi && mi->value()) ? TRUE:FALSE;
}

static void console_close_cb(Fl_Widget *, void *)
{
    console->win->hide();
}

extern "C" void create_monitor_frame(int force, char *msg)
{
    if (console == NULL) {
        Fl_Menu_Bar *mb;

        console = new ConsoleUI;
        console->popup_only_on_errors = FALSE;
        console->win = dialog_window(560, 260, "Console");
        mb = new Fl_Menu_Bar(0, 0, 560, DLG_ROW);
        mb->add("&File/&Save...", 0, console_save_cb);
        mb->add("&File/&Close", 0, console_close_cb);
        mb->add("&Edit/&Clear", 0, console_clear_cb);
        mb->add("&Options/Popup only on &errors", 0, console_popup_cb, NULL,
            FL_MENU_TOGGLE);
        mb->add("&Help/On &console", 0, help_button_cb,
            (void *) "doc/UsersGuide.html#console");
        console->buf = new Fl_Text_Buffer();
        console->text = new Fl_Text_Display(0, DLG_ROW, 560, 260 - DLG_ROW);
        console->text->buffer(console->buf);
        console->text->textfont(FL_COURIER);
        console->win->resizable(console->text);
        console->win->end();
    }

    if (msg != NULL) {
        console->buf->append(msg);
        console->text->insert_position(console->buf->length());
        console->text->show_insert_position();
    }

    if (force || console->popup_only_on_errors == FALSE) {
        raise_window(console->win);
    }
}

extern "C" void stufftextwin(char *msg)
{
    create_monitor_frame(FALSE, msg);
}

extern "C" void errwin(const char *msg)
{
    char *buf;

    buf = copy_string(NULL, "[Error] ");
    buf = concat_strings(buf, msg);
    buf = concat_strings(buf, "\n");

    create_monitor_frame(TRUE, buf);

    xfree(buf);
}

extern "C" void create_monitor_frame_cb(void *data)
{
    create_monitor_frame(TRUE, NULL);
}

/* ------------------------------------------------------------------ */
/* yes/no */

extern "C" int yesnowin(char *msg, char *s1, char *s2, char *help_anchor)
{
    int r;

    fl_message_title("Grace: Warning");
    r = fl_choice("%s", s2 ? s2:"Cancel", s1 ? s1:"OK", NULL,
        msg ? msg:"");

    return (r == 1) ? TRUE:FALSE;
}

/* ------------------------------------------------------------------ */
/* commands window */

class CommandInput : public Fl_Input {
public:
    CommandInput(int X, int Y, int W, int H, const char *L = 0) :
        Fl_Input(X, Y, W, H, L) {}
    int handle(int event);
};

static Fl_Double_Window *com_win = NULL;
static Fl_Hold_Browser *com_hist = NULL;
static CommandInput *com_input = NULL;

int CommandInput::handle(int event)
{
    if (event == FL_KEYBOARD && com_hist->size() > 0) {
        int n = com_hist->value();
        if (Fl::event_key() == FL_Up) {
            n = (n <= 1) ? com_hist->size() : n - 1;
            com_hist->value(n);
            value(com_hist->text(n));
            insert_position(size());
            return 1;
        } else if (Fl::event_key() == FL_Down) {
            n = (n >= com_hist->size() || n == 0) ? 1 : n + 1;
            com_hist->value(n);
            value(com_hist->text(n));
            insert_position(size());
            return 1;
        }
    }
    return Fl_Input::handle(event);
}

static void com_exec_cb(Fl_Widget *, void *)
{
    std::string s = com_input->value();

    if (s.empty()) {
        return;
    }
    com_hist->add(s.c_str());
    com_hist->value(0);
    com_hist->bottomline(com_hist->size());
    com_input->value("");

    scanner((char *) s.c_str());
}

static void com_hist_cb(Fl_Widget *, void *)
{
    int n = com_hist->value();
    if (n > 0) {
        com_input->value(com_hist->text(n));
        if (Fl::event_clicks()) {
            /* double click executes */
            Fl::event_clicks(0);
            scanner((char *) com_hist->text(n));
        }
    }
}

static void com_add_cb(Fl_Widget *, void *)
{
    const char *s = com_input->value();
    int n = com_hist->value();
    if (s[0] != '\0') {
        if (n > 0) {
            com_hist->insert(n + 1, s);
            com_hist->value(n + 1);
        } else {
            com_hist->add(s);
        }
        com_input->value("");
    }
}

static void com_delete_cb(Fl_Widget *, void *)
{
    int n = com_hist->value();
    if (n > 0) {
        com_hist->remove(n);
    }
}

static void com_replace_cb(Fl_Widget *, void *)
{
    const char *s = com_input->value();
    int n = com_hist->value();
    if (s[0] != '\0' && n > 0) {
        com_hist->text(n, s);
        com_input->value("");
    }
}

static void com_move_cb(Fl_Widget *, void *data)
{
    int n = com_hist->value();
    int down = (int) (long) data;
    if (n > 0) {
        int to;
        if (down) {
            to = (n < com_hist->size()) ? n + 1 : 1;
        } else {
            to = (n > 1) ? n - 1 : com_hist->size();
        }
        com_hist->move(to, n);
        com_hist->value(to);
    }
}

static void com_read_cb(Fl_Widget *, void *)
{
    std::string fn;
    if (choose_file(FALSE, "Read history", "*.com", fn)) {
        char buf[512];
        FILE *fp = grace_openr((char *) fn.c_str(), SOURCE_DISK);
        if (fp != NULL) {
            while (grace_fgets(buf, 255, fp) != NULL) {
                int sl = strlen(buf);
                if (sl > 0 && buf[sl - 1] == '\n') {
                    buf[sl - 1] = '\0';
                }
                if (strlen(buf) == 0) {
                    continue;
                }
                com_hist->add(buf);
            }
            grace_close(fp);
        }
    }
}

static void com_save_cb(Fl_Widget *, void *)
{
    std::string fn;
    if (choose_file(TRUE, "Write history", "*.com", fn)) {
        FILE *pp = grace_openw((char *) fn.c_str());
        if (pp != NULL) {
            int i;
            for (i = 1; i <= com_hist->size(); i++) {
                fprintf(pp, "%s\n", com_hist->text(i));
            }
            grace_close(pp);
        }
    }
}

static void com_clear_cb(Fl_Widget *, void *)
{
    com_hist->clear();
}

#define MAXERR 5
static void com_replay_cb(Fl_Widget *, void *)
{
    int i, errcount = 0;

    set_wait_cursor();
    for (i = 1; i <= com_hist->size(); i++) {
        if (scanner((char *) com_hist->text(i)) != RETURN_SUCCESS) {
            char buf[256];
            errcount++;
            sprintf(buf, "Error at line %d", i);
            errmsg(buf);
            if (errcount > MAXERR) {
                if (yesno((char *) "Lots of errors, cancel?", NULL, NULL, NULL)) {
                    break;
                } else {
                    errcount = 0;
                }
            }
        }
    }
    unset_wait_cursor();
}

static Fl_Button *flex_button(Fl_Flex *parent, const char *label,
    Fl_Callback *cb, void *data = NULL)
{
    Fl_Button *b = new Fl_Button(0, 0, 10, DLG_ROW, label);
    b->callback(cb, data);
    return b;
}

extern "C" void open_command(void *data)
{
    if (com_win == NULL) {
        int W = 520, H = 360;
        Fl_Flex *col, *row;

        com_win = dialog_window(W, H, "Commands");
        col = new Fl_Flex(0, 0, W, H, Fl_Flex::VERTICAL);
        col->margin(6);
        col->gap(4);

        com_hist = new Fl_Hold_Browser(0, 0, W, 100);
        com_hist->callback(com_hist_cb);
        com_hist->textfont(FL_COURIER);

        row = new Fl_Flex(0, 0, W, DLG_ROW, Fl_Flex::HORIZONTAL);
        Fl_Box *lab = new Fl_Box(0, 0, 70, DLG_ROW, "Command:");
        lab->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        row->fixed(lab, 70);
        com_input = new CommandInput(0, 0, 10, DLG_ROW);
        com_input->textfont(FL_COURIER);
        com_input->when(FL_WHEN_ENTER_KEY_ALWAYS);
        com_input->callback(com_exec_cb);
        row->end();
        col->fixed(row, DLG_ROW);

        row = new Fl_Flex(0, 0, W, DLG_ROW, Fl_Flex::HORIZONTAL);
        row->gap(2);
        flex_button(row, "Add", com_add_cb)->tooltip("Add to history without executing");
        flex_button(row, "Delete", com_delete_cb);
        flex_button(row, "Replace", com_replace_cb);
        flex_button(row, "Up", com_move_cb, (void *) 0);
        flex_button(row, "Down", com_move_cb, (void *) 1);
        row->end();
        col->fixed(row, DLG_ROW);

        row = new Fl_Flex(0, 0, W, DLG_ROW, Fl_Flex::HORIZONTAL);
        row->gap(2);
        flex_button(row, "Read...", com_read_cb);
        flex_button(row, "Save...", com_save_cb);
        flex_button(row, "Clear", com_clear_cb);
        flex_button(row, "Replay", com_replay_cb);
        flex_button(row, "Close", hide_window_cb, com_win);
        flex_button(row, "Help", help_button_cb,
            (void *) "doc/UsersGuide.html#commands");
        row->end();
        col->fixed(row, DLG_ROW);

        col->end();
        com_win->resizable(col);
        com_win->end();
    }
    raise_window(com_win);
    com_input->take_focus();
}

/* ------------------------------------------------------------------ */
/* about & help */

static Fl_Double_Window *about_win = NULL;

extern "C" void create_about_grtool(void *data)
{
    if (about_win == NULL) {
        int W = 520, H = 470;
        std::string s;
        char buf[1024];
        Fl_Flex *col;
        Fl_Text_Display *td;
        Fl_Text_Buffer *tb;

        about_win = dialog_window(W, H, "About");
        col = new Fl_Flex(0, 0, W, H, Fl_Flex::VERTICAL);
        col->margin(8);
        col->gap(6);

        Fl_Box *title = new Fl_Box(0, 0, W, 30, bi_version_string());
        title->labelfont(FL_HELVETICA_BOLD);
        title->labelsize(FL_NORMAL_SIZE + 4);
        col->fixed(title, 30);

        s += "Copyright (c) 1991-1995 Paul J Turner\n";
        s += "Copyright (c) 1996-2015 Grace Development Team\n";
        s += "Maintained by Evgeny Stambulchik\n";
        s += "All rights reserved\n";
        s += "The program is distributed under the terms of the "
             "GNU General Public License\n\n";
        s += "Third party copyrights:\n";
        s += "  FLTK, Copyright (c) 1998-2026 Bill Spitzak and others\n";
        s += "  Raster driver based on the GD-1.3 library,\n";
        s += "    Portions copyright (c) 1994-1998 Cold Spring Harbor Laboratory\n";
        s += "    Portions copyright (c) 1996-1998 Boutell.Com, Inc\n";
#ifdef HAVE_LIBPDF
        s += "  PDFlib library, Copyright (c) 1997-2012 PDFlib GmbH\n";
#endif
        s += "\nBuild info:\n";
        sprintf(buf, "  Host: %s\n", bi_system());
        s += buf;
        sprintf(buf, "  Time: %s\n", bi_date());
        s += buf;
        sprintf(buf, "  GUI toolkit: %s\n", bi_gui());
        s += buf;
        sprintf(buf, "  T1lib: %s\n", bi_t1lib());
        s += buf;
#ifdef HAVE_LIBPNG
        sprintf(buf, "  libpng: %s\n", bi_pnglib());
        s += buf;
#endif
#ifdef HAVE_LIBJPEG
        sprintf(buf, "  libjpeg: %s\n", bi_libjpeg());
        s += buf;
#endif
#ifdef HAVE_LIBPDF
        sprintf(buf, "  PDFlib: %s\n", bi_libpdf());
        s += buf;
#endif
        s += "\nHome page: http://plasma-gate.weizmann.ac.il/Grace/\n";

        tb = new Fl_Text_Buffer();
        tb->text(s.c_str());
        td = new Fl_Text_Display(0, 0, W, 100);
        td->buffer(tb);
        td->box(FL_FLAT_BOX);
        td->color(FL_BACKGROUND_COLOR);

        Fl_Flex *row = new Fl_Flex(0, 0, W, DLG_ROW, Fl_Flex::HORIZONTAL);
        new Fl_Box(0, 0, 10, DLG_ROW);
        Fl_Return_Button *b = new Fl_Return_Button(0, 0, 90, DLG_ROW, "Close");
        b->callback(hide_window_cb, about_win);
        row->fixed(b, 90);
        new Fl_Box(0, 0, 10, DLG_ROW);
        row->end();
        col->fixed(row, DLG_ROW);

        col->end();
        about_win->resizable(col);
        about_win->end();
    }
    raise_window(about_win);
}

extern "C" void HelpCB(void *data)
{
    char *URL, *ha;
    int remote;

    ha = (char *) data;

    if (ha == NULL) {
        ha = (char *) "doc/nohelp.html";
    }

    if (strstr(ha, "http:") || strstr(ha, "https:") ||
        strstr(ha, "ftp:") || strstr(ha, "mailto:")) {
        URL = copy_string(NULL, ha);
        remote = TRUE;
    } else {
        char *p, *pa;

        if (ha == strstr(ha, "file:")) {
            p = (ha + 5);
        } else {
            p = ha;
        }

        URL = copy_string(NULL, "file://");
        pa = strchr(p, '#');
        if (pa) {
            char *base = copy_string(NULL, p);
            base[pa - p] = '\0';
            URL = concat_strings(URL, grace_path(base));
            URL = concat_strings(URL, pa);
            xfree(base);
        } else {
            URL = concat_strings(URL, grace_path(p));
        }

        remote = FALSE;
    }

    if (getenv("GRACE_HELPVIEWER") == NULL) {
        /* use the desktop default browser */
        char msg[512];
        if (!fl_open_uri(URL, msg, sizeof(msg))) {
            errmsg(msg);
        }
    } else {
        char *help_viewer, *command;
        int i, j, len, urllen, comlen;

        help_viewer = get_help_viewer();
        len = strlen(help_viewer);
        urllen = strlen(URL);
        for (i = 0, comlen = len; i < len - 1; i++) {
    	    if ((help_viewer[i] == '%') && (help_viewer[i + 1] == 's')){
    	        comlen += urllen - 2;
    	        i++;
    	    }
        }
        command = (char *) xmalloc((comlen + 1)*SIZEOF_CHAR);
        command[comlen] = '\0';
        for (i = 0, j = 0; i < len; i++) {
    	    if ((help_viewer[i] == '%') && (help_viewer[i + 1] == 's')){
    	        strcpy (&command[j], URL);
    	        j += urllen;
    	        i++;
    	    } else {
    	        command[j++] = help_viewer[i];
    	    }
        }
        command = concat_strings(command, "&");
        system_wrap(command);
        xfree(command);
    }
    (void) remote;

    xfree(URL);
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* GUI appearance (new in the FLTK GUI) */

#define PREFS_VENDOR "grace"
#define PREFS_APP    "xmgrace"

static const char *schemes[] = {"base", "gtk+", "gleam", "oxy", "plastic"};
static const char *scheme_labels[] = {"Classic", "GTK+", "Gleam", "Oxy", "Plastic"};
#define N_SCHEMES 5

static const char *palettes[] = {"light", "grace", "dark"};
static const char *palette_labels[] = {"Light", "Grace (Motif colors)", "Dark"};
#define N_PALETTES 3

struct GuiPrefs {
    char scheme[32];
    char palette[32];
    char font[256];
    double scale;
};

static GuiPrefs gui_prefs;

static void load_gui_prefs(void)
{
    strcpy(gui_prefs.scheme, "gtk+");
    strcpy(gui_prefs.palette, "light");
    gui_prefs.font[0] = '\0';
    gui_prefs.scale = 0.0;
    
    Fl_Preferences p(Fl_Preferences::USER_L, PREFS_VENDOR, PREFS_APP);
    if (!p.groupExists("gui")) {
        /* don't create a preferences file just by reading it */
        return;
    }
    Fl_Preferences g(p, "gui");
    g.get("scheme", gui_prefs.scheme, "gtk+", sizeof(gui_prefs.scheme));
    g.get("palette", gui_prefs.palette, "light", sizeof(gui_prefs.palette));
    g.get("font", gui_prefs.font, "", sizeof(gui_prefs.font));
    g.get("scale", gui_prefs.scale, 0.0);
}

static void save_gui_prefs(void)
{
    Fl_Preferences p(Fl_Preferences::USER_L, PREFS_VENDOR, PREFS_APP);
    Fl_Preferences g(p, "gui");
    g.set("scheme", gui_prefs.scheme);
    g.set("palette", gui_prefs.palette);
    g.set("font", gui_prefs.font);
    g.set("scale", gui_prefs.scale);
    p.flush();
}

static void apply_palette(const char *name)
{
    if (strcmp(name, "dark") == 0) {
        Fl::background(0x33, 0x33, 0x36);
        Fl::background2(0x22, 0x22, 0x25);
        Fl::foreground(0xe6, 0xe6, 0xe6);
        Fl::set_color(FL_SELECTION_COLOR, 0x3d, 0x6e, 0xb4);
        Fl::set_color(FL_INACTIVE_COLOR, 0x80, 0x80, 0x80);
        Fl_Tooltip::color(fl_rgb_color(0x44, 0x44, 0x48));
        Fl_Tooltip::textcolor(fl_rgb_color(0xe6, 0xe6, 0xe6));
    } else if (strcmp(name, "grace") == 0) {
        Fl::background(0xe5, 0xe5, 0xe5);
        Fl::background2(0xcf, 0xe7, 0xe7);
        Fl::foreground(0x00, 0x00, 0x00);
        Fl::set_color(FL_SELECTION_COLOR, 0x00, 0x00, 0x80);
        Fl::set_color(FL_INACTIVE_COLOR, 0x80, 0x80, 0x80);
        Fl_Tooltip::color(fl_rgb_color(0xff, 0xff, 0xe0));
        Fl_Tooltip::textcolor(FL_BLACK);
    } else {
        Fl::background(0xf0, 0xf0, 0xf0);
        Fl::background2(0xff, 0xff, 0xff);
        Fl::foreground(0x00, 0x00, 0x00);
        Fl::set_color(FL_SELECTION_COLOR, 0x30, 0x60, 0xc0);
        Fl::set_color(FL_INACTIVE_COLOR, 0x80, 0x80, 0x80);
        Fl_Tooltip::color(fl_rgb_color(0xff, 0xff, 0xe0));
        Fl_Tooltip::textcolor(FL_BLACK);
    }
}

static void apply_font(const char *family)
{
    static std::string names[4];

    if (family == NULL || family[0] == '\0') {
        /* FLTK defaults */
        Fl::set_font(FL_HELVETICA, "sans");
        Fl::set_font(FL_HELVETICA_BOLD, "sans bold");
        Fl::set_font(FL_HELVETICA_ITALIC, "sans italic");
        Fl::set_font(FL_HELVETICA_BOLD_ITALIC, "sans bold italic");
        return;
    }
    names[0] = std::string(" ") + family;
    names[1] = std::string("B") + family;
    names[2] = std::string("I") + family;
    names[3] = std::string("P") + family;
    Fl::set_font(FL_HELVETICA, names[0].c_str());
    Fl::set_font(FL_HELVETICA_BOLD, names[1].c_str());
    Fl::set_font(FL_HELVETICA_ITALIC, names[2].c_str());
    Fl::set_font(FL_HELVETICA_BOLD_ITALIC, names[3].c_str());
}

static void redraw_all_windows(void)
{
    Fl_Window *w;
    for (w = Fl::first_window(); w; w = Fl::next_window(w)) {
        w->redraw();
    }
}

void fl_apply_gui_prefs(void)
{
    int i;

    load_gui_prefs();
    Fl::scheme(gui_prefs.scheme);
    apply_palette(gui_prefs.palette);
    apply_font(gui_prefs.font);
    if (gui_prefs.scale > 0.0) {
        for (i = 0; i < Fl::screen_count(); i++) {
            Fl::screen_scale(i, (float) gui_prefs.scale);
        }
    }
    Fl::option(Fl::OPTION_SHOW_SCALING, false);
}

static Fl_Double_Window *gprefs_win = NULL;
static Fl_Choice *gprefs_scheme, *gprefs_palette;
static Fl_Hold_Browser *gprefs_font;
static Fl_Box *gprefs_scale_lab;
static Fl_Check_Button *gprefs_keep_scale;

static void gprefs_update_scale_label(void)
{
    char buf[64];
    float f = Fl::screen_scale(fl_main_window() ? fl_main_window()->screen_num():0);
    sprintf(buf, "Current GUI size: %d%%", (int) (f*100 + 0.5));
    gprefs_scale_lab->copy_label(buf);
}

static void gprefs_apply_cb(Fl_Widget *, void *)
{
    int i = gprefs_scheme->value();
    if (i >= 0 && i < N_SCHEMES) {
        strcpy(gui_prefs.scheme, schemes[i]);
    }
    i = gprefs_palette->value();
    if (i >= 0 && i < N_PALETTES) {
        strcpy(gui_prefs.palette, palettes[i]);
    }
    i = gprefs_font->value();
    if (i == 1) {
        gui_prefs.font[0] = '\0';
    } else if (i > 1) {
        strncpy(gui_prefs.font, gprefs_font->text(i), sizeof(gui_prefs.font) - 1);
    }
    if (gprefs_keep_scale->value()) {
        gui_prefs.scale = Fl::screen_scale(
            fl_main_window() ? fl_main_window()->screen_num():0);
    } else {
        gui_prefs.scale = 0.0;
    }

    Fl::scheme(gui_prefs.scheme);
    apply_palette(gui_prefs.palette);
    apply_font(gui_prefs.font);
    Fl::reload_scheme();
    redraw_all_windows();
    save_gui_prefs();
    gprefs_update_scale_label();
}

static void gprefs_show_cb(Fl_Widget *w, void *)
{
    gprefs_update_scale_label();
}

extern "C" void create_gui_prefs_frame(void *data)
{
    int i;

    load_gui_prefs();

    if (gprefs_win == NULL) {
        int W = 420, H = 440, nfonts;
        Fl_Flex *col, *row;
        Fl_Box *lab;

        gprefs_win = dialog_window(W, H, "Grace: GUI appearance");
        col = new Fl_Flex(0, 0, W, H, Fl_Flex::VERTICAL);
        col->margin(8);
        col->gap(6);

        row = new Fl_Flex(0, 0, W, DLG_ROW, Fl_Flex::HORIZONTAL);
        lab = new Fl_Box(0, 0, 110, DLG_ROW, "Widget style:");
        lab->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        row->fixed(lab, 110);
        gprefs_scheme = new Fl_Choice(0, 0, 10, DLG_ROW);
        for (i = 0; i < N_SCHEMES; i++) {
            gprefs_scheme->add(scheme_labels[i]);
        }
        row->end();
        col->fixed(row, DLG_ROW);

        row = new Fl_Flex(0, 0, W, DLG_ROW, Fl_Flex::HORIZONTAL);
        lab = new Fl_Box(0, 0, 110, DLG_ROW, "Colors:");
        lab->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        row->fixed(lab, 110);
        gprefs_palette = new Fl_Choice(0, 0, 10, DLG_ROW);
        for (i = 0; i < N_PALETTES; i++) {
            gprefs_palette->add(palette_labels[i]);
        }
        row->end();
        col->fixed(row, DLG_ROW);

        lab = new Fl_Box(0, 0, W, DLG_ROW, "Font:");
        lab->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        col->fixed(lab, DLG_ROW);

        gprefs_font = new Fl_Hold_Browser(0, 0, W, 100);
        gprefs_font->add("(default)");
        nfonts = Fl::set_fonts(NULL);
        for (i = 0; i < nfonts; i++) {
            int attr;
            const char *name = Fl::get_font_name((Fl_Font) i, &attr);
            if (name && attr == 0 && i >= FL_FREE_FONT) {
                /* skip names the list can't display with the GUI font */
                const char *p;
                for (p = name; *p && (unsigned char) *p < 0x80; p++) {
                    ;
                }
                if (*p == '\0' && name[0] != '@') {
                    gprefs_font->add(name);
                }
            }
        }

        row = new Fl_Flex(0, 0, W, DLG_ROW, Fl_Flex::HORIZONTAL);
        gprefs_scale_lab = new Fl_Box(0, 0, 10, DLG_ROW);
        gprefs_scale_lab->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        gprefs_keep_scale = new Fl_Check_Button(0, 0, 170, DLG_ROW,
            "Remember GUI size");
        gprefs_keep_scale->tooltip("Change the size with Ctrl+ and Ctrl-");
        row->fixed(gprefs_keep_scale, 170);
        row->end();
        col->fixed(row, DLG_ROW);

        row = new Fl_Flex(0, 0, W, DLG_ROW, Fl_Flex::HORIZONTAL);
        row->gap(4);
        new Fl_Box(0, 0, 10, DLG_ROW);
        Fl_Button *b = new Fl_Button(0, 0, 80, DLG_ROW, "Apply");
        b->callback(gprefs_apply_cb);
        row->fixed(b, 80);
        b = new Fl_Button(0, 0, 80, DLG_ROW, "Close");
        b->callback(hide_window_cb, gprefs_win);
        row->fixed(b, 80);
        row->end();
        col->fixed(row, DLG_ROW);

        col->end();
        gprefs_win->resizable(col);
        gprefs_win->end();
        gprefs_win->callback(hide_window_cb, gprefs_win);
        (void) gprefs_show_cb;
    }

    for (i = 0; i < N_SCHEMES; i++) {
        if (strcmp(gui_prefs.scheme, schemes[i]) == 0) {
            gprefs_scheme->value(i);
        }
    }
    for (i = 0; i < N_PALETTES; i++) {
        if (strcmp(gui_prefs.palette, palettes[i]) == 0) {
            gprefs_palette->value(i);
        }
    }
    gprefs_font->value(1);
    for (i = 2; i <= gprefs_font->size(); i++) {
        if (strcmp(gprefs_font->text(i), gui_prefs.font) == 0) {
            gprefs_font->value(i);
            gprefs_font->middleline(i);
            break;
        }
    }
    gprefs_keep_scale->value(gui_prefs.scale > 0.0);
    gprefs_update_scale_label();

    raise_window(gprefs_win);
}
