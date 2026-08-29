#include "battle/BattleEffectCommandSystem.h"
#include "battle/BattleRuntimeUnits.h"
#include "battle/BattleUnitStore.h"
#include "BattleRuntimeRecordTestHelpers.h"
#include "BattleRuntimeStateTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;

TEST_CASE("BattleRuntimeUnits_RequiresAndMutatesCanonicalUnitValues", "[battle][core][runtime]")
{
    BattleRuntimeUnits store;
    BattleRuntimeUnit unit;
    unit.id = 0;
    unit.team = 0;
    unit.alive = true;
    unit.vitals = { 80, 100, 10, 50 };
    unit.stats.attack = 20;
    unit.stats.defence = 7;
    unit.shield = 3;
    appendRuntimeRecord(store, unit);

    BattleDamageUnitState damage;
    damage.id = 0;
    damage.vitals.hp = 25;
    damage.vitals.maxHp = 100;
    damage.alive = false;
    damage.vitals.mp = 18;
    damage.vitals.maxMp = 60;
    damage.attack = 31;
    damage.invincible = 4;
    damage.shield = 9;
    store.writeDamageUnit(damage);

    const auto& updated = store.requireCore(0);
    CHECK_FALSE(updated.alive);
    CHECK(updated.vitals.hp == 25);
    CHECK(updated.vitals.mp == 18);
    CHECK(updated.vitals.maxMp == 60);
    CHECK(updated.stats.attack == 31);
    CHECK(updated.invincible == 4);
    CHECK(updated.shield == 9);
}

TEST_CASE("BattleRuntimeUnits_UpdatesPositionAndGridWithCoreTransform", "[battle][core][runtime]")
{
    BattleRuntimeUnits store;
    BattleGridTransform gridTransform{ SceneTileWidth, 64 };
    BattleRuntimeUnit unit;
    unit.id = 0;
    unit.motion.position = { 64.0f * 36.0f, 0.0f, 0.0f };
    appendRuntimeRecord(store, unit);

    store.setPosition(0, { 64.0f * 36.0f + 36.0f, 36.0f, 0.0f }, gridTransform);

    const auto& updated = store.requireCore(0);
    CHECK(updated.motion.position.x == 64.0f * 36.0f + 36.0f);
    CHECK(updated.motion.position.y == 36.0f);
    CHECK(updated.grid.x == 1);
    CHECK(updated.grid.y == 0);
}

TEST_CASE("BattleRuntimeUnits_SelectsNearestAndFarthestLiveEnemyUnits", "[battle][core][runtime]")
{
    auto store = runtimeRecords({
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 130, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 260, 100, 0 }),
        runtimeUnitSnapshot(3, 0, 100, { 110, 100, 0 }),
        runtimeUnitSnapshot(4, 1, 0, { 105, 100, 0 }),
    });

    CHECK(findNearestEnemyUnitId(store, 0) == 1);
    CHECK(findFarthestEnemyUnitId(store, 0) == 2);
}

TEST_CASE("BattleRuntimeUnits_TargetSelectionReturnsNoUnitWithoutLiveEnemy", "[battle][core][runtime]")
{
    auto store = runtimeRecords({
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 0, 100, { 130, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 0, { 260, 100, 0 }),
    });

    CHECK(findNearestEnemyUnitId(store, 0) == -1);
    CHECK(findFarthestEnemyUnitId(store, 0) == -1);
}

TEST_CASE("BattleRuntimeUnits_TargetSelectionBreaksEqualDistancesByUnitId", "[battle][core][determinism]")
{
    auto store = runtimeRecords({
        runtimeUnitSnapshot(0, 0, 100, {100, 100, 0}),
        runtimeUnitSnapshot(2, 1, 100, {130, 100, 0}),
        runtimeUnitSnapshot(1, 1, 100, {70, 100, 0}),
    });

    CHECK(findNearestEnemyUnitId(store, 0) == 1);
    CHECK(findFarthestEnemyUnitId(store, 0) == 1);
}

TEST_CASE("BattleRuntimeUnitRecord_AdvanceFrameTick_CommitsCooldownAndIdleResourceTicks", "[battle][core]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 0;
    record.core.animation.cooldown = 1;
    record.core.animation.cooldownMax = 12;
    record.core.animation.actType = 2;
    record.core.operationType = BattleOperationType::Dash;
    record.core.haveAction = true;
    record.core.physicalPower = 4;
    record.core.vitals.mp = 10;
    record.core.vitals.maxMp = 20;
    record.core.motion.velocity = { 3, 4, 0 };

    auto result = record.advanceFrameTick({ 6, 3, 3 });

    CHECK(result.skillFinished);
    CHECK(record.core.animation.cooldown == 0);
    CHECK(record.core.animation.cooldownMax == 0);
    CHECK(record.core.animation.actFrame == 0);
    CHECK(record.core.animation.actType == -1);
    CHECK(record.core.operationType == BattleOperationType::None);
    CHECK_FALSE(record.core.haveAction);
    CHECK(record.core.physicalPower == 5);
    CHECK(record.core.vitals.mp == 11);
    CHECK(record.core.motion.velocity.x == 0.0f);
    CHECK(record.core.motion.velocity.y == 0.0f);
}

TEST_CASE("BattleRuntimeState_ComposesHeadlessRuntimeStateForFullFrameRunner", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 240, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleDamageTransactionInput damageInput;
    damageInput.request.attackerUnitId = 0;
    damageInput.request.defenderUnitId = 0;
    state.nextFrame.queueDamage(pendingDamageIntent(damageInput));

    state.units.requireCore(0).vitals.hp = 80;

    CHECK(state.units.size() == 2);
    CHECK(state.nextFrame.queuedDamage()[0].request.defenderUnitId == 0);
    CHECK(state.units.requireCore(0).vitals.hp == 80);
    CHECK(state.units.require(1).id() == 1);
    CHECK_FALSE(state.result.ended);
    CHECK(state.result.winningTeam == -1);
}

TEST_CASE("BattleRuntimeUnitRecord_TypedMpRecoveryBonusStaysSeparateFromMpBlock", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
    }));
    auto& runtimeUnit = state.units.require(0);
    runtimeUnit.setMpBlockFrames(5);
    addTypedAttributeModifier(
        state,
        0,
        BattleAttribute::MpRecoveryBonus,
        AttributeOperation::PercentAdd,
        40);

    CHECK(runtimeUnit.mpBlocked());
    CHECK(BattleEffectCommandSystem::queryAttribute(
              state,
              {
                  .unitId = 0,
                  .attribute = BattleAttribute::MpRecoveryBonus,
                  .baseValue = 0,
                  .frame = state.movement.frame,
              }) == 40);
}

TEST_CASE("BattleRuntimeUnitRecord_OwnsPerUnitRuntimeFacts", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 7;
    record.core.alive = true;
    record.movement.active = true;

    BattleActionPlanSeed plan;
    record.setActionPlan(plan);

    BattlePendingCastAction pending;
    pending.targetUnitId = 3;
    pending.effectCast.provenance = {
        .rootCastId = BattleCastId{ 1 },
        .castId = BattleCastId{ 1 },
        .sourceUnitId = 7,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
    };
    record.setPendingCast(pending);
    record.markUltimateCaster();

    CHECK(record.id() == 7);
    CHECK(record.alive());
    REQUIRE(record.actionPlan() != nullptr);
    REQUIRE(record.pendingCast() != nullptr);
    CHECK(record.pendingCast()->effectCast.provenance.sourceUnitId == 7);
    CHECK(record.pendingCast()->targetUnitId == 3);
    CHECK(record.isUltimateCaster());
    CHECK(record.isSkillCooldownUltimate());

    record.clearActionOwners();

    CHECK(record.pendingCast() == nullptr);
    CHECK_FALSE(record.isUltimateCaster());
    CHECK_FALSE(record.isSkillCooldownUltimate());
}

TEST_CASE("BattleRuntimeUnitRecord_ActionOwnershipReplacesRuntimeActionMaps", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    BattleRuntimeUnit unit;
    unit.id = 0;
    unit.alive = true;

    BattleActionPlanSeed seed;
    appendRuntimeUnit(state, makeRuntimeUnitSpawn(std::move(unit), {}, seed));

    auto& record = state.units.require(0);
    REQUIRE(record.actionPlan() != nullptr);

    BattlePendingCastAction pending;
    pending.targetUnitId = 1;
    pending.effectCast.provenance = {
        .rootCastId = BattleCastId{ 1 },
        .castId = BattleCastId{ 1 },
        .sourceUnitId = 0,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
    };
    record.setPendingCast(pending);
    record.markUltimateCaster();

    REQUIRE(record.pendingCast() != nullptr);
    CHECK(record.pendingCast()->effectCast.provenance.sourceUnitId == 0);
    CHECK(record.isUltimateCaster());
    CHECK(record.isSkillCooldownUltimate());

    record.clearActionOwners();

    CHECK(record.pendingCast() == nullptr);
    CHECK_FALSE(record.isUltimateCaster());
    CHECK_FALSE(record.isSkillCooldownUltimate());
}

TEST_CASE("BattleRuntimeUnitRecord_MovementAgentLivesOnRecord", "[battle][core][movement][ownership]")
{
    BattleRuntimeState state;
    BattleRuntimeUnit unit;
    unit.id = 1;
    unit.alive = true;
    unit.motion.position = { 12.0f, 18.0f, 0.0f };

    appendRuntimeUnit(state, makeRuntimeUnitSpawn(std::move(unit)));

    auto& record = state.units.require(1);
    record.movement.targetId = 0;
    record.movement.assignedSlot = 2;

    CHECK(record.movement.active);
    CHECK(record.movement.physics.position.x == 12.0f);
    CHECK(record.movement.targetId == 0);
    CHECK(record.movement.assignedSlot == 2);
}

TEST_CASE("BattleRuntimeUnitRecord_StatusDomainMethodsMutateOwnedStatus", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 2;
    record.core.vitals.hp = 50;
    record.core.vitals.maxHp = 100;
    record.status.effects.setFrames(BattleStatusKind::Stun, 5, 8);

    record.clearFrozen();
    record.setMpBlockFrames(3);

    CHECK_FALSE(record.frozen());
    CHECK(record.status.effects.maximumFrames(BattleStatusKind::Stun) == 0);
    CHECK(record.status.effects.remainingFrames(BattleStatusKind::MpBlocked) == 3);
}

TEST_CASE("BattleRuntimeUnitRecord_ClearActionOwnersDropsPendingState", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 1;

    BattlePendingCastAction pending;
    pending.targetUnitId = 2;
    pending.effectCast.provenance = {
        .rootCastId = BattleCastId{ 1 },
        .castId = BattleCastId{ 1 },
        .sourceUnitId = 1,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
    };
    record.setPendingCast(pending);
    record.markUltimateCaster();

    record.clearActionOwners();

    CHECK(record.pendingCast() == nullptr);
    CHECK_FALSE(record.isUltimateCaster());
    CHECK_FALSE(record.isSkillCooldownUltimate());
}

TEST_CASE("BattleRuntimeUnitRecord_DamageStateComposesOwnedFacts", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 3;
    record.core.alive = true;
    record.core.vitals.hp = 40;
    record.core.vitals.maxHp = 100;
    record.core.vitals.mp = 5;
    record.core.vitals.maxMp = 20;
    record.status.effects.setFrames(BattleStatusKind::MpBlocked, 4);

    const auto damage = record.damageState(30);

    CHECK(damage.id == 3);
    CHECK(damage.mpBlocked);
    CHECK(damage.mpRecoveryBonusPct == 30);
}

TEST_CASE("BattleRuntimeUnitRecord_RescueFactsLiveOnRecord", "[battle][core][ownership]")
{
    BattleRuntimeUnitRecord record;
    record.core.id = 5;
    record.rescue.forcePullProtectRemaining = 2;
    record.rescue.forcePullExecuteRemaining = 3;

    BattleRescueCounterDelta delta;
    delta.unitId = 5;
    delta.protectRemainingDelta = -1;
    delta.executeRemainingDelta = 2;
    record.applyRescueCounterDelta(delta);

    CHECK(record.forcePullProtectRemaining() == 1);
    CHECK(record.forcePullExecuteRemaining() == 5);

    record.clearForcePullProtect();
    CHECK(record.forcePullProtectRemaining() == 0);
}
