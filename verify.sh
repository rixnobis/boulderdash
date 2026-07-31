#!/bin/bash
# Four gates, in dependency order:
#
#   1. host rules      - scan-order fixtures, amoeba, magic wall, cave validity
#   2. build           - game and selftest, from clean objects
#   3. differential    - an R3000 replays the same tape and must reproduce
#                        every per-tick hash the host produced
#   4. render          - the game boots and draws real content
#
# Gate 4 grades on the colour histogram. `curl` exits 0 while writing a 0x0
# file, and the still endpoint serves a valid PNG of an entirely blank frame
# with the same HTTP status as a rendered one, so neither exit status nor
# `file` output can tell a real frame from a grab that landed too early.
#
# Objects are deleted between the host and console builds on purpose: cave.cpp
# is compiled by two different toolchains into the same directory, and make
# tracks source timestamps rather than which compiler produced the object.

set -u
cd "$(dirname "$0")" || exit 1

REDUX=${REDUX:-/home/pixel/sources/pcsx-redux-wt/tetris-bg/pcsx-redux}
BIOS=${BIOS:-/home/pixel/sources/pcsx-redux/src/mips/openbios/openbios.bin}
FAIL=0
step() { echo; echo "=== $* ==="; }

step "1/4 host rules"
g++ -std=c++20 -O2 -Wall -o simtest simtest.cpp cave.cpp levels.cpp || { echo "host build FAILED"; exit 1; }
./simtest || FAIL=1
./simtest --emit trace.inc || FAIL=1

step "2/4 build"
rm -f ./*.o ./*.dep
make -j"$(nproc)" >/dev/null 2>&1 || { echo "game build FAILED"; exit 1; }
rm -f ./*.o ./*.dep
make -f Makefile.selftest -j"$(nproc)" >/dev/null 2>&1 || { echo "selftest build FAILED"; exit 1; }
rm -f ./*.o ./*.dep
make -j"$(nproc)" >/dev/null 2>&1 || { echo "game rebuild FAILED"; exit 1; }
for f in boulderdash.ps-exe boulderdash-selftest.ps-exe; do
    echo "  $f  $(stat -c %s "$f") bytes  $(date -u -d @"$(stat -c %Y "$f")" -Iseconds)"
done

step "3/4 host vs R3000 differential"
timeout 90 "$REDUX" -no-ui -run -stdout -testmode -interpreter -bios "$BIOS" \
    -loadexe boulderdash-selftest.ps-exe 2>&1 | grep -aE "BOULDERDASH|FAIL|ticks simulated|matches"
SELFTEST=${PIPESTATUS[0]}
echo "  exit=$SELFTEST"
[ "$SELFTEST" -eq 0 ] || FAIL=1

step "4/4 render"
xvfb-run -a "$REDUX" -run -stdout -webserver -webserver-port 8299 -interpreter \
    -bios "$BIOS" -loadexe "$PWD/boulderdash.ps-exe" > /tmp/bd-verify.log 2>&1 &
LAUNCH=$!
sleep 7
curl -s -m 15 "http://localhost:8299/api/v1/screen/still" -o /tmp/bd-verify.png
# Walk the PID tree. A `pkill -f pcsx-redux` would self-match the shell running
# it and also kill every other worktree's emulator, including other people's.
for p in $(pgrep -P $LAUNCH 2>/dev/null); do
    for c in $(pgrep -P "$p" 2>/dev/null); do kill -TERM "$c" 2>/dev/null; done
    kill -TERM "$p" 2>/dev/null
done
kill -TERM $LAUNCH 2>/dev/null
wait $LAUNCH 2>/dev/null
DIMS=$(identify -format "%wx%h" /tmp/bd-verify.png 2>/dev/null)
COLORS=$(convert /tmp/bd-verify.png -format %k info:- 2>/dev/null)
echo "  frame: ${DIMS:-none}, ${COLORS:-0} colors"
if [ "${DIMS:-}" != "320x239" ] || [ "${COLORS:-0}" -lt 6 ]; then
    echo "  render FAILED (want 320x239 and >= 6 colors)"
    FAIL=1
fi

echo
[ "$FAIL" -eq 0 ] && { echo "VERIFY: all gates green"; exit 0; }
echo "VERIFY: FAILED"; exit 1
