#include "battle/BattleCombatIntent.h"
#include "battle/BattleCore.h"
#include "battle/BattleMovement.h"
#include "battle/BattlePresentationPlayback.h"

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
    request.initial.operationType = 2;
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
    CHECK(planner.operationTypeForAttackArea(0) == 0);
    CHECK(planner.operationTypeForAttackArea(1) == 2);
    CHECK(planner.operationTypeForAttackArea(2) == 2);
    CHECK(planner.operationTypeForAttackArea(3) == 1);
    CHECK(planner.operationTypeForAttackArea(99) == -1);
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
    CHECK(intent.operationType == 0);

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
    CHECK(intent.operationType == 2);

    CombatIntentInput meleeDash = forcedRanged;
    meleeDash.dashAttackEnabled = true;
    meleeDash.plannedSkill = skill(0, 137.5, false);
    intent = planner.select(meleeDash);
    CHECK(intent.startAttack);
    CHECK(intent.operationType == 3);
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
    projectile.state.operationType = 2;
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

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsTargetLostCancellationWithoutPairedAttack", "[battle][core]")
{
    BattleFrameState state;
    state.world = worldWith({
        unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged),
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

    BattleAttackInstance second;
    second.id = 20;
    second.state.attackerUnitId = 2;
    second.frame = 5;
    second.state.totalFrame = 30;
    second.state.position = { 500, 500, 0 };

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
    projectile.state.operationType = 2;
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
