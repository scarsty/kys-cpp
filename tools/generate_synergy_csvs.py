#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate CSVs from config/chess_combos.yaml for both full and easy pools

- `synergy_per_character_<pool>.csv`: one row per character in pool
- `characters_per_synergy_<pool>.csv`: one row per synergy with members in pool

Usage:
    python tools/generate_synergy_csvs.py
"""
import csv
import os
import re
import yaml

def parse_synergies_and_comments(yaml_path):
    synergies = []
    id_to_name = {}
    cur = None
    in_members = False

    with open(yaml_path, 'r', encoding='utf-8') as f:
        for line in f:
            raw = line.rstrip('\n')
            m = re.match(r'^\s*-\s*名称\s*:\s*(.+)$', raw)
            if m:
                cur = {'name': m.group(1).strip(), 'members': []}
                synergies.append(cur)
                in_members = False
                continue
            if '成员:' in raw:
                if cur is not None:
                    in_members = True
                continue
            if in_members:
                m2 = re.match(r'^\s*-\s*(\d+)(?:\s*#\s*(.+))?$', raw)
                if m2:
                    cid = int(m2.group(1))
                    cur['members'].append(cid)
                    if m2.group(2):
                        id_to_name[cid] = m2.group(2).strip()
                else:
                    in_members = False
    return synergies, id_to_name

def load_pool(pool_path):
    with open(pool_path, 'r', encoding='utf-8') as f:
        pool_data = yaml.safe_load(f)
    if isinstance(pool_data, dict):
        return {int(role_id) for role_id in pool_data.get('角色', [])}
    return {int(role_id) for tier in pool_data for role_id in tier['角色']}

def generate_csvs(synergies, id_to_name, pool_ids, pool_name, outdir):
    filtered_synergies = [{'name': s['name'], 'members': [m for m in s['members'] if m in pool_ids]} for s in synergies]
    filtered_synergies = [s for s in filtered_synergies if s['members']]

    char_to_synergies = {}
    for syn in filtered_synergies:
        for cid in syn['members']:
            char_to_synergies.setdefault(cid, []).append(syn['name'])

    max_synergies = max((len(s) for s in char_to_synergies.values()), default=0)
    max_members = max((len(s['members']) for s in filtered_synergies), default=0)

    csv1 = os.path.join(outdir, f'synergy_per_character_{pool_name}.csv')
    with open(csv1, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['id', 'name', 'synergy_count'] + [f'synergy_{i+1}' for i in range(max_synergies)])
        for cid in sorted(char_to_synergies.keys()):
            syns = char_to_synergies[cid]
            w.writerow([cid, id_to_name.get(cid, ''), len(syns)] + [syns[i] if i < len(syns) else '' for i in range(max_synergies)])

    csv2 = os.path.join(outdir, f'characters_per_synergy_{pool_name}.csv')
    with open(csv2, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['synergy', 'member_count'] + [f'member_{i+1}' for i in range(max_members)])
        for syn in filtered_synergies:
            names = [id_to_name.get(x, '') for x in syn['members']]
            w.writerow([syn['name'], len(syn['members'])] + [names[i] if i < len(names) else '' for i in range(max_members)])

    print(f'Wrote: {csv1}\n       {csv2}')

synergies, id_to_name = parse_synergies_and_comments('config/chess_combos.yaml')
os.makedirs('output', exist_ok=True)

full_pool = load_pool('config/chess_pool.yaml')
generate_csvs(synergies, id_to_name, full_pool, 'full', 'output')

easy_pool = load_pool('config/chess_pool_easy.yaml')
generate_csvs(synergies, id_to_name, easy_pool, 'easy', 'output')

