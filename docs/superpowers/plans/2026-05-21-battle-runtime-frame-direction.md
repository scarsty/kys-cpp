# Battle Runtime Frame Direction

> **For agentic workers:** This is the high-level direction for rescuing the battle refactor. Do not treat this as another extraction plan. The goal is to rebuild the battle frame shape around one gameplay owner, then delete synchronization paths. Update task checkboxes as work lands.

**Goal:** Recover the useful parts of the current refactor without returning to the old monolith. The final battle architecture should have one authoritative runtime state, one imperative frame transaction, and scene code that consumes frame events for rendering, audio, camera, and reports only.

**Core Problem:** The current code reduced `BattleSceneHades`, but it did not reduce the battle model. It created several semi-authoritative models that must be synchronized: runtime state, scene unit sidecar, frame deltas, presentation events, report playback, impact playback, combo globals, and legacy effect queues. The resulting bugs live between these models, so isolated unit tests often miss them.

**Primary Direction:** Stop refactoring by extraction. Refactor by ownership.

A battle frame is one imperative transaction over one runtime state. Subsystems are implementation details inside that transaction. They do not own persistent mirror worlds, and they do not require broad `makeXWorld` / `writeXWorld` synchronization pairs.

---

## Target Shape

```text
BattleSceneHades
  -> gathers player/input/setup commands
  -> calls BattleRuntimeSession::runFrame()
  -> consumes BattleFrameResult for presentation only

BattleRuntimeSession::runFrame()
  -> mutates BattleRuntimeState
  -> runs one ordered BattleFrameRunner transaction
  -> emits gameplay/log/visual/report events

BattleFrameRunner
  -> owns frame ordering
  -> calls small rule/calculation helpers
  -> updates BattleRuntimeState directly
```

The desired frame transaction should look conceptually like this:

```cpp
BattleFrameResult BattleFrameRunner::runFrame(BattleRuntimeState& state)
{
    BattleFrameContext frame;

    tickTimers(state, frame);
    updateMovement(state, frame);
    startActions(state, frame);
    updateProjectiles(state, frame);
    resolveHits(state, frame);
    resolveDeaths(state, frame);
    finishBattleIfNeeded(state, frame);

    return frame.consumeResult();
}
```

The exact function names are not important. The important rule is that ordering is visible in one place, and gameplay mutation happens inside the runtime frame transaction.

---

## Ownership Rules

### Runtime-owned gameplay state

- Unit identity required by gameplay.
- Team, alive/dead, HP, MP, max HP/MP, attack, defence, speed, cost, star.
- Position, velocity, acceleration, facing, grid cell, movement agent state.
- Cooldown, action frame, current action type, operation type, operation count, physical power.
- Statuses and combo state that affect gameplay decisions.
- Projectile/attack instances and pending attack spawns.
- Damage, hit, death, rescue, battle-end, resource, and cooldown consequences.
- Battle result and frame number.

### Scene-owned presentation state

- Camera center, manual camera drag state, zoom/focus, screen shake, scene slow/freeze effects.
- `BattleAttackEffect` and `BattleTextEffect` queues used only for rendering.
- Hurt flash timers and other transient visual overlays.
- Display metadata that runtime does not need: head id, fight frames, skill display names, equipment display ids.
- Pre-battle selection UI state before setup is committed.

### Explicit value transfer only

- Setup input from scene/config into runtime creation.
- Input commands from scene into runtime before a frame.
- Frame events/results from runtime into scene after a frame.
- Post-battle summary generated from runtime state plus display metadata/report data.

---

## Non-Negotiables

- Do not add new bridge layers unless an old bridge layer is deleted in the same change.
- Avoid `makeXWorld` / `writeXWorld` pairs for persistent gameplay domains. They are synchronization bugs by design unless the input is a narrow calculation value. A subsystem-owned copy of `BattleRuntimeState`, `BattleUnitStore`, or a runtime-shaped unit vector is still a mirror world even when it only lives for one frame.
- Scene-side code must not mutate gameplay after runtime initialization, except by sending explicit input/setup commands to runtime.
- `BattleSceneFrameDeltaBuilder` and any successor must be visual/audio/camera/report mapping only. It must not clear combo flags, transfer combo state, apply damage, alter resources, or infer gameplay outcomes.
- Unit tests for isolated systems are not enough. Every meaningful migration needs at least one frame-level scenario test or an existing scenario test updated to cover the full composition.
- Rebuilding `BattleCore` is allowed and expected. The rescue should not be limited to moving more code into smaller files.

---

## Current Anti-Patterns To Remove

1. **Runtime event interpreted as gameplay by scene glue**
   - Example smell: runtime emits a death, then scene-side delta code clears combo flags or transfers anti-combo.
   - Direction: death consequences belong in `BattleFrameRunner`; scene receives already-finished events.

2. **Subsystem state projected into mirror worlds**
   - Example smell: build attack/damage/status/cast mini-worlds from runtime, run a system, then write back.
   - Direction: use small rule inputs for calculations, or mutate `BattleRuntimeState` directly inside the frame transaction.

3. **Good-looking units with unclear composition order**
   - Example smell: each extracted system has reasonable tests, but the combined battle breaks.
   - Direction: `BattleFrameRunner` owns ordering. Tests target frame sequences through `BattleRuntimeSession::runFrame()`.

4. **Scene result reconstruction**
   - Example smell: scene code looks at raw damage/applications/actions and reconstructs logs, camera, sounds, and gameplay side effects.
   - Direction: runtime emits typed frame events. Scene maps those events to presentation.

5. **Compatibility paths that keep old and new models alive**
   - Example smell: names like legacy result, frame delta, scene unit store, runtime unit store all remain active for the same fact.
   - Direction: keep one model for gameplay facts. Delete transitional code as each fact migrates.

---

## Actionable Plan

### Phase 0: Declare The New Boundary

**Goal:** Make future changes converge instead of continuing the current drift.

**Files:**

- Modify this document as decisions become concrete.
- Use `src/battle/BattleRuntimeSession.h/.cpp`, `src/battle/BattleCore.h/.cpp`, and `src/BattleSceneHades.h/.cpp` as the first boundary map.

- [x] **Step 1: Name `BattleRuntimeSession::runFrame()` as the only gameplay frame path**

Document in code comments or API naming that callers may only advance gameplay through `BattleRuntimeSession::runFrame()` after initialization.

- [x] **Step 2: Classify every post-frame scene call**

Audit the code after `battle_session_->runFrame()` in `BattleSceneHades`. Classify each call as one of:

- visual
- audio
- camera
- report/log playback
- illegal gameplay mutation

Illegal gameplay mutation must move into runtime before the migration is considered complete.

Phase 0 classification after the `BattleSceneHades::backRun1()` `battle_session_->runFrame()` call at the start of this plan. Several bridge paths listed here were intentionally deleted later, especially in phase 5.

| Call | Classification | Phase 1 consequence |
| --- | --- | --- |
| `applyCoreFrameResult(frameResult)` | Mixed presentation bridge | Keep the call as the frame-result fanout for now, but its internals must become presentation/report only. |
| `frame_delta_builder_.build(frameResult, result_, context)` inside `applyCoreFrameResult` | Visual, audio, camera, and illegal gameplay mutation | Illegal pieces are `context.comboStates` clearing `onSkillTeamHealPending` on death and `context.transferAntiCombo` calling `ChessCombo::transferAntiCombo`, which mutates global combo state and fabricates the `獨行轉移` log. Move those death consequences into runtime. |
| `applySceneFrameDelta(...)` | Visual, audio, camera, scene battle-result consumption | Blood effects, hurt flash, sounds, rumble, camera focus, freeze/slow/shake are presentation. `result_` is a scene/post-battle mirror of the runtime battle-end event, not a gameplay writer, but should stay explicit as runtime result consumption. |
| `report_player_.playProjectileCancelDamageCommands(...)` | Report/log playback | Acceptable scene-side report update. Runtime already decided the cancel-damage facts. |
| Recording `frameResult.frame.visualEvents` into `presentation_recorder_` | Visual | Acceptable presentation staging. |
| Recording `frameResult.frame.logEvents` into `presentation_recorder_` | Report/log playback | Acceptable log staging. |
| `impact_player_.play(frameResult, ...)` | Visual, audio, camera/rumble | Sets unit/scene shake and plays hit sounds/rumble from runtime attack events. No gameplay state mutation found. |
| `applyLegacyBattleFrameResult(result)` | Visual housekeeping and post-battle scene lifecycle | `scene_units_.decreaseTransientPresentationState()` and text-effect aging are presentation. `calExpGot()`/`setExit(true)` run after the runtime result has been consumed and are post-battle scene/progression handling, not frame gameplay mutation. |
| `playCorePresentationFrame()` | Visual and report/log playback | `publishPresentationFrame()` forwards recorded logs to `BattleSceneReportPlayer` and visuals to `BattleScenePresentationPlayer`. No gameplay state mutation found. |

- [x] **Step 3: Freeze extraction-only refactors**

Do not create new scene players, builders, appliers, adapters, or subsystem DTOs unless the same change deletes an older bridge or makes a gameplay owner unambiguous.

Phase 0 decision: no new scene bridge/helper should be added for phase 1. The next change should delete the mutable gameplay bindings from `BattleSceneFrameDeltaBuildContext` and move the death combo/anti-combo consequences into the runtime frame transaction.

---

### Phase 1: Make Scene Delta Presentation-Only

**Goal:** Remove scene-side gameplay authority before rebuilding deeper core code.

**Files:**

- Modify: `src/BattleSceneFrameDeltaBuilder.h`
- Modify: `src/BattleSceneFrameDeltaBuilder.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/battle/BattleCore.h/.cpp`
- Modify tests around death/combo/anti-combo/frame runner behavior.

- [x] **Step 1: Remove mutable gameplay bindings from `BattleSceneFrameDeltaBuildContext`**

Remove or stop using fields that allow scene delta construction to mutate gameplay:

```cpp
std::map<int, KysChess::RoleComboState>* comboStates;
std::function<std::vector<KysChess::AntiComboTransferEvent>(int)> transferAntiCombo;
```

If these are still needed, runtime must emit the result as events/logs instead.

Completed in phase 1: `BattleSceneFrameDeltaBuildContext` no longer exposes combo state or anti-combo transfer callbacks, and `BattleSceneFrameDelta` no longer carries scene-fabricated log events.

- [x] **Step 2: Move death gameplay consequences into runtime**

Move death-triggered combo cleanup and anti-combo transfer into the hit/death resolution portion of the runtime frame transaction. Runtime should emit log events such as `獨行轉移`; scene should only play them.

Completed in phase 1: runtime death resolution now clears dead-unit `onSkillTeamHealPending`, transfers anti-combo applied effects to the strongest eligible living ally from runtime state, and emits the `獨行轉移` log through the runtime presentation frame.

- [x] **Step 3: Keep death visuals in scene delta**

Keep camera focus, shake, slow motion, blood effects, and hurt flash in scene-side mapping. These are presentation effects and may remain outside runtime.

Completed in phase 1: `BattleSceneFrameDeltaBuilder` still owns blood effects, hurt flash, death camera focus, freeze, slow, and scene shake only.

- [x] **Step 4: Add frame-level scenario coverage for one death case**

Add or update a `BattleRuntimeSession::runFrame()` scenario that proves death consequences happen before the scene consumes the frame result.

Completed in phase 1: `BattleRuntimeSession_RunFrame_AppliesDeathComboConsequencesBeforeSceneConsumption` covers the runtime death combo cleanup, anti-combo transfer, and `獨行轉移` log before scene consumption.

---

### Phase 2: Rebuild `BattleCore` Around A Frame Transaction

**Goal:** Turn `BattleCore` from a migration container into an ordered runtime transaction.

**Planning status:** Phase 2 is intentionally split into a dedicated research and breakdown plan before implementation:

- See [2026-05-22-battle-runtime-frame-transaction-breakdown.md](2026-05-22-battle-runtime-frame-transaction-breakdown.md).
- Do not treat the bullets below as one implementation task.
- The only currently ready implementation slice is phase 2A: introduce a one-frame `BattleFrameContext` mechanically without changing behavior, deleting result channels, or collapsing subsystem APIs.
- Phase 2B and later require the inventories in the dedicated plan: helper signature routing, `BattleFrameResult` consumer audit, mirror-world synchronization classification, and scenario coverage gates.

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: subsystem headers only as needed to remove mirror-world APIs.
- Update: `tests/BattleCoreUnitTests.cpp`, `tests/BattleFrameRunnerRuntimeUnitTests.cpp`, and focused subsystem tests.

- [x] **Step 1: Introduce `BattleFrameContext`**

Create a local/context type owned by the frame runner. It collects frame events, logs, visual events, applications, report commands, and temporary per-frame scratch data.

This context should not become a second persistent world. It exists for one `runFrame()` call only.

Completed in phase 2: `BattleFrameContext` is private to `BattleCore.cpp` and owns one-frame movement, action, hit, team-effect, effect-command, and motion snapshot scratch.

- [x] **Step 2: Make frame ordering explicit**

Restructure `BattleFrameRunner::advanceFrame()` or its replacement so the top-level function visibly owns ordering:

```cpp
tickUnitFrameState(...);
runMovement(...);
commitActions(...);
spawnPendingAttacks(...);
tickAttacks(...);
resolveHits(...);
resolveDeaths(...);
resolveBattleEnd(...);
emitFramePresentation(...);
```

Do not hide ordering across callbacks that mutate state from multiple subsystems.

Completed in phase 2: `BattleFrameRunner::runFrame()` keeps the visible ordered transaction and routes top-level frame helpers through the context instead of parallel vector plumbing.

- [x] **Step 3: Convert subsystem APIs to rule helpers or direct mutators**

For each subsystem, choose one shape:

- stateless calculation helper returning a value/result
- direct runtime mutator called by the frame runner
- one-frame local helper that cannot persist state

Avoid subsystem APIs that require copying a runtime-shaped world in and writing it back out.

Completed in phase 2: movement, damage, action, hit, rescue, and team/effect runtime mutation boundaries are named or narrowed; remaining scene-facing channels are explicitly kept for later scene-contract work.

- [x] **Step 4: Collapse duplicated event/application channels**

Audit `BattleFrameResult` for overlapping ways to describe the same thing. Prefer ordered frame events with enough information for scene/report consumers over several parallel vectors that must be correlated later.

Completed in phase 2: public diagnostic channels for movement physics, movement ticks, action results, hit results, team-effect events, and effect commands were removed or narrowed. Scene-facing `attackEvents`, `damageRenderApplications`, and `rescueResults` remain public where consumers still require them.

- [x] **Step 5: Keep `BattleCore.cpp` allowed to be imperative**

Do not split code only because the file is long. Split only when it reduces persistent state ownership or makes a pure rule reusable. A readable ordered transaction is better than many attractive pieces with hidden composition bugs.

Completed in phase 2: `BattleCore.cpp` remains the imperative transaction owner; no new subsystem bridge was added for line-count reasons.

---

### Phase 3: Remove Mirror-World Synchronization Pairs

**Goal:** Delete the pattern that produced the current bugs.

**Implementation status:** Phase 3 is complete for the frame-time mirror synchronization pairs identified in the phase 3 breakdown. The first local-copy implementation direction was rejected after review and unwound.

- See [2026-05-23-battle-runtime-phase-3-mirror-sync-breakdown.md](2026-05-23-battle-runtime-phase-3-mirror-sync-breakdown.md).
- The corrected phase 3 rule is: pure calculation kernels below, frame-runner-owned runtime reducers above, no broad frame-time mirror state in the middle.
- Do not fix hidden writeback by copying full runtime stores into a subsystem-owned working state. That only changes the location of the synchronization bug and adds copy cost.
- Completed reducer cleanup:
  - `BattleDamageApplicationSystem` was deleted; damage application now reduces pending damage inside `BattleCore.cpp::applyDamageAndLifecycle()`.
  - pending-damage execute/block reactions now happen at damage reduction time against authoritative committed runtime state.
  - movement planning no longer mutates a caller-owned world or returns `result.units`; it consumes a private `BattleMovementPlanInput` value and returns decisions/reservations for `commitFrameMovement()`.
  - action frame results no longer carry copied action state; action fields are committed back to `BattleRuntimeUnit` inside the frame runner.
- Remaining acceptable conversions:
  - `BattleMovementPlanInput` is still a copied planning value. That is acceptable for phase 3 because it is not authoritative and has no writeback pair. Future performance work may narrow it to views, but ownership is no longer hidden there.
  - damage/status make/write helpers still exist for setup, tests, and immediate single-transaction reducer commits. They must not be reused as broad subsystem synchronization APIs.
- Verification on 2026-05-23: `git diff --check`, `.github\build-command.ps1 -Target kys_tests`, focused battle tests, full test suite, and `.github\build-command.ps1` all passed.

**Files:**

- Search-driven across `src/battle/` and `src/BattleScene*.cpp`.

- [x] **Step 1: Inventory all conversion pairs**

Run:

```powershell
rg -n "make.*World|write.*World|make.*State|write.*State|make.*Input|apply.*Snapshot|apply.*Frame" src/battle src/BattleScene*.cpp src/BattleScene*.h
```

Classify each match as:

- harmless pure calculation
- setup-time conversion
- one-frame local scratch
- persistent mirror synchronization

Completed in phase 3 inventory: the PowerShell-safe equivalent search found 123 matches across battle and `BattleScene*` files. The rejected 3A local-copy approach was unwound; no frame-time mirror pair should be considered removed until it is collapsed into a frame-runner-owned reducer or narrowed to a true calculation input. The identified frame-time mirror pairs have now been collapsed or narrowed: damage application, pending-damage preview, movement planner input/writeback, and action state result/writeback no longer use subsystem-owned runtime copies as authority. Scene `apply*Frame` calls are still classified as presentation/report synchronization and are deferred until phase 5 unless a runtime result bridge is deleted in the same change.

- [x] **Step 2: Collapse persistent mirror synchronization into runtime reducers**

For each persistent mirror pair, either mutate `BattleRuntimeState` directly in the visible `BattleFrameRunner` order or reduce the helper to a pure calculation input that contains only the fields needed for one calculation. Do not replace a writeback pair with a broad local copy plus state-shaped output.

Completed in phase 3: damage is a frame-runner-owned reducer, movement decisions are reduced by `commitFrameMovement()`, and action state is committed inside `advanceActionFrameUnits()`. `BattleMovementPlanInput` remains a private planning value, not an authoritative frame-time mirror.

- [x] **Step 3: Keep setup-time conversion narrow**

Setup conversion is acceptable when converting from config/scene setup into runtime creation input. It must not remain active during frame execution.

Completed in phase 3: setup/test conversion helpers remain, and single-transaction damage/status conversion is used immediately by the frame reducer. Do not re-expand these helpers into a subsystem-owned working state.

---

### Phase 4: Replace Isolated Confidence With Scenario Confidence

**Goal:** Catch composition bugs where they happen.

**Implementation status:** Phase 4 is complete for the scenario-confidence goal defined in [2026-05-23-battle-runtime-phase-4-scenario-confidence-breakdown.md](2026-05-23-battle-runtime-phase-4-scenario-confidence-breakdown.md).

- Added `tests/BattleRuntimeScenarioTestHelpers.h` with a compact runtime/frame digest for scenario assertions.
- Added `tests/BattleRuntimeScenarioUnitTests.cpp` with `[battle][scenario][runtime]` coverage for:
  - basic `BattleRuntimeSession::createInitialized()` frame digest progression
  - multi-frame damage, death effects, death AOE follow-up, and next-frame damage
  - action/cast/projectile/damage composition and action-state cleanup
  - projectile cancellation runtime/presentation digest
  - rescue during damage lifecycle
- Verification on 2026-05-23: `git diff --check`, `.github\build-command.ps1 -Target kys_tests`, `[battle][scenario][runtime]`, focused runtime/breakthrough tests, full test suite, and `.github\build-command.ps1` all passed.

**Files:**

- Add/update tests under `tests/` using `BattleRuntimeSession` and `BattleFrameRunner`.

- [x] **Step 0: Break down the scenario test plan**

Completed in planning: see [2026-05-23-battle-runtime-phase-4-scenario-confidence-breakdown.md](2026-05-23-battle-runtime-phase-4-scenario-confidence-breakdown.md). The plan lists:

- existing `BattleRuntimeSession` / `BattleFrameRunner` scenario coverage to keep
- recent bug fixes that deserve frame-level protection
- the minimal fixture/helper changes needed to express scenarios without duplicating setup
- exact scenario digests to assert, such as alive unit ids, HP/MP, pending attack count, event/log sequence, and battle result
- execution order so each scenario can be added and verified independently

- [x] **Step 1: Build fixed-seed frame scenarios**

Create small scenario tests that initialize a real runtime-ish battle, run N frames, and assert a compact digest:

- alive unit ids
- HP/MP totals or key unit vitals
- death count
- battle result
- important log/event sequence
- projectile count or no leaked pending attacks

- [x] **Step 2: Port known bug fixes into scenarios**

For each recent battle bug fix worth keeping, add one scenario or adapt an existing scenario so the behavior is protected at the frame level.

Completed in phase 4: phase 3 reducer risks are now covered by scenario digest tests without deleting the narrower rule/unit tests.

- [x] **Step 3: Keep unit tests for rules, not orchestration confidence**

Small unit tests are still useful for damage math, cast planning, movement probing, and formatting. They should not be the main proof that battle composition works.

Completed in phase 4: new scenario tests were added in a separate file; existing subsystem and frame-runner unit tests remain in place.

---

### Phase 5: Simplify `BattleSceneHades` Only After Runtime Ownership Is Stable

**Goal:** Avoid repeating the previous mistake of making the scene file look cleaner while pushing complexity elsewhere.

**Implementation status:** Phase 5 is complete. The detailed breakdown lives in [2026-05-23-battle-runtime-phase-5-scene-simplification-breakdown.md](2026-05-23-battle-runtime-phase-5-scene-simplification-breakdown.md).

Deleted bridge paths:

- Removed `BattleSceneHades::checkResult()` alive-team fallback and deleted `BattleSceneUnitStore::aliveUnitsOnTeam()`.
- Removed `BattleSceneHades`'s `presentation_recorder_` / `last_presentation_frame_` state; initialization and runtime playback now consume `BattlePresentationFrame` directly.
- Removed `BattleFrameResult::projectileCancelDamageCommands` and `BattleSceneReportPlayer::playProjectileCancelDamageCommands()`; projectile-cancel report stats are recorded from ordered log playback via `BattleLogEvent::secondaryAmount`.
- Changed `BattleSceneFrameDeltaBuilder` to return hurt-flash commands in `BattleSceneFrameDelta` instead of mutating `hurt_flash_timers_` through its build context.
- Re-evaluated scene helpers and kept `BattleScenePresentationPlayer`, `BattleSceneReportPlayer`, `BattleSceneImpactPlayer`, `BattleSceneFrameDeltaBuilder`, and `BattleSceneUnitStore` because they still own presentation/report mapping rather than pure forwarding.

**Files:**

- Modify: `src/BattleSceneHades.h/.cpp`
- Modify/delete scene-side helper files only when runtime boundaries are settled.

- [x] **Step 1: Remove scene calls that exist only for gameplay synchronization**

After runtime owns gameplay consequences, delete scene-side glue whose only job was to reconcile models.

- [x] **Step 2: Keep rendering and camera code close enough to read**

Do not extract rendering chunks into new files solely for line count. Extract only when the target has a clear presentation ownership and no gameplay authority.

- [x] **Step 3: Re-evaluate helper classes**

Once runtime emits cleaner frame events, re-check whether `BattleSceneFrameDeltaBuilder`, `BattleSceneImpactPlayer`, `BattleSceneReportPlayer`, and `BattleScenePresentationPlayer` are still pulling their weight. Delete or merge helpers that only forward data.

---

## Success Criteria

- `BattleRuntimeSession::runFrame()` is the only gameplay frame mutation path.
- `BattleFrameRunner` has visible, readable frame ordering.
- Scene code cannot mutate HP, MP, alive/dead, cooldown, action state, combo gameplay state, projectile state, or battle result except through explicit runtime commands.
- Runtime emits enough frame events that scene code does not infer gameplay consequences.
- Persistent `makeXWorld` / `writeXWorld` synchronization pairs are gone or reduced to setup-time conversion.
- Scenario tests cover the recently fixed battle bugs at frame-composition level.
- `BattleCore` may still be sizable, but it is sizable because it owns the frame transaction, not because it is a dumping ground for adapters.

---

## What This Plan Does Not Require

- It does not require returning to the old `BattleSceneHades` monolith.
- It does not require making every subsystem pure.
- It does not require adding broad invariant checks everywhere.
- It does not require shrinking files before ownership is clear.
- It does not require preserving compatibility paths. Prefer deleting transitional code once callers migrate.
