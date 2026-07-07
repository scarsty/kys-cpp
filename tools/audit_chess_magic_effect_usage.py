from __future__ import annotations

import argparse
import json
import re
import sqlite3
import sys
from dataclasses import asdict, dataclass
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")

HYBRID_PURPOSE = "普通+絕招"
NORMAL_PURPOSE = "普通"
ULTIMATE_PURPOSE = "絕招"
UNUSED_PURPOSE = "未使用"
STAR_SPECS = (
    ("一", "一星武功1", "一星威力1", "一星武功2", "一星威力2"),
    ("二", "二星武功1", "二星威力1", "二星武功2", "二星威力2"),
    ("三", "三星武功1", "三星威力1", "三星武功2", "三星威力2"),
)

MAGIC_ID_RE = re.compile(r"^\s*-\s+武功:\s*(\d+)\s*$")
MAGIC_NAME_RE = re.compile(r"^\s+(?:名称|名稱):\s*(.*?)\s*$")
MAGIC_PURPOSE_RE = re.compile(r"^\s+用途:\s*(.*?)\s*$")
POOL_ROLE_RE = re.compile(r"^\s*-\s+(\d+)\s*(?:#\s*(.*?))?\s*$")
POOL_TIER_RE = re.compile(r"^\s*(?:#|-)\s*(?:费用|費用):\s*(\d+)\s*$")


@dataclass(frozen=True)
class MagicEffectEntry:
    magic_id: int
    name: str
    purpose: str


@dataclass(frozen=True)
class PoolRole:
    role_id: int
    tier: int
    comment_name: str


@dataclass(frozen=True)
class MagicUse:
    role_id: int
    role_name: str
    tier: int
    star: str
    power: int
    other_magic_id: int | None = None
    other_magic_name: str | None = None
    other_power: int | None = None
    pool: str = ""


@dataclass
class MagicUsageReport:
    magic_id: int
    name: str
    purpose: str
    normal: list[MagicUse]
    ultimate: list[MagicUse]
    both: list[MagicUse]


@dataclass
class PurposeValidationIssue:
    pool: str
    magic_id: int
    name: str
    declared: str
    observed: str
    problem: str
    normal: list[MagicUse]
    ultimate: list[MagicUse]
    both: list[MagicUse]


def parse_magic_effect_entries(path: Path) -> list[MagicEffectEntry]:
    entries: list[MagicEffectEntry] = []
    current_id: int | None = None
    current_name = ""
    current_purpose = ""

    def finish_current() -> None:
        if current_id is not None:
            entries.append(MagicEffectEntry(current_id, current_name, current_purpose))

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        id_match = MAGIC_ID_RE.match(raw_line)
        if id_match:
            finish_current()
            current_id = int(id_match.group(1))
            current_name = ""
            current_purpose = ""
            continue

        if current_id is None:
            continue

        name_match = MAGIC_NAME_RE.match(raw_line)
        if name_match:
            current_name = name_match.group(1)
            continue

        purpose_match = MAGIC_PURPOSE_RE.match(raw_line)
        if purpose_match:
            current_purpose = purpose_match.group(1)

    finish_current()
    return entries


def parse_pool_roles(path: Path) -> dict[int, PoolRole]:
    roles: dict[int, PoolRole] = {}
    current_tier: int | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        tier_match = POOL_TIER_RE.match(raw_line)
        if tier_match:
            current_tier = int(tier_match.group(1))
            continue

        role_match = POOL_ROLE_RE.match(raw_line)
        if role_match and current_tier is not None:
            role_id = int(role_match.group(1))
            roles[role_id] = PoolRole(
                role_id=role_id,
                tier=current_tier,
                comment_name=(role_match.group(2) or "").strip(),
            )
    return roles


def fetch_magic_names(cur: sqlite3.Cursor) -> dict[int, str]:
    return {
        int(row[0]): str(row[1])
        for row in cur.execute("SELECT 编号, 名称 FROM magic")
    }


def fetch_pool_role_rows(cur: sqlite3.Cursor, pool_roles: dict[int, PoolRole]) -> list[sqlite3.Row]:
    if not pool_roles:
        return []
    columns = ["编号", "名字"]
    for _, skill1, power1, skill2, power2 in STAR_SPECS:
        columns.extend((skill1, power1, skill2, power2))
    placeholders = ",".join("?" for _ in pool_roles)
    sql = (
        "SELECT "
        + ", ".join(f'"{column}"' for column in columns)
        + f" FROM role WHERE 编号 IN ({placeholders}) ORDER BY 编号"
    )
    return list(cur.execute(sql, tuple(sorted(pool_roles))))


def make_use(
    row: sqlite3.Row,
    pool_role: PoolRole,
    star: str,
    selected: dict[str, int],
    other: dict[str, int] | None,
    magic_names: dict[int, str],
    pool_label: str = "",
) -> MagicUse:
    return MagicUse(
        role_id=int(row["编号"]),
        role_name=str(row["名字"]),
        tier=pool_role.tier,
        star=star,
        power=selected["power"],
        other_magic_id=None if other is None else other["magic_id"],
        other_magic_name=None if other is None else magic_names.get(other["magic_id"], str(other["magic_id"])),
        other_power=None if other is None else other["power"],
        pool=pool_label,
    )


def build_report(
    effects_path: Path,
    pool_path: Path,
    db_path: Path,
    include_unused: bool = False,
    purpose_filter: str | None = HYBRID_PURPOSE,
    pool_label: str = "",
) -> list[MagicUsageReport]:
    effect_entries = parse_magic_effect_entries(effects_path)
    if purpose_filter is not None:
        effect_entries = [entry for entry in effect_entries if entry.purpose == purpose_filter]
    reports = {
        entry.magic_id: MagicUsageReport(entry.magic_id, entry.name, entry.purpose, [], [], [])
        for entry in effect_entries
    }
    pool_roles = parse_pool_roles(pool_path)

    con = sqlite3.connect(db_path)
    try:
        con.row_factory = sqlite3.Row
        cur = con.cursor()
        magic_names = fetch_magic_names(cur)

        for row in fetch_pool_role_rows(cur, pool_roles):
            pool_role = pool_roles[int(row["编号"])]
            for star, skill1, power1, skill2, power2 in STAR_SPECS:
                slots = [
                    {"slot": 1, "magic_id": int(row[skill1]), "power": int(row[power1])},
                    {"slot": 2, "magic_id": int(row[skill2]), "power": int(row[power2])},
                ]
                valid_slots = [slot for slot in slots if slot["magic_id"] > 0]
                if not valid_slots:
                    continue

                min_power = min(slot["power"] for slot in valid_slots)
                max_power = max(slot["power"] for slot in valid_slots)
                normal = next(slot for slot in valid_slots if slot["power"] == min_power)
                ultimate = next(slot for slot in valid_slots if slot["power"] == max_power)

                def other_for(selected: dict[str, int]) -> dict[str, int] | None:
                    return next((slot for slot in valid_slots if slot["slot"] != selected["slot"]), None)

                if normal["magic_id"] == ultimate["magic_id"]:
                    report = reports.get(normal["magic_id"])
                    if report is not None:
                        report.both.append(make_use(
                            row,
                            pool_role,
                            star,
                            normal,
                            other_for(normal),
                            magic_names,
                            pool_label))
                    continue

                report = reports.get(normal["magic_id"])
                if report is not None:
                    report.normal.append(make_use(
                        row,
                        pool_role,
                        star,
                        normal,
                        other_for(normal),
                        magic_names,
                        pool_label))

                report = reports.get(ultimate["magic_id"])
                if report is not None:
                    report.ultimate.append(make_use(
                        row,
                        pool_role,
                        star,
                        ultimate,
                        other_for(ultimate),
                        magic_names,
                        pool_label))
    finally:
        con.close()

    return [
        report
        for report in reports.values()
        if include_unused or report.normal or report.ultimate or report.both
    ]


def declared_attack_tokens(purpose: str) -> set[str]:
    tokens: set[str] = set()
    if "普通" in purpose:
        tokens.add("普通")
    if "絕招" in purpose or "绝招" in purpose:
        tokens.add("絕招")
    return tokens


def observed_attack_tokens(report: MagicUsageReport) -> set[str]:
    tokens: set[str] = set()
    if report.normal or report.both:
        tokens.add("普通")
    if report.ultimate or report.both:
        tokens.add("絕招")
    return tokens


def format_attack_tokens(tokens: set[str]) -> str:
    if tokens == {NORMAL_PURPOSE, ULTIMATE_PURPOSE}:
        return HYBRID_PURPOSE
    if tokens == {NORMAL_PURPOSE}:
        return NORMAL_PURPOSE
    if tokens == {ULTIMATE_PURPOSE}:
        return ULTIMATE_PURPOSE
    return UNUSED_PURPOSE


def merge_report(target: MagicUsageReport, source: MagicUsageReport) -> None:
    target.normal.extend(source.normal)
    target.ultimate.extend(source.ultimate)
    target.both.extend(source.both)


def validate_purpose_usage(
    effects_path: Path,
    pool_paths: list[Path],
    db_path: Path,
    include_unused: bool = False,
) -> list[PurposeValidationIssue]:
    effect_entries = parse_magic_effect_entries(effects_path)
    reports = {
        entry.magic_id: MagicUsageReport(entry.magic_id, entry.name, entry.purpose, [], [], [])
        for entry in effect_entries
    }

    for pool_path in pool_paths:
        pool_reports = build_report(
            effects_path,
            pool_path,
            db_path,
            include_unused=True,
            purpose_filter=None,
            pool_label=pool_path.stem,
        )
        for pool_report in pool_reports:
            merge_report(reports[pool_report.magic_id], pool_report)

    issues: list[PurposeValidationIssue] = []
    pool_label = "+".join(pool_path.stem for pool_path in pool_paths)
    for report in reports.values():
        declared_tokens = declared_attack_tokens(report.purpose)
        observed_tokens = observed_attack_tokens(report)
        if not observed_tokens and not include_unused:
            continue

        if declared_tokens != observed_tokens:
            issues.append(
                PurposeValidationIssue(
                    pool=pool_label,
                    magic_id=report.magic_id,
                    name=report.name,
                    declared=format_attack_tokens(declared_tokens),
                    observed=format_attack_tokens(observed_tokens),
                    problem="declared purpose mismatch",
                    normal=report.normal,
                    ultimate=report.ultimate,
                    both=report.both,
                )
            )
    return issues


def collapsed_uses(uses: list[MagicUse]) -> list[str]:
    grouped: dict[tuple[str, int, str, int, int, int | None, str | None, int | None], list[str]] = {}
    for use in uses:
        key = (
            use.pool,
            use.role_id,
            use.role_name,
            use.tier,
            use.power,
            use.other_magic_id,
            use.other_magic_name,
            use.other_power,
        )
        grouped.setdefault(key, []).append(use.star)

    parts: list[str] = []
    for key, stars in sorted(grouped.items(), key=lambda item: (item[0][3], item[0][1], item[0][4], item[1])):
        pool, role_id, role_name, tier, power, _, other_magic_name, other_power = key
        star_text = "" if "".join(stars) == "一二三" else f"[{''.join(stars)}]"
        comparison = ""
        if other_magic_name is not None and other_power is not None:
            comparison = f" vs {other_magic_name}{other_power}"
        pool_text = "" if not pool else f"{pool}:"
        parts.append(f"{pool_text}{role_id}:{role_name}{star_text}(費{tier}, {power}{comparison})")
    return parts


def format_markdown(report: list[MagicUsageReport]) -> str:
    lines = [
        "| 武功 | 普通 users | 絕招 users | both |",
        "|---|---|---|---|",
    ]
    for entry in report:
        normal = "<br>".join(collapsed_uses(entry.normal)) or "—"
        ultimate = "<br>".join(collapsed_uses(entry.ultimate)) or "—"
        both = "<br>".join(collapsed_uses(entry.both)) or "—"
        lines.append(f"| {entry.magic_id} {entry.name} | {normal} | {ultimate} | {both} |")
    return "\n".join(lines)


def format_validation_markdown(issues: list[PurposeValidationIssue]) -> str:
    if not issues:
        return "No purpose validation issues found."

    lines = [
        "| Pool | 武功 | Declared | Observed | Problem | 普通 users | 絕招 users | both |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for issue in issues:
        normal = "<br>".join(collapsed_uses(issue.normal)) or "—"
        ultimate = "<br>".join(collapsed_uses(issue.ultimate)) or "—"
        both = "<br>".join(collapsed_uses(issue.both)) or "—"
        lines.append(
            f"| {issue.pool} | {issue.magic_id} {issue.name} | {issue.declared} | "
            f"{issue.observed} | {issue.problem} | {normal} | {ultimate} | {both} |"
        )
    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(
        description=(
            "Audit chess magic effect purpose metadata against role skill powers. "
            "Default mode reports effects already marked 普通+絕招; --validate checks all configured effects."
        )
    )
    parser.add_argument(
        "--effects",
        type=Path,
        default=repo_root / "config" / "chess_magic_effects.yaml",
        help="Magic effect YAML to audit.",
    )
    parser.add_argument(
        "--pool",
        type=Path,
        default=None,
        help=(
            "Chess pool YAML used to filter roles. "
            "Defaults to chess_pool.yaml in report mode; --validate defaults to both top-level pools."
        ),
    )
    parser.add_argument(
        "--db",
        type=Path,
        default=repo_root / "work" / "game-dev" / "save" / "game.db",
        help="Source SQLite game database.",
    )
    parser.add_argument(
        "--include-unused",
        action="store_true",
        help="Show configured effects that have no selected pool-role usage.",
    )
    parser.add_argument(
        "--validate",
        "--validate-purpose",
        dest="validate",
        action="store_true",
        help="Validate declared 用途 for all configured effects against selected normal/ultimate usage across selected pools.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit JSON instead of a Markdown table.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.validate:
        pool_paths = [
            args.pool,
        ] if args.pool is not None else [
            Path(__file__).resolve().parents[1] / "config" / "chess_pool.yaml",
            Path(__file__).resolve().parents[1] / "config" / "chess_pool_easy.yaml",
        ]
        issues = validate_purpose_usage(args.effects, pool_paths, args.db, include_unused=args.include_unused)
        if args.json:
            print(json.dumps([asdict(issue) for issue in issues], ensure_ascii=False, indent=2))
        else:
            print(format_validation_markdown(issues))
    else:
        pool_path = args.pool if args.pool is not None else Path(__file__).resolve().parents[1] / "config" / "chess_pool.yaml"
        report = build_report(args.effects, pool_path, args.db, include_unused=args.include_unused)
        if args.json:
            print(json.dumps([asdict(entry) for entry in report], ensure_ascii=False, indent=2))
        else:
            print(format_markdown(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
