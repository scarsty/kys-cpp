#include "battle/BattleCore.h"
#include "battle/BattleAreaEffectSystem.h"
#include "battle/BattleLogSegments.h"
#include "battle/BattleMovement.h"
#include "battle/BattleRuntimeSession.h"
#include "battle/BattleRuntimeRules.h"
#include "battle/BattleRuntimeUnitSpawn.h"
#include "ChessEftIds.h"
#include "ChessCombo.h"
#include "Find.h"
#include "BattleLogTestHelpers.h"
#include "BattleMovementTestHelpers.h"
#include "BattleRuntimeStateTestHelpers.h"
#include "BattlePresentationTestHelpers.h"
#include "BattleRuntimeRecordTestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "BattleCoreTestHelpers.h"

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;
using namespace KysChess;
using namespace BattlePresentationTest;

template <class T, class = void>
struct HasUnitIdMember : std::false_type
{
};

template <class T>
struct HasUnitIdMember<T, std::void_t<decltype(std::declval<T&>().unitId)>> : std::true_type
{
};

static_assert(!HasUnitIdMember<BattleRescueUnitRuntime>::value);

TEST_CASE("BattleFrameRunner_RoutesDamageTransactionsThroughRuntimeUnits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
});
    queuePendingDamage(state, lethalDamageInput(0, 1));
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 10);

    runBattleFrame(state);

    const auto& defender = state.units.requireCore(1);
    CHECK(defender.vitals.hp == 0);
    CHECK_FALSE(defender.alive);
}

TEST_CASE("BattleFrameRunner_DispatchesTypedDeathRulesForStatusDamage", "[battle][core][effect][death]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
        unit(2, 1, { 140, 100, 0 }),
        unit(3, 1, { 160, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 140, 100, 0 }),
    });
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 10);
    state.units.require(2).status = statusRuntimeSnapshot(2, 100);

    const EffectSourceBinding deathBinding{
        EffectSourceKind::Magic,
        95,
        1,
        1,
    };
    EffectRule ownDeath;
    ownDeath.id = { 1 };
    ownDeath.event = EffectEvent::UnitDied;
    ownDeath.selector.kind = EffectSelectorKind::Self;
    ChangeResourceAction deathShield;
    deathShield.resource = BattleResource::Shield;
    deathShield.kind = ResourceChangeKind::Grant;
    deathShield.amount.flat = 1;
    ownDeath.actions = { { EffectActionValue{ deathShield } } };
    state.effectRules.append(deathBinding, ownDeath);

    auto attackOnlyDeath = ownDeath;
    attackOnlyDeath.id = { 2 };
    attackOnlyDeath.conditions = { DamageOriginIsAttackCondition{} };
    state.effectRules.append(deathBinding, attackOnlyDeath);

    const EffectSourceBinding allyBinding{
        EffectSourceKind::Magic,
        95,
        2,
        1,
    };
    auto allyDeath = ownDeath;
    allyDeath.id = { 3 };
    allyDeath.event = EffectEvent::AllyDied;
    state.effectRules.append(allyBinding, allyDeath);

    queuePendingDamage(
        state,
        preResolvedDamageInput(0, 1, 10, 20),
        {},
        EffectStatusDamageOrigin{ BattleStatusKind::Poison, 0 });
    const auto* queuedOrigin = std::get_if<EffectStatusDamageOrigin>(
        &state.nextFrame.queuedDamage().front().effectOrigin);
    REQUIRE(queuedOrigin != nullptr);
    CHECK(queuedOrigin->status == BattleStatusKind::Poison);
    CHECK(queuedOrigin->sourceUnitId == 0);

    runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.effectRules.activationCount(deathBinding, { 1 }) == 1);
    CHECK(state.effectRules.activationCount(deathBinding, { 2 }) == 0);
    CHECK(state.effectRules.activationCount(allyBinding, { 3 }) == 1);
}

TEST_CASE("BattleFrameRunner_RoutesMovementPhysicsThroughRuntimeUnits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({ unit(0, 0, { 100, 100, 0 }) }));
    state.attacks = attackWorld();
    state.gridTransform = { SceneTileWidth, 64 };
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(0).motion.acceleration = { 0, 0, 0 };
    state.units.require(0).movement.physics.position = { 100, 100, 0 };
    state.units.require(0).movement.physics.velocity = { 5, 0, 0 };
    state.units.require(0).movement.physics.acceleration = { 0, 0, 0 };
    state.units.require(0).movement.physics.movementDashFrames = 1;
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);

    const auto& unit = state.units.requireCore(0);
    CHECK(unit.motion.position.x == 105.0f);
    CHECK(unit.motion.velocity.x == 5.0f);
    CHECK(unit.motion.acceleration.x == 0.0f);
    CHECK(state.units.require(0).movement.physics.movementDashFrames == 0);
    CHECK(state.units.require(0).movement.physics.movementDashSpreadFrames == 6);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsMovementBeforeProjectileEvents", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 600, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.preferredTargetUnitId = 0;
    projectile.state.totalFrame = 30;
    projectile.state.visualEffectId = 33;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 0, 0, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    CHECK(state.movement.frame == 1);
    REQUIRE(result.logEvents.empty());
    REQUIRE(!result.visualEvents.empty());
    CHECK(result.frame == 1);
    const auto& firstProjectileEvent = result.visualEvents[0];
    CHECK(firstProjectileEvent.type == BattleVisualEventType::ProjectileMoved);
    CHECK(firstProjectileEvent.effectId == 10);
    CHECK(firstProjectileEvent.sourceUnitId == 0);
    CHECK(firstProjectileEvent.targetUnitId == 0);
    CHECK(firstProjectileEvent.durationFrames == 30);
    CHECK(firstProjectileEvent.visualEffectId == 33);
    CHECK(firstProjectileEvent.position.x == 5.0f);
    CHECK(firstProjectileEvent.velocity.x == 5.0f);
    CHECK(firstProjectileEvent.operationKind == 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_UsesGroupedRuntimeUnitState", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 600, 100, 0 }),
    }));
    state.attacks = attackWorld();

    auto grouped = runtimeUnitSnapshot(0, 0, 80, { 100, 100, 0 });
    grouped.vitals = { 80, 120, 4, 50 };
    grouped.motion = { { 100, 100, 0 }, { 7, 8, 0 }, { 0, 0, 0 }, { 1, 0, 0 } };
    grouped.animation = { 2, 60, 5, 13 };
    grouped.operationType = BattleOperationType::Dash;
    grouped.haveAction = true;

    auto target = runtimeUnitSnapshot(1, 1, 100, { 600, 100, 0 });
    seedRuntimeUnits(state, { grouped, target });
    state.units.require(0).status = statusRuntimeSnapshot(0, 80);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);

    runBattleFrame(state);

    const auto& updated = state.units.requireCore(0);
    CHECK(updated.animation.cooldown == 1);
    CHECK(updated.animation.cooldownMax == 60);
    CHECK(updated.animation.actFrame == 6);
    CHECK(updated.animation.actType == 13);
    CHECK(updated.vitals.mp == 5);
    CHECK(updated.motion.velocity.x == 7.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsPendingAttackSpawnRequest", "[battle][core][presentation]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({}));
    state.attacks = attackWorld();
    state.attacks.hitRadius = 10.0;
    state.attacks.nextAttackId = 50;
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 0, 0, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 106, 120, 0 }),
    });
    queueTrackedAttack(state, attackSpawnRequest());

    auto result = runBattleFrame(state);

    CHECK(state.nextFrame.queuedAttacks().empty());
    REQUIRE(state.attacks.attacks.size() == 1);
    CHECK(state.attacks.attacks[0].id == 50);
    CHECK(state.attacks.attacks[0].state.position.x == 106.0f);
    CHECK(state.attacks.nextAttackId == 51);

    REQUIRE(result.gameplayEvents.size() >= 3);
    auto gameplayIt = std::find_if(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.effectId == 50;
        });
    REQUIRE(gameplayIt != result.gameplayEvents.end());
    CHECK(gameplayIt->sourceUnitId == 0);
    CHECK(gameplayIt->targetUnitId == 0);
    CHECK(gameplayIt->position.x == 100.0f);
    CHECK(gameplayIt->position.y == 120.0f);

    const auto* presentation = findVisualEvent(result, BattleVisualEventType::ProjectileSpawned, 50);
    REQUIRE(presentation);
    CHECK(presentation->sourceUnitId == 0);
    CHECK(presentation->targetUnitId == 0);
    CHECK(presentation->durationFrames == 30);
    CHECK(presentation->visualEffectId == 44);
    CHECK(presentation->position.x == 100.0f);
    CHECK(presentation->position.y == 120.0f);
    CHECK(presentation->velocity.x == 6.0f);
    CHECK(presentation->operationKind == 2);

    const auto* moved = findVisualEvent(result, BattleVisualEventType::ProjectileMoved, 50);
    REQUIRE(moved);
    CHECK(moved->position.x == 106.0f);
    REQUIRE(findVisualEvent(result, BattleVisualEventType::ProjectileHit, 50));
    auto hitGameplayIt = std::find_if(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileHit
                && event.targetUnitId == 1;
        });
    CHECK(hitGameplayIt != result.gameplayEvents.end());

    CHECK(presentation->effectId == 50);
    CHECK(presentation->visualEffectId == 44);
    CHECK(presentation->position.x == 100.0f);
    CHECK(presentation->velocity.x == 6.0f);
    CHECK(presentation->durationFrames == 30);
    CHECK(presentation->operationKind == 2);
}

TEST_CASE("BattleFrameRunner_BlinkAttackTeleportsRuntimeUnit", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({}));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 2304, 0, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 2376, 72, 0 }),
    });
    state.gridTransform = { SceneTileWidth, BattleCoordCount };
    state.units.setPosition(0, { 2304, 0, 0 }, state.gridTransform);
    state.units.setPosition(1, { 2376, 72, 0 }, state.gridTransform);
    state.movement.config = testConfig();
    state.random = BattleRuntimeRandom(7u);

    addTestCastMobilityRule(state, 0, CastMobilityPolicy::BlinkAttack);

    auto cast = frameCastInput(0, 1);
    cast.unit.position = { 2304, 0, 0 };
    cast.targetPosition = { 2376, 72, 0 };
    cast.targetDistance = 100.0;
    cast.normalSkill.reach = 144.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    runBattleFrame(state);
    preparePendingCastCommitFrame(state, 0, BattleOperationType::Melee, 6);

    auto result = runBattleFrame(state);

    CHECK(result.blinkSoundCount == 1);
    const auto& teleported = state.units.requireCore(0);
    CHECK(teleported.motion.position.x != 2304.0f);
    CHECK(teleported.motion.position.x == state.units.require(0).movement.physics.position.x);
    CHECK(teleported.grid.x != 0);
    CHECK(teleported.motion.velocity.norm() == 0.0f);
}

TEST_CASE("BattleFrameRunner_BlinkAttackStartsMeleeFromOutsideBattlefieldAtLongRange", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({}));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 0, 0, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 2376, 72, 0 }),
    });
    state.gridTransform = { SceneTileWidth, BattleCoordCount };
    state.units.setPosition(0, { 0, 0, 0 }, state.gridTransform);
    state.units.setPosition(1, { 2376, 72, 0 }, state.gridTransform);
    state.movement.config = testConfig();
    state.random = BattleRuntimeRandom(7u);

    addTestCastMobilityRule(state, 0, CastMobilityPolicy::BlinkAttack);

    auto cast = frameCastInput(0, 1);
    cast.unit.position = { 0, 0, 0 };
    cast.targetPosition = { 2376, 72, 0 };
    cast.targetDistance = 2377.0;
    cast.normalSkill.reach = 137.5;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    runBattleFrame(state);

    REQUIRE(state.units.require(0).pendingCast() != nullptr);
    preparePendingCastCommitFrame(state, 0, BattleOperationType::Melee, 6);
    auto result = runBattleFrame(state);

    CHECK(result.blinkSoundCount == 1);
    CHECK(state.units.requireCore(0).grid.x >= 0);
    CHECK(state.units.requireCore(0).grid.y >= 0);
    CHECK_FALSE(state.attacks.attacks.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastPlanningRecordsStartWithoutSpawningAttack", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 82, 20, 0 }),
    }));
    state.attacks = attackWorld();
    state.attacks.hitRadius = 10.0;
    state.attacks.nextAttackId = 90;
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.forceRanged = true;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->effectCast.provenance.magicId == 301);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
    REQUIRE(result.gameplayEvents.size() == 1);
    CHECK(result.gameplayEvents[0].type == BattleGameplayEventType::CastStarted);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].targetUnitId == 1);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "施放框架招式");
    CHECK(result.logEvents[0].skillName == "框架招式");
}

TEST_CASE("BattleFrameRunner_ForcedRangedMeleeUsesDefaultProjectileProfileWithoutOptionalTuning", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 260, 20, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.selectDistance = 1;
    configureRuntimeActionPlan(state, cast);
    ModifyCastAction forceRanged;
    forceRanged.rangeMode = CastRangeMode::Ranged;
    addTestOwnerRule(
        state,
        0,
        9300,
        1,
        EffectEvent::CastPlanned,
        EffectActionValue{ forceRanged });
    state.units.requireCore(0).animation.cooldown = 0;

    auto start = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->operationType == BattleOperationType::RangedProjectile);
    CHECK_FALSE(hasProjectilePresentationEvent(start));

    auto& caster = state.units.requireCore(0);
    caster.haveAction = true;
    caster.operationType = BattleOperationType::RangedProjectile;
    caster.animation.actType = 1;
    caster.animation.actFrame = state.action.castConfig.castFrames[battleOperationIndex(BattleOperationType::RangedProjectile)];
    caster.animation.cooldown = 20;

    auto result = runBattleFrame(state);

    auto projectile = std::find_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned;
        });
    REQUIRE(projectile != result.visualEvents.end());
    CHECK(projectile->durationFrames == 30);
}

TEST_CASE("BattleFrameRunner_BorrowedExactCastRuleUsesPlanningCastThroughDelayedCommit",
          "[battle][core][runtime][borrow][exact-runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Melee),
        unit(1, 1, { 260, 20, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto cast = frameCastInput(0, 1);
    cast.ultimateSkill.id = 43;
    cast.ultimateSkill.attackAreaType = 0;
    cast.ultimateSkill.selectDistance = 1;
    cast.ultimateSkill.rangedStyle = false;
    cast.ultimateSkill.reach = state.movement.config.meleeAttackReach;
    configureRuntimeActionPlan(state, cast);
    auto& caster = state.units.requireCore(0);
    caster.animation.cooldown = 0;
    caster.vitals.mp = 100;
    caster.vitals.maxMp = 100;

    BorrowEffectRulesAction borrow;
    borrow.sourceUnits.kind = EffectSelectorKind::Enemies;
    borrow.sourceCount.flat = 1;
    borrow.filter.allowedActionCategories = {
        BorrowedRuleActionCategory::Cast,
    };
    borrow.propagation = CastPropagationPolicy::BorrowedUltimateRules;
    EffectRule borrowRule;
    borrowRule.id = EffectRuleId{ 1 };
    borrowRule.event = EffectEvent::CastPlanned;
    borrowRule.selector.kind = EffectSelectorKind::Self;
    borrowRule.actions = {
        EffectAction{ EffectActionValue{ StateMachineAction{ borrow } } },
    };
    state.effectRules.append({
        .kind = EffectSourceKind::Magic,
        .sourceId = 43,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    }, borrowRule);

    ModifyCastAction ranged;
    ranged.rangeMode = CastRangeMode::Ranged;
    EffectRule rangedRule;
    rangedRule.id = EffectRuleId{ 2 };
    rangedRule.event = EffectEvent::CastPlanned;
    rangedRule.selector.kind = EffectSelectorKind::Self;
    rangedRule.actions = { EffectAction{ EffectActionValue{ ranged } } };
    state.effectRules.append({
        .kind = EffectSourceKind::Magic,
        .sourceId = 18,
        .ownerUnitId = 1,
        .sourceTeam = 1,
    }, rangedRule);

    const auto plannedFrame = runBattleFrame(state);
    const auto* pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    const auto castId = pending->effectCast.provenance.castId;
    const int castFrame = pending->castFrame;
    CHECK(pending->effectCast.provenance.magicId == 43);
    CHECK(pending->effectCast.provenance.ultimate);
    CHECK(pending->operationType == BattleOperationType::RangedProjectile);
    CHECK(pending->skillPlan.forceRanged);
    CHECK(state.effectRules.castScopedRuleCount(castId) == 1);
    CHECK_FALSE(hasProjectilePresentationEvent(plannedFrame));

    caster.haveAction = true;
    caster.operationType = BattleOperationType::RangedProjectile;
    caster.animation.actType = 1;
    caster.animation.actFrame = castFrame;
    caster.animation.cooldown = 20;
    const auto committedFrame = runBattleFrame(state);

    CHECK(state.units.require(0).pendingCast() == nullptr);
    CHECK(hasProjectilePresentationEvent(committedFrame));
    REQUIRE_FALSE(state.attacks.attacks.empty());
    const auto& attack = state.attacks.attacks.front();
    CHECK(attack.state.operationType == BattleOperationType::RangedProjectile);
    CHECK(attack.provenance.cast.castId == castId);
    CHECK(attack.provenance.cast.magicId == 43);
    CHECK(state.effectRules.castScopedRuleCount(castId) == 1);
}

TEST_CASE("BattleFrameRunner_ForcedRangedMeleeKeepsRangedMovementProfile", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }),
        unit(1, 1, { 360, 20, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.selectDistance = 1;
    configureRuntimeActionPlan(state, cast);
    ModifyCastAction forceRanged;
    forceRanged.rangeMode = CastRangeMode::Ranged;
    forceRanged.projectileSpeedPct = 500;
    forceRanged.minimumSelectDistance = 10;
    addTestOwnerRule(
        state,
        0,
        9300,
        1,
        EffectEvent::CastPlanned,
        EffectActionValue{ forceRanged });
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).style == CombatStyle::Ranged);
    CHECK(state.units.requireCore(0).reach > state.movement.config.meleeAttackReach);
    CHECK(state.units.requireCore(0).motion.position.x == Catch::Approx(10.0));
    CHECK(state.units.requireCore(0).motion.position.y == Catch::Approx(20.0));
    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->operationType == BattleOperationType::RangedProjectile);
}

TEST_CASE("BattleFrameRunner_ForcedRangedAreaSkillKeepsExtendedMovementProfile", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }),
        unit(1, 1, { 360, 20, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 3;
    cast.normalSkill.selectDistance = 1;
    configureRuntimeActionPlan(state, cast);
    ModifyCastAction forceRanged;
    forceRanged.rangeMode = CastRangeMode::Ranged;
    forceRanged.projectileSpeedPct = 500;
    forceRanged.minimumSelectDistance = 10;
    addTestOwnerRule(
        state,
        0,
        9300,
        1,
        EffectEvent::CastPlanned,
        EffectActionValue{ forceRanged });
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).style == CombatStyle::Ranged);
    CHECK(state.units.requireCore(0).reach > 360.0);
    CHECK(state.units.requireCore(0).motion.position.x == Catch::Approx(10.0));
    CHECK(state.units.requireCore(0).motion.position.y == Catch::Approx(20.0));
    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->operationType == BattleOperationType::RangedProjectile);
}

TEST_CASE("BattleFrameRunner_AreaSkillUsesHeavyReachForMovementAndCast", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 152, 20, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.ultimateSkill.attackAreaType = 3;
    cast.ultimateSkill.selectDistance = 1;
    configureRuntimeActionPlan(state, cast);
    auto& caster = state.units.requireCore(0);
    caster.animation.cooldown = 0;
    caster.vitals.mp = caster.vitals.maxMp;

    runBattleFrame(state);

    CHECK(caster.style == CombatStyle::Ranged);
    CHECK(caster.reach == Catch::Approx(SceneTileWidth * 4.0));
    CHECK(caster.motion.position.x == Catch::Approx(10.0));
    CHECK(caster.motion.position.y == Catch::Approx(20.0));
    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->operationType == BattleOperationType::TrackingProjectile);
    CHECK(pending->skillPlan.reach == Catch::Approx(SceneTileWidth * 4.0));
    CHECK(pending->skillPlan.blinkReach == Catch::Approx(SceneTileWidth * 4.0));
}

TEST_CASE("BattleFrameRunner_UltimateCommitDispatchesTypedTeamHealAtCooldownFinish", "[battle][core][runtime][magic]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 152, 20, 0 }),
        unit(2, 0, { 50, 20, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto cast = frameCastInput(0, 1);
    cast.ultimateSkill.attackAreaType = 3;
    cast.ultimateSkill.selectDistance = 1;
    configureRuntimeActionPlan(state, cast);

    auto& caster = state.units.requireCore(0);
    caster.animation.cooldown = 0;
    caster.vitals.hp = 50;
    caster.vitals.mp = caster.vitals.maxMp;
    auto& ally = state.units.requireCore(2);
    ally.vitals.hp = 70;
    ally.vitals.maxHp = 100;

    ChangeResourceAction teamHeal;
    teamHeal.resource = BattleResource::Hp;
    teamHeal.amount.flat = 12;
    teamHeal.kind = ResourceChangeKind::Restore;
    teamHeal.healKind = EffectHealKind::Team;
    EffectRule cooldownHeal;
    cooldownHeal.id = { 1 };
    cooldownHeal.event = EffectEvent::UltimateCooldownFinished;
    cooldownHeal.selector.kind = EffectSelectorKind::Allies;
    cooldownHeal.actions = { { EffectActionValue{ teamHeal } } };
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 401,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    state.effectRules.append(binding, cooldownHeal);

    runBattleFrame(state);
    auto* pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(state.effectRules.activationCount(binding, { 1 }) == 0);

    preparePendingCastCommitFrame(
        state,
        0,
        pending->operationType,
        pending->castFrame);
    runBattleFrame(state);

    CHECK(state.units.require(0).pendingCast() == nullptr);
    CHECK(state.effectRules.activationCount(binding, { 1 }) == 0);
    CHECK(state.units.require(0).isSkillCooldownUltimate());

    state.units.requireCore(0).animation.cooldown = 1;
    auto finish = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == 62);
    CHECK(state.units.requireCore(2).vitals.hp == 82);
    CHECK_FALSE(state.units.require(0).isSkillCooldownUltimate());
    CHECK(state.effectRules.activationCount(binding, { 1 }) == 1);
    CHECK(hasHealVisualEvent(finish, 0));
    CHECK(hasHealVisualEvent(finish, 2));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_SelectsCastTargetFromRuntimeUnits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 260, 20, 0 }),
        unit(2, 1, { 82, 20, 0 }),
    }));
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 10, 20, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 260, 20, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 82, 20, 0 }),
});
    state.attacks = attackWorld();
    auto cast = frameCastInput(0, -1);
    cast.targetPosition = {};
    cast.targetDistance = 0.0;
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.forceRanged = true;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->targetUnitId == 2);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastInputUsesCommittedFrameState", "[battle][core]")
{
    BattleRuntimeState state;
    auto caster = unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged);
    caster.reach = 400.0;
    configureRuntimeMovement(state, worldWith({
        caster,
        unit(1, 1, { 82, 20, 0 }),
    }));
    state.attacks = attackWorld();
    state.attacks.nextAttackId = 70;
    BattleStatusUnitState frozenStatus;
    frozenStatus.id = 0;
    frozenStatus.alive = true;
    frozenStatus.effects.setFrames(BattleStatusKind::Stun, 3);
    state.units.require(0).status = makeBattleStatusRuntimeUnit(frozenStatus);
    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK_FALSE(state.units.requireCore(0).haveAction);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CastOriginUsesPostMovementPosition", "[battle][core]")
{
    BattleRuntimeState state;
    auto caster = unit(0, 0, { 10, 20, 0 }, CombatStyle::Ranged);
    caster.reach = 400.0;
    configureRuntimeMovement(state, worldWith({
        caster,
        unit(1, 1, { 82, 20, 0 }),
    }));
    state.attacks = attackWorld();
    state.attacks.nextAttackId = 80;
    seedRuntimeUnitsFromWorld(state);
    auto cast = frameCastInput(0, 1);
    cast.unit.position = { -500, -500, 0 };
    cast.targetPosition = { -250, -250, 0 };
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.forceRanged = true;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    REQUIRE((state.units.require(0).pendingCast() != nullptr));
    CHECK_FALSE(hasProjectilePresentationEvent(result));
    auto& releaseUnit = state.units.requireCore(0);
    releaseUnit.haveAction = true;
    releaseUnit.operationType = BattleOperationType::RangedProjectile;
    releaseUnit.animation.actType = 1;
    releaseUnit.animation.actFrame = state.action.castConfig.castFrames[battleOperationIndex(BattleOperationType::RangedProjectile)];
    releaseUnit.animation.cooldown = 20;

    auto release = runBattleFrame(state);

    auto spawn = std::find_if(
        release.visualEvents.begin(),
        release.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned;
        });
    REQUIRE(spawn != release.visualEvents.end());
    CHECK(spawn->position.x == state.units.requireCore(0).motion.position.x + SceneTileWidth * 2.0);
    CHECK(spawn->position.y == state.units.requireCore(0).motion.position.y);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsActionInputsBeforeAttackTick", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    setTrackedPendingCast(state, 0, framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    const auto* spawn = findVisualEvent(result, BattleVisualEventType::ProjectileSpawned, 0);
    REQUIRE(spawn);
    CHECK(spawn->sourceUnitId == 0);
    REQUIRE(state.attacks.attacks.size() >= 1);
    CHECK(state.attacks.attacks.front().state.skillId == 101);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConsumesPreAttackLocalSpawnsInsideFrame", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    preparePendingCastCommitFrame(state, 0);

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    setTrackedPendingCast(state, 0, framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.nextFrame.queuedAttacks().empty());
    CHECK(state.nextFrame.queuedDamage().empty());
    REQUIRE(state.attacks.attacks.size() == 1);
    CHECK(state.attacks.attacks.front().state.attackSourceUnitId == 0);
    CHECK(state.attacks.attacks.front().state.skillId == 101);
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.sourceUnitId == 0;
        }));

    result = runBattleFrame(state);

    CHECK(std::none_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.sourceUnitId == 0;
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConsumesRuntimeOwnedActionDirectives", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    setTrackedPendingCast(state, 0, framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    REQUIRE(hasVisualEvent(result, BattleVisualEventType::ProjectileSpawned));
}

TEST_CASE("BattleFrameRunner_PlansCastFromRuntimeOwnedCastPlanInput", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(0).animation.cooldown = 0;
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->effectCast.provenance.magicId == 301);
}

TEST_CASE("BattleFrameRunner_RuntimeCastStartFacesTargetDirection", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 180, 180, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(0).motion.facing = { 1, 0, 0 };
    state.units.requireCore(0).animation.cooldown = 0;
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    cast.targetPosition = { 180, 180, 0 };
    cast.targetDistance = static_cast<double>((cast.targetPosition - cast.unit.position).norm());
    configureRuntimeActionPlan(state, cast);

    runBattleFrame(state);

    const auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->targetUnitId == 1);
    const auto& runtimeUnit = state.units.requireCore(0);
    CHECK(runtimeUnit.motion.facing.x > 0.6f);
    CHECK(runtimeUnit.motion.facing.y > 0.6f);
}

TEST_CASE("BattleFrameRunner_RuntimeCastPopulatesProjectileSpreadTargetsFromAliveEnemies", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
        unit(2, 1, { 220, 140, 0 }),
        unit(3, 1, { 220, 90, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(0).animation.cooldown = 0;
    state.units.requireCore(0).vitals.mp = state.units.requireCore(0).vitals.maxMp;
    state.units.requireCore(3).alive = false;

    auto cast = frameCastInput(0, 1);
    cast.ultimateSkill.attackAreaType = 3;
    cast.ultimateSkill.rangedStyle = true;
    cast.ultimateSkill.reach = 400.0;
    cast.ultimateSkill.selectDistance = 4;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto start = runBattleFrame(state);
    REQUIRE((state.units.require(0).pendingCast() != nullptr));
    CHECK_FALSE(hasProjectilePresentationEvent(start));

    auto& caster = state.units.requireCore(0);
    caster.haveAction = true;
    caster.operationType = BattleOperationType::TrackingProjectile;
    caster.animation.actType = 1;
    caster.animation.actFrame = state.action.castConfig.castFrames[battleOperationIndex(BattleOperationType::TrackingProjectile)];
    caster.animation.cooldown = 20;

    auto result = runBattleFrame(state);

    std::vector<BattleVisualEvent> spawned;
    std::copy_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        std::back_inserter(spawned),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned;
        });
    REQUIRE(spawned.size() == 2);
    CHECK(spawned[0].targetUnitId == 1);
    CHECK(spawned[1].targetUnitId == 2);
}

TEST_CASE("BattleFrameRunner_ClearsDashSpreadWhenRuntimeCastStarts", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(0).animation.cooldown = 0;
    state.units.require(0).movement.physics.movementDashSpreadFrames = 9;
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto result = runBattleFrame(state);

    REQUIRE((state.units.require(0).pendingCast() != nullptr));
    CHECK(state.units.require(0).movement.physics.movementDashSpreadFrames == 0);
}

TEST_CASE("BattleFrameRunner_RollsDashHitCountFromRuntimeStateWhenDashCastStarts", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Melee),
        unit(1, 1, { 300, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto& runtimeUnit = state.units.requireCore(0);
    runtimeUnit.animation.cooldown = 0;
    runtimeUnit.stats.speed = 360;
    addTestCastMobilityRule(state, 0, CastMobilityPolicy::DashAttack);

    auto cast = frameCastInput(0, 1);
    cast.unit.dashAttackEnabled = true;
    cast.unit.dashAttackReach = 350.0;
    cast.unit.dashHitCount = 1;
    cast.unit.dashVelocity = { 10.0f, 0.0f, 0.0f };
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.rangedStyle = false;
    cast.normalSkill.reach = 350.0;
    cast.normalSkill.actProperty = 0;
    configureRuntimeActionPlan(state, cast);

    auto start = runBattleFrame(state);

    REQUIRE((state.units.require(0).pendingCast() != nullptr));
    CHECK_FALSE(hasProjectilePresentationEvent(start));
    const auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->operationType == BattleOperationType::Dash);

    runtimeUnit.haveAction = true;
    runtimeUnit.operationType = BattleOperationType::Dash;
    runtimeUnit.animation.actType = 1;
    runtimeUnit.animation.actFrame = state.action.castConfig.castFrames[battleOperationIndex(BattleOperationType::Dash)];
    runtimeUnit.animation.cooldown = 20;

    auto result = runBattleFrame(state);

    const auto dashHitCount = std::count_if(
        state.attacks.attacks.begin(),
        state.attacks.attacks.end(),
        [](const BattleAttackInstance& attack)
        {
            return attack.state.operationType == BattleOperationType::Dash;
        });
    CHECK(dashHitCount == 3);
}

TEST_CASE("BattleFrameRunner_CommittedDashKeepsHitVectorWhenTargetMovesInsideMeleeReach", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Melee),
        unit(1, 1, { 300, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto& runtimeUnit = state.units.requireCore(0);
    runtimeUnit.animation.cooldown = 0;
    addTestCastMobilityRule(state, 0, CastMobilityPolicy::DashAttack);

    auto cast = frameCastInput(0, 1);
    cast.unit.dashAttackEnabled = true;
    cast.unit.dashAttackReach = 350.0;
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.rangedStyle = false;
    cast.normalSkill.reach = 350.0;
    configureRuntimeActionPlan(state, cast);

    auto start = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    REQUIRE(pending->operationType == BattleOperationType::Dash);
    CHECK_FALSE(hasProjectilePresentationEvent(start));

    state.units.setPosition(1, { 200, 100, 0 }, state.gridTransform);
    runtimeUnit.haveAction = true;
    runtimeUnit.operationType = BattleOperationType::Dash;
    runtimeUnit.animation.actType = 1;
    runtimeUnit.animation.actFrame = state.action.castConfig.castFrames[battleOperationIndex(BattleOperationType::Dash)];
    runtimeUnit.animation.cooldown = 20;

    auto result = runBattleFrame(state);

    const auto dashIt = std::find_if(
        state.attacks.attacks.begin(),
        state.attacks.attacks.end(),
        [](const BattleAttackInstance& attack)
        {
            return attack.state.operationType == BattleOperationType::Dash
                && attack.state.castSubrequestKind == BattleAttackCastSubrequestKind::DashHit;
        });
    REQUIRE(dashIt != state.attacks.attacks.end());
    CHECK(dashIt->state.velocity.norm() > TestMinimumVectorNorm);
    CHECK(state.units.require(0).movement.physics.postDashRetreatVelocity.norm() > TestMinimumVectorNorm);
}

TEST_CASE("BattleFrameRunner_RangedDashAttackCastsProjectileWithoutDashHits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 180, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(0).motion.facing = { 1, 0, 0 };
    state.units.requireCore(0).stats.speed = 180;
    state.units.requireCore(0).animation.cooldown = 0;
    addTestCastMobilityRule(state, 0, CastMobilityPolicy::DashAttack);

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    cast.targetPosition = { 180, 100, 0 };
    cast.targetDistance = 80.0;
    configureRuntimeActionPlan(state, cast);

    auto start = runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->operationType == BattleOperationType::RangedProjectile);
    CHECK_FALSE(hasProjectilePresentationEvent(start));
    auto& caster = state.units.requireCore(0);
    caster.haveAction = true;
    caster.operationType = BattleOperationType::RangedProjectile;
    caster.animation.actType = 1;
    caster.animation.actFrame = state.action.castConfig.castFrames[battleOperationIndex(BattleOperationType::RangedProjectile)];
    caster.animation.cooldown = 20;

    auto result = runBattleFrame(state);

    const auto dashHitCount = std::count_if(
        state.attacks.attacks.begin(),
        state.attacks.attacks.end(),
        [](const BattleAttackInstance& attack)
        {
            return attack.state.operationType == BattleOperationType::Dash;
        });
    CHECK(dashHitCount == 0);
}

TEST_CASE("BattleFrameRunner_MeleeDashCommitSchedulesPostDashRetreat", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Melee),
        unit(1, 1, { 300, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::Dash;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.id = 101;
    cast.normalSkill.attackAreaType = 0;
    cast.normalSkill.rangedStyle = false;
    cast.normalSkill.reach = 350.0;
    configureRuntimeActionPlan(state, cast);
    auto pending = framePendingCastAction();
    pending.operationType = BattleOperationType::Dash;
    pending.skillPlan = cast.normalSkill;
    setTrackedPendingCast(state, 0, std::move(pending));

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.require(0).movement.physics.postDashRetreatVelocity.x < -3.0f);
    CHECK(std::abs(state.units.require(0).movement.physics.postDashRetreatVelocity.y) > 1.0f);
    CHECK(state.units.require(0).movement.physics.postDashRetreatFrames == 11);
    CHECK(state.units.require(0).movement.physics.postDashChaosFrames == 6);
}

TEST_CASE("BattleFrameRunner_PostDashRetreatYieldsMovementPlannerToPhysics", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Melee),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.gridTransform = { SceneTileWidth, 64 };
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 210, 100, 0 }),
});
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);
    state.units.require(0).movement.physics.position = { 100, 100, 0 };
    state.units.require(0).movement.physics.velocity = { 0, 0, 0 };
    state.units.require(0).movement.physics.acceleration = { 0, 0, 0 };
    state.units.require(0).movement.physics.postDashRetreatVelocity = { -5, 0, 0 };
    state.units.require(0).movement.physics.postDashRetreatFrames = 2;
    state.units.require(1).movement.physics.position = { 210, 100, 0 };

    runBattleFrame(state);

    CHECK(state.units.requireCore(0).motion.position.x == Catch::Approx(95.0f));
    CHECK(state.units.requireCore(0).motion.velocity.x == Catch::Approx(-5.0f));
    CHECK(state.units.require(0).movement.physics.postDashRetreatFrames == 1);
}

TEST_CASE("BattleFrameRunner_StoresPendingCastIntentWhenCastStarts", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    runBattleFrame(state);
    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->effectCast.provenance.sourceUnitId == 0);
    CHECK(pending->targetUnitId == 1);
    CHECK(pending->effectCast.provenance.magicId == 301);
    CHECK(pending->operationType == BattleOperationType::RangedProjectile);
}

TEST_CASE("BattleFrameRunner_CastStartJittersPendingReleaseFrame", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.action.castConfig.castFrames = { 6, 6, 6, 6 };
    state.random = BattleRuntimeRandom(2u);

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.action.castConfig.castFrames = { 6, 6, 6, 6 };
    state.units.requireCore(0).animation.cooldown = 0;

    BattleRuntimeRandom expectedRandom(2u);
    const int baseCastFrame = state.action.castConfig.castFrames[battleOperationIndex(BattleOperationType::RangedProjectile)];
    const int expectedReleaseFrame = baseCastFrame + expectedRandom.nextInt(3) - 1;
    REQUIRE(expectedReleaseFrame != baseCastFrame);

    runBattleFrame(state);

    auto pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->castFrame == expectedReleaseFrame);

    auto& caster = state.units.requireCore(0);
    caster.animation.actFrame = expectedReleaseFrame - 1;
    auto early = runBattleFrame(state);
    CHECK_FALSE(hasProjectilePresentationEvent(early));
    REQUIRE(state.units.require(0).pendingCast() != nullptr);

    auto release = runBattleFrame(state);
    CHECK(hasVisualEvent(release, BattleVisualEventType::ProjectileSpawned));
    CHECK(state.units.require(0).pendingCast() == nullptr);
}

TEST_CASE("BattleFrameRunner_DualWieldUltimateRetainsCastIdentityWhileSuppressingAttackRules", "[battle][core][runtime][lifecycle]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 220, 150, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    ModifyAttackAction dualWield;
    dualWield.runtimeBehavior = DelayedAlternateAttackBehavior{
        .delayFrames = 6,
        .damagePct = 45,
        .attackerBlockGainChancePct = 50,
    };
    addTestOwnerRule(
        state,
        0,
        9400,
        1,
        EffectEvent::AttackCommitted,
        EffectActionValue{ dualWield });
    auto& caster = state.units.requireCore(0);
    caster.shield = 30;
    caster.animation.cooldown = 0;
    caster.vitals.mp = caster.vitals.maxMp;

    runBattleFrame(state);
    const auto* pending = state.units.require(0).pendingCast();
    REQUIRE(pending);
    REQUIRE(pending->effectCast.provenance.ultimate);
    caster.animation.actFrame = pending->castFrame;

    auto release = runBattleFrame(state);

    CHECK(state.units.requireCore(0).shield == 30);
    const auto followUp = std::ranges::find_if(
        state.nextFrame.queuedAttacks(),
        [](const BattleAttackSpawnRequest& request)
        {
            return request.initial.castSubrequestKind == BattleAttackCastSubrequestKind::DualWieldFollowUp;
        });
    REQUIRE(followUp != state.nextFrame.queuedAttacks().end());
    CHECK(followUp->spawnDelayFrames == 5);
    CHECK(followUp->attackerDualWieldBlockGainChancePct == 50);
    CHECK(followUp->initial.preferredTargetUnitId == 2);
    CHECK(followUp->provenance.cast.ultimate);
    CHECK(followUp->provenance.cast.origin == CastOriginKind::Ultimate);
    CHECK(followUp->provenance.cast.propagation == CastPropagationPolicy::SourceRules);
    CHECK(followUp->provenance.propagation
          == CastPropagationPolicy::SuppressUltimateRules);
    CHECK(followUp->provenance.origin == BattleAttackOriginKind::FollowUp);
    CHECK_FALSE(followUp->provenance.rootAttack);
    CHECK_FALSE(followUp->provenance.mainProjectile);
    CHECK_FALSE(std::ranges::any_of(release.logEvents, [](const BattleLogEvent& event)
        {
            return event.resourceId == BattleResourceSemanticId::Shield;
        }));
}

TEST_CASE("BattleFrameRunner_RefreshesRuntimeCastTargetAtCommitFrame", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto cast = frameCastInput(0, 1);
    cast.unit.position = { 100, 100, 0 };
    cast.targetPosition = { 220, 100, 0 };
    cast.targetDistance = 120.0;
    cast.normalSkill.attackAreaType = 1;
    cast.normalSkill.rangedStyle = true;
    cast.normalSkill.reach = 400.0;
    configureRuntimeActionPlan(state, cast);
    state.units.requireCore(0).animation.cooldown = 0;

    auto start = runBattleFrame(state);
    REQUIRE((state.units.require(0).pendingCast() != nullptr));
    CHECK_FALSE(hasProjectilePresentationEvent(start));
    REQUIRE((state.units.require(0).pendingCast() != nullptr));

    auto& caster = state.units.requireCore(0);
    caster.haveAction = true;
    caster.operationType = BattleOperationType::RangedProjectile;
    caster.animation.actType = 1;
    caster.animation.actFrame = state.action.castConfig.castFrames[battleOperationIndex(BattleOperationType::RangedProjectile)];
    caster.animation.cooldown = 20;
    state.units.requireCore(1).motion.position = { 220, 220, 0 };

    auto release = runBattleFrame(state);

    std::vector<BattleVisualEvent> spawned;
    std::copy_if(
        release.visualEvents.begin(),
        release.visualEvents.end(),
        std::back_inserter(spawned),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned;
        });
    REQUIRE(spawned.size() == 3);
    CHECK(spawned[0].velocity.x > 0.0f);
    CHECK(spawned[0].velocity.y > 0.0f);
}

TEST_CASE("BattleFrameRunner_CommitsRuntimeOwnedPendingCastInputWithoutSceneDirective", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    setTrackedPendingCast(state, 0, framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    REQUIRE(hasVisualEvent(result, BattleVisualEventType::ProjectileSpawned));
}

TEST_CASE("BattleFrameRunner_RetargetsPendingCastWhenOriginalTargetDiesBeforeCommit", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
        unit(2, 1, { 220, 220, 0 }),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    preparePendingCastCommitFrame(state, 0);
    auto& caster = state.units.requireCore(0);
    caster.operationCount = 1;
    caster.vitals.mp = 20;
    caster.vitals.maxMp = 50;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    setTrackedPendingCast(state, 0, framePendingCastAction());
    auto& target = state.units.requireCore(1);
    target.alive = false;
    target.vitals.hp = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.requireCore(0).operationCount == 1);
    CHECK(state.units.requireCore(0).vitals.mp == 25);
    auto request = std::find_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned;
        });
    REQUIRE(request != result.visualEvents.end());
    CHECK(request->velocity.x > 0.0f);
    CHECK(request->velocity.y > 0.0f);
}

TEST_CASE("BattleFrameRunner_CancelsPendingCastWhenNoLiveEnemyRemainsBeforeCommit", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    preparePendingCastCommitFrame(state, 0);
    auto& caster = state.units.requireCore(0);
    caster.operationCount = 1;
    caster.vitals.mp = 20;
    caster.vitals.maxMp = 50;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    ChangeResourceAction borrowedShield;
    borrowedShield.resource = BattleResource::Shield;
    borrowedShield.kind = ResourceChangeKind::Grant;
    borrowedShield.amount.flat = 10;
    EffectRule borrowedRule;
    borrowedRule.id = EffectRuleId{ 1 };
    borrowedRule.event = EffectEvent::HitBeforeDamage;
    borrowedRule.selector.kind = EffectSelectorKind::HitTarget;
    borrowedRule.actions = { EffectAction{ EffectActionValue{ borrowedShield } } };
    state.effectRules.append({
        .kind = EffectSourceKind::Magic,
        .sourceId = 18,
        .ownerUnitId = 1,
        .sourceTeam = 1,
    }, borrowedRule);
    setTrackedPendingCast(state, 0, framePendingCastAction());
    const auto castId = state.units.require(0).pendingCast()->effectCast.provenance.castId;
    BorrowedRuleFilter borrowFilter;
    borrowFilter.allowedActionCategories = {
        BorrowedRuleActionCategory::ResourceChange,
    };
    const std::array sourceUnitIds{ 1 };
    state.effectRules.bindBorrowedUltimateRules(
        castId,
        0,
        0,
        sourceUnitIds,
        borrowFilter,
        CastPropagationPolicy::BorrowedUltimateRules);
    REQUIRE(state.effectRules.castScopedRuleCount(castId) == 1);
    auto& target = state.units.requireCore(1);
    target.alive = false;
    target.vitals.hp = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.requireCore(0).operationCount == 1);
    CHECK(state.units.requireCore(0).vitals.mp == 20);
    CHECK_FALSE(state.units.requireCore(0).haveAction);
    CHECK(state.units.requireCore(0).operationType == BattleOperationType::None);
    CHECK(state.units.requireCore(0).animation.actType == -1);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
    CHECK(state.effectRules.castScopedRuleCount(castId) == 0);
    CHECK(state.castLifecycle.activeCastCount() == 0);
    CHECK(state.castLifecycle.trackedWorkCount() == 0);
}

TEST_CASE("BattleFrameRunner_CancelsPendingCastWhenCasterDiesBeforeActionFrame", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    preparePendingCastCommitFrame(state, 0);

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    setTrackedPendingCast(state, 0, framePendingCastAction());
    auto& caster = state.units.requireCore(0);
    caster.alive = false;
    caster.vitals.hp = 0;

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK_FALSE(state.units.requireCore(0).haveAction);
    CHECK(state.units.requireCore(0).operationType == BattleOperationType::None);
    CHECK(state.units.requireCore(0).animation.actType == -1);
    CHECK_FALSE(hasProjectilePresentationEvent(result));
}

TEST_CASE("BattleFrameRunner_PrunesPendingCastWhenCasterDiesDuringDamageLifecycle", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 220, 100, 0 }, CombatStyle::Ranged),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    preparePendingCastCommitFrame(state, 1, BattleOperationType::RangedProjectile, 0);
    configureRuntimeActionPlan(state, frameCastInput(1, 0));
    auto pending = framePendingCastAction();
    pending.targetUnitId = 0;
    setTrackedPendingCast(state, 1, std::move(pending));

    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.units.pendingCastCount() == 0);
    CHECK_FALSE(state.units.requireCore(1).haveAction);
    CHECK(state.units.requireCore(1).operationType == BattleOperationType::None);
    CHECK(state.units.requireCore(1).animation.actType == -1);
}

TEST_CASE("BattleFrameRunner_CommitsTypedResourceEffectsOnActionCommit", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;
    unit.vitals.mp = 5;
    unit.vitals.maxMp = 50;

    ChangeResourceAction restoreMp;
    restoreMp.resource = BattleResource::Mp;
    restoreMp.amount.flat = 8;
    restoreMp.kind = ResourceChangeKind::Restore;
    ChangeResourceAction grantShield;
    grantShield.resource = BattleResource::Shield;
    grantShield.amount.flat = 12;
    grantShield.kind = ResourceChangeKind::Grant;
    ChangeResourceAction grantInvincibility;
    grantInvincibility.resource = BattleResource::InvincibilityFrames;
    grantInvincibility.amount.flat = 12;
    grantInvincibility.kind = ResourceChangeKind::Grant;
    EffectRule committedResources;
    committedResources.id = { 1 };
    committedResources.event = EffectEvent::AttackCommitted;
    committedResources.selector.kind = EffectSelectorKind::Self;
    committedResources.actions = {
        { EffectActionValue{ restoreMp } },
        { EffectActionValue{ grantShield } },
        { EffectActionValue{ grantInvincibility } },
    };
    const auto binding = testOwnerRuleBinding(state, 0, 9600);
    state.effectRules.append(binding, committedResources);

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    setTrackedPendingCast(state, 0, framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.requireCore(0).vitals.mp == 19);
    CHECK(state.units.requireCore(0).shield == 12);
    CHECK(state.units.requireCore(0).invincible == 12);
    CHECK(state.effectRules.activationCount(binding, { 1 }) == 1);
    CHECK_FALSE(result.logEvents.empty());
}

TEST_CASE("BattleFrameRunner_TypedSpiralBleedCarriesCastLineageAndWork", "[battle][core][runtime][magic]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    ModifyAttackAction spiral;
    spiral.runtimeBehavior = ExpandingSpiralAttackBehavior{
        .projectileCount = 1,
        .bleedStacks = 2,
    };
    EffectRule spiralRule;
    spiralRule.id = { 1 };
    spiralRule.event = EffectEvent::AttackCommitted;
    spiralRule.selector.kind = EffectSelectorKind::Self;
    spiralRule.actions = { EffectAction{ EffectActionValue{ spiral } } };
    const EffectSourceBinding spiralBinding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 301,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    state.effectRules.append(spiralBinding, spiralRule);

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    auto pending = framePendingCastAction();
    pending.skillPlan.id = 301;
    setTrackedPendingCast(state, 0, std::move(pending), true);

    runBattleFrame(state);

    const auto& attacks = state.attacks.attacks;
    const auto spiralIt = std::find_if(
        attacks.begin(),
        attacks.end(),
        [](const BattleAttackInstance& attack)
        {
            return attack.state.scriptedBleedStacks == 2;
    });
    REQUIRE(spiralIt != attacks.end());
    REQUIRE(spiralIt->provenance.valid());
    CHECK(spiralIt->provenance.cast.magicId == 301);
    CHECK_FALSE(spiralIt->provenance.rootAttack);
    CHECK(spiralIt->provenance.origin == BattleAttackOriginKind::CastDerived);
    CHECK(spiralIt->provenance.propagation == CastPropagationPolicy::SourceHitRulesOnly);
    CHECK(spiralIt->provenance.sharedHitGroupId > 0);
    CHECK(spiralIt->castWork.valid());
    CHECK(state.effectRules.activationCount(spiralBinding, { 1 }) == 1);
}

TEST_CASE("BattleFrameRunner_EnemyMpDamageAllTriggersOnceOnActionCommit", "[battle][core][runtime]")
{
    auto makeState = []()
    {
        BattleRuntimeState state;
        configureRuntimeMovement(state, worldWith({
            unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
            unit(1, 1, { 220, 100, 0 }),
            unit(2, 1, { 240, 100, 0 }),
            unit(3, 0, { 120, 100, 0 }),
        }));
        state.attacks = attackWorld();
        seedRuntimeUnitsFromWorld(state);
        auto& unit = state.units.requireCore(0);
        unit.haveAction = true;
        unit.animation.actFrame = 6;
        unit.operationType = BattleOperationType::RangedProjectile;
        unit.animation.actType = 1;
        unit.animation.cooldown = 10;
        state.units.requireCore(0).vitals.mp = 30;
        state.units.requireCore(1).vitals.mp = 25;
        state.units.requireCore(2).vitals.mp = 6;
        state.units.requireCore(3).vitals.mp = 40;

        configureRuntimeActionPlan(state, frameCastInput(0, 1));
        setTrackedPendingCast(state, 0, framePendingCastAction());
        return state;
    };

    auto baseline = makeState();
    runBattleFrame(baseline);

    auto state = makeState();
    ChangeResourceAction drainMp;
    drainMp.resource = BattleResource::Mp;
    drainMp.amount.flat = 10;
    drainMp.kind = ResourceChangeKind::Remove;
    EffectRule drainEnemies;
    drainEnemies.id = { 1 };
    drainEnemies.event = EffectEvent::AttackCommitted;
    drainEnemies.selector.kind = EffectSelectorKind::Enemies;
    drainEnemies.actions = { { EffectActionValue{ drainMp } } };
    const auto binding = testOwnerRuleBinding(state, 0, 9601);
    state.effectRules.append(binding, drainEnemies);

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.mp == baseline.units.requireCore(0).vitals.mp);
    CHECK(state.units.requireCore(1).vitals.mp == baseline.units.requireCore(1).vitals.mp - 10);
    CHECK(state.units.requireCore(2).vitals.mp == 0);
    CHECK(state.units.requireCore(3).vitals.mp == baseline.units.requireCore(3).vitals.mp);
    CHECK(state.effectRules.activationCount(binding, { 1 }) == 1);
    CHECK_FALSE(result.logEvents.empty());
}

TEST_CASE("BattleFrameRunner_AppliesCastScopedMpRestoreAfterUltimateSpend", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto& unit = state.units.requireCore(0);
    preparePendingCastCommitFrame(state, 0);
    unit.vitals.mp = 100;
    unit.vitals.maxMp = 100;

    ChangeResourceAction restoreMp;
    restoreMp.resource = BattleResource::Mp;
    restoreMp.amount.flat = 8;
    restoreMp.kind = ResourceChangeKind::Restore;
    EffectRule restoreAfterUltimate;
    restoreAfterUltimate.id = { 1 };
    restoreAfterUltimate.event = EffectEvent::UltimateCommitted;
    restoreAfterUltimate.selector.kind = EffectSelectorKind::Self;
    restoreAfterUltimate.actions = { { EffectActionValue{ restoreMp } } };
    const auto binding = testOwnerRuleBinding(state, 0, 9602);
    state.effectRules.append(binding, restoreAfterUltimate);
    state.units.require(0).markUltimateCaster();

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.id = 101;
    configureRuntimeActionPlan(state, cast);
    auto action = framePendingCastAction();
    setTrackedPendingCast(state, 0, std::move(action), true);

    runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.requireCore(0).vitals.mp == 8);
    CHECK(state.effectRules.activationCount(binding, { 1 }) == 1);
}

TEST_CASE("BattleFrameRunner_CastScopedMpRestoreDoesNotChangeLaterSameFrameCastSelection", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 0, { 120, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    preparePendingCastCommitFrame(state, 0);
    state.units.requireCore(0).vitals.mp = 5;
    state.units.requireCore(0).vitals.maxMp = 100;
    state.units.requireCore(1).animation.cooldown = 0;
    state.units.requireCore(1).vitals.mp = 90;
    state.units.requireCore(1).vitals.maxMp = 100;

    ChangeResourceAction restoreMp;
    restoreMp.resource = BattleResource::Mp;
    restoreMp.amount.flat = 10;
    restoreMp.kind = ResourceChangeKind::Restore;
    EffectRule restoreTeam;
    restoreTeam.id = { 1 };
    restoreTeam.event = EffectEvent::AttackCommitted;
    restoreTeam.selector.kind = EffectSelectorKind::Allies;
    restoreTeam.actions = { { EffectActionValue{ restoreMp } } };
    state.effectRules.append(testOwnerRuleBinding(state, 0, 9603), restoreTeam);

    configureRuntimeActionPlan(state, frameCastInput(0, 2));
    configureRuntimeActionPlan(state, frameCastInput(1, 2));
    setTrackedPendingCast(state, 0, framePendingCastAction());

    runBattleFrame(state);

    const auto pending = state.units.require(1).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK_FALSE(pending->effectCast.provenance.ultimate);
    CHECK(pending->effectCast.provenance.magicId == 301);
    CHECK(state.units.requireCore(1).vitals.mp == 100);
}

TEST_CASE("BattleFrameRunner_CommitsRuntimeOwnedPendingCastSound", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    auto action = framePendingCastAction();
    action.skillPlan.soundId = 55;
    setTrackedPendingCast(state, 0, std::move(action));

    auto result = runBattleFrame(state);

    REQUIRE(result.attackSoundIds.size() == 1);
    CHECK(result.attackSoundIds[0] == 55);
}

TEST_CASE("BattleFrameRunner_ConsumesUltimateCasterWhenRuntimeOwnedCastCommits", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;
    unit.vitals.mp = 100;
    unit.vitals.maxMp = 100;
    state.units.require(0).markUltimateCaster();

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.id = 101;
    configureRuntimeActionPlan(state, cast);
    auto action = framePendingCastAction();
    setTrackedPendingCast(state, 0, std::move(action), true);

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.ultimateCasterCount() == 0);
    CHECK(state.units.requireCore(0).vitals.mp == 0);
}

TEST_CASE("BattleFrameRunner_AppliesCommittedNormalCastMpGain", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;
    unit.vitals.mp = 0;
    unit.vitals.maxMp = 100;

    auto cast = frameCastInput(0, 1);
    cast.normalSkill.id = 101;
    configureRuntimeActionPlan(state, cast);
    auto action = framePendingCastAction();
    setTrackedPendingCast(state, 0, std::move(action));

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    CHECK(state.units.requireCore(0).vitals.mp == state.action.castConfig.normalCastMpDelta + 1);
}

TEST_CASE("BattleFrameRunner_PrunesFinishedRuntimeAttacksAfterFrame", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 220, 100, 0 }),
    }));
    state.attacks = attackWorld();
    BattleAttackInstance attack{ ordinaryProjectilePayload() };
    attack.id = 77;
    attack.frame = 0;
    attack.state.attackSourceUnitId = 0;
    attack.state.preferredTargetUnitId = 0;
    attack.state.operationType = BattleOperationType::RangedProjectile;
    attack.state.position = { 100, 100, 0 };
    attack.state.velocity = { 0, 0, 0 };
    attack.state.totalFrame = 1;
    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(attack));

    auto result = runBattleFrame(state);

    REQUIRE(hasProjectilePresentationEvent(result));
    CHECK(std::any_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileExpired && event.effectId == 77;
        }));
    CHECK(state.attacks.attacks.empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsRuntimePendingCastInput", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 160, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    setTrackedPendingCast(state, 0, framePendingCastAction());

    auto result = runBattleFrame(state);

    CHECK(state.units.pendingCastCount() == 0);
    const auto* spawn = findVisualEvent(result, BattleVisualEventType::ProjectileSpawned);
    REQUIRE(spawn);
    CHECK(spawn->sourceUnitId == 0);
    REQUIRE(state.attacks.attacks.size() >= 1);
    CHECK(state.attacks.attacks.front().state.skillId == 101);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ClearsRecoveredActionFrameUnitState", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    auto& unit = state.units.requireCore(0);
    unit.haveAction = true;
    unit.animation.actFrame = 11;
    unit.animation.actType = 2;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.cooldown = 30;

    runBattleFrame(state);

    const auto& recovered = state.units.requireCore(0);
    CHECK(recovered.animation.actFrame == 12);
    CHECK_FALSE(recovered.haveAction);
    CHECK(recovered.animation.actType == -1);
    CHECK(recovered.operationType == BattleOperationType::None);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeadUnitActionCleanupClearsAllActionOwners", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
});
    auto& deadBefore = state.units.requireCore(1);
    deadBefore.animation.cooldown = 4;
    deadBefore.animation.cooldownMax = 12;
    deadBefore.animation.actFrame = 2;
    deadBefore.animation.actType = 3;
    deadBefore.operationType = BattleOperationType::Melee;
    deadBefore.haveAction = true;
    BattlePendingCastAction pending;
    pending.castFrame = 6;
    setTrackedPendingCast(state, 1, std::move(pending));
    state.units.require(1).markUltimateCaster();
    queuePendingDamage(state, lethalDamageInput(0, 1));

    runBattleFrame(state);

    const auto& dead = state.units.requireCore(1);
    CHECK(dead.animation.cooldown == 0);
    CHECK(dead.animation.cooldownMax == 0);
    CHECK(dead.animation.actFrame == 0);
    CHECK(dead.animation.actType == -1);
    CHECK(dead.operationType == BattleOperationType::None);
    CHECK_FALSE(dead.haveAction);
    CHECK(state.units.require(1).pendingCast() == nullptr);
    CHECK_FALSE(state.units.require(1).isUltimateCaster());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DamageDeathPrecedesBattleEndEvent", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 10);
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
});
    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto result = runBattleFrame(state);

    const auto damageLogs = damageLogsFor(result, 1);
    REQUIRE(damageLogs.size() == 1);
    CHECK(damageLogs[0].amount > 0);
    CHECK(state.units.requireCore(1).vitals.hp == 0);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);

    std::vector<BattleGameplayEventType> gameplayTypes;
    for (const auto& event : result.gameplayEvents)
    {
        gameplayTypes.push_back(event.type);
    }
    REQUIRE(gameplayTypes.size() >= 3);
    CHECK(gameplayTypes[gameplayTypes.size() - 3] == BattleGameplayEventType::DamageApplied);
    CHECK(gameplayTypes[gameplayTypes.size() - 2] == BattleGameplayEventType::UnitDied);
    CHECK(gameplayTypes[gameplayTypes.size() - 1] == BattleGameplayEventType::BattleEnded);
}

TEST_CASE("BattleFrameRunner_AcceptedHitResourcePresentationRequiresAppliedTransaction", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();

    auto& attacker = state.units.require(0).core;
    attacker.vitals.hp = 60;
    auto& defender = state.units.require(1).core;
    defender.animation.cooldown = 20;
    defender.animation.cooldownMax = 20;
    defender.haveAction = true;
    defender.operationType = BattleOperationType::Melee;
    defender.animation.actType = 1;

    auto accepted = preResolvedDamageInput(0, 1, 100, 20);
    accepted.attacker.vitals = { 60, 100, 10, 50 };
    accepted.defender.vitals = { 100, 100, 0, 50 };
    accepted.request.acceptedHit = true;
    accepted.request.hpOnHit = 30;
    accepted.request.cooldownExtendPct = 50;
    queuePendingDamage(state, accepted);

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == 90);
    CHECK(state.units.requireCore(1).animation.cooldown == 29);
    CHECK(hasLogText(result, "命中回血"));
    CHECK(hasLogText(result, "冷卻延長（+10幀）"));
    CHECK(hasHealVisualEvent(result, 0));
}

TEST_CASE("BattleFrameRunner_BlockedAcceptedHitSuppressesResourcePresentation", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();

    auto& attacker = state.units.require(0).core;
    attacker.vitals.hp = 60;
    auto& defender = state.units.require(1).core;
    defender.invincible = 10;
    defender.animation.cooldown = 20;
    defender.animation.cooldownMax = 20;
    defender.haveAction = true;
    defender.operationType = BattleOperationType::Melee;
    defender.animation.actType = 1;

    auto blocked = preResolvedDamageInput(0, 1, 100, 20);
    blocked.attacker.vitals = { 60, 100, 10, 50 };
    blocked.defender.vitals = { 100, 100, 0, 50 };
    blocked.defender.invincible = 10;
    blocked.defenderStatus.invincible = 10;
    blocked.request.acceptedHit = true;
    blocked.request.hpOnHit = 30;
    blocked.request.cooldownExtendPct = 50;
    queuePendingDamage(state, blocked);

    auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == 60);
    CHECK(state.units.requireCore(1).animation.cooldown == 19);
    CHECK_FALSE(hasLogText(result, "命中回血"));
    CHECK_FALSE(hasLogText(result, "冷卻延長（+10幀）"));
    CHECK_FALSE(hasHealVisualEvent(result, 0));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathClearsFrozenStatusAuthority", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
});
    state.units.require(0).status =
        makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(state.units.requireCore(0)));
    state.units.require(1).status =
        makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(state.units.requireCore(1)));
    queuePendingDamage(state, lethalDamageInput(0, 1));
    state.units.require(1).status.effects.setFrames(BattleStatusKind::Stun, 5, 8);

    runBattleFrame(state);

    CHECK_FALSE(state.units.require(1).frozen());
}

TEST_CASE("BattleFrameRunner_PublishesRenderComboFromRuntimeRecords", "[battle][core][runtime]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 210, 100, 0 }),
});
    state.units.requireCore(1).shield = 33;

    BattleDamageRuntimeUnit damage;
    damage.dualWieldBlocksRemaining = 1;
    state.units.require(0).damage = {};
    state.units.require(1).damage = damage;

    runBattleFrame(state);

    CHECK(state.units.requireCore(1).shield == 33);
    CHECK(state.units.require(1).damage.dualWieldBlocksRemaining == 1);
}

TEST_CASE("BattleFrameRunner_DualWieldBlockConsumesBlock", "[battle][core][runtime]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& defender = frame.state.units.require(1).damage;
    defender.dualWieldBlocksRemaining = 1;

    const auto result = runBattleFrame(frame.state);

    CHECK(frame.state.units.requireCore(1).vitals.hp == 100);
    CHECK(frame.state.units.require(1).damage.dualWieldBlocksRemaining == 0);
    const auto event = std::ranges::find_if(result.gameplayEvents, [](const BattleGameplayEvent& gameplay)
        {
            return gameplay.statusId == BattleStatusSemanticId::BlockedByDualWield;
        });
    REQUIRE(event != result.gameplayEvents.end());
    CHECK(event->targetUnitId == 1);
    CHECK(event->text == "互搏抵擋了本次傷害");
    CHECK(std::ranges::any_of(result.logEvents, [](const BattleLogEvent& log)
        {
            return BattleLogTest::textOf(log) == "互搏抵擋了本次傷害";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RunsMovementPhysicsInsideCore", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 200, 100, 0 }),
});
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(0).motion.acceleration = { 0, 0, -4 };
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(0).motion.acceleration = { 0, 0, -4 };
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);
    state.units.require(1).status.effects.setFrames(BattleStatusKind::Stun, 2);
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.1f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth * 1.5;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);
    state.units.require(0).movement.physics.position = { 100, 100, 0 };
    state.units.require(0).movement.physics.velocity = { 5, 0, 0 };
    state.units.require(0).movement.physics.acceleration = { 0, 0, -4 };
    state.units.require(0).movement.physics.movementDashFrames = 1;
    state.units.require(1).movement.physics.position = { 200, 100, 0 };
    state.units.require(1).movement.physics.velocity = { 5, 0, 0 };
    state.units.require(1).movement.physics.acceleration = { 0, 0, -4 };

    runBattleFrame(state);

    const auto& movedUnit = state.units.requireCore(0);
    CHECK(movedUnit.motion.position.x == 105.0f);
    CHECK(state.units.require(0).movement.physics.movementDashFrames == 0);
    CHECK(state.units.require(0).movement.physics.movementDashSpreadFrames == 6);

    const auto& stoppedUnit = state.units.requireCore(1);
    CHECK(state.units.require(1).frozenFrames() == 1);
    CHECK(stoppedUnit.motion.position.x == 200.0f);
    CHECK(stoppedUnit.motion.velocity.x == 0.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_KeepsMovingCorpsesInMovementPhysics", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    auto dead = unit(1, 1, { 200, 100, 0 });
    dead.alive = false;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        dead,
    }));
    state.attacks = attackWorld();
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.units.requireCore(1).motion.velocity = { 6, 0, 8 };
    state.units.requireCore(1).motion.acceleration = { 0, 0, -4 };
    state.units.require(1).movement.physics.velocity = { 6, 0, 8 };
    state.units.require(1).movement.physics.acceleration = { 0, 0, -4 };
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);

    const auto& corpse = state.units.requireCore(1);
    CHECK(corpse.motion.position.x == 206.0f);
    CHECK(corpse.motion.position.z == 8.0f);
    CHECK(state.units.require(1).id() == 1);
    CHECK(state.units.require(1).movement.active);
    CHECK(state.units.require(1).movement.physics.position.x == 206.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_InactivatesInertDeadMovementAgents", "[battle][core][movement][ownership]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    auto dead = unit(1, 1, { 200, 100, 0 });
    dead.alive = false;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        dead,
    }));
    state.attacks = attackWorld();
    state.units.requireCore(0).motion.velocity = { 5, 0, 0 };
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);

    REQUIRE(state.units.require(1).id() == 1);
    CHECK_FALSE(state.units.require(1).movement.active);
    CHECK(state.units.requireCore(1).motion.position.x == Catch::Approx(200.0f));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesDeathKickVelocity", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);
    state.units.requireCore(1).motion.velocity = { -50.0f, 20.0f, 40.0f };
    state.units.require(1).movement.physics.velocity = state.units.requireCore(1).motion.velocity;
    state.units.requireCore(0).stats.speed = 0;
    state.units.requireCore(1).stats.speed = 0;

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    const auto& dead = state.units.requireCore(1);
    const double deathSpeed = std::sqrt(
        dead.motion.velocity.x * dead.motion.velocity.x
        + dead.motion.velocity.y * dead.motion.velocity.y
        + dead.motion.velocity.z * dead.motion.velocity.z);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.x > dead.motion.velocity.z);
    CHECK(dead.motion.velocity.y != Catch::Approx(20.0));
    CHECK(dead.motion.velocity.x > 0.0f);
    CHECK(dead.motion.velocity.z > 0.0f);
    CHECK(dead.motion.velocity.z <= 6.0f);
    CHECK(deathSpeed == Catch::Approx(20.0 / 3.0 + 5.0));
    CHECK(dead.motion.position.x == Catch::Approx(200.0f));
    CHECK(dead.motion.position.z == Catch::Approx(36.0f));
    CHECK(state.units.require(1).id() == 1);
    CHECK(state.units.require(1).movement.physics.velocity.z == dead.motion.velocity.z);

}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesFallbackDeathKickForAttackerlessDamage", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = OptionalDamageAttackerUnitId,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    runBattleFrame(state);

    const auto& dead = state.units.requireCore(1);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.x > 0.0f);
    CHECK(dead.motion.velocity.y == Catch::Approx(0.0f));
    CHECK(dead.motion.velocity.z > 0.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ClampsDeathKickVelocity", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 16;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(16 * 16, 1);

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 1000,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 500;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    auto result = runBattleFrame(state);

    const auto damageAmounts = damageLogAmountsFor(result, 1);
    REQUIRE(damageAmounts.size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    const auto& dead = state.units.requireCore(1);
    CHECK(damageAmounts[0] / 3.0 + 5.0 > 75.0);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.norm() == Catch::Approx(75.0));
    CHECK(dead.motion.velocity.z == Catch::Approx(6.0));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_SeedsDeathKickForNextFramePhysics", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
        unit(2, 0, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 16;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(16 * 16, 1);

    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 2,
            .defenderUnitId = 1,
            .baseDamage = 30,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    writeBattleDamageRuntimeUnit(
        state.units.require(1).damage,
        makeBattleDamageUnitState(state.units.requireCore(1), static_cast<const BattleDamageRuntimeUnit*>(nullptr)));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());

    const auto& dead = state.units.requireCore(1);
    CHECK_FALSE(dead.alive);
    CHECK(dead.motion.velocity.norm() > 0.01f);
    CHECK(dead.motion.position.z >= 0.0f);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_MovingCorpsePhysicsPersistsIntoRuntime", "[battle][core][movement]")
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    auto dead = unit(1, 1, { 200, 100, 0 });
    dead.alive = false;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        dead,
    }));
    state.attacks = attackWorld();
    state.units.requireCore(1).motion.velocity = { 6, 0, 8 };
    state.units.requireCore(1).motion.acceleration = { 0, 0, -4 };
    state.units.require(1).movement.physics.velocity = { 6, 0, 8 };
    state.units.require(1).movement.physics.acceleration = { 0, 0, -4 };
    state.movementPhysics.config.gravity = -4.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);
    const auto& firstFrameCorpse = state.units.requireCore(1);
    CHECK(firstFrameCorpse.motion.position.z == 8.0f);

    runBattleFrame(state);

    const auto& deadUnit = state.units.requireCore(1);
    CHECK(deadUnit.motion.position.z == 12.0f);
    CHECK(deadUnit.motion.velocity.z == 0.0f);
    CHECK(state.units.require(1).movement.physics.position.z == deadUnit.motion.position.z);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_StoresDamageApplicationResultInFrameState", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
        unit(2, 0, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.damage.sortPendingDamageByDefenderMagnitude = true;
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
        runtimeUnitSnapshot(2, 0, 80, { 120, 100, 0 }),
});

    auto first = lethalDamageInput(0, 1);
    first.request.baseDamage = 3;
    first.defender.vitals.hp = 10;
    first.defenderStatus.hp = 10;
    auto second = lethalDamageInput(2, 1);
    second.request.baseDamage = 4;
    second.attacker.id = 2;
    second.attacker.vitals.hp = 80;
    second.attacker.vitals.maxHp = 100;
    second.defender.vitals.hp = 10;
    second.defenderStatus.hp = 10;

    BattleDamagePresentationInput firstPresentation;
    firstPresentation.enabled = true;
    firstPresentation.skillName = "先手";
    firstPresentation.segments = battleLogText("第一段");
    firstPresentation.normalDamageColor = { 10, 20, 30, 255 };
    firstPresentation.normalDamageTextSize = 22;

    BattleDamagePresentationInput secondPresentation;
    secondPresentation.enabled = true;
    secondPresentation.skillName = "終段";
    secondPresentation.segments = battleLogText("第二段");
    secondPresentation.critical = true;
    secondPresentation.criticalMultiplier = 150;
    secondPresentation.emphasizedDamageColor = { 40, 50, 60, 255 };
    secondPresentation.emphasizedDamageTextSize = 33;

    queuePendingDamage(state, first, firstPresentation);
    queuePendingDamage(state, second, secondPresentation);

    auto result = runBattleFrame(state);

    CHECK(damageLogSourceIdsFor(result, 1) == std::vector<int>{ 2, 0 });
    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 4, 3 });
    CHECK(state.units.requireCore(1).vitals.hp == 3);
    CHECK(std::any_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::DamageNumber
                && event.targetUnitId == 1
                && event.amount == 4
                && event.criticalMultiplier == 150
                && event.textSize == 33
                && event.color.r == 40;
        }));
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.skillName == "終段"
                && BattleLogTest::textOf(event) == "第二段";
        }));
    CHECK_FALSE(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::UnitDied;
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathPreventionKeepsRuntimeUnitAlive", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 99,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    auto& runtime = state.units.require(1).damage;
    runtime.deathPrevention = true;
    runtime.deathPreventionFrames = 30;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.requireCore(1).alive);
    CHECK(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    const auto& defender = state.units.requireCore(1);
    CHECK(defender.alive);
    CHECK(defender.vitals.hp == 1);
    CHECK(defender.invincible == 30);
    CHECK(runtime.deathPreventionUsed);
    CHECK(runtime.deathPreventionFrames == 30);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathPreventionUsedRuntimeDoesNotTriggerAgain", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 99,
            .preResolvedDamage = true,
        },
    });
    state.units.requireCore(1).vitals.hp = 20;
    auto& runtime = state.units.require(1).damage;
    runtime.deathPrevention = true;
    runtime.deathPreventionFrames = 30;
    runtime.deathPreventionUsed = true;

    auto result = runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK(runtime.deathPreventionUsed);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DeathAoeProjectileDamagesOnNextFrame", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
        unit(2, 1, { 700, 700, 0 }),
        unit(3, 0, { 140, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100.0f, 100.0f, 0.0f }),
        runtimeUnitSnapshot(1, 1, 10, { 120.0f, 100.0f, 0.0f }),
        runtimeUnitSnapshot(2, 1, 100, { 700.0f, 700.0f, 0.0f }),
        runtimeUnitSnapshot(3, 0, 100, { 140.0f, 100.0f, 0.0f }),
});
    queuePendingDamage(state, lethalDamageInput(0, 1));
    appendDeathBlastRule(
        state.effectRules,
        {
            .kind = EffectSourceKind::Combo,
            .sourceId = 9001,
            .ownerUnitId = 1,
            .sourceTeam = 1,
        },
        50,
        2,
        6);
    state.projectileFollowUps.projectileSpeed = SceneProjectileSpeed;
    state.projectileFollowUps.minimumProjectileFrames = 20;
    state.projectileFollowUps.areaProjectileFramePadding = 15;
    state.projectileFollowUps.areaSpawnDistance = SceneTileWidth;

    auto result = runBattleFrame(state);

    REQUIRE(state.nextFrame.queuedAttacks().size() == 2);
    const auto& first = state.nextFrame.queuedAttacks()[0];
    const auto& second = state.nextFrame.queuedAttacks()[1];
    CHECK(first.initial.attackSourceUnitId == 1);
    CHECK(first.initial.preferredTargetUnitId == 0);
    CHECK(first.initial.scriptedDamage == 50);
    CHECK_FALSE(first.initial.scriptedDamageAppliesModifiers);
    CHECK_FALSE(first.initial.scriptedDamageTriggersDefenseEffects);
    CHECK(first.initial.scriptedStunFrames == 6);
    CHECK(second.initial.preferredTargetUnitId == 3);
    REQUIRE(first.provenance.valid());
    REQUIRE(second.provenance.valid());
    CHECK(first.provenance.cast.castId == second.provenance.cast.castId);
    CHECK(first.provenance.cast.rootCastId == first.provenance.cast.castId);
    CHECK_FALSE(first.provenance.cast.parentCastId);
    CHECK(first.provenance.cast.origin == CastOriginKind::Echo);
    CHECK(first.provenance.cast.propagation == CastPropagationPolicy::NoEffectRules);
    CHECK(first.provenance.attackOrdinal == 0);
    CHECK(second.provenance.attackOrdinal == 1);
    CHECK(first.provenance.rootAttack);
    CHECK_FALSE(second.provenance.rootAttack);
    CHECK_FALSE(first.provenance.mainProjectile);
    CHECK_FALSE(second.provenance.mainProjectile);
    CHECK(first.provenance.propagation == CastPropagationPolicy::NoEffectRules);
    CHECK(second.provenance.propagation == CastPropagationPolicy::NoEffectRules);
    CHECK(first.castWork.valid());
    CHECK(second.castWork.valid());
    CHECK(state.castLifecycle.outstandingWork(first.provenance.cast.castId) == 2);
    CHECK(std::none_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.sourceUnitId == 1
                && event.targetUnitId == 0;
        }));
    CHECK(damageLogsFor(result, 0).empty());

    result = runBattleFrame(state);

    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.targetUnitId == 0
                && event.amount == 50;
        }));
    CHECK(state.units.requireCore(0).vitals.hp == 50);
    CHECK(state.units.requireCore(3).vitals.hp == 50);
}

TEST_CASE("BattleFrameRunner_TargetlessDeathAoeCancelsItsEchoRootCast", "[battle][core][lifecycle]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 1, { 700, 700, 0 }),
        unit(1, 1, { 100, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 1, 100, { 700.0f, 700.0f, 0.0f }),
        runtimeUnitSnapshot(1, 1, 10, { 100.0f, 100.0f, 0.0f }),
    });
    queuePendingDamage(state, lethalDamageInput(0, 1));
    appendDeathBlastRule(
        state.effectRules,
        {
            .kind = EffectSourceKind::Combo,
            .sourceId = 9002,
            .ownerUnitId = 1,
            .sourceTeam = 1,
        },
        50,
        2,
        0);
    state.projectileFollowUps.projectileSpeed = SceneProjectileSpeed;
    state.projectileFollowUps.minimumProjectileFrames = 20;
    state.projectileFollowUps.areaProjectileFramePadding = 15;
    state.projectileFollowUps.areaSpawnDistance = SceneTileWidth;

    runBattleFrame(state);

    CHECK(state.result.ended);
    CHECK(state.nextFrame.queuedAttacks().empty());
    const auto snapshot = state.castLifecycle.snapshot();
    const auto cancelled = std::ranges::find_if(
        snapshot.retiredCasts,
        [](const BattleCastRuntime& cast)
        {
            return cast.provenance.origin == CastOriginKind::Echo;
        });
    REQUIRE(cancelled != snapshot.retiredCasts.end());
    CHECK(cancelled->provenance.propagation == CastPropagationPolicy::NoEffectRules);
    CHECK(cancelled->terminalReason == BattleCastTerminalReason::PlannedCastCancelled);
    CHECK(cancelled->cancelledBeforeCommit);
    CHECK_FALSE(cancelled->continuationDispatched);
    CHECK_FALSE(cancelled->settledDispatched);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CommitsRuntimeAutoUltimateReadyInsideCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    configureAutoUltimateActionRuntime(state, 1, 0);
    addTestPeriodicAutoUltimateRule(state, 1);

    auto result = runBattleFrame(state);

    REQUIRE(state.nextFrame.queuedAttacks().size() == 4);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.attackSourceUnitId == 1);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.skillId == 401);
    CHECK(state.nextFrame.queuedAttacks()[1].initial.castSubrequestKind == BattleAttackCastSubrequestKind::ExtraProjectile);
    CHECK(state.nextFrame.queuedAttacks()[1].initial.strengthPct == 35);
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 1
                && BattleLogTest::textOf(event) == "自動絕招·絕招";
        }));
    REQUIRE(result.attackSoundIds.size() == 1);
    CHECK(result.attackSoundIds[0] == 55);
}

TEST_CASE("BattleFrameRunner_AutoUltimateUsesTypedPlanCommitModifiersAndTrackedContext", "[battle][core][effect][auto-ultimate]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    configureAutoUltimateActionRuntime(state, 1, 0);
    addTestPeriodicAutoUltimateRule(state, 1);

    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 401,
        .ownerUnitId = 1,
        .sourceTeam = 1,
    };
    ModifyCastAction castModifier;
    castModifier.mpCost = EffectNumber{ .flat = 15 };
    ModifyAttackAction plannedAttackModifier;
    plannedAttackModifier.strengthPct = 50;
    EffectRule plannedRule;
    plannedRule.id = { 1 };
    plannedRule.event = EffectEvent::CastPlanned;
    plannedRule.selector.kind = EffectSelectorKind::Self;
    plannedRule.actions = {
        { EffectActionValue{ castModifier } },
        { EffectActionValue{ plannedAttackModifier } },
    };
    state.effectRules.append(binding, plannedRule);

    ModifyAttackAction attackCommittedModifier;
    attackCommittedModifier.strengthPct = 133;
    EffectRule attackCommittedRule;
    attackCommittedRule.id = { 2 };
    attackCommittedRule.event = EffectEvent::AttackCommitted;
    attackCommittedRule.selector.kind = EffectSelectorKind::Self;
    attackCommittedRule.actions = {
        { EffectActionValue{ attackCommittedModifier } },
    };
    state.effectRules.append(binding, attackCommittedRule);

    ModifyAttackAction committedAttackModifier;
    committedAttackModifier.strengthPct = 50;
    ChangeResourceAction grantShield;
    grantShield.resource = BattleResource::Shield;
    grantShield.kind = ResourceChangeKind::Grant;
    grantShield.amount.flat = 7;
    DealDamageAction committedDamage;
    committedDamage.amount.flat = 9;
    committedDamage.kind = BattleDamageKind::Effect;
    EffectRule committedRule;
    committedRule.id = { 3 };
    committedRule.event = EffectEvent::UltimateCommitted;
    committedRule.selector.kind = EffectSelectorKind::Self;
    committedRule.actions = {
        { EffectActionValue{ committedAttackModifier } },
        { EffectActionValue{ grantShield } },
        { EffectActionValue{ committedDamage } },
    };
    state.effectRules.append(binding, committedRule);

    REQUIRE(state.units.requireCore(1).vitals.mp < state.units.requireCore(1).vitals.maxMp);
    const int shieldBefore = state.units.requireCore(1).shield;
    runBattleFrame(state);

    CHECK(state.effectRules.activationCount(binding, { 1 }) == 1);
    CHECK(state.effectRules.activationCount(binding, { 2 }) == 1);
    CHECK(state.effectRules.activationCount(binding, { 3 }) == 1);
    CHECK(state.units.requireCore(1).shield == shieldBefore + 7);
    REQUIRE(state.nextFrame.queuedDamage().size() == 1);
    CHECK(state.nextFrame.queuedDamage()[0].request.defenderUnitId == 1);
    CHECK(state.nextFrame.queuedDamage()[0].request.baseDamage == 9);
    REQUIRE(state.nextFrame.queuedAttacks().size() == 4);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.strengthPct == 33);
    CHECK(state.nextFrame.queuedAttacks()[1].initial.strengthPct == 11);

    const auto& root = state.nextFrame.queuedAttacks()[0];
    REQUIRE(root.provenance.valid());
    CHECK(root.provenance.cast.ultimate);
    CHECK(root.provenance.cast.magicId == 401);
    CHECK(root.provenance.rootAttack);
    // The manually injected hit still owns a live attack token during its
    // post-impact lifetime, so its independent root context remains valid.
    // Every retained context must still name an active cast; only the auto
    // ultimate context is relevant to the assertions below.
    REQUIRE(state.effectIntegration.casts.size() == 2);
    CHECK(std::ranges::all_of(
        state.effectIntegration.casts,
        [&](const auto& entry)
        {
            return state.castLifecycle.containsCast(entry.first);
        }));
    const auto context = state.effectIntegration.casts.find(root.provenance.cast.castId);
    REQUIRE(context != state.effectIntegration.casts.end());
    CHECK(context->second.originalTargetUnitId == 0);
    CHECK(context->second.skill.id == 401);
    CHECK(context->second.operationType == BattleOperationType::RangedProjectile);
    CHECK(state.castLifecycle.outstandingWork(root.provenance.cast.castId) == 5);
    const auto lifecycle = state.castLifecycle.snapshot();
    CHECK(std::ranges::count_if(
        lifecycle.work,
        [&](const BattleCastWorkSnapshot& work)
        {
            return work.token.castId == root.provenance.cast.castId
                && work.kind == CastWorkKind::DelayedEffectCommand;
        }) == 1);
}

TEST_CASE("BattleFrameRunner_AutoUltimatePreservesConfiguredAttackAreaOperationWhenForcedRanged", "[battle][core][effect][auto-ultimate]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    auto cast = frameCastInput(1, 0);
    cast.ultimateSkill.attackAreaType = 0;
    cast.ultimateSkill.rangedStyle = false;
    configureRuntimeActionPlan(state, cast);
    addTestPeriodicAutoUltimateRule(state, 1);

    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 401,
        .ownerUnitId = 1,
        .sourceTeam = 1,
    };
    ModifyCastAction forceRanged;
    forceRanged.rangeMode = CastRangeMode::Ranged;
    EffectRule rule;
    rule.id = { 1 };
    rule.event = EffectEvent::CastPlanned;
    rule.selector.kind = EffectSelectorKind::Self;
    rule.actions = { { EffectActionValue{ forceRanged } } };
    state.effectRules.append(binding, rule);

    runBattleFrame(state);

    CHECK(state.effectRules.activationCount(binding, { 1 }) == 0);
    REQUIRE_FALSE(state.nextFrame.queuedAttacks().empty());
    CHECK(state.nextFrame.queuedAttacks()[0].initial.operationType
          == BattleOperationType::Melee);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DropsDeferredAutoUltimateWhenBattleEndsFirst", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 180, 100, 0 }),
        unit(2, 0, { 120, 120, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 180, 100, 0 }),
        runtimeUnitSnapshot(2, 0, 100, { 120, 120, 0 }),
});
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 10);
    state.units.require(2).status = statusRuntimeSnapshot(2, 100);

    queuePendingDamage(state, lethalDamageInput(0, 1));
    configureAutoUltimateActionRuntime(state, 2, 1);
    addTestPeriodicAutoUltimateRule(state, 2);

    auto result = runBattleFrame(state);

    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);
    CHECK(state.nextFrame.queuedAttacks().empty());
    CHECK(std::none_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 2
                && BattleLogTest::textOf(event) == "自動絕招·絕招";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_BattleEndEventEmitsOnce", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 210, 100, 0 }),
});
    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto first = runBattleFrame(state);
    auto second = runBattleFrame(state);

    int firstEndEvents = 0;
    for (const auto& event : first.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded)
        {
            ++firstEndEvents;
        }
    }
    int secondEndEvents = 0;
    for (const auto& event : second.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded)
        {
            ++secondEndEvents;
        }
    }

    CHECK(firstEndEvents == 1);
    CHECK(secondEndEvents == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsProjectileGameplayEventsSeparatelyFromPresentation", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    BattleAttackInstance expiringProjectile{ ordinaryProjectilePayload() };
    expiringProjectile.id = 20;
    expiringProjectile.state.attackSourceUnitId = 0;
    expiringProjectile.state.totalFrame = 1;
    expiringProjectile.noHurt = true;
    expiringProjectile.state.position = { 300, 100, 0 };
    expiringProjectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(projectile));
    appendTrackedAttack(state, std::move(expiringProjectile));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() == 4);
    REQUIRE(result.logEvents.empty());
    REQUIRE(result.visualEvents.size() == 4);

    CHECK(result.gameplayEvents[0].type == BattleGameplayEventType::ProjectileMoved);
    CHECK(result.gameplayEvents[0].effectId == 10);
    CHECK(result.gameplayEvents[0].sourceUnitId == 0);
    CHECK(result.gameplayEvents[0].position.x == 105.0f);
    CHECK(result.visualEvents[0].type == BattleVisualEventType::ProjectileMoved);

    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::ProjectileHit);
    CHECK(result.gameplayEvents[1].effectId == 10);
    CHECK(result.gameplayEvents[1].sourceUnitId == 0);
    CHECK(result.gameplayEvents[1].targetUnitId == 1);
    CHECK(result.visualEvents[1].type == BattleVisualEventType::ProjectileHit);

    CHECK(result.gameplayEvents[2].type == BattleGameplayEventType::ProjectileMoved);
    CHECK(result.gameplayEvents[2].effectId == 20);
    CHECK(result.visualEvents[2].type == BattleVisualEventType::ProjectileMoved);

    CHECK(result.gameplayEvents[3].type == BattleGameplayEventType::ProjectileExpired);
    CHECK(result.gameplayEvents[3].effectId == 20);
    CHECK(result.visualEvents[3].type == BattleVisualEventType::ProjectileExpired);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_QueuesHitGeneratedProjectilesForNextFrame", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 140, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    ModifyAttackAction nearbyTracking;
    nearbyTracking.runtimeBehavior = NearbyTrackingAttackBehavior{
        .rangePixels = 80,
        .damagePct = 45,
    };
    addTestOwnerRule(
        state,
        0,
        9500,
        1,
        EffectEvent::MainProjectileBeforeDamage,
        EffectActionValue{ nearbyTracking });

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 480;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.visualEffectId = 44;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    REQUIRE(state.nextFrame.queuedAttacks().size() == 2);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.attackSourceUnitId == 0);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.preferredTargetUnitId == 1);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.suppressNearbyTrackingProjectileProc);
    CHECK(state.nextFrame.queuedAttacks()[1].initial.attackSourceUnitId == 0);
    CHECK(state.nextFrame.queuedAttacks()[1].initial.preferredTargetUnitId == 2);
    CHECK(state.nextFrame.queuedAttacks()[1].initial.suppressNearbyTrackingProjectileProc);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ResolvesHitEventsWithFrameHitInputs", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 840;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    const auto damageLogs = damageLogsFor(result, 1);
    REQUIRE(damageLogs.size() == 1);
    CHECK(damageLogs[0].sourceUnitId == 0);
    CHECK(damageLogs[0].amount > 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesHitDamageInsideSameFrame", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;

    auto result = runBattleFrame(state);

    const auto damageLogs = damageLogsFor(result, 1);
    REQUIRE(damageLogs.size() == 1);
    CHECK(damageLogs[0].amount > 0);
    CHECK(state.units.requireCore(1).vitals.hp < 100);
    CHECK(state.nextFrame.queuedDamage().empty());
}

TEST_CASE("BattleFrameRunner_ExpiresDamageAbsorptionIntoSameFrameRandomPureDamage", "[battle][core][effect][absorption]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
        unit(2, 1, { 300, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.movement.frame = 9;
    state.random = BattleRuntimeRandom(97);
    for (int targetUnitId : { 1, 2 })
    {
        state.units.requireCore(targetUnitId).shield = 10;
        addTypedAttributeModifier(
            state,
            targetUnitId,
            BattleAttribute::DamageReduction,
            AttributeOperation::PercentAdd,
            25);
    }

    BattleDamageAbsorptionInstance absorption;
    absorption.sequence = 1;
    absorption.binding = {
        .kind = EffectSourceKind::Magic,
        .sourceId = 97,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    absorption.ruleId = { 97 };
    absorption.targetUnitId = 0;
    absorption.slot = EffectStateSlot::AbsorbedDamage;
    absorption.absorbedPct = 40;
    absorption.appliedFrame = 1;
    absorption.expiresFrameExclusive = 10;
    absorption.settleOnSourceDeath = true;
    absorption.settlementTarget.kind = EffectSelectorKind::Enemies;
    absorption.settlementTarget.count = 1;
    absorption.settlementTarget.tieBreak = EffectTieBreak::BattleRandom;
    absorption.settlementDamageKind = BattleDamageKind::Pure;
    absorption.returnedPct = 100;
    absorption.accumulatedDamage = 40;
    state.effectCommands.damageAbsorptions.push_back(absorption);
    state.effectCommands.nextDamageAbsorptionSequence = 2;

    runBattleFrame(state);

    CHECK(state.movement.frame == 10);
    CHECK(state.effectCommands.damageAbsorptions.empty());
    const auto hpAfterSettlement = std::array{
        state.units.requireCore(1).vitals.hp,
        state.units.requireCore(2).vitals.hp,
    };
    CHECK(std::ranges::count(hpAfterSettlement, 80) == 1);
    CHECK(std::ranges::count(hpAfterSettlement, 100) == 1);
    CHECK(std::ranges::count_if(std::array{
        state.units.requireCore(1).shield,
        state.units.requireCore(2).shield,
    }, [](int shield) { return shield == 0; }) == 1);
    CHECK(state.random.rawDrawCount() == 1);
    CHECK(state.nextFrame.queuedDamage().empty());

    runBattleFrame(state);
    CHECK(state.units.requireCore(1).vitals.hp == hpAfterSettlement[0]);
    CHECK(state.units.requireCore(2).vitals.hp == hpAfterSettlement[1]);
}

TEST_CASE("BattleFrameRunner_SourceDeathSettlesAccumulatedAndCurrentAbsorptionExactlyOnce", "[battle][core][effect][absorption]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
        unit(2, 1, { 300, 100, 0 }),
        unit(3, 0, { 120, 100, 0 }),
    }));
    state.attacks = attackWorld();
    state.random = BattleRuntimeRandom(97);
    for (int targetUnitId : { 1, 2 })
    {
        state.units.requireCore(targetUnitId).vitals.hp = 200;
        state.units.requireCore(targetUnitId).vitals.maxHp = 200;
    }

    BattleDamageAbsorptionInstance absorption;
    absorption.sequence = 1;
    absorption.binding = {
        .kind = EffectSourceKind::Magic,
        .sourceId = 97,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    absorption.ruleId = { 97 };
    absorption.targetUnitId = 0;
    absorption.slot = EffectStateSlot::AbsorbedDamage;
    absorption.absorbedPct = 40;
    absorption.appliedFrame = 0;
    absorption.expiresFrameExclusive = 100;
    absorption.settleOnSourceDeath = true;
    absorption.settlementTarget.kind = EffectSelectorKind::Enemies;
    absorption.settlementTarget.count = 1;
    absorption.settlementTarget.tieBreak = EffectTieBreak::BattleRandom;
    absorption.settlementDamageKind = BattleDamageKind::Pure;
    absorption.returnedPct = 100;
    absorption.accumulatedDamage = 20;
    state.effectCommands.damageAbsorptions.push_back(absorption);
    state.effectCommands.nextDamageAbsorptionSequence = 2;
    state.nextFrame.queueDamage({
        .request = {
            .attackerUnitId = 1,
            .defenderUnitId = 0,
            .baseDamage = 200,
            .preResolvedDamage = true,
        },
    });

    runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(0).alive);
    CHECK(state.effectCommands.damageAbsorptions.empty());
    const auto hpAfterSettlement = std::array{
        state.units.requireCore(1).vitals.hp,
        state.units.requireCore(2).vitals.hp,
    };
    CHECK(std::ranges::count(hpAfterSettlement, 100) == 1);
    CHECK(std::ranges::count(hpAfterSettlement, 200) == 1);

    runBattleFrame(state);
    CHECK(state.units.requireCore(1).vitals.hp == hpAfterSettlement[0]);
    CHECK(state.units.requireCore(2).vitals.hp == hpAfterSettlement[1]);
}

TEST_CASE("BattleFrameRunner_ExpiredAbsorptionWithoutLivingEnemyClearsWithoutRandomDraw", "[battle][core][effect][absorption]")
{
    const auto makeNoEnemyState = []
    {
        BattleRuntimeState state;
        configureRuntimeMovement(state, worldWith({
            unit(0, 0, { 100, 100, 0 }),
            unit(1, 0, { 200, 100, 0 }),
        }));
        state.attacks = attackWorld();
        state.movement.frame = 9;
        state.random = BattleRuntimeRandom(97);
        return state;
    };

    auto state = makeNoEnemyState();
    auto baseline = makeNoEnemyState();
    BattleDamageAbsorptionInstance absorption;
    absorption.sequence = 1;
    absorption.binding = {
        .kind = EffectSourceKind::Magic,
        .sourceId = 97,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    absorption.ruleId = { 97 };
    absorption.targetUnitId = 0;
    absorption.slot = EffectStateSlot::AbsorbedDamage;
    absorption.absorbedPct = 40;
    absorption.appliedFrame = 1;
    absorption.expiresFrameExclusive = 10;
    absorption.settlementTarget.kind = EffectSelectorKind::Enemies;
    absorption.settlementTarget.count = 1;
    absorption.settlementTarget.tieBreak = EffectTieBreak::BattleRandom;
    absorption.settlementDamageKind = BattleDamageKind::Pure;
    absorption.returnedPct = 100;
    absorption.accumulatedDamage = 40;
    state.effectCommands.damageAbsorptions.push_back(absorption);

    runBattleFrame(state);
    runBattleFrame(baseline);

    CHECK(state.effectCommands.damageAbsorptions.empty());
    CHECK(state.units.requireCore(0).vitals.hp == 100);
    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.random.nextInt(10'000) == baseline.random.nextInt(10'000));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesDamageTakenMpGainInsideRuntime", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.movement.frame = 1;
    auto& defender = state.units.requireCore(1);
    defender.vitals.mp = 5;
    defender.vitals.maxMp = 100;
    addTypedAttributeModifier(
        state,
        1,
        BattleAttribute::MpRecoveryBonus,
        AttributeOperation::PercentAdd,
        50);

    auto result = runBattleFrame(state);

    const auto damageAmounts = damageLogAmountsFor(result, 1);
    REQUIRE(damageAmounts.size() == 1);
    const int baseGain = static_cast<int>(
        static_cast<double>(damageAmounts[0]) / state.units.requireCore(1).vitals.maxHp * 75.0);
    const int expectedGain = static_cast<int>(baseGain * 1.5);
    CHECK(state.units.requireCore(1).vitals.mp == 5 + expectedGain);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AccumulatesDamageTakenMpGainAcrossSameFrameHits", "[battle][core][breakthrough]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 210, 100, 0 }),
        unit(2, 0, { 120, 100, 0 }),
    }));
    state.movement.frame = 1;
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 210, 100, 0 }),
        runtimeUnitSnapshot(2, 0, 80, { 120, 100, 0 }),
});
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);
    state.units.require(2).status = statusRuntimeSnapshot(2, 80);

    auto first = preResolvedDamageInput(0, 1, 100, 20);
    first.defender.vitals.mp = 5;
    first.defender.vitals.maxMp = 100;

    auto second = preResolvedDamageInput(2, 1, 100, 20);
    second.attacker.id = 2;
    second.attacker.vitals.hp = 80;
    second.attacker.vitals.maxHp = 100;
    second.defender.vitals.mp = 5;
    second.defender.vitals.maxMp = 100;
    addTypedAttributeModifier(
        state,
        1,
        BattleAttribute::MpRecoveryBonus,
        AttributeOperation::PercentAdd,
        50);

    queuePendingDamage(state, first);
    queuePendingDamage(state, second);

    auto result = runBattleFrame(state);

    const auto damageAmounts = damageLogAmountsFor(result, 1);
    CHECK(damageAmounts == std::vector<int>{ 20, 20 });
    const int baseGain = static_cast<int>(20.0 / 100.0 * 75.0);
    const int expectedGainPerHit = static_cast<int>(baseGain * 1.5);
    CHECK(expectedGainPerHit > 0);
    CHECK(state.units.requireCore(1).vitals.mp == 5 + expectedGainPerHit * 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ExecutePreviewUsesResolvedPendingDamage", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(3, 30);
    auto& state = frame.state;
    state.movement.frame = 1;
    state.units.requireCore(1).stats.defence = 200;
    addTestExecuteRule(state, 0, 5);

    BattleDamageTransactionInput pending;
    pending.request.attackerUnitId = 0;
    pending.request.defenderUnitId = 1;
    pending.request.baseDamage = 40;
    pending.request.preResolvedDamage = false;
    pending.attacker = makeBattleDamageUnitState(
        state.units.requireCore(0),
        &state.units.require(0).damage);
    pending.defender = makeBattleDamageUnitState(
        state.units.requireCore(1),
        &state.units.require(1).damage);
    pending.defenderStatus = makeBattleStatusUnitState(
        state.units.require(1).status,
        state.units.requireCore(1));
    queuePendingDamage(state, pending);

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK(std::none_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::FloatingText
                && event.targetUnitId == 1
                && event.text == "處決！";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ExecuteUsesCommittedPendingDamage", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(3, 60);
    auto& state = frame.state;
    state.movement.frame = 1;
    addTestExecuteRule(state, 0, 50);
    state.damage.presentationStylesByDefender[1].executeTextSize = 44;

    queuePendingDamage(state, preResolvedDamageInput(0, 1, 60, 25));

    auto result = runBattleFrame(state);

    const auto damageAmounts = damageLogAmountsFor(result, 1);
    REQUIRE(damageAmounts.size() == 2);
    CHECK(damageAmounts[0] == 25);
    CHECK(damageAmounts[1] > 0);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK(std::any_of(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::FloatingText
                && event.targetUnitId == 1
                && event.text == "處決！";
        }));
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && event.sourceUnitId == 0
                && event.targetUnitId == 1
                && BattleLogTest::textOf(event) == "觸發處決（斬殺線50%）";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DamageTakenMpGainHonorsMpBlock", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.movement.frame = 1;
    auto& defender = state.units.requireCore(1);
    defender.vitals.mp = 5;
    defender.vitals.maxMp = 100;
    state.units.require(1).status.effects.setFrames(BattleStatusKind::MpBlocked, 2);

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.requireCore(1).vitals.mp == 5);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesMainProjectileImpactFreezeInCore", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK(state.units.require(1).frozenFrames() == 5);
    CHECK(state.units.require(1).status.effects.maximumFrames(BattleStatusKind::Stun) == 5);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DoesNotApplyImpactFreezeForNonMainProjectile", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(70, 100);
    auto& state = frame.state;
    state.attacks.attacks.front().provenance.mainProjectile = false;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.require(1).frozen());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesLethalHitToDeathAndBattleEndInsideSameFrame", "[battle][core][breakthrough]")
{
    auto frame = hitDamageFrameState(120, 20);
    auto& state = frame.state;

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::UnitDied
                && event.sourceUnitId == 0
                && event.targetUnitId == 1;
        }));
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::BattleEnded
                && event.amount == 0;
        }));
    CHECK(state.attacks.attacks.empty());
    CHECK(state.nextFrame.queuedAttacks().empty());
    CHECK(state.nextFrame.queuedDamage().empty());
    CHECK(state.effectIntegration.casts.empty());
    CHECK(state.effectIntegration.queuedCommandBatches.empty());
    CHECK(state.effectIntegration.damageContinuations.empty());
    CHECK(state.units.pendingCastCount() == 0);

    const auto snapshot = state.castLifecycle.snapshot();
    CHECK(snapshot.terminalState == BattleCastLifecycleTerminalState::BattleEnded);
    CHECK(snapshot.battleEndedFrame == state.result.endedFrame);
    CHECK(snapshot.activeCasts.empty());
    CHECK(snapshot.work.empty());
    REQUIRE(snapshot.retiredCasts.size() == 1);
    const auto& retired = snapshot.retiredCasts.front();
    CHECK(retired.terminalReason == BattleCastTerminalReason::BattleEnded);
    CHECK_FALSE(retired.continuationDispatched);
    CHECK_FALSE(retired.settledDispatched);
    REQUIRE(retired.aggregate.attacksByOrdinal.size() == 1);
    CHECK(retired.aggregate.attacksByOrdinal.begin()->second.finishReason
          == AttackFinishReason::BattleEnded);
    CHECK(state.castLifecycle.drainReadyEvents(state.movement.frame).empty());
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_TransferredAntiComboDeathAoeUsesTypedRuleStore", "[battle][core][ownership]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
        unit(2, 1, { 140, 100, 0 }),
        unit(3, 1, { 160, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 60, { 140, 100, 0 }),
        runtimeUnitSnapshot(3, 1, 100, { 160, 100, 0 }),
});
    state.projectileFollowUps.projectileSpeed = SceneProjectileSpeed;
    state.projectileFollowUps.minimumProjectileFrames = 20;
    state.projectileFollowUps.areaProjectileFramePadding = 15;
    state.projectileFollowUps.areaSpawnDistance = SceneTileWidth;

    state.antiComboIds.insert(33);
    state.units.require(1).comboFacts.memberComboIds.insert(33);
    state.units.require(1).comboFacts.appliedComboIds.insert(33);
    state.units.require(2).comboFacts.memberComboIds.insert(33);
    appendDeathBlastRule(
        state.effectRules,
        {
            .kind = EffectSourceKind::Combo,
            .sourceId = 33,
            .ownerUnitId = 1,
            .sourceTeam = 1,
        },
        50,
        1,
        6);
    queuePendingDamage(state, lethalDamageInput(0, 1));

    auto result = runBattleFrame(state);

    CHECK(std::ranges::any_of(
        state.effectRules.rules(),
        [](const BoundEffectRule& bound)
        {
            return bound.binding.kind == EffectSourceKind::Combo
                && bound.binding.sourceId == 33
                && bound.binding.ownerUnitId == 2
                && bound.rule.event == EffectEvent::UnitDied
                && std::ranges::any_of(bound.rule.actions, [](const EffectAction& action)
                {
                    const auto* damage = std::get_if<DealDamageAction>(&action.value);
                    return damage && damage->areaProjectiles.has_value();
                });
        }));
    REQUIRE(state.nextFrame.queuedAttacks().size() == 1);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.attackSourceUnitId == 1);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.preferredTargetUnitId == 0);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.scriptedDamage == 50);
    CHECK_FALSE(state.nextFrame.queuedAttacks()[0].initial.scriptedDamageAppliesModifiers);
    CHECK_FALSE(state.nextFrame.queuedAttacks()[0].initial.scriptedDamageTriggersDefenseEffects);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.scriptedStunFrames == 6);

    queuePendingDamage(state, lethalDamageInput(0, 2));
    result = runBattleFrame(state);

    REQUIRE(state.nextFrame.queuedAttacks().size() == 1);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.attackSourceUnitId == 2);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.preferredTargetUnitId == 0);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.scriptedDamage == 50);
    CHECK(state.nextFrame.queuedAttacks()[0].initial.scriptedStunFrames == 6);
}

TEST_CASE("BattleFrameRunner_SummonedCloneDoesNotTransferAntiComboOwnership", "[battle][core][ownership][clone]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 120, 100, 0 }),
        unit(2, 1, { 140, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 10, { 120, 100, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 140, 100, 0 }),
    });

    state.antiComboIds.insert(33);
    auto& clone = state.units.require(1);
    clone.core.cloneSourceUnitId = 9;
    clone.comboFacts.appliedComboIds.insert(33);
    state.units.require(2).comboFacts.memberComboIds.insert(33);
    queuePendingDamage(state, lethalDamageInput(0, 1));

    runBattleFrame(state);

    CHECK_FALSE(state.units.require(2).comboFacts.appliedComboIds.contains(33));
}

TEST_CASE("BattleFrameRunner_TypedCombatRateAttributesReachRuntimeConsumers", "[battle][core][typed-attribute]")
{
    SECTION("閃避率")
    {
        auto frame = hitDamageFrameState(30, 100);
        auto& state = frame.state;
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::DodgeChance,
            AttributeOperation::PercentAdd,
            100);

        const auto result = runBattleFrame(state);

        CHECK(state.units.requireCore(1).vitals.hp == 100);
        CHECK(std::ranges::any_of(result.logEvents, [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "閃避了來襲攻擊";
        }));
    }

    SECTION("暴擊率與暴擊傷害")
    {
        auto frame = hitDamageFrameState(30, 100);
        auto& state = frame.state;
        state.damage.presentationStylesByDefender[1] = {};
        addTypedAttributeModifier(
            state,
            0,
            BattleAttribute::CriticalChance,
            AttributeOperation::PercentAdd,
            100);
        addTypedAttributeModifier(
            state,
            0,
            BattleAttribute::CriticalDamage,
            AttributeOperation::PercentAdd,
            35);

        const auto result = runBattleFrame(state);
        const auto critical = std::ranges::find_if(
            result.visualEvents,
            [](const BattleVisualEvent& event)
            {
                return event.type == BattleVisualEventType::DamageNumber
                    && event.targetUnitId == 1;
            });

        REQUIRE(critical != result.visualEvents.end());
        CHECK(critical->criticalMultiplier == 185);
    }

    SECTION("格擋率")
    {
        auto frame = hitDamageFrameState(30, 100);
        auto& state = frame.state;
        addTypedAttributeModifier(
            state,
            1,
            BattleAttribute::BlockChance,
            AttributeOperation::PercentAdd,
            100);

        const auto result = runBattleFrame(state);

        CHECK(state.units.requireCore(1).vitals.hp == 100);
        CHECK(std::ranges::any_of(result.logEvents, [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "格擋了本次攻擊";
        }));
    }
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsTargetLostCancellationWithoutPairedAttack", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 700, 100, 0 }, CombatStyle::Ranged),
        unit(2, 1, { 700, 120, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.preferredTargetUnitId = 2;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(2).alive = false;
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 2);
    REQUIRE(result.logEvents.size() == 1);
    REQUIRE(result.visualEvents.size() == 2);

    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileCancelled
                && event.effectId == 10;
        }));
    CHECK(result.gameplayEvents[1].effectId == 10);
    CHECK(result.gameplayEvents[1].targetUnitId == -1);
    CHECK(result.gameplayEvents[1].otherAttackId == -1);
    CHECK(result.visualEvents[1].type == BattleVisualEventType::ProjectileTargetLost);
    CHECK(result.visualEvents[1].amount == -1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].targetUnitId == -1);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "彈道停止：1枚目標遺失");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_LabelsChainedProjectileTargetLost", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 700, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(1).alive = false;
    appendTrackedAttack(state, std::move(projectile), 9);

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 2);
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileCancelled
                && event.effectId == 10;
        }));
    CHECK(std::any_of(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "連鎖彈道停止：1枚原目標失效";
        }));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_CoalescesSameFrameChainedProjectileStopLogs", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 700, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance first{ ordinaryProjectilePayload() };
    first.id = 10;
    first.state.attackSourceUnitId = 0;
    first.state.preferredTargetUnitId = 1;
    first.state.requirePreferredTarget = true;
    first.state.totalFrame = 30;
    first.state.position = { 100, 100, 0 };
    first.state.velocity = { 5, 0, 0 };

    BattleAttackInstance second = first;
    second.id = 11;

    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(1).alive = false;
    appendTrackedAttack(state, std::move(first), 8);
    appendTrackedAttack(state, std::move(second), 9);

    auto result = runBattleFrame(state);

    const int targetLostCount = static_cast<int>(std::count_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileTargetLost;
        }));
    CHECK(targetLostCount == 2);
    const int stopLogCount = static_cast<int>(std::count_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "連鎖彈道停止：2枚原目標失效";
        }));
    CHECK(stopLogCount == 1);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AggregatesProjectileContactIgnoredByInvincible", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.totalFrame = 30;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    projectile.state.operationType = BattleOperationType::RangedProjectile;

    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, 100, { 105, 100, 0 }),
});
    state.units.requireCore(1).invincible = 3;
    appendTrackedAttack(state, projectile);
    projectile.id = 11;
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() == 4);
    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[1].sourceUnitId == 0);
    CHECK(result.gameplayEvents[1].targetUnitId == 1);
    CHECK(result.gameplayEvents[3].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[3].sourceUnitId == 0);
    CHECK(result.gameplayEvents[3].targetUnitId == 1);
    CHECK(damageLogAmountsFor(result).empty());
    CHECK(gameplayEventsFor(result, BattleGameplayEventType::DamageApplied).empty());
    CHECK(state.units.requireCore(1).invincible > 0);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 1);
    CHECK(result.logEvents[0].targetUnitId == -1);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "彈道命中無敵：2枚傷害忽略");
    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[1].text == "彈道命中無敵：傷害忽略");
    CHECK(result.gameplayEvents[3].type == BattleGameplayEventType::StatusApplied);
    CHECK(result.gameplayEvents[3].text == "彈道命中無敵：傷害忽略");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsProjectileCancelPairWithOtherAttackId", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 900, 900, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance first{ ordinaryProjectilePayload() };
    first.id = 10;
    first.state.attackSourceUnitId = 0;
    first.frame = 5;
    first.state.totalFrame = 30;
    first.state.position = { 500, 500, 0 };
    first.state.operationType = BattleOperationType::TrackingProjectile;
    first.state.projectileCancelDamage = 11;

    BattleAttackInstance second{ ordinaryProjectilePayload() };
    second.id = 20;
    second.state.attackSourceUnitId = 1;
    second.frame = 5;
    second.state.totalFrame = 30;
    second.state.position = { 500, 500, 0 };
    second.state.operationType = BattleOperationType::RangedProjectile;
    second.state.projectileCancelDamage = 10;

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(first));
    appendTrackedAttack(state, std::move(second));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 3);
    CHECK(result.gameplayEvents[2].type == BattleGameplayEventType::ProjectileCancelled);
    CHECK(result.gameplayEvents[2].effectId == 10);
    CHECK(result.gameplayEvents[2].otherAttackId == 20);
    CHECK(result.gameplayEvents[2].amount == 0);
    CHECK(result.visualEvents[2].type == BattleVisualEventType::ProjectileCancelled);
    CHECK(result.visualEvents[2].amount == 20);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].targetUnitId == 1);
    CHECK(result.logEvents[0].amount == 17);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "抵消彈道 #10 vs #20（17 - 10 = 7）");
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#10", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#20", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " - ", BattleLogTextTone::FormulaValue));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " = ", BattleLogTextTone::FormulaValue));
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ProjectileCancelLogPutsWinnerOnLeft", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 900, 900, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance first{ ordinaryProjectilePayload() };
    first.id = 10;
    first.state.attackSourceUnitId = 0;
    first.frame = 5;
    first.state.totalFrame = 30;
    first.state.position = { 500, 500, 0 };
    first.state.operationType = BattleOperationType::RangedProjectile;
    first.state.projectileCancelDamage = 10;

    BattleAttackInstance second{ ordinaryProjectilePayload() };
    second.id = 20;
    second.state.attackSourceUnitId = 1;
    second.frame = 5;
    second.state.totalFrame = 30;
    second.state.position = { 500, 500, 0 };
    second.state.operationType = BattleOperationType::TrackingProjectile;
    second.state.projectileCancelDamage = 11;

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(first));
    appendTrackedAttack(state, std::move(second));

    auto result = runBattleFrame(state);

    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].sourceUnitId == 1);
    CHECK(result.logEvents[0].targetUnitId == 0);
    CHECK(result.logEvents[0].amount == 17);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "抵消彈道 #20 vs #10（17 - 10 = 7）");
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#20", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], "#10", BattleLogTextTone::ProjectileId));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " - ", BattleLogTextTone::FormulaValue));
    CHECK(BattleLogTest::hasSegment(result.logEvents[0], " = ", BattleLogTextTone::FormulaValue));
}

TEST_CASE("BattleFrameRunner_ProjectilePressureCombinesTypedAndSpawnScalesOnce", "[battle][core][typed-attribute]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 900, 900, 0 }, CombatStyle::Ranged),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance first{ ordinaryProjectilePayload() };
    first.id = 10;
    first.state.attackSourceUnitId = 0;
    first.frame = 5;
    first.state.totalFrame = 30;
    first.state.position = { 500, 500, 0 };
    first.state.operationType = BattleOperationType::TrackingProjectile;
    first.state.projectileCancelDamage = 11;
    first.state.projectilePressurePct = 50;

    BattleAttackInstance second{ ordinaryProjectilePayload() };
    second.id = 20;
    second.state.attackSourceUnitId = 1;
    second.frame = 5;
    second.state.totalFrame = 30;
    second.state.position = { 500, 500, 0 };
    second.state.operationType = BattleOperationType::RangedProjectile;
    second.state.projectileCancelDamage = 10;

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(first));
    appendTrackedAttack(state, std::move(second));
    addTypedAttributeModifier(
        state,
        0,
        BattleAttribute::ProjectilePressureDamage,
        AttributeOperation::Multiply,
        300);

    const auto result = runBattleFrame(state);

    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].amount == 25);
    CHECK(result.logEvents[0].secondaryAmount == 10);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "抵消彈道 #10 vs #20（25 - 10 = 15）");
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_RecordsBounceAsAttackSpawnedGameplay", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 180, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 840;
    projectile.state.preferredTargetUnitId = 1;
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
    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 3);
    const auto* bounce = findVisualEvent(result, BattleVisualEventType::ProjectileBounced, 10);
    REQUIRE(bounce);
    const auto gameplaySpawn = std::find_if(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.effectId == 30;
        });
    REQUIRE(gameplaySpawn != result.gameplayEvents.end());
    CHECK(gameplaySpawn->sourceUnitId == 0);
    CHECK(gameplaySpawn->targetUnitId == 2);
    CHECK(bounce->effectId == 10);
    CHECK(bounce->amount == 30);
    const auto visualSpawn = std::find_if(
        result.visualEvents.begin(),
        result.visualEvents.end(),
        [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::ProjectileSpawned
                && event.effectId == 30;
        });
    REQUIRE(visualSpawn != result.visualEvents.end());
    const auto& spawnEvent = *visualSpawn;
    CHECK(spawnEvent.type == BattleVisualEventType::ProjectileSpawned);
    CHECK(spawnEvent.effectId == 30);
    CHECK(spawnEvent.sourceUnitId == 0);
    CHECK(spawnEvent.targetUnitId == 2);
    CHECK(spawnEvent.durationFrames >= 20);
    CHECK(spawnEvent.visualEffectId == 44);
    CHECK(spawnEvent.position.x != 0.0f);
    CHECK(spawnEvent.velocity.x > 0.0f);
    CHECK(spawnEvent.operationKind == 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_LogsBounceChainTerminalReasons", "[battle][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 400, 100, 0 }),
    }));
    state.attacks = attackWorld();

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 840;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.bounceRemaining = 2;
    projectile.state.bounceRange = 90;
    projectile.state.bounceChancePct = 100;
    projectile.state.bounceRollPct = 0;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };

    seedRuntimeUnitsFromWorld(state);
    appendTrackedAttack(state, std::move(projectile));

    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() >= 3);
    CHECK(std::any_of(
        result.gameplayEvents.begin(),
        result.gameplayEvents.end(),
        [](const BattleGameplayEvent& event)
        {
            return event.type == BattleGameplayEventType::ProjectileExpired
                && event.effectId == 10;
        }));
    const auto damageLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.sourceUnitId == 0
                && event.targetUnitId == 1;
        });
    const auto terminalLog = std::find_if(
        result.logEvents.begin(),
        result.logEvents.end(),
        [](const BattleLogEvent& event)
        {
            return event.sourceUnitId == 0
                && event.targetUnitId == -1
                && BattleLogTest::textOf(event) == "連鎖彈道停止：1枚搜尋範圍內無可連鎖目標";
        });
    REQUIRE(damageLog != result.logEvents.end());
    REQUIRE(terminalLog != result.logEvents.end());
    CHECK(damageLog < terminalLog);
}

TEST_CASE("BattleFrameRunner_RemovesExpiredAndDeadSourceAreasAtLifecycleBoundaries", "[battle][area][core]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    auto expiring = fixedCircleAreaRequest(0, 0, { 1 }, { 100, 100, 0 }, 0, 1);
    BattleAreaEffectSystem::create(state.areas, std::move(expiring));
    runBattleFrame(state);
    CHECK(state.areas.areas.empty());

    auto removeOnDeath = fixedCircleAreaRequest(1, 1, { 2 }, { 105, 100, 0 }, 1, 100);
    removeOnDeath.anchor = { BattleAreaAnchorKind::FollowSourceUnit, {}, 1 };
    removeOnDeath.sourceDeath = AreaSourceDeathPolicy::RemoveImmediately;
    BattleAreaEffectSystem::create(state.areas, std::move(removeOnDeath));
    const auto persistent = BattleAreaEffectSystem::create(
        state.areas,
        fixedCircleAreaRequest(1, 1, { 3 }, { 105, 100, 0 }, 1, 100));

    BattleAttackInstance projectile{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::scriptedDamage(),
        BattleAttackReflectionLineageKind::Ordinary) };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.scriptedDamage = 200;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 105, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    appendTrackedAttack(state, std::move(projectile));

    runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(1).alive);
    REQUIRE(state.areas.areas.size() == 1);
    CHECK(state.areas.areas[0].id == persistent.areaId);
}
