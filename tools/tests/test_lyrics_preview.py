# SPDX-License-Identifier: GPL-3.0-only
import io
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from lyrics_preview import byte_range, handler


class BackgroundRouteTests(unittest.TestCase):
    def request(self, path, background=None, body=True):
        request = object.__new__(handler(background=background))
        request.path = path
        request.headers = {}
        request.wfile = io.BytesIO()
        for name in ("send_response", "send_header", "end_headers", "send_error"):
            setattr(request, name, Mock())
        request.respond(body)
        return request

    def test_background_get_and_head_are_bounded_to_prepared_bytes(self):
        for body in (True, False):
            request = self.request("/background.png", b"prepared-image", body)
            request.send_response.assert_called_once_with(200)
            request.send_header.assert_any_call("Content-Type", "image/png")
            request.send_header.assert_any_call("Content-Length", "14")
            self.assertEqual(
                request.wfile.getvalue(), b"prepared-image" if body else b""
            )

    def test_missing_background_and_arbitrary_paths_are_not_served(self):
        for path, background in (
            ("/background.png", None),
            ("/../background.png", b"prepared-image"),
            ("/some-song.background.png", b"prepared-image"),
        ):
            request = self.request(path, background)
            request.send_error.assert_called_once_with(404)
            self.assertEqual(request.wfile.getvalue(), b"")


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
