# Native live lyrics

This fork implements an experimental line-timed lyric view in the Apple 2.0.5
payload. It uses the embedded lyric text already loaded by Apple's player and
the existing Now Playing Extra Info screen. This is native firmware code, not
the browser preview, but it has not been installed or verified on an iPod.

## Controls and limitations

Hold Center in Now Playing and choose **Live Lyrics**. This action is available
when Apple's model reports extra information for the track. Timed LRC text is
replaced by the current line; ordinary text, descriptions and unsupported tags
keep the stock text path. The usual Center-button route to lyrics also uses
the replacement property. Empty cues and the interval before the first cue
display no lyric text.

After a timed tag has loaded, the context menu offers **Lyrics: 0.1s earlier**,
**Lyrics: 0.1s later**, and **Lyrics: reset timing**. The adjustment is limited
to ±5 seconds, applies only to the display, and resets on track replacement.
It is not saved to disk. Seek and pause behavior follow the player position,
not elapsed wall time.

The lyric body uses Apple's existing large bold font instead of the small
Now Playing font. This is one fixed larger size, not the preview's three-size
selector. Native wrapping, scrolling, navigation and character support remain
Apple's; their appearance and behavior with changing lines need hardware tests.

The stock position event is emitted after a position change of at least
1,000 ms. This implementation therefore refreshes at approximately one-second
intervals during playback, with additional refreshes on relevant stock events.
It does not yet provide smooth scrolling, neighboring-line highlighting,
word-level karaoke, or album-color blurred backgrounds. Those remain browser
prototype features. The existing GitHub Dark palette and Sleep Timer shortcut
remain in the firmware independently of lyrics.

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
1,000 ms threshold described above.

The new patch declarations check the original words before replacement:

| Patch location | Expected original | Replacement |
| --- | --- | --- |
| `0x08998708` | Property entry `0x081eda2c` | `cfw_lyrics_property` |
| `0x089986d4` | Event entry `0x081efe74` | `cfw_lyrics_model_event` |
| `0x0882ad3c` | Font ID `0x0dad0013` | Large font ID `0x0dad001e` |

The font word belongs to View resource `0x0dad092f`, at resource offset `0x108`.
Its text-view instance is `0x0dad0931`; source resource `0x51a` binds model
`0x7f00` and property `0x7f1b`. The patch changes only that view's font reference,
not a shared font definition. The original and replacement named fonts are
`NowPlaying_Music_Small_Font` and `Settings_LargeFont`.

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
property, and only when the active cue changes. No audio callback, additional
timer, filesystem write, sidecar path, or title-based track mapping is added.

## Validation and remaining checks

Host tests compile the actual payload parser and mocked native hooks under
AddressSanitizer and UndefinedBehaviorSanitizer. They cover track replacement,
seeking, paused position, timing clamps, missing and malformed tags, UTF-8,
oversized input, support-query dispatch, and preservation of stock UI events.
An independent Python parser is used for comparison with generated test cases.
The private library audit matched all 1,479 timed tracks across 93,953 cues;
209 tracks had no lyrics and one mixed-format tag uses the stock fallback.
Music and firmware inputs stay outside Git.

A compiled recipe must still be assembled against the pinned Apple input to
verify patch preimages and the payload reservation. Successful host tests and
assembly do not establish UI lifetime, refresh latency, font fit, scrolling,
power use or playback reliability on hardware. Any hardware trial requires a
separately agreed installation and recovery plan. In particular, a Mac-formatted
iPod requires an explicitly approved FAT32 migration; development must not
restore or repartition it.
