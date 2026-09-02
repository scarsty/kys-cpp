#include "battle/BattleCore.h"
#include "battle/BattleCoreDetail.h"
#include "battle/BattleEffectCommandSystem.h"
#include "battle/BattleEffectEventBridge.h"
#include "BattleCoreTestHelpers.h"
#include "ChessBattleEffectParser.h"

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

namespace
{

BattleStatusEffectOrigin addTrueQiStatus(
    BattleRuntimeState& state,
    int sourceUnitId,
    int stacks,
    int pureDamagePerHit)
{
    const BattleStatusEffectOrigin origin{
        .binding = {
            .kind = EffectSourceKind::Magic,
            .sourceId = 106,
            .ownerUnitId = sourceUnitId,
            .sourceTeam = state.units.requireCore(sourceUnitId).team,
        },
        .ruleId = EffectRuleId{ static_cast<std::uint64_t>(106) << 32 },
        .ruleOrder = 4,
    };
    auto& effects = state.units.require(sourceUnitId).status.effects;
    effects.statuses.push_back({
        .kind = BattleStatusKind::TrueQi,
        .sourceUnitId = sourceUnitId,
        .stacks = stacks,
        .potency = pureDamagePerHit,
        .origin = origin,
        .appliedSequence = effects.nextStatusSequence++,
    });
    return origin;
}

EffectCommand orderedDamageCommand(
    const EffectSourceBinding& binding,
    EffectRuleId ruleId,
    std::uint32_t ruleOrder)
{
    return {
        .metadata = {
            .binding = binding,
            .ruleId = ruleId,
            .event = EffectEvent::HitBeforeDamage,
            .ruleOrder = ruleOrder,
            .targetUnitId = 1,
            .eventSourceUnitId = 0,
        },
        .value = DealDamageEffectCommand{},
    };
}

}

TEST_CASE("True-Qi intrinsic hit command preserves the removed consumer contract",
          "[battle][core][true-qi][contract]")
{
    auto frame = hitDamageFrameState(20, 100);
    auto& state = frame.state;
    const auto origin = addTrueQiStatus(state, 0, 3, 9);
    EffectRule authoredSuccessor;
    authoredSuccessor.id = EffectRuleId{ origin.ruleId.value + 1 };
    authoredSuccessor.event = EffectEvent::HitBeforeDamage;
    authoredSuccessor.selector.kind = EffectSelectorKind::HitTarget;
    authoredSuccessor.actions = { EffectAction{ DealDamageAction{} } };
    state.effectRules.append(origin.binding, authoredSuccessor);
    const EffectRuleId earlierRule{ origin.ruleId.value - 1 };
    const EffectRuleId laterRule = authoredSuccessor.id;
    BattleEffectDispatchResult dispatched;
    dispatched.commands = {
        orderedDamageCommand(origin.binding, earlierRule, origin.ruleOrder - 1),
        // Removing the explicit consumer shifts every later bound rule down by
        // one runtime order. The intrinsic command retains the vacated order
        // and must sort before that now-equal successor.
        orderedDamageCommand(origin.binding, laterRule, origin.ruleOrder + 1),
    };
    BattleAttackEvent event;
    event.type = BattleAttackEventType::Hit;
    event.sourceUnitId = 0;
    event.unitId = 1;
    event.position = { 105, 100, 0 };
    event.provenance = state.attacks.attacks.front().provenance;

    CoreDetail::insertTrueQiHitDamage(state, event, dispatched);

    REQUIRE(dispatched.commands.size() == 3);
    const auto& intrinsic = dispatched.commands[1];
    CHECK(intrinsic.metadata.binding == origin.binding);
    CHECK(intrinsic.metadata.ruleId == intrinsicStatusEffectRuleId(origin.ruleId));
    CHECK(intrinsic.metadata.ruleId != authoredSuccessor.id);
    CHECK(intrinsic.metadata.event == EffectEvent::HitBeforeDamage);
    CHECK(intrinsic.metadata.ruleOrder == origin.ruleOrder + 1);
    CHECK(intrinsic.metadata.actionOrder == 0);
    CHECK(intrinsic.metadata.targetOrder == 0);
    CHECK(intrinsic.metadata.targetUnitId == 1);
    CHECK(intrinsic.metadata.eventSourceUnitId == 0);
    CHECK(intrinsic.metadata.commandOrdinal == 1);
    CHECK(dispatched.commands[0].metadata.commandOrdinal == 0);
    CHECK(dispatched.commands[2].metadata.commandOrdinal == 2);

    const auto& damage = std::get<DealDamageEffectCommand>(intrinsic.value);
    CHECK(damage.amount == 27);
    CHECK(damage.action.amount.flat == 27);
    CHECK(damage.action.kind == BattleDamageKind::Pure);
    CHECK(damage.action.appliesDamageModifiers);
    CHECK(damage.action.triggersHurtInvincibility);
    CHECK(damage.transactionCount == 1);

    REQUIRE(dispatched.activations.size() == 1);
    CHECK(dispatched.activations.front().binding == origin.binding);
    CHECK(dispatched.activations.front().ruleId
        == intrinsicStatusEffectRuleId(origin.ruleId));
    CHECK(dispatched.activations.front().targetUnitIds == std::vector<int>{ 1 });
}

TEST_CASE("True-Qi keeps its exact hit-command slot across repeated runtime rule removal",
          "[battle][core][true-qi][ordering][rule-lifecycle]")
{
    auto frame = hitDamageFrameState(20, 100);
    auto& state = frame.state;

    ChangeResourceAction borrowedShield;
    borrowedShield.resource = BattleResource::Shield;
    borrowedShield.kind = ResourceChangeKind::Grant;
    borrowedShield.amount.flat = 1;
    EffectRule borrowable;
    borrowable.id = EffectRuleId{ 7001 };
    borrowable.event = EffectEvent::HitBeforeDamage;
    borrowable.selector.kind = EffectSelectorKind::HitTarget;
    borrowable.actions = { EffectAction{ borrowedShield } };
    state.effectRules.append({
        .kind = EffectSourceKind::Magic,
        .sourceId = 7001,
        .ownerUnitId = 2,
        .sourceTeam = 1,
    }, borrowable);

    BorrowedRuleFilter filter;
    filter.allowedActionCategories = { BorrowedRuleActionCategory::ResourceChange };
    const std::array borrowedOwner{ 2 };
    state.effectRules.bindBorrowedUltimateRules(
        BattleCastId{ 81 }, 0, 0, borrowedOwner, filter,
        CastPropagationPolicy::BorrowedUltimateRules);

    ApplyStatusAction applyTrueQi;
    applyTrueQi.status = BattleStatusKind::TrueQi;
    applyTrueQi.quantity = AddStatusLayers{ 1, 10 };
    applyTrueQi.effects = TrueQiStatusEffects{
        .pureDamagePerHit = EffectNumber{ .flat = 9 },
    };
    EffectRule producer;
    producer.id = EffectRuleId{ 7002 };
    producer.event = EffectEvent::AttackCommitted;
    producer.selector.kind = EffectSelectorKind::Self;
    producer.actions = { EffectAction{ applyTrueQi } };
    const EffectSourceBinding producerBinding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 106,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    state.effectRules.append(producerBinding, producer);

    EffectRule successor;
    successor.id = EffectRuleId{ 7003 };
    successor.event = EffectEvent::HitBeforeDamage;
    successor.selector.kind = EffectSelectorKind::HitTarget;
    successor.actions = { EffectAction{ DealDamageAction{
        .amount = EffectNumber{ .flat = 1 },
        .kind = BattleDamageKind::Pure,
    } } };
    state.effectRules.append(producerBinding, successor);

    const auto producerBound = std::ranges::find_if(
        state.effectRules.rules(), [&](const BoundEffectRule& bound)
        {
            return bound.binding == producerBinding && bound.rule.id == producer.id;
        });
    const auto successorBound = std::ranges::find_if(
        state.effectRules.rules(), [&](const BoundEffectRule& bound)
        {
            return bound.binding == producerBinding && bound.rule.id == successor.id;
        });
    REQUIRE(producerBound != state.effectRules.rules().end());
    REQUIRE(successorBound != state.effectRules.rules().end());
    const auto producerOrder = producerBound->order;
    const auto successorOrder = successorBound->order;
    REQUIRE(successorOrder == producerOrder + 1);

    state.effectRules.removeCastScopedRules(BattleCastId{ 81 });
    for (std::uint64_t cycle = 82; cycle < 86; ++cycle)
    {
        state.effectRules.bindBorrowedUltimateRules(
            BattleCastId{ cycle }, 0, 0, borrowedOwner, filter,
            CastPropagationPolicy::BorrowedUltimateRules);
        state.effectRules.removeCastScopedRules(BattleCastId{ cycle });
    }

    const auto liveProducer = std::ranges::find_if(
        state.effectRules.rules(), [&](const BoundEffectRule& bound)
        {
            return bound.binding == producerBinding && bound.rule.id == producer.id;
        });
    const auto liveSuccessor = std::ranges::find_if(
        state.effectRules.rules(), [&](const BoundEffectRule& bound)
        {
            return bound.binding == producerBinding && bound.rule.id == successor.id;
        });
    REQUIRE(liveProducer != state.effectRules.rules().end());
    REQUIRE(liveSuccessor != state.effectRules.rules().end());
    CHECK(liveProducer->order == producerOrder);
    CHECK(liveSuccessor->order == successorOrder);

    auto& effects = state.units.require(0).status.effects;
    effects.statuses.push_back({
        .kind = BattleStatusKind::TrueQi,
        .sourceUnitId = 0,
        .stacks = 3,
        .potency = 9,
        .origin = BattleStatusEffectOrigin{
            .binding = producerBinding,
            .ruleId = producer.id,
            .ruleOrder = producerOrder,
        },
        .appliedSequence = effects.nextStatusSequence++,
    });
    BattleEffectDispatchResult dispatched;
    dispatched.commands = {
        orderedDamageCommand(producerBinding, successor.id, successorOrder),
    };
    BattleAttackEvent event;
    event.type = BattleAttackEventType::Hit;
    event.sourceUnitId = 0;
    event.unitId = 1;
    event.position = { 105, 100, 0 };
    event.provenance = state.attacks.attacks.front().provenance;

    CoreDetail::insertTrueQiHitDamage(state, event, dispatched);

    REQUIRE(dispatched.commands.size() == 2);
    CHECK(dispatched.commands.front().metadata.ruleId
        == intrinsicStatusEffectRuleId(producer.id));
    CHECK(dispatched.commands.front().metadata.ruleOrder == successorOrder);
    CHECK(dispatched.commands.back().metadata.ruleId == successor.id);
}

TEST_CASE("True-Qi insertion shares cross-source precedence with normal dispatch",
          "[battle][core][true-qi][ordering][source-precedence]")
{
    auto frame = hitDamageFrameState(20, 100);
    auto& state = frame.state;
    const auto origin = addTrueQiStatus(state, 0, 3, 9);
    const auto binding = [](EffectSourceKind kind, int sourceId)
    {
        return EffectSourceBinding{
            .kind = kind,
            .sourceId = sourceId,
            .ownerUnitId = 0,
            .sourceTeam = 0,
        };
    };

    BattleEffectDispatchResult dispatched;
    dispatched.commands = {
        orderedDamageCommand(binding(EffectSourceKind::Combo, 1), EffectRuleId{ 1 }, 99),
        orderedDamageCommand(binding(EffectSourceKind::Equipment, 2), EffectRuleId{ 2 }, 99),
        orderedDamageCommand(
            binding(EffectSourceKind::EquipmentSynergy, 3), EffectRuleId{ 3 }, 99),
        orderedDamageCommand(binding(EffectSourceKind::Neigong, 4), EffectRuleId{ 4 }, 99),
        orderedDamageCommand(origin.binding, EffectRuleId{ 5 }, origin.ruleOrder - 1),
        orderedDamageCommand(origin.binding, EffectRuleId{ 6 }, origin.ruleOrder + 1),
    };
    BattleAttackEvent event;
    event.type = BattleAttackEventType::Hit;
    event.sourceUnitId = 0;
    event.unitId = 1;
    event.position = { 105, 100, 0 };
    event.provenance = state.attacks.attacks.front().provenance;

    CoreDetail::insertTrueQiHitDamage(state, event, dispatched);

    REQUIRE(dispatched.commands.size() == 7);
    CHECK(dispatched.commands[0].metadata.binding.kind == EffectSourceKind::Combo);
    CHECK(dispatched.commands[1].metadata.binding.kind == EffectSourceKind::Equipment);
    CHECK(dispatched.commands[2].metadata.binding.kind
        == EffectSourceKind::EquipmentSynergy);
    CHECK(dispatched.commands[3].metadata.binding.kind == EffectSourceKind::Neigong);
    CHECK(dispatched.commands[4].metadata.ruleId == EffectRuleId{ 5 });
    CHECK(dispatched.commands[5].metadata.ruleId
        == intrinsicStatusEffectRuleId(origin.ruleId));
    CHECK(dispatched.commands[6].metadata.ruleId == EffectRuleId{ 6 });
}

TEST_CASE("Shipped Jiuyang producer carries its origin through reduction and a later hit",
          "[battle][core][true-qi][integration][origin]")
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path() / "config" / "chess_magic_effects.yaml";
    REQUIRE(loadMagicEffectsFile(path.string(), definitions));
    const auto jiuyang = std::ranges::find(
        definitions, 106, &ChessMagicEffectDefinition::magicId);
    REQUIRE(jiuyang != definitions.end());
    const auto producer = std::ranges::find(
        jiuyang->rules, EffectEvent::AttackCommitted, &EffectRule::event);
    REQUIRE(producer != jiuyang->rules.end());

    auto frame = hitDamageFrameState(20, 100);
    auto& state = frame.state;
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Magic,
        .sourceId = jiuyang->magicId,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    const auto boundIndex = state.effectRules.append(binding, *producer);
    REQUIRE(boundIndex < state.effectRules.rules().size());
    const auto producerOrder = state.effectRules.rules()[boundIndex].order;

    const auto producerCast = state.castLifecycle.beginRootCast({
        .sourceUnitId = 0,
        .magicId = jiuyang->magicId,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
    });
    state.effectIntegration.casts.emplace(
        producerCast.provenance.castId,
        BattleEffectCastRuntimeContext{ .originalTargetUnitId = 1 });
    state.castLifecycle.completeWork(producerCast.commitBarrier);
    const auto dispatched = BattleEffectEventBridge().dispatch(
        state,
        {
            .frame = state.movement.frame,
            .eventOrdinal = 1,
            .ownerUnitId = 0,
        },
        EffectEvent::AttackCommitted,
        CastCommitEventData{
            .provenance = producerCast.provenance,
            .targetUnitId = 1,
        });
    REQUIRE(dispatched.commands.size() == 2);
    const auto reduced = BattleEffectCommandSystem().reduce(
        state,
        dispatched.commands,
        {
            .frame = state.movement.frame,
            .cast = producerCast.provenance,
        });
    REQUIRE(reduced.entries.size() == 2);

    const auto& statuses = state.units.require(0).status.effects.statuses;
    REQUIRE(std::ranges::count(
        statuses, BattleStatusKind::TrueQi, &BattleTypedStatusInstance::kind) == 1);
    const auto* trueQi = state.units.require(0).status.effects.find(
        BattleStatusKind::TrueQi);
    REQUIRE(trueQi);
    CHECK(trueQi->stacks == 1);
    CHECK(trueQi->potency == 9);
    REQUIRE(trueQi->origin);
    CHECK(trueQi->origin->binding == binding);
    CHECK(trueQi->origin->ruleId == producer->id);
    CHECK(trueQi->origin->ruleOrder == producerOrder);

    const auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 9, 35 });
    CHECK(state.units.requireCore(1).vitals.hp == 56);
}

TEST_CASE("True-Qi damage is queued before the accepted base hit",
          "[battle][core][true-qi][ordering]")
{
    auto frame = hitDamageFrameState(20, 100);
    addTrueQiStatus(frame.state, 0, 3, 9);
    const auto castId = frame.state.attacks.attacks.front().provenance.cast.castId;

    const auto result = runBattleFrame(frame.state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 27, 35 });
    CHECK(frame.state.units.requireCore(1).vitals.hp == 38);
    const auto& aggregate = frame.state.castLifecycle.runtime(castId).aggregate;
    CHECK(aggregate.totalActualHpDamage == 62);
    CHECK(aggregate.highestActualHpDamage == 35);
    CHECK(aggregate.distinctHitUnitIds
        == std::set<int>{ 1 });
}

TEST_CASE("True-Qi damage behaviorally grants hurt invincibility before the base hit",
          "[battle][core][true-qi][ordering][hurt-invincibility]")
{
    auto frame = hitDamageFrameState(20, 100);
    addTrueQiStatus(frame.state, 0, 3, 9);
    frame.state.units.require(1).damage.hurtInvincFrames = 5;

    const auto result = runBattleFrame(frame.state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 27 });
    CHECK(frame.state.units.requireCore(1).vitals.hp == 73);
    CHECK(frame.state.units.requireCore(1).invincible == 5);
    CHECK(std::ranges::any_of(result.logEvents, [](const BattleLogEvent& event)
    {
        return event.type == BattleLogEventType::Status
            && event.targetUnitId == 1
            && event.amount == 5
            && BattleLogTest::textOf(event).contains("受傷無敵");
    }));
}

TEST_CASE("Main-projectile effects resolve before True-Qi and the accepted base hit",
          "[battle][core][true-qi][ordering][main-projectile]")
{
    auto frame = hitDamageFrameState(20, 100);
    addTrueQiStatus(frame.state, 0, 3, 9);

    EffectRule mainProjectile;
    mainProjectile.id = EffectRuleId{ 7100 };
    mainProjectile.event = EffectEvent::MainProjectileBeforeDamage;
    mainProjectile.castMatch = EffectCastMatch::OwnerAnyCast;
    mainProjectile.selector.kind = EffectSelectorKind::HitTarget;
    mainProjectile.actions = { EffectAction{ DealDamageAction{
        .amount = EffectNumber{ .flat = 5 },
        .kind = BattleDamageKind::Pure,
    } } };
    frame.state.effectRules.append({
        .kind = EffectSourceKind::Magic,
        .sourceId = 7100,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    }, mainProjectile);

    const auto result = runBattleFrame(frame.state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 5, 27, 35 });
    CHECK(frame.state.units.requireCore(1).vitals.hp == 33);
}

TEST_CASE("Lethal True-Qi damage resolves before and suppresses the base hit",
          "[battle][core][true-qi][ordering][death]")
{
    auto frame = hitDamageFrameState(20, 25);
    addTrueQiStatus(frame.state, 0, 3, 9);

    const auto result = runBattleFrame(frame.state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 25 });
    CHECK_FALSE(frame.state.units.requireCore(1).alive);
    const auto lifecycle = frame.state.castLifecycle.snapshot();
    REQUIRE(lifecycle.retiredCasts.size() == 1);
    CHECK(lifecycle.retiredCasts.front().aggregate.totalActualHpDamage == 25);
    CHECK(lifecycle.retiredCasts.front().aggregate.distinctHitUnitIds
        == std::set<int>{ 1 });
    CHECK(frame.state.castLifecycle.activeCastCount() == 0);
    CHECK(frame.state.castLifecycle.trackedWorkCount() == 0);
}

TEST_CASE("True-Qi contributes once for every accepted contact of a multi-hit attack",
          "[battle][core][true-qi][multi-hit]")
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 205, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    addTrueQiStatus(state, 0, 3, 9);

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = 240;
    projectile.state.preferredTargetUnitId = 1;
    projectile.state.requirePreferredTarget = true;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    appendTrackedAttack(state, std::move(projectile));

    const auto rootProvenance = state.attacks.attacks.front().provenance;
    BattleAttackInstance followUp{ ordinaryProjectilePayload() };
    followUp.id = 11;
    followUp.state.attackSourceUnitId = 0;
    followUp.state.skillId = 101;
    followUp.state.skillMagicPower = 240;
    followUp.state.preferredTargetUnitId = 2;
    followUp.state.requirePreferredTarget = true;
    followUp.state.totalFrame = 30;
    followUp.state.operationType = BattleOperationType::RangedProjectile;
    followUp.state.position = { 200, 100, 0 };
    followUp.state.velocity = { 5, 0, 0 };
    const auto reservation = state.castLifecycle.reserveAttack(
        rootProvenance.cast.castId,
        {
            .parentAttackId = rootProvenance.attackId,
            .origin = BattleAttackOriginKind::FollowUp,
            .mainProjectile = false,
        });
    followUp.provenance = completeAttackProvenance(
        reservation.provenance,
        battleAttackIdFromRuntimeId(followUp.id));
    followUp.castWork = reservation.work;
    state.castLifecycle.transferToLiveAttack(
        followUp.castWork,
        followUp.provenance.attackId);
    state.attacks.attacks.push_back(std::move(followUp));

    const auto castId = state.attacks.attacks.front().provenance.cast.castId;
    const auto result = runBattleFrame(state);

    const auto firstTargetDamage = damageLogAmountsFor(result, 1);
    const auto secondTargetDamage = damageLogAmountsFor(result, 2);
    REQUIRE(firstTargetDamage.size() >= 2);
    REQUIRE(secondTargetDamage.size() >= 2);
    CHECK(firstTargetDamage.front() == 27);
    CHECK(secondTargetDamage.front() == 27);
    const auto& aggregate = state.castLifecycle.runtime(castId).aggregate;
    CHECK(aggregate.distinctHitUnitIds == std::set<int>{ 1, 2 });
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
