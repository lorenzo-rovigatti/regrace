#!/bin/sh
#
# Batch-mode smoke test: render every example project with gracebat to
# several output devices. Fails if gracebat exits with an error, writes
# anything to stderr or produces an empty file. Needs no display.
#
# usage: tests/batch.sh [outdir]
#
# GRACE_TEST_DEVICES overrides the devices (default: PostScript EPS SVG PNG).
#

TOP=`cd "\`dirname "$0"\`/.." && pwd`
OUT=${1:-"$TOP/tests/out/batch"}
DEVICES=${GRACE_TEST_DEVICES:-"PostScript EPS SVG PNG"}

GRACE_HOME=$TOP
export GRACE_HOME
unset DISPLAY

for prog in xmgrace grace; do
    if test -x "$TOP/src/$prog"; then
        EXE=$TOP/src/$prog
        break
    fi
done
if test -z "$EXE"; then
    echo "no Grace executable in $TOP/src; build it first" >&2
    exit 2
fi

rm -rf "$OUT"
mkdir -p "$OUT"
# the executable runs in batch mode when called as gracebat
ln -s "$EXE" "$OUT/gracebat"
"$OUT/gracebat" -version

failed=0
total=0
for agr in "$TOP"/examples/*.agr; do
    name=`basename "$agr" .agr`
    for dev in $DEVICES; do
        total=`expr $total + 1`
        file=$OUT/$name.`echo $dev | tr 'A-Z' 'a-z'`
        # run from the examples directory: some projects read data files
        (cd "$TOP/examples" && "$OUT/gracebat" -nosafe -noask \
            -hdevice "$dev" -printfile "$file" "$agr") 2> "$OUT/stderr" \
            > /dev/null
        rc=$?
        if test $rc -ne 0 || test -s "$OUT/stderr" || test ! -s "$file"; then
            echo "FAIL: $name -> $dev (exit status $rc)"
            sed 's/^/    /' "$OUT/stderr"
            failed=`expr $failed + 1`
        fi
    done
done
rm -f "$OUT/stderr" "$OUT/gracebat"

echo "batch: $total renderings, $failed failed"
test $failed -eq 0
