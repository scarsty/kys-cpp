#!/usr/bin/env python3
"""Pack only the SMAP assets needed for chess battle into a zip archive.

The packer derives its input set from the live game data used by chess battle:
- curated battle IDs listed in ``src/ChessBattleMapCatalog.cpp``
- battle-to-battlefield mappings from ``work/game-dev/resource/war.sta``
- battlefield layers from ``work/game-dev/resource/warfld.idx`` + ``warfld.grp``
- main scene submap 53 from ``work/game-dev/save/allsin.grp`` + ``alldef.grp``

The output zip keeps the original file names so the runtime can use it as a
drop-in ``smap.zip`` replacement.
"""
from __future__ import annotations

import argparse
import array
import re
import sqlite3
import struct
import sys
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WORK_ROOT = ROOT / "work" / "game-dev"
DEFAULT_OUTPUT = DEFAULT_WORK_ROOT / "resource" / "smap.chess-battle.zip"
DEFAULT_BATTLE_MAP_CATALOG = ROOT / "src" / "ChessBattleMapCatalog.cpp"
DEFAULT_MAIN_SUBMAP_ID = 53

SUBMAP_COORD_COUNT = 64
SUBMAP_LAYER_COUNT = 6
SUBMAP_EVENT_COUNT = 200
SUBMAP_CELL_COUNT = SUBMAP_COORD_COUNT * SUBMAP_COORD_COUNT
BATTLEMAP_COORD_COUNT = 64
BATTLEMAP_SAVE_LAYER_COUNT = 2
BATTLEMAP_CELL_COUNT = BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT
BATTLE_INFO_RECORD_SIZE = 186
BATTLE_INFO_ID_OFFSET = 0
BATTLE_INFO_BATTLEFIELD_ID_OFFSET = 12

SUBMAP_EXTRA_IDS = {1, *range(2501, 2529)}


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def read_packaged_bytes(path: Path) -> bytes:
    zip_path = path.with_suffix(path.suffix + ".zip")
    if zip_path.is_file() and zipfile.is_zipfile(zip_path):
        with zipfile.ZipFile(zip_path, "r") as archive:
            return archive.read(path.name)
    return path.read_bytes()


def read_int16_array(payload: bytes) -> array.array:
    values = array.array("h")
    values.frombytes(payload)
    if sys.byteorder != "little":
        values.byteswap()
    return values


def count_sqlite_rows(db_path: Path, table_name: str) -> int:
    with sqlite3.connect(db_path) as connection:
        cursor = connection.execute(f"select count(*) from {table_name}")
        row = cursor.fetchone()
        if row is None:
            raise RuntimeError(f"Failed to count rows in {table_name} from {db_path}")
        return int(row[0])


def parse_curated_battle_ids(catalog_path: Path) -> list[int]:
    text = read_text(catalog_path)
    declaration = "static const std::vector<ChessBattleMapCatalogEntry> maps"
    declaration_offset = text.find(declaration)
    if declaration_offset < 0:
        raise RuntimeError(f"Curated battle map catalog was not found in {catalog_path}")

    return_offset = text.find("return maps;", declaration_offset)
    if return_offset < 0:
        raise RuntimeError(f"Curated battle map catalog terminator was not found in {catalog_path}")

    catalog_text = text[declaration_offset:return_offset]
    matches = re.findall(
        r"(?m)^\s*\{\s*$\n\s*(\d+)\s*,\s*$\n\s*\d+\s*,\s*$\n\s*\{\{",
        catalog_text,
    )
    battle_ids = [int(battle_id) for battle_id in matches]
    if not battle_ids:
        raise RuntimeError(f"No curated battle maps found in {catalog_path}")
    if len(battle_ids) != len(set(battle_ids)):
        raise RuntimeError(f"Duplicate curated battle IDs found in {catalog_path}")
    return battle_ids


def read_battlefield_ids_by_battle_id(war_sta_path: Path) -> dict[int, int]:
    payload = read_packaged_bytes(war_sta_path)
    if len(payload) % BATTLE_INFO_RECORD_SIZE != 0:
        raise RuntimeError(
            f"{war_sta_path} size {len(payload)} is not divisible by "
            f"BattleInfo record size {BATTLE_INFO_RECORD_SIZE}"
        )

    result: dict[int, int] = {}
    for record_offset in range(0, len(payload), BATTLE_INFO_RECORD_SIZE):
        battle_id = struct.unpack_from(
            "<h",
            payload,
            record_offset + BATTLE_INFO_ID_OFFSET,
        )[0]
        battlefield_id = struct.unpack_from(
            "<h",
            payload,
            record_offset + BATTLE_INFO_BATTLEFIELD_ID_OFFSET,
        )[0]
        # Match ChessContentLoader's std::map::emplace behavior: the first
        # record for a battle ID is authoritative and later duplicates are ignored.
        result.setdefault(battle_id, battlefield_id)
    return result


def resolve_curated_battlefields(
    catalog_path: Path,
    war_sta_path: Path,
) -> list[tuple[int, int]]:
    battle_ids = parse_curated_battle_ids(catalog_path)
    battlefield_ids = read_battlefield_ids_by_battle_id(war_sta_path)
    missing = [battle_id for battle_id in battle_ids if battle_id not in battlefield_ids]
    if missing:
        raise RuntimeError(
            f"Curated battle IDs missing from {war_sta_path}: "
            f"{', '.join(map(str, missing))}"
        )
    return [(battle_id, battlefield_ids[battle_id]) for battle_id in battle_ids]


def read_cumulative_offsets(idx_path: Path) -> list[int]:
    payload = read_packaged_bytes(idx_path)
    if len(payload) % 4 != 0:
        raise RuntimeError(f"{idx_path} size {len(payload)} is not divisible by 4")
    offsets = [0]
    for offset in struct.iter_unpack("<i", payload):
        offsets.append(int(offset[0]))
    return offsets


def collect_ids_from_submap(work_root: Path, submap_id: int) -> set[int]:
    save_root = work_root / "save"
    db_path = save_root / "game.db"
    submap_count = count_sqlite_rows(db_path, "submap")
    if submap_id < 0 or submap_id >= submap_count:
        raise RuntimeError(f"Submap {submap_id} is out of range for {db_path} (count={submap_count})")

    sdata = read_packaged_bytes(save_root / "allsin.grp")
    ddata = read_packaged_bytes(save_root / "alldef.grp")
    if len(sdata) % submap_count != 0:
        raise RuntimeError(f"{save_root / 'allsin.grp'} size {len(sdata)} is not divisible by submap count {submap_count}")
    if len(ddata) % submap_count != 0:
        raise RuntimeError(f"{save_root / 'alldef.grp'} size {len(ddata)} is not divisible by submap count {submap_count}")

    sdata_record_size = len(sdata) // submap_count
    ddata_record_size = len(ddata) // submap_count
    sdata_offset = submap_id * sdata_record_size
    ddata_offset = submap_id * ddata_record_size
    sdata_record = sdata[sdata_offset : sdata_offset + sdata_record_size]
    ddata_record = ddata[ddata_offset : ddata_offset + ddata_record_size]

    if len(sdata_record) != sdata_record_size or len(ddata_record) != ddata_record_size:
        raise RuntimeError(f"Submap {submap_id} record could not be read from packaged save data")

    ids: set[int] = set(SUBMAP_EXTRA_IDS)

    layer_values = read_int16_array(sdata_record)
    layer_limit = SUBMAP_LAYER_COUNT * SUBMAP_CELL_COUNT
    for value in layer_values[:layer_limit]:
        if value > 0:
            ids.add(value // 2)

    event_record_size = ddata_record_size // SUBMAP_EVENT_COUNT
    current_pic_offset = 5 * 2
    for event_index in range(SUBMAP_EVENT_COUNT):
        event_offset = event_index * event_record_size + current_pic_offset
        current_pic = struct.unpack_from("<h", ddata_record, event_offset)[0]
        if current_pic > 0:
            ids.add(current_pic // 2)

    return ids


def collect_ids_from_battlefield(work_root: Path, battlefield_id: int) -> set[int]:
    battle_root = work_root / "resource"
    idx_path = battle_root / "warfld.idx"
    grp_path = battle_root / "warfld.grp"
    offsets = read_cumulative_offsets(idx_path)
    if battlefield_id < 0 or battlefield_id + 1 >= len(offsets):
        raise RuntimeError(f"Battlefield {battlefield_id} is out of range for {idx_path}")

    grp_payload = read_packaged_bytes(grp_path)
    start = offsets[battlefield_id]
    end = offsets[battlefield_id + 1]
    if end > len(grp_payload):
        raise RuntimeError(f"Battlefield {battlefield_id} extends beyond {grp_path}")

    record = grp_payload[start:end]
    expected_min = BATTLEMAP_SAVE_LAYER_COUNT * BATTLEMAP_CELL_COUNT * 2
    if len(record) < expected_min:
        raise RuntimeError(f"Battlefield {battlefield_id} record is too small: {len(record)} bytes")

    ids: set[int] = set()
    layer_values = read_int16_array(record[:expected_min])
    for value in layer_values:
        if value > 0:
            ids.add(value // 2)
    return ids


def resolve_texture_file(source_dir: Path, base_name: str) -> Path | None:
    for suffix in (".webp", ".png"):
        candidate = source_dir / f"{base_name}{suffix}"
        if candidate.is_file():
            return candidate
    return None


def gather_texture_files(source_dir: Path, ids: set[int]) -> list[Path]:
    files: list[Path] = []
    seen: set[Path] = set()
    for texture_id in sorted(ids):
        base_name = str(texture_id)
        texture = resolve_texture_file(source_dir, base_name)
        if texture and texture not in seen:
            seen.add(texture)
            files.append(texture)
        for variant_index in range(10):
            variant_base = f"{base_name}_{variant_index}"
            texture = resolve_texture_file(source_dir, variant_base)
            if texture and texture not in seen:
                seen.add(texture)
                files.append(texture)
    return sorted(files, key=lambda path: path.name.lower())


def write_zip(output_path: Path, index_path: Path, texture_files: list[Path]) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(delete=False, dir=output_path.parent, suffix=".tmp") as temp_file:
        temp_path = Path(temp_file.name)

    try:
        with zipfile.ZipFile(temp_path, "w", compression=zipfile.ZIP_STORED) as archive:
            archive.write(index_path, arcname="index.ka")
            for texture_file in texture_files:
                archive.write(texture_file, arcname=texture_file.name)
        temp_path.replace(output_path)
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Pack the chess-battle SMAP subset into a zip archive")
    parser.add_argument(
        "--work-root",
        type=Path,
        default=DEFAULT_WORK_ROOT,
        help="Base game working directory (default: work/game-dev)",
    )
    parser.add_argument(
        "--battle-map-catalog",
        type=Path,
        default=DEFAULT_BATTLE_MAP_CATALOG,
        help="C++ source file that defines the curated auto-chess battle map catalog",
    )
    parser.add_argument(
        "--main-submap-id",
        type=int,
        default=DEFAULT_MAIN_SUBMAP_ID,
        help="Main scene submap ID to include (default: 53)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="Output zip path (default: work/game-dev/resource/smap.chess-battle.zip)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    work_root = args.work_root.resolve()
    source_dir = work_root / "resource" / "smap"
    index_path = source_dir / "index.ka"

    if not source_dir.is_dir():
        raise SystemExit(f"SMAP source directory not found: {source_dir}")
    if not index_path.is_file():
        raise SystemExit(f"Missing index.ka: {index_path}")

    curated_battles = resolve_curated_battlefields(
        args.battle_map_catalog.resolve(),
        work_root / "resource" / "war.sta",
    )
    selected_ids = collect_ids_from_submap(work_root, args.main_submap_id)
    selected_battlefield_ids: set[int] = set()
    for _, battlefield_id in curated_battles:
        selected_battlefield_ids.add(battlefield_id)
        selected_ids.update(collect_ids_from_battlefield(work_root, battlefield_id))

    texture_files = gather_texture_files(source_dir, selected_ids)
    missing_ids = sorted(
        texture_id
        for texture_id in selected_ids
        if resolve_texture_file(source_dir, str(texture_id)) is None
        and not any(resolve_texture_file(source_dir, f"{texture_id}_{variant_index}") for variant_index in range(10))
    )

    output_path = args.output.resolve()
    write_zip(output_path, index_path, texture_files)

    print(f"curated battles: {len(curated_battles)}")
    print(f"battlefields included: {len(selected_battlefield_ids)}")
    print(f"selected texture ids: {len(selected_ids)}")
    print(f"packed texture files: {len(texture_files)}")
    print(f"wrote zip: {output_path}")
    if missing_ids:
        print(f"missing texture ids: {', '.join(map(str, missing_ids[:50]))}")
        if len(missing_ids) > 50:
            print(f"missing texture ids truncated: {len(missing_ids) - 50} more")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
