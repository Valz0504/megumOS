#!/usr/bin/env python3
"""
Convert a video into a compact ASCII frame stream for the OS player.

Requirements:
- ffmpeg installed on the host

The output format (little endian):
  struct {
      uint32 magic;      // "ASCI" = 0x41534349
      uint16 width;
      uint16 height;
      uint16 fps;
      uint16 reserved;
      uint32 frame_count;
      uint8  frames[frame_count][width*height]; // raw ASCII bytes, row-major
  }
"""

import argparse
import shutil
import struct
import subprocess
import sys


ASCII_RAMP = b" .:-=+*#%@"
MAGIC = 0x41534349


def luminance_to_ascii_byte(gray: int) -> int:
    # Map 0..255 grayscale to ramp
    idx = (gray * len(ASCII_RAMP)) // 256
    if idx >= len(ASCII_RAMP):
        idx = len(ASCII_RAMP) - 1
    return ASCII_RAMP[idx]


def main() -> int:
    parser = argparse.ArgumentParser(description="Pack video into ASCII frames for the OS player.")
    parser.add_argument("input", help="Input video file (e.g., bad apple.mp4)")
    parser.add_argument(
        "-o",
        "--output-prefix",
        default="bin/badapple",
        help="Output prefix; files are written as <prefix>_NNN.avf (default: bin/badapple)",
    )
    parser.add_argument("--width", type=int, default=64, help="Target text width (<=64). Default: 64")
    parser.add_argument("--height", type=int, default=25, help="Target text height (<=25). Default: 25")
    parser.add_argument("--fps", type=int, default=35, help="Target FPS (default: 35)")
    parser.add_argument(
        "--max-frames",
        type=int,
        default=0,
        help="Total frames to encode (0 = full video). Default: 0",
    )
    parser.add_argument(
        "--max-bytes",
        type=int,
        default=70_000,
        help="Warn if output would exceed this many bytes (default: 70 KB, fits single-indirect ext2 here)",
    )
    args = parser.parse_args()

    if shutil.which("ffmpeg") is None:
        print("ffmpeg not found. Install ffmpeg and retry.", file=sys.stderr)
        return 1

    if args.width > 64 or args.height > 25:
        print("Width/height must fit within 64x25 text grid.", file=sys.stderr)
        return 1

    frame_size = args.width * args.height
    header_bytes = 16  # sizeof struct header packed below
    max_frames_per_chunk = (args.max_bytes - header_bytes) // frame_size
    if max_frames_per_chunk < 1:
        print(
            f"max-bytes budget ({args.max_bytes}) too small for one {args.width}x{args.height} frame. "
            "Increase --max-bytes or reduce resolution.",
            file=sys.stderr,
        )
        return 1
    total_limit = args.max_frames if args.max_frames > 0 else None

    cmd = [
        "ffmpeg",
        "-i",
        args.input,
        "-vf",
        f"scale={args.width}:{args.height},fps={args.fps},format=gray",
        "-f",
        "rawvideo",
        "-",
    ]

    print(f"Running: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=frame_size)
    if proc.stdout is None:
        print("Failed to start ffmpeg", file=sys.stderr)
        return 1

    frames = 0
    chunk_index = 0

    while True:
        chunk_frames = []
        while True:
            if len(chunk_frames) >= max_frames_per_chunk:
                break
            if total_limit is not None and frames >= total_limit:
                break
            chunk = proc.stdout.read(frame_size)
            if not chunk or len(chunk) < frame_size:
                break
            ascii_frame = bytearray(frame_size)
            for i, px in enumerate(chunk):
                ascii_frame[i] = luminance_to_ascii_byte(px)
            chunk_frames.append(ascii_frame)
            frames += 1
        if not chunk_frames:
            break

        fname = f"{args.output_prefix}_{chunk_index:03d}.avf"
        with open(fname, "wb") as outf:
            outf.write(struct.pack("<IHHHHI", MAGIC, args.width, args.height, args.fps, 0, len(chunk_frames)))
            for fr in chunk_frames:
                outf.write(fr)
        total_bytes = header_bytes + len(chunk_frames) * frame_size
        print(f"Wrote chunk {chunk_index:03d}: {len(chunk_frames)} frames -> {fname} ({total_bytes} bytes)")
        if total_bytes > args.max_bytes:
            print(
                f"Warning: chunk {chunk_index:03d} exceeds {args.max_bytes} bytes. "
                f"Reduce width/height or fps.",
                file=sys.stderr,
            )
        chunk_index += 1

        # Stop if overall max_frames reached
        if total_limit is not None and frames >= total_limit:
            break

    proc.stdout.close()
    proc.wait()
    print(f"Total frames written: {frames}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
