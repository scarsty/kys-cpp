#!/usr/bin/env python3
"""Convert binary index.ka files into sparse index.txt files.

Accepted targets:
- a directory: recursively processes standalone index.ka files and .zip archives
- a .zip file: converts every index.ka entry inside the archive into a sibling index.txt
- a standalone index.ka file: writes a sibling index.txt next to it

The text format is:
    <index>: <dx>, <dy>

Only non-zero pairs are emitted by default. The source index.ka is preserved
unless --remove-ka is supplied.
"""
from __future__ import annotations

import argparse
import os
import struct
import sys
import tempfile
import zipfile
from pathlib import Path


INDEX_KA_NAME = "index.ka"
INDEX_TXT_NAME = "index.txt"


def normalize_zip_name(name: str) -> str:
    return name.replace("\\", "/")


def is_index_ka_path(path: Path) -> bool:
    return path.is_file() and path.name.lower() == INDEX_KA_NAME


def parse_index_pairs(data: bytes, source: str | Path) -> list[tuple[int, int]]:
    if len(data) % 4 != 0:
        raise ValueError(f"{source}: index.ka size {len(data)} is not divisible by 4")
    return [struct.unpack_from("<hh", data, offset) for offset in range(0, len(data), 4)]

def parse_index_pairs_unsafe(data: bytes, source: str | Path) -> list[tuple[int, int]]:
    if len(data) % 4 != 0:
        data = data[:-1]
    return [struct.unpack_from("<hh", data, offset) for offset in range(0, len(data), 4)]


def format_index_txt(pairs: list[tuple[int, int]], include_zero: bool) -> tuple[str, bool]:
    lines: list[str] = []
    all_zero = True
    for index, (dx, dy) in enumerate(pairs):
        if dx == 0 and dy == 0 and not include_zero:
            continue
        if dx != 0 or dy != 0:
            all_zero = False
        lines.append(f"{index}: {dx}, {dy}")

    if include_zero and pairs:
        all_zero = all(dx == 0 and dy == 0 for dx, dy in pairs)

    content = "\n".join(lines)
    if content:
        content += "\n"
    return content, all_zero


def replace_file(path: Path, data: bytes) -> None:
    fd, temp_name = tempfile.mkstemp(prefix=f"{path.stem}.", suffix=".tmp", dir=path.parent)
    os.close(fd)
    temp_path = Path(temp_name)
    try:
        temp_path.write_bytes(data)
        os.replace(temp_path, path)
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def sibling_index_txt_name(name: str) -> str:
    normalized = normalize_zip_name(name)
    if "/" not in normalized:
        return INDEX_TXT_NAME
    parent = normalized.rsplit("/", 1)[0]
    return f"{parent}/{INDEX_TXT_NAME}"


def clone_zip_info(info: zipfile.ZipInfo, filename: str | None = None) -> zipfile.ZipInfo:
    clone = zipfile.ZipInfo(filename or info.filename, info.date_time)
    clone.comment = info.comment
    clone.extra = info.extra
    clone.create_system = info.create_system
    clone.create_version = info.create_version
    clone.extract_version = info.extract_version
    clone.reserved = info.reserved
    clone.flag_bits = info.flag_bits
    clone.volume = info.volume
    clone.internal_attr = info.internal_attr
    clone.external_attr = info.external_attr
    clone.compress_type = info.compress_type
    return clone


def convert_index_file(index_path: Path, include_zero: bool, write_empty: bool, remove_ka: bool) -> bool:
    pairs = parse_index_pairs_unsafe(index_path.read_bytes(), index_path)
    content, all_zero = format_index_txt(pairs, include_zero)
    txt_path = index_path.with_name(INDEX_TXT_NAME)

    changed = False
    if content or write_empty:
        new_bytes = content.encode("utf-8")
        current_bytes = txt_path.read_bytes() if txt_path.exists() else None
        if current_bytes != new_bytes:
            replace_file(txt_path, new_bytes)
            changed = True
    elif txt_path.exists():
        txt_path.unlink()
        changed = True

    if remove_ka:
        index_path.unlink()
        changed = True
    elif all_zero and not content and not write_empty:
        return changed

    return changed


def collect_zip_index_entries(zip_file: zipfile.ZipFile) -> list[zipfile.ZipInfo]:
    entries: list[zipfile.ZipInfo] = []
    for info in zip_file.infolist():
        if info.is_dir():
            continue
        normalized = normalize_zip_name(info.filename)
        if Path(normalized).name.lower() == INDEX_KA_NAME:
            entries.append(info)
    return entries


def rewrite_zip(zip_path: Path, include_zero: bool, write_empty: bool, remove_ka: bool) -> bool:
    fd, temp_name = tempfile.mkstemp(prefix=f"{zip_path.stem}.", suffix=".tmp.zip", dir=zip_path.parent)
    os.close(fd)
    temp_path = Path(temp_name)

    try:
        with zipfile.ZipFile(zip_path, "r") as source_zip:
            index_infos = collect_zip_index_entries(source_zip)
            if not index_infos:
                temp_path.unlink(missing_ok=True)
                return False

            txt_updates: dict[str, bytes | None] = {}
            txt_sources: dict[str, zipfile.ZipInfo] = {}
            for info in index_infos:
                normalized_name = normalize_zip_name(info.filename)
                txt_name = sibling_index_txt_name(normalized_name)
                pairs = parse_index_pairs_unsafe(source_zip.read(info), f"{zip_path}!{normalized_name}")
                content, _ = format_index_txt(pairs, include_zero)
                if content or write_empty:
                    txt_updates[txt_name.lower()] = content.encode("utf-8")
                    txt_sources[txt_name.lower()] = info
                else:
                    txt_updates[txt_name.lower()] = None
                    txt_sources[txt_name.lower()] = info

            with zipfile.ZipFile(temp_path, "w") as target_zip:
                target_zip.comment = source_zip.comment
                written_txt: set[str] = set()

                for info in source_zip.infolist():
                    normalized_name = normalize_zip_name(info.filename)
                    normalized_key = normalized_name.lower()

                    if info.is_dir():
                        target_zip.writestr(clone_zip_info(info), b"")
                        continue

                    if normalized_key in txt_updates:
                        payload = txt_updates[normalized_key]
                        if payload is not None and normalized_key not in written_txt:
                            target_zip.writestr(clone_zip_info(info), payload)
                            written_txt.add(normalized_key)
                        continue

                    if Path(normalized_name).name.lower() == INDEX_KA_NAME and remove_ka:
                        continue

                    target_zip.writestr(clone_zip_info(info), source_zip.read(info.filename))

                for normalized_key, payload in txt_updates.items():
                    if payload is None or normalized_key in written_txt:
                        continue
                    source_info = txt_sources[normalized_key]
                    target_zip.writestr(clone_zip_info(source_info, sibling_index_txt_name(source_info.filename)), payload)

        os.replace(temp_path, zip_path)
        return True
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def iter_directory_targets(root: Path) -> list[Path]:
    targets: list[Path] = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if path.name.lower() == INDEX_KA_NAME or path.suffix.lower() == ".zip":
            targets.append(path)
    return targets


def process_target(target: Path, include_zero: bool, write_empty: bool, remove_ka: bool) -> tuple[int, int]:
    updated = 0
    failed = 0

    if target.is_dir():
        for child in iter_directory_targets(target):
            try:
                changed = process_single_target(child, include_zero, write_empty, remove_ka)
                updated += int(changed)
            except Exception as exc:
                failed += 1
                print(f"failed {child}: {exc}", file=sys.stderr)
        return updated, failed

    changed = process_single_target(target, include_zero, write_empty, remove_ka)
    return int(changed), 0


def process_single_target(target: Path, include_zero: bool, write_empty: bool, remove_ka: bool) -> bool:
    if not target.is_file():
        raise FileNotFoundError(f"Target not found: {target}")

    if target.suffix.lower() == ".zip":
        changed = rewrite_zip(target, include_zero, write_empty, remove_ka)
        if changed:
            print(f"converted {target}")
        return changed

    if target.name.lower() == INDEX_KA_NAME:
        changed = convert_index_file(target, include_zero, write_empty, remove_ka)
        if changed:
            print(f"converted {target}")
        return changed

    raise ValueError(f"Unsupported target type: {target}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert binary index.ka files into sparse index.txt files")
    parser.add_argument("target", type=Path, help="directory, zip file, or standalone index.ka")
    parser.add_argument("--include-zero", action="store_true", help="emit zero offsets into index.txt as well")
    parser.add_argument("--write-empty", action="store_true", help="write an empty index.txt when all offsets are zero")
    parser.add_argument("--remove-ka", action="store_true", help="delete index.ka after generating index.txt")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    target = args.target.resolve()
    try:
        updated, failed = process_target(target, args.include_zero, args.write_empty, args.remove_ka)
    except Exception as exc:
        print(f"failed {target}: {exc}", file=sys.stderr)
        return 1

    if target.is_dir():
        print(f"updated {updated} target(s)")
        if failed:
            print(f"failed {failed} target(s)", file=sys.stderr)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())