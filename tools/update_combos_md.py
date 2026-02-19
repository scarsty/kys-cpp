"""Parse chess_combos.md combo definitions and regenerate summary/coverage tables."""
import re, sys
from collections import defaultdict, OrderedDict
from pathlib import Path

MD_PATH = Path(__file__).resolve().parent.parent / "docs" / "chess_combos.md"

def parse(text):
    # Parse cost lookup from 棋子一览
    cost_map = {}
    for m in re.finditer(r'\|\s*(\d)费\(T\d\)\s*\|\s*(.+?)\s*\|', text):
        cost = int(m.group(1))
        for name in re.split(r'[、，,]', m.group(2).strip()):
            name = name.strip()
            if name:
                cost_map[name] = cost

    # Parse combo sections
    combos = []
    # Split by ### headers
    sections = re.split(r'(?=^### \d+\.)', text, flags=re.MULTILINE)
    for sec in sections:
        hdr = re.match(r'### (\d+)\.\s*(.+)', sec)
        if not hdr:
            continue
        num, name = int(hdr.group(1)), hdr.group(2).strip()

        # Parse member table: find the table after "| 成员 | 费用 |"
        members = []
        member_table = re.search(r'\|\s*成员\s*\|\s*费用\s*\|.*?\n\|[-\s|]+\n((?:\|.+\n)*)', sec)
        if member_table:
            for row in re.finditer(r'^\|\s*(.+?)\s*\|\s*(\d)\s*\|', member_table.group(1), re.MULTILINE):
                members.append((row.group(1).strip(), int(row.group(2))))

        # Parse thresholds from effect table (阈值/条件 header)
        thresholds = []
        effect_table = re.search(r'\|\s*(?:阈值|条件)\s*\|\s*效果名\s*\|\s*效果\s*\|.*?\n\|[-\s|]+\n((?:\|.+\n)*)', sec)
        if effect_table:
            for row in re.finditer(r'^\|\s*(.+?)\s*\|', effect_table.group(1), re.MULTILINE):
                t = row.group(1).strip()
                if t and t != '---':
                    thresholds.append(t)

        combos.append({'num': num, 'name': name, 'members': members, 'thresholds': thresholds})

    return cost_map, combos


def gen_summary(combos, cost_map, existing_positions):
    """Generate 羁绊总览 table."""
    lines = [
        "## 羁绊总览",
        "",
        "| # | 羁绊名 | 成员数 | 成员费用 | 阈值 | 定位 |",
        "|---|--------|--------|----------|------|------|",
    ]
    for c in combos:
        # Cost breakdown
        costs = sorted([cost for _, cost in c['members']])
        cost_counts = defaultdict(int)
        for co in costs:
            cost_counts[co] += 1
        cost_parts = []
        for co in sorted(cost_counts):
            cnt = cost_counts[co]
            cost_parts.append(f"{cnt}×{co}费" if cnt > 1 else f"{co}费")
        cost_str = ", ".join(cost_parts)

        # Thresholds
        thresh_str = "/".join(c['thresholds']) if c['thresholds'] else "?"
        # Check if it's 独行 style (anti-combo)
        if any('场上' in t for t in c['thresholds']):
            thresh_str = "反羁绊"

        pos = existing_positions.get(c['num'], "")
        lines.append(f"| {c['num']} | {c['name']} | {len(c['members'])} | {cost_str} | {thresh_str} | {pos} |")
    return "\n".join(lines)


def gen_coverage(combos, cost_map):
    """Generate 棋子羁绊覆盖表."""
    # Build piece -> combos mapping
    piece_combos = defaultdict(list)
    for c in combos:
        for mname, _ in c['members']:
            piece_combos[mname].append(c['name'])

    # Sort by cost then by order in cost_map
    all_pieces = []
    for name, cost in sorted(cost_map.items(), key=lambda x: (x[1], x[0])):
        all_pieces.append((name, cost))

    lines = [
        "## 棋子羁绊覆盖表",
        "",
        "| 棋子 | 费用 | 所属羁绊 | 数量 |",
        "|------|------|----------|------|",
    ]
    warnings = []
    for name, cost in all_pieces:
        clist = piece_combos.get(name, [])
        cnt = len(clist)
        mark = "✓" if cnt >= 2 else "✗"
        lines.append(f"| {name} | {cost} | {', '.join(clist)} | {cnt} {mark} |")
        if cnt < 2:
            warnings.append(f"  WARNING: {name}({cost}费) 只有 {cnt} 个羁绊!")

    return "\n".join(lines), warnings


POSITIONS = {
    1: "固定减伤+%防御",
    2: "%攻击+速度+低血狂暴",
    3: "固定生命+反弹",
    4: "固定生命+回蓝+减控",
    5: "%攻击+固定防御速度+控制免疫",
    6: "固定速度+冷却缩减+技能后无敌",
    7: "回蓝+%技能伤害",
    8: "%攻击-%防御+双倍伤害",
    9: "闪避+闪避后暴击",
    10: "固定防御+%生命+回血",
    11: "开局护盾+冷却缩减",
    12: "固定攻击生命+眩晕",
    13: "低血爆发(%攻击+固定速度)",
    14: "%当前生命DOT+中毒增伤",
    15: "命中回蓝+吸取敌方MP",
    16: "击杀回血+击杀后无敌",
    17: "固定攻击+%技能伤害+穿甲",
    18: "%攻击+速度+穿透",
    19: "固定攻击生命+击退",
    20: "%攻击+暴击",
    21: "治疗光环+被治疗者攻速",
    22: "冷却缩减+%技能伤害",
    23: "人少则强，减伤+%攻防速",
}


def parse_existing_positions(text):
    """Extract 定位 column from existing 羁绊总览 table, fall back to hardcoded."""
    positions = dict(POSITIONS)
    m = re.search(r'## 羁绊总览\n(.*?)(?=\n## )', text, re.DOTALL)
    if not m:
        return positions
    table_text = m.group(1)
    for row in re.finditer(r'^\|\s*(\d+)\s*\|(.+)\|$', table_text, re.MULTILINE):
        num = int(row.group(1))
        cols = [c.strip() for c in row.group(2).split('|')]
        if len(cols) >= 5:
            val = cols[-1]
            if val and val not in ('---', '定位', ''):
                positions[num] = val
    return positions


def main():
    text = MD_PATH.read_text(encoding='utf-8')
    cost_map, combos = parse(text)

    summary = gen_summary(combos, cost_map, POSITIONS)
    coverage, warnings = gen_coverage(combos, cost_map)

    # Replace from "## 羁绊总览" to just before "## 速度"
    pattern = r'## 羁绊总览.*?(?=## 速度)'
    replacement = summary + "\n\n" + coverage + "\n\n"
    new_text = re.sub(pattern, replacement, text, flags=re.DOTALL)

    MD_PATH.write_text(new_text, encoding='utf-8')
    print(f"Updated {MD_PATH}")
    print(f"  {len(combos)} combos, {len(cost_map)} pieces")
    for w in warnings:
        print(w)


if __name__ == '__main__':
    main()
