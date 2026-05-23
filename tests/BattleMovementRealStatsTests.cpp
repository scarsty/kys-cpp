#include "battle/BattleMovement.h"
#include "BattleMovementTestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;

namespace
{

constexpr double SceneTileWidth = 36.0;
constexpr double MaxEffectiveBattleReach = 480.0;

struct PinnedRoleMovement
{
    int roleId = -1;
    const char* name = "";
    double speed = 4.0;
    double reach = 137.5;
    CombatStyle style = CombatStyle::Melee;
    bool taXue = false;
};

const std::map<int, PinnedRoleMovement>& pinnedRoles()
{
    // 這些數值是目前遊戲資料的移動測試快照；測試不可再讀取 game.db 或 YAML。
    static const std::map<int, PinnedRoleMovement> roles = {
        { 1, { 1, "胡斐", 4.55, 137.5, CombatStyle::Melee, true } },
        { 4, { 4, "閻基", 4.10, 425.0, CombatStyle::Ranged, false } },
        { 5, { 5, "張三豐", 4.00, 137.5, CombatStyle::Melee, false } },
        { 29, { 29, "令狐沖", 4.35, 137.5, CombatStyle::Melee, false } },
        { 72, { 72, "田伯光", 4.30, 137.5, CombatStyle::Melee, false } },
        { 97, { 97, "楊過", 4.25, 137.5, CombatStyle::Melee, false } },
        { 114, { 114, "掃地老僧", 4.15, 137.5, CombatStyle::Melee, false } },
        { 116, { 116, "金輪法王", 4.05, 137.5, CombatStyle::Melee, false } },
        { 118, { 118, "鳩摩智", 4.00, 137.5, CombatStyle::Melee, false } },
        { 589, { 589, "胡一刀", 4.50, 137.5, CombatStyle::Melee, true } },
    };
    return roles;
}

struct PinnedUnitSpec
{
    int roleId = -1;
    int team = 0;
    Pointf position;
};

BattleMovementConfig pinnedConfig()
{
    BattleMovementGeometry geometry;
    geometry.tileWidth = SceneTileWidth;
    geometry.meleeAttackEffectOffset = geometry.tileWidth * 2.0;
    geometry.meleeAttackHitRadius = geometry.tileWidth * 2.0;
    geometry.maxRangedReach = MaxEffectiveBattleReach;
    return BattleGeometry(geometry).movementConfig();
}

std::vector<BattleTerrainCell> terrainRectangle(double minX, double minY, double maxX, double maxY)
{
    std::vector<BattleTerrainCell> cells;
    for (double x = minX; x <= maxX; x += SceneTileWidth)
    {
        for (double y = minY; y <= maxY; y += SceneTileWidth)
        {
            cells.push_back({ { static_cast<float>(x), static_cast<float>(y), 0 }, true });
        }
    }
    return cells;
}

std::vector<BattleTerrainCell> terrainGridWithVerticalWallGap(
    int coordCount,
    int wallX,
    int gapY)
{
    std::vector<BattleTerrainCell> cells;
    cells.reserve(static_cast<std::size_t>(coordCount * coordCount));
    for (int x = 0; x < coordCount; ++x)
    {
        for (int y = 0; y < coordCount; ++y)
        {
            const bool wall = x == wallX && y != gapY;
            cells.push_back({
                {
                    static_cast<float>(x * SceneTileWidth),
                    static_cast<float>(y * SceneTileWidth),
                    0.0f,
                },
                !wall,
            });
        }
    }
    return cells;
}

BattleMovementPlanInput makeWorld(const std::vector<PinnedUnitSpec>& specs,
                           std::vector<BattleTerrainCell> terrain = {})
{
    BattleMovementPlanInput world;
    world.config = pinnedConfig();
    world.terrainCells = std::move(terrain);

    for (size_t i = 0; i < specs.size(); ++i)
    {
        const auto& spec = specs[i];
        auto pinned = pinnedRoles().find(spec.roleId);
        REQUIRE(pinned != pinnedRoles().end());

        BattleUnitState unit;
        unit.id = static_cast<int>(i + 1);
        unit.team = spec.team;
        unit.position = spec.position;
        unit.speed = pinned->second.speed;
        unit.reach = pinned->second.reach;
        unit.style = pinned->second.style;
        unit.taXue = pinned->second.taXue;
        world.units.push_back(unit);
    }
    return world;
}

int countEvents(const MovementRunResult& run, int unitId, BattleEventType type)
{
    return static_cast<int>(std::count_if(run.events.begin(), run.events.end(), [&](const BattleEvent& event)
        {
            return event.unitId == unitId && event.type == type;
        }));
}

}  // namespace

TEST_CASE("YanJi_AllyBlock_RecoversBeforeStall", "[battle][movement]")
{
    auto world = makeWorld({
        { 4, 0, { 100, 100, 0 } },
        { 72, 0, { 160, 100, 0 } },
        { 116, 1, { 520, 100, 0 } },
    });

    auto run = runMovementPlanForFrames(world, 240);
    auto stats = run.stats.at(1);
    CHECK(stats.consecutiveNoProgressFrames < 45);
    CHECK(stats.consecutiveAllyBlockedFrames < 12);
    CHECK((stats.dashCount > 0 || stats.slotSwitches > 0 || stats.attackReadyFrames > 0));
}

TEST_CASE("BattleMovementPhysicsSystem_SlidesAndTicksDashRuntime", "[battle][movement]")
{
    BattleMovementPhysicsInput input;
    input.state.position = { 10, 10, 0 };
    input.state.velocity = { 5, 2, 0 };
    input.state.movementDashFrames = 1;
    input.state.movementDashCooldown = 3;
    input.config.gravity = -4.0f;
    input.config.friction = 0.1f;
    input.config.postDashSpreadFrames = 12;
    input.actionDashActive = true;
    BattleMovementPhysicsCollisionWorld collision;
    collision.tileWidth = 1.0;
    collision.coordCount = 10;
    collision.defaultSeparationDistance = 4.0;
    collision.units = { { 0, true, input.state.position } };
    collision.walkableByCell.assign(10 * 10, 1);
    collision.walkableByCell[movementPhysicsCellIndex(collision, 8, 4)] = 0;
    collision.walkableByCell[movementPhysicsCellIndex(collision, 9, 4)] = 0;
    collision.walkableByCell[movementPhysicsCellIndex(collision, 6, 6)] = 0;
    input.collisionWorld = &collision;
    input.unitId = 0;
    input.currentPosition = input.state.position;

    auto state = BattleMovementPhysicsSystem().advance(input);

    CHECK(state.position.x == 15.0f);
    CHECK(state.position.y == 10.0f);
    CHECK(state.velocity.x == Catch::Approx(4.9));
    CHECK(state.velocity.y == 0.0f);
    CHECK(state.velocity.z == 0.0f);
    CHECK(state.acceleration.x == Catch::Approx(-0.1));
    CHECK(state.acceleration.y == 0.0f);
    CHECK(state.acceleration.z == -4.0f);
    CHECK(state.movementDashFrames == 0);
    CHECK(state.movementDashSpreadFrames == 12);
    CHECK(state.movementDashCooldown == 2);
}

TEST_CASE("SaoDi_MeleeJitter_DoesNotIdleHundredsOfFrames", "[battle][movement]")
{
    auto world = makeWorld({
        { 114, 0, { 100, 100, 0 } },
        { 5, 0, { 155, 110, 0 } },
        { 116, 1, { 500, 115, 0 } },
        { 118, 1, { 560, 140, 0 } },
    });

    auto run = runMovementPlanForFrames(world, 300);
    auto stats = run.stats.at(1);
    CHECK(stats.consecutiveNoProgressFrames < 60);
    CHECK(stats.directionReversalCount < 30);
    CHECK(stats.slotSwitches < 20);
    CHECK(stats.attackReadyFrames > 0);
}

TEST_CASE("HuFei_HuYiDao_TaXue_DashAttackStillHappens", "[battle][movement]")
{
    auto world = makeWorld({
        { 1, 0, { 100, 100, 0 } },
        { 589, 0, { 110, 170, 0 } },
        { 116, 1, { 650, 120, 0 } },
    });

    auto run = runMovementPlanForFrames(world, 260);
    CHECK((run.stats.at(1).dashCount + run.stats.at(2).dashCount) > 0);
    CHECK((run.stats.at(1).attackReadyFrames + run.stats.at(2).attackReadyFrames) > 0);
}

TEST_CASE("TaXue_DashDoesNotForceMaximumDistance", "[battle][movement]")
{
    auto world = makeWorld({
        { 1, 0, { 100, 100, 0 } },
        { 116, 1, { 260, 100, 0 } },
    });

    auto tick = BattleMovementPlanner(world).tick();

    const auto dash = std::find_if(
        tick.events.begin(),
        tick.events.end(),
        [](const BattleEvent& event)
        {
            return event.unitId == 1 && event.type == BattleEventType::DashStart;
        });
    REQUIRE(dash != tick.events.end());
    CHECK(dash->value < world.config.maxDashDistance);
}

TEST_CASE("BattleMovementPlanner_TickReturnsDecisionsWithoutMutatingCallerWorld", "[battle][movement]")
{
    auto world = makeWorld({
        { 97, 0, { 100, 100, 0 } },
        { 116, 1, { 300, 100, 0 } },
    });
    world.config.reservationHorizonFrames = 3;
    world.units[0].dashCooldownRemaining = 999;

    const auto originalPosition = world.units[0].position;
    const auto originalVelocity = world.units[0].velocity;
    auto tick = BattleMovementPlanner(world).tick();

    REQUIRE(tick.decisions.contains(1));
    CHECK(tick.decisions.at(1).action == MovementAction::Move);
    CHECK(tick.decisions.at(1).destination.x > originalPosition.x);
    CHECK(world.units[0].position.x == originalPosition.x);
    CHECK(world.units[0].velocity.x == originalVelocity.x);
    CHECK(world.movementReservations.empty());
}

TEST_CASE("TaXue_MeleeChaosStartsImmediatePeelDash", "[battle][movement]")
{
    auto world = makeWorld({
        { 1, 0, { 100, 100, 0 } },
        { 116, 1, { 220, 100, 0 } },
    });
    world.units[0].postDashChaosFramesRemaining = world.config.dashFrames + 1;

    auto tick = BattleMovementPlanner(world).tick();

    const auto dash = std::find_if(
        tick.events.begin(),
        tick.events.end(),
        [](const BattleEvent& event)
        {
            return event.unitId == 1 && event.type == BattleEventType::DashStart;
        });
    REQUIRE(dash != tick.events.end());
    REQUIRE(tick.decisions.contains(1));
    CHECK(tick.decisions.at(1).action == MovementAction::Dash);
    CHECK(dash->to.x < dash->from.x);
    CHECK(std::abs(dash->to.y - dash->from.y) > 0.1);
    CHECK(tick.decisions.at(1).postDashChaosFramesRemaining == 0);
    CHECK(tick.decisions.at(1).dashCooldownRemaining == world.config.dashCooldownFrames);
}

TEST_CASE("TaXue_RangedPeelsWhenCrowded", "[battle][movement]")
{
    auto world = makeWorld({
        { 4, 0, { 100, 100, 0 } },
        { 116, 1, { 300, 100, 0 } },
    });
    world.units[0].taXue = true;

    auto tick = BattleMovementPlanner(world).tick();

    const auto dash = std::find_if(
        tick.events.begin(),
        tick.events.end(),
        [](const BattleEvent& event)
        {
            return event.unitId == 1 && event.type == BattleEventType::DashStart;
        });
    REQUIRE(dash != tick.events.end());
    CHECK(dash->to.x < dash->from.x);
    REQUIRE(tick.decisions.contains(1));
    CHECK(tick.decisions.at(1).action == MovementAction::Dash);
    CHECK(tick.decisions.at(1).dashCooldownRemaining == world.config.dashCooldownFrames);
}

TEST_CASE("MeleeSwarm_DoesNotReserveSameApproachSlot", "[battle][movement]")
{
    auto world = makeWorld({
        { 97, 0, { 100, 100, 0 } },
        { 29, 0, { 100, 160, 0 } },
        { 589, 0, { 80, 220, 0 } },
        { 1, 0, { 120, 250, 0 } },
        { 116, 1, { 520, 150, 0 } },
    });

    auto run = runMovementPlanForFrames(world, 260);
    for (int id = 1; id <= 4; ++id)
    {
        CHECK(run.stats.at(id).consecutiveAllyBlockedFrames < 15);
        CHECK(run.stats.at(id).consecutiveNoProgressFrames < 70);
    }
}

TEST_CASE("ReservationHorizon_AvoidsSoftReservedStepWhenAlternativeExists", "[battle][movement]")
{
    auto world = makeWorld({
        { 97, 0, { 100, 100, 0 } },
        { 29, 0, { 700, 700, 0 } },
        { 116, 1, { 300, 100, 0 } },
    });
    world.config.reservationHorizonFrames = 3;
    world.units[0].dashCooldownRemaining = 999;
    world.units[1].canAttack = false;
    world.movementReservations[2] = BattleMovementReservation{
        2,
        { 104.25f, 100.0f, 0.0f },
        2.0,
        world.frame + world.config.reservationHorizonFrames,
    };

    auto tick = BattleMovementPlanner(world).tick();

    CHECK(tick.decisions.at(1).destination.x > 100.0f);
    CHECK(tick.decisions.at(1).destination.y != Catch::Approx(100.0f));
    REQUIRE(tick.movementReservations.contains(1));
    CHECK(tick.movementReservations.at(1).expiresFrame == tick.frame + world.config.reservationHorizonFrames);
}

TEST_CASE("ReservationHorizon_SoftReservationsFallbackInsteadOfDeadlocking", "[battle][movement]")
{
    auto world = makeWorld({
        { 97, 0, { 100, 100, 0 } },
        { 29, 0, { 700, 700, 0 } },
        { 116, 1, { 300, 100, 0 } },
    });
    world.config.reservationHorizonFrames = 3;
    world.units[0].dashCooldownRemaining = 999;
    world.units[1].canAttack = false;
    world.movementReservations[2] = BattleMovementReservation{
        2,
        { 104.25f, 100.0f, 0.0f },
        500.0,
        world.frame + world.config.reservationHorizonFrames,
    };

    auto tick = BattleMovementPlanner(world).tick();

    CHECK(tick.decisions.at(1).action == MovementAction::Move);
    CHECK(tick.decisions.at(1).destination.x > 100.0f);
    CHECK(tick.decisions.at(1).destination.y == Catch::Approx(100.0f));
}

TEST_CASE("ReservationHorizon_PrunesExpiredAndClearsAttackReadyReservations", "[battle][movement]")
{
    auto world = makeWorld({
        { 97, 0, { 100, 100, 0 } },
        { 29, 0, { 700, 700, 0 } },
        { 116, 1, { 190, 100, 0 } },
    });
    world.config.reservationHorizonFrames = 3;
    world.frame = 10;
    world.units[1].alive = false;
    world.movementReservations[1] = BattleMovementReservation{ 1, { 120, 100, 0 }, 2.0, 20 };
    world.movementReservations[2] = BattleMovementReservation{ 2, { 700, 700, 0 }, 2.0, 9 };

    auto tick = BattleMovementPlanner(world).tick();

    CHECK_FALSE(tick.movementReservations.contains(1));
    CHECK_FALSE(tick.movementReservations.contains(2));
}

TEST_CASE("TaXue_UnstableIgnoresReservationsAndUnitBodiesButRespectsTerrain", "[battle][movement]")
{
    auto world = makeWorld({
        { 1, 0, { 100, 100, 0 } },
        { 116, 1, { 104.55f, 100, 0 } },
    });
    world.config.reservationHorizonFrames = 3;
    world.units[0].dashFramesRemaining = 2;
    world.units[0].velocity = { 4.55f, 0, 0 };
    world.movementReservations[2] = BattleMovementReservation{ 2, { 104.55f, 100, 0 }, 2.0, 3 };

    auto tick = BattleMovementPlanner(world).tick();

    CHECK(tick.decisions.at(1).destination.x == Catch::Approx(104.55f));
    CHECK_FALSE(tick.movementReservations.contains(1));

    auto blocked = makeWorld({
        { 1, 0, { 100, 100, 0 } },
        { 116, 1, { 104.55f, 100, 0 } },
    }, {
        { { 100, 100, 0 }, true },
        { { 104.55f, 100, 0 }, false },
    });
    blocked.units[0].dashFramesRemaining = 2;
    blocked.units[0].velocity = { 4.55f, 0, 0 };

    auto blockedTick = BattleMovementPlanner(blocked).tick();

    CHECK(blockedTick.decisions.at(1).destination.x == Catch::Approx(100.0f));
}

TEST_CASE("BattleMovementPhysicsSystem_CanIgnoreUnitCollisionButNeedsHeightToClearTerrain", "[battle][movement]")
{
    BattleMovementPhysicsCollisionWorld collision;
    collision.tileWidth = 1.0;
    collision.coordCount = 10;
    collision.defaultSeparationDistance = 4.0;
    collision.units = {
        { 1, true, { 10, 10, 0 } },
        { 2, true, { 15, 10, 0 } },
    };
    collision.walkableByCell.assign(10 * 10, 1);

    BattleMovementPhysicsInput input;
    input.state.position = { 10, 10, 0 };
    input.state.velocity = { 5, 0, 0 };
    input.config.gravity = 0.0f;
    input.config.friction = 0.0f;
    input.collisionWorld = &collision;
    input.unitId = 1;
    input.currentPosition = input.state.position;
    input.ignoreUnitCollision = true;

    auto state = BattleMovementPhysicsSystem().advance(input);

    CHECK(state.position.x == Catch::Approx(15.0f));

    collision.walkableByCell[movementPhysicsCellIndex(collision, 8, 3)] = 0;
    input.state.position = { 10, 10, 0 };
    input.state.velocity = { 5, 0, 0 };
    input.currentPosition = input.state.position;

    state = BattleMovementPhysicsSystem().advance(input);

    CHECK(state.position.x == Catch::Approx(10.0f));

    collision.walkableByCell.assign(10 * 10, 1);
    collision.walkableByCell[movementPhysicsCellIndex(collision, 7, 3)] = 0;
    input.state.position = { 10, 10, 2 };
    input.state.velocity = { 6, 0, 0 };
    input.currentPosition = input.state.position;

    state = BattleMovementPhysicsSystem().advance(input);

    CHECK(state.position.x == Catch::Approx(10.0f));

    input.state.position = { 10, 10, 3 };
    input.state.velocity = { 6, 0, 0 };
    input.currentPosition = input.state.position;

    state = BattleMovementPhysicsSystem().advance(input);

    CHECK(state.position.x == Catch::Approx(16.0f));
}

TEST_CASE("RangedRetarget_HoldsWhenAlreadyCanShoot", "[battle][movement]")
{
    auto world = makeWorld({
        { 4, 0, { 100, 100, 0 } },
        { 72, 0, { 160, 100, 0 } },
        { 116, 1, { 300, 100, 0 } },
        { 118, 1, { 340, 120, 0 } },
    });
    world.units[2].alive = false;

    auto run = runMovementPlanForFrames(world, 120);
    CHECK(run.stats.at(1).attackReadyFrames > 40);
    CHECK(run.stats.at(1).consecutiveAllyBlockedFrames < 8);
    CHECK(countEvents(run, 1, BattleEventType::BlockedByAlly) < 12);
}

TEST_CASE("CorneredRanged_NoWallBumpLoop", "[battle][movement]")
{
    auto world = makeWorld({
        { 4, 0, { 90, 90, 0 } },
        { 116, 1, { 280, 120, 0 } },
    }, terrainRectangle(80, 80, 700, 700));

    auto run = runMovementPlanForFrames(world, 160);
    CHECK(run.stats.at(1).consecutiveWallBlockedFrames < 10);
    CHECK(run.stats.at(1).attackReadyFrames > 30);
}

TEST_CASE("MeleePathing_RoutesAroundWallGapInsteadOfHolding", "[battle][movement]")
{
    auto world = makeWorld({
        { 116, 0, { 2.0f * SceneTileWidth, 4.0f * SceneTileWidth, 0 } },
        { 4, 1, { 8.0f * SceneTileWidth, 4.0f * SceneTileWidth, 0 } },
    }, terrainGridWithVerticalWallGap(12, 5, 9));
    world.units[0].reach = 60.0;
    world.units[1].speed = 0.0;

    auto run = runMovementPlanForFrames(world, 220);
    const auto& chaser = run.world.units[0];

    CHECK(chaser.position.x > 5.0f * SceneTileWidth);
    CHECK(run.stats.at(1).consecutiveWallBlockedFrames < 30);
    CHECK(run.stats.at(1).consecutiveNoProgressFrames < 60);
}

TEST_CASE("MeleeAcrossWall_HoldsAttackReadyBecauseMeleeHitsIgnoreTerrain", "[battle][movement]")
{
    auto world = makeWorld({
        { 116, 0, { 4.0f * SceneTileWidth, 4.0f * SceneTileWidth, 0 } },
        { 4, 1, { 6.0f * SceneTileWidth, 4.0f * SceneTileWidth, 0 } },
    }, terrainGridWithVerticalWallGap(12, 5, 9));
    world.units[1].speed = 0.0;

    auto run = runMovementPlanForFrames(world, 160);
    const auto& chaser = run.world.units[0];

    CHECK(chaser.position.y == Catch::Approx(4.0f * SceneTileWidth));
    CHECK(run.stats.at(1).attackReadyFrames == 160);
    CHECK(run.stats.at(1).consecutiveNoProgressFrames == 0);
}

TEST_CASE("RangedAcrossWall_HoldsAttackReadyBecauseProjectilesIgnoreTerrain", "[battle][movement]")
{
    auto world = makeWorld({
        { 4, 0, { 4.0f * SceneTileWidth, 4.0f * SceneTileWidth, 0 } },
        { 116, 1, { 6.0f * SceneTileWidth, 4.0f * SceneTileWidth, 0 } },
    }, terrainGridWithVerticalWallGap(12, 5, 9));
    world.units[1].speed = 0.0;

    auto run = runMovementPlanForFrames(world, 160);
    const auto& shooter = run.world.units[0];

    CHECK(shooter.position.y == Catch::Approx(4.0f * SceneTileWidth));
    CHECK(run.stats.at(1).attackReadyFrames == 160);
    CHECK(run.stats.at(1).consecutiveNoProgressFrames == 0);
}

TEST_CASE("Deterministic_ReplaySameWorldSameEvents", "[battle][movement]")
{
    auto make = []()
    {
        return makeWorld({
            { 4, 0, { 100, 100, 0 } },
            { 72, 0, { 160, 100, 0 } },
            { 116, 1, { 520, 100, 0 } },
        });
    };

    auto first = runMovementPlanForFrames(make(), 180);
    auto second = runMovementPlanForFrames(make(), 180);
    REQUIRE(first.events.size() == second.events.size());
    for (size_t i = 0; i < first.events.size(); ++i)
    {
        CHECK(first.events[i].type == second.events[i].type);
        CHECK(first.events[i].unitId == second.events[i].unitId);
        CHECK(first.events[i].targetId == second.events[i].targetId);
        CHECK(first.events[i].blockerId == second.events[i].blockerId);
        CHECK(first.events[i].slot == second.events[i].slot);
    }
}
