# Battle Runtime Phase 5 Scene Simplification Breakdown Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** Complete. Verification gate passed with `git diff --check`, targeted battle runtime/presentation suites, full `kys_tests.exe`, and `.github\build-command.ps1`.

**Goal:** Simplify `BattleSceneHades` by deleting scene-side synchronization glue that is no longer authoritative, while keeping real presentation/report ownership intact.

**Architecture:** Runtime remains the only gameplay frame writer. The scene may read runtime state for rendering and post-battle summaries, and may map runtime frame output into visual/audio/camera/report side effects. Phase 5 should remove scene-side mini-state copies, fallback gameplay inference, and redundant result side channels before considering helper deletion.

**Tech Stack:** C++20, Catch2, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Research Summary

Phase 5 is ready to implement, but it should be executed as small deletion-first slices. The current scene boundary is much narrower than the old `BattleSceneHades` monolith, and most helper classes now have real presentation/report responsibilities. The useful cleanup is not "merge everything back into Hades"; it is deleting the few remaining bridge paths that duplicate or repackage runtime-owned facts.

The production frame-result consumers are:

| Consumer | Current inputs | Classification | Phase 5 decision |
| --- | --- | --- | --- |
| `BattleSceneHades::applyCoreFrameResult` | `BattleFrameResult` | Scene orchestration | Modify to stop copying frame presentation output into `presentation_recorder_`. |
| `BattleSceneFrameDeltaBuilder` | `frame.gameplayEvents`, `damageRenderApplications`, `applications`, `blinkTeleports`, `rescueResults` | Scene visual/audio/camera mapping | Keep, but make it pure: return hurt-flash commands instead of mutating `hurt_flash_timers_` through context. |
| `BattleSceneReportPlayer` | `frame.logEvents`, `projectileCancelDamageCommands` | Report playback | Keep log playback; delete `playProjectileCancelDamageCommands` after projectile-cancel report amounts are carried by the log event. |
| `BattleScenePresentationPlayer` | `BattlePresentationFrame.visualEvents` | Visual/text/projectile playback | Keep. It maps ordered runtime visual events to scene effect queues. |
| `BattleSceneImpactPlayer` | `attackEvents` | Hit impact presentation | Keep for now. It owns hit sound/shake/rumble decisions that are not yet represented by ordered visual/audio commands. |
| `BattleSceneUnitStore` | runtime session pointer plus presentation identity/shake | Read-only runtime view plus presentation sidecar | Keep. Delete only `aliveUnitsOnTeam()` if `checkResult()` stops inferring battle results. |

Exact things we can modify/delete now:

- Delete `BattleSceneHades` result fallback logic that counts alive teams. Runtime already emits `BattleGameplayEventType::BattleEnded` and stores the result.
- Delete `BattleSceneUnitStore::aliveUnitsOnTeam()` if no remaining caller exists after the result fallback is removed.
- Delete `BattleSceneHades`'s `presentation_recorder_` / `last_presentation_frame_` mini-state for the runtime frame path. `BattleFrameResult::frame` is already the ordered presentation frame.
- Delete `BattleSceneReportPlayer::playProjectileCancelDamageCommands()` and `BattleFrameResult::projectileCancelDamageCommands` after the projectile-cancel log event carries both cancel-damage amounts.
- Modify `BattleSceneFrameDeltaBuilder` so it returns hurt-flash commands rather than mutating `hurt_flash_timers_` while "building" a delta.
- Simplify `SceneBattleFrameResult` / `applyLegacyBattleFrameResult()` after presentation-frame playback no longer depends on recorder state.

Things not to delete in the first phase 5 pass:

- Do not delete `BattleScenePresentationPlayer`; it is not a forwarding helper.
- Do not delete `BattleSceneReportPlayer`; report building is still a real scene-side playback responsibility.
- Do not delete `BattleSceneImpactPlayer` until runtime emits hit impact presentation commands that cover unit shake, scene shake, effect sound, and rumble.
- Do not delete `BattleSceneFrameDeltaBuilder` while it owns blood effects, death/battle-end camera timing, slow/frozen frames, blink sound effects, and scene shake.
- Do not delete test/debug fields such as `unitApplications`, `movementPresentationResults`, `damageTransactions`, or `stateApplications` as part of scene simplification unless their tests are migrated to runtime state or scenario digests in the same slice.

---

## Task 1: Remove Scene-Side Battle Result Inference

**Files:**

- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneUnitStore.h`
- Modify: `src/BattleSceneUnitStore.cpp`
- Modify: `tests/BattleSceneUnitStoreUnitTests.cpp`

- [x] **Step 1: Remove the fallback team-count logic**

Change `BattleSceneHades::checkResult()` so it returns `result_` directly. `result_` is updated from runtime `BattleEnded` output by `applySceneFrameDelta()`.

Expected final shape:

```cpp
int BattleSceneHades::checkResult()
{
    return result_;
}
```

- [x] **Step 2: Delete the now-unused unit-store query**

Delete:

```cpp
int BattleSceneUnitStore::aliveUnitsOnTeam(int team) const;
```

Also delete its declaration and the `BattleSceneUnitStore_InitializesDenseRowsAndRequiresByUnitId` assertion that only exists to cover that query.

- [x] **Step 3: Verify no fallback result query remains**

Run:

```powershell
rg -n "aliveUnitsOnTeam|team0|team1" src/BattleSceneHades.cpp src/BattleSceneUnitStore.* tests/BattleSceneUnitStoreUnitTests.cpp
```

Expected: no `aliveUnitsOnTeam` references and no `team0` / `team1` result inference in `BattleSceneHades`.

---

## Task 2: Remove BattleSceneHades Presentation Recorder State

**Files:**

- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [x] **Step 1: Add direct presentation-frame playback**

Replace `beginPresentationFrame()` / `publishPresentationFrame()` with one helper that plays an already-built frame:

```cpp
void BattleSceneHades::playPresentationFrame(
    const KysChess::Battle::BattlePresentationFrame& frame)
{
    report_player_.playLogs(frame.logEvents, {
        &battle_report_,
        &scene_units_,
    });
    presentation_player_.play(frame, {
        &text_effects_,
        &attack_effects_,
        &scene_units_,
    });
}
```

- [x] **Step 2: Play initialization presentation directly**

In `initializeBattleRuntime(...)`, build a `BattlePresentationFrame` from `initialization.logEvents` and `initialization.visualEvents`, then call `playPresentationFrame(frame)`. Do not record each event into `presentation_recorder_`.

- [x] **Step 3: Play runtime frame presentation directly**

In `backRun1()`, keep the runtime frame result local until after `applyLegacyBattleFrameResult(...)`, then call `playPresentationFrame(frameResult.frame)`.

The important ordering to preserve:

1. advance presentation effects
2. run runtime frame
3. apply scene frame delta and impact side effects
4. advance legacy transient scene presentation state
5. play ordered runtime log/visual frame

- [x] **Step 4: Delete recorder members and methods from the scene**

Delete from `BattleSceneHades`:

- `beginPresentationFrame()`
- `publishPresentationFrame()`
- `playCorePresentationFrame()`
- `presentation_recorder_`
- `last_presentation_frame_`

Keep `BattlePresentationRecorder` itself. Runtime core and its tests still use it.

- [x] **Step 5: Verify the scene no longer owns presentation-frame mini-state**

Run:

```powershell
rg -n "presentation_recorder_|last_presentation_frame_|beginPresentationFrame|publishPresentationFrame|playCorePresentationFrame" src/BattleSceneHades.*
```

Expected: no output.

---

## Task 3: Make `BattleSceneFrameDeltaBuilder` Pure

**Files:**

- Modify: `src/BattleSceneFrameDeltaBuilder.h`
- Modify: `src/BattleSceneFrameDeltaBuilder.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleSceneFrameDeltaBuilderUnitTests.cpp`

- [x] **Step 1: Move hurt flash from context mutation to delta output**

Add a small command:

```cpp
struct BattleSceneFrameHurtFlashCommand
{
    int unitId = -1;
    int frames = 0;
};
```

Add to `BattleSceneFrameDelta`:

```cpp
std::vector<BattleSceneFrameHurtFlashCommand> hurtFlashes;
```

Remove `hurtFlashTimers` from `BattleSceneFrameDeltaBuildContext`.

- [x] **Step 2: Return hurt flash commands from the builder**

In `collectDamageSceneEffects(...)`, replace:

```cpp
(*context.hurtFlashTimers)[damage.defender.unitId] = context.hurtFlashDuration;
```

with:

```cpp
result.hurtFlashes.push_back({
    damage.defender.unitId,
    context.hurtFlashDuration,
});
```

- [x] **Step 3: Apply hurt flash commands in the scene**

In `BattleSceneHades::applySceneFrameDelta(...)`, add:

```cpp
for (const auto& command : result.hurtFlashes)
{
    hurt_flash_timers_[command.unitId] = command.frames;
}
```

- [x] **Step 4: Update the frame-delta tests**

Change `BattleSceneFrameDeltaBuilder_CollectsDeathPresentationEffects` to assert `result.hurtFlashes` instead of checking a mutated `hurtFlashTimers` map.

- [x] **Step 5: Verify the builder has no hidden scene mutation**

Run:

```powershell
rg -n "hurtFlashTimers|std::unordered_map<int, int>\\*" src/BattleSceneFrameDeltaBuilder.* tests/BattleSceneFrameDeltaBuilderUnitTests.cpp
```

Expected: no builder context pointer to `hurt_flash_timers_`.

---

## Task 4: Delete Projectile-Cancel Report Side Channel

**Files:**

- Modify: `src/battle/BattlePresentation.h`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneReportPlayer.h`
- Modify: `src/BattleSceneReportPlayer.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/BattleSceneReportPlayerUnitTests.cpp`
- Modify scenario tests only if the scenario digest needs to assert the new log payload explicitly.

- [x] **Step 1: Add a second report amount to log events**

Add to `BattleLogEvent`:

```cpp
int secondaryAmount = 0;
```

Use it for the second side of projectile-cancel report accounting. Do not introduce a projectile-specific report command just for the scene.

- [x] **Step 2: Populate projectile-cancel log payloads**

In `appendProjectileCancellationLogEvents(...)`, keep the current left/right ordering, and set:

```cpp
log.sourceUnitId = leftSourceUnitId;
log.targetUnitId = rightSourceUnitId;
log.amount = leftDamage;
log.secondaryAmount = rightDamage;
log.category = BattleLogCategory::ProjectileCancel;
```

Use the existing `event.sourceUnitId` / `event.otherSourceUnitId` values to derive `leftSourceUnitId` and `rightSourceUnitId` consistently with `leftAttackId` / `rightAttackId`.

- [x] **Step 3: Record cancel stats from log playback**

In `BattleSceneReportPlayer::recordStatus(...)`, before `recordStatus(...)`, add a branch for `BattleLogCategory::ProjectileCancel`:

```cpp
if (event.category == KysChess::Battle::BattleLogCategory::ProjectileCancel)
{
    bindings.report->recordProjectileCancel(event.sourceUnitId, event.amount);
    bindings.report->recordProjectileCancel(event.targetUnitId, event.secondaryAmount);
}
```

Keep the status report event so the battle log still shows the ordered projectile-cancel message.

- [x] **Step 4: Delete the result side channel**

Delete:

- `BattleFrameResult::projectileCancelDamageCommands`
- the `projectileCancelDamageCommands` parameter from `applyProjectileCancelDamageResults(...)`
- `projectileCancelDamageCommands.push_back(command)`
- `BattleSceneHades::applyCoreFrameResult()` call to `report_player_.playProjectileCancelDamageCommands(...)`
- `BattleSceneReportPlayer::playProjectileCancelDamageCommands(...)`

Keep `BattleProjectileCancelDamageCommand` as an internal `BattleGameplayCommand` variant for now. Removing that command is a separate frame-command cleanup.

- [x] **Step 5: Update tests away from the side channel**

Update `BattleFrameRunner_AdvanceFrame_AppliesProjectileCancelDamageCommand` to assert:

- `result.attackEvents` still contains the cancel event with resolved damages
- runtime projectile weaken state is committed
- `result.frame.logEvents` contains a projectile-cancel status log with `amount` and `secondaryAmount`

Update `BattleSceneReportPlayer_RecordsProjectileCancelStats` to call `playLogs(...)` with a projectile-cancel log event instead of calling the deleted side-channel method.

- [x] **Step 6: Verify the side channel is gone**

Run:

```powershell
rg -n "projectileCancelDamageCommands|playProjectileCancelDamageCommands" src tests
```

Expected: no output.

---

## Task 5: Re-Evaluate Helper Classes After Deletions

**Files:**

- Read: `src/BattleSceneHades.h`
- Read: `src/BattleSceneHades.cpp`
- Read: `src/BattleSceneFrameDeltaBuilder.*`
- Read: `src/BattleSceneImpactPlayer.*`
- Read: `src/BattleScenePresentationPlayer.*`
- Read: `src/BattleSceneReportPlayer.*`
- Read: `src/BattleSceneUnitStore.*`
- Modify only if a helper has become a pure pass-through after Tasks 1-4.

- [x] **Step 1: Run the helper-use inventory**

Run:

```powershell
rg -n "frame_delta_builder_|impact_player_|presentation_player_|report_player_|scene_units_|applyCoreFrameResult|applySceneFrameDelta" src/BattleSceneHades.*
```

- [x] **Step 2: Apply the helper retention rules**

Keep helpers that make real decisions:

- `BattleScenePresentationPlayer`: maps playback commands into `BattleTextEffect` / `BattleAttackEffect`.
- `BattleSceneReportPlayer`: maps ordered logs into `BattleReportBuilder`.
- `BattleSceneImpactPlayer`: maps hit events into unit shake, scene shake, sound, and rumble.
- `BattleSceneFrameDeltaBuilder`: maps frame output into blood, hurt flash, blink sound, death/battle-end camera timing, slow/frozen frames, and scene shake.
- `BattleSceneUnitStore`: keeps presentation identity/shake and read-only runtime views together.

Delete or merge only if a helper now forwards data without adding domain meaning. Do not extract rendering chunks from `BattleSceneHades` only for line count.

- [x] **Step 3: Write the phase 5 status back to the direction doc**

Once Tasks 1-5 pass, update `docs/superpowers/plans/2026-05-21-battle-runtime-frame-direction.md` to mark phase 5 complete and list the exact deleted bridge paths.

---

## Verification Gate

Run before declaring phase 5 complete:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_delta],[battle][scene_report],[battle][scene_impact],[battle][presentation]"
x64\Debug\kys_tests.exe "[battle][scenario][runtime]"
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner][runtime][unit]"
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

The existing `[death-kick]` debug output may still appear during tests. That should be cleaned separately unless phase 5 touches that code path directly.

---

## Phase 5 Done Criteria

Phase 5 can be declared done when:

- `BattleSceneHades::checkResult()` no longer infers battle results by counting alive runtime units.
- `BattleSceneUnitStore::aliveUnitsOnTeam()` is deleted unless another non-result presentation use appears.
- `BattleSceneHades` no longer stores `presentation_recorder_` or `last_presentation_frame_`.
- Runtime frame presentation playback consumes `BattleFrameResult::frame` directly.
- `BattleSceneFrameDeltaBuilder` no longer mutates `hurt_flash_timers_` through its build context.
- `BattleFrameResult::projectileCancelDamageCommands` and `BattleSceneReportPlayer::playProjectileCancelDamageCommands()` are deleted.
- Projectile-cancel report stats are recorded from ordered log playback.
- Remaining scene helpers are either retained with a clear presentation/report responsibility or deleted because they became pass-throughs.
- The verification gate passes.
