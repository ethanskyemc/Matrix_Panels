#!/usr/bin/env python3
"""Convert PNG, JPG, or animated GIF into compact 16x16 .mpx format for Matrix Acrylic Display."""

import argparse
import struct
import sys

try:
    from PIL import Image, ImageEnhance, ImageSequence
except ImportError:
    print("Pillow is required: pip install pillow", file=sys.stderr)
    sys.exit(1)


def center_crop_square(im: Image.Image) -> Image.Image:
    w, h = im.size
    side = min(w, h)
    left = (w - side) // 2
    top = (h - side) // 2
    return im.crop((left, top, left + side, top + side))


def convert(in_path: str, out_path: str, mode: str = "area", contrast: float = 1.15, sat: float = 1.25, max_frames: int = 100):
    img = Image.open(in_path)
    resample = Image.Resampling.NEAREST if mode == "nearest" else Image.Resampling.LANCZOS

    frames = []
    for frame in ImageSequence.Iterator(img):
        delay = int(frame.info.get("duration", 100)) or 100
        rgb = center_crop_square(frame.convert("RGB")).resize((16, 16), resample=resample)
        if contrast != 1.0:
            rgb = ImageEnhance.Contrast(rgb).enhance(contrast)
        if sat != 1.0:
            rgb = ImageEnhance.Color(rgb).enhance(sat)
        frames.append((max(25, min(5000, delay)), rgb.tobytes()))
        if len(frames) >= max_frames:
            break

    with open(out_path, "wb") as f:
        default_delay = frames[0][0] if frames else 100
        f.write(b"MPX1")
        f.write(struct.pack("<BBHH", 16, 16, len(frames), default_delay))
        for delay, data in frames:
            f.write(struct.pack("<H", delay))
            f.write(data)

    print(f"Wrote {out_path} ({len(frames)} frame(s), {10 + len(frames) * 770} bytes)")


def main():
    p = argparse.ArgumentParser(description="Convert image or GIF to 16x16 .mpx")
    p.add_argument("input", help="Input PNG/JPG/GIF path")
    p.add_argument("output", help="Output .mpx path")
    p.add_argument("--filter", choices=["area", "nearest"], default="area")
    p.add_argument("--contrast", type=float, default=1.15)
    p.add_argument("--saturation", type=float, default=1.25)
    args = p.parse_args()
    convert(args.input, args.output, args.filter, args.contrast, args.saturation)


if __name__ == "__main__":
    main()
