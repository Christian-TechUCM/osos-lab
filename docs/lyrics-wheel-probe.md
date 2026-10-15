# Temporary wheel-input diagnostic

The user confirmed build `e6dab640` is running, but wheel volume still does
not work in lyrics. Playback buttons work, and volume works outside lyrics.
The host fixtures verified a simulated input path, not the device's actual
focus hierarchy. This build collects evidence; it is **not another claimed fix**.

## Scope

On this diagnostic branch, `payload/lyrics_probe.h` enables `WHEEL PROBE 1`.
The ready timed-lyrics panel temporarily displays eight rows of diagnostic
text over a solid navy panel. The track header remains available. Use a song
whose synchronized lyrics already worked. Untimed/stock fallback views do not
show this panel.

The existing routing checks and native volume action are unchanged. Counters
are updated inside the already-installed dispatcher hook and lyric-view event
hook. There are no additional native hook sites, additional native getter
calls, event ownership changes, event pointer retention, raw register access,
or storage writes. Values live in RAM and reset on reboot. Ordinary firmware
settings retain their existing behavior.

The diagnostic refreshes at 250 ms even when paused; it deliberately is not
a scrolling-performance benchmark. It preserves the existing timed-lyrics
scene and input ownership. Small native text is used for the panel, with the
chosen lyric font restored after each draw. Setting the compile-time flag to
zero removes the instrumentation and panel; host tests cover both modes.

## Test later, after a separately agreed installation

Do not enter DFU or use Restore for this diagnostic update. Before a future
copy, recheck the mounted device identity, battery, and sync state; preserve
and checksum the current `osos-cfw.bin`, stage/checksum the candidate, then
replace only that firmware file and safely eject. This document does not
authorize or perform an installation. Keep build `e6dab640` available for
rollback. No companion, bootloader, NOR, partition, or music changes are needed.

1. After installation and reboot, verify the new build and `WHEEL PROBE 1`
   label in Settings → CFW Info. Set a low volume outside lyrics first and
   keep the earbuds out of your ears during the input test.
2. Start a song with working timed lyrics. Enter lyrics, then pause using the
   physical Play/Pause button. Wait a second and photograph the whole screen
   **before turning the wheel**.
3. Turn the wheel clockwise a small amount, wait one second, and photograph
   the screen again. Stay in lyrics; do not change tracks or navigate menus.
4. Turn counterclockwise a small amount, wait one second, and take a third
   photo. Report whether the volume changed or a volume bar appeared.
5. If useful, repeat while playing the same song, reporting any difference.
   After the photos, leave lyrics and verify normal volume and buttons still
   work. Stop testing if unexpected volume jumps or other problems occur.

There is no need to connect to a computer for the photos. Do not reboot between
photos: counters are cumulative since boot. Wheel use outside lyrics also
contributes; compare the before/after photos rather than absolute totals.

## Reading the panel

All values are hexadecimal. Displayed counters wrap at their shown width.
These are **handler visits, not physical detents**: native bubbling can call
the hook repeatedly for a single event.

| Field | Meaning |
| --- | --- |
| C | Total focused-dispatch hook visits, including timers/buttons |
| W | Wheel-type visits reaching that hook |
| V | Wheel visits passing the visible-lyrics check |
| L | Visits passing the ready-lyrics layout check |
| M | Visits with a non-null controller manager |
| N | Visits with the expected Now Playing controller vtable |
| P | Visits whose focus belongs to that controller |
| S / R | Volume action calls sent / returned; return does not prove an audible change |
| G | Last wheel's furthest gate, listed below |
| E | Last lyric-view event type other than wheel, timer (0f), or property (13) |
| Layout | Last wheel's layout value, if the visible check passed |
| VT | Last wheel's controller vtable, if its controller existed |
| VW | Wheel events reaching the lyric TextView itself |

Gate values: 0 = no wheel seen; 1 = wheel; 2 = visible lyrics;
3 = ready layout; 4 = manager exists; 5 = current controller exists;
6 = expected controller type; 7 = focus ancestry passed/action entered;
8 = action returned. `Layout` and `VT` reset on each wheel visit so an early
failure cannot display a stale value from an earlier successful event.

For example, W increasing without V narrows the problem to visibility; V
without L points at the layout check; N without P points at focus ancestry.
W staying unchanged means the existing hook did not see wheel-type events;
it does **not** prove a broken physical wheel or identify which earlier native
filter/path is responsible. S/R increasing without an audible change calls
for investigation inside the existing native action/decoder path.

The firmware's device behavior and text legibility still require this hardware
trial. Passing host tests and binary checks cannot establish them.
