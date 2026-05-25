# Battle Runtime Ownership Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the reset/clear bugs found in battle runtime by making each runtime fact have one authoritative owner and moving frame-only queues into `BattleFrameContext`.

**Architecture:** `BattleRuntimeState` owns persistent battle state, `BattleFrameContext` owns values that exist only during one `BattleFrameRunner::runFrame()`, and subsystem-specific inputs/results remain snapshots. Persistent derived state is removed unless it is owned behind the same mutation boundary as its source fact.

**Tech Stack:** C++20/C++23, Catch2, PowerShell, ripgrep, Visual Studio/MSBuild via `.github/build-command.ps1`.

---

## Direction

The target model is:

- `BattleRuntimeState` owns persistent facts.
- `BattleFrameContext` owns one-frame queues and presentation accumulators.
- Combo effects stay authoritative in `RoleComboState`.
- Status timers stay authoritative in `BattleStatusRuntimeUnit`.
- Action cancellation is a named runtime transition, not repeated field assignment.
- A `clear()` in battle runtime is acceptable only for a container whose owner is obvious from type and scope.

This plan fixes four concrete problems:

- `DeathAOE` can go stale because it is cached in `BattleRuntimeState::DamageState::unitEffects` while combo state can mutate later.
- Frozen timers are duplicated on `BattleRuntimeUnit` and `BattleStatusRuntimeUnit`.
- Frame-only queues are local variables manually cleared by callees instead of being owned by `BattleFrameContext`.
- Active action reset logic is duplicated across cooldown completion, dead-unit cleanup, and cancelled casts.

## File Structure

- Modify `src/battle/BattleCore.h`
  - Remove stale persistent derived damage effect storage if it becomes unused.
  - Remove duplicate runtime frozen fields from `BattleRuntimeUnit` only in `src/battle/BattleUnitStore.h`, not here.
  - Add a short ownership comment near `BattleRuntimeState`.

- Modify `src/battle/BattleCore.cpp`
  - Query `RoleComboState` directly for `DeathAOE`.
  - Move frame queues into `BattleFrameContext`.
  - Consolidate active action cancellation.

- Modify `src/battle/BattleRuntimeSession.h`
  - Keep `BattleSetupUnitInput::frozen` and `BattleSetupUnitInput::frozenMax` as import/setup values.

- Modify `src/battle/BattleRuntimeSession.cpp`
  - Stop populating removed duplicate runtime frozen fields.
  - Remove `refreshDeathAoeDamageEffects(...)`.

- Modify `src/battle/BattleUnitStore.h`
  - Remove `BattleRuntimeUnit::frozen` and `BattleRuntimeUnit::frozenMax`.

- Modify `src/battle/BattleInitialization.cpp`
  - Remove clone resets for removed runtime frozen fields.

- Modify `tests/BattleCoreUnitTests.cpp`
  - Add DeathAOE combo-authority regression.
  - Update DeathAOE tests to seed combo state instead of `damage.unitEffects`.
  - Add action cancellation regression if existing coverage does not fully assert all stores.

- Modify `tests/BattleInitializationUnitTests.cpp`
  - Update clone test to assert clone status timers instead of runtime unit frozen fields.

- Modify `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
  - Update setup/death frozen tests so only status state is asserted.

## Task 1: Make DeathAOE Combo-Owned

**Files:**
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`

- [ ] **Step 1: Write the failing DeathAOE combo-authority regression**

In `tests/BattleCoreUnitTests.cpp`, add this test near the existing anti-combo transfer test around the `BattleFrameRunner_AdvanceFrame_TransfersAntiComboEffectsAfterDeath` coverage:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_TransferredAntiComboDeathAoeUsesComboState", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
        unit(2, 1, { 140, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.unitStore.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 60, { 140, 100, 0 }),
    };
    state.projectileFollowUps.projectileSpeed = SceneProjectileSpeed;
    state.projectileFollowUps.minimumProjectileFrames = 20;
    state.projectileFollowUps.areaProjectileFramePadding = 15;
    state.projectileFollowUps.areaSpawnDistance = SceneTileWidth;

    const AppliedEffectInstance antiComboDeathAoe{
        EffectType::DeathAOE,
        50,
        1,
        "",
        Trigger::Always,
        0,
        6,
        0,
        33,
    };
    state.combo.units[1].appliedEffects.push_back(antiComboDeathAoe);
    state.deathEffects.store.units = {
        { .id = 0 },
        { .id = 1, .comboIds = { 33 }, .appliedEffects = { antiComboDeathAoe } },
        { .id = 2, .comboIds = { 33 } },
    };
    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto result = runBattleFrame(state);

    CHECK(std::any_of(
        state.combo.units.at(2).appliedEffects.begin(),
        state.combo.units.at(2).appliedEffects.end(),
        [](const AppliedEffectInstance& effect)
        {
            return effect.type == EffectType::DeathAOE
                && effect.sourceComboId == 33;
        }));
    CHECK(state.pendingAttackSpawns.empty());

    queuePendingDamage(state, lethalDamageInput(0, 2));
    result = runBattleFrame(state);

    REQUIRE(state.pendingAttackSpawns.size() == 1);
    CHECK(state.pendingAttackSpawns[0].initial.attackerUnitId == 2);
    CHECK(state.pendingAttackSpawns[0].initial.preferredTargetUnitId == 0);
    CHECK(state.pendingAttackSpawns[0].initial.scriptedDamage == 30);
    CHECK(state.pendingAttackSpawns[0].initial.scriptedStunFrames == 6);
}
```

This test must fail before implementation because `state.damage.unitEffects` is never updated after anti-combo transfer.

- [ ] **Step 2: Run the focused failing test**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\build\src\Debug\kys_tests.exe "BattleFrameRunner_AdvanceFrame_TransferredAntiComboDeathAoeUsesComboState"
```

Expected: the test fails because no DeathAOE projectile is queued after unit `2` dies.

- [ ] **Step 3: Replace DeathAOE cache lookup with combo lookup**

In `src/battle/BattleCore.cpp`, replace `appendFrameDeathAoeProjectiles(...)` so it queries the dead unit's current combo state:

```cpp
void appendFrameDeathAoeProjectiles(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleDamageTransactionResult& transaction,
    int deadUnitId)
{
    const auto comboIt = state.combo.units.find(deadUnitId);
    if (comboIt == state.combo.units.end())
    {
        return;
    }

    const auto* deathAoe = firstAlwaysEffect(comboIt->second, EffectType::DeathAOE);
    if (!deathAoe || deathAoe->value <= 0)
    {
        return;
    }

    const int damage = std::max(1, transaction.defender.vitals.maxHp * deathAoe->value / 100);
    frame.areaProjectileFollowUps.push_back({
        deadUnitId,
        7,
        transaction.attacker.id,
        deathAoe->value2,
        KysChess::EFT_DEATH_BLAST,
        damage,
        deathAoe->value,
        deathAoe->duration,
        "殉爆",
        std::format("殉爆{}%（{}幀）", deathAoe->value, deathAoe->duration),
    });
}
```

Use the existing `firstAlwaysEffect(...)` helper already available in this file. Do not add a new cache.

- [ ] **Step 4: Update existing DeathAOE tests to seed combo state**

In `tests/BattleCoreUnitTests.cpp`, update the existing test `BattleFrameRunner_AdvanceFrame_DeathAoeProjectileDamagesOnNextFrame`.

Replace:

```cpp
state.damage.unitEffects[1] = { 50, 6, 1 };
```

with:

```cpp
KysChess::ChessBattleEffects::applyEffect(
    state.combo.units[1],
    { EffectType::DeathAOE, 50, 1, "", Trigger::Always, 0, 6 });
```

If any other tests write `state.damage.unitEffects`, replace them the same way: write the corresponding `EffectType::DeathAOE` into `state.combo.units[unitId]`.

- [ ] **Step 5: Remove the stale derived store**

In `src/battle/BattleCore.h`, remove this member from `BattleRuntimeState::DamageState`:

```cpp
std::map<int, BattleDamageApplicationUnitEffects> unitEffects;
```

In `src/battle/BattleRuntimeSession.cpp`, delete `refreshDeathAoeDamageEffects(...)` and remove this call from `deriveRuntimeStores(...)`:

```cpp
refreshDeathAoeDamageEffects(runtime);
```

In tests and production code, remove any remaining references to `damage.unitEffects`.

- [ ] **Step 6: Verify no stale store references remain**

Run:

```powershell
rg -n "unitEffects|refreshDeathAoeDamageEffects" src tests
```

Expected: no matches.

- [ ] **Step 7: Run tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\build\src\Debug\kys_tests.exe "[battle][core]"
```

Expected: build succeeds and core battle tests pass.

- [ ] **Step 8: Commit**

```powershell
git add src\battle\BattleCore.cpp src\battle\BattleCore.h src\battle\BattleRuntimeSession.cpp tests\BattleCoreUnitTests.cpp
git commit -m "fix: make battle death aoe combo-owned"
```

## Task 2: Remove Duplicate Runtime Frozen Fields

**Files:**
- Modify: `src/battle/BattleUnitStore.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Update clone initialization test to assert status ownership**

In `tests/BattleInitializationUnitTests.cpp`, update `BattleInitializationSystem_CreatesRuntimeCloneBeforeSceneMirror`.

Remove these setup lines:

```cpp
source.frozen = 6;
source.frozenMax = 12;
```

Replace these assertions:

```cpp
CHECK(clone.frozen == 0);
CHECK(clone.frozenMax == 0);
```

with:

```cpp
CHECK(cloneSpawn.status.effects.frozenTimer == 0);
CHECK(cloneSpawn.status.effects.frozenMaxTimer == 0);
```

- [ ] **Step 2: Run the focused clone test**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\build\src\Debug\kys_tests.exe "BattleInitializationSystem_CreatesRuntimeCloneBeforeSceneMirror"
```

Expected: the test passes before field removal. This establishes the desired behavior against the authoritative status store.

- [ ] **Step 3: Remove duplicate fields from `BattleRuntimeUnit`**

In `src/battle/BattleUnitStore.h`, delete:

```cpp
int frozen = 0;
int frozenMax = 0;
```

Keep `BattleSetupUnitInput::frozen` and `BattleSetupUnitInput::frozenMax` in `src/battle/BattleRuntimeSession.h`; those are setup/import values, not runtime authority.

- [ ] **Step 4: Remove production writes to deleted fields**

In `src/battle/BattleInitialization.cpp`, remove these lines from `makeCloneRuntimeUnit(...)`:

```cpp
clone.frozen = 0;
clone.frozenMax = 0;
```

In `src/battle/BattleCore.cpp`, remove these lines from the killed-unit branch in `applyDamageResultToFrameState(...)`:

```cpp
unit.frozen = 0;
unit.frozenMax = 0;
```

Keep the status reset that immediately follows:

```cpp
auto& status = requireById(state.status.units, unit.id);
status.effects.frozenTimer = 0;
status.effects.frozenMaxTimer = 0;
```

- [ ] **Step 5: Update any remaining tests and setup code**

Run:

```powershell
rg -n "\.frozen\b|frozenMax" src\battle tests -g "*.cpp" -g "*.h"
```

Expected remaining production matches:

```text
src\battle\BattleCore.cpp: input.frozen = requireById(...).effects.frozenTimer > 0;
src\battle\BattleRuntimeSession.h: int frozen = 0;
src\battle\BattleRuntimeSession.h: int frozenMax = 0;
src\battle\BattleRuntimeSession.cpp: spawn.status.effects.frozenTimer = setup.frozen;
src\battle\BattleRuntimeSession.cpp: spawn.status.effects.frozenMaxTimer = setup.frozenMax;
src\battle\BattleStatusSystem.h: int frozenTimer = 0;
src\battle\BattleStatusSystem.h: int frozenMaxTimer = 0;
```

If tests still reference `BattleRuntimeUnit::frozen`, replace those assertions with `requireById(state.status.units, unitId).effects.frozenTimer`.

- [ ] **Step 6: Add a death reset ownership regression if missing**

In `tests/BattleCoreUnitTests.cpp`, add or update a test so death clears frozen through status only:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathClearsFrozenStatusAuthority", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.unitStore.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
    };
    seedDamageExtrasFromUnits(state);
    state.status.units = {
        makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(state.unitStore.units[0], state.combo.units[0])),
        makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(state.unitStore.units[1], state.combo.units[1])),
    };
    requireById(state.status.units, 1).effects.frozenTimer = 5;
    requireById(state.status.units, 1).effects.frozenMaxTimer = 8;
    queuePendingDamage(state, lethalDamageInput(0, 1));

    runBattleFrame(state);

    CHECK(requireById(state.status.units, 1).effects.frozenTimer == 0);
    CHECK(requireById(state.status.units, 1).effects.frozenMaxTimer == 0);
}
```

If `seedDamageExtrasFromUnits(...)` is not available in `BattleCoreUnitTests.cpp`, use the existing local test helper that seeds `state.damage.unitExtras` from `state.unitStore.units`.

- [ ] **Step 7: Run tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\build\src\Debug\kys_tests.exe "[battle][initialization]"
.\build\src\Debug\kys_tests.exe "[battle][core]"
.\build\src\Debug\kys_tests.exe "[battle][runtime]"
```

Expected: build succeeds and the selected battle tests pass.

- [ ] **Step 8: Commit**

```powershell
git add src\battle\BattleUnitStore.h src\battle\BattleInitialization.cpp src\battle\BattleCore.cpp tests\BattleInitializationUnitTests.cpp tests\BattleFrameRunnerRuntimeUnitTests.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: make frozen status single-owned"
```

## Task 3: Move Frame Queues Into BattleFrameContext

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Test: existing battle core/runtime tests

- [ ] **Step 1: Extend `BattleFrameContext`**

In `src/battle/BattleCore.cpp`, add the frame-only queues to the private `BattleFrameContext` struct:

```cpp
struct BattleFrameContext
{
    BattlePresentationFrame result;
    std::vector<BattleGameplayCommand> frameCommands;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<int> attackSoundIds;
    std::vector<BattleFrameRumbleEvent> rumbles;
    int blinkSoundCount{};
    std::vector<BattleAttackEvent> attackEvents;
    std::vector<BattleAreaProjectileFollowUp> areaProjectileFollowUps;
    std::vector<BattleFrameMpRestore> lateMpRestores;
    std::vector<BattleAttackSpawnRequest> attackSpawns;
    std::vector<BattlePendingDamageIntent> pendingDamage;
    std::vector<BattleFrameCastScopedComboEffects> castScopedComboEffects;
    UnitMotionSnapshotMap frameStartMotion;
};
```

- [ ] **Step 2: Move pending runtime queues into the context at frame start**

In `makeBattleFrameContext(...)`, swap runtime carry-over queues into the context:

```cpp
BattleFrameContext makeBattleFrameContext(BattleRuntimeState& state)
{
    BattleFrameContext frame;
    frame.frameStartMotion = makeUnitMotionSnapshot(state.unitStore);
    frame.attackSpawns.swap(state.pendingAttackSpawns);
    frame.pendingDamage.swap(state.damage.pendingDamage);
    return frame;
}
```

Update the declaration and call site from `const BattleRuntimeState&` to `BattleRuntimeState&`.

- [ ] **Step 3: Update function signatures to use frame-owned queues**

Change these signatures and their call sites:

```cpp
void advanceActionFrameUnits(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleTickResult& movement)
```

Inside it, replace `attackSpawns` with `frame.attackSpawns` and `castScopedComboEffects` with `frame.castScopedComboEffects`.

```cpp
void advanceAttacksAndResolveHits(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
```

Inside it, replace the parameters with `frame.attackSpawns` and `frame.pendingDamage`.

```cpp
void applyDamageAndLifecycle(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
```

Inside it, replace `pendingDamage` with `frame.pendingDamage`.

```cpp
void applyFrameCastScopedComboEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
```

Inside it, iterate `frame.castScopedComboEffects` and write to `frame.attackSpawns`, `frame.pendingDamage`, `frame.logEvents`, and `frame.visualEvents`.

- [ ] **Step 4: Update command reduction wrapper**

Replace:

```cpp
void reduceFrameGameplayCommands(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    std::vector<BattlePendingDamageIntent>& pendingDamage)
```

with:

```cpp
void reduceFrameGameplayCommands(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.frameCommands,
        frame.attackSoundIds,
        frame.rumbles,
        frame.attackSpawns,
        frame.pendingDamage,
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents);
}
```

For the final late-frame reduction that intentionally writes to next-frame runtime queues, keep a separate explicit helper:

```cpp
void reduceFrameGameplayCommandsToRuntimeQueues(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.frameCommands,
        frame.attackSoundIds,
        frame.rumbles,
        state.pendingAttackSpawns,
        state.damage.pendingDamage,
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents);
}
```

- [ ] **Step 5: Rewrite `runFrame()` to show ownership**

In `BattleFrameRunner::runFrame(...)`, remove local declarations of:

```cpp
std::vector<BattleAttackSpawnRequest> frameAttackSpawns;
std::vector<BattlePendingDamageIntent> framePendingDamage;
std::vector<BattleFrameCastScopedComboEffects> frameCastScopedComboEffects;
```

The frame flow should read:

```cpp
auto frame = makeBattleFrameContext(state);

advanceStatus(state, frame.pendingDamage);
auto runtimeAdvance = advanceRuntimeUnits(state);
auto deferredCommands = applyRuntimeComboEvents(state, frame, runtimeAdvance.runtimeCommits);
applySkillFinishedTeamHeals(state, frame, runtimeAdvance.skillFinishedTeamHeals);
reduceFrameGameplayCommands(state, frame);
auto movement = advanceMotionFrame(state);
advanceActionFrameUnits(state, frame, movement);
applyFrameCastScopedComboEffects(state, frame);
reduceFrameGameplayCommands(state, frame);
advanceAttacksAndResolveHits(state, frame);
applyDamageAndLifecycle(state, frame);
appendProjectileCancellationLogEvents(state.attacks, frame.attackEvents, frame.logEvents, true);
applyLateFrameMpRestores(state, frame);
frame.frameCommands.insert(
    frame.frameCommands.end(),
    std::make_move_iterator(deferredCommands.begin()),
    std::make_move_iterator(deferredCommands.end()));
reduceFrameGameplayCommandsToRuntimeQueues(state, frame);
assert(frame.frameCommands.empty());
emitPresentationFrame(state, frame);
pruneFinishedRuntimeAttacks(state);
return consumeBattleFrameContext(std::move(frame));
```

- [ ] **Step 6: Remove callee-side frame queue clears**

Remove these lines because the queues are now consumed by frame ownership:

```cpp
attackSpawns.clear();
pendingDamage.clear();
```

For `frame.lateMpRestores.clear()`, keep it if `applyLateFrameMpRestores(...)` may be called more than once in a future frame flow. Otherwise replace with:

```cpp
auto restores = std::move(frame.lateMpRestores);
for (const auto& restore : restores)
{
    applyFrameMpRestore(
        state,
        restore.unitId,
        restore.amount,
        restore.reason,
        frame.logEvents);
}
```

Prefer the move-based version because it makes consumption explicit.

- [ ] **Step 7: Verify frame queue ownership**

Run:

```powershell
rg -n "frameAttackSpawns|framePendingDamage|frameCastScopedComboEffects|attackSpawns\.clear\(\)|pendingDamage\.clear\(\)" src\battle\BattleCore.cpp
```

Expected: no matches for the old local variable names or callee-side clears.

- [ ] **Step 8: Run tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\build\src\Debug\kys_tests.exe "[battle][core]"
.\build\src\Debug\kys_tests.exe "[battle][runtime]"
```

Expected: build succeeds and selected tests pass.

- [ ] **Step 9: Commit**

```powershell
git add src\battle\BattleCore.cpp
git commit -m "refactor: move battle frame queues into frame context"
```

## Task 4: Consolidate Runtime Action Cancellation

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a regression for complete dead-unit action cleanup**

In `tests/BattleCoreUnitTests.cpp`, add this test near other action/runtime cleanup tests:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_DeadUnitActionCleanupClearsAllActionOwners", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.unitStore.units = {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
    };
    state.unitStore.requireUnit(1).animation.cooldown = 4;
    state.unitStore.requireUnit(1).animation.actFrame = 2;
    state.unitStore.requireUnit(1).animation.actType = 3;
    state.unitStore.requireUnit(1).operationType = BattleOperationType::Melee;
    state.unitStore.requireUnit(1).haveAction = true;
    state.action.pendingCasts.emplace(1, BattlePendingCastAction{});
    state.ultimateCasters.insert(1);
    queuePendingDamage(state, lethalDamageInput(0, 1));

    runBattleFrame(state);

    const auto& dead = state.unitStore.requireUnit(1);
    CHECK(dead.animation.cooldown == 0);
    CHECK(dead.animation.actFrame == 0);
    CHECK(dead.animation.actType == -1);
    CHECK(dead.operationType == BattleOperationType::None);
    CHECK_FALSE(dead.haveAction);
    CHECK(state.action.pendingCasts.count(1) == 0);
    CHECK(state.ultimateCasters.count(1) == 0);
}
```

- [ ] **Step 2: Run the focused test**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\build\src\Debug\kys_tests.exe "BattleFrameRunner_AdvanceFrame_DeadUnitActionCleanupClearsAllActionOwners"
```

Expected: the test should pass before refactor. If it fails, fix the behavior in the same task before moving reset code.

- [ ] **Step 3: Introduce one local action-state reset helper**

In `src/battle/BattleCore.cpp`, replace `clearActionFrameState(...)` with a value-level helper:

```cpp
void resetActionFrameState(BattleUnitFrameTickState& state)
{
    state.cooldown = 0;
    state.actFrame = 0;
    state.actType = -1;
    state.operationType = BattleOperationType::None;
    state.haveAction = false;
}
```

Rename call sites from `clearActionFrameState(actionState)` to `resetActionFrameState(actionState)`.

- [ ] **Step 4: Make runtime cancellation call the value helper**

Replace `clearRuntimePendingAction(...)` with:

```cpp
void cancelRuntimeAction(BattleRuntimeState& state, int unitId)
{
    auto& unit = state.unitStore.requireUnit(unitId);
    auto actionState = makeActionRuntimeState(unit);
    resetActionFrameState(actionState);
    commitActionFrameStateToRuntime(unit, actionState);
    state.action.pendingCasts.erase(unitId);
    state.ultimateCasters.erase(unitId);
}
```

Rename call sites:

```cpp
clearRuntimePendingAction(state, unit.id);
```

to:

```cpp
cancelRuntimeAction(state, unit.id);
```

Rename `clearDeadRuntimePendingActions(...)` to:

```cpp
void cancelDeadRuntimeActions(BattleRuntimeState& state)
```

and update its call site after damage lifecycle.

- [ ] **Step 5: Replace repeated active-action completion assignment**

In `advanceActionFrameUnits(...)`, replace:

```cpp
actionState.haveAction = false;
actionState.operationType = BattleOperationType::None;
actionState.actType = -1;
state.action.pendingCasts.erase(unit.id);
```

with:

```cpp
actionState.haveAction = false;
actionState.operationType = BattleOperationType::None;
actionState.actType = -1;
state.action.pendingCasts.erase(unit.id);
state.ultimateCasters.erase(unit.id);
```

Do not call `cancelRuntimeAction(...)` here because this branch is operating on the local `actionState` before it is committed. The important fix is that this branch also clears `ultimateCasters` when the active action ends.

- [ ] **Step 6: Verify reset naming**

Run:

```powershell
rg -n "clearActionFrameState|clearRuntimePendingAction|clearDeadRuntimePendingActions|haveAction = false|operationType = BattleOperationType::None|actType = -1" src\battle\BattleCore.cpp
```

Expected:

- No matches for the old `clear*Action*` helper names.
- Remaining manual `haveAction`/`operationType`/`actType` assignments are either inside `resetActionFrameState(...)`, inside `BattleUnitFrameTickSystem::advance(...)`, or in the local active-action completion branch documented in Step 5.

- [ ] **Step 7: Run tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\build\src\Debug\kys_tests.exe "[battle][core]"
```

Expected: build succeeds and core battle tests pass.

- [ ] **Step 8: Commit**

```powershell
git add src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: consolidate battle action cancellation"
```

## Task 5: Add Runtime Ownership Notes and Final Verification

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Add ownership comment near `BattleRuntimeState`**

In `src/battle/BattleCore.h`, add this comment immediately before `struct BattleRuntimeState`:

```cpp
// Persistent battle facts live here. One-frame queues and presentation accumulation
// belong in BattleFrameContext inside BattleCore.cpp. Do not add cached copies of
// combo/status/action facts here unless all mutations to the source fact update the
// cache through the same owner.
```

- [ ] **Step 2: Add frame-context ownership comment if missing**

In `src/battle/BattleCore.cpp`, keep or update the existing `BattleFrameContext` comment to:

```cpp
// Private runFrame() state. Persistent gameplay lives in BattleRuntimeState.
// Anything consumed within one frame belongs here, not in BattleRuntimeState.
// Keep this type private to BattleCore.cpp and do not pass it to subsystem classes.
```

- [ ] **Step 3: Run ownership searches**

Run:

```powershell
rg -n "unitEffects|refreshDeathAoeDamageEffects|\.frozen\b|frozenMax|frameAttackSpawns|framePendingDamage|frameCastScopedComboEffects|clearActionFrameState|clearRuntimePendingAction|clearDeadRuntimePendingActions" src\battle tests
```

Expected:

- No `unitEffects` or `refreshDeathAoeDamageEffects`.
- No `BattleRuntimeUnit::frozen` or `BattleRuntimeUnit::frozenMax` usage.
- `frozen`/`frozenMax` only remain as setup input or status timer names.
- No old frame queue local names.
- No old action reset helper names.

- [ ] **Step 4: Run focused battle tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\build\src\Debug\kys_tests.exe "[battle][core]"
.\build\src\Debug\kys_tests.exe "[battle][initialization]"
.\build\src\Debug\kys_tests.exe "[battle][runtime]"
```

Expected: build succeeds and selected test groups pass.

- [ ] **Step 5: Run the full test target build**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
```

Expected: build succeeds. A final link failure is acceptable only if the game or test executable is currently running and locking the output.

- [ ] **Step 6: Commit**

```powershell
git add src\battle\BattleCore.h src\battle\BattleCore.cpp
git commit -m "docs: record battle runtime ownership rules"
```

## Self-Review

- Spec coverage: The plan addresses all four findings: stale DeathAOE cache, duplicate frozen state, frame queue ownership, and repeated action reset.
- Placeholder scan: The plan contains no `TBD`, no `TODO`, and no open-ended implementation steps.
- Type consistency: The plan uses existing types from the reviewed code: `BattleRuntimeState`, `BattleFrameContext`, `RoleComboState`, `AppliedEffectInstance`, `BattlePendingDamageIntent`, `BattleFrameCastScopedComboEffects`, and `BattlePendingCastAction`.
- Scope check: The plan does not attempt a full battle-runtime rewrite. It fixes the found bugs and adds ownership guardrails for the specific state categories involved.
