#!/usr/bin/env python3
"""Scale int16 texture offsets in index.ka and index.txt files.

Directories are processed recursively. ZIP files are rewritten in place with
their root-level index.ka and index.txt entries scaled.
"""
from __future__ import annotations

import argparse
import os
import re
import struct
import sys
import tempfile
import zipfile
from pathlib import Path


INT16_MIN = -32768
INT16_MAX = 32767
INDEX_KA = "index.ka"
INDEX_TXT = "index.txt"
NUMBER_PATTERN = re.compile(r"[+-]?\d+")


def scale_value(value: int, factor: int, path: Path, index: int, axis: str) -> int:
    result = value * factor
    if not INT16_MIN <= result <= INT16_MAX:
        raise OverflowError(f"{path}: entry {index} {axis} overflows: {value} -> {result}")
    return result


def scale_ka(data: bytes, factor: int, path: Path) -> bytes:
    if len(data) % 4 != 0:
        raise ValueError(f"{path}: index.ka size {len(data)} is not divisible by 4")

    output = bytearray(len(data))
    for index, offset in enumerate(range(0, len(data), 4)):
        dx, dy = struct.unpack_from("<hh", data, offset)
        struct.pack_into(
            "<hh",
            output,
            offset,
            scale_value(dx, factor, path, index, "dx"),
            scale_value(dy, factor, path, index, "dy"),
        )
    return bytes(output)


def scale_txt(data: bytes, factor: int, path: Path) -> bytes:
    output: list[str] = []
    for line in data.decode("utf-8-sig").splitlines(keepends=True):
        body = line.rstrip("\r\n")
        newline = line[len(body):]
        values = [int(value) for value in NUMBER_PATTERN.findall(body)]
        if len(values) < 3:
            output.append(line)
            continue
        index, dx, dy = values[:3]
        output.append(
            f"{index}: {scale_value(dx, factor, path, index, 'dx')}, "
            f"{scale_value(dy, factor, path, index, 'dy')}{newline}"
        )
    return "".join(output).encode("utf-8")


def scale_index(name: str, data: bytes, factor: int, path: Path) -> bytes:
    normalized = name.replace("\\", "/").lower()
    if normalized == INDEX_KA:
        return scale_ka(data, factor, path)
    if normalized == INDEX_TXT:
        return scale_txt(data, factor, path)
    raise ValueError(f"Unsupported index file: {name}")


def replace_file(path: Path, data: bytes) -> None:
    descriptor, temporary_name = tempfile.mkstemp(prefix=f"{path.stem}.", suffix=".tmp", dir=path.parent)
    os.close(descriptor)
    temporary_path = Path(temporary_name)
    try:
        temporary_path.write_bytes(data)
        os.replace(temporary_path, path)
    except Exception:
        temporary_path.unlink(missing_ok=True)
        raise


def rewrite_zip(path: Path, factor: int) -> None:
    descriptor, temporary_name = tempfile.mkstemp(prefix=f"{path.stem}.", suffix=".tmp.zip", dir=path.parent)
    os.close(descriptor)
    temporary_path = Path(temporary_name)
    try:
        with zipfile.ZipFile(path) as source, zipfile.ZipFile(temporary_path, "w") as target:
            target.comment = source.comment
            found = False
            for info in source.infolist():
                name = info.filename.replace("\\", "/").lower()
                content = source.read(info.filename)
                if not info.is_dir() and name in {INDEX_KA, INDEX_TXT}:
                    content = scale_index(name, content, factor, path)
                    found = True
                target.writestr(info, content)
            if not found:
                raise FileNotFoundError(f"{path}: no root-level index.ka or index.txt")
        os.replace(temporary_path, path)
    except Exception:
        temporary_path.unlink(missing_ok=True)
        raise


def process_file(path: Path, factor: int) -> bool:
    if path.suffix.lower() == ".zip":
        rewrite_zip(path, factor)
        return True
    if path.name.lower() in {INDEX_KA, INDEX_TXT}:
        replace_file(path, scale_index(path.name, path.read_bytes(), factor, path))
        return True
    return False


def process_target(path: Path, factor: int) -> tuple[int, int]:
    if path.is_file():
        return (1, 0) if process_file(path, factor) else (0, 1)
    if not path.is_dir():
        raise FileNotFoundError(path)

    updated = 0
    failed = 0
    for child in sorted(path.rglob("*")):
        if not child.is_file() or (child.name.lower() not in {INDEX_KA, INDEX_TXT} and child.suffix.lower() != ".zip"):
            continue
        try:
            process_file(child, factor)
            print(f"scaled {child}")
            updated += 1
        except Exception as error:
            print(f"failed {child}: {error}", file=sys.stderr)
            failed += 1
    return updated, failed


def main() -> int:
    parser = argparse.ArgumentParser(description="Scale texture offsets in index.ka and index.txt")
    parser.add_argument("target", type=Path)
    parser.add_argument("--factor", type=int, default=2)
    args = parser.parse_args()
    try:
        updated, failed = process_target(args.target.resolve(), args.factor)
    except Exception as error:
        print(f"failed: {error}", file=sys.stderr)
        return 1
    print(f"updated {updated} file(s)")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())