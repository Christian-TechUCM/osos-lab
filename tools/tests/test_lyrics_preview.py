# SPDX-License-Identifier: GPL-3.0-only
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from lyrics_preview import byte_range


class RangeTests(unittest.TestCase):
    def test_audio_range_requests(self):
        for header, expected in (
            (None, (0, 100)),
            ("bytes=0-1", (0, 2)),
            ("bytes=80-", (80, 100)),
            ("bytes=-10", (90, 100)),
            ("bytes=-500", (0, 100)),
            ("bytes=90-200", (90, 100)),
        ):
            self.assertEqual(byte_range(header, 100), expected)

    def test_bad_ranges_are_rejected(self):
        for header in (
            "bytes=100-",
            "bytes=20-10",
            "bytes=-0",
            "bytes=-",
            "bytes=0-1,5-6",
            "bad",
        ):
            with self.subTest(header=header), self.assertRaises(ValueError):
                byte_range(header, 100)
