# Battle Adapter Ownership Reduction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Shrink `BattleSceneBattleAdapter` by removing dead surface area, fixing setup-side action RNG, and moving action planning computation from the adapter into runtime-owned frame code.

**Architecture:** `BattleSceneBattleAdapter` remains the legacy edge translator for `Role` and `Magic`, but it should not own runtime rules, runtime random rolls, or frame action planning. The adapter may seed immutable legacy facts; `BattleRuntimeSession` / `BattleFrameRunner` owns per-frame cast input construction, combo-dependent action rules, random dash-hit rolling, and runtime maintenance. Presentation apply stays legacy-facing for now.

**Tech Stack:** C++20, Visual Studio/MSBuild, Catch2-style `kys_tests`, existing battle runtime modules.

---

## File Structure

- Modify: `src/BattleSceneBattleAdapter.h`
  - Remove dead/public helper declarations.
  - Narrow `BattleActionFrameApplyResult`.
  - Replace setup-time action planning context with seed-only input after runtime planning is moved.

- Modify: `src/BattleSceneBattleAdapter.cpp`
  - Delete unused helpers.
  - Keep legacy projection helpers private in the anonymous namespace.
  - Replace full `BattleCastInput` setup construction with legacy skill/action seed construction.

- Modify: `src/BattleSceneHades.cpp`
  - Remove loops over empty adapter result fields.
  - Continue to consume explicit runtime frame results and presentation-only adapter output.

- Modify: `src/battle/BattleCore.h`
  - Add runtime-owned action skill seed types.
  - Add runtime action rules/config storage needed to build cast inputs in core.
  - Replace or phase out `ActionState::castPlanInputs` with runtime-owned action plan seeds.

- Modify: `src/battle/BattleCore.cpp`
  - Move dash-hit rolling to runtime using `BattleRuntimeRandom`.
  - Build/refine `BattleCastInput` from runtime unit state, combo state, runtime rules, and seeded skill facts.
  - Preserve existing directive-based selected cast inputs for tests and explicit callers.

- Modify tests:
  - `tests/BattleCoreUnitTests.cpp`
  - Existing cast/action tests only if affected by type moves.

---

## Task 1: Remove Dead Adapter Helpers And Empty Apply Outputs

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Confirm the dead helper set**

Run:

```powershell
rg -n "\bmakeBattleAttackUnit\b|\bmakeBattleActionCommitUnitSnapshot\b|\bmakeBattleActionTargetSnapshot\b|\bactionRandomInt\b|\bmakeBlinkGeometryInput\b|\bpositionForCell\b|\bcellWalkable\b" src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h src\BattleSceneHades.cpp src\battle
```

Expected before cleanup: matches are only definitions/declarations and internal references from the dead blink helper block.

- [ ] **Step 2: Remove unused `BattleActionFrameApplyResult` fields**

In `src/BattleSceneBattleAdapter.h`, change:

```cpp
struct BattleActionFrameApplyResult
{
    std::vector<int> attackSoundIds;
    int blinkSoundCount = 0;
    std::vector<Battle::BattleAttackSpawnRequest> attackSpawnRequests;
    std::vector<Battle::BattleLogEvent> logEvents;
    std::vector<Battle::BattleVisualEvent> visualEvents;
    std::vector<int> faceTowardsNearestUnitIds;
};
```

to:

```cpp
struct BattleActionFrameApplyResult
{
    std::vector<int> attackSoundIds;
    int blinkSoundCount = 0;
    std::vector<int> faceTowardsNearestUnitIds;
};
```

- [ ] **Step 3: Remove empty scene loops**

In `BattleSceneHades::applyCoreFrameResult`, remove the loops over `actionApply.visualEvents` and `actionApply.logEvents`:

```cpp
for (const auto& event : actionApply.visualEvents)
{
    presentation_recorder_.recordVisual(event);
}
for (const auto& event : actionApply.logEvents)
{
    presentation_recorder_.recordLog(event);
}
```

Keep the later loops over `frameResult.frame.visualEvents` and `frameResult.frame.logEvents`.

- [ ] **Step 4: Delete unused helper declarations from the adapter header**

Remove these declarations from `src/BattleSceneBattleAdapter.h`:

```cpp
Magic* selectLowerPowerMagic(Role* role);
Magic* selectHigherPowerMagic(Role* role);
void applyBattleCastStart(Role* unit, const Battle::BattleCastResult& result, int actType);
void applyBattleCastCommit(Role* unit, const Battle::BattleCastResult& result);
Battle::BattlePresentationColor makeBattlePresentationColor(Color color);
Battle::BattleRuntimeUnit makeBattleRuntimeUnit(
    Role* role,
    const RoleComboState* state,
    const Battle::BattleGridTransform& gridTransform);
Battle::BattleActionCommitUnitSnapshot makeBattleActionCommitUnitSnapshot(Role* role);
Battle::BattleActionTargetSnapshot makeBattleActionTargetSnapshot(Role* role);
Battle::BattleStatusUnitState makeBattleStatusUnit(Role* role, const RoleComboState& state);
Battle::BattleDamageUnitState makeBattleDamageUnit(Role* role, const RoleComboState* state);
Battle::BattleDamagePresentationStyle makeBattleDamagePresentationStyle(Role* role);
```

Do not remove declarations used directly by `BattleSceneHades.cpp`, such as `writeBattleStatusUnit`, `writeBattleDamageUnit`, `applyBattleMovementFrameResults`, and `applyBattleActionFrameResults`.

- [ ] **Step 5: Delete dead helper definitions from the adapter cpp**

Remove these complete definitions from `src/BattleSceneBattleAdapter.cpp`:

```cpp
Battle::BattleAttackUnit makeBattleAttackUnit(Role* role);
int actionRandomInt(const BattleActionPlanInputContext& context, int upperBound);
Pointf positionForCell(int x, int y, double tileWidth, int coordCount);
bool cellWalkable(int x, int y, int coordCount);
Battle::BattleBlinkGeometryInput makeBlinkGeometryInput(
    Role* role,
    double reach,
    const BattleActionPlanInputContext& context);
```

Also remove the forward declaration of `makeBlinkGeometryInput`.

- [ ] **Step 6: Verify cleanup**

Run:

```powershell
rg -n "\bmakeBattleAttackUnit\b|\bmakeBattleActionCommitUnitSnapshot\b|\bmakeBattleActionTargetSnapshot\b|\bactionRandomInt\b|\bmakeBlinkGeometryInput\b|\bpositionForCell\b|\bcellWalkable\b|attackSpawnRequests|actionApply\.visualEvents|actionApply\.logEvents" src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h src\BattleSceneHades.cpp
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: search has no matches except unrelated `frameResult` event handling; build and tests pass.

---

## Task 2: Move Dash-Hit RNG Out Of Adapter Setup

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a failing runtime test for dash-hit rolling**

Add this test to `tests/BattleCoreUnitTests.cpp` near the other `BattleFrameRunner_*` action tests:

```cpp
TEST_CASE("BattleFrameRunner_RollsDashHitCountFromRuntimeStateWhenDashCastStarts", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Melee),
        unit(2, 1, { 160, 100, 0 }),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto& runtimeUnit = state.units.requireUnit(1);
    runtimeUnit.cooldown = 0;
    runtimeUnit.speed = 360;

    auto cast = frameCastInput(1, 2);
    cast.unit.dashAttackEnabled = true;
    cast.unit.dashAttackReach = 350.0;
    cast.unit.dashHitCount = 1;
    cast.unit.dashVelocity = { 10.0f, 0.0f, 0.0f };
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.rangedStyle = false;
    cast.normalSkill.reach = 350.0;
    cast.normalSkill.actProperty = 0;
    state.action.castPlanInputs.emplace(1, cast);

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    const auto& action = result.actionResults[0];
    REQUIRE(action.castStarted);
    REQUIRE(action.castResult.decision.operationType == BattleOperationType::Dash);
    CHECK(action.castResult.attackSpawnRequests.size() == 3);
    CHECK(action.actionInput.cast.attackSpawnRequests.size() == 3);
}
```

Expected before implementation: the test fails because runtime uses the setup-provided `dashHitCount == 1`.

- [ ] **Step 2: Run the focused test and verify failure**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleFrameRunner_RollsDashHitCountFromRuntimeStateWhenDashCastStarts"
```

Expected: test fails on attack spawn count before runtime owns the roll.

- [ ] **Step 3: Add runtime dash-hit roll helpers**

In `src/battle/BattleCore.cpp`, near `refreshRuntimeCastSkillBonuses`, add:

```cpp
double nextRuntimeUnitRoll(BattleRuntimeState& state)
{
    return state.random.nextPercent() / 100.0;
}

int rollRuntimeDashHitCount(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleCastSkillState& selectedSkill)
{
    int dashHitCount = 1;
    if (selectedSkill.id < 0)
    {
        return dashHitCount;
    }

    const double multiHitScore = (unit.speed + selectedSkill.actProperty) / 180.0;
    if (nextRuntimeUnitRoll(state) < multiHitScore)
    {
        ++dashHitCount;
    }
    if (nextRuntimeUnitRoll(state) < multiHitScore * 0.5)
    {
        ++dashHitCount;
    }
    return dashHitCount;
}

void refreshRuntimeDashHitCount(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    BattleCastInput& input,
    bool ultimate)
{
    const auto& selectedSkill = ultimate ? input.ultimateSkill : input.normalSkill;
    input.unit.dashHitCount = rollRuntimeDashHitCount(state, unit, selectedSkill);
}
```

- [ ] **Step 4: Use runtime dash-hit rolling before runtime cast planning**

In `advanceActionFrameUnits`, update the selected-cast branch from:

```cpp
auto cast = BattleCastPlanner().commitSelectedCast(
    refreshedCastInput(state, movement, input->selectedCastInput),
    input->selectedCastUltimate,
    input->selectedOperationType);
```

to:

```cpp
auto selectedCastInput = refreshedCastInput(state, movement, input->selectedCastInput);
refreshRuntimeDashHitCount(state, unit, selectedCastInput, input->selectedCastUltimate);
auto cast = BattleCastPlanner().commitSelectedCast(
    selectedCastInput,
    input->selectedCastUltimate,
    input->selectedOperationType);
```

In the runtime plan branch, update:

```cpp
auto cast = BattleCastPlanner().plan(castInput);
```

to:

```cpp
const bool plannedUltimate = castInput.ultimateSkill.id >= 0
    && castInput.unit.maxMp > 0
    && castInput.unit.mp >= castInput.unit.maxMp;
refreshRuntimeDashHitCount(state, unit, castInput, plannedUltimate);
auto cast = BattleCastPlanner().plan(castInput);
```

In `tryCommitAutoUltimate`, update:

```cpp
auto cast = BattleCastPlanner().commitSelectedCast(castInput, true, operationType);
```

to:

```cpp
refreshRuntimeDashHitCount(state, *unit, castInput, true);
auto cast = BattleCastPlanner().commitSelectedCast(castInput, true, operationType);
```

- [ ] **Step 5: Remove adapter random ownership**

In `src/BattleSceneBattleAdapter.h`, remove this field from `BattleActionPlanInputContext`:

```cpp
RandomDouble* random = nullptr;
```

In `src/BattleSceneBattleAdapter.cpp`, delete:

```cpp
double actionRandomRoll(const BattleActionPlanInputContext& context)
{
    assert(context.random);
    return context.random->rand();
}

int rollDashHitCount(Role* role, Magic* selectedMagic, const BattleActionPlanInputContext& context)
{
    int dashHitCount = 1;
    if (selectedMagic)
    {
        const double multiHitScore = (role->Speed + role->getActProperty(selectedMagic->MagicType)) / 180.0;
        if (actionRandomRoll(context) < multiHitScore)
        {
            dashHitCount++;
        }
        if (actionRandomRoll(context) < multiHitScore * 0.5)
        {
            dashHitCount++;
        }
    }
    return dashHitCount;
}
```

In `makeActionFrameCastInput`, replace:

```cpp
castInput.unit.dashHitCount = rollDashHitCount(role, selectedMagic, context);
```

with:

```cpp
castInput.unit.dashHitCount = 1;
```

- [ ] **Step 6: Verify dash RNG fix**

Run:

```powershell
rg -n "RandomDouble\* random|actionRandomRoll|rollDashHitCount|context\.random" src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleFrameRunner_RollsDashHitCountFromRuntimeStateWhenDashCastStarts"
x64\Debug\kys_tests.exe
```

Expected: search has no matches; focused test and full tests pass.

---

## Task 3: Narrow The Adapter Public API To Scene-Used Operations

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp` only if a removed declaration was accidentally used externally.

- [ ] **Step 1: Confirm remaining header declarations are externally used**

Run:

```powershell
rg -n "findRoleByBattleId|createInitializedBattleRuntimeSession|configureInitializedBattleRuntimeState|applyBattleInitializationResult|makeBattleSetupPlacementInput|applyBattleFrameUnitRuntimeResult|applyBattleProjectileCancelDamage|writeBattleStatusUnit|writeBattleDamageUnit|applyBattleMovementPhysicsFrameResults|applyBattleMovementFrameResults|initializeBattleActionPlanInputs|applyBattleActionFrameResults|applyBattleLifecycleEvents" src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

Expected: only functions called by `BattleSceneHades.cpp` or needed as adapter entry points remain in the header.

- [ ] **Step 2: Move internal forward declarations into the cpp only**

Keep these declarations in `src/BattleSceneBattleAdapter.cpp` above the anonymous namespace if needed by earlier helper definitions:

```cpp
bool roleForcesRangedMagic(const std::map<int, RoleComboState>& comboStates, int unitId);
int forcedRangedMinSelectDistance(const std::map<int, RoleComboState>& comboStates, int unitId);
int projectileSpeedMultiplierPct(const std::map<int, RoleComboState>& comboStates, int unitId);
bool isBattleRangedStyleMagic(const Magic* magic, bool forceRanged);
double effectiveBattleReach(
    const Magic* magic,
    bool forceRanged,
    int forcedRangedMinSelectDistance,
    int projectileSpeedMultiplierPct,
    const Battle::BattleActionRulesConfig& actionRules,
    const Battle::BattleCastGeometry& castGeometry);
void configureBattleAttackWorld(
    Battle::BattleAttackWorld& world,
    const Battle::BattleRuntimeRulesConfig& rules);
```

Do not add these to `BattleSceneBattleAdapter.h`.

- [ ] **Step 3: Verify public API shrink**

Run:

```powershell
rg -n "selectLowerPowerMagic|selectHigherPowerMagic|applyBattleCastStart|applyBattleCastCommit|makeBattlePresentationColor|makeBattleRuntimeUnit|makeBattleStatusUnit|makeBattleDamageUnit|makeBattleDamagePresentationStyle" src\BattleSceneBattleAdapter.h
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: header search has no matches; build and tests pass.

---

## Task 4: Replace Setup-Time `BattleCastInput` Plans With Runtime-Owned Action Plan Seeds

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add runtime seed types**

In `src/battle/BattleCore.h`, add these near `BattleFrameActionUnitInput`:

```cpp
struct BattleActionSkillSeed
{
    int id = -1;
    std::string name;
    int soundId = -1;
    int hurtType = 0;
    int attackAreaType = -1;
    int magicType = -1;
    int visualEffectId = -1;
    int selectDistance = 1;
    int actProperty = 0;
    int magicPower = 0;
};

struct BattleActionPlanSeed
{
    int unitId = -1;
    bool hasEquippedSkill = false;
    BattleActionSkillSeed normalSkill;
    BattleActionSkillSeed ultimateSkill;
};
```

In `BattleRuntimeState::ActionState`, add:

```cpp
std::map<int, BattleActionPlanSeed> planSeeds;
BattleCastConfig castConfig;
BattleCastGeometry castGeometry;
BattleActionRulesConfig actionRules;
```

If `BattleActionRulesConfig` still lives in `BattleRuntimeRules.h`, move that struct to `BattleCore.h` so `BattleCore.h` does not need to include `BattleRuntimeRules.h`.

- [ ] **Step 2: Add a failing runtime test for core-built cast input**

Add this test near `BattleFrameRunner_PlansCastFromRuntimeOwnedCastPlanInput`:

```cpp
TEST_CASE("BattleFrameRunner_BuildsCastPlanFromRuntimeActionSeed", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireUnit(1).cooldown = 0;
    state.units.requireUnit(1).mp = 0;
    state.units.requireUnit(1).maxMp = 100;

    state.action.castConfig = testCastConfig();
    state.action.castGeometry.projectileSpeed = 20.0;
    state.action.castGeometry.projectileSpawnOffset = 30.0;
    state.action.castGeometry.projectileBaseTravel = 100.0;
    state.action.castGeometry.projectileTravelPerSelectDistance = 40.0;
    state.action.actionRules.tileWidth = 50.0;
    state.action.actionRules.maxEffectiveBattleReach = 480.0;
    state.action.actionRules.meleeAttackHitRadius = 100.0;
    state.action.actionRules.meleeAttackReach = 137.5;
    state.action.actionRules.dashAttackMeleeReach = 375.0;
    state.action.actionRules.dashMomentumFrames = 5;

    BattleActionPlanSeed seed;
    seed.unitId = 1;
    seed.normalSkill.id = 301;
    seed.normalSkill.name = "runtime seed";
    seed.normalSkill.soundId = 7;
    seed.normalSkill.attackAreaType = 1;
    seed.normalSkill.magicType = 2;
    seed.normalSkill.selectDistance = 4;
    seed.normalSkill.actProperty = 20;
    seed.normalSkill.magicPower = 80;
    state.action.planSeeds.emplace(1, seed);

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    const auto& action = result.actionResults[0];
    CHECK(action.castStarted);
    CHECK(action.castResult.decision.skillId == 301);
    CHECK(action.castResult.decision.operationType == BattleOperationType::RangedProjectile);
}
```

If existing test helpers use different names than `testCastConfig()`, use the local helper already used by `frameCastInput`.

- [ ] **Step 3: Implement core skill conversion helpers**

In `src/battle/BattleCore.cpp`, add:

```cpp
bool runtimeRoleForcesRangedMagic(const BattleRuntimeState& state, int unitId)
{
    auto it = state.combo.units.find(unitId);
    return it != state.combo.units.end() && it->second.forceRangedAttack;
}

int runtimeForcedRangedMinSelectDistance(const BattleRuntimeState& state, int unitId)
{
    auto it = state.combo.units.find(unitId);
    if (it == state.combo.units.end() || it->second.forceRangedMinSelectDistance <= 0)
    {
        return 6;
    }
    return std::max(1, it->second.forceRangedMinSelectDistance);
}

int runtimeProjectileSpeedMultiplierPct(const BattleRuntimeState& state, int unitId)
{
    auto it = state.combo.units.find(unitId);
    if (it == state.combo.units.end())
    {
        return 100;
    }
    return std::max(100, it->second.projectileSpeedMultiplierPct);
}

BattleCastSkillState makeRuntimeCastSkillState(
    const BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed,
    bool ultimate,
    bool consumeFrameSkillBonuses)
{
    BattleCastSkillState skill;
    if (seed.id < 0)
    {
        return skill;
    }

    const bool forceRanged = runtimeRoleForcesRangedMagic(state, unit.id);
    const int speedMultiplierPct = runtimeProjectileSpeedMultiplierPct(state, unit.id);
    skill.id = seed.id;
    skill.name = seed.name;
    skill.soundId = seed.soundId;
    skill.hurtType = seed.hurtType;
    skill.attackAreaType = seed.attackAreaType;
    skill.magicType = seed.magicType;
    skill.visualEffectId = seed.visualEffectId;
    skill.selectDistance = seed.selectDistance;
    skill.projectileSpeedMultiplierPct = speedMultiplierPct;
    skill.actProperty = seed.actProperty;
    skill.magicPower = seed.magicPower;
    skill.meleeSplashCount = ultimate && seed.attackAreaType == 0 ? 1 : 0;
    skill.extraProjectileCount = ultimate && consumeFrameSkillBonuses
        ? collectFrameExtraProjectileCount(
            state.combo.units.at(unit.id),
            unit.id,
            std::max(0, state.combo.units.at(unit.id).ultimateExtraProjectiles))
        : 0;
    skill.reach = runtimeEffectiveBattleReach(
        seed,
        forceRanged,
        runtimeForcedRangedMinSelectDistance(state, unit.id),
        speedMultiplierPct,
        state.action.actionRules,
        state.action.castGeometry);
    skill.forceRanged = forceRanged;
    skill.rangedStyle = runtimeBattleRangedStyle(seed.attackAreaType, forceRanged);
    skill.blinkReach = runtimeBattleBlinkReach(seed, state.action.actionRules, state.action.castGeometry);
    return skill;
}
```

Implement `runtimeEffectiveBattleReach`, `runtimeBattleRangedStyle`, and `runtimeBattleBlinkReach` by moving the existing formulas from `BattleSceneBattleAdapter.cpp` into `BattleCore.cpp`, replacing `Magic*` reads with `BattleActionSkillSeed` fields.

- [ ] **Step 4: Build `BattleCastInput` in core from action seeds**

In `src/battle/BattleCore.cpp`, add:

```cpp
BattleCastInput makeRuntimeCastInputFromSeed(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionPlanSeed& seed,
    bool canStartAttack,
    bool movementDashActive,
    bool consumeFrameSkillBonuses)
{
    BattleCastInput input;
    input.config = state.action.castConfig;
    input.geometry = state.action.castGeometry;
    input.unit.id = unit.id;
    input.unit.position = unit.position;
    input.unit.facing = unit.facing;
    input.unit.alive = unit.alive;
    input.unit.canStartAttack = canStartAttack;
    input.unit.mp = unit.mp;
    input.unit.maxMp = unit.maxMp;
    input.unit.speed = unit.speed;
    input.unit.operationCount = unit.operationCount;
    input.unit.meleeAttackReach = state.action.actionRules.meleeAttackReach;
    input.unit.dashAttackReach = state.action.actionRules.dashAttackMeleeReach;
    input.unit.hasEquippedSkill = seed.hasEquippedSkill;
    input.unit.movementDashActive = movementDashActive;

    if (const auto* status = findStatusUnit(state.status.units, unit.id))
    {
        input.unit.frozen = status->frozenTimer > 0;
    }
    input.unit.stunned = unit.hurtFrame > 0;

    auto comboIt = state.combo.units.find(unit.id);
    if (comboIt != state.combo.units.end())
    {
        input.unit.cooldownReductionPct = comboIt->second.cdrPct;
        input.unit.dashAttackEnabled = comboIt->second.dashAttack;
    }

    input.unit.dashVelocity = unit.facing;
    if (input.unit.dashVelocity.norm() > 0.01)
    {
        input.unit.dashVelocity.normTo(
            state.action.actionRules.meleeAttackHitRadius / state.action.actionRules.dashMomentumFrames);
    }

    const bool ultimateReady = unit.maxMp > 0 && unit.mp >= unit.maxMp;
    const auto& selectedSeed = ultimateReady && seed.ultimateSkill.id >= 0
        ? seed.ultimateSkill
        : seed.normalSkill;
    input.unit.emitDashFollowUpSkillAttack = input.unit.dashAttackEnabled && selectedSeed.id >= 0;
    input.unit.dashFollowUpOperationType = selectedSeed.id >= 0
        ? BattleCombatIntentPlanner().operationTypeForAttackArea(selectedSeed.attackAreaType)
        : BattleOperationType::None;

    input.normalSkill = makeRuntimeCastSkillState(state, unit, seed.normalSkill, false, consumeFrameSkillBonuses);
    input.ultimateSkill = makeRuntimeCastSkillState(state, unit, seed.ultimateSkill, true, consumeFrameSkillBonuses);
    return input;
}
```

- [ ] **Step 5: Replace runtime use of `castPlanInputs`**

In `advanceActionFrameUnits`, replace the runtime-owned cast plan lookup:

```cpp
auto runtimeCastPlanIt = state.action.castPlanInputs.find(unit.id);
```

with:

```cpp
auto runtimePlanSeedIt = state.action.planSeeds.find(unit.id);
```

When no directive cast input exists and the unit can runtime-plan, build the cast input with:

```cpp
auto runtimeCastInput = makeRuntimeCastInputFromSeed(
    state,
    unit,
    runtimePlanSeedIt->second,
    unit.cooldown == 0,
    actionMovementDashActive(state, unit.id),
    true);
castPlanInput = &runtimeCastInput;
usingRuntimeCastPlan = true;
```

Because `runtimeCastInput` must outlive the pointer use in the branch, keep it as a local `std::optional<BattleCastInput>` declared before assigning `castPlanInput`.

- [ ] **Step 6: Change adapter setup to seed skill facts only**

In `src/BattleSceneBattleAdapter.cpp`, replace `initializeBattleActionPlanInputs` body with:

```cpp
runtime.action.planSeeds.clear();
runtime.action.castConfig = context.castConfig;
runtime.action.castGeometry = context.castGeometry;
runtime.action.actionRules = context.actionRules;
for (auto role : *context.roles)
{
    assert(role);
    Magic* equippedMagic = role->UsingMagic;
    Magic* normalMagic = equippedMagic ? equippedMagic : selectLowerPowerMagic(role);
    Magic* ultimateMagic = equippedMagic ? equippedMagic : selectHigherPowerMagic(role);
    runtime.action.planSeeds[role->ID] = makeBattleActionPlanSeed(role, normalMagic, ultimateMagic, equippedMagic != nullptr);
}
```

Add adapter helper:

```cpp
Battle::BattleActionSkillSeed makeBattleActionSkillSeed(Role* role, Magic* magic)
{
    Battle::BattleActionSkillSeed seed;
    if (!magic)
    {
        return seed;
    }
    seed.id = magic->ID;
    seed.name = magic->Name;
    seed.soundId = magic->SoundID;
    seed.hurtType = magic->HurtType;
    seed.attackAreaType = magic->AttackAreaType;
    seed.magicType = magic->MagicType;
    seed.visualEffectId = magic->EffectID;
    seed.selectDistance = magic->SelectDistance;
    seed.actProperty = role->getActProperty(magic->MagicType);
    seed.magicPower = role->getMagicPower(magic);
    return seed;
}

Battle::BattleActionPlanSeed makeBattleActionPlanSeed(
    Role* role,
    Magic* normalMagic,
    Magic* ultimateMagic,
    bool hasEquippedSkill)
{
    assert(role);
    Battle::BattleActionPlanSeed seed;
    seed.unitId = role->ID;
    seed.hasEquippedSkill = hasEquippedSkill;
    seed.normalSkill = makeBattleActionSkillSeed(role, normalMagic);
    seed.ultimateSkill = makeBattleActionSkillSeed(role, ultimateMagic);
    return seed;
}
```

- [ ] **Step 7: Remove adapter-owned action planning formulas**

After runtime seed planning compiles, remove these from `src/BattleSceneBattleAdapter.cpp` if they are no longer used:

```cpp
roleForcesRangedMagic
forcedRangedMinSelectDistance
projectileSpeedMultiplierPct
isForcedRangedMagic
isProjectileStyleMagic
isBattleRangedStyleMagic
effectiveProjectileSelectDistance
battleBlinkReach
effectiveBattleReach
actionUltimateExtraProjectileCount
makeActionFrameSkillState
makeActionFrameCastInput
legacyOperationForAttackArea
```

Keep `selectLowerPowerMagic` and `selectHigherPowerMagic` private if adapter still chooses which legacy `Magic` to seed.

- [ ] **Step 8: Verify runtime-owned planning**

Run:

```powershell
rg -n "makeActionFrameCastInput|makeActionFrameSkillState|effectiveBattleReach|battleBlinkReach|roleForcesRangedMagic|projectileSpeedMultiplierPct|castPlanInputs\\[|castPlanInputs\\.emplace" src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h src\battle tests
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected:
- Adapter no longer owns cast reach/style/projection formulas.
- Existing direct test fixtures may still use directive `BattleCastInput`; runtime-owned setup uses `planSeeds`.
- Test and game targets build.

---

## Task 5: Final Boundary Verification

**Files:**
- No new implementation files.
- Verify all modified files.

- [ ] **Step 1: Run whitespace and boundary searches**

Run:

```powershell
git diff --check
rg -n "RandomDouble\* random|actionRandomRoll|rollDashHitCount|makeActionFrameCastInput|makeActionFrameSkillState|makeBlinkGeometryInput|makeBattleAttackUnit|makeBattleActionCommitUnitSnapshot|makeBattleActionTargetSnapshot|attackSpawnRequests|actionApply\.visualEvents|actionApply\.logEvents" src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h src\BattleSceneHades.cpp
rg -n "battleRuntime\(\)" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected:
- `git diff --check` exit code 0.
- Adapter search has no matches except intentionally retained core/test names outside adapter.
- `BattleSceneHades` still has no frame-time `battleRuntime()` access.

- [ ] **Step 2: Run full verification**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected:
- `kys_tests` target builds.
- Full `kys_tests.exe` passes.
- `kys` target builds. Existing warnings may remain; no errors.

---

## Review Notes

- Do not move `Role` or `Magic` into runtime core. The adapter may read them to build immutable seed facts.
- Do not keep backward-compatible adapter APIs. Remove old declarations and migrate callers directly.
- Use `assert` for invariants. Do not add defensive no-op checks for required data.
- Keep presentation-only legacy writes in the adapter for this batch; moving all scene apply code is a separate boundary task.
- Avoid splitting files just to split files. Shrink the adapter first by deleting dead code and moving ownership; file decomposition can follow once responsibilities are clearer.
