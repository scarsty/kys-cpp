#include "battle/BattleEffectEventBridge.h"
#include "battle/BattleRuntimeUnits.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <set>
#include <utility>
#include <variant>
#include <vector>

using namespace KysChess;
using namespace KysChess::Battle;

namespace
{

BattleRuntimeUnitRecord runtimeUnit(
    int id,
    int team,
    int hp,
    int maxHp,
    int mp = 100,
    int maxMp = 100)
{
    BattleRuntimeUnitRecord record;
    record.core.id = id;
    record.core.team = team;
    record.core.alive = hp > 0;
    record.core.vitals = { hp, maxHp, mp, maxMp };
    record.core.stats = { 120 + id, 40 + id, 30 + id };
    record.core.motion.position = Pointf{ static_cast<float>(id * 10), 0.0f };
    record.core.star = id;
    return record;
}

BattleRuntimeState runtimeWithTwoUnits()
{
    BattleRuntimeState runtime;
    runtime.gridTransform.tileWidth = 32.0;
    // Runtime snapshots must not inherit append order as selector order.
    runtime.units.append(runtimeUnit(2, 1, 90, 100));
    runtime.units.append(runtimeUnit(1, 0, 100, 100));
    return runtime;
}

EffectSourceBinding binding(EffectSourceKind kind, int sourceId, int ownerUnitId, int sourceTeam)
{
    return {
        .kind = kind,
        .sourceId = sourceId,
        .ownerUnitId = ownerUnitId,
        .sourceTeam = sourceTeam,
    };
}

template<class Action>
EffectAction action(Action value)
{
    return { EffectActionValue{ std::move(value) } };
}

EffectRule resourceRule(
    std::uint64_t id,
    EffectEvent event,
    int amount,
    EffectNumberBase base = EffectNumberBase::Constant)
{
    ChangeResourceAction resource;
    resource.resource = BattleResource::Shield;
    resource.kind = ResourceChangeKind::Grant;
    resource.amount.base = base;
    resource.amount.flat = amount;
    if (base != EffectNumberBase::Constant)
    {
        resource.amount.percent = 100;
    }

    EffectRule rule;
    rule.id = EffectRuleId{ id };
    rule.event = event;
    rule.selector.kind = event == EffectEvent::DamageResolved
        ? EffectSelectorKind::TransactionTarget
        : EffectSelectorKind::Self;
    rule.actions.push_back(action(std::move(resource)));
    return rule;
}

BattleAttackProvenance attackProvenance(const BattleCastProvenance& cast)
{
    return {
        .cast = cast,
        .propagation = cast.propagation,
        .origin = BattleAttackOriginKind::Initial,
        .attackId = BattleAttackId(91),
        .attackOrdinal = 0,
        .rootAttack = true,
        .mainProjectile = true,
    };
}

BattleCastProvenance ultimateCast(std::uint64_t id, int magicId = 43)
{
    return {
        .rootCastId = BattleCastId(id),
        .castId = BattleCastId(id),
        .sourceUnitId = 1,
        .magicId = magicId,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
        .propagation = CastPropagationPolicy::SourceRules,
    };
}

EffectRule borrowRule(std::uint64_t id = 1)
{
    BorrowEffectRulesAction borrow;
    borrow.sourceUnits.kind = EffectSelectorKind::Enemies;
    borrow.sourceUnits.tieBreak = EffectTieBreak::BattleRandom;
    borrow.sourceCount.base = EffectNumberBase::SourceStar;
    borrow.sourceCount.percent = 50;
    borrow.sourceCount.rounding = EffectRounding::Ceil;
    borrow.sourceCount.minimum = 1;
    borrow.sourceCount.maximum = 2;
    borrow.filter.allowedActionCategories = {
        BorrowedRuleActionCategory::ResourceChange,
    };
    borrow.propagation = CastPropagationPolicy::BorrowedUltimateRules;

    EffectRule rule;
    rule.id = EffectRuleId{ id };
    rule.event = EffectEvent::CastPlanned;
    rule.selector.kind = EffectSelectorKind::Self;
    rule.actions.push_back(action(StateMachineAction{ std::move(borrow) }));
    return rule;
}

std::vector<int> resourceAmounts(const BattleEffectDispatchResult& result)
{
    std::vector<int> amounts;
    for (const auto& command : result.commands)
    {
        if (const auto* resource = std::get_if<ChangeResourceEffectCommand>(&command.value))
        {
            amounts.push_back(resource->amount);
        }
    }
    std::ranges::sort(amounts);
    return amounts;
}

}  // namespace

TEST_CASE("BattleRuntimeEffects snapshots include active typed core attributes",
          "[battle][effect][snapshot]")
{
    auto runtime = runtimeWithTwoUnits();
    ModifyAttributeAction buff;
    buff.attribute = BattleAttribute::Attack;
    buff.operation = AttributeOperation::FlatAdd;
    buff.amount.flat = 30;
    buff.durationFrames = 20;
    buff.stack = EffectStackPolicy::Refresh;

    EffectCommand command;
    command.metadata.binding = binding(EffectSourceKind::Magic, 16, 1, 0);
    command.metadata.ruleId = EffectRuleId{ 1 };
    command.metadata.event = EffectEvent::UltimateCommitted;
    command.metadata.targetUnitId = 1;
    command.value = ModifyAttributeEffectCommand{ buff, 30 };
    BattleEffectCommandSystem().reduce(runtime, command, { .frame = 5 });

    runtime.movement.frame = 6;
    const auto snapshot = makeEffectUnitSnapshot(runtime, runtime.units.require(1));
    CHECK(snapshot.attack == runtime.units.requireCore(1).stats.attack + 30);
}

TEST_CASE("BattleEffectEventBridge preserves continuation and settlement payloads",
          "[battle][effect][bridge][lifecycle]")
{
    auto runtime = runtimeWithTwoUnits();
    const auto magic = binding(EffectSourceKind::Magic, 71, 1, 0);
    runtime.effectRules.append(magic, resourceRule(1, EffectEvent::CastContinuation, 11));
    runtime.effectRules.append(magic, resourceRule(2, EffectEvent::CastSettled, 22));

    const auto cast = runtime.castLifecycle.beginRootCast({
        .sourceUnitId = 1,
        .magicId = 71,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
    });
    const auto reservation = runtime.castLifecycle.reserveAttack(
        cast.provenance.castId,
        { .rootAttack = true });
    runtime.castLifecycle.completeWork(cast.commitBarrier);
    runtime.castLifecycle.transferToLiveAttack(reservation.work, BattleAttackId(91));
    const auto provenance = attackProvenance(cast.provenance);
    runtime.castLifecycle.recordHit(provenance, 2);
    runtime.castLifecycle.recordActualHpDamage(provenance, 2, 37);
    runtime.castLifecycle.completeWork(
        reservation.work,
        CastWorkResult::attackFinished(AttackFinishReason::SpentOnHit));

    BattleEffectEventBridge bridge;
    const auto continuationEvents = runtime.castLifecycle.drainReadyEvents(runtime.movement.frame);
    REQUIRE(continuationEvents.size() == 1);
    const auto continuation = bridge.makeCastLifecycleEvent(
        runtime,
        { .frame = 12, .eventOrdinal = 1001, .ownerUnitId = 1 },
        continuationEvents.front(),
        {
            .originalTargetUnitId = 2,
            .resourcesBeforeCast = { { 1, 100, 100 }, { 2, 60, 100 } },
        });
    runtime.units.requireCore(1).vitals.hp = 44;
    const auto continuationContext = continuation.context();
    CHECK(continuation.event() == EffectEvent::CastContinuation);
    CHECK(continuationContext.header.eventOrdinal == 1001);
    CHECK(continuationContext.header.owner.id == 1);
    CHECK(continuationContext.header.owner.hp == 100);
    CHECK(continuationContext.header.battle.findUnit(1)->hp == 100);
    CHECK(continuationContext.header.battle.units()[0].id == 1);
    CHECK(continuationContext.header.battle.units()[1].id == 2);
    const auto& continuationPayload =
        std::get<CastAggregateEventData>(continuationContext.payload);
    CHECK(continuationPayload.originalTargetUnitId == 2);
    CHECK(continuationPayload.resourcesBeforeCast.size() == 2);
    CHECK(continuationPayload.aggregate.distinctHitUnitIds == std::set<int>{ 2 });
    CHECK(continuationPayload.aggregate.highestActualHpDamage == 37);
    CHECK(continuationPayload.aggregate.totalActualHpDamage == 37);

    const auto continuationDispatch = bridge.dispatch(runtime, continuation);
    REQUIRE(continuationDispatch.commands.size() == 1);
    CHECK(std::get<ChangeResourceEffectCommand>(
        continuationDispatch.commands.front().value).amount == 11);

    const auto settledEvents = runtime.castLifecycle.drainReadyEvents(runtime.movement.frame);
    REQUIRE(settledEvents.size() == 1);
    const auto settled = bridge.makeCastLifecycleEvent(
        runtime,
        { .frame = 12, .eventOrdinal = 1002, .ownerUnitId = 1 },
        settledEvents.front(),
        {
            .originalTargetUnitId = 2,
            .resourcesBeforeCast = { { 1, 100, 100 }, { 2, 60, 100 } },
        });
    const auto settledContext = settled.context();
    CHECK(settled.event() == EffectEvent::CastSettled);
    CHECK(settledContext.header.eventOrdinal == 1002);
    const auto& settledPayload = std::get<CastAggregateEventData>(settledContext.payload);
    CHECK(settledPayload.provenance.castId == cast.provenance.castId);
    CHECK(settledPayload.aggregate.totalActualHpDamage == 37);

    const auto settledDispatch = bridge.dispatch(runtime, settled);
    REQUIRE(settledDispatch.commands.size() == 1);
    CHECK(std::get<ChangeResourceEffectCommand>(
        settledDispatch.commands.front().value).amount == 22);
}

TEST_CASE("BattleEffectEventBridge converts completed attack damage without guessing pre-state",
          "[battle][effect][bridge][damage]")
{
    auto runtime = runtimeWithTwoUnits();
    runtime.units.requireCore(2).shield = 20;
    const auto attackerBefore = makeEffectUnitSnapshot(runtime, runtime.units.require(1));
    const auto defenderBefore = makeEffectUnitSnapshot(runtime, runtime.units.require(2));

    BattleDamageTransactionInput damage;
    damage.request.attackerUnitId = 1;
    damage.request.defenderUnitId = 2;
    damage.request.baseDamage = 75;
    damage.request.acceptedHit = true;
    damage.request.preResolvedDamage = true;
    damage.request.damageKind = BattleDamageKind::Skill;
    damage.attacker = runtime.units.require(1).damageState(0);
    damage.defender = runtime.units.require(2).damageState(0);
    damage.attackerStatus = runtime.units.require(1).statusDamageState();
    damage.defenderStatus = runtime.units.require(2).statusDamageState();
    const auto transaction = BattleDamageSystem{}.resolveTransaction(damage);
    REQUIRE(transaction.shieldAbsorbed == 20);
    REQUIRE(transaction.finalHpDamage == 55);

    runtime.units.writeDamageUnit(transaction.attacker);
    runtime.units.writeDamageUnit(transaction.defender);
    runtime.units.require(2).writeStatusDamageResult(transaction.defenderStatus);

    const auto matchingMagic = binding(EffectSourceKind::Magic, 79, 1, 0);
    const auto otherMagic = binding(EffectSourceKind::Magic, 80, 1, 0);
    runtime.effectRules.append(
        matchingMagic,
        resourceRule(10, EffectEvent::DamageResolved, 0, EffectNumberBase::FinalHpDamage));
    runtime.effectRules.append(otherMagic, resourceRule(11, EffectEvent::DamageResolved, 999));

    const BattleCastProvenance cast{
        .rootCastId = BattleCastId(7),
        .castId = BattleCastId(7),
        .sourceUnitId = 1,
        .magicId = 79,
        .ultimate = true,
        .origin = CastOriginKind::Ultimate,
    };
    const auto provenance = attackProvenance(cast);
    BattleEffectEventBridge bridge;
    const auto event = bridge.makeDamageResolvedEvent(
        runtime,
        {
            .frame = 21,
            .eventOrdinal = 2001,
            .ownerUnitId = 1,
            .formulaInputs = {
                .accumulatedStateValue = 9,
            },
        },
        transaction,
        EffectAttackDamageOrigin{ provenance },
        {
            .transactionId = 501,
            .attackerBefore = attackerBefore,
            .defenderBefore = defenderBefore,
            .rawDamage = 75,
            .resolvedDamage = 75,
        });

    const auto context = event.context();
    CHECK(context.header.eventOrdinal == 2001);
    CHECK(context.header.formulaInputs.accumulatedStateValue == 9);
    const auto& payload = std::get<DamageResultEventData>(context.payload);
    CHECK(payload.transactionId == 501);
    CHECK(payload.attackerBefore->hp == 100);
    CHECK(payload.defenderBefore.hp == 90);
    CHECK(payload.defenderBefore.shield == 20);
    CHECK(payload.defenderAfter.hp == 35);
    CHECK(payload.defenderAfter.shield == 0);
    CHECK(payload.rawDamage == 75);
    CHECK(payload.resolvedDamage == 75);
    CHECK(payload.shieldAbsorbed == 20);
    CHECK(payload.finalHpDamage == 55);
    CHECK_FALSE(payload.blocked);

    const auto dispatched = bridge.dispatch(runtime, event);
    REQUIRE(dispatched.activations.size() == 1);
    CHECK(dispatched.activations.front().binding.sourceId == 79);
    REQUIRE(dispatched.commands.size() == 1);
    const auto& command =
        std::get<ChangeResourceEffectCommand>(dispatched.commands.front().value);
    CHECK(command.amount == 55);
    CHECK(dispatched.commands.front().metadata.targetUnitId == 2);
}

TEST_CASE("BattleEffectEventBridge preserves explicit attackerless damage origins",
          "[battle][effect][bridge][damage]")
{
    auto runtime = runtimeWithTwoUnits();
    const auto defenderBefore = makeEffectUnitSnapshot(runtime, runtime.units.require(2));

    BattleDamageTransactionInput damage;
    damage.request.attackerUnitId = OptionalDamageAttackerUnitId;
    damage.request.defenderUnitId = 2;
    damage.request.baseDamage = 13;
    damage.request.acceptedHit = true;
    damage.request.preResolvedDamage = true;
    damage.attacker.id = OptionalDamageAttackerUnitId;
    damage.defender = runtime.units.require(2).damageState(0);
    damage.defenderStatus = runtime.units.require(2).statusDamageState();
    const auto transaction = BattleDamageSystem{}.resolveTransaction(damage);
    REQUIRE(transaction.finalHpDamage == 13);

    runtime.units.writeDamageUnit(transaction.defender);
    runtime.units.require(2).writeStatusDamageResult(transaction.defenderStatus);
    runtime.effectRules.append(
        binding(EffectSourceKind::Equipment, 81, 2, 1),
        resourceRule(12, EffectEvent::DamageResolved, 0, EffectNumberBase::FinalHpDamage));

    BattleEffectEventBridge bridge;
    const auto event = bridge.makeDamageResolvedEvent(
        runtime,
        { .frame = 22, .eventOrdinal = 2002, .ownerUnitId = 2 },
        transaction,
        EffectEnvironmentDamageOrigin{},
        {
            .transactionId = 502,
            .defenderBefore = defenderBefore,
            .rawDamage = 13,
            .resolvedDamage = 13,
        });

    const auto& payload = std::get<DamageResultEventData>(event.payload());
    CHECK(std::holds_alternative<EffectEnvironmentDamageOrigin>(payload.origin));
    CHECK_FALSE(payload.attackerBefore);
    CHECK(payload.defenderBefore.hp == 90);
    CHECK(payload.defenderAfter.hp == 77);

    const auto dispatched = bridge.dispatch(runtime, event);
    REQUIRE(dispatched.commands.size() == 1);
    CHECK(std::get<ChangeResourceEffectCommand>(
        dispatched.commands.front().value).amount == 13);
    CHECK(dispatched.commands.front().metadata.targetUnitId == 2);
}

TEST_CASE("BattleEffectEventBridge dispatch is deterministically scoped to the specified owner",
          "[battle][effect][bridge][owner]")
{
    auto runtime = runtimeWithTwoUnits();
    const auto ownerOne = binding(EffectSourceKind::Equipment, 301, 1, 0);
    const auto ownerTwo = binding(EffectSourceKind::Equipment, 302, 2, 1);
    runtime.effectRules.append(ownerTwo, resourceRule(20, EffectEvent::FrameAdvanced, 202));
    runtime.effectRules.append(ownerOne, resourceRule(21, EffectEvent::FrameAdvanced, 101));

    BattleEffectEventBridge bridge;
    const auto ownerTwoResult = bridge.dispatch(
        runtime,
        { .frame = 8, .eventOrdinal = 3001, .ownerUnitId = 2 },
        EffectEvent::FrameAdvanced,
        FrameTickEventData{ .deltaFrames = 1, .periodOrdinal = 8 });
    REQUIRE(ownerTwoResult.commands.size() == 1);
    CHECK(ownerTwoResult.commands.front().metadata.binding.ownerUnitId == 2);
    CHECK(ownerTwoResult.commands.front().metadata.binding.sourceId == 302);
    CHECK(ownerTwoResult.commands.front().metadata.targetUnitId == 2);
    CHECK(std::get<ChangeResourceEffectCommand>(
        ownerTwoResult.commands.front().value).amount == 202);

    const auto ownerOneResult = bridge.dispatch(
        runtime,
        { .frame = 8, .eventOrdinal = 3002, .ownerUnitId = 1 },
        EffectEvent::FrameAdvanced,
        FrameTickEventData{ .deltaFrames = 1, .periodOrdinal = 8 });
    REQUIRE(ownerOneResult.commands.size() == 1);
    CHECK(ownerOneResult.commands.front().metadata.binding.ownerUnitId == 1);
    CHECK(ownerOneResult.commands.front().metadata.binding.sourceId == 301);
    CHECK(ownerOneResult.commands.front().metadata.targetUnitId == 1);
    CHECK(std::get<ChangeResourceEffectCommand>(
        ownerOneResult.commands.front().value).amount == 101);
}

TEST_CASE("BattleEffectEventBridge rebinds borrowed ultimate rules for one cast without recursive copies",
          "[battle][effect][bridge][borrow]")
{
    BattleRuntimeState runtime;
    runtime.gridTransform.tileWidth = 32.0;
    auto caster = runtimeUnit(1, 0, 100, 100);
    caster.core.star = 3;
    runtime.units.append(std::move(caster));
    runtime.units.append(runtimeUnit(2, 1, 100, 100));
    runtime.units.append(runtimeUnit(3, 1, 100, 100));

    const auto casterMagic = binding(EffectSourceKind::Magic, 43, 1, 0);
    const auto enemyTwoMagic = binding(EffectSourceKind::Magic, 71, 2, 1);
    const auto enemyThreeMagic = binding(EffectSourceKind::Magic, 72, 3, 1);
    runtime.effectRules.append(casterMagic, borrowRule());

    const auto appendBorrowableRules = [&](EffectSourceBinding source,
                                           int commitAmount,
                                           int hitAmount,
                                           int continuationAmount,
                                           int settledAmount,
                                           int receivedDamageAmount)
    {
        auto committed = resourceRule(10, EffectEvent::UltimateCommitted, commitAmount);
        committed.conditions = {
            IsUltimateCondition{},
            MagicIdEqualsCondition{ source.sourceId },
        };

        CopyAttackDefinitionAction copy;
        copy.sourceUnits.kind = EffectSelectorKind::AllLivingUnits;
        copy.sourceUnits.excludeOwner = true;
        copy.copyCount = 1;
        copy.filter.conditions = {
            CopiedMagicCondition::HasUltimateAttackDefinition,
            CopiedMagicCondition::ExcludesRecursiveEffects,
        };
        copy.propagation = CastPropagationPolicy::SuppressUltimateRules;
        EffectRule recursiveCopy;
        recursiveCopy.id = EffectRuleId{ 15 };
        recursiveCopy.event = EffectEvent::UltimateCommitted;
        recursiveCopy.selector.kind = EffectSelectorKind::Self;
        recursiveCopy.actions.push_back(action(StateMachineAction{ copy }));
        runtime.effectRules.append(source, recursiveCopy);

        BorrowEffectRulesAction nestedBorrow;
        nestedBorrow.sourceUnits.kind = EffectSelectorKind::Enemies;
        nestedBorrow.sourceCount.flat = 1;
        nestedBorrow.filter.allowedActionCategories = {
            BorrowedRuleActionCategory::ResourceChange,
        };
        nestedBorrow.propagation = CastPropagationPolicy::BorrowedUltimateRules;
        EffectRule recursiveBorrow;
        recursiveBorrow.id = EffectRuleId{ 16 };
        recursiveBorrow.event = EffectEvent::CastPlanned;
        recursiveBorrow.selector.kind = EffectSelectorKind::Self;
        recursiveBorrow.actions.push_back(action(StateMachineAction{ nestedBorrow }));
        runtime.effectRules.append(source, recursiveBorrow);
        runtime.effectRules.append(source, committed);

        ModifyAttributeAction disallowedAttribute;
        disallowedAttribute.attribute = BattleAttribute::Attack;
        disallowedAttribute.operation = AttributeOperation::FlatAdd;
        disallowedAttribute.amount.flat = 99;
        EffectRule disallowedRule;
        disallowedRule.id = EffectRuleId{ 17 };
        disallowedRule.event = EffectEvent::UltimateCommitted;
        disallowedRule.selector.kind = EffectSelectorKind::Self;
        disallowedRule.actions.push_back(action(std::move(disallowedAttribute)));
        runtime.effectRules.append(source, disallowedRule);

        auto hit = resourceRule(11, EffectEvent::HitBeforeDamage, hitAmount);
        hit.conditions = {
            IsUltimateCondition{},
            MagicIdEqualsCondition{ source.sourceId },
        };
        runtime.effectRules.append(source, hit);
        runtime.effectRules.append(
            source,
            resourceRule(12, EffectEvent::CastContinuation, continuationAmount));
        runtime.effectRules.append(
            source,
            resourceRule(13, EffectEvent::CastSettled, settledAmount));
        auto received = resourceRule(
            14,
            EffectEvent::DamageResolved,
            receivedDamageAmount);
        received.conditions = {
            DamagePerspectiveCondition{ DamagePerspective::Received },
        };
        runtime.effectRules.append(source, received);
    };
    appendBorrowableRules(enemyTwoMagic, 12, 21, 31, 41, 51);
    appendBorrowableRules(enemyThreeMagic, 13, 22, 32, 42, 52);

    const auto cast = ultimateCast(700);
    BattleEffectEventBridge bridge;
    const auto planned = bridge.dispatch(
        runtime,
        { .frame = 5, .eventOrdinal = 100, .ownerUnitId = 1 },
        EffectEvent::CastPlanned,
        CastPlanEventData{ .provenance = cast, .preferredTargetUnitId = 2 });

    REQUIRE(runtime.effectRules.castScopedRuleCount(cast.castId) == 10);
    REQUIRE(planned.commands.size() == 1);
    const auto borrowedCommand = std::ranges::find_if(planned.commands, [](const auto& command)
    {
        const auto* stateMachine = std::get_if<StateMachineEffectCommand>(&command.value);
        return stateMachine
            && std::holds_alternative<BorrowEffectRulesAction>(stateMachine->action);
    });
    REQUIRE(borrowedCommand != planned.commands.end());
    const auto& borrowed = std::get<StateMachineEffectCommand>(borrowedCommand->value);
    CHECK(borrowed.selectedSourceUnitIds.size() == 2);
    CHECK(borrowed.outputValue == 2);

    const auto committed = bridge.dispatch(
        runtime,
        { .frame = 5, .eventOrdinal = 101, .ownerUnitId = 1 },
        EffectEvent::UltimateCommitted,
        CastCommitEventData{ .provenance = cast, .targetUnitId = 2 });
    CHECK(resourceAmounts(committed) == std::vector{ 12, 13 });
    REQUIRE(committed.commands.size() == 2);
    CHECK(std::ranges::none_of(planned.commands, [](const auto& command)
    {
        const auto* stateMachine = std::get_if<StateMachineEffectCommand>(&command.value);
        return stateMachine
            && std::holds_alternative<CopyAttackDefinitionAction>(stateMachine->action);
    }));
    CHECK(std::ranges::none_of(committed.commands, [](const auto& command)
    {
        return std::holds_alternative<ModifyAttributeEffectCommand>(command.value);
    }));

    std::set<int> borrowedMagicIds;
    std::set<std::uint64_t> borrowedInstances;
    for (const auto& command : committed.commands)
    {
        if (!std::holds_alternative<ChangeResourceEffectCommand>(command.value))
        {
            continue;
        }
        CHECK(command.metadata.binding.ownerUnitId == 1);
        CHECK(command.metadata.binding.sourceTeam == 0);
        CHECK(command.metadata.binding.runtimeInstanceId != 0);
        borrowedMagicIds.insert(command.metadata.binding.sourceId);
        borrowedInstances.insert(command.metadata.binding.runtimeInstanceId);
    }
    CHECK(borrowedMagicIds == std::set<int>{ 71, 72 });
    CHECK(borrowedInstances.size() == 2);
    CHECK(runtime.effectRules.activationCount(enemyTwoMagic, EffectRuleId{ 10 }) == 0);
    CHECK(runtime.effectRules.activationCount(enemyThreeMagic, EffectRuleId{ 10 }) == 0);

    const auto owner = makeEffectUnitSnapshot(runtime, runtime.units.require(1));
    const auto defender = makeEffectUnitSnapshot(runtime, runtime.units.require(2));
    const auto hit = bridge.dispatch(
        runtime,
        { .frame = 6, .eventOrdinal = 102, .ownerUnitId = 1 },
        EffectEvent::HitBeforeDamage,
        HitEventData{
            .provenance = attackProvenance(cast),
            .attackerBefore = owner,
            .defenderBefore = defender,
            .originalTargetUnitId = 2,
            .damageKind = BattleDamageKind::Skill,
        });
    CHECK(resourceAmounts(hit) == std::vector{ 21, 22 });

    auto unrelatedCast = cast;
    unrelatedCast.rootCastId = BattleCastId(701);
    unrelatedCast.castId = BattleCastId(701);
    const auto unrelatedHit = bridge.dispatch(
        runtime,
        { .frame = 6, .eventOrdinal = 103, .ownerUnitId = 1 },
        EffectEvent::HitBeforeDamage,
        HitEventData{
            .provenance = attackProvenance(unrelatedCast),
            .attackerBefore = owner,
            .defenderBefore = defender,
            .originalTargetUnitId = 2,
            .damageKind = BattleDamageKind::Skill,
        });
    CHECK(unrelatedHit.commands.empty());

    const auto receivedDamage = bridge.dispatch(
        runtime,
        { .frame = 6, .eventOrdinal = 104, .ownerUnitId = 1 },
        EffectEvent::DamageResolved,
        DamageResultEventData{
            .origin = EffectAttackDamageOrigin{ attackProvenance(unrelatedCast) },
            .attackerBefore = defender,
            .defenderBefore = owner,
            .defenderAfter = owner,
            .finalHpDamage = 10,
            .damageKind = BattleDamageKind::Skill,
        });
    CHECK(resourceAmounts(receivedDamage) == std::vector{ 51, 52 });

    const auto continuation = bridge.dispatch(
        runtime,
        { .frame = 7, .eventOrdinal = 105, .ownerUnitId = 1 },
        EffectEvent::CastContinuation,
        CastAggregateEventData{ .provenance = cast, .originalTargetUnitId = 2 });
    CHECK(resourceAmounts(continuation) == std::vector{ 31, 32 });

    const auto settled = bridge.dispatch(
        runtime,
        { .frame = 7, .eventOrdinal = 106, .ownerUnitId = 1 },
        EffectEvent::CastSettled,
        CastAggregateEventData{ .provenance = cast, .originalTargetUnitId = 2 });
    CHECK(resourceAmounts(settled) == std::vector{ 41, 42 });
    CHECK(runtime.effectRules.castScopedRuleCount(cast.castId) == 0);

    const auto afterSettlement = bridge.dispatch(
        runtime,
        { .frame = 8, .eventOrdinal = 107, .ownerUnitId = 1 },
        EffectEvent::HitBeforeDamage,
        HitEventData{
            .provenance = attackProvenance(cast),
            .attackerBefore = owner,
            .defenderBefore = defender,
            .originalTargetUnitId = 2,
            .damageKind = BattleDamageKind::Skill,
        });
    CHECK(afterSettlement.commands.empty());
}

TEST_CASE("BattleEffectEventBridge exposes borrowed planning and exact cast rules in the bound cast",
          "[battle][effect][bridge][borrow][exact_runtime]")
{
    BattleRuntimeState runtime;
    runtime.gridTransform.tileWidth = 32.0;
    auto caster = runtimeUnit(1, 0, 100, 100);
    caster.core.star = 1;
    runtime.units.append(std::move(caster));
    runtime.units.append(runtimeUnit(2, 1, 100, 100));

    const auto casterMagic = binding(EffectSourceKind::Magic, 43, 1, 0);
    const auto sourceMagic = binding(EffectSourceKind::Magic, 18, 2, 1);
    auto borrow = borrowRule();
    auto& borrowMachine = std::get<StateMachineAction>(borrow.actions.front().value);
    std::get<BorrowEffectRulesAction>(borrowMachine)
        .filter.allowedActionCategories = {
            BorrowedRuleActionCategory::ResourceChange,
            BorrowedRuleActionCategory::Cast,
        };
    runtime.effectRules.append(casterMagic, borrow);

    ModifyCastAction cost;
    cost.mpCost = EffectNumber{ .flat = 37 };
    EffectRule planningCost;
    planningCost.id = EffectRuleId{ 10 };
    planningCost.event = EffectEvent::CastPlanned;
    planningCost.selector.kind = EffectSelectorKind::Self;
    planningCost.actions.push_back(action(std::move(cost)));
    runtime.effectRules.append(sourceMagic, planningCost);

    ModifyCastAction ranged;
    ranged.rangeMode = CastRangeMode::Ranged;
    EffectRule exactRange;
    exactRange.id = EffectRuleId{ 11 };
    exactRange.event = EffectEvent::CastPlanned;
    exactRange.selector.kind = EffectSelectorKind::Self;
    exactRange.actions.push_back(action(std::move(ranged)));
    runtime.effectRules.append(sourceMagic, exactRange);
    runtime.effectRules.append(
        sourceMagic,
        resourceRule(12, EffectEvent::UltimateCommitted, 49));

    const auto cast = ultimateCast(750);
    BattleEffectEventBridge bridge;
    const auto plannedEvent = bridge.makeEvent(
        runtime,
        { .frame = 5, .eventOrdinal = 150, .ownerUnitId = 1 },
        EffectEvent::CastPlanned,
        CastPlanEventData{ .provenance = cast, .preferredTargetUnitId = 2 });
    const auto planned = bridge.dispatch(runtime, plannedEvent);

    REQUIRE(runtime.effectRules.castScopedRuleCount(cast.castId) == 3);
    REQUIRE(planned.commands.size() == 2);
    const auto borrowCommand = std::ranges::find_if(planned.commands, [](const auto& command)
    {
        return std::holds_alternative<StateMachineEffectCommand>(command.value);
    });
    REQUIRE(borrowCommand != planned.commands.end());
    CHECK(std::get<StateMachineEffectCommand>(borrowCommand->value)
              .selectedSourceUnitIds == std::vector{ 2 });

    const auto planningCommand = std::ranges::find_if(planned.commands, [](const auto& command)
    {
        return std::holds_alternative<ModifyCastEffectCommand>(command.value);
    });
    REQUIRE(planningCommand != planned.commands.end());
    const auto& resolvedCost = std::get<ModifyCastEffectCommand>(planningCommand->value);
    REQUIRE(resolvedCost.mpCost);
    CHECK(*resolvedCost.mpCost == 37);
    CHECK(planningCommand->metadata.binding.sourceId == sourceMagic.sourceId);
    CHECK(planningCommand->metadata.binding.ownerUnitId == 1);
    REQUIRE(planningCommand->metadata.binding.runtimeInstanceId != 0);
    const auto borrowedInstanceId = planningCommand->metadata.binding.runtimeInstanceId;

    const auto exactMatches = BattleEffectSystem().queryExactRuntimeRules(
        runtime.effectRules,
        plannedEvent.context(),
        runtime.random);
    REQUIRE(exactMatches.size() == 1);
    REQUIRE(exactMatches.front().bound != nullptr);
    CHECK(exactMatches.front().bound->castScope == cast.castId);
    CHECK(exactMatches.front().bound->binding.sourceId == sourceMagic.sourceId);
    CHECK(exactMatches.front().bound->binding.ownerUnitId == 1);
    CHECK(exactMatches.front().bound->binding.runtimeInstanceId == borrowedInstanceId);
    REQUIRE(exactMatches.front().bound->rule.actions.size() == 1);
    const auto& exactAction = std::get<ModifyCastAction>(
        exactMatches.front().bound->rule.actions.front().value);
    REQUIRE(exactAction.rangeMode);
    CHECK(*exactAction.rangeMode == CastRangeMode::Ranged);

    auto unrelatedContext = plannedEvent.context();
    std::get<CastPlanEventData>(unrelatedContext.payload).provenance = ultimateCast(751);
    CHECK(BattleEffectSystem().queryExactRuntimeRules(
              runtime.effectRules,
              unrelatedContext,
              runtime.random).empty());

    const auto committed = bridge.dispatch(
        runtime,
        { .frame = 6, .eventOrdinal = 151, .ownerUnitId = 1 },
        EffectEvent::UltimateCommitted,
        CastCommitEventData{ .provenance = cast, .targetUnitId = 2 });
    REQUIRE(committed.commands.size() == 1);
    CHECK(resourceAmounts(committed) == std::vector{ 49 });
    CHECK(committed.commands.front().metadata.binding.sourceId == sourceMagic.sourceId);
    CHECK(committed.commands.front().metadata.binding.runtimeInstanceId == borrowedInstanceId);
    CHECK(runtime.effectRules.castScopedRuleCount(cast.castId) == 3);

    bridge.dispatch(
        runtime,
        { .frame = 7, .eventOrdinal = 152, .ownerUnitId = 1 },
        EffectEvent::CastSettled,
        CastAggregateEventData{ .provenance = cast, .originalTargetUnitId = 2 });
    CHECK(runtime.effectRules.castScopedRuleCount(cast.castId) == 0);
    CHECK(BattleEffectSystem().queryExactRuntimeRules(
              runtime.effectRules,
              plannedEvent.context(),
              runtime.random).empty());
}

TEST_CASE("BattleEffectEventBridge honours borrowed source count and cancellation cleanup",
          "[battle][effect][bridge][borrow]")
{
    for (const auto [star, expectedSources] : std::array{
             std::pair{ 1, std::size_t{ 1 } },
             std::pair{ 2, std::size_t{ 1 } },
             std::pair{ 3, std::size_t{ 2 } },
         })
    {
        CAPTURE(star);
        BattleRuntimeState runtime;
        runtime.gridTransform.tileWidth = 32.0;
        auto caster = runtimeUnit(1, 0, 100, 100);
        caster.core.star = star;
        runtime.units.append(std::move(caster));
        runtime.units.append(runtimeUnit(2, 1, 100, 100));
        runtime.units.append(runtimeUnit(3, 1, 100, 100));
        auto dead = runtimeUnit(4, 1, 0, 100);
        dead.core.alive = false;
        runtime.units.append(std::move(dead));

        runtime.effectRules.append(
            binding(EffectSourceKind::Magic, 43, 1, 0),
            borrowRule());
        runtime.effectRules.append(
            binding(EffectSourceKind::Magic, 71, 2, 1),
            resourceRule(10, EffectEvent::HitBeforeDamage, 21));
        runtime.effectRules.append(
            binding(EffectSourceKind::Magic, 72, 3, 1),
            resourceRule(10, EffectEvent::HitBeforeDamage, 22));
        runtime.effectRules.append(
            binding(EffectSourceKind::Magic, 73, 4, 1),
            resourceRule(10, EffectEvent::HitBeforeDamage, 23));

        const auto cast = ultimateCast(800 + static_cast<std::uint64_t>(star));
        BattleEffectEventBridge bridge;
        const auto planned = bridge.dispatch(
            runtime,
            { .frame = 1, .eventOrdinal = 200, .ownerUnitId = 1 },
            EffectEvent::CastPlanned,
            CastPlanEventData{ .provenance = cast, .preferredTargetUnitId = 2 });
        REQUIRE(planned.commands.size() == 1);
        const auto& borrow = std::get<StateMachineEffectCommand>(
            planned.commands.front().value);
        CHECK(borrow.selectedSourceUnitIds.size() == expectedSources);
        CHECK(std::ranges::find(borrow.selectedSourceUnitIds, 4)
            == borrow.selectedSourceUnitIds.end());
        CHECK(runtime.effectRules.castScopedRuleCount(cast.castId) == expectedSources);

        bridge.releaseCastScopedRules(runtime, cast.castId);
        CHECK(runtime.effectRules.castScopedRuleCount(cast.castId) == 0);
    }
}
