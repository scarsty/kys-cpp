# Implementation Plan: Expanded Chess Pool — 倚天/飞狐/补完 Characters

## Overview

The implementation adds ~37 new characters, 13 new synergies, disbands 1 synergy, reworks 4 existing synergies, adds 20 new battle effect types, configurable shop size, a chess banning system, curated Easy mode pool, and expanded battle maps for clone summons. The work is organized into layers: data first (DB + config), then C++ plumbing (enums, parsing, state), then C++ battle logic, then systems (shop/ban/pool), and finally integration.

## Architecture Changes

No new modules are introduced. Changes extend existing systems:

1. **`ChessBattleEffects.h/cpp`** — Add ~20 new `EffectType` enum values, extend `RoleComboState` with new fields, add Chinese→enum mappings, extend `applyEffect()` switch.
2. **`ChessCombo.h/cpp`** — Add ~13 new `ComboId` enum values. No structural change — synergy definitions are YAML-driven.
3. **`BattleSceneHades.cpp`** — Implement runtime behavior for new effects in the combat loop (bleed ticks, blink repositioning, death AOE, clone spawning, force-pull, etc.).
4. **`ChessPool.h/cpp`** — Parameterize shop size, add ban filtering.
5. **`ChessShopFlow.cpp`** — Adapt UI for 6 slots, add seen-tracking.
6. **`GameDataStore.h/cpp`** — Add `seenRoleIds`, `bannedRoleIds` fields with serialization.
7. **`ChessBalance.h/cpp`** — Add `shopSlotCount` and `maxBanCount` config fields, parse from YAML.
8. **`DynamicChessMap.h/cpp`** — Extend `MapInfo` with 3 more clone positions (13 total), update `createBattle()`.
9. **`BattleScene.h`** — No change to `ExtendedTeammateInfo` struct (already a vector, no fixed-size limit).
10. **Config files** — `chess_pool.yaml`, `chess_combos.yaml`, `chess_balance_*.yaml`, new `chess_pool_easy.yaml`.
11. **Database** — Python script `tools/add_chess_characters.py` to insert ~37 characters and their skills into `0.db`.
12. **Ban UI** — New flow/panel in chess menu for banning, similar to existing menu patterns.

## Implementation Steps

### Phase A: Data Foundation (DB + Resources)

**Files to create**:
- `tools/add_chess_characters.py` — Python script to add ~37 characters to `0.db`

**Technical approach**:
Script reads `D:\games\人在江湖cpp版\人在江湖cpp版\game\1_renwu.csv` to find role IDs, fight frame IDs, head IDs. For each new character: INSERT OR REPLACE into `role` table with tier-appropriate stats, and INSERT matching `magic` entries for skills. Script also handles resource copying (fight zips + head images).

**Edge Cases**:
- Characters not in reference CSV (马钰, 谭处端, 刘处玄) need fallback fight frames from similar martial art styles.
- Script must be idempotent — safe to re-run.

**Dependencies**: None

---

### Phase B: Config — Pool, Combos, Balance

**Files to modify**:
- `config/chess_pool.yaml` — Add ~37 new role IDs to tiers
- `config/chess_combos.yaml` — Add 13 new synergies, remove 三大吸功, rework 4 synergies (剑客/拳师/红颜/全真教), redistribute members (独行+无崖子, 阴险+丁春秋, 拳师+虚竹, 红颜+韩小莺)
- `config/chess_balance_normal.yaml` — Add `商店数量: 6`, `最大禁棋数: 15`
- `config/chess_balance_easy.yaml` — Add `商店数量: 5`, `最大禁棋数: 0`

**Files to create**:
- `config/chess_pool_easy.yaml` — Curated ~50-55 character subset

**Technical approach**:
Manual YAML edits following existing format. New combo entries use new effect type names (Chinese keys) that will be registered in Phase C. The combos YAML will reference new effect types before the C++ parsing code is updated — this is fine because parsing failures are logged but don't crash.

**Edge Cases**:
- Synergy member counts must match spec exactly (e.g., 龙套 has 6 members, not accidentally 7).
- Easy mode pool must be verified for synergy completeness (all-or-nothing per synergy).

**Dependencies**: Phase A (need role IDs in DB)

---

### Phase C: C++ Effect Type Registration

**Files to modify**:
- `src/ChessBattleEffects.h` — Add ~20 new `EffectType` enum values, new `RoleComboState` fields
- `src/ChessBattleEffects.cpp` — Add Chinese→enum entries in `effectTypeMap`, add entries in `applyEffect()` switch, add new trigger types if needed
- `src/ChessCombo.h` — Add ~13 new `ComboId` enum values

**Technical approach**:
Extend the existing enum pattern. New effect types that are purely "marker" effects (like `BlinkAttack`, `DeathPrevention`) just store values in `RoleComboState` and are consumed by `BattleSceneHades`. More complex effects like `CloneSummon` and `EnemyTopDebuff` are pre-battle effects that need special handling in the battle setup code.

New `RoleComboState` fields needed:
- `int bleedChancePct = 0;` + `bool bleedPersist = false;` + `int bleedMaxStacks = 5;`
- `bool postSkillDash = false;`
- `bool blinkAttack = false;`
- `int allyDeathStatBoost = 0;`
- `int cloneSummonCount = 0;` + `int cloneSummonComboId = -1;`
- `int projectileReflectPct = 0;`
- `bool ignoreDefense = false;`
- `int onSkillTeamHeal = 0;`
- `bool deathPrevention = false;` + `bool deathPreventionUsed = false;`
- `bool forcePullProtect = false;` + `bool forcePullExecute = false;` + `bool forcePullProtectUsed = false;` + `bool forcePullExecuteUsed = false;`
- `int executePct = 0;` + `int executeThresholdPct = 0;`
- `int mpBlockFrames = 0;`
- `int charmCDRChancePct = 0;` + `int charmCDRAmountPct = 0;`
- `bool offensiveCharm = false;`
- `int deathAOEPct = 0;` + `int deathAOEStunFrames = 0;`
- `int shieldExplosionMult = 0;` (percentage multiplier, e.g. 20 = 0.2×)
- `int shieldOnAllyDeathCount = 0;` + `int shieldOnAllyDeathTracker = 0;`
- `int enemyTopDebuffCount = 0;` + `int enemyTopDebuffValue = 0;`
- Runtime bleed state on target: `int bleedStacks = 0;` + `int bleedTimer = 0;` + `bool bleedPersistFlag = false;`
- Runtime MP block: `int mpBlockTimer = 0;`

**Edge Cases**:
- `ArmorPen` and `Stun` already use the triggered-effect pattern; new effects like `Execute` and `MPBlock` should follow the same convention (route through `triggeredEffects` vector when they have on-hit triggers).
- `DeathPrevention` and `ForcePull` must track per-character "used" flags that reset each battle.

**Dependencies**: None (can run in parallel with Phase A/B, but compile-test needs Phase B combos to exercise)

---

### Phase D: Battle Logic — Simple Effects

**Files to modify**:
- `src/BattleSceneHades.cpp` — Add runtime logic for simpler effects

Effects to implement in this phase:
1. **Bleed** (刀客): In the damage-dealt handler, check `bleedChancePct`, apply bleed stack to target. In tick loop, process bleed ticks (1% maxHP × stacks every 10 frames), clear or persist based on `bleedPersistFlag`.
2. **Execute** (剑客 9-piece): In damage-dealt handler, check if target HP < threshold%, roll execute chance, instant kill.
3. **Stun** (拳师): Already partially exists. Ensure stun chance from new thresholds works.
4. **MPBlock** (拳师 9-piece): On hit, apply MP block timer to target. In MP regen tick, skip regen if `mpBlockTimer > 0`.
5. **IgnoreDefense** (倚天屠龙): In damage calculation, if attacker has `ignoreDefense`, skip defense/DR in formula.
6. **DeathPrevention** (先天神照): In death handler, if `deathPrevention` and not `deathPreventionUsed`, set HP=1, grant invincibility frames, set used flag.
7. **CharmCDRDebuff** (红颜 倾国倾城): When attacked, roll chance, if triggered, multiply attacker's remaining cooldown by (1 + amount/100).
8. **OffensiveCharm** (红颜 9-piece): When attacking, 10% chance to apply charm CDR to the target.
9. **AllyDeathStatBoost** (四大法王): In ally death handler, for each surviving combo member, increase ATK/DEF/SPD by boost value.
10. **DeathAOE** (龙套): In death handler, deal % maxHP damage to enemies in 3×3 area around dying character. At 6-piece, apply stun.
11. **OnSkillTeamHeal** (妙手仁心): After skill use, heal all allies by flat HP amount.
12. **ProjectileReflect** (逍遥二仙): For ranged attacks, roll reflect chance. If triggered, damage is applied to attacker instead.
13. **EnemyTopDebuff** (阴险): Pre-battle setup, find N highest-star enemies, reduce their ATK/DEF/SPD.
14. **ShieldExplosion** (全真教): When shield breaks (HP goes to shield=0), deal AoE damage = `shieldValue × shieldExplosionMult / 100`.
15. **ShieldOnAllyDeath** (全真教 7-piece): Track combo ally deaths. Every N deaths, re-apply shield to surviving members.

**Technical approach**:
Follow existing patterns in `BattleSceneHades.cpp`:
- Damage calculation section → add IgnoreDefense check
- Post-hit section → add Bleed, Execute, MPBlock, CharmCDR
- Tick/frame section → add Bleed ticking, MPBlock countdown
- Death handler → add DeathPrevention, DeathAOE, AllyDeathStatBoost, ShieldOnAllyDeath
- Pre-battle setup → add EnemyTopDebuff
- Post-skill section → add OnSkillTeamHeal

**Edge Cases**:
- Execute should not trigger on bosses (if applicable) — for now, no boss immunity.
- DeathAOE should not trigger if character was already dead (no double-trigger).
- ShieldExplosion needs to store the shield value before it's zeroed.
- Bleed and poison should be distinguishable (separate tracking).

**Dependencies**: Phase C

---

### Phase E: Battle Logic — Complex Effects

**Files to modify**:
- `src/BattleSceneHades.cpp` — Add complex runtime logic
- `src/DynamicChessMap.h` — Extend `MapInfo` with clone positions
- `src/DynamicChessMap.cpp` — Add 3 more positions per map, update `createBattle()`
- `tools/analyze_battle_maps.py` — Update to find 3 additional walkable positions

Effects to implement:
1. **PostSkillDash** (踏雪无痕): After ultimate, apply speed burst (temporary speed multiplier for N frames) moving away from nearest enemy. Not a teleport — uses existing movement system with acceleration.
2. **BlinkAttack** (宗师): On every auto-attack, before attack animation, teleport to random walkable position within attack range of target. Play hardcoded SFX. Stay at new position.
3. **ForcePull** (乾坤挪移): Two variants:
   - Protective: Hook into ally-damaged handler. When ally drops below 25% HP, pull attacker to beside nearest 乾坤挪移 member. Once per member per battle.
   - Execute: Hook into enemy-damaged handler. When enemy drops below 15% HP and has no melee attacker targeting them, pull enemy to beside nearest 乾坤挪移 member. Once per member per battle.
4. **CloneSummon** (真武七截阵): At battle start (in combo setup), identify highest-star 真武七截 member. Create N clone Role copies with full stats. Add them as additional teammates at clone positions. Clones are NOT counted in synergy detection (filter by `chessInstanceId == -1` or a clone flag).

**Technical approach**:
- BlinkAttack: In the auto-attack targeting code, before the attack plays, check if attacker has `blinkAttack`. If so, find valid walkable cells within attack range of target, pick random one, teleport attacker there. Use `Audio::getInstance()->playASound()` for SFX.
- PostSkillDash: After skill animation completes (existing `postSkillInvincFrames` hook point), if `postSkillDash`, calculate direction away from nearest enemy, set a temporary speed boost and movement target.
- ForcePull: New handler functions called from damage processing. Find 乾坤挪移 members via combo state. Teleport enemy to adjacent walkable cell of the member.
- CloneSummon: Before `buildComboStates`, after `detectCombos`, if any active combo is 真武七截 with clone threshold met: find highest-star member, create N Role copies, add to `friends_obj_` with clone positions from `DynamicChessMap`. Set `chessInstanceId = -1` for clones to exclude from synergy recount.

**Map expansion**:
- Add `std::vector<std::pair<int, int>> clone_positions;` to `MapInfo` (3 per map).
- In `analyze_battle_maps.py`, extend to find 3 more walkable ally positions (preferring positions near existing ally cluster).
- Update `createBattle()` to pass clone positions when clones are requested.

**Edge Cases**:
- Clone positions may overlap with teammate positions on some maps — need validation.
- BlinkAttack must handle edge case where no valid positions exist (skip blink).
- ForcePull must handle edge case where no adjacent walkable cells exist near member (skip pull).
- Clones dying should not trigger 四大法王 or other death-based synergies (they're not real combo members).

**Dependencies**: Phase C, Phase D (for death handlers)

---

### Phase F: Shop System — Configurable Size + Ban System

**Files to modify**:
- `src/ChessBalance.h` — Add `int shopSlotCount = 5;` and `int maxBanCount = 15;` to `BalanceConfig`
- `src/ChessBalance.cpp` — Parse `商店数量` and `最大禁棋数` from YAML
- `src/ChessPool.h` — Add `setBannedRoleIds()` method
- `src/ChessPool.cpp` — Change hardcoded `5` loop to `shopSlotCount`, skip banned IDs in `selectFromPool()`
- `src/ChessShopFlow.cpp` — Adapt UI layout for variable slot count, track `seenRoleIds`
- `src/GameDataStore.h` — Add `std::set<int> seenRoleIds;` and `std::set<int> bannedRoleIds;`
- `src/GameDataStore.cpp` — Serialize/deserialize new fields (glaze JSON)

**Technical approach**:
- `ChessBalance`: Simple new fields, parsed from YAML like others.
- `ChessPool::selectFromPool()`: After checking `rejected_`, also check `banned_.contains(idx)`. New `banned_` member populated from `GameDataStore::bannedRoleIds`.
- `ChessPool::getChessFromPool()`: Replace `for (int i = 0; i < 5;` with `ChessBalance::config().shopSlotCount`.
- `ChessShopFlow::getChess()`: After rolling shop, add all role IDs to `GameDataStore::seenRoleIds`. The `menuConfig.perPage` already dynamically sizes from `menuData.labels.size()`.
- Ban UI: New function in shop flow or separate flow. Uses existing `makeIndexedMenu` pattern. Lists all `seenRoleIds`, shows ban status, allows toggling up to `maxBanCount`.

**Edge Cases**:
- If all candidates in a tier are banned, `selectFromPool()` could infinite-loop. Add a maximum retry count (e.g. 100) and fall through to a different tier.
- `GameDataStore` backward compatibility: new `set<int>` fields default to empty, so old saves load fine.
- Easy mode: `shopSlotCount = 5`, `maxBanCount = 0` means ban UI is hidden.

**Dependencies**: Phase B (config), Phase C (compiles)

---

### Phase G: Easy Mode Pool

**Files to modify**:
- `src/ChessBalance.cpp` or `src/ChessPool.cpp` — Load `chess_pool_easy.yaml` when difficulty == Easy

**Files to create**:
- `config/chess_pool_easy.yaml` — Curated subset

**Technical approach**:
The `chessPool` array in `BalanceConfig` is already loaded per-difficulty (Easy loads `chess_balance_easy.yaml`). The pool role IDs come from `chess_pool.yaml` loaded separately. Need to split: Easy loads `chess_pool_easy.yaml`, Normal loads `chess_pool.yaml`. This may already work if pool loading is done in balance config loading (check `ChessBalance::loadConfig()`).

**Edge Cases**:
- Must verify every synergy in `chess_combos.yaml` either has 0 members in easy pool or enough for max threshold.

**Dependencies**: Phase B (config files exist), Phase F (difficulty-aware loading)

---

### Phase H: Integration Testing & Validation

**Files to create/modify**:
- `tools/verify_chess_pool.py` — Extend to validate new pool
- Manual testing checklist

**Technical approach**:
- Verify all 88 characters have valid fight frames and head resources.
- Verify all synergies compute correctly with new members.
- Verify clone spawning, blink attack, death AOE in battle.
- Verify shop with 6 slots displays correctly.
- Verify banning persists across shops within a run.
- Verify Easy mode pool synergy completeness.
- Run existing `verify_chess_pool.py` against new `chess_pool.yaml`.

**Dependencies**: All previous phases
