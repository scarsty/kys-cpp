# BattleSceneHades Simplification Migration Plan

> **For agentic workers:** Implement this plan task-by-task and update checkbox state as work lands. Prefer small compiling migrations over partial bridge layers. When a task says delete, delete the old path in the same change that migrates callers.

**Goal:** Simplify `BattleSceneHades` and the current battle runtime boundary by removing duplicated mutable state, adapter-only DTOs, per-frame mirror application code, and translation-only abstractions.

**Architecture:** `BattleRuntimeSession` owns live battle state. Scene code owns rendering, camera, input, and scene-only presentation state. Setup code creates one canonical runtime creation input. Runtime frame execution emits a scene-facing delta that is already shaped for the scene, instead of leaking many internal subsystem vectors that must be replayed into a mirrored scene model.

**Non-Negotiables:**

- No bridge artifacts: no compatibility aliases, duplicate old/new APIs, fallback double paths, or TODO-backed temporary ownership.
- No silent defensive defaults: assert invariants at boundaries where data must exist or be valid.
- No broad DTO replacement that recreates the same copying under a new name.
- No copy-paste migration. Move or refactor existing behavior into one owner.
- No behavior changes unless explicitly called out in the task.
- Use traditional Chinese for source strings/comments when text is added.

**Validation Standard:** For every non-trivial task, build `kys_tests` or the debug target. Run focused unit tests when the touched area has coverage. A final link failure caused by the game executable already running is acceptable as a build success signal for this repo.

---

## Current Problems

1. `BattleSceneUnitStore` mirrors much of `BattleRuntimeState::unitStore` and must be manually synchronized every frame.
2. `BattleSceneSetupBuilder` splits one unit setup into several parallel vectors that are immediately consumed together.
3. `BattleSceneBattleAdapter` mostly copies between scene DTOs and runtime DTOs, then projects runtime units back into scene units.
4. `BattleFrameResult` exposes many runtime-internal result vectors, forcing `BattleSceneFrameStateApplier` to know subsystem details.
5. `BattleSceneHades` still coordinates setup, camera, render traversal, pre-battle swap UI, runtime stepping, presentation playback, and post-battle reward in one class.

---

## Target Ownership

### Runtime-owned

- Unit identity used by gameplay.
- Grid position, motion, facing, vitals, stats, alive state, invincible state, frozen state, cooldown, action frame, combo shield/block render state.
- Battle result state.
- Runtime setup placement after pre-battle swap is committed.

### Scene-owned

- Camera center, manual camera drag state, close-up/freeze/slow/shake screen effects.
- `BattleAttackEffect` and `BattleTextEffect` queues.
- Hurt flash timers.
- Scene-only animation overlays such as `shake` and `attention` if they are not gameplay facts.
- Rendering metadata that runtime does not need, such as fight frame lookup and display-only skill names.
- Pre-battle swap UI state while setup placement is still editable.

### Shared by explicit value transfer only

- Setup input from scene to runtime.
- Frame delta from runtime to scene.
- Post-battle summary from runtime state plus battle report.

---

## File Map

- `src/BattleSceneHades.h/.cpp`
  - Shrink orchestration and remove mirrored unit mutation.
  - Replace adapter setup flow with direct runtime session creation.
  - Move camera/render/setup chunks after state ownership is simplified.
- `src/BattleSceneUnitStore.h/.cpp`
  - Replace with a thin scene sidecar or delete if all callers can read runtime state directly.
- `src/BattleSceneSetupBuilder.h/.cpp`
  - Build canonical runtime creation input directly.
  - Stop emitting parallel seed vectors as separate products.
- `src/BattleSceneBattleAdapter.h/.cpp`
  - Delete after setup/runtime creation call sites migrate.
- `src/BattleSceneFrameStateApplier.h/.cpp`
  - Delete or reduce to scene-only effects application after runtime emits a scene delta.
- `src/BattleScenePresentationPlayer.h/.cpp`
  - Consume runtime/scene views without depending on a full mirrored unit store.
- `src/BattleSceneImpactPlayer.h/.cpp`
  - Consume frame scene delta or presentation commands, not raw runtime internals.
- `src/BattleSceneReportPlayer.h/.cpp`
  - Resolve unit names/identity from runtime state or a narrow read-only view.
- `src/battle/BattleRuntimeSession.h/.cpp`
  - Own direct session creation, setup placement, runtime snapshot/view access, and scene delta generation.
- `src/battle/BattleCore.h/.cpp`
  - Replace fragmented frame output consumed by scene with a consolidated scene-facing delta.
- Tests under `tests/`
  - Update setup, runtime session, frame runner, and scene-side helper tests.

---

## Phase 1: Collapse Setup Into One Runtime Creation Input

**Goal:** Remove setup-time parallel vectors and the adapter input copy chain.

**Files:**

- Modify: `src/BattleSceneSetupBuilder.h`
- Modify: `src/BattleSceneSetupBuilder.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/battle/BattleRuntimeSession.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Delete: setup-only pieces in `src/BattleSceneBattleAdapter.h/.cpp`
- Update tests: `tests/BattleSceneSetupBuilderUnitTests.cpp`, `tests/BattleInitializationUnitTests.cpp`, runtime session tests if present

- [x] **Step 1: Define canonical setup output**

Change `BattleSceneSetupBuilder::BattleSceneSetupBuildResult` so it contains one runtime creation input, not several parallel vectors:

```cpp
struct BattleSceneSetupBuildResult
{
    KysChess::Battle::BattleRuntimeSessionCreationInput sessionInput;
    std::vector<KysChess::BattleSceneSetupBuilder::BattleSceneSetupDisplayUnit> displayUnits;
};
```

Only add `displayUnits` if runtime cannot own those fields. Required display-only fields should be explicit and narrow, not a second full unit model.

- [x] **Step 2: Move seed derivation into setup builder or runtime, not adapter**

While `Role` and `Magic` facts are in hand, fill `BattleRuntimeSessionCreationInput` directly. If a field can be derived from another already-present runtime setup field, derive it inside `BattleRuntimeSession::createInitialized` instead of storing it twice.

- [x] **Step 3: Remove `BattleRuntimeBuildContext` and `BattleRuntimeCreationInput`**

Delete adapter-only DTOs from `BattleSceneBattleAdapter.h`. Migrate callers to pass `BattleRuntimeSessionCreationInput` directly to `BattleRuntimeSession::createInitialized`.

- [x] **Step 4: Move definition population to runtime creation**

Move combo/equipment/neigong definition population out of `BattleSceneBattleAdapter.cpp` into runtime creation or a setup-definition builder used directly by runtime creation. Do not keep both paths.

- [x] **Step 5: Delete adapter setup conversion helpers**

Delete helpers that only re-project one setup shape into another, including setup-unit-to-roster/action/clone conversion when those are no longer needed as separate vectors.

- [x] **Step 6: Verify no bridge remains**

Run:

```powershell
rg -n "BattleRuntimeBuildContext|BattleRuntimeCreationInput|createInitializedBattleRuntimeSession|BattleSceneBattleAdapter" src tests
```

Expected: no matches, unless `BattleSceneBattleAdapter` still has unrelated behavior scheduled for deletion in the same phase. If it only exists as an include or wrapper, delete it before closing this phase.

---

## Phase 2: Make Runtime Unit State The Source Of Truth

**Goal:** Remove frame-time mirroring from runtime units into `BattleSceneUnitStore`.

**Files:**

- Modify: `src/BattleSceneHades.h/.cpp`
- Modify/Delete: `src/BattleSceneUnitStore.h/.cpp`
- Modify: `src/BattleScenePresentationPlayer.h/.cpp`
- Modify: `src/BattleSceneImpactPlayer.h/.cpp`
- Modify: `src/BattleSceneReportPlayer.h/.cpp`
- Modify: `src/battle/BattleRuntimeSession.h/.cpp`
- Update tests: `tests/BattleSceneUnitStoreUnitTests.cpp`, presentation/report/impact tests

- [x] **Step 1: Add a narrow read-only scene unit view**

Expose runtime unit data through a read-only view API on `BattleRuntimeSession`, for example:

```cpp
const BattleRuntimeUnit& requireUnit(int unitId) const;
std::span<const BattleRuntimeUnit> units() const;
```

Assert on invalid ids. Do not return default units or nullable values for required access.

- [x] **Step 2: Create a scene sidecar only for scene-owned fields**

Replace `BattleSceneUnit` with a sidecar keyed by unit id only if needed:

```cpp
struct BattleSceneUnitPresentationState
{
    std::array<int, 5> fightFrames{};
    std::string skillNames;
    int headId = -1;
    int sourceUnitId = -1;
    int chessInstanceId = -1;
    int weaponId = -1;
    int armorId = -1;
    int shake = 0;
    int attention = 0;
};
```

Fields with true required defaults may keep explicit initializers. Temporary values filled later should use `{}`.

- [x] **Step 3: Migrate draw and camera reads**

Change drawing, status bars, auto-camera, visibility checks, and selected-unit rendering in `BattleSceneHades.cpp` to read runtime units plus the sidecar. Do not copy runtime motion/vitals into scene units first.

- [x] **Step 4: Migrate pre-battle swap**

Pre-battle swap may mutate a setup placement sidecar before commit, or call a runtime setup placement editing API. After `commitSetupPlacement`, runtime owns final placement. There should not be a second scene placement store after commit.

- [x] **Step 5: Migrate post-battle summary**

Build `BattlePostBattleSummary` from runtime units and sidecar display metadata. Delete `BattleSceneUnitStore::makePostBattleSummary` if no longer needed.

- [x] **Step 6: Delete full scene unit mirror**

Remove `BattleSceneUnitStore::initialize`, `units()`, and mutation APIs that duplicate runtime state. Keep only a renamed sidecar store if scene-only data remains.

- [x] **Step 7: Verify no runtime mirror writes remain**

Run:

```powershell
rg -n "unit\.vitals|unit\.stats|unit\.motion|unit\.animation|unit\.alive|unit\.invincible|scene_units_\.units\(\)|BattleSceneUnitStore" src tests
```

Expected: no frame-time scene mirror writes. Remaining references must be sidecar-only, tests being migrated, or runtime-owned accessors.

---

## Phase 3: Replace Fragmented Frame Output With Scene Delta

**Goal:** Stop leaking runtime subsystem vectors into scene application code.

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleRuntimeSession.h/.cpp`
- Modify/Delete: `src/BattleSceneFrameStateApplier.h/.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Update tests: `tests/BattleCoreUnitTests.cpp`, `tests/BattleFrameRunnerRuntimeUnitTests.cpp`, `tests/BattleSceneFrameStateApplierUnitTests.cpp`

- [x] **Step 1: Define `BattleSceneFrameDelta` in runtime namespace**

Create a scene-facing result that contains final changed-unit snapshots and side effects already shaped for scene code:

```cpp
struct BattleSceneFrameUnitSnapshot
{
    int unitId = -1;
    // Include only facts scene must consume directly.
};

struct BattleSceneFrameDelta
{
    BattlePresentationFrame presentation;
    std::vector<BattleSceneFrameUnitSnapshot> changedUnits;
    std::vector<BattleSceneBloodEffectCommand> bloodEffects;
    std::vector<int> effectSoundIds;
    std::vector<int> attackSoundIds;
    std::vector<BattleFrameRumbleEvent> rumbles;
    std::optional<Pointf> cameraFocus;
    int closeUpFrames = 0;
    int frozenFrames = 0;
    int slowFrames = 0;
    int sceneShake = 0;
    bool battleEnded = false;
    int battleResult = -1;
};
```

Use names and fields that reflect actual final needs after inspecting current applier logic. Avoid copying every existing vector into the new struct.

- [x] **Step 2: Build scene delta inside runtime frame execution**

Move lifecycle, damage visual side effects, status render updates, and battle-end camera/freeze/slow decisions into a single delta builder close to `BattleFrameRunner`. Runtime already knows the facts; scene should not reconstruct them by interpreting internal transaction vectors.

- [x] **Step 3: Delete `BattleSceneFrameStateApplyContext` callbacks**

Remove `std::function` callbacks like `pos45To90` and `transferAntiCombo` from frame application. If runtime needs terrain conversion or anti-combo transfer, pass the required runtime data into runtime systems directly and assert required invariants there.

- [x] **Step 4: Simplify `BattleSceneHades::applyCoreFrameResult`**

It should become scene side-effect playback only: queue blood effects, play sounds/rumble, set camera/freeze/slow/shake/result, play presentation/log/report commands.

- [x] **Step 5: Delete or shrink `BattleSceneFrameStateApplier`**

If it no longer mutates mirrored unit state, delete the class and its tests. If a tiny scene-effect applier remains, rename it to match its real job and remove runtime-state concepts from it.

- [x] **Step 6: Verify no subsystem vector interpretation in scene**

Run:

```powershell
rg -n "damageRenderApplications|movementPresentationResults|unitApplications|stateApplications|rescueResults|teamEffectEvents|actionResults" src/BattleScene* src/tests tests
```

Expected: scene files do not consume these runtime-internal vectors.

---

## Phase 4: Simplify Presentation And Report Consumers

**Goal:** Presentation/report players consume narrow views and scene delta, not full scene unit mirrors.

**Files:**

- Modify: `src/BattleScenePresentationPlayer.h/.cpp`
- Modify: `src/BattleSceneImpactPlayer.h/.cpp`
- Modify: `src/BattleSceneReportPlayer.h/.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Update tests: presentation, impact, report player tests

- [x] **Step 1: Replace `BattleSceneUnitStore*` bindings**

Introduce a narrow read-only resolver interface or value bindings:

```cpp
struct BattleSceneUnitLookup
{
    std::function<const KysChess::Battle::BattleRuntimeUnit&(int)> requireRuntimeUnit;
    std::function<const BattleSceneUnitPresentationState&(int)> requirePresentationState;
};
```

If `std::function` creates unnecessary abstraction, use concrete references to `BattleRuntimeSession` and the sidecar store. Required lookup must assert on invalid ids.

- [x] **Step 2: Play runtime presentation frames directly**

Stop re-recording runtime `BattlePresentationFrame` events through `presentation_recorder_` unless the scene itself creates additional events. Runtime frames should flow directly to `BattleScenePresentationPlayer` and report playback.

- [x] **Step 3: Remove duplicate initialization event path**

Initialization log/visual events should be returned as an initial presentation frame or delta from runtime creation. Do not manually loop and re-record them in `BattleSceneHades::initializeBattleRuntime`.

- [x] **Step 4: Verify presentation has one path**

Run:

```powershell
rg -n "recordVisual|recordLog|beginPresentationFrame|publishPresentationFrame|last_presentation_frame_" src/BattleSceneHades.* src/BattleScenePresentationPlayer.*
```

Expected: either one scene-originated recorder path remains for true scene events, or all recording is removed from `BattleSceneHades`.

---

## Phase 5: Split `BattleSceneHades` By Real Responsibilities

**Goal:** After state ownership is simpler, move cohesive scene responsibilities out of the large class.

**Files:**

- Modify: `src/BattleSceneHades.h/.cpp`
- Create: `src/BattleSceneCamera.h/.cpp`
- Create: `src/BattleSceneSetupController.h/.cpp` if setup remains substantial
- Create: `src/BattleSceneRenderer.h/.cpp` only if the render function can move without excessive callback plumbing
- Update project files and tests

- [x] **Step 1: Extract camera state and behavior**

Move manual camera input, auto camera update, focus, clamp, close-up counters, and camera target into `BattleSceneCamera`. Keep rendering math inputs explicit. Assert that map dimensions and render centers are initialized before clamp/update calls.

- [x] **Step 2: Extract setup orchestration**

Move enemy/ally request construction, equipment lookup, fight-frame lookup, magic slot lookup, initial camera position, runtime creation input build, and pre-battle swap commit into a setup controller. The controller should return created runtime session plus scene presentation sidecar, not mutate half of `BattleSceneHades` through callbacks.

- [x] **Step 3: Extract renderer only if it reduces coupling**

Renderer extraction was intentionally skipped after the unit/read ownership cleanup because moving `draw()` still requires many rendering services and mutable effect queues. The lower-risk split completed here is camera extraction plus runtime/sidecar read cleanup.

Move the body of `draw()` after runtime/sidecar reads are stable. If extraction requires many mutable callbacks, stop and first narrow the data dependencies.

- [x] **Step 4: Delete empty overrides and dead helpers**

Remove empty or no-op functions where base class compatibility is not required. Keep required virtual overrides only when the base class needs them.

---

## Phase 6: Cleanup And Invariant Pass

**Goal:** Remove leftovers, enforce invariants, and make the simplified ownership obvious.

- [x] **Step 1: Delete files made obsolete**

Delete any of these if they became wrappers or empty shells:

- `src/BattleSceneBattleAdapter.h/.cpp`
- `src/BattleSceneUnitStore.h/.cpp`
- `src/BattleSceneFrameStateApplier.h/.cpp`

- [x] **Step 2: Remove project-file entries**

Update CMake, Visual Studio project files, and tests project files. Verify deleted files are not referenced.

- [x] **Step 3: Search for compatibility language**

Run:

```powershell
rg -n "legacy|compat|fallback|temporary|TODO|bridge|old path|new path" src tests docs/superpowers/plans/2026-05-17-battle-scene-hades-simplification.md
```

Review every hit. Keep only genuinely descriptive legacy game behavior, not migration leftovers.

- [x] **Step 4: Search for silent defaults**

Run:

```powershell
rg -n "return \{\}|return nullptr|return std::nullopt|if \(!.*\)\s*\{|>= 0 \? .* :|value_or|tryFind" src/BattleScene* src/battle tests
```

Review required-boundary code. Convert impossible-missing cases to `assert`. Keep `tryFind` only for true optional conditions.

- [x] **Step 5: Build and run focused tests**

Final validation completed with CMake Tools debug builds for `kys_tests` and `kys`, plus CTest `398/398` passing.

Run the debug tests target through VS Code CMake Tools or:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Then run `kys` debug build if the change touched scene/runtime integration.

---

## Suggested Commit Slices

1. Canonical setup input and adapter DTO deletion.
2. Runtime unit read API and scene sidecar introduction.
3. Draw/camera/pre-battle swap migration off full scene unit mirror.
4. Runtime scene delta and frame applier deletion.
5. Presentation/report binding cleanup.
6. `BattleSceneHades` camera/setup/render extraction.
7. Final invariant/search cleanup and project-file pruning.

Each slice should compile on its own and leave no old/new duplicate path behind.

---

## Stop Conditions

Pause and reassess if any task requires keeping both old and new ownership paths active for more than one compiling slice. That is a sign the slice is too broad or the target boundary is wrong.

Pause and add a focused test before continuing if a migration changes battle result timing, damage application order, setup unit id assignment, clone placement, rescue behavior, or presentation event ordering.