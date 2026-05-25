# Legacy Naming Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove misleading `legacy` naming from C++ source and C++ tests while preserving the intentionally named browser fallback API.

**Architecture:** This is a behavior-preserving C++ rename pass. Names should describe the current domain owner: configured triggers, presentation operation kinds, hit-shape damage, test fixtures, browser fallback, or old UI compile stubs. `GameUtil::isLegacyBrowser()` remains named as-is; non-C++ files are out of scope.

**Tech Stack:** C++23, Catch2, PowerShell, ripgrep, Visual Studio/MSBuild via `.github/build-command.ps1`.

---

## Scope Rules

- Only edit C++ source and C++ test files:
  - `src/**/*.h`
  - `src/**/*.cpp`
  - `tests/**/*.h`
  - `tests/**/*.cpp`
- Keep browser detection names:
  - `src/GameUtil.h::isLegacyBrowser`
  - calls to `GameUtil::isLegacyBrowser()`
- Do not edit non-C++ files.
- Rename every other C++ `legacy` identifier, test title, comment, and local variable to the current domain term.

## Rename Map

Use these exact replacement concepts:

```text
BattleLegacyHitShapeInput            -> BattleHitShapeInput
BattleLegacyHitShapeResult           -> BattleHitShapeResult
shapeLegacyHitDamage                 -> shapeHitDamage
hookMatchesLegacyTrigger             -> hookMatchesConfiguredTrigger
LegacyRangedSideProjectileAngle      -> RangedSideProjectileAngle
LEGACY_CAST_FRAMES                   -> DEFAULT_CAST_FRAMES
toLegacyOperationType                -> toPresentationOperationKind
battleOperationFromLegacy            -> delete if still unused
legacyOperation                      -> operationIndex
LegacyMinimumVectorNorm              -> TestMinimumVectorNorm
LegacyMinimumFacingNorm              -> TestMinimumFacingNorm
Legacy* test constants               -> Test* constants
legacyCastConfig                     -> testCastConfig
legacyGameplayProjectile             -> gameplayProjectile
```

---

## Task 1: Capture Baseline And Final Allowlist

**Files:**
- Inspect: `src`
- Inspect: `tests`

- [ ] **Step 1: Capture C++ `legacy` hits**

Run:

```powershell
rg -n "\blegacy\b|Legacy|LEGACY|legacy[A-Z_]|[A-Za-z0-9_]legacy" src tests -g "*.h" -g "*.cpp"
```

Expected: output includes battle source/test references listed in the rename map plus `GameUtil::isLegacyBrowser()` and call sites.

- [ ] **Step 2: Commit only if files changed**

No source edits are expected in this task. Do not commit unless implementation notes were intentionally added.

---

## Task 2: Rename Production Battle Concepts

**Files:**
- Modify: `src/battle/BattleDamageSystem.h`
- Modify: `src/battle/BattleDamageSystem.cpp`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Modify: `src/battle/BattleCastSystem.cpp`
- Modify: `src/battle/BattleRuntimeRules.cpp`
- Modify: `src/battle/BattleOperation.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleDamageSystemUnitTests.cpp`

- [ ] **Step 1: Rename hit-shape damage API**

In `src/battle/BattleDamageSystem.h`, replace:

```cpp
struct BattleLegacyHitShapeInput
```

with:

```cpp
struct BattleHitShapeInput
```

Replace:

```cpp
struct BattleLegacyHitShapeResult
```

with:

```cpp
struct BattleHitShapeResult
```

Replace the declaration:

```cpp
BattleLegacyHitShapeResult shapeLegacyHitDamage(const BattleLegacyHitShapeInput& input) const;
```

with:

```cpp
BattleHitShapeResult shapeHitDamage(const BattleHitShapeInput& input) const;
```

In `src/battle/BattleDamageSystem.cpp`, replace the definition header:

```cpp
BattleLegacyHitShapeResult BattleDamageSystem::shapeLegacyHitDamage(const BattleLegacyHitShapeInput& input) const
```

with:

```cpp
BattleHitShapeResult BattleDamageSystem::shapeHitDamage(const BattleHitShapeInput& input) const
```

Inside that function, replace:

```cpp
BattleLegacyHitShapeResult result;
```

with:

```cpp
BattleHitShapeResult result;
```

- [ ] **Step 2: Update hit resolver call sites**

In `src/battle/BattleHitResolver.cpp`, replace:

```cpp
BattleLegacyHitShapeInput shapeInput;
```

with:

```cpp
BattleHitShapeInput shapeInput;
```

Replace:

```cpp
const auto shaped = BattleDamageSystem().shapeLegacyHitDamage(shapeInput);
```

with:

```cpp
const auto shaped = BattleDamageSystem().shapeHitDamage(shapeInput);
```

In `tests/BattleDamageSystemUnitTests.cpp`, replace:

```cpp
BattleLegacyHitShapeInput input;
auto result = BattleDamageSystem().shapeLegacyHitDamage(input);
```

with:

```cpp
BattleHitShapeInput input;
auto result = BattleDamageSystem().shapeHitDamage(input);
```

- [ ] **Step 3: Rename configured trigger matcher**

In `src/battle/BattleComboTriggerSystem.cpp`, replace:

```cpp
bool hookMatchesLegacyTrigger(BattleComboTriggerHook hook, Trigger trigger)
```

with:

```cpp
bool hookMatchesConfiguredTrigger(BattleComboTriggerHook hook, Trigger trigger)
```

Then replace both call sites:

```cpp
hookMatchesLegacyTrigger(input.hook, effect.trigger)
```

with:

```cpp
hookMatchesConfiguredTrigger(input.hook, effect.trigger)
```

- [ ] **Step 4: Rename cast spread and default cast frame constants**

In `src/battle/BattleCastSystem.cpp`, replace:

```cpp
constexpr double LegacyRangedSideProjectileAngle = 3.14159265358979323846 / 12.0;
```

with:

```cpp
constexpr double RangedSideProjectileAngle = 3.14159265358979323846 / 12.0;
```

Replace all `LegacyRangedSideProjectileAngle` usages with `RangedSideProjectileAngle`.

In `src/battle/BattleRuntimeRules.cpp`, replace:

```cpp
constexpr std::array<int, 4> LEGACY_CAST_FRAMES = { 25, 30, 20, 25 };
```

with:

```cpp
constexpr std::array<int, 4> DEFAULT_CAST_FRAMES = { 25, 30, 20, 25 };
```

Replace:

```cpp
config.castFrames = LEGACY_CAST_FRAMES;
```

with:

```cpp
config.castFrames = DEFAULT_CAST_FRAMES;
```

- [ ] **Step 5: Rename presentation operation helpers and delete unused conversion**

First verify the reverse conversion is unused outside its definition:

```powershell
rg -n "battleOperationFromLegacy" src tests
```

Expected: one match in `src/battle/BattleOperation.h`.

In `src/battle/BattleOperation.h`, replace:

```cpp
inline int toLegacyOperationType(BattleOperationType operation)
{
    return static_cast<int>(operation);
}
```

with:

```cpp
inline int toPresentationOperationKind(BattleOperationType operation)
{
    return static_cast<int>(operation);
}
```

Then delete this unused helper:

```cpp
inline BattleOperationType battleOperationFromLegacy(int operationType)
{
    switch (operationType)
    {
    case 0:
        return BattleOperationType::Melee;
    case 1:
        return BattleOperationType::TrackingProjectile;
    case 2:
        return BattleOperationType::RangedProjectile;
    case 3:
        return BattleOperationType::Dash;
    default:
        return BattleOperationType::None;
    }
}
```

- [ ] **Step 6: Update operation helper call sites**

In `src/battle/BattleCore.cpp`, replace both presentation assignments:

```cpp
presentation.operationKind = toLegacyOperationType(attack->state.operationType);
presentation.operationKind = toLegacyOperationType(event.operationType);
```

with:

```cpp
presentation.operationKind = toPresentationOperationKind(attack->state.operationType);
presentation.operationKind = toPresentationOperationKind(event.operationType);
```

Replace `actionCastFrame(...)` with:

```cpp
int actionCastFrame(const BattleRuntimeState& state, BattleOperationType operationType)
{
    if (!isBattleOperation(operationType))
    {
        return 0;
    }
    const int operationIndex = battleOperationIndex(operationType);
    assert(static_cast<std::size_t>(operationIndex) < state.action.castFrames.size());
    return state.action.castFrames[operationIndex];
}
```

In `src/BattleSceneHades.cpp`, replace:

```cpp
return KysChess::Battle::toLegacyOperationType(
```

with:

```cpp
return KysChess::Battle::toPresentationOperationKind(
```

- [ ] **Step 7: Run focused battle build and tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][damage]"
.\x64\Debug\kys_tests.exe "[battle][hit_resolver]"
.\x64\Debug\kys_tests.exe "[battle][combo]"
.\x64\Debug\kys_tests.exe "[battle][cast]"
.\x64\Debug\kys_tests.exe "[battle][core]"
```

Expected: build succeeds and all focused tests pass.

- [ ] **Step 8: Commit**

Run:

```powershell
git add src\battle src\BattleSceneHades.cpp tests
git commit -m "refactor: rename current battle concepts"
```

---

## Task 3: Rename Test Fixtures And Test Titles

**Files:**
- Modify: `tests/BattleCastSystemUnitTests.cpp`
- Modify: `tests/BattleAttackSystemUnitTests.cpp`
- Modify: `tests/BattleDamageSystemUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/BattleComboTriggerSystemUnitTests.cpp`
- Modify: `tests/BattlePresentationEffectsUnitTests.cpp`
- Modify: `tests/BattleSceneFrameApplierUnitTests.cpp`

- [ ] **Step 1: Rename test constants**

In `tests/BattleCastSystemUnitTests.cpp`, apply these exact replacements:

```text
LegacyMinimumFacingNorm -> TestMinimumFacingNorm
LegacyActionRecoveryFrames -> TestActionRecoveryFrames
LegacyDashMomentumFrames -> TestDashMomentumFrames
LegacyNormalCastMpDelta -> TestNormalCastMpDelta
LegacyCooldownAfterCastPadding -> TestCooldownAfterCastPadding
LegacyCooldownMaxSpeed -> TestCooldownMaxSpeed
LegacySpeedCooldownReductionRatio -> TestSpeedCooldownReductionRatio
LegacyMeleeHitTotalFrame -> TestMeleeHitTotalFrame
LegacyStrengthenedMeleeTotalFrame -> TestStrengthenedMeleeTotalFrame
LegacyStrengthenedMeleeSelectDistanceDivisor -> TestStrengthenedMeleeSelectDistanceDivisor
LegacyStrengthenedMeleeMultiplier -> TestStrengthenedMeleeMultiplier
LegacyStrengthenedMeleeOperationCountThreshold -> TestStrengthenedMeleeOperationCountThreshold
LegacyMeleeSplashTotalFrame -> TestMeleeSplashTotalFrame
LegacyMeleeSplashInitialFrame -> TestMeleeSplashInitialFrame
LegacyMeleeSplashStrengthMultiplier -> TestMeleeSplashStrengthMultiplier
LegacyTrackingProjectileTotalFrame -> TestTrackingProjectileTotalFrame
LegacyDashHitTotalFrame -> TestDashHitTotalFrame
LegacyMeleeSplashProjectileSpeed -> TestMeleeSplashProjectileSpeed
LegacyDashHitPositionSpacing -> TestDashHitPositionSpacing
LegacyDashHitFrameStep -> TestDashHitFrameStep
legacyCastConfig -> testCastConfig
```

In `tests/BattleAttackSystemUnitTests.cpp`, `tests/BattleCoreUnitTests.cpp`, and `tests/BattleFrameRunnerRuntimeUnitTests.cpp`, replace:

```text
LegacyMinimumVectorNorm -> TestMinimumVectorNorm
```

- [ ] **Step 2: Rename test cases that describe current behavior**

Apply these exact test title replacements:

```text
BattleDamageSystem_MagicBaseDamageUsesLegacyAttackDefenseCurve -> BattleDamageSystem_MagicBaseDamageUsesAttackDefenseCurve
BattleDamageSystem_LegacyHitShapeOwnsProjectileFalloffFacingAndOperationDamage -> BattleDamageSystem_HitShapeOwnsProjectileFalloffFacingAndOperationDamage
BattleDamageSystem_LegacyDashHitShapeEmitsFreezeAndReducedDashDamage -> BattleDamageSystem_DashHitShapeEmitsFreezeAndReducedDashDamage
BattleCastSystem_MeleeSpawnUsesLegacyOriginAndFrameCount -> BattleCastSystem_MeleeSpawnUsesConfiguredOriginAndFrameCount
BattleCastSystem_StrengthenedMeleeSpawnUsesLegacyTrackingProjectileShape -> BattleCastSystem_StrengthenedMeleeSpawnUsesTrackingProjectileShape
BattleCastSystem_OperationOneSpawnTracksForLegacyFrameCount -> BattleCastSystem_TrackingProjectileSpawnUsesConfiguredFrameCount
BattleCastSystem_RangedAreaCastEmitsLegacySideProjectiles -> BattleCastSystem_RangedAreaCastEmitsSideProjectiles
BattleCastSystem_TrackingUltimateEmitsLegacyTwoProjectileSpread -> BattleCastSystem_TrackingUltimateEmitsTwoProjectileSpread
BattleAttackSystem_ApplyProjectileCancelDamageCommitsLegacyWeakenRules -> BattleAttackSystem_ApplyProjectileCancelDamageCommitsWeakenRules
BattleComboTriggerSystem_ArmorPenetration_AppliesLegacyAndTriggeredPen -> BattleComboTriggerSystem_ArmorPenetration_AppliesAlwaysAndTriggeredPen
BattleSceneFrameApplier_AppliesProjectileVisualEventsToLegacyAttackEffects -> BattleSceneFrameApplier_AppliesProjectileVisualEventsToSceneAttackEffects
```

- [ ] **Step 3: Rename presentation test local variable**

In `tests/BattlePresentationEffectsUnitTests.cpp`, replace:

```cpp
BattleAttackEffect legacyGameplayProjectile;
legacyGameplayProjectile.TotalFrame = 3;
effects.push_back(legacyGameplayProjectile);
```

with:

```cpp
BattleAttackEffect gameplayProjectile;
gameplayProjectile.TotalFrame = 3;
effects.push_back(gameplayProjectile);
```

- [ ] **Step 4: Run focused test files**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][attack]"
.\x64\Debug\kys_tests.exe "[battle][cast]"
.\x64\Debug\kys_tests.exe "[battle][damage]"
.\x64\Debug\kys_tests.exe "[battle][combo]"
.\x64\Debug\kys_tests.exe "[battle][scene_frame_applier]"
```

Expected: build succeeds and all focused tests pass.

- [ ] **Step 5: Commit**

Run:

```powershell
git add tests
git commit -m "test: rename current battle behavior fixtures"
```

---

## Task 4: Rename C++ Non-Browser Comments And Locals

**Files:**
- Modify: `src/BattleScene.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/GameUtil.h`
- Modify: `src/SubScene.cpp`
- Modify: `src/Types.h`

- [ ] **Step 1: Rename scene fallback comments and locals**

In `src/BattleScene.cpp`, replace:

```cpp
// make this legacy ONLY
```

with:

```cpp
// Browser fallback path only
```

In `src/BattleSceneHades.cpp`, replace:

```cpp
// make this legacy only
```

with:

```cpp
// Browser fallback path only
```

Replace:

```cpp
bool is_legacy = GameUtil::isLegacyBrowser();
use_whole_scene_ = !is_legacy
```

with:

```cpp
bool browserNeedsFallback = GameUtil::isLegacyBrowser();
use_whole_scene_ = !browserNeedsFallback
```

Replace:

```cpp
LOG("whole_scene disabled: legacy={}, required={}x{}, renderer max={}\n",
    is_legacy, whole_scene_w, whole_scene_h, max_texture_size);
```

with:

```cpp
LOG("whole_scene disabled: browser_fallback={}, required={}x{}, renderer max={}\n",
    browserNeedsFallback, whole_scene_w, whole_scene_h, max_texture_size);
```

In `src/SubScene.cpp`, replace:

```cpp
// legacy device only
```

with:

```cpp
// Browser fallback path only
```

In `src/GameUtil.h`, keep the function name `isLegacyBrowser()` but replace the log text:

```cpp
std::print("Legacy browser is {}\n", cached);
```

with:

```cpp
std::print("Browser fallback is {}\n", cached);
```

- [ ] **Step 2: Rename old UI compatibility comments**

In `src/Types.h`, replace:

```cpp
MAX_MAGIC_LEVEL_INDEX = 9,     // kept for legacy compat stubs
```

with:

```cpp
MAX_MAGIC_LEVEL_INDEX = 9,     // kept for old UI compile stubs
```

Replace:

```cpp
// === Legacy game fields (NOT persisted, kept for compilation of old UI code) ===
```

with:

```cpp
// === Old UI game fields (NOT persisted, kept for compilation of old UI code) ===
```

- [ ] **Step 3: Run focused build**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

Run:

```powershell
git add src\BattleScene.cpp src\BattleSceneHades.cpp src\GameUtil.h src\SubScene.cpp src\Types.h
git commit -m "refactor: rename cxx fallback terminology"
```

---

## Task 5: Final Legacy Naming Gate

**Files:**
- Inspect: `src`
- Inspect: `tests`

- [ ] **Step 1: Verify remaining C++ matches are allowlisted**

Run:

```powershell
rg -n "\blegacy\b|Legacy|LEGACY|legacy[A-Z_]|[A-Za-z0-9_]legacy" src tests -g "*.h" -g "*.cpp"
```

Expected: matches remain only for `GameUtil::isLegacyBrowser()` and its C++ call sites in browser fallback code.

- [ ] **Step 2: Verify no battle source or tests matches remain**

Run:

```powershell
rg -n "\blegacy\b|Legacy|LEGACY|legacy[A-Z_]|[A-Za-z0-9_]legacy" src\battle tests -g "*.h" -g "*.cpp"
```

Expected: no output.

- [ ] **Step 3: Run full battle tests**

Run:

```powershell
.\x64\Debug\kys_tests.exe "[battle]"
```

Expected: all battle tests pass.

- [ ] **Step 4: Run project build**

Run:

```powershell
.\.github\build-command.ps1
```

Expected: compile succeeds. Final link failure is acceptable only if caused by a running game process locking the binary.

- [ ] **Step 5: Commit cleanup only if files changed**

Run:

```powershell
git status --short
```

If there are cleanup changes:

```powershell
git add src tests
git commit -m "refactor: finish legacy naming cleanup"
```

---

## Execution Notes

- Do not change behavior while renaming.
- Do not add compatibility aliases for renamed C++ APIs.
- Do not edit non-C++ files.
- Do not rename `GameUtil::isLegacyBrowser()`.
- Prefer IDE rename or mechanical search/replace over hand-edited partial renames.
- Use Traditional Chinese if any source-visible Chinese text is added or changed.

## Plan Self-Review

- Spec coverage: the plan keeps browser legacy naming and removes misleading non-browser legacy naming from C++ source and C++ tests only.
- Red flag scan: no blocked wording markers remain.
- Type consistency: renamed C++ types and methods are introduced before use; deleted operation conversion is verified as unused by the call-site update task.
