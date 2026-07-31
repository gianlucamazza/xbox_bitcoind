#!/usr/bin/env python3
"""Generate UWP tile/splash assets from Bitcoin Core official icons.

Sources (pinned tree after fetch):
  third_party/bitcoin/share/pixmaps/bitcoin256.png  (preferred)
  third_party/bitcoin/src/qt/res/icons/bitcoin.png

Composites the Core mark onto the app dark background (#0E1116) with product
label text. Replaces orange-square placeholders under uwp/Assets/.

Requires: Pillow. Run from repo root:
  ./scripts/generate-uwp-assets.py
"""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "uwp" / "Assets"
CORE_CANDIDATES = [
    ROOT / "third_party/bitcoin/share/pixmaps/bitcoin256.png",
    ROOT / "third_party/bitcoin/src/qt/res/icons/bitcoin.png",
    ROOT / "third_party/bitcoin/share/pixmaps/bitcoin128.png",
]

BG = (14, 17, 22, 255)  # #0E1116 — match AppxManifest Splash/BackgroundColor
ORANGE = (247, 147, 26, 255)  # #F7931A
MUTED = (148, 156, 170, 255)


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    ]
    for path in candidates:
        if Path(path).is_file():
            return ImageFont.truetype(path, size=size)
    return ImageFont.load_default()


def load_core_icon() -> Image.Image:
    for path in CORE_CANDIDATES:
        if path.is_file():
            im = Image.open(path).convert("RGBA")
            print(f"source: {path.relative_to(ROOT)} ({im.size[0]}x{im.size[1]})")
            return im
    print(
        "error: Bitcoin Core icon not found. Fetch pin first:\n"
        "  ./scripts/fetch-bitcoin-core.sh\n"
        f"looked in:\n  " + "\n  ".join(str(p.relative_to(ROOT)) for p in CORE_CANDIDATES),
        file=sys.stderr,
    )
    sys.exit(1)


def solid(size: tuple[int, int], color=BG) -> Image.Image:
    return Image.new("RGBA", size, color)


def paste_icon(canvas: Image.Image, icon: Image.Image, cx: float, cy: float, diameter: float) -> None:
    """Center Core icon at (cx,cy) with given diameter (LANCZOS)."""
    d = max(1, int(round(diameter)))
    mark = icon.resize((d, d), Image.Resampling.LANCZOS)
    x = int(round(cx - d / 2))
    y = int(round(cy - d / 2))
    canvas.alpha_composite(mark, (x, y))


def save(img: Image.Image, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / name
    img.save(path, format="PNG", optimize=True)
    print(f"wrote {path.relative_to(ROOT)} {img.size[0]}x{img.size[1]}")


def make_square(icon: Image.Image, size: int) -> Image.Image:
    img = solid((size, size))
    # Full bleed mark with small margin (Core icons are circular on transparent/black).
    margin = size * 0.06
    paste_icon(img, icon, size / 2, size / 2, size - 2 * margin)
    return img


def make_wide(icon: Image.Image, w: int = 310, h: int = 150) -> Image.Image:
    img = solid((w, h))
    d = ImageDraw.Draw(img)
    margin = h * 0.12
    plate = h - 2 * margin
    paste_icon(img, icon, margin + plate / 2, h / 2, plate)

    title = "xbox_bitcoind"
    sub = "Bitcoin Core · Dev Mode"
    f_title = font(max(14, int(h * 0.18)), bold=True)
    f_sub = font(max(10, int(h * 0.11)), bold=False)
    tx = margin + plate + margin * 0.85
    # Ellipsize title if needed for small wide tiles
    max_w = w - tx - margin
    while title and d.textbbox((0, 0), title, font=f_title)[2] > max_w and len(title) > 4:
        title = title[:-1]
    if title != "xbox_bitcoind" and not title.endswith("…"):
        title = title.rstrip("_") + "…"
    d.text((tx, h * 0.30), title, font=f_title, fill=ORANGE)
    d.text((tx, h * 0.55), sub, font=f_sub, fill=MUTED)
    return img


def make_splash(icon: Image.Image, w: int = 620, h: int = 300) -> Image.Image:
    img = solid((w, h))
    d = ImageDraw.Draw(img)
    # Icon upper center
    diam = min(w, h) * 0.36
    paste_icon(img, icon, w / 2, h * 0.38, diam)

    title = "xbox_bitcoind"
    sub = "Bitcoin Core · pruned full node · Dev Mode"
    f_title = font(max(22, int(h * 0.11)), bold=True)
    f_sub = font(max(12, int(h * 0.055)), bold=False)
    tb = d.textbbox((0, 0), title, font=f_title)
    tw = tb[2] - tb[0]
    d.text(((w - tw) / 2, h * 0.62), title, font=f_title, fill=ORANGE)
    sb = d.textbbox((0, 0), sub, font=f_sub)
    sw = sb[2] - sb[0]
    d.text(((w - sw) / 2, h * 0.76), sub, font=f_sub, fill=MUTED)
    return img


def main() -> None:
    icon = load_core_icon()

    # Base (scale-100) — referenced by AppxManifest / vcxproj
    save(make_square(icon, 44), "Square44x44Logo.png")
    save(make_square(icon, 150), "Square150x150Logo.png")
    save(make_square(icon, 50), "StoreLogo.png")
    save(make_wide(icon, 310, 150), "Wide310x150Logo.png")
    save(make_splash(icon, 620, 300), "SplashScreen.png")

    # scale-200 for sharper Xbox TV splash/tiles
    save(make_square(icon, 88), "Square44x44Logo.scale-200.png")
    save(make_square(icon, 300), "Square150x150Logo.scale-200.png")
    save(make_square(icon, 100), "StoreLogo.scale-200.png")
    save(make_wide(icon, 620, 300), "Wide310x150Logo.scale-200.png")
    save(make_splash(icon, 1240, 600), "SplashScreen.scale-200.png")


if __name__ == "__main__":
    main()
