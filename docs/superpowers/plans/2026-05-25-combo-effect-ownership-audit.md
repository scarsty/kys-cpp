# Combo Effect Runtime Ownership Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make combo-effect ownership explicit so configured effects, resolved runtime payloads, and mutable runtime state cannot be confused by similarly named scalar fields.

**Architecture:** `RoleComboState` should stop storing runtime-effect configuration as named scalars. Configured behavior comes from `AppliedEffectInstance` query helpers; per-hit/per-frame values live in request/snapshot DTOs; persistent counters/timers live in the subsystem runtime store that already advances them. Each source change must either reduce duplicated state or keep code size roughly flat while removing a real source-of-truth problem; do not introduce categorization-only wrappers.

**Tech Stack:** C++23, Catch2, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Ownership Rules

- **Configured effect data:** `AppliedEffectInstance` in `RoleComboState::appliedEffects` / `triggeredEffects`.
- **Pre-battle stat aggregate:** retained owner for stat fields such as `flatHP`, `pctHP`, and fight-win growth; these are outside this runtime-effect ownership cleanup.
- **Resolved payload:** per-hit/per-frame structs such as `BattleDamageRequest`, `BattleOnHitResourceInput`, and `BattleRescueUnitSnapshot`.
- **Persistent runtime state:** subsystem-owned stores such as `BattleDamageRuntimeUnit`, `BattleStatusRuntimeUnit`, `BattleDeathEffectStore`, and rescue runtime state only when it replaces combo-owned counters.
- **Combo-trigger runtime state:** may stay in `RoleComboState` when that object is the actual owner and there is no existing subsystem store that would reduce copying.
- **Conversion boundary:** query configured effects once, preserve paired values from one effect instance when `value`, `value2`, or `duration` belong together.
- **No compatibility layers:** do not add write-through fields, duplicate old/new state, facade aliases, or staged migration artifacts. A task may be large, but the committed state must have one owner for each value.
- **No abstraction-only refactors:** adding a struct, store, or wrapper is allowed only if it deletes an equal or larger amount of scattered code/state or removes a concrete synchronization path.

## Current Risk Areas

- `RoleComboState` still has compiled config scalars: `blockChancePct`, `stunChancePct`, `stunFrames`, `cdrPct`, `onSkillTeamHeal`, `onSkillTeamHealPct`.
- `RoleComboState` still has mutable counters that already have better subsystem owners: `deathPreventionUsed`, `forcePullProtectRemaining`, `forcePullExecuteRemaining`, `shieldOnAllyDeathTracker`.
- `RoleComboState` also has combo-trigger runtime state that is internally coherent: `dodgedLast`, `lastAliveFlag`, trigger/effect timer maps, adaptation stacks, pending flags, clone/blink flags. These are not automatically a problem; moving them needs a stronger reason than naming.
- DTO fields with similar names are usually valid and should not be deleted blindly: `BattleDamageRequest::mpOnHit`, `BattleStatusEffectState::damageImmunityTimer`, `BattleRescueUnitSnapshot::forcePullProtect`, etc.

---

## Task 1: Capture Ownership Baseline And Guardrails

**Files:**
- Inspect: `src/ChessBattleEffects.h`
- Inspect: `src/ChessBattleEffects.cpp`
- Inspect: `src/battle/BattleDamageSystem.h`
- Inspect: `src/battle/BattleStatusSystem.h`
- Inspect: `src/battle/BattleRescueRepositionSystem.h`
- Inspect: `src/battle/BattleDeathEffectSystem.h`

- [ ] **Step 1: Capture remaining `RoleComboState` scalar fields**

Run:

```powershell
rg -n "^\s+(int|bool|double|std::vector|std::map|struct) " src\ChessBattleEffects.h
```

Expected: output includes all remaining `RoleComboState` fields and nested structs.

- [ ] **Step 2: Capture `applyEffect(...)` materialized fields**

Run:

```powershell
rg -n "case EffectType::|s\." src\ChessBattleEffects.cpp
```

Expected: output shows which effects still write named `RoleComboState` fields.

- [ ] **Step 3: Capture similarly named runtime DTO fields**

Run:

```powershell
rg -n "mpOnHit|hpOnHit|mpDrain|poisonPct|damageImmunity|forcePullProtect|forcePullExecute|deathPrevention|blockFirstHits|hurtInvinc|killHeal|killInvinc|bloodlust|shieldOnAllyDeath" src\battle tests
```

Expected: matches in subsystem DTOs and tests. Do not treat these matches as failures yet; classify them by ownership.

- [ ] **Step 4: Record the baseline in implementation notes**

Add a short note to the branch log or commit message body, not a generated metrics file:

```text
Ownership baseline:
- RoleComboState config scalars to remove: blockChancePct, stunChancePct, stunFrames, cdrPct, onSkillTeamHeal, onSkillTeamHealPct
- RoleComboState subsystem runtime counters to move: deathPreventionUsed, forcePullProtectRemaining, forcePullExecuteRemaining, shieldOnAllyDeathTracker
- RoleComboState combo-trigger runtime fields to audit, not automatically move: everyNthCounters, dodgedLast, lastAliveFlag, triggerTimers, effectActivationCounts, effectFrameTimers, adaptation/ramping stacks, onSkillTeamHealPending, postSkillDashPending, isSummonedClone, blinkAttackUseWeakest, enemyTopDebuffApplied
- Starting size baseline at plan time: src/ChessBattleEffects.h = 208 lines; src/battle/BattleCore.cpp = 5213 lines.
- Required net deletion baseline: remove the 10 listed config scalars/subsystem counters before accepting any optional combo-trigger runtime move.
```

- [ ] **Step 5: Commit only if files changed**

No source edits are expected in this task. If documentation notes were added, commit them separately; otherwise do not commit.

---

## Task 2: Remove Remaining Config-Owned Scalars From `RoleComboState`

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Modify: `src/battle/BattleCastSystem.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `tests/BattleComboTriggerSystemUnitTests.cpp`
- Modify: `tests/BattleCastSystemUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add failing tests for CDR from configured effects**

In `tests/BattleCastSystemUnitTests.cpp`, add or update a cooldown test so setup uses:

```cpp
KysChess::RoleComboState combo;
KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::CDR, 20 });
```

Expected assertion: the cast cooldown is reduced by 20% through the normal `BattleCore`/cast planning path, without assigning `combo.cdrPct`.

- [ ] **Step 2: Add failing tests for Always stun from configured effects**

In `tests/BattleHitResolverUnitTests.cpp`, replace direct setup:

```cpp
input.attackerCombo.stunChancePct = 100;
input.attackerCombo.stunFrames = 7;
```

with:

```cpp
KysChess::ChessBattleEffects::applyEffect(
    input.attackerCombo,
    { KysChess::EffectType::Stun, 7, 0, "", KysChess::Trigger::Always, 100 });
```

Expected assertion: the same accepted-hit side-effect stun is produced for main offensive hits and not for suppressed side projectiles.

- [ ] **Step 3: Add failing tests for pending skill-finished team heal from configured effects**

In `tests/BattleComboTriggerSystemUnitTests.cpp`, update pending heal setup from scalar fields:

```cpp
state.onSkillTeamHealPending = true;
state.onSkillTeamHeal = 5;
state.onSkillTeamHealPct = 7;
```

to configured effects:

```cpp
state.onSkillTeamHealPending = true;
ChessBattleEffects::applyEffect(state, { EffectType::OnSkillTeamHeal, 5 });
ChessBattleEffects::applyEffect(state, { EffectType::OnSkillTeamHealPct, 7 });
```

Expected assertion: `collectPendingSkillTeamHeal(...)` returns flat heal `5`, percent heal `7`, and clears only the pending flag.

- [ ] **Step 4: Run focused tests and confirm red state**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][hit_resolver]"
.\x64\Debug\kys_tests.exe "[battle][combo]"
.\x64\Debug\kys_tests.exe "[battle][cast]"
```

Expected: build or tests fail because production still reads removed/direct scalar setup paths.

- [ ] **Step 5: Migrate CDR reads to effect queries**

In `src/battle/BattleCore.cpp`, replace each `combo.cdrPct` read with:

```cpp
sumAlwaysEffectValue(combo, EffectType::CDR)
```

For code paths using an iterator:

```cpp
input.unit.cooldownReductionPct = sumAlwaysEffectValue(comboIt->second, EffectType::CDR);
```

- [ ] **Step 6: Migrate Always stun scalar reads to effect queries**

In `src/battle/BattleHitResolver.cpp`, replace the `attackerCombo.stunChancePct` / `stunFrames` block with a scan of `appliedEffects`:

```cpp
int alwaysStunFrames = 0;
if (offensiveControlEffectsAllowed)
{
    for (const auto& effect : attackerCombo.appliedEffects)
    {
        if (effect.type != EffectType::Stun || effect.trigger != Trigger::Always)
        {
            continue;
        }
        const int chancePct = effect.triggerValue > 0 ? effect.triggerValue : 100;
        if (chancePct > 0 && random.chance(chancePct))
        {
            alwaysStunFrames = std::max(alwaysStunFrames, effect.value);
        }
    }
}
```

Keep triggered `Stun` handling unchanged.

- [ ] **Step 7: Migrate pending skill-finished team heal values**

In `src/battle/BattleComboTriggerSystem.cpp`, update `collectPendingSkillTeamHeal(...)`:

```cpp
result.flatHeal = sumAlwaysEffectValue(state, EffectType::OnSkillTeamHeal);
result.pctHeal = sumAlwaysEffectValue(state, EffectType::OnSkillTeamHealPct);
```

Then add triggered heal values as it already does.

- [ ] **Step 8: Stop materializing deleted fields in `applyEffect(...)`**

In `src/ChessBattleEffects.cpp`, change these cases to `break;`:

```cpp
case EffectType::BlockChance: break;
case EffectType::CDR: break;
case EffectType::OnSkillTeamHeal: break;
case EffectType::OnSkillTeamHealPct: break;
```

`Stun` already should be stored only as an effect instance.

- [ ] **Step 9: Delete config scalars from `RoleComboState`**

Delete these members from `src/ChessBattleEffects.h`:

```cpp
int blockChancePct = 0;
int stunChancePct = 0;
int stunFrames = 0;
int cdrPct = 0;
int onSkillTeamHeal = 0;
int onSkillTeamHealPct = 0;
```

- [ ] **Step 10: Run focused tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][hit_resolver]"
.\x64\Debug\kys_tests.exe "[battle][combo]"
.\x64\Debug\kys_tests.exe "[battle][cast]"
.\x64\Debug\kys_tests.exe "[battle][core]"
```

Expected: tests pass.

- [ ] **Step 11: Commit**

Run:

```powershell
git add src\ChessBattleEffects.* src\battle tests
git commit -m "refactor: remove remaining combo config scalars"
```

---

## Task 3: Move Damage-Owned Runtime Flags Out Of `RoleComboState`

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `tests/BattleDamageSystemUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a regression test proving death prevention used state belongs to damage runtime**

In `tests/BattleCoreUnitTests.cpp`, add or update a test so the setup sets:

```cpp
auto& runtime = requireById(state.damage.unitExtras, defenderId);
runtime.deathPrevention = true;
runtime.deathPreventionFrames = 30;
runtime.deathPreventionUsed = true;
```

Expected assertion: a lethal hit does not trigger death prevention a second time.

- [ ] **Step 2: Remove combo-to-damage copy of `deathPreventionUsed`**

In `src/battle/BattleRuntimeUnitSpawn.cpp`, remove:

```cpp
damage.deathPreventionUsed = combo.deathPreventionUsed;
```

The initial `BattleDamageRuntimeUnit` default already starts as unused.

- [ ] **Step 3: Delete `deathPreventionUsed` from `RoleComboState`**

Delete from `src/ChessBattleEffects.h`:

```cpp
bool deathPreventionUsed = false;
```

- [ ] **Step 4: Run focused tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][damage]"
.\x64\Debug\kys_tests.exe "[battle][core]"
.\x64\Debug\kys_tests.exe "[battle][initialization]"
```

Expected: tests pass.

- [ ] **Step 5: Commit**

Run:

```powershell
git add src\ChessBattleEffects.h src\battle\BattleRuntimeUnitSpawn.cpp tests
git commit -m "refactor: keep death prevention usage in damage runtime"
```

---

## Task 4: Move Rescue Charge Counters To Rescue Runtime State

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/ChessBattleEffects.h`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleRuntimeScenarioUnitTests.cpp`

- [ ] **Step 1: Add rescue runtime state**

In `src/battle/BattleCore.h`, extend `BattleRuntimeState::RescueState`:

```cpp
struct RescueUnitRuntime
{
    int unitId = -1;
    int forcePullProtectRemaining = 0;
    int forcePullExecuteRemaining = 0;
};

std::vector<RescueUnitRuntime> units;
```

- [ ] **Step 2: Initialize rescue counters from configured effects**

In `src/battle/BattleInitialization.cpp`, when building runtime state, populate `state.rescue.units` from combo effects:

```cpp
BattleRuntimeState::RescueState::RescueUnitRuntime rescue;
rescue.unitId = spawn.unit.id;
rescue.forcePullProtectRemaining = sumAlwaysEffectCharges(spawn.combo, EffectType::ForcePullProtect);
rescue.forcePullExecuteRemaining = sumAlwaysEffectCharges(spawn.combo, EffectType::ForcePullExecute);
runtime.rescue.units.push_back(rescue);
```

Use the actual local runtime construction point in `BattleInitializationSystem::initialize(...)`.

- [ ] **Step 3: Build rescue snapshots from rescue runtime state**

In `src/battle/BattleCore.cpp`, update `makeRescueUnitSnapshots(...)` so:

```cpp
const auto* rescueRuntime = findById(state.rescue.units, unit.id);
snapshot.unit.forcePullProtectRemaining = rescueRuntime ? rescueRuntime->forcePullProtectRemaining : 0;
snapshot.unit.forcePullExecuteRemaining = rescueRuntime ? rescueRuntime->forcePullExecuteRemaining : 0;
```

Keep `forcePullProtect` / `forcePullExecute` booleans derived from effect queries.

- [ ] **Step 4: Commit rescue counter deltas to rescue runtime state**

In `src/battle/BattleCore.cpp`, replace writes to `state.combo.units[*].forcePullProtectRemaining` / `forcePullExecuteRemaining` with writes to `state.rescue.units`.

- [ ] **Step 5: Delete rescue counters from `RoleComboState`**

Delete from `src/ChessBattleEffects.h`:

```cpp
int forcePullProtectRemaining = 0;
int forcePullExecuteRemaining = 0;
```

- [ ] **Step 6: Update tests off combo counter setup**

Replace test setup:

```cpp
state.combo.units[2].forcePullProtectRemaining = 1;
```

with rescue runtime setup:

```cpp
state.rescue.units.push_back({ 2, 1, 0 });
```

For execute:

```cpp
state.rescue.units.push_back({ 0, 0, 2 });
```

- [ ] **Step 7: Run focused tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core]"
.\x64\Debug\kys_tests.exe "[battle][runtime_session]"
.\x64\Debug\kys_tests.exe "[battle][rescue]"
```

Expected: tests pass.

- [ ] **Step 8: Commit**

Run:

```powershell
git add src\ChessBattleEffects.h src\battle\BattleCore.* src\battle\BattleInitialization.cpp tests
git commit -m "refactor: keep rescue charges in rescue runtime"
```

---

## Task 5: Move Death-Effect Tracker Fully Into DeathEffectStore

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/battle/BattleDeathEffectSystem.*`
- Modify: `tests/BattleDeathEffectSystemUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a death-effect persistence regression test**

In `tests/BattleDeathEffectSystemUnitTests.cpp`, use `BattleDeathEffectStore` directly:

```cpp
auto& ally = extrasById(effects, 1);
ally.shieldOnAllyDeathTracker = 1;
```

Expected assertion: the second qualifying ally death grants shield and resets `ally.shieldOnAllyDeathTracker` to `0`.

- [ ] **Step 2: Remove combo-to-death tracker copy**

In `src/battle/BattleRuntimeSession.cpp`, remove:

```cpp
extras.shieldOnAllyDeathTracker = stateIt->second.shieldOnAllyDeathTracker;
```

`BattleDeathEffectStore` is the persistent owner.

- [ ] **Step 3: Delete tracker from `RoleComboState`**

Delete from `src/ChessBattleEffects.h`:

```cpp
int shieldOnAllyDeathTracker = 0;
```

- [ ] **Step 4: Run focused tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][death]"
.\x64\Debug\kys_tests.exe "[battle][core]"
```

Expected: tests pass.

- [ ] **Step 5: Commit**

Run:

```powershell
git add src\ChessBattleEffects.h src\battle\BattleRuntimeSession.cpp src\battle\BattleDeathEffectSystem.* tests
git commit -m "refactor: keep ally death shield tracker in death runtime"
```

---

## Task 6: Audit Combo-Trigger Runtime State Without Adding Abstractions

**Files:**
- Inspect: `src/ChessBattleEffects.h`
- Inspect: `src/ChessBattleEffects.cpp`
- Inspect: `src/battle/BattleComboTriggerSystem.*`
- Inspect: `src/battle/BattleCastSystem.cpp`
- Inspect: `src/battle/BattleCore.cpp`
- Modify: only files where the audit finds an existing subsystem owner that removes code/state

- [ ] **Step 1: Capture current combo-trigger runtime field usage**

Run:

```powershell
rg -n "everyNthCounters|dodgedLast|triggerTimers|lastAliveFlag|effectActivationCounts|effectFrameTimers|adaptationStacks|dodgeAdaptationStacks|rampingStacks|rampingIdleTimers|enemyTopDebuffApplied|onSkillTeamHealPending|postSkillDashPending|isSummonedClone|blinkAttackUseWeakest" src\ChessBattleEffects.* src\battle tests
```

Expected: output shows call sites clustered in `BattleComboTriggerSystem`, `BattleCastSystem`, `BattleCore`, initialization, and tests.

- [ ] **Step 2: Classify each field as keep-or-move**

Use this exact decision table:

```text
Keep in RoleComboState:
- everyNthCounters: keyed activation state for combo effect evaluation.
- dodgedLast: consumed by DodgeThenCrit inside combo trigger evaluation.
- triggerTimers: combo trigger timers.
- lastAliveFlag: frame-derived combo trigger state.
- effectActivationCounts: maxCount enforcement for configured effects.
- effectFrameTimers: configured periodic effect timers.
- adaptationStacks, dodgeAdaptationStacks, rampingStacks, rampingIdleTimers: per-effect combo trigger state indexed beside configured effect instances.
- enemyTopDebuffApplied: combo debuff accounting with no existing status/debuff store for the aggregate delta.
- onSkillTeamHealPending: skill-finished combo trigger latch.
- postSkillDashPending: action combo latch.
- blinkAttackUseWeakest: alternating blink target selection state.

Move only if already reduced elsewhere:
- isSummonedClone: move to BattleRuntimeUnit only if that deletes the combo field and removes snapshot copying without adding a new store.
```

Expected: most fields stay where they are. This is intentional: moving them into a nested struct would add churn without reducing state.

- [ ] **Step 3: Evaluate `isSummonedClone` only if it is a net reduction**

Before editing, run:

```powershell
rg -n "isSummonedClone|cloneSourceUnitId" src\battle src\ChessBattleEffects.* tests
```

Move `isSummonedClone` from `RoleComboState` to `BattleRuntimeUnit` only if all conditions are true:

```text
- BattleRuntimeUnit already has enough clone identity data or needs only one existing bool.
- Rescue snapshot can read clone status from BattleRuntimeUnit instead of combo.
- getComboLookupId can use BattleRuntimeUnit clone identity without adding another map lookup.
- The diff deletes at least as many lines/fields as it adds.
```

If any condition is false, leave `isSummonedClone` in `RoleComboState` and document it as combo lookup/runtime metadata.

- [ ] **Step 4: Do not add `RoleComboRuntimeState`**

Do not add:

```cpp
struct RoleComboRuntimeState
{
    // existing RoleComboState fields moved under runtime
};
```

Expected: no code changes are made for naming-only grouping.

- [ ] **Step 5: Run focused tests if code changed**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][combo]"
.\x64\Debug\kys_tests.exe "[battle][action]"
.\x64\Debug\kys_tests.exe "[battle][core]"
.\x64\Debug\kys_tests.exe "[battle][initialization]"
```

Expected: tests pass. If Task 6 made no source changes, skip this step and record that no abstraction-only change was made.

- [ ] **Step 6: Commit only if source changed**

Run:

```powershell
git status --short
```

If and only if a net-reducing source change was made:

```powershell
git add src tests
git commit -m "refactor: trim combo trigger runtime ownership"
```

Expected: no commit is required when the audit confirms the existing `RoleComboState` ownership is the least-copying design.

---

## Task 7: Final Ownership Proof

**Files:**
- Inspect: `src/ChessBattleEffects.h`
- Inspect: `src/ChessBattleEffects.cpp`
- Inspect: `src/battle`
- Modify: tests only if stale direct setup remains

- [ ] **Step 1: Verify no deleted config scalars remain**

Run:

```powershell
rg -n "blockChancePct|stunChancePct|stunFrames|cdrPct|onSkillTeamHeal =|onSkillTeamHealPct|deathPreventionUsed|forcePullProtectRemaining|forcePullExecuteRemaining|shieldOnAllyDeathTracker" src\ChessBattleEffects.h src\ChessBattleEffects.cpp src\battle tests
```

Expected: matches only in subsystem DTOs/runtime stores or test names. No deleted config scalar or subsystem-owned counter remains in `RoleComboState`.

- [ ] **Step 2: Verify `RoleComboState` has clear ownership sections**

Run:

```powershell
Get-Content -LiteralPath 'src/ChessBattleEffects.h' | Select-Object -Skip 140 -First 90
```

Expected: `RoleComboState` shows configured effects, retained stat aggregate fields, and audited combo-trigger runtime fields that remain there because `RoleComboState` is their least-copying owner. It should not intermix config scalars and subsystem runtime counters.

- [ ] **Step 3: Verify no abstraction-only artifact was introduced**

Run:

```powershell
rg -n "RoleComboRuntimeState|writeThrough|write-through|compatCombo|oldCombo|newCombo|previousCombo|migratedCombo" src tests
```

Expected: no output.

- [ ] **Step 4: Verify source size did not grow materially**

Run:

```powershell
(Get-Content -LiteralPath 'src/ChessBattleEffects.h').Count
(Get-Content -LiteralPath 'src/battle/BattleCore.cpp').Count
rg -n "^\s+(int|bool|double|std::vector|std::map|std::unordered_map|std::optional) " src\ChessBattleEffects.h
```

Expected: `src/ChessBattleEffects.h` is below the 208-line Task 1 baseline, and the `RoleComboState` field list no longer contains the 10 config scalar/subsystem counter fields from Task 1. `src/battle/BattleCore.cpp` stays at or below the 5213-line Task 1 baseline; if it increases, the same ownership cluster must delete more copied state/copy-path code in other touched files than it adds.

- [ ] **Step 5: Run full battle tests**

Run:

```powershell
.\x64\Debug\kys_tests.exe "[battle]"
```

Expected: all battle tests pass.

- [ ] **Step 6: Run project build**

Run:

```powershell
.\.github\build-command.ps1
```

Expected: compile succeeds. Final link failure is acceptable only if caused by a running game process locking the binary.

- [ ] **Step 7: Commit cleanup only if files changed**

Run:

```powershell
git status --short
```

If there are cleanup changes:

```powershell
git add src tests
git commit -m "refactor: prove combo effect ownership boundaries"
```

---

## Execution Notes

- Do not rename valid DTO payload fields just because they resemble old combo fields. Rename only when a field crosses ownership boundaries ambiguously.
- Do not add a new command bus or generic effect operation type.
- Do not add fake boundary checks; use assertions for invariants.
- Keep config compatibility: no changes to `config/chess_*.yaml` are required.
- Commit one ownership cluster at a time.
- Prefer effect queries that preserve paired data from one `AppliedEffectInstance` when values are semantically linked.

## Plan Self-Review

- Spec coverage: the plan covers remaining config scalars, subsystem-owned mutable counters, combo-trigger runtime ownership audit, and proof scans.
- Placeholder scan: no placeholder markers remain.
- Type consistency: new helper/runtime names are introduced before use; rescue runtime state is explicitly scoped under `BattleRuntimeState::RescueState`; no `RoleComboRuntimeState` type is introduced.
