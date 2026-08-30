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
        && modifier.attribute == command.action.attribute
        && modifier.eventSourceUnitId == (command.action.stackScope == EffectStackScope::EventSource
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
        && modifier.stage == command.action.stage
        && modifier.channel == command.action.channel
        && modifier.perspective == command.action.perspective
        && modifier.eventSourceUnitId == (command.action.stackScope == EffectStackScope::EventSource
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

template<class Instance,
         class SameDomain,
         class Make,
         class Refresh,
         class Strength,
         class IncomingExpiryLater,
         class RefreshExpiry,
         class AddStack>
std::pair<BattleModifierApplyOutcome, Instance> applyStackPolicy(
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
    Instance* applied{};
    switch (stack)
    {
    case EffectStackPolicy::Independent:
        applied = &append();
        outcome = BattleModifierApplyOutcome::Applied;
        break;
    case EffectStackPolicy::Replace:
    {
        const bool replaced = first != instances.end();
        std::erase_if(instances, sameDomain);
        applied = &append();
        outcome = replaced
            ? BattleModifierApplyOutcome::Replaced
            : BattleModifierApplyOutcome::Applied;
        break;
    }
    case EffectStackPolicy::Refresh:
        if (first == instances.end())
        {
            applied = &append();
            outcome = BattleModifierApplyOutcome::Applied;
        }
        else
        {
            refresh(*first);
            applied = &*first;
            outcome = BattleModifierApplyOutcome::Refreshed;
        }
        break;
    case EffectStackPolicy::KeepStrongest:
        if (first == instances.end())
        {
            applied = &append();
            outcome = BattleModifierApplyOutcome::Applied;
        }
        else if (incomingStrength > strength(*first))
        {
            const auto sequence = first->sequence;
            *first = make();
            first->sequence = sequence;
            applied = &*first;
            outcome = BattleModifierApplyOutcome::Replaced;
        }
        else if (incomingStrength == strength(*first) && incomingExpiryLater(*first))
        {
            refreshExpiry(*first);
            applied = &*first;
            outcome = BattleModifierApplyOutcome::Refreshed;
        }
        else
        {
            applied = &*first;
            outcome = BattleModifierApplyOutcome::KeptStronger;
        }
        break;
    case EffectStackPolicy::AddStack:
        if (first == instances.end())
        {
            applied = &append();
        }
        else
        {
            addStack(*first);
            applied = &*first;
        }
        outcome = BattleModifierApplyOutcome::StackChanged;
        break;
    }
    assert(applied);
    return { outcome, *applied };
}

BattleAttributeModifierInstance makeAttributeModifier(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyAttributeEffectCommand& command,
    int frame)
{
    return {
        .sequence = runtime.nextAttributeSequence++,
        .binding = metadata.binding,
        .ruleId = metadata.ruleId,
        .actionOrder = metadata.actionOrder,
        .targetUnitId = metadata.targetUnitId,
        .attribute = command.action.attribute,
        .operation = command.action.operation,
        .amount = command.amount,
        .appliedFrame = frame,
        .expiresFrameExclusive = modifierExpiry(frame, command.action.durationFrames),
        .stack = command.action.stack,
        .stackLimit = command.action.stackLimit,
        .stackCount = 1,
        .eventSourceUnitId = command.action.stackScope == EffectStackScope::EventSource
            ? metadata.eventSourceUnitId
            : -1,
        .negative = attributeModifierIsNegative(
            command.action.operation,
            command.amount),
    };
}

BattleAttributeEffectResult applyAttribute(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyAttributeEffectCommand& command,
    int frame)
{
    const auto expiry = modifierExpiry(frame, command.action.durationFrames);
    const auto [outcome, modifier] = applyStackPolicy(
        runtime.attributeModifiers,
        command.action.stack,
        [&](const auto& candidate)
        {
            return sameAttributeStackDomain(candidate, metadata, command);
        },
        [&]
        {
            return makeAttributeModifier(runtime, metadata, command, frame);
        },
        [&](auto& current)
        {
            current.operation = command.action.operation;
            current.amount = command.amount;
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
            current.stackLimit = command.action.stackLimit;
            current.negative = attributeModifierIsNegative(
                command.action.operation,
                command.amount);
        },
        [](const auto& current)
        {
            return attributeStrength(current.operation, current.amount);
        },
        attributeStrength(command.action.operation, command.amount),
        [&](const auto& current)
        {
            return expiryLater(expiry, current.expiresFrameExclusive);
        },
        [&](auto& current)
        {
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
        },
        [&](auto& current)
        {
            assert(command.action.stackLimit);
            current.stackCount = std::min(current.stackCount + 1, *command.action.stackLimit);
            current.operation = command.action.operation;
            current.amount = command.amount;
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
            current.negative = attributeModifierIsNegative(
                command.action.operation,
                command.amount);
        });
    return { outcome, modifier };
}

BattleDamageModifierInstance makeDamageModifier(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyDamageEffectCommand& command,
    int frame)
{
    return {
        .sequence = runtime.nextDamageSequence++,
        .binding = metadata.binding,
        .ruleId = metadata.ruleId,
        .actionOrder = metadata.actionOrder,
        .targetUnitId = metadata.targetUnitId,
        .eventSourceUnitId = command.action.stackScope == EffectStackScope::EventSource
            ? metadata.eventSourceUnitId
            : -1,
        .perspective = command.action.perspective,
        .stage = command.action.stage,
        .channel = command.action.channel,
        .operation = command.action.operation,
        .amount = command.amount,
        .appliedFrame = frame,
        .expiresFrameExclusive = modifierExpiry(frame, command.action.durationFrames),
        .stack = command.action.stack,
        .stackLimit = command.action.stackLimit,
        .stackCount = 1,
        .negative = damageModifierIsNegative(
            command.action.perspective,
            command.action.operation,
            command.amount),
    };
}

BattleDamageModifierEffectResult applyDamageModifier(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyDamageEffectCommand& command,
    int frame)
{
    const auto expiry = modifierExpiry(frame, command.action.durationFrames);
    const auto [outcome, modifier] = applyStackPolicy(
        runtime.damageModifiers,
        command.action.stack,
        [&](const auto& candidate)
        {
            return sameDamageStackDomain(candidate, metadata, command);
        },
        [&]
        {
            return makeDamageModifier(runtime, metadata, command, frame);
        },
        [&](auto& current)
        {
            current.operation = command.action.operation;
            current.amount = command.amount;
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
            current.stackLimit = command.action.stackLimit;
            current.negative = damageModifierIsNegative(
                command.action.perspective,
                command.action.operation,
                command.amount);
        },
        [](const auto& current)
        {
            return damageStrength(current.operation, current.amount);
        },
        damageStrength(command.action.operation, command.amount),
        [&](const auto& current)
        {
            return expiryLater(expiry, current.expiresFrameExclusive);
        },
        [&](auto& current)
        {
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
        },
        [&](auto& current)
        {
            assert(command.action.stackLimit);
            current.stackCount = std::min(current.stackCount + 1, *command.action.stackLimit);
            current.operation = command.action.operation;
            current.amount = command.amount;
            current.appliedFrame = frame;
            current.expiresFrameExclusive = expiry;
            current.stackLimit = command.action.stackLimit;
            current.negative = damageModifierIsNegative(
                command.action.perspective,
                command.action.operation,
                command.amount);
        });
    return { outcome, modifier };
}

template<class Command, class Apply>
auto applyProtectedPersistentModifier(
    BattleRuntimeState& state,
    const EffectCommandMetadata& metadata,
    const Command& command,
    int frame,
    bool negative,
    Apply&& apply)
{
    using Result = std::invoke_result_t<Apply, const Command&>;
    const int requestedDurationFrames = command.action.durationFrames;
    int appliedDurationFrames = requestedDurationFrames;
    int statusShieldAbsorbed{};
    Command effective = command;
    if (negative)
    {
        auto& target = state.units.require(metadata.targetUnitId);
        const auto protection = BattleStatusSystem(state.status.config)
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
        effective.action.durationFrames = appliedDurationFrames;
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
    int frame)
{
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
        record.core.shield = std::max(
            0,
            saturatedInt(static_cast<std::int64_t>(record.core.shield) + delta));
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
    const BattleEffectCommandContext& context)
{
    BattleHealRequest request;
    request.sourceUnitId = metadata.binding.ownerUnitId;
    request.targetUnitId = metadata.targetUnitId;
    request.source = metadata.binding;
    if (context.cast)
    {
        request.castId = context.cast->castId.value();
    }
    switch (command.action.healKind)
    {
    case EffectHealKind::Direct: request.kind = BattleHealKind::Direct; break;
    case EffectHealKind::Team: request.kind = BattleHealKind::Team; break;
    case EffectHealKind::Aura: request.kind = BattleHealKind::Aura; break;
    case EffectHealKind::OnHit: request.kind = BattleHealKind::OnHit; break;
    case EffectHealKind::KillReward: request.kind = BattleHealKind::KillReward; break;
    case EffectHealKind::DeathMedical: request.kind = BattleHealKind::DeathMedical; break;
    case EffectHealKind::Rescue: request.kind = BattleHealKind::Rescue; break;
    case EffectHealKind::Regeneration: request.kind = BattleHealKind::Regeneration; break;
    case EffectHealKind::Lifesteal: request.kind = BattleHealKind::Lifesteal; break;
    }
    request.amount = fixedHealAmount(command.amount);
    request.sourcePolicy = command.action.healSourcePolicy == EffectHealSourcePolicy::AllowDead
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
        const int available = resourceValue(state.units.require(unitId), command.action.resource);
        const int removed = std::min(requested, std::max(0, available));
        result.deltas.push_back(applyResourceDelta(
            state,
            unitId,
            command.action.resource,
            -removed,
            false));
        return removed;
    };
    const auto addTo = [&](int unitId, int amount, bool restore)
    {
        result.deltas.push_back(applyResourceDelta(
            state,
            unitId,
            command.action.resource,
            amount,
            restore));
    };

    switch (command.action.kind)
    {
    case ResourceChangeKind::Restore:
        addTo(metadata.targetUnitId, command.amount, true);
        break;
    case ResourceChangeKind::Grant:
        addTo(metadata.targetUnitId, command.amount, false);
        break;
    case ResourceChangeKind::Remove:
        removeFrom(metadata.targetUnitId, command.amount);
        break;
    case ResourceChangeKind::Drain:
    {
        const int removed = removeFrom(metadata.targetUnitId, command.amount);
        addTo(metadata.binding.ownerUnitId, removed, true);
        break;
    }
    case ResourceChangeKind::Transfer:
    {
        assert(command.transferDestinationUnitIds.size() == 1);
        const int removed = removeFrom(metadata.targetUnitId, command.amount);
        addTo(command.transferDestinationUnitIds.front(), removed, false);
        break;
    }
    case ResourceChangeKind::RefreshToAtLeast:
    {
        const int before = resourceValue(target, command.action.resource);
        addTo(metadata.targetUnitId, std::max(0, command.amount - before), false);
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
    BattleRuntimeState& state,
    const EffectCommandMetadata& metadata,
    const ApplyStatusEffectCommand& command,
    const BattleEffectCommandContext& context)
{
    auto& record = state.units.require(metadata.targetUnitId);
    record.status.effects.freezeReductionPct = BattleEffectCommandSystem::queryAttribute(
        state,
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
        state.status.config,
        record.core.shield > 0);
    record.writeStatusDamageResult(result.target);
    return { std::move(result) };
}

BattleStatusConsumeEffectResult consumeStatus(
    BattleRuntimeState& state,
    const EffectCommandMetadata& metadata,
    const ConsumeStatusEffectCommand& command,
    const BattleEffectCommandContext& context)
{
    auto& record = state.units.require(metadata.targetUnitId);
    BattleStatusConsumeRequest request;
    request.kind = command.action.status;
    request.stacks = command.action.quantity;
    if (command.action.source == StatusSourceMatch::EffectOwner)
    {
        request.sourceUnitId = metadata.binding.ownerUnitId;
    }

    auto statusConfig = state.status.config;
    statusConfig.frame = context.frame;
    BattleStatusSystem statusSystem(statusConfig);
    auto consumed = statusSystem.consume(record.statusDamageState(), request);
    record.writeStatusDamageResult(consumed.target);

    std::optional<BattleStatusApplyResult> depleted;
    if (consumed.consumed
        && consumed.remainingStacks == 0
        && command.whenDepleted)
    {
        auto applied = applyStatus(
            state,
            metadata,
            *command.whenDepleted,
            context);
        depleted = std::move(applied.status);
    }
    return {
        .status = std::move(consumed),
        .depletedStatus = std::move(depleted),
    };
}

BattleStatusRemoveEffectResult removeStatus(
    BattleRuntimeState& state,
    const EffectCommandMetadata& metadata,
    const RemoveStatusEffectCommand& command,
    int frame)
{
    assert(frame >= 0);
    auto& record = state.units.require(metadata.targetUnitId);
    BattleStatusRemoveRequest request;
    request.statuses = command.action.statuses;
    request.negativeOnly = command.action.negativeOnly;
    request.controlOnly = command.action.controlOnly;
    request.clearCurrentActionStagger = command.action.clearCurrentActionStagger;
    request.count = command.action.count;
    request.order = command.action.order;

    BattleStatusSystem statusSystem(state.status.config);
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

    const auto removeAllModifiers = [&]
    {
        auto& attributes = state.effectCommands.attributeModifiers;
        for (auto it = attributes.begin(); it != attributes.end();)
        {
            if (it->targetUnitId != metadata.targetUnitId || !it->negative)
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
            if (it->targetUnitId != metadata.targetUnitId || !it->negative)
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
        std::uint64_t sequence{};
        int remainingFrames{};
    };
    std::vector<Candidate> candidates;
    const auto statusSnapshot = statusSystem.snapshot(record.statusDamageState());
    for (const auto& status : statusSnapshot.statuses)
    {
        if (isNegativeBattleStatus(status.kind))
        {
            candidates.push_back({
                .storage = CandidateStorage::Status,
                .statusKind = status.kind,
                .sequence = status.appliedSequence,
                .remainingFrames = status.remainingFrames,
            });
        }
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
        if (modifier.targetUnitId == metadata.targetUnitId && modifier.negative)
        {
            candidates.push_back({
                .storage = CandidateStorage::AttributeModifier,
                .sequence = modifier.sequence,
                .remainingFrames = remainingModifierFrames(
                    modifier.expiresFrameExclusive),
            });
        }
    }
    for (const auto& modifier : state.effectCommands.damageModifiers)
    {
        if (modifier.targetUnitId == metadata.targetUnitId && modifier.negative)
        {
            candidates.push_back({
                .storage = CandidateStorage::DamageModifier,
                .sequence = modifier.sequence,
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
            if (lhs.sequence != rhs.sequence)
            {
                return lhs.sequence < rhs.sequence;
            }
            break;
        }
        case StatusRemovalOrder::Oldest:
            if (lhs.sequence != rhs.sequence)
            {
                return lhs.sequence < rhs.sequence;
            }
            break;
        case StatusRemovalOrder::Newest:
            if (lhs.sequence != rhs.sequence)
            {
                return lhs.sequence > rhs.sequence;
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
                candidate.sequence,
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
                candidate.sequence,
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
    const EffectCommandMetadata& metadata,
    const CreateAreaEffectCommand& command,
    const BattleEffectCommandContext& context)
{
    assert(command.modifierAmounts.size() == command.action.modifiers.size());
    BattleAreaCreateRequest request;
    request.source = metadata.binding;
    request.ruleId = metadata.ruleId;
    request.targetTeamDomain = context.areaTargetTeamDomain;
    request.geometry = {
        command.action.shape,
        command.action.radiusTiles,
        command.action.squareSideTiles,
    };
    switch (command.action.anchor)
    {
    case AreaAnchor::HitPosition:
        assert(context.effectPosition);
        request.anchor = {
            BattleAreaAnchorKind::FixedWorldPosition,
            *context.effectPosition,
            -1,
        };
        break;
    case AreaAnchor::FollowSourceUnit:
        request.anchor = {
            BattleAreaAnchorKind::FollowSourceUnit,
            {},
            metadata.binding.ownerUnitId,
        };
        break;
    }
    request.currentFrame = context.frame;
    request.durationFrames = command.action.durationFrames;
    request.sourceDeath = command.action.sourceDeath;
    request.merge = command.action.merge;
    request.modifiers = command.action.modifiers;
    for (std::size_t i = 0; i < request.modifiers.size(); ++i)
    {
        if (request.modifiers[i].kind != AreaModifierKind::Attribute)
        {
            continue;
        }
        request.modifiers[i].amount = {};
        request.modifiers[i].amount.flat = command.modifierAmounts[i];
    }
    return { BattleAreaEffectSystem::create(state.areas, std::move(request)) };
}

BattleEffectDamageRequestOutput makeDamageRequest(
    const EffectCommandMetadata& metadata,
    const DealDamageEffectCommand& command,
    const BattleEffectCommandContext& context)
{
    assert(command.transactionCount > 0);
    BattleDamageRequest request;
    request.attackerUnitId = metadata.binding.ownerUnitId;
    request.defenderUnitId = metadata.targetUnitId;
    request.baseDamage = command.amount;
    request.damageKind = command.action.kind;
    request.preResolvedDamage = !command.action.appliesDamageModifiers;
    request.triggersDefenseEffects = command.action.triggersHurtInvincibility;
    // 「處決」動作本身代表直接斬殺；生命門檻等規則條件已在命令產生前篩選。
    request.canExecute = command.action.kind == BattleDamageKind::Execute;
    request.executeThresholdPct = request.canExecute ? 100 : 0;
    request.acceptedHit = false;

    return {
        .request = request,
        .action = command.action,
        .source = metadata.binding,
        .ruleId = metadata.ruleId,
        .provenance = context.attack,
        .transactionCount = command.transactionCount,
        .eventSourceUnitId = metadata.eventSourceUnitId,
    };
}

BattleEffectReductionValue reduceCommand(
    BattleRuntimeState& state,
    const EffectCommand& effectCommand,
    const BattleEffectCommandContext& context)
{
    return std::visit(Overloaded{
        [&](const ModifyAttributeEffectCommand& command) -> BattleEffectReductionValue
        {
            return applyProtectedPersistentModifier(
                state,
                effectCommand.metadata,
                command,
                context.frame,
                attributeModifierIsNegative(
                    command.action.operation,
                    command.amount),
                [&](const auto& effective)
                {
                    return applyAttribute(
                        state.effectCommands,
                        effectCommand.metadata,
                        effective,
                        context.frame);
                });
        },
        [&](const ModifyDamageEffectCommand& command) -> BattleEffectReductionValue
        {
            if (command.action.durationFrames > 0
                || command.action.stack != EffectStackPolicy::Independent
                || effectCommand.metadata.event == EffectEvent::BattleInitialized)
            {
                return applyProtectedPersistentModifier(
                    state,
                    effectCommand.metadata,
                    command,
                    context.frame,
                    damageModifierIsNegative(
                        command.action.perspective,
                        command.action.operation,
                        command.amount),
                    [&](const auto& effective)
                    {
                        return applyDamageModifier(
                            state.effectCommands,
                            effectCommand.metadata,
                            effective,
                            context.frame);
                    });
            }
            return BattleRoutedEffectCommand<ModifyDamageEffectCommand>{ command };
        },
        [&](const ChangeResourceEffectCommand& command) -> BattleEffectReductionValue
        {
            assert(command.amount >= 0);
            if (command.action.resource == BattleResource::Hp)
            {
                if (command.action.kind == ResourceChangeKind::Restore
                    || command.action.kind == ResourceChangeKind::Grant)
                {
                    return commitHeal(state, effectCommand.metadata, command, context);
                }
                return BattleDeferredHpResourceOutput{ command };
            }
            return changeDirectResource(state, effectCommand.metadata, command);
        },
        [&](const ModifyHealTransactionEffectCommand& command) -> BattleEffectReductionValue
        {
            return BattleRoutedEffectCommand<ModifyHealTransactionEffectCommand>{ command };
        },
        [&](const ApplyStatusEffectCommand& command) -> BattleEffectReductionValue
        {
            return applyStatus(state, effectCommand.metadata, command, context);
        },
        [&](const ConsumeStatusEffectCommand& command) -> BattleEffectReductionValue
        {
            return consumeStatus(state, effectCommand.metadata, command, context);
        },
        [&](const RemoveStatusEffectCommand& command) -> BattleEffectReductionValue
        {
            return removeStatus(
                state,
                effectCommand.metadata,
                command,
                context.frame);
        },
        [&](const DealDamageEffectCommand& command) -> BattleEffectReductionValue
        {
            assert(command.amount >= 0);
            return makeDamageRequest(effectCommand.metadata, command, context);
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
            return createArea(state, effectCommand.metadata, command, context);
        },
        [&](const ModifyCastEffectCommand& command) -> BattleEffectReductionValue
        {
            return BattleRoutedEffectCommand<ModifyCastEffectCommand>{ command };
        },
        [&](const StateMachineEffectCommand& command) -> BattleEffectReductionValue
        {
            if (const auto* start = std::get_if<StartDamageAbsorptionAction>(&command.action))
            {
                return applyDamageAbsorption(
                    state,
                    effectCommand.metadata,
                    *start,
                    context.frame);
            }
            return BattleRoutedEffectCommand<StateMachineEffectCommand>{ command };
        },
    }, effectCommand.value);
}

}  // namespace

BattleEffectCommandReduction BattleEffectCommandSystem::reduce(
    BattleRuntimeState& state,
    std::span<const EffectCommand> commands,
    const BattleEffectCommandContext& context) const
{
    assert(context.frame >= 0);
    removeExpiredAttributeModifiers(state, context.frame);
    removeExpiredDamageModifiers(state, context.frame);

    BattleEffectCommandReduction result;
    result.entries.reserve(commands.size());
    for (std::size_t i = 0; i < commands.size(); ++i)
    {
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
    const EffectCommand& command,
    const BattleEffectCommandContext& context) const
{
    return reduce(state, std::span{ &command, std::size_t{ 1 } }, context);
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
    int frame)
{
    assert(frame >= 0);
    return applyAttribute(runtime, metadata, command, frame);
}

BattleDamageModifierEffectResult BattleEffectCommandSystem::applyPersistentDamageModifier(
    BattleEffectCommandRuntimeState& runtime,
    const EffectCommandMetadata& metadata,
    const ModifyDamageEffectCommand& command,
    int frame)
{
    assert(frame >= 0);
    return applyDamageModifier(runtime, metadata, command, frame);
}

BattleStatusApplyResult BattleEffectCommandSystem::applyStatusCommand(
    BattleStatusUnitState target,
    const EffectCommandMetadata& metadata,
    const ApplyStatusEffectCommand& command,
    const BattleEffectCommandContext& context,
    BattleStatusSystemConfig statusConfig,
    bool targetHasShield)
{
    BattleStatusApplyRequest request;
    request.kind = command.action.status;
    request.sourceUnitId = metadata.binding.ownerUnitId;
    request.durationFrames = command.evaluatedDurationFrames.value_or(
        command.action.durationFrames);
    const auto quantity = lowerStatusQuantity(command.action);
    request.stacks = quantity.stacks;
    request.potency = command.potency;
    request.secondaryPotency = command.secondaryPotency;
    request.origin = BattleStatusEffectOrigin{
        metadata.binding,
        metadata.ruleId,
        metadata.ruleOrder,
    };
    request.stack = lowerStatusReapplication(command.action);
    request.stackLimit = quantity.stackLimit;
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
    int cloneTeam)
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
    int comboId)
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
            if (!isAntiComboCoreAttribute(attribute->action.attribute))
            {
                continue;
            }
            auto& total = incomingTotals[attribute->action.attribute];
            switch (attribute->action.operation)
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
            result.commands.push_back({ transferred.metadata, *resource });
            continue;
        }
        const auto* status = std::get_if<ApplyStatusEffectCommand>(
            &transferred.value);
        assert(status);
        result.commands.push_back({ transferred.metadata, *status });
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
                    && !isAntiComboCoreAttribute(attribute->action.attribute)
                    && transferred.metadata.ruleId == sourceModifier.ruleId
                    && transferred.metadata.actionOrder == sourceModifier.actionOrder;
            });
        if (!initializedModifier)
        {
            continue;
        }

        auto transferred = sourceModifier;
        transferred.sequence = runtime.nextAttributeSequence++;
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
