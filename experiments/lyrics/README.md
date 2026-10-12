# Live lyrics prototype

This is a **Mac/browser proof of concept**, plus a portable C timeline core.
Its sidecar C core is outside `payload/`. A separate experimental
[native implementation](../../docs/lyrics-native.md) now uses embedded LRC tags
and Apple's existing lyric view, with coarse line updates, timing adjustment
and a larger font. It is not hardware-tested. The smooth preview, selectable
font sizes and album-color background described below are not yet native.

## What works

- Read M4A `©lyr` tags with timestamped LRC. Basic MP3 USLT and FLAC lyric text
  are also supported by the host reader. A LYRICIST credit is not lyric text.
- Normalize fractions, multiple timestamps per line, offsets, simultaneous
  lines, and total-seconds stamps such as `[00:123.45]`.
- Highlight and center the current line in a 320 × 240 screen preview, driven
  by actual audio playback time. Pause freezes the line; seeking recomputes it.
- Preserve empty timed cues for instrumental breaks. Show untimed lyrics as
  scrollable static text and missing lyrics as an empty state.
- Export a JSON preview and a bounded binary sidecar for the future C reader.
- Validate and search sidecars in portable C with no allocations or native
  firmware calls. The host tests exercise the actual C implementation.
- Optionally extract embedded cover art and prepare a dark, multicolor blurred
  background for the preview and a future native renderer. Bad/missing artwork
  falls back to the plain theme without disabling lyrics.
- Adjust the displayed timing by ±5 seconds in 100 ms steps, without changing
  playback or tags. Positive values display lyrics later; negative values
  display them earlier. Loading a new track resets the adjustment to zero.
- Choose Normal / Large / Extra large lyric text (20 / 24 / 28 px in the preview).
  Layout recalculates immediately, and tall cues can be scrolled in the lyric
  viewport. Static lyrics use 15 / 18 / 21 px. Size resets on page reload.
- Preview a one-shot sleep timer (15 / 30 / 60 / 90 / 120 minutes, or Off).
  It counts elapsed monotonic time, not track position: pause, seeking, and
  changing tracks do not extend it. Expiration pauses playback, not the Mac.
  Reloading the page cancels it. Browser/OS suspension can delay callbacks;
  this preview is not a hardware-timer validation.

The first version has **line-level timing**. Word-by-word highlighting needs
word timings. Enhanced LRC word markers are reduced to their enclosing line;
no timing is invented for static lyrics. A mixed timed/untimed tag is rejected
with an explanation so it can be corrected deliberately.

## Run locally

From the repository root, use Python 3.11+ and a project-local environment:

```sh
python3 -m venv .build/lyrics-venv
.build/lyrics-venv/bin/python -m pip install -r experiments/lyrics/requirements.txt
.build/lyrics-venv/bin/python tools/lyrics.py audit /path/to/music \
  --out .build/lyrics-private/audit.json
.build/lyrics-venv/bin/python tools/lyrics.py export /path/to/song.m4a --artwork
.build/lyrics-venv/bin/python tools/lyrics_preview.py --audio /path/to/song.m4a --artwork
```

Open <http://127.0.0.1:8766>. The server binds only to loopback and serves a fixed
asset list, the selected track's lyrics/background, and that single audio file. It does not
serve directories. Audio byte ranges support seeking. File controls also let
you open exported JSON and matching audio directly in the browser.

For browsers that cannot decode ALAC, create a temporary WAV using macOS:

```sh
afconvert -f WAVE -d LEI16 /path/to/song.m4a .build/lyrics-private/preview.wav
.build/lyrics-venv/bin/python tools/lyrics_preview.py --audio /path/to/song.m4a \
  --preview-audio .build/lyrics-private/preview.wav
```

The preview copy must have the same duration as the original. Audio files are
opened read-only. Keep all real music, lyric exports, reports, and converted
audio under the ignored `.build/lyrics-private/` directory, outside Git.
Only invented sample lines are included in this repository.

Add `--artwork` to the WAV preview command as well to enable cover colors.
Artwork is prepared on the Mac once, not blurred on every playback frame. It
retains spatial color variation from the cover and darkens it for readable text.
This is a **static blurred backdrop**, not Apple's animated fluid background.
The Album colors checkbox compares it with the plain theme. Opening another
lyrics JSON clears the previous cover; JSON files do not grant arbitrary file
access, so artwork preview currently requires the server's `--artwork` option.

Exports add `<key>.background.png` and `<key>.background.bmap`. The native-format
RGB565 bitmap is exactly 153,628 bytes (28-byte header plus 320 × 240 × 2 pixels).
That is only an asset format, not a native screen implementation or a measured
device-memory budget. Artwork inputs are capped at 8 MiB / 16 megapixels; the
future firmware must load only the current track's background and validate it.

## GitHub Dark palette

`payload/theme.h` is the shared native/host color source: background `#0d1117`,
ink `#e6edf3`, sidebar `#090c10`, and accent `#58a6ff`, following the palette
published in [DexThemes' GitHub Dark comparison](https://www.dexthemes.com/articles/github-dark-vs-one-dark-vs-dracula-for-codex).
Selection `#17345b` and border `#30363d` adapt that appearance to the iPod UI.
The existing native dark-mode fill, text, clear, reflection, and bitmap paths
use these colors. Light mode and semantic red/green icons remain unchanged;
source PNGs are not overwritten. The upstream light idle-clock exception is
preserved. The browser preview uses matching dark colors, not an exact model
of Apple's font rasterizer or the LCD's RGB565 quantization.

## Validation

```sh
python3 -m unittest discover -s patching/tests
python3 -m unittest discover -s tools/tests -p 'test_lyrics*.py'
node --test experiments/lyrics/test_engine.mjs
```

Tests cover native/exporter interoperability, UTF-8, before-first/exact-boundary
lookup, repeated pauses, backward seeking, invalid track replacement, integer
overflow, truncation, malformed UTF-8, and deterministic byte mutations under
AddressSanitizer/UndefinedBehaviorSanitizer. GitHub's Firmware workflow also
compiles the standalone core for ARM926EJ-S with the pinned GCC toolchain.

The full firmware workflow compiles the imported fixes and produces a bundle.
Build success does not validate hardware timing or Apple-image preimages; the
bundle assembler still checks those against the pinned Apple inputs.

## Sidecar format: LRCS v1

All integers are little-endian. The first 16 bytes contain magic `LRCS`, a
16-bit version (`1`), 16-bit flags (`0`), 32-bit cue count, and 32-bit text size.
Each 12-byte record contains a start time in milliseconds, a byte offset into
the text block, and its UTF-8 byte length. The text block follows all records.
It is length-delimited, not NUL-terminated. Empty strings are valid cues.

Limits: 128 KiB per sidecar, 1024 cues, 2048 UTF-8 bytes per cue, and 24 hours.
Times must strictly increase, text ranges must be contiguous, and the file must
end exactly after the text block. The view borrows immutable caller-owned data.
Always validate before rendering, and honor `length` when copying text.

The prototype export filename identifies the local source path. **It is not an
iTunesDB identifier and must not be used to bind a track on the iPod.**

`lyrics_current_adjusted` supplies the same ±5000 ms display-only adjustment in
the portable C reader. It uses bounded signed arithmetic so a positive delay
can correctly produce a before-first-line result, including cues at zero;
negative delays cannot wrap the unsigned playback clock. This sidecar reader
is not connected to the iPod player; the native implementation parses Apple's
embedded lyric string instead. Selectable native sizes and persistent per-track
preferences remain pending.

## Native sleep-timer shortcut

The Now Playing context menu now includes Sleep Timer. It resolves the stock
Alarms sleep screen by name and uses the existing Song Info controller-factory
and foreground-push ABI. Apple's existing controller owns timer selection,
expiration, and return navigation. No new firmware address, timer-state offset,
or audio callback is introduced. Missing resources, registry, or screen creation
stop the action without pushing a null controller.

This is a narrow adaptation of kayboll's [upstream PR #15](https://github.com/lgreitis/osos-lab/pull/15),
commit `0efdcab988d07102c5006ea40e9a5780d8c560ea`. The broader PR's Night Shift and
audiobook shuffle-selector replacement are not imported. Host mocks verify
action/support-query dispatch and failure paths; UI tests verify the existing
Now Playing events are preserved. Actual navigation and timer expiration still
require image assembly and a separately planned hardware test.

## No-device firmware-input search (2026-10-11)

- The [official distribution](https://github.com/lgreitis/osos-lab#distribution)
  intentionally packages patches/recipes, not Apple's plaintext firmware.
- The [installer preparation code](https://github.com/lgreitis/repriseos-installer/blob/main/src-tauri/src/install/firmware.rs)
  can reuse `firmware/osos-2.0.5.bin` and `firmware/aupd-2.0.5.bin` from an existing
  cache after verifying them. No existing installer cache was found on this Mac.
- The [installer's local-package path](https://github.com/lgreitis/repriseos-installer/blob/0549870e161fd2bc5fa5fdec3ffd9e20a846d4bb/src-tauri/src/package.rs)
  accepts a selected RepriseOS ZIP and validates its contents and compatibility.
  This provides a route for a future custom build without changing the official
  release feed. It does not supply Apple plaintext, validate our future native
  Lyrics implementation, or remove the FAT32 requirement. We have not launched
  the installer or selected an installation target.
- The [QEMU project](https://github.com/davidmonterocrespo24/qemu-ipod-classic)
  explicitly supplies no firmware and requires a device dump. Its emulator is
  not a way to obtain the missing decryption key or firmware.
- The [firmware-research project](https://github.com/giek2000/ipod-classic-firmware-research)
  contains analysis and a hardware-assisted decryption guide, not the required
  binaries. A public 2.0.4 image is a different version and cannot substitute
  for the pinned 2.0.5 inputs.

No verified public download of the required 2.0.5 plaintext was located. A
legitimately obtained preserved generic OSOS/AUPD pair can be checked with the
existing `tools/import_inputs.py` workflow and `firmware/apple.json` hashes.
Another device's personalized companion or NOR image must never be used as this
device's installation identity. No device was queried, rebooted, or written to
as part of this search.

## Firmware integration gates

Pinned Apple 2.0.5 inputs were acquired in a separately authorized temporary
DFU decryption session. The user subsequently confirmed normal Apple boot and
music playback. Inputs are private and ignored by Git. The new implementation
uses verified instructions and resource data from those inputs; see the
[native binding evidence and limitations](../../docs/lyrics-native.md).

Remaining gates are compiled-image preimage/bounds validation followed by a
separately planned hardware trial. That trial must check embedded tags after
sync, loading and navigation, track changes, pause/seek, timing controls, large
font wrapping/scrolling, dark-mode contrast, playback stability and battery use.
The v1 implementation uses the stock position event, not a new 100 ms timer.
Smooth scrolling and album-color rendering require further native work.

The current device was identified as a 2007 6G MB029 with Apple 1.1.2 Mac.
Reprise requires FAT32; Mac-formatted storage needs a separate migration before
installation. Do not restore/repartition as part of this development workflow.

## Imported upstream fixes

- [#16](https://github.com/lgreitis/osos-lab/pull/16), original commit
  `ae0374e9c2bc4dbf1bbb29efee5ea2faecfc5422`: preserve the light idle-clock
  background in dark mode. The color is cached at boot, so testing this behavior
  after toggling dark mode requires rebooting.
- [#17](https://github.com/lgreitis/osos-lab/pull/17), original commit
  `7052c51eec46f7aa559dd0abab1a37d1a59e0a22`: delay backlight requests 350 ms
  while leaving the idle clock. Its author reports a 6G hardware test; this fork
  has host mock coverage, not independent hardware verification.

They are separate commits, not a Git dependency chain. They complement one
another behaviorally. Original authorship is preserved by cherry-picking.
Added regressions exercise color forwarding, unchanged shared resources,
ordinary backlight requests, canceled/delayed wake, and unrelated timer events.
