# Task 3: C++ Effect Type Registration — Enums, Parsing, State

**Depends on**: None (can run in parallel with Tasks 1-2)
**Estimated complexity**: Medium
**Type**: Feature

## Objective

Add ~20 new `EffectType` enum values, extend `RoleComboState` with new fields, register Chinese→enum mappings, and wire up `applyEffect()` switch cases. This is the plumbing that enables all new battle effects.

## ⚠️ Important information

Before coding, Read FIRST -> Load [03-tasks-00-READBEFORE.md](03-tasks-00-READBEFORE.md)

Then read the full specification: [01-specification.md](01-specification.md) — specifically the "New Effect Types Summary" table.

## Files to Modify
- `src/ChessBattleEffects.h`
- `src/ChessBattleEffects.cpp`
- `src/ChessCombo.h`

## Reference Files to Read
- `src/ChessBattleEffects.h` — Current `EffectType` enum, `RoleComboState` struct
- `src/ChessBattleEffects.cpp` — Current `effectTypeMap`, `applyEffect()` switch
- `src/ChessCombo.h` — Current `ComboId` enum

## Detailed Steps

### 1. Add ComboId enum values in `src/ChessCombo.h`

Add before `COUNT`:
```cpp
    DaoKe,              // 20. 刀客      (already exists)
    HongYan,            // 21. 红颜      (already exists)
    QinQiShuHua,        // 22. 琴棋书画  (already exists)
    DuXing,             // 23. 独行      (already exists)
    // --- New synergies below ---
    TaXueWuHen,         // 24. 踏雪无痕
    YinXian,            // 25. 阴险
    ZongShi,            // 26. 宗师
    SiDaFaWang,         // 27. 四大法王
    ZhenWuQiJie,        // 28. 真武七截阵
    XiaoYaoErXian,      // 29. 逍遥二仙
    JiuYangChuanRen,    // 30. 九阳传人
    YiTianTuLong,       // 31. 倚天屠龙
    MiaoShouRenXin,     // 32. 妙手仁心
    XianTianShenZhao,   // 33. 先天神照
    QianKunNuoYi,       // 34. 乾坤挪移
    LongTao,            // 35. 龙套
    COUNT
```

### 2. Add EffectType enum values in `src/ChessBattleEffects.h`

Add after `HealBurst`:
```cpp
    // === New effects for expanded chess pool ===
    BleedChance,
    BleedPersist,
    PostSkillDash,
    EnemyTopDebuff,
    BlinkAttack,
    AllyDeathStatBoost,
    CloneSummon,
    ProjectileReflect,
    IgnoreDefense,
    OnSkillTeamHeal,
    DeathPrevention,
    ForcePullProtect,
    ForcePullExecute,
    Execute,
    MPBlock,
    CharmCDRDebuff,
    OffensiveCharm,
    DeathAOE,
    ShieldExplosion,
    ShieldOnAllyDeath,
```

### 3. Add new fields to `RoleComboState` in `src/ChessBattleEffects.h`

Add after the existing fields (before `// Mutable runtime state`):
```cpp
    // --- New effects (expanded pool) ---
    int bleedChancePct = 0;
    bool bleedPersist = false;
    int bleedMaxStacks = 5;
    bool postSkillDash = false;
    bool blinkAttack = false;
    int allyDeathStatBoost = 0;
    int cloneSummonCount = 0;
    int projectileReflectPct = 0;
    bool ignoreDefense = false;
    int onSkillTeamHeal = 0;
    bool deathPrevention = false;
    int executePct = 0;
    int executeThresholdPct = 0;
    int mpBlockFrames = 0;
    int charmCDRChancePct = 0;
    int charmCDRAmountPct = 0;
    bool offensiveCharm = false;
    int deathAOEPct = 0;
    int deathAOEStunFrames = 0;
    int shieldExplosionPct = 0;       // multiplier as integer percent (e.g. 20 = 0.2×)
    int shieldOnAllyDeathCount = 0;   // re-shield after every N combo ally deaths
    int enemyTopDebuffCount = 0;
    int enemyTopDebuffValue = 0;
    int forcePullProtect = false;     // Note: use int (0/1) not bool, for force-pull per-member tracking
    int forcePullExecute = false;
```

Add to `// Mutable runtime state` section:
```cpp
    // Bleed tracking (on target)
    int bleedStacks = 0;
    int bleedTimer = 0;
    // MP block tracking
    int mpBlockTimer = 0;
    // Death prevention tracking
    bool deathPreventionUsed = false;
    // Force pull tracking (per-member, once per battle)
    bool forcePullProtectUsed = false;
    bool forcePullExecuteUsed = false;
    // Shield on ally death tracker
    int shieldOnAllyDeathTracker = 0;
```

### 4. Add Chinese→enum mappings in `src/ChessBattleEffects.cpp`

Add to `effectTypeMap`:
```cpp
    {"流血", EffectType::BleedChance},
    {"流血持续", EffectType::BleedPersist},
    {"踏雪", EffectType::PostSkillDash},
    {"敌方削弱", EffectType::EnemyTopDebuff},
    {"闪击", EffectType::BlinkAttack},
    {"同袍之死", EffectType::AllyDeathStatBoost},
    {"七截分身", EffectType::CloneSummon},
    {"弹反", EffectType::ProjectileReflect},
    {"无视防御", EffectType::IgnoreDefense},
    {"群体施治", EffectType::OnSkillTeamHeal},
    {"死亡庇护", EffectType::DeathPrevention},
    {"保护挪移", EffectType::ForcePullProtect},
    {"处决挪移", EffectType::ForcePullExecute},
    {"斩杀", EffectType::Execute},
    {"破罡", EffectType::MPBlock},
    {"倾国倾城", EffectType::CharmCDRDebuff},
    {"攻击倾城", EffectType::OffensiveCharm},
    {"殉爆", EffectType::DeathAOE},
    {"护盾爆炸", EffectType::ShieldExplosion},
    {"护盾重获", EffectType::ShieldOnAllyDeath},
```

### 5. Add cases in `applyEffect()` switch in `src/ChessBattleEffects.cpp`

For each new effect type, add the appropriate case:
```cpp
    case EffectType::BleedChance: s.bleedChancePct += e.value; if (e.value2 > 0) s.bleedMaxStacks = e.value2; break;
    case EffectType::BleedPersist: s.bleedPersist = true; break;
    case EffectType::PostSkillDash: s.postSkillDash = true; break;
    case EffectType::EnemyTopDebuff: s.enemyTopDebuffCount = e.value; s.enemyTopDebuffValue = e.value2; break;
    case EffectType::BlinkAttack: s.blinkAttack = true; break;
    case EffectType::AllyDeathStatBoost: s.allyDeathStatBoost += e.value; break;
    case EffectType::CloneSummon: s.cloneSummonCount = std::max(s.cloneSummonCount, e.value); break;
    case EffectType::ProjectileReflect: s.projectileReflectPct += e.value; break;
    case EffectType::IgnoreDefense: s.ignoreDefense = true; break;
    case EffectType::OnSkillTeamHeal: s.onSkillTeamHeal = std::max(s.onSkillTeamHeal, e.value); break;
    case EffectType::DeathPrevention: s.deathPrevention = true; break;
    case EffectType::ForcePullProtect: s.forcePullProtect = true; break;
    case EffectType::ForcePullExecute: s.forcePullExecute = true; break;
    case EffectType::Execute: break;  // handled as triggered effect (OnHit)
    case EffectType::MPBlock: break;  // handled as triggered effect (OnHit)
    case EffectType::CharmCDRDebuff: s.charmCDRChancePct = e.value; s.charmCDRAmountPct = e.value2; break;
    case EffectType::OffensiveCharm: s.offensiveCharm = true; break;
    case EffectType::DeathAOE: s.deathAOEPct = std::max(s.deathAOEPct, e.value); if (e.value2 > 0) s.deathAOEStunFrames = e.value2; break;
    case EffectType::ShieldExplosion: s.shieldExplosionPct = std::max(s.shieldExplosionPct, e.value); break;
    case EffectType::ShieldOnAllyDeath: s.shieldOnAllyDeathCount = e.value; break;
```

### 6. Verify the project compiles
Build the solution (`kys.sln` on Windows, CMake on Linux) and fix any compilation errors.

### 7. Commit: `feat: register 20 new effect types and 13 new combo IDs`

## Acceptance Criteria
- [ ] All 20 new `EffectType` enum values added
- [ ] All 13 new `ComboId` enum values added (before `COUNT`)
- [ ] `RoleComboState` has all new fields with correct defaults
- [ ] `effectTypeMap` has all 20 new Chinese→enum mappings
- [ ] `applyEffect()` has case for each new enum value
- [ ] Project compiles without errors

## Notes
- `Execute` and `MPBlock` use the triggered-effect pattern (like `ArmorPen` and `Stun`) — they go through `triggeredEffects` vector when they have `OnHit` trigger. The `applyEffect()` switch just `break`s for these (the effect is stored in `triggeredEffects` by the non-Always trigger path at the top of `applyEffect()`).
- `CharmCDRDebuff` stores both chance and amount as direct fields (not triggered) because it's a defensive proc (when-attacked), not an on-hit effect. The runtime behavior is handled in BattleSceneHades.
- `EnemyTopDebuff` is a pre-battle effect — the `enemyTopDebuffCount` and `enemyTopDebuffValue` are read during battle setup to debuff enemies before the fight starts.
