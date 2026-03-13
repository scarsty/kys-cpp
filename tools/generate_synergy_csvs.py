#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate two CSVs from config/chess_combos.yaml

- `synergy_per_character.csv`: one row per character (id,name,synergies)
- `characters_per_synergy.csv`: one row per synergy (synergy,member_count,member_ids,member_names)

Usage:
    python tools/generate_synergy_csvs.py -i config/chess_combos.yaml -o output
"""
from __future__ import annotations

import argparse
import csv
import os
import re
import sys
from typing import Dict, List, Tuple


def parse_synergies_and_comments(yaml_path: str) -> Tuple[List[Dict], Dict[int, str]]:
    """Parse the YAML-like file to extract synergy names, member ids, and inline comment names.

    This is a lightweight parser that looks for lines like:
      - 名称: 江南七怪
        成员:
          - 130  # 柯鎮惡

    Returns a list of synergy dicts: {'name': str, 'members': [int,...]}
    and a dict mapping id->name extracted from inline comments when present.
    """
    synergies: List[Dict] = []
    id_to_name: Dict[int, str] = {}

    cur = None
    in_members = False

    with open(yaml_path, 'r', encoding='utf-8') as f:
        for line in f:
            raw = line.rstrip('\n')
            # detect new synergy block: '- 名称: <name>'
            m = re.match(r'^\s*-\s*名称\s*:\s*(.+)$', raw)
            if m:
                name = m.group(1).strip()
                cur = {'name': name, 'members': []}
                synergies.append(cur)
                in_members = False
                continue

            # detect members section
            if '成员:' in raw:
                if cur is not None:
                    in_members = True
                continue

            if in_members:
                # member line: '- 130  # name' or '- 130'
                m2 = re.match(r'^\s*-\s*(\d+)(?:\s*#\s*(.+))?$', raw)
                if m2:
                    cid = int(m2.group(1))
                    comment = m2.group(2)
                    cur['members'].append(cid)
                    if comment and comment.strip():
                        id_to_name[cid] = comment.strip()
                    continue
                else:
                    # left the members list
                    in_members = False

    return synergies, id_to_name


def main(argv=None):
    parser = argparse.ArgumentParser(description='Generate synergy CSVs from chess_combos.yaml')
    parser.add_argument('-i', '--input', default='config/chess_combos.yaml', help='path to chess_combos.yaml')
    parser.add_argument('-o', '--outdir', default='output', help='output directory')
    args = parser.parse_args(argv)

    yaml_path = args.input
    outdir = args.outdir

    if not os.path.exists(yaml_path):
        print(f'Input file not found: {yaml_path}', file=sys.stderr)
        sys.exit(2)

    synergies, id_to_name = parse_synergies_and_comments(yaml_path)

    # build char -> synergy list
    char_to_synergies: Dict[int, List[str]] = {}
    for syn in synergies:
        name = syn['name']
        for cid in syn['members']:
            char_to_synergies.setdefault(cid, []).append(name)

    os.makedirs(outdir, exist_ok=True)

    # determine maximum counts for fixed columns
    max_synergies = max((len(s) for s in char_to_synergies.values()), default=0)
    max_members = max((len(s['members']) for s in synergies), default=0)

    csv1 = os.path.join(outdir, 'synergy_per_character.csv')
    with open(csv1, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        header = ['id', 'name', 'synergy_count'] + [f'synergy_{i+1}' for i in range(max_synergies)]
        w.writerow(header)
        for cid in sorted(char_to_synergies.keys()):
            name = id_to_name.get(cid, '')
            syns = char_to_synergies[cid]
            row = [cid, name, len(syns)] + [syns[i] if i < len(syns) else '' for i in range(max_synergies)]
            w.writerow(row)

    csv2 = os.path.join(outdir, 'characters_per_synergy.csv')
    with open(csv2, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        header = ['synergy', 'member_count'] + [f'member_{i+1}' for i in range(max_members)]
        w.writerow(header)
        for syn in synergies:
            members = syn['members']
            names = [id_to_name.get(x, '') for x in members]
            row = [syn['name'], len(members)] + [names[i] if i < len(names) else '' for i in range(max_members)]
            w.writerow(row)

    print(f'Wrote: {csv1}\n       {csv2}')


if __name__ == '__main__':
    main()
