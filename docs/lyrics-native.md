# Native live lyrics

This fork implements an experimental line-timed lyric view in the Apple 2.0.5
payload, including eased scrolling and a multicolor blurred album background.
It uses the embedded lyric text already loaded by Apple's player and
the existing Now Playing Extra Info screen. This is native firmware code, not
the browser preview. The user installed the previous `56538c3` build on their
MB029 and reported working playback/lyrics. Their hardware photos exposed
layout, contrast, screen-idle and scrolling issues. The subsequent `237a756`
file update booted: the user reported excellent light-mode rendering, but dark
mode photos exposed a white header behind white text. Build `716321b` restored
readable header text, but the user still observed a boxed backdrop and lag.
The subsequent `ec97c78` source / `a9541fd0` build booted successfully. The user
confirmed full-screen album backgrounds and working playback buttons on lyrics,
but **the volume wheel still does not change volume and scrolling still lags**.
The next local changes below are not installed or hardware-verified.

## Controls and limitations

Hold Center in Now Playing and choose **Live Lyrics**. This action is available
when Apple's model reports extra information for the track. Timed LRC text is
displayed as a scrolling window of neighboring cues; ordinary text, descriptions and unsupported tags
keep the stock text path. The usual Center-button route to lyrics also uses
the replacement property. Empty cues and the interval before the first cue
have no highlighted lyric text; surrounding timed cues can remain visible.

After a timed tag has loaded, the context menu offers **Lyrics: 0.1s earlier**,
**Lyrics: 0.1s later**, and **Lyrics: reset timing**. The adjustment is limited
to ±5 seconds, applies only to the display, and resets on track replacement.
It is not saved to disk. Seek and pause behavior follow the player position,
not elapsed wall time.

Settings → Lyrics offers Small / Medium / Large text, Automatic / Blue / Cyan /
Purple / Gold lyric color, Background: Solid theme color / Album artwork, and
Keep screen awake. Defaults are Medium, Automatic, Solid theme color, On. Existing
saved artwork choices remain valid and are preserved; select Solid theme color manually
to compare performance on an already-configured iPod. Automatic means blue on
white in light mode and near-white on the chosen dark background. The other colors use separate
light/dark variants for contrast. Apple's font renderer handles wrapping and character support; the
lyric renderer positions up to five cue groups inside the existing reading
panel. Header, album thumbnail, navigation and playback controls remain native.

Transitions ease over 500 ms (less for short cues), moving the incoming line
toward the center and crossfading its brightness with the outgoing line. A cue
taller than the panel pans during its available time (a six-second display
window for the last cue, which has no following timestamp). The view's existing timer
polls at 33 ms while moving and 100 ms otherwise. Unchanged frames are not
invalidated; a paused song clock freezes the transition. These are scheduling
targets, not measured iPod frame rates. Animation frames reuse the copied tag;
cue changes reuse overlapping row measurements and no longer trigger native
full-text property relayout. An opaque background blit skips the redundant fill.
The cached native bitmap/context now live alongside their pixel allocation,
instead of being constructed and destroyed on every text frame. Polling switches
to 33 ms within 150 ms before the next cue; entering a transition also replaces
an already-armed 100 ms idle timer rather than waiting for it to expire.
Each timer-driven redraw now reuses the timer's playback snapshot and completed
scene step, instead of acquiring the audio-position mutex and computing the
same row geometry again. A track-generation change, unrelated native event or
view lifecycle change discards this pending snapshot. Non-property
input events no longer trigger redundant native text loading. These changes
reduce work, but do not establish an on-device frame-rate improvement. Compare
the same song, font size and cue transitions with the two background settings.

The reading panel reserves 140 pixels independently of its auto-sized parent's
initial bounds. This removes a one-line/empty-parent measurement feedback loop.
If custom drawing cannot run, timed tags now provide a complete, timestamp-free
plain-text fallback rather than only the current cue. Actual LOKERA behavior
must still be rechecked on hardware; the photos alone cannot prove the cause.

Artwork is sampled spatially into a 16 × 12 RGB grid, blurred with three box
passes, darkened and expanded into a cached RGB565 bitmap. Bounded ordered
dithering reduces gradient banding on the 16-bit display. Different regions of
the cover contribute different colors. The backdrop is intended to cover the
full screen, including album/artist header, margins and status strip. The status
strip is overridden only while a visible lyric view belongs to the same native
window. Icons and text remain native. It is static per track, not Apple's animated
fluid effect. Missing or unsupported bitmaps and allocation failures use the
theme's navy base color; Apple's placeholder can be sampled when the model
supplies it instead of a cover. No host-generated image or sidecar is needed.

Light mode now also supports album backgrounds: the blurred colors are blended
15% with 85% of the light theme surface for readable dark/blue text. Dark mode
retains its 30% artwork / 70% theme mixture. Both modes' backdrop, status strip
and reading panel crop the same 320 × 240 image
using native screen-space bounds. The previous forced navy top band was removed.
After the first lyric draw or a track-generation change, one complete repaint
refreshes the header that was drawn before the lyric tag loaded. Animation
continues to invalidate only the lyric view, not the whole screen.

## Physical clickwheel controls

The pinned native `SEVT` table for ExtraInfo (`0x0dad0974`) has an empty `wheel`
handler. The compiler now replaces exactly that record with `CFW_LyricsVolume`, rejecting missing/duplicate or
unexpected records. Other events and their template provenance are preserved.
The handler calls Apple's volume-wheel routine `0x0820d3e4`, preserving its
decoding, volume-limit and acknowledgement behavior, then stops the overlay
timeout at controller `+0xb8` so it does not leave lyrics four seconds later.
The native volume notification handler `0x082146c0` already suppresses its
overlay for ready ExtraInfo layout `0x0dad0974` (literal at `0x0821476c`).
Loading/failed layouts and the loading timer at `+0xe0` are untouched. Support
queries are inert.

Hardware feedback shows the resource binding alone was insufficient. The next
candidate also hooks **only** Now Playing's input-dispatch vtable slot
`0x0899be4c` (expected original `0x0812a070`). It uses the original event-name
formatter `0x08291974`, checks the current layout via `0x081747ec` + `0x28`, and
forwards a named `wheel` event on ready layout `0x0dad0974` directly to the same
Apple volume handler. It preserves the original event pointer, not a fabricated
wheel delta. All other events/layouts call the original dispatcher. The same
formatter and event pointer are used by the original dispatcher; these ABIs
were checked against the pinned 2.0.5 disassembly. This is a **candidate fix**,
not proof that wheel events reach this slot on hardware.

The stock tables already bind play/pause, next, previous, Menu, Center and
long-press seeking. Their handlers and event propagation are retained, not
duplicated with speculative raw-button hooks. The user's latest hardware report
confirms these buttons work. Only volume-wheel input remains unverified in the
new candidate; do not conflate it with the working playback buttons.

Earbud inline controls are a separate hardware path; see [the investigation](earpods-remote.md).

The dark palette adapts [Afterburner](https://www.dexthemes.com/afterburner/dark):
the main background is now a slightly lighter `#203049`, with `#162238` for the
sidebar, `#F2F7FF` ink and `#39D0FF` accent. Selection `#244058` and
border `#354762` are adaptations, not exact copies. Word-level karaoke remains
unsupported; Sleep Timer is independent.

## Selectable themes (local candidate)

Settings → Themes contains **Dark palette**, **Light palette**, and **Accent
color**. The existing Dark Mode switch still chooses which palette is active.
Defaults preserve Original navy and Original light. Theme selections are stored
separately in versioned/checksummed `iPod_Control/Device/CFWThemes`, with bounded
selectors, corrupt-record fallback, and a save-failure retry item. The existing
`CFWDark` and `CFWLyrics` formats are unchanged.

The requested palettes use published surface/sidebar/accent colors:

| Palette | Surface | Sidebar | Accent |
| --- | --- | --- | --- |
| Original navy | `#203049` | `#162238` | `#39D0FF` |
| [Chrome Future Hunter](https://www.dexthemes.com/chrome-future-hunter/dark) | `#111111` | `#0A0A0A` | `#DC2626` |
| [Huawei](https://www.dexthemes.com/deepseek-huawei/dark) | `#130C10` | `#0D080B` | `#E51A3C` |
| [Quiet Orbit](https://www.dexthemes.com/quiet-orbit/dark) | `#11121A` | `#0A0B13` | `#8B7CF6` |
| [Bioluminescent Moon](https://www.dexthemes.com/bioluminescent-moon/light) | `#ECFEFF` | `#E2F4F5` | `#14B8A6` |
| [Cybernetic Major](https://www.dexthemes.com/cybernetic-major/light) | `#F2FCFF` | `#E8F2F5` | `#38BDF8` |

Ink, borders and selection surfaces are small-screen adaptations, not exact
DexThemes replicas. Bright light-mode accents use a darker selection/text color
to preserve white-selection-text contrast. Accent override choices are Theme
default / Blue / Cyan / Red / Purple / Teal / Gold. Only known native color
paths and our manifest-listed UI assets are recolored; album artwork and
red/green semantic indicator pixels are not recolored. Some native UI elements
outside those paths may remain stock and need hardware review.

Recolored UI assets are cached once per listed asset (maximum 489,764 bytes of
pixel storage), not once per theme or frame. Their addresses stay valid for
native cached views. Every recolor starts from immutable templates to avoid
cumulative rounding errors. Allocation failure falls back to original navy
assets in dark mode or stock assets in light mode, with no per-frame retry
loop. The original navy bitmap path is byte-for-byte unchanged.

### Scrolling follow-up

The wallpaper blur is already generated only on track, size or palette changes.
Removing it avoids a background blit but does not remove the native wrapped-text
renderer, which still walks and draws visible text every moving frame. This
change removes duplicated timer/draw queries and stepping, but does **not**
claim smooth 30 fps. The next larger optimization would be a bounded glyph/line
raster cache (verify native font/alpha/clip ownership first), followed by measured
frame pacing and smaller dirty regions. Keep the same track, font size and cue
boundaries when comparing solid versus album backgrounds on the iPod.

Local verification: 45 patching tests (including address/undefined-behavior
sanitized native fixtures), 49 tool tests (one skipped), and 31 USB helper tests
pass. All 30 payload C translation units compile for ARM926EJ-S using Clang.
This is **not** a pinned GCC 9.5 release build: release linking, reserved-space
checking, recipe assembly and hardware testing remain required before installation.

Keep screen awake postpones only the lit-display inactivity timer while a
lyric TextView is visible (including ordinary-text fallback). It does not wake
a sleeping display, change the global Backlight preference, intercept input,
or change SleepService/Sleep Timer. Hide/destruction clears the view reference.
Normal inactivity behavior resumes on the next configured timeout after leaving
lyrics. Keeping the light on consumes more battery; the setting can be disabled.

Preferences use a versioned, checksummed 28-byte `iPod_Control/Device/CFWLyrics`
record. Bad/short records load defaults; failed writes retain the current RAM
choice and show “Save failed - retry.” No preferences are written by playback
or animation callbacks.

## Current update (`ec97c78`, 2026-10-11)

[CI run 38210779648](https://github.com/Christian-TechUCM/osos-lab/actions/runs/38210779648)
passed 125 Python cases (one skipped), 93 Rust tests (four ignored), prototype
checks, formatting/lint, GCC 9.5 compilation and packaging. The CI merge tree
equals the source tree exactly; Settings → CFW Info displays `a9541fd0`.
The payload uses 913,668 / 983,040 reserved bytes, leaving 69,372 free.

The locally assembled image is 11,617,536 bytes, SHA-256
`bb547be91ef6890158633f2d05db98d626ba760b17fbe0a67665512b8e33d9fe`.
The package ZIP SHA-256 is
`414f492938d51de0b07abffb7357f2035a3717f6d3457e766e8fccfd367fd08d`.
All 91 original preimages and ten distinct native hook targets passed independent
checks, as did package asset hashes, the unique ready-layout wheel override,
preserved native volume/compositor code and destructor trampoline.
The companion and bootloader are byte-identical to the installed `716321b` build.

Only `/osos-cfw.bin` was replaced after staging/readback verification. The old
file was verified at `/osos-cfw.previous-716321b.bin` and in the private Mac
backup `private-backups/ipod-mb029-2026-10-11/before-lyrics-controls-ec97c78/`.
Settings and companion contents were compared unchanged; music and NOR were not
written. Safe ejection succeeded. A reboot is required; successful boot and
the requested visual/volume tests have not yet been confirmed by the user.

## Verified firmware bindings

Local validation for these changes: 45 patching tests pass (including
ASan/UBSan native fixtures); 49 tools tests pass with one skipped; Ruff and
format checks pass. Four changed C units also compile as freestanding ARM926
objects with Clang. Regenerated UI structure metadata matches the pinned
private image. The subsequent GCC 9.5 CI build and package checks also pass;
on-device performance/input tests remain outstanding.

The supported plaintext input is `osos.bin`, 10,634,496 bytes, SHA-256
`660eb6ff5f498bb1a75e85f1f9921bf0c753525a8bca9b4fa47ed260e5ad0f1e`.
Addresses below were checked against that image's ARM instructions and resource
data. Names describe the observed behavior; they are not an Apple API contract.

| Address | Role and evidence |
| --- | --- |
| `0x0882ab6c` | View resource `0x0dad0929` (file `0x83629c`) +`0x68`: original `NowPlaying_Idle_Background` COLR ID `0x0dad0851`. Replaced with a private white COLR for light mode, without modifying the actual idle clock. This color ID is **not** the instance selector. |
| `0x08988974` → `0x0810012c` | BitmapView draw slot. The hook selects Extra Info instance ID `0x0dad092a` at view `+0x4c`, and shared status instance `0x0dad01a2` only in the same window as visible lyrics. Other views/light mode forward unchanged. |
| `0x081943ec`, `0x0829b090`, `0x08265bf8` | Descriptor construction: descriptor `+0x30` → options `+0x10` → view `+0x4c` is the instance ID. Descriptor color `+0x5c` resolves into RGBA at view `+0x90`. The old test of view `+0x44` was incorrect: that is the independent image-property source. |
| `0x08299080` | Resolves a view's owning window for status-strip scoping. |
| `0x0816f704` | Player position getter. The UI proxy thunk at `0x082a02c8` calls it; the Now Playing elapsed-time property divides that result by 1,000 for seconds. |
| `0x081eda2c` | Now Playing property getter, called with model, type, property and output pointer. String property `0x7f1b` returns cached UTF-8 extra-info text. |
| `0x081efe74` | Model event handler. Event `0x80008` updates position properties; `0x80009` and `0x8000a` refresh the current track. |
| `0x081ed74c` | Lazy singleton getter for the Now Playing model. |
| `0x081ef374` | Checks availability of lyrics or other extra information. |
| `0x081ef9a8` | Prepares the stock extra-info state before the layout transition. |
| `0x08214610` | Chooses the ready/loading Extra Info layout on the Now Playing controller. |
| `0x082879e8` | TextView font setter `(view, resourceService, fontID, invalidate)`: resolves a Type resource, stores it at `+0xfc`, applies the font to `+0xa4`. Small/medium/large IDs are `0x0dad0013`, `0x0dad0012`, `0x0dad001e`. |
| `0x0819cb2c` / slot `0x0899302c` | Display power event handler: event 15 with timer `controller+0x6c` dispatches inactivity event 0; timer `+0x98` dispatches later state event 14. The hook only postpones `+0x6c` while the lit-state byte `+0x68` is set and the lyric view requests it. |

Power binding evidence: `0x0819b44c` and `0x0819bdcc` set lit state `+0x68`,
select `+0x6c` intervals from the native Backlight setting (`0x6032..0x603c`),
and enable the display. Idle-clock entry `0x0819bf68` and dark entry
`0x0819af1c` clear `+0x68`; the clock controller is created by the former.
The primary slot and secondary-base slot `0x0899309c` (original `0x082a1674`)
are replaced, with both original words checked. The latter's ARM thunk subtracts
`0x48` from the UI base and directly branches to the handler; our adapter keeps
that adjustment so native timer delivery cannot bypass the hook.
The unrelated SleepService timer at `service+0x60` is untouched. Hardware
acceptance must explicitly test idle timeout, manual shutdown and Sleep Timer.

The readiness helper at `0x081ed8b8` checks the stock lyric-fetch success flag,
not the replacement string's length. Returning an empty current line does not
change this test. Position-notification producer `0x081717a0` supplies the
1,000 ms threshold; it remains a fallback notification, not the animation clock.

The new patch declarations check the original words before replacement:

| Patch location | Expected original | Replacement |
| --- | --- | --- |
| `0x08998708` | Property entry `0x081eda2c` | `cfw_lyrics_property` |
| `0x089986d4` | Event entry `0x081efe74` | `cfw_lyrics_model_event` |
| `0x0882ad3c` | Font ID `0x0dad0013` | Large font ID `0x0dad001e` |
| `0x089af0f0` | Draw `0x08288370` | `cfw_lyrics_view_draw` |
| `0x089af0ac` | Event `0x0828856c` | `cfw_lyrics_view_event` |
| `0x089af124` | Visibility `0x08288524` | `cfw_lyrics_view_visibility` |
| `0x089af198` | Measure `0x08287a64` | `cfw_lyrics_view_measure` |
| `0x082886dc` | `e92d4070 e1a04000` | Destructor entry hook with original-instruction trampoline |

The font word belongs to View resource `0x0dad092f`, at resource offset `0x108`.
Its text-view instance is `0x0dad0931`; source resource `0x51a` binds model
`0x7f00` and property `0x7f1b`. The patch changes only that view's font reference,
not a shared font definition. The original and replacement named fonts are
`NowPlaying_Music_Small_Font` and `Settings_LargeFont`.

The TextView hooks select only `Str ` source `0x7f1b` at object offsets
`0x40/0x44`. Unrelated views forward to their original handlers. The destructor
entry hook also covers direct subclass/deleting-destructor calls; it only
changes cleanup for the currently owned lyric view.

| Native binding | Verified role |
| --- | --- |
| `0x082881f8`, context `view + 0xa4` | Load native text/font state before measuring and drawing. |
| `0x08264208`, `0x08299014`, `0x08264bdc` | Set bounded panel height, get local content bounds and configure clipped drawing context. |
| `0x0825a9e0`, `0x08259b30` | Measure and draw wrapped UTF-8 using the selected stock font. |
| `0x0812195c`, `0x08121268`, `0x081219d0` | Set interval, start and stop the existing timer at `view + 0x1f4`. Stop also removes queued timer events. |
| `0x08138698` | Another native view's timer handler checks the timer pointer at event offset `+8`; TextView recognizes event code 15 at `+4`. |
| Draw property `0x7f1a`, `0x080e7f9c` | Borrow the current album bitmap and read bounded spatial samples via its surface decoder. |
| `0x08103e8c`, `0x08103984`, `0x08103ee0` | Construct a temporary Bitmap, attach the borrowed BMap pixels and destroy it. |
| `0x0825b008`, `0x0825aea4`, `0x08259568`, `0x0825b038` | Temporary source context lifecycle and clipped bitmap blit. |

## Ownership and fallback behavior

`payload/lyrics_lrc.c` is an allocation-free parser with a copied, immutable
UTF-8 tag supplied by `payload/lyrics.c`. It supports basic LRC timestamps,
fractional and overflowing seconds, repeated stamps, unsorted cues, one global
offset and enhanced-LRC word-marker removal. Timing remains line-level. Lines
at the same timestamp are displayed together in source order; repeated stamps
for the same source line do not repeat its text. Identical text on separate
source lines is retained.

Bounds are 32 KiB per embedded tag, 1,024 timestamp entries, 2,048 bytes per raw
lyric line or displayed group, and timestamps within 24 hours. Mixed or malformed
tags, unsupported Unicode line separators, invalid UTF-8, and oversized inputs
fall back to Apple's text. CR, LF and CRLF record separators are supported.
These limits are intentionally stricter than the host sidecar format.

The property hook first calls the original getter. It copies changed lyric
text instead of retaining Apple's temporary string; its model pointer is used
only for identity comparison. On a track event, it resets before forwarding the
event. Every stock model event is forwarded. The complete fallback string is
stable between track changes; position events no longer notify that property.
The view adapter independently
reads the same timeline and player clock on the UI thread. It reuses the
TextView's initialized timer, and stops it on hide, destruction or fallback.
No new timer object, audio callback, sidecar path, or title-based track mapping
is added. Only explicit lyric Settings actions write the preference file.

Five bounded text buffers hold the neighboring groups; a separate 32-KiB
buffer holds the complete plain fallback. If repeated timestamps expand beyond
that bound, the original Apple text is preserved instead of truncating it.
One cached background
allocation is 153,628 bytes (a shared 320 × 240 canvas; the reading
panel is at most 140 pixels tall). Allocation is attempted once per track/size,
not every frame. It is freed on track/size replacement, hide, destruction or
fallback when owned by a timed lyric view. An ordinary-text-only backdrop may
retain this one bounded cache until the next track/backdrop replacement. Native
source graphics objects are destroyed before their borrowed pixel storage is freed.
Artwork pointers are borrowed only during synchronous sampling;
track replacement cannot leave a previous cover cached under the new generation.
Native context clipping protects the surrounding UI. Malformed groups or
unsupported panel geometry fall back to the ordinary text renderer.

## Validation and remaining checks

### Dark backdrop follow-up (2026-10-11, source `716321b`)

[CI run 38205678238](https://github.com/Christian-TechUCM/osos-lab/actions/runs/38205678238)
passed 124 Python cases (one skipped), 93 Rust tests (four ignored), four
JavaScript tests, lint/format, ARM compilation and packaging. Native host tests
exercise the private backdrop scope, unchanged idle-clock/light-mode dispatch,
shared screen-space crops, cached native-object lifecycle, out-of-screen fallback,
and replacement of an already-armed idle timer at a transition.

Bundle version is `0.0.0-dev+751ee59d` (CI's PR merge revision). Local artifacts:

- `.build/ci-firmware-716321b/classic/repriseos.zip`, SHA-256
  `dd80755d88a09569464771a881c9e713fb42d0afb4949c94a0bb6ef879b9ccdf`.
- `.build/dfu-prep/verified-osos-716321b.bin`, 11,617,536 bytes, SHA-256
  `f9caa4e026c01098d0a306868d424d74d05379a906726baf3b08a09e7c7229a7`.

Offline assembly passed all 91 original preimages. Independent read-only checks
confirmed ten distinct payload hook targets, the private background source ID,
unchanged original BitmapView draw code and idle-clock color, and the destructor
trampoline. Companion recipe/data and bootloader are byte-identical to `237a756`.
The ZIP integrity check passed. **No iPod writes were performed for this follow-up.**
Dark header readability, seamless cropping, theme transitions and actual scrolling
smoothness still require hardware testing; host cadence is not measured FPS.

### Refinement candidate (2026-10-11, source `237a756`)

[CI run 38203200176](https://github.com/Christian-TechUCM/osos-lab/actions/runs/38203200176)
passed the 123 Python test cases (one skipped), 93 Rust tests (four ignored),
four JavaScript tests, lint/format checks, ARM GCC 9.5.0 compilation, reserved
payload-size checks and packaging. CI labels its PR-merge build
`0.0.0-dev+f6cccfda`; the feature-branch source is `237a756`.
New sanitized fixtures exercise preference corruption/short writes/retry,
all lyric choices, awake-view lifetime, both display-power dispatch bases,
non-inactivity event forwarding, zero/one-line parent geometry, light-mode
artwork exclusion, cached wrapping, and bounded complete-text fallback.

Offline assembly against the pinned original succeeded: 11,617,536 bytes,
89 checked preimages. Independent inspection verified nine distinct native
hook targets, the original destructor trampoline, and the fallback font word.
The installer ZIP passed archive integrity checks.

- OSOS SHA-256: `da04e3b6dbe3dec0f7df2c4c8fbcd273fd3d36da0fafa190856d3fb8705778cb`
- ZIP SHA-256: `a8cf39cb1634154db3af4e9d2c7e9af24b932d00ccc03656ab85dea507df5732`

No iPod files, firmware, or boot flash were changed during this refinement.
The new image is **not hardware-validated**. Before treating it as stable,
test LOKERA on first entry and after track changes; seek/pause; all font/color
settings and persistence after reboot; light/dark transitions; staying in
lyrics beyond the configured backlight timeout; normal timeout after leaving;
manual power-off; Sleep Timer expiry; and playback without stutters during
scrolling. The old working build and original backup must remain available.

### Previous baseline

Host tests compile the actual payload parser and mocked native hooks under
AddressSanitizer and UndefinedBehaviorSanitizer. They cover track replacement,
seeking, paused position, timing clamps, missing and malformed tags, UTF-8,
oversized input, support-query dispatch, and preservation of stock UI events.
Scene tests cover cue-boundary continuity, eased movement and brightness, long
cue panning, short cue intervals, pause/seek, blur mixing, RGB565 layout and
capacity guards. View tests cover scoped drawing, background cache replacement,
timer rearming, stale events, hide/destruction, unsupported artwork and allocation
failure. An independent Python parser is used for comparison with generated test cases.
The private library audit matched all 1,479 timed tracks across 93,953 cues;
209 tracks had no lyrics and one mixed-format tag uses the stock fallback.
Music and firmware inputs stay outside Git.

The [firmware build for `56538c3`](https://github.com/Christian-TechUCM/osos-lab/actions/runs/38188189959)
passed 121 Python, 93 Rust and four JavaScript tests, lint/format checks, GCC
9.5.0 ARM compilation and bundle packaging. One Python test was skipped and
four Rust tests were ignored. The same suites passed locally; additional
unsupported-artwork and unrelated-timer regressions also pass locally.

The payload uses 865,988 of 983,040 reserved bytes, leaving 117,052 bytes free.
Offline assembly against the pinned Apple input checked all 87 preimages and
output bounds. Independent inspection confirmed the seven lyric hook pointers,
the destructor's original-instruction trampoline and the large-font word.
The 11,617,536-byte OSOS image has SHA-256
`ecd28748d9febc8b9d65466172f89853a49854cc446d79811bacc33809941021`.
The development ZIP passed archive integrity checks, SHA-256
`fa58dfdbe4b773787f2916287d4ee3a731136599891164d8b28e36a06ed16b58`.
Host-rendered frames of the actual C layout/blur algorithms were visually
checked with an approximate Mac font. They are not device screenshots.
Those checks describe the previous baseline build, subsequently installed by
the user. They do not certify the new refinement build or its hardware behavior.

Successful host tests and assembly do not establish UI lifetime, refresh latency, font fit, scrolling,
power use or playback reliability on hardware. Any hardware trial requires a
separately agreed installation and recovery plan. In particular, a Mac-formatted
iPod requires an explicitly approved FAT32 migration; development must not
restore or repartition it.
