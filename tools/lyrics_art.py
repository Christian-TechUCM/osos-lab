#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare an album-colored lyric backdrop on the host, never during playback."""

import io
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from patching.bitmap import encode_bmap
from patching.theme import COLORS

MAX_COVER_BYTES = 8 * 1024 * 1024
MAX_COVER_PIXELS = 16 * 1024 * 1024


def embedded_cover(path):
    import mutagen

    audio = mutagen.File(path)
    if audio is None:
        return None
    tags = audio.tags or {}
    covers = tags.get("covr", [])
    if covers:
        cover = bytes(covers[0])
    elif hasattr(tags, "getall") and tags.getall("APIC"):
        cover = tags.getall("APIC")[0].data
    elif getattr(audio, "pictures", None):
        cover = audio.pictures[0].data
    else:
        return None
    if len(cover) > MAX_COVER_BYTES:
        raise ValueError("embedded cover exceeds 8 MiB")
    return cover


def backdrop(cover):
    """Return one opaque 320x240 RGB image. No network, animation, or device I/O.

    Blur a small image before upscaling. A 70% theme overlay bounds every
    output channel and retains several spatial colors, not a single average.
    """
    from PIL import Image, ImageFilter, ImageOps

    base = Image.new("RGB", (320, 240), COLORS["background"])
    if not cover:
        return base
    if len(cover) > MAX_COVER_BYTES:
        raise ValueError("embedded cover exceeds 8 MiB")
    with Image.open(io.BytesIO(cover)) as source:
        if source.width * source.height > MAX_COVER_PIXELS:
            raise ValueError("embedded cover exceeds 16 megapixels")
        # Flatten transparent artwork against the theme instead of turning
        # transparent pixels black; honor camera/cover orientation first.
        oriented = ImageOps.exif_transpose(source)
        art = ImageOps.fit(oriented.convert("RGBA"), (80, 60))
        flattened = Image.new("RGBA", art.size, (*COLORS["background"], 255))
        flattened.alpha_composite(art)
        blurred = flattened.convert("RGB").filter(ImageFilter.GaussianBlur(9))
        enlarged = blurred.resize(base.size, Image.Resampling.BICUBIC)
        return Image.blend(base, enlarged, 0.30)


def prepare(path):
    """Malformed/missing art falls back without disabling valid song lyrics."""
    from PIL.Image import DecompressionBombError

    warning = None
    try:
        cover = embedded_cover(path)
        image = backdrop(cover)
    except (ValueError, OSError, SyntaxError, DecompressionBombError) as error:
        cover = None
        warning = str(error)
        image = backdrop(None)
    stream = io.BytesIO()
    image.save(stream, format="PNG")
    bitmap = encode_bmap(
        320, 240, image.convert("RGBA").tobytes(), rgb565=True, allow_indexed=False
    )
    return stream.getvalue(), bitmap, bool(cover), warning
