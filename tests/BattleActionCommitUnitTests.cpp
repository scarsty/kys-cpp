#include "ChessBattleEffects.h"
#include "battle/BattleCastSystem.h"
#include "BattleLogTestHelpers.h"
#include "BattleRuntimeRecordTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{

BattleCastResult committedCast(bool ultimate, BattleOperationType operationType)
{
    BattleCastResult result;
    result.decision.canCast = true;
    result.decision.ultimate = ultimate;
    result.decision.unitId = 0;
    result.decision.targetUnitId = 1;
    result.decision.skillId = ultimate ? 201 : 101;
    result.decision.operationType = operationType;
    return result;
}

BattleActionCommitInput basicActionInput()
{
    BattleActionCommitInput input;
    input.sourceUnitId = 0;
    input.strengthenedMeleeOperationCountThreshold = 2;
    input.blinkWeakTargetDefWeight = 100;
    return input;
}

BattleRuntimeUnit unit(int id, int team, int hp, int defence, Pointf position)
{
    BattleRuntimeUnit result;
    result.id = id;
    result.team = team;
    result.alive = hp > 0;
    result.vitals.hp = hp;
    result.vitals.maxHp = hp;
    result.stats.defence = defence;
    result.motion.position = position;
    return result;
}

BattleRuntimeUnitRecords actionUnits()
{
    return KysChess::Battle::Test::runtimeRecords({
        unit(0, 0, 100, 0, { 10.0f, 20.0f, 0.0f }),
        unit(1, 1, 90, 0, { 100.0f, 20.0f, 0.0f }),
        unit(2, 1, 30, 0, { 120.0f, 20.0f, 0.0f }),
    });
}

}  // namespace

TEST_CASE("BattleActionCommit_UltimateCastSetsPendingSkillTeamHeal", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.hasCast = true;
    input.cast = committedCast(true, BattleOperationType::RangedProjectile);
    KysChess::RoleComboState combo;

    auto units = actionUnits();
    auto result = BattleActionCommitSystem().commit(input, combo, units);

    CHECK(result.combo.onSkillTeamHealPending);
}

TEST_CASE("BattleActionCommit_DoesNotReplayCastVisualEvents", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.hasCast = true;
    input.cast = committedCast(true, BattleOperationType::RangedProjectile);
    KysChess::RoleComboState combo;

    BattleVisualEvent textEvent;
    textEvent.type = BattleVisualEventType::FloatingText;
    textEvent.targetUnitId = 1;
    textEvent.text = "絕招";
    input.cast.visualEvents.push_back(std::move(textEvent));

    auto units = actionUnits();
    auto result = BattleActionCommitSystem().commit(input, combo, units);

    CHECK(result.visualEvents.empty());
}

TEST_CASE("BattleActionCommit_BlinkAttackAlternatesWeakestAndRandomIntent", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.blinkReach = 144.0;
    input.blinkGeometry.cells = {
        { 1, 0, { 100, 20, 0 }, true, false },
    };
    KysChess::RoleComboState combo;
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::BlinkAttack, 1 });
    combo.blinkAttackUseWeakest = true;
    auto units = actionUnits();

    auto weakest = BattleActionCommitSystem().commit(input, combo, units);

    REQUIRE(weakest.blinkCommands.size() == 1);
    CHECK(weakest.blinkCommands[0].unitId == 0);
    CHECK(weakest.blinkCommands[0].targetUnitId == 2);
    CHECK(weakest.blinkCommands[0].selectedWeakest);
    CHECK(weakest.blinkCommands[0].reach == 144.0);
    REQUIRE(weakest.logEvents.size() == 1);
    CHECK(weakest.logEvents[0].type == BattleLogEventType::Status);
    CHECK(weakest.logEvents[0].sourceUnitId == 0);
    CHECK(weakest.logEvents[0].targetUnitId == 2);
    CHECK(BattleLogTest::textOf(weakest.logEvents[0]) == "閃擊追殺");
    CHECK_FALSE(weakest.combo.blinkAttackUseWeakest);

    input.blinkRandomRoll = 1;
    auto random = BattleActionCommitSystem().commit(input, weakest.combo, units);

    REQUIRE(random.blinkCommands.size() == 1);
    CHECK(random.blinkCommands[0].targetUnitId == 2);
    CHECK_FALSE(random.blinkCommands[0].selectedWeakest);
    REQUIRE(random.logEvents.size() == 1);
    CHECK(random.logEvents[0].type == BattleLogEventType::Status);
    CHECK(random.logEvents[0].sourceUnitId == 0);
    CHECK(random.logEvents[0].targetUnitId == 2);
    CHECK(BattleLogTest::textOf(random.logEvents[0]) == "閃擊突襲");
    CHECK(random.combo.blinkAttackUseWeakest);
}

TEST_CASE("BattleActionCommit_BlinkAttackResolvesDestinationFromGeometry", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.blinkReach = 64.0;
    input.blinkCellRandomRoll = 1;
    input.blinkGeometry.currentGridX = 1;
    input.blinkGeometry.currentGridY = 1;
    input.blinkGeometry.cells = {
        { 1, 1, { 96, 20, 0 }, true, false },
        { 2, 1, { 108, 20, 0 }, false, false },
        { 3, 1, { 100, 84, 0 }, true, false },
        { 4, 1, { 132, 20, 0 }, true, false },
        { 5, 1, { 140, 20, 0 }, true, true },
    };
    KysChess::RoleComboState combo;
    KysChess::ChessBattleEffects::applyEffect(combo, { KysChess::EffectType::BlinkAttack, 1 });
    combo.blinkAttackUseWeakest = true;
    auto units = actionUnits();

    auto result = BattleActionCommitSystem().commit(input, combo, units);

    REQUIRE(result.blinkCommands.size() == 1);
    REQUIRE(result.blinkTeleports.size() == 1);
    const auto& teleport = result.blinkTeleports[0];
    CHECK(teleport.unitId == 0);
    CHECK(teleport.targetUnitId == 2);
    CHECK(teleport.gridX == 4);
    CHECK(teleport.gridY == 1);
    CHECK(teleport.position.x == 132.0f);
    CHECK(teleport.position.y == 20.0f);
    CHECK(teleport.facing.x == -1.0f);
    CHECK(teleport.facing.y == 0.0f);
}

TEST_CASE("BattleActionCommit_CommittedMeleeCastAdvancesOperationCount", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.hasCast = true;
    input.cast = committedCast(false, BattleOperationType::Melee);
    KysChess::RoleComboState combo;

    auto units = actionUnits();
    auto result = BattleActionCommitSystem().commit(input, combo, units);

    CHECK(result.operationCount == 1);
}
