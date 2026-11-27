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

The shortcuts also work while a dialog has the keyboard focus, except
in text fields, where Ctrl+Z still undoes the typing. Because Ctrl+Z is
Undo, the canvas shortcut to start a zoom, Ctrl+Z in the Motif
interface, is Ctrl+Alt+Z.

A change made through the command language now also marks the project
as modified (before, Grace did not warn about unsaved changes made with
commands).

## Colours

Two dialogs in the Plot menu, available in both GUIs
(`src/colorwin.c`, with the logic in `src/colormaps.c`):

* **Plot → Colors...**: edits the colour table. Pick a colour, change
  its name and its red/green/blue values (the swatch previews the
  result) and Apply, or add the result as a new colour. Changing
  colour 0 (background) or 1 (foreground) asks first. A **palette**
  replaces the colours from 2 onwards with a coherent set: Grace (the
  original colours), Okabe-Ito, Tol bright and Tol muted (all three
  safe for colour-blind readers) and Tableau 10. Colours 0 and 1 are
  kept, so the existing sets simply take the new colours.
* **Plot → Color by value...**: colours the symbols of a set according
  to the values of one of its columns (e.g. the third column of an
  x-y-z file), using a colour map (viridis, magma, inferno, plasma,
  cividis, coolwarm, RdBu, gray, jet), possibly reversed, with 2-128
  colours, over a linear or logarithmic range (by default the range of
  the data). The result is a copy of the set, of type XYCOLOR, with
  filled symbols and no line, optionally hiding the original and
  adding a colour bar: a narrow graph at the right of the plot with
  the map and the values on its axis. Applying it again replaces the
  copy and the colour bar.

Nothing new is stored in project files: the colours sampled from a
map are ordinary entries of the colour table (named like
`viridis_64_07`, reused by later operations), the coloured set is an
XYCOLOR set and the colour bar is a graph with boxes. Projects made
this way open in any Grace 5.1. The table holds at most 256 colours;
if a map does not fit, use fewer colours. Both dialogs are undoable.

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
opens the given menu entries at startup; entries `cmd:...` run commands
of the Grace language (e.g. `cmd:exit`). With it set, unknown menu
entries and the errors shown in the Console are also written to stderr.

## Tests

From the top of a built tree:

    tests/batch.sh                # every example with gracebat to PS, EPS, SVG, PNG
    xvfb-run -a tests/gui.sh      # every example on the canvas, then every dialog

Both fail on a crash, a hang, a non-zero exit status or any error
message, and leave their output in `tests/out/`. `tests/gui.sh` needs a
display (Xvfb on a headless machine). GitHub Actions
(`.github/workflows/ci.yml`) builds with gcc and clang on Linux
(x86-64 and arm64, FLTK, Motif and no GUI), with ASan/UBSan, and on
macOS (Cocoa), and runs both tests.

## Next steps

* antialiased lines on the canvas (FLTK can draw through Cairo when it
  is available at build time);
* transparency for PDF/SVG output, which would need an opt-in
  extension of the project file format;
* a pass over each dialog to check the computations end to end.
