#include "ChessBattleEffectSemantics.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <type_traits>

namespace KysChess
{
namespace
{

constexpr std::array statusCatalog{
    StatusCatalogEntry{ BattleStatusKind::Poison, StatusQuantityModel::TriggerCharges,
        StatusEffectScope::PerTrigger, StatusDurationModel::RequiredPositive,
        StatusReapplicationModel::PoisonDamage },
    StatusCatalogEntry{ BattleStatusKind::Bleed, StatusQuantityModel::Layers,
        StatusEffectScope::PerLayer, StatusDurationModel::Forbidden,
        StatusReapplicationModel::Implicit },
    StatusCatalogEntry{ BattleStatusKind::Stun, StatusQuantityModel::None,
        StatusEffectScope::None, StatusDurationModel::RequiredPositive,
        StatusReapplicationModel::StunDuration },
    StatusCatalogEntry{ BattleStatusKind::MpBlocked, StatusQuantityModel::None,
        StatusEffectScope::None, StatusDurationModel::RequiredPositive,
        StatusReapplicationModel::KeepLongerDuration },
    StatusCatalogEntry{ BattleStatusKind::ColdPoison, StatusQuantityModel::None,
        StatusEffectScope::Persistent, StatusDurationModel::RequiredPositive,
        StatusReapplicationModel::RefreshDuration },
    StatusCatalogEntry{ BattleStatusKind::WitheredBone, StatusQuantityModel::None,
        StatusEffectScope::Persistent, StatusDurationModel::RequiredPositive,
        StatusReapplicationModel::RefreshDuration },
    StatusCatalogEntry{ BattleStatusKind::SevenStarMark, StatusQuantityModel::Marks,
        StatusEffectScope::None, StatusDurationModel::RequiredPositive,
        StatusReapplicationModel::Implicit },
    StatusCatalogEntry{ BattleStatusKind::NeutralizeForce, StatusQuantityModel::TriggerCharges,
        StatusEffectScope::PerTrigger, StatusDurationModel::Forbidden,
        StatusReapplicationModel::Implicit },
    StatusCatalogEntry{ BattleStatusKind::Blinded, StatusQuantityModel::TriggerCharges,
        StatusEffectScope::PerTrigger, StatusDurationModel::Forbidden,
        StatusReapplicationModel::Implicit },
    StatusCatalogEntry{ BattleStatusKind::NextAttackMiss, StatusQuantityModel::TriggerCharges,
        StatusEffectScope::PerTrigger, StatusDurationModel::RequiredPositive,
        StatusReapplicationModel::Implicit },
    StatusCatalogEntry{ BattleStatusKind::DamageBlockLayer, StatusQuantityModel::DamageBlockCharges,
        StatusEffectScope::PerTrigger, StatusDurationModel::Forbidden,
        StatusReapplicationModel::Implicit },
    StatusCatalogEntry{ BattleStatusKind::SingleHitCapLayer, StatusQuantityModel::TriggerCharges,
        StatusEffectScope::PerTrigger, StatusDurationModel::Forbidden,
        StatusReapplicationModel::Implicit },
    StatusCatalogEntry{ BattleStatusKind::BattleSpirit, StatusQuantityModel::Layers,
        StatusEffectScope::PerLayer, StatusDurationModel::Forbidden,
        StatusReapplicationModel::Implicit },
    StatusCatalogEntry{ BattleStatusKind::TrueQi, StatusQuantityModel::Layers,
        StatusEffectScope::PerLayer, StatusDurationModel::Forbidden,
        StatusReapplicationModel::Implicit },
    StatusCatalogEntry{ BattleStatusKind::PoisonExplosion, StatusQuantityModel::Layers,
        StatusEffectScope::PerLayerValue, StatusDurationModel::Forbidden,
        StatusReapplicationModel::Implicit },
    StatusCatalogEntry{ BattleStatusKind::Shadowless, StatusQuantityModel::None,
        StatusEffectScope::None, StatusDurationModel::RequiredPositive,
        StatusReapplicationModel::RefreshDuration },
    StatusCatalogEntry{ BattleStatusKind::NextAttackCritical, StatusQuantityModel::Internal,
        StatusEffectScope::RuntimeOwned, StatusDurationModel::RuntimeOwned,
        StatusReapplicationModel::Implicit, false },
};

static_assert([]
{
    for (const auto& status : statusCatalog)
    {
        const auto quantityOperations = statusQuantityOperations(status.quantity);
        const bool expectsQuantityOperations = status.quantity != StatusQuantityModel::None
            && status.quantity != StatusQuantityModel::Internal;
        if (quantityOperations.empty() == expectsQuantityOperations) return false;
        for (const auto operation : quantityOperations)
        {
            if (statusQuantityOperationCatalogEntry(operation).model != status.quantity)
                return false;
        }
        const bool hasFields = std::ranges::any_of(
            statusEffectFieldCatalog,
            [&](const auto& field) { return field.status == status.status; });
        const bool expectsFields = status.effectScope != StatusEffectScope::None
            && status.effectScope != StatusEffectScope::RuntimeOwned;
        if (hasFields != expectsFields) return false;
    }
    for (const auto& field : statusEffectFieldCatalog)
    {
        const auto status = std::ranges::find(
            statusCatalog, field.status, &StatusCatalogEntry::status);
        if (status == statusCatalog.end()
            || status->effectScope == StatusEffectScope::None
            || status->effectScope == StatusEffectScope::RuntimeOwned)
            return false;
    }
    return true;
}());

bool selectorIsExactly(const EffectSelector& selector, EffectSelectorKind kind)
{
    EffectSelector expected;
    expected.kind = kind;
    return selector == expected;
}

bool hasDefaultRuleQualifiers(const EffectRule& rule)
{
    return rule.chancePct == 100
        && rule.maxActivations == 0
        && rule.sharedCooldownFrames == 0
        && rule.intervalFrames == 0
        && rule.everyNthEvent == 0
        && !rule.activationLimit
        && !rule.repetitionCount;
}

bool isPlainConstantNumber(const EffectNumber& number)
{
    return number.base == EffectNumberBase::Constant
        && !number.multiplierBase
        && !number.status
        && !number.statusEffect
        && !number.stateSlot
        && number.percent == 0
        && number.rounding == EffectRounding::TowardZero
        && !number.minimum
        && !number.maximum;
}

bool matchesStatusQuantityFormula(
    const EffectNumber& number,
    BattleStatusKind status)
{
    return number.base == EffectNumberBase::SourceStatusQuantity
        && !number.multiplierBase
        && number.status == status
        && !number.statusEffect
        && !number.stateSlot
        && number.flat == 0
        && number.percent == 100
        && number.rounding == EffectRounding::TowardZero
        && number.minimum == 1
        && !number.maximum;
}

bool matchesStatusEffectValueFormula(
    const EffectNumber& number,
    BattleStatusKind status,
    StatusEffectValueKind effect)
{
    return number.base == EffectNumberBase::SourceStatusEffectValue
        && !number.multiplierBase
        && number.status == status
        && number.statusEffect == effect
        && !number.stateSlot
        && number.flat == 0
        && number.percent == 100
        && number.rounding == EffectRounding::TowardZero
        && !number.minimum
        && !number.maximum;
}

}

std::optional<int> effectiveConstantEffectNumberValue(const EffectNumber& number)
{
    if (number.base != EffectNumberBase::Constant || number.multiplierBase)
    {
        return std::nullopt;
    }

    int value = number.flat;
    if (number.minimum)
    {
        value = std::max(value, *number.minimum);
    }
    if (number.maximum)
    {
        value = std::min(value, *number.maximum);
    }
    return value;
}

const StatusCatalogEntry& statusCatalogEntry(BattleStatusKind status)
{
    const auto found = std::ranges::find(statusCatalog, status, &StatusCatalogEntry::status);
    assert(found != statusCatalog.end());
    return *found;
}

std::span<const StatusCatalogEntry> statusCatalogEntries()
{
    return statusCatalog;
}

bool statusReapplicationPolicyAllowed(
    BattleStatusKind status,
    StatusReapplicationPolicy policy)
{
    switch (statusCatalogEntry(status).reapplication)
    {
    case StatusReapplicationModel::Implicit:
        return policy == StatusReapplicationPolicy::Implicit;
    case StatusReapplicationModel::StunDuration:
        return policy == StatusReapplicationPolicy::ExtendDuration
            || policy == StatusReapplicationPolicy::KeepLongerDuration
            || policy == StatusReapplicationPolicy::ReplaceDuration;
    case StatusReapplicationModel::KeepLongerDuration:
        return policy == StatusReapplicationPolicy::KeepLongerDuration;
    case StatusReapplicationModel::RefreshDuration:
        return policy == StatusReapplicationPolicy::RefreshDuration;
    case StatusReapplicationModel::PoisonDamage:
        return policy == StatusReapplicationPolicy::KeepHigherDamage
            || policy == StatusReapplicationPolicy::ReplaceAndReset;
    }
    assert(false);
    return false;
}

bool statusReapplicationPolicyRequired(BattleStatusKind status)
{
    return statusCatalogEntry(status).reapplication
        != StatusReapplicationModel::Implicit;
}

bool matchesSevenStarLifecycleProducer(const EffectRule& rule)
{
    if (rule.event != EffectEvent::MainProjectileBeforeDamage
        || rule.observation != EffectObservationScope::Owner
        || rule.castMatch != EffectCastMatch::BoundMagic
        || !selectorIsExactly(rule.selector, EffectSelectorKind::HitTarget)
        || !rule.conditions.empty()
        || !hasDefaultRuleQualifiers(rule)
        || rule.actions.size() != 1)
        return false;

    const auto* applied = std::get_if<ApplyStatusAction>(
        &rule.actions.front().value);
    const auto* marks = applied
        ? std::get_if<SetStatusMarks>(&applied->quantity)
        : nullptr;
    return applied
        && applied->status == BattleStatusKind::SevenStarMark
        && applied->durationFrames > 0
        && !applied->duration
        && marks
        && marks->count > 0
        && applied->reapplication == StatusReapplicationPolicy::Implicit
        && std::holds_alternative<NoStatusEffects>(applied->effects);
}

bool matchesSevenStarLifecycleConsumer(const EffectRule& rule)
{
    if (rule.event != EffectEvent::HitBeforeDamage
        || rule.observation != EffectObservationScope::OwnerTeamEventSource
        || rule.castMatch != EffectCastMatch::BoundMagic
        || !selectorIsExactly(rule.selector, EffectSelectorKind::HitTarget)
        || !hasDefaultRuleQualifiers(rule)
        || rule.conditions.size() != 1
        || rule.actions.size() != 2)
        return false;

    const auto* required = std::get_if<TargetHasStateFromEffectOwnerCondition>(
        &rule.conditions.front());
    const auto* damage = std::get_if<ModifyDamageAction>(
        &rule.actions.front().value);
    const auto* consumed = std::get_if<ConsumeStatusAction>(
        &rule.actions.back().value);
    if (!required || required->state != BattleStatusKind::SevenStarMark
        || !damage
        || damage->perspective != DamageModifierPerspective::Outgoing
        || damage->stage != DamageModifierStage::BeforeDefense
        || damage->channel != DamageChannel::Skill
        || damage->operation != DamageModifierOperation::IgnoreDefensePercent
        || !isPlainConstantNumber(damage->amount)
        || damage->durationFrames != 0
        || damage->stack != EffectStackPolicy::Independent
        || damage->stackLimit
        || damage->stackScope != EffectStackScope::Shared
        || !consumed
        || consumed->status != BattleStatusKind::SevenStarMark
        || consumed->quantity <= 0
        || consumed->source != StatusSourceMatch::EffectOwner
        || !consumed->whenDepleted)
        return false;

    const auto& depleted = *consumed->whenDepleted;
    return depleted.status == BattleStatusKind::Stun
        && depleted.durationFrames > 0
        && !depleted.duration
        && std::holds_alternative<NoStatusQuantity>(depleted.quantity)
        && depleted.reapplication == StatusReapplicationPolicy::KeepLongerDuration
        && std::holds_alternative<NoStatusEffects>(depleted.effects);
}

bool matchesPoisonExplosionLifecycleProducer(const EffectRule& rule)
{
    if (rule.event != EffectEvent::AttackCommitted
        || rule.observation != EffectObservationScope::Owner
        || rule.castMatch != EffectCastMatch::BoundMagic
        || !selectorIsExactly(rule.selector, EffectSelectorKind::Self)
        || !rule.conditions.empty()
        || !hasDefaultRuleQualifiers(rule)
        || rule.actions.size() != 1)
        return false;

    const auto* applied = std::get_if<ApplyStatusAction>(
        &rule.actions.front().value);
    const auto* layers = applied
        ? std::get_if<AddStatusLayers>(&applied->quantity)
        : nullptr;
    return applied
        && applied->status == BattleStatusKind::PoisonExplosion
        && applied->durationFrames == 0
        && !applied->duration
        && layers
        && layers->count > 0
        && layers->limit >= layers->count
        && applied->reapplication == StatusReapplicationPolicy::Implicit
        && std::holds_alternative<PoisonExplosionStatusEffects>(applied->effects);
}

bool matchesPoisonExplosionLifecycleConsumer(const EffectRule& rule)
{
    if (rule.event != EffectEvent::UnitDied
        || rule.observation != EffectObservationScope::Owner
        || rule.castMatch != EffectCastMatch::BoundMagic
        || rule.conditions.size() != 1
        || rule.chancePct != 100
        || rule.maxActivations != 0
        || rule.sharedCooldownFrames != 0
        || rule.intervalFrames != 0
        || rule.everyNthEvent != 0
        || rule.activationLimit
        || !rule.repetitionCount
        || rule.actions.size() != 2)
        return false;

    EffectSelector expectedTarget;
    expectedTarget.kind = EffectSelectorKind::UnitsInRadius;
    expectedTarget.radiusTiles = rule.selector.radiusTiles;
    expectedTarget.team = EffectTeamFilter::Enemy;
    if (rule.selector.radiusTiles <= 0 || rule.selector != expectedTarget)
        return false;

    const auto* required = std::get_if<SourceHasStateCondition>(
        &rule.conditions.front());
    const auto* damage = std::get_if<DealDamageAction>(
        &rule.actions.front().value);
    const auto* applied = std::get_if<ApplyStatusAction>(
        &rule.actions.back().value);
    const auto* poisonQuantity = applied
        ? std::get_if<SetStatusTriggerCharges>(&applied->quantity)
        : nullptr;
    const auto* poisonEffects = applied
        ? std::get_if<PoisonStatusEffects>(&applied->effects)
        : nullptr;
    return required
        && required->state == BattleStatusKind::PoisonExplosion
        && matchesStatusQuantityFormula(
            *rule.repetitionCount, BattleStatusKind::PoisonExplosion)
        && damage
        && matchesStatusEffectValueFormula(
            damage->amount,
            BattleStatusKind::PoisonExplosion,
            StatusEffectValueKind::PoisonExplosionDeathPureDamage)
        && !damage->transactionCount
        && damage->kind == BattleDamageKind::Pure
        && damage->appliesDamageModifiers
        && damage->triggersHurtInvincibility
        && damage->area.kind == DamageAreaKind::SingleTarget
        && damage->area.radiusTiles == 0
        && damage->area.squareSideTiles == 0
        && damage->perCast.perTargetLimit == 0
        && !damage->areaProjectiles
        && applied
        && applied->status == BattleStatusKind::Poison
        && applied->durationFrames > 0
        && !applied->duration
        && poisonQuantity
        && poisonQuantity->count > 0
        && applied->reapplication == StatusReapplicationPolicy::ReplaceAndReset
        && poisonEffects
        && isPlainConstantNumber(poisonEffects->currentHpDamagePercent)
        && poisonEffects->sameEventMerge == PoisonSameEventMerge::None;
}

std::string_view statusEffectValueLabel(StatusEffectValueKind value)
{
    const auto found = std::ranges::find_if(statusEffectFieldCatalog, [&](const auto& field)
    {
        return field.value == value;
    });
    assert(found != statusEffectFieldCatalog.end());
    return found->label;
}

bool statusEffectValueBelongsToStatus(
    StatusEffectValueKind value,
    BattleStatusKind status)
{
    const auto found = std::ranges::find_if(statusEffectFieldCatalog, [&](const auto& field)
    {
        return field.value == value;
    });
    assert(found != statusEffectFieldCatalog.end());
    return found->status == status;
}

StatusRuntimeValueSlot statusEffectRuntimeValueSlot(StatusEffectValueKind value)
{
    const auto found = std::ranges::find_if(statusEffectFieldCatalog, [&](const auto& field)
    {
        return field.value == value;
    });
    assert(found != statusEffectFieldCatalog.end());
    assert(found->runtimeSlot);
    return *found->runtimeSlot;
}

std::optional<BattleStatusKind> statusEffectPayloadStatus(
    const StatusEffectPayload& payload)
{
    std::optional<BattleStatusKind> result;
    const auto record = [&](StatusEffectFieldId id)
    {
        const auto status = statusEffectFieldCatalogEntry(id).status;
        assert(!result || *result == status);
        result = status;
    };
    forEachStatusEffectField(
        payload,
        [&](StatusEffectFieldId id, const EffectNumber&) { record(id); },
        [&](StatusEffectFieldId id, bool) { record(id); });
    return result;
}

LoweredStatusQuantity lowerStatusQuantity(const ApplyStatusAction& action)
{
    return std::visit([&](const auto& quantity) -> LoweredStatusQuantity
    {
        using T = std::decay_t<decltype(quantity)>;
        if constexpr (std::is_same_v<T, NoStatusQuantity>)
            return { 1, EffectStackPolicy::Independent, std::nullopt };
        else if constexpr (std::is_same_v<T, AddStatusLayers>)
            return { quantity.count, EffectStackPolicy::AddStack, quantity.limit };
        else if constexpr (std::is_same_v<T, SetStatusMarks>)
            return { quantity.count, EffectStackPolicy::Replace, quantity.count };
        else if constexpr (std::is_same_v<T, AddDamageBlockCharges>)
            return { quantity.count, EffectStackPolicy::AddStack, quantity.limit };
        else if constexpr (std::is_same_v<T, SetDamageBlockCharges>)
            return { quantity.count, EffectStackPolicy::Replace, std::nullopt };
        else if constexpr (std::is_same_v<T, SetStatusTriggerCharges>)
            return { quantity.count, EffectStackPolicy::Replace, quantity.count };
    }, action.quantity);
}

EffectStackPolicy lowerStatusReapplication(const ApplyStatusAction& action)
{
    switch (action.reapplication)
    {
    case StatusReapplicationPolicy::Implicit:
        return lowerStatusQuantity(action).stack;
    case StatusReapplicationPolicy::ExtendDuration:
        return EffectStackPolicy::Independent;
    case StatusReapplicationPolicy::KeepLongerDuration:
        return EffectStackPolicy::Refresh;
    case StatusReapplicationPolicy::ReplaceDuration:
    case StatusReapplicationPolicy::ReplaceAndReset:
        return EffectStackPolicy::Replace;
    case StatusReapplicationPolicy::RefreshDuration:
        return EffectStackPolicy::Refresh;
    case StatusReapplicationPolicy::KeepHigherDamage:
        return EffectStackPolicy::KeepStrongest;
    }
    assert(false);
    return EffectStackPolicy::Independent;
}

bool battleAttributeUsesPercentagePoints(BattleAttribute attribute)
{
    switch (attribute)
    {
    case BattleAttribute::MaxHp:
    case BattleAttribute::Attack:
    case BattleAttribute::Defence:
    case BattleAttribute::Speed:
    case BattleAttribute::ProjectilePressureDamage:
        return false;
    case BattleAttribute::CriticalChance:
    case BattleAttribute::CriticalDamage:
    case BattleAttribute::DodgeChance:
    case BattleAttribute::BlockChance:
    case BattleAttribute::DamageReduction:
    case BattleAttribute::SkillDamage:
    case BattleAttribute::CooldownReduction:
    case BattleAttribute::MpRecoveryBonus:
    case BattleAttribute::StaggerResistance:
    case BattleAttribute::ProjectileReflectChance:
    case BattleAttribute::SkillReflectPercent:
    case BattleAttribute::CounterUltimateBlockChance:
    case BattleAttribute::CriticalAfterDodge:
    case BattleAttribute::DashChance:
    case BattleAttribute::OutgoingCooldownExtensionChance:
    case BattleAttribute::OutgoingCooldownExtensionPercent:
    case BattleAttribute::IncomingCooldownExtensionChance:
    case BattleAttribute::IncomingCooldownExtensionPercent:
        return true;
    }
    assert(false);
    return false;
}

bool attributeModifierIsNegative(AttributeOperation operation, int amount)
{
    switch (operation)
    {
    case AttributeOperation::FlatAdd:
    case AttributeOperation::PercentAdd:
    case AttributeOperation::PercentagePointAdd:
    case AttributeOperation::Override:
        return amount < 0;
    case AttributeOperation::Multiply:
        return amount < 100;
    case AttributeOperation::AtLeast:
        return false;
    }
    assert(false);
    return false;
}

bool hasOrdinaryAttackModification(const ModifyAttackAction& action)
{
    auto ordinary = action;
    ordinary.runtimeBehavior = {};
    return ordinary != ModifyAttackAction{};
}

}  // namespace KysChess
