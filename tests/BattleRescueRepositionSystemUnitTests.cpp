#include "battle/BattleRescueRepositionSystem.h"
#include "BattleCoreTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;
using namespace KysChess;
using namespace BattlePresentationTest;

namespace
{

BattleRescueUnitSnapshot rescueUnit(int id, int team, Point cell)
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

BattleRescueCellSnapshot rescueCellLocal(int x, int y, bool occupied = false)
{
    return { x, y, true, occupied, occupied ? 99 : -1, { static_cast<float>(x * 10), static_cast<float>(y * 10), 0.0f } };
}

void appendOpenCells(BattleRescueRepositionInput& input, int width, int height)
{
    input.cells.reserve(width * height);
    for (int x = 0; x < width; ++x)
    {
        for (int y = 0; y < height; ++y)
        {
            input.cells.push_back(rescueCellLocal(x, y));
        }
    }
}

}  // namespace

TEST_CASE("BattleRescueReposition_ProtectionPullSelectsLegalDestination", "[battle][rescue_reposition][unit]")
{
    BattleRescueRepositionInput input;
    input.mode = BattleRescuePullMode::Protect;
    input.pulledUnitId = 10;
    input.pullerTeam = 1;
    input.units = {
        rescueUnit(10, 1, { 5, 5 }),
        rescueUnit(11, 1, { 2, 2 }),
        rescueUnit(20, 0, { 7, 7 }),
    };
    input.units[1].forcePullProtect = true;
    input.units[1].forcePullProtectRemaining = 1;
    input.cells = {
        rescueCellLocal(2, 2, true),
        rescueCellLocal(2, 3),
        rescueCellLocal(3, 2, true),
        rescueCellLocal(5, 5),
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

TEST_CASE("BattleRescueReposition_ProtectionHealUsesCommonMinimumAndFullHpRules", "[battle][rescue_reposition][heal][unit]")
{
    BattleRescueRepositionInput input;
    input.mode = BattleRescuePullMode::Protect;
    input.pulledUnitId = 10;
    input.pullerTeam = 1;
    input.units = {
        rescueUnit(10, 1, { 5, 5 }),
        rescueUnit(11, 1, { 2, 2 }),
    };
    input.units[1].forcePullProtect = true;
    input.units[1].forcePullProtectRemaining = 1;
    input.cells = {
        rescueCellLocal(2, 2, true),
        rescueCellLocal(2, 3),
        rescueCellLocal(5, 5),
    };

    SECTION("minimum heal survives percentage truncation")
    {
        input.units[0].hp = 0;
        input.units[0].maxHp = 1;

        const auto result = BattleRescueRepositionSystem().resolve(input);

        REQUIRE(result.teleport.has_value());
        CHECK(result.heal.amount == 1);
        REQUIRE_FALSE(result.logEvents.empty());
        CHECK(result.logEvents.front().type == BattleLogEventType::Heal);
    }

    SECTION("full target has no applied heal event")
    {
        input.units[0].hp = input.units[0].maxHp;

        const auto result = BattleRescueRepositionSystem().resolve(input);

        REQUIRE(result.teleport.has_value());
        CHECK(result.heal.amount == 0);
        for (const auto& event : result.logEvents)
        {
            CHECK(event.type != BattleLogEventType::Heal);
        }
    }

    SECTION("寒毒阻止救援治療但不阻止位移與無敵")
    {
        input.units[0].healModifiers.blocked = true;

        const auto result = BattleRescueRepositionSystem().resolve(input);

        REQUIRE(result.teleport.has_value());
        CHECK(result.heal.amount == 0);
        CHECK(result.invincibility.frames == 10);
        for (const auto& event : result.logEvents)
        {
            CHECK(event.type != BattleLogEventType::Heal);
        }
    }
}

TEST_CASE("BattleRescueReposition_ProtectionPullSkipsDisconnectedWalkableCells", "[battle][rescue_reposition][unit]")
{
    BattleRescueRepositionInput input;
    input.mode = BattleRescuePullMode::Protect;
    input.pulledUnitId = 10;
    input.pullerTeam = 1;
    input.units = {
        rescueUnit(10, 1, { 5, 5 }),
        rescueUnit(11, 1, { 2, 2 }),
        rescueUnit(20, 0, { 2, 4 }),
    };
    input.units[1].forcePullProtect = true;
    input.units[1].forcePullProtectRemaining = 1;
    input.cells = {
        rescueCellLocal(2, 1),
        rescueCellLocal(2, 2, true),
        rescueCellLocal(2, 3),
        rescueCellLocal(3, 1),
        rescueCellLocal(3, 2),
        rescueCellLocal(3, 3),
        rescueCellLocal(7, 7),
    };

    auto result = BattleRescueRepositionSystem().resolve(input);

    REQUIRE(result.teleport.has_value());
    const bool choseDisconnectedCell = result.teleport->destinationCell.x == 7
        && result.teleport->destinationCell.y == 7;
    CHECK_FALSE(choseDisconnectedCell);
}

TEST_CASE("BattleRescueReposition_ProtectionPullAllowsLivePullerWhenAnotherMemberIsDead", "[battle][rescue_reposition][unit]")
{
    BattleRescueRepositionInput input;
    input.mode = BattleRescuePullMode::Protect;
    input.pulledUnitId = 10;
    input.pullerTeam = 1;
    input.units = {
        rescueUnit(10, 1, { 5, 5 }),
        rescueUnit(11, 1, { 2, 2 }),
        rescueUnit(12, 1, { 3, 2 }),
    };
    input.units[1].forcePullProtect = true;
    input.units[1].forcePullProtectRemaining = 1;
    input.units[2].alive = false;
    input.units[2].forcePullProtect = true;
    input.units[2].forcePullProtectRemaining = 1;
    input.cells = {
        rescueCellLocal(2, 2, true),
        rescueCellLocal(2, 3),
        rescueCellLocal(3, 2),
        rescueCellLocal(5, 5),
    };

    auto result = BattleRescueRepositionSystem().resolve(input);

    REQUIRE(result.teleport.has_value());
    CHECK(result.teleport->pullerUnitId == 11);
    CHECK(result.counterDelta.unitId == 11);
}

TEST_CASE("BattleRescueReposition_ExecutePullConsumesExecuteCounterAndRequestsCounterAttack", "[battle][rescue_reposition][unit]")
{
    BattleRescueRepositionInput input;
    input.mode = BattleRescuePullMode::Execute;
    input.pulledUnitId = 10;
    input.pullerTeam = 0;
    input.units = {
        rescueUnit(10, 1, { 5, 5 }),
        rescueUnit(20, 0, { 8, 8 }),
    };
    input.units[1].forcePullExecute = true;
    input.units[1].forcePullExecuteRemaining = 2;
    input.cells = {
        rescueCellLocal(7, 8),
        rescueCellLocal(8, 7),
        rescueCellLocal(8, 8, true),
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
        rescueUnit(10, 1, { 5, 5 }),
        rescueUnit(11, 1, { 2, 2 }),
    };
    input.units[1].forcePullProtect = true;
    input.units[1].forcePullProtectRemaining = 1;
    input.cells = {
        rescueCellLocal(2, 2, true),
        { 2, 3, false, false, -1, { 20.0f, 30.0f, 0.0f } },
        rescueCellLocal(5, 5),
    };

    auto result = BattleRescueRepositionSystem().resolve(input);

    CHECK_FALSE(result.teleport.has_value());
    CHECK_FALSE(result.basicCounterAttack.has_value());
    CHECK(result.counterDelta.unitId == -1);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsProtectRescueInsideDamageLifecycle", "[battle][core][breakthrough]")
{
    auto state = rescueDamageFrameState(50, 30);

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.requireCore(1).motion.position.x == Catch::Approx(2.0f * SceneTileWidth));
    CHECK(state.units.requireCore(1).motion.position.y == Catch::Approx(3.0f * SceneTileWidth));
    CHECK(std::any_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::RoleEffect
                && event.targetUnitId == 1
                && event.effectId == KysChess::EFT_HEAL;
        }));
    CHECK(state.units.requireCore(1).vitals.hp == 30);
    CHECK(state.units.requireCore(1).invincible == 10);
    CHECK(state.units.require(2).forcePullProtectRemaining() == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsExecuteRescueAndQueuesCounterAttackInsideDamageLifecycle", "[battle][core][breakthrough]")
{
    auto state = rescueDamageFrameState(20, 10);
    state.units.require(2).rescue.forcePullProtectRemaining = 0;
    state.units.require(0).rescue.forcePullExecuteRemaining = 2;
    state.units.requireCore(0).grid = { 10, 10 };
    state.units.requireCore(1).grid = { 5, 5 };
    state.rescue.cells = {
        rescueCell(9, 10),
        rescueCell(10, 9),
        rescueCell(10, 10),
    };

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(1).motion.position.x == Catch::Approx(9.0f * SceneTileWidth));
    CHECK(state.units.requireCore(1).motion.position.y == Catch::Approx(10.0f * SceneTileWidth));
    CHECK(state.units.require(0).forcePullExecuteRemaining() == 1);
    REQUIRE(state.nextFrame.queuedAttacks().size() == 1);
    const auto& counter = state.nextFrame.queuedAttacks().front();
    CHECK(counter.initial.attackSourceUnitId == 0);
    CHECK(counter.initial.preferredTargetUnitId == 1);
    CHECK(counter.initial.skillId == 1);
    CHECK(counter.initial.visualEffectId == 11);
    REQUIRE(counter.provenance.valid());
    CHECK(counter.provenance.cast.origin == CastOriginKind::RescueCounter);
    CHECK(counter.provenance.cast.propagation == CastPropagationPolicy::NoEffectRules);
    CHECK_FALSE(counter.provenance.cast.ultimate);
    CHECK(counter.provenance.origin == BattleAttackOriginKind::Initial);
    CHECK(counter.provenance.rootAttack);
    CHECK_FALSE(counter.provenance.mainProjectile);
    CHECK(counter.provenance.propagation == CastPropagationPolicy::NoEffectRules);
    CHECK_FALSE(counter.provenance.parentAttackId);
    CHECK(counter.provenance.attackOrdinal == 0);
    CHECK(counter.castWork.valid());
    CHECK(state.castLifecycle.outstandingWork(counter.provenance.cast.castId) == 1);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DoesNotEmitRescueDeltaWithoutLegalCell", "[battle][core][breakthrough]")
{
    auto state = rescueDamageFrameState(50, 30);
    state.rescue.cells = {
        rescueCell(2, 3, false),
        rescueCell(5, 5),
    };

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.require(2).forcePullProtectRemaining() == 1);
    CHECK(state.units.requireCore(1).motion.position.x == Catch::Approx(180.0f));
    CHECK(state.units.requireCore(1).motion.position.y == Catch::Approx(180.0f));
    CHECK(state.units.requireCore(1).vitals.hp == 20);
}
