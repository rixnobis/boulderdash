#!/bin/bash
# Boot boulderdash.ps-exe headless and grab a frame.
#
# Two things this script refuses to get wrong, both learned the expensive way:
#
#  * A valid PNG is not evidence. The still endpoint serves an all-background
#    frame with the same HTTP status and the same `file` output as a rendered
#    one, so grading on "curl exited 0 and the file is a PNG" passes a run that
#    grabbed before the guest drew anything. The histogram is the discriminator:
#    a real frame has many colors, a too-early frame has one.
#  * Teardown walks the PID tree. `pkill -f pcsx-redux` self-matches the shell
#    running it and also kills every other worktree's emulator, including other
#    people's sessions.
#
# Usage: ./shoot.sh [out.png] [port] [seconds-before-grab]

set -u

OUT=${1:-/tmp/boulderdash.png}
PORT=${2:-8171}
SETTLE=${3:-6}

HERE=$(cd "$(dirname "$0")" && pwd)
EXE="$HERE/boulderdash.ps-exe"
REDUX=${REDUX:-/home/pixel/sources/pcsx-redux-wt/tetris-bg/pcsx-redux}

if [ ! -f "$EXE" ]; then echo "no $EXE - build first"; exit 1; fi
if [ ! -x "$REDUX" ]; then echo "no emulator at $REDUX"; exit 1; fi

echo "exe:    $EXE ($(stat -c %s "$EXE") bytes, $(date -u -d @$(stat -c %Y "$EXE") -Iseconds))"
rm -f "$OUT"

xvfb-run -a "$REDUX" -run -stdout -webserver -webserver-port "$PORT" -interpreter \
    -loadexe "$EXE" > /tmp/boulderdash-run.log 2>&1 &
LAUNCH_PID=$!

sleep "$SETTLE"
curl -s -m 15 "http://localhost:$PORT/api/v1/screen/still" -o "$OUT"
echo "curl=$?"

for p in $(pgrep -P $LAUNCH_PID 2>/dev/null); do
    for c in $(pgrep -P "$p" 2>/dev/null); do kill -TERM "$c" 2>/dev/null; done
    kill -TERM "$p" 2>/dev/null
done
kill -TERM $LAUNCH_PID 2>/dev/null
wait $LAUNCH_PID 2>/dev/null

if [ ! -s "$OUT" ]; then
    echo "FAIL: no image came back"
    tail -20 /tmp/boulderdash-run.log
    exit 1
fi

echo "file:   $(file -b "$OUT")"
COLORS=$(convert "$OUT" -format %k info:- 2>/dev/null)
echo "colors: $COLORS"
convert "$OUT" -format %c histogram:info:- 2>/dev/null | sort -rn | head -8

if [ "${COLORS:-1}" -lt 4 ]; then
    echo "FAIL: ${COLORS} distinct colors - grabbed before the guest drew, or it never drew"
    tail -20 /tmp/boulderdash-run.log
    exit 1
fi
echo "OK"
