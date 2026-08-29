#pragma once

#include "ChessBattleEffectTypes.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace KysChess
{

enum class EffectEventCapability : std::uint32_t
{
    None = 0,
    Initialization = 1u << 0,
    Hit = 1u << 1,
    Damage = 1u << 2,
    DamageOrigin = 1u << 3,
    Heal = 1u << 4,
    CastAggregate = 1u << 5,
    CastProvenance = 1u << 6,
    AttackContext = 1u << 7,
    TransactionTarget = 1u << 8,
    OriginalAttackTarget = 1u << 9,
    Death = 1u << 10,
    PreCastResources = 1u << 11,
    CurrentShield = 1u << 12,
    CurrentCooldown = 1u << 13,
};

using EffectEventCapabilities = std::uint32_t;

constexpr EffectEventCapabilities effectCapability(EffectEventCapability capability)
{
    return static_cast<EffectEventCapabilities>(capability);
}

constexpr EffectEventCapabilities operator|(
    EffectEventCapability left,
    EffectEventCapability right)
{
    return effectCapability(left) | effectCapability(right);
}

constexpr EffectEventCapabilities operator|(
    EffectEventCapabilities left,
    EffectEventCapability right)
{
    return left | effectCapability(right);
}

constexpr EffectEventCapabilities effectEventCapabilities(EffectEvent event)
{
    using C = EffectEventCapability;
    switch (event)
    {
    case EffectEvent::BattleInitialized:
        return effectCapability(C::Initialization);
    case EffectEvent::FrameAdvanced:
        return effectCapability(C::CurrentCooldown);
    case EffectEvent::UltimateCooldownFinished:
        return effectCapability(C::None);
    case EffectEvent::CastPlanned:
        return C::CastProvenance | C::OriginalAttackTarget | C::PreCastResources;
    case EffectEvent::AttackCommitted:
    case EffectEvent::UltimateCommitted:
        return C::CastProvenance | C::OriginalAttackTarget | C::PreCastResources;
    case EffectEvent::AttackSpawned:
        return C::CastProvenance | C::AttackContext | C::OriginalAttackTarget;
    case EffectEvent::MainProjectileBeforeDamage:
    case EffectEvent::HitBeforeDamage:
        return C::Hit | C::CastProvenance | C::AttackContext
            | C::TransactionTarget | C::OriginalAttackTarget | C::CurrentShield;
    case EffectEvent::DamageResolved:
        return C::Damage | C::DamageOrigin | C::AttackContext
            | C::TransactionTarget | C::CurrentShield | C::CurrentCooldown;
    case EffectEvent::HealAttempted:
    case EffectEvent::HealApplied:
        return C::Heal | C::TransactionTarget;
    case EffectEvent::CastContinuation:
    case EffectEvent::CastSettled:
        return C::CastAggregate | C::CastProvenance | C::OriginalAttackTarget
            | C::PreCastResources;
    case EffectEvent::ShieldBroken:
        return C::DamageOrigin | C::TransactionTarget | C::CurrentShield;
    case EffectEvent::UnitDied:
    case EffectEvent::AllyDied:
        return C::DamageOrigin | C::TransactionTarget | C::Death;
    }
    return effectCapability(C::None);
}

constexpr bool effectEventHas(
    EffectEvent event,
    EffectEventCapability capability)
{
    return (effectEventCapabilities(event) & effectCapability(capability)) != 0;
}

constexpr bool effectEventHasAll(
    EffectEvent event,
    EffectEventCapabilities required)
{
    return (effectEventCapabilities(event) & required) == required;
}

struct EffectEventConstraint
{
    EffectEventCapabilities allOf{};
    EffectEventCapabilities anyOf{};

    constexpr bool allows(EffectEvent event) const
    {
        const auto available = effectEventCapabilities(event);
        return (available & allOf) == allOf
            && (anyOf == 0 || (available & anyOf) != 0);
    }
};

using EffectEventMask = std::uint32_t;

static_assert(std::variant_size_v<EffectActionValue> == 14);
static_assert(std::variant_size_v<StateMachineAction> == 12);

constexpr EffectEventMask effectEventBit(EffectEvent event)
{
    return 1u << static_cast<std::uint32_t>(event);
}

constexpr EffectEventMask effectEventMask(std::initializer_list<EffectEvent> events)
{
    EffectEventMask result{};
    for (const auto event : events) result |= effectEventBit(event);
    return result;
}

constexpr EffectEventMask allEffectEventsMask()
{
    constexpr auto count = static_cast<std::uint32_t>(EffectEvent::AllyDied) + 1;
    static_assert(count < 32);
    return (1u << count) - 1;
}

constexpr bool effectEventMaskAllows(EffectEventMask mask, EffectEvent event)
{
    return (mask & effectEventBit(event)) != 0;
}

// The indices intentionally match EffectActionValue. These are unconditional
// event restrictions only; payload-dependent composition rules remain in the
// runtime validator.
constexpr EffectEventMask effectActionEventMask(std::size_t variantIndex)
{
    switch (variantIndex)
    {
    case 1: // ModifyDamageAction
        return effectEventMask({
            EffectEvent::BattleInitialized,
            EffectEvent::UltimateCommitted,
            EffectEvent::MainProjectileBeforeDamage,
            EffectEvent::HitBeforeDamage,
            EffectEvent::DamageResolved,
        });
    case 3: // ModifyHealTransactionAction
        return effectEventBit(EffectEvent::HealAttempted);
    case 7: // DealDamageAction
        return allEffectEventsMask()
            & ~effectEventMask({
                EffectEvent::CastPlanned,
                EffectEvent::AttackSpawned,
                EffectEvent::HealAttempted,
            });
    case 8: // ModifyAttackAction
        return effectEventMask({
            EffectEvent::CastPlanned,
            EffectEvent::AttackCommitted,
            EffectEvent::UltimateCommitted,
            EffectEvent::AttackSpawned,
            EffectEvent::MainProjectileBeforeDamage,
            EffectEvent::CastContinuation,
        });
    case 9: // ForceMoveAction
        return effectEventMask({
            EffectEvent::MainProjectileBeforeDamage,
            EffectEvent::HitBeforeDamage,
        });
    case 10: // CreateAreaAction
        return effectEventMask({
            EffectEvent::AttackCommitted,
            EffectEvent::UltimateCommitted,
            EffectEvent::MainProjectileBeforeDamage,
            EffectEvent::HitBeforeDamage,
            EffectEvent::UnitDied,
        });
    case 11: // ModifyCastAction
        return effectEventMask({
            EffectEvent::FrameAdvanced,
            EffectEvent::CastPlanned,
            EffectEvent::CastContinuation,
            EffectEvent::ShieldBroken,
        });
    case 13: // ConditionalEffectAction
        return allEffectEventsMask();
    default:
        return allEffectEventsMask() & ~effectEventBit(EffectEvent::HealAttempted);
    }
}

constexpr bool effectActionAllowedAtEvent(
    std::size_t variantIndex,
    EffectEvent event)
{
    return effectEventMaskAllows(effectActionEventMask(variantIndex), event);
}

// The indices intentionally match StateMachineAction.
constexpr EffectEventMask effectStateMachineActionEventMask(std::size_t variantIndex)
{
    switch (variantIndex)
    {
    case 2: // RecordMaximumDamageAction
        return effectEventBit(EffectEvent::DamageResolved);
    case 6: // BorrowEffectRulesAction
        return effectEventBit(EffectEvent::CastPlanned);
    case 7: // CopyAttackDefinitionAction
        return effectEventBit(EffectEvent::UltimateCommitted);
    case 9:  // GenerateClonesAction
    case 10: // PreventDeathAction
    case 11: // ConfigureRescueRepositionAction
        return effectEventBit(EffectEvent::BattleInitialized);
    default:
        return effectActionEventMask(12);
    }
}

constexpr bool effectStateMachineActionAllowedAtEvent(
    std::size_t variantIndex,
    EffectEvent event)
{
    return effectEventMaskAllows(
        effectStateMachineActionEventMask(variantIndex), event);
}

constexpr bool effectSelectorKindAllowedAtEvent(
    EffectSelectorKind kind,
    EffectEvent event)
{
    using C = EffectEventCapability;
    switch (kind)
    {
    case EffectSelectorKind::TransactionTarget:
        return effectEventHas(event, C::TransactionTarget);
    case EffectSelectorKind::HitTarget:
        return effectEventHas(event, C::Hit) || effectEventHas(event, C::Damage);
    case EffectSelectorKind::OriginalAttackTarget:
        return effectEventHas(event, C::OriginalAttackTarget);
    default:
        return true;
    }
}

constexpr bool effectRequiredTargetAllowedAtEvent(
    EffectRequiredTarget target,
    EffectEvent event)
{
    switch (target)
    {
    case EffectRequiredTarget::Self:
    case EffectRequiredTarget::SourceUnit:
        return true;
    case EffectRequiredTarget::TransactionTarget:
        return effectSelectorKindAllowedAtEvent(EffectSelectorKind::TransactionTarget, event);
    case EffectRequiredTarget::HitTarget:
        return effectSelectorKindAllowedAtEvent(EffectSelectorKind::HitTarget, event);
    case EffectRequiredTarget::OriginalAttackTarget:
        return effectSelectorKindAllowedAtEvent(EffectSelectorKind::OriginalAttackTarget, event);
    }
    return false;
}

constexpr bool effectNumberBaseAllowedAtEvent(
    EffectNumberBase base,
    EffectEvent event)
{
    using C = EffectEventCapability;
    switch (base)
    {
    case EffectNumberBase::FinalHpDamage:
        return effectEventHas(event, C::Damage);
    case EffectNumberBase::TargetCurrentShield:
        return effectEventHas(event, C::CurrentShield);
    case EffectNumberBase::TargetCurrentCooldown:
        return effectEventHas(event, C::CurrentCooldown);
    default:
        return true;
    }
}

// The indices intentionally match EffectCondition. Keeping this table next to
// the event payload mapping gives runtime validation and authoring schemas one
// source of truth without moving author-only names into the runtime types.
constexpr EffectEventConstraint effectConditionConstraint(
    std::size_t variantIndex)
{
    using C = EffectEventCapability;
    switch (variantIndex)
    {
    case 0:  // IsUltimateCondition
    case 1:  // CastUsesEffectSourceMagicCondition
        return { effectCapability(C::CastProvenance), {} };
    case 2:  // IsMainProjectileCondition
    case 3:  // IsRootAttackCondition
    case 15: // AttackOrdinalEqualsCondition
        return { effectCapability(C::AttackContext), {} };
    case 7:  // TargetHpRatioAtMostCondition
        return { {}, C::Hit | C::Damage | C::Heal };
    case 14: // CastDistinctTargetCountAtLeastCondition
        return { effectCapability(C::CastAggregate), {} };
    case 16: // HealKindInCondition
        return { effectCapability(C::Heal), {} };
    case 17: // DamageOriginIsAttackCondition
        return { effectCapability(C::DamageOrigin), {} };
    case 18: // DamageKilledTargetCondition
    case 19: // AcceptedHitCondition
    case 21: // DamagePerspectiveCondition
        return { effectCapability(C::Damage), {} };
    case 20: // EventTargetBelongsToBoundSourceCondition
        return { effectCapability(C::Death), {} };
    case 22: // DamageKindInCondition
        return { {}, C::Hit | C::Damage };
    case 23: // TargetMpWasFullBeforeCastCondition
        return { effectCapability(C::PreCastResources), {} };
    default:
        return {};
    }
}

constexpr bool effectConditionAllowedAtEvent(
    std::size_t variantIndex,
    EffectEvent event)
{
    return effectConditionConstraint(variantIndex).allows(event);
}

}
