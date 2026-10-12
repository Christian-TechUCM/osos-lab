# Native live lyrics

This fork implements an experimental line-timed lyric view in the Apple 2.0.5
payload, including eased scrolling and a multicolor blurred album background.
It uses the embedded lyric text already loaded by Apple's player and
the existing Now Playing Extra Info screen. This is native firmware code, not
the browser preview, but it has not been installed or verified on an iPod.

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

The lyric body uses Apple's existing large bold font instead of the small
Now Playing font. This is one fixed larger size, not the preview's three-size
selector. Apple's font renderer handles wrapping and character support; the
lyric renderer positions up to five cue groups inside the existing reading
panel. Header, album thumbnail, navigation and playback controls remain native.

Transitions ease over 350 ms (less for short cues), moving the incoming line
toward the center and crossfading its brightness with the outgoing line. A cue
taller than the panel pans during its available time (a six-second display
window for the last cue, which has no following timestamp). The view's existing timer
polls at 33 ms while moving and 100 ms otherwise. Unchanged frames are not
invalidated; a paused song clock freezes the transition. These are scheduling
targets, not measured iPod frame rates.

Artwork is sampled spatially into a 16 × 12 RGB grid, blurred with three box
passes, darkened and expanded into a cached RGB565 bitmap. Bounded ordered
dithering reduces gradient banding on the 16-bit display. Different regions of
the cover contribute different colors. The background covers only the lyric
reading panel, not the full screen. It is static per track, not Apple's animated
fluid effect. Missing or unsupported bitmaps and allocation failures use the
GitHub Dark base color; Apple's placeholder can be sampled when the model
supplies it instead of a cover. No host-generated image or sidecar is needed.

Selectable font sizes and word-level karaoke are not implemented in firmware.
The existing GitHub Dark palette and Sleep Timer remain independent features.

## Verified firmware bindings

The supported plaintext input is `osos.bin`, 10,634,496 bytes, SHA-256
`660eb6ff5f498bb1a75e85f1f9921bf0c753525a8bca9b4fa47ed260e5ad0f1e`.
Addresses below were checked against that image's ARM instructions and resource
data. Names describe the observed behavior; they are not an Apple API contract.

| Address | Role and evidence |
| --- | --- |
| `0x0816f704` | Player position getter. The UI proxy thunk at `0x082a02c8` calls it; the Now Playing elapsed-time property divides that result by 1,000 for seconds. |
| `0x081eda2c` | Now Playing property getter, called with model, type, property and output pointer. String property `0x7f1b` returns cached UTF-8 extra-info text. |
| `0x081efe74` | Model event handler. Event `0x80008` updates position properties; `0x80009` and `0x8000a` refresh the current track. |
| `0x081ed74c` | Lazy singleton getter for the Now Playing model. |
| `0x081ef374` | Checks availability of lyrics or other extra information. |
| `0x081ef9a8` | Prepares the stock extra-info state before the layout transition. |
| `0x08214610` | Chooses the ready/loading Extra Info layout on the Now Playing controller. |

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
event. Every stock event is forwarded. A position event notifies just the lyric
property, and only when the active cue changes. The view adapter independently
reads the same timeline and player clock on the UI thread. It reuses the
TextView's initialized timer, and stops it on hide, destruction or fallback.
No new timer object, audio callback, filesystem write, sidecar path, or
title-based track mapping is added.

Five bounded text buffers hold the neighboring groups. One cached background
allocation is bounded to 153,628 bytes (the full 320 × 240 limit; the current
panel is at most 140 pixels tall). Allocation is attempted once per track/size,
not every frame. It is freed on track/size replacement, hide, destruction or
fallback. Artwork pointers are borrowed only during synchronous sampling;
track replacement cannot leave a previous cover cached under the new generation.
Native context clipping protects the surrounding UI. Malformed groups or
unsupported panel geometry fall back to the ordinary text renderer.

## Validation and remaining checks

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

Build validation is recorded after each native change. The previous one-line
build is not evidence for the new drawing hooks. No hardware installation has
been performed.

Successful host tests and assembly do not establish UI lifetime, refresh latency, font fit, scrolling,
power use or playback reliability on hardware. Any hardware trial requires a
separately agreed installation and recovery plan. In particular, a Mac-formatted
iPod requires an explicitly approved FAT32 migration; development must not
restore or repartition it.
