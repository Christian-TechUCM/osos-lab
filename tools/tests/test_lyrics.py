# SPDX-License-Identifier: GPL-3.0-only
"""Real exporter/native-core interop, malformed input, and seek boundaries."""

import ctypes
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import lyrics

ROOT = Path(__file__).resolve().parents[2]


class ParserTests(unittest.TestCase):
    def test_lyricist_credit_is_not_lyrics(self):
        audio = SimpleNamespace(
            tags={"----:com.apple.iTunes:LYRICIST": ["Writer"]},
            info=SimpleNamespace(length=120),
        )
        module = SimpleNamespace(File=lambda path: audio)
        with patch.dict(sys.modules, {"mutagen": module}):
            metadata, result = lyrics.read_track(Path("example.m4a"))
            self.assertEqual(result.mode, "missing")
            self.assertEqual(metadata["duration_ms"], 120000)
            audio.tags["\xa9lyr"] = ["[00:01]Example"]
            self.assertEqual(lyrics.read_track(Path("example.m4a"))[1].mode, "synced")

    def test_fraction_precision_and_repeated_chorus(self):
        result = lyrics.parse_lrc("[00:01.1][00:03.12]Chorus\n[00:02.123]Verse")
        self.assertEqual([c.start_ms for c in result.cues], [1100, 2123, 3120])
        self.assertEqual([c.text for c in result.cues], ["Chorus", "Verse", "Chorus"])

    def test_offsets_simultaneous_lines_and_instrumental_breaks(self):
        result = lyrics.parse_lrc(
            "\ufeff[ar:Example]\n[00:00.2]First\n[00:00.2]Second\n"
            "[00:01.2]\n[offset:-500]"
        )
        self.assertEqual(
            result.cues, (lyrics.Cue(0, "First\nSecond"), lyrics.Cue(700, ""))
        )

    def test_total_seconds_format_seen_in_library(self):
        self.assertEqual(lyrics.parse_lrc("[00:123.45]Line").cues[0].start_ms, 123450)

    def test_missing_and_static_are_not_fake_synced(self):
        self.assertEqual(lyrics.parse_lrc("").mode, "missing")
        result = lyrics.parse_lrc("First line\nSecond line")
        self.assertEqual(result.mode, "static")
        with self.assertRaises(ValueError):
            lyrics.pack(result)

    def test_word_timing_reduces_to_documented_line_resolution(self):
        result = lyrics.parse_lrc("[00:01.00]<00:01.00>Hello <00:01.50>there")
        self.assertTrue(result.has_word_timing)
        self.assertEqual(result.cues[0].text, "Hello there")

    def test_rejects_ambiguous_and_oversized_inputs(self):
        for text in (
            "[00:01.1234]Bad",
            "[00:xx]Bad",
            "[00:01]Timed\nUntimed",
            "[offset:1]\n[offset:2]\n[00:01]Line",
            "[9999:00]Far away",
            "[00:01]" + "é" * 1025,
            "[00:01]Zero\x00byte",
            "\n".join(f"[00:{n:02d}]Line" for n in range(1025)),
        ):
            with self.subTest(text=text[:60]), self.assertRaises(ValueError):
                lyrics.parse_lrc(text)


class Timeline(ctypes.Structure):
    _fields_ = [
        ("data", ctypes.c_void_p),
        ("size", ctypes.c_size_t),
        ("count", ctypes.c_uint32),
    ]


class Line(ctypes.Structure):
    _fields_ = [
        ("start_ms", ctypes.c_uint32),
        ("text", ctypes.c_void_p),
        ("length", ctypes.c_uint32),
    ]


@unittest.skipUnless(shutil.which("cc"), "Host C compiler required")
class NativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="lyrics-test-")
        library = Path(cls.temp.name) / "timeline.so"
        subprocess.run(
            [
                "cc",
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-shared",
                "-fPIC",
                str(ROOT / "experiments/lyrics/timeline.c"),
                "-o",
                str(library),
            ],
            check=True,
        )
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.lyrics_open.argtypes = [
            ctypes.POINTER(Timeline),
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        cls.lib.lyrics_open.restype = ctypes.c_int
        cls.lib.lyrics_current.argtypes = [ctypes.POINTER(Timeline), ctypes.c_uint32]
        cls.lib.lyrics_current.restype = ctypes.c_int
        cls.lib.lyrics_current_adjusted.argtypes = [
            ctypes.POINTER(Timeline),
            ctypes.c_uint32,
            ctypes.c_int32,
        ]
        cls.lib.lyrics_current_adjusted.restype = ctypes.c_int
        cls.lib.lyrics_line_at.argtypes = [
            ctypes.POINTER(Timeline),
            ctypes.c_uint32,
            ctypes.POINTER(Line),
        ]
        cls.lib.lyrics_line_at.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def open(self, blob):
        self.buffer = ctypes.create_string_buffer(blob)
        self.timeline = Timeline()
        return self.lib.lyrics_open(ctypes.byref(self.timeline), self.buffer, len(blob))

    def test_export_roundtrip_unicode_pause_seek_and_track_change(self):
        source = lyrics.parse_lrc("[00:01]Hola, corazón 🎵\n[00:02]\n[00:03]Last line")
        self.assertTrue(self.open(lyrics.pack(source)))
        for time, expected in (
            (0, -1),
            (999, -1),
            (1000, 0),
            (2999, 1),
            (3000, 2),
            (1500, 0),
            (1500, 0),
            (0, -1),
            (99999, 2),
        ):
            self.assertEqual(
                self.lib.lyrics_current(ctypes.byref(self.timeline), time), expected
            )
        line = Line()
        self.assertTrue(
            self.lib.lyrics_line_at(ctypes.byref(self.timeline), 0, ctypes.byref(line))
        )
        self.assertEqual(
            ctypes.string_at(line.text, line.length).decode(), source.cues[0].text
        )
        self.assertFalse(
            self.lib.lyrics_line_at(ctypes.byref(self.timeline), 3, ctypes.byref(line))
        )
        self.assertFalse(self.open(b"broken new track"))
        self.assertEqual(self.lib.lyrics_current(ctypes.byref(self.timeline), 9999), -1)

    def test_rejects_truncation_overflow_bad_order_bad_utf8(self):
        blob = lyrics.pack(lyrics.parse_lrc("[00:01]One\n[00:02]Two"))
        for size in range(len(blob)):
            self.assertFalse(self.open(blob[:size]))
        for offset, value in (
            (4, 2),
            (8, 0xFFFFFFFF),
            (12, 0xFFFFFFFF),
            (16, 86400001),
            (20, 0xFFFFFFFF),
            (24, 0xFFFFFFFF),
            (28, 1000),
            (32, 0),
        ):
            bad = bytearray(blob)
            struct.pack_into("<I", bad, offset, value)
            self.assertFalse(self.open(bytes(bad)), (offset, value))
        for invalid in (b"\xc0\x80x", b"\xed\xa0\x80", b"\xf0\x80\x80", b"a\x00b"):
            bad = blob[:40] + invalid + blob[43:]
            self.assertFalse(self.open(bad))
        self.assertFalse(self.open(blob + b"trailing"))

    def test_random_seek_matches_reference(self):
        rng = random.Random(42)
        times = sorted(rng.sample(range(100000), 100))
        source = lyrics.Lyrics("synced", tuple(lyrics.Cue(t, "Line") for t in times))
        self.assertTrue(self.open(lyrics.pack(source)))
        for time in [*times, *(rng.randrange(110000) for _ in range(1000))]:
            expected = max((i for i, t in enumerate(times) if t <= time), default=-1)
            self.assertEqual(
                self.lib.lyrics_current(ctypes.byref(self.timeline), time), expected
            )

    def test_timing_adjustment_is_bounded_and_does_not_change_source(self):
        source = lyrics.parse_lrc("[00:00]Zero\n[00:01]One\n[00:02]Two")
        blob = lyrics.pack(source)
        self.assertTrue(self.open(blob))
        times = [0, 1000, 2000]
        for position in (0, 1, 900, 1000, 1100, 1999, 2000, 6000, 0xFFFFFFFF):
            for delay in (-2147483648, -5000, -100, 0, 100, 5000, 2147483647):
                adjusted = position - max(-5000, min(5000, delay))
                expected = max(
                    (i for i, t in enumerate(times) if t <= adjusted), default=-1
                )
                self.assertEqual(
                    self.lib.lyrics_current_adjusted(
                        ctypes.byref(self.timeline), position, delay
                    ),
                    expected,
                )
        self.assertEqual(self.buffer.raw[: len(blob)], blob)

    def test_sanitized_malformed_input_harness(self):
        executable = Path(self.temp.name) / "sanitized"
        subprocess.run(
            [
                "cc",
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-fsanitize=address,undefined",
                str(ROOT / "experiments/lyrics/timeline.c"),
                str(ROOT / "experiments/lyrics/test_timeline.c"),
                "-o",
                str(executable),
            ],
            check=True,
        )
        subprocess.run([str(executable)], check=True)
