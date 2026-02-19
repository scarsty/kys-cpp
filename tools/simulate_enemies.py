#!/usr/bin/env python3
"""Simulate enemy scaling: shop-weight + budget system."""

import random

TOTAL_FIGHTS = 28
BOSS_INTERVAL = 4
TIER_PRICES = [1, 2, 3, 4, 5]
STAR_COST_MULT = 3
TIER_NAMES = ["1-cost", "2-cost", "3-cost", "4-cost", "5-cost"]

INITIAL_GOLD = 15
REWARD_BASE = 4
REWARD_GROWTH = 12
BOSS_REWARD_BONUS = 5
BUDGET_RATIO = 0.50

BATTLE_EXP = 3
BOSS_BATTLE_EXP = 8
EXP_TABLE = [4, 6, 10, 14, 18, 22, 26, 32, 38]

# Shop weights [level 0-9][tier 0-4]
SHOP_WEIGHTS = [
    [100, 0, 0, 0, 0],
    [70, 30, 0, 0, 0],
    [60, 35, 5, 0, 0],
    [50, 35, 15, 0, 0],
    [40, 35, 23, 2, 0],
    [33, 30, 30, 7, 0],
    [30, 30, 30, 10, 0],
    [24, 30, 30, 15, 1],
    [22, 30, 25, 20, 3],
    [19, 25, 25, 25, 6],
]


def is_boss(f):
    return (f + 1) % BOSS_INTERVAL == 0

def fight_reward(f):
    r = REWARD_BASE + f * REWARD_GROWTH // (TOTAL_FIGHTS - 1)
    return r + BOSS_REWARD_BONUS if is_boss(f) else r

def piece_cost(tier, star):
    return TIER_PRICES[tier] * (STAR_COST_MULT ** star)

def pick_tier(level, max_tier):
    """Weighted random tier pick using shop weights at given level."""
    w = SHOP_WEIGHTS[min(level, 9)][:max_tier + 1]
    total = sum(w)
    if total == 0:
        return 0
    r = random.randint(0, total - 1)
    for t, wt in enumerate(w):
        r -= wt
        if r < 0:
            return t
    return 0

def fmt(enemies):
    comp = {}
    for t, s in enemies:
        k = f"{TIER_NAMES[t]}*{s+1}"
        comp[k] = comp.get(k, 0) + 1
    return ", ".join(f"{v}x {k}" for k, v in sorted(comp.items()))


def allocate(budget, count, bosses_seen, fight, enemy_level):
    """Shop-weight tiers + budget for star-ups.

    - 2 bad pieces (1-cost*1), -1 per boss seen (gone after boss 2)
    - No 5-cost until final 2 rounds
    - Pick tiers via shop weights at enemy_level
    - Spend budget on star-ups
    """
    bad_count = min(max(0, 2 - bosses_seen), count)
    max_tier = 4 if fight >= TOTAL_FIGHTS - 2 else 3

    # Bad pieces
    ene = [[0, 0] for _ in range(bad_count)]

    # Good pieces: roll tiers from shop weights
    for _ in range(count - bad_count):
        ene.append([pick_tier(enemy_level, max_tier), 0])

    # Spend budget on star-ups
    base_cost = sum(TIER_PRICES[e[0]] for e in ene)
    remaining = budget - base_cost
    if remaining < 0:
        remaining = 0

    changed = True
    while changed and remaining > 0:
        changed = False
        idx = list(range(len(ene)))
        random.shuffle(idx)
        for i in idx:
            t, s = ene[i]
            if s < 2:
                cost = piece_cost(t, s + 1) - piece_cost(t, s)
                if cost <= remaining:
                    ene[i][1] += 1
                    remaining -= cost
                    changed = True

    random.shuffle(ene)
    return [(e[0], e[1]) for e in ene]


def simulate():
    print("=" * 90)
    print(f"BUDGET SYSTEM: {TOTAL_FIGHTS} rounds, boss every {BOSS_INTERVAL},"
          f" {int(BUDGET_RATIO*100)}% budget, shop-weight tiers, 2 bad pieces")
    print("=" * 90)

    rounds = []
    cumul = INITIAL_GOLD
    bosses_seen = 0
    enemy_exp = 0
    enemy_level = 0

    print(f"{'Rnd':>4} {'':>4} {'#':>3} {'Lv':>3} {'Budg':>5} {'Spnt':>5} {'Bad':>4}  Composition")
    print("-" * 90)

    for f in range(TOTAL_FIGHTS):
        boss = is_boss(f)
        cumul += fight_reward(f)
        count = 2 + bosses_seen + (1 if boss else 0)
        budget = int(cumul * BUDGET_RATIO)
        bad = min(max(0, 2 - bosses_seen), count)

        ene = allocate(budget, count, bosses_seen, f, enemy_level)
        rounds.append(ene)
        spent = sum(piece_cost(t, s) for t, s in ene)

        tag = "BOSS" if boss else ""
        print(f"{f+1:>4} {tag:>4} {count:>3} {enemy_level:>3}"
              f" {budget:>5} {spent:>5} {bad:>4}  {fmt(ene)}")

        # Level up enemy after fight
        enemy_exp += BOSS_BATTLE_EXP if boss else BATTLE_EXP
        while enemy_level < 9 and enemy_level < len(EXP_TABLE) and enemy_exp >= EXP_TABLE[enemy_level]:
            enemy_exp -= EXP_TABLE[enemy_level]
            enemy_level += 1

        if boss:
            bosses_seen += 1

    print(f"\nFinal count: {2 + bosses_seen} | Bosses: {bosses_seen}"
          f" (rounds {', '.join(str(i+1) for i in range(TOTAL_FIGHTS) if is_boss(i))})")
    return rounds


def dump_yaml(rounds):
    """Print YAML for the enemy table."""
    print("\n# --- YAML enemy table ---")
    print("敌人表:")
    for i, ene in enumerate(rounds):
        boss = is_boss(i)
        tag = "  # BOSS" if boss else ""
        slots = ", ".join(f"[{t}, {s}]" for t, s in ene)
        print(f"  - [{slots}]{tag}")


if __name__ == "__main__":
    random.seed(42)
    rounds = simulate()
    dump_yaml(rounds)
