#include "BattleEffectEventBridge.h"

#include "BattleRuntimeUnits.h"

#include <cassert>
#include <algorithm>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace KysChess::Battle
{

namespace
{

template<class... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};

template<class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

EffectSourceBinding pendingRuleBinding(const EffectUnitSnapshot& owner)
{
    // BattleEffectSystem replaces this placeholder with each matching bound
    // rule before evaluating selectors, conditions or actions.
    return {
        .kind = EffectSourceKind::Combo,
        .sourceId = -1,
        .ownerUnitId = owner.id,
        .sourceTeam = owner.team,
    };
}

EffectEvent effectEvent(BattleCastLifecycleEventType type)
{
    switch (type)
    {
    case BattleCastLifecycleEventType::CastContinuation:
        return EffectEvent::CastContinuation;
    case BattleCastLifecycleEventType::CastSettled:
        return EffectEvent::CastSettled;
    }
    assert(false);
    return EffectEvent::CastSettled;
}

EffectUnitSnapshot defenderAfterSnapshot(
    const EffectUnitSnapshot& before,
    const BattleDamageTransactionResult& transaction)
{
    const auto& after = transaction.defender;
    if (before.id != after.id || transaction.defenderStatus.id != after.id)
    {
        throw std::invalid_argument("傷害結果與防守方事件快照的單位不一致");
    }

    auto result = before;
    result.alive = after.alive;
    result.hp = after.vitals.hp;
    result.maxHp = after.vitals.maxHp;
    result.mp = after.vitals.mp;
    result.maxMp = after.vitals.maxMp;
    result.shield = after.shield;
    result.attack = after.attack;
    refreshEffectStatusSnapshot(result, transaction.defenderStatus.effects);
    return result;
}

bool damageWasBlocked(const BattleDamageTransactionResult& transaction)
{
    return transaction.blockedByInvincible
        || transaction.blockedByDualWield
        || transaction.blockedByDamageLayer;
}

void validateDamageSnapshots(
    const BattleDamageTransactionResult& transaction,
    const EffectDamageOrigin& origin,
    const BattleDamageResolvedEffectInput& input)
{
    const auto* attack = std::get_if<EffectAttackDamageOrigin>(&origin);
    if (attack && !attack->provenance.valid())
    {
        throw std::invalid_argument("攻擊傷害事件缺少完整 provenance");
    }
    if (attack && !input.attackerBefore)
    {
        throw std::invalid_argument("攻擊傷害事件缺少攻擊方事件快照");
    }
    if (input.attackerBefore
        && input.attackerBefore->id != transaction.attacker.id)
    {
        throw std::invalid_argument("傷害結果與攻擊方事件快照的單位不一致");
    }
    if (!input.attackerBefore
        && transaction.attacker.id != OptionalDamageAttackerUnitId)
    {
        throw std::invalid_argument("傷害結果有攻擊方卻缺少事件快照");
    }
    if (input.defenderBefore.id != transaction.defender.id)
    {
        throw std::invalid_argument("傷害結果與防守方事件快照的單位不一致");
    }
    if (input.rawDamage < 0 || input.resolvedDamage < 0)
    {
        throw std::invalid_argument("傷害事件的原始與結算傷害不可為負數");
    }
}

const BattleCastProvenance* eventCastProvenance(const EffectEventPayload& payload)
{
    const auto fromOrigin = [](const EffectDamageOrigin& origin) -> const BattleCastProvenance*
    {
        if (const auto* attack = std::get_if<EffectAttackDamageOrigin>(&origin))
        {
            return &attack->provenance.cast;
        }
        return nullptr;
    };
    return std::visit(Overloaded{
        [](const CastPlanEventData& data) { return &data.provenance; },
        [](const CastCommitEventData& data) { return &data.provenance; },
        [](const AttackEventData& data) { return &data.provenance.cast; },
        [](const HitEventData& data) { return &data.provenance.cast; },
        [](const CastAggregateEventData& data) { return &data.provenance; },
        [&](const DamageResultEventData& data) { return fromOrigin(data.origin); },
        [&](const ShieldBreakEventData& data) { return fromOrigin(data.cause); },
        [&](const DeathEventData& data) { return fromOrigin(data.cause); },
        [](const auto&) -> const BattleCastProvenance* { return nullptr; },
    }, payload);
}

void appendDispatchResult(BattleEffectDispatchResult& destination,
                          BattleEffectDispatchResult source)
{
    std::uint64_t nextCommandOrdinal{};
    for (const auto& command : destination.commands)
    {
        assert(command.metadata.commandOrdinal
            < std::numeric_limits<std::uint64_t>::max());
        nextCommandOrdinal = std::max(
            nextCommandOrdinal,
            command.metadata.commandOrdinal + 1);
    }
    for (auto& command : source.commands)
    {
        assert(command.metadata.commandOrdinal
            <= std::numeric_limits<std::uint64_t>::max() - nextCommandOrdinal);
        command.metadata.commandOrdinal += nextCommandOrdinal;
    }
    destination.activations.insert(
        destination.activations.end(),
        std::make_move_iterator(source.activations.begin()),
        std::make_move_iterator(source.activations.end()));
    destination.commands.insert(
        destination.commands.end(),
        std::make_move_iterator(source.commands.begin()),
        std::make_move_iterator(source.commands.end()));
}

}  // namespace

BattleEffectOwnedEvent::BattleEffectOwnedEvent(
    const BattleRuntimeState& runtime,
    BattleEffectEventHeaderInput header,
    EffectEvent event,
    EffectEventPayload payload)
    : battle_(runtime)
    , header_(std::move(header))
    , event_(event)
    , payload_(std::move(payload))
{
    const auto* owner = battle_.readView().findUnit(header_.ownerUnitId);
    if (!owner)
    {
        throw std::invalid_argument("效果事件指定了不存在的 owner 單位");
    }

    if (!BattleEffectSystem::eventPayloadMatches(context()))
    {
        throw std::invalid_argument("EffectEvent 與 typed payload 不相符");
    }
}

EffectEvent BattleEffectOwnedEvent::event() const
{
    return event_;
}

const EffectEventPayload& BattleEffectOwnedEvent::payload() const
{
    return payload_;
}

EffectEventContext BattleEffectOwnedEvent::context() const &
{
    const auto battle = battle_.readView();
    const auto* owner = battle.findUnit(header_.ownerUnitId);
    assert(owner);
    return {
        .event = event_,
        .header = {
            .frame = header_.frame,
            .eventOrdinal = header_.eventOrdinal,
            .binding = pendingRuleBinding(*owner),
            .owner = owner,
            .battle = battle,
            .formulaInputs = header_.formulaInputs,
        },
        .payload = payload_,
    };
}

BattleEffectOwnedEvent BattleEffectEventBridge::makeEvent(
    const BattleRuntimeState& runtime,
    BattleEffectEventHeaderInput header,
    EffectEvent event,
    EffectEventPayload payload) const
{
    return BattleEffectOwnedEvent(runtime, std::move(header), event, std::move(payload));
}

BattleEffectDispatchResult BattleEffectEventBridge::dispatch(
    BattleRuntimeState& runtime,
    const BattleEffectOwnedEvent& event) const
{
    const auto context = event.context();
    BattleEffectSystem system;
    auto result = system.dispatch(runtime.effectRules, context, runtime.random);

    std::vector<std::size_t> addedRuleIndices;
    const auto* cast = eventCastProvenance(event.payload());
    for (const auto& command : result.commands)
    {
        const auto* stateMachine = std::get_if<StateMachineEffectCommand>(&command.value);
        if (!stateMachine)
        {
            continue;
        }
        const auto* borrow = std::get_if<BorrowEffectRulesAction>(&stateMachine->action);
        if (!borrow)
        {
            continue;
        }
        if (!cast)
        {
            throw std::logic_error("借用效果規則需要 cast provenance");
        }
        assert(borrow->propagation == CastPropagationPolicy::BorrowedUltimateRules);
        auto added = runtime.effectRules.bindBorrowedUltimateRules(
            cast->castId,
            context.header.owner->id,
            context.header.owner->team,
            stateMachine->selectedSourceUnitIds,
            borrow->filter,
            borrow->propagation);
        addedRuleIndices.insert(
            addedRuleIndices.end(),
            added.begin(),
            added.end());
    }
    if (!addedRuleIndices.empty())
    {
        appendDispatchResult(
            result,
            system.dispatchRuleIndices(
                runtime.effectRules,
                context,
                runtime.random,
                addedRuleIndices));
    }

    if (event.event() == EffectEvent::CastSettled)
    {
        assert(cast);
        releaseCastScopedRules(runtime, cast->castId);
    }
    return result;
}

BattleEffectDispatchResult BattleEffectEventBridge::dispatch(
    BattleRuntimeState& runtime,
    BattleEffectEventHeaderInput header,
    EffectEvent event,
    EffectEventPayload payload) const
{
    const auto owned = makeEvent(
        runtime,
        std::move(header),
        event,
        std::move(payload));
    return dispatch(runtime, owned);
}

BattleEffectOwnedEvent BattleEffectEventBridge::makeCastLifecycleEvent(
    const BattleRuntimeState& runtime,
    BattleEffectEventHeaderInput header,
    const BattleCastLifecycleEvent& event,
    BattleCastLifecycleEffectInput input) const
{
    if (!event.provenance.valid())
    {
        throw std::invalid_argument("施放生命週期事件缺少完整 provenance");
    }

    CastAggregateEventData payload;
    payload.provenance = event.provenance;
    payload.originalTargetUnitId = input.originalTargetUnitId;
    payload.aggregate = event.aggregate;
    payload.resourcesBeforeCast = std::move(input.resourcesBeforeCast);
    return makeEvent(
        runtime,
        std::move(header),
        effectEvent(event.type),
        std::move(payload));
}

BattleEffectDispatchResult BattleEffectEventBridge::dispatchCastLifecycleEvent(
    BattleRuntimeState& runtime,
    BattleEffectEventHeaderInput header,
    const BattleCastLifecycleEvent& event,
    BattleCastLifecycleEffectInput input) const
{
    const auto owned = makeCastLifecycleEvent(
        runtime,
        std::move(header),
        event,
        std::move(input));
    return dispatch(runtime, owned);
}

BattleEffectOwnedEvent BattleEffectEventBridge::makeDamageResolvedEvent(
    const BattleRuntimeState& runtime,
    BattleEffectEventHeaderInput header,
    const BattleDamageTransactionResult& transaction,
    EffectDamageOrigin origin,
    BattleDamageResolvedEffectInput input) const
{
    validateDamageSnapshots(transaction, origin, input);

    DamageResultEventData payload;
    payload.transactionId = input.transactionId;
    payload.origin = std::move(origin);
    payload.attackerBefore = std::move(input.attackerBefore);
    payload.defenderBefore = input.defenderBefore;
    payload.defenderAfter = defenderAfterSnapshot(input.defenderBefore, transaction);
    payload.rawDamage = input.rawDamage;
    payload.resolvedDamage = input.resolvedDamage;
    payload.shieldAbsorbed = transaction.shieldAbsorbed;
    payload.finalHpDamage = transaction.finalHpDamage;
    payload.finalMpDamage = transaction.finalMpDamage;
    payload.damageKind = transaction.damageKind;
    payload.blocked = damageWasBlocked(transaction);
    payload.executed = transaction.executed;
    payload.killed = transaction.killed;
    return makeEvent(
        runtime,
        std::move(header),
        EffectEvent::DamageResolved,
        std::move(payload));
}

BattleEffectDispatchResult BattleEffectEventBridge::dispatchDamageResolvedEvent(
    BattleRuntimeState& runtime,
    BattleEffectEventHeaderInput header,
    const BattleDamageTransactionResult& transaction,
    EffectDamageOrigin origin,
    BattleDamageResolvedEffectInput input) const
{
    const auto owned = makeDamageResolvedEvent(
        runtime,
        std::move(header),
        transaction,
        std::move(origin),
        std::move(input));
    return dispatch(runtime, owned);
}

void BattleEffectEventBridge::releaseCastScopedRules(
    BattleRuntimeState& runtime,
    BattleCastId castId) const
{
    runtime.effectRules.removeCastScopedRules(castId);
}

}  // namespace KysChess::Battle
