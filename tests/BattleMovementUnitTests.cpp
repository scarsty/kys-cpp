#include "battle/BattleMovement.h"
#include "BattleMovementTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <vector>

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;

TEST_CASE("BattleMovementGeometryAndConfig_MaxRangedReachStartsEmptyUntilSupplied", "[battle][core]")
{
    BattleMovementGeometry geometry;
    BattleMovementConfig config;

    CHECK(geometry.maxRangedReach == 0.0);
    CHECK(config.maxRangedReach == 0.0);
}

TEST_CASE("BattleMovement_MovementConfig_DerivesSharedGeometry", "[battle][core]")
{
    auto config = testConfig();
    CHECK(config.engagementDeadband == 18.0);
    CHECK(config.engagementArriveDistance == 27.0);
    CHECK(config.meleeAttackReach == 99.0);
    CHECK(config.meleeLocalTargetRadius == 171.0);
    CHECK(config.bodyRadius == 54.0);
    CHECK(config.movementDashDistanceMultiplier == 2.0);
}

TEST_CASE("BattleMovement_ProbeMove_SeparatesWallsUnitsAndReservations", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 160, 100, 0 }),
        unit(3, 1, { 300, 100, 0 }),
    });
    world.terrainCells = {
        { { -5, 100, 0 }, false },
        { { 135, 135, 0 }, true },
        { { 150, 100, 0 }, true },
        { { 290, 100, 0 }, true },
    };

    BattleMovementPlanner planner(world);
    CHECK(planner.probeMove(world.units[0], { -5, 100, 0 }, false).reason == MoveBlockReason::Wall);
    CHECK(planner.probeMove(world.units[0], { 150, 100, 0 }, false).reason == MoveBlockReason::Ally);
    CHECK(planner.probeMove(world.units[0], { 290, 100, 0 }, false).reason == MoveBlockReason::Enemy);

    std::map<int, Pointf> reservations = { { 2, { 130, 130, 0 } } };
    CHECK(planner.probeMove(world.units[0], { 135, 135, 0 }, false, reservations).reason == MoveBlockReason::Reservation);
    CHECK(planner.probeMove(world.units[0], { 150, 100, 0 }, true).canMove);
}

TEST_CASE("BattleMovement_AttackReady_HoldsWhenAlreadyInRange", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 190, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::AttackReady);
    CHECK(world.units[0].position.x == 100.0f);
    CHECK(world.units[0].velocity.norm() == 0.0f);
}

TEST_CASE("BattleMovement_Skirmish_OnlyStepsInSingletonTeamFights", "[battle][core][skirmish]")
{
    auto world = worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 190, 100, 0 }),
    });
    world.units[0].canAttack = false;
    world.units[0].canRepositionDuringCooldown = true;
    bool shouldStep = true;
    SECTION("duel") {}
    SECTION("one against many") { world.units.push_back(unit(2, 1, { 400, 400, 0 })); }
    SECTION("many against one") { world.units.push_back(unit(2, 0, { 400, 400, 0 })); }
    SECTION("full teams")
    {
        world.units.push_back(unit(2, 0, { 400, 400, 0 }));
        world.units.push_back(unit(3, 1, { 500, 400, 0 }));
        shouldStep = false;
    }
    SECTION("dead ally does not prevent duel")
    {
        world.units.push_back(unit(2, 0, { 400, 400, 0 }));
        world.units.back().alive = false;
    }
    const auto decision = BattleMovementPlanner(world).tick().decisions.at(0);
    CHECK(decision.action == (shouldStep ? MovementAction::Move : MovementAction::AttackReady));
    if (shouldStep)
    {
        CHECK(decision.destination.x < world.units[0].position.x);
        CHECK(decision.destination.y != world.units[0].position.y);
        CHECK(pointDistance(decision.destination, world.units[0].position) <= world.units[0].speed + 0.001);
        CHECK(decision.dashFramesRemaining == 0);
    }
}

TEST_CASE("BattleMovement_Skirmish_PreservesAttackAndMovementPriorities", "[battle][core][skirmish]")
{
    auto world = worldWith({ unit(0, 0, { 100, 100, 0 }), unit(1, 1, { 190, 100, 0 }) });
    world.units[0].canAttack = false;
    world.units[0].canRepositionDuringCooldown = true;
    SECTION("attack ready") { world.units[0].canAttack = true; }
    SECTION("busy or incapacitated") { world.units[0].canRepositionDuringCooldown = false; }
    SECTION("stationary") { world.units[0].speed = 0; }
    SECTION("airborne") { world.units[0].position.z = 1; }
    SECTION("taxue") { world.units[0].taXue = true; }
    SECTION("knockback") { world.units[0].knockbackFramesRemaining = 3; }
    SECTION("knockback control") { world.units[0].knockbackControlFramesRemaining = 3; }
    SECTION("post dash retreat") { world.units[0].postDashRetreatFramesRemaining = 3; }
    const auto decision = BattleMovementPlanner(world).tick().decisions.at(0);
    CHECK(decision.action != MovementAction::Move);
    CHECK(decision.destination.x == world.units[0].position.x);
    CHECK(decision.destination.y == world.units[0].position.y);
}

TEST_CASE("BattleMovement_Skirmish_BlockedRetreatHoldsInsteadOfDashing", "[battle][core][skirmish]")
{
    auto world = worldWith({ unit(0, 0, { 100, 100, 0 }), unit(1, 1, { 190, 100, 0 }) });
    world.units[0].canAttack = false;
    world.units[0].canRepositionDuringCooldown = true;
    SECTION("walls")
    {
        world.terrainCells = {
            { { 100, 100, 0 }, true },
            { { 96, 96, 0 }, false },
            { { 100, 95, 0 }, false },
            { { 100, 105, 0 }, false },
            { { 190, 100, 0 }, true },
        };
    }
    SECTION("units")
    {
        for (int index = 0; index < 8; ++index)
        {
            world.units.push_back(unit(index + 2, 1,
                Pointf{ 100, 100, 0 } + rotateBattlePoint({ 55, 0, 0 }, BattlePi * index / 4.0)));
            world.units.back().speed = 0;
        }
    }
    const auto decision = BattleMovementPlanner(world).tick().decisions.at(0);
    CHECK(decision.action == MovementAction::AttackReady);
    CHECK(decision.destination.x == 100);
    CHECK(decision.destination.y == 100);
}

TEST_CASE("BattleMovement_Skirmish_ProducesBoundedPursuitWithPauses", "[battle][core][skirmish]")
{
    auto world = worldWith({ unit(0, 0, { 100, 100, 0 }), unit(1, 1, { 190, 100, 0 }) });
    world.units[0].canAttack = false;
    world.units[0].canRepositionDuringCooldown = true;
    int retreatFrames = 0;
    int pursuitFrames = 0;
    int holdFrames = 0;
    for (int frame = 0; frame < 216; ++frame)
    {
        const auto result = BattleMovementPlanner(world).tick();
        const auto repeated = BattleMovementPlanner(world).tick();
        CHECK(result.decisions.at(0).destination.x == repeated.decisions.at(0).destination.x);
        CHECK(result.decisions.at(0).destination.y == repeated.decisions.at(0).destination.y);
        const auto& step = result.decisions.at(0);
        if (step.action == MovementAction::Move) ++retreatFrames;
        if (step.action == MovementAction::AttackReady) ++holdFrames;
        if (result.decisions.at(1).action == MovementAction::Move) ++pursuitFrames;
        CHECK(step.action != MovementAction::Dash);
        applyMovementTickToPlanWorld(world, result);
        CHECK(battleDistance2d(world.units[0].position, world.units[1].position)
            <= world.config.meleeAttackReach + world.config.tileWidth);
    }
    CHECK(retreatFrames > 0);
    CHECK(pursuitFrames > 0);
    CHECK(holdFrames > retreatFrames * 3);
    world.units[0].canAttack = true;
    world.units[1].position = world.units[0].position + Pointf{ 90, 0, 0 };
    CHECK(BattleMovementPlanner(world).tick().decisions.at(0).action == MovementAction::AttackReady);
}

TEST_CASE("BattleMovement_Skirmish_RangedStepStaysWithinAttackRange", "[battle][core][skirmish]")
{
    auto world = worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 490, 100, 0 }),
    });
    world.units[0].canAttack = false;
    world.units[0].canRepositionDuringCooldown = true;
    world.units[1].speed = 0;
    auto run = runMovementPlanForFrames(world, 216);
    CHECK(run.world.units[0].position.y != world.units[0].position.y);
    CHECK(battleDistance2d(run.world.units[0].position, run.world.units[1].position) <= world.units[0].reach);
    CHECK(run.stats.at(0).dashCount == 0);
    CHECK(run.stats.at(0).attackReadyFrames > 150);
}

TEST_CASE("BattleMovement_PlannedDash_UsesSpeedScaledDistanceForNormalUnits", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 600, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::Dash);
    CHECK(decision.dashDistance == 50.0);
    CHECK(decision.dashFramesRemaining == world.config.dashFrames);
    CHECK(decision.dashCooldownRemaining == world.config.dashCooldownFrames);
}

TEST_CASE("BattleMovement_MovementStats_CountsDashStartsOnly", "[battle][core]")
{
    auto run = runMovementPlanForFrames(worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 600, 100, 0 }),
    }), 6);

    CHECK(run.stats.at(1).dashCount == 1);
    CHECK(run.stats.at(1).lastDashDistance == 50.0);
}

TEST_CASE("BattleMovement_PlannedDash_IgnoresUnitsButRespectsTerrainSegment", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 130, 100, 0 }),
        unit(3, 1, { 600, 100, 0 }),
    });
    world.units[1].speed = 0.0;

    auto openResult = BattleMovementPlanner(world).tick();
    CHECK(openResult.decisions.at(1).action == MovementAction::Dash);

    auto blockedWorld = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 130, 100, 0 }),
        unit(3, 1, { 600, 100, 0 }),
    });
    blockedWorld.units[1].speed = 0.0;
    blockedWorld.terrainCells = {
        { { 100, 100, 0 }, true },
        { { 125, 100, 0 }, false },
        { { 150, 100, 0 }, false },
        { { 180, 100, 0 }, true },
    };

    auto blockedResult = BattleMovementPlanner(blockedWorld).tick();
    CHECK(blockedResult.decisions.at(1).action != MovementAction::Dash);
}

TEST_CASE("BattleMovement_TaXue_AllowsLongDash", "[battle][core]")
{
    auto chaser = unit(1, 0, { 100, 100, 0 });
    chaser.taXue = true;
    auto world = worldWith({
        chaser,
        unit(2, 1, { 600, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::Dash);
    CHECK(decision.dashDistance == world.config.maxDashDistance);
}

TEST_CASE("BattleMovement_RangedHold_DoesNotBackIntoOccupiedRingWhenInRange", "[battle][core]")
{
    auto ranged = unit(1, 0, { 100, 100, 0 }, CombatStyle::Ranged);
    ranged.canAttack = false;
    auto world = worldWith({
        ranged,
        unit(2, 0, { 45, 100, 0 }),
        unit(3, 1, { 300, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    auto decision = result.decisions.at(1);
    CHECK(decision.action == MovementAction::AttackReady);
    CHECK(world.units[0].position.x == 100.0f);
    CHECK(world.units[0].velocity.norm() == 0.0f);
}

TEST_CASE("BattleMovement_MeleeOutsideReach_MovesInsteadOfReportingReady", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 1, { 260, 100, 0 }),
    });

    auto result = BattleMovementPlanner(world).tick();
    CHECK(result.decisions.at(1).action == MovementAction::Move);
}

TEST_CASE("BattleMovement_SlotSwitchCooldown_BoundsRepeatedReplans", "[battle][core]")
{
    auto world = worldWith({
        unit(1, 0, { 100, 100, 0 }),
        unit(2, 0, { 125, 100, 0 }),
        unit(3, 1, { 500, 100, 0 }),
    });
    world.units[0].dashCooldownRemaining = 999;
    world.units[0].speed = 0.0;

    auto firstRun = runMovementPlanForFrames(world, 1);
    CHECK((firstRun.stats.at(1).lastBlockReason == MoveBlockReason::Ally
        || firstRun.stats.at(1).lastBlockReason == MoveBlockReason::Reservation));
    CHECK(firstRun.world.units[0].slotSwitchCooldownRemaining > 0);
    const int slotAfterFirst = firstRun.world.units[0].assignedSlot;

    auto secondRun = runMovementPlanForFrames(firstRun.world, 1);
    CHECK(secondRun.world.units[0].assignedSlot == slotAfterFirst);
    CHECK(secondRun.world.units[0].slotSwitchCooldownRemaining < firstRun.world.units[0].slotSwitchCooldownRemaining);
}
