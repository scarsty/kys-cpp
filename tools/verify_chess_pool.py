import argparse
import re
import sqlite3
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')

ROLE_LINE_RE = re.compile(r'^\s*-\s+(\d+)\s+#\s+(.+)$')
TIER_LINE_RE = re.compile(r'^\s*(?:#|-)\s*费用:\s*(\d+)\s*$')


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description='Validate a chess pool YAML against the role DB.')
    parser.add_argument(
        '--pool',
        type=Path,
        default=repo_root / 'config' / 'chess_pool.yaml',
        help='Pool YAML to validate.')
    parser.add_argument(
        '--db',
        type=Path,
        default=repo_root / 'work' / 'game-dev' / 'save' / '0.db',
        help='SQLite database to validate against.')
    parser.add_argument(
        '--strict-names',
        action='store_true',
        help='Treat comment/DB name mismatches as errors instead of warnings.')
    return parser.parse_args()


def parse_pool(pool_path: Path) -> dict[int, list[tuple[int, str]]]:
    pool: dict[int, list[tuple[int, str]]] = {}
    current_tier: int | None = None

    for raw_line in pool_path.read_text(encoding='utf-8').splitlines():
        line = raw_line.rstrip()
        tier_match = TIER_LINE_RE.match(line)
        if tier_match:
            current_tier = int(tier_match.group(1))
            pool.setdefault(current_tier, [])
            continue

        role_match = ROLE_LINE_RE.match(line)
        if role_match and current_tier is not None:
            pool[current_tier].append((int(role_match.group(1)), role_match.group(2).strip()))

    return pool


def validate_pool(pool: dict[int, list[tuple[int, str]]], db_path: Path, strict_names: bool) -> int:
    if not pool:
        print('ERROR: No tier entries were parsed from the pool YAML.')
        return 1

    seen_ids: set[int] = set()
    duplicate_ids: set[int] = set()
    for entries in pool.values():
        for role_id, _ in entries:
            if role_id in seen_ids:
                duplicate_ids.add(role_id)
            seen_ids.add(role_id)

    if duplicate_ids:
        print('ERROR: Duplicate role IDs in pool:', ', '.join(str(role_id) for role_id in sorted(duplicate_ids)))
        return len(duplicate_ids)

    conn = sqlite3.connect(str(db_path))
    cur = conn.cursor()

    errors = 0
    for tier in sorted(pool):
        print(f'\n=== Tier {tier} ({len(pool[tier])} roles) ===')
        for role_id, expected_name in pool[tier]:
            cur.execute('SELECT 名字 FROM role WHERE 编号 = ?', (role_id,))
            row = cur.fetchone()
            if row is None:
                print(f"  ERROR: ID {role_id} not found in DB (expected: {expected_name})")
                errors += 1
                continue

            actual_name = row[0].strip()
            if actual_name == expected_name:
                print(f'  OK: {role_id} = {actual_name}')
            else:
                level = 'MISMATCH' if strict_names else 'WARNING'
                print(f'  {level}: ID {role_id} comment name differs from DB value')
                if strict_names:
                    errors += 1

    conn.close()
    return errors


def main() -> int:
    args = parse_args()
    if not args.pool.exists():
        print(f'ERROR: Pool YAML not found: {args.pool}')
        return 1
    if not args.db.exists():
        print(f'ERROR: SQLite DB not found: {args.db}')
        return 1

    pool = parse_pool(args.pool)
    errors = validate_pool(pool, args.db, args.strict_names)
    print(f"\n{'ALL GOOD' if errors == 0 else f'{errors} ERRORS FOUND'}!")
    return 0 if errors == 0 else 1


if __name__ == '__main__':
    raise SystemExit(main())
