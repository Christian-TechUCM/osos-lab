# Live lyrics prototype

This is a **Mac/browser proof of concept**, plus a portable C timeline core.
It does not add a Lyrics screen to the installed firmware yet. The C core is
outside `payload/`, so the firmware bundle contains only the existing Reprise
features and the imported idle-clock fixes. No new firmware addresses are guessed.

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
.build/lyrics-venv/bin/python tools/lyrics.py export /path/to/song.m4a
.build/lyrics-venv/bin/python tools/lyrics_preview.py --audio /path/to/song.m4a
```

Open <http://127.0.0.1:8766>. The server binds only to loopback and serves a fixed
asset list, the selected track's lyrics, and that single audio file. It does not
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

## Firmware integration gates

1. Confirm the player clock units. The tracked Apple 2.0.5 analysis names
   `MediaPlayer_GetPosition` at `0x0816f704`, but its annotation explicitly says
   the backend units are not established. Do not assume milliseconds. Validate
   duration, normal playback, pause, and seeking against that same backend.
2. Confirm a stable current-track identity and lifetime. Use the actual synced
   iTunesDB persistent ID and a verified path mapping, not title/artist guesses.
   A host exporter can read the synced files and key their sidecars by that ID.
   Verify that Finder preserves the embedded tags through sync/conversion.
3. Verify the Lyrics controller lifecycle, text layout, click-wheel handling,
   and timer/event ABI against 2.0.5. Proposed access is a Lyrics action from
   Now Playing. Reset all state on track change or screen exit; free storage
   only after callbacks cannot access it.
4. Read and validate one sidecar on track change. Poll the verified playback
   clock on a UI-owned timer; redraw only when the cue changes. Keep file I/O
   and parsing out of audio callbacks. Proposed polling interval: 100 ms,
   subject to measurement of UI load and battery use on hardware.
5. Prototype long-line scrolling and UTF-8-to-native-text conversion using the
   actual iPod fonts. Missing/malformed sidecars fall back to static text or a
   clear message without interrupting audio. A seek must reset the active cue.
6. Build, check image preimages and memory bounds, and review the first native
   implementation before a separate, explicitly approved hardware install.

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
