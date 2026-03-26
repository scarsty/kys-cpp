#!/usr/bin/env python3
"""Scale int16 offsets in an index.ka by a fixed factor.

Accepted targets:
- a directory: updates every direct child *.zip file
- a .zip file: updates the root-level index.ka inside the archive
- a standalone index.ka file: updates the file in place

The default scale factor is 2.
"""
from __future__ import annotations

import argparse
import os
import struct
import sys
import tempfile
import zipfile
from pathlib import Path


INT16_MIN = -32768
INT16_MAX = 32767
INDEX_NAME = "index.ka"


def scale_pair(value: int, factor: int, path: Path, pair_index: int, axis: str) -> int:
    scaled = value * factor
    if scaled < INT16_MIN or scaled > INT16_MAX:
        raise OverflowError(f"{path}: pair {pair_index} {axis} overflow after scaling by {factor}: {value} -> {scaled}")
    return scaled


def scale_index_bytes(data: bytes, factor: int, path: Path) -> bytes:
    if len(data) % 4 != 0:
        raise ValueError(f"{path}: index.ka size {len(data)} is not divisible by 4")

    scaled = bytearray(len(data))
    for pair_index, offset in enumerate(range(0, len(data), 4)):
        dx, dy = struct.unpack_from("<hh", data, offset)
        dx = scale_pair(dx, factor, path, pair_index, "dx")
        dy = scale_pair(dy, factor, path, pair_index, "dy")
        struct.pack_into("<hh", scaled, offset, dx, dy)
    return bytes(scaled)


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


def scale_index_file(index_path: Path, factor: int) -> None:
    data = index_path.read_bytes()
    scaled = scale_index_bytes(data, factor, index_path)
    replace_file(index_path, scaled)


def find_root_index_info(zip_file: zipfile.ZipFile) -> zipfile.ZipInfo | None:
    for info in zip_file.infolist():
        if info.is_dir():
            continue
        if info.filename.replace("\\", "/").lower() == INDEX_NAME:
            return info
    return None


def scale_zip_info(zip_file: zipfile.ZipFile, factor: int, zip_path: Path) -> bytes:
    index_info = find_root_index_info(zip_file)
    if index_info is None:
        raise FileNotFoundError(f"{zip_path}: missing root-level {INDEX_NAME}")
    return scale_index_bytes(zip_file.read(index_info), factor, zip_path)


def clone_zip_info(info: zipfile.ZipInfo) -> zipfile.ZipInfo:
    clone = zipfile.ZipInfo(info.filename, info.date_time)
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


def rewrite_zip(zip_path: Path, factor: int) -> None:
    fd, temp_name = tempfile.mkstemp(prefix=f"{zip_path.stem}.", suffix=".tmp.zip", dir=zip_path.parent)
    os.close(fd)
    temp_path = Path(temp_name)

    try:
        with zipfile.ZipFile(zip_path, "r") as source_zip:
            scaled_index = scale_zip_info(source_zip, factor, zip_path)

            with zipfile.ZipFile(temp_path, "w") as target_zip:
                target_zip.comment = source_zip.comment
                wrote_index = False

                for info in source_zip.infolist():
                    if info.is_dir():
                        target_zip.writestr(clone_zip_info(info), b"")
                        continue

                    normalized_name = info.filename.replace("\\", "/").lower()
                    if normalized_name == INDEX_NAME:
                        if not wrote_index:
                            target_zip.writestr(clone_zip_info(info), scaled_index)
                            wrote_index = True
                        continue

                    target_zip.writestr(clone_zip_info(info), source_zip.read(info.filename))

                if not wrote_index:
                    raise FileNotFoundError(f"{zip_path}: missing root-level {INDEX_NAME}")

        os.replace(temp_path, zip_path)
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def scale_directory(directory: Path, factor: int) -> tuple[int, int]:
    updated = 0
    failed = 0
    for child in sorted(directory.iterdir()):
        if not child.is_file() or child.suffix.lower() != ".zip":
            continue
        try:
            rewrite_zip(child, factor)
            print(f"scaled {child}")
            updated += 1
        except Exception as exc:
            failed += 1
            print(f"failed {child}: {exc}", file=sys.stderr)
    return updated, failed


def process_target(target: Path, factor: int) -> tuple[int, int]:
    if target.is_dir():
        return scale_directory(target, factor)

    if not target.is_file():
        raise FileNotFoundError(f"Target not found: {target}")

    if target.suffix.lower() == ".zip":
        rewrite_zip(target, factor)
        print(f"scaled {target}")
        return 1, 0

    if target.name.lower() == INDEX_NAME:
        scale_index_file(target, factor)
        print(f"scaled {target}")
        return 1, 0

    raise ValueError(f"Unsupported target type: {target}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Scale offsets in an index.ka by a fixed factor")
    parser.add_argument("target", type=Path, help="directory, zip file, or standalone index.ka")
    parser.add_argument("--factor", type=int, default=2, help="multiplier applied to each dx and dy value")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    target = args.target.resolve()
    try:
        updated, failed = process_target(target, args.factor)
    except Exception as exc:
        print(f"failed {target}: {exc}", file=sys.stderr)
        return 1

    if target.is_dir():
        print(f"updated {updated} zip file(s)")
        if failed:
            print(f"failed {failed} zip file(s)", file=sys.stderr)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())