#!/usr/bin/env python3
"""Generate `config/chess_pool_easy.yaml` with an exact optimizer.

The target is the smallest pool with size strictly greater than a lower bound
while enforcing this rule for every synergy:

- either zero members of the synergy are in the pool
- or the pool contains at least that synergy's maximum threshold

The solver runs in two phases:
1. Minimize pool size subject to the synergy constraints and pool size > N.
2. Among all minimum-size solutions, minimize total tier cost.

Optional overrides make it easier to tune the resulting easy pool without
editing the script:
- `--exclude-synergy 名称`
- `--threshold-override 名称=人数`

Report mode highlights where the current combo graph is inefficient:
- inactive synergies and how to activate them
- selected low-degree roles that are good candidates for extra tags
- unselected high-degree roles if adding characters becomes necessary
"""

from __future__ import annotations

import argparse
import itertools
import re
import sqlite3
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Sequence

import yaml

try:
    import pulp
except ImportError as exc:  # pragma: no cover - import guard for runtime clarity
    raise SystemExit(
        'PuLP is required. Install it with: pip install PuLP'
    ) from exc


@dataclass(frozen=True)
class Synergy:
    name: str
    members: tuple[int, ...]
    threshold: int
    reverse: bool


@dataclass(frozen=True)
class SynergyStatus:
    synergy: Synergy
    hit_count: int

    @property
    def is_active(self) -> bool:
        return self.hit_count >= self.synergy.threshold


@dataclass(frozen=True)
class SyntheticBridgeResult:
    bundle: tuple[str, ...]
    min_count: int
    total_cost: int
    selected: bool
    activated_count: int


def load_yaml(path: Path):
    with path.open('r', encoding='utf-8') as handle:
        return yaml.safe_load(handle)


def parse_name_comments(path: Path) -> Dict[int, str]:
    id_to_name: Dict[int, str] = {}
    pattern = re.compile(r'^\s*-\s*(\d+)\s*#\s*(.+?)\s*$')
    with path.open('r', encoding='utf-8') as handle:
        for line in handle:
            match = pattern.match(line.rstrip('\n'))
            if match:
                id_to_name[int(match.group(1))] = match.group(2)
    return id_to_name


def load_role_names(pool_path: Path, db_path: Path | None) -> Dict[int, str]:
    names = parse_name_comments(pool_path)
    if db_path is None or not db_path.exists():
        return names

    conn = sqlite3.connect(str(db_path))
    try:
        cursor = conn.cursor()
        cursor.execute('SELECT 编号, 名字 FROM role')
        for role_id, role_name in cursor.fetchall():
            names[role_id] = role_name
    finally:
        conn.close()
    return names


def parse_threshold_overrides(values: Sequence[str]) -> Dict[str, int]:
    overrides: Dict[str, int] = {}
    for raw in values:
        if '=' not in raw:
            raise ValueError(f'Invalid threshold override: {raw!r}')
        name, value = raw.split('=', 1)
        name = name.strip()
        if not name:
            raise ValueError(f'Invalid threshold override: {raw!r}')
        threshold = int(value.strip())
        if threshold < 0:
            raise ValueError(f'Threshold must be non-negative: {raw!r}')
        overrides[name] = threshold
    return overrides


def collect_pool(pool_path: Path) -> tuple[list[int], dict[int, int]]:
    ordered_roles: list[int] = []
    tier_map: dict[int, int] = {}
    current_tier: int | None = None
    tier_pattern = re.compile(r'^\s*#\s*费用\s*:\s*(\d+)\s*$')
    role_pattern = re.compile(r'^\s*-\s*(\d+)\s*(?:#.*)?$')

    with pool_path.open('r', encoding='utf-8') as handle:
        for raw_line in handle:
            line = raw_line.rstrip('\n')
            tier_match = tier_pattern.match(line)
            if tier_match:
                current_tier = int(tier_match.group(1))
                continue

            role_match = role_pattern.match(line)
            if not role_match:
                continue
            if current_tier is None:
                raise ValueError(f'Missing tier comment before role entry in {pool_path}')

            role_id = int(role_match.group(1))
            ordered_roles.append(role_id)
            tier_map[role_id] = current_tier

    return ordered_roles, tier_map


def collect_synergies(
    combos_raw,
    universe: set[int],
    threshold_overrides: Dict[str, int],
    excluded_synergies: set[str],
) -> list[Synergy]:
    combos = combos_raw.get('羁绊', []) if isinstance(combos_raw, dict) else combos_raw
    synergies: list[Synergy] = []

    for combo in combos:
        name = combo['名称']
        if name in excluded_synergies:
            continue

        members = tuple(role_id for role_id in combo.get('成员', []) if role_id in universe)
        if not members:
            continue

        threshold = threshold_overrides.get(
            name,
            max((threshold.get('人数', 0) for threshold in combo.get('阈值', [])), default=0),
        )
        if threshold <= 0:
            continue
        if threshold > len(members):
            raise ValueError(
                f'Synergy {name!r} threshold {threshold} exceeds member count {len(members)}'
            )

        synergies.append(
            Synergy(
                name=name,
                members=members,
                threshold=threshold,
                reverse=bool(combo.get('反向羁绊', False)),
            )
        )

    return synergies


def build_problem(
    roles: Sequence[int],
    tier_map: Dict[int, int],
    synergies: Sequence[Synergy],
    min_selected: int,
):
    problem = pulp.LpProblem('generate_easy_pool', pulp.LpMinimize)
    role_vars = {role_id: pulp.LpVariable(f'x_{role_id}', cat='Binary') for role_id in roles}
    synergy_vars = {
        synergy.name: pulp.LpVariable(f'y_{index}', cat='Binary')
        for index, synergy in enumerate(synergies)
    }

    selected_count = pulp.lpSum(role_vars.values())
    total_cost = pulp.lpSum(tier_map[role_id] * role_vars[role_id] for role_id in roles)
    problem += selected_count >= min_selected

    for synergy in synergies:
        active = synergy_vars[synergy.name]
        member_sum = pulp.lpSum(role_vars[role_id] for role_id in synergy.members)
        problem += member_sum >= synergy.threshold * active
        problem += member_sum <= len(synergy.members) * active

    return problem, role_vars, synergy_vars, selected_count, total_cost


def solve_pool(
    roles: Sequence[int],
    tier_map: Dict[int, int],
    synergies: Sequence[Synergy],
    min_selected: int,
) -> tuple[set[int], int, int]:
    solver = pulp.PULP_CBC_CMD(msg=False)

    phase1, role_vars1, _, selected_count1, _ = build_problem(roles, tier_map, synergies, min_selected)
    phase1 += selected_count1
    status = phase1.solve(solver)
    if pulp.LpStatus[status] != 'Optimal':
        raise RuntimeError(f'Phase 1 failed with status: {pulp.LpStatus[status]}')
    min_count = int(round(pulp.value(selected_count1)))

    phase2, role_vars2, _, selected_count2, total_cost2 = build_problem(roles, tier_map, synergies, min_selected)
    phase2 += selected_count2 == min_count
    phase2 += total_cost2
    status = phase2.solve(solver)
    if pulp.LpStatus[status] != 'Optimal':
        raise RuntimeError(f'Phase 2 failed with status: {pulp.LpStatus[status]}')

    selected_roles = {role_id for role_id in roles if pulp.value(role_vars2[role_id]) > 0.5}
    total_cost = int(round(pulp.value(total_cost2)))
    return selected_roles, min_count, total_cost


def summarize_synergies(selected_roles: set[int], synergies: Sequence[Synergy]) -> list[str]:
    lines: list[str] = []
    for synergy in synergies:
        hit_count = sum(role_id in selected_roles for role_id in synergy.members)
        if hit_count:
            lines.append(f'{synergy.name}: {hit_count}/{synergy.threshold}')
    return lines


def build_role_to_synergies(synergies: Sequence[Synergy]) -> dict[int, list[str]]:
    role_to_synergies: dict[int, list[str]] = {}
    for synergy in synergies:
        for role_id in synergy.members:
            role_to_synergies.setdefault(role_id, []).append(synergy.name)
    for names in role_to_synergies.values():
        names.sort()
    return role_to_synergies


def evaluate_synergies(selected_roles: set[int], synergies: Sequence[Synergy]) -> list[SynergyStatus]:
    return [
        SynergyStatus(
            synergy=synergy,
            hit_count=sum(role_id in selected_roles for role_id in synergy.members),
        )
        for synergy in synergies
    ]


def format_role(
    role_id: int,
    role_names: Dict[int, str],
    tier_map: Dict[int, int],
    role_to_synergies: dict[int, list[str]],
) -> str:
    name = role_names.get(role_id, '???')
    synergies = ', '.join(role_to_synergies.get(role_id, []))
    degree = len(role_to_synergies.get(role_id, []))
    tier = tier_map.get(role_id, '?')
    return f'{role_id}:{name} tier={tier} degree={degree} [{synergies}]'


def build_burden_rows(
    active_statuses: Sequence[SynergyStatus],
    selected_roles: set[int],
    role_to_synergies: dict[int, list[str]],
):
    active_role_to_synergies: dict[int, list[str]] = {}
    for status in active_statuses:
        for role_id in status.synergy.members:
            if role_id in selected_roles:
                active_role_to_synergies.setdefault(role_id, []).append(status.synergy.name)

    burden_rows = []
    for status in active_statuses:
        chosen_members = [role_id for role_id in status.synergy.members if role_id in selected_roles]
        unique_members = [
            role_id
            for role_id in chosen_members
            if len(active_role_to_synergies.get(role_id, [])) == 1
        ]
        low_degree_members = [
            role_id
            for role_id in chosen_members
            if len(role_to_synergies.get(role_id, [])) < 3
        ]
        burden_rows.append((status, unique_members, low_degree_members))

    burden_rows.sort(
        key=lambda row: (
            -len(row[1]),
            -len(row[2]),
            -row[0].synergy.threshold,
            row[0].synergy.name,
        )
    )
    return burden_rows


def extend_synergies_with_synthetic_role(
    synergies: Sequence[Synergy],
    synthetic_role_id: int,
    bundle: Sequence[str],
) -> list[Synergy]:
    bundle_set = set(bundle)
    extended: list[Synergy] = []
    for synergy in synergies:
        if synergy.name in bundle_set and synthetic_role_id not in synergy.members:
            members = synergy.members + (synthetic_role_id,)
        else:
            members = synergy.members
        extended.append(
            Synergy(
                name=synergy.name,
                members=members,
                threshold=synergy.threshold,
                reverse=synergy.reverse,
            )
        )
    return extended


def evaluate_synthetic_bridges(
    roles: Sequence[int],
    selected_roles: set[int],
    min_count: int,
    tier_map: Dict[int, int],
    synergies: Sequence[Synergy],
    statuses: Sequence[SynergyStatus],
    burden_rows,
    min_selected: int,
    candidate_limit: int,
    max_tags: int,
    synthetic_tier: int,
) -> list[SyntheticBridgeResult]:
    candidate_names: list[str] = []
    for status in statuses:
        if not status.is_active and status.synergy.name not in candidate_names:
            candidate_names.append(status.synergy.name)
    for status, _, low_degree_members in burden_rows:
        if not low_degree_members:
            continue
        if status.synergy.name not in candidate_names:
            candidate_names.append(status.synergy.name)
        if len(candidate_names) >= candidate_limit:
            break

    candidate_names = candidate_names[:candidate_limit]
    if not candidate_names:
        return []

    synthetic_role_id = max(roles) + 1000
    augmented_roles = list(roles) + [synthetic_role_id]
    augmented_tier_map = dict(tier_map)
    augmented_tier_map[synthetic_role_id] = synthetic_tier

    results: list[SyntheticBridgeResult] = []
    for bundle_size in range(1, min(max_tags, len(candidate_names)) + 1):
        for bundle in itertools.combinations(candidate_names, bundle_size):
            augmented_synergies = extend_synergies_with_synthetic_role(synergies, synthetic_role_id, bundle)
            bridge_selected, bridge_min_count, bridge_total_cost = solve_pool(
                augmented_roles,
                augmented_tier_map,
                augmented_synergies,
                min_selected,
            )
            activated_count = sum(
                1
                for status in evaluate_synergies(bridge_selected, augmented_synergies)
                if status.synergy.name in bundle and status.is_active
            )
            results.append(
                SyntheticBridgeResult(
                    bundle=bundle,
                    min_count=bridge_min_count,
                    total_cost=bridge_total_cost,
                    selected=synthetic_role_id in bridge_selected,
                    activated_count=activated_count,
                )
            )

    results.sort(
        key=lambda result: (
            result.min_count,
            -result.selected,
            -result.activated_count,
            len(result.bundle),
            result.total_cost,
            result.bundle,
        )
    )
    return results


def print_report(
    roles: Sequence[int],
    selected_roles: set[int],
    min_count: int,
    total_cost: int,
    synergies: Sequence[Synergy],
    tier_map: Dict[int, int],
    role_names: Dict[int, str],
    report_limit: int,
    min_selected: int,
    synthetic_candidate_limit: int,
    synthetic_max_tags: int,
    synthetic_tier: int,
) -> None:
    role_to_synergies = build_role_to_synergies(synergies)
    statuses = evaluate_synergies(selected_roles, synergies)
    active_statuses = [status for status in statuses if status.is_active]
    inactive_statuses = [status for status in statuses if not status.is_active]

    print(f'Report for minimum pool size >= {min_selected}')
    print(f'Optimal pool size: {min_count}')
    print(f'Total tier cost: {total_cost}')
    print(f'Active synergies: {len(active_statuses)} / {len(statuses)}')
    print(f'Inactive synergies: {len(inactive_statuses)}')

    selected_low_degree = [
        role_id for role_id in selected_roles if len(role_to_synergies.get(role_id, [])) < 3
    ]
    selected_low_degree.sort(
        key=lambda role_id: (
            len(role_to_synergies.get(role_id, [])),
            tier_map.get(role_id, 99),
            role_names.get(role_id, ''),
            role_id,
        )
    )

    print('\nInactive synergies and activation options:')
    if not inactive_statuses:
        print('  None.')
    else:
        inactive_statuses.sort(
            key=lambda status: (
                status.synergy.threshold,
                sum(tier_map.get(role_id, 99) for role_id in status.synergy.members),
                status.synergy.name,
            )
        )
        for status in inactive_statuses[:report_limit]:
            synergy = status.synergy
            cheapest_members = sorted(
                synergy.members,
                key=lambda role_id: (
                    tier_map.get(role_id, 99),
                    -len(role_to_synergies.get(role_id, [])),
                    role_names.get(role_id, ''),
                    role_id,
                ),
            )[:synergy.threshold]
            recruit_cost = sum(tier_map.get(role_id, 99) for role_id in cheapest_members)
            tag_candidates = [
                role_id for role_id in selected_low_degree if role_id not in synergy.members
            ]
            print(
                f'  {synergy.name}: threshold={synergy.threshold}, members={len(synergy.members)}, '
                f'recruit_cost={recruit_cost}'
            )
            print(
                '    Cheapest new-member route: '
                + '; '.join(
                    format_role(role_id, role_names, tier_map, role_to_synergies)
                    for role_id in cheapest_members
                )
            )
            if len(tag_candidates) >= synergy.threshold:
                print(
                    '    Tag-only route using current low-degree roles: '
                    + '; '.join(
                        format_role(role_id, role_names, tier_map, role_to_synergies)
                        for role_id in tag_candidates[:synergy.threshold]
                    )
                )
            else:
                print(
                    f'    Tag-only route is short by {synergy.threshold - len(tag_candidates)} roles. '
                    'Best current candidates: '
                    + '; '.join(
                        format_role(role_id, role_names, tier_map, role_to_synergies)
                        for role_id in tag_candidates[:report_limit]
                    )
                )

    print('\nSelected low-degree roles (<3 synergies):')
    if not selected_low_degree:
        print('  None.')
    else:
        for role_id in selected_low_degree[:report_limit]:
            print(f'  {format_role(role_id, role_names, tier_map, role_to_synergies)}')

    burden_rows = build_burden_rows(active_statuses, selected_roles, role_to_synergies)

    print('\nMost expensive active synergies to clean up:')
    for status, unique_members, low_degree_members in burden_rows[:report_limit]:
        print(
            f'  {status.synergy.name}: hit={status.hit_count}/{status.synergy.threshold}, '
            f'unique_active_members={len(unique_members)}, low_degree_members={len(low_degree_members)}'
        )
        if unique_members:
            print(
                '    Unique active members: '
                + '; '.join(
                    format_role(role_id, role_names, tier_map, role_to_synergies)
                    for role_id in unique_members[:report_limit]
                )
            )

    unselected_roles = [role_id for role_id in roles if role_id not in selected_roles]
    efficient_unselected = [
        role_id for role_id in unselected_roles if len(role_to_synergies.get(role_id, [])) >= 3
    ]
    efficient_unselected.sort(
        key=lambda role_id: (
            -len(role_to_synergies.get(role_id, [])),
            tier_map.get(role_id, 99),
            role_names.get(role_id, ''),
            role_id,
        )
    )

    print('\nBest unselected additions if you must add people:')
    if not efficient_unselected:
        print('  None with 3+ synergies.')
    else:
        for role_id in efficient_unselected[:report_limit]:
            print(f'  {format_role(role_id, role_names, tier_map, role_to_synergies)}')

    synthetic_results = evaluate_synthetic_bridges(
        roles=roles,
        selected_roles=selected_roles,
        min_count=min_count,
        tier_map=tier_map,
        synergies=synergies,
        statuses=statuses,
        burden_rows=burden_rows,
        min_selected=min_selected,
        candidate_limit=max(1, synthetic_candidate_limit),
        max_tags=max(1, synthetic_max_tags),
        synthetic_tier=synthetic_tier,
    )

    print(
        f'\nBest brand-new role bridges (tier={synthetic_tier}, up to {synthetic_max_tags} tags):'
    )
    if not synthetic_results:
        print('  None.')
    else:
        for result in synthetic_results[:report_limit]:
            improvement = min_count - result.min_count
            bundle_text = ', '.join(result.bundle)
            print(
                f'  new_min={result.min_count} delta={improvement} selected={result.selected} '
                f'activated={result.activated_count}/{len(result.bundle)} bundle=[{bundle_text}]'
            )


def write_pool_yaml(
    output_path: Path,
    roles: Sequence[int],
    tier_map: Dict[int, int],
    selected_roles: set[int],
    role_names: Dict[int, str],
) -> None:
    with output_path.open('w', encoding='utf-8', newline='') as handle:
        handle.write('角色:\n')
        last_tier = None
        for role_id in roles:
            if role_id not in selected_roles:
                continue
            tier = tier_map[role_id]
            if tier != last_tier:
                handle.write(f'  # 费用: {tier}\n')
                last_tier = tier
            role_name = role_names.get(role_id)
            if role_name:
                handle.write(f'  - {role_id}  # {role_name}\n')
            else:
                handle.write(f'  - {role_id}\n')


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description='Generate an exact easy chess pool')
    parser.add_argument('--combos', default='config/chess_combos.yaml', help='combo config path')
    parser.add_argument('--pool', default='config/chess_pool.yaml', help='full pool config path')
    parser.add_argument('--output', default='config/chess_pool_easy.yaml', help='easy pool output path')
    parser.add_argument('--db', default='work/game-dev/save/0.db', help='role name database path')
    parser.add_argument(
        '--mode',
        choices=['generate', 'report'],
        default='generate',
        help='generate writes chess_pool_easy.yaml; report prints diagnostics only',
    )
    parser.add_argument(
        '--min-size-exclusive',
        type=int,
        default=30,
        help='require pool size to be strictly greater than this value',
    )
    parser.add_argument(
        '--exclude-synergy',
        action='append',
        default=[],
        help='exclude a synergy by name before solving; can be passed multiple times',
    )
    parser.add_argument(
        '--threshold-override',
        action='append',
        default=[],
        help='override max threshold as 名称=人数; can be passed multiple times',
    )
    parser.add_argument(
        '--report-limit',
        type=int,
        default=12,
        help='maximum number of rows to print per report section',
    )
    parser.add_argument(
        '--synthetic-candidate-limit',
        type=int,
        default=8,
        help='how many candidate synergies to consider for brand-new role bridge analysis',
    )
    parser.add_argument(
        '--synthetic-max-tags',
        type=int,
        default=3,
        help='maximum number of tags to place on one brand-new role in report mode',
    )
    parser.add_argument(
        '--synthetic-tier',
        type=int,
        default=1,
        help='tier cost to assume for a brand-new role in report mode tie-breaks',
    )
    args = parser.parse_args(argv)

    combos_path = Path(args.combos)
    pool_path = Path(args.pool)
    output_path = Path(args.output)
    db_path = Path(args.db) if args.db else None
    min_selected = args.min_size_exclusive + 1

    try:
        threshold_overrides = parse_threshold_overrides(args.threshold_override)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    combos_raw = load_yaml(combos_path)
    roles, tier_map = collect_pool(pool_path)
    universe = set(roles)
    synergies = collect_synergies(combos_raw, universe, threshold_overrides, set(args.exclude_synergy))
    role_names = load_role_names(pool_path, db_path)

    selected_roles, min_count, total_cost = solve_pool(roles, tier_map, synergies, min_selected)

    if args.mode == 'generate':
        write_pool_yaml(output_path, roles, tier_map, selected_roles, role_names)
        print(f'Generated {output_path} with {min_count} roles and total tier cost {total_cost}.')
        included_synergies = summarize_synergies(selected_roles, synergies)
        print(f'Included synergies: {len(included_synergies)} / {len(synergies)}')
        for line in included_synergies:
            print(f'  {line}')
        return 0

    print_report(
        roles=roles,
        selected_roles=selected_roles,
        min_count=min_count,
        total_cost=total_cost,
        synergies=synergies,
        tier_map=tier_map,
        role_names=role_names,
        report_limit=max(1, args.report_limit),
        min_selected=min_selected,
        synthetic_candidate_limit=max(1, args.synthetic_candidate_limit),
        synthetic_max_tags=max(1, args.synthetic_max_tags),
        synthetic_tier=args.synthetic_tier,
    )
    return 0


if __name__ == '__main__':
    raise SystemExit(main())