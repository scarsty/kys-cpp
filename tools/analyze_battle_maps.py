#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Analyze battle maps to find good ones for 10 teammates.
Good maps have:
1. At least 10 enemies with valid coordinates
2. At least 10 walkable positions for teammates that are:
   - Close to existing teammate positions (within 1-2 spaces)
   - Connected and walkable
   - Not water, not building, not out of bounds
"""

import struct
import os
import sys
from typing import List, Tuple, Set
from collections import deque

# Ensure UTF-8 output
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

# Constants from the C++ code
TEAMMATE_COUNT = 6
BATTLE_ENEMY_COUNT = 20
BATTLEMAP_COORD_COUNT = 64
BATTLEMAP_SAVE_LAYER_COUNT = 2

# Water tile numbers from isWater() function
WATER_TILES = set([179, 180, 181, 261, 511, 662, 663, 664, 665, 674])


class BattleInfo:
    """Corresponds to BattleInfo struct in C++"""
    def __init__(self, data: bytes):
        # Parse the binary structure
        # MAP_INT is int16_t (2 bytes)
        offset = 0

        self.id = struct.unpack('<h', data[offset:offset+2])[0]
        offset += 2

        # Decode from Big5/CP950 to Unicode string
        try:
            self.name = data[offset:offset+10].decode('big5', errors='ignore').rstrip('\x00')
        except:
            self.name = data[offset:offset+10].decode('gbk', errors='ignore').rstrip('\x00')
        offset += 10

        self.battlefield_id = struct.unpack('<h', data[offset:offset+2])[0]
        offset += 2
        self.exp = struct.unpack('<h', data[offset:offset+2])[0]
        offset += 2
        self.music = struct.unpack('<h', data[offset:offset+2])[0]
        offset += 2

        # TeamMate arrays
        self.teammate = list(struct.unpack('<' + 'h' * TEAMMATE_COUNT, data[offset:offset+2*TEAMMATE_COUNT]))
        offset += 2 * TEAMMATE_COUNT

        self.auto_teammate = list(struct.unpack('<' + 'h' * TEAMMATE_COUNT, data[offset:offset+2*TEAMMATE_COUNT]))
        offset += 2 * TEAMMATE_COUNT

        self.teammate_x = list(struct.unpack('<' + 'h' * TEAMMATE_COUNT, data[offset:offset+2*TEAMMATE_COUNT]))
        offset += 2 * TEAMMATE_COUNT

        self.teammate_y = list(struct.unpack('<' + 'h' * TEAMMATE_COUNT, data[offset:offset+2*TEAMMATE_COUNT]))
        offset += 2 * TEAMMATE_COUNT

        # Enemy arrays
        self.enemy = list(struct.unpack('<' + 'h' * BATTLE_ENEMY_COUNT, data[offset:offset+2*BATTLE_ENEMY_COUNT]))
        offset += 2 * BATTLE_ENEMY_COUNT

        self.enemy_x = list(struct.unpack('<' + 'h' * BATTLE_ENEMY_COUNT, data[offset:offset+2*BATTLE_ENEMY_COUNT]))
        offset += 2 * BATTLE_ENEMY_COUNT

        self.enemy_y = list(struct.unpack('<' + 'h' * BATTLE_ENEMY_COUNT, data[offset:offset+2*BATTLE_ENEMY_COUNT]))
        offset += 2 * BATTLE_ENEMY_COUNT


class BattleFieldData:
    """Corresponds to BattleFieldData2 struct"""
    def __init__(self, data: bytes):
        # 2 layers, each 64x64 grid of int16_t
        self.layers = []
        offset = 0
        for layer in range(BATTLEMAP_SAVE_LAYER_COUNT):
            grid = []
            for i in range(BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT):
                value = struct.unpack('<h', data[offset:offset+2])[0]
                grid.append(value)
                offset += 2
            self.layers.append(grid)

    def get_earth(self, x: int, y: int) -> int:
        """Get earth layer value (layer 0)"""
        if 0 <= x < BATTLEMAP_COORD_COUNT and 0 <= y < BATTLEMAP_COORD_COUNT:
            return self.layers[0][x + y * BATTLEMAP_COORD_COUNT]
        return -1

    def get_building(self, x: int, y: int) -> int:
        """Get building layer value (layer 1)"""
        if 0 <= x < BATTLEMAP_COORD_COUNT and 0 <= y < BATTLEMAP_COORD_COUNT:
            return self.layers[1][x + y * BATTLEMAP_COORD_COUNT]
        return -1


def load_battle_infos(filepath: str) -> List[BattleInfo]:
    """Load all battle info records from war.sta"""
    with open(filepath, 'rb') as f:
        data = f.read()

    # Calculate size of one BattleInfo struct
    # 2 + 10 + 2 + 2 + 2 + (6*2)*4 + (20*2)*3 = 2 + 10 + 6 + 48 + 120 = 186 bytes
    record_size = 2 + 10 + 2 + 2 + 2 + (TEAMMATE_COUNT * 2) * 4 + (BATTLE_ENEMY_COUNT * 2) * 3

    infos = []
    offset = 0
    while offset + record_size <= len(data):
        info = BattleInfo(data[offset:offset+record_size])
        infos.append(info)
        offset += record_size

    return infos


def load_grp_idx(idx_path: str, grp_path: str) -> Tuple[List[bytes], List[int], List[int]]:
    """Load GRP/IDX file pair"""
    # Read IDX file (array of int32 offsets)
    with open(idx_path, 'rb') as f:
        idx_data = f.read()

    num_records = len(idx_data) // 4
    offsets = [0]
    for i in range(num_records):
        offset = struct.unpack('<I', idx_data[i*4:(i+1)*4])[0]
        offsets.append(offset)

    lengths = [offsets[i+1] - offsets[i] for i in range(len(offsets) - 1)]

    # Read GRP file
    with open(grp_path, 'rb') as f:
        grp_data = f.read()

    # Extract each record
    records = []
    for i in range(len(lengths)):
        start = offsets[i]
        end = offsets[i+1]
        records.append(grp_data[start:end])

    return records, offsets, lengths


def is_water(earth_value: int) -> bool:
    """Check if a tile is water based on earth layer value"""
    num = earth_value // 2
    return num in WATER_TILES


def is_walkable(field: BattleFieldData, x: int, y: int, occupied: Set[Tuple[int, int]]) -> bool:
    """Check if a coordinate is walkable"""
    # Out of bounds
    if x < 0 or x >= BATTLEMAP_COORD_COUNT or y < 0 or y >= BATTLEMAP_COORD_COUNT:
        return False

    # Already occupied
    if (x, y) in occupied:
        return False

    # Building
    if field.get_building(x, y) > 0:
        return False

    # Water
    if is_water(field.get_earth(x, y)):
        return False

    return True


def find_connected_walkable_positions(field: BattleFieldData, start_positions: List[Tuple[int, int]],
                                     occupied: Set[Tuple[int, int]], max_distance: int = 5) -> List[Tuple[int, int]]:
    """
    Find walkable positions connected to start positions within max_distance.
    Uses BFS to find positions reachable from any start position.
    Returns positions with distance information for smarter selection.
    """
    walkable = []
    visited = set()
    queue = deque()

    # Initialize queue with start positions and their distances
    for pos in start_positions:
        queue.append((pos[0], pos[1], 0))
        visited.add(pos)

    while queue:
        x, y, dist = queue.popleft()

        # Add this position if it's walkable, not occupied, and within distance
        if dist > 0 and (x, y) not in occupied:
            if 0 <= x < BATTLEMAP_COORD_COUNT and 0 <= y < BATTLEMAP_COORD_COUNT:
                if not field.get_building(x, y) > 0 and not is_water(field.get_earth(x, y)):
                    walkable.append((x, y, dist))  # Include distance for scoring

        # Continue BFS if within max distance
        if dist < max_distance:
            for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                nx, ny = x + dx, y + dy
                if (nx, ny) not in visited:
                    # Check if next position is valid for traversal
                    if 0 <= nx < BATTLEMAP_COORD_COUNT and 0 <= ny < BATTLEMAP_COORD_COUNT:
                        if not field.get_building(nx, ny) > 0 and not is_water(field.get_earth(nx, ny)):
                            visited.add((nx, ny))
                            queue.append((nx, ny, dist + 1))

    return walkable


def select_best_teammate_positions(candidates: List[Tuple[int, int, int]],
                                   existing_positions: List[Tuple[int, int]],
                                   num_needed: int) -> List[Tuple[int, int]]:
    """
    Select the best positions for new teammates based on strategic criteria.
    Prioritizes positions that:
    1. Fill gaps between existing teammates (create uniform spacing)
    2. Are close to existing teammates (distance 1-2 preferred)
    3. Maintain formation coherence
    """
    if not candidates or not existing_positions:
        return []

    # Score each candidate position
    scored_candidates = []

    for x, y, dist in candidates:
        score = 0

        # Prefer closer positions (distance 1-2 is ideal)
        if dist == 1:
            score += 100
        elif dist == 2:
            score += 50
        else:
            score += max(0, 30 - dist * 5)

        # Check if this position fills a gap between existing teammates
        # A gap-filling position is adjacent to 2+ existing teammates
        adjacent_teammates = 0
        for ex, ey in existing_positions:
            manhattan_dist = abs(x - ex) + abs(y - ey)
            if manhattan_dist == 1:
                adjacent_teammates += 1
            elif manhattan_dist == 2:
                # Check if it's on the same line (horizontal or vertical)
                if x == ex or y == ey:
                    score += 20  # Bonus for being on same line

        # Big bonus for filling gaps (adjacent to multiple teammates)
        if adjacent_teammates >= 2:
            score += 200
        elif adjacent_teammates == 1:
            score += 80

        # Check if position maintains formation pattern
        # Detect if existing teammates form a line (horizontal, vertical, or diagonal)
        if len(existing_positions) >= 3:
            # Check for vertical line
            x_coords = [p[0] for p in existing_positions]
            y_coords = [p[1] for p in existing_positions]

            if len(set(x_coords)) <= 2:  # Mostly vertical
                # Prefer positions with same X coordinate
                if x in x_coords:
                    score += 150
                elif x in [xc - 1 for xc in x_coords] or x in [xc + 1 for xc in x_coords]:
                    score += 50

            if len(set(y_coords)) <= 2:  # Mostly horizontal
                # Prefer positions with same Y coordinate
                if y in y_coords:
                    score += 150
                elif y in [yc - 1 for yc in y_coords] or y in [yc + 1 for yc in y_coords]:
                    score += 50

        scored_candidates.append((score, x, y))

    # Sort by score (descending) and return top positions
    scored_candidates.sort(reverse=True)
    return [(x, y) for _, x, y in scored_candidates[:num_needed]]


def visualize_battle_map(info: BattleInfo, field: BattleFieldData, result: dict,
                         show_range: int = 15) -> str:
    """
    Create ASCII visualization of the battle map.
    Shows terrain, enemies, teammates, and candidate positions.
    """
    lines = []

    # Find the center of action (average of all positions)
    all_positions = (result['existing_teammate_positions'] +
                    result['valid_enemy_positions'][:10])
    if not all_positions:
        return "No positions to visualize"

    center_x = sum(p[0] for p in all_positions) // len(all_positions)
    center_y = sum(p[1] for p in all_positions) // len(all_positions)

    # Define the view window
    min_x = max(0, center_x - show_range)
    max_x = min(BATTLEMAP_COORD_COUNT, center_x + show_range)
    min_y = max(0, center_y - show_range)
    max_y = min(BATTLEMAP_COORD_COUNT, center_y + show_range)

    # Create position lookup sets
    enemy_pos = set(result['valid_enemy_positions'])
    teammate_pos = set(result['existing_teammate_positions'])
    candidate_pos = set(result['candidate_teammate_positions'][:4])

    # Build the map
    lines.append(f"\nMap visualization (center: {center_x}, {center_y}):")
    lines.append("Legend: T=Teammate, E=Enemy, +=Candidate, #=Building, ~=Water, .=Ground")
    lines.append("")

    # Top border with X coordinates
    header = "    "
    for x in range(min_x, max_x):
        if x % 5 == 0:
            header += str(x % 10)
        else:
            header += " "
    lines.append(header)
    lines.append("   +" + "-" * (max_x - min_x) + "+")

    # Map rows
    for y in range(min_y, max_y):
        row = f"{y:2d} |"
        for x in range(min_x, max_x):
            pos = (x, y)

            # Priority: Teammate > Enemy > Candidate > Terrain
            if pos in teammate_pos:
                row += "T"
            elif pos in enemy_pos:
                row += "E"
            elif pos in candidate_pos:
                row += "+"
            else:
                # Show terrain
                building = field.get_building(x, y)
                earth = field.get_earth(x, y)

                if building > 0:
                    row += "#"
                elif is_water(earth):
                    row += "~"
                else:
                    row += "."

        row += "|"
        lines.append(row)

    # Bottom border
    lines.append("   +" + "-" * (max_x - min_x) + "+")

    return "\n".join(lines)


def analyze_battle_map(info: BattleInfo, field: BattleFieldData) -> dict:
    """Analyze a single battle map for suitability"""
    result = {
        'id': info.id,
        'name': info.name,
        'battlefield_id': info.battlefield_id,
        'enemy_count': 0,
        'valid_enemy_positions': [],
        'existing_teammate_positions': [],
        'candidate_teammate_positions': [],
        'is_good': False,
        'reason': ''
    }

    # Count valid enemies
    for i in range(BATTLE_ENEMY_COUNT):
        if info.enemy[i] >= 0:
            x, y = info.enemy_x[i], info.enemy_y[i]
            if 0 <= x < BATTLEMAP_COORD_COUNT and 0 <= y < BATTLEMAP_COORD_COUNT:
                result['enemy_count'] += 1
                result['valid_enemy_positions'].append((x, y))

    # Get existing teammate positions
    occupied = set()
    for i in range(TEAMMATE_COUNT):
        x, y = info.teammate_x[i], info.teammate_y[i]
        if 0 <= x < BATTLEMAP_COORD_COUNT and 0 <= y < BATTLEMAP_COORD_COUNT:
            result['existing_teammate_positions'].append((x, y))
            occupied.add((x, y))

    # Add enemy positions to occupied set
    for pos in result['valid_enemy_positions']:
        occupied.add(pos)

    # Check if we have at least 10 enemies
    if result['enemy_count'] < 10:
        result['reason'] = f"Only {result['enemy_count']} enemies (need 10+)"
        return result

    # Find candidate positions for additional teammates
    if result['existing_teammate_positions']:
        candidates_with_dist = find_connected_walkable_positions(
            field,
            result['existing_teammate_positions'],
            occupied,
            max_distance=3
        )

        # We need 4 more positions (6 existing + 4 new = 10 total)
        needed = 10 - len(result['existing_teammate_positions'])

        if len(candidates_with_dist) >= needed:
            # Use smart selection to pick the best positions
            best_positions = select_best_teammate_positions(
                candidates_with_dist,
                result['existing_teammate_positions'],
                needed
            )
            result['candidate_teammate_positions'] = best_positions
            result['all_candidate_positions'] = [(x, y) for x, y, _ in candidates_with_dist]
            result['is_good'] = True
            result['reason'] = f"[OK] {result['enemy_count']} enemies, {len(candidates_with_dist)} candidate positions (need {needed})"
        else:
            result['candidate_teammate_positions'] = [(x, y) for x, y, _ in candidates_with_dist]
            result['all_candidate_positions'] = result['candidate_teammate_positions']
            result['reason'] = f"Only {len(candidates_with_dist)} candidate positions (need {needed})"
    else:
        result['reason'] = "No existing teammate positions"

    return result


def main():
    base_path = r"D:\projects\kys-cpp\kys-cpp\work\game-dev\resource"
    war_sta = os.path.join(base_path, "war.sta")
    warfld_idx = os.path.join(base_path, "warfld.idx")
    warfld_grp = os.path.join(base_path, "warfld.grp")

    print("Loading battle info from war.sta...")
    battle_infos = load_battle_infos(war_sta)
    print(f"Loaded {len(battle_infos)} battle records")

    print("\nLoading battlefield data from warfld.idx/grp...")
    field_records, offsets, lengths = load_grp_idx(warfld_idx, warfld_grp)
    print(f"Loaded {len(field_records)} battlefield records")

    print("\n" + "="*80)
    print("ANALYZING BATTLE MAPS")
    print("="*80)

    good_maps = []
    all_results = []

    for info in battle_infos:
        if info.battlefield_id < 0 or info.battlefield_id >= len(field_records):
            continue

        # Parse battlefield data
        field_data = field_records[info.battlefield_id]
        if len(field_data) < BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT * 2 * BATTLEMAP_SAVE_LAYER_COUNT:
            continue

        field = BattleFieldData(field_data)
        result = analyze_battle_map(info, field)
        all_results.append(result)

        if result['is_good']:
            good_maps.append(result)
            print(f"\n[GOOD] Battle {result['id']:3d}: {result['name']:10s} (Field {result['battlefield_id']:3d})")
            print(f"  {result['reason']}")
            print(f"  Existing teammate positions: {result['existing_teammate_positions'][:6]}")
            print(f"  Sample candidate positions: {result['candidate_teammate_positions'][:10]}")

    # Debug: Show some maps that almost made it
    print("\n" + "="*80)
    print("DEBUG: Maps with 10+ enemies (showing first 20)")
    print("="*80)
    maps_with_enemies = [r for r in all_results if r['enemy_count'] >= 10]
    for result in maps_with_enemies[:20]:
        print(f"\nBattle {result['id']:3d}: {result['name']:10s}")
        print(f"  {result['reason']}")
        print(f"  Existing teammates: {len(result['existing_teammate_positions'])}")
        print(f"  Candidate positions: {len(result['candidate_teammate_positions'])}")

    print("\n" + "="*80)
    print(f"SUMMARY: Found {len(good_maps)} good battle maps")
    print("="*80)

    if good_maps:
        print("\nGood battle map IDs:", [m['id'] for m in good_maps])

        # Output ASCII visualizations for all good maps
        print("\n" + "="*80)
        print("ASCII VISUALIZATIONS FOR ALL GOOD MAPS")
        print("="*80)

        for idx, result in enumerate(good_maps):
            # Get the battlefield for visualization
            field_data = field_records[result['battlefield_id']]
            field = BattleFieldData(field_data)

            print(f"\n{'='*80}")
            print(f"Battle ID {result['id']} - Battlefield {result['battlefield_id']} - {result['enemy_count']} enemies")
            print(f"{'='*80}")

            # Show visualization
            visualization = visualize_battle_map(
                battle_infos[result['id']],
                field,
                result,
                show_range=15
            )
            print(visualization)

            print(f"\nPositions: Existing={result['existing_teammate_positions']}")
            print(f"           New={result['candidate_teammate_positions'][:4]}")


if __name__ == "__main__":
    main()
