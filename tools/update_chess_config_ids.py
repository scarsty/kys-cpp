#!/usr/bin/env python3
"""Update chess config files with new character IDs."""

import re
from pathlib import Path

ID_MAPPING = {
    2000: 589, 2001: 78, 2002: 4, 2003: 116, 2004: 29,
    2005: 27, 2006: 608, 2007: 609, 2008: 9, 2009: 14,
    2010: 13, 2011: 610, 2012: 11, 2013: 12, 2014: 611,
    2015: 612, 2016: 92, 2017: 613, 2018: 614, 2019: 615,
    2020: 181, 2021: 616, 2022: 617, 2023: 618, 2024: 619,
    2025: 182, 2026: 620, 2027: 621, 2028: 1, 2029: 2,
    2030: 28, 2031: 16, 2032: 622, 2033: 19, 2034: 133,
    2035: 136, 2036: 134, 2037: 135,
    # Previous intermediate mappings
    590: 589, 79: 78, 5: 4, 117: 116, 30: 29,
    28: 27, 609: 608, 610: 609, 10: 9, 15: 14,
    14: 13, 611: 610, 12: 11, 13: 12, 612: 611,
    613: 612, 93: 92, 614: 613, 615: 614, 616: 615,
    182: 181, 617: 616, 618: 617, 619: 618, 620: 619,
    183: 182, 621: 620, 622: 621, 2: 1, 3: 2,
    29: 28, 17: 16, 623: 622, 20: 19, 134: 133,
    137: 136, 135: 134, 136: 135,
}

def update_file(path: Path) -> int:
    content = path.read_text(encoding='utf-8')
    original = content

    for old_id, new_id in ID_MAPPING.items():
        content = re.sub(rf'\b{old_id}\b', str(new_id), content)

    if content != original:
        path.write_text(content, encoding='utf-8')
        return 1
    return 0

if __name__ == '__main__':
    repo = Path(__file__).resolve().parents[1]
    patterns = ['**/chess_pool*.yaml', '**/chess_combo*.yaml']

    updated = 0
    for pattern in patterns:
        for path in repo.glob(pattern):
            if update_file(path):
                print(f'Updated: {path.relative_to(repo)}')
                updated += 1

    print(f'\nTotal files updated: {updated}')
