# Battle Scene Frame Applier Deletion-First Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:test-driven-development` before implementation, and use `superpowers:verification-before-completion` before claiming completion. This plan is deletion-first: a slice is not complete if it only wraps the old playback paths.

**Goal:** Replace the current multi-path scene playback flow with one scene-owned frame application boundary, while deleting redundant intermediate playback models and fake control-flow result objects.

**Selected Approach:** Add a `BattleSceneFrameApplier` as the only live-scene consumer of `KysChess::Battle::BattlePresentationFrame`. `BattleSceneHades` should no longer coordinate frame delta, report playback, impact playback, visual playback, and legacy frame advancement separately.

**Primary Complexity Target:** Remove duplicated event playback/report/impact handling. Do not use this branch to rewrite the battle runtime model or broadly split `BattleCore.cpp`.

**Tech Stack:** C++20, Catch2, PowerShell, ripgrep, Visual Studio/MSBuild via `.github/build-command.ps1`.

---

## Current Shape

- `BattleRuntimeSession::runFrame()` returns a `BattlePresentationFrame`.
- `BattleSceneHades::backRun1()` currently splits a frame across several semi-authoritative scene paths:
  - `applyCoreFrameResult(...)`
  - `applyLegacyFrameResult(...)`
  - `playPresentationFrame(...)`
  - `BattleSceneFrameDeltaBuilder`
  - `BattleSceneImpactPlayer`
  - `BattleSceneReportPlayer`
  - `BattleScenePresentationPlayer`
  - `BattlePresentationPlaybackPlanner`
- `SceneLegacyFrameResult::advanced` is not a result. It is a control-flow flag used to decide whether to age transient scene presentation state and finish the battle.
- `BattlePresentationPlayback` creates a second presentation model:
  - `BattleVisualEvent`
  - `BattlePresentationCommand`
  - `BattleAttackEffect` / `BattleTextEffect`
- The scene must keep multiple playback/report queues synchronized even though all of them originate from the same `BattlePresentationFrame`.

## End State

- `BattleSceneHades` has one live-frame call:

```cpp
auto frame = battle_session_->runFrame();
frame_applier_.apply(frame);
```

- `BattleSceneHades` owns ongoing scene state, but it does not manually fan out one battle frame into separate report, impact, delta, and visual playback APIs.
- `BattleSceneFrameApplier` owns the frame application order:
  1. Apply lifecycle and damage scene effects.
  2. Apply impact effects from visual events.
  3. Record report logs.
  4. Apply visual events to legacy text/effect queues.
  5. Apply frame-level sounds, rumble, blink sounds, camera, slow/freeze, and battle result changes.
- The fake `.advanced` object is gone.
- `BattlePresentationCommand` and `BattlePresentationPlaybackPlan` are gone.
- `BattleSceneFrameDelta` is gone.
- `BattleSceneHades` no longer has direct members for frame-delta building, report playback, impact playback, or presentation-command playback.
- Tests verify behavior through the new frame applier and through direct visual event playback where that remains a separate helper.

---

## Deletion Requirements

The implementation must delete these old surfaces or the refactor is not complete:

- `BattleSceneHades::SceneLegacyFrameResult`
- `BattleSceneHades::applyLegacyFrameResult(...)`
- `BattleSceneHades::applyCoreFrameResult(...)`
- `BattleSceneHades::playPresentationFrame(...)`
- `BattlePresentationPlayback.h`
- `BattlePresentationPlayback.cpp`
- `BattlePresentationCommand`
- `BattlePresentationPlaybackPlan`
- `BattlePresentationPlaybackPlanner`
- `BattleSceneFrameDelta`
- `BattleSceneFrameDeltaBuildContext`
- `BattleSceneFrameDeltaBuilder`

The implementation should also delete these if their remaining role is only pass-through after the applier exists:

- `BattleSceneImpactPlayer`
- `BattleSceneReportPlayer`

`BattleScenePresentationPlayer` may survive only if it consumes `BattleVisualEvent` directly and no longer depends on `BattlePresentationCommand`.

## Non-Goals

- Do not rewrite `BattleRuntimeState`.
- Do not move combo globals in this branch.
- Do not split `BattleCore.cpp` just to move code between files.
- Do not introduce an event bus or a sink registration framework.
- Do not add broad defensive null checks around dependencies that are invariants.
- Do not preserve backwards compatibility for deleted playback APIs.

---

## API Boundary

Create a scene-owned applier:

```cpp
class BattleSceneFrameApplier
{
public:
    void apply(const KysChess::Battle::BattlePresentationFrame& frame);
};
```

The exact constructor or binding shape can be chosen during implementation, but keep these rules:

- Prefer one narrow owner object over several small `Bindings` bags.
- If a bindings object is unavoidable, it must replace the existing bindings/context structs rather than adding another permanent layer beside them.
- The applier should be scene-specific. It does not belong in `src/battle` because it mutates legacy scene queues, camera, audio, controller rumble, hurt flash timers, and battle result state.
- The applier should not create a new persistent frame model. It applies the `BattlePresentationFrame` directly.

Expected ownership after the refactor:

- `BattleRuntimeSession`: authoritative runtime simulation.
- `BattlePresentationFrame`: one-frame immutable report of what runtime produced.
- `BattleSceneFrameApplier`: only live-scene consumer of the frame.
- `BattleSceneHades`: owns scene state and calls the applier, but does not fan out the frame itself.

---

## Task 1: Characterization Tests for the New Boundary

**Files:**
- Add: `tests/BattleSceneFrameApplierUnitTests.cpp`
- Modify: `tests/kys_tests.vcxproj`
- Modify: `src/CMakeLists.txt`

- [ ] Add tests that build a small `BattlePresentationFrame` and verify the applier records report damage, death, battle end, and projectile cancel stats.
- [ ] Add tests that verify damage logs create one hurt flash and one blood effect per damaged unit per frame.
- [ ] Add tests that verify unit death applies camera/freeze/slow/shake/battle-end state.
- [ ] Add tests that verify visual projectile events update legacy attack effects without `BattlePresentationCommand`.
- [ ] Add tests that verify impact fields on `BattleVisualEvent` apply unit shake, scene shake, sound, and rumble.

The tests should use existing battle scene test helpers where possible. Avoid creating a second fake battle model.

Verification:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_applier]"
```

Expected before implementation: the new tests fail to compile or fail behaviorally because `BattleSceneFrameApplier` does not exist yet.

---

## Task 2: Add `BattleSceneFrameApplier` and Route Live Frames Through It

**Files:**
- Add: `src/BattleSceneFrameApplier.h`
- Add: `src/BattleSceneFrameApplier.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `src/kys.vcxproj.filters`
- Modify: `tests/kys_tests.vcxproj`
- Modify: `src/CMakeLists.txt`

- [ ] Add `BattleSceneFrameApplier`.
- [ ] Move the frame application ordering out of `BattleSceneHades`.
- [ ] Replace `BattleSceneHades::playPresentationFrame(...)` call sites with applier calls, including initialization frames from `BattleInitializationResult`.
- [ ] Remove direct `BattleSceneHades` members for report, impact, frame delta, and presentation-command playback if they are now owned by the applier.
- [ ] Keep behavior ordering equivalent for the first route-through slice.

This task is allowed to temporarily call existing helpers from inside the applier, but only as a transition to the deletion tasks below. Do not stop after this task.

Verification:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_applier],[battle][scene_report],[battle][scene_impact],[battle][scene_frame_delta],[battle][presentation]"
```

---

## Task 3: Delete `.advanced` and the Legacy Frame Result Object

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] Delete `SceneLegacyFrameResult`.
- [ ] Delete `applyLegacyFrameResult(...)`.
- [ ] Replace `buildBattleFrameInput()` with a named slow-frame gate such as `shouldAdvanceBattleSimulation()`.
- [ ] Add explicit scene-tick methods for the behavior previously hidden behind `.advanced`:
  - age transient unit presentation state
  - age text effects
  - finish battle once slow motion has completed
- [ ] Make `backRun1()` read as direct control flow:

```cpp
if (!shouldAdvanceBattleSimulation())
{
    return;
}

advanceBattlePresentationEffects(attack_effects_, true);
auto frame = battle_session_->runFrame();
frame_applier_.apply(frame);
advanceScenePresentationFrame();
finishBattleIfReady();
```

Verification:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_applier],[battle][presentation]"
rg -n "SceneLegacyFrameResult|applyLegacyFrameResult|\\.advanced|advanced =" src tests
```

Expected: tests pass and `rg` returns no matches for the deleted legacy result path.

---

## Task 4: Delete `BattleSceneFrameDeltaBuilder`

**Files:**
- Modify: `src/BattleSceneFrameApplier.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Delete: `src/BattleSceneFrameDeltaBuilder.h`
- Delete: `src/BattleSceneFrameDeltaBuilder.cpp`
- Delete or replace: `tests/BattleSceneFrameDeltaBuilderUnitTests.cpp`
- Modify project files and CMake file lists.

- [ ] Move lifecycle and damage-scene-effect application into the applier.
- [ ] Apply blood effects, hurt flashes, camera focus, freeze, slow, scene shake, attack sounds, rumble, and battle result directly.
- [ ] Do not introduce a replacement `BattleSceneFrameDelta` DTO.
- [ ] Replace old frame-delta tests with applier tests that verify externally visible scene state/effect queues.

Verification:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_applier]"
rg -n "BattleSceneFrameDelta|BattleSceneFrameDeltaBuilder|BattleSceneFrameDeltaBuildContext" src tests
```

Expected: tests pass and `rg` returns no matches.

---

## Task 5: Delete `BattlePresentationPlayback`

**Files:**
- Modify: `src/BattleScenePresentationPlayer.h`
- Modify: `src/BattleScenePresentationPlayer.cpp`
- Modify: `src/BattleSceneFrameApplier.cpp`
- Delete: `src/battle/BattlePresentationPlayback.h`
- Delete: `src/battle/BattlePresentationPlayback.cpp`
- Delete or replace: `tests/BattlePresentationPlaybackUnitTests.cpp`
- Modify project files and CMake file lists.

- [ ] Change visual playback to consume `BattleVisualEvent` directly.
- [ ] Delete `BattlePresentationCommand`, `BattlePresentationPlaybackPlan`, and `BattlePresentationPlaybackPlanner`.
- [ ] Keep small private helper functions for projectile payload, damage text sizing, and effect finishing.
- [ ] Move useful order/payload coverage from `BattlePresentationPlaybackUnitTests.cpp` into `BattleSceneFrameApplierUnitTests.cpp` or direct `BattleScenePresentationPlayer` tests.
- [ ] Remove `#include "battle/BattlePresentationPlayback.h"` everywhere.

Verification:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_applier],[battle][presentation]"
rg -n "BattlePresentationPlayback|BattlePresentationCommand|BattlePresentationPlaybackPlan|BattlePresentationPlaybackPlanner" src tests
```

Expected: tests pass and `rg` returns no matches.

---

## Task 6: Collapse Impact Playback Into the Applier

**Files:**
- Modify: `src/BattleSceneFrameApplier.cpp`
- Delete if pass-through: `src/BattleSceneImpactPlayer.h`
- Delete if pass-through: `src/BattleSceneImpactPlayer.cpp`
- Delete or replace: `tests/BattleSceneImpactPlayerUnitTests.cpp`
- Modify project files and CMake file lists.

- [ ] Apply `impactUnitShake`, `impactSceneShake`, `impactEffectSoundId`, and `impactRumble` while applying visual events.
- [ ] Delete `BattleSceneImpactCommand` if it is only an intermediate playback DTO.
- [ ] Delete `BattleSceneImpactPlanner` if its only job is mapping a `BattleVisualEvent` into the command DTO.
- [ ] Preserve the behavior tested by the existing impact tests through applier tests.

Verification:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_applier]"
rg -n "BattleSceneImpactPlayer|BattleSceneImpactPlanner|BattleSceneImpactCommand" src tests
```

Expected: tests pass and `rg` returns no matches unless a clearly useful pure planner remains. A remaining pure planner needs a short justification in the final change summary.

---

## Task 7: Collapse Report Playback Into the Applier

**Files:**
- Modify: `src/BattleSceneFrameApplier.cpp`
- Delete if pass-through: `src/BattleSceneReportPlayer.h`
- Delete if pass-through: `src/BattleSceneReportPlayer.cpp`
- Delete or replace: `tests/BattleSceneReportPlayerUnitTests.cpp`
- Modify project files and CMake file lists.

- [ ] Record `BattleLogEvent` into `BattleReportBuilder` from the applier.
- [ ] Keep report event ordering currently tested for damage, kill, death, and battle end.
- [ ] Preserve projectile cancel stats.
- [ ] Delete `BattleSceneReportPlayer::Bindings` and the public report-player API if all calls now go through the applier.

Verification:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_applier]"
rg -n "BattleSceneReportPlayer" src tests
```

Expected: tests pass and `rg` returns no matches unless a clearly useful pure report helper remains. A remaining helper needs a short justification in the final change summary.

---

## Task 8: Clean Build File Entries and Dead Includes

**Files:**
- Modify: `src/CMakeLists.txt`
- Modify: `src/kys.vcxproj`
- Modify: `src/kys.vcxproj.filters`
- Modify: `tests/kys_tests.vcxproj`
- Modify includes in `src` and `tests`.

- [ ] Remove deleted `.h` and `.cpp` files from Visual Studio projects.
- [ ] Remove deleted test files from the test project.
- [ ] Remove deleted source files from CMake lists.
- [ ] Run include cleanup for `BattleSceneHades.h`, `BattleSceneHades.cpp`, and `BattleScenePresentationPlayer.h`.

Verification:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_applier],[battle][presentation]"
```

---

## Task 9: Final Verification and Complexity Gate

Run:

```powershell
git diff --check
rg -n "SceneLegacyFrameResult|applyLegacyFrameResult|\\.advanced|BattleSceneFrameDelta|BattleSceneFrameDeltaBuilder|BattlePresentationPlayback|BattlePresentationCommand|BattlePresentationPlaybackPlan|BattlePresentationPlaybackPlanner" src tests
rg -n "BattleSceneImpactPlayer|BattleSceneReportPlayer" src tests
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_applier]"
x64\Debug\kys_tests.exe "[battle][presentation]"
x64\Debug\kys_tests.exe "[battle][scenario][runtime]"
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected:

- `git diff --check` exits 0.
- Deleted API grep has no matches.
- Impact/report grep has no matches, or any remaining helper has a documented reason and is not a second frame playback path.
- Focused tests pass.
- Full tests pass.
- Debug build completes. A final link failure is acceptable only if caused by the game executable already running.

## Acceptance Criteria

- `BattleSceneHades::backRun1()` has direct frame control flow and no fake result object.
- There is one live-scene entry point for applying a battle frame.
- The old presentation-command model is deleted.
- The old scene-frame-delta model is deleted.
- Direct report and impact playback APIs are deleted or justified as pure helpers, not separate frame consumers.
- Net public playback APIs decrease.
- Net translation structs decrease.
- The refactor does not add an event bus, generic sink framework, or new persistent frame model.
- The branch demonstrates deletion with `rg` checks, not just moved code.
