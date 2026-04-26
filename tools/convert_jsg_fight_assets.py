#!/usr/bin/env python3
from __future__ import annotations

import argparse
import collections
import io
import json
import shutil
import struct
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from PIL import Image

from tools.jsg_legacy import ROOT, dedupe_rows, find_fight_path, iter_jsg_rows


DEFAULT_OUTPUT_DIR = ROOT / "output" / "jsg_fight_zips"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build modern fight zip assets from JSG legacy fight ids by copying the source zip payload and injecting metadata from ranger.grp"
    )
    parser.add_argument("ids", nargs="*", type=int, help="Record ids or fight/head ids to convert. Defaults to all unique fight ids.")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR, help="Directory for generated fight zips")
    parser.add_argument("--overwrite", action="store_true", help="Overwrite existing output zips")
    return parser.parse_args()


def load_idx_entry_count(path: Path) -> int:
    raw = path.read_bytes()
    if len(raw) % 4 != 0:
        raise RuntimeError(f"Unexpected idx size: {path}")
    return len(raw) // 4


def load_idx_offsets(path: Path) -> list[int]:
    raw = path.read_bytes()
    if len(raw) % 4 != 0:
        raise RuntimeError(f"Unexpected idx size: {path}")
    return list(struct.unpack('<' + 'I' * (len(raw) // 4), raw))


def build_palette_from_existing_zips(fight_root: Path) -> dict[int, tuple[int, int, int, int]]:
    votes: dict[int, collections.Counter[tuple[int, int, int, int]]] = collections.defaultdict(collections.Counter)
    for zpath in sorted(fight_root.glob("fight*.zip")):
        stem = zpath.stem
        idx_path = fight_root / f"{stem}.idx"
        grp_path = fight_root / f"{stem}.grp"
        if not idx_path.exists() or not grp_path.exists():
            continue

        grp_bytes = grp_path.read_bytes()
        if grp_bytes.startswith(PNG_SIGNATURE):
            continue

        offsets = load_idx_offsets(idx_path)
        starts = [0] + offsets[:-1]
        with zipfile.ZipFile(zpath) as zf:
            pngs = sorted(
                [name for name in zf.namelist() if name.lower().endswith('.png')],
                key=lambda name: int(Path(name).stem),
            )
            for frame_no, png_name in enumerate(pngs):
                chunk = grp_bytes[starts[frame_no]:offsets[frame_no]]
                parsed = parse_legacy_chunk(chunk)
                if parsed is None:
                    continue
                image = Image.open(io.BytesIO(zf.read(png_name))).convert("RGBA")
                if image.size != (parsed["width"], parsed["height"]):
                    continue
                for y, row in enumerate(parsed["rows"]):
                    for x, palette_index in iter_row_packets(row):
                        votes[palette_index][image.getpixel((x, y))] += 1

    palette = {palette_index: counter.most_common(1)[0][0] for palette_index, counter in votes.items()}
    if not palette:
        raise RuntimeError("Failed to derive any palette entries from existing JSG fight zips")
    return palette


def parse_legacy_chunk(chunk: bytes) -> dict[str, object] | None:
    if len(chunk) < 8 or chunk.startswith(PNG_SIGNATURE):
        return None
    width, height, offset_x, offset_y = struct.unpack('<4H', chunk[:8])
    payload = chunk[8:]
    rows: list[bytes] = []
    position = 0
    for _ in range(height):
        if position >= len(payload):
            return None
        row_size = payload[position]
        position += 1
        if position + row_size > len(payload):
            return None
        rows.append(payload[position:position + row_size])
        position += row_size
    return {
        "width": width,
        "height": height,
        "offset_x": offset_x,
        "offset_y": offset_y,
        "rows": rows,
    }


def iter_row_packets(row: bytes):
    cursor = 0
    position = 0
    while position < len(row):
        if position + 2 > len(row):
            raise RuntimeError(f"Truncated row packet: {row.hex(' ')}")
        skip = row[position]
        run = row[position + 1]
        position += 2
        if position + run > len(row):
            raise RuntimeError(f"Invalid row packet run: {row.hex(' ')}")
        cursor += skip
        for palette_index in row[position:position + run]:
            yield cursor, palette_index
            cursor += 1
        position += run


def decode_legacy_chunk(chunk: bytes, palette: dict[int, tuple[int, int, int, int]]) -> tuple[bytes, tuple[int, int]]:
    parsed = parse_legacy_chunk(chunk)
    if parsed is None:
        raise RuntimeError("Chunk is not a valid legacy frame")
    width = int(parsed["width"])
    height = int(parsed["height"])
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    for y, row in enumerate(parsed["rows"]):
        for x, palette_index in iter_row_packets(row):
            color = palette.get(palette_index)
            if color is None:
                raise RuntimeError(f"Missing palette entry {palette_index}")
            image.putpixel((x, y), color)
    buf = io.BytesIO()
    image.save(buf, format="PNG")
    return buf.getvalue(), (int(parsed["offset_x"]), int(parsed["offset_y"]))


def read_png_offset(png_bytes: bytes) -> tuple[int, int] | None:
    if not png_bytes.startswith(PNG_SIGNATURE):
        return None
    position = 8
    while position + 8 <= len(png_bytes):
        chunk_length = struct.unpack('>I', png_bytes[position:position + 4])[0]
        chunk_type = png_bytes[position + 4:position + 8]
        chunk_data = png_bytes[position + 8:position + 8 + chunk_length]
        if chunk_type == b'oFFs' and len(chunk_data) >= 9:
            x, y, unit = struct.unpack('>iiB', chunk_data[:9])
            if unit == 0:
                return x, y
        position += 12 + chunk_length
        if chunk_type == b'IEND':
            break
    return None


def default_png_offset(png_bytes: bytes) -> tuple[int, int]:
    image = Image.open(io.BytesIO(png_bytes)).convert("RGBA")
    return image.width // 2, image.height // 2


def synthesize_frames_from_grp(head_id: int, palette: dict[int, tuple[int, int, int, int]]) -> list[dict[str, object]]:
    idx_path = find_fight_path(head_id, ".idx")
    grp_path = find_fight_path(head_id, ".grp")
    if idx_path is None or grp_path is None:
        raise FileNotFoundError(f"Missing .grp/.idx source for fight {head_id}")

    grp_bytes = grp_path.read_bytes()
    offsets = load_idx_offsets(idx_path)
    starts = [0] + offsets[:-1]
    frames: list[dict[str, object]] = []
    for frame_index, (start, end) in enumerate(zip(starts, offsets)):
        chunk = grp_bytes[start:end]
        if chunk.startswith(PNG_SIGNATURE):
            png_bytes = bytes(chunk)
            offset = read_png_offset(png_bytes) or default_png_offset(png_bytes)
        else:
            png_bytes, offset = decode_legacy_chunk(chunk, palette)
        frames.append(
            {
                "name": f"{frame_index}.png",
                "png_bytes": png_bytes,
                "offset": offset,
                "source_mode": "png-chunk" if chunk.startswith(PNG_SIGNATURE) else "legacy-row",
            }
        )
    return frames


def write_generated_zip(destination_zip: Path, frames: list[dict[str, object]], row: dict[str, object], legacy_entry_count: int) -> None:
    destination_zip.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(suffix=".zip", dir=destination_zip.parent, delete=False) as tmp:
        temp_zip = Path(tmp.name)

    index_values: list[int] = []
    for frame in frames:
        offset_x, offset_y = frame["offset"]
        index_values.extend((int(offset_x), int(offset_y)))

    metadata = {
        "record_id": int(row["record_id"]),
        "head_id": int(row["head_id"]),
        "name": str(row["name"]),
        "nickname": str(row["nickname"]),
        "legacy_entry_count": legacy_entry_count,
        "zip_png_count": len(frames),
        "frame_count_matches_legacy_idx": len(frames) == legacy_entry_count,
        "attack_frame": int(row["attack_frame"]),
        "attack_sound_frame": int(row["attack_sound_frame"]),
        "frame_num": list(row["frame_num"]),
        "frame_delay": list(row["frame_delay"]),
        "sound_delay": list(row["sound_delay"]),
        "generated_from_grp": True,
        "frame_source_modes": dict(collections.Counter(str(frame["source_mode"]) for frame in frames)),
    }

    with zipfile.ZipFile(temp_zip, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for frame in frames:
            zf.writestr(str(frame["name"]), frame["png_bytes"])
        zf.writestr("index.ka", struct.pack('<' + 'h' * len(index_values), *index_values))
        zf.writestr("fightframe.txt", str(row["fightframe_text"]))
        zf.writestr("attackframe.txt", f"{int(row['attack_frame'])}\n")
        zf.writestr("jsg-source.json", json.dumps(metadata, ensure_ascii=False, indent=2) + "\n")

    shutil.move(str(temp_zip), destination_zip)


def select_rows(selected_ids: list[int]) -> list[dict[str, object]]:
    rows = dedupe_rows(iter_jsg_rows())
    if not selected_ids:
        by_head: dict[int, dict[str, object]] = {}
        for row in rows:
            head_id = int(row["head_id"])
            by_head.setdefault(head_id, row)
        return sorted(by_head.values(), key=lambda row: int(row["head_id"]))

    selected = set(selected_ids)
    chosen: dict[int, dict[str, object]] = {}
    for row in rows:
        record_id = int(row["record_id"])
        head_id = int(row["head_id"])
        if record_id in selected or head_id in selected:
            chosen.setdefault(head_id, row)
    missing = sorted(selected - {int(row["record_id"]) for row in chosen.values()} - {int(row["head_id"]) for row in chosen.values()})
    if missing:
        raise RuntimeError(f"Could not resolve requested ids: {missing}")
    return sorted(chosen.values(), key=lambda row: int(row["head_id"]))


def write_zip_with_metadata(source_zip: Path, destination_zip: Path, row: dict[str, object], legacy_entry_count: int) -> None:
    destination_zip.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(suffix=".zip", dir=destination_zip.parent, delete=False) as tmp:
        temp_zip = Path(tmp.name)

    with zipfile.ZipFile(source_zip, "r") as zin, zipfile.ZipFile(temp_zip, "w", compression=zipfile.ZIP_DEFLATED) as zout:
        png_entries = 0
        for info in zin.infolist():
            data = zin.read(info.filename)
            normalized = info.filename.replace("\\", "/")
            if normalized.lower() in {"fightframe.txt", "attackframe.txt", "jsg-source.json"}:
                continue
            if normalized.lower().endswith(".png"):
                png_entries += 1
            zout.writestr(info, data)

        frame_count_matches = png_entries == legacy_entry_count
        metadata = {
            "record_id": int(row["record_id"]),
            "head_id": int(row["head_id"]),
            "name": str(row["name"]),
            "nickname": str(row["nickname"]),
            "legacy_entry_count": legacy_entry_count,
            "zip_png_count": png_entries,
            "frame_count_matches_legacy_idx": frame_count_matches,
            "attack_frame": int(row["attack_frame"]),
            "attack_sound_frame": int(row["attack_sound_frame"]),
            "frame_num": list(row["frame_num"]),
            "frame_delay": list(row["frame_delay"]),
            "sound_delay": list(row["sound_delay"]),
        }
        zout.writestr("fightframe.txt", str(row["fightframe_text"]))
        zout.writestr("attackframe.txt", f"{int(row['attack_frame'])}\n")
        zout.writestr("jsg-source.json", json.dumps(metadata, ensure_ascii=False, indent=2) + "\n")

    shutil.move(str(temp_zip), destination_zip)
    if not frame_count_matches:
        print(
            f"warn {source_zip.name}: zip has {png_entries} png frames but legacy idx has {legacy_entry_count} entries"
        )


def main() -> int:
    args = parse_args()
    rows = select_rows(args.ids)
    converted = 0
    skipped_missing_zip: list[int] = []
    skipped_missing_idx: list[int] = []
    skipped_missing_grp: list[int] = []
    synthesized_from_grp: list[int] = []
    palette: dict[int, tuple[int, int, int, int]] | None = None
    for row in rows:
        head_id = int(row["head_id"])
        source_zip = find_fight_path(head_id, ".zip")
        source_idx = find_fight_path(head_id, ".idx")
        source_grp = find_fight_path(head_id, ".grp")
        if source_zip is None:
            if source_idx is None:
                skipped_missing_idx.append(head_id)
                print(f"warn fight{head_id:03}: missing source .idx, skipped")
                continue
            if source_grp is None:
                skipped_missing_grp.append(head_id)
                print(f"warn fight{head_id:03}: missing source .grp, skipped")
                continue
            if palette is None:
                palette = build_palette_from_existing_zips(source_idx.parent)
            destination_zip = args.output_dir / f"fight{head_id:03}.zip"
            if destination_zip.exists() and not args.overwrite:
                print(f"skip {destination_zip} (exists)")
                continue
            legacy_entry_count = load_idx_entry_count(source_idx)
            frames = synthesize_frames_from_grp(head_id, palette)
            write_generated_zip(destination_zip, frames, row, legacy_entry_count)
            print(f"wrote {destination_zip} (generated from grp)")
            synthesized_from_grp.append(head_id)
            converted += 1
            continue
        if source_idx is None:
            skipped_missing_idx.append(head_id)
            print(f"warn fight{head_id:03}: missing source .idx, skipped")
            continue
        if source_grp is None:
            skipped_missing_grp.append(head_id)
            print(f"warn fight{head_id:03}: missing source .grp, skipped")
            continue
        destination_zip = args.output_dir / f"fight{head_id:03}.zip"
        if destination_zip.exists() and not args.overwrite:
            print(f"skip {destination_zip} (exists)")
            continue
        legacy_entry_count = load_idx_entry_count(source_idx)
        write_zip_with_metadata(source_zip, destination_zip, row, legacy_entry_count)
        print(f"wrote {destination_zip}")
        converted += 1

    print(f"converted {converted} fight archives")
    print(
        json.dumps(
            {
                "skipped_missing_zip": skipped_missing_zip,
                "skipped_missing_idx": skipped_missing_idx,
                "skipped_missing_grp": skipped_missing_grp,
                "synthesized_from_grp": synthesized_from_grp,
            },
            ensure_ascii=False,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())