# Battle Lost Behavior Restoration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore legacy projectile spread targeting, projectile speed scaling, dash attack positioning, and follow-up projectile speed behavior lost during the runtime migration.

**Architecture:** Keep the ownership boundary runtime-owned. The scene must not provide live lookups during frame execution; instead, runtime frame input carries explicit enemy projectile target snapshots and runtime/combo state supplies per-attacker speed multipliers. Avoid generic adapter bridges and do not add defensive no-op checks.

**Tech Stack:** C++20, Catch2 unit tests, MSBuild via `.github\build-command.ps1`.

---

## Files

- Modify: `src/battle/BattleCastSystem.h`
  - Add a narrow projectile spread target snapshot to `BattleCastInput`.
  - No scene pointers or runtime store references.
- Modify: `src/battle/BattleCastSystem.cpp`
  - Replace clone-only extra projectile generation with target-aware spread generation.
  - Restore alternate-target assignment and initial-frame jitter for tracking spread.
  - Apply projectile speed multiplier to ranged side projectiles.
- Modify: `src/battle/BattleCore.cpp`
  - Populate cast spread targets from `state.units`.
  - Compute dash attack velocity using old ranged/melee positioning rules only when the selected action is actually a dash.
- Modify: `src/battle/BattleHitResolver.h`
  - Add explicit projectile speed fields to follow-up commands that need attacker speed scaling.
- Modify: `src/battle/BattleHitResolver.cpp`
  - Populate and consume follow-up projectile speeds.
- Test: `tests/BattleCastSystemUnitTests.cpp`
  - Cast-level tests for alternate targets, tracking ultimate jitter/targeting, side projectile speed scaling.
- Test: `tests/BattleCoreUnitTests.cpp`
  - Runtime-level tests for spread target population and dash velocity behavior.
- Test: `tests/BattleHitResolverUnitTests.cpp`
  - Follow-up speed tests for nearby tracking and spiral bleed projectiles.

---

### Task 1: Add Cast Spread Target Input

**Files:**
- Modify: `src/battle/BattleCastSystem.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCastSystemUnitTests.cpp`

- [ ] **Step 1: Add a failing cast test for alternate target selection**

Add this test near the existing projectile tests in `tests/BattleCastSystemUnitTests.cpp`:

```cpp
TEST_CASE("BattleCastSystem_ExtraProjectilesPreferAlternateSpreadTargets", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp;
    input.ultimateSkill = skill(230, 1, 400.0);
    input.ultimateSkill.selectDistance = 4;
    input.ultimateSkill.extraProjectileCount = 2;
    input.targetUnitId = 2;
    input.targetPosition = { 82.0f, 20.0f, 0.0f };
    input.targetDistance = 72.0;
    input.projectileSpreadTargets = {
        { 2, { 82.0f, 20.0f, 0.0f } },
        { 3, { 82.0f, 56.0f, 0.0f } },
        { 4, { 82.0f, -16.0f, 0.0f } },
    };

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.ultimate);
    REQUIRE(result.attackSpawnRequests.size() == 6); // main + 2 ultimate extras + 3 area sides
    CHECK(result.attackSpawnRequests[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[1].initial.preferredTargetUnitId == 3);
    CHECK(result.attackSpawnRequests[2].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(result.attackSpawnRequests[2].initial.preferredTargetUnitId == 4);
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleCastSystem_ExtraProjectilesPreferAlternateSpreadTargets"
```

Expected: compile fails because `projectileSpreadTargets` does not exist, or the test fails because extras keep `targetUnitId`.

- [ ] **Step 3: Add the narrow target type**

In `src/battle/BattleCastSystem.h`, add before `BattleCastInput`:

```cpp
struct BattleCastProjectileTarget
{
    int unitId = -1;
    Pointf position;
};
```

Then add to `BattleCastInput`:

```cpp
std::vector<BattleCastProjectileTarget> projectileSpreadTargets;
```

- [ ] **Step 4: Populate spread targets from runtime units**

In `src/battle/BattleCore.cpp`, in `makeRuntimeCastInput(...)`, after `input.targetDistance` and target selection are refreshed, populate enemy targets from `state.units.units`:

```cpp
input.projectileSpreadTargets.clear();
for (const auto& candidate : state.units.units)
{
    if (!candidate.alive || candidate.team == unit.team)
    {
        continue;
    }
    input.projectileSpreadTargets.push_back({
        candidate.id,
        candidate.motion.position,
    });
}
```

This is a real conditional because dead/allied units are not projectile targets.

- [ ] **Step 5: Run the focused test again**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleCastSystem_ExtraProjectilesPreferAlternateSpreadTargets"
```

Expected: still fails because extra projectile logic does not use the targets yet.

---

### Task 2: Restore Target-Aware Projectile Spread

**Files:**
- Modify: `src/battle/BattleCastSystem.cpp`
- Test: `tests/BattleCastSystemUnitTests.cpp`

- [ ] **Step 1: Add a failing tracking ultimate target test**

Add this test near `BattleCastSystem_TrackingUltimateEmitsLegacyTwoProjectileSpread`:

```cpp
TEST_CASE("BattleCastSystem_TrackingUltimateSpreadAssignsAlternateTarget", "[battle][cast]")
{
    auto input = basicInput();
    input.unit.mp = input.unit.maxMp;
    input.ultimateSkill = skill(231, 3, 400.0);
    input.ultimateSkill.selectDistance = 4;
    input.targetUnitId = 2;
    input.targetPosition = { 82.0f, 20.0f, 0.0f };
    input.targetDistance = 72.0;
    input.projectileSpreadTargets = {
        { 2, { 82.0f, 20.0f, 0.0f } },
        { 3, { 86.0f, 54.0f, 0.0f } },
    };

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.ultimate);
    REQUIRE(result.decision.operationType == BattleOperationType::TrackingProjectile);
    REQUIRE(result.attackSpawnRequests.size() == 2);
    CHECK(result.attackSpawnRequests[0].initial.preferredTargetUnitId == 2);
    CHECK(result.attackSpawnRequests[1].initial.preferredTargetUnitId == 3);
    CHECK(result.attackSpawnRequests[1].initial.requirePreferredTarget);
}
```

- [ ] **Step 2: Run the tracking target test and verify it fails**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleCastSystem_TrackingUltimateSpreadAssignsAlternateTarget"
```

Expected: FAIL because both tracking projectiles keep the primary target.

- [ ] **Step 3: Add local spread target helpers**

In `src/battle/BattleCastSystem.cpp`, replace the current fixed spread helpers with target-aware helpers in the anonymous namespace:

```cpp
double angleDelta(double lhs, double rhs)
{
    return std::abs(std::atan2(std::sin(lhs - rhs), std::cos(lhs - rhs)));
}

std::vector<BattleCastProjectileTarget> orderedAlternateProjectileTargets(
    const BattleCastInput& input,
    Pointf launchPosition,
    Pointf baseDirection,
    double maxTravel)
{
    const double baseAngle = std::atan2(baseDirection.y, baseDirection.x);
    std::vector<BattleCastProjectileTarget> targets;
    for (const auto& target : input.projectileSpreadTargets)
    {
        if (target.unitId == input.targetUnitId)
        {
            continue;
        }
        if (pointDistance(target.position, launchPosition) > maxTravel)
        {
            continue;
        }
        targets.push_back(target);
    }

    std::sort(targets.begin(), targets.end(), [&](const auto& left, const auto& right)
    {
        const auto leftDir = left.position - launchPosition;
        const auto rightDir = right.position - launchPosition;
        const double leftAngle = pointNorm(leftDir) > input.config.minimumFacingNorm
            ? std::atan2(leftDir.y, leftDir.x)
            : baseAngle;
        const double rightAngle = pointNorm(rightDir) > input.config.minimumFacingNorm
            ? std::atan2(rightDir.y, rightDir.x)
            : baseAngle;
        const double leftDelta = angleDelta(leftAngle, baseAngle);
        const double rightDelta = angleDelta(rightAngle, baseAngle);
        if (leftDelta != rightDelta)
        {
            return leftDelta < rightDelta;
        }
        return pointDistance(left.position, launchPosition) < pointDistance(right.position, launchPosition);
    });
    return targets;
}

void assignProjectileTargetOrSpread(
    BattleAttackSpawnRequest& request,
    const BattleCastInput& input,
    const std::vector<BattleCastProjectileTarget>& alternates,
    int projectileIndex,
    int projectileCount,
    double speed)
{
    const BattleCastProjectileTarget* assigned = nullptr;
    if (projectileIndex == 0 && input.targetUnitId >= 0)
    {
        request.initial.preferredTargetUnitId = input.targetUnitId;
        request.initial.requirePreferredTarget = false;
        request.initial.velocity = normalizedTo(
            input.targetPosition - request.initial.position,
            speed,
            input.config.minimumFacingNorm);
        return;
    }
    const int alternateIndex = projectileIndex - 1;
    if (alternateIndex >= 0 && alternateIndex < static_cast<int>(alternates.size()))
    {
        assigned = &alternates[alternateIndex];
    }

    if (assigned)
    {
        request.initial.preferredTargetUnitId = assigned->unitId;
        request.initial.requirePreferredTarget = true;
        request.initial.velocity = normalizedTo(
            assigned->position - request.initial.position,
            speed,
            input.config.minimumFacingNorm);
        return;
    }

    if (projectileCount > 1)
    {
        const auto facing = castFacing(input);
        const double offset = (projectileIndex - (projectileCount - 1) / 2.0) * LegacyRangedSideProjectileAngle;
        request.initial.velocity = normalizedTo(
            rotated(facing, offset),
            speed,
            input.config.minimumFacingNorm);
    }
}
```

- [ ] **Step 4: Replace `appendTrackingProjectileSpread()` implementation**

Update `appendTrackingProjectileSpread()` so it:

```cpp
const int projectileCount = result.decision.ultimate ? 2 : 1;
auto prototype = makeOperationRequest(...TrackingProjectile...);
const double speed = projectileSpeedForSkill(input.geometry, selectedSkill);
const double maxTravel = speed * std::max(1, prototype.initial.totalFrame - prototype.initialFrame)
    + input.geometry.projectileSpawnOffset;
const auto alternates = orderedAlternateProjectileTargets(
    input,
    prototype.initial.position,
    castFacing(input),
    maxTravel);
for (int i = 0; i < projectileCount; ++i)
{
    auto request = prototype;
    request.initial.mainProjectile = i == 0;
    request.initialFrame = result.decision.ultimate ? (i * 5) : 0;
    assignProjectileTargetOrSpread(request, input, alternates, i, projectileCount, speed);
    requests.push_back(request);
}
```

Use the existing request fields; do not add a new DTO.

- [ ] **Step 5: Replace `appendExtraProjectiles()` with target-aware spread**

Change the function signature to:

```cpp
void appendExtraProjectiles(std::vector<BattleAttackSpawnRequest>& requests,
                            const BattleCastInput& input,
                            const BattleCastSkillState& selectedSkill)
```

Implementation should:

```cpp
assert(selectedSkill.extraProjectileCount >= 0);
assert(!requests.empty());
const auto prototype = requests.front();
const double speed = pointNorm(prototype.initial.velocity) > input.config.minimumFacingNorm
    ? pointNorm(prototype.initial.velocity)
    : projectileSpeedForSkill(input.geometry, selectedSkill);
const double maxTravel = speed * std::max(1, prototype.initial.totalFrame - prototype.initialFrame)
    + input.geometry.projectileSpawnOffset;
const auto alternates = orderedAlternateProjectileTargets(
    input,
    prototype.initial.position,
    prototype.initial.velocity,
    maxTravel);
for (int i = 0; i < selectedSkill.extraProjectileCount; ++i)
{
    auto extra = prototype;
    extra.initial.castSubrequestKind = BattleAttackCastSubrequestKind::ExtraProjectile;
    extra.initial.mainProjectile = false;
    assignProjectileTargetOrSpread(
        extra,
        input,
        alternates,
        i + 1,
        selectedSkill.extraProjectileCount + 1,
        speed);
    requests.push_back(extra);
}
```

Update all call sites to pass `input`.

- [ ] **Step 6: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleCastSystem_ExtraProjectilesPreferAlternateSpreadTargets"
x64\Debug\kys_tests.exe "BattleCastSystem_TrackingUltimateSpreadAssignsAlternateTarget"
x64\Debug\kys_tests.exe "BattleCastSystem_TrackingUltimateEmitsLegacyTwoProjectileSpread"
```

Expected: all pass.

---

### Task 3: Restore Ranged Side Projectile Speed Scaling

**Files:**
- Modify: `src/battle/BattleCastSystem.cpp`
- Test: `tests/BattleCastSystemUnitTests.cpp`

- [ ] **Step 1: Add failing test**

Add this test near `BattleCastSystem_RangedAreaCastEmitsLegacySideProjectiles`:

```cpp
TEST_CASE("BattleCastSystem_RangedSideProjectilesUseProjectileSpeedMultiplier", "[battle][cast]")
{
    auto input = basicInput();
    input.normalSkill = skill(121, 1, 400.0);
    input.normalSkill.selectDistance = 4;
    input.normalSkill.projectileSpeedMultiplierPct = 200;
    input.targetDistance = 300.0;

    auto result = BattleCastPlanner().plan(input);

    REQUIRE(result.decision.operationType == BattleOperationType::RangedProjectile);
    REQUIRE(result.attackSpawnRequests.size() == 3);
    CHECK(result.attackSpawnRequests[1].initial.velocity.norm() == Catch::Approx(10.0f));
    CHECK(result.attackSpawnRequests[2].initial.velocity.norm() == Catch::Approx(9.0f));
}
```

- [ ] **Step 2: Run test and verify it fails**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleCastSystem_RangedSideProjectilesUseProjectileSpeedMultiplier"
```

Expected: FAIL because side speeds are `5.0` and `4.5`, not doubled.

- [ ] **Step 3: Apply multiplier in side projectile generation**

In `appendRangedSideProjectiles()`, change:

```cpp
const double speed = 5.0 - 0.5 / sideCount * 2.0 * i;
```

to:

```cpp
const double speed = (5.0 - 0.5 / sideCount * 2.0 * i)
    * selectedSkill.projectileSpeedMultiplierPct / 100.0;
```

- [ ] **Step 4: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleCastSystem_RangedSideProjectilesUseProjectileSpeedMultiplier"
x64\Debug\kys_tests.exe "BattleCastSystem_RangedAreaCastEmitsLegacySideProjectiles"
```

Expected: both pass.

---

### Task 4: Restore Dash Attack Positioning Logic

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add failing ranged close dash test**

Add this test near the existing dash action frame tests in `tests/BattleCoreUnitTests.cpp`:

```cpp
TEST_CASE("BattleFrameRunner_DashAttackStrafesWhenRangedTargetAlreadyInRange", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 180, 100, 0 }),
    });
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireUnit(0).motion.facing = { 1, 0, 0 };
    state.units.requireUnit(0).stats.speed = 180;
    state.units.requireUnit(0).animation.cooldown = 0;
    state.combo.units[0].dashAttack = true;

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    cast.targetPosition = { 180, 100, 0 };
    cast.targetDistance = 80.0;
    configureRuntimeActionPlan(state, cast);

    auto result = runBattleFrame(state);

    REQUIRE(result.actionResults.size() == 1);
    REQUIRE(result.actionResults[0].castResult.decision.operationType == BattleOperationType::Dash);
    const auto& dash = result.actionResults[0].castResult.attackSpawnRequests[0];
    CHECK(std::abs(dash.initial.velocity.y) > 0.01f);
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleFrameRunner_DashAttackStrafesWhenRangedTargetAlreadyInRange"
```

Expected: FAIL because dash velocity remains `{x > 0, y == 0}`.

- [ ] **Step 3: Add a local dash velocity helper**

In `src/battle/BattleCore.cpp`, near `rollRuntimeDashHitCount()`, add:

```cpp
Pointf runtimeDashAttackVelocity(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill)
{
    auto direction = input.targetPosition - unit.motion.position;
    if (pointNorm(direction) <= state.action.castConfig.minimumFacingNorm)
    {
        direction = unit.motion.facing;
    }
    assert(pointNorm(direction) > state.action.castConfig.minimumFacingNorm);

    double dashDistance = state.action.actionRules.meleeAttackHitRadius
        / state.action.actionRules.dashMomentumFrames;

    const bool rangedStyle = selectedSkill.rangedStyle;
    if (rangedStyle)
    {
        const double attackRange = selectedSkill.attackAreaType == 3
            ? 180.0
            : std::min(selectedSkill.reach, state.action.actionRules.maxEffectiveBattleReach);
        const double forwardGap = std::max(0.0, input.targetDistance - attackRange);
        dashDistance = state.action.actionRules.meleeAttackHitRadius
            / state.action.actionRules.dashMomentumFrames;
        if (forwardGap > state.movementConfig.engagementDeadband)
        {
            dashDistance = std::min(dashDistance, forwardGap / state.action.actionRules.dashMomentumFrames);
        }
        else
        {
            auto away = unit.motion.position - input.targetPosition;
            if (pointNorm(away) > state.action.castConfig.minimumFacingNorm)
            {
                away = normalizedTo(away, 1.0, state.action.castConfig.minimumFacingNorm);
                Pointf side{ -away.y, away.x, 0 };
                if (state.random.nextPercent() < 50.0)
                {
                    side = scaled(side, -1.0);
                }
                side = normalizedTo(side, 1.0, state.action.castConfig.minimumFacingNorm);
                direction = side + scaled(
                    away,
                    std::clamp((attackRange - input.targetDistance) / std::max(attackRange, 1.0), 0.0, 1.0));
            }
        }
    }
    else if (selectedSkill.attackAreaType == 0)
    {
        const double usefulAdvance = input.targetDistance
            - state.action.actionRules.meleeAttackReach
            + state.movementConfig.engagementDeadband;
        dashDistance = std::clamp(
            usefulAdvance,
            0.0,
            state.movementConfig.maxDashDistance)
            / state.action.actionRules.dashMomentumFrames;
    }

    return normalizedTo(direction, dashDistance, state.action.castConfig.minimumFacingNorm);
}
```

- [ ] **Step 4: Compute dash details only after cast decision**

Replace `refreshRuntimeDashHitCount(...)` with `refreshRuntimeDashAttackDetails(...)`:

```cpp
void refreshRuntimeDashAttackDetails(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    BattleCastInput& input,
    bool ultimate,
    bool dashOperation)
{
    if (!dashOperation)
    {
        return;
    }
    const auto& skill = selectedCastSkill(input, ultimate);
    input.unit.dashHitCount = rollRuntimeDashHitCount(state, unit, skill);
    input.unit.dashVelocity = runtimeDashAttackVelocity(state, unit, input, skill);
}
```

Update the one call site that currently refreshes dash hit count.

- [ ] **Step 5: Run dash-focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleFrameRunner_DashAttackStrafesWhenRangedTargetAlreadyInRange"
x64\Debug\kys_tests.exe "BattleFrameRunner_AdvanceFrame_RollsRuntimeDashHitCountAtCommit"
x64\Debug\kys_tests.exe "BattleCastSystem_DashCastUsesDashRecoveryAndHitEffectRequest"
```

Expected: all pass.

---

### Task 5: Restore Follow-Up Projectile Speed Scaling

**Files:**
- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Test: `tests/BattleHitResolverUnitTests.cpp`

- [ ] **Step 1: Add failing nearby tracking speed test**

Add this test near `BattleProjectileFollowUpResolver_ExpandsNearbyTrackingIntoSpawnCommands`:

```cpp
TEST_CASE("BattleProjectileFollowUpResolver_NearbyTrackingUsesCommandProjectileSpeed", "[battle][hit_resolver][unit]")
{
    BattleProjectileFollowUpContext context;
    BattleUnitStore units;
    units.units = {
        runtimeUnit(0, 0, { 0.0f, 0.0f, 0.0f }),
        runtimeUnit(1, 1, { 80.0f, 0.0f, 0.0f }),
    };
    context.projectileSpeed = 10.0;

    BattleAttackEvent prototype;
    prototype.sourceUnitId = 0;
    prototype.unitId = 1;
    prototype.skillId = 101;
    prototype.visualEffectId = 44;
    prototype.position = { 0, 0, 0 };
    prototype.velocity = { 20, 0, 0 };

    std::vector<BattleGameplayCommand> commands;
    commands.push_back(BattleNearbyTrackingProjectilesCommand{
        prototype,
        1,
        100,
        40,
        20.0,
    });

    auto expanded = expandBattleProjectileFollowUpCommands(commands, context, units);

    REQUIRE(expanded.commands.size() == 1);
    const auto* spawn = std::get_if<BattleProjectileSpawnCommand>(&expanded.commands[0]);
    REQUIRE(spawn);
    CHECK(spawn->request.initial.velocity.norm() == Catch::Approx(20.0f));
}
```

- [ ] **Step 2: Add failing spiral speed test**

Add:

```cpp
TEST_CASE("BattleProjectileFollowUpResolver_SpiralBleedUsesCommandProjectileSpeed", "[battle][hit_resolver][unit]")
{
    BattleProjectileFollowUpContext context;
    BattleUnitStore units;
    units.units = {
        runtimeUnit(0, 0, { 0.0f, 0.0f, 0.0f }),
    };
    context.projectileSpeed = 10.0;

    std::vector<BattleGameplayCommand> commands;
    commands.push_back(BattleSpiralBleedProjectileCommand{
        0,
        3,
        2,
        20.0,
    });

    auto expanded = expandBattleProjectileFollowUpCommands(commands, context, units);

    REQUIRE(expanded.commands.size() == 2);
    const auto* spawn = std::get_if<BattleProjectileSpawnCommand>(&expanded.commands[0]);
    REQUIRE(spawn);
    CHECK(spawn->request.spiralRadiusGrowth == Catch::Approx(18.0f));
}
```

- [ ] **Step 3: Run tests and verify they fail to compile**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleProjectileFollowUpResolver_NearbyTrackingUsesCommandProjectileSpeed"
x64\Debug\kys_tests.exe "BattleProjectileFollowUpResolver_SpiralBleedUsesCommandProjectileSpeed"
```

Expected: compile fails because command constructors do not have the speed fields yet.

- [ ] **Step 4: Add speed fields to command structs**

In `src/battle/BattleHitResolver.h`, update:

```cpp
struct BattleSpiralBleedProjectileCommand
{
    int sourceUnitId{};
    int bleedStacks{};
    int projectileCount{};
    double projectileSpeed{};
};

struct BattleNearbyTrackingProjectilesCommand
{
    BattleAttackEvent prototype;
    int centerTargetUnitId{};
    int rangePixels{};
    int damagePct{};
    double projectileSpeed{};
};
```

- [ ] **Step 5: Populate command speeds in resolver**

In `BattleHitResolver::resolve(...)`, where nearby tracking and spiral bleed commands are created, set:

```cpp
const double attackerProjectileSpeed = pointNorm(input.attackEvent.velocity) > 0.01
    ? pointNorm(input.attackEvent.velocity)
    : 0.0;
```

Use that value in `BattleNearbyTrackingProjectilesCommand`. For `BattleSpiralBleedProjectileCommand`, use the same value when available; if the triggering attack had no velocity, leave `0.0` and let expansion fall back to context speed.

- [ ] **Step 6: Consume command speeds in expansion**

In `makeNearbyFollowUpSpawn(...)`, replace `context.projectileSpeed` with:

```cpp
const double projectileSpeed = command.projectileSpeed > 0.0
    ? command.projectileSpeed
    : context.projectileSpeed;
```

Use `projectileSpeed` for velocity and total frame calculations.

In spiral bleed expansion, replace:

```cpp
request.spiralRadiusGrowth = static_cast<float>(context.projectileSpeed * 0.9);
```

with:

```cpp
const double projectileSpeed = spiral->projectileSpeed > 0.0
    ? spiral->projectileSpeed
    : context.projectileSpeed;
request.spiralRadiusGrowth = static_cast<float>(projectileSpeed * 0.9);
```

- [ ] **Step 7: Run follow-up tests**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleProjectileFollowUpResolver_NearbyTrackingUsesCommandProjectileSpeed"
x64\Debug\kys_tests.exe "BattleProjectileFollowUpResolver_SpiralBleedUsesCommandProjectileSpeed"
x64\Debug\kys_tests.exe "BattleProjectileFollowUpResolver_ExpandsNearbyTrackingIntoSpawnCommands"
```

Expected: all pass.

---

### Task 6: Full Verification

**Files:**
- Verify all modified files.

- [ ] **Step 1: Run formatting whitespace check**

Run:

```powershell
git diff --check
```

Expected: no output.

- [ ] **Step 2: Build tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: MSBuild exits with code `0`.

- [ ] **Step 3: Run full test suite**

Run:

```powershell
x64\Debug\kys_tests.exe
```

Expected: all tests pass.

- [ ] **Step 4: Build game**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: MSBuild exits with code `0`. A final link failure caused by a running `kys.exe` is acceptable per repo instructions.

- [ ] **Step 5: Manual gameplay smoke test**

Start a Hades battle with:

- A ranged area skill user with projectile speed multiplier.
- A tracking ultimate user with multiple enemies visible.
- A TaXue/dash-attack unit with a ranged skill.
- A combo that emits nearby tracking or spiral bleed projectiles.

Expected observations:

- Ultimate/extra projectiles distribute to alternate enemies when available.
- Tracking ultimate does not visually collapse onto one target when nearby enemies exist.
- Side projectiles are visibly faster for projectile-speed-buffed units.
- Ranged dash attack does not always dash straight forward into the target; when already in range it moves sideways/back as before.
- Follow-up projectiles inherit the attacker’s projectile speed.

---

## Notes

- The melee stale-counter timeout is intentionally out of scope for this plan.
- Do not reintroduce scene-side target lookup callbacks or `std::function` roll/position hooks.
- Do not add broad render or adapter DTOs. The only new cast DTO is the minimal projectile target row needed by runtime-owned spread generation.
- If dash velocity tests are flaky due random side choice, assert only non-zero lateral movement or bounded distance, not exact sign.
