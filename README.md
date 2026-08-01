# BOULDER DASH

A Boulder Dash for the PlayStation, built on PSYQo.

The 1984 rule set, implemented from documented behaviour and the C64
disassembly: row-major top-down scan, the moved-this-scan mark carried in the
low bit of the element code, left-before-right rolling, butterflies that pay out
in diamonds and fireflies that pay out in nothing, a magic wall on one global
timer, and an amoeba whose fate is a property of the colony rather than of any
cell. Ten caves. The caves are original - the format is reproduced, the 1984
cave data is not.

## Building

    git clone --recursive <this repo>
    make

Needs `mipsel-none-elf-gcc` and the nugget submodule (pulled by `--recursive`;
`git submodule update --init` if you forgot). Produces `boulderdash.ps-exe`.

    make DEMO=3            # boot straight into cave 3 instead of the title
    make DEMO=3 DEATH=1    # ...and drop a rock on the player, to see him die

The demo builds exist because START cannot be pressed in a headless emulator,
which for a while made the title screen the only photographable part of the game.

## Playing

Pad moves. X grabs without moving - it digs, collects and pushes in the given
direction without vacating your cell. Collect the quota, then find the exit;
diamonds past the quota are worth more than diamonds before it, and leftover
time is worth points, so there is a reason to keep digging and a reason to
hurry. A resting boulder on your head is harmless. A falling one is not.

## Verifying

    ./verify.sh

Five gates in about twenty seconds:

1. **Host rules** - scan-order fixtures, amoeba, magic wall, cave validity, and
   a greedy planner that plays every cave start to finish. It completes 9 of 10.
2. **Build** - game, self-test and sound-test, from clean objects.
3. **Differential** - an R3000 replays the same input tape as the host and must
   reproduce a per-tick hash of the whole simulation, then plays a winning run
   of cave 1 through to the exit.
4. **Audio** - the SPU's per-voice envelope is read back to confirm each effect
   actually goes live, with an unstarted voice as the control.
5. **Render** - the game boots and draws real content, graded on the colour
   histogram because a valid PNG of a blank frame looks identical to a good one
   by every other measure.

Sound is synthesised and ADPCM-encoded at boot rather than shipped as samples,
which is what makes the whole audio path checkable on the host.

The four cave-design gates in gate 1 were each written after a cave slipped past
its predecessors, so the harness is a fossil record: exit reachable, supply
reachable after the cave settles, quota-reached-but-exit-not (always a cave bug,
never a planner limit), and player survives 300 idle ticks.
