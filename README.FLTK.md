# Grace with an FLTK user interface

This tree contains a port of the Grace GUI (xmgrace) from Motif to
[FLTK](https://www.fltk.org) 1.4. The aim is a faithful port: same
menus, same tool bar, same canvas interaction, same project files and
command language, with a few modern extras (scalable GUI, themes,
choice of GUI font).

FLTK is **bundled** (`fltk/`, version 1.4.5) and linked statically, so
building needs nothing beyond what any Linux machine with X11 client
libraries already has. This makes it suitable for HPC clusters where
Motif/Qt/GTK development packages are usually missing.

## Building

    ./configure [options]
    make
    make install

Relevant `configure` options:

| option | meaning |
|---|---|
| `--with-gui=fltk` | FLTK GUI (default) |
| `--with-gui=motif` | the original Motif GUI |
| `--with-gui=none` | command line / batch only |
| `--without-bundled-fltk` / `--with-fltk-config=PROG` | use an installed FLTK >= 1.4 instead of the bundled one |

Requirements for the FLTK GUI: a C++ compiler and the X11 client
libraries and headers (libX11, libXft, fontconfig, libXrender,
libXcursor, libXfixes, libXinerama). The bundled FLTK is configured
automatically by Grace's `configure` (X11 only, no OpenGL, no Wayland)
and only `libfltk.a` is built; nothing of FLTK gets installed.

`src/xmgrace` can be run directly from the build tree, from any
directory, and an installed Grace home (`<prefix>/grace`) can be moved
or copied elsewhere: Grace looks for its fonts, templates and docs in
`$GRACE_HOME` if set, else in the parent of the executable's directory,
else in the home given at configure time. `xmgrace -version` shows the
one in use.

The resulting `xmgrace` depends only on standard X11/system shared
libraries (check with `ldd`), so a binary built on one node usually
runs on the others. `gracebat` (batch mode) does not need a display.

## What is new in the GUI

* **GUI size**: Ctrl+`+` / Ctrl+`-` / Ctrl+`0`, or View → Larger /
  Smaller / Normal GUI size. Everything (fonts, buttons, dialogs) is
  scaled; the plot itself keeps its full device resolution.
* **View → GUI appearance...**: widget style (Classic, GTK+, Gleam,
  Oxy, Plastic), colours (Light, Grace/Motif colours, Dark), GUI font
  (any font known to fontconfig), and whether to remember the GUI
  size. Settings are stored in `~/.config/grace/xmgrace.prefs`.
* The file selection boxes have a "Browse..." button opening the
  desktop's file chooser (GTK's when available, FLTK's own otherwise).
* Help opens in the desktop's default browser unless
  `GRACE_HELPVIEWER` is set; F1 gives help on the focused dialog.

## Undo and redo

Edit → Undo (Ctrl+Z) and Redo (Ctrl+Shift+Z or Ctrl+Y) go back and
forth through the changes made to the project: data, appearance, axes, drawing objects, regions, page
settings, i.e. everything a project file stores. Each user action
(a dialog's Apply/Accept, a menu entry, a canvas operation, a command
typed in Window → Commands) is one step. Preferences, print settings
and other settings not saved in projects are not affected. Opening or
creating a project starts a new history.

How it works (`src/undo.c`, independent of the toolkit): after each
user action the state is recorded as the settings text that a project
file would contain, plus the data columns in memory at full precision.
Columns that did not change are shared with the previous step, so
appearance changes cost a few kB even with millions of points; with a
10^6-point set, recording a step takes about 2 ms and undoing it less
than 10 ms. Undoing restores a step the way a project is loaded. The
history keeps up to 100 steps and 256 MB.

Because Ctrl+Z is Undo, the canvas shortcut to start a zoom, Ctrl+Z
in the Motif interface, is Ctrl+Alt+Z. In text fields of dialogs
Ctrl+Z still undoes the typing.

A change made through the command language now also marks the project
as modified (before, Grace did not warn about unsaved changes made with
commands).

## Status of the port

All the dialogs of the Motif GUI are available. They are not
re-implemented: the original dialog sources (`compwin.c`, `setwin.c`,
`setappwin.c`, `tickwin.c`, `strwin.c`, `fileswin.c`, `editpwin.c`, ...)
are compiled unchanged for both toolkits, on top of a toolkit-neutral
widget API (see below). Only the main window, the canvas, the console,
the commands window and the About box have separate FLTK versions.

Tested so far (Linux, X11):

* rendering of the examples, rubber-band zoom, tool bar, menus,
  double-click on an axis, GUI scaling and the GUI appearance dialog,
  free page layout, `-npipe` input, batch mode without a display;
* every dialog of the menus opens and lays out correctly (all 47 at
  once, without errors);
* Data sets (statistics table), the spreadsheet editor (opening it by
  double-clicking a set, editing a cell), the Commands window;
* `configure && make && make install` from a clean tree, and the Motif
  build (`--with-gui=motif`) from the same sources.

Not tested systematically yet: the actual computations behind each
dialog (they are the original code), printing (the default print
command sends the job to `lp`), netCDF import (built only with
`--enable-netcdf`).

Differences from the Motif GUI:

* the file selection box keeps the Motif layout (filter, file list,
  selection, the dialog's own options) and adds a "Browse..." button
  that opens the desktop's file chooser;
* the data source of Data/Import/ASCII is an option menu instead of a
  pair of radio buttons (in both GUIs);
* help opens in the desktop's default browser unless `GRACE_HELPVIEWER`
  is set; F1 shows the help for the dialog under the keyboard focus.

## How the code is organised

The core of Grace is unchanged. The interface between the core and a
GUI turned out to be small (about 15 functions such as `update_all()`,
`errwin()`, `yesnowin()`, `set_title()`), plus the "X11" screen
driver.

The dialogs are written against the widget API of `motifinc.h`
(`CreateDialogForm`, `CreateFrame`, `CreateToggleButton`,
`CreateOptionChoice`, `CreateSetChoice`, `CreateAACDialog`, ...):

| file | contents |
|---|---|
| `motifutils.c` | the API on Motif (as in Grace 5.1.25) |
| `fl_motifutils.cpp` | the API on FLTK: a tree of widgets mirroring the Motif hierarchy, laid out automatically from the natural sizes of the widgets, as Motif does |
| `guiutils.c` | the parts that only use the API (set/graph selectors and their popup menus, color/font/format choices, ...), shared by both |

A few helpers were added to the API so that no dialog calls Xm/Xt/Xbae
directly any more: `IsManaged`, `IsSensitive`,
`CreateScrolledVContainer`, `SetTextCursorPos`, `GetTextItemString`,
`CreateDialogShell`, `CreateDialogRC`, `CreateColumnGrid`,
`AddDialogFormBottomChild`, simple string tables (`CreateTable`, ...)
and callback-driven spreadsheets (`CreateSpreadSheet`, ...).

FLTK-specific files, all in `src/`:

| file | replaces | contents |
|---|---|---|
| `fl_mainwin.cpp` | `xmgrace.c`, part of `xutil.c` | main window, menus, tool bar, cursors, real-time input |
| `fl_drv.cpp` | `x11drv.c`, part of `xutil.c` | canvas widget, screen driver, XOR-style overlays |
| `fl_events.c` | `events.c` | canvas event handling (same logic, toolkit-neutral input) |
| `fl_motifutils.cpp` | `motifutils.c` | the widget API |
| `fl_fontwin.cpp` | `fontwin.c` | font tool (glyph table) |
| `fl_dialogs.cpp` | `monwin.c`, `comwin.c`, `helpwin.c` | console, commands, about/help, yes/no, GUI appearance |
| `fl_gui.h`, `fl_widgets.h`, `fl_files.h` | | internal headers |

Changes to the core are limited to the build system and to a few
`#ifdef`s keeping Motif-only declarations out of the FLTK build. The
Motif GUI is still selectable with `--with-gui=motif`.

The screen device keeps the name "X11" so that parameter files and
scripts work unchanged.

For automated tests, `GRACE_FLTK_OPEN="Plot/Axis properties...;Edit/Data sets..."`
opens the given menu entries at startup.

## Next steps

* antialiased lines on the canvas (FLTK can draw through Cairo when it
  is available at build time);
* extended colour handling (colour maps, transparency for PDF/SVG
  output) as an opt-in extension of the project file format;
* a pass over each dialog to check the computations end to end.
