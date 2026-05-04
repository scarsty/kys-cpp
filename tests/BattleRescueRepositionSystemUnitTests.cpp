#include "battle/BattleRescueRepositionSystem.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{

BattleRescueUnitSnapshot unit(int id, int team, Point cell)
{
    BattleRescueUnitSnapshot snapshot;
    snapshot.id = id;
    snapshot.team = team;
    snapshot.alive = true;
    snapshot.hp = 20;
    snapshot.maxHp = 100;
    snapshot.cell = cell;
    return snapshot;
}

BattleRescueCellSnapshot cell(int x, int y, bool occupied = false)
{
    return { x, y, true, occupied, occupied ? 99 : -1 };
}

}  // namespace

TEST_CASE("BattleRescueReposition_ProtectionPullSelectsLegalDestination", "[battle][rescue_reposition][unit]")
{
    BattleRescueRepositionInput input;
    input.mode = BattleRescuePullMode::Protect;
    input.pulledUnitId = 10;
    input.pullerTeam = 1;
    input.units = {
        unit(10, 1, { 5, 5 }),
        unit(11, 1, { 2, 2 }),
        unit(20, 0, { 7, 7 }),
    };
    input.units[1].forcePullProtect = true;
    input.units[1].forcePullProtectRemaining = 1;
    input.cells = {
        cell(2, 3),
        cell(3, 2, true),
        cell(5, 5),
    };

    auto result = BattleRescueRepositionSystem().resolve(input);

    REQUIRE(result.teleport.has_value());
    CHECK(result.teleport->unitId == 10);
    CHECK(result.teleport->destinationCell.x == 2);
    CHECK(result.teleport->destinationCell.y == 3);
    CHECK(result.counterDelta.unitId == 11);
    CHECK(result.counterDelta.protectRemainingDelta == -1);
    CHECK(result.heal.targetUnitId == 10);
    CHECK(result.heal.amount == 10);
    CHECK(result.invincibility.targetUnitId == 10);
    CHECK(result.invincibility.frames == 10);
}

TEST_CASE("BattleRescueReposition_ExecutePullConsumesExecuteCounterAndRequestsCounterAttack", "[battle][rescue_reposition][unit]")
{
    BattleRescueRepositionInput input;
    input.mode = BattleRescuePullMode::Execute;
    input.pulledUnitId = 10;
    input.pullerTeam = 0;
    input.units = {
        unit(10, 1, { 5, 5 }),
        unit(20, 0, { 8, 8 }),
    };
    input.units[1].forcePullExecute = true;
    input.units[1].forcePullExecuteRemaining = 2;
    input.cells = {
        cell(7, 8),
        cell(8, 7),
        cell(8, 8, true),
    };

    auto result = BattleRescueRepositionSystem().resolve(input);

    REQUIRE(result.teleport.has_value());
    CHECK(result.teleport->unitId == 10);
    CHECK(result.counterDelta.unitId == 20);
    CHECK(result.counterDelta.executeRemainingDelta == -1);
    CHECK(result.basicCounterAttack.has_value());
    CHECK(result.basicCounterAttack->attackerUnitId == 20);
    CHECK(result.basicCounterAttack->targetUnitId == 10);
}

TEST_CASE("BattleRescueReposition_NoCommandWhenNoLegalCellExists", "[battle][rescue_reposition][unit]")
{
    BattleRescueRepositionInput input;
    input.mode = BattleRescuePullMode::Protect;
    input.pulledUnitId = 10;
    input.pullerTeam = 1;
    input.units = {
        unit(10, 1, { 5, 5 }),
        unit(11, 1, { 2, 2 }),
    };
    input.units[1].forcePullProtect = true;
    input.units[1].forcePullProtectRemaining = 1;
    input.cells = {
        { 2, 3, false, false, -1 },
        cell(5, 5),
    };

    auto result = BattleRescueRepositionSystem().resolve(input);

    CHECK_FALSE(result.teleport.has_value());
    CHECK_FALSE(result.basicCounterAttack.has_value());
    CHECK(result.counterDelta.unitId == -1);
}
