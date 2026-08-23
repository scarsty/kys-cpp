#include "BattleHealSystem.h"

#include "BattleEffectEventBridge.h"
#include "BattleRuntimeEffects.h"
#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <ranges>
#include <string_view>
#include <utility>

namespace KysChess::Battle
{

namespace
{

BattleHealUnitSnapshot healSnapshot(const BattleRuntimeUnit& unit)
{
    return {
        .id = unit.id,
        .alive = unit.alive,
        .hp = unit.vitals.hp,
        .maxHp = unit.vitals.maxHp,
    };
}

EffectUnitSnapshot effectSnapshot(
    const BattleRuntimeState& state,
    const BattleHealUnitSnapshot& heal)
{
    auto result = makeEffectUnitSnapshot(state, state.units.require(heal.id));
    result.alive = heal.alive;
    result.hp = heal.hp;
    result.maxHp = heal.maxHp;
    return result;
}

int roundedPercent(int value, int percent, BattleHealRounding rounding)
{
    assert(value >= 0);
    assert(percent >= 0);

    const long long numerator = static_cast<long long>(value) * percent;
    assert(numerator / 100 <= std::numeric_limits<int>::max());
    switch (rounding)
    {
    case BattleHealRounding::TowardZero:
        return static_cast<int>(numerator / 100);
    }
    assert(false);
    return 0;
}

int healBaseValue(const BattleHealAmount& amount, const BattleHealUnitSnapshot& target)
{
    switch (amount.base)
    {
    case BattleHealBase::TargetMaxHp:
        return target.maxHp;
    case BattleHealBase::FinalHpDamage:
    case BattleHealBase::FixedAmount:
        return amount.baseValue;
    }
    assert(false);
    return 0;
}

void validateSnapshot(const BattleHealUnitSnapshot& unit)
{
    assert(unit.id >= 0);
    assert(unit.maxHp >= 0);
    assert(unit.hp >= 0);
    assert(unit.hp <= unit.maxHp);
}

bool matchesHealKindLabel(BattleHealKind kind, std::string_view label)
{
    switch (kind)
    {
    case BattleHealKind::Direct: return label == "直接" || label == "Direct";
    case BattleHealKind::Team: return label == "隊伍" || label == "Team";
    case BattleHealKind::Aura: return label == "光環" || label == "Aura";
    case BattleHealKind::OnHit: return label == "命中" || label == "OnHit";
    case BattleHealKind::KillReward: return label == "擊殺" || label == "KillReward";
    case BattleHealKind::DeathMedical: return label == "死亡醫療" || label == "DeathMedical";
    case BattleHealKind::Rescue: return label == "救援" || label == "Rescue";
    case BattleHealKind::Regeneration: return label == "再生" || label == "Regeneration";
    case BattleHealKind::Lifesteal: return label == "吸血" || label == "Lifesteal";
    }
    assert(false);
    return false;
}

BattleEffectEventHeaderInput nextHealEffectHeader(BattleRuntimeState& state, int ownerUnitId)
{
    return {
        .frame = state.movement.frame,
        .eventOrdinal = state.effectIntegration.nextEventOrdinal++,
        .ownerUnitId = ownerUnitId,
    };
}

void appendAttemptedEffectModifiers(
    BattleRuntimeState& state,
    const BattleHealRequest& request,
    const EffectUnitSnapshot& sourceBefore,
    const EffectUnitSnapshot& targetBefore,
    int calculatedAmount,
    BattleHealModifierState& modifiers)
{
    if (state.effectRules.rules().empty())
    {
        return;
    }

    HealRequestEventData payload{
        .transactionId = request.id,
        .kind = request.kind,
        .sourceBefore = sourceBefore,
        .targetBefore = targetBefore,
        .calculatedAmount = calculatedAmount,
        .castId = request.castId
            ? std::optional<BattleCastId>{ BattleCastId{ *request.castId } }
            : std::nullopt,
    };
    auto dispatched = BattleEffectEventBridge().dispatch(
        state,
        nextHealEffectHeader(state, request.targetUnitId),
        EffectEvent::HealAttempted,
        std::move(payload));
    for (const auto& command : dispatched.commands)
    {
        const auto* heal = std::get_if<ModifyHealTransactionEffectCommand>(&command.value);
        assert(heal);
        if (command.metadata.targetUnitId != request.targetUnitId
            || !std::ranges::any_of(heal->action.kinds, [&](const std::string& label)
            {
                return matchesHealKindLabel(request.kind, label);
            }))
        {
            continue;
        }
        switch (heal->action.operation)
        {
        case HealModifierOperation::Block:
            modifiers.blocked = true;
            break;
        case HealModifierOperation::MultiplyReceived:
            modifiers.receivedHealPcts.push_back(heal->action.percent);
            break;
        }
    }
}

void queueAppliedEffectCommands(
    BattleRuntimeState& state,
    const BattleHealResult& result,
    const EffectUnitSnapshot& sourceBefore,
    const EffectUnitSnapshot& targetBefore,
    EffectUnitSnapshot targetAfter)
{
    if (result.appliedAmount <= 0 || state.effectRules.rules().empty())
    {
        return;
    }

    HealResultEventData payload;
    payload.request = {
        .transactionId = result.request.id,
        .kind = result.request.kind,
        .sourceBefore = sourceBefore,
        .targetBefore = targetBefore,
        .calculatedAmount = result.calculatedAmount,
        .castId = result.request.castId
            ? std::optional<BattleCastId>{ BattleCastId{ *result.request.castId } }
            : std::nullopt,
    };
    payload.targetAfter = std::move(targetAfter);
    payload.modifiedAmount = result.modifiedAmount;
    payload.appliedAmount = result.appliedAmount;
    auto dispatched = BattleEffectEventBridge().dispatch(
        state,
        nextHealEffectHeader(state, result.request.targetUnitId),
        EffectEvent::HealApplied,
        std::move(payload));
    if (dispatched.commands.empty())
    {
        return;
    }

    const auto& target = state.units.requireCore(result.request.targetUnitId);
    BattleEffectCommandContext context{
        .frame = state.movement.frame,
        .effectPosition = target.motion.position,
        .healKind = result.request.kind,
        .healSourcePolicy = result.request.sourcePolicy,
        .areaTargetTeamDomain = target.team,
    };
    if (result.request.castId)
    {
        const BattleCastId castId{ *result.request.castId };
        assert(castId.valid());
        assert(state.castLifecycle.containsCast(castId));
        context.cast = state.castLifecycle.runtime(castId).provenance;
    }
    state.effectIntegration.queuedCommandBatches.push_back({
        .commands = std::move(dispatched.commands),
        .context = std::move(context),
    });
}

BattleHealRequest ownRuntimeRequest(
    BattleRuntimeState& state,
    BattleHealRequest request)
{
    if (request.id.value == 0)
    {
        request.id.value = state.heals.nextTransactionId++;
    }
    else
    {
        state.heals.nextTransactionId = std::max(
            state.heals.nextTransactionId,
            request.id.value + 1);
    }
    const auto [_, inserted] = state.heals.committedTransactions.insert(request.id);
    assert(inserted);
    return request;
}

void appendRuntimeHealEvents(
    BattleRuntimeState& state,
    const BattleHealResult& result)
{
    const auto& request = result.request;
    switch (result.outcome)
    {
    case BattleHealOutcome::Applied:
        state.heals.events.push_back({
            BattleHealEventType::Attempted,
            request,
            result.calculatedAmount,
            0,
        });
        state.heals.events.push_back({
            BattleHealEventType::Applied,
            request,
            result.calculatedAmount,
            result.appliedAmount,
        });
        break;
    case BattleHealOutcome::Blocked:
        state.heals.events.push_back({
            BattleHealEventType::Attempted,
            request,
            result.calculatedAmount,
            0,
        });
        state.heals.events.push_back({
            BattleHealEventType::Blocked,
            request,
            result.calculatedAmount,
            0,
        });
        break;
    case BattleHealOutcome::ZeroAfterModifier:
        state.heals.events.push_back({
            BattleHealEventType::Attempted,
            request,
            result.calculatedAmount,
            0,
        });
        break;
    case BattleHealOutcome::AlreadyFull:
    case BattleHealOutcome::IneligibleSource:
    case BattleHealOutcome::IneligibleTarget:
        break;
    }
}

}  // namespace

BattleHealAmount targetMaxHpHealAmount(int flat, int percent, int minimum)
{
    assert(flat >= 0);
    assert(percent >= 0);
    assert(minimum >= 0);

    BattleHealAmount amount;
    amount.base = BattleHealBase::TargetMaxHp;
    amount.flat = flat;
    amount.percent = percent;
    amount.rounding = BattleHealRounding::TowardZero;
    amount.minimum = minimum;
    return amount;
}

BattleHealAmount fixedHealAmount(int amount)
{
    assert(amount >= 0);

    BattleHealAmount result;
    result.base = BattleHealBase::FixedAmount;
    result.baseValue = amount;
    result.percent = 100;
    result.rounding = BattleHealRounding::TowardZero;
    return result;
}

BattleHealResult resolveHeal(
    const BattleHealRequest& request,
    const BattleHealUnitSnapshot& source,
    const BattleHealUnitSnapshot& target,
    const BattleHealModifierState& modifiers)
{
    validateSnapshot(source);
    validateSnapshot(target);
    assert(request.sourceUnitId == source.id);
    assert(request.targetUnitId == target.id);
    assert(request.amount.baseValue >= 0);
    assert(request.amount.flat >= 0);
    assert(request.amount.percent >= 0);
    assert(request.amount.minimum >= 0);
    for (int percent : modifiers.receivedHealPcts)
    {
        assert(percent >= 0);
    }

    BattleHealResult result;
    result.request = request;
    result.hpBefore = target.hp;
    result.hpAfter = target.hp;

    if (!source.alive && request.sourcePolicy == BattleHealSourcePolicy::RequireAlive)
    {
        result.outcome = BattleHealOutcome::IneligibleSource;
        return result;
    }
    if (!target.alive)
    {
        result.outcome = BattleHealOutcome::IneligibleTarget;
        return result;
    }

    const int baseValue = healBaseValue(request.amount, target);
    const int scaled = roundedPercent(baseValue, request.amount.percent, request.amount.rounding);
    assert(static_cast<long long>(scaled) + request.amount.flat <= std::numeric_limits<int>::max());
    result.calculatedAmount = scaled + request.amount.flat;
    const bool positiveBeforeRounding = request.amount.flat > 0
        || (baseValue > 0 && request.amount.percent > 0);
    if (positiveBeforeRounding)
    {
        result.calculatedAmount = std::max(result.calculatedAmount, request.amount.minimum);
    }

    if (target.hp >= target.maxHp)
    {
        result.outcome = BattleHealOutcome::AlreadyFull;
        return result;
    }
    if (modifiers.blocked)
    {
        result.outcome = BattleHealOutcome::Blocked;
        return result;
    }

    result.modifiedAmount = result.calculatedAmount;
    for (int percent : modifiers.receivedHealPcts)
    {
        result.modifiedAmount = roundedPercent(
            result.modifiedAmount,
            percent,
            BattleHealRounding::TowardZero);
    }
    if (result.modifiedAmount <= 0)
    {
        result.outcome = BattleHealOutcome::ZeroAfterModifier;
        return result;
    }

    result.appliedAmount = std::min(result.modifiedAmount, target.maxHp - target.hp);
    assert(result.appliedAmount > 0);
    result.hpAfter = target.hp + result.appliedAmount;
    result.outcome = BattleHealOutcome::Applied;
    return result;
}

BattleHealResult BattleHealSystem::commit(
    BattleRuntimeUnits& units,
    const BattleHealRequest& request,
    const BattleHealModifierState& modifiers) const
{
    const auto source = healSnapshot(units.requireCore(request.sourceUnitId));
    auto& targetRecord = units.require(request.targetUnitId);
    auto& targetUnit = targetRecord.core;
    const auto target = healSnapshot(targetUnit);

    auto effectiveModifiers = modifiers;
    const auto statusModifiers = BattleStatusSystem({}).snapshot(
        targetRecord.statusDamageState());
    effectiveModifiers.blocked = effectiveModifiers.blocked || statusModifiers.healingBlocked;
    effectiveModifiers.receivedHealPcts.insert(
        effectiveModifiers.receivedHealPcts.end(),
        statusModifiers.receivedHealMultipliersPct.begin(),
        statusModifiers.receivedHealMultipliersPct.end());

    auto result = resolveHeal(request, source, target, effectiveModifiers);
    targetUnit.vitals.hp = result.hpAfter;
    return result;
}

BattleHealResult BattleHealSystem::commit(
    BattleRuntimeState& state,
    const BattleHealRequest& request,
    const BattleHealModifierState& modifiers) const
{
    const auto ownedRequest = ownRuntimeRequest(state, request);

    const auto sourceBefore = makeEffectUnitSnapshot(
        state,
        state.units.require(ownedRequest.sourceUnitId));
    const auto targetBefore = makeEffectUnitSnapshot(
        state,
        state.units.require(ownedRequest.targetUnitId));
    const auto unmodified = resolveHeal(
        ownedRequest,
        healSnapshot(state.units.requireCore(ownedRequest.sourceUnitId)),
        healSnapshot(state.units.requireCore(ownedRequest.targetUnitId)));

    auto effectiveModifiers = modifiers;
    if (unmodified.outcome != BattleHealOutcome::AlreadyFull
        && unmodified.outcome != BattleHealOutcome::IneligibleSource
        && unmodified.outcome != BattleHealOutcome::IneligibleTarget)
    {
        appendAttemptedEffectModifiers(
            state,
            ownedRequest,
            sourceBefore,
            targetBefore,
            unmodified.calculatedAmount,
            effectiveModifiers);
    }

    auto result = commit(state.units, ownedRequest, effectiveModifiers);
    appendRuntimeHealEvents(state, result);
    queueAppliedEffectCommands(
        state,
        result,
        sourceBefore,
        targetBefore,
        makeEffectUnitSnapshot(
            state,
            state.units.require(result.request.targetUnitId)));
    return result;
}

BattleHealResult BattleHealSystem::recordResolved(
    BattleRuntimeState& state,
    BattleResolvedHealTransaction transaction) const
{
    assert(transaction.result.request.sourceUnitId == transaction.sourceBefore.id);
    assert(transaction.result.request.targetUnitId == transaction.targetBefore.id);
    assert(transaction.result.request.targetUnitId == transaction.targetAfter.id);
    assert(transaction.result.hpBefore == transaction.targetBefore.hp);
    assert(transaction.result.hpAfter == transaction.targetAfter.hp);
    assert(transaction.result.appliedAmount
        == transaction.targetAfter.hp - transaction.targetBefore.hp);

    auto result = std::move(transaction.result);
    result.request = ownRuntimeRequest(state, std::move(result.request));
    const auto sourceBefore = effectSnapshot(state, transaction.sourceBefore);
    const auto targetBefore = effectSnapshot(state, transaction.targetBefore);
    const auto targetAfter = effectSnapshot(state, transaction.targetAfter);

    if (result.outcome != BattleHealOutcome::AlreadyFull
        && result.outcome != BattleHealOutcome::IneligibleSource
        && result.outcome != BattleHealOutcome::IneligibleTarget)
    {
        // 數值已在傷害 snapshot 中套用過 typed status modifier；
        // 此處只發出同一筆 HealAttempted 的 runtime 事件。
        BattleHealModifierState ignoredModifiers;
        appendAttemptedEffectModifiers(
            state,
            result.request,
            sourceBefore,
            targetBefore,
            result.calculatedAmount,
            ignoredModifiers);
    }

    appendRuntimeHealEvents(state, result);
    queueAppliedEffectCommands(
        state,
        result,
        sourceBefore,
        targetBefore,
        targetAfter);
    return result;
}

std::vector<BattleHealEvent> BattleHealSystem::drainEvents(BattleRuntimeState& state) const
{
    return std::exchange(state.heals.events, {});
}

}  // namespace KysChess::Battle
