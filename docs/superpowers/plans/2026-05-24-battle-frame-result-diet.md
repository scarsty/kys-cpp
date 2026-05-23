# Battle Frame Result Diet Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Shrink `BattleFrameResult` so it stops publishing frame-internal synchronization channels and remains a narrow presentation/report contract.

**Architecture:** Keep `BattleRuntimeState` as the only persistent gameplay owner and keep `BattleFrameContext` as private one-frame scratch in `BattleCore.cpp`. Migrate production scene consumers toward `BattlePresentationFrame` plus a narrowed `BattleFrameApplications` sidecar for sound/rumble only. Do not add a new bridge layer or split files only for line count.

**Tech Stack:** C++20, Catch2, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Current Shape To Preserve

- `BattleRuntimeSession::runFrame()` remains the only post-initialization gameplay advance path.
- `BattleFrameRunner::runFrame()` remains the visible ordered frame transaction in `src/battle/BattleCore.cpp`.
- Scene code may consume presentation/report outputs, but must not infer or mutate gameplay state.
- `BattleCore.cpp` may stay large. The goal is deleting public synchronization surfaces, not cosmetic extraction.

## End State For This Plan

`BattleFrameResult` keeps only:

```cpp
struct BattleFrameResult
{
    BattlePresentationFrame frame;
    BattleFrameApplications applications;
    std::vector<BattleDamageTransactionResult> damageTransactions;
};
```

`damageTransactions` stays for now because many damage-rule tests still use it as a diagnostic channel. Do not remove it in this plan.

Remove these public fields:

```cpp
std::vector<BattleAttackEvent> attackEvents;
std::vector<BattleFrameUnitApplication> unitApplications;
std::vector<BattleFrameMovementPresentationUnitResult> movementPresentationResults;
std::vector<BattleBlinkTeleportDelta> blinkTeleports;
std::vector<BattleFrameDamageRenderApplication> damageRenderApplications;
std::vector<BattleRescueRepositionResult> rescueResults;
BattleFrameStateApplications stateApplications;
```

Narrow `BattleFrameApplications` to:

```cpp
struct BattleFrameRumbleEvent
{
    int lowFrequency{};
    int highFrequency{};
    int durationMs{};
};

struct BattleFrameApplications
{
    std::vector<int> attackSoundIds;
    std::vector<BattleFrameRumbleEvent> rumbles;
    int blinkSoundCount{};
};
```

Do not introduce a new public impact channel. For hit impact playback, enrich existing `BattleVisualEvent` with the small scene impact payload.

## Task 1: Remove Test-Only Runtime Publication Channels

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Verify current consumers before editing**

Run:

```powershell
rg -n "unitApplications|stateApplications|movementPresentationResults|BattleFrameUnitApplication|BattleFrameStateApplications|BattleFrameMovementPresentationUnitResult|BattleFrameComboRenderApplication|BattleFrameRenderStatusUnit" src tests
```

Expected before the task: production matches are only declarations/publishers in `BattleCore.h/.cpp`; direct consumers are tests.

- [ ] **Step 2: Update tests to assert runtime state instead of `unitApplications`**

Replace `result.unitApplications` assertions in `tests/BattleCoreUnitTests.cpp` and `tests/BattleFrameRunnerRuntimeUnitTests.cpp` with assertions against `state.unitStore.requireUnit(...)`.

Examples of equivalent assertions:

```cpp
auto result = runBattleFrame(state);
(void)result;

const auto& unit = state.unitStore.requireUnit(0);
CHECK(unit.animation.cooldown == 0);
CHECK(unit.animation.actType == -1);
CHECK(unit.vitals.mp == 21);
CHECK(unit.physicalPower == 5);
CHECK_FALSE(unit.haveAction);
```

For tests that only assert publication count, remove the count assertion and assert the concrete runtime mutation that justified the test.

- [ ] **Step 3: Update tests to assert runtime state instead of `stateApplications`**

Replace `result.stateApplications.statusRenderUnits` assertions with `state.status.units` and `state.unitStore`.

Use this pattern:

```cpp
const auto& runtimeUnit = state.unitStore.requireUnit(0);
const auto& status = requireById(state.status.units, 0);

CHECK(runtimeUnit.invincible == 3);
CHECK(status.effects.frozenTimer == 3);
CHECK(status.effects.frozenMaxTimer == 9);
CHECK(runtimeUnit.shield == 12);
CHECK(requireById(state.damage.unitExtras, 0).blockFirstHitsRemaining == 2);
```

If a helper is needed in a test file, reuse existing local helpers such as `requireById`/`requireMappedById` when already included; otherwise add a small local helper in the test file.

- [ ] **Step 4: Update tests to assert runtime state or existing visual events instead of `movementPresentationResults`**

Replace movement publication assertions with:

```cpp
const auto& runtimeUnit = state.unitStore.requireUnit(0);
CHECK(runtimeUnit.motion.position.x == Catch::Approx(105.0f));
CHECK(runtimeUnit.motion.velocity.x == Catch::Approx(5.0f));
CHECK(runtimeUnit.motion.acceleration.x == Catch::Approx(0.0f));
```

For same-frame death kick movement tests, assert the runtime unit motion directly after `runFrame()`:

```cpp
const auto& dead = state.unitStore.requireUnit(1);
CHECK_FALSE(dead.alive);
CHECK(dead.motion.velocity.norm() > 0.01f);
CHECK(dead.motion.position.z >= 0.0f);
```

- [ ] **Step 5: Remove the public types and fields**

In `src/battle/BattleCore.h`, delete:

```cpp
struct BattleFrameUnitApplication;
struct BattleFrameComboRenderApplication;
struct BattleFrameRenderStatusUnit;
struct BattleFrameStateApplications;
struct BattleFrameMovementPresentationUnitResult;
```

Delete these fields from `BattleFrameResult`:

```cpp
std::vector<BattleFrameUnitApplication> unitApplications;
std::vector<BattleFrameMovementPresentationUnitResult> movementPresentationResults;
BattleFrameStateApplications stateApplications;
```

- [ ] **Step 6: Remove the dead publishers**

In `src/battle/BattleCore.cpp`:

- Change `advanceRuntimeUnits(...)` so it no longer takes `std::vector<BattleFrameUnitApplication>&`.
- Delete the `unitApplications.clear()` and `unitApplications.push_back(...)` code.
- Delete `publishMovementPresentationResults(...)`.
- Delete `makeBattleFrameComboRenderApplication(...)`, `makeBattleFrameRenderStatusUnit(...)`, and `publishFrameApplyOutputs(...)`.
- Remove these calls from `BattleFrameRunner::runFrame()`:

```cpp
publishMovementPresentationResults(state, frame.result, frame.movement, frame.movementPhysicsResults);
publishFrameApplyOutputs(state, frame.result);
```

- [ ] **Step 7: Verify slice 1**

Run:

```powershell
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner]"
rg -n "unitApplications|stateApplications|movementPresentationResults|BattleFrameUnitApplication|BattleFrameStateApplications|BattleFrameMovementPresentationUnitResult|BattleFrameComboRenderApplication|BattleFrameRenderStatusUnit" src tests
```

Expected: build succeeds, focused tests pass, and `rg` returns no matches for the deleted public channels.

## Task 2: Narrow `BattleFrameApplications`

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Verify current `BattleFrameApplications` consumers**

Run:

```powershell
rg -n "BattleFrameApplications|applications\\.(knockbacks|mpRestores|unitHeals|lastAttackers|attackSoundIds|rumbles)" src tests
```

Expected before editing: scene code consumes only `attackSoundIds` and `rumbles`; tests may consume `knockbacks`; other fields are only written.

- [ ] **Step 2: Replace stale application assertions**

For knockback tests, assert runtime velocity and movement-agent physics:

```cpp
auto result = runBattleFrame(state);
(void)result;

const auto& unit = state.unitStore.requireUnit(targetUnitId);
CHECK(unit.motion.velocity.x == Catch::Approx(expectedX));
CHECK(requireMappedById(state.movement.agents, targetUnitId).physics.velocity.x == Catch::Approx(expectedX));
```

For MP restore/heal/last-attacker application tests, assert runtime vitals, logs, and visual events. Do not create replacement result fields.

- [ ] **Step 3: Delete unused subchannels**

In `src/battle/BattleCore.h`, remove from `BattleFrameApplications`:

```cpp
std::vector<BattleFrameKnockbackDelta> knockbacks;
std::vector<BattleFrameMpRestoreDelta> mpRestores;
std::vector<BattleFrameUnitHealDelta> unitHeals;
std::vector<BattleFrameLastAttackerDelta> lastAttackers;
```

Delete the now-unused structs:

```cpp
BattleFrameKnockbackDelta
BattleFrameMpRestoreDelta
BattleFrameUnitHealDelta
BattleFrameLastAttackerDelta
```

- [ ] **Step 4: Remove writes to deleted subchannels**

In `src/battle/BattleCore.cpp`, remove these writes while preserving gameplay mutation/log/visual behavior:

```cpp
applications.mpRestores.push_back(...);
applications.unitHeals.push_back(...);
applications.knockbacks.push_back(...);
applications.lastAttackers.push_back(...);
frame.result.applications.unitHeals.push_back(...);
```

Keep:

```cpp
applications.attackSoundIds.push_back(...);
applications.rumbles.push_back(...);
```

- [ ] **Step 5: Verify slice 2**

Run:

```powershell
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][scene_frame_delta]"
rg -n "knockbacks|mpRestores|unitHeals|lastAttackers|BattleFrameKnockbackDelta|BattleFrameMpRestoreDelta|BattleFrameUnitHealDelta|BattleFrameLastAttackerDelta" src tests
```

Expected: build succeeds, focused tests pass, and deleted application fields/types no longer exist.

## Task 3: Move Scene Delta To Presentation Inputs

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneFrameDeltaBuilder.h`
- Modify: `src/BattleSceneFrameDeltaBuilder.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleSceneFrameDeltaBuilderUnitTests.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `tests/BattleRuntimeScenarioTestHelpers.h`
- Modify: `tests/BattleRuntimeScenarioUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add blink sound count to the narrowed applications sidecar**

In `src/battle/BattleCore.h`, add:

```cpp
int blinkSoundCount{};
```

to `BattleFrameApplications`.

In `advanceActionFrameUnits(...)`, after appending `result.actionResult.blinkTeleports`, increment the count:

```cpp
applications.blinkSoundCount += static_cast<int>(result.actionResult.blinkTeleports.size());
```

This keeps blink sound playback without exposing teleport deltas through `BattleFrameResult`.

- [ ] **Step 2: Change the scene delta builder API**

Change `BattleSceneFrameDeltaBuilder::build(...)` to:

```cpp
BattleSceneFrameDelta build(
    const KysChess::Battle::BattlePresentationFrame& frame,
    const KysChess::Battle::BattleFrameApplications& applications,
    int currentBattleResult,
    BattleSceneFrameDeltaBuildContext context) const;
```

Change private helpers so they no longer accept `BattleFrameResult`:

```cpp
void collectDamageSceneEffects(
    const KysChess::Battle::BattlePresentationFrame& frame,
    int currentBattleResult,
    BattleSceneFrameDeltaBuildContext& context,
    BattleSceneFrameDelta& result) const;

void collectFrameApplicationSceneEffects(
    const KysChess::Battle::BattleFrameApplications& applications,
    BattleSceneFrameDelta& result) const;
```

Delete `collectBlinkSceneEffects(...)`; blink sound is handled by `applications.blinkSoundCount`.

- [ ] **Step 3: Rebuild death/damage scene effects from ordered frame events**

In `BattleSceneFrameDeltaBuilder.cpp`, collect damage unit ids from log events:

```cpp
std::set<int> damagedUnitIds;
for (const auto& log : frame.logEvents)
{
    if (log.type == KysChess::Battle::BattleLogEventType::Damage && log.amount > 0 && log.targetUnitId >= 0)
    {
        damagedUnitIds.insert(log.targetUnitId);
    }
}
```

For each damaged unit id, emit the existing blood effect and hurt flash:

```cpp
result.bloodEffects.push_back({
    unitId,
    std::format("eft/bld{:03}", int(context.random->rand() * 5)),
});
result.hurtFlashes.push_back({
    unitId,
    context.hurtFlashDuration,
});
```

Use `frame.gameplayEvents` to collect `UnitDied` and `BattleEnded` exactly as the current helper does.

For each died unit id, look up the runtime unit through `context.units->requireRuntimeUnit(unitId)` and keep the current camera/jitter/freeze/slow behavior.

Do not use `DamageApplied` gameplay events for blood/hurt flash because shield absorption and MP changes also map to `DamageApplied`.

- [ ] **Step 4: Keep sound/rumble behavior through applications**

In `collectFrameApplicationSceneEffects(...)`, keep copying `attackSoundIds` and `rumbles`.

Add blink sounds:

```cpp
for (int i = 0; i < applications.blinkSoundCount; ++i)
{
    assert(context.blinkSoundEffectId >= 0);
    result.effectSoundIds.push_back(context.blinkSoundEffectId);
}
```

This helper needs `context`; pass it in or move the blink loop into `build(...)`.

- [ ] **Step 5: Update scene call site**

In `BattleSceneHades::applyCoreFrameResult(...)`, call:

```cpp
applySceneFrameDelta(frame_delta_builder_.build(
    frameResult.frame,
    frameResult.applications,
    result_,
    context));
```

- [ ] **Step 6: Delete raw scene result fields**

In `src/battle/BattleCore.h`, delete from `BattleFrameResult`:

```cpp
std::vector<BattleBlinkTeleportDelta> blinkTeleports;
std::vector<BattleFrameDamageRenderApplication> damageRenderApplications;
std::vector<BattleRescueRepositionResult> rescueResults;
```

Delete `BattleFrameDamageRenderUnit` and `BattleFrameDamageRenderApplication`.

In `src/battle/BattleCore.cpp`:

- Delete `makeBattleFrameDamageRenderUnit(...)`.
- Delete `makeBattleFrameDamageRenderApplication(...)`.
- Remove `result.damageRenderApplications.push_back(...)`.
- Change `tryApplyRescue(...)` and `applyRescueRepositionForDamage(...)` so they no longer accept or write `BattleFrameResult& result`.
- Keep `commitRescueResultToRuntime(...)` and its log/visual event insertion.
- Remove the `blinkTeleports` alias and insertion from `advanceActionFrameUnits(...)`; only increment `blinkSoundCount`.

- [ ] **Step 7: Update tests and scenario digest**

In `tests/BattleSceneFrameDeltaBuilderUnitTests.cpp`, build `BattlePresentationFrame` directly:

```cpp
KysChess::Battle::BattlePresentationFrame frame;
frame.logEvents.push_back({
    KysChess::Battle::BattleLogEventType::Damage,
    90,
    0,
    1,
    44,
});
frame.gameplayEvents.push_back({
    KysChess::Battle::BattleGameplayEventType::UnitDied,
    90,
    0,
    1,
});
```

For rescue, assert the builder does not mutate runtime by passing an empty presentation frame; no `rescueResults` setup remains.

In `tests/BattleInitializationUnitTests.cpp`, replace `damageRenderApplications` checks with either:

```cpp
appliedDamage = appliedDamage || std::ranges::any_of(frameResult.frame.logEvents, [](const auto& log)
{
    return log.type == KysChess::Battle::BattleLogEventType::Damage && log.amount > 0;
});
```

or an equivalent explicit loop if `<ranges>` is not already used.

In `tests/BattleRuntimeScenarioTestHelpers.h`, replace `rescueResultCount` and rescue ids from `result.rescueResults` with log/visual/runtime-state signals that already exist after rescue. Prefer runtime unit position/vitals/invincibility plus `RoleEffect` visual events.

In `tests/BattleCoreUnitTests.cpp`, replace `damageRenderApplications` assertions with log event and runtime-state assertions.

- [ ] **Step 8: Verify slice 3**

Run:

```powershell
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_delta],[battle][core],[battle][scenario][runtime]"
rg -n "damageRenderApplications|rescueResults|BattleFrameDamageRenderApplication|BattleFrameDamageRenderUnit|frameResult\\.blinkTeleports|result\\.blinkTeleports" src tests
```

Expected: build succeeds, focused tests pass, and deleted result fields/types no longer exist. `BattleActionCommitResult::blinkTeleports` may still exist for action-commit rule tests.

## Task 4: Move Attack Events To Private Frame Scratch

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/BattleRuntimeScenarioTestHelpers.h`
- Modify: `tests/BattleRuntimeScenarioUnitTests.cpp`

- [ ] **Step 1: Add private attack-event scratch**

In private `BattleFrameContext` in `src/battle/BattleCore.cpp`, add:

```cpp
std::vector<BattleAttackEvent> attackEvents;
```

Do not add it to `BattleFrameResult`.

- [ ] **Step 2: Route attack advancement through private scratch**

In `advanceAttacksAndResolveHits(...)`, replace `result.attackEvents` writes and reads with `frame.attackEvents`.

Keep all existing behavior:

```cpp
frame.attackEvents.push_back(attackSystem.spawn(state.attacks, request));
frame.attackEvents.insert(... tickEvents ...);
applyProjectileCancelDamageResults(state, frame.attackEvents, frameCommands);
appendProjectileCancellationLogEvents(state.attacks, frame.attackEvents, logEvents, false);
resolveHitEvents(state, frame.attackEvents, frame.hitResults, frameCommands, logEvents, visualEvents);
```

In `BattleFrameRunner::runFrame()`, change the late projectile cancellation log call to:

```cpp
appendProjectileCancellationLogEvents(state.attacks, frame.attackEvents, frame.logEvents, true);
```

In `emitPresentationFrame(...)`, iterate over `frame.attackEvents` instead of `result.attackEvents`.

- [ ] **Step 3: Delete public `attackEvents`**

In `src/battle/BattleCore.h`, delete:

```cpp
std::vector<BattleAttackEvent> attackEvents;
```

- [ ] **Step 4: Update projectile and attack tests**

Replace `result.attackEvents` assertions with `result.frame.gameplayEvents` and `result.frame.visualEvents`.

Common mappings:

```cpp
BattleAttackEventType::AttackSpawned -> BattleGameplayEventType::AttackSpawned and BattleVisualEventType::ProjectileSpawned
BattleAttackEventType::Moved -> BattleGameplayEventType::ProjectileMoved and BattleVisualEventType::ProjectileMoved
BattleAttackEventType::Hit -> BattleGameplayEventType::ProjectileHit and BattleVisualEventType::ProjectileHit
BattleAttackEventType::Expired -> BattleGameplayEventType::ProjectileExpired and BattleVisualEventType::ProjectileExpired
BattleAttackEventType::TargetLost -> BattleGameplayEventType::ProjectileCancelled and BattleVisualEventType::ProjectileTargetLost
BattleAttackEventType::ProjectileCancel -> BattleGameplayEventType::ProjectileCancelled and BattleVisualEventType::ProjectileCancelled
BattleAttackEventType::Bounce -> BattleGameplayEventType::AttackSpawned and BattleVisualEventType::ProjectileBounced plus a `ProjectileSpawned` visual event
BattleAttackEventType::BlockedByInvincible -> BattleGameplayEventType::StatusApplied and no projectile visual event
```

For projectile cancel damage, assert the ordered projectile cancel log:

```cpp
auto cancelLog = std::ranges::find_if(result.frame.logEvents, [](const auto& log)
{
    return log.category == KysChess::Battle::BattleLogCategory::ProjectileCancel;
});
REQUIRE(cancelLog != result.frame.logEvents.end());
CHECK(cancelLog->amount == 17);
CHECK(cancelLog->secondaryAmount == 10);
```

If `<ranges>` is not already included in the file, use `std::find_if`.

In `tests/BattleRuntimeScenarioTestHelpers.h`, populate attack/projectile digest fields from `result.frame.gameplayEvents` and `result.frame.visualEvents`, not raw attack events.

- [ ] **Step 5: Verify slice 4**

Run:

```powershell
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][scenario][runtime]"
rg -n "result\\.attackEvents|frameResult\\.attackEvents|BattleFrameResult.*attackEvents" src tests
```

Expected: build succeeds, focused tests pass, and no public `BattleFrameResult::attackEvents` consumer remains.

## Task 5: Move Scene Impact Playback To Visual Events

**Files:**
- Modify: `src/battle/BattlePresentation.h`
- Modify: `src/battle/BattlePresentation.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneImpactPlayer.h`
- Modify: `src/BattleSceneImpactPlayer.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleSceneImpactPlayerUnitTests.cpp`
- Modify: `tests/BattlePresentationUnitTests.cpp`
- Modify: `tests/BattlePresentationPlaybackUnitTests.cpp`

- [ ] **Step 1: Add impact fields to `BattleVisualEvent`**

In `src/battle/BattlePresentation.h`, add fields near existing presentation payload fields:

```cpp
int impactEffectSoundId = -1;
int impactUnitShake = 0;
int impactSceneShake = 0;
bool impactRumble = false;
```

These fields are meaningful only for `BattleVisualEventType::ProjectileHit`.

- [ ] **Step 2: Validate impact fields for projectile hit events**

In `assertValidEvent(const BattleVisualEvent& event)` in `BattlePresentation.cpp`, inside `ProjectileHit`, add:

```cpp
assert(event.impactUnitShake >= 0);
assert(event.impactSceneShake >= 0);
```

Do not require an effect sound id, because existing hit events may have no sound.

- [ ] **Step 3: Populate impact fields from private attack events**

In `toVisualEvents(const BattleAttackEvent& event, const BattleAttackState& world)`, for `BattleAttackEventType::Hit`, preserve existing projectile visual data and add:

```cpp
presentation.impactEffectSoundId = event.skillEffectId;
if (event.scriptedDamage > 0 || event.scriptedStunFrames > 0 || event.scriptedBleedStacks > 0)
{
    presentation.impactUnitShake = 5;
}
else
{
    presentation.impactSceneShake = event.ultimate ? 10 : 0;
    presentation.impactUnitShake = event.ultimate ? 10 : 5;
    presentation.impactRumble = event.operationType != BattleOperationType::None;
}
```

Do not move this logic into the scene. Runtime already has the attack payload; the scene should only play the presentation event.

- [ ] **Step 4: Change impact planner/player inputs**

In `BattleSceneImpactPlayer.h`, remove the dependency on `battle/BattleAttackSystem.h` and include `battle/BattlePresentation.h`.

Change planner signature:

```cpp
std::vector<BattleSceneImpactCommand> plan(
    const std::vector<KysChess::Battle::BattleVisualEvent>& events) const;
```

Change player signature:

```cpp
void play(
    const KysChess::Battle::BattlePresentationFrame& frame,
    const Bindings& bindings) const;
```

Delete the overload that accepts `BattleFrameResult`.

In `BattleSceneImpactPlayer.cpp`, plan only `BattleVisualEventType::ProjectileHit` events:

```cpp
if (event.type != KysChess::Battle::BattleVisualEventType::ProjectileHit)
{
    continue;
}
BattleSceneImpactCommand command;
command.unitId = event.targetUnitId;
command.effectSoundId = event.impactEffectSoundId;
command.unitShake = event.impactUnitShake;
command.sceneShake = event.impactSceneShake;
command.rumble = event.impactRumble;
commands.push_back(command);
```

- [ ] **Step 5: Update scene call site**

In `BattleSceneHades::applyCoreFrameResult(...)`, replace:

```cpp
impact_player_.play(frameResult, { ... });
```

with:

```cpp
impact_player_.play(frameResult.frame, { ... });
```

- [ ] **Step 6: Update impact tests**

In `tests/BattleSceneImpactPlayerUnitTests.cpp`, build `BattleVisualEvent` inputs:

```cpp
KysChess::Battle::BattleVisualEvent hit;
hit.type = KysChess::Battle::BattleVisualEventType::ProjectileHit;
hit.targetUnitId = 1;
hit.effectId = 10;
hit.impactUnitShake = 5;
hit.impactSceneShake = 0;
hit.impactRumble = true;
```

Assert the same `BattleSceneImpactCommand` values currently asserted.

- [ ] **Step 7: Update presentation tests for aggregate initialization**

Because `BattleVisualEvent` gains fields, tests using aggregate initializers may need explicit construction instead of positional initializers.

Prefer named assignment in touched tests:

```cpp
KysChess::Battle::BattleVisualEvent event;
event.type = KysChess::Battle::BattleVisualEventType::ProjectileHit;
event.frame = 18;
event.sourceUnitId = 1;
event.targetUnitId = 2;
event.effectId = 10;
event.durationFrames = 30;
event.position = { 11, 12, 0 };
event.visualEffectId = 33;
event.velocity = { 2, 0, 0 };
event.operationKind = 2;
event.impactUnitShake = 5;
```

Do this only where aggregate initializer breakage occurs.

- [ ] **Step 8: Verify slice 5**

Run:

```powershell
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_impact],[battle][presentation],[battle][core]"
rg -n "BattleSceneImpactPlayer.*BattleFrameResult|frameResult\\.attackEvents|BattleAttackEvent" src\\BattleSceneImpactPlayer.* tests\\BattleSceneImpactPlayerUnitTests.cpp
```

Expected: build succeeds, focused tests pass, and scene impact no longer depends on raw attack events.

## Task 6: Final Cleanup And Verification

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify tests touched by earlier tasks

- [ ] **Step 1: Confirm final result shape**

Run:

```powershell
rg -n "struct BattleFrameResult|attackEvents|unitApplications|movementPresentationResults|blinkTeleports|damageRenderApplications|rescueResults|stateApplications|knockbacks|mpRestores|unitHeals|lastAttackers" src\\battle\\BattleCore.h src\\battle\\BattleCore.cpp src\\BattleScene*.cpp src\\BattleScene*.h tests
```

Expected: deleted result fields and deleted application subchannels do not appear. `BattleActionCommitResult::blinkTeleports` may still appear in `BattleCastSystem.h/.cpp` and action-commit tests.

- [ ] **Step 2: Confirm no scene helper consumes full frame result except orchestration**

Run:

```powershell
rg -n "BattleFrameResult|frameResult\\." src\\BattleScene*.cpp src\\BattleScene*.h
```

Expected: `BattleSceneHades` may still receive the runtime result and pass `frameResult.frame` / `frameResult.applications`; `BattleSceneFrameDeltaBuilder` and `BattleSceneImpactPlayer` should not take `BattleFrameResult`.

- [ ] **Step 3: Run final verification**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_delta],[battle][scene_impact],[battle][presentation]"
x64\Debug\kys_tests.exe "[battle][scenario][runtime]"
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner]"
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected: diff check exits 0, focused tests pass, full tests pass, and the debug build completes. A final link failure is acceptable only if caused by the game executable already running.

- [ ] **Step 4: Commit**

Use a single commit if the implementation stayed coherent:

```powershell
git add src tests docs/superpowers/plans/2026-05-24-battle-frame-result-diet.md
git commit -m "refactor: narrow battle frame result"
```

If the work is split into reviewable commits, use these commit messages:

```powershell
git commit -m "refactor: remove battle frame test publication channels"
git commit -m "refactor: move battle scene delta to presentation events"
git commit -m "refactor: keep battle attack events frame-local"
```

## Implementation Notes

- Do not add new public result channels to replace deleted ones.
- Do not create new bridge/player/builder classes.
- Do not move large code blocks into new files just to reduce `BattleCore.cpp` line count.
- Do not add boundary `if` checks for impossible state; keep existing invariant style with `assert`.
- Use traditional Chinese for any new source strings.
- Prefer runtime state assertions and scenario digests over diagnostic result payload assertions.
- Keep `damageTransactions` until a later plan specifically migrates damage-rule tests away from it.
