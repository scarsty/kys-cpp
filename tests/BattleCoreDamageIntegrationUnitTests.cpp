#include "battle/BattleCore.h"
#include "battle/BattleCoreDetail.h"
#include "battle/BattleFrameContext.h"
#include "BattleCoreTestHelpers.h"
#include "ChessEftIds.h"

#include "BattleLogTestHelpers.h"
#include "BattleMovementTestHelpers.h"
#include "BattlePresentationTestHelpers.h"
#include "BattleRuntimeRecordTestHelpers.h"
#include "BattleRuntimeStateTestHelpers.h"
#include "ChessGameplayEffect.h"
#include <yaml-cpp/yaml.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;
using namespace KysChess;
using namespace BattlePresentationTest;

namespace
{
void checkDefensiveReactionVisual(const BattlePresentationFrame& frame, int effectId)
{
    const auto effects = visualEventsFor(frame, BattleVisualEventType::RoleEffect, 1);
    const auto effect = std::ranges::find(effects, effectId, &BattleVisualEvent::effectId);
    REQUIRE(effect != effects.end());
    CHECK(effect->durationFrames == 18);
}

BattleRuntimeState auraBattleState()
{
    BattleRuntimeState state;
    state.gridTransform = {SceneTileWidth, 64};
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, {100, 100, 0}),
        unit(1, 0, {120, 100, 0}),
        unit(2, 1, {140, 100, 0}),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 1000, {100, 100, 0}),
        runtimeUnitSnapshot(1, 0, 1000, {120, 100, 0}),
        runtimeUnitSnapshot(2, 1, 1000, {140, 100, 0}),
    });
    for (int id = 0; id < 3; ++id)
        state.units.require(id).status = statusRuntimeSnapshot(id, 1000);
    return state;
}

BattleAreaCreateRequest auraRequest(AreaModifier modifier)
{
    auto request = fixedCircleAreaRequest(0, 0, {1}, {100, 100, 0}, 0, 60);
    request.anchor = {BattleAreaAnchorKind::FollowSourceUnit, {}, 0};
    request.sourceDeath = AreaSourceDeathPolicy::RemoveImmediately;
    request.modifiers = {modifier};
    return request;
}
}

TEST_CASE("BattleFrameRunner_PersistsIndependentStrengtheningPoolsAcrossAttackAndDefense", "[battle][core][talent][backbone]")
{
    auto state = auraBattleState();
    state.units.require(0).damage.strengthening = {2, 2, 50};
    for (const auto [attacker, defender] : {std::pair{0, 2}, std::pair{2, 0}, std::pair{0, 2},
        std::pair{2, 0}, std::pair{0, 2}, std::pair{2, 0}})
    {
        state.nextFrame.queueDamage({.request = {
            .attackerUnitId = attacker, .defenderUnitId = defender, .baseDamage = 100,
        }});
    }

    const auto frame = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.hp == 800);
    CHECK(state.units.requireCore(2).vitals.hp == 600);
    CHECK(state.units.require(0).damage.strengthening == (BattleStrengthening{0, 0, 50}));
    CHECK(std::ranges::count_if(frame.logEvents, [](const auto& log)
    {
        return BattleLogTest::textOf(log).contains("中堅強化");
    }) == 4);
    CHECK(std::ranges::any_of(frame.logEvents, [](const auto& log)
    {
        return BattleLogTest::textOf(log).contains("增傷50%（剩餘1次");
    }));
    CHECK(std::ranges::any_of(frame.logEvents, [](const auto& log)
    {
        return BattleLogTest::textOf(log).contains("減傷50%（剩餘0次");
    }));
}

TEST_CASE("BattleFrameRunner_AuraPulsesUseElapsedFramesAndRespectImmunity", "[battle][area][ultimate]")
{
    auto state = auraBattleState();
    auto request = auraRequest({.kind = AreaModifierKind::PeriodicDamage,
        .relation = EffectTeamFilter::Enemy, .amount = {.flat = 45},
        .intervalFrames = 20, .overlap = AreaOverlapPolicy::Add});
    BattleAreaEffectSystem::create(state.areas, request);
    for (int i = 0; i < 19; ++i) runBattleFrame(state);
    CHECK(state.units.requireCore(2).vitals.hp == 1000);
    runBattleFrame(state);
    const int afterPulse = state.units.requireCore(2).vitals.hp;
    CHECK(afterPulse < 1000);
    CHECK(state.units.requireCore(0).vitals.hp == 1000);
    CHECK(state.units.requireCore(1).vitals.hp == 1000);
    for (int i = 0; i < 19; ++i) runBattleFrame(state);
    CHECK(state.units.requireCore(2).vitals.hp == afterPulse);
    state.units.requireCore(2).invincible = 10;
    runBattleFrame(state);
    CHECK(state.units.requireCore(2).vitals.hp == afterPulse);
    for (int i = 0; i < 20; ++i) runBattleFrame(state);
    CHECK(state.units.requireCore(2).vitals.hp == afterPulse);
    CHECK(state.areas.areas.empty());
}

TEST_CASE("Independent ground fire keeps damaging after caster death until each area expires",
          "[battle][area][fire][regression]")
{
    auto state = auraBattleState();
    auto request = fixedCircleAreaRequest(0, 0, {1}, {140, 100, 0}, 0, 45,
        AreaMergePolicy::Independent);
    request.modifiers = {{.kind = AreaModifierKind::PeriodicDamage,
        .relation = EffectTeamFilter::Enemy, .amount = {.flat = 45},
        .intervalFrames = 20, .overlap = AreaOverlapPolicy::Add}};
    BattleAreaEffectSystem::create(state.areas, request);
    request.durationFrames = 25;
    BattleAreaEffectSystem::create(state.areas, request);
    state.nextFrame.queueDamage({.request = {
        .attackerUnitId = 2, .defenderUnitId = 0, .baseDamage = 1000, .preResolvedDamage = true}});
    runBattleFrame(state);
    CHECK_FALSE(state.units.requireCore(0).alive);
    REQUIRE(state.areas.areas.size() == 2);
    for (int i = 1; i < 20; ++i) runBattleFrame(state);
    CHECK(state.units.requireCore(2).vitals.hp == 910);
    for (int i = 20; i < 40; ++i) runBattleFrame(state);
    CHECK(state.units.requireCore(2).vitals.hp == 865);
    REQUIRE(state.areas.areas.size() == 1);
    CHECK(state.areas.areas.front().anchor.fixedPosition.x == 140);
    for (int i = 40; i < 60; ++i) runBattleFrame(state);
    CHECK(state.units.requireCore(2).vitals.hp == 865);
    CHECK(state.areas.areas.empty());
}

TEST_CASE("BattleFrameRunner_GuardianReducesTransferredDamageWithoutRecursion", "[battle][area][ultimate]")
{
    auto state = auraBattleState();
    addTypedAttributeModifier(state, 0, BattleAttribute::DamageReduction,
        AttributeOperation::PercentagePointAdd, 40);
    auto request = auraRequest({.kind = AreaModifierKind::DamageRedirect,
        .relation = EffectTeamFilter::Ally, .percent = 40});
    BattleAreaEffectSystem::create(state.areas, request);
    request.source.ownerUnitId = 1;
    request.anchor.sourceUnitId = 1;
    BattleAreaEffectSystem::create(state.areas, request);
    BattleDamageRequest damage;
    damage.attackerUnitId = 2;
    damage.defenderUnitId = 1;
    damage.baseDamage = 100;
    damage.preResolvedDamage = true;
    state.nextFrame.queueDamage({.request = damage});
    runBattleFrame(state);
    CHECK(state.units.requireCore(1).vitals.hp == 1000);
    CHECK(state.units.requireCore(0).vitals.hp == 940);
}

TEST_CASE("BattleFrameRunner_GuardianTransferSharesThePercentageReductionCap", "[battle][area][reduction]")
{
    for (const int priorReductionBasisPoints : {0, 5000, 8000})
    {
        for (const int transferReductionPct : {80, 100, 1000000})
        {
            CAPTURE(priorReductionBasisPoints, transferReductionPct);
            auto state = auraBattleState();
            auto area = auraRequest({.kind = AreaModifierKind::DamageRedirect,
                .relation = EffectTeamFilter::Ally, .percent = transferReductionPct});
            BattleAreaEffectSystem::create(state.areas, area);
            BattleDamageRequest damage;
            damage.attackerUnitId = 2;
            damage.defenderUnitId = 1;
            damage.baseDamage = (10'000 - priorReductionBasisPoints) / 100;
            damage.preResolvedDamage = true;
            damage.preResolvedDamageReductionBasisPoints = priorReductionBasisPoints;
            state.nextFrame.queueDamage({.request = damage});

            runBattleFrame(state);

            CHECK(state.units.requireCore(1).vitals.hp == 1000);
            CHECK(state.units.requireCore(0).vitals.hp == 980);
        }
    }
}

TEST_CASE("BattleFrameRunner_DamageReductionAttributeAppliesToNormalHitsAndSharesTheCap", "[battle][core][reduction]")
{
    const auto damageWithReduction = [](int attributeReductionPct, int statusReductionPct)
    {
        auto frame = hitDamageFrameState(100, 1000);
        frame.state.units.requireCore(1).vitals.maxHp = 1000;
        addTypedAttributeModifier(frame.state, 1, BattleAttribute::DamageReduction,
            AttributeOperation::PercentagePointAdd, attributeReductionPct);
        if (statusReductionPct > 0)
        {
            frame.state.units.require(1).status.effects.statuses.push_back(boundStatusBehaviorContribution(
                BattleStatusKind::BattleSpirit, battleSpiritStatusBehavior(0, statusReductionPct), 1, 9001));
        }
        runBattleFrame(frame.state);
        return 1000 - frame.state.units.requireCore(1).vitals.hp;
    };

    const int ordinaryDamage = damageWithReduction(0, 0);
    const int cappedDamage = damageWithReduction(80, 0);
    CHECK(cappedDamage > 0);
    CHECK(cappedDamage < ordinaryDamage);
    CHECK(damageWithReduction(100, 0) == cappedDamage);
    CHECK(damageWithReduction(50, 50) == cappedDamage);
}

TEST_CASE("BattleFrameRunner_DirectDamageMultipliesEachLayerAndSharesTheCap", "[battle][core][reduction]")
{
    for (const auto perspective : {DamageModifierPerspective::Outgoing, DamageModifierPerspective::Incoming})
    {
        CAPTURE(perspective);
        auto frame = hitDamageFrameState(100, 100);
        auto& state = frame.state;
        state.attacks = attackWorld();
        state.effectCommands.damageModifiers.push_back({
            .sequence = state.effectCommands.nextDamageSequence++,
            .targetUnitId = perspective == DamageModifierPerspective::Outgoing ? 0 : 1,
            .perspective = perspective,
            .stage = DamageModifierStage::BeforeDefense,
            .channel = DamageChannel::All,
            .operation = DamageModifierOperation::Multiply,
            .amount = 50,
            .stackCount = 3,
        });
        state.nextFrame.queueDamage({.request = {
            .attackerUnitId = 0,
            .defenderUnitId = 1,
            .baseDamage = 100,
        }});

        runBattleFrame(state);

        CHECK(state.units.requireCore(1).vitals.hp == 80);
    }
}

TEST_CASE("BattleFrameRunner_GuardianPersonalReductionProtectsDirectHits", "[battle][area][ultimate]")
{
    auto state = auraBattleState();
    addTypedAttributeModifier(state, 0, BattleAttribute::DamageReduction,
        AttributeOperation::PercentagePointAdd, 40);
    auto request = auraRequest({.kind = AreaModifierKind::DamageRedirect,
        .relation = EffectTeamFilter::Ally, .percent = 40});
    BattleAreaEffectSystem::create(state.areas, request);
    BattleDamageRequest damage;
    damage.attackerUnitId = 2;
    damage.defenderUnitId = 0;
    damage.baseDamage = 100;
    damage.damageKind = BattleDamageKind::Pure;
    state.nextFrame.queueDamage({.request = damage});
    runBattleFrame(state);
    CHECK(state.units.requireCore(0).vitals.hp == 940);
    CHECK(state.units.requireCore(1).vitals.hp == 1000);
}

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

    const EffectSourceBinding statusBinding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 21,
        .ownerUnitId = 0,
        .sourceTeam = 0,
        .runtimeInstanceId = 17,
    };
    EffectRule statusDamageResolved;
    statusDamageResolved.id = { 4 };
    statusDamageResolved.event = EffectEvent::DamageResolved;
    statusDamageResolved.selector.kind = EffectSelectorKind::Self;
    statusDamageResolved.conditions = { IsUltimateCondition{} };
    ChangeResourceAction lineageShield;
    lineageShield.resource = BattleResource::Shield;
    lineageShield.kind = ResourceChangeKind::Grant;
    lineageShield.amount.flat = 7;
    statusDamageResolved.actions = { { EffectActionValue{ lineageShield } } };
    state.effectRules.append(statusBinding, statusDamageResolved);

    const BattleAttackProvenance triggeringAttack{
        .cast = {
            .rootCastId = BattleCastId{ 10 },
            .castId = BattleCastId{ 10 },
            .sourceUnitId = 0,
            .magicId = 21,
            .ultimate = true,
        },
        .attackId = BattleAttackId{ 11 },
        .rootAttack = true,
        .mainProjectile = true,
    };

    queuePendingDamage(
        state,
        preResolvedDamageInput(0, 1, 10, 20),
        {},
        EffectStatusDamageOrigin{
            .binding = statusBinding,
            .contribution = {
                .holderUnitId = 1,
                .sourceUnitId = 0,
                .kind = BattleStatusKind::Poison,
                .quantity = 3,
                .appliedSequence = 41,
                .producerRuleId = EffectRuleId{ 40 },
                .producerRuleOrder = 12,
                .producerActionOrder = 2,
                .behaviorRuleOrder = 1,
            },
            .behaviorActionOrder = 3,
            .triggeringCast = triggeringAttack.cast,
            .triggeringAttack = triggeringAttack,
        });
    const auto* queuedOrigin = std::get_if<EffectStatusDamageOrigin>(
        &state.nextFrame.queuedDamage().front().effectOrigin);
    REQUIRE(queuedOrigin != nullptr);
    CHECK(queuedOrigin->binding == statusBinding);
    CHECK(queuedOrigin->contribution.holderUnitId == 1);
    CHECK(queuedOrigin->contribution.sourceUnitId == 0);
    CHECK(queuedOrigin->contribution.kind == BattleStatusKind::Poison);
    CHECK(queuedOrigin->contribution.quantity == 3);
    CHECK(queuedOrigin->contribution.appliedSequence == 41);
    CHECK(queuedOrigin->contribution.producerRuleId == EffectRuleId{ 40 });
    CHECK(queuedOrigin->contribution.producerRuleOrder == 12);
    CHECK(queuedOrigin->contribution.producerActionOrder == 2);
    CHECK(queuedOrigin->contribution.behaviorRuleOrder == 1);
    CHECK(queuedOrigin->behaviorActionOrder == 3);
    REQUIRE(queuedOrigin->triggeringAttack);
    CHECK(queuedOrigin->triggeringAttack->attackId == BattleAttackId{ 11 });

    runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.effectRules.activationCount(deathBinding, { 1 }) == 1);
    CHECK(state.effectRules.activationCount(deathBinding, { 2 }) == 0);
    CHECK(state.effectRules.activationCount(allyBinding, { 3 }) == 1);
    CHECK(state.effectRules.activationCount(statusBinding, { 4 }) == 1);
    CHECK(state.units.requireCore(0).shield == 7);
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
        checkDefensiveReactionVisual(result, EFT_EVADE);
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
        checkDefensiveReactionVisual(result, EFT_BLOCK);
    }
}

TEST_CASE("BattleFrameRunner_DodgeChanceCapsAtEightyPercentAfterAllBonuses", "[battle][core][dodge][boundary]")
{
    auto frame = hitDamageFrameState(30, 100);
    auto& state = frame.state;
    bool expectedDodge{};
    SECTION("79.99% roll evades at the cap")
    {
        state.random = BattleRuntimeRandom(3727);
        expectedDodge = true;
    }
    SECTION("80% roll lands at the cap")
    {
        state.random = BattleRuntimeRandom(3378);
    }
    auto probe = state.random;
    REQUIRE(probe.nextPercent() == (expectedDodge ? 79.99 : 80.0));

    addTypedAttributeModifier(state, 1, BattleAttribute::DodgeChance,
        AttributeOperation::PercentagePointAdd, std::numeric_limits<int>::max());
    auto area = fixedCircleAreaRequest(0, 0, {1}, {105, 100, 0}, 0, 60);
    area.modifiers = {{.kind = AreaModifierKind::Attribute,
        .relation = EffectTeamFilter::Enemy, .attribute = BattleAttribute::DodgeChance,
        .amount = {.flat = 50}}};
    BattleAreaEffectSystem::create(state.areas, area);

    const auto result = runBattleFrame(state);

    CHECK(std::ranges::any_of(result.logEvents, [](const BattleLogEvent& event)
    {
        return BattleLogTest::textOf(event) == "閃避了來襲攻擊";
    }) == expectedDodge);
    CHECK((state.units.requireCore(1).vitals.hp == 100) == expectedDodge);
}

TEST_CASE("BattleFrameRunner_GuaranteedHitOnlyBypassesDodgeAndBlock", "[battle][core][ultimate]")
{
    auto frame = hitDamageFrameState(30, 100);
    auto& state = frame.state;
    addTypedAttributeModifier(state, 0, BattleAttribute::GuaranteedHit,
        AttributeOperation::Override, 1);
    addTypedAttributeModifier(state, 1, BattleAttribute::DodgeChance,
        AttributeOperation::PercentagePointAdd, 100);
    addTypedAttributeModifier(state, 1, BattleAttribute::BlockChance,
        AttributeOperation::PercentagePointAdd, 100);
    SECTION("無視閃避與格擋")
    {
        runBattleFrame(state);
        CHECK(state.units.requireCore(1).vitals.hp < 100);
    }
    SECTION("無敵仍有效")
    {
        state.units.requireCore(1).invincible = 10;
        runBattleFrame(state);
        CHECK(state.units.requireCore(1).vitals.hp == 100);
    }
    SECTION("互搏抵擋仍有效")
    {
        state.units.require(1).damage.dualWieldBlocksRemaining = 1;
        runBattleFrame(state);
        CHECK(state.units.requireCore(1).vitals.hp == 100);
        CHECK(state.units.require(1).damage.dualWieldBlocksRemaining == 0);
    }
    SECTION("護盾仍有效")
    {
        state.units.requireCore(1).shield = 1000;
        runBattleFrame(state);
        CHECK(state.units.requireCore(1).vitals.hp == 100);
        CHECK(state.units.requireCore(1).shield < 1000);
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
    checkDefensiveReactionVisual(result, EFT_BLOCK);
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


TEST_CASE("The last unit entering feigned death ends battle in the same frame", "[battle][core][feigned-death]")
{
    const int losingTeam = GENERATE(0, 1);
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 1 - losingTeam, { 100, 100, 0 }),
        unit(1, losingTeam, { 200, 100, 0 }),
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
    runtime.feignedDeath = BattleFeignedDeath{.config = {30, 40, 4, 50}};

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).size() == 1);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 1 - losingTeam);
    CHECK(state.result.outcome == (losingTeam == 0 ? BattleOutcome::PlayerDefeat : BattleOutcome::PlayerVictory));
    CHECK(state.result.endedFrame == state.movement.frame);
    const auto ended = gameplayEventsFor(result, BattleGameplayEventType::BattleEnded);
    REQUIRE(ended.size() == 1);
    CHECK(ended.front().amount == 1 - losingTeam);
    CHECK(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    const auto& defender = state.units.requireCore(1);
    CHECK_FALSE(defender.alive);
    CHECK(defender.vitals.hp == 0);
    CHECK(defender.invincible == 0);
    CHECK(runtime.feignedDeath->used);
    CHECK(runtime.feignedDeath->config.delayFrames == 30);
    CHECK(defender.feignedDeathFrames == 30);
}

TEST_CASE("A team loses when its last living ally dies during feigned death", "[battle][core][feigned-death]")
{
    const int losingTeam = GENERATE(0, 1);
    const bool allyHasFeignedDeath = GENERATE(false, true);
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 1 - losingTeam, {100, 100, 0}),
        unit(1, losingTeam, {200, 100, 0}),
        unit(2, losingTeam, {300, 100, 0}),
    }));
    state.attacks = attackWorld();
    for (auto& record : state.units.all())
    {
        record.core.canAttack = false;
        record.core.stats.speed = 0;
        record.core.animation = {};
    }
    state.units.require(1).damage.feignedDeath = BattleFeignedDeath{.config = {30, 40, 4, 50}};
    if (allyHasFeignedDeath)
    {
        state.units.require(2).damage.feignedDeath = BattleFeignedDeath{.config = {30, 40, 4, 50}};
    }
    state.nextFrame.queueDamage({.request = {.attackerUnitId = 0, .defenderUnitId = 1,
        .baseDamage = 1000, .preResolvedDamage = true}});

    const auto entered = runBattleFrame(state);
    REQUIRE_FALSE(state.units.requireCore(1).alive);
    REQUIRE(state.units.requireCore(1).feignedDeathFrames == 30);
    REQUIRE(state.units.requireCore(2).alive);
    REQUIRE_FALSE(state.result.ended);
    CHECK(gameplayEventsFor(entered, BattleGameplayEventType::BattleEnded).empty());

    state.nextFrame.queueDamage({.request = {.attackerUnitId = 0, .defenderUnitId = 2,
        .baseDamage = 1000, .preResolvedDamage = true}});
    const auto defeated = runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK(state.units.requireCore(1).feignedDeathFrames == 29);
    CHECK_FALSE(state.units.requireCore(2).alive);
    CHECK(state.units.requireCore(2).feignedDeathFrames == (allyHasFeignedDeath ? 30 : 0));
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 1 - losingTeam);
    CHECK(state.result.outcome == (losingTeam == 0 ? BattleOutcome::PlayerDefeat : BattleOutcome::PlayerVictory));
    CHECK(state.result.endedFrame == state.movement.frame);
    const auto ended = gameplayEventsFor(defeated, BattleGameplayEventType::BattleEnded);
    REQUIRE(ended.size() == 1);
    CHECK(ended.front().amount == 1 - losingTeam);
}


TEST_CASE("Immediate lethal saves precede feigned death and remain spent after revival", "[battle][core][feigned-death][talent]")
{
    const int recoveryChance = GENERATE(0, 100);
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, {100, 100, 0}), unit(1, 1, {200, 100, 0}),
        unit(2, 1, {300, 100, 0}),
    }));
    state.attacks = attackWorld();
    for (auto& record : state.units.all())
    {
        record.core.canAttack = false;
        record.core.stats.speed = 0;
        record.core.animation = {};
    }
    auto& defender = state.units.require(1);
    defender.core.vitals.hp = 20;
    defender.damage.deathPrevention = true;
    defender.damage.deathPreventionFrames = 30;
    defender.damage.lethalRecovery = BattleLethalRecovery{recoveryChance, 1, 120};
    defender.damage.feignedDeath = BattleFeignedDeath{.config = {100, 40, 4, 50}};
    const auto lethalHit = [&]()
    {
        state.nextFrame.queueDamage({.request = {.attackerUnitId = 0, .defenderUnitId = 1,
            .baseDamage = 1000, .preResolvedDamage = true}});
        return runBattleFrame(state);
    };

    const auto armorSave = lethalHit();
    CHECK(defender.core.alive);
    CHECK(defender.core.vitals.hp == 1);
    CHECK(defender.damage.deathPreventionUsed);
    CHECK_FALSE(defender.damage.lethalRecovery->used);
    CHECK_FALSE(defender.damage.feignedDeath->used);
    CHECK(defender.core.feignedDeathFrames == 0);
    CHECK(state.talentRandom.rawDrawCount() == 0);
    CHECK(gameplayEventsFor(armorSave, BattleGameplayEventType::UnitDied, 1).empty());

    defender.core.invincible = 0;
    auto pending = lethalHit();
    CHECK(defender.damage.lethalRecovery->used);
    CHECK(state.talentRandom.rawDrawCount() == 1);
    CHECK(gameplayEventsFor(pending, BattleGameplayEventType::UnitDied, 1).empty());
    if (recoveryChance == 100)
    {
        CHECK(defender.core.alive);
        CHECK(defender.core.vitals.hp == 1);
        CHECK_FALSE(defender.damage.feignedDeath->used);
        CHECK(defender.core.feignedDeathFrames == 0);
        defender.core.invincible = 0;
        pending = lethalHit();
    }
    CHECK_FALSE(defender.core.alive);
    CHECK(defender.damage.feignedDeath->used);
    CHECK(defender.core.feignedDeathFrames == 100);
    CHECK(state.talentRandom.rawDrawCount() == 1);
    CHECK(gameplayEventsFor(pending, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK_FALSE(state.result.ended);

    for (int i = 0; i < 100; ++i) runBattleFrame(state);
    REQUIRE(defender.core.alive);
    CHECK(defender.core.feignedDeathFrames == 0);
    CHECK(defender.core.vitals.hp == 40);
    CHECK(defender.damage.lethalRecovery->used);
    CHECK(state.talentRandom.rawDrawCount() == 1);

    const auto finalDeath = lethalHit();
    CHECK_FALSE(defender.core.alive);
    CHECK(defender.core.vitals.hp == 0);
    CHECK(defender.core.feignedDeathFrames == 0);
    CHECK(defender.damage.lethalRecovery->used);
    CHECK(state.talentRandom.rawDrawCount() == 1);
    CHECK(gameplayEventsFor(finalDeath, BattleGameplayEventType::UnitDied, 1).size() == 1);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_FeignedDeathUsedRuntimeDoesNotTriggerAgain", "[battle][core][breakthrough]")
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
    runtime.feignedDeath = BattleFeignedDeath{.config = {30, 40, 4, 50}};
    runtime.feignedDeath->used = true;

    auto result = runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(gameplayEventsFor(result, BattleGameplayEventType::UnitDied, 1).empty());
    CHECK(runtime.feignedDeath->used);
}

TEST_CASE("Feigned death waits exactly its delay, restores resources, and stuns only the revival square",
    "[battle][core][feigned-death]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, {300, 300, 0}), unit(1, 1, {371, 371, 0}),
        unit(2, 1, {373, 300, 0}), unit(3, 1, {300, 227, 0}),
        unit(4, 0, {310, 310, 0}), unit(5, 1, {300, 230, 0}),
    }));
    state.attacks = attackWorld();
    for (auto& record : state.units.all())
    {
        record.core.canAttack = false;
        record.core.stats.speed = 0;
        record.core.animation = {};
    }
    auto& defender = state.units.require(0);
    defender.core.vitals = {10, 1000, 3, 100};
    defender.core.motion.velocity = {5, 8, 2};
    defender.damage.feignedDeath = BattleFeignedDeath{.config = {100, 40, 4, 50},
        .binding = {.kind = EffectSourceKind::Combo, .sourceId = 9001, .ownerUnitId = 0}, .ruleId = {9001}};
    state.units.require(5).status.effects.controlImmunityFrames = 300;
    state.nextFrame.queueDamage({.request = {.attackerUnitId = 1, .defenderUnitId = 0,
        .baseDamage = 100, .preResolvedDamage = true}});

    const auto entered = runBattleFrame(state);
    REQUIRE(defender.core.feignedDeathFrames == 100);
    CHECK_FALSE(defender.core.alive);
    CHECK_FALSE(defender.core.haveAction);
    CHECK(defender.core.vitals.hp == 0);
    CHECK(defender.core.motion.velocity.norm() == 0);
    CHECK(gameplayEventsFor(entered, BattleGameplayEventType::UnitDied, 0).empty());
    const auto position = defender.core.motion.position;
    CHECK(findNearestEnemyUnitId(state.units, 1) == 4);

    state.nextFrame.queueDamage({.request = {.attackerUnitId = 1, .defenderUnitId = 0,
        .baseDamage = 1000, .preResolvedDamage = true}});
    for (int i = 0; i < 99; ++i) runBattleFrame(state);
    CHECK(defender.core.feignedDeathFrames == 1);
    CHECK_FALSE(defender.core.alive);
    CHECK(defender.core.vitals.hp == 0);
    CHECK(defender.core.motion.position.x == position.x);
    CHECK(defender.core.motion.position.y == position.y);
    CHECK(defender.core.motion.position.z == 0);

    const auto revived = runBattleFrame(state);
    CHECK(defender.core.alive);
    CHECK(defender.core.feignedDeathFrames == 0);
    CHECK(defender.core.vitals.hp == 400);
    CHECK(defender.core.vitals.mp == 100);
    CHECK(defender.core.invincible == 0);
    REQUIRE(state.units.require(1).status.effects.has(BattleStatusKind::Stun));
    CHECK(state.units.require(1).status.effects.statuses.front().remainingFrames == 50);
    for (int id : {2, 3, 4, 5})
        CHECK_FALSE(state.units.require(id).status.effects.has(BattleStatusKind::Stun));
    CHECK(std::ranges::any_of(revived.visualEvents, [](const auto& event) { return event.text == "復甦"; }));

    state.nextFrame.queueDamage({.request = {.attackerUnitId = 2, .defenderUnitId = 0,
        .baseDamage = 1000, .preResolvedDamage = true}});
    const auto finalDeath = runBattleFrame(state);
    CHECK_FALSE(defender.core.alive);
    CHECK(defender.core.feignedDeathFrames == 0);
    CHECK(gameplayEventsFor(finalDeath, BattleGameplayEventType::UnitDied, 0).size() == 1);
}

TEST_CASE("Feigned death revival refreshes Shendiao and Mizong battle activation limits",
    "[battle][core][feigned-death][activation-limit]")
{
    const auto* emergencyHeal = GENERATE(
        "{類型: 低血緊急回血, 生命門檻百分比: 20, 治療生命百分比: 20}",
        "{類型: 低血緊急回血狂暴, 生命門檻百分比: 20, 治療生命百分比: 20, "
        "暴擊百分比: 100, 冷卻縮減百分比: 30, 吸血百分比: 100, 持續幀數: 5}");
    auto definitions = YAML::Load(
        "[{類型: 成員低血全員增攻, 生命門檻百分比: 50, 攻擊百分比: 23, 持續幀數: 9}]");
    definitions.push_back(YAML::Load(emergencyHeal));
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t nextRuleId{};
    REQUIRE(parseGameplayEffects(definitions, effects, rules, nextRuleId, "復甦次數重設測試"));
    REQUIRE(rules.size() == 2);
    const EffectSourceBinding revivedBinding{
        .kind = EffectSourceKind::Combo, .sourceId = 9001, .ownerUnitId = 0, .sourceTeam = 0};
    auto allyBinding = revivedBinding;
    allyBinding.ownerUnitId = 1;
    auto state = auraBattleState();
    for (auto& record : state.units.all())
    {
        record.core.canAttack = false;
        record.core.stats.speed = 0;
        record.core.animation = {};
    }
    for (int id : {0, 1})
    {
        auto& record = state.units.require(id);
        record.core.vitals.maxHp = 1000;
        record.core.vitals.hp = 100;
        record.comboFacts.addMember(9001);
        record.comboFacts.addApplied(9001);
    }
    state.effectRules.append(revivedBinding, rules);
    state.effectRules.append(allyBinding, rules);
    state.units.requireCore(2).vitals.maxHp = 1000;
    auto& defender = state.units.require(0);
    defender.damage.feignedDeath = BattleFeignedDeath{.config = {2, 40, 4, 50}};
    const auto activations = [&](const EffectSourceBinding& binding, int ruleIndex)
    {
        return state.effectRules.activationCount(binding, rules[ruleIndex].id);
    };
    const auto lethalHit = [&]()
    {
        state.nextFrame.queueDamage({.request = {.attackerUnitId = 2, .defenderUnitId = 0,
            .baseDamage = 10000, .preResolvedDamage = true}});
        return runBattleFrame(state);
    };

    runBattleFrame(state);
    for (const auto& binding : {revivedBinding, allyBinding})
    {
        REQUIRE(activations(binding, 0) == 1);
        REQUIRE(activations(binding, 1) == 1);
        CHECK(state.units.requireCore(binding.ownerUnitId).vitals.hp == 300);
    }
    const auto sharedCooldown = state.effectRules.runtime(revivedBinding, rules[0].id).sharedCooldownUntilFrame;
    REQUIRE(sharedCooldown == 200);

    lethalHit();
    REQUIRE_FALSE(defender.core.alive);
    REQUIRE(defender.core.feignedDeathFrames == 2);
    CHECK(activations(revivedBinding, 0) == 1);
    CHECK(activations(revivedBinding, 1) == 1);
    runBattleFrame(state);
    REQUIRE_FALSE(defender.core.alive);
    CHECK(activations(revivedBinding, 0) == 1);
    CHECK(activations(revivedBinding, 1) == 1);

    runBattleFrame(state);
    REQUIRE(defender.core.alive);
    CHECK(defender.core.vitals.hp == 400);
    CHECK(defender.damage.feignedDeath->used);
    CHECK(activations(revivedBinding, 0) == 0);
    CHECK(activations(revivedBinding, 1) == 0);
    CHECK(state.effectRules.runtime(revivedBinding, rules[0].id).sharedCooldownUntilFrame == sharedCooldown);
    CHECK(activations(allyBinding, 0) == 1);
    CHECK(activations(allyBinding, 1) == 1);

    defender.core.vitals.hp = 100;
    state.units.requireCore(1).vitals.hp = 100;
    runBattleFrame(state);
    CHECK(defender.core.vitals.hp == 300);
    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(activations(revivedBinding, 0) == 0);
    CHECK(activations(revivedBinding, 1) == 1);
    while (state.movement.frame <= sharedCooldown) runBattleFrame(state);
    CHECK(activations(revivedBinding, 0) == 1);
    CHECK(activations(allyBinding, 0) == 1);
    for (int id : {0, 1})
    {
        CHECK(BattleEffectCommandSystem::queryAttribute(state,
            {.unitId = id, .attribute = BattleAttribute::Attack, .baseValue = 100,
                .frame = state.movement.frame}) == (id == 0 ? 100 : 123));
    }

    const auto finalDeath = lethalHit();
    CHECK_FALSE(defender.core.alive);
    CHECK(defender.core.feignedDeathFrames == 0);
    CHECK(gameplayEventsFor(finalDeath, BattleGameplayEventType::UnitDied, 0).size() == 1);
}

TEST_CASE("Simultaneous feigned death revivals stun both teams while their allies survive",
    "[battle][core][feigned-death]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, {100, 100, 0}), unit(1, 1, {130, 100, 0}),
        unit(2, 0, {700, 700, 0}), unit(3, 1, {800, 700, 0}),
    }));
    state.attacks = attackWorld();
    for (auto& record : state.units.all())
    {
        record.core.canAttack = false;
        record.core.stats.speed = 0;
    }
    for (int id : {0, 1})
    {
        auto& record = state.units.require(id);
        record.damage.feignedDeath = BattleFeignedDeath{.config = {2, 40, 4, 50},
            .binding = {.kind = EffectSourceKind::Combo, .sourceId = 9001,
                .ownerUnitId = record.id(), .sourceTeam = record.core.team}, .ruleId = {9001}};
        state.nextFrame.queueDamage({.request = {.attackerUnitId = 1 - record.id(),
            .defenderUnitId = record.id(), .baseDamage = 1000, .preResolvedDamage = true}});
    }
    runBattleFrame(state);
    CHECK_FALSE(state.units.requireCore(0).alive);
    CHECK_FALSE(state.units.requireCore(1).alive);
    CHECK_FALSE(state.result.ended);
    runBattleFrame(state);
    CHECK_FALSE(state.result.ended);
    runBattleFrame(state);
    for (int id : {0, 1})
    {
        const auto& record = state.units.require(id);
        CHECK(record.core.alive);
        CHECK(record.core.vitals.hp == 40);
        CHECK(record.status.effects.has(BattleStatusKind::Stun));
    }
    CHECK_FALSE(state.result.ended);
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


TEST_CASE("Damage absorption settlement preserves status or configured action provenance",
          "[battle][core][effect][absorption][status][provenance]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 200, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);

    const BattleAttackProvenance triggeringAttack{
        .cast = {
            .rootCastId = BattleCastId{ 31 },
            .castId = BattleCastId{ 31 },
            .sourceUnitId = 0,
            .magicId = 97,
            .ultimate = true,
        },
        .attackId = BattleAttackId{ 32 },
        .rootAttack = true,
        .mainProjectile = true,
    };
    BattleDamageAbsorptionInstance absorption;
    absorption.sequence = 1;
    absorption.binding = {
        .kind = EffectSourceKind::Magic,
        .sourceId = 97,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    absorption.ruleId = EffectRuleId{ 97 };
    absorption.authoredActionOrder = 7;
    absorption.statusContribution = EffectStatusContributionContext{
        .holderUnitId = 0,
        .sourceUnitId = 0,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 1,
        .appliedSequence = 41,
        .producerRuleId = EffectRuleId{ 97 },
        .producerRuleOrder = 11,
        .producerActionOrder = 2,
        .behaviorRuleOrder = 5,
    };
    absorption.triggeringCast = triggeringAttack.cast;
    absorption.triggeringAttack = triggeringAttack;
    absorption.targetUnitId = 0;
    absorption.slot = EffectStateSlot::AbsorbedDamage;
    absorption.absorbedPct = 40;
    absorption.appliedFrame = 0;
    absorption.expiresFrameExclusive = 10;
    absorption.settlementTarget.kind = EffectSelectorKind::Enemies;
    absorption.settlementTarget.count = 1;
    absorption.settlementDamageKind = BattleDamageKind::Pure;
    absorption.returnedPct = 100;
    absorption.accumulatedDamage = 10;

    std::array<std::byte, 4096> storage{};
    auto frame = BattleFrameContext::begin(state, {}, storage.data(), storage.size());
    std::vector<BattlePendingDamageIntent> pendingDamage;
    const std::array statusOwned{ absorption };
    CoreDetail::appendDamageAbsorptionSettlements(
        state,
        frame,
        pendingDamage,
        statusOwned,
        10);
    REQUIRE(pendingDamage.size() == 1);
    CHECK_FALSE(pendingDamage.front().provenance.valid());
    CHECK_FALSE(pendingDamage.front().delayedCastWork.valid());
    CHECK_FALSE(state.castLifecycle.containsCast(triggeringAttack.cast.castId));
    const auto* statusOrigin = std::get_if<EffectStatusDamageOrigin>(
        &pendingDamage.front().effectOrigin);
    REQUIRE(statusOrigin);
    CHECK(statusOrigin->binding == absorption.binding);
    CHECK(statusOrigin->contribution == *absorption.statusContribution);
    CHECK(statusOrigin->behaviorActionOrder == 7);
    REQUIRE(statusOrigin->triggeringAttack);
    CHECK(statusOrigin->triggeringAttack->attackId == BattleAttackId{ 32 });

    absorption.sequence = 2;
    absorption.statusContribution.reset();
    pendingDamage.clear();
    const std::array configured{ absorption };
    CoreDetail::appendDamageAbsorptionSettlements(
        state,
        frame,
        pendingDamage,
        configured,
        10);
    REQUIRE(pendingDamage.size() == 1);
    CHECK_FALSE(pendingDamage.front().provenance.valid());
    CHECK_FALSE(pendingDamage.front().delayedCastWork.valid());
    CHECK_FALSE(state.castLifecycle.containsCast(triggeringAttack.cast.castId));
    const auto* ruleOrigin = std::get_if<EffectRuleDamageOrigin>(
        &pendingDamage.front().effectOrigin);
    REQUIRE(ruleOrigin);
    CHECK(ruleOrigin->binding == absorption.binding);
    CHECK(ruleOrigin->ruleId == EffectRuleId{ 97 });
    CHECK(ruleOrigin->actionOrder == 7);
    REQUIRE(ruleOrigin->triggeringAttack);
    CHECK(ruleOrigin->triggeringAttack->attackId == BattleAttackId{ 32 });
}

TEST_CASE("BattleFrameRunner_ExpiresDamageAbsorptionIntoSameFrameRandomPureDamage", "[battle][core][effect][absorption][logging]")
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

    state.effectSourceNames[{ EffectSourceKind::Magic, 97 }] = "乾坤大挪移";
    const auto frame = runBattleFrame(state);
    const auto ended = std::ranges::find(frame.logEvents,
        BattleStatusSemanticId::DamageAbsorptionEnded, &BattleLogEvent::statusId);
    REQUIRE(ended != frame.logEvents.end());
    CHECK(ended->semanticSourceName == "乾坤大挪移");
    CHECK(ended->amount == 40);
    CHECK(ended->secondaryAmount == 40);
    CHECK(ended->frame == 10);
    const auto damage = std::ranges::find(frame.logEvents, BattleLogEventType::Damage, &BattleLogEvent::type);
    REQUIRE(damage != frame.logEvents.end());
    CHECK(damage->semanticSourceName == "乾坤大挪移");
    CHECK(damage->semanticSourceKind == "magic");
    CHECK(damage->skillId == 97);

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


TEST_CASE("BattleFrameRunner_AdvanceFrame_TypedExecuteDamageShowsExecutionPresentationAndLogs", "[battle][core][execute]")
{
    auto state = auraBattleState();
    state.movement.frame = 1;
    state.damage.presentationStylesByDefender[2].executeTextSize = 44;
    state.effectSourceNames[{ EffectSourceKind::Magic, 67 }] = "胡家刀法";

    auto execute = preResolvedDamageInput(0, 2, 40, 100);
    execute.request.damageKind = BattleDamageKind::Execute;
    execute.request.canExecute = true;
    execute.request.executeThresholdPct = 100;
    queuePendingDamage(
        state,
        execute,
        {},
        EffectRuleDamageOrigin{
            .ruleId = EffectRuleId{ 1 },
            .binding = {
                .kind = EffectSourceKind::Magic,
                .sourceId = 67,
                .ownerUnitId = 0,
                .sourceTeam = 0,
            },
        });

    const auto result = runBattleFrame(state);

    CHECK_FALSE(state.units.requireCore(2).alive);
    CHECK(std::ranges::any_of(result.visualEvents, [](const BattleVisualEvent& event)
    {
        return event.type == BattleVisualEventType::FloatingText
            && event.targetUnitId == 2
            && event.text == "處決！"
            && event.textSize == 44;
    }));
    CHECK(std::ranges::any_of(result.logEvents, [](const BattleLogEvent& event)
    {
        return event.type == BattleLogEventType::Status
            && event.sourceUnitId == 0
            && event.targetUnitId == 2
            && event.statusId == BattleStatusSemanticId::ExecuteTriggered
            && BattleLogTest::textOf(event) == "觸發處決";
    }));
    CHECK(std::ranges::any_of(result.logEvents, [](const BattleLogEvent& event)
    {
        return event.type == BattleLogEventType::Damage
            && event.sourceUnitId == 0
            && event.targetUnitId == 2
            && event.skillName == "胡家刀法"
            && BattleLogTest::textOf(event) == "處決";
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
    state.units.require(1).comboFacts.addMember(33);
    state.units.require(1).comboFacts.addApplied(33);
    state.units.require(2).comboFacts.addMember(33);
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
                && bound.rule().event == EffectEvent::UnitDied
                && std::ranges::any_of(bound.rule().actions, [](const EffectAction& action)
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
    clone.comboFacts.addApplied(33);
    state.units.require(2).comboFacts.addMember(33);
    queuePendingDamage(state, lethalDamageInput(0, 1));

    runBattleFrame(state);

    CHECK_FALSE(state.units.require(2).comboFacts.hasApplied(33));
}


TEST_CASE("Poison conversion shares a rolling healing budget across victims and obeys healing rules",
          "[battle][damage][lore-equipment]")
{
    auto state = auraBattleState();
    state.units.requireCore(0).vitals = {500, 1000, 20, 50};
    for (int id : {1, 2}) state.units.requireCore(id).vitals = {1000, 1000, 20, 50};
    state.effectCommands.poisonConversions.emplace(0, BattlePoisonConversion{
        .config = {25, 30, 2},
        .binding = {.kind = EffectSourceKind::Equipment, .sourceId = 243, .ownerUnitId = 0, .sourceTeam = 0}});
    const auto poison = [&](int target, int damage = 100, int source = 0)
    {
        BattleDamageRequest request;
        request.attackerUnitId = source;
        request.defenderUnitId = target;
        request.baseDamage = damage;
        request.damageKind = BattleDamageKind::Poison;
        request.preResolvedDamage = true;
        request.triggersDefenseEffects = false;
        state.nextFrame.queueDamage({.request = request});
        const int before = state.units.requireCore(0).vitals.hp;
        const auto frame = runBattleFrame(state);
        const int healed = state.units.requireCore(0).vitals.hp - before;
        const auto count = std::ranges::count_if(frame.logEvents, [](const auto& log)
        {
            return log.skillName == "化毒養身";
        });
        CHECK(count == (healed > 0 ? 1 : 0));
        if (healed > 0)
        {
            const auto log = std::ranges::find(frame.logEvents, "化毒養身", &BattleLogEvent::skillName);
            REQUIRE(log != frame.logEvents.end());
            CHECK(log->type == BattleLogEventType::Heal);
            CHECK(log->sourceUnitId == 0);
            CHECK(log->targetUnitId == 0);
            CHECK(log->amount == healed);
            CHECK(log->previousAmount == before);
            CHECK(log->newAmount == before + healed);
            CHECK(BattleLogTest::textOf(*log).contains(std::format("毒傷轉化{}", healed)));
        }
    };
    SECTION("共用上限與滑動窗口")
    {
        poison(1);
        CHECK(state.units.requireCore(0).vitals.hp == 520);
        CHECK(state.units.requireCore(1).vitals.hp == 920);
        poison(2);
        CHECK(state.units.requireCore(0).vitals.hp == 520);
        CHECK(state.units.requireCore(2).vitals.hp == 900);
        for (int i = 0; i < 28; ++i) runBattleFrame(state);
        poison(2);
        CHECK(state.units.requireCore(0).vitals.hp == 540);
        CHECK(state.units.requireCore(2).vitals.hp == 820);
    }
    SECTION("滿血保留完整毒傷")
    {
        state.units.requireCore(0).vitals.hp = 1000;
        poison(1);
        CHECK(state.units.requireCore(0).vitals.hp == 1000);
        CHECK(state.units.requireCore(1).vitals.hp == 900);
    }
    SECTION("只轉化缺失生命")
    {
        state.units.requireCore(0).vitals.hp = 993;
        poison(1);
        CHECK(state.units.requireCore(0).vitals.hp == 1000);
        CHECK(state.units.requireCore(1).vitals.hp == 907);
    }
    SECTION("護盾吸收不產生治療")
    {
        state.units.requireCore(1).shield = 100;
        poison(1);
        CHECK(state.units.requireCore(0).vitals.hp == 500);
        CHECK(state.units.requireCore(1).vitals.hp == 1000);
        CHECK(state.units.requireCore(1).shield == 0);
    }
    SECTION("過量擊殺傷害不產生額外治療")
    {
        state.units.requireCore(1).vitals.hp = 20;
        poison(1);
        CHECK(state.units.requireCore(0).vitals.hp == 505);
        CHECK(state.units.requireCore(1).vitals.hp == 5);
        CHECK(state.units.requireCore(1).alive);
    }
    SECTION("其他來源的毒不治療持杖者")
    {
        poison(2, 100, 1);
        CHECK(state.units.requireCore(0).vitals.hp == 500);
    }
    SECTION("來源陣亡不復活")
    {
        state.units.requireCore(0).vitals.hp = 0;
        state.units.requireCore(0).alive = false;
        poison(1);
        CHECK(state.units.requireCore(0).vitals.hp == 0);
        CHECK(state.units.requireCore(1).vitals.hp == 900);
    }
    SECTION("禁療與減療走標準治療流程")
    {
        const int receivedPct = GENERATE(0, 50, 100);
        EffectRule rule;
        rule.id = {100};
        rule.event = EffectEvent::HealAttempted;
        rule.actions = {EffectAction{.value = ModifyHealTransactionAction{
            .operation = HealModifierOperation::MultiplyReceived, .kinds = {"吸血"}, .percent = receivedPct}}};
        state.effectRules.append({.kind = EffectSourceKind::Equipment, .sourceId = 99,
            .ownerUnitId = 0, .sourceTeam = 0}, rule);
        poison(1);
        const int healed = receivedPct == 0 ? 0 : receivedPct == 50 ? 12 : 20;
        CHECK(state.units.requireCore(0).vitals.hp == 500 + healed);
        CHECK(state.units.requireCore(1).vitals.hp == 900 + healed);
    }
}

TEST_CASE("Revolving guard affects direct attacks but not poison and regenerates configurable charges",
          "[battle][damage][lore-equipment]")
{
    BattleGuardCharges charges{{2, 5, 30, 6, 2}, 0, 0};
    charges.advanceTo(5);
    CHECK(charges.charges == 0);
    charges.advanceTo(6);
    CHECK(charges.charges == 2);
    charges.advanceTo(18);
    CHECK(charges.charges == 5);
    --charges.charges;
    charges.advanceTo(18);
    CHECK(charges.charges == 4);
    const auto damage = [](bool guarded)
    {
        auto frame = hitDamageFrameState(100, 1000);
        frame.state.units.requireCore(1).vitals.maxHp = 1000;
        if (guarded) frame.state.effectCommands.guardCharges.emplace(1,
            BattleGuardCharges{{5, 5, 30, 60, 1}, 5, 0});
        const auto result = runBattleFrame(frame.state);
        if (guarded)
        {
            CHECK(frame.state.effectCommands.guardCharges.at(1).charges == 4);
            const auto log = std::ranges::find(result.logEvents, "輪轉護身", &BattleLogEvent::skillName);
            REQUIRE(log != result.logEvents.end());
            CHECK(log->amount > 0);
            CHECK(BattleLogTest::textOf(*log).contains("消耗1層"));
            CHECK(BattleLogTest::textOf(*log).contains("剩餘4/5層"));
        }
        return 1000 - frame.state.units.requireCore(1).vitals.hp;
    };
    CHECK(damage(true) < damage(false));
    auto state = auraBattleState();
    state.units.requireCore(2).vitals = {1000, 1000, 20, 50};
    state.effectCommands.guardCharges.emplace(2, BattleGuardCharges{{5, 5, 30, 60, 1}, 5, 0});
    BattleDamageRequest request;
    request.attackerUnitId = 0;
    request.defenderUnitId = 2;
    request.baseDamage = 100;
    request.damageKind = BattleDamageKind::Poison;
    request.preResolvedDamage = true;
    request.triggersDefenseEffects = false;
    state.nextFrame.queueDamage({.request = request});
    runBattleFrame(state);
    CHECK(state.units.requireCore(2).vitals.hp == 900);
    CHECK(state.effectCommands.guardCharges.at(2).charges == 5);
}

TEST_CASE("Guard recovery is logged on its frame with actual gains and stays quiet at the cap",
          "[battle][damage][lore-equipment][logging]")
{
    auto state = auraBattleState();
    state.effectCommands.guardCharges.emplace(0, BattleGuardCharges{{0, 3, 30, 3, 2}, 0, 0});
    for (int frameNumber = 1; frameNumber <= 9; ++frameNumber)
    {
        const auto frame = runBattleFrame(state);
        const auto count = std::ranges::count_if(frame.logEvents, [](const auto& log)
        {
            return log.skillName == "輪轉護身";
        });
        CHECK(count == (frameNumber == 3 || frameNumber == 6 ? 1 : 0));
        if (count == 1)
        {
            const auto log = std::ranges::find(frame.logEvents, "輪轉護身", &BattleLogEvent::skillName);
            CHECK(log->frame == frameNumber);
            CHECK(log->amount == (frameNumber == 3 ? 2 : 1));
            CHECK(log->previousAmount == (frameNumber == 3 ? 0 : 2));
            CHECK(log->newAmount == (frameNumber == 3 ? 2 : 3));
            CHECK(BattleLogTest::textOf(*log).contains("回復"));
        }
    }
}

TEST_CASE("Melody shield logs its name and actual replenishment without full-shield spam",
          "[battle][effect][lore-equipment][logging]")
{
    auto state = auraBattleState();
    state.units.requireCore(1).vitals.hp = 100;
    state.units.requireCore(1).vitals.maxHp = 1000;
    state.units.requireCore(1).shield = 50;
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t nextId = 100;
    REQUIRE(parseGameplayEffects(YAML::Load(
        "[{類型: 清音護心, 出招次數: 1, 生命護盾百分比: 12}]"),
        effects, rules, nextId, "測試"));
    // 此處只測日誌結算；自主出招觸發由事件橋接測試覆蓋。
    auto rule = rules.front();
    rule.event = EffectEvent::FrameAdvanced;
    rule.naturalCastsOnly = false;
    rule.intervalFrames = 1;
    state.effectRules.append({.kind = EffectSourceKind::Equipment, .sourceId = 777,
        .ownerUnitId = 0, .sourceTeam = 0}, rule);
    const auto checkShield = [&](int expectedGain, int before, int after)
    {
        const auto frame = runBattleFrame(state);
        const auto count = std::ranges::count_if(frame.logEvents, [](const auto& log)
        {
            return log.skillName == "清音護心";
        });
        CHECK(count == (expectedGain > 0 ? 1 : 0));
        if (expectedGain > 0)
        {
            const auto log = std::ranges::find(frame.logEvents, "清音護心", &BattleLogEvent::skillName);
            REQUIRE(log != frame.logEvents.end());
            CHECK(log->sourceUnitId == 0);
            CHECK(log->targetUnitId == 1);
            CHECK(log->amount == expectedGain);
            CHECK(log->previousAmount == before);
            CHECK(log->newAmount == after);
            CHECK(BattleLogTest::textOf(*log).contains("清音護心：護盾+"));
        }
    };
    checkShield(120, 50, 170);
    checkShield(0, 170, 170);
    auto defender = state.units.require(1).damageState(state.movement.frame);
    defender.shield = 100;
    state.units.writeDamageUnit(defender);
    checkShield(70, 100, 170);
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
