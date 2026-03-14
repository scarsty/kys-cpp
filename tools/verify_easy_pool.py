#!/usr/bin/env python3
import yaml

with open('config/chess_combos.yaml', 'r', encoding='utf-8') as f:
    combos_raw = yaml.safe_load(f)
with open('config/chess_pool_easy.yaml', 'r', encoding='utf-8') as f:
    easy_pool = yaml.safe_load(f)

if isinstance(easy_pool, dict):
    pool_roles = {int(role_id) for role_id in easy_pool.get('角色', [])}
else:
    pool_roles = set()
    for tier in easy_pool:
        pool_roles.update(tier['角色'])

print(f"Total roles in pool: {len(pool_roles)}")

combos_list = combos_raw.get('羁绊', []) if isinstance(combos_raw, dict) else combos_raw
violations = []

for combo in combos_list:
    members = set(combo.get('成员', []))
    thresholds = combo.get('阈值', [])
    max_threshold = max((t.get('人数', 0) for t in thresholds), default=0)

    in_pool = members & pool_roles

    if len(in_pool) > 0 and len(in_pool) < max_threshold:
        violations.append(f"{combo['名称']}: has {len(in_pool)} but needs {max_threshold}")

if violations:
    print("\nVIOLATIONS:")
    for v in violations:
        print(f"  {v}")
else:
    print("\nOK: All synergies valid!")
