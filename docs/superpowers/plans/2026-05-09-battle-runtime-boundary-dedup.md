# Battle Runtime Boundary Dedup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove duplicated battle rule constants, split mixed scene/runtime setup inputs, and centralize repeated battle unit projection logic without changing battle behavior.

**Architecture:** Introduce one runtime-owned Hades battle rules object, then make `BattleSceneHades` provide only dynamic scene facts plus the map geometry inputs `tileWidth` and `coordCount`. Runtime rule values such as projectile speed, dash timing, melee geometry, cast config, action recovery, rescue counterattack defaults, and projectile follow-up timing are derived once by the battle runtime rules factory and stored in runtime/session setup.

**Tech Stack:** C++20, Visual Studio/MSBuild, Catch2-style `kys_tests`, existing battle runtime and adapter modules.

---

## File Structure

- Create: `src/battle/BattleRuntimeRules.h`
  - Define `BattleRuntimeRulesConfig`.
  - Declare `makeHadesBattleRuntimeRules(double tileWidth, int coordCount)`.

- Create: `src/battle/BattleRuntimeRules.cpp`
  - Own the canonical rule factory for the current Hades battle behavior.
  - Derive projectile speed, melee geometry, movement/action timing, rescue defaults, and projectile follow-up defaults from `tileWidth` and `coordCount`.

- Modify: `src/BattleSceneBattleAdapter.h`
  - Split `BattleRuntimeBuildContext` into smaller setup/rules inputs.
  - Narrow `BattleActionFrameApplyContext` to presentation-only fields.
  - Remove temporary cast adapter DTO declarations after replacing them.

- Modify: `src/BattleSceneBattleAdapter.cpp`
  - Use the runtime-owned rules factory output.
  - Move duplicated constants out of `BattleSceneHades` and avoid owning runtime rules in the adapter.
  - Populate runtime config from `BattleRuntimeRulesConfig`.
  - Centralize role-to-snapshot projection helpers.
  - Build `Battle::BattleCastInput` directly or through one builder function.

- Modify: `src/BattleSceneHades.cpp`
  - Stop declaring runtime rule constants.
  - Build only scene-owned setup input.
  - Use narrowed presentation apply context.
  - Keep presentation constants such as camera timing, status bar sizes, and local render values in the scene.

- Modify: `src/BattleSceneHades.h`
  - Rename and narrow helper signatures to match the split setup/apply contexts.

- Modify: `src/battle/BattleCore.h` and `src/battle/BattleCore.cpp`
  - Add small methods only where ownership clearly belongs in runtime types, such as snapshot creation from `BattleRuntimeUnit` if needed.
  - Keep DTOs as structs unless behavior and invariants are moved into the type.

- Modify tests:
  - `tests/BattleCoreUnitTests.cpp`
  - `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
  - Add adapter-focused tests only if an existing adapter test file exists; otherwise add focused coverage to the nearest existing battle test file.

---

## Task 1: Add a Runtime-Owned Hades Rules Factory

**Files:**
- Create: `src/battle/BattleRuntimeRules.h`
- Create: `src/battle/BattleRuntimeRules.cpp`
- Modify: project file if needed so the new `.cpp` is compiled by `kys` and `kys_tests`
- Test: `tests/BattleCoreUnitTests.cpp` or nearest existing adapter/core test file

- [ ] **Step 1: Add a failing test for Hades rules values**

Add a test that asserts the runtime rules factory returns the currently expected values. Use concrete values already present in the code so this refactor is behavior-preserving:

```cpp
TEST_CASE("BattleRuntimeRules_HadesRulesDeriveCurrentSceneValuesFromGrid")
{
    const auto rules = KysChess::Battle::makeHadesBattleRuntimeRules(
        Scene::TILE_W,
        BATTLEMAP_COORD_COUNT);

    REQUIRE(rules.gridTransform.tileWidth == Scene::TILE_W);
    REQUIRE(rules.gridTransform.coordCount == BATTLEMAP_COORD_COUNT);
    REQUIRE(rules.projectileFollowUps.projectileSpeed == Scene::TILE_W / 3.0);
    REQUIRE(rules.projectileFollowUps.minimumProjectileFrames == 20);
    REQUIRE(rules.projectileFollowUps.nearbyProjectileFramePadding == 18);
    REQUIRE(rules.projectileFollowUps.areaProjectileFramePadding == 15);
    REQUIRE(rules.projectileFollowUps.areaSpawnDistance == Scene::TILE_W * 1.5);
    REQUIRE(rules.rescueCounterAttack.projectileSpeed == Scene::TILE_W / 3.0);
    REQUIRE(rules.rescueCounterAttack.meleeAttackEffectOffset == Scene::TILE_W * 2.0);
    REQUIRE(rules.actionRecoveryFrames == 4);
    REQUIRE(rules.dashRecoveryFrames == 5);
    REQUIRE(rules.movementPhysicsDashMomentumFrames == 5);
    REQUIRE(rules.action.projectileBounceRange == 90);
}
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleRuntimeRules_HadesRulesDeriveCurrentSceneValuesFromGrid"
```

Expected before implementation: compile failure because `makeHadesBattleRuntimeRules()` and `BattleRuntimeRulesConfig` do not exist.

- [ ] **Step 3: Add `BattleRuntimeRulesConfig` to `src/battle/BattleRuntimeRules.h`**

Create the runtime rules header. It should not include `BattleSceneHades` or depend on scene-owned constants.

```cpp
#pragma once

#include "BattleCastSystem.h"
#include "BattleCore.h"
#include "BattleHitResolver.h"
#include "BattlePhysicsSystem.h"
#include "BattleTypes.h"

namespace KysChess::Battle
{

struct BattleActionRulesConfig
{
    double maxEffectiveBattleReach = 0.0;
    double meleeAttackHitRadius = 0.0;
    double meleeAttackReach = 0.0;
    double dashAttackMeleeReach = 0.0;
    double blinkWeakTargetDefWeight = 0.0;
    int dashMomentumFrames = 0;
    int movementDashCooldownFrames = 0;
    int actionRecoveryFrames = 0;
    int dashRecoveryFrames = 0;
    int strengthenedMeleeOperationCountThreshold = 0;
    int projectileBounceRange = 0;
    int coordCount = 0;
};

struct BattleRuntimeRulesConfig
{
    BattleGridTransform gridTransform;
    BattleMovementConfig movementConfig;
    BattleMovementPhysicsConfig movementPhysicsConfig;
    BattleMovementPhysicsCollisionWorld movementCollisionWorld;
    BattleCastConfig castConfig;
    BattleFrameRescueCounterAttackConfig rescueCounterAttack;
    BattleProjectileFollowUpContext projectileFollowUps;
    BattleActionRulesConfig action;
    double teamEffectHealAuraRadius = 0.0;
    double rescueExecuteUnattendedRadius = 0.0;
    int movementPhysicsDashMomentumFrames = 0;
};

BattleRuntimeRulesConfig makeHadesBattleRuntimeRules(double tileWidth, int coordCount);

}  // namespace KysChess::Battle
```

- [ ] **Step 4: Implement the factory in `src/battle/BattleRuntimeRules.cpp`**

Use runtime-owned constants inside this file. Do not read from `BattleSceneHades` or `BattleSceneBattleAdapter`. The only scene-provided inputs are `tileWidth` and `coordCount`. Move the current cast-config construction into this file as an internal `makeHadesBattleCastConfig()` helper.

```cpp
BattleRuntimeRulesConfig makeHadesBattleRuntimeRules(double tileWidth, int coordCount)
{
    assert(tileWidth > 0.0);
    assert(coordCount > 0);

    BattleRuntimeRulesConfig rules;
    rules.gridTransform = { tileWidth, coordCount };

    Battle::BattleMovementGeometry movementGeometry;
    movementGeometry.tileWidth = tileWidth;
    movementGeometry.maxRangedReach = MAX_EFFECTIVE_BATTLE_REACH;
    movementGeometry.dashFrames = DASH_MOMENTUM_FRAMES;
    movementGeometry.dashCooldownFrames = 18;
    movementGeometry.meleeAttackEffectOffset = MELEE_ATTACK_EFFECT_OFFSET;
    movementGeometry.meleeAttackHitRadius = MELEE_ATTACK_HIT_RADIUS;
    rules.movementConfig = Battle::BattleGeometry(movementGeometry).movementConfig();

    rules.teamEffectHealAuraRadius = tileWidth * 6.0;
    rules.rescueExecuteUnattendedRadius = tileWidth * 3.0;
    rules.rescueCounterAttack.visualEffectId = 11;
    rules.rescueCounterAttack.projectileSpeed = tileWidth / 3.0;
    rules.rescueCounterAttack.meleeAttackEffectOffset = MELEE_ATTACK_EFFECT_OFFSET;
    rules.rescueCounterAttack.minimumTotalFrames = 20;
    rules.rescueCounterAttack.totalFramePadding = 15;

    rules.castConfig = makeHadesBattleCastConfig();
    rules.movementPhysicsConfig.postDashSpreadFrames = 12;
    rules.movementPhysicsDashMomentumFrames = DASH_MOMENTUM_FRAMES;
    rules.action.maxEffectiveBattleReach = MAX_EFFECTIVE_BATTLE_REACH;
    rules.action.meleeAttackHitRadius = MELEE_ATTACK_HIT_RADIUS;
    rules.action.meleeAttackReach = MELEE_ATTACK_REACH;
    rules.action.dashAttackMeleeReach = MELEE_ATTACK_REACH + (MELEE_ATTACK_REACH + MELEE_ATTACK_EFFECT_OFFSET);
    rules.action.blinkWeakTargetDefWeight = 4.0;
    rules.action.dashMomentumFrames = DASH_MOMENTUM_FRAMES;
    rules.action.movementDashCooldownFrames = 18;
    rules.action.actionRecoveryFrames = ACTION_RECOVERY_FRAMES;
    rules.action.dashRecoveryFrames = DASH_MOMENTUM_FRAMES;
    rules.action.strengthenedMeleeOperationCountThreshold = STRENGTHENED_MELEE_OPERATION_COUNT_THRESHOLD;
    rules.action.projectileBounceRange = 90;
    rules.action.coordCount = coordCount;

    rules.projectileFollowUps.projectileSpeed = tileWidth / 3.0;
    rules.projectileFollowUps.minimumProjectileFrames = 20;
    rules.projectileFollowUps.nearbyProjectileFramePadding = 18;
    rules.projectileFollowUps.areaProjectileFramePadding = 15;
    rules.projectileFollowUps.areaSpawnDistance = tileWidth * 1.5;

    rules.movementCollisionWorld.tileWidth = tileWidth;
    rules.movementCollisionWorld.coordCount = coordCount;
    rules.movementCollisionWorld.defaultSeparationDistance = tileWidth * 1.5;
    return rules;
}
```

- [ ] **Step 5: Run focused test and verify it passes**

Run the same focused test command. Expected: test passes.

---

## Task 2: Split Scene Setup Input From Runtime Rules

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Test: existing initialization/runtime tests

- [ ] **Step 1: Add split input structs**

Replace the mixed `BattleRuntimeBuildContext` shape with two explicit inputs:

```cpp
struct BattleRuntimeSceneSetupInput
{
    std::vector<Role*> roles;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::vector<Battle::BattleTerrainCell> terrainCells;
    std::vector<Battle::BattleRescueCellSnapshot> rescueCells;
    std::map<std::pair<int, int>, Pointf> rescuePositionsByCell;
    std::vector<Battle::BattleSetupRosterUnit> allyRoster;
    std::vector<Battle::BattleSetupRosterUnit> enemyRoster;
    std::vector<int> obtainedNeigongMagicIds;
    std::vector<std::pair<int, int>> cloneSpawnCells;
    std::map<int, int> pendingAliveByTeam;
    int nextCloneUnitId = 100000;
    float gravity = 0.0f;
    float friction = 0.0f;
    int battleFrame = 0;
    int rescueCounterAttackSkillId = -1;
};

struct BattleRuntimeBuildContext
{
    BattleRuntimeSceneSetupInput setup;
    Battle::BattleRuntimeRulesConfig rules;
};
```

- [ ] **Step 2: Update `BattleSceneHades::makeBattleRuntimeBuildContext()`**

Keep only dynamic scene facts in `context.setup`, and assign `context.rules = KysChess::Battle::makeHadesBattleRuntimeRules(TILE_W, BATTLE_COORD_COUNT)`.

Important assignments:

```cpp
context.rules = KysChess::Battle::makeHadesBattleRuntimeRules(TILE_W, BATTLE_COORD_COUNT);
context.rules.movementPhysicsConfig.gravity = gravity_;
context.rules.movementPhysicsConfig.friction = friction_;

context.setup.roles = battle_roles_;
context.setup.comboStates = &KysChess::ChessCombo::getMutableStates();
context.setup.cloneSpawnCells = clone_spawn_positions_;
context.setup.pendingAliveByTeam.emplace(1, static_cast<int>(enemies_.size()));
context.setup.gravity = gravity_;
context.setup.friction = friction_;
context.setup.battleFrame = battle_frame_;
```

For the basic counterattack skill:

```cpp
auto* basicMagic = Save::getInstance()->getMagic(1);
assert(basicMagic);
context.setup.rescueCounterAttackSkillId = basicMagic->ID;
```

- [ ] **Step 3: Update adapter code to read from `context.setup` and `context.rules`**

Examples:

```cpp
assert(context.setup.comboStates);
init.runtime.units.gridTransform = context.rules.gridTransform;
init.runtime.combo.units = *context.setup.comboStates;
init.setup = makeBattleRuntimeSetupSeed(context.setup, context.rules);
```

Runtime configuration should use rules:

```cpp
runtime.world.frame = context.setup.battleFrame;
runtime.world.config = context.rules.movementConfig;
runtime.world.terrainCells = context.setup.terrainCells;
runtime.teamEffects.healAuraRadius = context.rules.teamEffectHealAuraRadius;
runtime.rescue.executeUnattendedRadius = context.rules.rescueExecuteUnattendedRadius;
runtime.rescue.counterAttack = context.rules.rescueCounterAttack;
runtime.rescue.counterAttack.skillId = context.setup.rescueCounterAttackSkillId;
runtime.movementPhysics.config = context.rules.movementPhysicsConfig;
runtime.movementPhysics.collision = context.rules.movementCollisionWorld;
runtime.movementPhysics.actionCastFrames.assign(
    context.rules.castConfig.castFrames.begin(),
    context.rules.castConfig.castFrames.end());
runtime.movementPhysics.dashMomentumFrames = context.rules.movementPhysicsDashMomentumFrames;
runtime.action.castFrames.assign(
    context.rules.castConfig.castFrames.begin(),
    context.rules.castConfig.castFrames.end());
runtime.action.actionRecoveryFrames = context.rules.action.actionRecoveryFrames;
runtime.action.dashRecoveryFrames = context.rules.action.dashRecoveryFrames;
runtime.action.blinkWeakTargetDefWeight = context.rules.action.blinkWeakTargetDefWeight;
runtime.action.strengthenedMeleeOperationCountThreshold = context.rules.action.strengthenedMeleeOperationCountThreshold;
runtime.action.projectileBounceRange = context.rules.action.projectileBounceRange;
runtime.projectileFollowUps = context.rules.projectileFollowUps;
```

Planning code that currently consumes `BattleActionFrameAdapterConfig` should consume `context.rules.action` instead.

- [ ] **Step 4: Run build and tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: build succeeds and all tests pass.

---

## Task 3: Narrow Frame Apply Context To Presentation Only

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Test: existing action/frame tests

- [ ] **Step 1: Replace `BattleActionFrameApplyContext` config with explicit presentation fields**

Change the struct to:

```cpp
struct BattleActionFrameApplyContext
{
    const std::vector<Role*>* roles = nullptr;
    int battleFrame = 0;
    float gravity = 0.0f;
};
```

- [ ] **Step 2: Update `makeBattleActionFrameApplyContext()`**

The scene should only pass:

```cpp
BattleActionFrameApplyContext BattleSceneHades::makeBattleActionFrameApplyContext() const
{
    BattleActionFrameApplyContext context;
    context.roles = &battle_roles_;
    context.battleFrame = battle_frame_;
    context.gravity = gravity_;
    return context;
}
```

- [ ] **Step 3: Update frame apply users**

Replace:

```cpp
role->Acceleration = { 0, 0, context.config.gravity };
role->PreActTimer = context.config.battleFrame;
```

with:

```cpp
role->Acceleration = { 0, 0, context.gravity };
role->PreActTimer = context.battleFrame;
```

- [ ] **Step 4: Remove apply-time reads of action rule fields**

Search:

```powershell
rg -n "context\.config|BattleActionFrameAdapterConfig" src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected after this task: `BattleActionFrameAdapterConfig` has been removed or renamed to runtime-owned `Battle::BattleActionRulesConfig`, and frame presentation apply code has no `context.config` use.

- [ ] **Step 5: Run build and tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: build succeeds and all tests pass.

---

## Task 4: Remove Adapter Cast DTO Duplication

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Test: existing cast/action tests

- [ ] **Step 1: Add focused tests around cast input construction behavior**

Cover the fields most likely to shift during DTO removal:

```cpp
TEST_CASE("BattleActionPlanning_PreservesDashAndProjectileCastFields")
{
    // Use the existing action planning test fixture or add to the nearest battle action test file.
    // Assert selected cast input keeps projectile speed multiplier, dash attack reach,
    // dash velocity, cooldown reduction, operation count, and strengthened melee fields.
}
```

If no fixture exists, skip adding a new synthetic fixture and rely on existing action tests plus full `kys_tests`.

- [ ] **Step 2: Replace `BattleCastSkillAdapterInput` with direct `Battle::BattleCastSkillState` construction**

Change `makeActionFrameSkillInput()` to return `Battle::BattleCastSkillState`:

```cpp
Battle::BattleCastSkillState makeActionFrameSkillState(
    Role* role,
    Magic* magic,
    bool ultimate,
    const BattleActionPlanInputContext& context,
    bool consumeFrameSkillBonuses)
```

Fill the core type directly.

- [ ] **Step 3: Replace `BattleCastAdapterInput` with direct `Battle::BattleCastInput` construction**

Change `makeActionFrameCastInput()` to build `Battle::BattleCastInput` directly:

```cpp
Battle::BattleCastInput castInput;
castInput.config = context.rules.castConfig or makeBattleCastConfig();
castInput.geometry = makeBattleCastGeometry();
castInput.unit.id = role->ID;
castInput.unit.position = role->Pos;
castInput.unit.facing = role->RealTowards;
castInput.unit.alive = role->Dead == 0;
castInput.unit.frozen = role->Frozen > 0;
castInput.unit.stunned = role->HurtFrame > 0;
castInput.unit.canStartAttack = canStartAttack;
castInput.unit.mp = role->MP;
castInput.unit.maxMp = role->MaxMP;
castInput.unit.speed = role->Speed;
castInput.unit.cooldownReductionPct = cooldownReductionPct;
castInput.unit.operationCount = role->OperationCount;
castInput.unit.meleeAttackReach = context.config.meleeAttackReach;
castInput.unit.dashAttackReach = context.config.dashAttackMeleeReach;
castInput.unit.hasEquippedSkill = normalMagic != nullptr;
castInput.unit.movementDashActive = movementDashActive;
castInput.unit.dashAttackEnabled = dashAttackEnabled;
castInput.unit.dashVelocity = dashVelocity;
castInput.unit.dashHitCount = rollDashHitCount(role, selectedMagic, context);
castInput.unit.emitDashFollowUpSkillAttack = dashAttackEnabled && selectedMagic;
castInput.unit.dashFollowUpOperationType = selectedMagic ? Battle::battleOperationFromLegacy(legacyOperationForAttackArea(selectedMagic->AttackAreaType)) : Battle::BattleOperationType::None;
castInput.normalSkill = makeActionFrameSkillState(role, normalMagic, false, context, consumeFrameSkillBonuses);
castInput.ultimateSkill = makeActionFrameSkillState(role, ultimateMagic, true, context, consumeFrameSkillBonuses);
```

- [ ] **Step 4: Delete temporary DTO structs and conversion helpers**

Remove from `BattleSceneBattleAdapter.h`:

```cpp
struct BattleCastSkillAdapterInput;
struct BattleCastAdapterInput;
Battle::BattleCastSkillState makeBattleCastSkillState(...);
Battle::BattleCastInput makeBattleCastInput(...);
```

Remove corresponding definitions from `BattleSceneBattleAdapter.cpp`.

- [ ] **Step 5: Run search, build, and tests**

Run:

```powershell
rg -n "BattleCastSkillAdapterInput|BattleCastAdapterInput|makeBattleCastSkillState|makeBattleCastInput" src tests
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: search returns no matches for removed adapter DTO names; build and tests pass.

---

## Task 5: Centralize Unit Snapshot Projection

**Files:**
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Optionally modify: `src/battle/BattleCore.h`
- Optionally modify: `src/battle/BattleCore.cpp`
- Test: existing frame/damage/status tests

- [ ] **Step 1: Add a local role snapshot helper in the adapter**

Add one helper in the anonymous namespace:

```cpp
struct RoleBattleSnapshot
{
    int id = -1;
    int realRoleId = -1;
    std::string name;
    int team = 0;
    bool alive = true;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
    int invincible = 0;
    int hurtFrame = 0;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf facing;
};

RoleBattleSnapshot makeRoleBattleSnapshot(Role* role)
{
    assert(role);
    return {
        role->ID,
        role->RealID,
        role->Name,
        role->Team,
        role->Dead == 0,
        role->HP,
        role->MaxHP,
        role->MP,
        role->MaxMP,
        role->Attack,
        role->Defence,
        role->Speed,
        role->Invincible,
        role->HurtFrame,
        role->Pos,
        role->Velocity,
        role->Acceleration,
        role->RealTowards,
    };
}
```

- [ ] **Step 2: Rewrite simple mappers to use the helper**

Update:

```cpp
makeBattleAttackUnit(Role* role)
makeBattleWorldUnit(Role* role)
makeBattleActionCommitUnitSnapshot(Role* role)
makeBattleActionTargetSnapshot(Role* role)
makeBattleStatusUnit(Role* role, const RoleComboState& state)
makeBattleDamageUnit(Role* role, const RoleComboState* state)
```

Each function should call `const auto snapshot = makeRoleBattleSnapshot(role);` once and copy from the snapshot.

- [ ] **Step 3: Keep null-tolerant behavior only where it is genuinely used**

`makeBattleStatusUnit()` and `makeBattleDamageUnit()` currently accept null roles. Search callers:

```powershell
rg -n "makeBattleStatusUnit\(|makeBattleDamageUnit\(" src tests
```

If all production callers pass non-null roles, replace nullable behavior with `assert(role)` and remove fake null defaults. If any real caller passes null, keep the nullable path only for that function and document it in the function name or call site.

- [ ] **Step 4: Prefer runtime-owned projection where source is already `BattleRuntimeUnit`**

If projection starts from `BattleRuntimeUnit`, use existing runtime helpers:

```cpp
Battle::makeBattleStatusUnitState(status, runtimeUnit)
Battle::makeBattleDamageUnitState(runtimeUnit, damageRuntime)
```

Do not add another adapter-side mapper for runtime-owned fields.

- [ ] **Step 5: Run searches, build, and tests**

Run:

```powershell
rg -n "unit\.id = role->ID|unit\.team = role->Team|unit\.alive = role->Dead|snapshot\.id = role->ID|snapshot\.team = role->Team" src\BattleSceneBattleAdapter.cpp
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: repeated direct role-field assignments are reduced to the single `makeRoleBattleSnapshot()` helper plus any justified special cases; build and tests pass.

---

## Task 6: Final Boundary Verification

**Files:**
- No new implementation files.
- Verify all modified files.

- [ ] **Step 1: Run formatting and boundary searches**

Run:

```powershell
git diff --check
rg -n "PROJECTILE_SPEED|MELEE_ATTACK_EFFECT_OFFSET|MELEE_ATTACK_HIT_RADIUS|MELEE_ATTACK_REACH|DASH_MOMENTUM_FRAMES|ACTION_RECOVERY_FRAMES|PROJECTILE_BOUNCE_RANGE" src\BattleSceneHades.cpp
rg -n "BattleCastSkillAdapterInput|BattleCastAdapterInput|makeBattleCastSkillState|makeBattleCastInput" src tests
rg -n "context\.config" src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected:
- `git diff --check` exit code 0.
- No runtime-rule constants remain in `BattleSceneHades.cpp`.
- Removed cast adapter DTO names have no matches.
- `context.config` does not appear in frame apply code.

- [ ] **Step 2: Run full verification**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected:
- `kys_tests` target builds.
- `x64\Debug\kys_tests.exe` passes.
- `kys` target builds. Existing warnings may remain, but no new errors should appear.

---

## Review Notes

- Keep DTOs as structs unless the type owns invariants or behavior. The important cleanup is centralizing construction and ownership, not changing `struct` to `class` mechanically.
- Do not move camera, status bar, sound presentation, or local render constants into runtime rules.
- Do not preserve backwards-compatible adapter APIs. Remove old temporary structs and migrate callers directly.
- Use `assert` for invariants such as non-null `Role*` where callers are expected to provide valid roles.
- Avoid broad file splits in this batch. The target is ownership cleanup, not a full adapter decomposition.
