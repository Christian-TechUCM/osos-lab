# SPDX-License-Identifier: GPL-3.0-only
import io
import struct
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import lyrics_art
from PIL import Image


def encoded(image):
    stream = io.BytesIO()
    image.save(stream, format="PNG")
    return stream.getvalue()


class ArtworkTests(unittest.TestCase):
    def test_spatial_colors_survive_blur_and_have_bounded_brightness(self):
        cover = Image.new("RGB", (80, 60), "red")
        cover.paste("blue", (40, 0, 80, 60))
        image = lyrics_art.backdrop(encoded(cover))
        self.assertEqual(image.size, (320, 240))
        left, right = image.getpixel((10, 120)), image.getpixel((310, 120))
        self.assertGreater(left[0], left[2])
        self.assertGreater(right[2], right[0])
        for low, high in image.getextrema():
            self.assertGreaterEqual(low, 0)
            self.assertLessEqual(high, 93)

    def test_missing_and_transparent_art_use_theme_background(self):
        expected = (13, 17, 23)
        self.assertEqual(lyrics_art.backdrop(None).getpixel((0, 0)), expected)
        transparent = Image.new("RGBA", (10, 10), (255, 0, 0, 0))
        self.assertEqual(
            lyrics_art.backdrop(encoded(transparent)).getpixel((0, 0)), expected
        )

    def test_bad_art_falls_back_and_native_bitmap_has_exact_dimensions(self):
        with patch.object(lyrics_art, "embedded_cover", return_value=b"not an image"):
            png, bitmap, available, warning = lyrics_art.prepare(Path("synthetic.m4a"))
        self.assertFalse(available)
        self.assertTrue(warning)
        self.assertEqual(Image.open(io.BytesIO(png)).size, (320, 240))
        self.assertEqual(struct.unpack_from("<HHHH", bitmap), (0x565, 0, 640, 16))
        self.assertEqual(len(bitmap), 28 + 320 * 240 * 2)

    def test_decode_limits_fail_before_large_allocation(self):
        with self.assertRaises(ValueError):
            lyrics_art.backdrop(b"x" * (lyrics_art.MAX_COVER_BYTES + 1))
        with patch.object(lyrics_art, "MAX_COVER_PIXELS", 4):
            with self.assertRaises(ValueError):
                lyrics_art.backdrop(encoded(Image.new("RGB", (3, 3))))
        with (
            patch.object(Image, "MAX_IMAGE_PIXELS", 1),
            patch.object(
                lyrics_art,
                "embedded_cover",
                return_value=encoded(Image.new("RGB", (3, 3))),
            ),
        ):
            _, _, available, warning = lyrics_art.prepare(Path("synthetic.m4a"))
        self.assertFalse(available)
        self.assertTrue(warning)
