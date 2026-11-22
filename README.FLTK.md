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
* Native file chooser (GTK's when available, FLTK's own otherwise).
* Help opens in the desktop's default browser unless
  `GRACE_HELPVIEWER` is set.

## Status of the port

Working:

* main window: menu bar, locator bar, tool bar, status bar (all
  toggleable), fixed and free (`-free`) page layouts;
* canvas: rendering of all plot elements (Type 1 fonts with
  antialiasing, fill patterns, line styles, arcs), focus markers,
  rubber-band lines/boxes, crosshair, all mouse actions of the
  original (zoom, zoom X/Y, viewport, move/copy/delete objects, legend
  and timestamp placement, point tracking, regions, ...), double-click
  shortcuts, canvas keyboard shortcuts (Ctrl+A, Ctrl+Z, ...);
* tool bar: draw, zoom, autoscale, expand/shrink, scrolling, autoticks,
  autoscale on set, zoom/autoscale along X/Y, world stack;
* File: new, open, save, save as, revert, print (with the current
  device settings), exit;
* Data → Import → ASCII (single set or NXY, set type, source,
  autoscale);
* Window → Commands (full command language with history: add,
  delete, replace, reorder, read/save, replay);
* Window → Console, error and yes/no dialogs;
* Help menu, examples, About;
* real-time input (`-pipe`, `-npipe`), hot-link refresh (Ctrl+U).

Tested so far (Linux, X11): rendering of the examples, rubber-band
zoom, tool bar, menus, double-click on an axis, the Commands window,
`-npipe` input, GUI scaling, the GUI appearance dialog, free page
layout, batch mode without a display, `configure && make && make
install` from a clean tree. The other canvas actions use the original
event code unchanged but have not been exercised systematically yet;
printing was not tested (the default print command sends the job to
`lp`).

Not ported yet (they show a "not yet ported" notice; the same
operations are available through the command language in Window →
Commands, or through parameter files):

* Print/Page setup and the per-device option dialogs;
* the Edit, Data (except Import ASCII) and Plot dialogs: data sets,
  set operations, transformations, fits, feature extraction, plot /
  graph / set / axis appearance, preferences, regions, hot links,
  locator properties, parameter load/save;
* point explorer, drawing objects, font tool, spreadsheet editor.

## How the code is organised

The core of Grace is unchanged. The interface between the core and a
GUI turned out to be small (about 15 functions such as `update_all()`,
`errwin()`, `yesnowin()`, `set_title()`), plus the "X11" screen
driver.

New files, all in `src/`:

| file | replaces | contents |
|---|---|---|
| `fl_mainwin.cpp` | `xmgrace.c`, part of `xutil.c` | main window, menus, tool bar, cursors, real-time input |
| `fl_drv.cpp` | `x11drv.c`, part of `xutil.c` | canvas widget, screen driver, XOR-style overlays |
| `fl_events.c` | `events.c` | canvas event handling (same logic, toolkit-neutral input) |
| `fl_dialogs.cpp` | `monwin.c`, `comwin.c`, parts of `fileswin.c`, `helpwin.c`, `motifutils.c` | the dialogs ported so far, GUI appearance |
| `fl_stubs.c` | the remaining `*win.c` | placeholders for dialogs not yet ported |
| `fl_gui.h`, `fl_widgets.h`, `fl_files.h` | | internal headers |

Changes to existing files are limited to the build system
(`ac-tools/configure.in`, `ac-tools/Make.conf.in`,
`ac-tools/config.h.in`, `src/Make.common`, `src/Makefile`, the new
`fltkbuild/Makefile`) and to a few `#ifdef`s that keep Motif-only
declarations out of the FLTK build (`xprotos.h`, `x11drv.h`,
`events.h`, `buildinfo.c`, and the device setup dialogs in `psdrv.c`,
`pdfdrv.c`, `rstdrv.c`). The Motif GUI is still selectable with
`--with-gui=motif`.

The screen device keeps the name "X11" so that parameter files and
scripts work unchanged.

## Next steps

Most Motif dialogs are built through the helper layer in
`motifutils.c` (`CreateToggleButton`, `CreateSpinChoice`,
`CreateSetChoice`, `CreateAACDialog`, ...): about 1900 of the 2300
widget calls in the dialog files go through it. Re-implementing that
helper API on top of FLTK will let most `*win.c` dialogs (and the
device option dialogs in the drivers) be brought over with small,
mechanical changes; the spreadsheet editor needs an `Fl_Table`-based
replacement for Xbae.

Possible later improvements: antialiased lines on the canvas (build
FLTK with Cairo, which needs cairo development files), and extended
colour handling (colour maps, transparency for PDF/SVG output) as an
opt-in extension of the project file format.
