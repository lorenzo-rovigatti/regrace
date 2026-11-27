# regrace

[![CI](https://github.com/lorenzo-rovigatti/regrace/actions/workflows/ci.yml/badge.svg)](https://github.com/lorenzo-rovigatti/regrace/actions/workflows/ci.yml)

**regrace is a modified version of [Grace](https://plasma-gate.weizmann.ac.il/Grace/)
5.1.25**, the WYSIWYG 2D plotting tool by Paul J Turner and the Grace
Development Team (maintained by Evgeny Stambulchik). It is a derivative
work, distributed under the same licence as Grace (the GNU GPL, version
2 or later). It is **not** an official Grace release and is not
supported or endorsed by the Grace Development Team: please report
problems with regrace [here](https://github.com/lorenzo-rovigatti/regrace/issues),
not to the Grace authors.

The main change is that the graphical interface (`xmgrace`) runs on
[FLTK](https://www.fltk.org) 1.4 instead of Motif. FLTK is bundled and
linked statically, so regrace builds on any machine with the X11 client
libraries (including HPC clusters where Motif, Qt or GTK development
packages are usually missing) and natively on macOS. The port is meant
to be faithful: same menus, tool bar, dialogs, canvas interaction,
project files and command language. On top of it regrace adds undo and
redo, a colour editor with colour-blind-safe palettes, colouring of
symbols by value with a colour bar, a scalable and themeable GUI, and
fixes for modern compilers.

Project files are unchanged: anything made with regrace opens in Grace
5.1 and vice versa.

The executables and the installed layout keep their Grace names
(`xmgrace`, `gracebat`, `grace`, `$GRACE_HOME`, ...), so existing
scripts, parameter files and habits keep working; `xmgrace -version`
and the About box identify the program as `Grace-5.1.25 (regrace)`.

Contents:
[What is new](#what-is-new-compared-with-grace-5125) ·
[Building](#building) ·
[Running](#running) ·
[Tests](#tests) ·
[Code organisation](#how-the-code-is-organised) ·
[Licensing](#licensing-and-credits)

## What is new compared with Grace 5.1.25

### A GUI without Motif

The whole `xmgrace` interface runs on FLTK (`--with-gui=fltk`, the
default). The original Motif GUI is still available with
`--with-gui=motif`, and a GUI-less build with `--with-gui=none`.

All the dialogs of the Motif GUI are available. They are not
re-implemented: the original dialog sources are compiled unchanged for
both toolkits on top of a toolkit-neutral widget API (see
[below](#how-the-code-is-organised)), so the computations behind them
are Grace's own code.

Extras of the FLTK GUI:

* **GUI size**: Ctrl+`+` / Ctrl+`-` / Ctrl+`0`, or View → Larger /
  Smaller / Normal GUI size. Fonts, buttons and dialogs are scaled; the
  plot keeps its full device resolution.
* **View → GUI appearance...**: widget style (Classic, GTK+, Gleam,
  Oxy, Plastic), colours (Light, Grace/Motif colours, Dark), GUI font
  (any font known to fontconfig), and whether to remember the GUI size.
  Settings are stored in `~/.config/grace/xmgrace.prefs`.
* The file selection boxes keep the Motif layout (filter, file list,
  selection, the dialog's own options) and add a **Browse...** button
  opening the desktop's file chooser (GTK's when available, FLTK's own
  otherwise).
* **Help** opens in the desktop's default browser unless
  `GRACE_HELPVIEWER` is set; **F1** shows the help for the dialog that
  has the keyboard focus.

### Undo and redo

Edit → **Undo** (Ctrl+Z) and **Redo** (Ctrl+Shift+Z or Ctrl+Y) go back
and forth through the changes made to the project: data, appearance,
axes, drawing objects, regions, page settings, i.e. everything a
project file stores. Each user action (a dialog's Apply/Accept, a menu
entry, a canvas operation, a command typed in Window → Commands) is one
step. Preferences, print settings and other settings not saved in
projects are not affected. Opening or creating a project starts a new
history, which keeps up to 100 steps and 256 MB.

The shortcuts also work while a dialog has the keyboard focus, except
in text fields, where Ctrl+Z still undoes the typing.

Undo is cheap even for large data sets: after each action the state is
recorded as the settings text a project file would contain, plus the
data columns at full precision, sharing the columns that did not change
with the previous step. With a 10^6-point set, recording a step takes
about 2 ms and undoing it less than 10 ms.

Undo is available in the FLTK GUI only.

### Colours

Two new dialogs in the Plot menu, available in both the FLTK and the
Motif GUI:

* **Plot → Colors...** edits the colour table. Pick a colour, change
  its name and its red/green/blue values (a swatch previews the result)
  and Apply, or add the result as a new colour. Changing colour 0
  (background) or 1 (foreground) asks first.

  A **palette** replaces colours 2 onwards with a coherent set: Grace
  (the original colours), Okabe-Ito, Tol bright and Tol muted (all
  three safe for colour-blind readers) and Tableau 10. Colours 0 and 1
  are kept, so existing sets simply take the new colours.

* **Plot → Color by value...** colours the symbols of a set according
  to the values of one of its columns (e.g. the third column of an
  x-y-z file):

  1. choose the set and the column holding the values;
  2. choose a colour map (viridis, magma, inferno, plasma, cividis,
     coolwarm, RdBu, gray, jet), possibly reversed, the number of
     colours (2-128) and a linear or logarithmic range (by default the
     range of the data);
  3. Apply. The result is a copy of the set, of type XYCOLOR, with
     filled symbols and no line. Optionally the original set is hidden
     and a **colour bar** is added: a narrow graph at the right of the
     plot showing the map, with the values on its axis.

  Applying again replaces the copy and the colour bar.

Nothing new is stored in project files: the colours sampled from a map
are ordinary entries of the colour table (named like `viridis_64_07`
and reused by later operations), the coloured set is an XYCOLOR set and
the colour bar is a graph with boxes, so these projects open in any
Grace 5.1. The colour table holds at most 256 colours; if a map does
not fit, use fewer colours. Both dialogs can be undone.

### Easier to build, install and move

* **Self-contained build.** FLTK 1.4.5 is bundled (`fltk/`), configured
  by regrace's `configure` and linked statically. The resulting
  `xmgrace` depends only on standard X11/system shared libraries (check
  with `ldd`), so a binary built on one node of a cluster usually runs
  on the others.
* **macOS**: the FLTK GUI builds with the native Cocoa backend.
* **Relocatable.** `src/xmgrace` runs directly from the build tree,
  from any directory, and an installed Grace home (`<prefix>/grace`)
  can be moved or copied elsewhere. Grace looks for its fonts,
  templates and docs in `$GRACE_HOME` if set, else in the parent of the
  executable's directory, else in the home given at configure time;
  `xmgrace -version` shows the one in use. Long paths are now supported
  (the limits were 120-256 characters, now 1024).
* **Building from a git checkout** no longer needs yacc or linuxdoc:
  the distributed parser and manuals are used when the tools to
  regenerate them are missing.

### Fixes

* Builds with recent gcc and clang (implicit declarations, K&R
  definitions, `isnan`/`isfinite` clashes with C++).
* `configure`: the run tests failed silently with recent clang (on
  Apple Silicon the FPU was taken as big endian, so the special
  functions used byte-swapped constants); with user-supplied `CFLAGS`
  every later run test failed; the X11 path and declaration checks
  always failed; the XDR check accepted systems without `rpc/rpc.h`.
* T1lib crashed on every text when built with clang `-O2` (strict
  aliasing); integer overflow in the Type 1 decryption.
* The SVG export crashed on text containing `%`.
* Undefined behaviour in the text rendering found with UBSan.
* Crashes were always reported as SIGSYS.
* Changes made through the command language now mark the project as
  modified, so Grace warns about unsaved changes made with commands.

### Differences in behaviour

* In the FLTK GUI Ctrl+Z is Undo, so the canvas shortcut that starts a
  zoom (Ctrl+Z in the Motif GUI) is **Ctrl+Alt+Z**.
* Data → Import → ASCII chooses the data source with an option menu
  instead of a pair of radio buttons (in both GUIs).
* The version string is `Grace-5.1.25 (regrace)`, and appears as such
  in the "Creator" field of exported files.

## Building

    ./configure [options]
    make
    make install

Relevant `configure` options (`./configure --help` lists all of them):

| option | meaning |
|---|---|
| `--with-gui=fltk` | FLTK GUI (default) |
| `--with-gui=motif` | the original Motif GUI |
| `--with-gui=none` | command line / batch only |
| `--without-bundled-fltk` / `--with-fltk-config=PROG` | use an installed FLTK >= 1.4 instead of the bundled one |
| `--prefix=DIR` | install under `DIR` (the Grace home is `DIR/grace`) |

Requirements for the FLTK GUI on Linux: a C and a C++ compiler and the
X11 client libraries and headers (libX11, libXft, fontconfig,
libXrender, libXcursor, libXfixes, libXinerama); libpng, libjpeg and
zlib are optional (PNG/JPEG export). On Debian/Ubuntu:

    sudo apt-get install build-essential libx11-dev libxft-dev \
        libfontconfig1-dev libxrender-dev libxcursor-dev libxfixes-dev \
        libxinerama-dev libxext-dev libpng-dev libjpeg-dev zlib1g-dev

The bundled FLTK is configured automatically (X11 only, no OpenGL, no
Wayland) and only `libfltk.a` is built; nothing of FLTK gets installed.

On macOS the bundled FLTK uses Cocoa/Quartz and X11 is not needed.
With Homebrew's libpng and libjpeg (for PNG/JPEG export):

    brew install libpng jpeg-turbo
    ./configure --without-x \
        --with-extra-incpath="$(brew --prefix)/include" \
        --with-extra-ldpath="$(brew --prefix)/lib"
    make

The Motif GUI additionally needs the Motif, Xt, Xmu and Xpm development
packages (`libmotif-dev libxmu-dev libxpm-dev libxt-dev`).

## Running

`xmgrace` starts the GUI, `gracebat` renders in batch mode without a
display, and `grace` is the command-line interface, exactly as in
Grace. For example:

    xmgrace data.dat
    xmgrace -nxy data.dat                     # one set per column
    gracebat -hdevice PNG -printfile plot.png project.agr

The command-line options, the project and parameter files, the command
language and the resource files are those of Grace 5.1, documented in
the User's Guide (Help → User's Guide, or `doc/UsersGuide.html`).

Documentation shipped with the sources:

| file | contents |
|---|---|
| `doc/UsersGuide.html` | Grace User's Guide (also PDF) |
| `doc/Tutorial.html` | Grace tutorial (also PDF) |
| `doc/FAQ.html` | frequently asked questions (also PDF) |
| `CHANGES` | history of Grace up to 5.1.25 (regrace's changes are in this file and in the git history) |
| `COPYRIGHT`, `LICENSE` | Grace copyright and licence (GPL) |
| `arch/<os>/README` | notes for VMS, OS/2 and Windows from the original distribution |

## Tests

From the top of a built tree:

    tests/batch.sh                # every example with gracebat to PS, EPS, SVG, PNG
    xvfb-run -a tests/gui.sh      # every example on the canvas, then every dialog

Both fail on a crash, a hang, a non-zero exit status or any error
message, and leave their output in `tests/out/`. `tests/gui.sh` needs a
display (Xvfb on a headless machine).

GitHub Actions (`.github/workflows/ci.yml`) builds with gcc and clang
on Linux (x86-64 and arm64, FLTK, Motif and no GUI, with and without
ASan/UBSan) and on macOS (Cocoa), and runs both tests.

For automated tests, `GRACE_FLTK_OPEN="Plot/Axis properties...;Edit/Data sets..."`
opens the given menu entries at startup; entries `cmd:...` run commands
of the Grace language (e.g. `cmd:exit`). With it set, unknown menu
entries and the errors shown in the Console are also written to stderr.

What is not tested systematically yet: the computations behind each
dialog end to end (they are the original code), printing (the default
print command sends the job to `lp`) and netCDF import (built only with
`--enable-netcdf`).

## How the code is organised

The core of Grace is unchanged. The interface between the core and a
GUI is small (about 15 functions such as `update_all()`, `errwin()`,
`yesnowin()`, `set_title()`), plus the "X11" screen driver; the screen
device keeps that name so that parameter files and scripts work
unchanged.

The dialogs are written against the widget API of `motifinc.h`
(`CreateDialogForm`, `CreateFrame`, `CreateToggleButton`,
`CreateOptionChoice`, `CreateSetChoice`, `CreateAACDialog`, ...):

| file | contents |
|---|---|
| `motifutils.c` | the API on Motif (as in Grace 5.1.25) |
| `fl_motifutils.cpp` | the API on FLTK: a tree of widgets mirroring the Motif hierarchy, laid out automatically from the natural sizes of the widgets, as Motif does |
| `guiutils.c` | the parts that only use the API (set/graph selectors and their popup menus, colour/font/format choices, ...), shared by both |

A few helpers were added to the API so that no dialog calls Xm/Xt/Xbae
directly any more: `IsManaged`, `IsSensitive`,
`CreateScrolledVContainer`, `SetTextCursorPos`, `GetTextItemString`,
`CreateDialogShell`, `CreateDialogRC`, `CreateColumnGrid`,
`AddDialogFormBottomChild`, `AddScaleCB`, `CreateColorSwatch`, simple
string tables (`CreateTable`, ...) and callback-driven spreadsheets
(`CreateSpreadSheet`, ...).

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

New toolkit-independent files: `undo.c` (undo/redo), `colormaps.c`
(palettes, colour maps, colour by value) and `colorwin.c` (the two
colour dialogs).

Changes to the rest of the core are limited to the build system, a few
`#ifdef`s keeping Motif-only declarations out of the FLTK build, the
hooks needed by undo, and the fixes listed above.

### Next steps

* antialiased lines on the canvas (FLTK can draw through Cairo when it
  is available at build time);
* transparency for PDF/SVG output, which would need an opt-in
  extension of the project file format;
* a pass over each dialog to check the computations end to end.

## Licensing and credits

regrace as a whole is distributed under the **GNU General Public
License, version 2 or (at your option) any later version**, the licence
of Grace: see [`COPYRIGHT`](COPYRIGHT) and [`LICENSE`](LICENSE). It
comes with **no warranty**.

    Copyright (c) 1991-1995 Paul J Turner, Portland, OR
    Copyright (c) 1996-2015 Grace Development Team
    Copyright (c) 2026 Lorenzo Rovigatti (regrace modifications)

regrace is based on the Grace 5.1.25 sources as released by the Grace
Development Team (the first commit of this repository is the pristine
5.1.25 tree). As required by the GPL, the source files that were
changed carry a notice saying so in their header, and every change,
with its date, is recorded in the git history (`git log`, or
`git diff 19b0db3` against the pristine tree). Files without a comment
header that were changed (build files: `configure`,
`ac-tools/configure.in`, `ac-tools/aclocal.m4`, `ac-tools/*.in`,
`src/Make.common`, `src/Makefile`, `T1lib/Makefile`, `doc/Makefile`;
and the User's Guide, which documents undo) are covered by this notice
and by the git history. "Grace" is the name of the original program;
its use here only describes where regrace comes from.

regrace contains third-party components, each under its own licence,
which is kept in the corresponding directory:

| component | where | licence | notes |
|---|---|---|---|
| Grace 5.1.25 | everything not listed below | GPL v2 or later (`COPYRIGHT`, `LICENSE`) | modified |
| [FLTK](https://www.fltk.org) 1.4.5 | `fltk/` | LGPL v2 with the FLTK exceptions, which allow static linking (`fltk/COPYING`) | unmodified, without its documentation, tests, examples, FLUID and fltk-options |
| libjpeg, libpng, zlib, nanosvg, libdecor (bundled with FLTK) | `fltk/jpeg`, `fltk/png`, `fltk/zlib`, `fltk/nanosvg`, `fltk/libdecor` | their own licences (README/LICENSE files in those directories) | unmodified; not compiled by regrace, which builds only the core `libfltk.a` |
| T1lib 1.3.1 | `T1lib/` | LGPL (`T1lib/LGPL`, `T1lib/LICENSE`); the Type 1 rasterizer is © IBM and the AFM parser © Adobe, under the permissive notices in the source files | 4 files of the rasterizer modified (longer font paths, integer overflow fix) |
| Cephes Math Library | `cephes/` | GPL v2, by permission of the author (`cephes/COPYING`) | unmodified |
| grace_np library | `grace_np/` | LGPL v2 (`grace_np/LICENSE`) | `gracef_np.c` modified (build fix) |
| Xbae widget | `Xbae/` | permissive Bellcore/Andrew Lister licence (`Xbae/LICENSE`) | used by the Motif GUI only |
| Tab widget | `src/Tab.c` | permissive licence in the file (© Pralay Dakua) | used by the Motif GUI only |
| Raster driver | `src/gd.c`, `src/rstdrv.c` | based on GD 1.3, © Cold Spring Harbor Laboratory and Boutell.Com, Inc. | unmodified |
| URW++ Type 1 fonts | `fonts/type1/` | GPL | unmodified |

regrace is based in part on the work of the FLTK project
(https://www.fltk.org).

The colour maps and palettes in `src/colormaps.c` are sampled from
published schemes: viridis, magma, inferno and plasma by Stéfan van der
Walt and Nathaniel Smith (CC0), cividis by Jamie R. Nuñez, Christopher
R. Anderton and Ryan S. Renslow, coolwarm by Kenneth Moreland, the
colour-blind-safe palette by Masataka Okabe and Kei Ito, the bright and
muted schemes by Paul Tol, and Tableau 10 by Tableau. RdBu: this
product includes color specifications and designs developed by Cynthia
Brewer (http://colorbrewer.org/).

Many thanks to Paul J Turner, Evgeny Stambulchik and the Grace
Development Team for Grace, and to the FLTK developers.
