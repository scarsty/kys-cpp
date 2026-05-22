# Battle Damage Frame State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make damage/status frame writeback explicit and covered before deleting any damage mirror state.

**Architecture:** Damage calculation may still use `BattleDamageUnitState` as a rule input/result value, but `BattleRuntimeState` must remain the only persistent owner. This plan first closes the death-prevention coverage gap, then narrows the writeback boundary around `applyDamageResultToFrameState`.

**Tech Stack:** C++17, Catch2 unit tests, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Required Behavior Gates

These checks must stay covered before damage writeback changes:

```text
BattleFrameRunner_AdvanceFrame_ConvertsPoisonTickToDamageTransaction
BattleFrameRunner_AdvanceFrame_ConvertsBleedTickToDamageTransaction
BattleFrameRunner_AdvanceFrame_AppliesDamageTakenMpGainInsideRuntime
BattleFrameRunner_AdvanceFrame_AppliesMainProjectileImpactFreezeInCore
BattleFrameRunner_AdvanceFrame_BattleEndEventEmitsOnce
```

Add the death-prevention frame-level test in Task 1 before changing production code.

---

## Task 1: Add Frame-Level Death Prevention Coverage

**Files:**

- Modify: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add the test**

Add this test near the other damage lifecycle tests:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathPreventionKeepsRuntimeUnitAlive", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.deathEffects.store.units = { { 0 }, { 1 } };
    state.damage.pendingDamage.push_back({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 99,
            .preResolvedDamage = true,
        },
    });
    state.unitStore.requireUnit(1).vitals.hp = 20;
    auto& runtime = requireById(state.damage.unitExtras, 1);
    runtime.deathPrevention = true;
    runtime.deathPreventionFrames = 30;

    auto result = runBattleFrame(state);

    REQUIRE(result.damageTransactions.size() == 1);
    CHECK_FALSE(result.damageTransactions[0].killed);
    const auto& defender = state.unitStore.requireUnit(1);
    CHECK(defender.alive);
    CHECK(defender.vitals.hp == 1);
    CHECK(defender.invincible == 30);
    CHECK(runtime.deathPreventionUsed);
    CHECK(runtime.deathPreventionFrames == 30);
}
```

- [ ] **Step 2: Run the new test**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleFrameRunner_AdvanceFrame_DeathPreventionKeepsRuntimeUnitAlive"
```

Expected: the test passes on current behavior. If it fails, investigate the behavior before editing production code.

- [ ] **Step 3: Commit coverage**

Run:

```powershell
git add tests/BattleCoreUnitTests.cpp docs/superpowers/plans/2026-05-23-battle-damage-frame-state-plan.md
git commit -m "test: cover runtime death prevention frame result"
```

Expected: one test-only commit.

---

## Task 2: Split Damage Writeback Into Named Runtime Mutations

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Extract attacker/defender writeback helpers**

Add these helpers near `applyDamageResultToFrameState`:

```cpp
void commitDamageUnitCoreToRuntime(BattleRuntimeState& state, const BattleDamageUnitState& unit)
{
    state.unitStore.writeDamageUnit(unit);
    writeBattleDamageRuntimeUnit(requireById(state.damage.unitExtras, unit.id), unit);
}

void commitDamageDefenderStatusToRuntime(
    BattleRuntimeState& state,
    const BattleDamageTransactionResult& transaction)
{
    writeBattleStatusRuntimeUnit(
        requireById(state.status.units, transaction.defender.id),
        transaction.defenderStatus);
}

void commitDamageCooldownToRuntime(BattleRuntimeState& state, const BattleDamageTransactionResult& transaction)
{
    auto& unit = state.unitStore.requireUnit(transaction.defender.id);
    unit.animation.cooldown = transaction.defenderCooldown.cooldown;
}
```

- [ ] **Step 2: Use helpers in `applyDamageResultToFrameState`**

Replace the raw write calls for attacker/defender damage state with:

```cpp
commitDamageUnitCoreToRuntime(state, transaction.attacker);
commitDamageUnitCoreToRuntime(state, transaction.defender);
commitDamageDefenderStatusToRuntime(state, transaction);
commitDamageCooldownToRuntime(state, transaction);
```

Keep death-kick motion logic in `applyDamageResultToFrameState` for this task.

- [ ] **Step 3: Run focused damage tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][frame_runner][runtime][unit]"
```

Expected: selected damage/status tests pass.

- [ ] **Step 4: Commit the boundary split**

Run:

```powershell
git add src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-23-battle-damage-frame-state-plan.md
git commit -m "refactor: name damage runtime writeback boundary"
```

Expected: one refactor commit with no behavior changes.

---

## Task 3: Decide The First Damage Result Channel To Narrow

**Files:**

- Read: `src/battle/BattleCore.cpp`
- Read: `src/BattleSceneFrameDeltaBuilder.cpp`
- Read: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Audit damage channel consumers**

Run:

```powershell
rg -n "damageTransactions|damageRenderApplications|stateApplications|unitApplications" src tests
```

Expected: `damageRenderApplications` has scene consumers; `damageTransactions` is mostly tests plus runtime lifecycle source.

- [ ] **Step 2: Record the next candidate in this plan**

Update this plan with the chosen next field before changing code. The expected candidate is `damageTransactions` only after tests assert behavior through canonical runtime state, frame events, and `damageRenderApplications`.

---

## Completion Gate

Run:

```powershell
git diff --check
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected: diff check exits 0, all Catch2 tests pass, and MSBuild exits 0.
