#!/bin/sh
#
# GUI smoke test for the FLTK build: open every example on the canvas,
# then every dialog of the menus at once (through GRACE_FLTK_OPEN, see
# src/fl_mainwin.cpp). Fails if xmgrace crashes, hangs or reports an
# error. Needs a display: on a headless machine run it under Xvfb, e.g.
#
#     xvfb-run -a tests/gui.sh
#
# usage: tests/gui.sh [outdir]
#
# If `import` (ImageMagick, X11) or `screencapture` (macOS) is available,
# a screenshot with all the dialogs open is saved in outdir.
#

TOP=`cd "\`dirname "$0"\`/.." && pwd`
OUT=${1:-"$TOP/tests/out/gui"}
EXE=$TOP/src/xmgrace
# seconds allowed to each run of xmgrace
TIMEOUT=${GRACE_TEST_TIMEOUT:-60}

GRACE_HOME=$TOP
export GRACE_HOME

if test ! -x "$EXE"; then
    echo "$EXE not found; build Grace with the FLTK GUI first" >&2
    exit 2
fi

rm -rf "$OUT"
mkdir -p "$OUT"

# The dialogs of the menus. Not here: entries that need a user's answer
# (File/New, Revert...), that print, quit or start external programs
# (help pages).
DIALOGS="File/Open...;File/Save as...;File/Print setup...
Edit/Data sets...;Edit/Set operations...
Edit/Arrange graphs...;Edit/Overlay graphs...;Edit/Autoscale graphs...
Edit/Regions/Status...;Edit/Regions/Define...;Edit/Regions/Clear...
Edit/Regions/Report on...;Edit/Hot links...;Edit/Locator props...
Edit/Preferences...
Data/Data set operations...
Data/Transformations/Evaluate expression...
Data/Transformations/Histograms...
Data/Transformations/Fourier transforms...
Data/Transformations/Running averages...
Data/Transformations/Differences...
Data/Transformations/Seasonal differences...
Data/Transformations/Integration...
Data/Transformations/Interpolation\/splines...
Data/Transformations/Regression...
Data/Transformations/Non-linear curve fitting...
Data/Transformations/Correlation\/covariance...
Data/Transformations/Digital filter...
Data/Transformations/Linear convolution...
Data/Transformations/Geometric transforms...
Data/Transformations/Sample points...
Data/Transformations/Prune data...
Data/Feature extraction...
Data/Import/ASCII...;Data/Export/ASCII...
Plot/Plot appearance...;Plot/Graph appearance...;Plot/Set appearance...
Plot/Axis properties...;Plot/Colors...;Plot/Color by value...
Plot/Load parameters...;Plot/Save parameters...
View/Page setup...;View/Redraw;View/Update all
View/Larger GUI (Ctrl +);View/Normal GUI size (Ctrl 0);View/GUI appearance...
Window/Commands;Window/Point explorer;Window/Drawing objects
Window/Font tool;Window/Console
Help/About..."
# Data/Transformations/Interpolation/splines: '/' in a label is escaped
DIALOGS=`echo "$DIALOGS" | tr '\n' ';' | sed 's,\\\\/,/,g'`

failed=0

# run xmgrace with GRACE_FLTK_OPEN=$1 on the remaining arguments
run()
{
    steps=$1
    shift
    GRACE_FLTK_OPEN=$steps "$EXE" -noask "$@" > "$OUT/log" 2>&1 &
    pid=$!
    t=0
    while kill -0 $pid 2> /dev/null; do
        if test $t -ge `expr $TIMEOUT \* 10`; then
            echo "    timeout: killing xmgrace"
            kill -9 $pid
            break
        fi
        sleep 0.1 2> /dev/null || sleep 1
        t=`expr $t + 1`
    done
    wait $pid
}

check()
{
    rc=$1
    what=$2
    # "XOpenIM() failed" is harmless: Xvfb has no input method
    grep -v 'XOpenIM() failed' "$OUT/log" > "$OUT/log.f"
    if test $rc -ne 0 || test -s "$OUT/log.f"; then
        echo "FAIL: $what (exit status $rc)"
        sed 's/^/    /' "$OUT/log.f"
        failed=`expr $failed + 1`
    else
        echo "ok: $what"
    fi
}

# every example on the canvas
cd "$TOP/examples"
for agr in *.agr; do
    run "View/Redraw;cmd:exit" "$agr"
    check $? "open $agr"
done

# every dialog, with a project loaded
shot=""
if command -v screencapture > /dev/null 2>&1; then
    shot="screencapture -x"
elif command -v import > /dev/null 2>&1 && test -n "$DISPLAY"; then
    shot="import -window root"
fi
if test -n "$shot"; then
    # keep the windows on the screen for the screenshot
    run "${DIALOGS}cmd:sleep 4;cmd:exit" co2.agr &
    runner=$!
    sleep 3
    $shot "$OUT/dialogs.png" || echo "(screenshot failed)"
    wait $runner
else
    run "${DIALOGS}cmd:exit" co2.agr
fi
check $? "open all the dialogs"

rm -f "$OUT/log" "$OUT/log.f"
echo "gui: $failed failures"
test $failed -eq 0
