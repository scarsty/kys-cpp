#include "BattleEffectEventBridge.h"

#include "BattleRuntimeUnits.h"

#include <cassert>
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
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

void sortDispatchCommands(BattleEffectDispatchResult& result)
{
    std::ranges::stable_sort(result.commands, [](const auto& lhs, const auto& rhs)
    {
        return effectExecutionOrderKey(lhs.metadata)
            < effectExecutionOrderKey(rhs.metadata);
    });
    for (std::uint64_t ordinal = 0; ordinal < result.commands.size(); ++ordinal)
        result.commands[ordinal].metadata.commandOrdinal = ordinal;
}

std::vector<ActiveStatusBehaviorView> gatherActiveStatusBehaviors(
    BattleRuntimeState& runtime)
{
    std::vector<ActiveStatusBehaviorView> result;
    for (auto& holder : runtime.units.all())
    {
        for (auto& contribution : holder.status.effects.statuses)
        {
            if (!contribution.behavior) continue;
            assert(contribution.origin);
            assert(contribution.producer);
            assert(contribution.behaviorRuntime.size()
                == contribution.behavior->rules.size());
            result.push_back({
                .binding = contribution.origin->binding,
                .producerRuleId = contribution.origin->ruleId,
                .producerRuleOrder = contribution.origin->ruleOrder,
                .producerActionOrder = contribution.producer->actionOrder,
                .holderUnitId = holder.id(),
                .sourceUnitId = contribution.sourceUnitId,
                .kind = contribution.kind,
                .quantity = contribution.stacks,
                .appliedSequence = contribution.appliedSequence,
                .behavior = contribution.behavior,
                .runtime = &contribution.behaviorRuntime,
            });
        }
    }
    return result;
}

bool livenessReductionStartsDamageContinuation(
    const BattleEffectReductionValue& value)
{
    return std::holds_alternative<BattleEffectDamageRequestOutput>(value);
}


}  // namespace

BattleEffectDispatchPrediction::BattleEffectDispatchPrediction(const BattleRuntimeState& source)
    : runtime_(std::make_unique<BattleRuntimeState>(
        BattleEffectCommandSystem::copyDispatchState(source)))
{
    hooks_.contributionQuantity = [&](
        int holderUnitId,
        std::uint64_t appliedSequence,
        BattleStatusKind kind) -> std::optional<int>
    {
        return BattleEffectCommandSystem::contributionQuantity(
            runtime_->units.require(holderUnitId).status.effects,
            appliedSequence, kind);
    };
    hooks_.reduceRuleCommands = [&](std::span<const EffectCommand> commands)
    {
        reduceRuleCommands(commands);
    };
}

BattleEffectDispatchPrediction::~BattleEffectDispatchPrediction() = default;

void BattleEffectDispatchPrediction::reduceRuleCommands(std::span<const EffectCommand> commands)
{
    std::size_t actionBegin{};
    while (actionBegin < commands.size())
    {
        const auto actionOrder = commands[actionBegin].metadata.actionOrder;
        std::size_t actionEnd = actionBegin + 1;
        while (actionEnd < commands.size()
               && commands[actionEnd].metadata.actionOrder == actionOrder)
        {
            ++actionEnd;
        }

        bool startsDamageContinuation = false;
        for (std::size_t index = actionBegin; index < actionEnd; ++index)
        {
            auto reduced = BattleEffectCommandSystem().reduce(
                *runtime_,
                commands[index]);
            assert(reduced.entries.size() == 1);
            startsDamageContinuation = startsDamageContinuation
                || livenessReductionStartsDamageContinuation(
                    reduced.entries.front().value);
        }
        if (startsDamageContinuation) return;
        actionBegin = actionEnd;
    }
}

BattleEffectOwnedEvent::BattleEffectOwnedEvent(
    const BattleRuntimeState& runtime,
    BattleEffectEventHeaderInput header,
    EffectEvent event,
    EffectEventPayload payload)
    : battle_(runtime)
    , data_{ .event = event, .payload = std::move(payload) }
{
    const auto* owner = battle_.readView().findUnit(header.ownerUnitId);
    if (!owner)
    {
        throw std::invalid_argument("效果事件指定了不存在的 owner 單位");
    }

    data_.header = {
        .frame = header.frame,
        .eventOrdinal = header.eventOrdinal,
        .binding = pendingRuleBinding(*owner),
        .owner = owner,
        .battle = battle_.readView(),
        .formulaInputs = header.formulaInputs,
        .executionFrame = header.executionFrame,
    };

    const auto* heal = std::get_if<HealRequestEventData>(&data_.payload);
    if (const auto* result = std::get_if<HealResultEventData>(&data_.payload)) heal = &result->request;
    if (heal && heal->castId)
        data_.header.healCast = runtime.castLifecycle.runtime(*heal->castId).provenance;

    if (!BattleEffectSystem::eventPayloadMatches(context()))
    {
        throw std::invalid_argument("EffectEvent 與 typed payload 不相符");
    }
}

EffectEvent BattleEffectOwnedEvent::event() const
{
    return data_.event;
}

const EffectEventPayload& BattleEffectOwnedEvent::payload() const
{
    return data_.payload;
}

EffectEventContext BattleEffectOwnedEvent::context() const &
{
    return EffectEventContext(data_);
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
    auto activeStatusBehaviors = gatherActiveStatusBehaviors(runtime);
    std::optional<BattleEffectDispatchPrediction> liveness;
    if (!activeStatusBehaviors.empty())
    {
        liveness.emplace(runtime);
    }
    auto result = system.dispatchMerged(
        runtime.effectRules,
        context,
        runtime.random,
        activeStatusBehaviors,
        event.event() == EffectEvent::HitBeforeDamage
            ? StatusBehaviorDispatchFilter::ExcludeAttackInterceptors
            : StatusBehaviorDispatchFilter::All,
        false,
        liveness ? &liveness->hooks() : nullptr);

    if (event.event() == EffectEvent::CastSettled)
    {
        const auto* cast = effectCastProvenance(context);
        assert(cast);
        releaseCastScopedRules(runtime, cast->castId);
    }
    return result;
}

BattleEffectDispatchResult BattleEffectEventBridge::dispatchFrameAdvanced(
    BattleRuntimeState& runtime,
    const BattleEffectOwnedEvent& event) const
{
    assert(event.event() == EffectEvent::FrameAdvanced);
    auto behaviors = gatherActiveStatusBehaviors(runtime);
    std::erase_if(behaviors, [&](const ActiveStatusBehaviorView& behavior)
    {
        return !runtime.units.requireCore(behavior.holderUnitId).alive;
    });
    const auto context = event.context();
    std::optional<BattleEffectDispatchPrediction> liveness;
    if (!behaviors.empty())
    {
        liveness.emplace(runtime);
    }
    return BattleEffectSystem().dispatchMerged(
        runtime.effectRules,
        context,
        runtime.random,
        behaviors,
        StatusBehaviorDispatchFilter::All,
        true,
        liveness ? &liveness->hooks() : nullptr);
}


BattleEffectDispatchResult BattleEffectEventBridge::dispatchActiveStatusBehaviors(
    BattleRuntimeState& runtime,
    const BattleEffectOwnedEvent& event,
    StatusBehaviorDispatchFilter filter) const
{
    auto behaviors = gatherActiveStatusBehaviors(runtime);
    const auto context = event.context();
    std::optional<BattleEffectDispatchPrediction> liveness;
    if (!behaviors.empty())
    {
        liveness.emplace(runtime);
    }
    auto result = BattleEffectSystem().dispatchStatusBehaviors(
        context,
        runtime.random,
        behaviors,
        filter,
        liveness ? &liveness->hooks() : nullptr);
    sortDispatchCommands(result);
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
