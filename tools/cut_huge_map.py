#!/usr/bin/env python3
"""Split a large map image into numbered transparent tiles.

Defaults preserve the former cuthug tool behavior: map.png is placed at the
top-left of a 17280x8640 transparent canvas and split into an 8x8 grid.
Only tiles whose pixels do not all have the same channel value are written.
"""
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


def has_non_uniform_pixels(image: Image.Image) -> bool:
    extrema = image.getextrema()
    values = [value for channel in extrema for value in channel]
    return min(values) != max(values)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Split a large map image into numbered tiles")
    parser.add_argument("input", nargs="?", type=Path, default=Path("map.png"), help="source image (default: map.png)")
    parser.add_argument("--output", "-o", type=Path, default=Path("."), help="output directory (default: current directory)")
    parser.add_argument("--canvas-width", type=int, default=17280, help="transparent canvas width (default: 17280)")
    parser.add_argument("--canvas-height", type=int, default=8640, help="transparent canvas height (default: 8640)")
    parser.add_argument("--grid", type=int, default=8, help="rows and columns in the square grid (default: 8)")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.grid <= 0 or args.canvas_width <= 0 or args.canvas_height <= 0:
        raise SystemExit("Canvas dimensions and grid size must be positive.")
    if args.canvas_width % args.grid or args.canvas_height % args.grid:
        raise SystemExit("Canvas dimensions must be divisible by the grid size.")
    if not args.input.is_file():
        raise SystemExit(f"Input image not found: {args.input}")

    with Image.open(args.input) as source_file:
        source = source_file.convert("RGBA")
    if source.width > args.canvas_width or source.height > args.canvas_height:
        raise SystemExit(
            f"Input image {source.width}x{source.height} exceeds "
            f"the {args.canvas_width}x{args.canvas_height} canvas."
        )

    canvas = Image.new("RGBA", (args.canvas_width, args.canvas_height))
    canvas.paste(source, (0, 0))
    tile_width = args.canvas_width // args.grid
    tile_height = args.canvas_height // args.grid
    args.output.mkdir(parents=True, exist_ok=True)

    written = 0
    for row in range(args.grid):
        for column in range(args.grid):
            index = row * args.grid + column
            left = column * tile_width
            top = row * tile_height
            tile = canvas.crop((left, top, left + tile_width, top + tile_height))
            if has_non_uniform_pixels(tile):
                output = args.output / f"{index}.png"
                tile.save(output)
                print(f"{index}: {output}")
                written += 1

    print(f"Wrote {written} non-uniform tiles to {args.output}.")


if __name__ == "__main__":
    main()