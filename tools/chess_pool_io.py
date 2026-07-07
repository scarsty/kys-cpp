from __future__ import annotations

import re
from pathlib import Path
from typing import NamedTuple

import yaml


TIER_LINE_RE = re.compile(r"^\s*(?:#|-)\s*(?:\u8cbb\u7528|\u8d39\u7528):\s*(\d+)\s*$")
ROLE_LINE_RE = re.compile(r"^\s*-\s*(\d+)(?:\s*#\s*(.+?))?\s*$")


class PoolEntry(NamedTuple):
    role_id: int
    comment_name: str
    star: int


def parse_pool_entries(pool_path: Path) -> list[PoolEntry]:
    entries: list[PoolEntry] = []
    current_star: int | None = None

    for raw_line in pool_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.rstrip()
        tier_match = TIER_LINE_RE.match(line)
        if tier_match:
            current_star = int(tier_match.group(1))
            continue

        role_match = ROLE_LINE_RE.match(line)
        if role_match and current_star is not None:
            entries.append(PoolEntry(int(role_match.group(1)), (role_match.group(2) or "").strip(), current_star))

    return entries


def load_cost_mapping(pool_path: Path) -> dict[int, int]:
    return {entry.role_id: entry.star for entry in parse_pool_entries(pool_path)}


def mapping_value(mapping: dict[object, object], keys: tuple[str, ...]) -> object | None:
    for key in keys:
        if key in mapping:
            return mapping[key]
    return None


def load_synergy_members(combos_path: Path) -> dict[int, list[str]]:
    raw = yaml.safe_load(combos_path.read_text(encoding="utf-8")) or {}
    combos = mapping_value(raw, ("\u7f81\u7eca", "\u7f88\u7d46")) if isinstance(raw, dict) else raw
    if combos is None:
        return {}

    result: dict[int, list[str]] = {}
    for combo in combos:
        if not isinstance(combo, dict):
            continue
        name = mapping_value(combo, ("\u540d\u79f0", "\u540d\u7a31"))
        members = mapping_value(combo, ("\u6210\u5458", "\u6210\u54e1")) or []
        if name is None:
            raise ValueError(f"Missing synergy name in {combos_path}")
        for role_id in members:
            result.setdefault(int(role_id), []).append(str(name))
    return result
