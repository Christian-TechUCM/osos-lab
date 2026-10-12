# SPDX-License-Identifier: GPL-3.0-only
import unittest

from patching.theme import COLORS, map_pixels


class ThemeTests(unittest.TestCase):
    def test_shared_palette_and_surface_roles(self):
        self.assertEqual(COLORS["background"], (32, 48, 73))
        self.assertEqual(COLORS["ink"], (242, 247, 255))
        self.assertEqual(COLORS["accent"], (57, 208, 255))
        source = bytes((30, 30, 30, 255, 20, 20, 20, 127))
        self.assertEqual(
            map_pixels(source, "dark/background-dark-image.png"),
            bytes((32, 48, 73, 255, 32, 48, 73, 127)),
        )
        self.assertEqual(
            map_pixels(source[:4], "dark/status-bar-background.png"),
            bytes((22, 34, 56, 255)),
        )

    def test_neutral_tones_accent_alpha_and_semantic_colors(self):
        pixels = bytes(
            (
                255,
                255,
                255,
                89,
                50,
                200,
                255,
                123,
                248,
                81,
                73,
                255,
                30,
                160,
                40,
                255,
                250,
                240,
                200,
                0,
            )
        )
        mapped = map_pixels(pixels, "icon.png")
        self.assertEqual(mapped[:8], bytes((242, 247, 255, 89, 57, 208, 255, 123)))
        self.assertEqual(mapped[8:], pixels[8:])
        with self.assertRaises(ValueError):
            map_pixels(b"bad", "icon.png")
