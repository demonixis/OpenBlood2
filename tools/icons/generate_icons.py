#!/usr/bin/env python3
"""Regenerate every OpenBlood 2 application icon from one procedural design.

The artwork is project-owned (no original Blood II asset is used) and drawn
with signed distances, so the script needs only the Python standard library.
Outputs are checked in; run it again after changing the design:

    python3 tools/icons/generate_icons.py

Writes:
  platform/icons/blood2.png                                   512 px preview
  platform/windows/blood2.ico                                 16..256 px
  platform/apple/macos/blood2.icns                            16..1024 px
  platform/apple/ios/Assets.xcassets/AppIcon.appiconset/      1024 px, opaque
  platform/android/app/src/main/res/mipmap-*/                 adaptive foreground
"""

import json
import math
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLATFORM = ROOT / "platform"
MASTER = 1024

# Palette (linear blending is unnecessary at this level of detail).
BACKGROUND_INNER = (74, 12, 12)
BACKGROUND_OUTER = (16, 4, 4)
DROP_TOP = (226, 30, 30)
DROP_BOTTOM = (118, 6, 6)
BONE = (236, 224, 202)
ANDROID_BACKGROUND = "#2A0707"


def clamp(value, low=0.0, high=1.0):
    return low if value < low else high if value > high else value


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def coverage(distance, pixel):
    """Anti-aliased coverage of a signed distance (negative inside)."""
    return clamp(0.5 - distance / pixel)


def rounded_box(x, y, cx, cy, half, radius):
    qx = abs(x - cx) - half + radius
    qy = abs(y - cy) - half + radius
    outside = math.hypot(max(qx, 0.0), max(qy, 0.0))
    return outside + min(max(qx, qy), 0.0) - radius


class Drop:
    """A blood drop: a circle joined to a tip by its two tangent lines."""

    def __init__(self, cx, cy, radius, tip):
        self.cx, self.cy, self.r = cx, cy, radius
        self.tip = cy - tip
        d = cy - self.tip
        self.sin = radius / d
        self.cos = math.sqrt(1.0 - self.sin * self.sin)
        self.tangent_y = self.tip + (d * d - radius * radius) / d

    def distance(self, x, y):
        circle = math.hypot(x - self.cx, y - self.cy) - self.r
        if y >= self.tangent_y:
            return circle
        if y <= self.tip:
            return min(circle, math.hypot(x - self.cx, y - self.tip))
        slope = self.sin / self.cos
        cone = (abs(x - self.cx) - (y - self.tip) * slope) * self.cos
        return min(circle, cone)

    def colour(self, y):
        top, bottom = self.tip, self.cy + self.r
        return mix(DROP_TOP, DROP_BOTTOM, clamp((y - top) / (bottom - top)))


def blend(dst, colour, alpha):
    a = alpha + dst[3] * (1.0 - alpha)
    if a <= 0.0:
        return (0.0, 0.0, 0.0, 0.0)
    rgb = tuple((colour[i] * alpha + dst[i] * dst[3] * (1.0 - alpha)) / a for i in range(3))
    return (*rgb, a)


def render(size, plate, drop, background):
    """Render RGBA rows. plate: (half, radius) of the rounded square or None
    for a full-bleed tile; background: draw the dark plate/tile."""
    pixel = 1.0 / size
    rows = []
    for py in range(size):
        y = (py + 0.5) * pixel
        row = bytearray()
        for px in range(size):
            x = (px + 0.5) * pixel
            out = (0.0, 0.0, 0.0, 0.0)
            if background:
                if plate:
                    half, radius = plate
                    # Soft drop shadow below the plate (macOS/Windows style).
                    shadow = rounded_box(x, y - 0.012, 0.5, 0.5, half, radius)
                    out = blend(out, (0, 0, 0), 0.30 * clamp(1.0 - (shadow + 0.004) / 0.03))
                    plate_alpha = coverage(rounded_box(x, y, 0.5, 0.5, half, radius), pixel)
                else:
                    plate_alpha = 1.0
                if plate_alpha > 0.0:
                    t = clamp(math.hypot(x - 0.5, y - 0.42) / 0.62)
                    out = blend(out, mix(BACKGROUND_INNER, BACKGROUND_OUTER, t), plate_alpha)
            d = drop.distance(x, y)
            # Dark rim keeps the drop readable on light taskbars.
            rim = coverage(d - 0.010 * drop.r / 0.25, pixel)
            if rim > 0.0:
                out = blend(out, (40, 0, 0), rim * 0.85)
            body = coverage(d, pixel)
            if body > 0.0:
                out = blend(out, drop.colour(y), body)
                hx, hy = drop.cx - drop.r * 0.38, drop.cy - drop.r * 0.30
                glint = math.hypot((x - hx) / 0.55, (y - hy) / 1.0) - drop.r * 0.20
                out = blend(out, (255, 236, 236), body * 0.55 * coverage(glint, pixel * 2.0))
            row += bytes((round(out[0]), round(out[1]), round(out[2]), round(out[3] * 255)))
        rows.append(row)
    return rows


def downsample(rows, size):
    """Box-filter a square RGBA master to a smaller square (premultiplied)."""
    source = len(rows)
    if size == source:
        return rows
    scale = source / size
    result = []
    for py in range(size):
        y0, y1 = int(py * scale), max(int(py * scale) + 1, int((py + 1) * scale))
        row = bytearray()
        for px in range(size):
            x0, x1 = int(px * scale), max(int(px * scale) + 1, int((px + 1) * scale))
            r = g = b = a = 0
            for sy in range(y0, y1):
                line = rows[sy]
                for sx in range(x0, x1):
                    o = sx * 4
                    alpha = line[o + 3]
                    r += line[o] * alpha
                    g += line[o + 1] * alpha
                    b += line[o + 2] * alpha
                    a += alpha
            count = (y1 - y0) * (x1 - x0)
            if a:
                row += bytes((round(r / a), round(g / a), round(b / a), round(a / count)))
            else:
                row += bytes(4)
        result.append(row)
    return result


def opaque(rows):
    return [bytes(c for i, c in enumerate(row) if i % 4 != 3) for row in rows]


def png(rows, channels=4):
    size = len(rows)
    raw = b"".join(b"\x00" + bytes(row) for row in rows)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    colour_type = 6 if channels == 4 else 2
    header = struct.pack(">IIBBBBB", size, size, 8, colour_type, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def ico(images):
    """images: {size: png bytes}; PNG-compressed entries (Windows Vista+)."""
    sizes = sorted(images)
    header = struct.pack("<HHH", 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    entries, data = b"", b""
    for size in sizes:
        blob = images[size]
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    return header + entries + data


def icns(images):
    """images: {size: png bytes}; every entry PNG (macOS 10.7+)."""
    kinds = [(b"icp4", 16), (b"icp5", 32), (b"icp6", 64), (b"ic07", 128), (b"ic08", 256),
             (b"ic09", 512), (b"ic10", 1024), (b"ic11", 32), (b"ic12", 64), (b"ic13", 512), (b"ic14", 1024)]
    body = b"".join(kind + struct.pack(">I", len(images[size]) + 8) + images[size] for kind, size in kinds)
    return b"icns" + struct.pack(">I", len(body) + 8) + body


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    print(path.relative_to(ROOT))


def main():
    game_drop = Drop(0.5, 0.585, 0.215, 0.43)
    # macOS grid: an 824 px plate in a 1024 px canvas; Windows reuses it.
    plate = (0.4023, 0.0898 * 2)

    master = render(MASTER, plate, game_drop, True)
    sizes = {s: png(downsample(master, s)) for s in (16, 24, 32, 48, 64, 128, 256, 512, 1024)}
    write(PLATFORM / "icons" / "blood2.png", sizes[512])
    write(PLATFORM / "windows" / "blood2.ico", ico({s: sizes[s] for s in (16, 24, 32, 48, 64, 128, 256)}))
    write(PLATFORM / "apple" / "macos" / "blood2.icns", icns(sizes))

    # iOS masks the corners itself and rejects transparency: full bleed, opaque.
    catalog = PLATFORM / "apple" / "ios" / "Assets.xcassets"
    ios = render(MASTER, None, game_drop, True)
    write(catalog / "AppIcon.appiconset" / "AppIcon-1024.png", png(opaque(ios), 3))
    write(catalog / "Contents.json", (json.dumps({"info": {"author": "xcode", "version": 1}}, indent=2) + "\n").encode())
    write(catalog / "AppIcon.appiconset" / "Contents.json", (json.dumps({
        "images": [{"filename": "AppIcon-1024.png", "idiom": "universal", "platform": "ios", "size": "1024x1024"}],
        "info": {"author": "xcode", "version": 1}}, indent=2) + "\n").encode())

    # Android adaptive icon: 108 dp foreground, content inside the 66 dp safe circle.
    res = PLATFORM / "android" / "app" / "src" / "main" / "res"
    foreground = render(MASTER, None, Drop(0.5, 0.56, 0.135, 0.27), False)
    for density, size in (("mdpi", 108), ("hdpi", 162), ("xhdpi", 216), ("xxhdpi", 324), ("xxxhdpi", 432)):
        write(res / f"mipmap-{density}" / "ic_launcher_foreground.png", png(downsample(foreground, size)))
    write(res / "values" / "ic_launcher_background.xml",
          f'<?xml version="1.0" encoding="utf-8"?>\n<resources>\n'
          f'    <color name="ic_launcher_background">{ANDROID_BACKGROUND}</color>\n</resources>\n'.encode())
    write(res / "mipmap-anydpi-v26" / "ic_launcher.xml",
          b'<?xml version="1.0" encoding="utf-8"?>\n'
          b'<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
          b'    <background android:drawable="@color/ic_launcher_background" />\n'
          b'    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n'
          b'    <monochrome android:drawable="@mipmap/ic_launcher_foreground" />\n'
          b'</adaptive-icon>\n')


if __name__ == "__main__":
    main()
