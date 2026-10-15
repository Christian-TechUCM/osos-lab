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
scene and input ownership. Small native text is used for the panel without
changing the saved lyric-font preference or swapping fonts per frame.
Setting the compile-time flag to
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

## Preparation record — 2026-10-14

Diagnostic source: `b53682f00d23cebe64c1c78277be1872a6032063`, isolated on
`codex/lyrics-wheel-diagnostics`. The regular feature branch is not replaced.
[Firmware run 38664148302](https://github.com/Christian-TechUCM/osos-lab/actions/runs/38664148302)
was dispatched for that exact commit. At the initial preparation handoff it was still running the
pinned ARM toolchain build; there is **no verified diagnostic firmware image
yet**. An earlier run for `6220b00` was superseded and cancelled.

Local checks passed: 45 patching tests (including 18 sanitized C fixtures),
49 tools tests (one skipped), 31 USB-helper tests, Rust workspace tests
(four ignored fixture-dependent tests), four JavaScript tests, Ruff lint and
format checks, and ARM926 Clang compilation of the modified native C files.
The input tests run with diagnostics both on and off. These checks do not
substitute for the pinned GCC production build or hardware testing.

Before any future installation:

1. Confirm the above run succeeded for the exact source commit; inspect the
   build log for compiler/payload-size checks and tests.
2. Download its `firmware` artifact to `.build/ci-firmware-b53682f/`.
3. Assemble only its `classic/osos.json` / `osos.data` recipe against the
   already-verified local inputs into
   `.build/dfu-prep/verified-osos-b53682f.bin`. Do not prepare or write a new
   companion, boot flash, or device image through USB.
4. Run the prepared private read-only audit
   `.build/verify_wheel_probe_image.py`. It verifies the unchanged preimage
   set, asset hashes, image bounds, existing hook sites, preserved native
   dispatcher/volume/decoder code, diagnostic labels, source revision, ZIP
   integrity, and unchanged companion/bootloader bytes. Record the resulting
   version and SHA-256 before calling the candidate ready.
5. Obtain the user's readiness confirmation for a separately agreed update
   and follow the backup/stage/readback/eject process above.

Rollback candidate `e6dab640` remains available locally at
`.build/dfu-prep/verified-osos-89f9c6e.bin`, SHA-256
`0804f3bcb23a83e5efe3338507828f76756b1002a0f5c0b51ff12fc5e59726bd`.
Its checksum was rechecked during preparation. No iPod access or installation
was performed during this diagnostic preparation.

## Verified and copied — 2026-10-14

Run `38664148302` subsequently completed successfully for the pinned diagnostic
source. The local assembled image passed all 92 preimage checks and the
independent read-only audit described above. Build label: **`b53682f0`**.
The image is 11,617,536 bytes, SHA-256
`325d600cec118366dd1a70d133eabd6593e556e505e233e83d8fb83ff7f3537a`.
Bundle SHA-256:
`fe289f2542ae74ad50e1d09b2be7b698d74ddadd80b72925ca508fed77d6571b`.

After the user explicitly confirmed battery, secure connection, no sync, and
permission to install, the mounted FAT32 iPod was verified by volume UUID
`51CBB953-E32A-3660-A943-9C91B0702890` and the expected existing firmware and
companion hashes. The old firmware, companion and complete Device settings
directory were backed up under
`private-backups/ipod-mb029-2026-10-11/before-wheel-probe-b53682f/`.
The previous firmware was also preserved as
`/osos-cfw.previous-e6dab640.bin` on the iPod.

The diagnostic was staged, synchronized and checksum-verified before replacing
`/osos-cfw.bin`, then verified at its final path. The rollback copy and Mac
backup matched the original installed hash. Device settings compared unchanged,
and the companion hash was unchanged. No music, partition, bootloader or NOR
writes were performed. Safe ejection of the identity-checked device succeeded.

**The copy is complete; reboot, successful diagnostic boot, panel legibility,
and wheel-event observations still await the user's hardware test.**

## Hardware evidence and controller correction

The user's photos `IMG_5076`, `IMG_5077`, and `IMG_5078` show the diagnostic
running. In capture order, W increases `00006b → 0000a1 → 0000f6`; V, L and M
increase `00005e → 000094 → 0000e9`. Every photo has G=5, N=P=S=R=0,
Layout=`0dad0974`, VT=`089a2cec`, VW=0. Thus wheel events reach the hook and
pass visibility/layout/manager checks, but the current controller-type check
rejects them before ancestry or volume dispatch. The readings do not establish
whether those later stages would succeed.

Read-only inspection of the pinned Apple image confirms:

- Constructor `0x08214db4` allocates 0x128 bytes and calls the known Now Playing
  base constructor `0x08215aec` on that same allocation (`bl` at `0x08214dd4`).
- It then writes the vtable from literal `0x08214e1c` to object offset zero;
  the literal is exactly `0x089a2cec`. Its screen-definition member starts at
  +0xec, after the existing base fields. There is no base-pointer adjustment.
- Derived destructor `0x08217d40` destroys that member, subtracts 0xec and
  tail-calls base destructor `0x08215ba8`. The two primary vtables share the
  native parent accessor at +0x1c (`0x0828ea64`).

The correction explicitly accepts only the existing base vtable and this
verified derived vtable, forwarding the unchanged controller/event pointers
to the existing native volume action. Visibility, ready-layout, non-null
manager/current-controller, bounded focus ancestry, native input filters,
native volume limits and event ownership remain unchanged. There are no new
hook sites. The WHEEL PROBE 1 panel remains enabled for the next device test.

The host routing fixture now executes its complete positive/negative matrix
for both known vtable types, with diagnostic mode both enabled and disabled.
It also rejects unknown, null and adjacent-but-not-exact vtable values. This
targets the observed type-check failure; it does not claim on-device volume
is fixed before the next hardware test.

### Verified correction candidate — not installed

Source `9c8df629573ddec97964bb38c5e45011748639cc` built successfully in
[run 38680047090](https://github.com/Christian-TechUCM/osos-lab/actions/runs/38680047090).
The production build passed the Python, sanitized native C, Rust, JavaScript,
lint and packaging checks with the pinned ARM GCC 9.5.0 toolchain. Payload use
is 932,580 / 983,040 bytes, leaving 50,460 bytes free.

The local image `.build/dfu-prep/verified-osos-9c8df62.bin` is 11,617,536 bytes
and reports **`9c8df629`** in CFW Info. SHA-256:
`cf1b6c065e75a436cff688f3be69aa135a7b11487e1f9c5cf43aa30f1b9eeaba`.
Bundle SHA-256:
`e7ccacb3516beb926b823b393088711717230cb34ec71446f64f83cc6acf687e`.

Offline assembly and `.build/verify_wheel_probe_image.py 9c8df62` passed:
the same 92 original-byte checks, asset hashes, output bounds, existing hook
targets, preserved native dispatcher/volume/decoder code, diagnostic labels,
revision and ZIP integrity. The strengthened audit also verifies the derived
constructor's decoded base-constructor branch, vtable literals, unmodified
constructor/destructor bytes and both native parent-accessor slots. Companion
and bootloader files remain byte-identical to the earlier package.

No iPod access or installation was performed while preparing this correction.
The currently installed diagnostic remains `b53682f0` until a separately
approved update. After updating, repeat the photos and actual-volume test;
G=8 with increasing S/R demonstrates dispatch/return, not by itself an audible
volume change. A remaining G=6 would point to the focus-ancestry check and must
be investigated rather than bypassed blindly.
