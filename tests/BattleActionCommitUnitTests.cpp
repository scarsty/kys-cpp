#include "ChessBattleEffects.h"
#include "battle/BattleCastSystem.h"
#include "BattleLogTestHelpers.h"
#include "BattleRuntimeRecordTestHelpers.h"

#include <catch2/catch_approx.hpp>
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

BattleRuntimeUnits actionUnits()
{
    return KysChess::Battle::Test::runtimeRecords({
        unit(0, 0, 100, 0, { 10.0f, 20.0f, 0.0f }),
        unit(1, 1, 90, 0, { 100.0f, 20.0f, 0.0f }),
        unit(2, 1, 30, 0, { 120.0f, 20.0f, 0.0f }),
    });
}

}  // namespace

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

    auto units = actionUnits();
    auto result = BattleActionCommitSystem().commit(input, units);

    CHECK(result.visualEvents.empty());
}

TEST_CASE("BattleActionCommit_BlinkAttackUsesExternallySelectedWeakestAndRandomIntent", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.mobility = KysChess::CastMobilityPolicy::BlinkAttack;
    input.blinkUseWeakestTarget = true;
    input.blinkReach = 144.0;
    input.blinkGeometry.cells = {
        { 1, 0, { 100, 20, 0 }, true, false },
    };
    auto units = actionUnits();

    auto weakest = BattleActionCommitSystem().commit(input, units);

    REQUIRE(weakest.blinkTeleports.size() == 1);
    CHECK(weakest.blinkTeleports[0].unitId == 0);
    CHECK(weakest.blinkTeleports[0].targetUnitId == 2);
    CHECK(weakest.blinkTeleports[0].selectedWeakest);
    REQUIRE(weakest.logEvents.size() == 1);
    CHECK(weakest.logEvents[0].type == BattleLogEventType::Status);
    CHECK(weakest.logEvents[0].sourceUnitId == 0);
    CHECK(weakest.logEvents[0].targetUnitId == 2);
    CHECK(BattleLogTest::textOf(weakest.logEvents[0]) == "閃擊追殺");
    CHECK(input.blinkUseWeakestTarget);

    input.blinkRandomRoll = 1;
    input.blinkUseWeakestTarget = false;
    auto random = BattleActionCommitSystem().commit(input, units);

    REQUIRE(random.blinkTeleports.size() == 1);
    CHECK(random.blinkTeleports[0].targetUnitId == 2);
    CHECK_FALSE(random.blinkTeleports[0].selectedWeakest);
    REQUIRE(random.logEvents.size() == 1);
    CHECK(random.logEvents[0].type == BattleLogEventType::Status);
    CHECK(random.logEvents[0].sourceUnitId == 0);
    CHECK(random.logEvents[0].targetUnitId == 2);
    CHECK(BattleLogTest::textOf(random.logEvents[0]) == "閃擊突襲");
    CHECK_FALSE(input.blinkUseWeakestTarget);
}

TEST_CASE("BattleActionCommit_BlinkAttackResolvesDestinationFromGeometry", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.mobility = KysChess::CastMobilityPolicy::BlinkAttack;
    input.blinkUseWeakestTarget = true;
    input.blinkReach = 64.0;
    input.blinkCellRandomRoll = 1;
    input.blinkGeometry.currentGridX = 1;
    input.blinkGeometry.currentGridY = 1;
    input.hasCast = true;
    input.cast = committedCast(false, BattleOperationType::Melee);
    BattleAttackSpawnRequest attack{ BattleAttackPayload(
        BattleAttackDelivery::contact(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    attack.initial.attackSourceUnitId = 0;
    attack.initial.preferredTargetUnitId = 1;
    attack.initial.position = { 20, 20, 0 };
    attack.initial.operationType = BattleOperationType::Melee;
    input.cast.attackSpawnRequests.push_back(attack);
    input.blinkGeometry.cells = {
        { 1, 1, { 96, 20, 0 }, true, false },
        { 2, 1, { 108, 20, 0 }, false, false },
        { 3, 1, { 100, 84, 0 }, true, false },
        { 4, 1, { 132, 20, 0 }, true, false },
        { 5, 1, { 140, 20, 0 }, true, true },
    };
    auto units = actionUnits();

    auto result = BattleActionCommitSystem().commit(input, units);

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
    REQUIRE(result.attackSpawnRequests.size() == 1);
    CHECK(result.attackSpawnRequests[0].initial.preferredTargetUnitId == 2);
    CHECK(result.attackSpawnRequests[0].initial.position.x == Catch::Approx(122.0f));
    CHECK(result.attackSpawnRequests[0].initial.position.y == Catch::Approx(20.0f));
}

TEST_CASE("BattleActionCommit_CommittedMeleeCastAdvancesOperationCount", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.hasCast = true;
    input.cast = committedCast(false, BattleOperationType::Melee);
    auto units = actionUnits();
    auto result = BattleActionCommitSystem().commit(input, units);

    CHECK(result.operationCount == 1);
}

TEST_CASE("BattleActionCommit_DualWieldAddsDelayedSecondaryTargetFollowUpAndBlockChance", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.hasCast = true;
    input.normalAttackActType = 7;
    input.cast = committedCast(false, BattleOperationType::Melee);
    BattleAttackSpawnRequest main{ BattleAttackPayload(
        BattleAttackDelivery::contact(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    main.initial.attackSourceUnitId = 0;
    main.initial.preferredTargetUnitId = 1;
    main.initial.requirePreferredTarget = true;
    main.initial.operationType = BattleOperationType::Melee;
    main.initial.totalFrame = 20;
    main.provenance.mainProjectile = true;
    main.initial.position = { 20.0f, 20.0f, 0.0f };
    main.initial.strengthPct = 100;
    input.cast.attackSpawnRequests.push_back(main);
    input.delayedAlternateAttack = KysChess::DelayedAlternateAttackBehavior{ 6, 45, 50 };
    auto units = actionUnits();
    auto result = BattleActionCommitSystem().commit(input, units);

    REQUIRE(result.attackSpawnRequests.size() == 2);
    const auto& followUp = result.attackSpawnRequests[1];
    CHECK(followUp.initial.castSubrequestKind == BattleAttackCastSubrequestKind::DualWieldFollowUp);
    CHECK(followUp.initial.preferredTargetUnitId == 2);
    CHECK(followUp.initial.requirePreferredTarget);
    CHECK(followUp.initial.track);
    CHECK_FALSE(followUp.provenance.mainProjectile);
    CHECK(followUp.initial.roleAttackEchoActType == 7);
    CHECK(followUp.initial.strengthPct == 45);
    CHECK(followUp.spawnDelayFrames == 6);
    CHECK(followUp.attackerDualWieldBlockGainChancePct == 50);
}

TEST_CASE("BattleActionCommit_DualWieldFallsBackToPrimaryTarget", "[battle][action_commit][unit]")
{
    auto input = basicActionInput();
    input.hasCast = true;
    input.normalAttackActType = 7;
    input.cast = committedCast(false, BattleOperationType::Melee);
    BattleAttackSpawnRequest main{ BattleAttackPayload(
        BattleAttackDelivery::contact(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    main.initial.attackSourceUnitId = 0;
    main.initial.preferredTargetUnitId = 1;
    main.initial.operationType = BattleOperationType::Melee;
    main.initial.totalFrame = 20;
    main.provenance.mainProjectile = true;
    main.initial.position = { 20.0f, 20.0f, 0.0f };
    input.cast.attackSpawnRequests.push_back(main);
    input.delayedAlternateAttack = KysChess::DelayedAlternateAttackBehavior{ 6, 45, 50 };
    auto units = KysChess::Battle::Test::runtimeRecords({
        unit(0, 0, 100, 0, { 10.0f, 20.0f, 0.0f }),
        unit(1, 1, 90, 0, { 100.0f, 20.0f, 0.0f }),
    });

    auto result = BattleActionCommitSystem().commit(input, units);

    REQUIRE(result.attackSpawnRequests.size() == 2);
    CHECK(result.attackSpawnRequests[1].initial.preferredTargetUnitId == 1);
}
