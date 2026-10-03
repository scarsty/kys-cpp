#include "BattleEffectEventBridge.h"

#include "BattleRuntimeUnits.h"

#include <cassert>
#include <algorithm>
#include <optional>
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
    assert(before.id == after.id);
    assert(transaction.defenderStatus.id == after.id);

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

std::pmr::vector<ActiveStatusBehaviorView> gatherActiveStatusBehaviors(
    BattleRuntimeState& runtime,
    EffectEvent event,
    StatusBehaviorDispatchFilter filter,
    std::pmr::memory_resource* memoryResource)
{
    std::pmr::vector<ActiveStatusBehaviorView> result(memoryResource);
    for (auto& holder : runtime.units.all())
    {
        if (event == EffectEvent::FrameAdvanced && !holder.alive()) continue;
        for (auto& contribution : holder.status.effects.statuses)
        {
            if (!contribution.behavior) continue;
            assert(contribution.origin);
            assert(contribution.producer);
            assert(contribution.behaviorRuntime.size()
                == contribution.behavior->rules.size());
            if (!std::ranges::any_of(contribution.behavior->rules, [&](const auto& rule)
                {
                    return BattleEffectSystem::statusBehaviorRuleMatchesEvent(rule, event, filter);
                })) continue;
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

BattleEffectDispatchPrediction::BattleEffectDispatchPrediction(const BattleRuntimeState& source, bool needsReduction)
    : source_(source)
{
    // 可能修改狀態存活時在事件開始就複製，保留規則計時器供巢狀 dispatch 使用。
    if (needsReduction)
    {
        // 直接初始化以保證省略暫存值的移動；MSVC 的 map 移動仍會分配空節點。
        runtime_.reset(new BattleRuntimeState(
            BattleEffectCommandSystem::copyDispatchState(source, &memory_)));
    }
    hooks_.contributionQuantity = [&](
        int holderUnitId,
        std::uint64_t appliedSequence,
        BattleStatusKind kind) -> std::optional<int>
    {
        return BattleEffectCommandSystem::contributionQuantity(
            (runtime_ ? *runtime_ : source_).units.require(holderUnitId).status.effects,
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
    // 已證明本次事件不能修改狀態存活時，quantity 直接讀取未被 reducer 改動的來源。
    if (commands.empty() || !runtime_) return;
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
    assert(owner);

    data_.header = {
        .frame = header.frame,
        .eventOrdinal = header.eventOrdinal,
        .binding = pendingRuleBinding(*owner),
        .owner = owner,
        .battle = battle_.readView(),
        .formulaInputs = header.formulaInputs,
        .executionFrame = header.executionFrame,
    };

    if (const auto* cast = effectEventCastProvenance(data_.payload))
    {
        // 規劃階段尚未分配施放 ID，結算後也不再持有可保留的工作。
        data_.header.retainCastUntilDamageDescendants =
            runtime.castLifecycle.containsCast(cast->castId);
    }

    assert(BattleEffectSystem::eventPayloadMatches(context()));
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
    const auto filter = event.event() == EffectEvent::HitBeforeDamage
        ? StatusBehaviorDispatchFilter::ExcludeAttackInterceptors
        : StatusBehaviorDispatchFilter::All;
    std::array<std::byte, 4096> buffer;
    std::pmr::monotonic_buffer_resource memory(buffer.data(), buffer.size());
    auto activeStatusBehaviors = gatherActiveStatusBehaviors(runtime, event.event(), filter, &memory);
    std::optional<BattleEffectDispatchPrediction> liveness;
    if (!activeStatusBehaviors.empty())
    {
        liveness.emplace(runtime, system.rulesNeedStatusPrediction(
            runtime.effectRules, context, activeStatusBehaviors, runtime.random));
    }
    auto result = system.dispatchMerged(
        runtime.effectRules,
        context,
        runtime.random,
        activeStatusBehaviors,
        filter,
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
    std::array<std::byte, 4096> buffer;
    std::pmr::monotonic_buffer_resource memory(buffer.data(), buffer.size());
    auto behaviors = gatherActiveStatusBehaviors(
        runtime, event.event(), StatusBehaviorDispatchFilter::All, &memory);
    const auto context = event.context();
    std::optional<BattleEffectDispatchPrediction> liveness;
    if (!behaviors.empty())
    {
        liveness.emplace(runtime, BattleEffectSystem().rulesNeedStatusPrediction(
            runtime.effectRules, context, behaviors, runtime.random, true));
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
    std::array<std::byte, 4096> buffer;
    std::pmr::monotonic_buffer_resource memory(buffer.data(), buffer.size());
    auto behaviors = gatherActiveStatusBehaviors(runtime, event.event(), filter, &memory);
    const auto context = event.context();
    std::optional<BattleEffectDispatchPrediction> liveness;
    if (!behaviors.empty())
    {
        const BattleEffectRuleStore statusOnly;
        liveness.emplace(runtime, BattleEffectSystem().rulesNeedStatusPrediction(
            statusOnly, context, behaviors, runtime.random));
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
    assert(event.provenance.valid());

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
    if (const auto* attack = std::get_if<EffectAttackDamageOrigin>(&origin))
    {
        assert(attack->provenance.valid());
        assert(input.attackerBefore);
    }
    assert(input.attackerBefore
        ? input.attackerBefore->id == transaction.attacker.id
        : transaction.attacker.id == OptionalDamageAttackerUnitId);

    DamageResultEventData payload;
    payload.transactionId = input.transactionId;
    payload.origin = std::move(origin);
    payload.attackerBefore = std::move(input.attackerBefore);
    payload.defenderBefore = input.defenderBefore;
    payload.defenderAfter = defenderAfterSnapshot(input.defenderBefore, transaction);
    assert(input.rawDamage >= 0);
    payload.rawDamage = input.rawDamage;
    assert(input.resolvedDamage >= 0);
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
