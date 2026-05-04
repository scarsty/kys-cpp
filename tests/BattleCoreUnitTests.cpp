#include "battle/BattleCombatIntent.h"
#include "battle/BattleCore.h"
#include "battle/BattleMovement.h"
#include "battle/BattlePresentationPlayback.h"
#include "BattleSceneBattleAdapter.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>

using namespace KysChess::Battle;

namespace
{

constexpr double SceneTileWidth = 36.0;
constexpr double MaxEffectiveBattleReach = 480.0;
constexpr double SceneAttackHitRadius = SceneTileWidth * 2.0;
constexpr double SceneBounceSpawnDistance = SceneTileWidth * 1.5;
constexpr double SceneProjectileSpeed = SceneTileWidth / 3.0;
constexpr double LegacyMinimumVectorNorm = 0.0001;

BattleMovementConfig testConfig()
{
    BattleMovementGeometry geometry;
    geometry.tileWidth = SceneTileWidth;
    geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    geometry.meleeAttackHitRadius = SceneTileWidth * 2.0;
    geometry.dashFrames = 5;
    geometry.dashCooldownFrames = 18;
    geometry.maxRangedReach = MaxEffectiveBattleReach;
    return BattleGeometry(geometry).movementConfig();
}

BattleUnitState unit(int id, int team, Pointf position, CombatStyle style = CombatStyle::Melee)
{
    BattleUnitState state;
    state.id = id;
    state.realRoleId = id;
    state.name = std::to_string(id);
    state.team = team;
    state.position = position;
    state.speed = 5.0;
    state.reach = style == CombatStyle::Ranged ? 400.0 : 137.5;
    state.style = style;
    return state;
}

BattleWorldState worldWith(std::vector<BattleUnitState> units)
{
    BattleWorldState world;
    world.config = testConfig();
    world.units = std::move(units);
    world.canStandAt = [](Pointf) { return true; };
    return world;
}

BattleAttackWorld attackWorld()
{
    BattleAttackWorld world;
    world.hitRadius = SceneAttackHitRadius;
    world.minimumVectorNorm = LegacyMinimumVectorNorm;
    world.bounceSpawnDistance = SceneBounceSpawnDistance;
    world.defaultProjectileSpeed = SceneProjectileSpeed;
    return world;
}

BattleTeamEffectUnit adapterTeamEffectUnit(int id, int team, int hp, int mp, int shield = 0)
{
    BattleTeamEffectUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = true;
    unit.hp = hp;
    unit.maxHp = 100;
    unit.mp = mp;
    unit.maxMp = 50;
    unit.shield = shield;
    return unit;
}

BattleSkillState skill(int attackAreaType, double reach = 400.0, bool forceRanged = false)
{
    BattleSkillState state;
    state.id = 1;
    state.name = "test";
    state.attackAreaType = attackAreaType;
    state.magicType = 1;
    state.reach = reach;
    state.forceRanged = forceRanged;
    state.rangedStyle = forceRanged || attackAreaType == 1 || attackAreaType == 2 || attackAreaType == 3;
    return state;
}

BattleAttackSpawnRequest attackSpawnRequest()
{
    BattleAttackSpawnRequest request;
    request.initial.attackerUnitId = 1;
    request.initial.skillId = 101;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.visualEffectId = 44;
    request.initial.preferredTargetUnitId = 2;
    request.initial.position = { 100, 120, 0 };
    request.initial.velocity = { 6, 0, 0 };
    request.initial.totalFrame = 30;
    request.initial.track = true;
    request.initial.through = true;
    request.initial.sharedHitGroupId = 7;
    return request;
}

BattleHitUnitSnapshot hitUnitSnapshot(int id, int team, int hp, Pointf position)
{
    BattleHitUnitSnapshot snapshot;
    snapshot.id = id;
    snapshot.team = team;
    snapshot.alive = true;
    snapshot.hp = hp;
    snapshot.maxHp = 100;
    snapshot.mp = 50;
    snapshot.maxMp = 100;
    snapshot.attack = 30;
    snapshot.defence = 5.0;
    snapshot.position = position;
    snapshot.facing = { 1, 0, 0 };
    return snapshot;
}

BattleHitSkillSnapshot hitSkillSnapshot(int id, int resolvedBaseDamage)
{
    BattleHitSkillSnapshot snapshot;
    snapshot.id = id;
    snapshot.name = "test skill";
    snapshot.magicType = 1;
    snapshot.resolvedBaseDamage = resolvedBaseDamage;
    return snapshot;
}

BattleHitItemSnapshot hitItemSnapshot(int id, int resolvedDamage)
{
    BattleHitItemSnapshot snapshot;
    snapshot.id = id;
    snapshot.name = "test item";
    snapshot.resolvedDamage = resolvedDamage;
    return snapshot;
}

BattleCastConfig frameCastConfig()
{
    BattleCastConfig config;
    config.castFrames = { 25, 30, 20, 25 };
    config.baseCooldownFrames = { 105, 185, 115, 45 };
    config.minimumCooldownFrames = { 60, 70, 70, 45 };
    config.cooldownActPropertyDivisors = { 2, 1, 2, 0 };
    config.recoveryFrames = { 4, 4, 4, 5 };
    config.maxCooldownSpeed = 150;
    config.speedCooldownReductionRatio = 0.5;
    config.minimumCooldownAfterCastPadding = 2;
    config.normalCastMpDelta = 5;
    config.minimumFacingNorm = LegacyMinimumVectorNorm;
    config.meleeHitTotalFrame = 10;
    config.strengthenedMeleeTotalFrame = 30;
    config.strengthenedMeleeSelectDistanceDivisor = 2.0;
    config.strengthenedMeleeMultiplier = 2.0f;
    config.meleeSplashTotalFrame = 60;
    config.meleeSplashInitialFrame = 5;
    config.meleeSplashStrengthMultiplier = 0.5f;
    config.trackingProjectileTotalFrame = 120;
    config.dashHitTotalFrame = 30;
    config.strengthenedMeleeOperationCountThreshold = 2;
    return config;
}

BattleCastInput frameCastInput(int sourceUnitId, int targetUnitId)
{
    BattleCastInput input;
    input.config = frameCastConfig();
    input.geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    input.geometry.projectileSpeed = SceneProjectileSpeed;
    input.geometry.projectileSpawnOffset = SceneTileWidth * 2.0;
    input.geometry.projectileBaseTravel = SceneTileWidth * 5.0;
    input.geometry.projectileTravelPerSelectDistance = SceneTileWidth;
    input.geometry.meleeSplashProjectileSpeed = 3.0;
    input.geometry.dashHitPositionSpacing = 2.0;
    input.geometry.dashHitFrameStep = 3;
    input.unit.id = sourceUnitId;
    input.unit.position = { 10.0f, 20.0f, 0.0f };
    input.unit.facing = { 1.0f, 0.0f, 0.0f };
    input.unit.alive = true;
    input.unit.canStartAttack = true;
    input.unit.mp = 20;
    input.unit.maxMp = 100;
    input.unit.meleeAttackReach = 137.5;
    input.targetUnitId = targetUnitId;
    input.targetPosition = { 82.0f, 20.0f, 0.0f };
    input.targetDistance = 100.0;
    input.normalSkill.id = 301;
    input.normalSkill.name = "框架招式";
    input.normalSkill.attackAreaType = 0;
    input.normalSkill.magicType = 1;
    input.normalSkill.visualEffectId = 77;
    input.normalSkill.reach = 137.5;
    input.ultimateSkill.id = 401;
    input.ultimateSkill.attackAreaType = 1;
    input.ultimateSkill.magicType = 1;
    input.ultimateSkill.visualEffectId = 88;
    input.ultimateSkill.reach = 400.0;
    input.ultimateSkill.rangedStyle = true;
    return input;
}

BattleActionCommitInput frameActionCommitInput()
{
    BattleActionCommitInput input;
    input.unit.id = 1;
    input.unit.team = 0;
    input.unit.position = { 100, 100, 0 };
    input.unit.facing = { 1, 0, 0 };
    input.unit.operationCount = 0;
    input.strengthenedMeleeOperationCountThreshold = 2;
    return input;
}

BattleFrameActionUnitInput castPlanningActionUnit(BattleCastInput castInput)
{
    BattleFrameActionUnitInput actionUnit;
    actionUnit.unitId = castInput.unit.id;
    actionUnit.canPlanCast = true;
    actionUnit.castInput = std::move(castInput);
    return actionUnit;
}

BattleFrameActionUnitInput pendingActionUnit(BattleActionCommitInput actionInput)
{
    BattleFrameActionUnitInput actionUnit;
    actionUnit.unitId = actionInput.unit.id;
    actionUnit.state.haveAction = true;
    actionUnit.state.actFrame = 0;
    actionUnit.state.castFrame = 0;
    actionUnit.hasPendingActionInput = true;
    actionUnit.pendingActionInput = std::move(actionInput);
    return actionUnit;
}

BattleCastResult committedFrameCast()
{
    BattleCastResult result;
    result.decision.canCast = true;
    result.decision.unitId = 1;
    result.decision.targetUnitId = 2;
    result.decision.skillId = 101;
    result.decision.operationType = BattleOperationType::RangedProjectile;
    BattleAttackSpawnRequest request;
    request.initial.attackerUnitId = 1;
    request.initial.skillId = 101;
    request.initial.preferredTargetUnitId = 2;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.position = { 100, 100, 0 };
    request.initial.velocity = { 5, 0, 0 };
    request.initial.totalFrame = 30;
    result.attackSpawnRequests.push_back(request);
    return result;
}

BattleDamageTransactionInput lethalDamageInput(int attackerUnitId, int defenderUnitId)
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = attackerUnitId;
    input.request.defenderUnitId = defenderUnitId;
    input.request.baseDamage = 20;
    input.attacker.id = attackerUnitId;
    input.attacker.alive = true;
    input.attacker.hp = 100;
    input.attacker.maxHp = 100;
    input.defender.id = defenderUnitId;
    input.defender.alive = true;
    input.defender.hp = 10;
    input.defender.maxHp = 100;
    input.defenderStatus.id = defenderUnitId;
    input.defenderStatus.alive = true;
    input.defenderStatus.hp = 10;
    input.defenderStatus.maxHp = 100;
    return input;
}

}  // namespace

TEST_CASE("BattleMovementGeometryAndConfig_MaxRangedReachStartsEmptyUntilSupplied", "[battle][core]")
{
    BattleMovementGeometry geometry;
    BattleMovementConfig config;

    CHECK(geometry.maxRangedReach == 0.0);
    CHECK(config.maxRangedReach == 0.0);
}

TEST_CASE("BattleCombatIntent_OperationTypeMapping_MatchesSceneAnimationTypes", "[battle][intent]")
{
    BattleCombatIntentPlanner planner;
    CHECK(planner.operationTypeForAttackArea(0) == BattleOperationType::Melee);
    CHECK(planner.operationTypeForAttackArea(1) == BattleOperationType::RangedProjectile);
    CHECK(planner.operationTypeForAttackArea(2) == BattleOperationType::RangedProjectile);
    CHECK(planner.operationTypeForAttackArea(3) == BattleOperationType::TrackingProjectile);
    CHECK(planner.operationTypeForAttackArea(99) == BattleOperationType::None);
}

TEST_CASE("BattleCombatIntent_UltimateEquipsOnlyWhenReadyInputIsSet", "[battle][intent]")
{
    CombatIntentInput input;
    input.canStartAttack = true;
    input.hasEquippedSkill = false;
    input.ultimateReady = true;
    input.targetDistance = 100.0;
    input.meleeAttackReach = 137.5;
    input.dashAttackReach = 375.0;
    input.plannedSkill = skill(0, 137.5);

    BattleCombatIntentPlanner planner;
    auto intent = planner.select(input);
    CHECK(intent.equipPlannedSkill);
    CHECK(intent.announceUltimate);
    CHECK(intent.startAttack);
    CHECK(intent.operationType == BattleOperationType::Melee);

    input.ultimateReady = false;
    intent = planner.select(input);
    CHECK(intent.equipPlannedSkill);
    CHECK_FALSE(intent.announceUltimate);
}

TEST_CASE("BattleCombatIntent_PreservesMeleeBasicForcedRangedAndDashAttackRules", "[battle][intent]")
{
    CombatIntentInput forcedRanged;
    forcedRanged.canStartAttack = true;
    forcedRanged.hasEquippedSkill = true;
    forcedRanged.targetDistance = 300.0;
    forcedRanged.meleeAttackReach = 137.5;
    forcedRanged.dashAttackReach = 375.0;
    forcedRanged.plannedSkill = skill(0, 425.0, true);

    BattleCombatIntentPlanner planner;
    auto intent = planner.select(forcedRanged);
    CHECK(intent.startAttack);
    CHECK(intent.operationType == BattleOperationType::RangedProjectile);

    CombatIntentInput meleeDash = forcedRanged;
    meleeDash.dashAttackEnabled = true;
    meleeDash.plannedSkill = skill(0, 137.5, false);
    intent = planner.select(meleeDash);
    CHECK(intent.startAttack);
    CHECK(intent.operationType == BattleOperationType::Dash);
}

TEST_CASE("BattleCombatIntent_BlocksAttackWhileMovementDashContinues", "[battle][intent]")
{
    CombatIntentInput input;
    input.canStartAttack = true;
    input.hasEquippedSkill = false;
    input.ultimateReady = true;
    input.movementDashActive = true;
    input.targetDistance = 100.0;
    input.meleeAttackReach = 137.5;
    input.dashAttackReach = 375.0;
    input.plannedSkill = skill(0, 137.5);

    auto intent = BattleCombatIntentPlanner().select(input);
    CHECK(intent.equipPlannedSkill);
    CHECK(intent.announceUltimate);
    CHECK_FALSE(intent.startAttack);
}

TEST_CASE("BattleCore_MovementConfig_DerivesSharedGeometry", "[battle][core]")
{
    auto config = testConfig();
    CHECK(config.engagementDeadband == 18.0);
    CHECK(config.engagementArriveDistance == 27.0);
    CHECK(config.meleeAttackReach == 99.0);
    CHECK(config.meleeLocalTargetRadius == 171.0);
    CHECK(config.bodyRadius == 54.0);
    CHECK(config.movementDashDistanceMultiplier == 2.0);
}

TEST_CASE("BattleCore_ProbeMove_SeparatesWallsUnitsAndReservations", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 160, 100, 0 }),
        unit(3, 1, { 300, 100, 0 }),
    });
    world.canStandAt = [](Pointf p) { return p.x >= 0 && p.y >= 0; };

    BattleMovementPlanner planner(world);
    CHECK(planner.probeMove(world.units[0], { -5, 100, 0 }, false).reason == MoveBlockReason::Wall);
    CHECK(planner.probeMove(world.units[0], { 150, 100, 0 }, false).reason == MoveBlockReason::Ally);
    CHECK(planner.probeMove(world.units[0], { 290, 100, 0 }, false).reason == MoveBlockReason::Enemy);

    std::map<int, Pointf> reservations = { { 2, { 130, 130, 0 } } };
    CHECK(planner.probeMove(world.units[0], { 135, 135, 0 }, false, reservations).reason == MoveBlockReason::Reservation);
    CHECK(planner.probeMove(world.units[0], { 150, 100, 0 }, true).canMove);
}

TEST_CASE("BattleCore_AttackReady_HoldsWhenAlreadyInRange", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 210, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::AttackReady);
    CHECK(world.units[0].position.x == 100.0f);
    CHECK(world.units[0].velocity.norm() == 0.0f);
}

TEST_CASE("BattleCore_PlannedDash_UsesSpeedScaledDistanceForNormalUnits", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 600, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::Dash);
    CHECK(decision.dashDistance == 50.0);
    CHECK(world.units[0].dashFramesRemaining == world.config.dashFrames);
}

TEST_CASE("BattleCore_MovementStats_CountsDashStartsOnly", "[battle][core]")
{
    auto run = BattleMovementSimulator(worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 600, 100, 0 }),
    })).run(6, 1);

    CHECK(run.stats.at(1).dashCount == 1);
    CHECK(run.stats.at(1).lastDashDistance == 50.0);
}

TEST_CASE("BattleCore_PlannedDash_IgnoresUnitsButRespectsTerrainSegment", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 130, 100, 0 }),
        unit(3, 1, { 600, 100, 0 }),
    });

    auto openResult = BattleMovementPlanner(world).tick();
    CHECK(openResult.decisions.at(1).action == MovementAction::Dash);

    auto blockedWorld = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 130, 100, 0 }),
        unit(3, 1, { 600, 100, 0 }),
    });
    blockedWorld.canStandAt = [](Pointf p) { return p.x < 120.0f || p.x > 170.0f; };

    auto blockedResult = BattleMovementPlanner(blockedWorld).tick();
    CHECK(blockedResult.decisions.at(1).action != MovementAction::Dash);
}

TEST_CASE("BattleCore_TaXue_AllowsLongDash", "[battle][core]")
{
    auto chaser = unit(1, 0, { 100, 100, 0 });
    chaser.taXue = true;
    chaser.dashAttack = true;
    auto world = worldWith({
        chaser,
        unit(2, 1, { 600, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::Dash);
    CHECK(decision.dashDistance == world.config.maxDashDistance);
}

TEST_CASE("BattleCore_RangedHold_DoesNotBackIntoOccupiedRingWhenInRange", "[battle][core]")
{
    auto ranged = unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged);
    ranged.canAttack = false;
    auto world = worldWith({
        ranged,
        unit(2, 0, { 60, 100, 0 }),
        unit(3, 1, { 300, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::AttackReady);
    CHECK(world.units[0].position.x == 100.0f);
    CHECK(world.units[0].velocity.norm() == 0.0f);
}

TEST_CASE("BattleCore_MeleeOutsideReach_MovesInsteadOfReportingReady", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 260, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    CHECK(result.decisions.at(1).action == MovementAction::Move);
}

TEST_CASE("BattleCore_SlotSwitchCooldown_BoundsRepeatedReplans", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 125, 100, 0 }),
        unit(3, 1, { 500, 100, 0 }),
    });
    world.units[0].dashCooldownRemaining = 999;

    auto first = BattleMovementPlanner(world).tick();
    CHECK((first.decisions.at(1).blockReason == MoveBlockReason::Ally
        || first.decisions.at(1).blockReason == MoveBlockReason::Reservation));
    CHECK(world.units[0].slotSwitchCooldownRemaining > 0);
    int slotAfterFirst = world.units[0].assignedSlot;

    auto second = BattleMovementPlanner(world).tick();
    CHECK(world.units[0].assignedSlot == slotAfterFirst);
    CHECK(second.decisions.at(1).slot == slotAfterFirst);
}

TEST_CASE("BattleFrameUnitRuntimeSystem_AdvancesCooldownAndIdleResourceTicks", "[battle][core]")
{
    BattleFrameUnitRuntimeState state;
    state.cooldown = 1;
    state.actType = 2;
    state.operationType = BattleOperationType::Dash;
    state.haveAction = true;
    state.physicalPower = 9;

    BattleFrameUnitRuntimeInput input;
    input.state = state;
    input.frame = 6;
    input.frozen = false;
    input.mpRegenIntervalFrames = 3;
    input.physicalPowerRegenIntervalFrames = 3;

    auto result = BattleFrameUnitRuntimeSystem().advance(input);

    CHECK(result.state.cooldown == 0);
    CHECK(result.state.actFrame == 0);
    CHECK(result.state.actType == -1);
    CHECK(result.state.operationType == BattleOperationType::None);
    CHECK_FALSE(result.state.haveAction);
    CHECK(result.state.physicalPower == 10);
    CHECK(result.mpDelta == 1);
    CHECK(result.skillFinished);
    CHECK(result.resetDashVelocity);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsMovementBeforeProjectileEvents", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 600, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 1;
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.totalFrame = 30;
    projectile.state.visualEffectId = 33;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 0, 0, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 600, 100, 0 } },
    };
    state.attacks.attacks.push_back(projectile);

    auto result = BattleFrameRunner().advanceFrame(state);

    CHECK(state.world.frame == 1);
    CHECK(result.movement.events[0].type == BattleEventType::DashStart);
    REQUIRE(result.frame.presentationEvents.size() > result.movement.events.size());
    CHECK(result.frame.snapshot.frame == 1);
    CHECK(result.frame.presentationEvents[0].type == BattlePresentationEventType::StatusLog);
    CHECK(result.frame.presentationEvents[0].text == "dash-start");
    const auto& firstProjectileEvent = result.frame.presentationEvents[result.movement.events.size()];
    CHECK(firstProjectileEvent.type == BattlePresentationEventType::ProjectileMoved);
    CHECK(firstProjectileEvent.effectId == 10);
    CHECK(firstProjectileEvent.sourceUnitId == 1);
    CHECK(firstProjectileEvent.targetUnitId == 2);
    CHECK(firstProjectileEvent.durationFrames == 30);
    CHECK(firstProjectileEvent.visualEffectId == 33);
    CHECK(firstProjectileEvent.position.x == 5.0f);
    CHECK(firstProjectileEvent.velocity.x == 5.0f);
    CHECK(firstProjectileEvent.operationKind == 2);
}

TEST_CASE("BattleFrameState_ComposesHeadlessRuntimeStateForFullFrameRunner", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 240, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleCastInput castInput;
    castInput.unit.id = 1;
    castInput.targetUnitId = 2;
    state.actions.units.push_back(castPlanningActionUnit(castInput));

    BattleDamageTransactionInput damageInput;
    damageInput.request.attackerUnitId = 1;
    damageInput.request.defenderUnitId = 2;
    state.damage.pendingTransactions.push_back(damageInput);

    BattleStatusUnitState statusUnit;
    statusUnit.id = 1;
    statusUnit.hp = 80;
    statusUnit.maxHp = 100;
    state.status.units.push_back(statusUnit);

    KysChess::RoleComboState comboState;
    comboState.postSkillInvincFrames = 12;
    state.combo.units.emplace(1, comboState);

    BattleDeathEffectUnit deathEffectUnit;
    deathEffectUnit.id = 1;
    state.deathEffects.world.units.push_back(deathEffectUnit);

    CHECK(state.world.units.size() == 2);
    CHECK(state.actions.units[0].castInput.unit.id == 1);
    CHECK(state.damage.pendingTransactions[0].request.defenderUnitId == 2);
    CHECK(state.status.units[0].hp == 80);
    CHECK(state.combo.units.at(1).postSkillInvincFrames == 12);
    CHECK(state.deathEffects.world.units[0].id == 1);
    CHECK_FALSE(state.result.ended);
    CHECK(state.result.winningTeam == -1);
}

TEST_CASE("BattleSceneFrameBundle_CarriesCoreStateAndAdapterApplicationFacts", "[battle][core]")
{
    KysChess::BattleSceneBattleAdapter::BattleSceneFrameBundle bundle;

    bundle.state.world.frame = 9;
    bundle.rolesByBattleId.emplace(7, nullptr);
    BattlePresentationEvent event;
    event.type = BattlePresentationEventType::StatusLog;
    event.sourceUnitId = 7;
    event.text = "adapter fact";
    bundle.pendingPresentationEvents.push_back(event);

    CHECK(bundle.state.world.frame == 9);
    REQUIRE(bundle.rolesByBattleId.contains(7));
    REQUIRE(bundle.pendingPresentationEvents.size() == 1);
    CHECK(bundle.pendingPresentationEvents[0].sourceUnitId == 7);
}

TEST_CASE("BattleCore_AppliesTeamEffectGameplayCommands", "[battle][core]")
{
    BattleTeamEffectWorld world;
    world.units = {
        adapterTeamEffectUnit(1, 0, 40, 10),
        adapterTeamEffectUnit(2, 0, 90, 45, 3),
        adapterTeamEffectUnit(3, 1, 20, 5),
    };

    auto heal = applyBattleTeamEffectCommand(
        world,
        BattleTeamHealCommand{ 1, 5, 10, "技能群療" });
    REQUIRE(heal.events.size() == 2);
    CHECK(heal.events[0].targetUnitId == 1);
    CHECK(heal.events[0].value == 15);
    CHECK(world.units[0].hp == 55);
    CHECK(world.units[1].hp == 100);
    REQUIRE(heal.presentationEvents.size() == 2);
    CHECK(heal.presentationEvents[0].type == BattlePresentationEventType::HealLog);
    CHECK(heal.presentationEvents[0].text == "技能群療");

    auto mp = applyBattleTeamEffectCommand(
        world,
        BattleTeamMpRestoreCommand{ 1, 8, "全隊回內" });
    REQUIRE(mp.events.size() == 2);
    CHECK(world.units[0].mp == 18);
    CHECK(world.units[1].mp == 50);
    REQUIRE(mp.presentationEvents.size() == 2);
    CHECK(mp.presentationEvents[0].type == BattlePresentationEventType::StatusLog);
    CHECK(mp.presentationEvents[0].text == "全隊回內+8MP");

    auto shield = applyBattleTeamEffectCommand(
        world,
        BattleTeamShieldCommand{ 1, 7, true, "全隊護盾" });
    REQUIRE(shield.events.size() == 2);
    CHECK(world.units[0].shield == 7);
    CHECK(world.units[1].shield == 7);
    REQUIRE(shield.presentationEvents.size() == 2);
    CHECK(shield.presentationEvents[0].type == BattlePresentationEventType::StatusLog);
    CHECK(shield.presentationEvents[0].text == "全隊護盾（7護盾）");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_SnapshotIncludesCommittedUnitState", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 600, 100, 0 }),
    });
    state.attacks = attackWorld();
    BattleStatusUnitState statusUnit;
    statusUnit.id = 1;
    statusUnit.hp = 80;
    statusUnit.maxHp = 120;
    statusUnit.invincible = 6;
    state.status.units.push_back(statusUnit);

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(result.frame.snapshot.units.size() == state.world.units.size());
    CHECK(result.frame.snapshot.frame == state.world.frame);
    CHECK(result.frame.snapshot.units[0].id == state.world.units[0].id);
    CHECK(result.frame.snapshot.units[0].hp == statusUnit.hp);
    CHECK(result.frame.snapshot.units[0].maxHp == statusUnit.maxHp);
    CHECK(result.frame.snapshot.units[0].invincible == statusUnit.invincible);
    CHECK(result.frame.snapshot.units[0].position.x == state.world.units[0].position.x);
    CHECK(result.frame.snapshot.units[0].velocity.x == state.world.units[0].velocity.x);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsPendingAttackSpawnRequest", "[battle][core][presentation]")
{
    BattleFrameState state;
    state.world = worldWith({});
    state.attacks = attackWorld();
    state.attacks.hitRadius = 10.0;
    state.attacks.nextAttackId = 50;
    state.attacks.units = {
        { 1, 0, true, false, false, { 0, 0, 0 } },
        { 2, 1, true, false, false, { 106, 120, 0 } },
    };
    state.pendingAttackSpawns.push_back(attackSpawnRequest());

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(result.attackEvents.size() == 3);
    CHECK(result.attackEvents[0].type == BattleAttackEventType::AttackSpawned);
    CHECK(result.attackEvents[0].attackId == 50);
    CHECK(result.attackEvents[1].type == BattleAttackEventType::Moved);
    CHECK(result.attackEvents[1].attackId == 50);
    CHECK(result.attackEvents[2].type == BattleAttackEventType::Hit);
    CHECK(result.attackEvents[2].attackId == 50);
    CHECK(result.attackEvents[2].unitId == 2);
    CHECK(state.pendingAttackSpawns.empty());
    REQUIRE(state.attacks.attacks.size() == 1);
    CHECK(state.attacks.attacks[0].id == 50);
    CHECK(state.attacks.attacks[0].state.position.x == 106.0f);
    CHECK(state.attacks.nextAttackId == 51);

    REQUIRE(result.frame.gameplayEvents.size() == 3);
    const auto& gameplay = result.frame.gameplayEvents[0];
    CHECK(gameplay.type == BattleGameplayEventType::AttackSpawned);
    CHECK(gameplay.effectId == 50);
    CHECK(gameplay.sourceUnitId == 1);
    CHECK(gameplay.targetUnitId == 2);
    CHECK(gameplay.position.x == 100.0f);
    CHECK(gameplay.position.y == 120.0f);

    REQUIRE(result.frame.presentationEvents.size() == 3);
    const auto& presentation = result.frame.presentationEvents[0];
    CHECK(presentation.type == BattlePresentationEventType::ProjectileSpawned);
    CHECK(presentation.effectId == 50);
    CHECK(presentation.sourceUnitId == 1);
    CHECK(presentation.targetUnitId == 2);
    CHECK(presentation.durationFrames == 30);
    CHECK(presentation.visualEffectId == 44);
    CHECK(presentation.position.x == 100.0f);
    CHECK(presentation.position.y == 120.0f);
    CHECK(presentation.velocity.x == 6.0f);
    CHECK(presentation.operationKind == 2);

    CHECK(result.frame.presentationEvents[1].type == BattlePresentationEventType::ProjectileMoved);
    CHECK(result.frame.presentationEvents[1].position.x == 106.0f);
    CHECK(result.frame.presentationEvents[2].type == BattlePresentationEventType::ProjectileHit);
    CHECK(result.frame.gameplayEvents[2].type == BattleGameplayEventType::ProjectileHit);
    CHECK(result.frame.gameplayEvents[2].targetUnitId == 2);

    auto plan = BattlePresentationPlaybackPlanner().build(result.frame);
    REQUIRE(plan.commands.size() == 3);
    CHECK(plan.commands[0].type == BattlePresentationCommandType::SpawnProjectile);
    CHECK(plan.commands[0].projectileAttackId == 50);
    CHECK(plan.commands[0].visualEffectId == 44);
    CHECK(plan.commands[0].projectilePosition.x == 100.0f);
    CHECK(plan.commands[0].projectileVelocity.x == 6.0f);
    CHECK(plan.commands[0].projectileDurationFrames == 30);
    CHECK(plan.commands[0].projectileOperationKind == 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsStatusBeforeCastPlanning", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 210, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleStatusUnitState statusUnit;
    statusUnit.id = 1;
    statusUnit.alive = true;
    statusUnit.attack = 15;
    statusUnit.tempAttackBuffs.push_back({ 5, 1 });
    state.status.units.push_back(statusUnit);

    auto cast = frameCastInput(1, 2);
    cast.unit.stunned = true;
    state.actions.units.push_back(castPlanningActionUnit(cast));

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.status.events.size() == 1);
    CHECK(state.status.events[0].type == BattleStatusEventType::TempAttackExpired);
    CHECK(state.status.units[0].attack == 10);
    REQUIRE(state.actions.unitResults.size() == 1);
    CHECK_FALSE(state.actions.unitResults[0].castResult.decision.canCast);
    CHECK(state.actions.unitResults[0].castResult.decision.reason == BattleCastBlockReason::Stunned);
    CHECK(result.frame.gameplayEvents.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastPlanningRecordsStartWithoutSpawningAttack", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 82, 20, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.hitRadius = 10.0;
    state.attacks.nextAttackId = 90;
    state.attacks.units = {
        { 1, 0, true, false, false, { 10, 20, 0 } },
        { 2, 1, true, false, false, { 82, 20, 0 } },
    };
    auto cast = frameCastInput(1, 2);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    state.actions.units.push_back(castPlanningActionUnit(cast));

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.actions.unitResults.size() == 1);
    REQUIRE(state.actions.unitResults[0].castResult.attackSpawnRequests.size() == 1);
    CHECK(result.attackEvents.empty());
    REQUIRE(result.frame.gameplayEvents.size() == 1);
    CHECK(result.frame.gameplayEvents[0].type == BattleGameplayEventType::CastStarted);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastInputUsesCommittedFrameState", "[battle][core]")
{
    BattleFrameState state;
    auto caster = unit(1, 0, { 10, 20, 0 }, CombatStyle::Ranged);
    caster.reach = 400.0;
    state.world = worldWith({
        caster,
        unit(2, 1, { 82, 20, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.nextAttackId = 70;
    state.attacks.units = {
        { 1, 0, true, false, false, { 10, 20, 0 } },
        { 2, 1, true, false, false, { 82, 20, 0 } },
    };
    BattleStatusUnitState frozenStatus;
    frozenStatus.id = 1;
    frozenStatus.alive = true;
    frozenStatus.frozenTimer = 3;
    state.status.units.push_back(frozenStatus);
    state.actions.units.push_back(castPlanningActionUnit(frameCastInput(1, 2)));

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.actions.unitResults.size() == 1);
    CHECK_FALSE(state.actions.unitResults[0].castResult.decision.canCast);
    CHECK(state.actions.unitResults[0].castResult.decision.reason == BattleCastBlockReason::Frozen);
    CHECK(result.attackEvents.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastOriginUsesPostMovementPosition", "[battle][core]")
{
    BattleFrameState state;
    auto caster = unit(1, 0, { 10, 20, 0 }, CombatStyle::Ranged);
    caster.reach = 400.0;
    state.world = worldWith({
        caster,
        unit(2, 1, { 210, 20, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.nextAttackId = 80;
    state.attacks.units = {
        { 1, 0, true, false, false, { 10, 20, 0 } },
        { 2, 1, true, false, false, { 210, 20, 0 } },
    };
    auto cast = frameCastInput(1, 2);
    cast.unit.position = { -500, -500, 0 };
    cast.targetPosition = { -250, -250, 0 };
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    state.actions.units.push_back(castPlanningActionUnit(cast));

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.actions.unitResults.size() == 1);
    REQUIRE(state.actions.unitResults[0].castResult.attackSpawnRequests.size() == 1);
    const auto& spawn = state.actions.unitResults[0].castResult.attackSpawnRequests[0];
    CHECK(spawn.initial.position.x == state.world.units[0].position.x + SceneTileWidth * 2.0);
    CHECK(spawn.initial.position.y == state.world.units[0].position.y);
    CHECK(result.attackEvents.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsActionInputsBeforeAttackTick", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 160, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 160, 100, 0 } },
    };

    auto action = frameActionCommitInput();
    action.hasCast = true;
    action.cast = committedFrameCast();
    state.actions.units.push_back(pendingActionUnit(action));

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.actions.unitResults.size() == 1);
    CHECK(state.actions.unitResults[0].unitId == 1);
    CHECK(state.actions.unitResults[0].actionInput.hasCast);
    CHECK(state.actions.unitResults[0].actionInput.cast.decision.skillId == 101);
    REQUIRE(state.actions.unitResults[0].actionResult.attackSpawnRequests.size() == 1);
    REQUIRE(result.attackEvents.size() >= 1);
    CHECK(result.attackEvents.front().type == BattleAttackEventType::AttackSpawned);
    CHECK(result.attackEvents.front().attackId == 0);
    CHECK(result.attackEvents.front().sourceUnitId == 1);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsSelectedCastInput", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();

    auto cast = frameCastInput(1, 2);
    cast.unit.mp = 0;
    cast.unit.maxMp = 100;
    cast.normalSkill.id = 101;
    cast.ultimateSkill.id = 202;
    BattleFrameActionUnitInput actionUnit;
    actionUnit.unitId = 1;
    actionUnit.hasSelectedCastInput = true;
    actionUnit.selectedCastInput = cast;
    actionUnit.selectedCastUltimate = true;
    actionUnit.selectedOperationType = BattleOperationType::RangedProjectile;
    actionUnit.selectedActionInput = frameActionCommitInput();
    state.actions.units.push_back(actionUnit);

    BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.actions.unitResults.size() == 1);
    CHECK(state.actions.unitResults[0].castResult.decision.canCast);
    CHECK(state.actions.unitResults[0].castResult.decision.ultimate);
    CHECK(state.actions.unitResults[0].castResult.decision.skillId == 202);
    CHECK(state.actions.unitResults[0].castResult.decision.operationType == BattleOperationType::RangedProjectile);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsSelectedCastFromActionFrameUnitInput", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleFrameActionUnitInput actionUnit;
    actionUnit.unitId = 1;
    actionUnit.hasSelectedCastInput = true;
    actionUnit.selectedCastInput = frameCastInput(1, 2);
    actionUnit.selectedCastInput.unit.mp = 0;
    actionUnit.selectedCastInput.unit.maxMp = 100;
    actionUnit.selectedCastInput.normalSkill.id = 101;
    actionUnit.selectedCastInput.ultimateSkill.id = 202;
    actionUnit.selectedCastUltimate = true;
    actionUnit.selectedOperationType = BattleOperationType::RangedProjectile;
    actionUnit.selectedActionInput = frameActionCommitInput();
    state.actions.units.push_back(actionUnit);

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.actions.unitResults.size() == 1);
    const auto& action = state.actions.unitResults[0];
    CHECK(action.actionCommitted);
    CHECK(action.castCommitted);
    CHECK(action.castResult.decision.canCast);
    CHECK(action.castResult.decision.ultimate);
    CHECK(action.castResult.decision.skillId == 202);
    CHECK(action.actionInput.hasCast);
    CHECK(action.actionInput.cast.decision.skillId == 202);
    REQUIRE(action.actionResult.attackSpawnRequests.size() == 1);
    REQUIRE(result.attackEvents.size() >= 1);
    CHECK(result.attackEvents.front().type == BattleAttackEventType::AttackSpawned);
    CHECK(result.attackEvents.front().sourceUnitId == 1);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_StartsCastFromActionFrameUnitInput", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 220, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleFrameActionUnitInput actionUnit;
    actionUnit.unitId = 1;
    actionUnit.castInput = frameCastInput(1, 2);
    actionUnit.castInput.normalSkill.attackAreaType = 1;
    actionUnit.castInput.normalSkill.rangedStyle = true;
    actionUnit.castInput.normalSkill.reach = 400.0;
    actionUnit.canPlanCast = true;
    state.actions.units.push_back(actionUnit);

    BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.actions.unitResults.size() == 1);
    const auto& action = state.actions.unitResults[0];
    CHECK(action.unitId == 1);
    CHECK(action.castStarted);
    CHECK(action.state.haveAction);
    CHECK(action.state.operationType == BattleOperationType::RangedProjectile);
    CHECK(action.state.actFrame == 0);
    CHECK(action.castResult.decision.canCast);
    CHECK(action.castResult.decision.skillId == 301);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsPendingCastFromActionFrameUnitInput", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 160, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 160, 100, 0 } },
    };

    BattleFrameActionUnitInput actionUnit;
    actionUnit.unitId = 1;
    actionUnit.state.haveAction = true;
    actionUnit.state.actFrame = 6;
    actionUnit.state.castFrame = 6;
    actionUnit.pendingActionInput = frameActionCommitInput();
    actionUnit.pendingActionInput.hasCast = true;
    actionUnit.pendingActionInput.cast = committedFrameCast();
    actionUnit.hasPendingActionInput = true;
    state.actions.units.push_back(actionUnit);

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.actions.unitResults.size() == 1);
    const auto& action = state.actions.unitResults[0];
    CHECK(action.castCommitted);
    CHECK(action.actionInput.hasCast);
    CHECK(action.actionInput.cast.decision.skillId == 101);
    REQUIRE(action.actionResult.attackSpawnRequests.size() == 1);
    REQUIRE(result.attackEvents.size() >= 1);
    CHECK(result.attackEvents.front().type == BattleAttackEventType::AttackSpawned);
    CHECK(result.attackEvents.front().sourceUnitId == 1);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ClearsRecoveredActionFrameUnitState", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
    });
    state.attacks = attackWorld();

    BattleFrameActionUnitInput actionUnit;
    actionUnit.unitId = 1;
    actionUnit.state.haveAction = true;
    actionUnit.state.actFrame = 11;
    actionUnit.state.actType = 2;
    actionUnit.state.castFrame = 6;
    actionUnit.state.operationType = BattleOperationType::RangedProjectile;
    actionUnit.state.cooldownFrames = 30;
    actionUnit.state.recoveryFrames = 4;
    state.actions.units.push_back(actionUnit);

    BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.actions.unitResults.size() == 1);
    const auto& action = state.actions.unitResults[0];
    CHECK(action.state.actFrame == 12);
    CHECK_FALSE(action.state.haveAction);
    CHECK(action.state.actType == -1);
    CHECK(action.state.operationType == BattleOperationType::None);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsPendingItemFromActionFrameUnitInput", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 160, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleFrameActionUnitInput actionUnit;
    actionUnit.unitId = 1;
    actionUnit.state.haveAction = true;
    actionUnit.state.actFrame = 6;
    actionUnit.state.castFrame = 6;
    actionUnit.pendingActionInput = frameActionCommitInput();
    actionUnit.pendingActionInput.hasItem = true;
    actionUnit.pendingActionInput.item.id = 501;
    actionUnit.pendingActionInput.item.itemType = 4;
    actionUnit.pendingActionInput.item.hiddenWeaponEffectId = 901;
    actionUnit.pendingActionInput.hiddenWeaponVelocity = { 10, 0, 0 };
    actionUnit.pendingActionInput.hiddenWeaponTotalFrame = 100;
    actionUnit.hasPendingActionInput = true;
    state.actions.units.push_back(actionUnit);

    BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.actions.unitResults.size() == 1);
    const auto& action = state.actions.unitResults[0];
    CHECK(action.actionCommitted);
    CHECK(action.actionResult.itemUseCommands.empty());
    REQUIRE(action.actionResult.itemCountDeltas.size() == 1);
    CHECK(action.actionResult.itemCountDeltas[0].delta == -1);
    REQUIRE(action.actionResult.attackSpawnRequests.size() == 1);
    CHECK(action.actionResult.attackSpawnRequests[0].initial.hiddenWeaponItemId == 501);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DamageDeathPrecedesBattleEndEvent", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 210, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.status.units = {
        { 1, true, 100, 100 },
        { 2, true, 10, 100 },
    };
    state.damage.pendingTransactions.push_back(lethalDamageInput(1, 2));

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.damage.committedTransactions.size() == 1);
    CHECK(state.world.units[1].alive == false);
    CHECK(state.status.units[1].alive == false);
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);

    std::vector<BattleGameplayEventType> gameplayTypes;
    for (const auto& event : result.frame.gameplayEvents)
    {
        gameplayTypes.push_back(event.type);
    }
    REQUIRE(gameplayTypes.size() >= 3);
    CHECK(gameplayTypes[gameplayTypes.size() - 3] == BattleGameplayEventType::DamageApplied);
    CHECK(gameplayTypes[gameplayTypes.size() - 2] == BattleGameplayEventType::UnitDied);
    CHECK(gameplayTypes[gameplayTypes.size() - 1] == BattleGameplayEventType::BattleEnded);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathEffectWorldSeesCommittedDamageRewards", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({});
    state.attacks = attackWorld();
    auto input = lethalDamageInput(1, 2);
    input.attacker.hp = 40;
    input.attacker.maxHp = 100;
    input.attacker.attack = 12;
    input.attacker.killHealPct = 25;
    input.attacker.bloodlustAttackPerKill = 7;
    state.damage.pendingTransactions.push_back(input);
    state.deathEffects.world.units = {
        { 1, 0, true, 40, 100, 12, 8 },
        { 2, 1, true, 10, 100, 9, 6 },
    };

    BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.damage.committedTransactions.size() == 1);
    REQUIRE(state.deathEffects.world.units.size() == 2);
    CHECK(state.damage.committedTransactions.front().attacker.hp == 65);
    CHECK(state.damage.committedTransactions.front().attacker.attack == 19);
    CHECK(state.deathEffects.world.units[0].hp == 65);
    CHECK(state.deathEffects.world.units[0].attack == 19);
    CHECK(state.deathEffects.world.units[1].alive == false);
    CHECK(state.deathEffects.world.units[1].hp == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_BattleEndEventEmitsOnce", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 210, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.damage.pendingTransactions.push_back(lethalDamageInput(1, 2));

    auto first = BattleFrameRunner().advanceFrame(state);
    auto second = BattleFrameRunner().advanceFrame(state);

    int firstEndEvents = 0;
    for (const auto& event : first.frame.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded)
        {
            ++firstEndEvents;
        }
    }
    int secondEndEvents = 0;
    for (const auto& event : second.frame.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded)
        {
            ++secondEndEvents;
        }
    }

    CHECK(firstEndEvents == 1);
    CHECK(secondEndEvents == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_BattleResultCountsPendingTeamUnits", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.result.pendingAliveByTeam[1] = 2;

    auto waiting = BattleFrameRunner().advanceFrame(state);

    CHECK_FALSE(state.result.ended);
    CHECK(waiting.frame.gameplayEvents.empty());

    state.result.pendingAliveByTeam[1] = 0;
    auto ended = BattleFrameRunner().advanceFrame(state);

    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);
    REQUIRE(ended.frame.gameplayEvents.size() == 1);
    CHECK(ended.frame.gameplayEvents.front().type == BattleGameplayEventType::BattleEnded);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsProjectileGameplayEventsSeparatelyFromPresentation", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 105, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 1;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    BattleAttackInstance expiringProjectile;
    expiringProjectile.id = 20;
    expiringProjectile.state.attackerUnitId = 1;
    expiringProjectile.state.totalFrame = 1;
    expiringProjectile.noHurt = true;
    expiringProjectile.state.position = { 300, 100, 0 };
    expiringProjectile.state.velocity = { 5, 0, 0 };

    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 105, 100, 0 } },
    };
    state.attacks.attacks.push_back(projectile);
    state.attacks.attacks.push_back(expiringProjectile);

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(result.attackEvents.size() == 4);
    REQUIRE(result.frame.gameplayEvents.size() == result.attackEvents.size());
    REQUIRE(result.frame.presentationEvents.size() == result.movement.events.size() + result.attackEvents.size());

    CHECK(result.attackEvents[0].type == BattleAttackEventType::Moved);
    CHECK(result.frame.gameplayEvents[0].type == BattleGameplayEventType::ProjectileMoved);
    CHECK(result.frame.gameplayEvents[0].effectId == 10);
    CHECK(result.frame.gameplayEvents[0].sourceUnitId == 1);
    CHECK(result.frame.gameplayEvents[0].position.x == 105.0f);
    CHECK(result.frame.presentationEvents[result.movement.events.size()].type == BattlePresentationEventType::ProjectileMoved);

    CHECK(result.attackEvents[1].type == BattleAttackEventType::Hit);
    CHECK(result.frame.gameplayEvents[1].type == BattleGameplayEventType::ProjectileHit);
    CHECK(result.frame.gameplayEvents[1].effectId == 10);
    CHECK(result.frame.gameplayEvents[1].sourceUnitId == 1);
    CHECK(result.frame.gameplayEvents[1].targetUnitId == 2);
    CHECK(result.frame.presentationEvents[result.movement.events.size() + 1].type == BattlePresentationEventType::ProjectileHit);

    CHECK(result.attackEvents[2].type == BattleAttackEventType::Moved);
    CHECK(result.frame.gameplayEvents[2].type == BattleGameplayEventType::ProjectileMoved);
    CHECK(result.frame.gameplayEvents[2].effectId == 20);
    CHECK(result.frame.presentationEvents[result.movement.events.size() + 2].type == BattlePresentationEventType::ProjectileMoved);

    CHECK(result.attackEvents[3].type == BattleAttackEventType::Expired);
    CHECK(result.frame.gameplayEvents[3].type == BattleGameplayEventType::ProjectileExpired);
    CHECK(result.frame.gameplayEvents[3].effectId == 20);
    CHECK(result.frame.presentationEvents[result.movement.events.size() + 3].type == BattlePresentationEventType::ProjectileExpired);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ResolvesHitEventsWithFrameHitInputs", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 105, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 105, 100, 0 } },
    };

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 1;
    projectile.state.skillId = 101;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    state.hits.units = {
        hitUnitSnapshot(1, 0, 80, { 100, 100, 0 }),
        hitUnitSnapshot(2, 1, 100, { 105, 100, 0 }),
    };
    state.hits.skills.push_back({ 10, 1, 2, hitSkillSnapshot(101, 70) });
    state.hits.scalars.push_back({ 10, 1, 2, 70, 0, 1, 0, {} });

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.hits.committedResults.size() == 1);
    CHECK(state.hits.committedResults[0].attackerUnitId == 1);
    CHECK(state.hits.committedResults[0].defenderUnitId == 2);
    CHECK(state.hits.committedResults[0].finalHpDamage == 69);

    const auto hpDamage = std::find_if(
        result.commands.begin(),
        result.commands.end(),
        [](const BattleGameplayCommand& command)
        {
            const auto* hp = std::get_if<BattleHpDamageCommand>(&command);
            return hp && hp->sourceUnitId == 1 && hp->targetUnitId == 2;
        });
    REQUIRE(hpDamage != result.commands.end());
    CHECK(std::get<BattleHpDamageCommand>(*hpDamage).damage == state.hits.committedResults[0].finalHpDamage);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DodgeConsumesHitBeforeDamage", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 105, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 105, 100, 0 } },
    };

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 1;
    projectile.state.skillId = 101;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    state.hits.units = {
        hitUnitSnapshot(1, 0, 80, { 100, 100, 0 }),
        hitUnitSnapshot(2, 1, 100, { 105, 100, 0 }),
    };
    state.hits.skills.push_back({ 10, 1, 2, hitSkillSnapshot(101, 70) });
    state.hits.scalars.push_back({ 10, 1, 2, 70, 0, 1, 0, { 0.0 } });
    KysChess::RoleComboState defenderCombo;
    defenderCombo.dodgeChancePct = 100;
    state.combo.units.emplace(2, defenderCombo);

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.hits.committedResults.size() == 1);
    CHECK(state.hits.committedResults[0].dodged);
    CHECK(state.combo.units.at(2).dodgedLast);
    CHECK(std::none_of(
        result.commands.begin(),
        result.commands.end(),
        [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleHpDamageCommand>(command);
        }));

    const auto dodgeLog = std::find_if(
        result.frame.presentationEvents.begin(),
        result.frame.presentationEvents.end(),
        [](const BattlePresentationEvent& event)
        {
            return event.type == BattlePresentationEventType::StatusLog
                && event.sourceUnitId == 2
                && event.targetUnitId == 1
                && event.text == "閃避了來襲攻擊";
        });
    CHECK(dodgeLog != result.frame.presentationEvents.end());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConsumesHiddenWeaponScalarInput", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 105, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 105, 100, 0 } },
    };

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 1;
    projectile.state.hiddenWeaponItemId = 501;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    state.hits.units = {
        hitUnitSnapshot(1, 0, 80, { 100, 100, 0 }),
        hitUnitSnapshot(2, 1, 100, { 105, 100, 0 }),
    };
    state.hits.items.push_back({ 10, 1, 2, hitItemSnapshot(501, 100) });
    state.hits.scalars.push_back({ 10, 1, 2, 0, 100, 1, 0, {} });

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.hits.committedResults.size() == 1);
    CHECK(state.hits.committedResults[0].finalHpDamage == 19);
    const auto hpDamage = std::find_if(
        result.commands.begin(),
        result.commands.end(),
        [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleHpDamageCommand>(command);
        });
    REQUIRE(hpDamage != result.commands.end());
    CHECK(std::get<BattleHpDamageCommand>(*hpDamage).detailText == "暗器");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ResolvesScriptedHitEvents", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 105, 100, 0 }),
    });
    state.attacks = attackWorld();
    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 105, 100, 0 } },
    };

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 1;
    projectile.state.scriptedDamage = 33;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    state.attacks.attacks.push_back(projectile);

    state.hits.units = {
        hitUnitSnapshot(1, 0, 80, { 100, 100, 0 }),
        hitUnitSnapshot(2, 1, 100, { 105, 100, 0 }),
    };
    state.hits.scalars.push_back({ 10, 1, 2, 0, 0, 1, 0, {} });

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.hits.committedResults.size() == 1);
    CHECK(state.hits.committedResults[0].finalHpDamage == 33);
    REQUIRE(!result.commands.empty());
    const auto* hp = std::get_if<BattleHpDamageCommand>(&result.commands.front());
    REQUIRE(hp);
    CHECK(hp->damage == 33);
    CHECK(hp->detailText == "特效傷害");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsTargetLostCancellationWithoutPairedAttack", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(3, 1, { 700, 100, 0 }, CombatStyle::Ranged),
    });
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 1;
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 3, 1, true, false, false, { 700, 100, 0 } },
    };
    state.attacks.attacks.push_back(projectile);

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(result.attackEvents.size() == 2);
    REQUIRE(result.frame.gameplayEvents.size() == 2);
    REQUIRE(result.frame.presentationEvents.size() == result.movement.events.size() + result.attackEvents.size());

    CHECK(result.attackEvents[1].type == BattleAttackEventType::TargetLost);
    CHECK(result.frame.gameplayEvents[1].type == BattleGameplayEventType::ProjectileCancelled);
    CHECK(result.frame.gameplayEvents[1].effectId == 10);
    CHECK(result.frame.gameplayEvents[1].targetUnitId == -1);
    CHECK(result.frame.gameplayEvents[1].otherAttackId == -1);
    CHECK(result.frame.presentationEvents[result.movement.events.size() + 1].type == BattlePresentationEventType::ProjectileTargetLost);
    CHECK(result.frame.presentationEvents[result.movement.events.size() + 1].amount == -1);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsProjectileCancelPairWithOtherAttackId", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 900, 900, 0 }, CombatStyle::Ranged),
    });
    state.attacks = attackWorld();

    BattleAttackInstance first;
    first.id = 10;
    first.state.attackerUnitId = 1;
    first.frame = 5;
    first.state.totalFrame = 30;
    first.state.position = { 500, 500, 0 };
    first.state.operationType = BattleOperationType::TrackingProjectile;
    first.state.projectileCancelDamage = 11;

    BattleAttackInstance second;
    second.id = 20;
    second.state.attackerUnitId = 2;
    second.frame = 5;
    second.state.totalFrame = 30;
    second.state.position = { 500, 500, 0 };
    second.state.operationType = BattleOperationType::RangedProjectile;
    second.state.projectileCancelDamage = 10;

    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 900, 900, 0 } },
    };
    state.attacks.attacks.push_back(first);
    state.attacks.attacks.push_back(second);

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(result.attackEvents.size() == 3);
    REQUIRE(result.frame.gameplayEvents.size() == 3);
    CHECK(result.attackEvents[2].type == BattleAttackEventType::ProjectileCancel);
    CHECK(result.attackEvents[2].sourceUnitId == 1);
    CHECK(result.attackEvents[2].otherSourceUnitId == 2);
    CHECK(result.attackEvents[2].projectileCancelDamage == 17);
    CHECK(result.attackEvents[2].otherProjectileCancelDamage == 10);
    CHECK(result.frame.gameplayEvents[2].type == BattleGameplayEventType::ProjectileCancelled);
    CHECK(result.frame.gameplayEvents[2].effectId == 10);
    CHECK(result.frame.gameplayEvents[2].otherAttackId == 20);
    CHECK(result.frame.gameplayEvents[2].amount == 0);
    CHECK(result.frame.presentationEvents[result.movement.events.size() + 2].type == BattlePresentationEventType::ProjectileCancelled);
    CHECK(result.frame.presentationEvents[result.movement.events.size() + 2].amount == 20);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsBounceAsAttackSpawnedGameplay", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 105, 100, 0 }),
        unit(3, 1, { 180, 100, 0 }),
    });
    state.attacks = attackWorld();

    BattleAttackInstance projectile;
    projectile.id = 10;
    projectile.state.attackerUnitId = 1;
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.totalFrame = 30;
    projectile.state.visualEffectId = 44;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.bounceRemaining = 1;
    projectile.state.bounceRange = 500;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    state.attacks.nextAttackId = 30;
    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 105, 100, 0 } },
        { 3, 1, true, false, false, { 180, 100, 0 } },
    };
    state.attacks.attacks.push_back(projectile);

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(result.attackEvents.size() == 3);
    REQUIRE(result.frame.gameplayEvents.size() == 3);
    REQUIRE(result.frame.presentationEvents.size() == result.movement.events.size() + result.attackEvents.size() + 1);
    CHECK(result.attackEvents[2].type == BattleAttackEventType::Bounce);
    CHECK(result.frame.gameplayEvents[2].type == BattleGameplayEventType::AttackSpawned);
    CHECK(result.frame.gameplayEvents[2].effectId == 30);
    CHECK(result.frame.gameplayEvents[2].sourceUnitId == 1);
    CHECK(result.frame.gameplayEvents[2].targetUnitId == 3);
    CHECK(result.frame.presentationEvents[result.movement.events.size() + 2].type == BattlePresentationEventType::ProjectileBounced);
    CHECK(result.frame.presentationEvents[result.movement.events.size() + 2].effectId == 10);
    CHECK(result.frame.presentationEvents[result.movement.events.size() + 2].amount == 30);
    const auto& spawnEvent = result.frame.presentationEvents[result.movement.events.size() + 3];
    CHECK(spawnEvent.type == BattlePresentationEventType::ProjectileSpawned);
    CHECK(spawnEvent.effectId == 30);
    CHECK(spawnEvent.sourceUnitId == 1);
    CHECK(spawnEvent.targetUnitId == 3);
    CHECK(spawnEvent.durationFrames >= 20);
    CHECK(spawnEvent.visualEffectId == 44);
    CHECK(spawnEvent.position.x != 0.0f);
    CHECK(spawnEvent.velocity.x > 0.0f);
    CHECK(spawnEvent.operationKind == 2);
}
