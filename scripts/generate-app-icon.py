#!/usr/bin/env python3
"""Generate Realm app icon raster assets from simple vector geometry."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
ICONSET = ROOT / "build" / "app-icon.iconset"
FONT_CANDIDATES = [
    "/System/Library/Fonts/Supplemental/Arial Black.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    "/System/Library/Fonts/Supplemental/Verdana Bold.ttf",
]


def lerp(a: int, b: int, t: float) -> int:
    return round(a + (b - a) * t)


def gradient(size: int, top: tuple[int, int, int], bottom: tuple[int, int, int]) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    for y in range(size):
        t = y / max(1, size - 1)
        color = tuple(lerp(top[i], bottom[i], t) for i in range(3)) + (255,)
        draw.line([(0, y), (size, y)], fill=color)
    return img


def rounded_mask(size: int, radius: int) -> Image.Image:
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return mask


def poly_layer(size: int, points: list[tuple[int, int]], color: tuple[int, int, int, int]) -> Image.Image:
    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    ImageDraw.Draw(layer).polygon(points, fill=color)
    return layer


def find_font(size: int) -> ImageFont.FreeTypeFont:
    for path in FONT_CANDIDATES:
        if Path(path).exists():
            return ImageFont.truetype(path, size=size)
    return ImageFont.load_default(size=size)


def draw_text_center(draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str, font: ImageFont.ImageFont, **kwargs) -> None:
    bbox = draw.textbbox((0, 0), text, font=font, stroke_width=kwargs.get("stroke_width", 0))
    x = xy[0] - (bbox[2] - bbox[0]) // 2 - bbox[0]
    y = xy[1] - (bbox[3] - bbox[1]) // 2 - bbox[1]
    draw.text((x, y), text, font=font, **kwargs)


def make_icon(size: int) -> Image.Image:
    scale = size / 1024
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    bg = gradient(size, (20, 34, 52), (3, 7, 11))
    bg.putalpha(rounded_mask(size, round(196 * scale)))
    img.alpha_composite(bg)

    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.ellipse([round(194 * scale), round(710 * scale), round(830 * scale), round(898 * scale)], fill=(0, 0, 0, 88))
    shadow = shadow.filter(ImageFilter.GaussianBlur(round(20 * scale)))
    img.alpha_composite(shadow)

    top = [(round(x * scale), round(y * scale)) for x, y in [(512, 360), (812, 526), (512, 692), (212, 526)]]
    left = [(round(x * scale), round(y * scale)) for x, y in [(212, 526), (512, 692), (512, 822), (212, 656)]]
    right = [(round(x * scale), round(y * scale)) for x, y in [(512, 692), (812, 526), (812, 656), (512, 822)]]

    block = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    block.alpha_composite(poly_layer(size, left, (29, 89, 47, 255)))
    block.alpha_composite(poly_layer(size, right, (17, 70, 43, 255)))
    block.alpha_composite(poly_layer(size, top, (62, 147, 69, 255)))
    bd = ImageDraw.Draw(block)
    bd.line(top + [top[0]], fill=(151, 219, 111, 130), width=max(2, round(4 * scale)))
    for x, y, r in [(352, 526, 6), (438, 492, 4), (592, 515, 5), (662, 560, 4), (476, 604, 4), (718, 522, 5)]:
        bd.ellipse([round((x-r)*scale), round((y-r)*scale), round((x+r)*scale), round((y+r)*scale)], fill=(216, 255, 210, 130))
    img.alpha_composite(block)

    font = find_font(round(500 * scale))
    text_layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    td = ImageDraw.Draw(text_layer)
    draw_text_center(td, (round(540 * scale), round(478 * scale)), "R", font, fill=(95, 52, 16, 180))
    draw_text_center(
        td,
        (round(512 * scale), round(458 * scale)),
        "R",
        font,
        fill=(255, 211, 72, 255),
        stroke_width=max(2, round(16 * scale)),
        stroke_fill=(255, 247, 207, 255),
    )
    img.alpha_composite(text_layer)

    return img


def main() -> int:
    ASSETS.mkdir(exist_ok=True)
    icon = make_icon(1024)
    icon.save(ASSETS / "app-icon.png")
    icon.save(ASSETS / "app-icon.ico", sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])

    ICONSET.mkdir(parents=True, exist_ok=True)
    sizes = [
        ("icon_16x16.png", 16),
        ("icon_16x16@2x.png", 32),
        ("icon_32x32.png", 32),
        ("icon_32x32@2x.png", 64),
        ("icon_128x128.png", 128),
        ("icon_128x128@2x.png", 256),
        ("icon_256x256.png", 256),
        ("icon_256x256@2x.png", 512),
        ("icon_512x512.png", 512),
        ("icon_512x512@2x.png", 1024),
    ]
    for filename, target_size in sizes:
        icon.resize((target_size, target_size), Image.Resampling.LANCZOS).save(ICONSET / filename)

    if sys.platform == "darwin":
        subprocess.run(["iconutil", "-c", "icns", str(ICONSET), "-o", str(ASSETS / "app-icon.icns")], check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
