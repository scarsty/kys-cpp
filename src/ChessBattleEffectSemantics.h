#pragma once

#include "ChessBattleEffectTypes.h"

#include <array>
#include <optional>
#include <span>
#include <string_view>
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

enum class StatusQuantityFieldId
{
    AddedLayers,
    LayerLimit,
    TriggerCharges,
    SetMarks,
    AddedDamageBlocks,
    DamageBlockLimit,
    SetDamageBlocks,
    Count,
};

enum class StatusQuantityOperationId
{
    AddLayers,
    SetTriggerCharges,
    SetMarks,
    AddDamageBlocks,
    SetDamageBlocks,
    Count,
};

struct StatusQuantityFieldCatalogEntry
{
    StatusQuantityFieldId id{};
    std::string_view label;
    std::string_view probeValue;
};

struct StatusQuantityOperationCatalogEntry
{
    StatusQuantityOperationId id{};
    StatusQuantityModel model{};
    std::array<StatusQuantityFieldId, 2> fields{};
    std::size_t fieldCount{};
};

inline constexpr std::array statusQuantityFieldCatalog{
    StatusQuantityFieldCatalogEntry{
        StatusQuantityFieldId::AddedLayers, "增加層數", "1" },
    StatusQuantityFieldCatalogEntry{
        StatusQuantityFieldId::LayerLimit, "層數上限", "10" },
    StatusQuantityFieldCatalogEntry{
        StatusQuantityFieldId::TriggerCharges, "可觸發次數", "1" },
    StatusQuantityFieldCatalogEntry{
        StatusQuantityFieldId::SetMarks, "設定印記層數", "7" },
    StatusQuantityFieldCatalogEntry{
        StatusQuantityFieldId::AddedDamageBlocks, "增加可抵擋次數", "1" },
    StatusQuantityFieldCatalogEntry{
        StatusQuantityFieldId::DamageBlockLimit, "可抵擋次數上限", "3" },
    StatusQuantityFieldCatalogEntry{
        StatusQuantityFieldId::SetDamageBlocks, "設定可抵擋次數", "3" },
};

inline constexpr std::array statusQuantityOperationCatalog{
    StatusQuantityOperationCatalogEntry{
        StatusQuantityOperationId::AddLayers,
        StatusQuantityModel::Layers,
        { StatusQuantityFieldId::AddedLayers, StatusQuantityFieldId::LayerLimit },
        2,
    },
    StatusQuantityOperationCatalogEntry{
        StatusQuantityOperationId::SetTriggerCharges,
        StatusQuantityModel::TriggerCharges,
        { StatusQuantityFieldId::TriggerCharges },
        1,
    },
    StatusQuantityOperationCatalogEntry{
        StatusQuantityOperationId::SetMarks,
        StatusQuantityModel::Marks,
        { StatusQuantityFieldId::SetMarks },
        1,
    },
    StatusQuantityOperationCatalogEntry{
        StatusQuantityOperationId::AddDamageBlocks,
        StatusQuantityModel::DamageBlockCharges,
        { StatusQuantityFieldId::AddedDamageBlocks,
          StatusQuantityFieldId::DamageBlockLimit },
        2,
    },
    StatusQuantityOperationCatalogEntry{
        StatusQuantityOperationId::SetDamageBlocks,
        StatusQuantityModel::DamageBlockCharges,
        { StatusQuantityFieldId::SetDamageBlocks },
        1,
    },
};

static_assert(statusQuantityFieldCatalog.size()
    == static_cast<std::size_t>(StatusQuantityFieldId::Count));
static_assert(statusQuantityOperationCatalog.size()
    == static_cast<std::size_t>(StatusQuantityOperationId::Count));
static_assert([]
{
    std::array<bool, static_cast<std::size_t>(StatusQuantityFieldId::Count)> seen{};
    for (std::size_t index = 0; index < statusQuantityFieldCatalog.size(); ++index)
    {
        const auto& field = statusQuantityFieldCatalog[index];
        if (static_cast<std::size_t>(field.id) != index
            || field.label.empty()
            || field.probeValue.empty())
            return false;
    }
    for (std::size_t index = 0; index < statusQuantityOperationCatalog.size(); ++index)
    {
        const auto& operation = statusQuantityOperationCatalog[index];
        if (static_cast<std::size_t>(operation.id) != index
            || operation.model == StatusQuantityModel::None
            || operation.model == StatusQuantityModel::Internal
            || operation.fieldCount == 0
            || operation.fieldCount > operation.fields.size())
            return false;
        for (std::size_t fieldIndex = 0; fieldIndex < operation.fieldCount; ++fieldIndex)
        {
            const auto id = static_cast<std::size_t>(operation.fields[fieldIndex]);
            if (id >= seen.size() || seen[id]) return false;
            seen[id] = true;
        }
    }
    for (const bool present : seen)
        if (!present) return false;
    return true;
}());

inline constexpr const StatusQuantityFieldCatalogEntry& statusQuantityFieldCatalogEntry(
    StatusQuantityFieldId id)
{
    return statusQuantityFieldCatalog[static_cast<std::size_t>(id)];
}

inline constexpr std::string_view statusQuantityFieldLabel(StatusQuantityFieldId id)
{
    return statusQuantityFieldCatalogEntry(id).label;
}

inline constexpr const StatusQuantityOperationCatalogEntry&
statusQuantityOperationCatalogEntry(StatusQuantityOperationId id)
{
    return statusQuantityOperationCatalog[static_cast<std::size_t>(id)];
}

inline constexpr std::span<const StatusQuantityFieldId> statusQuantityOperationFields(
    StatusQuantityOperationId id)
{
    const auto& operation = statusQuantityOperationCatalogEntry(id);
    return { operation.fields.data(), operation.fieldCount };
}

inline constexpr std::span<const StatusQuantityOperationId> statusQuantityOperations(
    StatusQuantityModel model)
{
    static constexpr std::array<StatusQuantityOperationId, 0> none{};
    static constexpr std::array layers{ StatusQuantityOperationId::AddLayers };
    static constexpr std::array triggerCharges{
        StatusQuantityOperationId::SetTriggerCharges };
    static constexpr std::array marks{ StatusQuantityOperationId::SetMarks };
    static constexpr std::array damageBlocks{
        StatusQuantityOperationId::AddDamageBlocks,
        StatusQuantityOperationId::SetDamageBlocks,
    };
    switch (model)
    {
    case StatusQuantityModel::Layers: return layers;
    case StatusQuantityModel::TriggerCharges: return triggerCharges;
    case StatusQuantityModel::Marks: return marks;
    case StatusQuantityModel::DamageBlockCharges: return damageBlocks;
    case StatusQuantityModel::None:
    case StatusQuantityModel::Internal:
        return none;
    }
    return none;
}

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
    bool authorable = true;
};

inline constexpr std::string_view statusQuantityCounter(StatusQuantityModel quantity)
{
    switch (quantity)
    {
    case StatusQuantityModel::Layers: return "層";
    case StatusQuantityModel::TriggerCharges: return "次";
    case StatusQuantityModel::Marks: return "枚";
    case StatusQuantityModel::DamageBlockCharges: return "次";
    case StatusQuantityModel::None:
    case StatusQuantityModel::Internal:
        return {};
    }
    return {};
}

inline constexpr std::string_view statusQuantityObjectSuffix(StatusQuantityModel quantity)
{
    switch (quantity)
    {
    case StatusQuantityModel::TriggerCharges: return "觸發";
    case StatusQuantityModel::Marks: return "印記";
    case StatusQuantityModel::None:
    case StatusQuantityModel::Layers:
    case StatusQuantityModel::DamageBlockCharges:
    case StatusQuantityModel::Internal:
        return {};
    }
    return {};
}

inline constexpr std::string_view statusQuantityMeasure(StatusQuantityModel quantity)
{
    switch (quantity)
    {
    case StatusQuantityModel::Layers: return "層數";
    case StatusQuantityModel::TriggerCharges: return "觸發次數";
    case StatusQuantityModel::Marks: return "印記數量";
    case StatusQuantityModel::DamageBlockCharges: return "抵擋次數";
    case StatusQuantityModel::None:
    case StatusQuantityModel::Internal:
        return {};
    }
    return {};
}

enum class StatusRuntimeValueSlot
{
    Potency,
    SecondaryPotency,
};

enum class StatusEffectFieldId
{
    PoisonCurrentHpDamagePercent,
    BleedMaxHpDamagePercent,
    ColdPoisonBlocksHealing,
    ColdPoisonSpeedReductionPercent,
    WitheredBoneDamageTakenIncreasePercent,
    WitheredBoneHealingReductionPercent,
    NeutralizeForcePreventsCast,
    NeutralizeForceOriginalTargetShield,
    BlindedPreventsCast,
    NextIncomingAttackMiss,
    DamageBlockPositiveNonExecuteDamage,
    SingleHitDamageCap,
    BattleSpiritSkillDamageIncreasePercent,
    BattleSpiritDamageReductionPercent,
    TrueQiPureDamagePerHit,
    PoisonExplosionDeathPureDamage,
    Count,
};

enum class StatusEffectFieldType
{
    Number,
    RequiredTrue,
};

enum class StatusEffectNumberConstraint
{
    None,
    Positive,
    Nonnegative,
    NonnegativeAtMost100,
    PositiveLayerProduct,
    NonnegativeLayerProduct,
};

struct StatusEffectFieldCatalogEntry
{
    StatusEffectFieldId id{};
    BattleStatusKind status{};
    std::string_view label;
    std::string_view coveragePath;
    StatusEffectFieldType type{};
    std::optional<StatusEffectValueKind> value;
    std::optional<StatusRuntimeValueSlot> runtimeSlot;
    StatusEffectNumberConstraint numberConstraint{};
    std::string_view probeValue;
};

inline constexpr std::array statusEffectFieldCatalog{
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::PoisonCurrentHpDamagePercent,
        BattleStatusKind::Poison,
        "目前生命傷害百分比",
        "perTrigger.currentHpDamagePercent",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::PoisonCurrentHpDamagePercent,
        StatusRuntimeValueSlot::Potency,
        StatusEffectNumberConstraint::Positive,
        "7",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::BleedMaxHpDamagePercent,
        BattleStatusKind::Bleed,
        "最大生命傷害百分比",
        "perLayer.maxHpDamagePercent",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::BleedMaxHpDamagePercent,
        StatusRuntimeValueSlot::Potency,
        StatusEffectNumberConstraint::PositiveLayerProduct,
        "1",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::ColdPoisonBlocksHealing,
        BattleStatusKind::ColdPoison,
        "禁止受到治療",
        "persistent.blocksHealing",
        StatusEffectFieldType::RequiredTrue,
        std::nullopt,
        std::nullopt,
        StatusEffectNumberConstraint::None,
        "true",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::ColdPoisonSpeedReductionPercent,
        BattleStatusKind::ColdPoison,
        "速度降低百分比",
        "persistent.speedReductionPercent",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::ColdPoisonSpeedReductionPercent,
        StatusRuntimeValueSlot::Potency,
        StatusEffectNumberConstraint::Nonnegative,
        "25",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::WitheredBoneDamageTakenIncreasePercent,
        BattleStatusKind::WitheredBone,
        "受到傷害增加百分比",
        "persistent.damageTakenIncreasePercent",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::WitheredBoneDamageTakenIncreasePercent,
        StatusRuntimeValueSlot::Potency,
        StatusEffectNumberConstraint::Nonnegative,
        "25",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::WitheredBoneHealingReductionPercent,
        BattleStatusKind::WitheredBone,
        "受到治療減少百分比",
        "persistent.healingReductionPercent",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::WitheredBoneHealingReductionPercent,
        StatusRuntimeValueSlot::SecondaryPotency,
        StatusEffectNumberConstraint::NonnegativeAtMost100,
        "75",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::NeutralizeForcePreventsCast,
        BattleStatusKind::NeutralizeForce,
        "阻止本次施放",
        "perTrigger.preventsCast",
        StatusEffectFieldType::RequiredTrue,
        std::nullopt,
        std::nullopt,
        StatusEffectNumberConstraint::None,
        "true",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::NeutralizeForceOriginalTargetShield,
        BattleStatusKind::NeutralizeForce,
        "原攻擊目標獲得護盾",
        "perTrigger.originalTargetShield",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::NeutralizeForceOriginalTargetShield,
        StatusRuntimeValueSlot::Potency,
        StatusEffectNumberConstraint::Positive,
        "每星級: 100",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::BlindedPreventsCast,
        BattleStatusKind::Blinded,
        "阻止本次施放",
        "perTrigger.preventsCast",
        StatusEffectFieldType::RequiredTrue,
        std::nullopt,
        std::nullopt,
        StatusEffectNumberConstraint::None,
        "true",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::NextIncomingAttackMiss,
        BattleStatusKind::NextAttackMiss,
        "使本次受到攻擊落空",
        "perTrigger.makesIncomingAttackMiss",
        StatusEffectFieldType::RequiredTrue,
        std::nullopt,
        std::nullopt,
        StatusEffectNumberConstraint::None,
        "true",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::DamageBlockPositiveNonExecuteDamage,
        BattleStatusKind::DamageBlockLayer,
        "抵擋非處決正傷害",
        "perTrigger.blocksPositiveNonExecuteDamage",
        StatusEffectFieldType::RequiredTrue,
        std::nullopt,
        std::nullopt,
        StatusEffectNumberConstraint::None,
        "true",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::SingleHitDamageCap,
        BattleStatusKind::SingleHitCapLayer,
        "傷害上限",
        "perTrigger.damageCap",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::SingleHitDamageCap,
        StatusRuntimeValueSlot::Potency,
        StatusEffectNumberConstraint::Positive,
        "目標最大生命百分比: 15\n取整: 向零\n最小: 1",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::BattleSpiritSkillDamageIncreasePercent,
        BattleStatusKind::BattleSpirit,
        "招式傷害增加百分比",
        "perLayer.skillDamageIncreasePercent",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::BattleSpiritSkillDamageIncreasePercent,
        StatusRuntimeValueSlot::Potency,
        StatusEffectNumberConstraint::NonnegativeLayerProduct,
        "5",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::BattleSpiritDamageReductionPercent,
        BattleStatusKind::BattleSpirit,
        "傷害減免百分比",
        "perLayer.damageReductionPercent",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::BattleSpiritDamageReductionPercent,
        StatusRuntimeValueSlot::SecondaryPotency,
        StatusEffectNumberConstraint::NonnegativeLayerProduct,
        "1",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::TrueQiPureDamagePerHit,
        BattleStatusKind::TrueQi,
        "命中附加純粹傷害",
        "perLayer.pureDamagePerHit",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::TrueQiPureDamagePerHit,
        StatusRuntimeValueSlot::Potency,
        StatusEffectNumberConstraint::PositiveLayerProduct,
        "9",
    },
    StatusEffectFieldCatalogEntry{
        StatusEffectFieldId::PoisonExplosionDeathPureDamage,
        BattleStatusKind::PoisonExplosion,
        "死亡爆炸純粹傷害",
        "perLayerValue.deathPureDamage",
        StatusEffectFieldType::Number,
        StatusEffectValueKind::PoisonExplosionDeathPureDamage,
        StatusRuntimeValueSlot::Potency,
        StatusEffectNumberConstraint::PositiveLayerProduct,
        "每星級: 60",
    },
};

static_assert(statusEffectFieldCatalog.size()
    == static_cast<std::size_t>(StatusEffectFieldId::Count));
static_assert([]
{
    std::array<bool, static_cast<std::size_t>(StatusEffectValueKind::Count)> seenValues{};
    for (std::size_t index = 0; index < statusEffectFieldCatalog.size(); ++index)
    {
        const auto& field = statusEffectFieldCatalog[index];
        if (static_cast<std::size_t>(field.id) != index
            || field.label.empty()
            || field.coveragePath.empty())
            return false;
        if (field.type == StatusEffectFieldType::Number)
        {
            if (!field.value || !field.runtimeSlot
                || field.numberConstraint == StatusEffectNumberConstraint::None)
                return false;
            const auto valueIndex = static_cast<std::size_t>(*field.value);
            if (valueIndex >= seenValues.size() || seenValues[valueIndex]) return false;
            seenValues[valueIndex] = true;
        }
        else if (field.value || field.runtimeSlot
                 || field.numberConstraint != StatusEffectNumberConstraint::None)
        {
            return false;
        }
    }
    for (const bool seen : seenValues)
        if (!seen) return false;
    return true;
}());

inline constexpr std::span<const StatusEffectFieldCatalogEntry>
statusEffectFieldCatalogEntries()
{
    return statusEffectFieldCatalog;
}

inline constexpr const StatusEffectFieldCatalogEntry& statusEffectFieldCatalogEntry(
    StatusEffectFieldId id)
{
    return statusEffectFieldCatalog[static_cast<std::size_t>(id)];
}

inline constexpr std::string_view statusEffectFieldLabel(StatusEffectFieldId id)
{
    return statusEffectFieldCatalogEntry(id).label;
}

inline constexpr std::string_view statusEffectScopeLabel(StatusEffectScope scope)
{
    switch (scope)
    {
    case StatusEffectScope::Persistent: return "持續生效";
    case StatusEffectScope::PerLayer: return "每層生效";
    case StatusEffectScope::PerTrigger: return "每次觸發";
    case StatusEffectScope::PerLayerValue: return "每層提供數值";
    case StatusEffectScope::None:
    case StatusEffectScope::RuntimeOwned:
        return {};
    }
    return {};
}

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
std::string_view statusEffectValueLabel(StatusEffectValueKind value);
bool statusEffectValueBelongsToStatus(
    StatusEffectValueKind value,
    BattleStatusKind status);
StatusRuntimeValueSlot statusEffectRuntimeValueSlot(StatusEffectValueKind value);
std::optional<BattleStatusKind> statusEffectPayloadStatus(
    const StatusEffectPayload& payload);
LoweredStatusQuantity lowerStatusQuantity(const ApplyStatusAction& action);
EffectStackPolicy lowerStatusReapplication(const ApplyStatusAction& action);
bool battleAttributeUsesPercentagePoints(BattleAttribute attribute);
bool attributeModifierIsNegative(AttributeOperation operation, int amount);
bool hasOrdinaryAttackModification(const ModifyAttackAction& action);

template <typename>
inline constexpr bool unsupportedStatusEffectPayload = false;

template <typename NumberVisitor, typename BooleanVisitor>
void forEachStatusEffectField(
    const StatusEffectPayload& payload,
    NumberVisitor&& numberVisitor,
    BooleanVisitor&& booleanVisitor)
{
    std::visit([&](const auto& effects)
    {
        using T = std::decay_t<decltype(effects)>;
        if constexpr (std::is_same_v<T, PoisonStatusEffects>)
            numberVisitor(StatusEffectFieldId::PoisonCurrentHpDamagePercent,
                effects.currentHpDamagePercent);
        else if constexpr (std::is_same_v<T, BleedStatusEffects>)
            numberVisitor(StatusEffectFieldId::BleedMaxHpDamagePercent,
                effects.maxHpDamagePercent);
        else if constexpr (std::is_same_v<T, ColdPoisonStatusEffects>)
        {
            booleanVisitor(StatusEffectFieldId::ColdPoisonBlocksHealing,
                effects.blocksHealing);
            numberVisitor(StatusEffectFieldId::ColdPoisonSpeedReductionPercent,
                effects.speedReductionPercent);
        }
        else if constexpr (std::is_same_v<T, WitheredBoneStatusEffects>)
        {
            numberVisitor(StatusEffectFieldId::WitheredBoneDamageTakenIncreasePercent,
                effects.damageTakenIncreasePercent);
            numberVisitor(StatusEffectFieldId::WitheredBoneHealingReductionPercent,
                effects.healingReductionPercent);
        }
        else if constexpr (std::is_same_v<T, NeutralizeForceStatusEffects>)
        {
            booleanVisitor(StatusEffectFieldId::NeutralizeForcePreventsCast,
                effects.preventsCast);
            numberVisitor(StatusEffectFieldId::NeutralizeForceOriginalTargetShield,
                effects.originalTargetShield);
        }
        else if constexpr (std::is_same_v<T, BlindedStatusEffects>)
            booleanVisitor(StatusEffectFieldId::BlindedPreventsCast,
                effects.preventsCast);
        else if constexpr (std::is_same_v<T, NextIncomingAttackMissStatusEffects>)
            booleanVisitor(StatusEffectFieldId::NextIncomingAttackMiss,
                effects.makesIncomingAttackMiss);
        else if constexpr (std::is_same_v<T, DamageBlockStatusEffects>)
            booleanVisitor(StatusEffectFieldId::DamageBlockPositiveNonExecuteDamage,
                effects.blocksPositiveNonExecuteDamage);
        else if constexpr (std::is_same_v<T, SingleHitCapStatusEffects>)
            numberVisitor(StatusEffectFieldId::SingleHitDamageCap, effects.damageCap);
        else if constexpr (std::is_same_v<T, BattleSpiritStatusEffects>)
        {
            numberVisitor(StatusEffectFieldId::BattleSpiritSkillDamageIncreasePercent,
                effects.skillDamageIncreasePercent);
            numberVisitor(StatusEffectFieldId::BattleSpiritDamageReductionPercent,
                effects.damageReductionPercent);
        }
        else if constexpr (std::is_same_v<T, TrueQiStatusEffects>)
            numberVisitor(StatusEffectFieldId::TrueQiPureDamagePerHit,
                effects.pureDamagePerHit);
        else if constexpr (std::is_same_v<T, PoisonExplosionStatusEffects>)
            numberVisitor(StatusEffectFieldId::PoisonExplosionDeathPureDamage,
                effects.deathPureDamage);
        else if constexpr (std::is_same_v<T, NoStatusEffects>)
        {
        }
        else
        {
            static_assert(unsupportedStatusEffectPayload<T>,
                "新的狀態效果 payload 必須加入共享欄位走訪器");
        }
    }, payload);
}

template <typename Visitor>
void forEachStatusEffectNumber(
    const StatusEffectPayload& payload,
    Visitor&& visitor)
{
    forEachStatusEffectField(
        payload,
        [&](StatusEffectFieldId, const EffectNumber& number)
        {
            visitor(number);
        },
        [](StatusEffectFieldId, bool) {});
}

}  // namespace KysChess
