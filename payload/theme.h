/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

/* Afterburner-inspired: use its lighter navigation navy as our main surface.
 * Also read by patching/theme.py. See docs/lyrics-native.md for source. */
#define CFW_THEME_BACKGROUND 0x203049u
#define CFW_THEME_SIDEBAR 0x162238u
#define CFW_THEME_INK 0xf2f7ffu
#define CFW_THEME_ACCENT 0x39d0ffu
/* Supporting roles adapted for readable small-screen selection and borders. */
#define CFW_THEME_SELECTION 0x244058u
#define CFW_THEME_BORDER 0x354762u

#define CFW_THEME_RED(rgb) (((rgb) >> 16) & 255)
#define CFW_THEME_GREEN(rgb) (((rgb) >> 8) & 255)
#define CFW_THEME_BLUE(rgb) ((rgb) & 255)
#define CFW_THEME_RGB565(rgb) \
    (((CFW_THEME_RED(rgb) >> 3) << 11) | \
     ((CFW_THEME_GREEN(rgb) >> 2) << 5) | (CFW_THEME_BLUE(rgb) >> 3))
