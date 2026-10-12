# SPDX-License-Identifier: GPL-3.0-only
"""Embedded-tag firmware parser, compared with the independent host parser."""

import ctypes
import random
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import lyrics

ROOT = Path(__file__).resolve().parents[2]


class Cue(ctypes.Structure):
    _fields_ = [
        ("milliseconds", ctypes.c_uint32),
        ("offset", ctypes.c_uint16),
        ("length", ctypes.c_uint16),
    ]


class Lrc(ctypes.Structure):
    _fields_ = [
        ("text", ctypes.c_void_p),
        ("count", ctypes.c_uint32),
        ("cues", Cue * 1024),
    ]


@unittest.skipUnless(shutil.which("cc"), "Host C compiler required")
class EmbeddedNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="native-lrc-")
        library = Path(cls.temp.name) / "lrc.so"
        subprocess.run(
            [
                "cc",
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-shared",
                "-fPIC",
                str(ROOT / "payload/lyrics_lrc.c"),
                "-o",
                str(library),
            ],
            check=True,
        )
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.cfw_lrc_parse.argtypes = [
            ctypes.POINTER(Lrc),
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        cls.lib.cfw_lrc_parse.restype = ctypes.c_int
        cls.lib.cfw_lrc_current.argtypes = [
            ctypes.POINTER(Lrc),
            ctypes.c_uint32,
            ctypes.c_int,
        ]
        cls.lib.cfw_lrc_current.restype = ctypes.c_int
        cls.lib.cfw_lrc_copy.argtypes = [
            ctypes.POINTER(Lrc),
            ctypes.c_uint,
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        cls.lib.cfw_lrc_copy.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def parse(self, text):
        raw = text.encode() if isinstance(text, str) else text
        self.buffer = ctypes.create_string_buffer(raw)
        self.lrc = Lrc()
        return self.lib.cfw_lrc_parse(ctypes.byref(self.lrc), self.buffer, len(raw))

    def check_track(self, text):
        expected = lyrics.parse_lrc(text)
        self.assertEqual(expected.mode, "synced")
        self.assertTrue(self.parse(text))
        output = ctypes.create_string_buffer(2049)
        for cue in expected.cues:
            index = self.lib.cfw_lrc_current(ctypes.byref(self.lrc), cue.start_ms, 0)
            self.assertGreaterEqual(index, 0)
            self.assertTrue(
                self.lib.cfw_lrc_copy(
                    ctypes.byref(self.lrc), index, output, len(output)
                )
            )
            self.assertEqual(output.value.decode(), cue.text)

    def test_host_interoperability(self):
        for text in (
            "[00:01.1][00:03.12]Chorus\n[00:02.123]Verse",
            "\ufeff[ar:Example]\r\n[00:00.2]First\r\n[00:00.2]Second\n[00:01.2]\n[offset:-500]",
            "[00:123.45]Line\n[00:124.55]Next",
            "[00:01.00]<00:01.00>Hola, <00:01.50>corazón 🎵",
            "[00:02]最後の行\n[00:01]مرحبا",
            "[OFFSET:+100]\n[00:00]Zero\n[00:01]One",
            "[00:01][00:01]Repeated stamp\n[00:02]After",
        ):
            with self.subTest(text=text):
                self.check_track(text)

    def test_unsorted_random_timestamps(self):
        rng = random.Random(91)
        for _ in range(30):
            times = rng.sample(range(5000), 100)
            self.check_track(
                "\n".join(f"[00:{time / 1000:06.3f}]Line {time}" for time in times)
            )

    def test_seek_pause_and_timing_adjustment(self):
        self.assertTrue(self.parse("[00:00]Zero\n[00:01]One\n[00:02]Two"))
        for position, delay, expected in (
            (0, 100, -1),
            (1000, 100, 0),
            (1100, 100, 1),
            (900, -100, 1),
            (1100, 100, 1),
            (500, 0, 0),
            (0, -2147483648, 2),
            (0, 2147483647, -1),
            (0xFFFFFFFF, -5000, 2),
        ):
            self.assertEqual(
                self.lib.cfw_lrc_current(ctypes.byref(self.lrc), position, delay),
                expected,
            )

    def test_invalid_static_and_limits(self):
        for text in (
            "",
            "Plain lyrics",
            "[00:01]Timed\nUntimed",
            "[00:01.1234]Bad",
            "[00:xx]Bad",
            "[offset:1]\n[offset:2]\n[00:01]Bad",
            "[offset:999999999999999]\n[00:01]Bad",
            "[9999:00]Far away",
            "[00:01]" + "é" * 1025,
            "[00:01]Zero\0byte",
            "\n".join(f"[00:{n:02d}]Line" for n in range(1025)),
            b"[00:01]\xc0\x80",
            b"[00:01]\xed\xa0\x80",
            b"[00:01]\xf4\x90\x80\x80",
            b"[00:01]\xe2",
            "x" * 32769,
            "[00:01]Timed\u2028Untimed",
            "[00:01]Timed\u2029Untimed",
            "[00:01]Timed\x85Untimed",
        ):
            with self.subTest(text=str(text)[:60]):
                self.assertFalse(self.parse(text))
                self.assertEqual(self.lrc.count, 0)
                self.assertFalse(self.lrc.text)

    def test_copy_fails_without_partial_utf8_or_stale_text(self):
        self.assertTrue(self.parse("[00:01]éé\n[00:01]Second"))
        out = ctypes.create_string_buffer(4)
        self.assertFalse(
            self.lib.cfw_lrc_copy(ctypes.byref(self.lrc), 0, out, len(out))
        )
        self.assertEqual(out.value, b"")
        self.assertFalse(
            self.lib.cfw_lrc_copy(ctypes.byref(self.lrc), 99, out, len(out))
        )
