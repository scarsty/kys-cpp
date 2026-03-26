#!/usr/bin/env python3
from __future__ import annotations

import argparse
import array
import io
import sqlite3
import struct
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK_ROOT = ROOT / "work" / "game-dev"
DEFAULT_OUTPUT_DIR = ROOT / "work" / "subscenes"

COORD_COUNT = 64
LAYER_COUNT = 6
BATTLEMAP_SAVE_LAYER_COUNT = 2
EVENT_COUNT = 200
EVENT_FIELD_COUNT = 11
CELL_COUNT = COORD_COUNT * COORD_COUNT

TILE_W = 18
TILE_H = 9
CANVAS_WIDTH = COORD_COUNT * TILE_W * 2
CANVAS_HEIGHT = COORD_COUNT * TILE_H * 2

SDATA_LENGTH = CELL_COUNT * LAYER_COUNT * 2
DDATA_LENGTH = EVENT_COUNT * EVENT_FIELD_COUNT * 2
BATTLE_FIELD_BLOCK_LENGTH = CELL_COUNT * BATTLEMAP_SAVE_LAYER_COUNT * 2
BATTLE_INFO_RECORD_SIZE = 186

SUBSCENE_DRAW_LAYERS = ("earth", "building", "event", "decoration")
BATTLE_DRAW_LAYERS = ("earth", "building")
LAYER_ALIASES = {
    "earth": "earth",
    "floor": "earth",
    "building": "building",
    "buildings": "building",
    "event": "event",
    "events": "event",
    "decoration": "decoration",
    "decorations": "decoration",
}


@dataclass(frozen=True)
class SubMapRow:
    chunk_index: int
    submap_id: int
    name: str


@dataclass(frozen=True)
class BattleInfoRow:
    battle_id: int
    battlefield_id: int
    name: str


@dataclass
class SubMapSnapshot:
    row: SubMapRow
    layer_values: array.array
    event_values: array.array

    def layer_value(self, layer: int, x: int, y: int) -> int:
        return int(self.layer_values[layer * CELL_COUNT + x + y * COORD_COUNT])

    def earth(self, x: int, y: int) -> int:
        return self.layer_value(0, x, y)

    def building(self, x: int, y: int) -> int:
        return self.layer_value(1, x, y)

    def decoration(self, x: int, y: int) -> int:
        return self.layer_value(2, x, y)

    def event_index(self, x: int, y: int) -> int:
        return self.layer_value(3, x, y)

    def building_height(self, x: int, y: int) -> int:
        return self.layer_value(4, x, y)

    def decoration_height(self, x: int, y: int) -> int:
        return self.layer_value(5, x, y)

    def event_current_pic(self, event_index: int) -> int:
        if event_index < 0 or event_index >= EVENT_COUNT:
            return 0
        base = event_index * EVENT_FIELD_COUNT
        current_pic = int(self.event_values[base + 5])
        end_pic = int(self.event_values[base + 6])
        begin_pic = int(self.event_values[base + 7])
        pic_delay = int(self.event_values[base + 8])
        if begin_pic < end_pic:
            current_pic = begin_pic + (pic_delay * 2) % (end_pic - begin_pic)
        return current_pic

    def event_pic_at(self, x: int, y: int) -> int:
        return self.event_current_pic(self.event_index(x, y))


@dataclass
class BattleFieldSnapshot:
    battlefield_id: int
    layer_values: array.array

    def layer_value(self, layer: int, x: int, y: int) -> int:
        return int(self.layer_values[layer * CELL_COUNT + x + y * COORD_COUNT])

    def earth(self, x: int, y: int) -> int:
        return self.layer_value(0, x, y)

    def building(self, x: int, y: int) -> int:
        return self.layer_value(1, x, y)


class ArchiveAccessor:
    def __init__(self, source_path: Path) -> None:
        self.source_path = source_path
        self._zip_file: zipfile.ZipFile | None = None
        self._name_map: dict[str, str | Path] = {}

        if source_path.is_dir():
            for child in source_path.iterdir():
                if child.is_file():
                    self._name_map[child.name.lower()] = child
        else:
            self._zip_file = zipfile.ZipFile(source_path)
            for info in self._zip_file.infolist():
                if info.is_dir():
                    continue
                key = Path(info.filename.replace("\\", "/")).name.lower()
                self._name_map.setdefault(key, info.filename)

    def close(self) -> None:
        if self._zip_file is not None:
            self._zip_file.close()

    def read_bytes(self, name: str) -> bytes | None:
        target = self._name_map.get(name.lower())
        if target is None:
            return None
        if isinstance(target, Path):
            return target.read_bytes()
        if self._zip_file is None:
            return None
        return self._zip_file.read(target)


class SmapArchive:
    def __init__(self, source_path: Path) -> None:
        self.source_path = source_path
        self.accessor = ArchiveAccessor(source_path)
        self.offsets = self._load_offsets()
        self.cache: dict[int, list[Image.Image] | None] = {}
        self.missing_ids: set[int] = set()

    def close(self) -> None:
        self.accessor.close()

    def _load_offsets(self) -> list[tuple[int, int]]:
        raw = self.accessor.read_bytes("index.ka")
        if raw is None:
            raise FileNotFoundError(f"{self.source_path}: missing index.ka")
        if len(raw) % 4 != 0:
            raise ValueError(f"{self.source_path}: index.ka size {len(raw)} is not divisible by 4")
        return [struct.unpack_from("<hh", raw, offset) for offset in range(0, len(raw), 4)]

    def _load_variants(self, index: int) -> list[Image.Image] | None:
        if index <= 0 or index >= len(self.offsets):
            self.missing_ids.add(index)
            return None

        direct = self.accessor.read_bytes(f"{index}.png")
        if direct is not None:
            return [decode_png(direct)]

        variants: list[Image.Image] = []
        sub_index = 0
        while True:
            payload = self.accessor.read_bytes(f"{index}_{sub_index}.png")
            if payload is None:
                break
            variants.append(decode_png(payload))
            sub_index += 1

        if not variants:
            self.missing_ids.add(index)
            return None
        return variants

    def draw(self, canvas: Image.Image, index: int, anchor_x: int, anchor_y: int, variant_frame: int) -> bool:
        variants = self.cache.get(index)
        if variants is None and index not in self.cache:
            variants = self._load_variants(index)
            self.cache[index] = variants

        if not variants:
            return False

        dx, dy = self.offsets[index]
        image = variants[variant_frame % len(variants)]
        alpha_composite_clipped(canvas, image, anchor_x - dx, anchor_y - dy)
        return True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Render KYS subscene and battle-map layers to PNGs using the current engine projection and layer logic."
    )
    parser.add_argument(
        "output_dir",
        nargs="?",
        default=str(DEFAULT_OUTPUT_DIR),
        help="Directory to write PNGs to. Defaults to work/subscenes.",
    )
    parser.add_argument(
        "--work-root",
        type=Path,
        default=DEFAULT_WORK_ROOT,
        help="Base game working directory. Defaults to work/game-dev.",
    )
    parser.add_argument(
        "--submap-id",
        dest="submap_ids",
        action="append",
        type=int,
        help="Submap ID to render. Repeat to render multiple IDs. If omitted, renders all submaps.",
    )
    parser.add_argument(
        "--battle-id",
        dest="battle_ids",
        action="append",
        type=int,
        help="Battle ID to render with its mapped battlefield. Repeat to render multiple battles.",
    )
    parser.add_argument(
        "--battlefield-id",
        dest="battlefield_ids",
        action="append",
        type=int,
        help="Battlefield ID to render directly. If no battle selectors are given, battle mode renders all referenced battlefields.",
    )
    parser.add_argument(
        "--mode",
        choices=("subscene", "battle", "both"),
        default="subscene",
        help="What to render. Default: subscene.",
    )
    parser.add_argument(
        "--layers",
        default="earth",
        help="Comma-separated subscene layers to draw: earth, building, event, decoration, or all. Default: earth.",
    )
    parser.add_argument(
        "--battle-layers",
        default="earth",
        help="Comma-separated battle-map layers to draw: earth, building, or all. Default: earth.",
    )
    parser.add_argument(
        "--smap-zip",
        type=Path,
        help="Path to smap.zip or an extracted smap directory. Defaults to work/game-dev/resource/smap.zip.",
    )
    parser.add_argument(
        "--game-db",
        type=Path,
        help="Path to save/game.db. Defaults to work/game-dev/save/game.db.",
    )
    parser.add_argument(
        "--sdata",
        type=Path,
        help="Path to allsin.grp or allsin.grp.zip. Defaults to work/game-dev/save/allsin.grp(.zip).",
    )
    parser.add_argument(
        "--ddata",
        type=Path,
        help="Path to alldef.grp or alldef.grp.zip. Defaults to work/game-dev/save/alldef.grp(.zip).",
    )
    parser.add_argument(
        "--war-sta",
        type=Path,
        help="Path to resource/war.sta. Defaults to work/game-dev/resource/war.sta.",
    )
    parser.add_argument(
        "--warfld-idx",
        type=Path,
        help="Path to resource/warfld.idx. Defaults to work/game-dev/resource/warfld.idx.",
    )
    parser.add_argument(
        "--warfld-grp",
        type=Path,
        help="Path to resource/warfld.grp or warfld.grp.zip. Defaults to work/game-dev/resource/warfld.grp.",
    )
    parser.add_argument(
        "--variant-frame",
        type=int,
        default=0,
        help="Frame index to use for animated smap tiles. Default: 0.",
    )
    parser.add_argument(
        "--opaque-background",
        action="store_true",
        help="Fill the canvas with opaque black instead of transparency.",
    )
    return parser.parse_args()


def parse_layers(raw: str, allowed_layers: tuple[str, ...]) -> tuple[str, ...]:
    parts = [part.strip().lower() for part in raw.split(",") if part.strip()]
    if not parts:
        raise ValueError("at least one layer must be specified")
    if "all" in parts:
        return allowed_layers

    resolved: list[str] = []
    for part in parts:
        layer = LAYER_ALIASES.get(part)
        if layer is None or layer not in allowed_layers:
            valid = ", ".join(sorted(set(LAYER_ALIASES) | {"all"}))
            raise ValueError(f"unknown layer '{part}'. Valid values: {valid}")
        if layer not in resolved:
            resolved.append(layer)
    return tuple(resolved)


def decode_png(payload: bytes) -> Image.Image:
    with Image.open(io.BytesIO(payload)) as image:
        return image.convert("RGBA")


def choose_existing(*paths: Path) -> Path:
    for path in paths:
        if path.exists():
            return path
    checked = ", ".join(str(path) for path in paths)
    raise FileNotFoundError(f"none of these paths exist: {checked}")


def resolve_resource_path(explicit: Path | None, *defaults: Path) -> Path:
    if explicit is not None:
        return explicit.resolve()
    return choose_existing(*defaults).resolve()


def read_binary_with_zip_fallback(path: Path) -> bytes:
    if path.is_file():
        if path.suffix.lower() != ".zip":
            return path.read_bytes()
        entry_name = Path(path.stem).name
        return read_zip_entry_by_basename(path, entry_name)

    zipped_path = path.with_name(path.name + ".zip")
    if zipped_path.is_file():
        return read_zip_entry_by_basename(zipped_path, path.name)

    raise FileNotFoundError(f"missing data file: {path}")


def read_zip_entry_by_basename(zip_path: Path, basename: str) -> bytes:
    target_name = basename.lower()
    with zipfile.ZipFile(zip_path) as archive:
        for info in archive.infolist():
            if info.is_dir():
                continue
            if Path(info.filename.replace("\\", "/")).name.lower() == target_name:
                return archive.read(info.filename)
    raise FileNotFoundError(f"{zip_path}: missing entry named {basename}")


def load_submap_rows(game_db: Path) -> list[SubMapRow]:
    conn = sqlite3.connect(game_db)
    conn.row_factory = sqlite3.Row
    try:
        rows = conn.execute("select rowid, 编号, 名称 from submap order by rowid").fetchall()
    finally:
        conn.close()

    out: list[SubMapRow] = []
    for chunk_index, row in enumerate(rows):
        out.append(
            SubMapRow(
                chunk_index=chunk_index,
                submap_id=int(row["编号"]),
                name=str(row["名称"] or ""),
            )
        )
    return out


def decode_battle_name(raw_name: bytes) -> str:
    return raw_name.split(b"\x00", 1)[0].decode("cp950", errors="ignore")


def load_battle_info_rows(war_sta_path: Path) -> list[BattleInfoRow]:
    payload = war_sta_path.read_bytes()
    if len(payload) % BATTLE_INFO_RECORD_SIZE != 0:
        raise ValueError(
            f"{war_sta_path}: unexpected war.sta size {len(payload)} for record size {BATTLE_INFO_RECORD_SIZE}"
        )

    rows: list[BattleInfoRow] = []
    for offset in range(0, len(payload), BATTLE_INFO_RECORD_SIZE):
        chunk = payload[offset:offset + BATTLE_INFO_RECORD_SIZE]
        battle_id = struct.unpack_from("<h", chunk, 0)[0]
        battlefield_id = struct.unpack_from("<h", chunk, 12)[0]
        rows.append(
            BattleInfoRow(
                battle_id=battle_id,
                battlefield_id=battlefield_id,
                name=decode_battle_name(chunk[2:12]),
            )
        )
    return rows


def read_int32_array(payload: bytes) -> array.array:
    if len(payload) % 4 != 0:
        raise ValueError(f"unexpected int32 payload size {len(payload)}")
    values = array.array("i")
    values.frombytes(payload)
    if sys.byteorder != "little":
        values.byteswap()
    return values


def load_battle_field_blocks(warfld_idx_path: Path, warfld_grp_path: Path) -> list[bytes]:
    offsets = read_int32_array(warfld_idx_path.read_bytes())
    grp_bytes = read_binary_with_zip_fallback(warfld_grp_path)
    chunks: list[bytes] = []
    previous = 0
    for end_offset in offsets:
        end = int(end_offset)
        if end < previous or end > len(grp_bytes):
            raise ValueError(f"{warfld_idx_path}: invalid offset {end} for warfld.grp size {len(grp_bytes)}")
        chunk = grp_bytes[previous:end]
        if len(chunk) < BATTLE_FIELD_BLOCK_LENGTH:
            raise ValueError(
                f"battlefield chunk {len(chunks)} too short: expected at least {BATTLE_FIELD_BLOCK_LENGTH}, got {len(chunk)}"
            )
        chunks.append(chunk[:BATTLE_FIELD_BLOCK_LENGTH])
        previous = end
    return chunks


def bytes_to_int16_array(payload: bytes) -> array.array:
    values = array.array("h")
    values.frombytes(payload)
    if sys.byteorder != "little":
        values.byteswap()
    return values


def load_snapshot(row: SubMapRow, sdata: bytes, ddata: bytes) -> SubMapSnapshot:
    s_offset = row.chunk_index * SDATA_LENGTH
    d_offset = row.chunk_index * DDATA_LENGTH
    layer_block = sdata[s_offset:s_offset + SDATA_LENGTH]
    event_block = ddata[d_offset:d_offset + DDATA_LENGTH]
    if len(layer_block) != SDATA_LENGTH:
        raise ValueError(f"submap {row.submap_id}: incomplete allsin chunk")
    if len(event_block) != DDATA_LENGTH:
        raise ValueError(f"submap {row.submap_id}: incomplete alldef chunk")
    return SubMapSnapshot(
        row=row,
        layer_values=bytes_to_int16_array(layer_block),
        event_values=bytes_to_int16_array(event_block),
    )


def load_battlefield_snapshot(battlefield_id: int, field_blocks: list[bytes]) -> BattleFieldSnapshot:
    if battlefield_id < 0 or battlefield_id >= len(field_blocks):
        raise ValueError(f"unknown battlefield ID: {battlefield_id}")
    return BattleFieldSnapshot(
        battlefield_id=battlefield_id,
        layer_values=bytes_to_int16_array(field_blocks[battlefield_id]),
    )


def cxx_div2(value: int) -> int:
    if value >= 0:
        return value // 2
    return -((-value) // 2)


def whole_earth_position(x: int, y: int) -> tuple[int, int]:
    return (-y * TILE_W + x * TILE_W + COORD_COUNT * TILE_W, y * TILE_H + x * TILE_H + 2 * TILE_H)


def alpha_composite_clipped(canvas: Image.Image, overlay: Image.Image, left: int, top: int) -> None:
    if left >= canvas.width or top >= canvas.height:
        return
    if left + overlay.width <= 0 or top + overlay.height <= 0:
        return

    src_left = max(0, -left)
    src_top = max(0, -top)
    dest_left = max(0, left)
    dest_top = max(0, top)
    width = min(canvas.width - dest_left, overlay.width - src_left)
    height = min(canvas.height - dest_top, overlay.height - src_top)
    if width <= 0 or height <= 0:
        return

    cropped = overlay.crop((src_left, src_top, src_left + width, src_top + height))
    canvas.alpha_composite(cropped, (dest_left, dest_top))


def iter_diagonal_tiles() -> list[tuple[int, int]]:
    out: list[tuple[int, int]] = []
    for sum_xy in range(0, COORD_COUNT * 2 - 1):
        x_start = max(0, sum_xy - (COORD_COUNT - 1))
        x_end = min(COORD_COUNT - 1, sum_xy)
        for x in range(x_start, x_end + 1):
            out.append((x, sum_xy - x))
    return out


DIAGONAL_TILES = iter_diagonal_tiles()


def render_submap(
    snapshot: SubMapSnapshot,
    smap: SmapArchive,
    layers: tuple[str, ...],
    variant_frame: int,
    opaque_background: bool,
) -> Image.Image:
    background_alpha = 255 if opaque_background else 0
    canvas = Image.new("RGBA", (CANVAS_WIDTH, CANVAS_HEIGHT), (0, 0, 0, background_alpha))
    enabled = set(layers)

    if "earth" in enabled:
        for x, y in DIAGONAL_TILES:
            earth = cxx_div2(snapshot.earth(x, y))
            if earth <= 0:
                continue
            if snapshot.building_height(x, y) > 2:
                continue
            anchor_x, anchor_y = whole_earth_position(x, y)
            smap.draw(canvas, earth, anchor_x, anchor_y, variant_frame)

    for x, y in DIAGONAL_TILES:
        anchor_x, anchor_y = whole_earth_position(x, y)
        building_height = snapshot.building_height(x, y)

        if "earth" in enabled:
            earth = cxx_div2(snapshot.earth(x, y))
            if earth > 0 and building_height > 2:
                smap.draw(canvas, earth, anchor_x, anchor_y, variant_frame)

        if "building" in enabled:
            building = cxx_div2(snapshot.building(x, y))
            if building > 0:
                smap.draw(canvas, building, anchor_x, anchor_y - building_height, variant_frame)

        if "event" in enabled:
            event_pic = cxx_div2(snapshot.event_pic_at(x, y))
            if event_pic > 0:
                smap.draw(canvas, event_pic, anchor_x, anchor_y - building_height, variant_frame)

        if "decoration" in enabled:
            decoration = cxx_div2(snapshot.decoration(x, y))
            if decoration > 0:
                smap.draw(canvas, decoration, anchor_x, anchor_y - snapshot.decoration_height(x, y), variant_frame)

    return canvas


def render_battlefield(
    snapshot: BattleFieldSnapshot,
    smap: SmapArchive,
    layers: tuple[str, ...],
    variant_frame: int,
    opaque_background: bool,
) -> Image.Image:
    background_alpha = 255 if opaque_background else 0
    canvas = Image.new("RGBA", (CANVAS_WIDTH, CANVAS_HEIGHT), (0, 0, 0, background_alpha))
    enabled = set(layers)

    if "earth" in enabled:
        for x in range(COORD_COUNT):
            for y in range(COORD_COUNT):
                earth = cxx_div2(snapshot.earth(x, y))
                if earth <= 0:
                    continue
                anchor_x, anchor_y = whole_earth_position(x, y)
                smap.draw(canvas, earth, anchor_x, anchor_y, variant_frame)

    if "building" in enabled:
        for x, y in DIAGONAL_TILES:
            building = cxx_div2(snapshot.building(x, y))
            if building <= 0:
                continue
            anchor_x, anchor_y = whole_earth_position(x, y)
            smap.draw(canvas, building, anchor_x, anchor_y, variant_frame)

    return canvas


def layer_suffix(layers: tuple[str, ...]) -> str:
    if tuple(layers) == SUBSCENE_DRAW_LAYERS or tuple(layers) == BATTLE_DRAW_LAYERS:
        return "all"
    return "-".join(layers)


def validate_data_lengths(rows: list[SubMapRow], sdata: bytes, ddata: bytes) -> None:
    expected_sdata = len(rows) * SDATA_LENGTH
    expected_ddata = len(rows) * DDATA_LENGTH
    if len(sdata) != expected_sdata:
        raise ValueError(f"allsin data size mismatch: expected {expected_sdata}, got {len(sdata)}")
    if len(ddata) != expected_ddata:
        raise ValueError(f"alldef data size mismatch: expected {expected_ddata}, got {len(ddata)}")


def select_submap_rows(rows: list[SubMapRow], requested_ids: set[int]) -> list[SubMapRow]:
    selected_rows = [row for row in rows if not requested_ids or row.submap_id in requested_ids]
    if requested_ids:
        found_ids = {row.submap_id for row in selected_rows}
        missing_ids = sorted(requested_ids - found_ids)
        if missing_ids:
            raise ValueError(f"unknown submap IDs: {', '.join(str(value) for value in missing_ids)}")
    return selected_rows


def select_battle_targets(
    battle_rows: list[BattleInfoRow],
    battle_ids: set[int],
    battlefield_ids: set[int],
    available_field_count: int,
) -> tuple[list[BattleInfoRow], list[int]]:
    selected_battles: list[BattleInfoRow] = []
    if battle_ids:
        selected_battles = [row for row in battle_rows if row.battle_id in battle_ids]
        found_battle_ids = {row.battle_id for row in selected_battles}
        missing_battle_ids = sorted(battle_ids - found_battle_ids)
        if missing_battle_ids:
            raise ValueError(f"unknown battle IDs: {', '.join(str(value) for value in missing_battle_ids)}")

    if battlefield_ids:
        invalid_field_ids = sorted(
            battlefield_id
            for battlefield_id in battlefield_ids
            if battlefield_id < 0 or battlefield_id >= available_field_count
        )
        if invalid_field_ids:
            raise ValueError(f"unknown battlefield IDs: {', '.join(str(value) for value in invalid_field_ids)}")
        selected_fields = sorted(battlefield_ids)
    elif not selected_battles:
        selected_fields = sorted({row.battlefield_id for row in battle_rows})
    else:
        selected_fields = []

    return selected_battles, selected_fields


def main() -> int:
    args = parse_args()
    try:
        subscene_layers = parse_layers(args.layers, SUBSCENE_DRAW_LAYERS)
        battle_layers = parse_layers(args.battle_layers, BATTLE_DRAW_LAYERS)
        work_root = args.work_root.resolve()
        output_dir = Path(args.output_dir).resolve()
        output_dir.mkdir(parents=True, exist_ok=True)

        smap_source = resolve_resource_path(
            args.smap_zip,
            work_root / "resource" / "smap.zip",
            work_root / "resource" / "smap",
        )
        game_db = resolve_resource_path(args.game_db, work_root / "save" / "game.db")
        sdata_path = resolve_resource_path(args.sdata, work_root / "save" / "allsin.grp", work_root / "save" / "allsin.grp.zip")
        ddata_path = resolve_resource_path(args.ddata, work_root / "save" / "alldef.grp", work_root / "save" / "alldef.grp.zip")
        war_sta_path = resolve_resource_path(args.war_sta, work_root / "resource" / "war.sta")
        warfld_idx_path = resolve_resource_path(args.warfld_idx, work_root / "resource" / "warfld.idx")
        warfld_grp_path = resolve_resource_path(
            args.warfld_grp,
            work_root / "resource" / "warfld.grp",
            work_root / "resource" / "warfld.grp.zip",
        )

        smap = SmapArchive(smap_source)
        try:
            written_files: list[Path] = []
            summary_parts: list[str] = []

            if args.mode in {"subscene", "both"}:
                rows = load_submap_rows(game_db)
                sdata = read_binary_with_zip_fallback(sdata_path)
                ddata = read_binary_with_zip_fallback(ddata_path)
                validate_data_lengths(rows, sdata, ddata)

                requested_ids = set(args.submap_ids or [])
                selected_rows = select_submap_rows(rows, requested_ids)
                suffix = layer_suffix(subscene_layers)
                for row in selected_rows:
                    snapshot = load_snapshot(row, sdata, ddata)
                    image = render_submap(snapshot, smap, subscene_layers, args.variant_frame, args.opaque_background)
                    output_path = output_dir / f"{row.submap_id:03d}_{suffix}.png"
                    image.save(output_path)
                    written_files.append(output_path)
                summary_parts.append(f"{len(selected_rows)} submap PNG(s) layers={suffix}")

            if args.mode in {"battle", "both"}:
                battle_rows = load_battle_info_rows(war_sta_path)
                field_blocks = load_battle_field_blocks(warfld_idx_path, warfld_grp_path)
                selected_battles, selected_fields = select_battle_targets(
                    battle_rows,
                    set(args.battle_ids or []),
                    set(args.battlefield_ids or []),
                    len(field_blocks),
                )

                suffix = layer_suffix(battle_layers)
                rendered_field_ids: set[int] = set()

                for row in selected_battles:
                    snapshot = load_battlefield_snapshot(row.battlefield_id, field_blocks)
                    image = render_battlefield(snapshot, smap, battle_layers, args.variant_frame, args.opaque_background)
                    output_path = output_dir / f"battle_{row.battle_id:03d}_field_{row.battlefield_id:03d}_{suffix}.png"
                    image.save(output_path)
                    written_files.append(output_path)
                    rendered_field_ids.add(row.battlefield_id)

                for battlefield_id in selected_fields:
                    snapshot = load_battlefield_snapshot(battlefield_id, field_blocks)
                    image = render_battlefield(snapshot, smap, battle_layers, args.variant_frame, args.opaque_background)
                    output_path = output_dir / f"battlefield_{battlefield_id:03d}_{suffix}.png"
                    image.save(output_path)
                    written_files.append(output_path)
                    rendered_field_ids.add(battlefield_id)

                summary_parts.append(
                    f"{len(selected_battles)} battle PNG(s) + {len(selected_fields)} battlefield PNG(s) layers={suffix}"
                )

            print(f"rendered {', '.join(summary_parts)} to {output_dir} using smap={smap_source.name}")
            if smap.missing_ids:
                sample = ", ".join(str(value) for value in sorted(smap.missing_ids)[:10])
                print(
                    f"skipped {len(smap.missing_ids)} missing smap tile id(s): {sample}",
                    file=sys.stderr,
                )
        finally:
            smap.close()
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())