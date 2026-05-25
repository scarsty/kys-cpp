# Battle Combo Effect Surface Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Delete the compiled combo-effect field layer so runtime combo behavior is driven by configured `AppliedEffectInstance` data plus small per-effect runtime state.

**Architecture:** Keep the existing YAML/config shape: `EffectType`, `Trigger`, `value`, `value2`, `duration`, and `maxCount`. Do not add a new command bus or richer config DSL. Replace `config effect -> RoleComboState field -> command/result struct -> BattleGameplayCommand -> reducer` with `config effect -> effect query/activation -> direct runtime application or existing damage/attack request`.

**Tech Stack:** C++23, Catch2, CMake/Visual Studio project files, PowerShell, ripgrep, `.github/build-command.ps1`.

---

## Hard Constraints

- Preserve top-level config compatibility. Do not require users to rewrite `config/chess_*.yaml`.
- New source-visible Chinese text must use Traditional Chinese.
- Do not add a replacement public variant type that recreates the current command surface.
- Do not add fake defensive boundary checks. Use assertions for invariants.
- Prefer deleting effect-specific fields over adding facade functions that only hide them.
- Keep behavior characterization tests around every migrated effect cluster.
- Build after non-trivial changes with `.github/build-command.ps1`; final link failure is acceptable only when caused by the game already running.

## Reduction Contract

The branch is not complete unless all checks below are true:

- `RoleComboState` in `src/ChessBattleEffects.h` has materially fewer effect-specific scalar fields.
- `ChessBattleEffects::applyEffect(...)` no longer expands most runtime effects into named fields.
- `BattleComboTriggerSystem.h` exposes fewer specialized command/result APIs.
- `BattleGameplayCommand` in `src/battle/BattleHitResolver.h` has fewer variants.
- `BattleOnHitComboCommand`, `BattleShieldBreakCommand`, and `BattleStunCommand` are deleted.
- No new public `BattleEffectCommand`, `BattleEffectOp`, or equivalent generic command variant is introduced.

## File Structure

- Modify `src/ChessBattleEffects.h`: keep config data types, add a small query/runtime-state surface, remove compiled runtime effect fields from `RoleComboState`.
- Modify `src/ChessBattleEffects.cpp`: keep parse compatibility, stop expanding runtime effects into scalar fields, implement query helpers.
- Modify `src/battle/BattleComboTriggerSystem.h`: keep generic activation APIs, delete specialized command APIs.
- Modify `src/battle/BattleComboTriggerSystem.cpp`: evaluate effects from `appliedEffects` / `triggeredEffects` and effect-index runtime state.
- Modify `src/battle/BattleRuntimeUnitSpawn.cpp`: derive initial status/damage/shield rows from effect queries.
- Modify `src/battle/BattleInitialization.cpp`: derive initialization-only stat and clone behavior from effect queries.
- Modify `src/battle/BattleCore.cpp`: apply activated effects directly at frame, cast, hit, shield-break, and lifecycle boundaries.
- Modify `src/battle/BattleHitResolver.cpp`: remove combo field reads and specialized command emission where hit logic can apply configured effects directly.
- Modify `src/battle/BattleTeamEffectSystem.cpp`: read MP recovery from effect queries instead of `mpRecoveryBonusPct`.
- Modify tests under `tests/`: migrate direct field setup to `AppliedEffectInstance` setup and add characterization tests for migrated clusters.
- Modify `src/CMakeLists.txt`, `tests/kys_tests.vcxproj`, and Visual Studio filter files if a new test file is created.

---

## Task 1: Capture Baseline Complexity And Guardrails

**Files:**
- Inspect: `src/ChessBattleEffects.h`
- Inspect: `src/battle/BattleComboTriggerSystem.h`
- Inspect: `src/battle/BattleHitResolver.h`
- Inspect: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Capture field and command counts before editing**

Run:

```powershell
@{
  RoleComboStateFieldLines = (Select-String -Path 'src/ChessBattleEffects.h' -Pattern '^\s+(int|bool|double|std::|struct) ' | Measure-Object).Count
  ComboTriggerStructs = (Select-String -Path 'src/battle/BattleComboTriggerSystem.h' -Pattern '^struct |^enum class ' | Measure-Object).Count
  GameplayCommandVariants = (Select-String -Path 'src/battle/BattleHitResolver.h' -Pattern '^\s+Battle[A-Za-z0-9]+Command,?' | Measure-Object).Count
  BattleCoreLines = (Get-Content -LiteralPath 'src/battle/BattleCore.cpp' | Measure-Object -Line).Lines
}
```

Expected: PowerShell prints four numeric properties. Save the output in the implementation notes for the branch. Do not commit generated metrics files.

- [ ] **Step 2: Confirm direct references to specialized command surfaces**

Run:

```powershell
rg -n "BattleOnHitComboCommand|BattleShieldBreakCommand|BattleStunCommand|collectOnHitComboCommands|collectShieldBreakCommands|collectStunCommands|BattleComboFrameRuntimeEvent|BattleGameplayCommand" src\battle src\ChessBattleEffects.*
```

Expected: matches in `BattleComboTriggerSystem.h`, `BattleComboTriggerSystem.cpp`, `BattleCore.cpp`, and `BattleHitResolver.h`.

- [ ] **Step 3: Commit baseline-free checkpoint only if the worktree already has staged changes from another task**

Run:

```powershell
git status --short
```

Expected: if there are unrelated user changes, leave them untouched. Do not reset, checkout, or revert files.

---

## Task 2: Add Effect Query Helpers And Tests

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/ChessBattleEffects.cpp`
- Create: `tests/ChessBattleEffectsUnitTests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Add failing tests for always-effect queries**

Create `tests/ChessBattleEffectsUnitTests.cpp` with:

```cpp
#include "ChessBattleEffects.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

namespace
{

ComboEffect effect(EffectType type, int value, int value2 = 0, Trigger trigger = Trigger::Always)
{
    ComboEffect result;
    result.type = type;
    result.value = value;
    result.value2 = value2;
    result.trigger = trigger;
    return result;
}

}  // namespace

TEST_CASE("ChessBattleEffects_StoresRuntimeEffectsAsConfiguredInstances", "[battle][effects]")
{
    RoleComboState state;

    ChessBattleEffects::applyEffect(state, effect(EffectType::MPOnHit, 5));
    ChessBattleEffects::applyEffect(state, effect(EffectType::MPOnHit, 7));
    ChessBattleEffects::applyEffect(state, effect(EffectType::Stun, 12, 0, Trigger::OnHit));

    REQUIRE(state.appliedEffects.size() == 3);
    CHECK(state.appliedEffects[0].type == EffectType::MPOnHit);
    CHECK(state.appliedEffects[1].type == EffectType::MPOnHit);
    CHECK(state.appliedEffects[2].type == EffectType::Stun);
    REQUIRE(state.triggeredEffects.size() == 1);
    CHECK(state.triggeredEffects[0].type == EffectType::Stun);
}

TEST_CASE("ChessBattleEffects_QueryHelpersReadConfiguredEffects", "[battle][effects]")
{
    RoleComboState state;

    ChessBattleEffects::applyEffect(state, effect(EffectType::MPOnHit, 5));
    ChessBattleEffects::applyEffect(state, effect(EffectType::MPOnHit, 7));
    ChessBattleEffects::applyEffect(state, effect(EffectType::FlatShield, 13));
    ChessBattleEffects::applyEffect(state, effect(EffectType::ShieldPctMaxHP, 25));
    ChessBattleEffects::applyEffect(state, effect(EffectType::ForceRangedAttack, 150, 4));

    CHECK(sumAlwaysEffectValue(state, EffectType::MPOnHit) == 12);
    CHECK(sumAlwaysEffectValue(state, EffectType::FlatShield) == 13);
    CHECK(maxAlwaysEffectValue(state, EffectType::ShieldPctMaxHP) == 25);
    REQUIRE(firstAlwaysEffect(state, EffectType::ForceRangedAttack) != nullptr);
    CHECK(firstAlwaysEffect(state, EffectType::ForceRangedAttack)->value == 150);
    CHECK(firstAlwaysEffect(state, EffectType::ForceRangedAttack)->value2 == 4);
}
```

- [ ] **Step 2: Add the new test file to build metadata**

In `src/CMakeLists.txt`, add this line inside `KYS_TEST_SOURCES` after `BattleCastSystemUnitTests.cpp`:

```cmake
    "${KYS_ROOT}/tests/ChessBattleEffectsUnitTests.cpp"
```

In `tests/kys_tests.vcxproj`, add this item in the existing `<ItemGroup>` with other test `.cpp` files:

```xml
    <ClCompile Include="ChessBattleEffectsUnitTests.cpp" />
```

- [ ] **Step 3: Run the new tests and verify the helper test fails**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "ChessBattleEffects_QueryHelpersReadConfiguredEffects"
```

Expected: build or test failure because `sumAlwaysEffectValue`, `maxAlwaysEffectValue`, and `firstAlwaysEffect` are not declared.

- [ ] **Step 4: Add query helper declarations**

In `src/ChessBattleEffects.h`, after `struct RoleComboState` and before `class ChessBattleEffects`, add:

```cpp
int sumAlwaysEffectValue(const RoleComboState& state, EffectType type);
int maxAlwaysEffectValue(const RoleComboState& state, EffectType type);
const AppliedEffectInstance* firstAlwaysEffect(const RoleComboState& state, EffectType type);
```

- [ ] **Step 5: Add query helper implementation**

In `src/ChessBattleEffects.cpp`, after `getEffectTypeMap()` and before `parseEffect(...)`, add:

```cpp
int sumAlwaysEffectValue(const RoleComboState& state, EffectType type)
{
    int total = 0;
    for (const auto& effect : state.appliedEffects)
    {
        if (effect.type == type && effect.trigger == Trigger::Always)
        {
            total += effect.value;
        }
    }
    return total;
}

int maxAlwaysEffectValue(const RoleComboState& state, EffectType type)
{
    int value = 0;
    for (const auto& effect : state.appliedEffects)
    {
        if (effect.type == type && effect.trigger == Trigger::Always)
        {
            value = std::max(value, effect.value);
        }
    }
    return value;
}

const AppliedEffectInstance* firstAlwaysEffect(const RoleComboState& state, EffectType type)
{
    for (const auto& effect : state.appliedEffects)
    {
        if (effect.type == type && effect.trigger == Trigger::Always)
        {
            return &effect;
        }
    }
    return nullptr;
}
```

- [ ] **Step 6: Run tests for the new helper**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "ChessBattleEffects_*"
```

Expected: the two `ChessBattleEffects_*` tests pass.

- [ ] **Step 7: Commit**

Run:

```powershell
git add src\ChessBattleEffects.h src\ChessBattleEffects.cpp src\CMakeLists.txt tests\kys_tests.vcxproj tests\ChessBattleEffectsUnitTests.cpp
git commit -m "test: characterize configured combo effect queries"
```

Expected: commit succeeds.

---

## Task 3: Stop Materializing Initialization-Owned Runtime Fields

**Files:**
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleStatusSystem.cpp`
- Modify: `src/battle/BattleTeamEffectSystem.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/BattleTeamEffectSystemUnitTests.cpp`

- [ ] **Step 1: Replace tests that set spawn-owned combo fields directly**

Search:

```powershell
rg -n "\.(flatShield|shieldPctMaxHP|shieldFreezeResPct|freezeReductionPct|controlImmunityFrames|damageImmunityAfterFrames|damageImmunityDuration|hurtInvincFrames|blockFirstHitsCount|deathPrevention|deathPreventionFrames|killHealPct|killInvincFrames|bloodlustATKPerKill|mpRecoveryBonusPct)" tests
```

For each match, replace direct field assignment with `ChessBattleEffects::applyEffect(...)`. Use this pattern:

```cpp
KysChess::RoleComboState combo;
KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::FlatShield, 12 });
KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::ShieldPctMaxHP, 25 });
```

Expected after editing:

```powershell
rg -n "\.(flatShield|shieldPctMaxHP|shieldFreezeResPct|freezeReductionPct|controlImmunityFrames|damageImmunityAfterFrames|damageImmunityDuration|hurtInvincFrames|blockFirstHitsCount|deathPrevention|deathPreventionFrames|killHealPct|killInvincFrames|bloodlustATKPerKill|mpRecoveryBonusPct)\s*=" tests
```

prints no matches.

- [ ] **Step 2: Update runtime spawn derivation to use effect queries**

In `src/battle/BattleRuntimeUnitSpawn.cpp`, replace reads of spawn-owned combo fields with query helper calls:

```cpp
int initialShieldFor(const BattleRuntimeUnit& unit, const RoleComboState& combo)
{
    int shield = sumAlwaysEffectValue(combo, EffectType::FlatShield);
    const int shieldPct = sumAlwaysEffectValue(combo, EffectType::ShieldPctMaxHP);
    if (shieldPct > 0)
    {
        shield += unit.vitals.maxHp * shieldPct / 100;
    }
    return shield;
}
```

For `makeInitialDamageRuntimeUnit(...)`, use:

```cpp
damage.hurtInvincFrames = maxAlwaysEffectValue(combo, EffectType::HurtInvincFrames);
damage.blockFirstHitsRemaining = sumAlwaysEffectValue(combo, EffectType::BlockFirstHits);
damage.deathPrevention = maxAlwaysEffectValue(combo, EffectType::DeathPrevention) > 0;
damage.deathPreventionUsed = combo.deathPreventionUsed;
damage.deathPreventionFrames = maxAlwaysEffectValue(combo, EffectType::DeathPrevention);
damage.killHealPct = sumAlwaysEffectValue(combo, EffectType::KillHealPct);
damage.killInvincFrames = maxAlwaysEffectValue(combo, EffectType::KillInvincFrames);
damage.bloodlustAttackPerKill = sumAlwaysEffectValue(combo, EffectType::Bloodlust);
```

- [ ] **Step 3: Update status runtime derivation to use effect queries**

In `src/battle/BattleStatusSystem.cpp`, inside `makeBattleStatusUnitState(...)` / `makeBattleStatusRuntimeUnit(...)` paths that read combo fields, replace with:

```cpp
status.effects.freezeReductionPct = sumAlwaysEffectValue(state, EffectType::FreezeReductionPct);
status.effects.shieldFreezeResPct = sumAlwaysEffectValue(state, EffectType::ShieldFreezeRes);
status.effects.controlImmunityFrames = sumAlwaysEffectValue(state, EffectType::ControlImmunityFrames);
status.effects.damageImmunityAfterFrames = maxAlwaysEffectValue(state, EffectType::DamageImmunityAfterFrames);
if (const auto* immunity = firstAlwaysEffect(state, EffectType::DamageImmunityAfterFrames))
{
    status.effects.damageImmunityDuration = immunity->value2;
}
if (status.effects.damageImmunityAfterFrames > 0)
{
    status.effects.damageImmunityTimer = status.effects.damageImmunityAfterFrames;
}
```

- [ ] **Step 4: Update MP recovery bonus reads**

In `src/battle/BattleTeamEffectSystem.cpp`, replace:

```cpp
const int recoveryBonus = comboIt != combos.end() ? comboIt->second.mpRecoveryBonusPct : 0;
```

with:

```cpp
const int recoveryBonus = comboIt != combos.end()
    ? sumAlwaysEffectValue(comboIt->second, EffectType::MPRecoveryBonus)
    : 0;
```

- [ ] **Step 5: Stop materializing migrated fields in `applyEffect(...)`**

In `src/ChessBattleEffects.cpp`, change these `case` bodies to only `break;` because `appliedEffects` already stores the configured instance:

```cpp
case EffectType::FlatShield: break;
case EffectType::ShieldPctMaxHP: break;
case EffectType::ShieldFreezeRes: break;
case EffectType::FreezeReductionPct: break;
case EffectType::ControlImmunityFrames: break;
case EffectType::DamageImmunityAfterFrames: break;
case EffectType::HurtInvincFrames: break;
case EffectType::BlockFirstHits: break;
case EffectType::DeathPrevention: break;
case EffectType::KillHealPct: break;
case EffectType::KillInvincFrames: break;
case EffectType::Bloodlust: break;
case EffectType::MPRecoveryBonus: break;
```

- [ ] **Step 6: Delete migrated fields from `RoleComboState`**

In `src/ChessBattleEffects.h`, delete these members:

```cpp
int flatShield = 0;
int shieldPctMaxHP = 0;
int shieldFreezeResPct = 0;
int freezeReductionPct = 0;
int controlImmunityFrames = 0;
int killHealPct = 0;
int killInvincFrames = 0;
int bloodlustATKPerKill = 0;
bool deathPrevention = false;
int deathPreventionFrames = 0;
int damageImmunityAfterFrames = 0;
int damageImmunityDuration = 0;
int blockFirstHitsCount = 0;
int hurtInvincFrames = 0;
int mpRecoveryBonusPct = 0;
```

Keep `deathPreventionUsed` for now because it is mutable runtime state and will be migrated in a later owner cleanup.

- [ ] **Step 7: Run focused tests**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "[battle][initialization]" "[battle][team_effect]" "BattleFrameRunner_RunFrame_AppliesRuntimeMpRegenBlockAndRecovery"
```

Expected: tests pass.

- [ ] **Step 8: Commit**

Run:

```powershell
git add src\ChessBattleEffects.h src\ChessBattleEffects.cpp src\battle\BattleRuntimeUnitSpawn.cpp src\battle\BattleStatusSystem.cpp src\battle\BattleTeamEffectSystem.cpp tests
git commit -m "refactor: derive spawn combo effects from configured instances"
```

Expected: commit succeeds.

---

## Task 4: Move Frame Periodic Effects Off Named Combo Fields

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/battle/BattleComboTriggerSystem.h`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleComboTriggerSystemUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Rewrite frame periodic tests to use configured effects**

In `tests/BattleComboTriggerSystemUnitTests.cpp`, update `BattleComboTriggerSystem_FrameRuntime_AdvancesTimersAndEmitsPeriodicEffects` so setup uses:

```cpp
RoleComboState state;
ChessBattleEffects::applyEffect(state, { EffectType::AutoUltimateAfterFrames, 3 });
ChessBattleEffects::applyEffect(state, { EffectType::HPRegenPct, 12, 5 });
ChessBattleEffects::applyEffect(state, { EffectType::HealAuraFlat, 7, 5 });
ChessBattleEffects::applyEffect(state, { EffectType::HealAuraPct, 9, 5 });
ChessBattleEffects::applyEffect(state, { EffectType::HealedATKSPDBoost, 15 });
state.effectFrameTimers[2] = 1;
state.triggerTimers[Trigger::AllyLowHPBurst] = 2;
state.rampings.push_back({ 10, 3 });
state.rampingStacks.push_back(2);
state.rampingIdleTimers.push_back(0);
```

The expected event assertions stay the same except auto-ultimate timer state should read:

```cpp
CHECK(state.effectFrameTimers.at(2) == 3);
```

- [ ] **Step 2: Add effect-index frame timers**

In `src/ChessBattleEffects.h`, add this runtime member beside `effectActivationCounts`:

```cpp
std::map<int, int> effectFrameTimers;
```

- [ ] **Step 3: Initialize periodic auto-ultimate timer from configured effects**

In `src/battle/BattleInitialization.cpp`, replace `combo.autoUltimateAfterFrames` / `combo.autoUltimateTimer` initialization with:

```cpp
for (int effectIndex = 0; effectIndex < static_cast<int>(combo.appliedEffects.size()); ++effectIndex)
{
    const auto& effect = combo.appliedEffects[effectIndex];
    if (effect.type == EffectType::AutoUltimateAfterFrames && effect.trigger == Trigger::Always && effect.value > 0)
    {
        combo.effectFrameTimers[effectIndex] = effect.value;
    }
}
```

Use this same loop in clone state creation if clones need fresh periodic timers.

- [ ] **Step 4: Rewrite `advanceFrameRuntime(...)` to scan effects**

In `src/battle/BattleComboTriggerSystem.cpp`, update periodic sections to scan `state.appliedEffects`:

```cpp
for (int effectIndex = 0; effectIndex < static_cast<int>(state.appliedEffects.size()); ++effectIndex)
{
    const auto& effect = state.appliedEffects[effectIndex];
    if (!input.alive || effect.trigger != Trigger::Always)
    {
        continue;
    }
    if (effect.type == EffectType::AutoUltimateAfterFrames && effect.value > 0)
    {
        int& timer = state.effectFrameTimers[effectIndex];
        if (timer <= 0)
        {
            timer = effect.value;
        }
        --timer;
        if (timer <= 0)
        {
            events.push_back({ BattleComboFrameRuntimeEventType::AutoUltimateReady });
            timer = effect.value;
        }
    }
    else if (effect.type == EffectType::HPRegenPct && effect.value > 0 && effect.value2 > 0 && input.frame % effect.value2 == 0)
    {
        events.push_back({
            BattleComboFrameRuntimeEventType::SelfHpRegen,
            effect.trigger,
            effectIndex,
            effect.value,
        });
    }
}
```

Then calculate heal aura by scanning `HealAuraFlat`, `HealAuraPct`, and `HealedATKSPDBoost` effects:

```cpp
int healAuraFlat = 0;
int healAuraPct = 0;
int healAuraInterval = 0;
int healedBoostPct = 0;
for (const auto& effect : state.appliedEffects)
{
    if (effect.trigger != Trigger::Always)
    {
        continue;
    }
    if (effect.type == EffectType::HealAuraFlat)
    {
        healAuraFlat = std::max(healAuraFlat, effect.value);
        healAuraInterval = effect.value2 > 0 ? effect.value2 : healAuraInterval;
    }
    else if (effect.type == EffectType::HealAuraPct)
    {
        healAuraPct = std::max(healAuraPct, effect.value);
        healAuraInterval = effect.value2 > 0 ? effect.value2 : healAuraInterval;
    }
    else if (effect.type == EffectType::HealedATKSPDBoost)
    {
        healedBoostPct += effect.value;
    }
}
if (input.alive && (healAuraPct > 0 || healAuraFlat > 0) && healAuraInterval > 0 && input.frame % healAuraInterval == 0)
{
    events.push_back({
        BattleComboFrameRuntimeEventType::HealAura,
        Trigger::Always,
        -1,
        healAuraFlat,
        healAuraPct,
        healedBoostPct,
    });
}
```

- [ ] **Step 5: Stop materializing frame-periodic fields**

In `src/ChessBattleEffects.cpp`, change these cases to only `break;`:

```cpp
case EffectType::HPRegenPct: break;
case EffectType::HealAuraPct: break;
case EffectType::HealAuraFlat: break;
case EffectType::HealedATKSPDBoost: break;
case EffectType::AutoUltimateAfterFrames: break;
```

- [ ] **Step 6: Delete frame-periodic fields**

In `src/ChessBattleEffects.h`, delete:

```cpp
int healAuraPct = 0;
int healAuraFlat = 0;
int healAuraInterval = 0;
int healedATKSPDBoostPct = 0;
int hpRegenPct = 0;
int hpRegenInterval = 0;
int autoUltimateAfterFrames = 0;
int autoUltimateTimer = 0;
```

- [ ] **Step 7: Run focused tests**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "BattleComboTriggerSystem_FrameRuntime_*" "BattleFrameRunner_AdvanceFrame_AppliesFrameRuntimeTeamEffects" "BattleFrameRunner_AdvanceFrame_AppliesBurstHealFrameTrigger"
```

Expected: tests pass.

- [ ] **Step 8: Commit**

Run:

```powershell
git add src\ChessBattleEffects.h src\ChessBattleEffects.cpp src\battle\BattleComboTriggerSystem.* src\battle\BattleCore.cpp src\battle\BattleInitialization.cpp tests
git commit -m "refactor: evaluate frame combo effects from configured effects"
```

Expected: commit succeeds.

---

## Task 5: Replace Specialized Trigger Command APIs With Activated Effects

**Files:**
- Modify: `src/battle/BattleComboTriggerSystem.h`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `tests/BattleComboTriggerSystemUnitTests.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Convert tests off specialized command APIs**

Replace tests that call `collectOnHitComboCommands`, `collectShieldBreakCommands`, or `collectStunCommands` with calls to `collectTriggerEvents(...)`.

For shield break, use:

```cpp
auto events = BattleComboTriggerSystem().collectTriggerEvents(
    state,
    { BattleComboTriggerHook::ShieldBreak, 1, 1 },
    {
        EffectType::ShieldExplosion,
        EffectType::AutoUltimate,
        EffectType::TempFlatATK,
        EffectType::MPRestore,
    },
    random);
```

For hit stun and MP block, use:

```cpp
auto events = BattleComboTriggerSystem().collectTriggerEvents(
    state,
    { BattleComboTriggerHook::DamageDealt, 1, 2 },
    {
        EffectType::Stun,
        EffectType::MPBlock,
        EffectType::NearbyTrackingProjectiles,
        EffectType::DmgReduceDebuff,
    },
    random);
```

- [ ] **Step 2: Run converted tests before API deletion**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "[battle][combo][unit]"
```

Expected: combo trigger tests pass while old APIs still exist.

- [ ] **Step 3: Rewrite shield-break application in `BattleCore.cpp`**

In `appendFrameShieldBreakCommands(...)`, replace `collectShieldBreakCommands(...)` with `collectTriggerEvents(...)`. Apply events by `event.effect.type`.

Use this logic:

```cpp
auto shieldBreakEvents = BattleComboTriggerSystem().collectTriggerEvents(
    defenderCombo,
    { BattleComboTriggerHook::ShieldBreak, transaction.defender.id, transaction.defender.id },
    {
        EffectType::ShieldExplosion,
        EffectType::AutoUltimate,
        EffectType::TempFlatATK,
        EffectType::MPRestore,
    },
    state.random,
    BattleComboActivationRecording::CallerRecords);
for (const auto& event : shieldBreakEvents)
{
    bool activated = true;
    switch (event.effect.type)
    {
    case EffectType::ShieldExplosion:
        shieldExplosionPct = std::max(shieldExplosionPct, event.effect.value);
        break;
    case EffectType::AutoUltimate:
        frame.frameCommands.push_back(BattleAutoUltimateCommand{ transaction.defender.id, false });
        break;
    case EffectType::TempFlatATK:
        frame.frameCommands.push_back(BattleTempAttackBuffCommand{
            transaction.defender.id,
            event.effect.value,
            event.effect.duration,
            std::format("護盾爆炸（臨時攻+{}，{}幀）", event.effect.value, event.effect.duration),
        });
        break;
    case EffectType::MPRestore:
    {
        const int restored = std::min(
            event.effect.value,
            std::max(0, transaction.defender.vitals.maxMp - transaction.defender.vitals.mp));
        if (restored > 0)
        {
            frame.frameCommands.push_back(BattleMpRestoreCommand{
                transaction.defender.id,
                restored,
                std::format("護盾爆炸·回內力+{}", restored),
            });
        }
        else
        {
            activated = false;
        }
        break;
    }
    default:
        assert(false);
    }
    if (activated)
    {
        BattleComboTriggerSystem().recordActivation(defenderCombo, static_cast<size_t>(event.effectIndex));
    }
}
```

- [ ] **Step 4: Rewrite stun and on-hit command collection**

In `src/battle/BattleHitResolver.cpp`, replace `collectStunCommands(...)` calls with `collectTriggerEvents(...)` and handle `EffectType::Stun` directly:

```cpp
auto hitStunEvents = BattleComboTriggerSystem().collectTriggerEvents(
    attackerCombo,
    { BattleComboTriggerHook::DamageDealt, input.attacker.id, input.defender.id },
    { KysChess::EffectType::Stun },
    random,
    BattleComboActivationRecording::CallerRecords);
for (const auto& event : hitStunEvents)
{
    BattleDamageRequest request;
    request.frozenFrames = event.effect.value;
    result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
    result.logEvents.push_back(statusEvent(
        input.attacker.id,
        input.defender.id,
        logStatusFrames("眩暈", event.effect.value)));
    BattleComboTriggerSystem().recordActivation(attackerCombo, static_cast<size_t>(event.effectIndex));
}
```

Use the same pattern for being-hit stun, with log text `反制並眩暈對手`.

- [ ] **Step 5: Delete specialized command APIs**

In `src/battle/BattleComboTriggerSystem.h`, delete:

```cpp
enum class BattleDefenderBlockCommand;
struct BattleStunCommand;
enum class BattleOnHitComboCommandType;
enum class BattleShieldBreakCommandType;
struct BattleOnHitComboCommand;
struct BattleShieldBreakCommand;
std::vector<BattleOnHitComboCommand> collectOnHitComboCommands(...);
std::vector<BattleShieldBreakCommand> collectShieldBreakCommands(...);
std::vector<BattleDefenderBlockCommand> collectDefenderBlockCommands(...);
std::vector<BattleStunCommand> collectStunCommands(...);
```

Delete corresponding implementations from `src/battle/BattleComboTriggerSystem.cpp`.

- [ ] **Step 6: Run deletion search**

Run:

```powershell
rg -n "BattleOnHitComboCommand|BattleShieldBreakCommand|BattleStunCommand|collectOnHitComboCommands|collectShieldBreakCommands|collectStunCommands" src tests
```

Expected: no matches.

- [ ] **Step 7: Run focused tests**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "[battle][combo]" "[battle][hit]" "BattleFrameRunner_AdvanceFrame_ReducesTempAttackBuffInsideCore"
```

Expected: tests pass.

- [ ] **Step 8: Commit**

Run:

```powershell
git add src\battle\BattleComboTriggerSystem.* src\battle\BattleCore.cpp src\battle\BattleHitResolver.cpp tests
git commit -m "refactor: remove specialized combo command APIs"
```

Expected: commit succeeds.

---

## Task 6: Migrate Hit Passive Fields To Configured Effects

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleComboTriggerSystemUnitTests.cpp`

- [ ] **Step 1: Convert hit tests off scalar combo fields**

Search:

```powershell
rg -n "\.(mpOnHit|hpOnHit|mpDrain|poisonDOTPct|poisonDuration|knockbackChancePct|offensiveCharmChancePct|charmCDRChancePct|charmCDRAmountPct|projectileReflectPct|nearbyTrackingProjectileRange|dmgReduceDebuffChancePct|dmgReduceDebuffDurationFrames|dmgReduceDebuffPct|skillReflectPct|skillDmgPct|mpRatioDmgBoostPct)\s*=" tests
```

For each match, replace direct field setup with `ChessBattleEffects::applyEffect(...)`.

Use these mappings:

```cpp
ChessBattleEffects::applyEffect(combo, { EffectType::MPOnHit, amount });
ChessBattleEffects::applyEffect(combo, { EffectType::HPOnHit, amount });
ChessBattleEffects::applyEffect(combo, { EffectType::MPDrain, amount });
ChessBattleEffects::applyEffect(combo, { EffectType::PoisonDOT, pct, durationSeconds });
ChessBattleEffects::applyEffect(combo, { EffectType::KnockbackChance, chancePct });
ChessBattleEffects::applyEffect(combo, { EffectType::OffensiveCharm, chancePct });
ChessBattleEffects::applyEffect(combo, { EffectType::CharmCDRDebuff, chancePct, extendPct });
ChessBattleEffects::applyEffect(combo, { EffectType::ProjectileReflect, chancePct });
ChessBattleEffects::applyEffect(combo, { EffectType::NearbyTrackingProjectiles, rangePixels, damagePct, "", Trigger::OnHit, chancePct });
ChessBattleEffects::applyEffect(combo, { EffectType::DmgReduceDebuff, pct, durationFrames });
ChessBattleEffects::applyEffect(combo, { EffectType::SkillReflectPct, pct });
ChessBattleEffects::applyEffect(combo, { EffectType::SkillDmgPct, pct });
ChessBattleEffects::applyEffect(combo, { EffectType::MPRatioDmgBoost, pct });
```

- [ ] **Step 2: Update hit resolver reads to use query helpers or trigger events**

In `src/battle/BattleHitResolver.cpp`, use query helpers for always effects:

```cpp
const int mpOnHit = sumAlwaysEffectValue(attackerCombo, EffectType::MPOnHit);
const int hpOnHit = sumAlwaysEffectValue(attackerCombo, EffectType::HPOnHit);
const int mpDrain = sumAlwaysEffectValue(attackerCombo, EffectType::MPDrain);
const int poisonPct = sumAlwaysEffectValue(attackerCombo, EffectType::PoisonDOT);
const int poisonDuration = firstAlwaysEffect(attackerCombo, EffectType::PoisonDOT)
    ? firstAlwaysEffect(attackerCombo, EffectType::PoisonDOT)->value2 * 30
    : 0;
```

Use `collectTriggerEvents(...)` for chance/hook effects such as `MPBlock`, `NearbyTrackingProjectiles`, `DmgReduceDebuff`, `Execute`, `ProjectileBounce`, `ArmorPen`, and `UltimateExtraProjectiles`.

- [ ] **Step 3: Move passive damage modifier reads**

In `makeBattleDamageModifierState(...)` and related calls, replace scalar fields with effect queries:

```cpp
modifier.flatDamageIncrease = sumAlwaysEffectValue(*state, EffectType::FlatDmgIncrease);
modifier.skillDamagePct = sumAlwaysEffectValue(*state, EffectType::SkillDmgPct);
modifier.poisonDamageAmpPct = sumAlwaysEffectValue(*state, EffectType::PoisonDmgAmp);
modifier.flatDamageReduction = sumAlwaysEffectValue(*state, EffectType::FlatDmgReduction);
modifier.damageReductionPct = sumAlwaysEffectValue(*state, EffectType::DmgReductionPct);
modifier.maxHitPctMaxHp = maxAlwaysEffectValue(*state, EffectType::MaxHitPctCurrentHP);
```

- [ ] **Step 4: Stop materializing migrated hit fields**

In `src/ChessBattleEffects.cpp`, change the migrated hit-effect `case` bodies to `break;`.

The set for this task is:

```cpp
FlatDmgReduction, FlatDmgIncrease, DodgeChance, DodgeThenCrit, CritChance,
CritMultiplier, EveryNthDouble, ArmorPenChance, ArmorPenPct, KnockbackChance,
PoisonDOT, PoisonDmgAmp, MPOnHit, HPOnHit, MPDrain, SkillDmgPct, SkillReflectPct,
DmgReductionPct, BleedChance, ProjectileReflect, ProjectileBounce, Execute,
MPBlock, CharmCDRDebuff, OffensiveCharm, UltimateExtraProjectiles,
MPRatioDmgBoost, DmgReduceDebuff, NearbyTrackingProjectiles,
CounterUltimateBlock, MaxHitPctCurrentHP
```

If an effect still needs mutable per-effect state, store it in existing maps/vectors keyed by effect index. Do not add a new named scalar field.

- [ ] **Step 5: Delete migrated fields from `RoleComboState`**

Delete the corresponding scalar members from `src/ChessBattleEffects.h`. Keep `everyNthCounters`, `dodgedLast`, `effectActivationCounts`, `adaptationStacks`, `dodgeAdaptationStacks`, `rampingStacks`, and `rampingIdleTimers` until their effect-index migration is explicitly complete.

- [ ] **Step 6: Run focused tests**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "[battle][hit]" "[battle][combo]" "BattleFrameRunner_AdvanceFrame_DodgeConsumesHitBeforeDamage" "BattleFrameRunner_AdvanceFrame_ResolvesHitEventsWithFrameHitInputs"
```

Expected: tests pass.

- [ ] **Step 7: Commit**

Run:

```powershell
git add src\ChessBattleEffects.* src\battle\BattleHitResolver.cpp src\battle\BattleComboTriggerSystem.* src\battle\BattleCore.cpp tests
git commit -m "refactor: read hit combo behavior from configured effects"
```

Expected: commit succeeds.

---

## Task 7: Migrate Cast, Action, And Projectile Style Fields

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleCastSystem.cpp`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Modify: `tests/BattleCastSystemUnitTests.cpp`
- Modify: `tests/BattleActionCommitUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Convert tests off direct cast/action fields**

Search:

```powershell
rg -n "\.(postSkillInvincFrames|postSkillDash|postSkillDashFrames|blinkAttack|ultimateExtraProjectiles|forceRangedAttack|forceRangedMinSelectDistance|projectileSpeedMultiplierPct|ignoreProjectileCancel|dashAttack|dashChanceBoostPct)\s*=" tests src\battle
```

For test setup, replace assignments with `ChessBattleEffects::applyEffect(...)` using:

```cpp
ChessBattleEffects::applyEffect(combo, { EffectType::PostSkillInvincFrames, frames });
ChessBattleEffects::applyEffect(combo, { EffectType::PostSkillDash, frames });
ChessBattleEffects::applyEffect(combo, { EffectType::BlinkAttack, 1 });
ChessBattleEffects::applyEffect(combo, { EffectType::UltimateExtraProjectiles, count, 0, "", Trigger::OnUltimate, chancePct });
ChessBattleEffects::applyEffect(combo, { EffectType::ForceRangedAttack, projectileSpeedPct, minSelectDistance });
ChessBattleEffects::applyEffect(combo, { EffectType::DashAttack, 1 });
ChessBattleEffects::applyEffect(combo, { EffectType::DashChanceBoost, pct });
```

- [ ] **Step 2: Update runtime action planning reads**

In `BattleCore.cpp` helper functions such as `runtimeRoleForcesRangedMagic(...)`, `runtimeForcedRangedMinSelectDistance(...)`, `runtimeProjectileSpeedMultiplierPct(...)`, and dash/blink helpers, replace scalar reads with effect queries:

```cpp
const auto* forceRanged = firstAlwaysEffect(combo, EffectType::ForceRangedAttack);
const bool forceRangedAttack = forceRanged != nullptr;
const int minSelectDistance = forceRanged && forceRanged->value2 > 0 ? forceRanged->value2 : 0;
const int projectileSpeedPct = forceRanged && forceRanged->value > 0 ? forceRanged->value : 100;
```

For `BlinkAttack`, `DashAttack`, and `DashChanceBoost`, use `firstAlwaysEffect(...)` or `sumAlwaysEffectValue(...)`.

- [ ] **Step 3: Update post-skill effects**

Replace `combo.postSkillInvincFrames`, `combo.postSkillDash`, and `combo.postSkillDashFrames` reads with configured effect queries:

```cpp
const int postSkillInvincFrames = maxAlwaysEffectValue(combo, EffectType::PostSkillInvincFrames);
const int postSkillDashFrames = maxAlwaysEffectValue(combo, EffectType::PostSkillDash);
```

- [ ] **Step 4: Stop materializing migrated action fields**

In `src/ChessBattleEffects.cpp`, change these cases to `break;`:

```cpp
case EffectType::PostSkillInvincFrames: break;
case EffectType::PostSkillDash: break;
case EffectType::BlinkAttack: break;
case EffectType::UltimateExtraProjectiles: break;
case EffectType::ForceRangedAttack: break;
case EffectType::DashAttack: break;
case EffectType::DashChanceBoost: break;
```

- [ ] **Step 5: Delete migrated action fields**

In `src/ChessBattleEffects.h`, delete:

```cpp
int postSkillInvincFrames = 0;
bool postSkillDash = false;
int postSkillDashFrames = 0;
bool blinkAttack = false;
int ultimateExtraProjectiles = 0;
bool forceRangedAttack = false;
int forceRangedMinSelectDistance = 0;
int projectileSpeedMultiplierPct = 100;
bool ignoreProjectileCancel = false;
bool dashAttack = false;
int dashChanceBoostPct = 0;
```

Keep `postSkillDashPending` and `blinkAttackUseWeakest` until the pending-action state owner is clarified.

- [ ] **Step 6: Run focused tests**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "[battle][cast]" "[battle][action]" "BattleFrameRunner_AdvanceFrame_AppliesProjectileCancelDamageCommand"
```

Expected: tests pass.

- [ ] **Step 7: Commit**

Run:

```powershell
git add src\ChessBattleEffects.* src\battle\BattleCore.cpp src\battle\BattleCastSystem.cpp src\battle\BattleComboTriggerSystem.* tests
git commit -m "refactor: read action combo behavior from configured effects"
```

Expected: commit succeeds.

---

## Task 8: Migrate Lifecycle, Death, Clone, Rescue, And Economy Fields

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/battle/BattleDeathEffectSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `tests/BattleDeathEffectSystemUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleRuntimeScenarioUnitTests.cpp`

- [ ] **Step 1: Convert lifecycle tests off direct fields**

Search:

```powershell
rg -n "\.(cloneSummonCount|deathAOEPct|deathAOEStunFrames|deathAOEMaxTargets|enemyTopDebuffCount|enemyTopDebuffValue|enemyTopDebuffApplied|forcePullProtect|forcePullExecute|forcePullProtectCharges|forcePullExecuteCharges|shieldOnAllyDeathTracker|goldCoefficient)\s*=" tests src\battle
```

For test setup, replace direct config fields with `ChessBattleEffects::applyEffect(...)`:

```cpp
ChessBattleEffects::applyEffect(combo, { EffectType::CloneSummon, count });
ChessBattleEffects::applyEffect(combo, { EffectType::DeathAOE, damagePct, maxTargets, "", Trigger::Always, 0, stunFrames });
ChessBattleEffects::applyEffect(combo, { EffectType::EnemyTopDebuff, targetCount, debuffValue });
ChessBattleEffects::applyEffect(combo, { EffectType::ForcePullProtect, charges });
ChessBattleEffects::applyEffect(combo, { EffectType::ForcePullExecute, charges });
ChessBattleEffects::applyEffect(combo, { EffectType::ShieldOnAllyDeath, value });
ChessBattleEffects::applyEffect(combo, { EffectType::GoldCoefficient, value });
```

- [ ] **Step 2: Update clone/debuff initialization**

In `src/battle/BattleInitialization.cpp`, derive clone count and enemy top debuff from effects:

```cpp
const int cloneCount = maxAlwaysEffectValue(combo, EffectType::CloneSummon);
const auto* topDebuff = firstAlwaysEffect(combo, EffectType::EnemyTopDebuff);
const int topTargets = topDebuff ? topDebuff->value : 0;
const int perMemberValue = topDebuff ? topDebuff->value2 : 0;
```

Keep `enemyTopDebuffApplied` as mutable runtime state until the debuff application accounting is moved into a keyed runtime-state map.

- [ ] **Step 3: Update death AOE and death-effect store derivation**

In `src/battle/BattleRuntimeSession.cpp`, replace `deathAOEPct` reads in `refreshDeathAoeDamageEffects(...)` with:

```cpp
const auto* deathAoe = firstAlwaysEffect(stateIt->second, EffectType::DeathAOE);
if (deathAoe && deathAoe->value > 0)
{
    runtime.damage.unitEffects.emplace(
        unit.id,
        BattleDamageApplicationUnitEffects{
            deathAoe->value,
            deathAoe->duration,
            deathAoe->value2,
        });
}
```

- [ ] **Step 4: Update rescue charge reads**

Replace `forcePullProtect` / `forcePullExecute` config reads with effect queries. Keep `forcePullProtectRemaining` and `forcePullExecuteRemaining` as mutable runtime counters if they are still decremented during battle.

- [ ] **Step 5: Stop materializing migrated lifecycle fields**

In `src/ChessBattleEffects.cpp`, change these cases to `break;`:

```cpp
case EffectType::CloneSummon: break;
case EffectType::EnemyTopDebuff: break;
case EffectType::DeathAOE: break;
case EffectType::ForcePullProtect: break;
case EffectType::ForcePullExecute: break;
case EffectType::ShieldOnAllyDeath: break;
case EffectType::GoldCoefficient: break;
```

- [ ] **Step 6: Delete migrated lifecycle fields**

Delete these fields from `src/ChessBattleEffects.h`:

```cpp
int cloneSummonCount = 0;
int deathAOEPct = 0;
int deathAOEStunFrames = 0;
int deathAOEMaxTargets = 0;
int enemyTopDebuffCount = 0;
int enemyTopDebuffValue = 0;
bool forcePullProtect = false;
bool forcePullExecute = false;
int forcePullProtectCharges = 0;
int forcePullExecuteCharges = 0;
int goldCoefficient = 0;
```

Keep these mutable counters until a separate ownership cleanup moves them:

```cpp
int enemyTopDebuffApplied = 0;
int forcePullProtectRemaining = 0;
int forcePullExecuteRemaining = 0;
int shieldOnAllyDeathTracker = 0;
```

- [ ] **Step 7: Run focused tests**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "[battle][initialization]" "[battle][death]" "BattleRuntimeSession_RunFrame_AppliesDeathComboConsequencesBeforeSceneConsumption"
```

Expected: tests pass.

- [ ] **Step 8: Commit**

Run:

```powershell
git add src\ChessBattleEffects.* src\battle\BattleInitialization.cpp src\battle\BattleDeathEffectSystem.cpp src\battle\BattleCore.cpp src\battle\BattleRuntimeSession.cpp tests
git commit -m "refactor: read lifecycle combo behavior from configured effects"
```

Expected: commit succeeds.

---

## Task 9: Shrink `BattleGameplayCommand`

**Files:**
- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Identify combo-only command variants**

Run:

```powershell
rg -n "Battle(CurrentHpBlast|SpiralBleedProjectile|NearbyTrackingProjectiles|HitExtraProjectiles|ShieldExplosion|AutoUltimate|TempAttackBuff|UnitHeal|UnitShield|TeamHeal|TeamMpRestore|TeamShield)Command" src\battle tests
```

Expected: matches in `BattleHitResolver.h`, `BattleHitResolver.cpp`, and `BattleCore.cpp`.

- [ ] **Step 2: Inline effect-only command applications where frame ordering allows**

Move effect application from `reduceFrameGameplayCommand(...)` into the phase that already has the activated effect:

- Cast-release effects apply inside `applyFrameCastScopedComboEffects(...)`.
- Shield-break effects apply inside `appendFrameShieldBreakCommands(...)`.
- Hit accepted side effects stay as `BattleDamageRequest` until damage transaction owns the status/resource mutation.
- Auto ultimate may remain a command if it must cross early/late frame ordering.

- [ ] **Step 3: Delete variants that no longer cross frame phases**

Remove command structs and variant entries only after `rg` shows no producer needs them. Prioritize:

```cpp
BattleCurrentHpBlastCommand
BattleSpiralBleedProjectileCommand
BattleNearbyTrackingProjectilesCommand
BattleHitExtraProjectilesCommand
BattleShieldExplosionCommand
BattleTeamHealCommand
BattleTeamMpRestoreCommand
BattleTeamShieldCommand
BattleUnitHealCommand
BattleUnitShieldCommand
```

Keep `BattleHpDamageCommand`, `BattleMpDamageCommand`, `BattleAcceptedHitSideEffectCommand`, `BattleProjectileSpawnCommand`, `BattleAutoUltimateCommand`, `BattleKnockbackCommand`, `BattleTempAttackBuffCommand`, and `BattleRumbleCommand` if they still cross phase boundaries.

- [ ] **Step 4: Run variant-count check**

Run:

```powershell
(Select-String -Path 'src/battle/BattleHitResolver.h' -Pattern '^\s+Battle[A-Za-z0-9]+Command,?' | Measure-Object).Count
```

Expected: count is lower than the baseline captured in Task 1.

- [ ] **Step 5: Run focused tests**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "[battle][hit]" "[battle][core]"
```

Expected: tests pass.

- [ ] **Step 6: Commit**

Run:

```powershell
git add src\battle\BattleHitResolver.* src\battle\BattleCore.cpp tests
git commit -m "refactor: shrink battle gameplay command surface"
```

Expected: commit succeeds.

---

## Task 10: Final Cleanup And Complexity Proof

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/battle/BattleComboTriggerSystem.h`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: affected tests

- [ ] **Step 1: Remove dead includes and stale helpers**

Run:

```powershell
rg -n "mpOnHit|hpOnHit|mpDrain|flatShield|shieldPctMaxHP|healAuraPct|healAuraFlat|hpRegenPct|deathAOEPct|cloneSummonCount|projectileReflectPct|forceRangedAttack|BattleOnHitComboCommand|BattleShieldBreakCommand|BattleStunCommand" src tests
```

Expected: matches only in config enum names, test names, log text, or explicit effect query code. There should be no `RoleComboState` field declarations or field assignments for deleted names.

- [ ] **Step 2: Re-run baseline metrics**

Run:

```powershell
@{
  RoleComboStateFieldLines = (Select-String -Path 'src/ChessBattleEffects.h' -Pattern '^\s+(int|bool|double|std::|struct) ' | Measure-Object).Count
  ComboTriggerStructs = (Select-String -Path 'src/battle/BattleComboTriggerSystem.h' -Pattern '^struct |^enum class ' | Measure-Object).Count
  GameplayCommandVariants = (Select-String -Path 'src/battle/BattleHitResolver.h' -Pattern '^\s+Battle[A-Za-z0-9]+Command,?' | Measure-Object).Count
  BattleCoreLines = (Get-Content -LiteralPath 'src/battle/BattleCore.cpp' | Measure-Object -Line).Lines
}
```

Expected: `RoleComboStateFieldLines`, `ComboTriggerStructs`, and `GameplayCommandVariants` are lower than Task 1 baseline. `BattleCoreLines` should not increase materially; if it increases, inspect whether effect logic was duplicated instead of moved/deleted.

- [ ] **Step 3: Run full battle-related tests**

Run:

```powershell
.\build\tests\Debug\kys_tests.exe "[battle]"
```

Expected: battle tests pass.

- [ ] **Step 4: Run project build**

Run:

```powershell
.\.github\build-command.ps1
```

Expected: compile succeeds. Final link failure is acceptable only if the output binary is locked by a running game process.

- [ ] **Step 5: Commit cleanup**

Run:

```powershell
git add src tests
git commit -m "refactor: complete combo effect surface rewrite"
```

Expected: commit succeeds.

---

## Execution Notes

- Prefer one cluster per commit. Do not batch Tasks 3 through 9 into one commit.
- If a field is both configuration and mutable runtime state, split it: configuration comes from `AppliedEffectInstance`; mutable counters remain in a keyed runtime-state structure until a dedicated ownership cleanup moves them.
- If a direct effect query is repeated in more than two runtime locations, add one named helper in `ChessBattleEffects.cpp`; do not create a new command abstraction.
- When converting tests, prefer `ChessBattleEffects::applyEffect(...)` over manually pushing `AppliedEffectInstance` unless the test is specifically about trigger activation internals.
- Keep `EffectType` enum values and parser aliases stable during this plan.

## Plan Self-Review

- Spec coverage: the plan covers config compatibility, compiled field deletion, trigger API shrinkage, command surface shrinkage, tests, and build verification.
- Placeholder scan: no task relies on unspecified behavior; every migration cluster names exact files, fields, commands, and verification commands.
- Type consistency: helper names are introduced in Task 2 and reused consistently as `sumAlwaysEffectValue`, `maxAlwaysEffectValue`, and `firstAlwaysEffect`.
