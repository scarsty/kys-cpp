#include "ChessGameplayEffect.h"
#include <yaml-cpp/yaml.h>
#include "EffectCommandTestHelpers.h"
#include "battle/BattleCore.h"
#include "battle/BattleCoreDetail.h"
#include "BattleCoreTestHelpers.h"

#include "BattleLogTestHelpers.h"
#include "BattleMovementTestHelpers.h"
#include "BattlePresentationTestHelpers.h"
#include "BattleRuntimeRecordTestHelpers.h"
#include "BattleRuntimeStateTestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;
using namespace KysChess;
using namespace BattlePresentationTest;

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
    unit.star = 3;
    unit.haveAction = true;
    unit.animation.actFrame = 6;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;

    ModifyAttackAction spiral;
    spiral.runtimeBehavior = ExpandingSpiralAttackBehavior{
        .projectileCount = 1,
        .bleedStacks = 2,
        .baseFrames = 20,
        .framesPerStar = 15,
    };
    EffectRule spiralRule;
    spiralRule.id = { 1 };
    spiralRule.event = EffectEvent::AttackCommitted;
    spiralRule.selector.kind = EffectSelectorKind::Self;
    spiralRule.actions = { EffectAction{ EffectActionValue{ spiral } } };
    const EffectSourceBinding spiralBinding{
        .kind = EffectSourceKind::Combo,
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
    CHECK(spiralIt->state.totalFrame == 65);
    CHECK(state.effectRules.activationCount(spiralBinding, { 1 }) == 1);

    for (int frame = 0;
         frame < 40
            && !state.units.require(1).status.effects.has(BattleStatusKind::Bleed);
         ++frame)
    {
        runBattleFrame(state);
    }

    const auto* bleed = state.units.require(1).status.effects.find(
        BattleStatusKind::Bleed);
    REQUIRE(bleed);
    REQUIRE(bleed->producer);
    REQUIRE(bleed->producerFamily);
    REQUIRE(bleed->origin);
    CHECK(bleed->sourceUnitId == 0);
    CHECK(bleed->producer->binding == spiralBinding);
    CHECK(bleed->producer->ruleId == EffectRuleId{ 1 });
    CHECK(bleed->producer->actionOrder == 0);
    CHECK(bleed->producerFamily->sourceKind == EffectSourceKind::Combo);
    CHECK(bleed->producerFamily->sourceId == spiralBinding.sourceId);
    CHECK(bleed->producerFamily->logicalOwnerUnitId == 0);
    CHECK(bleed->producerFamily->ruleId == EffectRuleId{ 1 });
    CHECK(bleed->origin->binding == spiralBinding);
    CHECK(bleed->origin->ruleId == EffectRuleId{ 1 });

    // A normal authored application must join the native 鴛鴦刀 packet rather
    // than creating a second bleed storage path or private ceiling.
    REQUIRE(bleed->targetTotalLimit);
    const int scriptedLayers = bleed->stacks;
    const int authoredCeiling = *bleed->targetTotalLimit + 2;
    ApplyStatusAction authoredBleed;
    authoredBleed.status = BattleStatusKind::Bleed;
    authoredBleed.quantity = AddSharedStatusLayers{ 1, authoredCeiling };
    authoredBleed.behavior = makeCatalogOwnedStatusBehavior(authoredBleed);
    EffectCommandMetadata authoredMetadata;
    authoredMetadata.binding = {
        .kind = EffectSourceKind::Equipment,
        .sourceId = 9901,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    authoredMetadata.ruleId = EffectRuleId{ 9901 };
    authoredMetadata.event = EffectEvent::HitBeforeDamage;
    authoredMetadata.targetUnitId = 1;
    BattleEffectCommandSystem{}.reduce(state, KysChess::Battle::Test::commandFixture(EffectCommand{
            authoredMetadata,
            KysChess::Battle::Test::statusApplication(authoredBleed),
        }, { .frame = state.movement.frame }));

    const auto& joinedEffects = state.units.require(1).status.effects;
    CHECK(std::ranges::count(joinedEffects.statuses,
        BattleStatusKind::Bleed,
        &BattleStatusContribution::kind) == 1);
    const auto* joinedBleed = joinedEffects.find(BattleStatusKind::Bleed);
    REQUIRE(joinedBleed);
    CHECK(joinedBleed->stacks == scriptedLayers + 1);
    CHECK(joinedBleed->targetTotalLimit == authoredCeiling);
    CHECK(joinedBleed->producer->binding == authoredMetadata.binding);
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

TEST_CASE("BattleFrameRunner launches allied seven star swords without interrupting the ally", "[battle][core][seven-star]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, {100, 100, 0}, CombatStyle::Ranged),
        unit(1, 1, {400, 100, 0}),
        unit(2, 0, {100, 150, 0}),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.requireCore(1).vitals.hp = 10000;
    state.units.requireCore(1).vitals.maxHp = 10000;
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t ruleId{};
    REQUIRE(parseGameplayEffects(YAML::Load("[{類型: 七星歸一, 出招次數: 2, 武功威力: 300, 特效編號: 48}]"),
        effects, rules, ruleId, "合擊整合測試"));
    for (int owner : {0, 2})
    {
        state.units.require(owner).comboFacts.addApplied(12);
        state.effectRules.append({.kind = EffectSourceKind::Combo, .sourceId = 12,
            .ownerUnitId = owner, .sourceTeam = 0}, rules);
    }
    state.effectSourceNames[{EffectSourceKind::Combo, 12}] = "全真教";
    state.units.requireCore(2).animation.cooldown = 100;
    configureRuntimeActionPlan(state, frameCastInput(0, 1));
    std::vector<BattleLogEvent> volleyFrameLogs;
    for (int count = 0; count < 2; ++count)
    {
        auto& caster = state.units.requireCore(0);
        caster.haveAction = true;
        caster.animation.actFrame = 6;
        caster.operationType = BattleOperationType::RangedProjectile;
        caster.animation.actType = 1;
        caster.animation.cooldown = 10;
        auto pending = framePendingCastAction();
        pending.skillPlan.id = 301;
        setTrackedPendingCast(state, 0, std::move(pending), true);
        auto frame = runBattleFrame(state);
        if (count == 1)
            volleyFrameLogs = std::move(frame.logEvents);
    }
    const auto volleyLogs = std::ranges::count_if(volleyFrameLogs, [](const BattleLogEvent& log)
    {
        return BattleLogTest::textOf(log) == "觸發七星歸一";
    });
    CHECK(volleyLogs == 1);
    const auto volleyLog = std::ranges::find_if(volleyFrameLogs, [](const BattleLogEvent& log)
    {
        return BattleLogTest::textOf(log) == "觸發七星歸一";
    });
    REQUIRE(volleyLog != volleyFrameLogs.end());
    CHECK(volleyLog->sourceUnitId == 0);
    CHECK(volleyLog->targetUnitId == 1);
    CHECK(volleyLog->category == BattleLogCategory::Cast);
    CHECK(volleyLog->skillName == "七星歸一");
    CHECK(volleyLog->semanticSourceName == "全真教");
    std::set<int> shooters;
    BattleCastId allyCast;
    for (const auto& attack : state.attacks.attacks)
    {
        if (attack.provenance.propagation != CastPropagationPolicy::NoEffectRules) continue;
        shooters.insert(attack.state.attackSourceUnitId);
        CHECK(attack.provenance.cast.sourceUnitId == attack.state.attackSourceUnitId);
        if (attack.state.attackSourceUnitId == 2)
        {
            allyCast = attack.provenance.cast.castId;
            CHECK(attack.provenance.cast.parentCastId.has_value());
            CHECK(attack.provenance.cast.origin == CastOriginKind::AssistedAttack);
        }
        CHECK(attack.state.delivery == BattleAttackDelivery::projectile());
        CHECK(attack.state.preferredTargetUnitId == 1);
        CHECK(attack.state.track);
        CHECK(attack.state.scriptedDamage == 0);
        CHECK(attack.state.payloadClass == BattleProjectilePayloadClass::combat());
        REQUIRE(attack.state.potencySnapshot);
        CHECK(attack.state.potencySnapshot->magicPower == 300);
        CHECK(attack.castWork.valid());
        CHECK_FALSE(attack.provenance.mainProjectile);
    }
    CHECK(shooters == std::set<int>{0, 2});
    CHECK(state.units.requireCore(2).animation.cooldown >= 98);
    REQUIRE(allyCast.valid());
    for (int frame = 0; frame < 150; ++frame)
        runBattleFrame(state);
    const auto snapshot = state.castLifecycle.snapshot();
    const auto retired = std::ranges::find_if(snapshot.retiredCasts, [&](const auto& cast)
    {
        return cast.provenance.castId == allyCast;
    });
    REQUIRE(retired != snapshot.retiredCasts.end());
    CHECK(retired->provenance.sourceUnitId == 2);
    CHECK(retired->aggregate.totalActualHpDamage > 0);
    CHECK(retired->aggregate.distinctHitUnitIds.contains(1));
    CHECK(retired->outstandingWork == 0);
}

TEST_CASE("Assisted effect attacks keep source hit rules without dispatching cast lifecycle rules",
          "[battle][core][cast][assisted-attack]")
{
    BattleCastLifecycle lifecycle;
    const auto reserveAssisted = [&](CastPropagationPolicy propagation)
    {
        const auto parent = lifecycle.beginRootCast({
            .sourceUnitId = 0,
            .magicId = 301,
            .ultimate = true,
            .origin = CastOriginKind::Ultimate,
            .propagation = CastPropagationPolicy::SourceRules,
        });
        BattleAttackSpawnRequest request{BattleAttackPayload(
            BattleAttackDelivery::projectile(),
            BattleProjectilePayloadClass::combat(),
            BattleAttackReflectionLineageKind::Ordinary)};
        request.initial.attackSourceUnitId = 2;
        request.initial.skillId = 301;
        request.provenance.rootAttack = false;
        request.provenance.mainProjectile = false;
        request.provenance.propagation = propagation;

        CoreDetail::reserveEffectAttack(lifecycle, parent.provenance, request);

        REQUIRE(request.provenance.valid());
        CHECK(request.provenance.cast.sourceUnitId == 2);
        CHECK(request.provenance.cast.parentCastId == parent.provenance.castId);
        CHECK(request.provenance.cast.origin == CastOriginKind::AssistedAttack);
        CHECK(request.provenance.propagation == propagation);
        return request.provenance.cast.propagation;
    };

    CHECK(reserveAssisted(CastPropagationPolicy::SourceRules)
          == CastPropagationPolicy::SourceHitRulesOnly);
    CHECK(reserveAssisted(CastPropagationPolicy::SuppressUltimateRules)
          == CastPropagationPolicy::SourceHitRulesOnly);
    CHECK(reserveAssisted(CastPropagationPolicy::BorrowedUltimateRules)
          == CastPropagationPolicy::SourceHitRulesOnly);
    CHECK(reserveAssisted(CastPropagationPolicy::NoEffectRules)
          == CastPropagationPolicy::NoEffectRules);
}
