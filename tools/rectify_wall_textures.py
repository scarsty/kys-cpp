#!/usr/bin/env python3
"""Rectify perspective-looking smap wall tiles into flatter wall texture candidates.

This is an offline experiment for Paper battle rendering. It does not overwrite
source assets. By default it reads known wall-tile id ranges from
work/game-dev/resource/smap and writes processed PNG files to
work/game-dev/resource/paper-wall-texture.

The source wall tiles are old 2D perspective pieces, not seamless materials. The
script therefore produces candidates, not final assets:

- crops transparent borders;
- finds the bottom content boundary above the transparent area;
- discards the side where the lowest bottom points indicate wall thickness;
- anchors two bottom content points and two parallel points above them;
- samples that source parallelogram into a rectangular texture.

Usage:
    python tools/rectify_wall_textures.py
    python tools/rectify_wall_textures.py --ids 701 702 1410 1505
    python tools/rectify_wall_textures.py --mode strip --width 32 --height 128
    python tools/rectify_wall_textures.py --mode full --source-height-ratio 0.65
"""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable

from PIL import Image


WALL_RANGES: tuple[tuple[int, int], ...] = (
    (701, 1139),
    (1410, 1436),
    (1505, 1621),
    (1816, 1849),
    (2116, 2144),
    (2184, 2285),
)


def iter_wall_ids() -> Iterable[int]:
    for first, last in WALL_RANGES:
        yield from range(first, last + 1)


def alpha_bbox(image: Image.Image) -> tuple[int, int, int, int] | None:
    if image.mode != "RGBA":
        image = image.convert("RGBA")
    return image.getchannel("A").getbbox()


def crop_content(image: Image.Image) -> Image.Image:
    bbox = alpha_bbox(image)
    if not bbox:
        return image
    return image.crop(bbox)


def alpha_bottom_points(image: Image.Image, threshold: int) -> list[tuple[float, float]]:
    alpha = image.getchannel("A")
    width, height = image.size
    points: list[tuple[float, float]] = []
    for x in range(width):
        for y in range(height - 1, -1, -1):
            if alpha.getpixel((x, y)) > threshold:
                points.append((float(x), float(y)))
                break
    return points


def fit_line(points: list[tuple[float, float]]) -> tuple[float, float]:
    count = len(points)
    mean_x = sum(point[0] for point in points) / count
    mean_y = sum(point[1] for point in points) / count
    var_x = sum((point[0] - mean_x) ** 2 for point in points)
    if var_x == 0:
        return 0.0, mean_y
    cov_xy = sum((point[0] - mean_x) * (point[1] - mean_y) for point in points)
    slope = cov_xy / var_x
    intercept = mean_y - slope * mean_x
    return slope, intercept


def crop_thickness_side(
    image: Image.Image,
    *,
    alpha_threshold: int,
    bottom_tolerance: float,
    crop_ratio: float,
) -> tuple[Image.Image, float]:
    points = alpha_bottom_points(image, alpha_threshold)
    if len(points) < 2 or crop_ratio <= 0:
        return image, 0.0

    min_x = min(point[0] for point in points)
    max_x = max(point[0] for point in points)
    max_y = max(point[1] for point in points)
    lowest = [point for point in points if point[1] >= max_y - bottom_tolerance]
    if not lowest:
        return image, 0.0

    lowest_center = sum(point[0] for point in lowest) / len(lowest)
    content_center = (min_x + max_x) * 0.5
    crop_width = int(round((max_x - min_x + 1) * crop_ratio))
    if crop_width <= 0:
        return image, 0.0

    width, height = image.size
    if lowest_center < content_center:
        left = min(width - 1, crop_width)
        return image.crop((left, 0, width, height)), float(left)

    right = max(1, width - crop_width)
    return image.crop((0, 0, right, height)), 0.0


def source_quad(
    image: Image.Image,
    *,
    mode: str,
    strip_ratio: float,
    source_height_ratio: float,
    alpha_threshold: int,
) -> tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]]:
    points = alpha_bottom_points(image, alpha_threshold)
    if len(points) < 2:
        width, height = image.size
        return (0.0, 0.0), (float(width - 1), 0.0), (0.0, float(height - 1)), (float(width - 1), float(height - 1))

    slope, intercept = fit_line(points)
    x_values = [point[0] for point in points]
    left_x = min(x_values)
    right_x = max(x_values)
    if mode == "strip" and 0 < strip_ratio < 1:
        center_x = (left_x + right_x) * 0.5
        half_width = (right_x - left_x) * strip_ratio * 0.5
        left_x = center_x - half_width
        right_x = center_x + half_width

    bottom_left = (left_x, slope * left_x + intercept)
    bottom_right = (right_x, slope * right_x + intercept)
    max_rise = max(1.0, min(bottom_left[1], bottom_right[1]))
    rise = max(1.0, max_rise * source_height_ratio)
    top_left = (bottom_left[0], bottom_left[1] - rise)
    top_right = (bottom_right[0], bottom_right[1] - rise)
    return top_left, top_right, bottom_left, bottom_right


def offset_quad(
    quad: tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]],
    dx: float,
) -> tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]]:
    return tuple((point[0] + dx, point[1]) for point in quad)


def sample_quad(
    image: Image.Image,
    quad: tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]],
    out_size: tuple[int, int],
) -> Image.Image:
    top_left, top_right, bottom_left, _bottom_right = quad
    out_width, out_height = out_size
    width_den = max(1, out_width - 1)
    height_den = max(1, out_height - 1)
    a = (top_right[0] - top_left[0]) / width_den
    b = (bottom_left[0] - top_left[0]) / height_den
    c = top_left[0]
    d = (top_right[1] - top_left[1]) / width_den
    e = (bottom_left[1] - top_left[1]) / height_den
    f = top_left[1]
    return image.transform(
        out_size,
        Image.Transform.AFFINE,
        (a, b, c, d, e, f),
        resample=Image.Resampling.BICUBIC,
        fillcolor=(0, 0, 0, 0),
    )


def rectify(
    image: Image.Image,
    *,
    mode: str,
    alpha_threshold: int,
    thickness_crop_ratio: float,
    strip_ratio: float,
    source_height_ratio: float,
    out_size: tuple[int, int],
) -> tuple[Image.Image, tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]]]:
    image = image.convert("RGBA")
    image = crop_content(image)
    image, offset_x = crop_thickness_side(
        image,
        alpha_threshold=alpha_threshold,
        bottom_tolerance=2.0,
        crop_ratio=thickness_crop_ratio,
    )
    image = crop_content(image)
    quad = source_quad(
        image,
        mode=mode,
        strip_ratio=strip_ratio,
        source_height_ratio=source_height_ratio,
        alpha_threshold=alpha_threshold,
    )
    return sample_quad(image, quad, out_size), offset_quad(quad, offset_x)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Rectify smap wall tiles for Paper mode texture experiments")
    parser.add_argument("--src", type=Path, default=Path("work/game-dev/resource/smap"), help="source smap directory")
    parser.add_argument("--dst", type=Path, default=Path("work/game-dev/resource/paper-wall-texture"), help="output directory")
    parser.add_argument("--ids", type=int, nargs="*", help="specific tile ids to process; defaults to known wall ranges")
    parser.add_argument("--mode", choices=("full", "strip"), default="strip", help="full tile or middle material strip")
    parser.add_argument("--strip-ratio", type=float, default=0.55, help="middle strip width ratio for --mode strip")
    parser.add_argument("--alpha-threshold", type=int, default=32, help="alpha threshold used when finding bottom content points")
    parser.add_argument("--thickness-crop-ratio", type=float, default=0.18, help="width ratio to discard on the side indicated by lowest bottom points")
    parser.add_argument("--source-height-ratio", type=float, default=0.78, help="height above the bottom boundary to sample")
    parser.add_argument("--width", type=int, default=32, help="output width")
    parser.add_argument("--height", type=int, default=128, help="output height")
    parser.add_argument("--preview-count", type=int, default=12, help="also write a contact-sheet preview for first N outputs")
    parser.add_argument("--verbose", action="store_true", help="print sampled source quad per tile")
    return parser.parse_args()


def make_preview(paths: list[Path], out_path: Path) -> None:
    if not paths:
        return
    images = [Image.open(path).convert("RGBA") for path in paths]
    gap = 4
    width = sum(img.width for img in images) + gap * (len(images) - 1)
    height = max(img.height for img in images)
    sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    x = 0
    for img in images:
        sheet.alpha_composite(img, (x, height - img.height))
        x += img.width + gap
    sheet.save(out_path)


def main() -> None:
    args = parse_args()
    ids = args.ids if args.ids else list(iter_wall_ids())
    args.dst.mkdir(parents=True, exist_ok=True)

    written: list[Path] = []
    missing = 0
    failed = 0
    for tile_id in ids:
        src = args.src / f"{tile_id}.png"
        if not src.exists():
            missing += 1
            continue
        try:
            image = Image.open(src)
            out, quad = rectify(
                image,
                mode=args.mode,
                alpha_threshold=args.alpha_threshold,
                thickness_crop_ratio=args.thickness_crop_ratio,
                strip_ratio=args.strip_ratio,
                source_height_ratio=args.source_height_ratio,
                out_size=(args.width, args.height),
            )
            dst = args.dst / f"{tile_id}.png"
            out.save(dst)
            written.append(dst)
            if args.verbose:
                top_left, top_right, bottom_left, bottom_right = quad
                print(
                    f"{tile_id}: "
                    f"tl=({top_left[0]:.1f},{top_left[1]:.1f}) "
                    f"tr=({top_right[0]:.1f},{top_right[1]:.1f}) "
                    f"bl=({bottom_left[0]:.1f},{bottom_left[1]:.1f}) "
                    f"br=({bottom_right[0]:.1f},{bottom_right[1]:.1f})"
                )
        except Exception as exc:
            failed += 1
            print(f"WARN: failed {src}: {exc}")

    if args.preview_count > 0:
        make_preview(written[: args.preview_count], args.dst / "preview.png")

    print(f"source: {args.src}")
    print(f"output: {args.dst}")
    print(f"written: {len(written)}, missing: {missing}, failed: {failed}")
    if written:
        print(f"preview: {args.dst / 'preview.png'}")


if __name__ == "__main__":
    main()
