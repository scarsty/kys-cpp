#include "ChessBattleEffects.h"
#include "battle/BattleCastSystem.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{

BattleCastResult committedCast(bool ultimate, BattleOperationType operationType)
{
    BattleCastResult result;
    result.decision.canCast = true;
    result.decision.ultimate = ultimate;
    result.decision.unitId = 1;
    result.decision.targetUnitId = 2;
    result.decision.skillId = ultimate ? 201 : 101;
    result.decision.operationType = operationType;
    return result;
}

BattleActionCommitInput basicActionInput()
{
    BattleActionCommitInput input;
    input.unit.id = 1;
    input.unit.team = 0;
    input.unit.position = { 10.0f, 20.0f, 0.0f };
    input.unit.facing = { 1.0f, 0.0f, 0.0f };
    input.unit.operationCount = 0;
    input.strengthenedMeleeOperationCountThreshold = 2;
    input.blinkWeakTargetDefWeight = 100;
    return input;
}

BattleActionTargetSnapshot target(int id, int hp, double defence, Pointf position)
{
    BattleActionTargetSnapshot snapshot;
    snapshot.id = id;
    snapshot.team = 1;
    snapshot.alive = true;
    snapshot.hp = hp;
    snapshot.maxHp = hp;
    snapshot.defence = defence;
    snapshot.position = position;
    return snapshot;
}

}  // namespace

TEST_CASE("BattleActionCommit_UltimateCastSetsPendingSkillTeamHeal", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.hasCast = true;
    input.cast = committedCast(true, BattleOperationType::RangedProjectile);

    auto result = BattleActionCommitSystem().commit(input);

    CHECK(result.combo.onSkillTeamHealPending);
}

TEST_CASE("BattleActionCommit_DoesNotReplayCastVisualEvents", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.hasCast = true;
    input.cast = committedCast(true, BattleOperationType::RangedProjectile);

    BattleVisualEvent textEvent;
    textEvent.type = BattleVisualEventType::FloatingText;
    textEvent.targetUnitId = 1;
    textEvent.text = "絕招";
    input.cast.visualEvents.push_back(std::move(textEvent));

    auto result = BattleActionCommitSystem().commit(input);

    CHECK(result.visualEvents.empty());
}

TEST_CASE("BattleActionCommit_BlinkAttackAlternatesWeakestAndRandomIntent", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.combo.blinkAttack = true;
    input.combo.blinkAttackUseWeakest = true;
    input.blinkReach = 144.0;
    input.targets = {
        target(2, 90, 0.0, { 100, 20, 0 }),
        target(3, 30, 0.0, { 120, 20, 0 }),
    };
    input.blinkGeometry.cells = {
        { 1, 0, { 100, 20, 0 }, true, false },
    };

    auto weakest = BattleActionCommitSystem().commit(input);

    REQUIRE(weakest.blinkCommands.size() == 1);
    CHECK(weakest.blinkCommands[0].unitId == 1);
    CHECK(weakest.blinkCommands[0].targetUnitId == 3);
    CHECK(weakest.blinkCommands[0].selectedWeakest);
    CHECK(weakest.blinkCommands[0].reach == 144.0);
    REQUIRE(weakest.logEvents.size() == 1);
    CHECK(weakest.logEvents[0].type == BattleLogEventType::Status);
    CHECK(weakest.logEvents[0].sourceUnitId == 1);
    CHECK(weakest.logEvents[0].targetUnitId == 3);
    CHECK(weakest.logEvents[0].text == "閃擊追殺");
    CHECK_FALSE(weakest.combo.blinkAttackUseWeakest);

    input.combo = weakest.combo;
    input.blinkRandomRoll = 1;
    auto random = BattleActionCommitSystem().commit(input);

    REQUIRE(random.blinkCommands.size() == 1);
    CHECK(random.blinkCommands[0].targetUnitId == 3);
    CHECK_FALSE(random.blinkCommands[0].selectedWeakest);
    REQUIRE(random.logEvents.size() == 1);
    CHECK(random.logEvents[0].type == BattleLogEventType::Status);
    CHECK(random.logEvents[0].sourceUnitId == 1);
    CHECK(random.logEvents[0].targetUnitId == 3);
    CHECK(random.logEvents[0].text == "閃擊突襲");
    CHECK(random.combo.blinkAttackUseWeakest);
}

TEST_CASE("BattleActionCommit_BlinkAttackResolvesDestinationFromGeometry", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.combo.blinkAttack = true;
    input.combo.blinkAttackUseWeakest = true;
    input.blinkReach = 64.0;
    input.blinkCellRandomRoll = 1;
    input.targets = {
        target(2, 30, 0.0, { 100, 20, 0 }),
    };
    input.blinkGeometry.currentGridX = 1;
    input.blinkGeometry.currentGridY = 1;
    input.blinkGeometry.cells = {
        { 1, 1, { 96, 20, 0 }, true, false },
        { 2, 1, { 108, 20, 0 }, false, false },
        { 3, 1, { 100, 84, 0 }, true, false },
        { 4, 1, { 132, 20, 0 }, true, false },
        { 5, 1, { 140, 20, 0 }, true, true },
    };

    auto result = BattleActionCommitSystem().commit(input);

    REQUIRE(result.blinkCommands.size() == 1);
    REQUIRE(result.blinkTeleports.size() == 1);
    const auto& teleport = result.blinkTeleports[0];
    CHECK(teleport.unitId == 1);
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

    auto result = BattleActionCommitSystem().commit(input);

    CHECK(result.operationCount == 1);
}
