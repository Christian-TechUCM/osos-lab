/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

/* GitHub Dark palette, from DexThemes. Also read by patching/theme.py. */
#define CFW_THEME_BACKGROUND 0x0d1117u
#define CFW_THEME_SIDEBAR 0x090c10u
#define CFW_THEME_INK 0xe6edf3u
#define CFW_THEME_ACCENT 0x58a6ffu
/* Supporting roles adapted for readable small-screen selection and borders. */
#define CFW_THEME_SELECTION 0x17345bu
#define CFW_THEME_BORDER 0x30363du

#define CFW_THEME_RED(rgb) (((rgb) >> 16) & 255)
#define CFW_THEME_GREEN(rgb) (((rgb) >> 8) & 255)
#define CFW_THEME_BLUE(rgb) ((rgb) & 255)
#define CFW_THEME_RGB565(rgb) \
    (((CFW_THEME_RED(rgb) >> 3) << 11) | \
     ((CFW_THEME_GREEN(rgb) >> 2) << 5) | (CFW_THEME_BLUE(rgb) >> 3))
