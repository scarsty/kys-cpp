# BattleSceneHades Backend Exit Continuation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish the backend exit honestly by removing the remaining scene-owned gameplay hidden behind the current short `BattleSceneHades::backRun1()` helper phases.

**Architecture:** Keep one frame owner: `KysChess::Battle::BattleFrameRunner`. `BattleSceneHades` builds legacy snapshots through `BattleSceneBattleAdapter`, calls the runner once per battle frame, applies explicit result objects through adapter helpers, and plays presentation. Core systems own gameplay decisions, typed gameplay commands, hit resolution, damage application, team effects, action/cast progression, rescue movement, and battle result decisions.

**Tech Stack:** C++23, existing `src/battle` systems, Catch2 tests under `tests`, MSBuild via `.github\build-command.ps1`.

---

## Authority And Current Reality

This plan supersedes:

- `docs/battle_refactor_completion_plan.md` Slice 7.3.
- `docs/battle_scene_hades_backend_exit_plan.md`.

The previous backend-exit plan is not authoritative even though its tasks are checked. Those checkboxes describe intended migration steps, not the current source reality.

Current source evidence from May 4, 2026:

- `src/BattleSceneHades.cpp:2776` has a short `backRun1()`, but it delegates to scene helper phases.
- `src/BattleSceneHades.cpp:2819` `advanceCoreBattleFrame()` still performs backend gameplay work and calls `BattleFrameRunner().advanceFrame()` multiple times.
- `src/BattleSceneHades.cpp:3100` still resolves dodge outside `BattleHitResolver`.
- `src/BattleSceneHades.cpp:3196` `applyCoreBattleFrameResult()` still aggregates HP damage, applies death/result lifecycle, performs rescue/execute force-pull movement, and records kill/death effects.
- `src/BattleSceneHades.cpp:3915` `Action()` still commits casts/items/blink and mutates action state.
- `src/BattleSceneHades.cpp:4887` `AI()` still plans casts, chooses selected magic, and mutates movement/cast state.
- `src/BattleSceneHades.cpp:5330` `applyBattleGameplayCommand()` still dispatches every typed gameplay command in the scene and mutates HP/MP/shield/projectiles/team effects.
- `src/BattleSceneHades.cpp:5627` `applyAcceptedHitSideEffectTransaction()` still runs a one-off `BattleFrameRunner` damage transaction from the scene.

Completion cannot be claimed while those facts remain true.

### Checkpoint After Tasks 2-7 Migration

May 4, 2026 checkpoint before Task 8:

- Tasks 2-7 removed large backend decision slices from `BattleSceneHades`, including scene-owned runtime/team-effect helpers, hit resolution, projectile follow-up targeting, action/cast helper layers, damage aggregation, rescue reposition selection, lifecycle tracker writes, and command variant visitation.
- This does **not** complete Task 1. The work so far reduced and relocated backend ownership so Task 1 can be finished with less risk, but the one full-frame contract is still outstanding.
- `BattleSceneHades` still calls `BattleFrameRunner().advanceFrame()` multiple times in frame-related paths. The remaining breakthrough is to replace those partial runner calls with one adapter-built `BattleFrameState`, one runner call, and one adapter result application path.
- `BattleSceneHades::backRun1()` is still not the final target shape. It still delegates through transitional scene helpers such as `advanceCoreBattleFrame()`, `applyCoreBattleFrameResult()`, and presentation/cleanup phases.
- Remaining design work before the backend exit can be called complete:
  - finish Task 1's adapter-facing frame bundle;
  - extend `BattleFrameState` to carry all currently separate inputs in one state;
  - move the remaining frame-order calls into `BattleFrameRunner::advanceFrame()`;
  - replace `advanceCoreBattleFrame()` with the single-call adapter pipeline;
  - then finish Task 8 and Task 9 boundary cleanup/search gates.

## Execution Rules

- Execute tasks in order. Later tasks assume earlier boundaries exist.
- Do not create `BattleFrameOrchestrator` or any second full-frame engine. Extend `BattleFrameRunner`.
- Do not replace typed gameplay commands with a generic enum-plus-optional-field bag.
- Do not add source-text tests that assert forbidden words are absent. Search gates are manual verification evidence, not unit tests.
- Build `kys_tests` before every focused test executable run in this plan.
- Use Traditional Chinese for touched source strings and comments.
- Do not add fake defensive boundary branches. Use `assert` for invariants; use one conditional invariant expression such as `assert(!condition || invariant)` when needed.
- Do not keep backwards-compatible scene paths after a core path owns the behavior. Delete duplicate scene logic in the same task.
- Derive battle geometry from existing scene/core constants such as `Scene::TILE_W` or adapter-provided geometry. Do not hard-code alternate gameplay geometry.

## Target End State

`BattleSceneHades::backRun1()` may do only this shape:

```cpp
void BattleSceneHades::backRun1()
{
    auto input = battle_adapter_.buildFrameInput();
    auto result = KysChess::Battle::BattleFrameRunner().advanceFrame(input.state);
    battle_adapter_.applyFrameResult(result);
    presentation_player_.play(result.frame);
    cleanupVisualOnlyBattleFrameState(result);
}
```

Allowed in `BattleSceneHades` after completion:

- Input, camera, render, audio playback, rumble playback, manual camera state, battle exit timing.
- Legacy pointer lookup only when adapting snapshots or applying already-decided deltas.
- Visual-only `AttackEffect` playback and cleanup.

Forbidden in `BattleSceneHades` after completion:

- Direct calls to `BattleHitResolver`, `BattleDamageApplicationSystem`, `BattleActionCommitSystem`, `BattleCastPlanner`, `BattleTeamEffectSystem`, `BattleEffectDispatcher`, or `BattleComboTriggerSystem`.
- Direct command variant visitation with `std::get_if<Battle...Command>`.
- Scene-owned `Action(r)` and `AI(r)` gameplay phases.
- Scene-owned hit dodge, damage aggregation, team effects, force-pull rescue decisions, projectile follow-up target selection, auto-ultimate dispatch, or battle result decisions.
- More than one `BattleFrameRunner().advanceFrame()` call in the frame path.

---

## Task 0: Authority Reset

**Files:**

- Modify: `docs/battle_refactor_completion_plan.md`
- Modify: `docs/battle_scene_hades_backend_exit_plan.md`
- Create: `docs/battle_scene_hades_backend_exit_continuation_plan.md`

- [x] Mark `docs/battle_scene_hades_backend_exit_plan.md` as historical/superseded at the top.
- [x] Repoint `docs/battle_refactor_completion_plan.md` Slice 7.3 to this continuation plan.
- [x] Keep the old plan body intact as checkpoint history. Do not re-mark hundreds of historical checkboxes.
- [x] Run:

```powershell
git diff --check -- docs\battle_refactor_completion_plan.md docs\battle_scene_hades_backend_exit_plan.md docs\battle_scene_hades_backend_exit_continuation_plan.md
rg -n "Continue remaining .*battle_scene_hades_backend_exit_plan\.md|Use .*battle_scene_hades_backend_exit_plan\.md" docs\battle_refactor_completion_plan.md
```

Expected: diff check passes. The `rg` command returns no stale active-plan references.

Task 0 verification on May 4, 2026: `git diff --check -- docs\battle_refactor_completion_plan.md docs\battle_scene_hades_backend_exit_plan.md docs\battle_scene_hades_backend_exit_continuation_plan.md` passed, and the stale active-plan reference search returned no matches.

## Task 1: Make One Full-Frame Contract

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

Purpose: stop using `BattleFrameRunner` as several isolated subsystem calls from the scene. The scene builds one full `BattleFrameState`; the runner owns the frame order.

Current status on May 4, 2026: Task 1 remains open. Tasks 2-7 were necessary prerequisite migrations, not a substitute for Task 1. Do not mark this task complete until the scene has exactly one battle-frame runner call in the thin adapter path and the focused `"[battle][core]"` coverage proves the full same-frame order.

- [ ] Add one adapter-facing frame bundle type in `BattleSceneBattleAdapter` that carries:
  - `BattleFrameState state`;
  - adapter-only maps needed to apply committed deltas to `Role*`;
  - pending presentation playback facts that are not gameplay.
- [ ] Extend `BattleFrameState` to carry all currently separate scene inputs:
  - status units;
  - runtime units;
  - combo units and percent rolls;
  - movement world;
  - active attack world;
  - pending attack spawns;
  - pending HP damage transactions;
  - pending alive counts by team;
  - projectile cancel scalar damage inputs.
- [ ] Move frame-order calls from `advanceCoreBattleFrame()` into `BattleFrameRunner::advanceFrame()`. The runner order must be explicit in code:

```cpp
advanceStatus(state, result);
advanceRuntimeUnits(state, result.commands);
advanceMovement(state, result);
planAndCommitActions(state, result);
advanceAttacksAndResolveHits(state, result);
applyDamageAndLifecycle(state, result);
emitPresentationFrame(state, result);
```

- [ ] Replace `advanceCoreBattleFrame()` with a thin call that builds the frame bundle, calls `BattleFrameRunner().advanceFrame()` once, stores the result, and returns.
- [ ] Add `BattleCoreUnitTests.cpp` coverage that one `advanceFrame()` call processes status, runtime, movement, attacks, damage, and result in the same frame state.
- [ ] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core]"
rg -n "BattleFrameRunner\(\)\.advanceFrame" src\BattleSceneHades.cpp
```

Expected: focused core tests pass. The search reports one scene call, inside the thin frame adapter path.

## Task 2: Fold Runtime, Status, And Team Effects Into The Runner

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleTeamEffectSystem.h`
- Modify: `src/battle/BattleTeamEffectSystem.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/BattleTeamEffectSystemUnitTests.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

Purpose: remove scene-owned combo-frame side effects, one-off team effect commits, and status damage queuing.

- [x] Add `BattleFrameState::teamEffects` with a `BattleTeamEffectWorld`, pending team effect commands, and committed events.
- [x] Make `BattleFrameRunner::advanceFrame()` apply runtime output team heal, self regen, heal aura, percent self heal, and post-skill invincibility through core systems and emit typed deltas/events.
- [x] Convert poison/bleed status tick events into core damage transactions inside the runner instead of returning them for `BattleSceneHades::handleCoreStatusEvents()`.
- [x] Classify `special_magic_effect_every_frame_` and `special_magic_effect_attack_`. Delete them if they are empty or unreachable; otherwise migrate each behavior to an explicit core effect hook and presentation event.
- [x] Delete `BattleSceneHades::commitLegacyTeamEffect()`, `playCommittedTeamEffectEvents()`, `applyComboFrameRuntimeEvent()`, and `handleCoreStatusEvents()` after their behavior is runner-owned.
- [x] Add tests proving:
  - skill-finished team heal mutates team world and emits presentation without scene helper calls;
  - self HP regen and heal aura update only eligible units;
  - poison/bleed status damage enters the same frame damage application path;
  - post-skill invincibility uses the shared core effect executor.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][frame_runner][runtime][unit],[battle][team_effect],[battle][effects],[battle][combo]"
rg -n "commitLegacyTeamEffect|playCommittedTeamEffectEvents|applyComboFrameRuntimeEvent|handleCoreStatusEvents|special_magic_effect_every_frame_|special_magic_effect_attack_|BattleTeamEffectSystem|BattleEffectDispatcher" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: focused tests pass. The search returns no matches in `BattleSceneHades`.

Task 2 verification on May 4, 2026: `kys_tests` Debug x64 build passed; focused tests `"[battle][frame_runner][runtime][unit],[battle][team_effect],[battle][effects],[battle][combo],[battle][core]"` passed with 656 assertions in 80 test cases; the blocker search returned no matches in `BattleSceneHades.cpp` or `BattleSceneHades.h`.

## Task 3: Move Hit Preparation, Dodge, And Resolution Into The Runner

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

Purpose: make projectile hit events core-owned all the way to typed commands and presentation facts.

- [x] Add a frame-level hit input table. It must carry explicit scalar snapshots, not callbacks:

```cpp
struct BattleFrameHitScalarInput
{
    int attackId = -1;
    int attackerUnitId = -1;
    int defenderUnitId = -1;
    int resolvedMagicBaseDamage = 0;
    int resolvedHiddenWeaponDamage = 0;
    int sharedBleedMaxStacks = 1;
    int randomDamageVariance = 0;
    std::vector<double> percentRolls;
};
```

- [x] Build this table in `BattleSceneBattleAdapter` from legacy `Role`, `Magic`, and `Item` data before the runner resolves hits.
- [x] Move dodge resolution into `BattleHitResolver` or a runner-owned hit phase. The defender combo state and roll must be part of the explicit input.
- [x] Make `BattleFrameRunner::advanceFrame()` convert each `BattleAttackEventType::Hit` into `BattleHitResolutionInput`, call `BattleHitResolver`, write combo state deltas, append commands, and append presentation events.
- [x] Keep hit audio and rumble as presentation/application facts, not hit decisions.
- [x] Delete direct scene calls to `BattleHitResolver`, `resolveBattleDodge`, `makeBattleHitUnitSnapshot`, and `resolveLegacyMagicBaseDamage`.
- [x] Add tests proving:
  - a hit event in `BattleFrameRunner` emits the same HP command as direct resolver input;
  - dodge updates defender combo state and emits dodge presentation;
  - scripted hit events resolve in the runner;
  - hidden weapon and magic scalar inputs are consumed without scene recomputation after the hit event exists.
- [ ] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core],[battle][hit_resolver][unit]"
rg -n "BattleHitResolver|resolveBattleDodge|makeBattleHitUnitSnapshot|makeBattleHitSkillSnapshot|makeBattleHitItemSnapshot|resolveLegacyMagicBaseDamage|pendingPreResolvedHpDamage" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: focused tests pass. The search returns no matches in `BattleSceneHades`.

Task 3 verification on May 4, 2026: `kys_tests` Debug x64 build passed; focused tests `"[battle][core],[battle][hit_resolver][unit]"` passed with 370 assertions in 54 test cases; the blocker search returned no matches in `BattleSceneHades.cpp` or `BattleSceneHades.h`; the Debug x64 `kys` target built with existing warnings.

## Task 4: Move Projectile Follow-Up Geometry And Targeting Out Of The Scene

**Files:**

- Modify: `src/battle/BattleAttackSystem.h`
- Modify: `src/battle/BattleAttackSystem.cpp`
- Modify: `src/battle/BattleProjectileTargetingSystem.h`
- Modify: `src/battle/BattleProjectileTargetingSystem.cpp`
- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `tests/BattleAttackSystemUnitTests.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

Purpose: commands such as spiral bleed, nearby tracking, death AOE, shield explosion, and hit extra projectiles must become concrete `BattleAttackSpawnRequest` results before the scene sees them.

- [x] Add a core follow-up spawn resolver that accepts:
  - attacker/target snapshots;
  - `BattleProjectileTargetWorld`;
  - battle geometry and projectile speed config;
  - prototype attack event data;
  - deterministic rolls and frame offsets.
- [x] Convert `BattleCurrentHpBlastCommand`, `BattleSpiralBleedProjectileCommand`, `BattleNearbyTrackingProjectilesCommand`, `BattleHitExtraProjectilesCommand`, `BattleShieldExplosionCommand`, and `BattleDeathAoeProjectileCommand` into either HP damage commands or explicit `BattleProjectileSpawnCommand` values before leaving core.
- [x] Remove scene helpers `spawnAreaImpactProjectiles()`, `spawnTrackingProjectileSpread()`, `spawnUltimateExtraProjectiles()`, `spawnHitExtraProjectiles()`, `spawnSpiralBleedProjectiles()`, and `spawnNearbyTrackingProjectiles()`.
- [x] Preserve visual presentation messages by returning `BattlePresentationEvent` values from core or adapter application, not by running scene selection logic.
- [x] Add tests proving:
  - nearby tracking selects targets from `BattleProjectileTargetWorld`;
  - spiral bleed creates the expected count and shared hit group;
  - death AOE and shield explosion create projectile spawn commands;
  - extra projectile spread uses deterministic target ordering and frame offsets.
- [ ] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][attack],[battle][hit_resolver][unit],[battle][projectile_targeting]"
rg -n "spawnAreaImpactProjectiles|spawnTrackingProjectileSpread|spawnUltimateExtraProjectiles|spawnHitExtraProjectiles|spawnSpiralBleedProjectiles|spawnNearbyTrackingProjectiles|BattleCurrentHpBlastCommand|BattleSpiralBleedProjectileCommand|BattleNearbyTrackingProjectilesCommand|BattleHitExtraProjectilesCommand|BattleShieldExplosionCommand|BattleDeathAoeProjectileCommand" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: focused tests pass. The search returns no matches in `BattleSceneHades`; command alternatives that no longer cross the scene boundary may also be removed from `BattleGameplayCommand`.

Task 4 verification on May 4, 2026: `kys_tests` Debug x64 build passed; focused tests `"[battle][attack],[battle][hit_resolver][unit],[battle][projectile],[battle][damage_application][unit]"` passed with 322 assertions in 57 test cases. The written `[battle][projectile_targeting]` tag has no matching tests in this repository; existing projectile-targeting tests use `[battle][projectile]`. The Task 4 blocker search returned no matches in `BattleSceneHades.cpp` or `BattleSceneHades.h`; the Debug x64 `kys` target built with existing warnings.

## Task 5: Move Action, AI, Cast Start, And Cast Commit Into The Runner

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleCastSystem.h`
- Modify: `src/battle/BattleCastSystem.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleCastSystemUnitTests.cpp`
- Modify: `tests/BattleActionCommitUnitTests.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

Purpose: remove the public scene `Action()` and `AI()` gameplay phases. The frame runner plans casts and commits actions using snapshots.

- [ ] Add `BattleFrameState::actions` with per-unit action inputs:
  - runtime/action state;
  - normal and ultimate skill snapshots;
  - equipped item snapshot;
  - nearest/farthest target facts;
  - blink geometry input;
  - movement decision for that unit;
  - deterministic rolls.
- [x] Extend `BattleFrameRunner::advanceFrame()` to run cast planning and action commit after movement/runtime and before attack update.
- [x] Return explicit application deltas for:
  - `UsingMagic`/`UsingItem` clearing;
  - cooldown and action animation state;
  - operation count;
  - MP consumption;
  - item count delta;
  - blink teleport intent or resolved teleport command;
  - queued attack spawns.
- [x] Move `queueCoreSkillAttackSpawn()` and `triggerAutoUltimate()` behavior into core action/runtime command handling.
- [ ] Keep audio as presentation/application only. Audio cannot decide whether a cast happened.
- [x] Delete `BattleSceneHades::Action()` and `BattleSceneHades::AI()` declarations and frame-loop calls, or reduce them to non-gameplay playback helpers with new names.
- [ ] Add tests proving:
  - runner starts a cast when movement reports attack-ready;
  - runner commits a cast on the configured cast frame;
  - blink intent does not choose a target in the scene;
  - hidden weapon item commit produces item delta and projectile spawn;
  - auto ultimate queues the same action path as normal casts.
- [ ] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core],[battle][cast],[battle][action_commit][unit]"
rg -n "void BattleSceneHades::Action|void BattleSceneHades::AI|Action\(r\)|AI\(r\)|queueCoreSkillAttackSpawn|triggerAutoUltimate\(|BattleActionCommitSystem|BattleCastPlanner|UsingMagic|UsingItem|OperationCount" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: focused tests pass. Remaining `UsingMagic`, `UsingItem`, and `OperationCount` matches in `BattleSceneHades` are render/setup/adapter application only; no frame gameplay decisions remain.

Task 5 corrective note on May 4, 2026: the previous Task 5 checkpoint is not sufficient. It removed the public `Action()`/`AI()` names and direct `BattleCastPlanner`/`BattleActionCommitSystem` calls, but it introduced replacement scene helpers that still own gameplay decisions:

- `BattleSceneHades::populateCoreActionState()`
- `BattleSceneHades::appendCoreActionInputForRole()`
- `BattleSceneHades::appendCoreCastInputForRole()`
- `BattleSceneHades::applyCoreActionState()`
- `BattleSceneHades::applyCoreCastState()`
- `BattleSceneHades::applyCoreBlinkAction()`
- `BattleSceneHades::advanceCoreActionAnimation()`
- `BattleSceneHades::queueCoreSelectedSkillAttackSpawn()`
- `BattleSceneHades::applyCoreAutoUltimateCommand()`

Those helpers are the wrong shape: they move code around inside `BattleSceneHades` instead of making the runner own action/cast behavior. Do not continue by extending them.

### Task 5A: Correct The Action/Cast Boundary

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleCastSystem.h`
- Modify: `src/battle/BattleCastSystem.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleCastSystemUnitTests.cpp`
- Test: `tests/BattleActionCommitUnitTests.cpp`

Purpose: replace the accidental scene-helper layer with a real action-frame contract. `BattleSceneHades` should not build action/cast decisions or select blink destinations. It should ask the adapter for a frame bundle, call the runner once for the attack/action frame, then apply explicit result deltas.

- [x] Add a core action-frame input model that carries per-unit snapshots, not `Role*`:
  - current action state (`HaveAction`, `ActFrame`, operation type, act type, cooldown/recovery facts);
  - pending cast fact if a cast is waiting for its commit frame;
  - item-use fact if an item is queued;
  - cast-planning snapshot for units without an action;
  - target facts for normal/ultimate selection;
  - blink geometry world: walkable candidate cells, occupancy, target position, deterministic cell roll;
  - hidden-weapon target/vector fact;
  - deterministic combat rolls.
- [x] Extend `BattleFrameRunner::advanceFrame()` to own the action phase:
  - advance action animation counters;
  - plan casts for idle units after movement;
  - start casts by returning action-state deltas;
  - commit pending casts on the configured cast frame;
  - commit pending item use;
  - resolve blink destination from the supplied geometry world;
  - queue attack spawns before attack ticking.
- [x] Return explicit action-frame deltas:
  - action state (`HaveAction`, `ActFrame`, `ActType`, `OperationType`, cooldowns);
  - `UsingMagic`/`UsingItem` clear or selected skill/item ids;
  - MP delta;
  - item count delta;
  - operation count;
  - combo state deltas;
  - teleport destination and facing update;
  - pending attack spawns and presentation events.
- [x] Move legacy snapshot construction and delta application into `BattleSceneBattleAdapter`.
- [x] Delete the accidental Task 5 scene helpers listed above.
- [x] `BattleSceneHades` may keep only:
  - invoking adapter build/apply;
  - audio playback from explicit presentation/application facts;
  - visual-only role effects;
  - no direct cast/action/blink/item decisions.
- [x] Add tests proving:
  - runner starts a cast from an idle unit action-frame input;
  - runner commits a pending cast exactly on the cast frame and spawns attacks before attack ticking;
  - runner advances recovery and clears action state;
  - runner resolves blink destination from explicit geometry input;
  - runner commits hidden weapon item use and returns item count delta;
  - auto ultimate uses the same selected-cast action path without a scene helper.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core],[battle][cast],[battle][action_commit][unit]"
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
rg -n "populateCoreActionState|appendCoreActionInputForRole|appendCoreCastInputForRole|applyCoreActionState|applyCoreCastState|applyCoreBlinkAction|advanceCoreActionAnimation|queueCoreSelectedSkillAttackSpawn|applyCoreAutoUltimateCommand|void BattleSceneHades::Action|void BattleSceneHades::AI|Action\(r\)|AI\(r\)|BattleActionCommitSystem|BattleCastPlanner" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: tests and builds pass. The blocker search returns no matches in `BattleSceneHades`.

Task 5A checkpoint on May 4, 2026: removed the accidental action/cast helper layer from `BattleSceneHades` and migrated frame action build/apply work into `BattleSceneBattleAdapter`, with `BattleFrameRunner` now advancing action-unit recovery, clearing completed action state, and returning committed action inputs/results for cast and item application. Focused tests `"[battle][core],[battle][cast],[battle][action_commit][unit]"` passed with 632 assertions in 66 test cases; Debug x64 `kys_tests` and `kys` targets built; `git diff --check` passed; the Task 5A blocker search returned no matches in `BattleSceneHades.cpp` or `BattleSceneHades.h`.

Task 5A blink-geometry checkpoint on May 4, 2026: moved blink destination selection into `BattleActionCommitSystem` via `BattleBlinkGeometryInput` and `BattleBlinkTeleportDelta`. The adapter now snapshots candidate cells, walkability, occupancy, and deterministic rolls before commit, then applies only the core-selected teleport delta. Focused tests `"[battle][action_commit][unit],[battle][core],[battle][cast]"` passed with 642 assertions in 67 test cases; Debug x64 `kys_tests` and `kys` targets built; `git diff --check` passed; the action/cast blocker search returned no matches in `BattleSceneHades` or adapter-side blink destination selection.

Task 5A closure checkpoint on May 4, 2026: removed the old parallel core bypass queues (`BattleFrameState::casts`, `actions.pendingInputs`, `actions.committedResults`) and migrated selected/auto-ultimate commit to the same `BattleFrameActionUnitInput` contract via selected-cast fields. Focused tests `"[battle][core],[battle][cast],[battle][action_commit][unit]"` passed with 654 assertions in 68 test cases; Debug x64 `kys_tests` and `kys` targets built; `git diff --check` passed; searches for the deleted queues and Task 5A scene blockers returned no matches.

## Task 6: Move Damage Application, Rescue Movement, And Battle Result Completion

**Files:**

- Modify: `src/battle/BattleDamageApplicationSystem.h`
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`
- Create: `src/battle/BattleRescueRepositionSystem.h`
- Create: `src/battle/BattleRescueRepositionSystem.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `src/kys.vcxproj.filters`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/kys_tests.vcxproj`
- Modify: `tests/kys_tests.vcxproj.filters`
- Modify: `tests/BattleDamageApplicationSystemUnitTests.cpp`
- Create: `tests/BattleRescueRepositionSystemUnitTests.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

Purpose: remove the remaining backend body of `applyCoreBattleFrameResult()`.

- [x] Move pending HP damage aggregation into core. The scene may adapt pending legacy damage into `BattleDamageTransactionInput`, but aggregation and committed order belong to `BattleFrameRunner` or `BattleDamageApplicationSystem`.
- [x] Add `BattleRescueRepositionSystem` for protection pull and execute pull. It consumes:
  - unit/team snapshots;
  - combo force-pull counters;
  - grid walkability/occupancy snapshots;
  - pull mode;
  - deterministic tie-break inputs.
- [x] Return explicit commands/deltas for:
  - teleporting the pulled unit;
  - decrementing pull counters;
  - protection heal;
  - temporary invincibility;
  - basic counter projectile spawn;
  - presentation text/log facts.
- [x] Move `refreshEnemyTopDebuffs()` into a core or adapter-owned lifecycle application path. If it is only needed after death, it must be triggered by a `UnitDied` lifecycle event, not called directly from the scene frame body.
- [x] Keep camera slow/zoom/shake and `setExit(true)` as scene presentation/flow reactions to explicit `BattleEnded` result events.
- [x] Delete `tryForcePull` local logic, pending HP aggregation loops, death lifecycle decisions, and battle result decisions from `BattleSceneHades::applyCoreBattleFrameResult()`.
- [x] Add tests proving:
  - aggregate pending HP damage commits once per target with deterministic source/detail selection;
  - death lifecycle emits kill/death/battle-end facts without scene team scans;
  - protection pull selects a legal destination from snapshots;
  - execute pull decrements the correct counter and spawns a basic attack command;
  - no rescue command is emitted when no legal cell exists.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][damage_application][unit],[battle][rescue_reposition][unit],[battle][core]"
rg -n "queuePreResolvedHpDamage|pendingPreResolvedHpDamage|applyAcceptedHitSideEffectTransaction|BattleDamageApplicationSystem|tryForcePull|refreshEnemyTopDebuffs|recordKill|recordDeath|recordBattleEnd|calExpGot\(|setExit\(true\)" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: focused tests pass. `calExpGot()` and `setExit(true)` may remain only in scene battle-end flow after an explicit result event, not in backend decision logic.

Task 6 checkpoint on May 4, 2026: added core support for optional aggregation of pending damage transactions by defender in `BattleDamageApplicationSystem`, with unit coverage proving two pending transactions against one defender commit as one deterministic transaction using the last source snapshot and summed damage. `kys_tests` Debug x64 build passed; focused tests `"[battle][core],[battle][cast],[battle][action_commit][unit],[battle][damage_application][unit]"` passed with 638 assertions in 68 test cases. Scene pending HP aggregation has not been deleted yet because `BattleSceneHades` still couples each aggregated damage transaction to presentation metadata; the next slice needs a core/application result shape for that metadata before removing the scene loop.

Task 6 checkpoint on May 4, 2026: moved pending HP damage presentation metadata into `BattleDamageApplicationSystem`, deleted the scene-side pending HP aggregation loop, and added `BattleRescueRepositionSystem` with unit coverage for protection pull, execute pull, and no-legal-cell behavior. `BattleSceneHades` now adapts rescue snapshots and applies explicit rescue deltas, while destination selection, counter decrements, heal/invincibility facts, counterattack request facts, and rescue presentation facts are core-owned. Focused tests `"[battle][damage_application][unit],[battle][rescue_reposition][unit],[battle][core]"` passed with 395 assertions in 51 test cases. Remaining Task 6 work is moving death lifecycle side effects, `refreshEnemyTopDebuffs()`, and battle-end flow decisions out of `BattleSceneHades::applyCoreBattleFrameResult()`.

Task 6 checkpoint on May 4, 2026: `BattleDamageApplicationSystem` now emits explicit `BattleGameplayEvent` lifecycle facts for `UnitDied` and `BattleEnded`; `BattleSceneHades` consumes those facts for visual flow only. Tracker kill/death/battle-end recording and enemy-top debuff refresh are applied through `BattleSceneBattleAdapter`, and the direct `BattleDamageApplicationSystem` dependency was moved behind the adapter boundary. `BattleSceneHades` no longer contains `tryForcePull`, `BattleDamageApplicationSystem`, `refreshEnemyTopDebuffs`, `recordKill`, `recordDeath`, or `recordBattleEnd` matches. Remaining blocker-search matches are the legacy damage queue/accepted-hit side-effect helpers plus the permitted battle-end flow calls to `calExpGot()` and `setExit(true)`.

Task 6 closure checkpoint on May 4, 2026: moved the remaining legacy damage queue adapter shape and accepted-hit side-effect transaction application behind `BattleSceneBattleAdapter`. `BattleSceneHades` no longer declares or defines `queuePreResolvedHpDamage`, `calculateQueuedHpDamage`, or `applyAcceptedHitSideEffectTransaction`, and the Task 6 blocker search now returns only the permitted battle-end flow calls to `calExpGot()` and `setExit(true)`. `kys_tests` Debug x64 build passed; focused tests `"[battle][damage_application][unit],[battle][rescue_reposition][unit],[battle][core]"` passed with 404 assertions in 51 test cases.

## Task 7: Move Gameplay Command Application Behind The Adapter Boundary

**Files:**

- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleScenePresentationPlayer.h`
- Modify: `src/BattleScenePresentationPlayer.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Add or modify focused tests only where existing legacy types allow stable coverage.

Purpose: `BattleSceneHades` can apply a frame result, but it cannot be the command interpreter.

- [x] Add adapter command/hit application entry points. Implemented as `BattleCommandApplicationContext`, `BattleCommandApplicationResult`, `applyBattleCommand()`, and `applyBattleHitApplication()` so the scene consumes application facts instead of visiting command variants:

```cpp
struct BattleCommandApplicationContext
{
    int frame = 0;
    const std::vector<Role*>* roles = nullptr;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::vector<BattlePendingDamageAdapterInput>* pendingDamage = nullptr;
};

struct BattleCommandApplicationResult
{
    std::vector<int> criticalHitUnitIds;
    std::vector<KysChess::Battle::BattlePresentationEvent> presentationEvents;
    std::vector<KysChess::Battle::BattleAttackSpawnRequest> attackSpawnRequests;
    std::vector<BattleAutoUltimateApplicationRequest> autoUltimateRequests;
};

BattleCommandApplicationResult applyBattleCommand(
    const BattleCommandApplicationContext& context,
    const KysChess::Battle::BattleGameplayCommand& command);
BattleCommandApplicationResult applyBattleHitApplication(
    const BattleCommandApplicationContext& context,
    const KysChess::Battle::BattleHitResolutionResult& hitResolution);
```

- [x] Move command variant visitation out of `BattleSceneHades::applyBattleGameplayCommand()` into adapter/core application helpers.
- [x] Delete `BattleSceneHades::applyBattleGameplayCommand()` and `BattleSceneHades::applyResolvedBattleHit()`.
- [x] Ensure adapter application does not decide gameplay semantics. It may only resolve ids to `Role*`, write committed deltas, queue already-decided spawns, and return presentation facts.
- [x] Keep rumble/audio/camera playback in scene as reactions to result/presentation events.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core],[battle][hit_resolver],[battle][damage_application]"
rg -n "applyBattleGameplayCommand|applyResolvedBattleHit|std::get_if<.*Battle.*Command|BattleGameplayCommand" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: focused tests pass. The search returns no command-dispatch matches in `BattleSceneHades`.

Task 7 checkpoint on May 4, 2026: moved command and hit-result application into `BattleSceneBattleAdapter`, including pending damage adaptation, side-effect damage transactions, team-effect application, queued projectile spawn facts, critical-hit marking, and presentation facts. Deleted `BattleSceneHades::applyBattleGameplayCommand()` and `BattleSceneHades::applyResolvedBattleHit()`. `kys_tests` Debug x64 build passed; focused tests `"[battle][core],[battle][hit_resolver],[battle][damage_application]"` passed with 483 assertions in 69 test cases; the Task 7 blocker search returned no matches in `BattleSceneHades.cpp` or `BattleSceneHades.h`.

## Task 8: Collapse `BattleSceneHades` Headers And Frame Helpers

**Files:**

- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`

Purpose: make the boundary visible in file structure, not only in implementation details.

- [ ] Remove direct battle system includes from `BattleSceneHades.h` when a forward declaration or adapter include is enough. `BattleSceneHades.h` must not expose `BattleDamageSystem`, `BattleHitResolver`, `BattleTeamEffectSystem`, or other backend systems.
- [ ] Rename remaining frame helpers to presentation/application names only. Names containing `Core` are acceptable only for snapshot build/apply adapters, not gameplay phases.
- [ ] Convert touched Simplified Chinese comments or string literals in `BattleSceneHades.cpp` and `BattleSceneHades.h` to Traditional Chinese.
- [ ] Delete empty or misleading helpers such as no-op cleanup wrappers unless the scene genuinely needs them for visual state.
- [ ] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core],[battle][presentation]"
rg -n "#include \"battle/(BattleDamageSystem|BattleHitResolver|BattleTeamEffectSystem|BattleComboTriggerSystem|BattleEffectSystem|BattleCastSystem)\\.h\"" src\BattleSceneHades.h
rg -n "继|战|场|动|击|画|处|险|这个|计算|删除|处理|出场|血条|音效" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: focused tests pass. The include search returns no matches. The Chinese search is a review aid; inspect matches and convert only source text touched by this continuation.

## Task 9: Final Boundary Gate

**Files:**

- Modify only files needed to fix verification failures.
- Modify: `docs/battle_scene_hades_backend_exit_continuation_plan.md`

- [ ] Run scene backend blocker searches:

```powershell
rg -n "BattleFrameRunner\(\)\.advanceFrame" src\BattleSceneHades.cpp
rg -n "BattleHitResolver|BattleDamageApplicationSystem|BattleActionCommitSystem|BattleCastPlanner|BattleTeamEffectSystem|BattleComboTriggerSystem|BattleDamageSystem|BattleEffectDispatcher|resolveBattleDodge|queuePreResolvedHpDamage|applyAcceptedHitSideEffectTransaction|commitLegacyTeamEffect|LegacyTeamEffectCommit|applyBattleGameplayCommand|applyResolvedBattleHit|std::get_if<.*Battle.*Command" src\BattleSceneHades.cpp src\BattleSceneHades.h
rg -n "void BattleSceneHades::Action|void BattleSceneHades::AI|Action\(r\)|AI\(r\)|triggerAutoUltimate\(|queueCoreSkillAttackSpawn|spawnAreaImpactProjectiles|spawnTrackingProjectileSpread|spawnUltimateExtraProjectiles|spawnHitExtraProjectiles|spawnSpiralBleedProjectiles|spawnNearbyTrackingProjectiles|tryForcePull|refreshEnemyTopDebuffs|resolveLegacyMagicBaseDamage" src\BattleSceneHades.cpp src\BattleSceneHades.h
rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle
rg -n "BattleFrameOrchestrator|frame_orchestrator|BattleGameplayRequest|BattleGameplayRequestType|value2" src tests
```

Expected:

- `BattleFrameRunner().advanceFrame` appears once in `BattleSceneHades.cpp`, in the thin adapter pipeline only.
- Scene backend blocker searches return no gameplay ownership matches. Any remaining match must be documented inline next to the gate with a reason it is visual/application only.
- `src/battle` has no forbidden legacy/render dependencies.
- No second frame orchestrator and no generic request bag names exist.

- [ ] Run full verification:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp-worktrees\kys-cpp\battle-refactor-completion\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: diff check passes, all tests pass, and the game target builds. If final game linking fails only because the executable is running, record that exact linker error and treat compilation before link as the useful build evidence.

## Completion Criteria

- `BattleSceneHades::backRun1()` is a thin adapter pipeline, not a dispatcher into backend scene helpers.
- `BattleFrameRunner::advanceFrame()` is called once per battle frame from the scene.
- `BattleSceneHades` does not resolve hits, dodge, damage, team effects, combo runtime effects, action/cast commits, projectile follow-up targets, force-pull rescue, or battle result.
- `BattleSceneHades` does not dispatch `BattleGameplayCommand` variants.
- `src/battle` remains free of legacy/render dependencies.
- Every migrated behavior has focused core coverage.
- Final search gates and full Debug x64 `kys_tests` plus `kys` build evidence are recorded in this document before anyone claims completion.
