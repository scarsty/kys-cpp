# Task 5: Battle Logic — Complex Effects (Blink, Dash, ForcePull, Clones)

**Depends on**: Task 3 (effect types), Task 4 (some shared death-handler infrastructure)
**Estimated complexity**: High
**Type**: Feature

## Objective

Implement the 4 most complex battle effects that involve spatial mechanics (movement, teleportation, spawning): PostSkillDash, BlinkAttack, ForcePull, and CloneSummon.

## ⚠️ Important information

Before coding, Read FIRST -> Load [03-tasks-00-READBEFORE.md](03-tasks-00-READBEFORE.md)

Then read the full specification: [01-specification.md](01-specification.md) — specifically 踏雪无痕, 宗师, 乾坤挪移, and 真武七截阵 synergy descriptions.

Before implementing, thoroughly read:
- `src/BattleSceneHades.cpp` — movement system, position coordinates, auto-attack targeting
- `src/BattleScene.h` — `ExtendedTeammateInfo`, coordinate system
- `src/DynamicChessMap.h/cpp` — `MapInfo`, `createBattle()`, position arrays
- `tools/analyze_battle_maps.py` — how positions are analyzed

## Files to Modify
- `src/BattleSceneHades.cpp`
- `src/DynamicChessMap.h`
- `src/DynamicChessMap.cpp`

## Files to Optionally Modify
- `tools/analyze_battle_maps.py` — add 3 more candidate positions per map
- `src/Audio.h` / `src/Audio.cpp` — for blink SFX (if a new helper is needed)

## Detailed Steps

### 1. BlinkAttack (宗师) — Teleport on every auto-attack

**Where to hook**: In the auto-attack targeting/execution code in `BattleSceneHades.cpp`. Find where a melee/ranged attack begins — before the attack animation plays.

**Logic**:
1. Check if attacker's combo state has `blinkAttack == true`.
2. If true:
   a. Get the target's position (x, y).
   b. Find all walkable cells within the attacker's attack range of the target.
   c. If any valid cells exist, pick one at random.
   d. Teleport the attacker to that cell (update position immediately).
   e. Play a hardcoded SFX. Use the existing audio system: search for `playASound` or `playESound` in the codebase for the pattern. Pick any existing combat sound ID as a placeholder.
3. If no valid cells exist (edge case), skip the blink and attack normally.

**Key questions to resolve by reading code**:
- How are positions represented? (likely x, y integer coordinates from battle map)
- How is "attack range" determined? (search for range/distance calculations in attack code)
- How to check if a cell is walkable? (search for walkable/passable checks)

### 2. PostSkillDash (踏雪无痕) — Speed burst away after ultimate

**Where to hook**: After skill/ultimate execution completes. There's already a `postSkillInvincFrames` hook — the dash should trigger at the same point.

**Logic**:
1. After skill animation finishes, check if caster has `postSkillDash == true`.
2. If true:
   a. Find the nearest enemy position.
   b. Calculate the direction AWAY from that enemy.
   c. Apply a temporary speed boost (e.g., 3× speed for 30 frames) and set the movement target to a position in the away-direction.
   d. The role physically moves during those frames (using existing movement system), not a teleport.
3. This should use the existing movement/pathfinding system if available, or simply set a high speed multiplier temporarily. 

**Implementation option**: If the movement system doesn't support temporary speed boosts easily, an alternative is to set a velocity vector for N frames. Search for how the movement system works (likely per-frame position updates based on speed). Add a `dashTimer` and `dashDirection` to the runtime state.

### 3. ForcePull — Protective and Execution pulls (乾坤挪移)

**Where to hook**: 
- Protective: In the damage-received handler, after HP is reduced, check if any ally dropped below 25% HP.
- Execution: In the damage-dealt handler, after HP is reduced, check if any enemy dropped below 15% HP.

**Logic for Protective Pull**:
1. When any ally's HP drops below 25% of maxHP:
   a. Find all roles with `forcePullProtect == true && forcePullProtectUsed == false`.
   b. Pick the nearest one (or first available).
   c. Identify the enemy that just attacked the low-HP ally.
   d. Find a walkable cell adjacent to the 乾坤挪移 member.
   e. Teleport the attacking enemy to that cell.
   f. Set `forcePullProtectUsed = true` for that member.
2. This fires at most once per 乾坤挪移 member per battle.

**Logic for Execution Pull**:
1. When any enemy's HP drops below 15% of maxHP:
   a. Check if no ally is currently in melee range of that enemy (the enemy is "unattended").
   b. Find a 乾坤挪移 member with `forcePullExecute == true && forcePullExecuteUsed == false`.
   c. Find a walkable cell adjacent to that member.
   d. Teleport the low-HP enemy there.
   e. Set `forcePullExecuteUsed = true`.

**Key questions to resolve**:
- How to find walkable adjacent cells (search for neighbor/adjacent cell logic).
- How to determine "currently attacking/targeted by" (search for target assignment logic).

### 4. CloneSummon (真武七截阵) — Spawn clones at battle start

**Where to hook**: In the pre-battle setup in `BattleSceneHades.cpp`, AFTER `detectCombos` but BEFORE `buildComboStates`.

**Logic**:
1. Check if any active ally combo is 真武七截阵 with an active threshold.
2. If active, determine N (clone count from threshold: 1/2/3).
3. Find the highest-star 真武七截阵 member among ally chess.
4. Create N clone Role entries:
   a. Copy the original role's stats (HP, ATK, DEF, SPD, skills, etc.).
   b. Assign new temporary battle IDs (ensure no collision).
   c. Set `chessInstanceId = -1` to mark as clone (excluded from synergy counting).
5. Add clone roles to `friends_obj_` deque.
6. Assign positions from clone position slots in the selected map.
7. Re-run synergy detection? NO — clones should NOT affect synergy counts. They should be added AFTER `detectCombos`.

**DynamicChessMap expansion**:
1. In `DynamicChessMap.h`:
   - Add `std::vector<std::pair<int, int>> clone_positions;` to `MapInfo` (3 positions per map).
2. In `DynamicChessMap.cpp`:
   - For each of the 10 maps in `getTopMaps()`, add 3 more walkable positions (near ally cluster).
   - These can be identified by studying the ASCII maps already in the code.
3. In `createBattle()`:
   - After building the `teammates` vector, also store clone positions in the battle scene (new method or field).
   - Expose clone positions so the pre-battle setup can use them for clone placement.

**Edge cases**:
- Clone IDs must not collide with real role IDs. Use negative IDs or IDs > 1000.
- Clones should NOT be in `extended_teammates_` since they're not real chess pieces — they're added to `friends_obj_` directly.
- When clones die, they should NOT trigger synergy-related death effects (四大法王, 全真教 shield-on-death, etc.) since they're not combo members.
- Clone stats should reflect the star+equipment bonuses of the original at time of battle, not base stats.

### 5. Verify compilation and test
Build and run test battles with each effect active.

### 6. Commit: `feat: implement complex spatial effects (blink, dash, force-pull, clones)`

## Acceptance Criteria
- [ ] BlinkAttack teleports attacker to random position near target on every auto-attack
- [ ] BlinkAttack plays SFX on each blink
- [ ] PostSkillDash moves caster away from enemies after ultimate (acceleration, not teleport)
- [ ] ForcePullProtect pulls attacker when ally drops below 25% HP (once per member per battle)
- [ ] ForcePullExecute pulls low-HP unattended enemy (once per member per battle)
- [ ] CloneSummon spawns correct number of clones at battle start
- [ ] Clones have full stats of original
- [ ] Clones don't count for synergy activation
- [ ] DynamicChessMap supports 13 total ally positions (6+4+3)
- [ ] Project compiles without errors

## Notes
- Finding walkable cells: look for the battle map's terrain data. The BattleScene likely has a method to check if a cell is passable.
- The audio system is in `src/Audio.h/cpp`. A simple `Audio::getInstance()->playESound(sound_id)` or similar should work for the blink SFX.
- Clone positions are per-map static data — they can be determined by examining the ASCII maps in `DynamicChessMap.cpp` and finding 3 more walkable spots near the ally cluster.
- PostSkillDash is the hardest to get "right" visually — start with a simple implementation (temporary speed boost + move-away target) and iterate.
