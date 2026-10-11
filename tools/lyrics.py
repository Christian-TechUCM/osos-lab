#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read embedded lyrics without changing audio; export bounded, timed sidecars.

Optional metadata dependency: pip install -r experiments/lyrics/requirements.txt
Only the audit/export commands need Mutagen. Parsing/packing use the stdlib.
"""

import argparse
import hashlib
import json
import re
import struct
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path

MAX_FILE = 128 * 1024
MAX_CUES = 1024
MAX_LINE = 2048
MAX_TIME = 24 * 60 * 60 * 1000
STAMP = re.compile(r"\[(\d{1,4}):(\d{2,5})(?:\.(\d{1,3}))?\]")
WORD_STAMP = re.compile(r"<\d{1,4}:[0-5]\d(?:\.\d{1,3})?>")
META = re.compile(r"\[(?:ar|al|ti|au|by|re|ve|length|id):[^\]]*\]", re.I)
OFFSET = re.compile(r"\[offset:([+-]?\d+)\]", re.I)
AUDIO = {".m4a", ".mp4", ".mp3", ".flac"}


@dataclass(frozen=True)
class Cue:
    start_ms: int
    text: str


@dataclass(frozen=True)
class Lyrics:
    mode: str
    cues: tuple[Cue, ...] = ()
    text: str = ""
    has_word_timing: bool = False


def parse_lrc(text):
    """Normalize line timing, preserving empty cues as instrumental breaks.

    Static lyrics remain static. Malformed/mixed timing is rejected rather than
    guessing when a line should appear. Offsets apply to all cues, even when the
    tag follows the lyrics. Simultaneous lines share a cue in source order.
    Seconds may overflow (some tags use [00:123.45] for 123.45 seconds).
    """
    if len(text.encode("utf-8")) > MAX_FILE:
        raise ValueError("lyrics tag exceeds 128 KiB")
    lines = text.lstrip("\ufeff").splitlines()
    offsets = [int(m[1]) for line in lines if (m := OFFSET.fullmatch(line.strip()))]
    if len(offsets) > 1 or (offsets and abs(offsets[0]) > MAX_TIME):
        raise ValueError("invalid or repeated offset tag")
    offset = offsets[0] if offsets else 0
    entries, untimed = [], []
    has_word_timing = False
    for raw in lines:
        line = raw.strip()
        if not line or META.fullmatch(line) or OFFSET.fullmatch(line):
            continue
        stamps = []
        while match := STAMP.match(line):
            minutes, seconds, fraction = match.groups()
            time = int(minutes) * 60000 + int(seconds) * 1000
            time += int((fraction or "").ljust(3, "0"))
            if time > MAX_TIME:
                raise ValueError("timestamp exceeds 24 hours")
            stamps.append(max(0, min(MAX_TIME, time + offset)))
            line = line[match.end() :].lstrip()
        if not stamps:
            if re.match(r"\[\d", line):
                raise ValueError("malformed timestamp")
            untimed.append(line)
            continue
        has_word_timing |= bool(WORD_STAMP.search(line))
        line = WORD_STAMP.sub("", line).strip()
        if len(line.encode("utf-8")) > MAX_LINE:
            raise ValueError("lyric line exceeds 2048 UTF-8 bytes")
        entries.extend((time, line) for time in stamps)
        if len(entries) > MAX_CUES:
            raise ValueError("lyrics exceed 1024 cues")
    if entries and untimed:
        raise ValueError("mixed timed and untimed lyric lines")
    if not entries:
        return Lyrics("static" if untimed else "missing", text="\n".join(untimed))
    grouped = {}
    for time, line in sorted(entries, key=lambda entry: entry[0]):
        group = grouped.setdefault(time, [])
        if line not in group:
            group.append(line)
    cues = tuple(Cue(time, "\n".join(group)) for time, group in grouped.items())
    result = Lyrics("synced", cues, has_word_timing=has_word_timing)
    pack(result)  # Enforce combined-line and whole-file bounds as well.
    return result


def pack(lyrics):
    """LRCS v1: 16-byte header, 12-byte cue records, length-delimited UTF-8."""
    if lyrics.mode != "synced" or not 0 < len(lyrics.cues) <= MAX_CUES:
        raise ValueError("binary sidecars require 1–1024 timed cues")
    records, text = bytearray(), bytearray()
    previous = -1
    for cue in lyrics.cues:
        data = cue.text.encode("utf-8")
        if any(ord(char) < 32 and char not in "\n\t" for char in cue.text):
            raise ValueError("unsupported control character in lyric text")
        if not previous < cue.start_ms <= MAX_TIME or len(data) > MAX_LINE:
            raise ValueError("invalid cue order, timestamp or line length")
        records.extend(struct.pack("<III", cue.start_ms, len(text), len(data)))
        text.extend(data)
        previous = cue.start_ms
    header = struct.pack("<4sHHII", b"LRCS", 1, 0, len(lyrics.cues), len(text))
    result = header + records + text
    if len(result) > MAX_FILE:
        raise ValueError("sidecar exceeds 128 KiB")
    return result


def read_track(path):
    import mutagen

    audio = mutagen.File(path)
    if audio is None:
        raise ValueError("unsupported audio format")
    tags = audio.tags or {}

    def first(*keys):
        for key in keys:
            value = tags.get(key)
            if value:
                if hasattr(value, "text"):
                    value = value.text
                if isinstance(value, (list, tuple)):
                    return str(value[0]) if value else ""
                return str(value)
        return ""

    # LYRICIST identifies the writer; it is deliberately not a lyric-text tag.
    lyrics = first("\xa9lyr", "lyrics", "LYRICS", "unsyncedlyrics", "UNSYNCEDLYRICS")
    if not lyrics and hasattr(tags, "getall"):
        uslt = tags.getall("USLT")
        if uslt:
            lyrics = uslt[0].text
    metadata = {
        "title": first("\xa9nam", "TIT2", "title", "TITLE") or path.stem,
        "artist": first("\xa9ART", "TPE1", "artist", "ARTIST"),
        "album": first("\xa9alb", "TALB", "album", "ALBUM"),
        "duration_ms": round(audio.info.length * 1000),
        "audio_filename": path.name,
    }
    return metadata, parse_lrc(lyrics)


def export_track(path, destination, artwork=False):
    metadata, lyrics = read_track(path)
    # This identifies a local export, not an iTunesDB persistent ID. Track binding
    # on the iPod is a separate integration step; do not guess based on filenames.
    digest = hashlib.sha256(str(path.resolve()).encode()).hexdigest()[:16]
    data = {"schema": 1, "track": metadata, **asdict(lyrics)}
    destination.mkdir(parents=True, exist_ok=True)
    if artwork:
        from lyrics_art import prepare

        png, bitmap, available, warning = prepare(path)
        (destination / f"{digest}.background.png").write_bytes(png)
        (destination / f"{digest}.background.bmap").write_bytes(bitmap)
        data["background"] = {
            "png": f"{digest}.background.png",
            "bmap": f"{digest}.background.bmap",
            "album_art": available,
            "warning": warning,
        }
    (destination / f"{digest}.json").write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    if lyrics.mode == "synced":
        (destination / f"{digest}.lrcs").write_bytes(pack(lyrics))
    return digest, lyrics.mode


def audit(root):
    counts, formats, failures = Counter(), Counter(), []
    timing_warnings = []
    max_cues = max_bytes = 0
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in AUDIO:
            continue
        formats[path.suffix.lower()] += 1
        counts["total"] += 1
        try:
            metadata, lyrics = read_track(path)
            counts[lyrics.mode] += 1
            if lyrics.has_word_timing:
                counts["with_word_timing"] += 1
            if lyrics.mode == "synced":
                max_cues = max(max_cues, len(lyrics.cues))
                max_bytes = max(max_bytes, len(pack(lyrics)))
                if lyrics.cues[-1].start_ms > metadata["duration_ms"]:
                    timing_warnings.append(
                        {
                            "file": str(path.relative_to(root)),
                            "duration_ms": metadata["duration_ms"],
                            "last_cue_ms": lyrics.cues[-1].start_ms,
                            "beyond_end_ms": lyrics.cues[-1].start_ms
                            - metadata["duration_ms"],
                        }
                    )
        except Exception as error:
            counts["errors"] += 1
            failures.append({"file": str(path.relative_to(root)), "error": str(error)})
    return {
        "counts": dict(counts),
        "formats": dict(formats),
        "largest_cue_count": max_cues,
        "largest_sidecar_bytes": max_bytes,
        "failures": failures,
        "timing_warnings": timing_warnings,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    scan = commands.add_parser("audit")
    scan.add_argument("library", type=Path)
    scan.add_argument("--out", type=Path)
    export = commands.add_parser("export")
    export.add_argument("audio", type=Path)
    export.add_argument("--out", type=Path, default=Path(".build/lyrics-private"))
    export.add_argument(
        "--artwork",
        action="store_true",
        help="Prepare an album-colored PNG and native RGB565 BMap",
    )
    args = parser.parse_args()
    if args.command == "audit":
        if not args.library.is_dir():
            parser.error("library must be an existing directory")
        result = json.dumps(audit(args.library), ensure_ascii=False, indent=2) + "\n"
        if args.out:
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(result, encoding="utf-8")
        print(result, end="")
    else:
        key, mode = export_track(args.audio, args.out, args.artwork)
        print(json.dumps({"export": str(args.out / f"{key}.json"), "mode": mode}))


if __name__ == "__main__":
    main()
