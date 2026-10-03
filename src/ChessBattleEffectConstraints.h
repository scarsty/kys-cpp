#pragma once

#include "ChessBattleEffectTypes.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

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
    EventSource = 1u << 14,
};

using EffectEventCapabilities = std::uint32_t;

enum class StatusNumberBindingPhase
{
    Literal,
    ApplicationBound,
    EventLive,
    ContributionLive,
};

enum class EffectNumberAuthoringScope
{
    Any,
    StatusBehaviorOnly,
};

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
        return C::CastProvenance | C::OriginalAttackTarget | C::PreCastResources
            | C::EventSource;
    case EffectEvent::AttackCommitted:
    case EffectEvent::UltimateCommitted:
        return C::CastProvenance | C::OriginalAttackTarget | C::PreCastResources
            | C::EventSource;
    case EffectEvent::AttackSpawned:
        return C::CastProvenance | C::AttackContext | C::OriginalAttackTarget
            | C::EventSource;
    case EffectEvent::MainProjectileBeforeDamage:
    case EffectEvent::HitBeforeDamage:
        return C::Hit | C::CastProvenance | C::AttackContext
            | C::TransactionTarget | C::OriginalAttackTarget | C::CurrentShield
            | C::EventSource;
    case EffectEvent::DamageResolved:
        return C::Damage | C::DamageOrigin | C::CastProvenance | C::AttackContext
            | C::TransactionTarget | C::CurrentShield | C::CurrentCooldown
            | C::EventSource;
    case EffectEvent::HealAttempted:
    case EffectEvent::HealApplied:
        return C::Heal | C::TransactionTarget | C::EventSource;
    case EffectEvent::CastContinuation:
    case EffectEvent::CastSettled:
        return C::CastAggregate | C::CastProvenance | C::OriginalAttackTarget
            | C::PreCastResources | C::EventSource;
    case EffectEvent::ShieldBroken:
        return C::DamageOrigin | C::CastProvenance | C::AttackContext
            | C::TransactionTarget | C::CurrentShield;
    case EffectEvent::UnitDied:
    case EffectEvent::AllyDied:
        return C::DamageOrigin | C::CastProvenance | C::AttackContext
            | C::TransactionTarget | C::Death | C::EventSource;
    case EffectEvent::StatusPersistent:
        return effectCapability(C::None);
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

enum class ApplicationBaseBindingKind
{
    None,
    SourceStar,
    SourceAttack,
    SourceMaxHp,
    SourceMissingHpRatio,
    SourceCurrentMpRatio,
    SourceStatusQuantity,
    StoredStateValue,
    ApplicationTargetMaxHp,
};

enum class EffectNumberEvaluationKind
{
    Constant,
    SourceStar,
    SourceAttack,
    SourceMaxHp,
    SourceMissingHpRatio,
    SourceCurrentMpRatio,
    TargetMaxHp,
    TargetCurrentHp,
    TargetCurrentShield,
    TargetCurrentCooldown,
    FinalHpDamage,
    SourceStatusQuantity,
    CurrentContributionQuantity,
    StoredStateValue,
    ApplicationTargetMaxHp,
    BoundRatio,
    Count,
};

struct EffectNumberBaseCatalogEntry
{
    EffectNumberBase base{};
    std::string_view authorLabel;
    StatusNumberBindingPhase bindingPhase{};
    ApplicationBaseBindingKind applicationBinding{};
    EffectNumberEvaluationKind evaluation{};
    EffectEventCapabilities requiredEventCapabilities{};
    EffectNumberAuthoringScope authoringScope = EffectNumberAuthoringScope::Any;
};

inline constexpr std::array effectNumberBaseCatalog{
    EffectNumberBaseCatalogEntry{ EffectNumberBase::Constant, "固定值", StatusNumberBindingPhase::Literal, ApplicationBaseBindingKind::None, EffectNumberEvaluationKind::Constant },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::SourceStar, "來源星級", StatusNumberBindingPhase::ApplicationBound, ApplicationBaseBindingKind::SourceStar, EffectNumberEvaluationKind::SourceStar },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::SourceAttack, "來源攻擊", StatusNumberBindingPhase::ApplicationBound, ApplicationBaseBindingKind::SourceAttack, EffectNumberEvaluationKind::SourceAttack },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::SourceMaxHp, "來源最大生命", StatusNumberBindingPhase::ApplicationBound, ApplicationBaseBindingKind::SourceMaxHp, EffectNumberEvaluationKind::SourceMaxHp },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::SourceMissingHpRatio, "來源已損生命比例", StatusNumberBindingPhase::ApplicationBound, ApplicationBaseBindingKind::SourceMissingHpRatio, EffectNumberEvaluationKind::SourceMissingHpRatio },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::SourceCurrentMpRatio, "來源目前內力比例", StatusNumberBindingPhase::ApplicationBound, ApplicationBaseBindingKind::SourceCurrentMpRatio, EffectNumberEvaluationKind::SourceCurrentMpRatio },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::TargetMaxHp, "目標最大生命", StatusNumberBindingPhase::EventLive, ApplicationBaseBindingKind::None, EffectNumberEvaluationKind::TargetMaxHp },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::TargetCurrentHp, "目標目前生命", StatusNumberBindingPhase::EventLive, ApplicationBaseBindingKind::None, EffectNumberEvaluationKind::TargetCurrentHp },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::TargetCurrentShield, "目標目前護盾", StatusNumberBindingPhase::EventLive, ApplicationBaseBindingKind::None, EffectNumberEvaluationKind::TargetCurrentShield, effectCapability(EffectEventCapability::CurrentShield) },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::TargetCurrentCooldown, "目標目前冷卻", StatusNumberBindingPhase::EventLive, ApplicationBaseBindingKind::None, EffectNumberEvaluationKind::TargetCurrentCooldown, effectCapability(EffectEventCapability::CurrentCooldown) },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::FinalHpDamage, "實際生命傷害", StatusNumberBindingPhase::EventLive, ApplicationBaseBindingKind::None, EffectNumberEvaluationKind::FinalHpDamage, effectCapability(EffectEventCapability::Damage) },
    // 來源狀態數量有自己的「來源狀態數量」作者欄位；已綁定比例只存在於 runtime。
    EffectNumberBaseCatalogEntry{ EffectNumberBase::SourceStatusQuantity, {}, StatusNumberBindingPhase::ApplicationBound, ApplicationBaseBindingKind::SourceStatusQuantity, EffectNumberEvaluationKind::SourceStatusQuantity },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::CurrentContributionQuantity, "此狀態貢獻數量", StatusNumberBindingPhase::ContributionLive, ApplicationBaseBindingKind::None, EffectNumberEvaluationKind::CurrentContributionQuantity, {}, EffectNumberAuthoringScope::StatusBehaviorOnly },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::StoredStateValue, "狀態槽值", StatusNumberBindingPhase::ApplicationBound, ApplicationBaseBindingKind::StoredStateValue, EffectNumberEvaluationKind::StoredStateValue },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::ApplicationTargetMaxHp, "套用目標最大生命", StatusNumberBindingPhase::ApplicationBound, ApplicationBaseBindingKind::ApplicationTargetMaxHp, EffectNumberEvaluationKind::ApplicationTargetMaxHp, {}, EffectNumberAuthoringScope::StatusBehaviorOnly },
    EffectNumberBaseCatalogEntry{ EffectNumberBase::BoundRatio, {}, StatusNumberBindingPhase::Literal, ApplicationBaseBindingKind::None, EffectNumberEvaluationKind::BoundRatio },
};

static_assert(effectNumberBaseCatalog.size()
    == static_cast<std::size_t>(EffectNumberBase::Count));
static_assert(effectNumberBaseCatalog.size()
    == static_cast<std::size_t>(EffectNumberEvaluationKind::Count));
static_assert([]
{
    std::array<bool, static_cast<std::size_t>(EffectNumberEvaluationKind::Count)> evaluations{};
    for (std::size_t index = 0; index < effectNumberBaseCatalog.size(); ++index)
    {
        const auto& entry = effectNumberBaseCatalog[index];
        if (static_cast<std::size_t>(entry.base) != index)
            return false;
        if ((entry.bindingPhase == StatusNumberBindingPhase::ApplicationBound)
                != (entry.applicationBinding != ApplicationBaseBindingKind::None))
            return false;
        const auto evaluation = static_cast<std::size_t>(entry.evaluation);
        if (evaluation >= evaluations.size() || evaluations[evaluation]) return false;
        evaluations[evaluation] = true;
    }
    for (const bool present : evaluations)
        if (!present) return false;
    return true;
}());

constexpr ApplicationBaseBindingKind applicationBaseBindingKind(
    EffectNumberBase base)
{
    const auto index = static_cast<std::size_t>(base);
    assert(index < effectNumberBaseCatalog.size());
    return effectNumberBaseCatalog[index].applicationBinding;
}

constexpr EffectNumberEvaluationKind effectNumberEvaluationKind(
    EffectNumberBase base)
{
    const auto index = static_cast<std::size_t>(base);
    assert(index < effectNumberBaseCatalog.size());
    return effectNumberBaseCatalog[index].evaluation;
}

constexpr const EffectNumberBaseCatalogEntry& effectNumberBaseCatalogEntry(
    EffectNumberBase base)
{
    const auto index = static_cast<std::size_t>(base);
    assert(index < effectNumberBaseCatalog.size());
    return effectNumberBaseCatalog[index];
}

constexpr StatusNumberBindingPhase statusNumberBindingPhase(
    EffectNumberBase base)
{
    return effectNumberBaseCatalogEntry(base).bindingPhase;
}

constexpr bool effectNumberBaseIsAuthorable(EffectNumberBase base)
{
    return !effectNumberBaseCatalogEntry(base).authorLabel.empty();
}

constexpr bool effectNumberBaseAllowedInAuthoringContext(
    EffectNumberBase base,
    bool statusBehavior)
{
    return effectNumberBaseCatalogEntry(base).authoringScope
            == EffectNumberAuthoringScope::Any
        || statusBehavior;
}

constexpr std::size_t effectNumberAuthorableBaseCount()
{
    std::size_t result{};
    for (const auto& entry : effectNumberBaseCatalog)
        if (!entry.authorLabel.empty()) ++result;
    return result;
}

constexpr bool effectObservationScopeUsesStatusContext(
    EffectObservationScope scope)
{
    switch (scope)
    {
    case EffectObservationScope::Owner:
    case EffectObservationScope::OwnerTeamEventSource:
    case EffectObservationScope::EventTarget:
    case EffectObservationScope::ComboMemberEventSource:
        return false;
    case EffectObservationScope::StatusHolderEventSource:
    case EffectObservationScope::StatusHolderEventTarget:
    case EffectObservationScope::StatusSourceEventSource:
    case EffectObservationScope::SourceOwnerTeamEventSource:
        return true;
    }
    return false;
}

// Observation legality is intentionally closed and shared by runtime
// validation and generated authoring schemas.  A status-context scope can
// never leak into a top-level rule (or vice versa), and scopes which name an
// event source/target require that payload capability.
constexpr bool effectObservationScopeAllowedAtEvent(
    EffectObservationScope scope,
    EffectEvent event,
    bool statusBehavior)
{
    if (effectObservationScopeUsesStatusContext(scope) != statusBehavior)
        return false;
    if (event == EffectEvent::StatusPersistent)
        return statusBehavior
            && scope == EffectObservationScope::StatusHolderEventSource;
    if (event == EffectEvent::FrameAdvanced)
        return statusBehavior
            ? scope == EffectObservationScope::StatusHolderEventSource
            : scope == EffectObservationScope::Owner;

    switch (scope)
    {
    case EffectObservationScope::Owner:
        return true;
    case EffectObservationScope::ComboMemberEventSource:
        return event == EffectEvent::AttackCommitted;
    case EffectObservationScope::OwnerTeamEventSource:
        return event == EffectEvent::AttackSpawned
            || event == EffectEvent::MainProjectileBeforeDamage
            || event == EffectEvent::HitBeforeDamage;
    case EffectObservationScope::EventTarget:
    case EffectObservationScope::StatusHolderEventTarget:
        return effectEventHas(event, EffectEventCapability::TransactionTarget);
    case EffectObservationScope::StatusHolderEventSource:
    case EffectObservationScope::StatusSourceEventSource:
    case EffectObservationScope::SourceOwnerTeamEventSource:
        return effectEventHas(event, EffectEventCapability::EventSource);
    }
    return false;
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

static_assert(std::variant_size_v<EffectActionValue> == 18);
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
    constexpr auto count = static_cast<std::uint32_t>(EffectEvent::StatusPersistent) + 1;
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
    case 0: // ModifyAttributeAction
        return allEffectEventsMask() & ~effectEventBit(EffectEvent::HealAttempted);
    case 1: // ModifyDamageAction
        return effectEventMask({
            EffectEvent::BattleInitialized,
            EffectEvent::UltimateCommitted,
            EffectEvent::MainProjectileBeforeDamage,
            EffectEvent::HitBeforeDamage,
            EffectEvent::DamageResolved,
            EffectEvent::StatusPersistent,
        });
    case 3: // ModifyHealTransactionAction
        return effectEventMask({
            EffectEvent::HealAttempted,
            EffectEvent::StatusPersistent,
        });
    case 7: // DealDamageAction
        return allEffectEventsMask()
            & ~effectEventMask({
                EffectEvent::CastPlanned,
                EffectEvent::AttackSpawned,
                EffectEvent::HealAttempted,
                EffectEvent::StatusPersistent,
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
    case 13: // ConsumeThisStatusAction
        return allEffectEventsMask() & ~effectEventBit(EffectEvent::StatusPersistent);
    case 14: // SuppressCurrentCastContactsAction
        return effectEventBit(EffectEvent::HitBeforeDamage);
    case 15: // MakeIncomingAttackMissAction
        return effectEventBit(EffectEvent::HitBeforeDamage);
    case 16: // BlockPositiveDamageAction
        return effectEventBit(EffectEvent::StatusPersistent);
    case 17: // ConditionalEffectAction
        return allEffectEventsMask() & ~effectEventBit(EffectEvent::StatusPersistent);
    default:
        return allEffectEventsMask()
            & ~effectEventBit(EffectEvent::HealAttempted)
            & ~effectEventBit(EffectEvent::StatusPersistent);
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
    return effectEventHasAll(
        event,
        effectNumberBaseCatalogEntry(base).requiredEventCapabilities);
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
    case 27: // EventTargetHasNegativeStatusCondition
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
