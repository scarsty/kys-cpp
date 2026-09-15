#pragma once

#include "ChessBattleEffectConstraints.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace KysChess
{

int sumResourceAmounts(int first, int second);

void forEachEffectNumber(
    EffectRule& rule,
    const std::function<void(EffectNumber&)>& visitor);
void forEachEffectNumber(
    const EffectRule& rule,
    const std::function<void(const EffectNumber&)>& visitor);
void forEachDirectEffectNumber(
    const EffectRule& rule,
    const std::function<void(const EffectNumber&)>& visitor);
bool statusBehaviorsEquivalent(
    const std::shared_ptr<const StatusBehaviorDefinition>& lhs,
    const std::shared_ptr<const StatusBehaviorDefinition>& rhs);

struct EffectHealKindCatalogEntry
{
    EffectHealKind kind{};
    std::string_view authorLabel;
};

inline constexpr std::array effectHealKindCatalog{
    EffectHealKindCatalogEntry{ EffectHealKind::Direct, "直接" },
    EffectHealKindCatalogEntry{ EffectHealKind::Team, "隊伍" },
    EffectHealKindCatalogEntry{ EffectHealKind::Aura, "光環" },
    EffectHealKindCatalogEntry{ EffectHealKind::OnHit, "命中" },
    EffectHealKindCatalogEntry{ EffectHealKind::KillReward, "擊殺獎勵" },
    EffectHealKindCatalogEntry{ EffectHealKind::DeathMedical, "死亡醫療" },
    EffectHealKindCatalogEntry{ EffectHealKind::Rescue, "救援" },
    EffectHealKindCatalogEntry{ EffectHealKind::Regeneration, "生命回復" },
    EffectHealKindCatalogEntry{ EffectHealKind::Lifesteal, "吸血" },
};

static_assert(effectHealKindCatalog.size()
    == static_cast<std::size_t>(EffectHealKind::Count));
static_assert([]
{
    for (std::size_t index = 0; index < effectHealKindCatalog.size(); ++index)
    {
        if (static_cast<std::size_t>(effectHealKindCatalog[index].kind) != index
            || effectHealKindCatalog[index].authorLabel.empty()) return false;
    }
    return true;
}());

inline constexpr std::string_view effectHealKindAuthorLabel(EffectHealKind kind)
{
    const auto index = static_cast<std::size_t>(kind);
    assert(index < effectHealKindCatalog.size());
    return effectHealKindCatalog[index].authorLabel;
}

enum class StatusQuantityModel
{
    None,
    Layers,
    SharedLayers,
    TriggerCharges,
    Marks,
    DamageBlockCharges,
    Internal,
};

enum class StatusQuantityFieldId
{
    AddedLayers,
    LayerLimit,
    TargetTotalLayerLimit,
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
    AddSharedLayers,
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
        StatusQuantityFieldId::TargetTotalLayerLimit, "目標總層數上限", "3" },
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
        StatusQuantityOperationId::AddSharedLayers,
        StatusQuantityModel::SharedLayers,
        { StatusQuantityFieldId::AddedLayers,
          StatusQuantityFieldId::TargetTotalLayerLimit },
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
            if (id >= seen.size()) return false;
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
    static constexpr std::array sharedLayers{
        StatusQuantityOperationId::AddSharedLayers };
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
    case StatusQuantityModel::SharedLayers: return sharedLayers;
    case StatusQuantityModel::TriggerCharges: return triggerCharges;
    case StatusQuantityModel::Marks: return marks;
    case StatusQuantityModel::DamageBlockCharges: return damageBlocks;
    case StatusQuantityModel::None:
    case StatusQuantityModel::Internal:
        return none;
    }
    return none;
}

// The model views above are intentionally convenient fixed spans for schema
// and parser iteration.  This inverse agreement check makes the operation
// catalog authoritative: every catalog entry must occur in exactly one view,
// and adding an operation without updating that view is a compile-time error.
static_assert([]
{
    for (const auto& operation : statusQuantityOperationCatalog)
    {
        int occurrences{};
        for (const auto model : {
                 StatusQuantityModel::None,
                 StatusQuantityModel::Layers,
                 StatusQuantityModel::SharedLayers,
                 StatusQuantityModel::TriggerCharges,
                 StatusQuantityModel::Marks,
                 StatusQuantityModel::DamageBlockCharges,
                 StatusQuantityModel::Internal })
        {
            for (const auto candidate : statusQuantityOperations(model))
            {
                if (candidate == operation.id)
                {
                    if (operation.model != model) return false;
                    ++occurrences;
                }
            }
        }
        if (occurrences != 1) return false;
    }
    return true;
}());

enum class StatusDurationModel
{
    RequiredPositive,
    Forbidden,
    RuntimeOwned,
};

struct StatusReapplicationPolicyCatalogEntry
{
    StatusReapplicationPolicy policy{};
    std::string_view authorLabel;
};

inline constexpr std::array statusReapplicationPolicyCatalog{
    StatusReapplicationPolicyCatalogEntry{
        StatusReapplicationPolicy::Implicit, {} },
    StatusReapplicationPolicyCatalogEntry{
        StatusReapplicationPolicy::ExtendDuration, "延長持續時間" },
    StatusReapplicationPolicyCatalogEntry{
        StatusReapplicationPolicy::KeepLongerDuration, "保留較長持續時間" },
    StatusReapplicationPolicyCatalogEntry{
        StatusReapplicationPolicy::RefreshDuration, "刷新持續時間" },
    StatusReapplicationPolicyCatalogEntry{
        StatusReapplicationPolicy::KeepHigherDamage, "保留較高傷害" },
    StatusReapplicationPolicyCatalogEntry{
        StatusReapplicationPolicy::ReplaceExistingPoison, "取代現有中毒" },
};

static_assert(statusReapplicationPolicyCatalog.size()
    == static_cast<std::size_t>(StatusReapplicationPolicy::Count));
static_assert([]
{
    for (std::size_t index = 0;
         index < statusReapplicationPolicyCatalog.size();
         ++index)
    {
        const auto& entry = statusReapplicationPolicyCatalog[index];
        if (static_cast<std::size_t>(entry.policy) != index) return false;
        if (entry.policy == StatusReapplicationPolicy::Implicit)
        {
            if (!entry.authorLabel.empty()) return false;
        }
        else if (entry.authorLabel.empty()) return false;
    }
    return true;
}());

inline constexpr std::string_view statusReapplicationPolicyLabel(
    StatusReapplicationPolicy policy)
{
    const auto index = static_cast<std::size_t>(policy);
    assert(index < statusReapplicationPolicyCatalog.size());
    assert(statusReapplicationPolicyCatalog[index].policy == policy);
    return statusReapplicationPolicyCatalog[index].authorLabel;
}

enum class StatusReapplicationModel
{
    Implicit,
    StunDuration,
    CatalogKeepLongerDuration,
    CatalogRefreshDuration,
    PoisonDamage,
    CatalogReplaceSelected,
    AuthoredRefreshDuration,
};

enum class StatusBehaviorClassification
{
    Intrinsic,
    CatalogOwned,
    Profiled,
    OpenMarker,
};

enum class StatusStorageModel
{
    ProducerOwnedContributions,
    SharedLayerDebuff,
    SelectedDebuffInstance,
    SharedDurationControl,
};

enum class StatusBehaviorProfile
{
    None,
    Poison,
    Bleed,
    ColdPoison,
    WitheredBone,
    SevenStar,
    NeutralizeForce,
    Blinded,
    NextIncomingAttackMiss,
    DamageBlock,
    SingleHitCap,
    BattleSpirit,
    TrueQi,
    PoisonExplosion,
};

enum class StatusNamedNumberFieldId
{
    NeutralizeMpRecovery,
    Count,
};

enum class StatusNamedNumberConstraint
{
    Positive,
};

enum class StatusNamedNumberDescriptionRole
{
    NeutralizeMpRecovery,
};

struct StatusNamedNumberFieldCatalogEntry
{
    StatusNamedNumberFieldId id{};
    BattleStatusKind status{};
    std::string_view label;
    std::string_view probeValue;
    bool required{};
    StatusNamedNumberConstraint constraint{};
    StatusNamedNumberDescriptionRole descriptionRole{};
};

inline constexpr std::array statusNamedNumberFieldCatalog{
    StatusNamedNumberFieldCatalogEntry{
        .id = StatusNamedNumberFieldId::NeutralizeMpRecovery,
        .status = BattleStatusKind::NeutralizeForce,
        .label = "命中回內",
        .probeValue = "100",
        .required = true,
        .constraint = StatusNamedNumberConstraint::Positive,
        .descriptionRole = StatusNamedNumberDescriptionRole::NeutralizeMpRecovery,
    },
};

struct StatusCatalogEntry
{
    BattleStatusKind status{};
    std::string_view label;
    bool negative{};
    bool control{};
    StatusStorageModel storage{};
    StatusQuantityModel quantity{};
    StatusDurationModel duration{};
    StatusReapplicationModel reapplication{};
    StatusBehaviorClassification behaviorClassification{};
    StatusBehaviorProfile behaviorProfile{};
    std::array<StatusNamedNumberFieldId,
        statusNamedNumberFieldCatalog.size()> namedNumberFields{};
    std::size_t namedNumberFieldCount{};
    bool authorable = true;
};

inline constexpr std::array statusCatalog{
    StatusCatalogEntry{
        .status = BattleStatusKind::Poison,
        .label = "中毒",
        .negative = true,
        .storage = StatusStorageModel::SelectedDebuffInstance,
        .quantity = StatusQuantityModel::TriggerCharges,
        .duration = StatusDurationModel::RequiredPositive,
        .reapplication = StatusReapplicationModel::PoisonDamage,
        .behaviorClassification = StatusBehaviorClassification::Profiled,
        .behaviorProfile = StatusBehaviorProfile::Poison,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::Bleed,
        .label = "流血",
        .negative = true,
        .storage = StatusStorageModel::SharedLayerDebuff,
        .quantity = StatusQuantityModel::SharedLayers,
        .duration = StatusDurationModel::Forbidden,
        .reapplication = StatusReapplicationModel::Implicit,
        .behaviorClassification = StatusBehaviorClassification::CatalogOwned,
        .behaviorProfile = StatusBehaviorProfile::Bleed,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::Stun,
        .label = "眩暈",
        .negative = true,
        .control = true,
        .storage = StatusStorageModel::SharedDurationControl,
        .quantity = StatusQuantityModel::None,
        .duration = StatusDurationModel::RequiredPositive,
        .reapplication = StatusReapplicationModel::StunDuration,
        .behaviorClassification = StatusBehaviorClassification::Intrinsic,
        .behaviorProfile = StatusBehaviorProfile::None,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::MpBlocked,
        .label = "封內",
        .negative = true,
        .storage = StatusStorageModel::SharedDurationControl,
        .quantity = StatusQuantityModel::None,
        .duration = StatusDurationModel::RequiredPositive,
        .reapplication = StatusReapplicationModel::CatalogKeepLongerDuration,
        .behaviorClassification = StatusBehaviorClassification::Intrinsic,
        .behaviorProfile = StatusBehaviorProfile::None,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::ColdPoison,
        .label = "寒毒",
        .negative = true,
        .storage = StatusStorageModel::SelectedDebuffInstance,
        .quantity = StatusQuantityModel::None,
        .duration = StatusDurationModel::RequiredPositive,
        .reapplication = StatusReapplicationModel::CatalogRefreshDuration,
        .behaviorClassification = StatusBehaviorClassification::CatalogOwned,
        .behaviorProfile = StatusBehaviorProfile::ColdPoison,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::WitheredBone,
        .label = "枯骨",
        .negative = true,
        .storage = StatusStorageModel::SelectedDebuffInstance,
        .quantity = StatusQuantityModel::None,
        .duration = StatusDurationModel::RequiredPositive,
        .reapplication = StatusReapplicationModel::CatalogRefreshDuration,
        .behaviorClassification = StatusBehaviorClassification::CatalogOwned,
        .behaviorProfile = StatusBehaviorProfile::WitheredBone,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::SevenStarMark,
        .label = "七星",
        .negative = true,
        .storage = StatusStorageModel::SelectedDebuffInstance,
        .quantity = StatusQuantityModel::Marks,
        .duration = StatusDurationModel::RequiredPositive,
        .reapplication = StatusReapplicationModel::CatalogReplaceSelected,
        .behaviorClassification = StatusBehaviorClassification::CatalogOwned,
        .behaviorProfile = StatusBehaviorProfile::SevenStar,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::NeutralizeForce,
        .label = "化勁",
        .negative = true,
        .storage = StatusStorageModel::SelectedDebuffInstance,
        .quantity = StatusQuantityModel::TriggerCharges,
        .duration = StatusDurationModel::Forbidden,
        .reapplication = StatusReapplicationModel::Implicit,
        .behaviorClassification = StatusBehaviorClassification::CatalogOwned,
        .behaviorProfile = StatusBehaviorProfile::NeutralizeForce,
        .namedNumberFields = { StatusNamedNumberFieldId::NeutralizeMpRecovery },
        .namedNumberFieldCount = 1,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::Blinded,
        .label = "刺目",
        .negative = true,
        .storage = StatusStorageModel::SelectedDebuffInstance,
        .quantity = StatusQuantityModel::TriggerCharges,
        .duration = StatusDurationModel::Forbidden,
        .reapplication = StatusReapplicationModel::Implicit,
        .behaviorClassification = StatusBehaviorClassification::CatalogOwned,
        .behaviorProfile = StatusBehaviorProfile::Blinded,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::NextAttackMiss,
        .label = "下一次受到攻擊必定落空",
        .quantity = StatusQuantityModel::TriggerCharges,
        .duration = StatusDurationModel::RequiredPositive,
        .reapplication = StatusReapplicationModel::Implicit,
        .behaviorClassification = StatusBehaviorClassification::Profiled,
        .behaviorProfile = StatusBehaviorProfile::NextIncomingAttackMiss,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::DamageBlockLayer,
        .label = "傷害抵擋",
        .quantity = StatusQuantityModel::DamageBlockCharges,
        .duration = StatusDurationModel::Forbidden,
        .reapplication = StatusReapplicationModel::Implicit,
        .behaviorClassification = StatusBehaviorClassification::Profiled,
        .behaviorProfile = StatusBehaviorProfile::DamageBlock,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::SingleHitCapLayer,
        .label = "下次承傷上限",
        .quantity = StatusQuantityModel::TriggerCharges,
        .duration = StatusDurationModel::Forbidden,
        .reapplication = StatusReapplicationModel::Implicit,
        .behaviorClassification = StatusBehaviorClassification::Profiled,
        .behaviorProfile = StatusBehaviorProfile::SingleHitCap,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::BattleSpirit,
        .label = "戰意",
        .quantity = StatusQuantityModel::Layers,
        .duration = StatusDurationModel::Forbidden,
        .reapplication = StatusReapplicationModel::Implicit,
        .behaviorClassification = StatusBehaviorClassification::Profiled,
        .behaviorProfile = StatusBehaviorProfile::BattleSpirit,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::TrueQi,
        .label = "真氣",
        .quantity = StatusQuantityModel::Layers,
        .duration = StatusDurationModel::Forbidden,
        .reapplication = StatusReapplicationModel::Implicit,
        .behaviorClassification = StatusBehaviorClassification::Profiled,
        .behaviorProfile = StatusBehaviorProfile::TrueQi,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::PoisonExplosion,
        .label = "毒爆",
        .quantity = StatusQuantityModel::Layers,
        .duration = StatusDurationModel::Forbidden,
        .reapplication = StatusReapplicationModel::Implicit,
        .behaviorClassification = StatusBehaviorClassification::Profiled,
        .behaviorProfile = StatusBehaviorProfile::PoisonExplosion,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::Shadowless,
        .label = "無影",
        .quantity = StatusQuantityModel::None,
        .duration = StatusDurationModel::RequiredPositive,
        .reapplication = StatusReapplicationModel::AuthoredRefreshDuration,
        .behaviorClassification = StatusBehaviorClassification::OpenMarker,
        .behaviorProfile = StatusBehaviorProfile::None,
    },
    StatusCatalogEntry{
        .status = BattleStatusKind::NextAttackCritical,
        .label = "下一次攻擊必定暴擊",
        .quantity = StatusQuantityModel::Internal,
        .duration = StatusDurationModel::RuntimeOwned,
        .reapplication = StatusReapplicationModel::Implicit,
        .behaviorClassification = StatusBehaviorClassification::Intrinsic,
        .behaviorProfile = StatusBehaviorProfile::None,
        .authorable = false,
    },
};

static_assert(statusCatalog.size()
    == static_cast<std::size_t>(BattleStatusKind::Count));
static_assert(statusNamedNumberFieldCatalog.size()
    == static_cast<std::size_t>(StatusNamedNumberFieldId::Count));
static_assert([]
{
    std::array<int, static_cast<std::size_t>(StatusNamedNumberFieldId::Count)>
        namedFieldOccurrences{};
    for (std::size_t index = 0; index < statusCatalog.size(); ++index)
    {
        const auto& status = statusCatalog[index];
        if (static_cast<std::size_t>(status.status) != index
            || status.label.empty()
            || (status.control && !status.negative))
        {
            return false;
        }
        const auto quantityOperations = statusQuantityOperations(status.quantity);
        const bool expectsQuantityOperations = status.quantity != StatusQuantityModel::None
            && status.quantity != StatusQuantityModel::Internal;
        if (quantityOperations.empty() == expectsQuantityOperations) return false;
        const bool classifiedBehaviorProfile =
            status.behaviorClassification == StatusBehaviorClassification::Profiled
            || status.behaviorClassification == StatusBehaviorClassification::CatalogOwned;
        if (classifiedBehaviorProfile
            != (status.behaviorProfile != StatusBehaviorProfile::None)) return false;
        if (status.negative
            && status.storage == StatusStorageModel::ProducerOwnedContributions)
            return false;
        if (status.namedNumberFieldCount > status.namedNumberFields.size())
            return false;
        for (std::size_t fieldIndex = 0;
             fieldIndex < status.namedNumberFieldCount;
             ++fieldIndex)
        {
            const auto id = static_cast<std::size_t>(
                status.namedNumberFields[fieldIndex]);
            if (id >= namedFieldOccurrences.size()
                || statusNamedNumberFieldCatalog[id].id
                    != status.namedNumberFields[fieldIndex]
                || statusNamedNumberFieldCatalog[id].status != status.status)
                return false;
            ++namedFieldOccurrences[id];
        }
        for (const auto operation : quantityOperations)
        {
            if (statusQuantityOperationCatalogEntry(operation).model != status.quantity)
                return false;
        }
    }
    for (const auto occurrences : namedFieldOccurrences)
        if (occurrences != 1) return false;
    return true;
}());

inline constexpr std::span<const StatusCatalogEntry> statusCatalogEntries()
{
    return statusCatalog;
}

inline constexpr const StatusCatalogEntry& statusCatalogEntry(BattleStatusKind status)
{
    const auto index = static_cast<std::size_t>(status);
    assert(index < statusCatalog.size());
    assert(statusCatalog[index].status == status);
    return statusCatalog[index];
}

inline constexpr const StatusNamedNumberFieldCatalogEntry&
statusNamedNumberFieldCatalogEntry(StatusNamedNumberFieldId id)
{
    const auto index = static_cast<std::size_t>(id);
    assert(index < statusNamedNumberFieldCatalog.size());
    assert(statusNamedNumberFieldCatalog[index].id == id);
    return statusNamedNumberFieldCatalog[index];
}

inline constexpr std::span<const StatusNamedNumberFieldId> statusNamedNumberFields(
    BattleStatusKind status)
{
    const auto& catalog = statusCatalogEntry(status);
    return { catalog.namedNumberFields.data(), catalog.namedNumberFieldCount };
}

inline constexpr bool statusHasNamedNumberField(
    BattleStatusKind status,
    StatusNamedNumberFieldId field)
{
    const auto fields = statusNamedNumberFields(status);
    return std::find(fields.begin(), fields.end(), field) != fields.end();
}

inline std::optional<EffectNumber>& statusNamedNumberField(
    ApplyStatusAction& action,
    StatusNamedNumberFieldId field)
{
    switch (field)
    {
    case StatusNamedNumberFieldId::NeutralizeMpRecovery:
        return action.neutralizeMpRecovery;
    case StatusNamedNumberFieldId::Count:
        break;
    }
    assert(false);
    std::unreachable();
}

inline const std::optional<EffectNumber>& statusNamedNumberField(
    const ApplyStatusAction& action,
    StatusNamedNumberFieldId field)
{
    switch (field)
    {
    case StatusNamedNumberFieldId::NeutralizeMpRecovery:
        return action.neutralizeMpRecovery;
    case StatusNamedNumberFieldId::Count:
        break;
    }
    assert(false);
    std::unreachable();
}

inline constexpr std::string_view statusQuantityCounter(StatusQuantityModel quantity)
{
    switch (quantity)
    {
    case StatusQuantityModel::Layers: return "層";
    case StatusQuantityModel::SharedLayers: return "層";
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
    case StatusQuantityModel::SharedLayers:
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
    case StatusQuantityModel::SharedLayers: return "共享層數";
    case StatusQuantityModel::TriggerCharges: return "觸發次數";
    case StatusQuantityModel::Marks: return "印記數量";
    case StatusQuantityModel::DamageBlockCharges: return "抵擋次數";
    case StatusQuantityModel::None:
    case StatusQuantityModel::Internal:
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

inline constexpr int CanonicalPoisonIntervalFrames = 30;

struct CanonicalPoisonDamageCapability
{
    const EffectRule* rule = nullptr;
    const DealDamageAction* damage = nullptr;
    const ConsumeThisStatusAction* consume = nullptr;
};

std::optional<int> effectiveConstantEffectNumberValue(
    const EffectNumber& number,
    int contributionQuantity = 1);
std::int64_t roundEffectRatio(
    std::int64_t numerator,
    std::int64_t denominator,
    EffectRounding rounding);
std::optional<CanonicalPoisonDamageCapability> canonicalPoisonDamageCapability(
    const StatusBehaviorDefinition& behavior);
bool statusReapplicationPolicyAllowed(
    BattleStatusKind status,
    StatusReapplicationPolicy policy);
bool statusReapplicationPolicyRequired(BattleStatusKind status);
bool statusBehaviorIsCatalogOwned(BattleStatusKind status);
std::shared_ptr<const StatusBehaviorDefinition> makeCatalogOwnedStatusBehavior(
    const ApplyStatusAction& action);
LoweredStatusQuantity lowerStatusQuantity(const ApplyStatusAction& action);
EffectStackPolicy lowerStatusReapplication(const ApplyStatusAction& action);
bool battleAttributeUsesPercentagePoints(BattleAttribute attribute);
bool attributeModifierIsNegative(AttributeOperation operation, int amount);
bool hasOrdinaryAttackModification(const ModifyAttackAction& action);

}  // namespace KysChess
