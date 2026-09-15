#!/usr/bin/env python3
"""Manually prepare and validate the low-noise chess battle-effect textures."""

from __future__ import annotations

import argparse
import io
import math
import re
import shutil
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
RUNTIME_ROOT = ROOT / "work" / "game-dev" / "resource" / "chess-effects"
SOURCE_ROOT = RUNTIME_ROOT / "source"
DEFAULT_EFT_ROOT = ROOT / "work" / "game-dev" / "resource" / "eft"
PRESENTATION_CONSTANTS = ROOT / "src" / "BattleScenePresentationConstants.h"
AREA_SIZE = 512
AREA_FRAME_COUNT = 16
CUE_FRAME_COUNT = 15
FIRE_FRAME_COUNT = 20
SHORT_CUE_FRAME_COUNT = 12
POSITIVE_CUE_SOURCE_SCALE = 0.1
MASTER_CUE_SIZE = 38
EXPECTED_GROUPS = {
    "cue-positive": (CUE_FRAME_COUNT, True),
    "cue-negative": (CUE_FRAME_COUNT, True),
    "cue-bleed": (CUE_FRAME_COUNT, True),
    "cue-control": (CUE_FRAME_COUNT, True),
    "cue-cleanse": (CUE_FRAME_COUNT, True),
    "area-sand": (AREA_FRAME_COUNT, False),
    "area-ward": (AREA_FRAME_COUNT, False),
    "area-fire": (FIRE_FRAME_COUNT, False),
    "cue-sword": (SHORT_CUE_FRAME_COUNT, True),
    "cue-guardian": (SHORT_CUE_FRAME_COUNT, True),
    "cue-fire": (SHORT_CUE_FRAME_COUNT, True),
}

ROLE_STATUS_EFT_Z_OFFSET_MATCH = re.search(
    r"ROLE_STATUS_EFT_Z_OFFSET\s*=\s*([0-9.]+)f",
    PRESENTATION_CONSTANTS.read_text(encoding="utf-8"),
)
assert ROLE_STATUS_EFT_Z_OFFSET_MATCH
ROLE_STATUS_EFT_Z_OFFSET = round(float(ROLE_STATUS_EFT_Z_OFFSET_MATCH.group(1)))


def reset_directory(path: Path) -> None:
    resolved = path.resolve()
    runtime = RUNTIME_ROOT.resolve()
    if runtime not in resolved.parents:
        raise RuntimeError(f"Refusing to replace non-runtime directory: {resolved}")
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)


def black_composite_to_rgba(image: Image.Image) -> Image.Image:
    background = Image.new("RGBA", image.size, (0, 0, 0, 255))
    image = Image.alpha_composite(background, image.convert("RGBA"))
    image = image.convert("RGB").resize((AREA_SIZE, AREA_SIZE), Image.Resampling.LANCZOS)
    rgb = np.asarray(image, dtype=np.float32)
    maximum = rgb.max(axis=2)
    visible = maximum >= 7.0

    alpha = np.zeros_like(maximum)
    alpha[visible] = 255.0 * np.power(maximum[visible] / 255.0, 0.90)

    straight = np.zeros_like(rgb)
    straight[visible] = rgb[visible] * (255.0 / maximum[visible, None])
    result = np.dstack((straight, alpha))
    return Image.fromarray(np.clip(result, 0, 255).astype(np.uint8), "RGBA")


def stationary_radial_base(image: Image.Image) -> Image.Image:
    rgba = np.asarray(image, dtype=np.float32)
    alpha = rgba[:, :, 3]
    coordinates = np.indices(alpha.shape, dtype=np.float32)
    center = (AREA_SIZE - 1) / 2.0
    radius = np.rint(np.hypot(coordinates[1] - center, coordinates[0] - center)).astype(np.int32)
    bin_count = int(radius.max()) + 1

    counts = np.bincount(radius.ravel(), minlength=bin_count)
    radial_alpha = np.bincount(radius.ravel(), weights=alpha.ravel(), minlength=bin_count)
    radial_alpha = np.divide(radial_alpha, counts, out=np.zeros_like(radial_alpha), where=counts > 0)

    color = np.zeros((bin_count, 3), dtype=np.float32)
    weight = np.maximum(alpha, 1.0)
    for channel in range(3):
        total = np.bincount(
            radius.ravel(),
            weights=(rgba[:, :, channel] * weight).ravel(),
            minlength=bin_count,
        )
        divisor = np.bincount(radius.ravel(), weights=weight.ravel(), minlength=bin_count)
        color[:, channel] = np.divide(total, divisor, out=np.zeros_like(total), where=divisor > 0)

    base = np.zeros_like(rgba)
    base[:, :, :3] = color[radius]
    base[:, :, 3] = radial_alpha[radius] * 0.45
    base[base[:, :, 3] < 2.0] = 0.0
    return Image.fromarray(np.clip(base, 0, 255).astype(np.uint8), "RGBA")


def accent_layer(image: Image.Image, base: Image.Image, opacity: float) -> Image.Image:
    source = np.asarray(image, dtype=np.float32).copy()
    base_alpha = np.asarray(base, dtype=np.float32)[:, :, 3]
    source[:, :, 3] = np.maximum(0.0, source[:, :, 3] - base_alpha) * opacity
    source[source[:, :, 3] < 2.0] = 0.0
    return Image.fromarray(np.clip(source, 0, 255).astype(np.uint8), "RGBA")


def split_ward_accents(accent: Image.Image) -> tuple[Image.Image, Image.Image]:
    rgba = np.asarray(accent, dtype=np.float32)
    blue_weight = np.clip((rgba[:, :, 2] - rgba[:, :, 0] + 20.0) / 80.0, 0.0, 1.0)

    blue = rgba.copy()
    gold = rgba.copy()
    blue[:, :, 3] *= blue_weight
    gold[:, :, 3] *= 1.0 - blue_weight
    return (
        Image.fromarray(np.clip(blue, 0, 255).astype(np.uint8), "RGBA"),
        Image.fromarray(np.clip(gold, 0, 255).astype(np.uint8), "RGBA"),
    )


def save_webp(image: Image.Image, path: Path) -> None:
    image.save(path, "WEBP", quality=88, method=4)


def prepare_area(master_name: str, output_name: str, ward: bool,
                 frame_count: int = AREA_FRAME_COUNT, inset: float = 1.0) -> None:
    source = black_composite_to_rgba(Image.open(SOURCE_ROOT / master_name))
    if inset != 1.0:
        source = centered_scale(source, inset)
    base = stationary_radial_base(source)
    accent = accent_layer(source, base, 0.78 if ward else 0.72)
    output = RUNTIME_ROOT / output_name
    reset_directory(output)

    if ward:
        blue, gold = split_ward_accents(accent)
    for frame in range(frame_count):
        phase = 360.0 * frame / frame_count
        composed = base.copy()
        if ward:
            composed = Image.alpha_composite(
                composed,
                blue.rotate(phase, Image.Resampling.BICUBIC, expand=False),
            )
            composed = Image.alpha_composite(
                composed,
                gold.rotate(-phase * 2.0, Image.Resampling.BICUBIC, expand=False),
            )
        else:
            composed = Image.alpha_composite(
                composed,
                accent.rotate(phase, Image.Resampling.BICUBIC, expand=False),
            )
        save_webp(composed, output / f"{frame}.webp")


def parse_offsets(index_text: str) -> dict[int, tuple[int, int]]:
    offsets: dict[int, tuple[int, int]] = {}
    for line in index_text.splitlines():
        values = [int(value) for value in re.findall(r"-?\d+", line)]
        if len(values) >= 3:
            offsets[values[0]] = (values[1], values[2])
    return offsets


def sampled_indices(frame_count: int, output_count: int = CUE_FRAME_COUNT) -> list[int]:
    if frame_count == output_count:
        return list(range(frame_count))
    return [round(index * (frame_count - 1) / (output_count - 1)) for index in range(output_count)]


def neutralize_cue_frame(image: Image.Image, scale: float) -> Image.Image:
    rgba = np.asarray(image.convert("RGBA"), dtype=np.float32)
    brightness = rgba[:, :, :3].max(axis=2) / 255.0
    alpha = rgba[:, :, 3] * np.sqrt(brightness)
    alpha[alpha < 3.0] = 0.0
    resized = Image.fromarray(np.clip(alpha, 0, 255).astype(np.uint8), "L").resize(
        (round(image.width * scale), round(image.height * scale)),
        Image.Resampling.LANCZOS,
    )
    neutral = Image.new("RGBA", resized.size, (255, 255, 255, 0))
    neutral.putalpha(resized)
    return neutral


def normalize_cue_canvas(
    frames: list[tuple[Image.Image, int, int]],
) -> tuple[list[Image.Image], tuple[int, int]]:
    left = min(-dx for _, dx, _ in frames)
    right = max(image.width - dx for image, dx, _ in frames)
    top = min(-dy for _, _, dy in frames)
    bottom = max(image.height - dy for image, _, dy in frames)
    size = (right - left, bottom - top)
    anchor = (-left, -top)

    normalized: list[Image.Image] = []
    for image, dx, dy in frames:
        canvas = Image.new("RGBA", size, (255, 255, 255, 0))
        canvas.alpha_composite(image, (anchor[0] - dx, anchor[1] - dy))
        normalized.append(canvas)
    return normalized, anchor


def prepare_cue(
    eft_root: Path,
    eft_id: int,
    output_name: str,
    scale: float = 0.5,
    anchor_y_adjustment: int = 0,
    frame_count: int = CUE_FRAME_COUNT,
) -> None:
    archive_path = eft_root / f"eft{eft_id:03}.zip"
    output = RUNTIME_ROOT / output_name
    reset_directory(output)

    with zipfile.ZipFile(archive_path) as archive:
        frame_names = sorted(
            (name for name in archive.namelist() if name.lower().endswith(".webp")),
            key=lambda name: int(Path(name).stem),
        )
        offsets = parse_offsets(archive.read("index.txt").decode("utf-8"))
        frames: list[tuple[Image.Image, int, int]] = []
        for source_index in sampled_indices(len(frame_names), frame_count):
            image = Image.open(io.BytesIO(archive.read(frame_names[source_index])))
            dx, dy = offsets[source_index]
            frames.append((
                neutralize_cue_frame(image, scale),
                round(dx * scale),
                round(dy * scale),
            ))

        normalized, (dx, natural_dy) = normalize_cue_canvas(frames)
        dy = natural_dy + anchor_y_adjustment
        index_lines: list[str] = []
        for output_index, image in enumerate(normalized):
            save_webp(image, output / f"{output_index}.webp")
            index_lines.append(f"{output_index}: {dx}, {dy}")
        (output / "index.txt").write_text("\n".join(index_lines) + "\n", encoding="utf-8")


def centered_scale(image: Image.Image, scale: float) -> Image.Image:
    size = round(image.width * scale)
    resized = image.resize((size, size), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", image.size, (0, 0, 0, 0))
    offset = (image.width - size) // 2
    canvas.alpha_composite(resized, (offset, offset))
    return canvas


def prepare_master_cue(master_name: str, output_name: str) -> None:
    source = black_composite_to_rgba(Image.open(SOURCE_ROOT / master_name))
    source = source.resize((MASTER_CUE_SIZE, MASTER_CUE_SIZE), Image.Resampling.LANCZOS)
    output = RUNTIME_ROOT / output_name
    reset_directory(output)
    indices = []
    for frame in range(SHORT_CUE_FRAME_COUNT):
        t = frame / (SHORT_CUE_FRAME_COUNT - 1)
        image = centered_scale(source, 0.55 + 0.30 * t)
        image = image.rotate(-18.0 * t, Image.Resampling.BICUBIC)
        envelope = math.sin(math.pi * (frame + 0.5) / SHORT_CUE_FRAME_COUNT) ** 1.4
        image.putalpha(image.getchannel("A").point(lambda alpha: round(alpha * envelope)))
        save_webp(image, output / f"{frame}.webp")
        indices.append(f"{frame}: {MASTER_CUE_SIZE // 2}, {MASTER_CUE_SIZE // 2}")
    (output / "index.txt").write_text("\n".join(indices) + "\n", encoding="utf-8")


def validate_outputs() -> None:
    for name, (frame_count, requires_offsets) in EXPECTED_GROUPS.items():
        directory = RUNTIME_ROOT / name
        frames = sorted(directory.glob("*.webp"), key=lambda path: int(path.stem))
        if len(frames) != frame_count:
            raise RuntimeError(f"{name}: expected {frame_count} frames, found {len(frames)}")
        expected_names = [f"{index}.webp" for index in range(frame_count)]
        if [frame.name for frame in frames] != expected_names:
            raise RuntimeError(f"{name}: frames must be numbered contiguously from 0")

        index_path = directory / "index.txt"
        if requires_offsets:
            if not index_path.is_file():
                raise RuntimeError(f"{name}: missing index.txt")
            offsets = parse_offsets(index_path.read_text(encoding="utf-8"))
            if sorted(offsets) != list(range(frame_count)):
                raise RuntimeError(f"{name}: index.txt must define every frame")
        elif index_path.exists():
            raise RuntimeError(f"{name}: area effects must not define sprite offsets")

        sizes = set()
        for frame in frames:
            signature = frame.read_bytes()[:12]
            if len(signature) != 12 or signature[:4] != b"RIFF" or signature[8:] != b"WEBP":
                raise RuntimeError(f"{frame}: not a WebP RIFF file")
            with Image.open(frame) as image:
                if image.format != "WEBP" or image.mode != "RGBA":
                    raise RuntimeError(f"{frame}: expected RGBA WebP, found {image.mode} {image.format}")
                rgba = image.convert("RGBA")
                sizes.add(rgba.size)
                alpha_minimum, alpha_maximum = rgba.getchannel("A").getextrema()
                if alpha_minimum != 0 or alpha_maximum == 0:
                    raise RuntimeError(f"{frame}: invalid transparency range")
        if len(sizes) != 1:
            raise RuntimeError(f"{name}: frame sizes do not match")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--eft-root", type=Path, default=DEFAULT_EFT_ROOT)
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument("--group", action="append", choices=EXPECTED_GROUPS,
                        help="Only rebuild the selected group; repeat for multiple groups.")
    args = parser.parse_args()

    if not args.validate_only:
        RUNTIME_ROOT.mkdir(parents=True, exist_ok=True)
        generators = {
            "cue-positive": lambda: prepare_cue(args.eft_root, 100, "cue-positive",
                scale=POSITIVE_CUE_SOURCE_SCALE),
            "cue-negative": lambda: prepare_cue(args.eft_root, 65, "cue-negative"),
            "cue-bleed": lambda: prepare_cue(args.eft_root, 35, "cue-bleed", scale=1.0),
            "cue-control": lambda: prepare_cue(args.eft_root, 98, "cue-control",
                anchor_y_adjustment=-ROLE_STATUS_EFT_Z_OFFSET),
            "cue-cleanse": lambda: prepare_cue(args.eft_root, 101, "cue-cleanse",
                scale=POSITIVE_CUE_SOURCE_SCALE),
            "area-sand": lambda: prepare_area("area_sand.png", "area-sand", ward=False),
            "area-ward": lambda: prepare_area("area_ward.png", "area-ward", ward=True),
            "area-fire": lambda: prepare_area("area_fire.png", "area-fire", ward=False,
                frame_count=FIRE_FRAME_COUNT, inset=0.84),
            "cue-sword": lambda: prepare_cue(args.eft_root, 100, "cue-sword",
                scale=POSITIVE_CUE_SOURCE_SCALE,
                frame_count=SHORT_CUE_FRAME_COUNT),
            "cue-guardian": lambda: prepare_master_cue("guardian_qi.png", "cue-guardian"),
            "cue-fire": lambda: prepare_master_cue("area_fire.png", "cue-fire"),
        }
        for name in args.group or generators:
            generators[name]()
    validate_outputs()


if __name__ == "__main__":
    main()
