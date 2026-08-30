#pragma once

#include "ChessBattleEffectTypes.h"

#include <span>
#include <type_traits>

namespace KysChess
{

enum class StatusQuantityModel
{
    None,
    Layers,
    TriggerCharges,
    Marks,
    DamageBlockCharges,
    Internal,
};

enum class StatusEffectScope
{
    None,
    Persistent,
    PerLayer,
    PerTrigger,
    PerLayerValue,
    RuntimeOwned,
};

enum class StatusDurationModel
{
    RequiredPositive,
    Forbidden,
    RuntimeOwned,
};

enum class StatusReapplicationModel
{
    Implicit,
    StunDuration,
    KeepLongerDuration,
    RefreshDuration,
    PoisonDamage,
};

struct StatusCatalogEntry
{
    BattleStatusKind status{};
    StatusQuantityModel quantity{};
    StatusEffectScope effectScope{};
    StatusDurationModel duration{};
    StatusReapplicationModel reapplication{};
    std::string_view quantityNoun;
    bool authorable = true;
};

enum class StatusRuntimeValueSlot
{
    Potency,
    SecondaryPotency,
};

struct StatusEffectValueCatalogEntry
{
    StatusEffectValueKind value{};
    BattleStatusKind status{};
    std::string_view label;
    StatusRuntimeValueSlot runtimeSlot{};
};

struct LoweredStatusQuantity
{
    int stacks{};
    EffectStackPolicy stack{};
    std::optional<int> stackLimit;
};

std::optional<int> effectiveConstantEffectNumberValue(const EffectNumber& number);
std::span<const StatusCatalogEntry> statusCatalogEntries();
const StatusCatalogEntry& statusCatalogEntry(BattleStatusKind status);
bool statusReapplicationPolicyAllowed(
    BattleStatusKind status,
    StatusReapplicationPolicy policy);
bool statusReapplicationPolicyRequired(BattleStatusKind status);
bool matchesSevenStarLifecycleProducer(const EffectRule& rule);
bool matchesSevenStarLifecycleConsumer(const EffectRule& rule);
bool matchesPoisonExplosionLifecycleProducer(const EffectRule& rule);
bool matchesPoisonExplosionLifecycleConsumer(const EffectRule& rule);
std::span<const StatusEffectValueCatalogEntry> statusEffectValueCatalogEntries();
std::string_view statusEffectValueLabel(StatusEffectValueKind value);
bool statusEffectValueBelongsToStatus(
    StatusEffectValueKind value,
    BattleStatusKind status);
StatusRuntimeValueSlot statusEffectRuntimeValueSlot(StatusEffectValueKind value);
LoweredStatusQuantity lowerStatusQuantity(const ApplyStatusAction& action);
EffectStackPolicy lowerStatusReapplication(const ApplyStatusAction& action);
bool battleAttributeUsesPercentagePoints(BattleAttribute attribute);
bool attributeModifierIsNegative(AttributeOperation operation, int amount);
bool hasOrdinaryAttackModification(const ModifyAttackAction& action);

template <typename Visitor>
void forEachStatusEffectNumber(
    const StatusEffectPayload& payload,
    Visitor&& visitor)
{
    std::visit([&](const auto& effects)
    {
        using T = std::decay_t<decltype(effects)>;
        if constexpr (std::is_same_v<T, PoisonStatusEffects>)
            visitor(effects.currentHpDamagePercent);
        else if constexpr (std::is_same_v<T, BleedStatusEffects>)
            visitor(effects.maxHpDamagePercent);
        else if constexpr (std::is_same_v<T, ColdPoisonStatusEffects>)
            visitor(effects.speedReductionPercent);
        else if constexpr (std::is_same_v<T, WitheredBoneStatusEffects>)
        {
            visitor(effects.damageTakenIncreasePercent);
            visitor(effects.healingReductionPercent);
        }
        else if constexpr (std::is_same_v<T, NeutralizeForceStatusEffects>)
            visitor(effects.originalTargetShield);
        else if constexpr (std::is_same_v<T, SingleHitCapStatusEffects>)
            visitor(effects.damageCap);
        else if constexpr (std::is_same_v<T, BattleSpiritStatusEffects>)
        {
            visitor(effects.skillDamageIncreasePercent);
            visitor(effects.damageReductionPercent);
        }
        else if constexpr (std::is_same_v<T, TrueQiStatusEffects>)
            visitor(effects.pureDamagePerHit);
        else if constexpr (std::is_same_v<T, PoisonExplosionStatusEffects>)
            visitor(effects.deathPureDamage);
    }, payload);
}

}  // namespace KysChess
