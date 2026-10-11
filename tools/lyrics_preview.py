#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Serve the lyrics prototype on loopback with one explicitly selected audio file."""

import argparse
import json
import re
from dataclasses import asdict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

import lyrics

ASSETS = Path(__file__).resolve().parents[1] / "experiments/lyrics"
ROUTES = {
    "/": ("preview.html", "text/html; charset=utf-8"),
    "/preview.css": ("preview.css", "text/css; charset=utf-8"),
    "/preview.mjs": ("preview.mjs", "text/javascript; charset=utf-8"),
    "/engine.mjs": ("engine.mjs", "text/javascript; charset=utf-8"),
}


def byte_range(header, size):
    if header is None:
        return 0, size
    match = re.fullmatch(r"bytes=(\d*)-(\d*)", header)
    if not match or not size or not any(match.groups()):
        raise ValueError("unsupported range")
    first, last = match.groups()
    if not first:
        length = int(last)
        if length == 0:
            raise ValueError("empty range")
        return max(0, size - length), size
    start = int(first)
    end = min(size, int(last) + 1) if last else size
    if start >= size or end <= start:
        raise ValueError("range outside file")
    return start, end


def handler(audio_path=None, track_data=None, background=None):
    class PreviewHandler(BaseHTTPRequestHandler):
        def do_GET(self):
            self.respond(body=True)

        def do_HEAD(self):
            self.respond(body=False)

        def respond(self, body):
            path = urlsplit(self.path).path
            source = None
            if path in ROUTES:
                name, content_type = ROUTES[path]
                data = (ASSETS / name).read_bytes()
            elif path == "/track.json" and track_data is not None:
                data = json.dumps(track_data, ensure_ascii=False).encode("utf-8")
                content_type = "application/json; charset=utf-8"
            elif path == "/background.png" and background is not None:
                data = background
                content_type = "image/png"
            elif path == "/audio" and audio_path is not None:
                source = audio_path
                data = b""
                content_type = {
                    ".mp3": "audio/mpeg",
                    ".flac": "audio/flac",
                    ".wav": "audio/wav",
                }.get(source.suffix.lower(), "audio/mp4")
            else:
                self.send_error(404)
                return
            size = source.stat().st_size if source else len(data)
            requested = self.headers.get("Range")
            try:
                start, end = byte_range(requested, size)
            except ValueError:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{size}")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            self.send_response(206 if requested else 200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(end - start))
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header(
                "Content-Security-Policy",
                "default-src 'self'; media-src 'self' blob:; style-src 'self' 'unsafe-inline'; base-uri 'none'; frame-ancestors 'none'",
            )
            if requested:
                self.send_header("Content-Range", f"bytes {start}-{end - 1}/{size}")
            self.end_headers()
            if not body:
                return
            try:
                if source:
                    with source.open("rb") as stream:
                        stream.seek(start)
                        remaining = end - start
                        while remaining:
                            chunk = stream.read(min(65536, remaining))
                            if not chunk:
                                break
                            self.wfile.write(chunk)
                            remaining -= len(chunk)
                else:
                    self.wfile.write(data[start:end])
            except (BrokenPipeError, ConnectionResetError):
                pass  # Seeking often cancels an earlier browser range request.

    return PreviewHandler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audio", type=Path)
    parser.add_argument(
        "--preview-audio",
        type=Path,
        help="Optional browser-compatible copy of the same recording",
    )
    parser.add_argument("--port", type=int, default=8766)
    parser.add_argument(
        "--artwork",
        action="store_true",
        help="Prepare a static blurred cover-art backdrop on this Mac",
    )
    args = parser.parse_args()
    track_data = None
    background = None
    if args.audio:
        metadata, parsed = lyrics.read_track(args.audio)
        track_data = {"schema": 1, "track": metadata, **asdict(parsed)}
        if args.artwork:
            from lyrics_art import prepare

            background, _, available, warning = prepare(args.audio)
            track_data["background"] = {"album_art": available, "warning": warning}
    if args.preview_audio:
        if not args.audio:
            parser.error("--preview-audio requires the original --audio for lyrics")
        import mutagen

        preview_audio = mutagen.File(args.preview_audio)
        if (
            preview_audio is None
            or abs(preview_audio.info.length * 1000 - metadata["duration_ms"]) > 100
        ):
            parser.error("preview copy must match the original recording duration")
    server = ThreadingHTTPServer(
        ("127.0.0.1", args.port),
        handler(args.preview_audio or args.audio, track_data, background),
    )
    print(f"Lyrics preview: http://127.0.0.1:{server.server_port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
