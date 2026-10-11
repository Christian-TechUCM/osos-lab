# SPDX-License-Identifier: GPL-3.0-only
"""Deterministic native bitmap palette mapping; source PNGs remain unchanged."""

import re
from pathlib import Path

HEADER = Path(__file__).resolve().parents[1] / "payload/theme.h"
COLORS = {
    name.lower(): tuple(bytes.fromhex(rgb))
    for name, rgb in re.findall(
        r"#define CFW_THEME_([A-Z]+) 0x([0-9a-f]{6})u", HEADER.read_text()
    )
}


def map_pixels(pixels, filename):
    if len(pixels) % 4:
        raise ValueError("Expected RGBA pixels")
    output = bytearray(pixels)
    surface = {
        "background-dark-image.png": COLORS["background"],
        "status-bar-background.png": COLORS["sidebar"],
    }.get(Path(filename).name)
    for start in range(0, len(pixels), 4):
        r, g, b, alpha = pixels[start : start + 4]
        if not alpha:
            continue
        if surface:
            color = surface
        elif max(r, g, b) - min(r, g, b) <= 8:
            # Preserve neutral shading and antialiasing, including its alpha.
            level = (r + g + b) // 3
            color = tuple(
                low + (high - low) * level // 255
                for low, high in zip(COLORS["background"], COLORS["ink"])
            )
        elif b > r + 20 and b >= g - 10:
            # Existing blue/cyan UI icons become the requested accent. Do not
            # recolor red/green semantic indicators or arbitrary album art.
            color = tuple(c * max(r, g, b) // 255 for c in COLORS["accent"])
        else:
            continue
        output[start : start + 3] = bytes(color)
    return bytes(output)
