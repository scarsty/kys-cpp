#pragma once

#include "ChessBattleEffectTypes.h"
#include "ChessBattleEffectSemantics.h"
#include "ChessEffectAuthoringDescriptors.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string_view>
#include <type_traits>

namespace KysChess::EffectAuthoring::Detail
{
namespace Metadata
{
template <typename Enum>
constexpr AuthorEnumLabel authorLabel(std::string_view name, Enum value)
{
    return AuthorEnumLabel{ name, static_cast<std::int64_t>(value) };
}

template <typename Enum>
std::optional<Enum> parseLabel(
    std::string_view label,
    const AuthorEnumDescriptor& descriptor)
{
    const auto found = std::ranges::find(descriptor.labels, label, &AuthorEnumLabel::name);
    if (found == descriptor.labels.end()) return std::nullopt;
    return static_cast<Enum>(found->value);
}

inline bool validateKnownKeys(
    const YAML::Node& node,
    std::initializer_list<std::string_view> allowed,
    std::string& error)
{
    if (!node || !node.IsMap())
    {
        error = "節點必須是映射表";
        return false;
    }
    std::set<std::string> keys;
    for (const auto& entry : node)
    {
        const auto key = entry.first.as<std::string>();
        if (!keys.insert(key).second)
        {
            error = std::format("重複欄位「{}」", key);
            return false;
        }
        if (std::ranges::find(allowed, key) == allowed.end())
        {
            error = std::format("未知欄位「{}」", key);
            return false;
        }
    }
    return true;
}

inline bool validateUniqueKeys(const YAML::Node& node, std::string& error)
{
    if (!node || !node.IsMap())
    {
        error = "節點必須是映射表";
        return false;
    }
    std::set<std::string> keys;
    for (const auto& entry : node)
    {
        const auto key = entry.first.as<std::string>();
        if (!keys.insert(key).second)
        {
            error = std::format("重複欄位「{}」", key);
            return false;
        }
    }
    return true;
}

bool isDynamicPayloadKey(PayloadDynamicKeyClass keyClass, std::string_view key);

enum class AttackRuntimeBehaviorKind
{
    ProjectileBounce,
    NearbyTracking,
    DelayedAlternate,
    ExpandingSpiral,
};

static constexpr std::array activationScopeLabels{
    authorLabel("每次施放每個目標", EffectActivationScope::PerCastPerTarget),
};
static constexpr AuthorEnumDescriptor activationScopeEnum{
    "EffectActivationScope", activationScopeLabels,
};

static constexpr std::array battleDamageKindLabels{
    authorLabel("物理", BattleDamageKind::Physical),
    authorLabel("招式", BattleDamageKind::Skill),
    authorLabel("純粹", BattleDamageKind::Pure),
    authorLabel("中毒", BattleDamageKind::Poison),
    authorLabel("流血", BattleDamageKind::Bleed),
    authorLabel("特效", BattleDamageKind::Effect),
    authorLabel("處決", BattleDamageKind::Execute),
};
static constexpr AuthorEnumDescriptor battleDamageKindEnum{
    "BattleDamageKind", battleDamageKindLabels,
};

static constexpr std::array effectRoundingLabels{
    authorLabel("向零", EffectRounding::TowardZero),
    authorLabel("向下", EffectRounding::Floor),
    authorLabel("向上", EffectRounding::Ceil),
    authorLabel("四捨五入", EffectRounding::Nearest),
};
static constexpr AuthorEnumDescriptor effectRoundingEnum{
    "EffectRounding", effectRoundingLabels,
};

static constexpr auto effectNumberBaseLabels = []
{
    std::array<AuthorEnumLabel, effectNumberAuthorableBaseCount()> result{};
    std::size_t index{};
    for (const auto& entry : effectNumberBaseCatalog)
    {
        if (entry.authorLabel.empty()) continue;
        result[index++] = authorLabel(entry.authorLabel, entry.base);
    }
    return result;
}();
static constexpr AuthorEnumDescriptor effectNumberBaseEnum{
    "EffectNumberBase", effectNumberBaseLabels,
};

static constexpr std::array selectorKindLabels{
    authorLabel("自身", EffectSelectorKind::Self),
    authorLabel("來源單位", EffectSelectorKind::SourceUnit),
    authorLabel("交易目標", EffectSelectorKind::TransactionTarget),
    authorLabel("命中目標", EffectSelectorKind::HitTarget),
    authorLabel("原攻擊目標", EffectSelectorKind::OriginalAttackTarget),
    authorLabel("羈絆成員", EffectSelectorKind::ComboMembers),
    authorLabel("所有存活單位", EffectSelectorKind::AllLivingUnits),
    authorLabel("友軍", EffectSelectorKind::Allies),
    authorLabel("全隊", EffectSelectorKind::Allies),
    authorLabel("敵軍", EffectSelectorKind::Enemies),
    authorLabel("所有敵人", EffectSelectorKind::Enemies),
    authorLabel("最低生命友軍", EffectSelectorKind::LowestHpAllies),
    authorLabel("最低內力友軍", EffectSelectorKind::LowestMpAllies),
    authorLabel("最高內力敵人", EffectSelectorKind::HighestMpEnemy),
    authorLabel("最強敵人", EffectSelectorKind::StrongestEnemies),
    authorLabel("最近敵人", EffectSelectorKind::NearestEnemies),
    authorLabel("最遠敵人", EffectSelectorKind::FarthestEnemy),
    authorLabel("半徑內單位", EffectSelectorKind::UnitsInRadius),
    authorLabel("方形內單位", EffectSelectorKind::UnitsInSquare),
    authorLabel("指定武學類別友軍", EffectSelectorKind::AlliesUsingMartialCategory),
    authorLabel("狀態持有者", EffectSelectorKind::StatusHolder),
};
static constexpr AuthorEnumDescriptor selectorKindEnum{
    "EffectSelectorKind", selectorKindLabels,
};

static constexpr std::array teamFilterLabels{
    authorLabel("不限", EffectTeamFilter::Any),
    authorLabel("友方", EffectTeamFilter::Ally),
    authorLabel("敵方", EffectTeamFilter::Enemy),
};
static constexpr AuthorEnumDescriptor teamFilterEnum{
    "EffectTeamFilter", teamFilterLabels,
};
static constexpr std::array areaRelationLabels{
    authorLabel("友方", EffectTeamFilter::Ally),
    authorLabel("敵方", EffectTeamFilter::Enemy),
};
static constexpr AuthorEnumDescriptor areaRelationEnum{
    "AreaUnitRelation", areaRelationLabels,
};

static constexpr std::array tieBreakLabels{
    authorLabel("單位ID", EffectTieBreak::UnitId),
    authorLabel("戰鬥亂數", EffectTieBreak::BattleRandom),
};
static constexpr AuthorEnumDescriptor tieBreakEnum{
    "EffectTieBreak", tieBreakLabels,
};

static constexpr std::array requiredTargetLabels{
    authorLabel("自身", EffectRequiredTarget::Self),
    authorLabel("來源單位", EffectRequiredTarget::SourceUnit),
    authorLabel("交易目標", EffectRequiredTarget::TransactionTarget),
    authorLabel("命中目標", EffectRequiredTarget::HitTarget),
    authorLabel("原攻擊目標", EffectRequiredTarget::OriginalAttackTarget),
};
static constexpr AuthorEnumDescriptor requiredTargetEnum{
    "EffectRequiredTarget", requiredTargetLabels,
};

static constexpr std::array stackPolicyLabels{
    authorLabel("獨立", EffectStackPolicy::Independent),
    authorLabel("刷新", EffectStackPolicy::Refresh),
    authorLabel("取代", EffectStackPolicy::Replace),
    authorLabel("保留最強", EffectStackPolicy::KeepStrongest),
    authorLabel("增加層數", EffectStackPolicy::AddStack),
};
static constexpr AuthorEnumDescriptor stackPolicyEnum{
    "EffectStackPolicy", stackPolicyLabels,
};

static constexpr auto statusKindLabels = []
{
    std::array<AuthorEnumLabel, statusCatalog.size()> result{};
    for (std::size_t index = 0; index < statusCatalog.size(); ++index)
    {
        const auto& status = statusCatalog[index];
        result[index] = authorLabel(status.label, status.status);
    }
    return result;
}();
static constexpr AuthorEnumDescriptor statusKindEnum{
    "BattleStatusKind", statusKindLabels,
};

consteval auto makeStatusReapplicationPolicyLabels()
{
    std::array<AuthorEnumLabel,
        statusReapplicationPolicyCatalog.size() - 1> result{};
    for (std::size_t index = 1;
         index < statusReapplicationPolicyCatalog.size();
         ++index)
    {
        const auto& entry = statusReapplicationPolicyCatalog[index];
        result[index - 1] = authorLabel(entry.authorLabel, entry.policy);
    }
    return result;
}
static constexpr auto statusReapplicationPolicyLabels =
    makeStatusReapplicationPolicyLabels();
static constexpr AuthorEnumDescriptor statusReapplicationPolicyEnum{
    "StatusReapplicationPolicy", statusReapplicationPolicyLabels,
};

static constexpr std::array poisonSameEventMergeLabels{
    authorLabel("合計傷害百分比", PoisonSameEventMerge::SumDamagePercent),
};
static constexpr AuthorEnumDescriptor poisonSameEventMergeEnum{
    "PoisonSameEventMerge", poisonSameEventMergeLabels,
};

static constexpr std::array damageChannelLabels{
    authorLabel("招式", DamageChannel::Skill),
    authorLabel("持續傷害", DamageChannel::Dot),
    authorLabel("特效", DamageChannel::Effect),
    authorLabel("全部", DamageChannel::All),
};
static constexpr AuthorEnumDescriptor damageChannelEnum{
    "DamageChannel", damageChannelLabels,
};

static constexpr std::array stateSlotLabels{
    authorLabel("最大招式生命傷害", EffectStateSlot::MaximumSkillHpDamage),
    authorLabel("本次施放最高生命傷害", EffectStateSlot::CastMaximumHpDamage),
    authorLabel("累計吸收傷害", EffectStateSlot::AbsorbedDamage),
    authorLabel("永久施放進展", EffectStateSlot::PermanentCastProgress),
};
static constexpr AuthorEnumDescriptor stateSlotEnum{
    "EffectStateSlot", stateSlotLabels,
};

static constexpr std::array resourceLabels{
    authorLabel("生命", BattleResource::Hp),
    authorLabel("內力", BattleResource::Mp),
    authorLabel("護盾", BattleResource::Shield),
    authorLabel("狀態護盾", BattleResource::StatusShield),
    authorLabel("僵直護盾", BattleResource::StaggerShield),
    authorLabel("目前冷卻", BattleResource::ActiveCooldown),
    authorLabel("僵直吸收幀數", BattleResource::ControlImmunityFrames),
    authorLabel("無敵幀數", BattleResource::InvincibilityFrames),
};
static constexpr AuthorEnumDescriptor resourceEnum{
    "BattleResource", resourceLabels,
};

static constexpr std::array resourceChangeKindLabels{
    authorLabel("回復", ResourceChangeKind::Restore),
    authorLabel("奪取", ResourceChangeKind::Drain),
    authorLabel("獲得", ResourceChangeKind::Grant),
    authorLabel("移除", ResourceChangeKind::Remove),
    authorLabel("轉移", ResourceChangeKind::Transfer),
    authorLabel("至少刷新至", ResourceChangeKind::RefreshToAtLeast),
};
static constexpr AuthorEnumDescriptor resourceChangeKindEnum{
    "ResourceChangeKind", resourceChangeKindLabels,
};

static constexpr auto healKindLabels = []
{
    std::array<AuthorEnumLabel, effectHealKindCatalog.size()> result{};
    for (std::size_t index = 0; index < effectHealKindCatalog.size(); ++index)
    {
        result[index] = authorLabel(
            effectHealKindCatalog[index].authorLabel,
            effectHealKindCatalog[index].kind);
    }
    return result;
}();
static constexpr AuthorEnumDescriptor healKindEnum{
    "EffectHealKind", healKindLabels,
};

static constexpr std::array healSourcePolicyLabels{
    authorLabel("來源必須存活", EffectHealSourcePolicy::RequireAlive),
    authorLabel("允許死亡來源", EffectHealSourcePolicy::AllowDead),
};
static constexpr AuthorEnumDescriptor healSourcePolicyEnum{
    "EffectHealSourcePolicy", healSourcePolicyLabels,
};

static constexpr std::array stackScopeLabels{
    authorLabel("共用", EffectStackScope::Shared),
    authorLabel("事件來源", EffectStackScope::EventSource),
};
static constexpr AuthorEnumDescriptor stackScopeEnum{
    "EffectStackScope", stackScopeLabels,
};

static constexpr std::array borrowedRuleActionCategoryLabels{
    authorLabel("屬性修正", BorrowedRuleActionCategory::AttributeModifier),
    authorLabel("傷害修正", BorrowedRuleActionCategory::DamageModifier),
    authorLabel("資源變更", BorrowedRuleActionCategory::ResourceChange),
    authorLabel("治療交易修正", BorrowedRuleActionCategory::HealTransactionModifier),
    authorLabel("狀態", BorrowedRuleActionCategory::Status),
    authorLabel("傷害", BorrowedRuleActionCategory::Damage),
    authorLabel("攻擊", BorrowedRuleActionCategory::Attack),
    authorLabel("強制移動", BorrowedRuleActionCategory::ForcedMovement),
    authorLabel("區域", BorrowedRuleActionCategory::Area),
    authorLabel("修改施放", BorrowedRuleActionCategory::Cast),
    authorLabel("狀態值", BorrowedRuleActionCategory::StateValue),
    authorLabel("傷害記憶", BorrowedRuleActionCategory::DamageMemory),
    authorLabel("傷害吸收", BorrowedRuleActionCategory::DamageAbsorption),
    authorLabel("狀態傷害結算", BorrowedRuleActionCategory::StatusDamageSettlement),
};
static constexpr AuthorEnumDescriptor borrowedRuleActionCategoryEnum{
    "BorrowedRuleActionCategory", borrowedRuleActionCategoryLabels,
};

static constexpr std::array copiedMagicConditionLabels{
    authorLabel("有絕招攻擊定義", CopiedMagicCondition::HasUltimateAttackDefinition),
    authorLabel("排除複製與借用遞迴", CopiedMagicCondition::ExcludesRecursiveEffects),
};
static constexpr AuthorEnumDescriptor copiedMagicConditionEnum{
    "CopiedMagicCondition", copiedMagicConditionLabels,
};

static constexpr std::array battleAttributeLabels{
    authorLabel("最大生命", BattleAttribute::MaxHp),
    authorLabel("攻擊", BattleAttribute::Attack),
    authorLabel("防禦", BattleAttribute::Defence),
    authorLabel("速度", BattleAttribute::Speed),
    authorLabel("暴擊率", BattleAttribute::CriticalChance),
    authorLabel("暴擊傷害", BattleAttribute::CriticalDamage),
    authorLabel("必中", BattleAttribute::GuaranteedHit),
    authorLabel("閃避率", BattleAttribute::DodgeChance),
    authorLabel("格擋率", BattleAttribute::BlockChance),
    authorLabel("傷害減免", BattleAttribute::DamageReduction),
    authorLabel("技能傷害", BattleAttribute::SkillDamage),
    authorLabel("彈道壓制傷害", BattleAttribute::ProjectilePressureDamage),
    authorLabel("冷卻縮減", BattleAttribute::CooldownReduction),
    authorLabel("內力回復加成", BattleAttribute::MpRecoveryBonus),
    authorLabel("僵直抗性", BattleAttribute::StaggerResistance),
    authorLabel("彈道反射率", BattleAttribute::ProjectileReflectChance),
    authorLabel("技能反彈百分比", BattleAttribute::SkillReflectPercent),
    authorLabel("格擋絕招反擊率", BattleAttribute::CounterUltimateBlockChance),
    authorLabel("閃避後暴擊", BattleAttribute::CriticalAfterDodge),
    authorLabel("滑步機率", BattleAttribute::DashChance),
    authorLabel("攻擊冷卻延長率", BattleAttribute::OutgoingCooldownExtensionChance),
    authorLabel("攻擊冷卻延長百分比", BattleAttribute::OutgoingCooldownExtensionPercent),
    authorLabel("受擊冷卻延長反擊率", BattleAttribute::IncomingCooldownExtensionChance),
    authorLabel("受擊冷卻延長百分比", BattleAttribute::IncomingCooldownExtensionPercent),
};
static constexpr AuthorEnumDescriptor battleAttributeEnum{
    "BattleAttribute", battleAttributeLabels,
};

static constexpr std::array attackPatternKindLabels{
    authorLabel("保留", AttackPatternKind::Preserve),
    authorLabel("扇形", AttackPatternKind::Fan),
    authorLabel("側翼", AttackPatternKind::Flanks),
    authorLabel("同落點延遲", AttackPatternKind::SamePointSequence),
    authorLabel("多目標", AttackPatternKind::MultiTarget),
    authorLabel("最近其他敵人殘影", AttackPatternKind::EchoNearestOthers),
};
static constexpr AuthorEnumDescriptor attackPatternKindEnum{
    "AttackPatternKind", attackPatternKindLabels,
};

static constexpr std::array propagationPolicyLabels{
    authorLabel("來源全部規則", CastPropagationPolicy::SourceRules),
    authorLabel("僅來源命中規則", CastPropagationPolicy::SourceHitRulesOnly),
    authorLabel("不傳播大招規則", CastPropagationPolicy::SuppressUltimateRules),
    authorLabel("借用大招規則", CastPropagationPolicy::BorrowedUltimateRules),
    authorLabel("不傳播效果規則", CastPropagationPolicy::NoEffectRules),
};
static constexpr AuthorEnumDescriptor propagationPolicyEnum{
    "CastPropagationPolicy", propagationPolicyLabels,
};

static constexpr std::array attackRuntimeBehaviorKindLabels{
    authorLabel("彈道彈射", AttackRuntimeBehaviorKind::ProjectileBounce),
    authorLabel("範圍追蹤", AttackRuntimeBehaviorKind::NearbyTracking),
    authorLabel("延遲替代攻擊", AttackRuntimeBehaviorKind::DelayedAlternate),
    authorLabel("擴張螺旋", AttackRuntimeBehaviorKind::ExpandingSpiral),
};
static constexpr AuthorEnumDescriptor attackRuntimeBehaviorKindEnum{
    "AttackRuntimeBehaviorKind", attackRuntimeBehaviorKindLabels,
};

static constexpr std::array areaModifierKindLabels{
    authorLabel("屬性修正", AreaModifierKind::Attribute),
    authorLabel("造成傷害修正", AreaModifierKind::OutgoingDamage),
    authorLabel("攻擊生成修正", AreaModifierKind::AttackSpawn),
    authorLabel("強制移動免疫", AreaModifierKind::ForcedMoveImmunity),
    authorLabel("週期傷害", AreaModifierKind::PeriodicDamage),
    authorLabel("傷害轉移", AreaModifierKind::DamageRedirect),
};
static constexpr AuthorEnumDescriptor areaModifierKindEnum{
    "AreaModifierKind", areaModifierKindLabels,
};

static constexpr std::array forceMoveDirectionLabels{
    authorLabel("遠離來源", ForceMoveDirection::AwayFromSource),
    authorLabel("接近來源", ForceMoveDirection::TowardSource),
};
static constexpr AuthorEnumDescriptor forceMoveDirectionEnum{
    "ForceMoveDirection", forceMoveDirectionLabels,
};
static constexpr std::array areaBlockedDirectionLabels{
    authorLabel("遠離來源", ForceMoveDirection::AwayFromSource),
    authorLabel("接近來源", ForceMoveDirection::TowardSource),
    authorLabel("接近指定點", ForceMoveDirection::TowardPoint),
};
static constexpr AuthorEnumDescriptor areaBlockedDirectionEnum{
    "AreaBlockedDirection", areaBlockedDirectionLabels,
};

static constexpr std::array areaOverlapPolicyLabels{
    authorLabel("相加", AreaOverlapPolicy::Add),
    authorLabel("保留最強", AreaOverlapPolicy::KeepStrongest),
    authorLabel("任一", AreaOverlapPolicy::Any),
};
static constexpr AuthorEnumDescriptor areaOverlapPolicyEnum{
    "AreaOverlapPolicy", areaOverlapPolicyLabels,
};

static constexpr std::array attributeOperationLabels{
    authorLabel("固定加算", AttributeOperation::FlatAdd),
    authorLabel("百分比加算", AttributeOperation::PercentAdd),
    authorLabel("百分點加算", AttributeOperation::PercentagePointAdd),
    authorLabel("覆寫", AttributeOperation::Override),
    authorLabel("乘算", AttributeOperation::Multiply),
    authorLabel("至少為", AttributeOperation::AtLeast),
};
static constexpr AuthorEnumDescriptor attributeOperationEnum{
    "AttributeOperation", attributeOperationLabels,
};

static constexpr std::array damageModifierPerspectiveLabels{
    authorLabel("造成", DamageModifierPerspective::Outgoing),
    authorLabel("承受", DamageModifierPerspective::Incoming),
};
static constexpr AuthorEnumDescriptor damageModifierPerspectiveEnum{
    "DamageModifierPerspective", damageModifierPerspectiveLabels,
};

static constexpr std::array damageModifierStageLabels{
    authorLabel("防禦前", DamageModifierStage::BeforeDefense),
    authorLabel("防禦後", DamageModifierStage::AfterDefense),
    authorLabel("最終", DamageModifierStage::Final),
};
static constexpr AuthorEnumDescriptor damageModifierStageEnum{
    "DamageModifierStage", damageModifierStageLabels,
};

static constexpr std::array damageModifierOperationLabels{
    authorLabel("固定加算", DamageModifierOperation::FlatAdd),
    authorLabel("百分比加算", DamageModifierOperation::PercentAdd),
    authorLabel("乘算", DamageModifierOperation::Multiply),
    authorLabel("忽略防禦百分比", DamageModifierOperation::IgnoreDefensePercent),
    authorLabel("每次承傷不超過最大生命百分比", DamageModifierOperation::CapSingleHitAtMaxHpPercent),
    authorLabel("單次承傷上限", DamageModifierOperation::CapSingleHitAtValue),
    authorLabel("低於最大生命百分比時處決", DamageModifierOperation::ExecuteBelowMaxHpPercent),
};
static constexpr AuthorEnumDescriptor damageModifierOperationEnum{
    "DamageModifierOperation", damageModifierOperationLabels,
};

static constexpr std::array healModifierOperationLabels{
    authorLabel("阻止", HealModifierOperation::Block),
    authorLabel("受到治療乘算", HealModifierOperation::MultiplyReceived),
};
static constexpr AuthorEnumDescriptor healModifierOperationEnum{
    "HealModifierOperation", healModifierOperationLabels,
};

static constexpr std::array statusSourceMatchLabels{
    authorLabel("不限", StatusSourceMatch::Any),
    authorLabel("效果擁有者", StatusSourceMatch::EffectOwner),
    authorLabel("效果綁定", StatusSourceMatch::EffectBinding),
};
static constexpr AuthorEnumDescriptor statusSourceMatchEnum{
    "StatusSourceMatch", statusSourceMatchLabels,
};

static constexpr std::array statusRemovalOrderLabels{
    authorLabel("最長剩餘", StatusRemovalOrder::LongestRemaining),
    authorLabel("最舊", StatusRemovalOrder::Oldest),
    authorLabel("最新", StatusRemovalOrder::Newest),
};
static constexpr AuthorEnumDescriptor statusRemovalOrderEnum{
    "StatusRemovalOrder", statusRemovalOrderLabels,
};

static constexpr std::array damageAreaKindLabels{
    authorLabel("圓形", DamageAreaKind::Circle),
    authorLabel("方形", DamageAreaKind::Square),
};
static constexpr AuthorEnumDescriptor damageAreaKindEnum{
    "DamageAreaKind", damageAreaKindLabels,
};

static constexpr std::array areaProjectileVisualLabels{
    authorLabel("死亡爆炸", AreaProjectileVisual::DeathBlast),
    authorLabel("護盾爆炸", AreaProjectileVisual::ShieldBlast),
};
static constexpr AuthorEnumDescriptor areaProjectileVisualEnum{
    "AreaProjectileVisual", areaProjectileVisualLabels,
};

static constexpr std::array attackTargetPolicyLabels{
    authorLabel("保留", AttackTargetPolicy::Preserve),
    authorLabel("選擇目標", AttackTargetPolicy::SelectedTargets),
    authorLabel("同落點", AttackTargetPolicy::SamePoint),
    authorLabel("同目標", AttackTargetPolicy::SameTarget),
};
static constexpr AuthorEnumDescriptor attackTargetPolicyEnum{
    "AttackTargetPolicy", attackTargetPolicyLabels,
};

static constexpr std::array forceMoveCollisionLabels{
    authorLabel("佔位前停止", ForceMoveCollision::StopBeforeOccupied),
    authorLabel("阻擋前停止", ForceMoveCollision::StopBeforeBlocked),
    authorLabel("遇阻停止", ForceMoveCollision::StopBeforeBlocked),
};
static constexpr AuthorEnumDescriptor forceMoveCollisionEnum{
    "ForceMoveCollision", forceMoveCollisionLabels,
};

static constexpr std::array forceMoveBlockedResultLabels{
    authorLabel("停止", ForceMoveBlockedResult::Stop),
    authorLabel("縮短", ForceMoveBlockedResult::Shorten),
};
static constexpr AuthorEnumDescriptor forceMoveBlockedResultEnum{
    "ForceMoveBlockedResult", forceMoveBlockedResultLabels,
};

static constexpr std::array areaShapeLabels{
    authorLabel("圓形", AreaShape::Circle),
    authorLabel("棋格方形", AreaShape::GridSquare),
};
static constexpr AuthorEnumDescriptor areaShapeEnum{
    "AreaShape", areaShapeLabels,
};

static constexpr std::array areaAnchorLabels{
    authorLabel("命中位置", AreaAnchor::HitPosition),
    authorLabel("跟隨來源", AreaAnchor::FollowSourceUnit),
};
static constexpr AuthorEnumDescriptor areaAnchorEnum{
    "AreaAnchor", areaAnchorLabels,
};

static constexpr std::array areaSourceDeathPolicyLabels{
    authorLabel("保留至到期", AreaSourceDeathPolicy::PersistUntilExpiry),
    authorLabel("立即移除", AreaSourceDeathPolicy::RemoveImmediately),
};
static constexpr AuthorEnumDescriptor areaSourceDeathPolicyEnum{
    "AreaSourceDeathPolicy", areaSourceDeathPolicyLabels,
};

static constexpr std::array areaMergePolicyLabels{
    authorLabel("獨立", AreaMergePolicy::Independent),
    authorLabel("同來源刷新", AreaMergePolicy::RefreshSameSource),
    authorLabel("同來源取代", AreaMergePolicy::ReplaceSameSource),
};
static constexpr AuthorEnumDescriptor areaMergePolicyEnum{
    "AreaMergePolicy", areaMergePolicyLabels,
};

static constexpr std::array castRangeModeLabels{
    authorLabel("保留", CastRangeMode::Preserve),
    authorLabel("遠程", CastRangeMode::Ranged),
};
static constexpr AuthorEnumDescriptor castRangeModeEnum{
    "CastRangeMode", castRangeModeLabels,
};

static constexpr std::array castMobilityPolicyLabels{
    authorLabel("保留", CastMobilityPolicy::Preserve),
    authorLabel("滑步攻擊", CastMobilityPolicy::DashAttack),
    authorLabel("閃擊", CastMobilityPolicy::BlinkAttack),
};
static constexpr AuthorEnumDescriptor castMobilityPolicyEnum{
    "CastMobilityPolicy", castMobilityPolicyLabels,
};

static constexpr std::array observationScopeLabels{
    authorLabel("效果擁有者", EffectObservationScope::Owner),
    authorLabel("效果擁有者同隊事件來源", EffectObservationScope::OwnerTeamEventSource),
    authorLabel("事件目標", EffectObservationScope::EventTarget),
    authorLabel("狀態持有者事件來源", EffectObservationScope::StatusHolderEventSource),
    authorLabel("狀態持有者事件目標", EffectObservationScope::StatusHolderEventTarget),
    authorLabel("狀態來源事件來源", EffectObservationScope::StatusSourceEventSource),
    authorLabel("來源效果擁有者同隊事件來源", EffectObservationScope::SourceOwnerTeamEventSource),
};
static constexpr AuthorEnumDescriptor observationScopeEnum{
    "EffectObservationScope", observationScopeLabels,
};

static constexpr std::array castMatchLabels{
    authorLabel("綁定武功", EffectCastMatch::BoundMagic),
    authorLabel("效果擁有者任意施放", EffectCastMatch::OwnerAnyCast),
};
static constexpr AuthorEnumDescriptor castMatchEnum{
    "EffectCastMatch", castMatchLabels,
};

static constexpr std::array martialCategoryLabels{
    authorLabel("拳掌", EffectMartialCategory::Fist),
    authorLabel("御劍", EffectMartialCategory::Sword),
    authorLabel("耍刀", EffectMartialCategory::Knife),
    authorLabel("特殊", EffectMartialCategory::Unusual),
};
static constexpr AuthorEnumDescriptor martialCategoryEnum{
    "EffectMartialCategory", martialCategoryLabels,
};

static constexpr std::array authorEnumDescriptors{
    &activationScopeEnum,
    &battleDamageKindEnum,
    &effectRoundingEnum,
    &effectNumberBaseEnum,
    &selectorKindEnum,
    &teamFilterEnum,
    &areaRelationEnum,
    &tieBreakEnum,
    &requiredTargetEnum,
    &stackPolicyEnum,
    &statusKindEnum,
    &damageChannelEnum,
    &stateSlotEnum,
    &resourceEnum,
    &resourceChangeKindEnum,
    &healKindEnum,
    &healSourcePolicyEnum,
    &stackScopeEnum,
    &borrowedRuleActionCategoryEnum,
    &copiedMagicConditionEnum,
    &battleAttributeEnum,
    &attackPatternKindEnum,
    &propagationPolicyEnum,
    &attackRuntimeBehaviorKindEnum,
    &areaModifierKindEnum,
    &forceMoveDirectionEnum,
    &areaBlockedDirectionEnum,
    &areaOverlapPolicyEnum,
    &attributeOperationEnum,
    &damageModifierPerspectiveEnum,
    &damageModifierStageEnum,
    &damageModifierOperationEnum,
    &healModifierOperationEnum,
    &statusSourceMatchEnum,
    &statusRemovalOrderEnum,
    &damageAreaKindEnum,
    &areaProjectileVisualEnum,
    &attackTargetPolicyEnum,
    &forceMoveCollisionEnum,
    &forceMoveBlockedResultEnum,
    &areaShapeEnum,
    &areaAnchorEnum,
    &areaSourceDeathPolicyEnum,
    &areaMergePolicyEnum,
    &castRangeModeEnum,
    &castMobilityPolicyEnum,
    &observationScopeEnum,
    &castMatchEnum,
    &martialCategoryEnum,
};

inline bool validatePayloadNodeShape(
    const YAML::Node& node,
    const PayloadFieldDescriptor& field,
    std::string& error)
{
    const auto reject = [&]
    {
        error = std::format("「{}」欄位外形不符合 payload descriptor", field.name);
        return false;
    };
    switch (field.shape)
    {
    case PayloadNodeShape::Any:
        return true;
    case PayloadNodeShape::Scalar:
    case PayloadNodeShape::String:
        return node.IsScalar() || reject();
    case PayloadNodeShape::Integer:
        if (!node.IsScalar()) return reject();
        try
        {
            static_cast<void>(node.as<int>());
            return true;
        }
        catch (const YAML::Exception&)
        {
            return reject();
        }
    case PayloadNodeShape::Boolean:
        if (!node.IsScalar()) return reject();
        try
        {
            static_cast<void>(node.as<bool>());
            return true;
        }
        catch (const YAML::Exception&)
        {
            return reject();
        }
    case PayloadNodeShape::Map:
    case PayloadNodeShape::ActionNode:
        return node.IsMap() || reject();
    case PayloadNodeShape::Sequence:
    case PayloadNodeShape::ActionList:
    case PayloadNodeShape::ConditionList:
        return node.IsSequence() || reject();
    case PayloadNodeShape::Number:
    case PayloadNodeShape::Selector:
        return (node.IsScalar() || node.IsMap()) || reject();
    case PayloadNodeShape::StringOrSequence:
        return (node.IsScalar() || node.IsSequence()) || reject();
    }
    assert(false);
    return false;
}

class PayloadView
{
public:
    PayloadView(const YAML::Node& node, const PayloadDescriptor& descriptor)
        : node_(node), descriptor_(descriptor)
    {
    }

    bool validate(std::string& error) const
    {
        if (!node_ || !node_.IsMap())
        {
            error = std::format("payload「{}」必須是映射表", descriptor_.name);
            return false;
        }
        std::set<std::string> supplied;
        for (const auto& entry : node_)
        {
            const auto key = entry.first.as<std::string>();
            if (!supplied.insert(key).second)
            {
                error = std::format("重複欄位「{}」", key);
                return false;
            }
            const auto* field = findField(key);
            if (!field)
            {
                if (isDynamicPayloadKey(descriptor_.dynamicKeyClass, key))
                {
                    const PayloadFieldDescriptor dynamicField{
                        key, true, descriptor_.dynamicValueShape,
                    };
                    if (!validatePayloadNodeShape(entry.second, dynamicField, error)) return false;
                    continue;
                }
                error = std::format("未知欄位「{}」", key);
                return false;
            }
            if (!validatePayloadNodeShape(entry.second, *field, error)) return false;
        }
        for (const auto& field : descriptor_.fields)
        {
            if (field.required && !node_[std::string(field.name)])
            {
                error = std::format("缺少「{}」欄位", field.name);
                return false;
            }
        }
        return true;
    }

    YAML::Node operator[](std::string_view key) const
    {
        const auto field = node_[std::string(key)];
        if (field) consumed_.insert(std::string(key));
        return field;
    }

    std::vector<std::pair<std::string, YAML::Node>> dynamicEntries() const
    {
        std::vector<std::pair<std::string, YAML::Node>> entries;
        for (const auto& entry : node_)
        {
            const auto key = entry.first.as<std::string>();
            if (findField(key)) continue;
            assert(isDynamicPayloadKey(descriptor_.dynamicKeyClass, key));
            consumed_.insert(key);
            entries.emplace_back(key, entry.second);
        }
        return entries;
    }

    bool finish(std::string& error) const
    {
        for (const auto& entry : node_)
        {
            const auto key = entry.first.as<std::string>();
            if (!consumed_.contains(key))
            {
                error = std::format(
                    "payload「{}」的欄位「{}」通過外形驗證但未被 typed parser 消耗",
                    descriptor_.name,
                    key);
                return false;
            }
        }
        return true;
    }

private:
    const PayloadFieldDescriptor* findField(std::string_view name) const
    {
        const auto found = std::ranges::find(descriptor_.fields, name, &PayloadFieldDescriptor::name);
        return found == descriptor_.fields.end() ? nullptr : &*found;
    }

    YAML::Node node_;
    const PayloadDescriptor& descriptor_;
    mutable std::set<std::string> consumed_;
};

template <typename Node>
bool requiredString(const Node& node, std::string_view key, std::string& value, std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field)
    {
        error = std::format("缺少「{}」欄位", key);
        return false;
    }
    try
    {
        value = field.template as<std::string>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效字串: {}", key, ex.what());
        return false;
    }
}

template <typename Node>
bool optionalInt(const Node& node, std::string_view key, int& value, std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field) return true;
    try
    {
        value = field.template as<int>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效整數: {}", key, ex.what());
        return false;
    }
}

template <typename Node>
bool requiredInt(const Node& node, std::string_view key, int& value, std::string& error)
{
    if (!node[std::string(key)])
    {
        error = std::format("缺少「{}」欄位", key);
        return false;
    }
    return optionalInt(node, key, value, error);
}

static constexpr std::array activationLimitFields{
    PayloadFieldDescriptor{
        "範圍", true, PayloadNodeShape::String, "每次施放每個目標", {},
        PayloadSchemaReference::None, &activationScopeEnum,
    },
    PayloadFieldDescriptor{ "次數", true, PayloadNodeShape::Integer, "1" },
};
static constexpr PayloadDescriptor activationLimitPayload{
    "觸發限制", activationLimitFields, R"(範圍: 每次施放每個目標
次數: 1)",
};

inline bool parseActivationLimitNode(
    const YAML::Node& node,
    EffectActivationLimit& out,
    std::string& error)
{
    PayloadView payload(node, activationLimitPayload);
    if (!payload.validate(error)) return false;

    std::string scopeLabel;
    if (!requiredString(payload, "範圍", scopeLabel, error)) return false;
    const auto scope = parseLabel<EffectActivationScope>(scopeLabel, activationScopeEnum);
    if (!scope)
    {
        error = std::format("未知觸發限制範圍「{}」", scopeLabel);
        return false;
    }

    out = {};
    out.scope = *scope;
    return requiredInt(payload, "次數", out.maxEvaluations, error)
        && payload.finish(error);
}

template <typename Node>
bool optionalBool(const Node& node, std::string_view key, bool& value, std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field) return true;
    try
    {
        value = field.template as<bool>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效布林值: {}", key, ex.what());
        return false;
    }
}

template <typename Node>
bool optionalBool(const Node& node,
                  std::string_view key,
                  std::optional<bool>& value,
                  std::string& error)
{
    const auto field = node[std::string(key)];
    if (!field) return true;
    try
    {
        value = field.template as<bool>();
        return true;
    }
    catch (const YAML::Exception& ex)
    {
        error = std::format("「{}」不是有效布林值: {}", key, ex.what());
        return false;
    }
}

inline bool parseDamageKindLabel(
    std::string_view label,
    BattleDamageKind& out,
    std::string& error)
{
    const auto parsed = parseLabel<BattleDamageKind>(label, battleDamageKindEnum);
    if (!parsed)
    {
        error = std::format("未知傷害種類「{}」", label);
        return false;
    }
    out = *parsed;
    return true;
}

bool parseStatusKind(std::string_view label, BattleStatusKind& out, std::string& error);
bool parseEffectStateSlot(const YAML::Node& node, EffectStateSlot& out, std::string& error);

static constexpr std::array effectNumberFields{
    PayloadFieldDescriptor{
        "基準", false, PayloadNodeShape::String, "來源攻擊", {},
        PayloadSchemaReference::None, &effectNumberBaseEnum,
    },
    PayloadFieldDescriptor{
        "乘數基準", false, PayloadNodeShape::String, "來源星級", {},
        PayloadSchemaReference::None, &effectNumberBaseEnum,
    },
    PayloadFieldDescriptor{
        "來源狀態數量", false, PayloadNodeShape::String, "毒爆", {},
        PayloadSchemaReference::None, &statusKindEnum,
    },
    PayloadFieldDescriptor{
        "狀態來源", false, PayloadNodeShape::String, "效果擁有者", {},
        PayloadSchemaReference::None, &statusSourceMatchEnum,
    },
    PayloadFieldDescriptor{
        "狀態槽", false, PayloadNodeShape::String, "最大招式生命傷害", {},
        PayloadSchemaReference::None, &stateSlotEnum,
    },
    PayloadFieldDescriptor{ "固定", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{ "百分比", false, PayloadNodeShape::Integer, "100" },
    PayloadFieldDescriptor{
        "取整", false, PayloadNodeShape::String, "向下", {},
        PayloadSchemaReference::None, &effectRoundingEnum,
    },
    PayloadFieldDescriptor{ "最小", false, PayloadNodeShape::Integer, "0" },
    PayloadFieldDescriptor{ "最大", false, PayloadNodeShape::Integer, "100" },
    PayloadFieldDescriptor{ "目標最大生命百分比", false, PayloadNodeShape::Integer, "10", "{}" },
    PayloadFieldDescriptor{ "來源最大生命百分比", false, PayloadNodeShape::Integer, "10", "{}" },
    PayloadFieldDescriptor{ "目標目前生命百分比", false, PayloadNodeShape::Integer, "10", "{}" },
    PayloadFieldDescriptor{ "目標目前護盾百分比", false, PayloadNodeShape::Integer, "10", "{}" },
    PayloadFieldDescriptor{ "實際生命傷害百分比", false, PayloadNodeShape::Integer, "10", "{}" },
    PayloadFieldDescriptor{ "每星級", false, PayloadNodeShape::Integer, "10", "{}" },
};
static constexpr PayloadDescriptor effectNumberPayload{
    "效果數值", effectNumberFields, "固定: 1", PayloadDynamicKeyClass::None,
    PayloadNodeShape::Any, {}, {}, 1,
};

inline bool parseEffectNumberNode(const YAML::Node& node, EffectNumber& out, std::string& error)
{
    if (!node)
    {
        error = "缺少「數值」欄位";
        return false;
    }
    if (node.IsScalar())
    {
        try
        {
            out = {};
            out.flat = node.as<int>();
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            error = std::format("數值不是有效整數: {}", ex.what());
            return false;
        }
    }

    PayloadView payload(node, effectNumberPayload);
    if (!payload.validate(error)) return false;

    struct NumberAlias
    {
        std::string_view field;
        EffectNumberBase base;
        int percentMultiplier;
    };
    static constexpr std::array aliases{
        NumberAlias{ "目標最大生命百分比", EffectNumberBase::TargetMaxHp, 1 },
        NumberAlias{ "來源最大生命百分比", EffectNumberBase::SourceMaxHp, 1 },
        NumberAlias{ "目標目前生命百分比", EffectNumberBase::TargetCurrentHp, 1 },
        NumberAlias{ "目標目前護盾百分比", EffectNumberBase::TargetCurrentShield, 1 },
        NumberAlias{ "實際生命傷害百分比", EffectNumberBase::FinalHpDamage, 1 },
        NumberAlias{ "每星級", EffectNumberBase::SourceStar, 100 },
    };
    const NumberAlias* matchedAlias = nullptr;
    for (const auto& alias : aliases)
    {
        if (!payload[alias.field]) continue;
        if (matchedAlias)
        {
            error = "簡式數值只能使用一個基準別名";
            return false;
        }
        matchedAlias = &alias;
    }
    if (matchedAlias)
    {
        int value{};
        if (!requiredInt(payload, matchedAlias->field, value, error)) return false;
        if (matchedAlias->percentMultiplier != 1
            && (value > std::numeric_limits<int>::max() / matchedAlias->percentMultiplier
                || value < std::numeric_limits<int>::min() / matchedAlias->percentMultiplier))
        {
            error = std::format("「{}」超出有效範圍", matchedAlias->field);
            return false;
        }
        out = {};
        out.base = matchedAlias->base;
        out.percent = value * matchedAlias->percentMultiplier;
        if (const auto rounding = payload["取整"])
        {
            const auto label = rounding.as<std::string>();
            const auto parsed = parseLabel<EffectRounding>(label, effectRoundingEnum);
            if (!parsed)
            {
                error = std::format("未知取整方式「{}」", label);
                return false;
            }
            out.rounding = *parsed;
        }
        if (payload["最小"])
        {
            int value{};
            if (!requiredInt(payload, "最小", value, error)) return false;
            out.minimum = value;
        }
        if (payload["最大"])
        {
            int value{};
            if (!requiredInt(payload, "最大", value, error)) return false;
            out.maximum = value;
        }
        return payload.finish(error);
    }
    out = {};
    auto readBase = [&](const YAML::Node& base, EffectNumberBase& destination)
    {
        const auto label = base.as<std::string>();
        const auto parsed = parseLabel<EffectNumberBase>(label, effectNumberBaseEnum);
        if (!parsed)
        {
            error = std::format("未知數值基準「{}」", label);
            return false;
        }
        destination = *parsed;
        return true;
    };
    if (const auto base = payload["基準"])
    {
        if (!readBase(base, out.base)) return false;
    }
    if (const auto multiplier = payload["乘數基準"])
    {
        EffectNumberBase parsedMultiplier{};
        if (!readBase(multiplier, parsedMultiplier)) return false;
        out.multiplierBase = parsedMultiplier;
    }
    const bool hasSourceStatusQuantity = static_cast<bool>(payload["來源狀態數量"]);
    if (hasSourceStatusQuantity && payload["基準"])
    {
        error = "來源狀態參照不可再指定「基準」";
        return false;
    }
    if (hasSourceStatusQuantity)
    {
        BattleStatusKind parsedStatus{};
        const auto label = payload["來源狀態數量"].as<std::string>();
        if (!parseStatusKind(label, parsedStatus, error)) return false;
        out.base = EffectNumberBase::SourceStatusQuantity;
        out.status = parsedStatus;
    }
    if (const auto source = payload["狀態來源"])
    {
        const auto label = source.as<std::string>();
        const auto parsed = parseLabel<StatusSourceMatch>(label, statusSourceMatchEnum);
        if (!parsed)
        {
            error = std::format("未知狀態來源「{}」", label);
            return false;
        }
        out.statusSource = *parsed;
    }
    if (payload["狀態槽"])
    {
        EffectStateSlot slot{};
        if (!parseEffectStateSlot(payload["狀態槽"], slot, error)) return false;
        out.stateSlot = slot;
    }
    if (!optionalInt(payload, "固定", out.flat, error)
        || !optionalInt(payload, "百分比", out.percent, error)) return false;
    if ((out.base != EffectNumberBase::Constant || out.multiplierBase)
        && !payload["百分比"])
    {
        out.percent = 100;
    }
    if (const auto rounding = payload["取整"])
    {
        const auto label = rounding.as<std::string>();
        const auto parsed = parseLabel<EffectRounding>(label, effectRoundingEnum);
        if (!parsed)
        {
            error = std::format("未知取整方式「{}」", label);
            return false;
        }
        out.rounding = *parsed;
    }
    if (payload["最小"])
    {
        int value{};
        if (!requiredInt(payload, "最小", value, error)) return false;
        out.minimum = value;
    }
    if (payload["最大"])
    {
        int value{};
        if (!requiredInt(payload, "最大", value, error)) return false;
        out.maximum = value;
    }
    return payload.finish(error);
}

static constexpr std::array selectorFields{
    PayloadFieldDescriptor{
        "類型", true, PayloadNodeShape::String, "半徑內單位", {},
        PayloadSchemaReference::None, &selectorKindEnum,
    },
    PayloadFieldDescriptor{ "數量", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "半徑格數", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{
        "方形邊長", false, PayloadNodeShape::Integer, "3", "類型: 方形內單位",
    },
    PayloadFieldDescriptor{
        "隊伍", false, PayloadNodeShape::String, "敵方", {},
        PayloadSchemaReference::None, &teamFilterEnum,
    },
    PayloadFieldDescriptor{
        "平手", false, PayloadNodeShape::String, "戰鬥亂數", {},
        PayloadSchemaReference::None, &tieBreakEnum,
    },
    PayloadFieldDescriptor{ "排除效果擁有者", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "使用此武功", false, PayloadNodeShape::Boolean, "true", "類型: 友軍" },
    PayloadFieldDescriptor{
        "武學類別", false, PayloadNodeShape::String, "御劍", "類型: 指定武學類別友軍",
        PayloadSchemaReference::None, &martialCategoryEnum,
    },
    PayloadFieldDescriptor{
        "必含目標", false, PayloadNodeShape::String, "自身", {},
        PayloadSchemaReference::None, &requiredTargetEnum,
    },
};
static constexpr PayloadDescriptor selectorPayload{
    "目標選擇器", selectorFields, R"(類型: 半徑內單位
半徑格數: 3)",
};

inline bool parseSelectorNode(const YAML::Node& node, EffectSelector& out, std::string& error)
{
    out = {};
    std::string label;
    std::optional<PayloadView> payload;
    if (node.IsScalar()) label = node.as<std::string>();
    else
    {
        payload.emplace(node, selectorPayload);
        if (!payload->validate(error)
            || !requiredString(*payload, "類型", label, error)) return false;
    }

    const auto kind = parseLabel<EffectSelectorKind>(label, selectorKindEnum);
    if (!kind)
    {
        error = std::format("未知目標選擇器「{}」", label);
        return false;
    }
    out.kind = *kind;
    if (payload)
    {
        bool acceptsCount{};
        bool acceptsTie{};
        bool acceptsMagic{};
        bool acceptsRequiredTarget{};
        bool acceptsRadius{};
        bool acceptsSquare{};
        bool acceptsTeam{};
        bool acceptsMartialCategory{};
        switch (out.kind)
        {
        case EffectSelectorKind::Self:
        case EffectSelectorKind::SourceUnit:
        case EffectSelectorKind::TransactionTarget:
        case EffectSelectorKind::HitTarget:
        case EffectSelectorKind::OriginalAttackTarget:
        case EffectSelectorKind::StatusHolder:
            break;
        case EffectSelectorKind::ComboMembers:
        case EffectSelectorKind::AllLivingUnits:
        case EffectSelectorKind::Allies:
        case EffectSelectorKind::Enemies:
            acceptsMagic = true;
            [[fallthrough]];
        case EffectSelectorKind::LowestHpAllies:
        case EffectSelectorKind::LowestMpAllies:
        case EffectSelectorKind::StrongestEnemies:
        case EffectSelectorKind::NearestEnemies:
            acceptsCount = true;
            acceptsTie = true;
            acceptsRequiredTarget = true;
            break;
        case EffectSelectorKind::HighestMpEnemy:
        case EffectSelectorKind::FarthestEnemy:
            acceptsTie = true;
            break;
        case EffectSelectorKind::UnitsInRadius:
            acceptsRadius = true;
            [[fallthrough]];
        case EffectSelectorKind::UnitsInSquare:
            acceptsSquare = out.kind == EffectSelectorKind::UnitsInSquare;
            acceptsCount = true;
            acceptsTie = true;
            acceptsTeam = true;
            acceptsRequiredTarget = true;
            break;
        case EffectSelectorKind::AlliesUsingMartialCategory:
            acceptsMartialCategory = true;
            acceptsRequiredTarget = true;
            break;
        }
        if (!optionalBool(*payload, "排除效果擁有者", out.excludeOwner, error)
            || (acceptsMagic && !optionalBool(*payload, "使用此武功", out.requiredBoundMagic, error))
            || (acceptsCount && !optionalInt(*payload, "數量", out.count, error))
            || (acceptsRadius && !optionalInt(*payload, "半徑格數", out.radiusTiles, error))
            || (acceptsSquare && !optionalInt(*payload, "方形邊長", out.squareSideTiles, error))
            ) return false;
        if (acceptsMartialCategory)
        {
            const auto category = (*payload)["武學類別"];
            if (category)
            {
                const auto label = category.as<std::string>();
                const auto parsed = parseLabel<EffectMartialCategory>(label, martialCategoryEnum);
                if (!parsed)
                {
                    error = std::format("未知武學類別「{}」", label);
                    return false;
                }
                out.requiredMartialCategory = *parsed;
            }
        }
        if (acceptsTeam)
        {
            if (const auto team = (*payload)["隊伍"])
            {
                const auto teamLabel = team.as<std::string>();
                const auto parsed = parseLabel<EffectTeamFilter>(teamLabel, teamFilterEnum);
                if (!parsed)
                {
                    error = std::format("未知隊伍篩選「{}」", teamLabel);
                    return false;
                }
                out.team = *parsed;
            }
        }
        if (acceptsTie)
        {
            if (const auto tie = (*payload)["平手"])
            {
                const auto tieLabel = tie.as<std::string>();
                const auto parsed = parseLabel<EffectTieBreak>(tieLabel, tieBreakEnum);
                if (!parsed)
                {
                    error = std::format("未知平手規則「{}」", tieLabel);
                    return false;
                }
                out.tieBreak = *parsed;
            }
        }
        if (acceptsRequiredTarget)
        {
            if (const auto required = (*payload)["必含目標"])
            {
                const auto requiredLabel = required.as<std::string>();
                const auto parsed = parseLabel<EffectRequiredTarget>(requiredLabel, requiredTargetEnum);
                if (!parsed)
                {
                    error = std::format("未知必含目標「{}」", requiredLabel);
                    return false;
                }
                out.requiredTarget = *parsed;
            }
        }
        if (!payload->finish(error)) return false;
    }
    if (out.count < 0)
    {
        error = "目標數量不可為負數";
        return false;
    }
    if (out.kind == EffectSelectorKind::AlliesUsingMartialCategory
        && out.requiredMartialCategory == EffectMartialCategory::None)
    {
        error = "指定武學類別友軍需要「武學類別」";
        return false;
    }
    return true;
}

static constexpr std::array timingDescriptors{
    TimingDescriptor{ "開場", EffectEvent::BattleInitialized, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "每幀", EffectEvent::FrameAdvanced, EffectSelectorKind::Self, TimingIntervalPolicy::Forbidden, TimingIntent::None },
    TimingDescriptor{ "每隔", EffectEvent::FrameAdvanced, EffectSelectorKind::Self, TimingIntervalPolicy::RequiredPositive, TimingIntent::None },
    TimingDescriptor{ "絕招冷卻完成", EffectEvent::UltimateCooldownFinished, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "施放規劃", EffectEvent::CastPlanned, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "攻擊提交", EffectEvent::AttackCommitted, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "絕招施放", EffectEvent::UltimateCommitted, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "攻擊生成", EffectEvent::AttackSpawned, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "主彈命中", EffectEvent::MainProjectileBeforeDamage, EffectSelectorKind::HitTarget, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "命中", EffectEvent::HitBeforeDamage, EffectSelectorKind::HitTarget, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "治療嘗試", EffectEvent::HealAttempted, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "治療套用", EffectEvent::HealApplied, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "施放延續", EffectEvent::CastContinuation, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "施放結算完成", EffectEvent::CastSettled, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "護盾破裂", EffectEvent::ShieldBroken, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "單位死亡", EffectEvent::UnitDied, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "友軍死亡", EffectEvent::AllyDied, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
    TimingDescriptor{ "造成傷害後", EffectEvent::DamageResolved, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::DamageDealt },
    TimingDescriptor{ "受傷後", EffectEvent::DamageResolved, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::DamageReceived },
    TimingDescriptor{ "擊殺後", EffectEvent::DamageResolved, EffectSelectorKind::Self, TimingIntervalPolicy::Unrestricted, TimingIntent::Kill },
    TimingDescriptor{ "持續", EffectEvent::StatusPersistent, EffectSelectorKind::StatusHolder, TimingIntervalPolicy::Unrestricted, TimingIntent::None },
};

static constexpr std::array<PayloadFieldDescriptor, 0> emptyPayloadFields{};
static constexpr std::array conditionPercentFields{
    PayloadFieldDescriptor{ "百分比", true, PayloadNodeShape::Integer, "30" },
};
static constexpr std::array conditionStatusFields{
    PayloadFieldDescriptor{
        "狀態", true, PayloadNodeShape::String, "中毒", {},
        PayloadSchemaReference::None, &statusKindEnum,
    },
};
static constexpr std::array conditionStackFields{
    PayloadFieldDescriptor{
        "狀態", true, PayloadNodeShape::String, "戰意", {},
        PayloadSchemaReference::None, &statusKindEnum,
    },
    PayloadFieldDescriptor{ "層數", true, PayloadNodeShape::Integer, "2" },
};
static constexpr std::array conditionCountFields{
    PayloadFieldDescriptor{ "數量", true, PayloadNodeShape::Integer, "2" },
};
static constexpr std::array conditionOrdinalFields{
    PayloadFieldDescriptor{ "序號", true, PayloadNodeShape::Integer, "1" },
};
static constexpr std::array conditionHealKindsFields{
    PayloadFieldDescriptor{
        "治療種類", true, PayloadNodeShape::Sequence, R"(- 命中
- 吸血)", {}, PayloadSchemaReference::None, &healKindEnum,
    },
};
static constexpr std::array conditionDamageKindsFields{
    PayloadFieldDescriptor{
        "傷害種類", true, PayloadNodeShape::Sequence, R"(- 招式
- 特效)", {}, PayloadSchemaReference::None, &damageChannelEnum,
    },
};
static constexpr std::array conditionAcceptedHitFields{
    PayloadFieldDescriptor{ "需要正傷害", false, PayloadNodeShape::Boolean, "true" },
};

static constexpr PayloadDescriptor conditionEmptyPayload{ "無參數條件", emptyPayloadFields, "{}" };
static constexpr PayloadDescriptor conditionPercentPayload{ "百分比條件", conditionPercentFields, "百分比: 30" };
static constexpr PayloadDescriptor conditionStatusPayload{ "狀態條件", conditionStatusFields, "狀態: 中毒" };
static constexpr PayloadDescriptor conditionStackPayload{
    "自身層數至少", conditionStackFields, R"(狀態: 戰意
層數: 2)",
};
static constexpr PayloadDescriptor conditionCountPayload{ "數量條件", conditionCountFields, "數量: 2" };
static constexpr PayloadDescriptor conditionOrdinalPayload{ "攻擊序號", conditionOrdinalFields, "序號: 1" };
static constexpr PayloadDescriptor conditionHealKindsPayload{
    "治療種類符合", conditionHealKindsFields, R"(治療種類:
  - 命中)",
};
static constexpr PayloadDescriptor conditionDamageKindsPayload{
    "傷害種類符合", conditionDamageKindsFields, R"(傷害種類:
  - 招式)",
};
static constexpr PayloadDescriptor conditionAcceptedHitPayload{ "已接受命中", conditionAcceptedHitFields, "{}" };

static constexpr std::array conditionDescriptors{
    ConditionDescriptor{ "僅限絕招", 0, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "施放武功為效果來源", 1, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "僅限主彈道", 2, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "僅限根攻擊", 3, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "自身生命不高於", 4, ConditionAuthorForm::SingleParameter, "百分比", &conditionPercentPayload },
    ConditionDescriptor{ "自身生命低於", 5, ConditionAuthorForm::SingleParameter, "百分比", &conditionPercentPayload },
    ConditionDescriptor{ "自身為最後存活", 6, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "目標生命不高於", 7, ConditionAuthorForm::SingleParameter, "百分比", &conditionPercentPayload },
    ConditionDescriptor{ "目標非無敵", 8, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "自身有狀態", 9, ConditionAuthorForm::SingleParameter, "狀態", &conditionStatusPayload },
    ConditionDescriptor{ "目標有狀態", 10, ConditionAuthorForm::SingleParameter, "狀態", &conditionStatusPayload },
    ConditionDescriptor{ "目標有此來源狀態", 11, ConditionAuthorForm::SingleParameter, "狀態", &conditionStatusPayload },
    ConditionDescriptor{ "自身層數至少", 12, ConditionAuthorForm::Map, {}, &conditionStackPayload },
    ConditionDescriptor{ "其他存活友軍使用此武功", 13, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "不同目標數至少", 14, ConditionAuthorForm::SingleParameter, "數量", &conditionCountPayload },
    ConditionDescriptor{ "攻擊序號", 15, ConditionAuthorForm::SingleParameter, "序號", &conditionOrdinalPayload },
    ConditionDescriptor{ "治療種類符合", 16, ConditionAuthorForm::SingleParameter, "治療種類", &conditionHealKindsPayload },
    ConditionDescriptor{ "傷害來自招式", 17, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "傷害造成死亡", 18, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "已接受命中", 19, ConditionAuthorForm::ScalarOrMap, {}, &conditionAcceptedHitPayload },
    ConditionDescriptor{ "事件目標屬於綁定來源", 20, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "傷害種類符合", 22, ConditionAuthorForm::SingleParameter, "傷害種類", &conditionDamageKindsPayload },
    ConditionDescriptor{ "受益者施放前滿內", 23, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "有合法隨機目標", 24, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload },
    ConditionDescriptor{ "目標為狀態持有者", 25, ConditionAuthorForm::Scalar, {}, &conditionEmptyPayload, true },
};

static constexpr std::array attackRuntimeBehaviorFields{
    PayloadFieldDescriptor{
        "類型", true, PayloadNodeShape::String, "彈道彈射", {},
        PayloadSchemaReference::None, &attackRuntimeBehaviorKindEnum,
    },
    PayloadFieldDescriptor{ "追加命中次數", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{ "機率", false, PayloadNodeShape::Integer, "50" },
    PayloadFieldDescriptor{ "範圍像素", false, PayloadNodeShape::Integer, "120" },
    PayloadFieldDescriptor{
        "傷害倍率", false, PayloadNodeShape::Integer, "80", R"(類型: 範圍追蹤
範圍像素: 120
傷害倍率: 100)",
    },
    PayloadFieldDescriptor{
        "延遲幀數", false, PayloadNodeShape::Integer, "10", R"(類型: 延遲替代攻擊
延遲幀數: 5
傷害倍率: 100
攻擊者獲得格擋機率: 20)",
    },
    PayloadFieldDescriptor{
        "攻擊者獲得格擋機率", false, PayloadNodeShape::Integer, "20", R"(類型: 延遲替代攻擊
延遲幀數: 5
傷害倍率: 100
攻擊者獲得格擋機率: 20)",
    },
    PayloadFieldDescriptor{
        "彈道數量", false, PayloadNodeShape::Integer, "4", R"(類型: 擴張螺旋
彈道數量: 4
流血層數: 1)",
    },
    PayloadFieldDescriptor{
        "流血層數", false, PayloadNodeShape::Integer, "1", R"(類型: 擴張螺旋
彈道數量: 4
流血層數: 1)",
    },
};
static constexpr PayloadDescriptor attackRuntimeBehaviorPayload{
    "攻擊執行行為", attackRuntimeBehaviorFields, R"(類型: 彈道彈射
追加命中次數: 1
機率: 50
範圍像素: 120)",
};

static constexpr std::array areaModifierFields{
    PayloadFieldDescriptor{
        "間隔幀數", false, PayloadNodeShape::Integer, "20", R"(類型: 週期傷害
關係: 敵方
數值: 40
間隔幀數: 20
重疊方式: 相加)",
    },
    PayloadFieldDescriptor{
        "類型", true, PayloadNodeShape::String, "強制移動免疫", {},
        PayloadSchemaReference::None, &areaModifierKindEnum,
    },
    PayloadFieldDescriptor{
        "關係", true, PayloadNodeShape::String, "友方", {},
        PayloadSchemaReference::None, &areaRelationEnum,
    },
    PayloadFieldDescriptor{
        "屬性", false, PayloadNodeShape::String, "攻擊", R"(類型: 屬性修正
關係: 友方
屬性: 防禦
數值: 10
重疊方式: 相加)", PayloadSchemaReference::None, &battleAttributeEnum,
    },
    PayloadFieldDescriptor{
        "數值", false, PayloadNodeShape::Number, "10", R"(類型: 屬性修正
關係: 友方
屬性: 防禦
數值: 10
重疊方式: 相加)", PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "百分比", false, PayloadNodeShape::Integer, "10", R"(類型: 造成傷害修正
關係: 友方
百分比: 10
傷害種類: 招式
重疊方式: 相加)",
    },
    PayloadFieldDescriptor{
        "傷害種類", false, PayloadNodeShape::String, "招式", R"(類型: 造成傷害修正
關係: 友方
百分比: 10
傷害種類: 招式
重疊方式: 相加)", PayloadSchemaReference::None, &damageChannelEnum,
    },
    PayloadFieldDescriptor{
        "追蹤", false, PayloadNodeShape::Boolean, "true", R"(類型: 攻擊生成修正
關係: 友方
追蹤: true
重疊方式: 任一)",
    },
    PayloadFieldDescriptor{
        "彈速百分比", false, PayloadNodeShape::Integer, "10", R"(類型: 攻擊生成修正
關係: 友方
彈速百分比: 10
重疊方式: 相加)",
    },
    PayloadFieldDescriptor{
        "彈道壓制百分比", false, PayloadNodeShape::Integer, "10", R"(類型: 攻擊生成修正
關係: 友方
彈道壓制百分比: 10
重疊方式: 相加)",
    },
    PayloadFieldDescriptor{
        "阻擋方向", false, PayloadNodeShape::String, "遠離來源", {},
        PayloadSchemaReference::None, &areaBlockedDirectionEnum,
    },
    PayloadFieldDescriptor{
        "重疊方式", true, PayloadNodeShape::String, "任一", {},
        PayloadSchemaReference::None, &areaOverlapPolicyEnum,
    },
};
static constexpr PayloadDescriptor areaModifierPayload{
    "區域修正", areaModifierFields, R"(類型: 強制移動免疫
關係: 友方
阻擋方向: 遠離來源
重疊方式: 任一)",
};

static constexpr std::array areaProjectileFields{
    PayloadFieldDescriptor{ "範圍格數", true, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{ "最多目標", true, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "眩暈幀數", true, PayloadNodeShape::Integer, "10" },
    PayloadFieldDescriptor{ "追蹤事件來源", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "特效", true, PayloadNodeShape::String, "死亡爆炸", {},
        PayloadSchemaReference::None, &areaProjectileVisualEnum,
    },
};
static constexpr PayloadDescriptor areaProjectilePayload{
    "區域投射物", areaProjectileFields, R"(範圍格數: 3
最多目標: 2
眩暈幀數: 10
特效: 死亡爆炸)",
};

static constexpr std::array autoUltimateFields{
    PayloadFieldDescriptor{ "消耗內力", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "顯示公告", false, PayloadNodeShape::Boolean, "true" },
};
static constexpr PayloadDescriptor autoUltimatePayload{
    "自動絕招", autoUltimateFields, "{}",
};

static constexpr std::array attributeModifierFields{
    PayloadFieldDescriptor{
        "屬性", true, PayloadNodeShape::String, "攻擊", {},
        PayloadSchemaReference::None, &battleAttributeEnum,
    },
    PayloadFieldDescriptor{
        "方式", true, PayloadNodeShape::String, "固定加算", {},
        PayloadSchemaReference::None, &attributeOperationEnum,
    },
    PayloadFieldDescriptor{
        "數值", false, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "每層數值", false, PayloadNodeShape::Number, "1",
        "屬性: 攻擊\n方式: 固定加算",
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{ "持續幀數", false, PayloadNodeShape::Integer, "30" },
    PayloadFieldDescriptor{
        "合併方式", false, PayloadNodeShape::String, "刷新", {},
        PayloadSchemaReference::None, &stackPolicyEnum,
    },
    PayloadFieldDescriptor{ "層數上限", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{
        "疊加範圍", false, PayloadNodeShape::String, "事件來源", {},
        PayloadSchemaReference::None, &stackScopeEnum,
    },
};
static constexpr std::array damageModifierFields{
    PayloadFieldDescriptor{
        "方位", false, PayloadNodeShape::String, "造成", {},
        PayloadSchemaReference::None, &damageModifierPerspectiveEnum,
    },
    PayloadFieldDescriptor{
        "階段", true, PayloadNodeShape::String, "防禦前", {},
        PayloadSchemaReference::None, &damageModifierStageEnum,
    },
    PayloadFieldDescriptor{
        "傷害種類", true, PayloadNodeShape::String, "招式", {},
        PayloadSchemaReference::None, &damageChannelEnum,
    },
    PayloadFieldDescriptor{
        "方式", true, PayloadNodeShape::String, "固定加算", {},
        PayloadSchemaReference::None, &damageModifierOperationEnum,
    },
    PayloadFieldDescriptor{
        "數值", false, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "每層數值", false, PayloadNodeShape::Number, "1",
        "方位: 造成\n階段: 防禦前\n傷害種類: 招式\n方式: 固定加算",
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{ "持續幀數", false, PayloadNodeShape::Integer, "30" },
    PayloadFieldDescriptor{
        "合併方式", false, PayloadNodeShape::String, "刷新", {},
        PayloadSchemaReference::None, &stackPolicyEnum,
    },
    PayloadFieldDescriptor{ "層數上限", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{
        "疊加範圍", false, PayloadNodeShape::String, "事件來源", {},
        PayloadSchemaReference::None, &stackScopeEnum,
    },
};
static constexpr std::array resourceChangeFields{
    PayloadFieldDescriptor{
        "資源", true, PayloadNodeShape::String, "內力", {},
        PayloadSchemaReference::None, &resourceEnum,
    },
    PayloadFieldDescriptor{
        "方式", true, PayloadNodeShape::String, "回復", {},
        PayloadSchemaReference::None, &resourceChangeKindEnum,
    },
    PayloadFieldDescriptor{
        "數值", false, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "每層數值", false, PayloadNodeShape::Number, "1",
        "資源: 內力\n方式: 回復",
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "轉移目標", false, PayloadNodeShape::Selector, "自身", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "治療種類", false, PayloadNodeShape::String, "直接", {},
        PayloadSchemaReference::None, &healKindEnum,
    },
    PayloadFieldDescriptor{
        "來源政策", false, PayloadNodeShape::String, "允許死亡來源", {},
        PayloadSchemaReference::None, &healSourcePolicyEnum,
    },
};
static constexpr std::array healTransactionModifierFields{
    PayloadFieldDescriptor{
        "方式", true, PayloadNodeShape::String, "阻止", {},
        PayloadSchemaReference::None, &healModifierOperationEnum,
    },
    PayloadFieldDescriptor{
        "百分比", false, PayloadNodeShape::Integer, "50", R"(方式: 受到治療乘算
百分比: 50
治療種類:
  - 直接)",
    },
    PayloadFieldDescriptor{
        "治療種類", true, PayloadNodeShape::Sequence, R"(- 直接
- 命中)", {}, PayloadSchemaReference::None, &healKindEnum,
    },
};
static constexpr std::string_view battleSpiritStatusProbe = R"(狀態: 戰意
增加層數: 1
層數上限: 10
效果:
  - 時機: 持續
    目標: 狀態持有者
    傷害修正:
      方位: 造成
      階段: 防禦前
      傷害種類: 招式
      方式: 百分比加算
      每層數值: 5)";
static constexpr std::string_view bleedStatusProbe = R"(狀態: 流血
增加層數: 1
目標總層數上限: 3)";
static constexpr std::string_view sevenStarStatusProbe = R"(狀態: 七星
設定印記層數: 7
持續幀數: 150)";
static constexpr std::string_view blindedStatusProbe = R"(狀態: 刺目
可觸發次數: 1)";
static constexpr std::string_view neutralizeForceStatusProbe = R"(狀態: 化勁
可觸發次數: 1)";
static constexpr std::string_view addedDamageBlockStatusProbe = R"(狀態: 傷害抵擋
增加可抵擋次數: 1
可抵擋次數上限: 3
效果:
  - 時機: 持續
    目標: 狀態持有者
    抵擋非處決正傷害: {})";
static constexpr std::string_view setDamageBlockStatusProbe = R"(狀態: 傷害抵擋
設定可抵擋次數: 3
效果:
  - 時機: 持續
    目標: 狀態持有者
    抵擋非處決正傷害: {})";

consteval std::string_view statusQuantityContextProbe(StatusQuantityFieldId id)
{
    switch (id)
    {
    case StatusQuantityFieldId::AddedLayers:
    case StatusQuantityFieldId::LayerLimit:
        return battleSpiritStatusProbe;
    case StatusQuantityFieldId::TargetTotalLayerLimit:
        return bleedStatusProbe;
    case StatusQuantityFieldId::TriggerCharges:
        return blindedStatusProbe;
    case StatusQuantityFieldId::SetMarks:
        return sevenStarStatusProbe;
    case StatusQuantityFieldId::AddedDamageBlocks:
    case StatusQuantityFieldId::DamageBlockLimit:
        return addedDamageBlockStatusProbe;
    case StatusQuantityFieldId::SetDamageBlocks:
        return setDamageBlockStatusProbe;
    case StatusQuantityFieldId::Count:
        break;
    }
    return {};
}

consteval std::string_view statusNamedNumberContextProbe(
    StatusNamedNumberFieldId id)
{
    switch (id)
    {
    case StatusNamedNumberFieldId::NeutralizeShield:
        return neutralizeForceStatusProbe;
    case StatusNamedNumberFieldId::Count:
        break;
    }
    return {};
}

consteval auto makeApplyStatusFields()
{
    std::array<PayloadFieldDescriptor,
        statusQuantityFieldCatalog.size()
            + statusNamedNumberFieldCatalog.size()
            + 4> result{};
    std::size_t index{};
    result[index++] = PayloadFieldDescriptor{
        "狀態", true, PayloadNodeShape::String, "眩暈", {},
        PayloadSchemaReference::None, &statusKindEnum,
    };
    result[index++] = PayloadFieldDescriptor{
        "持續幀數", false, PayloadNodeShape::Number, "30", {},
        PayloadSchemaReference::EffectNumber,
    };
    for (const auto& field : statusQuantityFieldCatalog)
    {
        result[index++] = PayloadFieldDescriptor{
            field.label,
            false,
            PayloadNodeShape::Integer,
            field.probeValue,
            statusQuantityContextProbe(field.id),
        };
    }
    for (const auto& field : statusNamedNumberFieldCatalog)
    {
        result[index++] = PayloadFieldDescriptor{
            field.label,
            false,
            PayloadNodeShape::Number,
            field.probeValue,
            statusNamedNumberContextProbe(field.id),
            PayloadSchemaReference::EffectNumber,
        };
    }
    result[index++] = PayloadFieldDescriptor{
        "重複套用", false, PayloadNodeShape::String,
        statusReapplicationPolicyLabel(StatusReapplicationPolicy::KeepLongerDuration), {},
        PayloadSchemaReference::None, &statusReapplicationPolicyEnum,
    };
    result[index++] = PayloadFieldDescriptor{
        "效果", false, PayloadNodeShape::Sequence,
        "- 時機: 持續\n  目標: 狀態持有者\n  動作:\n    - 傷害修正:\n        方位: 造成\n        階段: 防禦前\n        傷害種類: 招式\n        方式: 百分比加算\n        每層數值: 5",
        battleSpiritStatusProbe,
        PayloadSchemaReference::RuleList,
    };
    return result;
}
static constexpr auto applyStatusFields = makeApplyStatusFields();
static constexpr std::array poisonApplicationFields{
    PayloadFieldDescriptor{
        "持續幀數", true, PayloadNodeShape::Number, "90", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        statusQuantityFieldLabel(StatusQuantityFieldId::TriggerCharges),
        true,
        PayloadNodeShape::Integer,
        "3",
    },
    PayloadFieldDescriptor{
        "重複套用", true, PayloadNodeShape::String,
        statusReapplicationPolicyLabel(StatusReapplicationPolicy::KeepHigherDamage), {},
        PayloadSchemaReference::None, &statusReapplicationPolicyEnum,
    },
    PayloadFieldDescriptor{
        "同事件合併", false, PayloadNodeShape::String, "合計傷害百分比", {},
        PayloadSchemaReference::None, &poisonSameEventMergeEnum,
    },
    PayloadFieldDescriptor{
        "效果", true, PayloadNodeShape::Sequence,
        "- 時機: 每隔\n  間隔幀數: 30\n  目標: 狀態持有者\n  動作:\n    - 造成傷害:\n        數值:\n          目標目前生命百分比: 7\n        傷害種類: 中毒\n    - 消耗此狀態:\n        消耗數量: 1", {},
        PayloadSchemaReference::RuleList,
    },
};
static constexpr std::array consumeStatusFields{
    PayloadFieldDescriptor{
        "狀態", true, PayloadNodeShape::String, "中毒", {},
        PayloadSchemaReference::None, &statusKindEnum,
    },
    PayloadFieldDescriptor{ "消耗數量", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{
        "狀態來源", false, PayloadNodeShape::String, "效果擁有者", {},
        PayloadSchemaReference::None, &statusSourceMatchEnum,
    },
    PayloadFieldDescriptor{
        "最後一次", false, PayloadNodeShape::ActionNode, R"(套用狀態:
  狀態: 眩暈
  持續幀數: 30
  重複套用: 保留較長持續時間)", {}, PayloadSchemaReference::ActionNode,
    },
};
static constexpr std::array consumeThisStatusFields{
    PayloadFieldDescriptor{ "消耗數量", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{
        "最後一次", false, PayloadNodeShape::ActionNode, R"(套用狀態:
  狀態: 眩暈
  持續幀數: 30
  重複套用: 保留較長持續時間)", {}, PayloadSchemaReference::ActionNode,
    },
};
static constexpr std::array suppressCurrentCastContactsFields{
    PayloadFieldDescriptor{
        "成功後", false, PayloadNodeShape::Any,
        R"(- 原攻擊目標獲得護盾:
    每星級: 100)",
    },
};
static constexpr std::array removeStatusFields{
    PayloadFieldDescriptor{
        "狀態", false, PayloadNodeShape::StringOrSequence, R"(- 中毒
- 流血)", {}, PayloadSchemaReference::None, &statusKindEnum,
    },
    PayloadFieldDescriptor{ "僅負面", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "僅控制", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "狀態來源", false, PayloadNodeShape::String, "效果擁有者", {},
        PayloadSchemaReference::None, &statusSourceMatchEnum,
    },
    PayloadFieldDescriptor{ "解除目前動作僵直", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "數量", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{
        "順序", false, PayloadNodeShape::String, "最長剩餘", {},
        PayloadSchemaReference::None, &statusRemovalOrderEnum,
    },
};
static constexpr std::array dealDamageFields{
    PayloadFieldDescriptor{
        "數值", false, PayloadNodeShape::Number, "10", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "每層數值", false, PayloadNodeShape::Number, "10",
        "傷害種類: 純粹",
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "交易次數", false, PayloadNodeShape::Number, "2", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "傷害種類", true, PayloadNodeShape::String, "純粹", {},
        PayloadSchemaReference::None, &battleDamageKindEnum,
    },
    PayloadFieldDescriptor{
        "範圍", false, PayloadNodeShape::String, "圓形", {},
        PayloadSchemaReference::None, &damageAreaKindEnum,
    },
    PayloadFieldDescriptor{ "半徑格數", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "方形邊長", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{ "同目標命中上限", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{ "套用傷害修正", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "觸發受傷無敵", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "區域投射物", false, PayloadNodeShape::Map, R"(範圍格數: 3
最多目標: 2
眩暈幀數: 10
特效: 死亡爆炸)", {}, PayloadSchemaReference::Payload, nullptr,
        &areaProjectilePayload,
    },
};
static constexpr std::array modifyAttackFields{
    PayloadFieldDescriptor{
        "樣式", false, PayloadNodeShape::String, "扇形", {},
        PayloadSchemaReference::None, &attackPatternKindEnum,
    },
    PayloadFieldDescriptor{ "數量", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{ "展開角度", false, PayloadNodeShape::Integer, "30" },
    PayloadFieldDescriptor{ "間隔幀數", false, PayloadNodeShape::Integer, "5" },
    PayloadFieldDescriptor{ "傷害倍率", false, PayloadNodeShape::Integer, "100" },
    PayloadFieldDescriptor{ "貫穿", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "追蹤", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "視為主彈道", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{ "清除彈道半徑百分比", false, PayloadNodeShape::Integer, "150" },
    PayloadFieldDescriptor{ "同目標命中上限", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{
        "目標政策", false, PayloadNodeShape::String, "選擇目標", {},
        PayloadSchemaReference::None, &attackTargetPolicyEnum,
    },
    PayloadFieldDescriptor{
        "傳播政策", false, PayloadNodeShape::String, "來源全部規則", {},
        PayloadSchemaReference::None, &propagationPolicyEnum,
    },
    PayloadFieldDescriptor{ "追加至基礎攻擊", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "攻擊來源", false, PayloadNodeShape::Selector, "自身", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "傷害數值", false, PayloadNodeShape::Number, "10", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "傷害種類", false, PayloadNodeShape::String, "招式", {},
        PayloadSchemaReference::None, &battleDamageKindEnum,
    },
    PayloadFieldDescriptor{
        "執行行為", false, PayloadNodeShape::Map, R"(類型: 彈道彈射
追加命中次數: 1
機率: 50
範圍像素: 120)", {}, PayloadSchemaReference::Payload, nullptr,
        &attackRuntimeBehaviorPayload,
    },
};
static constexpr std::array forceMoveFields{
    PayloadFieldDescriptor{
        "方向", true, PayloadNodeShape::String, "遠離來源", {},
        PayloadSchemaReference::None, &forceMoveDirectionEnum,
    },
    PayloadFieldDescriptor{ "距離格數", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "距離像素", false, PayloadNodeShape::Integer, "40" },
    PayloadFieldDescriptor{ "鎖定幀數", false, PayloadNodeShape::Integer, "10" },
    PayloadFieldDescriptor{
        "碰撞", true, PayloadNodeShape::String, "阻擋前停止", {},
        PayloadSchemaReference::None, &forceMoveCollisionEnum,
    },
    PayloadFieldDescriptor{
        "受阻結果", true, PayloadNodeShape::String, "縮短", {},
        PayloadSchemaReference::None, &forceMoveBlockedResultEnum,
    },
};
static constexpr std::array createAreaFields{
    PayloadFieldDescriptor{
        "形狀", true, PayloadNodeShape::String, "圓形", {},
        PayloadSchemaReference::None, &areaShapeEnum,
    },
    PayloadFieldDescriptor{ "半徑格數", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "方形邊長", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{
        "錨點", true, PayloadNodeShape::String, "命中位置", {},
        PayloadSchemaReference::None, &areaAnchorEnum,
    },
    PayloadFieldDescriptor{ "持續幀數", true, PayloadNodeShape::Integer, "60" },
    PayloadFieldDescriptor{
        "來源死亡", true, PayloadNodeShape::String, "保留至到期", {},
        PayloadSchemaReference::None, &areaSourceDeathPolicyEnum,
    },
    PayloadFieldDescriptor{
        "合併方式", true, PayloadNodeShape::String, "獨立", {},
        PayloadSchemaReference::None, &areaMergePolicyEnum,
    },
    PayloadFieldDescriptor{
        "區域修正", true, PayloadNodeShape::Sequence, R"(- 類型: 強制移動免疫
  關係: 友方
  阻擋方向: 遠離來源
  重疊方式: 任一)", {}, PayloadSchemaReference::PayloadList, nullptr,
        &areaModifierPayload,
    },
};
static constexpr std::array modifyCastFields{
    PayloadFieldDescriptor{
        "內力消耗", false, PayloadNodeShape::Number, "10", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "射程模式", false, PayloadNodeShape::String, "保留", {},
        PayloadSchemaReference::None, &castRangeModeEnum,
    },
    PayloadFieldDescriptor{ "彈道速度百分比", false, PayloadNodeShape::Integer, "120" },
    PayloadFieldDescriptor{ "最小選擇距離", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "追加彈道數", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{
        "機動政策", false, PayloadNodeShape::String, "滑步攻擊", {},
        PayloadSchemaReference::None, &castMobilityPolicyEnum,
    },
    PayloadFieldDescriptor{
        "自動絕招", false, PayloadNodeShape::Map, R"(消耗內力: true
顯示公告: true)", {}, PayloadSchemaReference::Payload, nullptr,
        &autoUltimatePayload,
    },
    PayloadFieldDescriptor{ "免費追加施放", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "傳播政策", false, PayloadNodeShape::String, "來源全部規則", {},
        PayloadSchemaReference::None, &propagationPolicyEnum,
    },
    PayloadFieldDescriptor{
        "樣式", false, PayloadNodeShape::String, "扇形", {},
        PayloadSchemaReference::None, &attackPatternKindEnum,
    },
    PayloadFieldDescriptor{ "數量", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{ "展開角度", false, PayloadNodeShape::Integer, "30" },
    PayloadFieldDescriptor{ "間隔幀數", false, PayloadNodeShape::Integer, "5" },
};
static constexpr std::array changeStateValueFields{
    PayloadFieldDescriptor{
        "狀態槽", true, PayloadNodeShape::String, "永久施放進展", {},
        PayloadSchemaReference::None, &stateSlotEnum,
    },
    PayloadFieldDescriptor{ "增量", true, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{ "最小", false, PayloadNodeShape::Integer, "0" },
    PayloadFieldDescriptor{ "最大", false, PayloadNodeShape::Integer, "10" },
};
static constexpr PayloadDescriptor changeStateValuePayload{
    "變更狀態值", changeStateValueFields, R"(狀態槽: 永久施放進展
增量: 1)",
};
static constexpr std::array transferStateValueFields{
    PayloadFieldDescriptor{
        "來源狀態槽", true, PayloadNodeShape::String, "最大招式生命傷害", {},
        PayloadSchemaReference::None, &stateSlotEnum,
    },
    PayloadFieldDescriptor{
        "目標狀態槽", true, PayloadNodeShape::String, "本次施放最高生命傷害", {},
        PayloadSchemaReference::None, &stateSlotEnum,
    },
};
static constexpr PayloadDescriptor transferStateValuePayload{
    "轉移狀態值", transferStateValueFields, R"(來源狀態槽: 最大招式生命傷害
目標狀態槽: 本次施放最高生命傷害)",
};
static constexpr std::array recordMaximumDamageFields{
    PayloadFieldDescriptor{
        "狀態槽", true, PayloadNodeShape::String, "最大招式生命傷害", {},
        PayloadSchemaReference::None, &stateSlotEnum,
    },
};
static constexpr PayloadDescriptor recordMaximumDamagePayload{
    "記錄最大招式生命傷害", recordMaximumDamageFields, "狀態槽: 最大招式生命傷害",
};
static constexpr std::array consumeRecordedMaximumFields{
    PayloadFieldDescriptor{
        "狀態槽", true, PayloadNodeShape::String, "最大招式生命傷害", {},
        PayloadSchemaReference::None, &stateSlotEnum,
    },
    PayloadFieldDescriptor{ "百分比", false, PayloadNodeShape::Integer, "100" },
    PayloadFieldDescriptor{ "消耗後清除", false, PayloadNodeShape::Boolean, "true" },
};
static constexpr PayloadDescriptor consumeRecordAsDamagePayload{
    "消耗記錄為傷害", consumeRecordedMaximumFields, "狀態槽: 最大招式生命傷害",
};
static constexpr PayloadDescriptor consumeRecordAsShieldPayload{
    "消耗記錄為護盾", consumeRecordedMaximumFields, "狀態槽: 最大招式生命傷害",
};
static constexpr std::array startDamageAbsorptionFields{
    PayloadFieldDescriptor{
        "狀態槽", true, PayloadNodeShape::String, "累計吸收傷害", {},
        PayloadSchemaReference::None, &stateSlotEnum,
    },
    PayloadFieldDescriptor{ "百分比", true, PayloadNodeShape::Integer, "50" },
    PayloadFieldDescriptor{ "持續幀數", true, PayloadNodeShape::Integer, "60" },
    PayloadFieldDescriptor{ "死亡結算", false, PayloadNodeShape::Boolean, "true" },
    PayloadFieldDescriptor{
        "結算目標", true, PayloadNodeShape::Selector, "自身", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "結算傷害種類", true, PayloadNodeShape::String, "純粹", {},
        PayloadSchemaReference::None, &battleDamageKindEnum,
    },
    PayloadFieldDescriptor{ "結算百分比", true, PayloadNodeShape::Integer, "100" },
};
static constexpr PayloadDescriptor startDamageAbsorptionPayload{
    "開始傷害吸收", startDamageAbsorptionFields, R"(狀態槽: 累計吸收傷害
百分比: 50
持續幀數: 60
結算目標: 自身
結算傷害種類: 純粹
結算百分比: 100)",
};
static constexpr std::array settleDamageAbsorptionFields{
    PayloadFieldDescriptor{
        "狀態槽", true, PayloadNodeShape::String, "累計吸收傷害", {},
        PayloadSchemaReference::None, &stateSlotEnum,
    },
    PayloadFieldDescriptor{
        "目標", false, PayloadNodeShape::Selector, "自身", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{ "百分比", false, PayloadNodeShape::Integer, "100" },
};
static constexpr PayloadDescriptor settleDamageAbsorptionPayload{
    "結算傷害吸收", settleDamageAbsorptionFields, "狀態槽: 累計吸收傷害",
};
static constexpr std::array borrowEffectRulesFields{
    PayloadFieldDescriptor{
        "目標", true, PayloadNodeShape::Selector, "友軍", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "來源數量", true, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "允許動作類別", true, PayloadNodeShape::Sequence, R"(- 傷害修正
- 狀態)", {}, PayloadSchemaReference::None, &borrowedRuleActionCategoryEnum,
    },
    PayloadFieldDescriptor{
        "傳播政策", false, PayloadNodeShape::String, "借用大招規則", {},
        PayloadSchemaReference::None, &propagationPolicyEnum,
    },
};
static constexpr PayloadDescriptor borrowEffectRulesPayload{
    "借用效果規則", borrowEffectRulesFields, R"(目標: 友軍
來源數量: 1
允許動作類別:
  - 傷害修正)",
};
static constexpr std::array copyAttackDefinitionFields{
    PayloadFieldDescriptor{
        "目標", true, PayloadNodeShape::Selector, R"(類型: 所有存活單位
排除效果擁有者: true)", {}, PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "可選武功條件", true, PayloadNodeShape::Sequence, R"(- 有絕招攻擊定義
- 排除複製與借用遞迴)", {}, PayloadSchemaReference::None, &copiedMagicConditionEnum,
    },
    PayloadFieldDescriptor{ "來源數量", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{
        "傳播政策", false, PayloadNodeShape::String, "不傳播大招規則", {},
        PayloadSchemaReference::None, &propagationPolicyEnum,
    },
};
static constexpr PayloadDescriptor copyAttackDefinitionPayload{
    "複製攻擊定義", copyAttackDefinitionFields, R"(目標:
  類型: 所有存活單位
  排除效果擁有者: true
可選武功條件:
  - 有絕招攻擊定義
  - 排除複製與借用遞迴)",
};
static constexpr PayloadDescriptor settleRemainingPoisonDamagePayload{
    "結算剩餘中毒傷害", emptyPayloadFields, "{}",
};
static constexpr std::array generateClonesFields{
    PayloadFieldDescriptor{ "數量", true, PayloadNodeShape::Integer, "1" },
};
static constexpr PayloadDescriptor generateClonesPayload{
    "生成分身", generateClonesFields, "數量: 1",
};
static constexpr std::array preventDeathFields{
    PayloadFieldDescriptor{ "無敵幀數", true, PayloadNodeShape::Integer, "30" },
};
static constexpr PayloadDescriptor preventDeathPayload{
    "死亡庇護", preventDeathFields, "無敵幀數: 30",
};
static constexpr std::array rescueRepositionFields{
    PayloadFieldDescriptor{ "次數", true, PayloadNodeShape::Integer, "1" },
};
static constexpr PayloadDescriptor configureProtectRepositionPayload{
    "保護挪移", rescueRepositionFields, "次數: 1",
};
static constexpr PayloadDescriptor configureExecuteRepositionPayload{
    "處決挪移", rescueRepositionFields, "次數: 1",
};
static constexpr std::array conditionalFields{
    PayloadFieldDescriptor{
        "條件", true, PayloadNodeShape::ConditionList, R"(- 僅限絕招)", {},
        PayloadSchemaReference::ConditionList,
    },
    PayloadFieldDescriptor{
        "成立", true, PayloadNodeShape::ActionList, R"(- 獲得護盾: 1)", {},
        PayloadSchemaReference::ActionList,
    },
    PayloadFieldDescriptor{
        "否則", false, PayloadNodeShape::ActionList, R"(- 回復內力: 1)", {},
        PayloadSchemaReference::ActionList,
    },
};

static constexpr PayloadDescriptor attributeModifierPayload{
    "屬性修正", attributeModifierFields, R"(屬性: 攻擊
方式: 固定加算
數值: 1)",
};
static constexpr PayloadDescriptor damageModifierPayload{
    "傷害修正", damageModifierFields, R"(方位: 造成
階段: 防禦前
傷害種類: 招式
方式: 固定加算
數值: 1)",
};
static constexpr PayloadDescriptor resourceChangePayload{
    "資源變更", resourceChangeFields, R"(資源: 內力
方式: 回復
數值: 1)",
};
static constexpr PayloadDescriptor healTransactionModifierPayload{
    "治療交易修正", healTransactionModifierFields, R"(方式: 阻止
治療種類:
  - 直接)",
};
static constexpr PayloadDescriptor applyStatusPayload{
    "套用狀態", applyStatusFields, R"(狀態: 眩暈
持續幀數: 30
重複套用: 延長持續時間)",
};
static constexpr PayloadDescriptor poisonApplicationPayload{
    "施加中毒", poisonApplicationFields, R"(可觸發次數: 3
持續幀數: 90
重複套用: 保留較高傷害
同事件合併: 合計傷害百分比
效果:
  - 時機: 每隔
    間隔幀數: 30
    目標: 狀態持有者
    動作:
      - 造成傷害:
          數值:
            目標目前生命百分比: 7
            取整: 向零
            最小: 1
          傷害種類: 中毒
      - 消耗此狀態:
          消耗數量: 1)",
};
static constexpr PayloadDescriptor consumeStatusPayload{
    "消耗狀態", consumeStatusFields, "狀態: 中毒",
};
static constexpr PayloadDescriptor consumeThisStatusPayload{
    "消耗此狀態", consumeThisStatusFields, "消耗數量: 1",
};
static constexpr PayloadDescriptor suppressCurrentCastContactsPayload{
    "使本次施放攻擊落空", suppressCurrentCastContactsFields, "{}",
};
static constexpr PayloadDescriptor makeIncomingAttackMissPayload{
    "使本次受到攻擊落空", emptyPayloadFields, "{}",
};
static constexpr PayloadDescriptor blockPositiveDamagePayload{
    "抵擋非處決正傷害", emptyPayloadFields, "{}",
};
static constexpr PayloadDescriptor removeStatusPayload{
    "移除狀態", removeStatusFields, "狀態: 中毒",
};
static constexpr PayloadDescriptor dealDamagePayload{
    "造成傷害", dealDamageFields, R"(數值: 1
傷害種類: 純粹)",
};
static constexpr PayloadDescriptor modifyAttackPayload{
    "修改攻擊", modifyAttackFields, "樣式: 保留",
};
static constexpr PayloadDescriptor forceMovePayload{
    "強制移動", forceMoveFields, R"(方向: 遠離來源
距離格數: 1
碰撞: 阻擋前停止
受阻結果: 縮短)",
};
static constexpr PayloadDescriptor createAreaPayload{
    "建立區域", createAreaFields, R"(形狀: 圓形
半徑格數: 1
錨點: 命中位置
持續幀數: 1
來源死亡: 保留至到期
合併方式: 獨立
區域修正:
  - 類型: 強制移動免疫
    關係: 友方
    阻擋方向: 遠離來源
    重疊方式: 任一)",
};
static constexpr PayloadDescriptor modifyCastPayload{
    "修改施放", modifyCastFields, "射程模式: 保留",
};
static constexpr PayloadDescriptor conditionalPayload{
    "條件分支", conditionalFields, R"(條件:
  - 僅限絕招
成立:
  - 獲得護盾: 1)",
};

static constexpr std::array actionDescriptors{
    ActionDescriptor{ "屬性修正", 0, ActionPayloadKind::AttributeModifier, &attributeModifierPayload },
    ActionDescriptor{ "傷害修正", 1, ActionPayloadKind::DamageModifier, &damageModifierPayload },
    ActionDescriptor{ "資源變更", 2, ActionPayloadKind::ResourceChange, &resourceChangePayload },
    ActionDescriptor{
        "治療交易修正", 3, ActionPayloadKind::HealTransactionModifier,
        &healTransactionModifierPayload,
    },
    ActionDescriptor{ "套用狀態", 4, ActionPayloadKind::ApplyStatus, &applyStatusPayload },
    ActionDescriptor{ "施加中毒", 4, ActionPayloadKind::ApplyStatus, &poisonApplicationPayload },
    ActionDescriptor{ "消耗狀態", 5, ActionPayloadKind::ConsumeStatus, &consumeStatusPayload },
    ActionDescriptor{ "移除狀態", 6, ActionPayloadKind::RemoveStatus, &removeStatusPayload },
    ActionDescriptor{ "造成傷害", 7, ActionPayloadKind::Damage, &dealDamagePayload },
    ActionDescriptor{ "修改攻擊", 8, ActionPayloadKind::Attack, &modifyAttackPayload },
    ActionDescriptor{ "強制移動", 9, ActionPayloadKind::ForceMove, &forceMovePayload },
    ActionDescriptor{ "建立區域", 10, ActionPayloadKind::Area, &createAreaPayload },
    ActionDescriptor{ "修改施放", 11, ActionPayloadKind::Cast, &modifyCastPayload },
    ActionDescriptor{
        "變更狀態值", 12, ActionPayloadKind::StateMachine, &changeStateValuePayload,
        EffectAuthoringTier::Primitive, StateMachineMechanism::ChangeStateValue,
    },
    ActionDescriptor{
        "轉移狀態值", 12, ActionPayloadKind::StateMachine, &transferStateValuePayload,
        EffectAuthoringTier::Primitive, StateMachineMechanism::TransferStateValue,
    },
    ActionDescriptor{
        "記錄最大招式生命傷害", 12, ActionPayloadKind::StateMachine, &recordMaximumDamagePayload,
        EffectAuthoringTier::Primitive, StateMachineMechanism::RecordMaximumSkillDamage,
    },
    ActionDescriptor{
        "消耗記錄為傷害", 12, ActionPayloadKind::StateMachine, &consumeRecordAsDamagePayload,
        EffectAuthoringTier::Primitive, StateMachineMechanism::ConsumeRecordAsDamage,
    },
    ActionDescriptor{
        "消耗記錄為護盾", 12, ActionPayloadKind::StateMachine, &consumeRecordAsShieldPayload,
        EffectAuthoringTier::Primitive, StateMachineMechanism::ConsumeRecordAsShield,
    },
    ActionDescriptor{
        "開始傷害吸收", 12, ActionPayloadKind::StateMachine, &startDamageAbsorptionPayload,
        EffectAuthoringTier::Primitive, StateMachineMechanism::StartDamageAbsorption,
    },
    ActionDescriptor{
        "結算傷害吸收", 12, ActionPayloadKind::StateMachine, &settleDamageAbsorptionPayload,
        EffectAuthoringTier::Primitive, StateMachineMechanism::SettleDamageAbsorption,
    },
    ActionDescriptor{
        "借用效果規則", 12, ActionPayloadKind::StateMachine, &borrowEffectRulesPayload,
        EffectAuthoringTier::Specialized, StateMachineMechanism::BorrowEffectRules,
    },
    ActionDescriptor{
        "複製攻擊定義", 12, ActionPayloadKind::StateMachine, &copyAttackDefinitionPayload,
        EffectAuthoringTier::Specialized, StateMachineMechanism::CopyAttackDefinition,
    },
    ActionDescriptor{
        "結算剩餘中毒傷害", 12, ActionPayloadKind::StateMachine, &settleRemainingPoisonDamagePayload,
        EffectAuthoringTier::Specialized, StateMachineMechanism::SettleRemainingStatusDamage,
    },
    ActionDescriptor{
        "生成分身", 12, ActionPayloadKind::StateMachine, &generateClonesPayload,
        EffectAuthoringTier::Specialized, StateMachineMechanism::GenerateClones,
    },
    ActionDescriptor{
        "死亡庇護", 12, ActionPayloadKind::StateMachine, &preventDeathPayload,
        EffectAuthoringTier::Specialized, StateMachineMechanism::PreventDeath,
    },
    ActionDescriptor{
        "保護挪移", 12, ActionPayloadKind::StateMachine, &configureProtectRepositionPayload,
        EffectAuthoringTier::Specialized, StateMachineMechanism::ConfigureProtectReposition,
    },
    ActionDescriptor{
        "處決挪移", 12, ActionPayloadKind::StateMachine, &configureExecuteRepositionPayload,
        EffectAuthoringTier::Specialized, StateMachineMechanism::ConfigureExecuteReposition,
    },
    ActionDescriptor{
        "消耗此狀態", 13, ActionPayloadKind::StatusContext, &consumeThisStatusPayload },
    ActionDescriptor{
        "使本次施放攻擊落空", 14, ActionPayloadKind::StatusContext,
        &suppressCurrentCastContactsPayload },
    ActionDescriptor{
        "使本次受到攻擊落空", 15, ActionPayloadKind::StatusContext,
        &makeIncomingAttackMissPayload },
    ActionDescriptor{
        "抵擋非處決正傷害", 16, ActionPayloadKind::StatusContext,
        &blockPositiveDamagePayload },
    ActionDescriptor{ "條件分支", 17, ActionPayloadKind::Conditional, &conditionalPayload },
};

static constexpr std::array<PayloadFieldDescriptor, 0> attributePercentageFields{};
static constexpr PayloadDescriptor attributePercentagePayload{
    "屬性加成.百分比", attributePercentageFields, "攻擊: 1",
    PayloadDynamicKeyClass::BattleAttribute, PayloadNodeShape::Integer, "攻擊", "1", 1,
};
static constexpr std::array attributeBonusFields{
    PayloadFieldDescriptor{
        "百分比", false, PayloadNodeShape::Map, "攻擊: 10", "{}",
        PayloadSchemaReference::Payload, nullptr, &attributePercentagePayload,
    },
    PayloadFieldDescriptor{ "持續幀數", false, PayloadNodeShape::Integer, "30" },
    PayloadFieldDescriptor{
        "合併方式", false, PayloadNodeShape::String, "刷新", {},
        PayloadSchemaReference::None, &stackPolicyEnum,
    },
    PayloadFieldDescriptor{ "層數上限", false, PayloadNodeShape::Integer, "3" },
    PayloadFieldDescriptor{
        "疊加範圍", false, PayloadNodeShape::String, "事件來源", {},
        PayloadSchemaReference::None, &stackScopeEnum,
    },
};
static constexpr std::array resourceMacroFields{
    PayloadFieldDescriptor{
        "資源", true, PayloadNodeShape::String, "內力", {},
        PayloadSchemaReference::None, &resourceEnum,
    },
    PayloadFieldDescriptor{
        "數值", true, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "轉移目標", false, PayloadNodeShape::Selector, "自身", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "治療種類", false, PayloadNodeShape::String, "直接", {},
        PayloadSchemaReference::None, &healKindEnum,
    },
    PayloadFieldDescriptor{
        "來源政策", false, PayloadNodeShape::String, "允許死亡來源", {},
        PayloadSchemaReference::None, &healSourcePolicyEnum,
    },
};
static constexpr std::array healMacroFields{
    PayloadFieldDescriptor{
        "數值", true, PayloadNodeShape::Number, "1", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "治療種類", false, PayloadNodeShape::String, "直接", {},
        PayloadSchemaReference::None, &healKindEnum,
    },
    PayloadFieldDescriptor{
        "來源政策", false, PayloadNodeShape::String, "允許死亡來源", {},
        PayloadSchemaReference::None, &healSourcePolicyEnum,
    },
};
static constexpr std::array forceMoveMacroFields{
    PayloadFieldDescriptor{ "距離格數", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{ "距離像素", false, PayloadNodeShape::Integer, "40" },
    PayloadFieldDescriptor{ "鎖定幀數", false, PayloadNodeShape::Integer, "10" },
};
static constexpr PayloadDescriptor attributeBonusPayload{
    "屬性加成", attributeBonusFields, "攻擊: 1",
    PayloadDynamicKeyClass::BattleAttribute, PayloadNodeShape::Integer, "攻擊", "1", 1,
    "百分比",
};
static constexpr PayloadDescriptor resourceMacroPayload{
    "資源巨集", resourceMacroFields, R"(資源: 內力
數值: 1)",
};
static constexpr PayloadDescriptor healMacroPayload{
    "回復生命", healMacroFields, "數值: 1",
};
static constexpr PayloadDescriptor forceMoveMacroPayload{
    "移動巨集", forceMoveMacroFields, "距離格數: 1",
};

static constexpr std::array macroDescriptors{
    MacroDescriptor{ "屬性加成", MacroPayloadKind::AttributeBonus, &attributeBonusPayload, 0 },
    MacroDescriptor{ "回復資源", MacroPayloadKind::Resource, &resourceMacroPayload, 2 },
    MacroDescriptor{ "獲得資源", MacroPayloadKind::Resource, &resourceMacroPayload, 2 },
    MacroDescriptor{ "奪取資源", MacroPayloadKind::Resource, &resourceMacroPayload, 2 },
    MacroDescriptor{ "回復內力", MacroPayloadKind::Number, &effectNumberPayload, 2 },
    MacroDescriptor{ "回復生命", MacroPayloadKind::Heal, &healMacroPayload, 2 },
    MacroDescriptor{ "獲得護盾", MacroPayloadKind::Number, &effectNumberPayload, 2 },
    MacroDescriptor{ "忽略防禦", MacroPayloadKind::Number, &effectNumberPayload, 1 },
    MacroDescriptor{ "擊退", MacroPayloadKind::ForceMove, &forceMoveMacroPayload, 9 },
    MacroDescriptor{ "拉近", MacroPayloadKind::ForceMove, &forceMoveMacroPayload, 9 },
};

static constexpr std::array ruleFields{
    PayloadFieldDescriptor{
        "時機", true, PayloadNodeShape::String, "造成傷害後", {},
        PayloadSchemaReference::Timing,
    },
    PayloadFieldDescriptor{
        "觀察範圍", false, PayloadNodeShape::String, "效果擁有者", {},
        PayloadSchemaReference::None, &observationScopeEnum,
    },
    PayloadFieldDescriptor{
        "施放匹配", false, PayloadNodeShape::String, "綁定武功", {},
        PayloadSchemaReference::None, &castMatchEnum,
    },
    PayloadFieldDescriptor{
        "目標", false, PayloadNodeShape::Selector, "自身", {},
        PayloadSchemaReference::Selector,
    },
    PayloadFieldDescriptor{
        "條件", false, PayloadNodeShape::ConditionList, R"(- 已接受命中)", {},
        PayloadSchemaReference::ConditionList,
    },
    PayloadFieldDescriptor{ "機率", false, PayloadNodeShape::Integer, "50" },
    PayloadFieldDescriptor{ "次數", false, PayloadNodeShape::Integer, "1" },
    PayloadFieldDescriptor{ "同來源冷卻幀數", false, PayloadNodeShape::Integer, "10" },
    PayloadFieldDescriptor{
        "間隔幀數", false, PayloadNodeShape::Integer, "30", R"(時機: 每隔
間隔幀數: 30
獲得護盾: 1)",
    },
    PayloadFieldDescriptor{ "每N次事件", false, PayloadNodeShape::Integer, "2" },
    PayloadFieldDescriptor{
        "觸發限制", false, PayloadNodeShape::Map, R"(範圍: 每次施放每個目標
次數: 1)", R"(時機: 命中
觸發限制:
  範圍: 每次施放每個目標
  次數: 1
獲得護盾: 1)", PayloadSchemaReference::Payload, nullptr,
        &activationLimitPayload,
    },
    PayloadFieldDescriptor{
        "重複次數", false, PayloadNodeShape::Number, "2", {},
        PayloadSchemaReference::EffectNumber,
    },
    PayloadFieldDescriptor{
        "動作", false, PayloadNodeShape::ActionList, R"(- 獲得護盾: 1)", R"(時機: 造成傷害後
動作:
  - 獲得護盾: 1)", PayloadSchemaReference::ActionList,
    },
};
static constexpr PayloadDescriptor rulePayload{
    "效果規則", ruleFields, R"(時機: 造成傷害後
獲得護盾: 1)", PayloadDynamicKeyClass::NamedAction,
    PayloadNodeShape::Any, "獲得護盾", "1", 2,
};

enum class PayloadProbeKind
{
    EffectNumber,
    Selector,
    Condition,
    Action,
    Macro,
    ActivationLimit,
    AttackRuntimeBehavior,
    AreaModifier,
    AreaProjectile,
    AutoUltimate,
    AttributePercentage,
    Rule,
};

struct PayloadProbeDescriptor
{
    const PayloadDescriptor* payload;
    PayloadProbeKind kind;
    std::string_view authorName{};
};

static constexpr std::array payloadProbeDescriptors{
    PayloadProbeDescriptor{ &activationLimitPayload, PayloadProbeKind::ActivationLimit },
    PayloadProbeDescriptor{ &effectNumberPayload, PayloadProbeKind::EffectNumber },
    PayloadProbeDescriptor{ &selectorPayload, PayloadProbeKind::Selector },
    PayloadProbeDescriptor{ &conditionEmptyPayload, PayloadProbeKind::Condition, "僅限絕招" },
    PayloadProbeDescriptor{ &conditionPercentPayload, PayloadProbeKind::Condition, "自身生命低於" },
    PayloadProbeDescriptor{ &conditionStatusPayload, PayloadProbeKind::Condition, "自身有狀態" },
    PayloadProbeDescriptor{ &conditionStackPayload, PayloadProbeKind::Condition, "自身層數至少" },
    PayloadProbeDescriptor{ &conditionCountPayload, PayloadProbeKind::Condition, "不同目標數至少" },
    PayloadProbeDescriptor{ &conditionOrdinalPayload, PayloadProbeKind::Condition, "攻擊序號" },
    PayloadProbeDescriptor{ &conditionHealKindsPayload, PayloadProbeKind::Condition, "治療種類符合" },
    PayloadProbeDescriptor{ &conditionDamageKindsPayload, PayloadProbeKind::Condition, "傷害種類符合" },
    PayloadProbeDescriptor{ &conditionAcceptedHitPayload, PayloadProbeKind::Condition, "已接受命中" },
    PayloadProbeDescriptor{ &attributeModifierPayload, PayloadProbeKind::Action, "屬性修正" },
    PayloadProbeDescriptor{ &damageModifierPayload, PayloadProbeKind::Action, "傷害修正" },
    PayloadProbeDescriptor{ &resourceChangePayload, PayloadProbeKind::Action, "資源變更" },
    PayloadProbeDescriptor{ &healTransactionModifierPayload, PayloadProbeKind::Action, "治療交易修正" },
    PayloadProbeDescriptor{ &applyStatusPayload, PayloadProbeKind::Action, "套用狀態" },
    PayloadProbeDescriptor{ &poisonApplicationPayload, PayloadProbeKind::Action, "施加中毒" },
    PayloadProbeDescriptor{ &consumeStatusPayload, PayloadProbeKind::Action, "消耗狀態" },
    PayloadProbeDescriptor{ &consumeThisStatusPayload, PayloadProbeKind::Action, "消耗此狀態" },
    PayloadProbeDescriptor{ &suppressCurrentCastContactsPayload, PayloadProbeKind::Action, "使本次施放攻擊落空" },
    PayloadProbeDescriptor{ &makeIncomingAttackMissPayload, PayloadProbeKind::Action, "使本次受到攻擊落空" },
    PayloadProbeDescriptor{ &blockPositiveDamagePayload, PayloadProbeKind::Action, "抵擋非處決正傷害" },
    PayloadProbeDescriptor{ &removeStatusPayload, PayloadProbeKind::Action, "移除狀態" },
    PayloadProbeDescriptor{ &dealDamagePayload, PayloadProbeKind::Action, "造成傷害" },
    PayloadProbeDescriptor{ &modifyAttackPayload, PayloadProbeKind::Action, "修改攻擊" },
    PayloadProbeDescriptor{ &forceMovePayload, PayloadProbeKind::Action, "強制移動" },
    PayloadProbeDescriptor{ &createAreaPayload, PayloadProbeKind::Action, "建立區域" },
    PayloadProbeDescriptor{ &modifyCastPayload, PayloadProbeKind::Action, "修改施放" },
    PayloadProbeDescriptor{ &changeStateValuePayload, PayloadProbeKind::Action, "變更狀態值" },
    PayloadProbeDescriptor{ &transferStateValuePayload, PayloadProbeKind::Action, "轉移狀態值" },
    PayloadProbeDescriptor{ &recordMaximumDamagePayload, PayloadProbeKind::Action, "記錄最大招式生命傷害" },
    PayloadProbeDescriptor{ &consumeRecordAsDamagePayload, PayloadProbeKind::Action, "消耗記錄為傷害" },
    PayloadProbeDescriptor{ &consumeRecordAsShieldPayload, PayloadProbeKind::Action, "消耗記錄為護盾" },
    PayloadProbeDescriptor{ &startDamageAbsorptionPayload, PayloadProbeKind::Action, "開始傷害吸收" },
    PayloadProbeDescriptor{ &settleDamageAbsorptionPayload, PayloadProbeKind::Action, "結算傷害吸收" },
    PayloadProbeDescriptor{ &borrowEffectRulesPayload, PayloadProbeKind::Action, "借用效果規則" },
    PayloadProbeDescriptor{ &copyAttackDefinitionPayload, PayloadProbeKind::Action, "複製攻擊定義" },
    PayloadProbeDescriptor{ &settleRemainingPoisonDamagePayload, PayloadProbeKind::Action, "結算剩餘中毒傷害" },
    PayloadProbeDescriptor{ &generateClonesPayload, PayloadProbeKind::Action, "生成分身" },
    PayloadProbeDescriptor{ &preventDeathPayload, PayloadProbeKind::Action, "死亡庇護" },
    PayloadProbeDescriptor{ &configureProtectRepositionPayload, PayloadProbeKind::Action, "保護挪移" },
    PayloadProbeDescriptor{ &configureExecuteRepositionPayload, PayloadProbeKind::Action, "處決挪移" },
    PayloadProbeDescriptor{ &conditionalPayload, PayloadProbeKind::Action, "條件分支" },
    PayloadProbeDescriptor{ &attackRuntimeBehaviorPayload, PayloadProbeKind::AttackRuntimeBehavior },
    PayloadProbeDescriptor{ &areaModifierPayload, PayloadProbeKind::AreaModifier },
    PayloadProbeDescriptor{ &areaProjectilePayload, PayloadProbeKind::AreaProjectile },
    PayloadProbeDescriptor{ &autoUltimatePayload, PayloadProbeKind::AutoUltimate },
    PayloadProbeDescriptor{ &attributePercentagePayload, PayloadProbeKind::AttributePercentage },
    PayloadProbeDescriptor{ &attributeBonusPayload, PayloadProbeKind::Macro, "屬性加成" },
    PayloadProbeDescriptor{ &resourceMacroPayload, PayloadProbeKind::Macro, "回復資源" },
    PayloadProbeDescriptor{ &healMacroPayload, PayloadProbeKind::Macro, "回復生命" },
    PayloadProbeDescriptor{ &forceMoveMacroPayload, PayloadProbeKind::Macro, "擊退" },
    PayloadProbeDescriptor{ &rulePayload, PayloadProbeKind::Rule },
};

template <typename Descriptor, std::size_t Size>
consteval bool descriptorNamesAreUnique(const std::array<Descriptor, Size>& descriptors)
{
    for (std::size_t i = 0; i < Size; ++i)
        for (std::size_t j = i + 1; j < Size; ++j)
            if (descriptors[i].name == descriptors[j].name) return false;
    return true;
}

template <typename Descriptor, std::size_t Size>
consteval bool descriptorIndicesCoverVariants(const std::array<Descriptor, Size>& descriptors)
{
    std::array<bool, Size> seen{};
    for (const auto& descriptor : descriptors)
    {
        if (descriptor.variantIndex >= Size || seen[descriptor.variantIndex]) return false;
        seen[descriptor.variantIndex] = true;
    }
    return std::ranges::all_of(seen, [](bool value) { return value; });
}

template <typename Variant, typename Descriptor, std::size_t Size>
consteval bool descriptorIndicesCoverVariantsAllowingAliases(
    const std::array<Descriptor, Size>& descriptors)
{
    std::array<bool, std::variant_size_v<Variant>> seen{};
    for (const auto& descriptor : descriptors)
    {
        if (descriptor.variantIndex >= seen.size()) return false;
        seen[descriptor.variantIndex] = true;
    }
    return std::ranges::all_of(seen, [](bool value) { return value; });
}

template <typename Variant, typename Descriptor, std::size_t Size>
consteval bool descriptorIndicesAreUniqueAndInRange(
    const std::array<Descriptor, Size>& descriptors)
{
    std::array<bool, std::variant_size_v<Variant>> seen{};
    for (const auto& descriptor : descriptors)
    {
        if (descriptor.variantIndex >= seen.size() || seen[descriptor.variantIndex])
            return false;
        seen[descriptor.variantIndex] = true;
    }
    return true;
}

template <typename Left, std::size_t LeftSize, typename Right, std::size_t RightSize>
consteval bool descriptorNamesAreDisjoint(
    const std::array<Left, LeftSize>& left,
    const std::array<Right, RightSize>& right)
{
    for (const auto& a : left)
        for (const auto& b : right)
            if (a.name == b.name) return false;
    return true;
}

template <typename Descriptor, std::size_t Size, std::size_t FieldCount>
consteval bool descriptorNamesAreDisjointFromFields(
    const std::array<Descriptor, Size>& descriptors,
    const std::array<PayloadFieldDescriptor, FieldCount>& fields)
{
    for (const auto& descriptor : descriptors)
        for (const auto& field : fields)
            if (descriptor.name == field.name) return false;
    return true;
}

consteval bool payloadFieldNamesAreUnique(const PayloadDescriptor& descriptor)
{
    for (std::size_t i = 0; i < descriptor.fields.size(); ++i)
        for (std::size_t j = i + 1; j < descriptor.fields.size(); ++j)
            if (descriptor.fields[i].name == descriptor.fields[j].name) return false;
    return true;
}

template <typename Descriptor, std::size_t Size>
consteval bool descriptorPayloadFieldsAreUnique(
    const std::array<Descriptor, Size>& descriptors)
{
    for (const auto& descriptor : descriptors)
        if (descriptor.payload && !payloadFieldNamesAreUnique(*descriptor.payload)) return false;
    return true;
}

consteval bool authorEnumMetadataIsValid()
{
    for (std::size_t index = 0; index < authorEnumDescriptors.size(); ++index)
    {
        const auto& descriptor = *authorEnumDescriptors[index];
        if (descriptor.name.empty() || descriptor.labels.empty()) return false;
        for (std::size_t other = index + 1; other < authorEnumDescriptors.size(); ++other)
            if (descriptor.name == authorEnumDescriptors[other]->name) return false;
        for (std::size_t label = 0; label < descriptor.labels.size(); ++label)
            for (std::size_t other = label + 1; other < descriptor.labels.size(); ++other)
                if (descriptor.labels[label].name == descriptor.labels[other].name) return false;
    }
    return true;
}

consteval bool payloadMetadataIsComplete(const PayloadDescriptor& descriptor)
{
    if (descriptor.name.empty() || descriptor.minimalProbe.empty()
        || !payloadFieldNamesAreUnique(descriptor)) return false;
    if (descriptor.dynamicKeyClass != PayloadDynamicKeyClass::None
        && (descriptor.dynamicProbeKey.empty() || descriptor.dynamicProbeValue.empty())) return false;
    if (!descriptor.dynamicAlternativeField.empty()
        && (descriptor.dynamicKeyClass == PayloadDynamicKeyClass::None
            || std::ranges::none_of(descriptor.fields, [&](const auto& field)
            {
                return field.name == descriptor.dynamicAlternativeField;
            }))) return false;
    for (const auto& field : descriptor.fields)
    {
        if (field.probeValue.empty()) return false;
        if (field.enumLabels && field.enumLabels->labels.empty()) return false;
        const bool nested = field.schemaReference == PayloadSchemaReference::Payload
            || field.schemaReference == PayloadSchemaReference::PayloadList;
        if (nested != (field.nestedPayload != nullptr)) return false;
        if ((field.shape == PayloadNodeShape::String
                || field.shape == PayloadNodeShape::StringOrSequence)
            && !field.enumLabels
            && field.schemaReference != PayloadSchemaReference::Timing) return false;
    }
    return true;
}

consteval bool payloadProbeRegistryIsComplete()
{
    for (std::size_t index = 0; index < payloadProbeDescriptors.size(); ++index)
    {
        if (!payloadMetadataIsComplete(*payloadProbeDescriptors[index].payload)) return false;
        for (std::size_t other = index + 1; other < payloadProbeDescriptors.size(); ++other)
            if (payloadProbeDescriptors[index].payload == payloadProbeDescriptors[other].payload)
                return false;
    }
    return true;
}

static_assert(conditionDescriptors.size() + 1 == std::variant_size_v<EffectCondition>);
static_assert(descriptorIndicesCoverVariantsAllowingAliases<EffectActionValue>(actionDescriptors));
static_assert(descriptorIndicesAreUniqueAndInRange<EffectCondition>(conditionDescriptors));
static_assert(descriptorNamesAreUnique(timingDescriptors));
static_assert(descriptorNamesAreUnique(actionDescriptors));
static_assert(descriptorNamesAreUnique(conditionDescriptors));
static_assert(descriptorNamesAreUnique(macroDescriptors));
static_assert(authorEnumMetadataIsValid());
static_assert(payloadProbeRegistryIsComplete());
static_assert(std::ranges::all_of(actionDescriptors, [](const auto& descriptor)
{
    return descriptor.payload && !descriptor.payload->minimalProbe.empty();
}));
static_assert(std::ranges::all_of(conditionDescriptors, [](const auto& descriptor)
{
    return descriptor.payload && !descriptor.payload->minimalProbe.empty();
}));
static_assert(std::ranges::all_of(macroDescriptors, [](const auto& descriptor)
{
    return descriptor.payload && !descriptor.payload->minimalProbe.empty();
}));
static_assert(descriptorPayloadFieldsAreUnique(actionDescriptors));
static_assert(descriptorPayloadFieldsAreUnique(conditionDescriptors));
static_assert(descriptorPayloadFieldsAreUnique(macroDescriptors));
static_assert(descriptorNamesAreDisjoint(actionDescriptors, macroDescriptors));
static_assert(payloadFieldNamesAreUnique(rulePayload));
static_assert(descriptorNamesAreDisjointFromFields(actionDescriptors, ruleFields));
static_assert(descriptorNamesAreDisjointFromFields(macroDescriptors, ruleFields));


}  // namespace Metadata
}  // namespace KysChess::EffectAuthoring::Detail
