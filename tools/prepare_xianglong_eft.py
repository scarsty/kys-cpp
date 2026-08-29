#!/usr/bin/env python3
"""Slice, package, and validate the generated 降龍十八掌 sprite sheet."""

from __future__ import annotations

import argparse
import io
import tempfile
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
EFT_ROOT = ROOT / "work" / "game-dev" / "resource" / "eft"
SOURCE_ROOT = EFT_ROOT / "source"
SPRITE_SHEET_PATH = SOURCE_ROOT / "eft210-dragon-spritesheet.png"
ARCHIVE_PATH = EFT_ROOT / "eft210.zip"
CONTACT_PATH = SOURCE_ROOT / "eft210-contact.png"
PREVIEW_PATH = SOURCE_ROOT / "eft210-preview.webp"

EFT_ID = 210
GRID_SIZE = (4, 5)
FRAME_COUNT = GRID_SIZE[0] * GRID_SIZE[1]
CANVAS_SIZE = (336, 336)
SHEET_FRAME_SIZE = (324, 256)
SHEET_FRAME_LEFT = 6
SHEET_FRAME_TOP = 76
ANCHOR = (168, 348)
ALPHA_NOISE_FLOOR = 3
EDGE_FEATHER_PIXELS = 7
COMPONENT_ALPHA_FLOOR = 12
COMPONENT_MASK_EXPANSION = 7


def cell_edges(length: int, count: int) -> list[int]:
    return [round(index * length / count) for index in range(count + 1)]


def edge_feather(size: tuple[int, int]) -> np.ndarray:
    width, height = size
    x = np.minimum(np.arange(width), np.arange(width)[::-1]).astype(np.float32)
    y = np.minimum(np.arange(height), np.arange(height)[::-1]).astype(np.float32)
    x_weight = np.clip(x / EDGE_FEATHER_PIXELS, 0.0, 1.0)
    y_weight = np.clip(y / EDGE_FEATHER_PIXELS, 0.0, 1.0)
    return np.minimum(y_weight[:, None], x_weight[None, :])


def clean_image(image: Image.Image) -> Image.Image:
    pixels = np.asarray(image.convert("RGBA"), dtype=np.uint8).copy()
    alpha = pixels[:, :, 3].astype(np.float32)
    alpha[alpha <= ALPHA_NOISE_FLOOR] = 0.0
    alpha *= edge_feather(image.size)
    pixels[:, :, 3] = np.clip(alpha, 0.0, 255.0).astype(np.uint8)
    pixels[pixels[:, :, 3] == 0, :3] = 0
    return Image.fromarray(pixels, "RGBA")


def anchored_component(cell: Image.Image) -> Image.Image:
    pixels = np.asarray(cell.convert("RGBA"), dtype=np.uint8).copy()
    alpha = pixels[:, :, 3]
    region_left = cell.width // 4
    region_right = cell.width * 3 // 4
    region_top = cell.height // 2
    anchor_region = alpha[region_top:, region_left:region_right]
    seed_y, seed_x = np.unravel_index(np.argmax(anchor_region), anchor_region.shape)
    seed = (region_left + int(seed_x), region_top + int(seed_y))

    binary = Image.fromarray(
        np.where(alpha > COMPONENT_ALPHA_FLOOR, 255, 0).astype(np.uint8),
        "L",
    )
    connected = binary.copy()
    ImageDraw.floodfill(connected, seed, 128, thresh=0)
    component = Image.fromarray(
        np.where(np.asarray(connected) == 128, 255, 0).astype(np.uint8),
        "L",
    ).filter(ImageFilter.MaxFilter(COMPONENT_MASK_EXPANSION))

    pixels[:, :, 3] = np.where(np.asarray(component) > 0, alpha, 0)
    pixels[pixels[:, :, 3] == 0, :3] = 0
    return Image.fromarray(pixels, "RGBA")


def standard_frame(cell: Image.Image) -> Image.Image:
    cell = clean_image(anchored_component(cell)).resize(
        SHEET_FRAME_SIZE,
        Image.Resampling.LANCZOS,
    )
    canvas = Image.new("RGBA", CANVAS_SIZE, (0, 0, 0, 0))
    canvas.alpha_composite(cell, (SHEET_FRAME_LEFT, SHEET_FRAME_TOP))
    return canvas


def validate_frame(image: Image.Image, label: str) -> None:
    if image.mode != "RGBA" or image.size != CANVAS_SIZE:
        raise RuntimeError(f"{label}: expected RGBA {CANVAS_SIZE}, found {image.mode} {image.size}")
    alpha_minimum, alpha_maximum = image.getchannel("A").getextrema()
    if alpha_minimum != 0 or alpha_maximum == 0:
        raise RuntimeError(f"{label}: invalid transparency range {alpha_minimum}..{alpha_maximum}")


def slice_frames() -> list[Image.Image]:
    sheet = Image.open(SPRITE_SHEET_PATH).convert("RGBA")
    alpha_minimum, alpha_maximum = sheet.getchannel("A").getextrema()
    if alpha_minimum != 0 or alpha_maximum == 0:
        raise RuntimeError(f"sprite sheet does not contain genuine transparency: {alpha_minimum}..{alpha_maximum}")

    x_edges = cell_edges(sheet.width, GRID_SIZE[0])
    y_edges = cell_edges(sheet.height, GRID_SIZE[1])
    frames = []
    for frame_index in range(FRAME_COUNT):
        row, column = divmod(frame_index, GRID_SIZE[0])
        cell = sheet.crop((
            x_edges[column],
            y_edges[row],
            x_edges[column + 1],
            y_edges[row + 1],
        ))
        canvas = standard_frame(cell)
        validate_frame(canvas, f"generated frame {frame_index}")
        frames.append(canvas)
    return frames


def save_webp(image: Image.Image, path: Path) -> None:
    image.save(path, "WEBP", quality=92, method=6, exact=True)


def index_text() -> str:
    return "".join(f"{index}: {ANCHOR[0]}, {ANCHOR[1]}\n" for index in range(FRAME_COUNT))


def prepare_archive(frames: list[Image.Image]) -> None:
    EFT_ROOT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="eft210-", dir=EFT_ROOT) as temporary:
        temporary_root = Path(temporary)
        for index, frame in enumerate(frames):
            save_webp(frame, temporary_root / f"{index}.webp")
        (temporary_root / "index.txt").write_text(index_text(), encoding="utf-8", newline="\n")

        temporary_archive = temporary_root / ARCHIVE_PATH.name
        with zipfile.ZipFile(temporary_archive, "w", compression=zipfile.ZIP_STORED) as archive:
            for index in range(FRAME_COUNT):
                archive.write(temporary_root / f"{index}.webp", f"{index}.webp")
            archive.write(temporary_root / "index.txt", "index.txt")
        temporary_archive.replace(ARCHIVE_PATH)


def validate_archive() -> list[Image.Image]:
    expected_names = [f"{index}.webp" for index in range(FRAME_COUNT)] + ["index.txt"]
    frames = []
    with zipfile.ZipFile(ARCHIVE_PATH) as archive:
        if archive.namelist() != expected_names:
            raise RuntimeError(f"unexpected EFT members: {archive.namelist()}")
        if archive.read("index.txt").decode("utf-8") != index_text():
            raise RuntimeError("EFT offsets do not match the generated anchor")
        for index in range(FRAME_COUNT):
            payload = archive.read(f"{index}.webp")
            if payload[:4] != b"RIFF" or payload[8:12] != b"WEBP":
                raise RuntimeError(f"frame {index}: not a WebP RIFF image")
            with Image.open(io.BytesIO(payload)) as image:
                if image.format != "WEBP":
                    raise RuntimeError(f"frame {index}: unexpected format {image.format}")
                frame = image.convert("RGBA")
                validate_frame(frame, f"archive frame {index}")
                frames.append(frame.copy())
    return frames


def checker_cell(size: tuple[int, int]) -> Image.Image:
    cell = Image.new("RGBA", size, (23, 27, 34, 255))
    draw = ImageDraw.Draw(cell)
    for y in range(0, size[1], 16):
        for x in range(0, size[0], 16):
            if (x // 16 + y // 16) % 2:
                draw.rectangle((x, y, x + 15, y + 15), fill=(29, 34, 42, 255))
    return cell


def compose_preview_frame(frame: Image.Image, frame_index: int | None = None) -> Image.Image:
    preview = checker_cell((368, 384))
    target = (184, 364)
    draw = ImageDraw.Draw(preview)
    draw.polygon(
        ((target[0], target[1] - 5), (target[0] + 12, target[1]),
         (target[0], target[1] + 5), (target[0] - 12, target[1])),
        outline=(89, 190, 207, 220),
    )
    preview.alpha_composite(frame, (target[0] - ANCHOR[0], target[1] - ANCHOR[1]))
    if frame_index is not None:
        draw.text((8, 8), f"frame {frame_index}", fill=(238, 241, 245, 255))
    return preview


def write_previews(frames: list[Image.Image]) -> None:
    SOURCE_ROOT.mkdir(parents=True, exist_ok=True)
    previews = [compose_preview_frame(frame, index) for index, frame in enumerate(frames)]
    columns = 4
    rows = (len(previews) + columns - 1) // columns
    contact = Image.new("RGBA", (columns * 368, rows * 384), (18, 21, 27, 255))
    for index, preview in enumerate(previews):
        contact.alpha_composite(preview, ((index % columns) * 368, (index // columns) * 384))
    contact.convert("RGB").save(CONTACT_PATH, "PNG")

    animation = [compose_preview_frame(frame) for frame in frames]
    animation[0].save(
        PREVIEW_PATH,
        "WEBP",
        save_all=True,
        append_images=animation[1:],
        duration=78,
        loop=0,
        quality=88,
        method=6,
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()

    if not args.validate_only:
        prepare_archive(slice_frames())
    frames = validate_archive()
    write_previews(frames)

    print(f"Validated {ARCHIVE_PATH} ({FRAME_COUNT} generated RGBA WebP frames, anchor {ANCHOR})")
    print(f"Preview: {PREVIEW_PATH}")
    print(f"Contact sheet: {CONTACT_PATH}")


if __name__ == "__main__":
    main()
