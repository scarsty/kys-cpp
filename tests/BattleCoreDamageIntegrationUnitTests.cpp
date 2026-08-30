#include "battle/BattleCore.h"
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
            AttributeOperation::PercentagePointAdd,
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
            AttributeOperation::PercentagePointAdd,
            100);
        addTypedAttributeModifier(
            state,
            0,
            BattleAttribute::CriticalDamage,
            AttributeOperation::PercentagePointAdd,
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
            AttributeOperation::PercentagePointAdd,
            100);

        const auto result = runBattleFrame(state);

        CHECK(state.units.requireCore(1).vitals.hp == 100);
        CHECK(std::ranges::any_of(result.logEvents, [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "格擋了本次攻擊";
        }));
    }
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
            AttributeOperation::PercentagePointAdd,
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
        AttributeOperation::PercentagePointAdd,
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
        AttributeOperation::PercentagePointAdd,
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
