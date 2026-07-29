#!/usr/bin/env python3
"""Export ground-only isometric maps using mmap1x.zip and smap1x.zip."""

from __future__ import annotations

import argparse
import io
import struct
import zipfile
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


MAP_TILE_SIZE = 36
MAP_HALF_WIDTH = MAP_TILE_SIZE // 2
MAP_HALF_HEIGHT = 9
SCENE_SIZE = 64
MAIN_MAP_SIZE = 480
MAIN_MAP_SPLIT = 8
TILEMAP_WIDTH = MAP_HALF_WIDTH * (SCENE_SIZE - 1) * 2 + MAP_TILE_SIZE
TILEMAP_HEIGHT = MAP_HALF_HEIGHT * SCENE_SIZE * 2
MAIN_MAP_WIDTH = MAP_TILE_SIZE * MAIN_MAP_SIZE
MAIN_MAP_HEIGHT = MAP_HALF_HEIGHT * MAIN_MAP_SIZE * 2
MAIN_MAP_TILE_WIDTH = MAIN_MAP_WIDTH // MAIN_MAP_SPLIT
MAIN_MAP_TILE_HEIGHT = MAIN_MAP_HEIGHT // MAIN_MAP_SPLIT


DrawCommand = tuple[Image.Image, tuple[int, int]]


@dataclass(frozen=True)
class TileInfo:
    name: str
    x: int = 0
    y: int = 0


class TileArchive:
    def __init__(self, archive_path: Path) -> None:
        self.archive_path = archive_path
        self.archive = zipfile.ZipFile(archive_path)
        self.tiles = self._load_tile_index()
        self.cache: dict[int, Image.Image] = {}

    def _load_tile_index(self) -> dict[int, TileInfo]:
        names = self.archive.namelist()
        offsets = self._read_offsets(names)
        tiles: dict[int, TileInfo] = {}
        for name in names:
            stem = Path(name).stem
            if Path(name).suffix.lower() != ".png":
                continue
            if stem.isdecimal():
                tile_id = int(stem)
                tiles[tile_id] = TileInfo(name, *offsets.get(tile_id, (0, 0)))
                continue
            if not stem.endswith("_0"):
                continue
            tile_stem = stem[:-2]
            if tile_stem.isdecimal():
                tile_id = int(tile_stem)
                tiles.setdefault(tile_id, TileInfo(name, *offsets.get(tile_id, (0, 0))))
        return tiles

    def _read_offsets(self, names: list[str]) -> dict[int, tuple[int, int]]:
        if "index.txt" in names:
            content = self.archive.read("index.txt").decode("utf-8", errors="ignore")
            values = [int(value) for value in __import__("re").findall(r"-?\d+", content)]
            return {
                values[index]: (values[index + 1], values[index + 2])
                for index in range(0, len(values) - 2, 3)
            }
        if "index.ka" in names:
            data = self.archive.read("index.ka")
            values = struct.unpack(f"<{len(data) // 2}h", data[:len(data) // 2 * 2])
            return {
                tile_id: (values[tile_id * 2], values[tile_id * 2 + 1])
                for tile_id in range(len(values) // 2)
            }
        return {}

    def get(self, tile_id: int) -> tuple[Image.Image, TileInfo] | None:
        if tile_id not in self.tiles:
            return None
        if tile_id not in self.cache:
            self.cache[tile_id] = Image.open(io.BytesIO(self.archive.read(self.tiles[tile_id].name))).convert("RGBA")
        return self.cache[tile_id], self.tiles[tile_id]

    def close(self) -> None:
        self.archive.close()


def read_i16_grid(path: Path, width: int, height: int, offset: int = 0) -> list[list[int]]:
    size = width * height * 2
    with path.open("rb") as data_file:
        data_file.seek(offset)
        data = data_file.read(size)
    if len(data) != size:
        raise ValueError(f"{path} does not contain a {width}x{height} int16 grid at offset {offset}.")
    values = struct.unpack(f"<{width * height}h", data)
    return [list(values[row * height:(row + 1) * height]) for row in range(width)]


def read_idx_grp(idx_path: Path, grp_path: Path) -> list[bytes]:
    offsets_data = idx_path.read_bytes()
    offsets = [0, *struct.unpack(f"<{len(offsets_data) // 4}i", offsets_data)]
    data = grp_path.read_bytes()
    return [data[offsets[index]:offsets[index + 1]] for index in range(len(offsets) - 1)]


def expand_ground(grid: list[list[int]]) -> list[list[int]]:
    expanded = [row[:] for row in grid]
    for radius in range(1, 32):
        left = 31 - radius
        right = 32 + radius
        top = 31 - radius
        bottom = 32 + radius
        for y in range(top + 1, bottom):
            if expanded[left][y] <= 0:
                expanded[left][y] = expanded[left + 1][y]
            if expanded[right][y] <= 0:
                expanded[right][y] = expanded[right - 1][y]
        for x in range(left + 1, right):
            if expanded[x][top] <= 0:
                expanded[x][top] = expanded[x][top + 1]
            if expanded[x][bottom] <= 0:
                expanded[x][bottom] = expanded[x][bottom - 1]
        if expanded[left][top] <= 0:
            expanded[left][top] = expanded[left + 1][top] if expanded[left + 1][top] > 0 else expanded[left][top + 1]
        if expanded[right][top] <= 0:
            expanded[right][top] = expanded[right - 1][top] if expanded[right - 1][top] > 0 else expanded[right][top + 1]
        if expanded[left][bottom] <= 0:
            expanded[left][bottom] = expanded[left + 1][bottom] if expanded[left + 1][bottom] > 0 else expanded[left][bottom - 1]
        if expanded[right][bottom] <= 0:
            expanded[right][bottom] = expanded[right - 1][bottom] if expanded[right - 1][bottom] > 0 else expanded[right][bottom - 1]
    return expanded


def tile_position(x: int, y: int, info: TileInfo) -> tuple[int, int]:
    center_x = MAP_HALF_WIDTH * (SCENE_SIZE - 1)
    return (center_x - x * MAP_HALF_WIDTH + y * MAP_HALF_WIDTH - info.x,
            x * MAP_HALF_HEIGHT + y * MAP_HALF_HEIGHT - info.y)


def main_map_tile_position(x: int, y: int, info: TileInfo) -> tuple[int, int]:
    center_x = MAP_HALF_WIDTH * (MAIN_MAP_SIZE - 1)
    return (center_x - x * MAP_HALF_WIDTH + y * MAP_HALF_WIDTH - info.x,
            x * MAP_HALF_HEIGHT + y * MAP_HALF_HEIGHT - info.y)


def build_ground_commands(grid: list[list[int]], tiles: TileArchive, skip_nonpositive: bool = False) -> list[DrawCommand]:
    commands: list[DrawCommand] = []
    size = len(grid)
    for x in range(size):
        for y in range(size):
            if skip_nonpositive and grid[x][y] <= 0:
                continue
            tile = tiles.get(grid[x][y] // 2)
            if tile is None:
                continue
            image, info = tile
            commands.append((image, tile_position(x, y, info)))
    return commands


def build_main_map_commands(
    earth: list[list[int]],
    surface: list[list[int]],
    tiles: TileArchive,
) -> list[DrawCommand]:
    commands: list[DrawCommand] = []
    for x in range(MAIN_MAP_SIZE):
        for y in range(MAIN_MAP_SIZE):
            for tile_value in (earth[x][y], surface[x][y]):
                if tile_value <= 0:
                    continue
                tile = tiles.get(tile_value // 2)
                if tile is None:
                    continue
                image, info = tile
                commands.append((image, main_map_tile_position(x, y, info)))
    return commands


def render_commands(commands: list[DrawCommand]) -> Image.Image:
    if not commands:
        return Image.new("RGBA", (1, 1), (0, 0, 0, 0))
    min_x = min(position[0] for image, position in commands)
    min_y = min(position[1] for image, position in commands)
    max_x = max(position[0] + image.width for image, position in commands)
    max_y = max(position[1] + image.height for image, position in commands)
    canvas = Image.new("RGBA", (max_x - min_x, max_y - min_y), (0, 0, 0, 0))
    for image, position in commands:
        canvas.alpha_composite(image, (position[0] - min_x, position[1] - min_y))
    return canvas


def render_fixed_commands(commands: list[DrawCommand], width: int, height: int) -> Image.Image:
    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    if not commands:
        return canvas
    min_x = min(position[0] for image, position in commands)
    min_y = min(position[1] for image, position in commands)
    for image, position in commands:
        canvas.alpha_composite(image, (position[0] - min_x, position[1] - min_y))
    return canvas


def pad_for_split(canvas: Image.Image, split: int) -> Image.Image:
    width = ((canvas.width + split - 1) // split) * split
    height = ((canvas.height + split - 1) // split) * split
    if width == canvas.width and height == canvas.height:
        return canvas
    padded = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    padded.alpha_composite(canvas, (0, 0))
    return padded


def autocrop(canvas: Image.Image) -> Image.Image:
    bbox = canvas.getchannel("A").getbbox()
    if bbox is None:
        return canvas
    return canvas.crop(bbox)


def is_blank_or_black(canvas: Image.Image) -> bool:
    rgba = canvas.convert("RGBA")
    alpha = rgba.getchannel("A")
    if alpha.getextrema()[1] == 0:
        return True
    visible = rgba.copy()
    visible.putalpha(255)
    return max(channel.getextrema()[1] for channel in visible.split()[:3]) <= 2


def split_main_map(canvas: Image.Image, destination_dir: Path) -> None:
    destination_dir.mkdir(parents=True, exist_ok=True)
    canvas = pad_for_split(canvas, MAIN_MAP_SPLIT)
    width = canvas.width // MAIN_MAP_SPLIT
    height = canvas.height // MAIN_MAP_SPLIT
    tile_id = 0
    for row in range(MAIN_MAP_SPLIT):
        for column in range(MAIN_MAP_SPLIT):
            chunk = canvas.crop((column * width, row * height, (column + 1) * width, (row + 1) * height))
            write_png(chunk, destination_dir / f"{tile_id}.png", False)
            tile_id += 1


def write_png(canvas: Image.Image, destination: Path, skip_blank: bool = True) -> bool:
    if skip_blank and is_blank_or_black(canvas):
        return False
    destination.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(destination)
    print(destination)
    return True


def export_main_map(resource_dir: Path, output_dir: Path, mmap_tiles: TileArchive) -> None:
    earth = read_i16_grid(resource_dir / "earth.002", MAIN_MAP_SIZE, MAIN_MAP_SIZE)
    surface = read_i16_grid(resource_dir / "surface.002", MAIN_MAP_SIZE, MAIN_MAP_SIZE)
    destination_dir = output_dir / "mmap-earth"
    destination_dir.mkdir(parents=True, exist_ok=True)
    canvas = render_fixed_commands(build_main_map_commands(earth, surface, mmap_tiles), MAIN_MAP_WIDTH, MAIN_MAP_HEIGHT)
    tile_id = 0
    for row in range(MAIN_MAP_SPLIT):
        for column in range(MAIN_MAP_SPLIT):
            left = column * MAIN_MAP_TILE_WIDTH
            top = row * MAIN_MAP_TILE_HEIGHT
            tile = canvas.crop((left, top, left + MAIN_MAP_TILE_WIDTH, top + MAIN_MAP_TILE_HEIGHT))
            write_png(tile, destination_dir / f"{tile_id}.png", False)
            tile_id += 1


def export_scene_maps(scene_data_path: Path, output_dir: Path, smap_tiles: TileArchive) -> None:
    scene_stride = 6 * SCENE_SIZE * SCENE_SIZE * 2
    scene_count = scene_data_path.stat().st_size // scene_stride
    for scene_id in range(scene_count):
        grid = read_i16_grid(scene_data_path, SCENE_SIZE, SCENE_SIZE, scene_id * scene_stride)
        canvas = render_fixed_commands(build_ground_commands(expand_ground(grid), smap_tiles, True), TILEMAP_WIDTH, TILEMAP_HEIGHT)
        write_png(canvas, output_dir / "smap-earth" / f"{scene_id}.png", False)


def export_battle_maps(resource_dir: Path, output_dir: Path, smap_tiles: TileArchive) -> None:
    fields = read_idx_grp(resource_dir / "warfld.idx", resource_dir / "warfld.grp")
    for field_id, field in enumerate(fields):
        if len(field) < 2 * SCENE_SIZE * SCENE_SIZE:
            continue
        values = struct.unpack(f"<{SCENE_SIZE * SCENE_SIZE}h", field[:2 * SCENE_SIZE * SCENE_SIZE])
        grid = [list(values[row * SCENE_SIZE:(row + 1) * SCENE_SIZE]) for row in range(SCENE_SIZE)]
        canvas = render_fixed_commands(build_ground_commands(expand_ground(grid), smap_tiles, True), TILEMAP_WIDTH, TILEMAP_HEIGHT)
        write_png(canvas, output_dir / "battle-earth" / f"{field_id}.png", False)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path, default=Path("game"), help="Game data directory (default: game)")
    parser.add_argument("--save", type=Path, default=Path("game/save/1.zip"), help="Save ZIP or extracted s*.grp scene data")
    parser.add_argument("--output", type=Path, default=Path("game/exported_ground_maps"), help="Output directory")
    return parser.parse_args()


def extract_scene_data(save_path: Path) -> bytes:
    if save_path.suffix.lower() == ".zip":
        with zipfile.ZipFile(save_path) as save_file:
            scene_files = sorted(name for name in save_file.namelist() if name.startswith("s") and name.endswith(".grp"))
            if not scene_files:
                raise ValueError(f"No s*.grp scene data in {save_path}.")
            return save_file.read(scene_files[0])
    return save_path.read_bytes()


def main() -> None:
    args = parse_args()
    resource_dir = args.game_dir / "resource"
    mmap_tiles = TileArchive(resource_dir / "mmap1x.zip")
    smap_tiles = TileArchive(resource_dir / "smap1x.zip")
    try:
        export_main_map(resource_dir, args.output, mmap_tiles)
        scene_data_path = args.output / ".scene_data.grp"
        scene_data_path.parent.mkdir(parents=True, exist_ok=True)
        scene_data_path.write_bytes(extract_scene_data(args.save))
        try:
            export_scene_maps(scene_data_path, args.output, smap_tiles)
        finally:
            scene_data_path.unlink(missing_ok=True)
        export_battle_maps(resource_dir, args.output, smap_tiles)
    finally:
        mmap_tiles.close()
        smap_tiles.close()


if __name__ == "__main__":
    main()