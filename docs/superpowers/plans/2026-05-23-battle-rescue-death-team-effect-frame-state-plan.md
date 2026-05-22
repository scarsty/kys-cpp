# Battle Rescue Death Team Effect Frame State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep rescue, death, and team-effect lifecycle outputs ordered without relying on broad frame-result diagnostic vectors.

**Architecture:** Damage lifecycle remains the ordering owner for rescue, death, battle end, and pending team-effect cleanup. Scene-specific rescue and damage presentation channels stay public until equivalent ordered frame events exist.

**Tech Stack:** C++17, Catch2 unit tests, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Dependency

Run this after the damage/status plan is complete. Rescue and death behavior depends on damage writeback, battle-end emission, and death combo consequences.

## Required Behavior Gates

These tests must stay green:

```text
BattleFrameRunner_AdvanceFrame_RunsProtectRescueInsideDamageLifecycle
BattleFrameRunner_AdvanceFrame_RunsExecuteRescueAndQueuesCounterAttackInsideDamageLifecycle
BattleFrameRunner_AdvanceFrame_DoesNotEmitRescueDeltaWithoutLegalCell
BattleRuntimeSession_RunFrame_AppliesDeathComboConsequencesBeforeSceneConsumption
BattleFrameRunner_DropsPendingTeamEffectWhenSourceDiesBeforeApply
BattleFrameRunner_AdvanceFrame_BattleEndEventEmitsOnce
```

---

## Task 1: Name Rescue Runtime Mutation Boundary

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [x] **Step 1: Rename rescue writeback helper**

Rename:

```cpp
void applyRescueResultToFrameState(
```

to:

```cpp
void commitRescueResultToRuntime(
```

Keep the implementation unchanged.

- [x] **Step 2: Verify naming**

Run:

```powershell
rg -n "applyRescueResultToFrameState|commitRescueResultToRuntime" src/battle/BattleCore.cpp
```

Expected: only `commitRescueResultToRuntime` remains.

Result: only `commitRescueResultToRuntime` remains.

- [x] **Step 3: Run rescue tests and commit**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core][breakthrough]"
git add src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-23-battle-rescue-death-team-effect-frame-state-plan.md
git commit -m "refactor: name rescue runtime commit boundary"
```

Expected: selected tests pass, then one naming-only commit.

Result: after rebuilding `kys_tests`, `x64\Debug\kys_tests.exe "[battle][core][breakthrough]"` passed 128 assertions in 20 test cases.

---

## Task 2: Keep `rescueResults` Public Until Scene Delta Stops Reading It

**Files:**

- Read: `src/BattleSceneFrameDeltaBuilder.cpp`
- Read: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Confirm rescue result consumers**

Run:

```powershell
rg -n "rescueResults" src tests
```

Expected: `BattleSceneFrameDeltaBuilder` still reads `rescueResults`, and tests assert rescue behavior.

- [ ] **Step 2: Add ordered rescue event only if scene replacement is in scope**

Do not remove `rescueResults` in this plan unless a separate scene-delta task first replaces the scene dependency with an ordered `BattleGameplayEvent` or `BattleVisualEvent`. The replacement event must carry the pulled unit id, puller unit id, and teleport position.

---

## Task 3: Narrow Team Effect Diagnostic Channels

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Confirm team-effect consumers**

Run:

```powershell
rg -n "teamEffectEvents|effectCommands" src tests
```

Expected: production use is inside `BattleCore.cpp`; direct external consumers are tests.

- [ ] **Step 2: Rewrite tests to public frame behavior**

For pending team-effect tests, assert canonical HP/MP/cooldown state and `result.frame.logEvents` or `result.frame.visualEvents` instead of `result.teamEffectEvents`.

For effect-command tests, assert command consequences through canonical runtime state and frame logs.

- [ ] **Step 3: Move team/effect diagnostics private**

Add these fields to `BattleFrameContext`:

```cpp
std::vector<BattleTeamEffectEvent> teamEffectEvents;
std::vector<BattleEffectCommand> effectCommands;
```

Remove these fields from `BattleFrameResult` after all production and test consumers are migrated:

```cpp
std::vector<BattleTeamEffectEvent> teamEffectEvents;
std::vector<BattleEffectCommand> effectCommands;
```

Update context-routed helpers to use `frame.teamEffectEvents` and `frame.effectCommands` unless the values are intentionally public.

- [ ] **Step 4: Run tests and commit**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner]"
git add src/battle/BattleCore.h src/battle/BattleCore.cpp tests/BattleCoreUnitTests.cpp tests/BattleFrameRunnerRuntimeUnitTests.cpp docs/superpowers/plans/2026-05-23-battle-rescue-death-team-effect-frame-state-plan.md
git commit -m "refactor: keep team effect diagnostics private to frame"
```

Expected: selected tests pass, then one commit.

---

## Completion Gate

Run:

```powershell
git diff --check
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected: diff check exits 0, all Catch2 tests pass, and MSBuild exits 0.
