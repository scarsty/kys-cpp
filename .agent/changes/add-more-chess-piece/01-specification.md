# Specification: Expanded Chess Pool — 倚天/飞狐/补完 Characters

## Overview

This change expands the auto-chess pool from ~51 characters to ~88 characters, primarily adding characters from 倚天屠龙记, 飞狐外传, and 笑傲江湖. It introduces 13 new synergies, disbands 1 existing synergy (三大吸功), reworks 4 existing synergies, and adds complex battle mechanics (bleed, clones, blink attacks, death AOE, banning, etc.). The pool expansion requires rebalancing: a configurable shop size (5→6), a chess banning system, and a curated Easy mode pool.

## Character Additions

### From the Design Doc (28 characters)

| # | Name | Tier | Type | Synergies | Notes |
|---|------|------|------|-----------|-------|
| 1 | 胡一刀 | 4 | Melee | 刀客, 踏雪无痕 | |
| 2 | 田归农 | 1 | Ranged | 剑客, 阴险 | |
| 3 | 阎基 | 1 | Ranged | 刀客, 阴险 | |
| 4 | 王元霸 | 1 | Melee | 刀客, 龙套 | |
| 5 | 田伯光 | 2 | Melee | 刀客, 独行 | |
| 6 | 东方不败 | 5 | Ranged | 宗师, 独行, 剑客 | 3 synergies for 宗师 bonus |
| 7 | 黄裳 | 5 | Melee | 宗师, 独行, 拳师 | 3 synergies for 宗师 bonus |
| 8 | 张三丰 | 5 | Melee | 真武七截, 九阳传人, 宗师 | Already 3 synergies |
| 9 | 张无忌 | 4 | Melee | 九阳传人, 乾坤挪移 | |
| 10 | 韦一笑 | 3 | Melee | 四大法王, 踏雪无痕 | |
| 11 | 谢逊 | 4 | Melee | 四大法王, 倚天屠龙 | |
| 12 | 周芷若 | 3 | Melee | 红颜, 拳师 | |
| 13 | 杨逍 | 3 | Ranged | 逍遥二仙, 乾坤挪移 | |
| 14 | 殷天正 | 4 | Melee | 四大法王, 拳师 | |
| 15 | 灭绝师太 | 3 | Ranged | 剑客, 倚天屠龙 | Already exists as ID 20 in 人在江湖 |
| 16 | 赵敏 | 1 | Ranged | 倚天屠龙, 红颜 | |
| 17 | 宋青书 | 2 | Melee | 真武七截, 阴险 | Part of 真武七截 (9 members total) |
| 18 | 张翠山 | 2 | Ranged | 真武七截, 奇门 | |
| 19 | 黛绮丝 | 2 | Ranged | 四大法王, 红颜 | |
| 20 | 范瑶 | 3 | Ranged | 逍遥二仙, 阴险 | |
| 21 | 宋远桥 | 4 | Melee | 真武七截, 拳师 | |
| 22 | 张松溪 | 3 | Melee | 真武七截, 剑客 | |
| 23 | 俞莲舟 | 3 | Ranged | 真武七截, 剑客 | Already exists as ID 16 in 人在江湖 |
| 24 | 俞岱岩 | 1 | Melee | 真武七截, 龙套 | |
| 25 | 殷梨亭 | 2 | Melee | 真武七截, 剑客 | |
| 26 | 莫声谷 | 2 | Melee | 真武七截, 龙套 | |
| 27 | 无色禅师 | 4 | Melee | 少林寺, 拳师, 九阳传人 | |
| 28 | 郭襄 | 4 | Ranged | 剑客, 九阳传人, 倚天屠龙 | |

### Additionally Required Characters (~9 more)

| Name | Tier | Type | Synergies | Reason |
|------|------|------|-----------|--------|
| 胡斐 | 3 | Melee | 刀客, 踏雪无痕 | Omitted from table but referenced in synergies |
| 程灵素 | 1 | Ranged | 妙手仁心, 红颜 | 妙手仁心 synergy member |
| 平一指 | 2 | Melee | 妙手仁心, 独行 | 妙手仁心 synergy member |
| 胡青牛 | 2 | Ranged | 妙手仁心, 独行 | 妙手仁心 synergy member |
| 成昆 | 3 | Melee | 阴险, 拳师 | 阴险 synergy member; tough enemy |
| 岳不群 | 3 | Melee | 阴险, 剑客 | 阴险 synergy member |
| 马钰 | 1 | Melee | 全真教, 龙套 | 全真教 expansion (7 members needed) |
| 王处一 | 2 | Melee | 全真教, 拳师 | 全真教 expansion |
| 谭处端 | 1 | Ranged | 全真教, 龙套 | 全真教 expansion |
| 刘处玄 | 2 | Ranged | 全真教, 奇门 | 全真教 expansion |

**Total new characters: ~37**
**New pool total: ~88 characters**

### Tier Distribution (New Pool)

| Tier | Current | Added | New Total |
|------|---------|-------|-----------|
| 1费 | 15 | +8 (田归农, 阎基, 王元霸, 赵敏, 俞岱岩, 程灵素, 马钰, 谭处端) | 23 |
| 2费 | 10 | +8 (田伯光, 宋青书, 张翠山, 黛绮丝, 殷梨亭, 莫声谷, 平一指/胡青牛, 王处一/刘处玄) | 18 |
| 3费 | 12 | +10 (韦一笑, 周芷若, 杨逍, 灭绝师太, 范瑶, 张松溪, 俞莲舟, 胡斐, 成昆, 岳不群) | 22 |
| 4费 | 11 | +8 (胡一刀, 张无忌, 谢逊, 殷天正, 宋远桥, 无色禅师, 郭襄, + move some) | 19 |
| 5费 | 3 | +3 (张三丰, 东方不败, 黄裳) | 6 |
| **Total** | **51** | **~37** | **~88** |

## New Synergies (13 new)

### 1. 刀客 — Bleed specialists
- **Members**: 血刀老祖, 田伯光, 王元霸, 胡一刀, 胡斐, 阎基 (6 members)
- **Thresholds**: 3 / 6
  - 3: Speed +20, Bleed chance 15% on attack (1% max HP/tick × stacks, max 5 stacks, 10-frame interval, clears after each tick)
  - 6: Speed +40, Bleed chance 30%, stacks persist (don't clear), max 10 stacks
- **New effect types needed**: `Bleed` (DOT with stacking, on-hit probability)
- **Note**: Doc says 3/6/9 but only 6 members exist. Keep at 3/6 unless more刀客 characters are added. The 6-piece threshold absorbs the 9-piece bonuses (no-clear + 10-stack cap).

### 2. 踏雪无痕 — Post-ult dash + MP recovery
- **Members**: 胡斐, 胡一刀, 韦一笑, 段誉 (4 members)
- **Thresholds**: 2 / 4
  - 2: After using ultimate, dash away from enemies at high speed (acceleration burst, not teleport), MP recovery +15/frame
  - 4: Same but MP recovery +30/frame
- **New effect types needed**: `PostSkillDash` (speed acceleration away from enemies after ult — applies movement acceleration, not instant teleport), extends existing `MPRecoveryBonus`

### 3. 阴险 — Enemy debuff aura
- **Members**: 田归农, 阎基, 成昆, 岳不群, 宋青书, 范瑶, 丁春秋 (7 members)
- **Thresholds**: 2 / 4 / 6
  - 2: Reduce ATK/DEF/SPD of enemy's highest-star 1 chess piece by 50 each
  - 4: Reduce 2 enemy pieces by 100 each
  - 6: Reduce 3 enemy pieces by 150 each
- **New effect types needed**: `EnemyTopDebuff` (targets N highest-level enemies, reduces 3 stats)

### 4. 宗师 — Blink attack
- **Members**: 张三丰, 扫地僧, 东方不败, 黄裳 (4 members, all 5-cost)
- **All members receive 3 synergies** (宗师 is the special 3rd one)
- **Threshold**: 4
  - On every auto-attack, teleport to a random enemy within attack range before attacking. Stay at new position. If targeting same enemy, still repositions randomly within range.
- **New effect types needed**: `BlinkAttack` (teleport-to-target on auto-attack)
- **SFX**: Hardcode a sound effect on each blink (placeholder, to be swapped later)

### 5. 四大法王 — Rage on ally death
- **Members**: 谢逊, 殷天正, 韦一笑, 黛绮丝 (4 members, exact)
- **Threshold**: 4
  - Each time a 四大法王 member dies, surviving members gain +50 to ATK/DEF/SPD
- **New effect types needed**: `AllyDeathStatBoost` (flat stat gain per ally death within combo)

### 6. 真武七截阵 — Clone summoning
- **Members**: 张三丰, 宋远桥, 张松溪, 俞莲舟, 俞岱岩, 殷梨亭, 莫声谷, 张翠山, 宋青书 (9 members)
- **Thresholds**: 3 / 5 / 7
  - 3: At battle start, spawn 1 clone of the highest-star 真武七截 member
  - 5: Spawn 2 clones
  - 7: Spawn 3 clones
  - Clones have **full stats** of the original
  - Clones do NOT count for synergy activation (duplicates never count)
  - Clones persist until killed
  - Spawned at battle start at pre-defined extra positions
- **Battle map constraint**: Current maps support up to 10 ally positions (6 existing + 4 extended). Clones need additional positions (up to 3 more, for 13 total). Requires expanding `DynamicChessMap` with additional walkable positions and updating `ExtendedTeammateInfo` capacity.
- **New effect types needed**: `CloneSummon` (battle-start clone spawning system)

### 7. 逍遥二仙 — Projectile reflection
- **Members**: 杨逍, 范瑶 (2 members)
- **Threshold**: 2 — 15% probability to reflect projectiles
- **New effect types needed**: `ProjectileReflect` (chance-based projectile reflection)

### 8. 九阳传人 — MP recovery + defense
- **Members**: 张无忌, 张三丰, 郭襄, 无色禅师 (4 members)
- **Thresholds**: 2 / 4
  - 2: MP recovery +10/frame, Defense +100
  - 4: MP recovery +20/frame, Defense +200

### 9. 倚天屠龙 — True damage
- **Members**: 赵敏, 灭绝师太, 郭襄, 谢逊 (4 members)
- **Threshold**: 4 — All damage ignores defense and damage reduction
- **New effect types needed**: `IgnoreDefense` (bypass defense + damage reduction in damage calc)

### 10. 妙手仁心 — Team heal on ult
- **Members**: 程灵素, 平一指, 胡青牛, 薛慕华 (4 members)
- **Thresholds**: 2 / 4
  - 2: When member uses ultimate, heal all allies for 25 HP
  - 4: Heal all allies for 50 HP
- **New effect types needed**: `OnSkillTeamHeal` (triggered team-wide heal when using ultimate)

### 11. 先天神照 — Death prevention
- **Members**: 王重阳, 狄云 (2 members)
- **Threshold**: 2 — First time HP drops to 0, lock at 1 HP and grant 100 frames of invincibility. Once per battle per character.
- **New effect types needed**: `DeathPrevention` (lock HP at 1, grant invincibility, once per battle)
- Note: 狄云 gets 3 synergies (拳师, 连城诀, 先天神照). 王重阳 gets 3 (全真教, 独行, 先天神照).

### 12. 乾坤挪移 — Force-pull mechanics
- **Members**: 张无忌, 杨逍 (2 members)
- **Threshold**: 2 — Two effects:
  1. **Protective Pull**: When any ally drops below 25% HP, pull the enemy attacking that ally to beside a 乾坤挪移 member. Once per battle per member.
  2. **Execution Pull**: When any enemy drops below 15% HP and has no ally currently attacking them in range, pull that enemy to beside a 乾坤挪移 member. Once per battle per member.
- **New effect types needed**: `ForcePull` (teleport enemy to beside synergy member, two trigger conditions)

### 13. 龙套 — Death AOE
- **Members**: 王元霸, 俞岱岩, 莫声谷, 马钰, 谭处端, 游坦之 (6 members)
- **Design intent**: A "cannon fodder" synergy for less plot-relevant characters. Thinning out 拳师 (-3) and 剑客 (-1) bloat by giving these characters 龙套 instead.
- **Thresholds**: 2 / 4 / 6
  - 2: On death, deal 8% of own max HP as AOE damage to nearby enemies (3×3 area)
  - 4: On death, deal 15% of own max HP as AOE damage
  - 6: On death, deal 25% of own max HP as AOE damage + brief stun (15 frames)
- **New effect types needed**: `DeathAOE` (on-death % max HP AOE damage, with optional stun at highest tier)

## Disbanded Synergy

### 三大吸功 — REMOVED
- **Reason**: 段誉 had 4 synergies, and this synergy was the least impactful. Disbanded entirely.
- **Member redistribution**:
  - **段誉**: Keeps 天龙三兄弟 + 大理段氏, gains 踏雪无痕 → 3 synergies
  - **虚竹**: Keeps 天龙三兄弟 + 逍遥, gains 拳师 → 3 synergies
  - **无崖子**: Keeps 逍遥, gains 独行 → 2 synergies
  - **丁春秋**: Keeps 毒宗, gains 阴险 → 2 synergies
  - **游坦之**: Keeps 拳师, gains 龙套 → 2 synergies

## Reworked Synergies (4 existing)

### 1. 剑客 — 3/6 → 3/6/9 (Armor Pen focus)
- **Members**: Existing 9 + new (田归农, 灭绝师太, 张松溪, 俞莲舟, 殷梨亭, 岳不群, 东方不败, 郭襄) → sufficient for 9
- **Changes**:
  - 3: ATK +20%, Armor Pen 20% (on hit)
  - 6: ATK +40%, Armor Pen 50% (on hit)
  - 9: ATK +60%, Armor Pen 80% (on hit), **Execute**: 10% chance to instantly kill enemies below 10% HP
  - Removed: CDR from 3-piece and 6-piece
- **New effect types needed**: `Execute` (instant kill below HP threshold, on hit)

### 2. 拳师 — 3/6 → 3/6/9 (Stun focus)
- **Members**: Existing 16 + new (周芷若, 殷天正, 宋远桥, 无色禅师, 成昆, 黄裳, 王处一, 虚竹) → sufficient for 9
- **Note**: 俞岱岩, 莫声谷, 马钰 moved from 拳师 to 龙套 to reduce bloat. 虚竹 added (was 三大吸功, now gains 拳师 as 3rd synergy).
- **Changes**:
  - 3: ATK +50, HP +200, Stun 20% chance
  - 6: ATK +100, HP +350, Stun 35% chance, Knockback 20%
  - 9: ATK +150, HP +500, Stun 50% chance, Knockback 20%, **破罡**: 10% on attack, target cannot gain MP for 50 frames
  - Removed: Nothing; added stun to all tiers, added 破罡 at 9-piece
- **New effect types needed**: `MPBlock` (prevents MP gain for N frames, on hit)

### 3. 红颜 — 2/4 → 3/6/9 (Charm/CDR debuff)
- **Members**: Existing (阿紫, 阿朱, 阿碧, 黄蓉, 李秋水) + new (周芷若, 赵敏, 黛绮丝, 程灵素, 韩小莺) → 10 members, sufficient for 9-piece
- **Note**: 韩小莺 (ID 136) gets 红颜 as 3rd synergy (江南七怪 + 剑客 + 红颜). 郭襄 excluded (already at 3 synergies: 剑客+九阳传人+倚天屠龙).
- **Changes**:
  - 3: Speed +15, DEF +20%, Heal aura 10/120f, **倾国倾城**: 15% chance to increase attacker's cooldown by 50%
  - 6: Speed +25, DEF +35%, Heal aura 20/120f, Healed ATK/SPD +30%, **倾国倾城**: 30% chance to increase attacker's cooldown by 100%
  - 9: Speed +35, DEF +50%, Heal aura 30/120f, Healed ATK/SPD +50%, **倾国倾城**: 60% chance to increase attacker's cooldown by 150%, PLUS 10% chance to trigger 倾国倾城 offensively on attack
- **Note**: 倾国倾城 is per-tier, NOT the same effect at all thresholds. Old config effects for 倾国倾城 are removed and replaced with the above.
- **New effect types needed**: `CharmCDRDebuff` (increase enemy cooldown when attacked, per-tier chance/amount), `OffensiveCharm` (trigger charm on attack, 9-piece only)

### 4. 全真教 — 3 → 3/5/7 (Shield + explosion)
- **Members**: 丘处机, 周伯通, 王重阳, 马钰, 王处一, 谭处端, 刘处玄 (7 members)
- **Changes**:
  - 3: Shield = 20% max HP; on shield break, deal shield_value × 0.2 AoE damage
  - 5: Shield = 40% max HP; on shield break, deal shield_value × 0.4 AoE damage
  - 7: Shield = 60% max HP; on shield break, deal shield_value × 0.6 AoE damage; every 3 全真教 member deaths, surviving members gain a new shield
- **New effect types needed**: `ShieldExplosion` (AoE damage on shield break), `ShieldOnAllyDeath` (re-shield after N ally deaths)

## Balance & Pool Management

### Shop Size Configuration
- Add `商店数量` (shop slot count) to `chess_balance_*.yaml`, default 5 for Easy, 6 for Normal
- `ChessPool::getChessFromPool()` loop uses this config value instead of hardcoded 5
- UI layout in `ChessShopFlow` must accommodate 6 entries + 2 buttons (refresh + lock)

### Chess Banning System
- **New fields in `GameDataStore`**:
  - `std::set<int> seenRoleIds` — characters that appeared in any shop roll this run
  - `std::set<int> bannedRoleIds` — characters the player has banned
- **Config in `chess_balance_*.yaml`**:
  - `最大禁棋数` (max ban count), default 15
- **Rules**:
  - A character must have appeared in the shop at least once (`seenRoleIds`) before it can be banned
  - Bans are permanent for the run (not undoable)
  - Banned characters never appear in subsequent shop rolls
- **UI**: New "禁棋" button in chess menu → opens panel listing all seen characters with ban toggle, showing remaining ban count (e.g. "剩余禁棋: 12/15")
- **Pool filtering**: `ChessPool::selectFromPool()` skips banned characters (similar to existing `rejected_` logic)

### Easy Mode Curated Pool
- Easy mode gets its own `chess_pool_easy.yaml` (separate from `chess_pool.yaml`)
- Target: ~50-55 characters where every synergy either:
  - Has 0 members present (synergy not shown), OR
  - Has enough members to reach maximum threshold
- Shop size stays at 5 for Easy mode
- No banning system in Easy mode (unnecessary with curated pool)
- Character IDs annotated with name comments in YAML

### Pool Math
- With 88 characters and 6 shop slots: each roll sees ~6.8% of pool
- With 15 bans applied: effective pool ~73, seeing ~8.2% per roll
- This is comparable to pre-expansion (51 chars, 5 slots = ~9.8% per roll)

## Database Changes (0.db)

### Approach
- A Python script (`tools/add_chess_characters.py`) will:
  1. Query existing roles/magic by name to identify what exists
  2. INSERT new roles with appropriate stats scaled to tier
  3. INSERT new magic (skills) where needed
  4. UPDATE existing roles if stat adjustments are needed
  5. Cross-reference 人在江湖cpp版 CSV for fight frame IDs and head IDs
  6. Be re-runnable (idempotent — uses INSERT OR REPLACE)

### Stat Ranges by Tier (guidelines from existing pool)
| Tier | MaxHP | Attack | Defense | Speed |
|------|-------|--------|---------|-------|
| 1费 | 200-400 | 40-70 | 30-50 | 30-50 |
| 2费 | 350-550 | 60-90 | 40-65 | 40-60 |
| 3费 | 500-750 | 80-120 | 55-85 | 50-70 |
| 4费 | 650-950 | 100-150 | 70-110 | 60-85 |
| 5费 | 900-1200 | 140-200 | 90-130 | 75-100 |

### Resource Import
- Fight frames: Copy from `D:\games\人在江湖cpp版\人在江湖cpp版\game\resource\fight\fightXXXX.zip` to `work/game-dev/resource/fight/`
- Head images: Copy from `D:\games\人在江湖cpp版\人在江湖cpp版\game\resource\head\` to `work/game-dev/resource/head/`
- Character name → fight/head ID mapping done via 人在江湖 CSV (`1_renwu.csv`, columns: 编号, 头像/战斗代号, 姓名, 战斗图编号)

## Battle Map Expansion

The 真武七截阵 clone system requires ally positions beyond the current 10 (6 base + 4 extended). Need to:
1. Extend `DynamicChessMap::MapInfo` to include up to 3 additional positions (13 total)
2. Update `analyze_battle_maps.py` to find 3 more walkable positions per map
3. Update `DynamicChessMap::createBattle()` to pass extra positions
4. Update `ExtendedTeammateInfo` handling to support up to 13 teammates

## New Effect Types Summary (Code Changes)

New entries needed in `ChessBattleEffects.h` `EffectType` enum and `RoleComboState`:

| Effect | Enum Name | Description |
|--------|-----------|-------------|
| Bleed | `BleedChance` | On-hit % to apply bleed stack. Tick damage = 1% maxHP × stacks every 10 frames. |
| Bleed persist | `BleedPersist` | Stacks don't clear after tick. Part of 6-piece 刀客. |
| Post-skill dash | `PostSkillDash` | Speed acceleration away from enemies after ult (dash, not teleport). |
| Enemy top debuff | `EnemyTopDebuff` | Pre-battle: reduce N highest-star enemies' ATK/DEF/SPD by X. |
| Blink attack | `BlinkAttack` | Teleport to random position within range of target on every auto-attack. Hardcoded SFX. |
| Ally death stat boost | `AllyDeathStatBoost` | +X flat to ATK/DEF/SPD per combo ally death. |
| Clone summon | `CloneSummon` | Spawn N clones of highest-star member at battle start. |
| Projectile reflect | `ProjectileReflect` | % chance to reflect ranged attacks. |
| Ignore defense | `IgnoreDefense` | All damage bypasses defense and damage reduction. |
| Team heal on skill | `OnSkillTeamHeal` | Heal all allies by X when member uses ultimate. |
| Death prevention | `DeathPrevention` | Lock HP at 1 on lethal hit, grant invincibility. Once per battle. |
| Force pull (protect) | `ForcePullProtect` | Pull ally's attacker to beside synergy member. Once per battle per member. |
| Force pull (execute) | `ForcePullExecute` | Pull low-HP unattacked enemy to beside synergy member. Once per battle per member. |
| Execute | `Execute` | % chance to instantly kill targets below X% HP. |
| MP block | `MPBlock` | Prevents target from gaining MP for N frames. |
| Charm CDR debuff | `CharmCDRDebuff` | Per-tier % chance to increase attacker's cooldown by per-tier %. |
| Offensive charm | `OffensiveCharm` | 9-piece 红颜: 10% chance on attack to trigger 倾国倾城 offensively. |
| Death AOE | `DeathAOE` | On death, deal % of own max HP as AOE damage. Optional stun at 6-piece. |
| Shield explosion | `ShieldExplosion` | AoE damage on shield break. |
| Shield on ally death | `ShieldOnAllyDeath` | Re-apply shield after every N ally deaths. |

## Config File Changes

### New Files
- `config/chess_pool_easy.yaml` — curated Easy mode pool with name comments
- Existing `config/chess_pool.yaml` becomes the Normal mode pool (or rename for clarity)

### Modified Files
- `config/chess_pool.yaml` — add all ~37 new character IDs to appropriate tiers
- `config/chess_combos.yaml` — add 13 new synergies, remove 三大吸功, update 4 reworked synergies, add new members to existing synergies (独行 gets 田伯光/东方不败/黄裳/无崖子; 阴险 gets 丁春秋; 拳师 gets 虚竹; 红颜 gets 韩小莺; etc.)
- `config/chess_balance_normal.yaml` — add `商店数量: 6`, `最大禁棋数: 15`
- `config/chess_balance_easy.yaml` — add `商店数量: 5`, `最大禁棋数: 0`

## Constraints and Assumptions

### Constraints
- Max 3 synergies per character
- Duplicates (clones, same character bought twice) never count for synergy activation
- Battle maps currently have 10 ally positions; need to expand to 13 for clones
- Shop UI must accommodate 6 slots + 2 buttons (8 items total)
- All DB changes must be auditable via Python script
- `GameDataStore` serialization must remain backward-compatible (new fields have defaults)

### Assumptions
- Characters not found in either the current `0.db` or the 人在江湖 reference game will use closest-fit fight frames/heads
- New skills (magic) that don't exist are created with reasonable stats
- The Python script applies to all save files under `work/game-dev/save/`
- Easy mode pool can be determined analytically (synergy completeness check)

## Out of Scope
- Seasonal pool rotation system (design-aware but not implementing)
- Novel-based add-on packs (design-aware but not implementing)
- Re-tuning the enemy table in `chess_balance_*.yaml` for the expanded pool
- Equipment rebalancing
- Network/multiplayer implications

## Success Criteria
- [ ] All ~37 new characters added to `0.db` with correct stats, skills, fight frames, and heads
- [ ] Python script is auditable and re-runnable
- [ ] All 13 new synergies functional in battle
- [ ] 三大吸功 fully disbanded and members redistributed
- [ ] All 4 reworked synergies updated with new thresholds/effects
- [ ] All new effect types implemented in `ChessBattleEffects` and `BattleSceneHades`
- [ ] Shop size configurable (5 or 6) and UI adapts
- [ ] Chess banning system works (seen tracking, ban UI, persistence, pool filtering)
- [ ] Easy mode has curated pool where synergies are complete-or-absent
- [ ] Battle maps support up to 13 ally positions for clone summons
- [ ] Existing synergies and characters still function correctly  (no regressions)
- [ ] Game compiles and runs without errors

## Open Questions
- Fight frame availability for very obscure characters (马钰, 谭处端, 刘处玄) — use closest match from 人在江湖 reference game
- Whether 宗师 blink SFX should be a specific sound file (hardcoded placeholder for now, swappable later)
