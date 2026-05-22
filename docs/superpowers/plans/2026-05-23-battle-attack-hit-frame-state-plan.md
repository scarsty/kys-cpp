# Battle Attack Hit Frame State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep hit-resolution diagnostics private while preserving projectile scene/report behavior.

**Architecture:** `state.attacks` owns persistent projectile state. `BattleFrameResult::attackEvents` remains public while scene impact and presentation generation read it. `hitResults` should become private frame scratch once tests assert behavior through damage transactions, frame events, and canonical runtime state.

**Tech Stack:** C++17, Catch2 unit tests, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Required Behavior Gates

These tests must stay green:

```text
BattleFrameRunner_AdvanceFrame_ReducesHitDamageInsideSameFrame
BattleFrameRunner_AdvanceFrame_DodgeConsumesHitBeforeDamage
BattleFrameRunner_AdvanceFrame_RecordsTargetLostCancellationWithoutPairedAttack
BattleFrameRunner_AdvanceFrame_RecordsProjectileCancelPairWithOtherAttackId
BattleFrameRunner_AdvanceFrame_AggregatesProjectileContactIgnoredByInvincible
BattleFrameRunner_AdvanceFrame_LogsBounceChainTerminalReasons
```

---

## Task 1: Rewrite Tests Away From Public `hitResults`

**Files:**

- Modify: `tests/BattleCoreUnitTests.cpp`

- [x] **Step 1: Find direct `hitResults` assertions**

Run:

```powershell
rg -n "hitResults" tests/BattleCoreUnitTests.cpp tests/BattleFrameRunnerRuntimeUnitTests.cpp
```

Expected: matches in hit-to-damage, dodge, scripted hit, and invincible aggregation tests.

Result: direct assertions were in hit-to-damage, same-frame damage, dodge, scripted hit, and invincible aggregation tests.

- [x] **Step 2: Replace same-frame hit assertions**

In `BattleFrameRunner_AdvanceFrame_ReducesHitDamageInsideSameFrame`, replace direct `result.hitResults` assertions with:

```cpp
REQUIRE(result.damageTransactions.size() == 1);
CHECK(result.damageTransactions.front().attacker.id == 0);
CHECK(result.damageTransactions.front().defender.id == 1);
CHECK(result.damageTransactions.front().finalHpDamage > 0);
```

- [x] **Step 3: Replace dodge assertions**

In `BattleFrameRunner_AdvanceFrame_DodgeConsumesHitBeforeDamage`, remove `result.hitResults[0].dodged` and keep the public behavior checks:

```cpp
CHECK(result.damageTransactions.empty());
CHECK(dodgeLog != result.frame.logEvents.end());
CHECK(dodgeEffect != result.frame.visualEvents.end());
```

- [x] **Step 4: Replace scripted hit assertions**

In `BattleFrameRunner_AdvanceFrame_ResolvesScriptedHitEvents`, assert damage and frame events:

```cpp
REQUIRE(result.damageTransactions.size() == 1);
CHECK(result.damageTransactions.front().finalHpDamage == 33);
```

- [x] **Step 5: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core]"
```

Expected: selected tests pass before production field removal.

Result: after rebuilding `kys_tests`, `x64\Debug\kys_tests.exe "[battle][core]"` passed 677 assertions in 103 test cases.

- [ ] **Step 6: Commit test migration**

Run:

```powershell
git add tests/BattleCoreUnitTests.cpp docs/superpowers/plans/2026-05-23-battle-attack-hit-frame-state-plan.md
git commit -m "test: stop asserting public hit diagnostics"
```

Expected: one test-only commit.

---

## Task 2: Move `hitResults` Into Frame Context

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Add private context storage**

In `BattleFrameContext`, add:

```cpp
std::vector<BattleHitResolutionResult> hitResults;
```

- [ ] **Step 2: Remove public field**

In `BattleFrameResult`, remove:

```cpp
std::vector<BattleHitResolutionResult> hitResults;
```

- [ ] **Step 3: Route hit resolution through context**

In `advanceAttacksAndResolveHits`, change:

```cpp
result.hitResults
```

to:

```cpp
frame.hitResults
```

In `applyDamageAndLifecycle`, change critical-defender collection to iterate:

```cpp
for (const auto& hit : frame.hitResults)
```

- [ ] **Step 4: Run tests and commit**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner]"
git add src/battle/BattleCore.h src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-23-battle-attack-hit-frame-state-plan.md
git commit -m "refactor: keep hit diagnostics private to frame"
```

Expected: selected tests pass, then one commit.

---

## Task 3: Leave `attackEvents` Public Until Scene Impact Is Reworked

**Files:**

- Read: `src/BattleSceneImpactPlayer.cpp`
- Read: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Confirm attack event consumers**

Run:

```powershell
rg -n "attackEvents" src tests
```

Expected: `BattleSceneImpactPlayer` and presentation generation still consume `attackEvents`.

- [ ] **Step 2: Record the next split before removing `attackEvents`**

Before any future `attackEvents` removal, create a plan that moves scene impact commands into a narrow result channel and stops `emitPresentationFrame()` from re-reading attack events.

---

## Completion Gate

Run:

```powershell
git diff --check
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected: diff check exits 0, all Catch2 tests pass, and MSBuild exits 0.
