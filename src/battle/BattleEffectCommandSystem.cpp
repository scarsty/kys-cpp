#include "BattleEffectCommandSystem.h"

#include "../ChessBattleEffectSemantics.h"
#include "BattleResourceRules.h"
#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iterator>
#include <limits>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace KysChess::Battle
{
namespace
{

template<class... Visitors>
struct Overloaded : Visitors...
{
    using Visitors::operator()...;
};

bool sameBinding(const EffectSourceBinding& lhs, const EffectSourceBinding& rhs)
{
    return lhs.kind == rhs.kind
        && lhs.sourceId == rhs.sourceId
        && lhs.ownerUnitId == rhs.ownerUnitId
        && lhs.sourceTeam == rhs.sourceTeam
        && lhs.runtimeInstanceId == rhs.runtimeInstanceId;
}

bool sameStackSource(const EffectSourceBinding& lhs, const EffectSourceBinding& rhs)
{
    return lhs.kind == rhs.kind
        && lhs.sourceId == rhs.sourceId;
}

bool isAntiComboCoreAttribute(BattleAttribute attribute)
{
    return attribute == BattleAttribute::MaxHp
        || attribute == BattleAttribute::Attack
        || attribute == BattleAttribute::Defence
        || attribute == BattleAttribute::Speed;
}

bool bindingMatchesAntiCombo(
    const EffectSourceBinding& binding,
    int ownerUnitId,
    int comboId)
{
    return binding.ownerUnitId == ownerUnitId
        && binding.kind == EffectSourceKind::Combo
        && binding.sourceId == comboId;
}

void rewriteAntiComboMetadataOwner(
    EffectCommandMetadata& metadata,
    int sourceUnitId,
    int targetUnitId,
    int targetTeam)
{
    metadata.binding.ownerUnitId = targetUnitId;
    metadata.binding.sourceTeam = targetTeam;
    metadata.binding.runtimeInstanceId = 0;
    metadata.targetUnitId = targetUnitId;
    if (metadata.eventSourceUnitId == sourceUnitId)
    {
        metadata.eventSourceUnitId = targetUnitId;
    }
}

template<class Modifier>
void rewriteAntiComboModifierOwner(
    Modifier& modifier,
    int sourceUnitId,
    int targetUnitId,
    int targetTeam)
{
    modifier.binding.ownerUnitId = targetUnitId;
    modifier.binding.sourceTeam = targetTeam;
    modifier.binding.runtimeInstanceId = 0;
    modifier.targetUnitId = targetUnitId;
    if (modifier.eventSourceUnitId == sourceUnitId)
    {
        modifier.eventSourceUnitId = targetUnitId;
    }
}

bool sameAttributeStackDomain(
    const BattleAttributeModifierInstance& modifier,
    const EffectCommandMetadata& metadata,
    const ModifyAttributeEffectCommand& command)
{
    return sameStackSource(modifier.binding, metadata.binding)
        && modifier.ruleId == metadata.ruleId
        && modifier.actionOrder == metadata.actionOrder
        && modifier.targetUnitId == metadata.targetUnitId
        && modifier.attribute == command.attribute
        && modifier.eventSourceUnitId == (command.stackScope == EffectStackScope::EventSource
            ? metadata.eventSourceUnitId
            : -1);
}

bool sameDamageStackDomain(
    const BattleDamageModifierInstance& modifier,
    const EffectCommandMetadata& metadata,
    const ModifyDamageEffectCommand& command)
{
    return sameStackSource(modifier.binding, metadata.binding)
        && modifier.ruleId == metadata.ruleId
        && modifier.actionOrder == metadata.actionOrder
        && modifier.targetUnitId == metadata.targetUnitId
        && modifier.stage == command.stage
        && modifier.channel == command.channel
        && modifier.perspective == command.perspective
        && modifier.eventSourceUnitId == (command.stackScope == EffectStackScope::EventSource
            ? metadata.eventSourceUnitId
            : -1);
}

bool sameDamageAbsorptionDomain(
    const BattleDamageAbsorptionInstance& absorption,
    const EffectCommandMetadata& metadata,
    const StartDamageAbsorptionAction& action)
{
    return sameBinding(absorption.binding, metadata.binding)
        && absorption.ruleId == metadata.ruleId
        && absorption.actionOrder == metadata.actionOrder
        && absorption.statusContribution == metadata.statusContribution
        && absorption.targetUnitId == metadata.targetUnitId
        && absorption.slot == action.slot;
}

std::optional<std::int64_t> modifierExpiry(int frame, int durationFrames)
{
    if (durationFrames == 0)
    {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(frame) + durationFrames;
}

bool modifierActive(const BattleAttributeModifierInstance& modifier, int frame)
{
    return modifier.appliedFrame <= frame
        && (!modifier.expiresFrameExclusive
            || frame < *modifier.expiresFrameExclusive);
}

bool modifierActive(const BattleDamageModifierInstance& modifier, int frame)
{
    return modifier.appliedFrame <= frame
        && (!modifier.expiresFrameExclusive
            || frame < *modifier.expiresFrameExclusive);
}

bool absorptionActive(const BattleDamageAbsorptionInstance& absorption, int frame)
{
    return absorption.appliedFrame <= frame
        && frame < absorption.expiresFrameExclusive;
}

template<class Predicate>
std::vector<BattleDamageAbsorptionInstance> drainDamageAbsorptions(
    BattleRuntimeState& state,
    Predicate&& predicate)
{
    std::vector<BattleDamageAbsorptionInstance> removed;
    auto& absorptions = state.effectCommands.damageAbsorptions;
    for (auto it = absorptions.begin(); it != absorptions.end();)
    {
        if (!predicate(*it))
        {
            ++it;
            continue;
        }
        removed.push_back(std::move(*it));
        it = absorptions.erase(it);
    }
    return removed;
}

std::int64_t attributeStrength(AttributeOperation operation, int amount)
{
    if (operation == AttributeOperation::AtLeast)
    {
        return amount;
    }
    if (operation == AttributeOperation::Multiply)
    {
        return std::abs(static_cast<std::int64_t>(amount) - 100);
    }
    return std::abs(static_cast<std::int64_t>(amount));
}

std::int64_t damageStrength(DamageModifierOperation operation, int amount)
{
    switch (operation)
    {
    case DamageModifierOperation::Multiply:
        return std::abs(static_cast<std::int64_t>(amount) - 100);
    case DamageModifierOperation::CapSingleHitAtMaxHpPercent:
        // 承傷上限越低越強；其餘數值運算以偏離中性值的幅度比較。
        return -static_cast<std::int64_t>(amount);
    case DamageModifierOperation::ExecuteBelowMaxHpPercent:
        return static_cast<std::int64_t>(amount);
    case DamageModifierOperation::FlatAdd:
    case DamageModifierOperation::PercentAdd:
    case DamageModifierOperation::IgnoreDefensePercent:
        return std::abs(static_cast<std::int64_t>(amount));
    }
    assert(false);
    return 0;
}

bool damageModifierIsNegative(
    DamageModifierPerspective perspective,
    DamageModifierOperation operation,
    int amount)
{
    const bool outgoing = perspective == DamageModifierPerspective::Outgoing;
    switch (operation)
    {
    case DamageModifierOperation::FlatAdd:
    case DamageModifierOperation::PercentAdd:
    case DamageModifierOperation::IgnoreDefensePercent:
        return outgoing ? amount < 0 : amount > 0;
    case DamageModifierOperation::Multiply:
        return outgoing ? amount < 100 : amount > 100;
    case DamageModifierOperation::CapSingleHitAtMaxHpPercent:
        return outgoing;
    case DamageModifierOperation::ExecuteBelowMaxHpPercent:
        return !outgoing;
    }
    assert(false);
    return false;
}

bool expiryLater(
    const std::optional<std::int64_t>& candidate,
    const std::optional<std::int64_t>& current)
{
    if (!candidate)
    {
        return current.has_value();
    }
    return current && *candidate > *current;
}

std::uint64_t allocateNegativeEffectSequence(
    std::uint64_t* nextSequence,
    bool negative)
{
    if (!negative || !nextSequence) return 0;
    assert(*nextSequence > 0);
    assert(*nextSequence < std::numeric_limits<std::uint64_t>::max());
    return (*nextSequence)++;
}

template<class Instance>
void updateModifierPolarity(
    Instance& instance,
    bool negative,
    std::uint64_t* nextNegativeEffectSequence)
{
    if (negative && !instance.negative)
    {
        instance.negativeEffectSequence = allocateNegativeEffectSequence(
            nextNegativeEffectSequence,
            true);
    }
    else if (!negative)
    {
        instance.negativeEffectSequence = 0;
    }
    instance.negative = negative;
}

template<class Instance,
         class SameDomain,
         class Make,
         class Refresh,
         class Strength,
         class IncomingExpiryLater,
         class RefreshExpiry,
         class AddStack>
std::tuple<BattleModifierApplyOutcome, bool, Instance> applyStackPolicy(
    std::vector<Instance>& instances,
    EffectStackPolicy stack,
    SameDomain&& sameDomain,
    Make&& make,
    Refresh&& refresh,
    Strength&& strength,
    std::int64_t incomingStrength,
    IncomingExpiryLater&& incomingExpiryLater,
    RefreshExpiry&& refreshExpiry,
    AddStack&& addStack)
{
    auto first = std::find_if(instances.begin(), instances.end(), sameDomain);
    const auto append = [&]() -> Instance&
    {
        instances.push_back(make());
        return instances.back();
    };

    BattleModifierApplyOutcome outcome{};
    bool changed{};
    Instance* appliedInstance{};
    switch (stack)
    {
    case EffectStackPolicy::Independent:
        appliedInstance = &append();
        changed = true;
        outcome = BattleModifierApplyOutcome::Applied;
        break;
    case EffectStackPolicy::Replace:
    {
        const bool replaced = first != instances.end();
        std::erase_if(instances, sameDomain);
        appliedInstance = &append();
        changed = true;
        outcome = replaced
            ? BattleModifierApplyOutcome::Replaced
            : BattleModifierApplyOutcome::Applied;
        break;
    }
    case EffectStackPolicy::Refresh:
        if (first == instances.end())
        {
            appliedInstance = &append();
            changed = true;
            outcome = BattleModifierApplyOutcome::Applied;
        }
        else
        {
            changed = refresh(*first);
            appliedInstance = &*first;
            outcome = BattleModifierApplyOutcome::Refreshed;
        }
        break;
    case EffectStackPolicy::KeepStrongest:
        if (first == instances.end())
        {
            appliedInstance = &append();
            changed = true;
            outcome = BattleModifierApplyOutcome::Applied;
        }
        else if (incomingStrength > strength(*first))
        {
            const auto sequence = first->sequence;
            const auto negativeEffectSequence = first->negativeEffectSequence;
            const bool wasNegative = first->negative;
            *first = make();
            first->sequence = sequence;
            if (wasNegative && first->negative)
            {
                first->negativeEffectSequence = negativeEffectSequence;
            }
            appliedInstance = &*first;
            changed = true;
            outcome = BattleModifierApplyOutcome::Replaced;
        }
        else if (incomingStrength == strength(*first) && incomingExpiryLater(*first))
        {
            changed = refreshExpiry(*first);
            appliedInstance = &*first;
            outcome = BattleModifierApplyOutcome::Refreshed;
        }
        else
        {
            appliedInstance = &*first;
            outcome = BattleModifierApplyOutcome::KeptStronger;
        }
        break;
    case EffectStackPolicy::AddStack:
        if (first == instances.end())
        {
            appliedInstance = &append();
            changed = true;
        }
        else
        {
            changed = addStack(*first);
            appliedInstance = &*first;
        }
        outcome = BattleModifierApplyOutcome::StackChanged;
        break;
    }
    assert(appliedInstance);
    return { outcome, changed, *appliedInstance };
}

template<class Modifier, class Command>
bool modifierApplicationChanged(
    const Modifier& current,
    const Command& command,
    const std::optional<std::int64_t>& expiry,
    int stackCount)
{
    return current.stackCount != stackCount
        || current.operation != command.operation
        || current.amount != command.amount
        || current.expiresFrameExclusive != expiry;
}

BattleAttributeModifierInstance makeAttributeModifier(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyAttributeEffectCommand& command,
    int frame,
    std::uint64_t* nextNegativeEffectSequence)
{
    const bool negative = attributeModifierIsNegative(
        command.operation,
        command.amount);
    return {
        .sequence = runtime.nextAttributeSequence++,
        .negativeEffectSequence = allocateNegativeEffectSequence(
            nextNegativeEffectSequence,
            negative),
        .binding = metadata.binding,
        .ruleId = metadata.ruleId,
        .actionOrder = metadata.actionOrder,
        .targetUnitId = metadata.targetUnitId,
        .attribute = command.attribute,
        .operation = command.operation,
        .amount = command.amount,
        .appliedFrame = frame,
        .expiresFrameExclusive = modifierExpiry(frame, command.durationFrames),
        .stack = command.stack,
        .stackLimit = command.stackLimit,
        .stackCount = 1,
        .eventSourceUnitId = command.stackScope == EffectStackScope::EventSource
            ? metadata.eventSourceUnitId
            : -1,
        .negative = negative,
    };
}

BattleAttributeEffectResult applyAttribute(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyAttributeEffectCommand& command,
    int frame,
    std::uint64_t* nextNegativeEffectSequence)
{
    const auto expiry = modifierExpiry(frame, command.durationFrames);
    const auto [outcome, applied, modifier] = applyStackPolicy(
        runtime.attributeModifiers,
        command.stack,
        [&](const auto& candidate)
        {
            return sameAttributeStackDomain(candidate, metadata, command);
        },
        [&]
        {
            return makeAttributeModifier(
                runtime,
                metadata,
                command,
                frame,
                nextNegativeEffectSequence);
        },
        [&](auto& current)
        {
            const bool changed = modifierApplicationChanged(current, command, expiry, current.stackCount);
            current.operation = command.operation;
            current.amount = command.amount;
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
            current.stackLimit = command.stackLimit;
            updateModifierPolarity(
                current,
                attributeModifierIsNegative(
                    command.operation,
                    command.amount),
                nextNegativeEffectSequence);
            return changed;
        },
        [](const auto& current)
        {
            return attributeStrength(current.operation, current.amount);
        },
        attributeStrength(command.operation, command.amount),
        [&](const auto& current)
        {
            return expiryLater(expiry, current.expiresFrameExclusive);
        },
        [&](auto& current)
        {
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
            return true;
        },
        [&](auto& current)
        {
            assert(command.stackLimit);
            const int stackCount = std::min(current.stackCount + 1, *command.stackLimit);
            const bool changed = modifierApplicationChanged(current, command, expiry, stackCount);
            current.stackCount = stackCount;
            current.operation = command.operation;
            current.amount = command.amount;
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
            updateModifierPolarity(
                current,
                attributeModifierIsNegative(
                    command.operation,
                    command.amount),
                nextNegativeEffectSequence);
            return changed;
        });
    BattleAttributeEffectResult result;
    result.outcome = outcome;
    result.modifier = modifier;
    result.applied = applied;
    return result;
}

BattleDamageModifierInstance makeDamageModifier(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyDamageEffectCommand& command,
    int frame,
    std::uint64_t* nextNegativeEffectSequence)
{
    const bool negative = damageModifierIsNegative(
        command.perspective,
        command.operation,
        command.amount);
    return {
        .sequence = runtime.nextDamageSequence++,
        .negativeEffectSequence = allocateNegativeEffectSequence(
            nextNegativeEffectSequence,
            negative),
        .binding = metadata.binding,
        .ruleId = metadata.ruleId,
        .actionOrder = metadata.actionOrder,
        .targetUnitId = metadata.targetUnitId,
        .eventSourceUnitId = command.stackScope == EffectStackScope::EventSource
            ? metadata.eventSourceUnitId
            : -1,
        .perspective = command.perspective,
        .stage = command.stage,
        .channel = command.channel,
        .operation = command.operation,
        .amount = command.amount,
        .appliedFrame = frame,
        .expiresFrameExclusive = modifierExpiry(frame, command.durationFrames),
        .stack = command.stack,
        .stackLimit = command.stackLimit,
        .stackCount = 1,
        .negative = negative,
    };
}

BattleDamageModifierEffectResult applyDamageModifier(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyDamageEffectCommand& command,
    int frame,
    std::uint64_t* nextNegativeEffectSequence)
{
    const auto expiry = modifierExpiry(frame, command.durationFrames);
    const auto [outcome, applied, modifier] = applyStackPolicy(
        runtime.damageModifiers,
        command.stack,
        [&](const auto& candidate)
        {
            return sameDamageStackDomain(candidate, metadata, command);
        },
        [&]
        {
            return makeDamageModifier(
                runtime,
                metadata,
                command,
                frame,
                nextNegativeEffectSequence);
        },
        [&](auto& current)
        {
            const bool changed = modifierApplicationChanged(current, command, expiry, current.stackCount);
            current.operation = command.operation;
            current.amount = command.amount;
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
            current.stackLimit = command.stackLimit;
            updateModifierPolarity(
                current,
                damageModifierIsNegative(
                    command.perspective,
                    command.operation,
                    command.amount),
                nextNegativeEffectSequence);
            return changed;
        },
        [](const auto& current)
        {
            return damageStrength(current.operation, current.amount);
        },
        damageStrength(command.operation, command.amount),
        [&](const auto& current)
        {
            return expiryLater(expiry, current.expiresFrameExclusive);
        },
        [&](auto& current)
        {
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
            return true;
        },
        [&](auto& current)
        {
            assert(command.stackLimit);
            const int stackCount = std::min(current.stackCount + 1, *command.stackLimit);
            const bool changed = modifierApplicationChanged(current, command, expiry, stackCount);
            current.stackCount = stackCount;
            current.operation = command.operation;
            current.amount = command.amount;
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
            current.stackLimit = command.stackLimit;
            updateModifierPolarity(
                current,
                damageModifierIsNegative(
                    command.perspective,
                    command.operation,
                    command.amount),
                nextNegativeEffectSequence);
            return changed;
        });
    BattleDamageModifierEffectResult result;
    result.outcome = outcome;
    result.modifier = modifier;
    result.applied = applied;
    return result;
}

// 狀態與保護轉移只依賴單位、持續修正與狀態規則；真實執行和 prediction 共用此邊界。
struct StatusProtectionState
{
    BattleRuntimeUnits& units;
    BattleEffectCommandRuntimeState& effectCommands;
    BattleStatusSystemConfig config;
};

StatusProtectionState statusProtectionState(BattleRuntimeState& state)
{
    return { state.units, state.effectCommands, state.status.config };
}

template<class Command, class Apply>
auto applyProtectedPersistentModifier(
    StatusProtectionState state,
    const EffectCommandMetadata& metadata,
    const Command& command,
    int frame,
    bool negative,
    Apply&& apply)
{
    using Result = std::invoke_result_t<Apply, const Command&>;
    const int requestedDurationFrames = command.durationFrames;
    int appliedDurationFrames = requestedDurationFrames;
    int statusShieldAbsorbed{};
    Command effective = command;
    if (negative)
    {
        auto& target = state.units.require(metadata.targetUnitId);
        const auto protection = BattleStatusSystem(state.config)
            .protectNegativeEffect(
                target.statusDamageState(),
                requestedDurationFrames);
        target.writeStatusDamageResult(protection.target);
        appliedDurationFrames = protection.remainingDurationFrames;
        statusShieldAbsorbed = protection.statusShieldAbsorbed;
        if (protection.blocked)
        {
            Result result;
            result.outcome = BattleModifierApplyOutcome::BlockedByStatusShield;
            result.requestedDurationFrames = requestedDurationFrames;
            result.appliedDurationFrames = appliedDurationFrames;
            result.statusShieldAbsorbed = statusShieldAbsorbed;
            return result;
        }
        effective.durationFrames = appliedDurationFrames;
    }

    auto result = apply(effective);
    result.requestedDurationFrames = requestedDurationFrames;
    result.appliedDurationFrames = appliedDurationFrames;
    result.statusShieldAbsorbed = statusShieldAbsorbed;
    return result;
}

BattleDamageAbsorptionEffectResult applyDamageAbsorption(
    BattleRuntimeState& state,
    const EffectCommandMetadata& metadata,
    const StartDamageAbsorptionAction& action,
    const EffectExecutionInputs& context)
{
    const int frame = context.frame;
    assert(action.durationFrames > 0);
    assert(action.absorbedPct >= 0 && action.absorbedPct <= 100);
    assert(action.returnedPct >= 0);

    auto& runtime = state.effectCommands;
    const auto expiry = static_cast<std::int64_t>(frame) + action.durationFrames;
    const auto current = std::ranges::find_if(runtime.damageAbsorptions, [&](const auto& candidate)
    {
        return sameDamageAbsorptionDomain(candidate, metadata, action);
    });

    BattleDamageAbsorptionApplyOutcome outcome{};
    if (current == runtime.damageAbsorptions.end())
    {
        runtime.damageAbsorptions.push_back({
            .sequence = runtime.nextDamageAbsorptionSequence++,
            .binding = metadata.binding,
            .ruleId = metadata.ruleId,
            .actionOrder = metadata.actionOrder,
            .authoredActionOrder = metadata.authoredActionOrder,
            .statusContribution = metadata.statusContribution,
            .triggeringCast = context.cast,
            .triggeringAttack = context.attack,
            .targetUnitId = metadata.targetUnitId,
            .slot = action.slot,
            .absorbedPct = action.absorbedPct,
            .appliedFrame = frame,
            .expiresFrameExclusive = expiry,
            .settleOnSourceDeath = action.settleOnSourceDeath,
            .settlementTarget = action.settlementTarget,
            .settlementDamageKind = action.settlementDamageKind,
            .returnedPct = action.returnedPct,
        });
        outcome = BattleDamageAbsorptionApplyOutcome::Applied;
    }
    else
    {
        current->absorbedPct = action.absorbedPct;
        current->authoredActionOrder = metadata.authoredActionOrder;
        current->statusContribution = metadata.statusContribution;
        current->triggeringCast = context.cast;
        current->triggeringAttack = context.attack;
        current->appliedFrame = frame;
        current->expiresFrameExclusive = expiry;
        current->settleOnSourceDeath = action.settleOnSourceDeath;
        current->settlementTarget = action.settlementTarget;
        current->settlementDamageKind = action.settlementDamageKind;
        current->returnedPct = action.returnedPct;
        current->accumulatedDamage = 0;
        outcome = BattleDamageAbsorptionApplyOutcome::Refreshed;
    }

    state.effectRules.setStateValue(metadata.binding, action.slot, 0);
    const auto applied = std::ranges::find_if(runtime.damageAbsorptions, [&](const auto& candidate)
    {
        return sameDamageAbsorptionDomain(candidate, metadata, action);
    });
    assert(applied != runtime.damageAbsorptions.end());
    return { outcome, *applied };
}

int saturatedInt(std::int64_t value)
{
    return static_cast<int>(std::clamp(
        value,
        static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

BattleResourceDelta makeResourceDelta(
    int unitId,
    BattleResource resource,
    int before,
    int after)
{
    return { unitId, resource, before, after };
}

int resourceValue(const BattleRuntimeUnitRecord& record, BattleResource resource)
{
    switch (resource)
    {
    case BattleResource::Hp:
        return record.core.vitals.hp;
    case BattleResource::Mp:
        return record.core.vitals.mp;
    case BattleResource::Shield:
        return record.core.shield;
    case BattleResource::StatusShield:
        return record.status.effects.statusShield;
    case BattleResource::StaggerShield:
        return record.status.effects.staggerShield;
    case BattleResource::ActiveCooldown:
        return record.core.animation.cooldown;
    case BattleResource::ControlImmunityFrames:
        return record.status.effects.controlImmunityFrames;
    case BattleResource::InvincibilityFrames:
        return record.core.invincible;
    }
    assert(false);
    return 0;
}

BattleResourceDelta applyResourceDelta(
    BattleRuntimeState& state,
    int unitId,
    BattleResource resource,
    int delta,
    bool restoreMp)
{
    auto& record = state.units.require(unitId);
    const int before = resourceValue(record, resource);
    switch (resource)
    {
    case BattleResource::Hp:
        assert(false);
        break;
    case BattleResource::Mp:
    {
        int effectiveDelta = delta;
        if (delta > 0 && restoreMp)
        {
            effectiveDelta = adjustedMpRestore(
                record.mpBlocked(),
                BattleEffectCommandSystem::queryAttribute(
                    state,
                    {
                        .unitId = unitId,
                        .attribute = BattleAttribute::MpRecoveryBonus,
                        .baseValue = 0,
                        .frame = state.movement.frame,
                    }),
                delta);
        }
        record.core.vitals.mp = std::clamp(
            saturatedInt(static_cast<std::int64_t>(record.core.vitals.mp) + effectiveDelta),
            0,
            record.core.vitals.maxMp);
        break;
    }
    case BattleResource::Shield:
        record.setShield(std::max(
            0,
            saturatedInt(static_cast<std::int64_t>(record.core.shield) + delta)));
        break;
    case BattleResource::StatusShield:
    case BattleResource::StaggerShield:
    {
        BattleStatusSystem statusSystem(state.status.config);
        auto changed = statusSystem.changeProtection(
            record.statusDamageState(),
            resource,
            delta);
        record.writeStatusDamageResult(changed.target);
        break;
    }
    case BattleResource::ActiveCooldown:
        record.core.animation.cooldown = std::max(
            0,
            saturatedInt(static_cast<std::int64_t>(record.core.animation.cooldown) + delta));
        break;
    case BattleResource::ControlImmunityFrames:
        record.status.effects.controlImmunityFrames = std::max(
            0,
            saturatedInt(static_cast<std::int64_t>(record.status.effects.controlImmunityFrames) + delta));
        break;
    case BattleResource::InvincibilityFrames:
        record.core.invincible = std::max(
            0,
            saturatedInt(static_cast<std::int64_t>(record.core.invincible) + delta));
        break;
    }
    return makeResourceDelta(unitId, resource, before, resourceValue(record, resource));
}

BattleResourceEffectResult commitHeal(
    BattleRuntimeState& state,
    const EffectCommandMetadata& metadata,
    const ChangeResourceEffectCommand& command,
    const EffectExecutionInputs& context)
{
    const auto& target = state.units.requireCore(metadata.targetUnitId);
    if (command.healRequiresFullMp
        && target.vitals.mp < target.vitals.maxMp)
    {
        BattleResourceEffectResult result;
        result.outcome = BattleResourceEffectOutcome::NoChange;
        return result;
    }

    BattleHealRequest request;
    request.sourceUnitId = metadata.binding.ownerUnitId;
    request.targetUnitId = metadata.targetUnitId;
    request.source = metadata.binding;
    request.cast = context.cast;
    assert(command.healKind != EffectHealKind::Count);
    request.kind = command.healKind;
    request.amount = fixedHealAmount(command.resolvedAmount());
    request.sourcePolicy = command.healSourcePolicy == EffectHealSourcePolicy::AllowDead
        ? BattleHealSourcePolicy::AllowDead
        : BattleHealSourcePolicy::RequireAlive;

    BattleResourceEffectResult result;
    result.heal = BattleHealSystem().commit(state, request, context.healModifiers);
    result.deltas.push_back(makeResourceDelta(
        metadata.targetUnitId,
        BattleResource::Hp,
        result.heal->hpBefore,
        result.heal->hpAfter));
    result.outcome = result.heal->appliedAmount > 0
        ? BattleResourceEffectOutcome::Applied
        : (result.heal->outcome == BattleHealOutcome::IneligibleTarget
            ? BattleResourceEffectOutcome::IneligibleTarget
            : BattleResourceEffectOutcome::NoChange);
    return result;
}

BattleResourceEffectResult changeDirectResource(
    BattleRuntimeState& state,
    const EffectCommandMetadata& metadata,
    const ChangeResourceEffectCommand& command)
{
    BattleResourceEffectResult result;
    auto& target = state.units.require(metadata.targetUnitId);
    if (!target.alive())
    {
        result.outcome = BattleResourceEffectOutcome::IneligibleTarget;
        return result;
    }

    const auto removeFrom = [&](int unitId, int requested)
    {
        const int available = resourceValue(state.units.require(unitId), command.resource);
        const int removed = std::min(requested, std::max(0, available));
        result.deltas.push_back(applyResourceDelta(
            state,
            unitId,
            command.resource,
            -removed,
            false));
        return removed;
    };
    const auto addTo = [&](int unitId, int amount, bool restore)
    {
        result.deltas.push_back(applyResourceDelta(
            state,
            unitId,
            command.resource,
            amount,
            restore));
    };

    switch (command.kind)
    {
    case ResourceChangeKind::Restore:
        addTo(metadata.targetUnitId, command.resolvedAmount(), true);
        break;
    case ResourceChangeKind::Grant:
        if (command.sourceShieldMaxHpPct)
        {
            assert(command.resource == BattleResource::Shield);
            const int before = target.core.shield;
            const int limit = saturatedInt(static_cast<std::int64_t>(target.core.vitals.maxHp)
                * *command.sourceShieldMaxHpPct / 100);
            target.grantSourceShield(metadata.binding, metadata.ruleId, command.resolvedAmount(), limit);
            result.deltas.push_back(makeResourceDelta(
                metadata.targetUnitId, BattleResource::Shield, before, target.core.shield));
        }
        else
            addTo(metadata.targetUnitId, command.resolvedAmount(), false);
        break;
    case ResourceChangeKind::Remove:
        removeFrom(metadata.targetUnitId, command.resolvedAmount());
        break;
    case ResourceChangeKind::Drain:
    {
        const int removed = removeFrom(metadata.targetUnitId, command.resolvedAmount());
        addTo(metadata.binding.ownerUnitId, removed, true);
        break;
    }
    case ResourceChangeKind::Transfer:
    {
        assert(command.transferDestinationUnitIds.size() == 1);
        const int removed = removeFrom(metadata.targetUnitId, command.resolvedAmount());
        addTo(command.transferDestinationUnitIds.front(), removed, false);
        break;
    }
    case ResourceChangeKind::RefreshToAtLeast:
    {
        const int before = resourceValue(target, command.resource);
        addTo(metadata.targetUnitId, std::max(0, command.resolvedAmount() - before), false);
        break;
    }
    }

    result.outcome = std::ranges::any_of(result.deltas, [](const auto& delta)
    {
        return delta.before != delta.after;
    })
        ? BattleResourceEffectOutcome::Applied
        : BattleResourceEffectOutcome::NoChange;
    return result;
}

BattleStatusApplyEffectResult applyStatus(
    StatusProtectionState state,
    const EffectCommandMetadata& metadata,
    const ApplyStatusEffectCommand& command,
    const EffectExecutionInputs& context)
{
    auto& record = state.units.require(metadata.targetUnitId);
    record.status.effects.freezeReductionPct = BattleEffectCommandSystem::queryAttribute(
        state.effectCommands,
        {
            .unitId = metadata.targetUnitId,
            .attribute = BattleAttribute::StaggerResistance,
            .baseValue = 0,
            .frame = context.frame,
        });
    auto result = BattleEffectCommandSystem::applyStatusCommand(
        record.statusDamageState(),
        metadata,
        command,
        context,
        state.config,
        record.core.shield > 0);
    record.writeStatusDamageResult(result.target);
    return { std::move(result) };
}

BattleStatusConsumeEffectResult consumeStatusContribution(
    StatusProtectionState state,
    const EffectCommandMetadata& metadata,
    int holderUnitId,
    const BattleStatusConsumeRequest& request,
    const std::optional<ApplyStatusEffectCommand>& whenDepleted,
    const EffectExecutionInputs& context)
{
    auto& record = state.units.require(holderUnitId);
    auto config = state.config;
    config.frame = context.frame;
    auto consumed = BattleStatusSystem(config).consume(record.statusDamageState(), request);
    record.writeStatusDamageResult(consumed.target);

    std::optional<BattleStatusApplyResult> depleted;
    if (consumed.consumed && consumed.remainingStacks == 0 && whenDepleted)
        depleted = applyStatus(state, metadata, *whenDepleted, context).status;
    return { .status = std::move(consumed), .depletedStatus = std::move(depleted) };
}

BattleStatusConsumeEffectResult consumeStatus(
    StatusProtectionState state,
    const EffectCommandMetadata& metadata,
    const ConsumeStatusEffectCommand& command,
    const EffectExecutionInputs& context)
{
    return consumeStatusContribution(state, metadata, metadata.targetUnitId,
        command.request, command.whenDepleted, context);
}

BattleStatusConsumeEffectResult consumeThisStatus(
    StatusProtectionState state,
    const EffectCommandMetadata& metadata,
    const ConsumeThisStatusEffectCommand& command,
    const EffectExecutionInputs& context)
{
    assert(metadata.statusContribution);
    const auto& contribution = *metadata.statusContribution;
    return consumeStatusContribution(state, metadata, contribution.holderUnitId,
        command.request, command.whenDepleted, context);
}

BattleStatusRemoveEffectResult removeStatus(
    StatusProtectionState state,
    const EffectCommandMetadata& metadata,
    const RemoveStatusEffectCommand& command,
    int frame)
{
    assert(frame >= 0);
    auto& record = state.units.require(metadata.targetUnitId);
    const auto& request = command.request;

    BattleStatusSystem statusSystem(state.config);
    BattleStatusRemoveEffectResult result;
    const bool removeGenericNegatives = request.negativeOnly
        && request.statuses.empty()
        && !request.controlOnly;
    if (!removeGenericNegatives)
    {
        result.status = statusSystem.remove(record.statusDamageState(), request);
        record.writeStatusDamageResult(result.status.target);
        return result;
    }

    const auto modifierMatchesFilter = [&](const EffectSourceBinding& binding)
    {
        if (request.filter.holderUnitId
            && *request.filter.holderUnitId != metadata.targetUnitId) return false;
        if (request.filter.sourceUnitId
            && *request.filter.sourceUnitId != binding.ownerUnitId) return false;
        if (request.filter.producerBinding
            && *request.filter.producerBinding != binding) return false;
        // A persistent modifier has no contribution-generation identity.  A
        // CurrentContribution filter therefore selects only the exact status
        // contribution from which the command is executing.
        return !request.filter.appliedSequence;
    };

    const auto removeAllModifiers = [&]
    {
        auto& attributes = state.effectCommands.attributeModifiers;
        for (auto it = attributes.begin(); it != attributes.end();)
        {
            if (it->targetUnitId != metadata.targetUnitId
                || !it->negative
                || !modifierMatchesFilter(it->binding))
            {
                ++it;
                continue;
            }
            result.removedAttributeModifiers.push_back(std::move(*it));
            it = attributes.erase(it);
            ++result.status.removedCount;
        }

        auto& damage = state.effectCommands.damageModifiers;
        for (auto it = damage.begin(); it != damage.end();)
        {
            if (it->targetUnitId != metadata.targetUnitId
                || !it->negative
                || !modifierMatchesFilter(it->binding))
            {
                ++it;
                continue;
            }
            result.removedDamageModifiers.push_back(std::move(*it));
            it = damage.erase(it);
            ++result.status.removedCount;
        }
    };

    if (request.count == 0)
    {
        result.status = statusSystem.remove(record.statusDamageState(), request);
        removeAllModifiers();
        record.writeStatusDamageResult(result.status.target);
        return result;
    }

    enum class CandidateStorage
    {
        Status,
        AttributeModifier,
        DamageModifier,
    };
    struct Candidate
    {
        CandidateStorage storage{};
        BattleStatusKind statusKind{};
        std::uint64_t orderSequence{};
        std::uint64_t storageSequence{};
        int remainingFrames{};
    };
    std::vector<Candidate> candidates;
    for (const auto& status : statusSystem.removalCandidates(
             record.statusDamageState(), request))
    {
        candidates.push_back({
            .storage = CandidateStorage::Status,
            .statusKind = status.kind,
            .orderSequence = status.sequence,
            .remainingFrames = status.remainingFrames,
        });
    }
    const auto remainingModifierFrames = [frame](
        const std::optional<std::int64_t>& expiry)
    {
        if (!expiry)
        {
            return 0;
        }
        const auto remaining = *expiry - frame;
        assert(remaining > 0);
        assert(remaining <= std::numeric_limits<int>::max());
        return static_cast<int>(remaining);
    };
    for (const auto& modifier : state.effectCommands.attributeModifiers)
    {
        if (modifier.targetUnitId == metadata.targetUnitId
            && modifier.negative
            && modifierMatchesFilter(modifier.binding))
        {
            candidates.push_back({
                .storage = CandidateStorage::AttributeModifier,
                .orderSequence = modifier.negativeEffectSequence != 0
                    ? modifier.negativeEffectSequence
                    : modifier.sequence,
                .storageSequence = modifier.sequence,
                .remainingFrames = remainingModifierFrames(
                    modifier.expiresFrameExclusive),
            });
        }
    }
    for (const auto& modifier : state.effectCommands.damageModifiers)
    {
        if (modifier.targetUnitId == metadata.targetUnitId
            && modifier.negative
            && modifierMatchesFilter(modifier.binding))
        {
            candidates.push_back({
                .storage = CandidateStorage::DamageModifier,
                .orderSequence = modifier.negativeEffectSequence != 0
                    ? modifier.negativeEffectSequence
                    : modifier.sequence,
                .storageSequence = modifier.sequence,
                .remainingFrames = remainingModifierFrames(
                    modifier.expiresFrameExclusive),
            });
        }
    }

    const auto durationSortValue = [](int remainingFrames)
    {
        return remainingFrames > 0
            ? remainingFrames
            : std::numeric_limits<int>::max();
    };
    std::sort(candidates.begin(), candidates.end(), [&](const auto& lhs, const auto& rhs)
    {
        switch (request.order)
        {
        case StatusRemovalOrder::LongestRemaining:
        {
            const int lhsDuration = durationSortValue(lhs.remainingFrames);
            const int rhsDuration = durationSortValue(rhs.remainingFrames);
            if (lhsDuration != rhsDuration)
            {
                return lhsDuration > rhsDuration;
            }
            if (lhs.orderSequence != rhs.orderSequence)
            {
                return lhs.orderSequence < rhs.orderSequence;
            }
            break;
        }
        case StatusRemovalOrder::Oldest:
            if (lhs.orderSequence != rhs.orderSequence)
            {
                return lhs.orderSequence < rhs.orderSequence;
            }
            break;
        case StatusRemovalOrder::Newest:
            if (lhs.orderSequence != rhs.orderSequence)
            {
                return lhs.orderSequence > rhs.orderSequence;
            }
            break;
        }
        return std::tuple(lhs.storage, lhs.statusKind)
            < std::tuple(rhs.storage, rhs.statusKind);
    });
    candidates.resize(std::min<std::size_t>(request.count, candidates.size()));

    result.status.target = record.statusDamageState();
    const auto mergeStatusRemoval = [&](BattleStatusRemoveResult removed)
    {
        result.status.target = std::move(removed.target);
        result.status.removedCount += removed.removedCount;
        result.status.removedStatuses.insert(
            result.status.removedStatuses.end(),
            removed.removedStatuses.begin(),
            removed.removedStatuses.end());
        result.status.currentActionStaggerCleared =
            result.status.currentActionStaggerCleared
            || removed.currentActionStaggerCleared;
    };
    for (const auto& candidate : candidates)
    {
        switch (candidate.storage)
        {
        case CandidateStorage::Status:
        {
            BattleStatusRemoveRequest single;
            single.statuses = { candidate.statusKind };
            single.filter = request.filter;
            single.negativeOnly = true;
            single.count = 1;
            single.order = request.order;
            mergeStatusRemoval(statusSystem.remove(
                std::move(result.status.target),
                single));
            break;
        }
        case CandidateStorage::AttributeModifier:
        {
            auto& modifiers = state.effectCommands.attributeModifiers;
            const auto found = std::ranges::find(
                modifiers,
                candidate.storageSequence,
                &BattleAttributeModifierInstance::sequence);
            assert(found != modifiers.end());
            result.removedAttributeModifiers.push_back(std::move(*found));
            modifiers.erase(found);
            ++result.status.removedCount;
            break;
        }
        case CandidateStorage::DamageModifier:
        {
            auto& modifiers = state.effectCommands.damageModifiers;
            const auto found = std::ranges::find(
                modifiers,
                candidate.storageSequence,
                &BattleDamageModifierInstance::sequence);
            assert(found != modifiers.end());
            result.removedDamageModifiers.push_back(std::move(*found));
            modifiers.erase(found);
            ++result.status.removedCount;
            break;
        }
        }
    }

    if (request.clearCurrentActionStagger)
    {
        BattleStatusRemoveRequest stagger;
        stagger.clearCurrentActionStagger = true;
        mergeStatusRemoval(statusSystem.remove(
            std::move(result.status.target),
            stagger));
    }
    record.writeStatusDamageResult(result.status.target);
    return result;
}

BattleAreaEffectResult createArea(
    BattleRuntimeState& state,
    const CreateAreaEffectCommand& command)
{
    return { BattleAreaEffectSystem::create(state.areas, command.request) };
}

int remainingPoisonDamage(const BattleRuntimeState& state, int targetUnitId)
{
    auto& target = state.units.require(targetUnitId);
    struct PendingPoisonTick
    {
        std::int64_t nextTickFrame{};
        std::int64_t remainingTicks{};
        int intervalFrames{};
        int damagePct{};
        EffectExecutionOrderKey order;
    };
    std::vector<PendingPoisonTick> pendingTicks;
    for (const auto& poison : target.status.effects.statuses)
    {
        if (poison.kind != BattleStatusKind::Poison) continue;
        assert(poison.remainingFrames > 0);
        assert(poison.stacks > 0);
        assert(poison.behavior);
        assert(poison.behaviorRuntime.size() == poison.behavior->rules.size());
        assert(poison.producer);
        assert(poison.origin);

        const auto capability = canonicalPoisonDamageCapability(
            *poison.behavior);
        assert(capability && capability->rule && capability->damage);
        const auto ruleIndex = static_cast<std::size_t>(
            capability->rule - poison.behavior->rules.data());
        assert(ruleIndex < poison.behavior->rules.size());
        const auto& rule = poison.behavior->rules[ruleIndex];
        const auto& runtime = poison.behaviorRuntime[ruleIndex];
        assert(runtime.intervalFramesRemaining > 0);
        assert(runtime.intervalFramesRemaining <= rule.intervalFrames);
        if (runtime.intervalFramesRemaining > poison.remainingFrames)
            continue;
        const std::int64_t scheduledTicks = 1
            + (static_cast<std::int64_t>(poison.remainingFrames)
                - runtime.intervalFramesRemaining)
                / rule.intervalFrames;
        pendingTicks.push_back({
            .nextTickFrame = runtime.intervalFramesRemaining,
            .remainingTicks = std::min<std::int64_t>(
                scheduledTicks,
                poison.stacks),
            .intervalFrames = rule.intervalFrames,
            .damagePct = capability->damage->amount.percent,
            .order = statusBehaviorExecutionOrderKey(
                poison.producer->binding,
                poison.origin->ruleOrder,
                poison.producer->actionOrder,
                static_cast<std::uint32_t>(ruleIndex),
                target.core.id,
                poison.appliedSequence,
                0),
        });
    }

    int settlementDamage{};
    int projectedHp = target.core.vitals.hp;
    while (projectedHp > 0 && !pendingTicks.empty())
    {
        const auto next = std::ranges::min_element(
            pendingTicks,
            {},
            [](const PendingPoisonTick& tick)
            {
                return std::pair{ tick.nextTickFrame, tick.order };
            });
        assert(next != pendingTicks.end());
        const int tickDamage = static_cast<int>(std::max<std::int64_t>(
            1,
            static_cast<std::int64_t>(projectedHp) * next->damagePct / 100));
        const int appliedDamage = std::min(projectedHp, tickDamage);
        settlementDamage += appliedDamage;
        projectedHp -= appliedDamage;
        --next->remainingTicks;
        if (next->remainingTicks == 0)
        {
            pendingTicks.erase(next);
        }
        else
        {
            next->nextTickFrame += next->intervalFrames;
        }
    }
    return settlementDamage;
}

BattleEffectReductionValue reduceCommand(
    BattleRuntimeState& state,
    const EffectCommand& effectCommand,
    const EffectExecutionInputs& context)
{
    if (effectCommand.metadata.statusContribution)
    {
        const auto& status = *effectCommand.metadata.statusContribution;
        const auto& holder = state.units.require(status.holderUnitId);
        const auto quantity = BattleEffectCommandSystem::contributionQuantity(
            holder.status.effects, status.appliedSequence, status.kind);
        if (!quantity || *quantity != status.quantity)
        {
            return BattleSkippedEffectResult{};
        }
    }
    return std::visit(Overloaded{
        [&](const ModifyAttributeEffectCommand& command) -> BattleEffectReductionValue
        {
            return applyProtectedPersistentModifier(
                statusProtectionState(state),
                effectCommand.metadata,
                command,
                context.frame,
                attributeModifierIsNegative(
                    command.operation,
                    command.amount),
                [&](const auto& effective)
                {
                    auto& nextNegativeEffectSequence = state.units
                        .require(effectCommand.metadata.targetUnitId)
                        .status.effects.nextNegativeEffectSequence;
                    return applyAttribute(
                        state.effectCommands,
                        effectCommand.metadata,
                        effective,
                        context.frame,
                        &nextNegativeEffectSequence);
                });
        },
        [&](const ModifyDamageEffectCommand& command) -> BattleEffectReductionValue
        {
            if (command.durationFrames > 0
                || command.stack != EffectStackPolicy::Independent
                || effectCommand.metadata.event == EffectEvent::BattleInitialized)
            {
                return applyProtectedPersistentModifier(
                    statusProtectionState(state),
                    effectCommand.metadata,
                    command,
                    context.frame,
                    damageModifierIsNegative(
                        command.perspective,
                        command.operation,
                        command.amount),
                    [&](const auto& effective)
                    {
                        auto& nextNegativeEffectSequence = state.units
                            .require(effectCommand.metadata.targetUnitId)
                            .status.effects.nextNegativeEffectSequence;
                        return applyDamageModifier(
                            state.effectCommands,
                            effectCommand.metadata,
                            effective,
                            context.frame,
                            &nextNegativeEffectSequence);
                    });
            }
            return BattleRoutedEffectCommand<ModifyDamageEffectCommand>{ command };
        },
        [&](const ChangeResourceEffectCommand& command) -> BattleEffectReductionValue
        {
            assert(command.resolvedAmount() >= 0);
            if (command.resource == BattleResource::Hp)
            {
                if (command.kind == ResourceChangeKind::Restore
                    || command.kind == ResourceChangeKind::Grant)
                {
                    return commitHeal(state, effectCommand.metadata, command, context);
                }
                DealDamageEffectCommand damage;
                damage.amount = command.resolvedAmount();
                damage.kind = BattleDamageKind::Effect;
                damage.delivery.statusTickPresentation = false;
                return BattleEffectCommandSystem::prepareDamageOutput(effectCommand.metadata, damage, context);
            }
            return changeDirectResource(state, effectCommand.metadata, command);
        },
        [&](const ModifyHealTransactionEffectCommand& command) -> BattleEffectReductionValue
        {
            return BattleRoutedEffectCommand<ModifyHealTransactionEffectCommand>{ command };
        },
        [&](const ApplyStatusEffectCommand& command) -> BattleEffectReductionValue
        {
            return applyStatus(statusProtectionState(state), effectCommand.metadata, command, context);
        },
        [&](const ConsumeStatusEffectCommand& command) -> BattleEffectReductionValue
        {
            return consumeStatus(statusProtectionState(state), effectCommand.metadata, command, context);
        },
        [&](const ConsumeThisStatusEffectCommand& command) -> BattleEffectReductionValue
        {
            return consumeThisStatus(statusProtectionState(state), effectCommand.metadata, command, context);
        },
        [&](const RemoveStatusEffectCommand& command) -> BattleEffectReductionValue
        {
            return removeStatus(
                statusProtectionState(state),
                effectCommand.metadata,
                command,
                context.frame);
        },
        [&](const DealDamageEffectCommand& command) -> BattleEffectReductionValue
        {
            assert(command.amount >= 0);
            return BattleEffectCommandSystem::prepareDamageOutput(effectCommand.metadata, command, context);
        },
        [&](const SuppressCurrentCastContactsEffectCommand& command)
            -> BattleEffectReductionValue
        {
            return BattleRoutedEffectCommand<SuppressCurrentCastContactsEffectCommand>{
                command };
        },
        [&](const MakeIncomingAttackMissEffectCommand& command)
            -> BattleEffectReductionValue
        {
            return BattleRoutedEffectCommand<MakeIncomingAttackMissEffectCommand>{
                command };
        },
        [&](const ModifyAttackEffectCommand& command) -> BattleEffectReductionValue
        {
            return BattleRoutedEffectCommand<ModifyAttackEffectCommand>{ command };
        },
        [&](const ForceMoveEffectCommand& command) -> BattleEffectReductionValue
        {
            return BattleRoutedEffectCommand<ForceMoveEffectCommand>{ command };
        },
        [&](const CreateAreaEffectCommand& command) -> BattleEffectReductionValue
        {
            return createArea(state, command);
        },
        [&](const ModifyCastEffectCommand& command) -> BattleEffectReductionValue
        {
            return BattleRoutedEffectCommand<ModifyCastEffectCommand>{ command };
        },
        [&](const StateMachineEffectCommand& command) -> BattleEffectReductionValue
        {
            if (const auto* start = std::get_if<StartDamageAbsorptionAction>(&command.value))
                return applyDamageAbsorption(state, effectCommand.metadata, *start, context);
            DealDamageEffectCommand damage;
            damage.delivery.statusTickPresentation = false;
            if (const auto* settlement = std::get_if<StateDamageEffectCommand>(&command.value))
            {
                if (settlement->targetUnitIds.empty()) return BattleSkippedEffectResult{};
                damage.amount = settlement->amount;
                damage.kind = settlement->kind;
                damage.delivery.targetUnitIds = settlement->targetUnitIds;
            }
            else if (const auto* poison = std::get_if<SettleRemainingStatusDamageAction>(&command.value))
            {
                assert(poison->status == BattleStatusKind::Poison);
                damage.amount = remainingPoisonDamage(state, effectCommand.metadata.targetUnitId);
                damage.kind = BattleDamageKind::Poison;
                damage.appliesDamageModifiers = false;
                damage.applyDefenderTypedStatuses = true;
            }
            else
            {
                return BattleSkippedEffectResult{};
            }
            if (damage.amount <= 0) return BattleSkippedEffectResult{};
            return BattleEffectCommandSystem::prepareDamageOutput(effectCommand.metadata, damage, context);
        },
    }, effectCommand.value);
}

}  // namespace

BattleEffectDamageRequestOutput BattleEffectCommandSystem::prepareDamageOutput(
    const EffectCommandMetadata& metadata,
    const DealDamageEffectCommand& command,
    const EffectExecutionInputs& context)
{
    assert(command.transactionCount > 0);
    BattleDamageRequest request;
    request.attackerUnitId = metadata.binding.ownerUnitId;
    request.defenderUnitId = metadata.targetUnitId;
    request.baseDamage = command.amount;
    request.damageKind = command.kind;
    request.preResolvedDamage = !command.appliesDamageModifiers;
    request.triggersDefenseEffects = command.triggersHurtInvincibility;
    // 「處決」動作本身代表直接斬殺；生命門檻等規則條件已在命令產生前篩選。
    request.canExecute = command.canExecute;
    request.executeThresholdPct = request.canExecute ? 100 : 0;
    request.preResolvedModifierPolicy = command.applyDefenderTypedStatuses
        ? BattlePreResolvedModifierPolicy::DefenderTypedStatuses
        : BattlePreResolvedModifierPolicy::None;
    request.acceptedHit = false;

    return {
        .request = request,
        .delivery = command.delivery,
        .source = metadata.binding,
        .ruleId = metadata.ruleId,
        .triggeringCast = context.cast,
        .triggeringAttack = context.attack,
        .hitDamageCredit = context.hitDamageCredit
            && context.hitDamageCredit->provenance.cast.sourceUnitId == metadata.binding.ownerUnitId
            ? context.hitDamageCredit : std::nullopt,
        .statusContribution = metadata.statusContribution,
        .authoredActionOrder = metadata.authoredActionOrder,
        .transactionCount = command.transactionCount,
        .eventSourceUnitId = metadata.eventSourceUnitId,
    };
}



std::optional<int> BattleEffectCommandSystem::contributionQuantity(
    const BattleStatusEffectState& effects, std::uint64_t sequence, BattleStatusKind kind)
{
    const auto found = std::ranges::find(effects.statuses, sequence,
        &BattleStatusContribution::appliedSequence);
    if (found == effects.statuses.end() || found->kind != kind || found->stacks <= 0)
        return std::nullopt;
    return found->stacks;
}

bool BattleEffectCommandSystem::actionMayAffectStatusLiveness(const EffectAction& action)
{
    return std::visit([](const auto& value)
    {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
        {
            assert(value);
            return std::ranges::any_of(value->whenTrue, actionMayAffectStatusLiveness)
                || std::ranges::any_of(value->whenFalse, actionMayAffectStatusLiveness);
        }
        else if constexpr (std::is_same_v<T, ChangeResourceAction>)
        {
            // MP 與一般護盾不影響 contribution 數量或狀態保護；HP 補血可能巢狀 dispatch。
            return value.resource != BattleResource::Mp && value.resource != BattleResource::Shield;
        }
        else if constexpr (std::is_same_v<T, DealDamageAction>
            || std::is_same_v<T, ModifyAttackAction>
            || std::is_same_v<T, ForceMoveAction>
            || std::is_same_v<T, ModifyCastAction>
            || std::is_same_v<T, ModifyHealTransactionAction>
            || std::is_same_v<T, SuppressCurrentCastContactsAction>
            || std::is_same_v<T, MakeIncomingAttackMissAction>
            || std::is_same_v<T, BlockPositiveDamageAction>)
        {
            // 這些命令只路由或準備後續傷害，不會在 prediction reducer 內修改狀態存活。
            return false;
        }
        else
        {
            // 新動作預設走完整 reducer；漏分類只影響效能，不會略過必要的狀態預測。
            return true;
        }
    }, action.value);
}

BattleRuntimeState BattleEffectCommandSystem::copyDispatchState(
    const BattleRuntimeState& source,
    std::pmr::memory_resource* memoryResource)
{
    // 完整 reducer 仍處理補血和區域；補血可能巢狀 dispatch，不能只複製狀態陣列。
    // 不複製攻擊世界、物理地形、救援搜尋或未執行的佇列，也不複製已輸出的事件。
    BattleRuntimeState state{
        .castLifecycle = BattleCastLifecycle(BattleCastExecutionState(
            source.castLifecycle.executionState(), memoryResource)),
        .random = source.random,
        .talentRandom = BattleRuntimeRandom{},
        .effectRules = BattleEffectRuleStore(memoryResource),
    };
    state.gridTransform = source.gridTransform;
    state.units = source.units;
    state.movement.frame = source.movement.frame;
    state.heals.nextTransactionId = source.heals.nextTransactionId;
    state.heals.committedTransactions = source.heals.committedTransactions;
    state.areas = source.areas;
    state.effectRules = source.effectRules;
    state.effectCommands = source.effectCommands;
    state.effectIntegration.nextEventOrdinal = source.effectIntegration.nextEventOrdinal;
    state.status = source.status;
    return state;
}

BattleEffectCommandReduction BattleEffectCommandSystem::reduce(
    BattleRuntimeState& state,
    std::span<const EffectCommand> commands) const
{

    BattleEffectCommandReduction result;
    result.entries.reserve(commands.size());
    for (std::size_t i = 0; i < commands.size(); ++i)
    {
        const auto& context = commands[i].execution;
        assert(context.frame >= 0);
        removeExpiredAttributeModifiers(state, context.frame);
        removeExpiredDamageModifiers(state, context.frame);
        result.entries.push_back({
            .inputOrder = i,
            .metadata = commands[i].metadata,
            .value = reduceCommand(state, commands[i], context),
        });
    }
    return result;
}

BattleEffectCommandReduction BattleEffectCommandSystem::reduce(
    BattleRuntimeState& state,
    const EffectCommand& command) const
{
    return reduce(state, std::span{ &command, std::size_t{ 1 } });
}

int BattleEffectCommandSystem::queryAttribute(
    const BattleRuntimeState& state,
    const BattleAttributeQuery& query)
{
    return queryAttribute(state.effectCommands, query);
}

int BattleEffectCommandSystem::queryAttribute(
    const BattleEffectCommandRuntimeState& runtime,
    const BattleAttributeQuery& query)
{
    assert(query.unitId >= 0);
    assert(query.frame >= 0);

    const std::int64_t baseValue = query.baseValue;
    std::int64_t value = baseValue;
    for (const auto& modifier : runtime.attributeModifiers)
    {
        if (modifier.targetUnitId != query.unitId
            || (modifier.eventSourceUnitId >= 0
                && modifier.eventSourceUnitId != query.eventSourceUnitId)
            || modifier.attribute != query.attribute
            || !modifierActive(modifier, query.frame))
        {
            continue;
        }
        const std::int64_t amount = static_cast<std::int64_t>(modifier.amount)
            * modifier.stackCount;
        switch (modifier.operation)
        {
        case AttributeOperation::FlatAdd:
            value += amount;
            break;
        case AttributeOperation::PercentAdd:
            value += baseValue * amount / 100;
            break;
        case AttributeOperation::PercentagePointAdd:
            value += amount;
            break;
        case AttributeOperation::Override:
            value = amount;
            break;
        case AttributeOperation::Multiply:
            value = value * amount / 100;
            break;
        case AttributeOperation::AtLeast:
            value = std::max(value, amount);
            break;
        }
    }
    return saturatedInt(value);
}

BattleAttributeEffectResult BattleEffectCommandSystem::applyPersistentAttributeModifier(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyAttributeEffectCommand& command,
    int frame,
    std::uint64_t* nextNegativeEffectSequence)
{
    assert(frame >= 0);
    return applyAttribute(
        runtime,
        metadata,
        command,
        frame,
        nextNegativeEffectSequence);
}

BattleDamageModifierEffectResult BattleEffectCommandSystem::applyPersistentDamageModifier(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyDamageEffectCommand& command,
    int frame,
    std::uint64_t* nextNegativeEffectSequence)
{
    assert(frame >= 0);
    return applyDamageModifier(
        runtime,
        metadata,
        command,
        frame,
        nextNegativeEffectSequence);
}

EffectDamageOrigin BattleEffectCommandSystem::damageOrigin(
    const BattleEffectDamageRequestOutput& output)
{
    return makeEffectDamageOrigin(
        output.source,
        output.ruleId,
        output.authoredActionOrder,
        output.statusContribution,
        output.triggeringCast,
        output.triggeringAttack);
}

BattleStatusProducerProvenance BattleEffectCommandSystem::statusProducerProvenance(
    const EffectCommandMetadata& metadata)
{
    const auto producerActionOrder = metadata.statusContribution
        ? metadata.producerActionOrder
        : metadata.authoredActionOrder;
    const auto behaviorActionOrder = metadata.statusContribution
        ? metadata.authoredActionOrder
        : 0;
    return makeBattleStatusProducerProvenance(
        metadata.binding,
        metadata.ruleId,
        metadata.ruleOrder,
        producerActionOrder,
        metadata.behaviorRuleOrder,
        behaviorActionOrder);
}

BattleStatusApplyResult BattleEffectCommandSystem::applyStatusCommand(
    BattleStatusUnitState target,
    const EffectCommandMetadata& metadata,
    const ApplyStatusEffectCommand& command,
    const EffectExecutionInputs& context,
    BattleStatusSystemConfig statusConfig,
    bool targetHasShield)
{
    BattleStatusApplyRequest request;
    request.kind = command.status;
    const auto provenance = statusProducerProvenance(metadata);
    request.producer = provenance.producer;
    request.producerFamily = provenance.producerFamily;
    request.behavior = command.behavior;
    request.sourceUnitId = provenance.sourceUnitId;
    request.origin = provenance.origin;
    request.durationFrames = command.durationFrames;
    request.stacks = command.stacks;
    request.stack = command.stack;
    request.stackLimit = command.stackLimit;
    request.targetTotalLimit = command.targetTotalLimit;
    request.targetHasShield = targetHasShield;
    request.controlLowHpImmunityPct = context.controlLowHpImmunityPct;
    request.bypassStatusShield = context.bypassStatusShield;

    statusConfig.frame = context.frame;
    return BattleStatusSystem(statusConfig).apply(std::move(target), request);
}

int BattleEffectCommandSystem::antiComboAttributeValue(
    const BattleAntiComboAttributeBasis& basis)
{
    const int valueAfterFlat = basis.baseValue + basis.flatTotal;
    if (basis.percentTotal == 0)
    {
        return valueAfterFlat;
    }
    return valueAfterFlat * (100 + basis.percentTotal) / 100;
}

void BattleEffectCommandSystem::recordAntiComboInitialization(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    BattleAntiComboInitializationValue value)
{
    assert(metadata.event == EffectEvent::BattleInitialized);
    assert(metadata.binding.kind == EffectSourceKind::Combo);
    runtime.antiComboInitializationRecords.push_back({
        .metadata = metadata,
        .value = std::move(value),
    });
}

void BattleEffectCommandSystem::inheritCloneEffectModifiers(
    BattleEffectCommandRuntimeState& runtime,
    int sourceUnitId,
    int cloneUnitId,
    int cloneTeam,
    std::uint64_t& nextNegativeEffectSequence)
{
    assert(sourceUnitId >= 0);
    assert(cloneUnitId >= 0);

    const auto cloneUnitScopedInstances = [&]<class Instance>(
        std::vector<Instance>& instances,
        std::uint64_t& nextSequence)
    {
        std::vector<Instance> clonedInstances;
        for (const auto& sourceInstance : instances)
        {
            if (sourceInstance.targetUnitId != sourceUnitId)
            {
                continue;
            }
            auto cloned = sourceInstance;
            cloned.sequence = nextSequence++;
            cloned.targetUnitId = cloneUnitId;
            if (cloned.negative)
            {
                assert(cloned.negativeEffectSequence > 0);
                assert(cloned.negativeEffectSequence
                    < std::numeric_limits<std::uint64_t>::max());
                nextNegativeEffectSequence = std::max(
                    nextNegativeEffectSequence,
                    cloned.negativeEffectSequence + 1);
            }
            if (cloned.binding.ownerUnitId == sourceUnitId)
            {
                cloned.binding.ownerUnitId = cloneUnitId;
                cloned.binding.sourceTeam = cloneTeam;
                cloned.binding.runtimeInstanceId = 0;
            }
            if constexpr (requires { cloned.eventSourceUnitId; })
            {
                if (cloned.eventSourceUnitId == sourceUnitId)
                {
                    cloned.eventSourceUnitId = cloneUnitId;
                }
            }
            clonedInstances.push_back(std::move(cloned));
        }
        instances.insert(
            instances.end(),
            std::make_move_iterator(clonedInstances.begin()),
            std::make_move_iterator(clonedInstances.end()));
    };

    // 分身在 BattleInitialized 完成後立即建立；直接複製來源單位的
    // 屬性與傷害修正，不依賴反向羈絆初始化記錄。
    cloneUnitScopedInstances(
        runtime.attributeModifiers,
        runtime.nextAttributeSequence);
    cloneUnitScopedInstances(
        runtime.damageModifiers,
        runtime.nextDamageSequence);
}

BattleAntiComboTransfer BattleEffectCommandSystem::transferAntiComboInitialization(
    BattleEffectCommandRuntimeState& runtime,
    int sourceUnitId,
    int targetUnitId,
    int targetTeam,
    int comboId,
    int frame,
    std::uint64_t* nextNegativeEffectSequence)
{
    assert(sourceUnitId >= 0);
    assert(targetUnitId >= 0);

    std::vector<BattleAntiComboInitializationRecord> transferredRecords;
    for (const auto& sourceRecord : runtime.antiComboInitializationRecords)
    {
        if (sourceRecord.metadata.targetUnitId != sourceUnitId
            || !bindingMatchesAntiCombo(
                sourceRecord.metadata.binding,
                sourceUnitId,
                comboId))
        {
            continue;
        }
        auto transferred = sourceRecord;
        rewriteAntiComboMetadataOwner(
            transferred.metadata,
            sourceUnitId,
            targetUnitId,
            targetTeam);
        transferredRecords.push_back(std::move(transferred));
    }

    struct IncomingAttributeTotals
    {
        int flat{};
        int percent{};
    };
    std::map<BattleAttribute, IncomingAttributeTotals> incomingTotals;
    BattleAntiComboTransfer result;
    for (const auto& transferred : transferredRecords)
    {
        if (const auto* attribute = std::get_if<ModifyAttributeEffectCommand>(
                &transferred.value))
        {
            if (!isAntiComboCoreAttribute(attribute->attribute))
            {
                continue;
            }
            auto& total = incomingTotals[attribute->attribute];
            switch (attribute->operation)
            {
            case AttributeOperation::FlatAdd:
                total.flat += attribute->amount;
                break;
            case AttributeOperation::PercentAdd:
                total.percent += attribute->amount;
                break;
            case AttributeOperation::Override:
            case AttributeOperation::Multiply:
            case AttributeOperation::AtLeast:
                assert(false);
                break;
            }
            continue;
        }

        if (std::holds_alternative<ModifyDamageEffectCommand>(transferred.value))
        {
            continue;
        }
        if (const auto* resource = std::get_if<ChangeResourceEffectCommand>(
                &transferred.value))
        {
            result.commands.push_back({ transferred.metadata, *resource, { .frame = frame } });
            continue;
        }
        const auto* status = std::get_if<ApplyStatusEffectCommand>(
            &transferred.value);
        assert(status);
        result.commands.push_back({ transferred.metadata, *status, { .frame = frame } });
    }

    for (const auto& [attribute, incoming] : incomingTotals)
    {
        const BattleAntiComboAttributeKey key{ targetUnitId, attribute };
        const auto basisIt = runtime.antiComboAttributeBases.find(key);
        assert(basisIt != runtime.antiComboAttributeBases.end());
        auto& basis = basisIt->second;
        const int before = antiComboAttributeValue(basis);
        basis.flatTotal += incoming.flat;
        basis.percentTotal += incoming.percent;
        const int after = antiComboAttributeValue(basis);
        result.coreAttributeDeltas.push_back({ attribute, after - before });
    }

    std::vector<BattleAttributeModifierInstance> transferredModifiers;
    for (const auto& sourceModifier : runtime.attributeModifiers)
    {
        if (sourceModifier.targetUnitId != sourceUnitId
            || sourceModifier.expiresFrameExclusive
            || !bindingMatchesAntiCombo(
                sourceModifier.binding,
                sourceUnitId,
                comboId))
        {
            continue;
        }
        const bool initializedModifier = std::ranges::any_of(
            transferredRecords,
            [&](const BattleAntiComboInitializationRecord& transferred)
            {
                const auto* attribute = std::get_if<ModifyAttributeEffectCommand>(
                    &transferred.value);
                return attribute
                    && !isAntiComboCoreAttribute(attribute->attribute)
                    && transferred.metadata.ruleId == sourceModifier.ruleId
                    && transferred.metadata.actionOrder == sourceModifier.actionOrder;
            });
        if (!initializedModifier)
        {
            continue;
        }

        auto transferred = sourceModifier;
        transferred.sequence = runtime.nextAttributeSequence++;
        transferred.negativeEffectSequence = allocateNegativeEffectSequence(
            nextNegativeEffectSequence,
            transferred.negative);
        rewriteAntiComboModifierOwner(
            transferred,
            sourceUnitId,
            targetUnitId,
            targetTeam);
        transferredModifiers.push_back(std::move(transferred));
    }

    runtime.attributeModifiers.insert(
        runtime.attributeModifiers.end(),
        std::make_move_iterator(transferredModifiers.begin()),
        std::make_move_iterator(transferredModifiers.end()));

    std::vector<BattleDamageModifierInstance> transferredDamageModifiers;
    for (const auto& sourceModifier : runtime.damageModifiers)
    {
        if (sourceModifier.targetUnitId != sourceUnitId
            || sourceModifier.expiresFrameExclusive
            || !bindingMatchesAntiCombo(
                sourceModifier.binding,
                sourceUnitId,
                comboId))
        {
            continue;
        }
        const bool initializedModifier = std::ranges::any_of(
            transferredRecords,
            [&](const BattleAntiComboInitializationRecord& transferred)
            {
                return std::holds_alternative<ModifyDamageEffectCommand>(
                           transferred.value)
                    && transferred.metadata.ruleId == sourceModifier.ruleId
                    && transferred.metadata.actionOrder == sourceModifier.actionOrder;
            });
        if (!initializedModifier)
        {
            continue;
        }

        auto transferred = sourceModifier;
        transferred.sequence = runtime.nextDamageSequence++;
        transferred.negativeEffectSequence = allocateNegativeEffectSequence(
            nextNegativeEffectSequence,
            transferred.negative);
        rewriteAntiComboModifierOwner(
            transferred,
            sourceUnitId,
            targetUnitId,
            targetTeam);
        transferredDamageModifiers.push_back(std::move(transferred));
    }
    runtime.damageModifiers.insert(
        runtime.damageModifiers.end(),
        std::make_move_iterator(transferredDamageModifiers.begin()),
        std::make_move_iterator(transferredDamageModifiers.end()));
    runtime.antiComboInitializationRecords.insert(
        runtime.antiComboInitializationRecords.end(),
        std::make_move_iterator(transferredRecords.begin()),
        std::make_move_iterator(transferredRecords.end()));
    return result;
}

std::vector<BattleDamageModifierInstance>
BattleEffectCommandSystem::queryDamageModifiers(
    const BattleRuntimeState& state,
    const BattleDamageModifierQuery& query)
{
    assert(query.unitId >= 0);
    assert(query.frame >= 0);

    std::vector<BattleDamageModifierInstance> result;
    for (const auto& modifier : state.effectCommands.damageModifiers)
    {
        if (modifier.targetUnitId != query.unitId
            || modifier.perspective != query.perspective
            || (modifier.eventSourceUnitId >= 0
                && modifier.eventSourceUnitId != query.eventSourceUnitId)
            || modifier.stage != query.stage
            || (modifier.channel != DamageChannel::All
                && modifier.channel != query.channel)
            || !modifierActive(modifier, query.frame))
        {
            continue;
        }
        result.push_back(modifier);
    }
    return result;
}

std::vector<BattleAttributeModifierInstance>
BattleEffectCommandSystem::removeExpiredAttributeModifiers(
    BattleRuntimeState& state,
    int frame)
{
    assert(frame >= 0);
    std::vector<BattleAttributeModifierInstance> removed;
    auto& modifiers = state.effectCommands.attributeModifiers;
    for (auto it = modifiers.begin(); it != modifiers.end();)
    {
        if (modifierActive(*it, frame))
        {
            ++it;
            continue;
        }
        removed.push_back(std::move(*it));
        it = modifiers.erase(it);
    }
    return removed;
}

std::vector<BattleDamageModifierInstance>
BattleEffectCommandSystem::removeExpiredDamageModifiers(
    BattleRuntimeState& state,
    int frame)
{
    assert(frame >= 0);
    std::vector<BattleDamageModifierInstance> removed;
    auto& modifiers = state.effectCommands.damageModifiers;
    for (auto it = modifiers.begin(); it != modifiers.end();)
    {
        if (modifierActive(*it, frame))
        {
            ++it;
            continue;
        }
        removed.push_back(std::move(*it));
        it = modifiers.erase(it);
    }
    return removed;
}

std::vector<BattleDamageAbsorptionLayer>
BattleEffectCommandSystem::queryDamageAbsorptions(
    const BattleRuntimeState& state,
    int targetUnitId,
    int frame)
{
    assert(targetUnitId >= 0);
    assert(frame >= 0);

    std::vector<BattleDamageAbsorptionLayer> result;
    for (const auto& absorption : state.effectCommands.damageAbsorptions)
    {
        if (absorption.targetUnitId != targetUnitId
            || !absorptionActive(absorption, frame))
        {
            continue;
        }
        result.push_back({ absorption.sequence, absorption.absorbedPct });
    }
    return result;
}

void BattleEffectCommandSystem::accumulateDamageAbsorptions(
    BattleRuntimeState& state,
    std::span<const BattleDamageAbsorptionReceipt> receipts)
{
    std::uint64_t previousSequence{};
    for (const auto& receipt : receipts)
    {
        assert(receipt.sequence > previousSequence);
        assert(receipt.absorbedDamage >= 0);
        previousSequence = receipt.sequence;

        const auto absorption = std::ranges::find(
            state.effectCommands.damageAbsorptions,
            receipt.sequence,
            &BattleDamageAbsorptionInstance::sequence);
        assert(absorption != state.effectCommands.damageAbsorptions.end());
        assert(absorption->accumulatedDamage
            <= std::numeric_limits<std::int64_t>::max() - receipt.absorbedDamage);
        absorption->accumulatedDamage += receipt.absorbedDamage;
        state.effectRules.setStateValue(
            absorption->binding,
            absorption->slot,
            absorption->accumulatedDamage);
    }
}

std::vector<BattleDamageAbsorptionInstance>
BattleEffectCommandSystem::removeExpiredDamageAbsorptions(
    BattleRuntimeState& state,
    int frame)
{
    assert(frame >= 0);
    return drainDamageAbsorptions(state, [&](const auto& absorption)
    {
        return absorption.expiresFrameExclusive <= frame;
    });
}

std::vector<BattleDamageAbsorptionInstance>
BattleEffectCommandSystem::removeDamageAbsorptionsForSourceDeath(
    BattleRuntimeState& state,
    int sourceUnitId)
{
    assert(sourceUnitId >= 0);
    return drainDamageAbsorptions(state, [&](const auto& absorption)
    {
        return absorption.binding.ownerUnitId == sourceUnitId
            && absorption.settleOnSourceDeath;
    });
}

}  // namespace KysChess::Battle
