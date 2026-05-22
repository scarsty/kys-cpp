# Battle Action Frame State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Narrow action/cast frame state so persistent action ownership is written directly to runtime stores and only scene-facing action effects remain public.

**Architecture:** `BattleRuntimeState::action` owns pending casts and runtime cast plans. `BattleUnitStore` owns action animation state. `BattleFrameContext` may keep one-frame action results; `BattleFrameResult::actionResults` must stay public only while scene delta uses blink teleport data.

**Tech Stack:** C++17, Catch2 unit tests, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Required Behavior Gates

These tests must stay green:

```text
BattleFrameRunner_AdvanceFrame_CastPlanningRecordsStartWithoutSpawningAttack
BattleFrameRunner_RefreshesRuntimeCastTargetAtCommitFrame
BattleFrameRunner_RetargetsPendingCastWhenOriginalTargetDiesBeforeCommit
BattleFrameRunner_CancelsPendingCastWhenNoLiveEnemyRemainsBeforeCommit
BattleFrameRunner_AppliesCommittedNormalCastMpGain
BattleFrameRunner_CommitsCastScopedComboEffectsOnActionCommit
```

---

## Task 1: Name The Action Runtime Writeback Boundary

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [x] **Step 1: Rename the writeback helper**

Rename:

```cpp
void writeActionStateToUnitStore(BattleRuntimeState& state, const BattleFrameActionUnitResult& result)
```

to:

```cpp
void commitActionFrameStateToRuntime(BattleRuntimeState& state, const BattleFrameActionUnitResult& result)
```

Keep the implementation unchanged.

- [x] **Step 2: Verify the old name is gone**

Run:

```powershell
rg -n "writeActionStateToUnitStore|commitActionFrameStateToRuntime" src/battle/BattleCore.cpp
```

Expected: only `commitActionFrameStateToRuntime` remains.

Result: only `commitActionFrameStateToRuntime` remains.

- [ ] **Step 3: Run action tests and commit**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core][runtime]"
git add src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-23-battle-action-frame-state-plan.md
git commit -m "refactor: name action runtime writeback boundary"
```

Expected: selected tests pass, then one naming-only commit.

Result: after rebuilding `kys_tests`, `x64\Debug\kys_tests.exe "[battle][core][runtime]"` passed 219 assertions in 38 test cases.

---

## Task 2: Split Scene Blink Output From Action Diagnostic Results

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneFrameDeltaBuilder.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a narrow blink output field**

In `BattleFrameResult`, add:

```cpp
std::vector<BattleBlinkTeleportDelta> blinkTeleports;
```

- [ ] **Step 2: Publish blink teleports from action commit**

In `advanceActionFrameUnits`, after `result.actionResult = BattleActionCommitSystem().commit(...)`, append blink teleports:

```cpp
frame.result.blinkTeleports.insert(
    frame.result.blinkTeleports.end(),
    result.actionResult.blinkTeleports.begin(),
    result.actionResult.blinkTeleports.end());
```

- [ ] **Step 3: Change scene delta builder to read the narrow field**

Change `BattleSceneFrameDeltaBuilder::build` from:

```cpp
collectActionSceneEffects(frameResult.actionResults, context, result);
```

to:

```cpp
collectBlinkSceneEffects(frameResult.blinkTeleports, context, result);
```

Rename `collectActionSceneEffects` to `collectBlinkSceneEffects` and change its parameter to the blink teleport vector.

- [ ] **Step 4: Run scene and action tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][scene_frame_delta]"
```

Expected: selected tests pass.

- [ ] **Step 5: Commit the scene contract split**

Run:

```powershell
git add src/battle/BattleCore.h src/battle/BattleCore.cpp src/BattleSceneFrameDeltaBuilder.cpp tests/BattleCoreUnitTests.cpp docs/superpowers/plans/2026-05-23-battle-action-frame-state-plan.md
git commit -m "refactor: publish blink teleports outside action diagnostics"
```

Expected: one commit that keeps `actionResults` for tests but removes the scene dependency.

---

## Task 3: Move `actionResults` Private After Scene Split

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Confirm no scene consumers remain**

Run:

```powershell
rg -n "actionResults" src tests
```

Expected: no matches in scene files; remaining matches are `BattleCore.cpp`, `BattleCore.h`, and tests.

- [ ] **Step 2: Rewrite tests to public behavior**

For cast-start tests, assert `result.frame.gameplayEvents` and `state.action.pendingCasts`.

For action-commit tests, assert `state.pendingAttackSpawns` is empty after attack advancement, `result.attackEvents` contains spawned attacks, and canonical unit MP/action state changed.

For cancellation tests, assert pending casts are erased and `result.attackEvents` is empty.

- [ ] **Step 3: Move storage into context**

Add to `BattleFrameContext`:

```cpp
std::vector<BattleFrameActionUnitResult> actionResults;
```

Remove this field from `BattleFrameResult`:

```cpp
std::vector<BattleFrameActionUnitResult> actionResults;
```

Change `advanceActionFrameUnits` to append to `frame.actionResults`.

- [ ] **Step 4: Run tests and commit**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner]"
git add src/battle/BattleCore.h src/battle/BattleCore.cpp tests/BattleCoreUnitTests.cpp docs/superpowers/plans/2026-05-23-battle-action-frame-state-plan.md
git commit -m "refactor: keep action diagnostics private to frame"
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
