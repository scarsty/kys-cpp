from __future__ import annotations

import argparse
import json
import re
import struct
from collections import defaultdict
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[1]
INT_MAX = 2**31 - 1

TIER_RE = re.compile(r'^\s*(?:#|-)\s*费用:\s*(\d+)\s*$')
ROLE_RE = re.compile(r'^\s*-\s+(\d+)\s+#\s+(.+)$')
MAP_RE = re.compile(r'^\s*\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,')


def f32(value: float) -> float:
    return struct.unpack('!f', struct.pack('!f', float(value)))[0]


class MT19937:
    def __init__(self, seed: int):
        self.mt = [0] * 624
        self.index = 624
        self.seed(seed)

    def seed(self, seed: int) -> None:
        self.index = 624
        self.mt[0] = seed & 0xFFFFFFFF
        for i in range(1, 624):
            self.mt[i] = (1812433253 * (self.mt[i - 1] ^ (self.mt[i - 1] >> 30)) + i) & 0xFFFFFFFF

    def twist(self) -> None:
        for i in range(624):
            y = (self.mt[i] & 0x80000000) + (self.mt[(i + 1) % 624] & 0x7FFFFFFF)
            self.mt[i] = self.mt[(i + 397) % 624] ^ (y >> 1)
            if y & 1:
                self.mt[i] ^= 0x9908B0DF
        self.index = 0

    def rand_u32(self) -> int:
        if self.index >= 624:
            self.twist()
        y = self.mt[self.index]
        self.index += 1
        y ^= (y >> 11)
        y ^= (y << 7) & 0x9D2C5680
        y ^= (y << 15) & 0xEFC60000
        y ^= (y >> 18)
        return y & 0xFFFFFFFF

    def rand_float(self) -> float:
        return f32(self.rand_u32() / 4294967296.0)

    def rand_int(self, n: int) -> int:
        if n <= 0:
            raise ValueError(f'rand_int requires positive n, got {n}')
        return int(self.rand_float() * n)


class CppRandom:
    def __init__(self, seed: int):
        self.enemy = MT19937(seed)

    def enemy_rand_int(self, n: int) -> int:
        return self.enemy.rand_int(n)


def load_pool(path: Path):
    roles_by_tier: dict[int, list[int]] = defaultdict(list)
    role_name: dict[int, str] = {}
    role_tier: dict[int, int] = {}
    current_tier = None

    for raw in path.read_text(encoding='utf-8').splitlines():
        tier_match = TIER_RE.match(raw)
        if tier_match:
            current_tier = int(tier_match.group(1))
            continue

        role_match = ROLE_RE.match(raw)
        if role_match and current_tier is not None:
            role_id = int(role_match.group(1))
            name = role_match.group(2).strip()
            roles_by_tier[current_tier].append(role_id)
            role_name[role_id] = name
            if role_id in role_tier and role_tier[role_id] != current_tier:
                raise ValueError(f'Role {role_id} tier mismatch: {role_tier[role_id]} vs {current_tier}')
            role_tier[role_id] = current_tier

    return roles_by_tier, role_name, role_tier


def load_balance(path: Path):
    data = yaml.safe_load(path.read_text(encoding='utf-8'))
    enemy_table = [[(int(slot[0]), int(slot[1])) for slot in round_] for round_ in data['敌人表']]
    enemy_equipment_levels = [
        {
            'fight': int(entry['关卡']),
            'maxTier': int(entry['最高层级']),
            'count': int(entry['装备数量']),
            'equipBoth': bool(entry.get('双装备', False)),
        }
        for entry in data.get('敌人装备', [])
    ]
    total_fights = int(data['进度']['总关卡数'])
    boss_interval = int(data['进度']['Boss间隔'])
    return enemy_table, total_fights, boss_interval, enemy_equipment_levels


def load_combos(path: Path):
    data = yaml.safe_load(path.read_text(encoding='utf-8'))
    combos = []
    for idx, node in enumerate(data['羁绊']):
        combos.append(
            {
                'id': idx,
                'name': str(node['名称']),
                'member_ids': [int(v) for v in node['成员']],
                'thresholds': [int(t['人数']) for t in node['阈值']],
                'threshold_names': [str(t['名称']) for t in node['阈值']],
                'anti': bool(node.get('反向羁绊', False)),
                'star_bonus': bool(node.get('星级羁绊加成', False)),
            }
        )
    return combos


def load_equipment_counts(path: Path):
    data = yaml.safe_load(path.read_text(encoding='utf-8'))
    counts = defaultdict(lambda: {0: 0, 1: 0})
    for node in data['装备列表']:
        tier = int(node['层级'])
        equip_type = int(node['装备类型'])
        counts[tier][equip_type] += 1
    return counts


def build_star_map(selected):
    star_by_role = {}
    for role_id, star in selected:
        star_by_role[role_id] = star
    return star_by_role


def compute_ownership(combo, star_by_role):
    owned = 0
    effective = 0
    for rid in combo['member_ids']:
        if rid not in star_by_role:
            continue
        owned += 1
        effective += star_by_role[rid] if combo['star_bonus'] else 1
    return owned, effective


def detect_combos(selected, combos, role_tier):
    star_by_role = build_star_map(selected)
    result = []
    for combo in combos:
        member_count = 0
        member_ids = set()
        for rid in combo['member_ids']:
            if rid in star_by_role:
                member_count += 1
                member_ids.add(rid)
        if member_count == 0:
            continue

        physical_member_count = member_count
        if combo['star_bonus']:
            for rid in member_ids:
                member_count += star_by_role[rid] - 1

        active_threshold_idx = -1
        is_anti = combo['anti']
        if is_anti:
            best_id = -1
            best_tier = -1
            for rid in member_ids:
                tier = role_tier.get(rid, -1)
                if tier > best_tier:
                    best_tier = tier
                    best_id = rid
            member_ids = {best_id} if best_id != -1 else set()
            member_count = 1
            physical_member_count = 1
            active_threshold_idx = 0
        else:
            for i in range(len(combo['thresholds']) - 1, -1, -1):
                if member_count >= combo['thresholds'][i]:
                    active_threshold_idx = i
                    break

        result.append(
            {
                'id': combo['id'],
                'member_count': member_count,
                'physical_member_count': physical_member_count,
                'active_threshold_idx': active_threshold_idx,
                'is_anti': is_anti,
                'member_ids': member_ids,
            }
        )
    return result


def format_combo_count(physical_count, effective_count, denominator, star_bonus, is_anti):
    if is_anti:
        return f'solo/{denominator}'

    extra = effective_count - physical_count
    if star_bonus and extra > 0:
        return f'{physical_count}+{extra}/{denominator}'
    return f'{physical_count}/{denominator}'


def summarize_synergies(selected, combos, role_tier):
    summaries = []
    for active in detect_combos(selected, combos, role_tier):
        if active['active_threshold_idx'] < 0:
            continue

        combo = combos[active['id']]
        threshold_index = active['active_threshold_idx']
        threshold_count = combo['thresholds'][threshold_index]
        threshold_name = combo['threshold_names'][threshold_index]
        summaries.append(
            {
                'id': combo['id'],
                'name': combo['name'],
                'threshold_index': threshold_index,
                'threshold_count': threshold_count,
                'threshold_name': threshold_name,
                'count_text': format_combo_count(
                    active['physical_member_count'],
                    active['member_count'],
                    threshold_count,
                    combo['star_bonus'],
                    active['is_anti'],
                ),
                'physical_member_count': active['physical_member_count'],
                'effective_member_count': active['member_count'],
                'is_anti': active['is_anti'],
                'star_bonus': combo['star_bonus'],
            }
        )
    return summaries


def count_enemy_slots_by_tier(slots):
    counts = [0] * 6
    for tier, _ in slots:
        if 1 <= tier <= 5:
            counts[tier] += 1
    return counts


def count_feasible_combo_members_for_slots(combo, slot_counts, role_tier):
    combo_tier_counts = [0] * 6
    for rid in combo['member_ids']:
        tier = role_tier.get(rid, -1)
        if 1 <= tier <= 5:
            combo_tier_counts[tier] += 1
    feasible = 0
    for tier in range(1, 6):
        feasible += min(slot_counts[tier], combo_tier_counts[tier])
    return feasible


def collect_feasible_enemy_combos(slots, role_tier, combos):
    result = []
    slot_counts = count_enemy_slots_by_tier(slots)
    for combo in combos:
        if combo['anti'] or not combo['thresholds']:
            continue
        feasible_members = count_feasible_combo_members_for_slots(combo, slot_counts, role_tier)
        if any(t >= 2 and t <= feasible_members for t in combo['thresholds']):
            result.append(combo['id'])
    return result


def choose_target_threshold(combo, feasible_members, rng):
    thresholds = [t for t in combo['thresholds'] if t >= 2 and t <= feasible_members]
    if not thresholds:
        return -1
    return thresholds[rng.enemy_rand_int(len(thresholds))]


def pick_enemy_role_id(tier_roles, rng, used_ids):
    if not tier_roles:
        return -1
    available = [rid for rid in tier_roles if rid not in used_ids]
    if available:
        return available[rng.enemy_rand_int(len(available))]
    return tier_roles[rng.enemy_rand_int(len(tier_roles))]


def build_random_enemy_lineup(slots, pool_roles_by_tier, rng):
    candidate = {'ids': [], 'stars': []}
    used_ids = set()
    for tier, star in slots:
        role_id = pick_enemy_role_id(pool_roles_by_tier.get(tier, []), rng, used_ids)
        if role_id < 0:
            continue
        candidate['ids'].append(role_id)
        candidate['stars'].append(min(star, 3))
        used_ids.add(role_id)
    return candidate


def nudge_enemy_lineup_toward_combo(candidate, slots, combo, role_tier, rng):
    feasible_members = count_feasible_combo_members_for_slots(combo, count_enemy_slots_by_tier(slots), role_tier)
    target_threshold = choose_target_threshold(combo, feasible_members, rng)
    if target_threshold < 0:
        return

    present_counts = defaultdict(int)
    for role_id in candidate['ids']:
        present_counts[role_id] += 1

    slot_order = list(range(len(candidate['ids'])))
    for i in range(len(slot_order) - 1, 0, -1):
        j = rng.enemy_rand_int(i + 1)
        slot_order[i], slot_order[j] = slot_order[j], slot_order[i]

    replacement_budget = 1 + rng.enemy_rand_int(min(3, max(1, target_threshold)))
    for slot_index in slot_order:
        if replacement_budget <= 0 or slot_index >= len(slots):
            break

        current_pieces = [(rid, star) for rid, star in zip(candidate['ids'], candidate['stars']) if rid in role_tier]
        _, effective = compute_ownership(combo, build_star_map(current_pieces))
        if effective >= target_threshold:
            break

        current_role_id = candidate['ids'][slot_index]
        if current_role_id in combo['member_ids']:
            continue

        slot_tier = slots[slot_index][0]
        replacements = [
            role_id
            for role_id in combo['member_ids']
            if role_tier.get(role_id) == slot_tier and present_counts.get(role_id, 0) == 0
        ]

        if not replacements:
            continue
        if rng.enemy_rand_int(100) >= 75:
            continue

        replacement = replacements[rng.enemy_rand_int(len(replacements))]
        present_counts[current_role_id] -= 1
        if present_counts[current_role_id] <= 0:
            present_counts.pop(current_role_id, None)
        candidate['ids'][slot_index] = replacement
        present_counts[replacement] += 1
        replacement_budget -= 1


def score_enemy_lineup(candidate, combos, role_tier):
    pieces = [(rid, star) for rid, star in zip(candidate['ids'], candidate['stars']) if rid in role_tier]
    star_by_role = build_star_map(pieces)
    active_combos = detect_combos(pieces, combos, role_tier)
    score = 0

    for combo in combos:
        if combo['anti'] or not combo['thresholds']:
            continue
        owned, effective = compute_ownership(combo, star_by_role)
        if owned < 2:
            continue
        score += owned * 4
        best_missing = min((max(0, threshold - effective) for threshold in combo['thresholds'] if threshold >= 2), default=None)
        if best_missing == 1:
            score += 18
        elif best_missing == 2:
            score += 7

    for active in active_combos:
        if active['active_threshold_idx'] < 0 or active['is_anti']:
            continue
        combo = combos[active['id']]
        threshold = combo['thresholds'][active['active_threshold_idx']]
        score += 90
        score += threshold * 25
        score += active['active_threshold_idx'] * 20
        score += active['member_count'] * 5
        if combo['star_bonus']:
            score += 10

    score -= (len(candidate['ids']) - len(set(candidate['ids']))) * 35
    return score


def generate_synergy_biased_enemy_lineup(slots, pool_roles_by_tier, role_tier, combos, rng):
    feasible_combos = collect_feasible_enemy_combos(slots, role_tier, combos)
    attempt_count = max(10, min(20, 6 + len(slots) * 2))
    candidates = []

    for _ in range(attempt_count):
        candidate = build_random_enemy_lineup(slots, pool_roles_by_tier, rng)
        if feasible_combos and rng.enemy_rand_int(100) < 70:
            combo_id = feasible_combos[rng.enemy_rand_int(len(feasible_combos))]
            nudge_enemy_lineup_toward_combo(candidate, slots, combos[combo_id], role_tier, rng)
        candidate['score'] = score_enemy_lineup(candidate, combos, role_tier)
        candidate['tiebreak'] = rng.enemy_rand_int(INT_MAX)
        candidates.append(candidate)

    candidates.sort(key=lambda c: (c['score'], c['tiebreak']), reverse=True)
    top_count = min(4, max(1, len(candidates) // 3 + 1))
    return candidates[rng.enemy_rand_int(top_count)]


def equipment_rng_calls(fight_num_1based, enemy_count, enemy_equipment_levels, equip_counts_by_tier):
    max_tier = 0
    equip_count = 0
    equip_both = False
    for level in sorted(enemy_equipment_levels, key=lambda x: x['fight']):
        if fight_num_1based >= level['fight']:
            max_tier = level['maxTier']
            equip_count = level['count']
            equip_both = level['equipBoth']

    if max_tier <= 0 or equip_count <= 0:
        return 0

    tier_counts = equip_counts_by_tier.get(max_tier, {0: 0, 1: 0})
    if tier_counts[0] + tier_counts[1] == 0:
        return 0

    slots = min(equip_count, enemy_count)
    if not equip_both:
        return slots

    calls = 0
    if tier_counts[0] > 0:
        calls += slots
    if tier_counts[1] > 0:
        calls += slots
    return calls


def parse_map_candidate_counts(dynamic_map_path: Path):
    enemy_counts = []
    for line in dynamic_map_path.read_text(encoding='utf-8').splitlines():
        match = MAP_RE.match(line)
        if match:
            enemy_counts.append(int(match.group(3)))

    counts_by_needed = {}
    for needed in range(1, 21):
        counts_by_needed[needed] = sum(1 for count in enemy_counts if count >= needed)
    return counts_by_needed


def simulate_run(label, balance_path, pool_path, seed, combos, role_tier, role_name, equip_counts_by_tier, map_candidate_counts):
    enemy_table, total_fights, boss_interval, enemy_equipment_levels = load_balance(balance_path)
    pool_roles_by_tier, _, _ = load_pool(pool_path)
    rng = CppRandom(seed)

    result = []
    for fight_idx in range(total_fights):
        fight_no = fight_idx + 1
        slots = enemy_table[min(fight_idx, len(enemy_table) - 1)]

        if label == 'normal':
            candidate = generate_synergy_biased_enemy_lineup(slots, pool_roles_by_tier, role_tier, combos, rng)
            enemy_ids = candidate['ids']
            enemy_stars = candidate['stars']
        else:
            enemy_ids = []
            enemy_stars = []
            for tier, star in slots:
                tier_roles = pool_roles_by_tier.get(tier, [])
                role_id = tier_roles[rng.enemy_rand_int(len(tier_roles))]
                enemy_ids.append(role_id)
                enemy_stars.append(min(star, 3))

        eq_calls = equipment_rng_calls(fight_no, len(enemy_ids), enemy_equipment_levels, equip_counts_by_tier)
        for _ in range(eq_calls):
            rng.enemy_rand_int(1000000)

        rng.enemy_rand_int(INT_MAX)

        candidate_count = max(1, map_candidate_counts.get(len(enemy_ids), 1))
        rng.enemy_rand_int(candidate_count)

        synergies = summarize_synergies(list(zip(enemy_ids, enemy_stars)), combos, role_tier)

        result.append(
            {
                'fight': fight_no,
                'boss': fight_no % boss_interval == 0,
                'enemies': [
                    {
                        'id': rid,
                        'name': role_name.get(rid, f'ID{rid}'),
                        'star': star,
                    }
                    for rid, star in zip(enemy_ids, enemy_stars)
                ],
                'synergies': synergies,
            }
        )

    return {
        'difficulty': label,
        'seed': seed,
        'result': result,
    }


def print_report(payload):
    def escape_text(text):
        return text.encode('unicode_escape').decode('ascii')

    for key in ('easy', 'normal'):
        run = payload[key]
        print(f"{key.upper()} run using {run['difficulty']} pool and seed {run['seed']}")
        for fight in run['result']:
            boss_tag = ' BOSS' if fight['boss'] else ''
            enemies = ', '.join(
                f"{enemy['id']}:{escape_text(enemy['name'])}({enemy['star']}*)" for enemy in fight['enemies']
            )
            print(f"F{fight['fight']:02d}{boss_tag}: {enemies}")

            synergies = fight.get('synergies', [])
            if synergies:
                for synergy in synergies:
                    print(
                        f"  synergy: {escape_text(synergy['name'])} -> {synergy['count_text']} "
                        f"[{escape_text(synergy['threshold_name'])} ({synergy['threshold_count']})]"
                    )
            else:
                print('  synergy: none')
        print()


def main() -> int:
    parser = argparse.ArgumentParser(description='Simulate enemy encounters for chess balance configs.')
    parser.add_argument('--seed', type=int, default=20260322, help='Deterministic enemy RNG seed.')
    parser.add_argument('--json', action='store_true', help='Print JSON instead of the human-readable report.')
    args = parser.parse_args()

    easy_pool_roles, easy_names, easy_tiers = load_pool(ROOT / 'config' / 'chess_pool_easy.yaml')
    normal_pool_roles, normal_names, normal_tiers = load_pool(ROOT / 'config' / 'chess_pool.yaml')

    role_name = dict(easy_names)
    role_name.update(normal_names)
    role_tier = dict(easy_tiers)
    for rid, tier in normal_tiers.items():
        if rid in role_tier and role_tier[rid] != tier:
            raise ValueError(f'Role {rid} tier mismatch across pools: {role_tier[rid]} vs {tier}')
        role_tier[rid] = tier

    combos = load_combos(ROOT / 'config' / 'chess_combos.yaml')
    missing_ids = sorted({rid for combo in combos for rid in combo['member_ids'] if rid not in role_tier})
    if missing_ids:
        raise SystemExit(f'Missing role tier data for combo members: {missing_ids}')

    equip_counts_by_tier = load_equipment_counts(ROOT / 'config' / 'chess_equipment.yaml')
    map_candidate_counts = parse_map_candidate_counts(ROOT / 'src' / 'DynamicChessMap.cpp')

    payload = {
        'easy': simulate_run('easy', ROOT / 'config' / 'chess_balance_easy.yaml', ROOT / 'config' / 'chess_pool_easy.yaml', args.seed, combos, role_tier, role_name, equip_counts_by_tier, map_candidate_counts),
        'normal': simulate_run('normal', ROOT / 'config' / 'chess_balance_normal.yaml', ROOT / 'config' / 'chess_pool.yaml', args.seed, combos, role_tier, role_name, equip_counts_by_tier, map_candidate_counts),
    }

    if args.json:
        print(json.dumps(payload, ensure_ascii=True, indent=2))
    else:
        print_report(payload)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())