# Task 4: Battle Logic — Simple Combat Effects

**Depends on**: Task 3 (effect type registration)
**Estimated complexity**: High
**Type**: Feature

## Objective

Implement runtime battle behavior for 15 "simple" effects in `BattleSceneHades.cpp`. These are effects that integrate into existing combat hooks (damage dealt, damage received, death, tick, pre-battle, post-skill).

## ⚠️ Important information

Before coding, Read FIRST -> Load [03-tasks-00-READBEFORE.md](03-tasks-00-READBEFORE.md)

Then read the full specification: [01-specification.md](01-specification.md) — specifically each synergy's effect description.

Before implementing, read `src/BattleSceneHades.cpp` thoroughly to understand:
- Where damage is calculated (search for `attack * attack` or `Attack.*Defence`)
- Where death is handled (search for `HP <= 0` or dead/alive checks)
- Where per-frame ticks happen (search for `poisonTimer` for tick pattern)
- Where post-skill handling is done (search for `postSkillInvincFrames`)
- Where pre-battle setup occurs (search for `buildComboStates` and `applyStateToCopy`)

## Files to Modify
- `src/BattleSceneHades.cpp`

## Detailed Steps

### 1. Pre-battle: EnemyTopDebuff (阴险)
In the pre-battle setup section (after `buildComboStates`), before applying states to copies:
- Iterate ally combo states. For each role with `enemyTopDebuffCount > 0`, collect the debuff parameters.
- Take the maximum `enemyTopDebuffCount` and `enemyTopDebuffValue` across all ally roles with this effect.
- Sort enemies by star level (descending). For the top N (`enemyTopDebuffCount`) enemies, reduce their base ATK, DEF, SPD by `enemyTopDebuffValue` each.
- Apply this BEFORE `applyStateToCopy` so the reduction is baked into the enemy copies.

### 2. Damage calculation: IgnoreDefense (倚天屠龙)
In the damage formula section:
- Check if the attacker's combo state has `ignoreDefense == true`.
- If so, set defense to 0 and damage reduction to 0 in the damage calculation.
- The formula is `damage = attack² / (attack + defence) / 4.0` — with defence=0, it becomes `damage = attack / 4.0`.

### 3. Post-hit: Bleed application (刀客)
After damage is dealt (in the attack resolution section):
- Check attacker's `bleedChancePct`. Roll a random number.
- If triggered: increment target's `bleedStacks` by 1 (up to `bleedMaxStacks`). Set `bleedTimer` to 10 if not already ticking. Copy attacker's `bleedPersist` flag to target's `bleedPersistFlag` (if attacker is 6-piece 刀客).

### 4. Per-frame tick: Bleed damage
In the per-frame update loop (near `poisonTimer` handling):
- For each role with `bleedStacks > 0`:
  - Decrement `bleedTimer`. When it reaches 0:
    - Deal damage = `bleedStacks * maxHP / 100` (1% per stack).
    - If `bleedPersistFlag` is false, decrement `bleedStacks` by 1 (stacks clear one at a time per tick).
    - Reset `bleedTimer` to 10 for next tick.
    - If `bleedStacks` reaches 0, stop ticking.

### 5. Post-hit: Execute (剑客 9-piece)
In the triggered effects processing (after attack hits):
- For `Execute` triggered effects: check if target HP < `executeThresholdPct`% of maxHP.
- Roll chance (`executePct`%). If triggered, set target HP to 0 (instant kill).

### 6. Post-hit: MPBlock (拳师 9-piece 破罡)
In the triggered effects processing:
- For `MPBlock` triggered effects: roll chance. If triggered, set target's `mpBlockTimer` to the specified frames.

### 7. Per-frame tick: MPBlock countdown
In the per-frame update:
- For each role with `mpBlockTimer > 0`: decrement each frame.
- In MP recovery logic: skip MP gain if `mpBlockTimer > 0`.

### 8. When attacked: CharmCDRDebuff (红颜 倾国倾城)
In the damage-received handler:
- Check defender's `charmCDRChancePct`. Roll chance.
- If triggered: increase attacker's current cooldown by `charmCDRAmountPct`%. (Find where cooldown/skill timer is stored and multiply remaining cooldown.)

### 9. On attack: OffensiveCharm (红颜 9-piece)
In the attack handler:
- Check attacker's `offensiveCharm`. If true, 10% chance to apply the charm CDR effect to the target (increase target's cooldown).

### 10. Ally death handler: AllyDeathStatBoost (四大法王)
When any ally dies:
- For each surviving ally in the same combo group that has `allyDeathStatBoost > 0`:
  - Increase their runtime ATK, DEF, SPD by `allyDeathStatBoost` each.
- Need to identify "same combo group" — check if the dead role and the surviving role share the 四大法王 combo membership.

### 11. Death handler: DeathPrevention (先天神照)
In the death/HP<=0 check:
- Before marking role as dead, check if their combo state has `deathPrevention == true && deathPreventionUsed == false`.
- If so: set HP = 1, grant invincibility frames (100), set `deathPreventionUsed = true`. Do NOT mark as dead.

### 12. Death handler: DeathAOE (龙套)
When a role dies:
- Check if their combo state has `deathAOEPct > 0`.
- Deal `deathAOEPct`% of the dead role's maxHP as AOE damage to enemies within 3×3 area around the dead role's position.
- If `deathAOEStunFrames > 0`, apply stun to affected enemies.

### 13. Post-skill: OnSkillTeamHeal (妙手仁心)
After a skill/ultimate completes:
- Check if the caster's combo state has `onSkillTeamHeal > 0`.
- Heal all living allies by `onSkillTeamHeal` HP (capped at maxHP).

### 14. Post-hit: ProjectileReflect (逍遥二仙)
For ranged attacks (check if attack is ranged):
- Check defender's `projectileReflectPct`. Roll chance.
- If triggered: the damage is applied to the attacker instead of the defender. (Or reduce defender damage to 0 and deal the same damage to attacker.)

### 15. Shield break: ShieldExplosion (全真教)
When shield reaches 0 (find where `s.shield` is decremented):
- Check if the role has `shieldExplosionPct > 0`.
- Deal `shieldValue * shieldExplosionPct / 100` as AoE damage to nearby enemies (3×3 area).
- Note: need to capture the shield value BEFORE it's set to 0.

### 16. Ally death: ShieldOnAllyDeath (全真教 7-piece)
When a combo ally dies:
- For surviving members with `shieldOnAllyDeathCount > 0`:
  - Increment `shieldOnAllyDeathTracker`.
  - If tracker >= `shieldOnAllyDeathCount`: reset tracker, re-apply shield (`shieldPctMaxHP`% of maxHP).

### 17. Verify compilation and basic smoke test
Build and run a quick test battle to ensure no crashes.

### 18. Commit: `feat: implement 15 simple combat effects for expanded chess pool`

## Acceptance Criteria
- [ ] Bleed DOT applies and ticks correctly
- [ ] Execute instant-kills below threshold
- [ ] MPBlock prevents MP gain
- [ ] IgnoreDefense bypasses defense in damage calc
- [ ] DeathPrevention locks HP at 1 once per battle
- [ ] CharmCDRDebuff increases attacker cooldown on being attacked
- [ ] DeathAOE deals AOE damage on death
- [ ] OnSkillTeamHeal heals allies after ult
- [ ] ProjectileReflect reflects ranged damage
- [ ] ShieldExplosion deals AOE on shield break
- [ ] ShieldOnAllyDeath re-applies shield after N ally deaths
- [ ] AllyDeathStatBoost buffs surviving combo members
- [ ] EnemyTopDebuff reduces top enemies' stats pre-battle
- [ ] Project compiles without errors

## Notes
- The damage formula is `damage = attack² / (attack + defence) / 4.0`. With `ignoreDefense`, defence=0: `damage = attack / 4.0`.
- Shield handling: search for where `s.shield` is modified — damage first reduces shield, then if shield <= 0, remaining damage hits HP.
- "3×3 area" for AOE: use the battle map's coordinate system. A role at (x,y) affects enemies within |dx|<=1 && |dy|<=1.
- DeathPrevention should fire BEFORE DeathAOE in the death handler (so if the role has both, prevention fires first and they don't die/explode).
- `CharmCDRDebuff` needs access to the attacker's skill cooldown timer. Search for how cooldown/skill timer works in `BattleSceneHades.cpp`.
