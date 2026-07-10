# Mobile Touch Camera Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace duplicate SDL touch/mouse handling with one ordered pointer pipeline and add complete one- and two-finger controls for the 3D paper battle camera on Android and Emscripten.

**Architecture:** `PointerInput` owns the global ordered SDL/action queue, immutable present-geometry timeline, primary-finger selection, capture, and pointer/direct-touch dispatch. `RunNode` supplies pointer-only traversal without simulation updates. Pure `BattleSceneControls`, `BattleScenePaperCameraTouch`, and `PaperCameraControlMath` modules isolate layout, gesture recognition, and camera formulas from `BattleSceneHades`.

**Tech Stack:** C++23, SDL 3.4.2, Dear ImGui SDL3 backend, Catch2, Android JNI/Java, Emscripten shell JavaScript, MSBuild/CMake project metadata.

---

### Task 1: Pure input and geometry types

**Files:**
- Create: `src/PointerInput.h`
- Create: `src/PointerInput.cpp`
- Create: `tests/PointerInputUnitTests.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/kys_tests.vcxproj`
- Modify: `src/kys.vcxproj`
- Modify: `src/kys.vcxproj.filters`

- [ ] Write failing tests for normalized first/last pixel mapping, present-local mapping, residual synthetic stream rejection, primary-finger selection, secondary-finger suppression, short down/up preservation, and direct-contact snapshots.
- [ ] Build `kys_tests` and confirm the tests fail because the input types and converter do not exist.
- [ ] Add `PointerPhase`, `PointerSource`, `PointerEvent`, `TouchFingerKey`, `TouchSample`, `TouchPolicy`, `PresentLayoutInput`, `PresentGeometrySnapshot`, and pure `computePresentLayout()` / coordinate conversion helpers.
- [ ] Add the initial `PointerInput` queue and physical-contact tracker with test injection APIs that exercise real routing logic without SDL mocks.
- [ ] Rebuild and run the focused `[pointer_input]` tests until green.

### Task 2: Ordered action transport and geometry publication

**Files:**
- Modify: `src/PointerInput.h`
- Modify: `src/PointerInput.cpp`
- Modify: `src/Engine.h`
- Modify: `src/Engine.cpp`
- Modify: `tests/PointerInputUnitTests.cpp`

- [ ] Write failing tests for `touch(L0) / geometry(L1) / touch(L1)`, adjacent geometry coalescing, non-coalescing across input, one-time marker payload ownership, and identical-layout publication as a no-op.
- [ ] Run the focused tests and confirm expected failures.
- [ ] Register application-cancel, application-lifecycle, and present-geometry SDL user event types; store payloads by opaque serial in thread-safe owned stores.
- [ ] Implement the source-side SDL event watch for geometry and lifecycle order, raw-wrapper correlation, synchronous termination handling, and queue cleanup.
- [ ] Move Engine layout calculation to shared `computePresentLayout()` and route window size, texture size, UI size, keep-ratio, rotation, and render-ratio mutations through `publishPresentGeometryTransition()`.
- [ ] Install the initial immutable snapshot before input-ready and disable redundant per-draw layout publication.
- [ ] Rebuild and run focused tests.

### Task 3: RunNode pointer-only dispatch and render cadence

**Files:**
- Modify: `src/RunNode.h`
- Modify: `src/RunNode.cpp`
- Modify: `src/PointerInput.h`
- Modify: `src/PointerInput.cpp`
- Create: `tests/RunNodePointerInputUnitTests.cpp`
- Modify: project/test source lists above.

- [ ] Write failing tests for N pointer callbacks plus one `EVENT_FIRST` update, pointer/legacy/pointer ordering, nested-owner transfer, epoch invalidation, capture release/cancel, reverse-draw-order hit testing, and noninteractive discard behavior.
- [ ] Run the focused tests and verify RED.
- [ ] Add `PointerResult`, `onPointerEvent()`, `touchPolicy()`, `onTouchFrame()`, pointer-only reverse traversal, weak capture records, owner stack, ownership epoch, and control-layout revision hooks.
- [ ] Replace native/Emscripten event-loop divergence with the shared queue-front pump: drain the eligible pointer prefix, detach at most one legacy boundary, and call normal scene update at most once.
- [ ] Make nested `run()` entry/exit and scene exit invalidate ownership while preserving undetached queue suffixes.
- [ ] Rebuild and run focused tests.

### Task 4: ImGui primary-touch translation and cancellation

**Files:**
- Modify: `src/ImGuiLayer.h`
- Modify: `src/ImGuiLayer.cpp`
- Modify: `src/Engine.h`
- Modify: `src/Engine.cpp`
- Modify: `src/PointerInput.cpp`
- Modify: `tests/PointerInputUnitTests.cpp`

- [ ] Write failing tests around an injectable ImGui touch sink for motion-before-down, balanced up/cancel, capture latching, overlay invalidation, and exclusion of secondary fingers.
- [ ] Run focused tests and verify RED.
- [ ] Add the dedicated non-queued primary-touch entry point that emits temporary `SDL_TOUCH_MOUSEID` mouse events directly to the backend.
- [ ] Add ImGui overlay/capture queries and application-cancel consumption; ensure raw finger events never reach `ImGui_ImplSDL3_ProcessEvent()`.
- [ ] Wire PointerInput ownership priority and balanced reset behavior.
- [ ] Rebuild and run focused tests.

### Task 5: Migrate generic single-pointer UI

**Files:**
- Modify: `src/Button.h`, `src/Button.cpp`
- Modify: `src/Menu.cpp`
- Modify: `src/UI.cpp`
- Modify: `src/UIItem.h`, `src/UIItem.cpp`
- Modify: `src/ChessSystemSettingsMenu.h`, `src/ChessSystemSettingsMenu.cpp`
- Modify: `src/SuperMenuText.h`, `src/SuperMenuText.cpp`
- Modify: `src/BattleStatsView.h`, `src/BattleStatsView.cpp`
- Modify: `tests/ButtonUnitTests.cpp`
- Modify: `tests/ChessSystemSettingsMenuUnitTests.cpp`
- Create: `tests/PointerMigrationUnitTests.cpp`

- [ ] Write failing tests for one activation per touch, down-outside/up-inside rejection, capture invalidation, slider motion, UI item dragging, battle stats taps, and the intentional two-distinct-tap `SuperMenuText` flow without timing suppression.
- [ ] Run focused tests and verify RED.
- [ ] Move hover, press, capture, wheel, drag, and activation behavior to `PointerEvent`; remove direct SDL mouse branches and `SDL_GetMouseState()` interaction queries.
- [ ] Remove `tapLockTime_`, `lastPageFlipTime_`, `kDoubleTapMinIntervalMs`, `canFlipPage()`, and duplicate-stream comments.
- [ ] Rebuild and run focused tests.

### Task 6: Migrate scene and classic battle pointer consumers

**Files:**
- Modify: `src/Scene.h`, `src/Scene.cpp`
- Modify: `src/MainScene.h`, `src/MainScene.cpp`
- Modify: `src/SubScene.h`, `src/SubScene.cpp`
- Modify: `src/BattleSceneCamera.h`, `src/BattleSceneCameraInput.cpp`
- Modify: `src/BattleCursor.h`, `src/BattleCursor.cpp`
- Modify: `src/BattleSceneHades.h`, `src/BattleSceneHades.cpp`
- Modify: `tests/PointerMigrationUnitTests.cpp`

- [ ] Write failing tests for logical-pointer pathfinding coordinates, classic camera drag, battle cursor targeting, physical right-click semantic cancel, and position-swap capture.
- [ ] Run focused tests and verify RED.
- [ ] Convert all consumers to `onPointerEvent()` and pass present-local positions/deltas directly; keep classic 2D behavior unchanged.
- [ ] Remove mouse activation from `isPressOK()` / `isPressCancel()` and add the shared semantic cancel path for physical right-click and `ApplicationCancelAction`.
- [ ] Rebuild and run focused tests.

### Task 7: Battle control layout and capture

**Files:**
- Create: `src/BattleSceneControls.h`
- Create: `src/BattleSceneControls.cpp`
- Create: `tests/BattleSceneControlsUnitTests.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: project/test source lists above.

- [ ] Write failing tests for pure layout, visibility, hit testing, same-control down/up activation, battlefield-drag rejection, and one armed touch control at a time.
- [ ] Run focused tests and verify RED.
- [ ] Extract the existing top-right rectangles and control identity into the pure module shared by drawing, mouse/primary pointer, and DirectTouch classification.
- [ ] Replace release-only activation with explicit arm/release/cancel state and increment `controlLayoutRevision` whenever visibility changes.
- [ ] Rebuild and run focused tests.

### Task 8: Paper-camera gesture recognizer and shared camera math

**Files:**
- Create: `src/BattleScenePaperCameraTouch.h`
- Create: `src/BattleScenePaperCameraTouch.cpp`
- Create: `src/PaperCameraControlMath.h`
- Create: `src/PaperCameraControlMath.cpp`
- Create: `tests/BattleScenePaperCameraTouchUnitTests.cpp`
- Create: `tests/PaperCameraControlMathUnitTests.cpp`
- Modify: project/test source lists above.

- [ ] Write failing recognizer tests for baseline, 10-pixel threshold, one/two/one transitions, order-independent pairs, control suspension/rebaseline, >2 contacts, outside starts, and cancel/reset.
- [ ] Write failing math tests for shared ground basis, pan at several angles, multiplicative pinch, rotation/height signs, named sensitivity scales, and clamps.
- [ ] Run both focused suites and verify RED.
- [ ] Implement the pure recognizer with ordered semantic actions and no Engine/BattleScene dependency.
- [ ] Implement shared keyboard/touch camera helpers with the design formulas and traditional-Chinese source comments where comments are needed.
- [ ] Rebuild and run focused tests.

### Task 9: Integrate DirectTouch paper controls

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleScenePaperCameraTouchUnitTests.cpp`
- Modify: `tests/PaperCameraControlMathUnitTests.cpp`

- [ ] Write failing integration-level tests for DirectTouch policy in paper mode, PrimaryPointer in classic mode, auto-center exit only on accepted pan/manual keyboard pan, paused camera controls, and overlay resets.
- [ ] Run focused tests and verify RED.
- [ ] Consume `TouchFrame` samples in `BattleSceneHades`, apply ordered control and camera actions, clamp once per action, and stop on policy/layout/epoch changes.
- [ ] Refactor keyboard W/A/S/D to the shared basis and make nonzero manual pan exit auto-center; keep zoom/rotation/height follow mode unchanged.
- [ ] Reset the recognizer on paper-view, overlay, owner, epoch, lifecycle, and geometry boundaries.
- [ ] Rebuild and run focused tests.

### Task 10: Platform source policy, cancel action, and VirtualStick deletion

**Files:**
- Modify: `src/Engine.cpp`, `src/Engine.h`
- Modify: `src/Application.cpp`
- Delete: `src/VirtualStick.cpp`, `src/VirtualStick.h`
- Modify: `src/RunNode.cpp`, `src/RunNode.h`
- Modify: `src/Button.h`
- Modify: `src/kys.vcxproj`, `src/kys.vcxproj.filters`
- Modify: `android/app/src/main/java/com/kysgame/kyschess/KysActivity.java`
- Modify: `wasm/shell.html`
- Modify: `tests/PointerInputUnitTests.cpp`

- [ ] Write failing source-policy tests/scans for disabled SDL synthesis/pinch hints, no VirtualStick symbols, no fake-right-click names, and no stale web touch listeners.
- [ ] Run the focused tests/scans and verify RED.
- [ ] Set `SDL_HINT_TOUCH_MOUSE_EVENTS`, `SDL_HINT_MOUSE_TOUCH_EVENTS`, and `SDL_HINT_PEN_TOUCH_EVENTS` to `0` before SDL initialization; disable SDL pinch events after initialization.
- [ ] Export/enqueue one `ApplicationCancelAction` for Android and web; enable the Android Cancel overlay only after native input-ready.
- [ ] Rename/remove platform right-click shims and unused web touch tracking while preserving DOM exclusion and `preventDefault()` / `stopPropagation()`.
- [ ] Delete VirtualStick runtime/config/controller/project integration without compatibility code.
- [ ] Rebuild and run focused tests/scans.

### Task 11: Full verification

**Files:**
- Modify only if verification exposes a defect.

- [ ] Run all focused Catch2 tags added by this plan.
- [ ] Run `x64/Debug/kys_tests.exe --reporter compact` and require zero failed test cases.
- [ ] Run `powershell -ExecutionPolicy Bypass -File ./.github/build-command.ps1` and accept only the documented final-link contention exception.
- [ ] Run `rg` scans proving no scene/UI direct SDL mouse branches, `SDL_GetMouseState()` interaction coordinates, VirtualStick symbols, or fake-right-click platform shims remain.
- [ ] Re-read every design testing bullet and record automated coverage versus Android/Emscripten device-only smoke checks.
