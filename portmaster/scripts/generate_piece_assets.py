#!/usr/bin/env python3
"""Generate size-specific PortMaster Xiangqi piece BMPs.

The runtime renderer chooses the closest set instead of scaling 180px source
pieces down to handheld sizes every frame.
"""

from pathlib import Path

from PIL import Image, ImageFilter


PIECE_SIZES = (24, 28, 34, 40, 42, 46, 52, 60, 72, 84, 96, 128)
PIECE_FILES = (
    "b_a.png", "b_b.png", "b_c.png", "b_k.png", "b_n.png", "b_p.png", "b_r.png",
    "r_a.png", "r_b.png", "r_c.png", "r_k.png", "r_n.png", "r_p.png", "r_r.png",
)
MAGENTA = (255, 0, 255)
BOARD_MATTE = (221, 185, 118)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def resize_piece(image: Image.Image, size: int) -> Image.Image:
    rgba = image.convert("RGBA")
    resized = rgba.resize((size, size), Image.Resampling.LANCZOS)
    if size <= 46:
        resized = resized.filter(ImageFilter.UnsharpMask(radius=0.45, percent=80, threshold=3))
    else:
        resized = resized.filter(ImageFilter.UnsharpMask(radius=0.55, percent=65, threshold=3))
    return resized


def composite_for_colorkey(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    rgb = Image.new("RGB", rgba.size, MAGENTA)
    pixels = rgba.load()
    out = rgb.load()

    for y in range(rgba.height):
        for x in range(rgba.width):
            red, green, blue, alpha = pixels[x, y]
            if alpha <= 8:
                out[x, y] = MAGENTA
            elif alpha >= 248:
                out[x, y] = (red, green, blue)
            else:
                inv = 255 - alpha
                out[x, y] = (
                    (red * alpha + BOARD_MATTE[0] * inv) // 255,
                    (green * alpha + BOARD_MATTE[1] * inv) // 255,
                    (blue * alpha + BOARD_MATTE[2] * inv) // 255,
                )
    return rgb


def main() -> None:
    root = repo_root()
    source_dir = root / "assets" / "xiangqi"
    output_root = root / "portmaster" / "port" / "exact_chinesechess" / "exact_chinesechess" / "assets" / "xiangqi"

    for size in PIECE_SIZES:
        output_dir = output_root / f"pieces_{size}"
        output_dir.mkdir(parents=True, exist_ok=True)
        for piece_file in PIECE_FILES:
            source = source_dir / piece_file
            output = output_dir / piece_file.replace(".png", ".bmp")
            image = Image.open(source)
            composite_for_colorkey(resize_piece(image, size)).save(output, format="BMP")


if __name__ == "__main__":
    main()
