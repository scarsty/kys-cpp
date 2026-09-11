#include "ChessEffectDescriptionInternal.h"
#include "ChessBattleEffectSemantics.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>

namespace KysChess::EffectDescriptionDetail
{

struct DescriptionDurationFramesQualifier
{
    int frames{};
    auto operator<=>(const DescriptionDurationFramesQualifier&) const = default;
};

struct DescriptionStackPolicyQualifier
{
    EffectStackPolicy policy{};
    auto operator<=>(const DescriptionStackPolicyQualifier&) const = default;
};

struct DescriptionStackLimitQualifier
{
    int count{};
    auto operator<=>(const DescriptionStackLimitQualifier&) const = default;
};

struct DescriptionStackScopeQualifier
{
    EffectStackScope scope{};
    auto operator<=>(const DescriptionStackScopeQualifier&) const = default;
};

struct DescriptionMaxActivationsQualifier
{
    int count{};
    auto operator<=>(const DescriptionMaxActivationsQualifier&) const = default;
};

struct DescriptionSharedCooldownQualifier
{
    int frames{};
    auto operator<=>(const DescriptionSharedCooldownQualifier&) const = default;
};

struct DescriptionIntervalQualifier
{
    int frames{};
    auto operator<=>(const DescriptionIntervalQualifier&) const = default;
};

struct DescriptionEveryNthEventQualifier
{
    int count{};
    auto operator<=>(const DescriptionEveryNthEventQualifier&) const = default;
};

struct DescriptionRepetitionCountQualifier
{
    EffectNumber count;
    bool operator==(const DescriptionRepetitionCountQualifier&) const = default;
};

struct DescriptionActivationLimitQualifier
{
    EffectActivationScope scope{};
    int count{};
    auto operator<=>(const DescriptionActivationLimitQualifier&) const = default;
};

using DescriptionQualifier = std::variant<
    DescriptionDurationFramesQualifier,
    DescriptionStackPolicyQualifier,
    DescriptionStackLimitQualifier,
    DescriptionStackScopeQualifier,
    DescriptionMaxActivationsQualifier,
    DescriptionSharedCooldownQualifier,
    DescriptionIntervalQualifier,
    DescriptionEveryNthEventQualifier,
    DescriptionRepetitionCountQualifier,
    DescriptionActivationLimitQualifier>;

std::string ruleEventLabel(EffectEvent event, EffectDescriptionStyle style)
{
    const bool detailed = style == EffectDescriptionStyle::Detailed;
    const bool compact = style == EffectDescriptionStyle::Compact;
    switch (event)
    {
    case EffectEvent::BattleInitialized: return detailed ? "戰鬥開始時" : "";
    case EffectEvent::FrameAdvanced: return "每幀";
    case EffectEvent::UltimateCooldownFinished: return compact ? "絕招冷卻完成" : "絕招冷卻完成時";
    case EffectEvent::CastPlanned: return compact ? "準備施放" : detailed ? "規劃施放時" : "準備施放時";
    case EffectEvent::AttackCommitted: return compact ? "出手" : "出手時";
    case EffectEvent::UltimateCommitted: return compact ? "絕招" : "施放絕招時";
    case EffectEvent::AttackSpawned: return compact ? "彈道生成" : "攻擊生成時";
    case EffectEvent::MainProjectileBeforeDamage:
        return compact ? "主彈命中" : detailed ? "主彈道命中、傷害結算前" : "主彈道命中時";
    case EffectEvent::HitBeforeDamage:
        return compact ? "命中" : detailed ? "命中且傷害結算前" : "每次命中時";
    case EffectEvent::DamageResolved: return compact ? "傷害後" : "傷害結算後";
    case EffectEvent::HealAttempted: return compact ? "治療前" : "嘗試治療時";
    case EffectEvent::HealApplied: return compact ? "治療後" : detailed ? "實際治療後" : "治療生效後";
    case EffectEvent::CastContinuation:
        return compact ? "施放延續" : detailed ? "本次施放第一輪攻擊完成後" : "第一輪攻擊後";
    case EffectEvent::CastSettled:
        return compact ? "施放結算" : detailed ? "本次施放全部攻擊結算完成後" : "本次施放結算後";
    case EffectEvent::ShieldBroken: return compact ? "破盾" : "護盾破裂時";
    case EffectEvent::UnitDied: return compact ? "效果持有者死亡" : "效果持有者死亡時";
    case EffectEvent::AllyDied: return compact ? "友軍死亡" : "友軍死亡時";
    }
    assert(false);
    return {};
}

std::string selectorLabel(const EffectSelector& selector, bool compact)
{
    const auto countSuffix = selector.count > 0 ? std::format("{}人", selector.count) : std::string{};
    std::string result;
    switch (selector.kind)
    {
    case EffectSelectorKind::Self: result = "效果持有者"; break;
    case EffectSelectorKind::SourceUnit: result = "造成該次事件的單位"; break;
    case EffectSelectorKind::TransactionTarget: result = "本次交易作用的單位"; break;
    case EffectSelectorKind::HitTarget: result = "被命中的單位"; break;
    case EffectSelectorKind::OriginalAttackTarget: result = "本次攻擊原本選定的目標"; break;
    case EffectSelectorKind::ComboMembers: result = "羈絆成員"; break;
    case EffectSelectorKind::AllLivingUnits:
        result = selector.count > 0 ? std::format("存活單位{}", countSuffix) : "所有存活單位";
        break;
    case EffectSelectorKind::Allies:
        result = selector.count > 0 ? std::format("友軍{}", countSuffix) : "全隊";
        break;
    case EffectSelectorKind::Enemies:
        result = selector.count > 0 ? std::format("敵軍{}", countSuffix) : "所有敵人";
        break;
    case EffectSelectorKind::LowestHpAllies:
        result = selector.count > 0
            ? std::format("生命比例最低的{}名友軍", selector.count)
            : "生命比例最低的友軍";
        break;
    case EffectSelectorKind::LowestMpAllies:
        result = selector.count > 0
            ? std::format("內力最低的{}名友軍", selector.count)
            : "內力最低的友軍";
        break;
    case EffectSelectorKind::HighestMpEnemy: result = "內力最高的敵人"; break;
    case EffectSelectorKind::StrongestEnemies:
        result = selector.count > 0
            ? std::format("最強{}名敵人", selector.count)
            : "最強的敵人";
        break;
    case EffectSelectorKind::NearestEnemies:
        result = selector.count > 0
            ? std::format("最近{}名敵人", selector.count)
            : "最近的敵人";
        break;
    case EffectSelectorKind::FarthestEnemy: result = "最遠的敵人"; break;
    case EffectSelectorKind::UnitsInRadius:
    {
        const auto relation = selector.team == EffectTeamFilter::Ally ? "友軍"
            : selector.team == EffectTeamFilter::Enemy ? "敵軍"
            : "單位";
        result = selector.count > 0
            ? std::format("{}格內至多{}名{}", selector.radiusTiles, selector.count, relation)
            : std::format("{}格內所有{}", selector.radiusTiles, relation);
        break;
    }
    case EffectSelectorKind::UnitsInSquare:
    {
        const auto relation = selector.team == EffectTeamFilter::Ally ? "友軍"
            : selector.team == EffectTeamFilter::Enemy ? "敵軍"
            : "單位";
        result = selector.count > 0
            ? std::format("{}×{}範圍內至多{}名{}",
                selector.squareSideTiles,
                selector.squareSideTiles,
                selector.count,
                relation)
            : std::format("{}×{}範圍內所有{}",
                selector.squareSideTiles,
                selector.squareSideTiles,
                relation);
        break;
    }
    case EffectSelectorKind::AlliesUsingMartialCategory:
        switch (selector.requiredMartialCategory)
        {
        case EffectMartialCategory::Fist: result = compact ? "友方拳掌角色" : "使用拳掌類武功的友軍"; break;
        case EffectMartialCategory::Sword: result = compact ? "友方御劍角色" : "使用御劍類武功的友軍"; break;
        case EffectMartialCategory::Knife: result = compact ? "友方耍刀角色" : "使用耍刀類武功的友軍"; break;
        case EffectMartialCategory::Unusual: result = compact ? "友方特殊角色" : "使用特殊類武功的友軍"; break;
        case EffectMartialCategory::None: assert(false); std::unreachable();
        }
        break;
    case EffectSelectorKind::StatusHolder:
        result = "狀態持有者";
        break;
    }
    assert(!result.empty());
    if (selector.excludeOwner)
    {
        result += "（不含效果持有者）";
    }
    if (selector.requiredBoundMagic)
    {
        result += "（使用此武功）";
    }
    if (selector.requiredTarget)
    {
        switch (*selector.requiredTarget)
        {
        case EffectRequiredTarget::Self: result += "（必含自身）"; break;
        case EffectRequiredTarget::SourceUnit: result += "（必含來源單位）"; break;
        case EffectRequiredTarget::TransactionTarget: result += "（必含交易目標）"; break;
        case EffectRequiredTarget::HitTarget: result += "（必含命中目標）"; break;
        case EffectRequiredTarget::OriginalAttackTarget: result += "（必含原攻擊目標）"; break;
        }
    }
    if (selector.tieBreak == EffectTieBreak::BattleRandom)
    {
        if (selector.kind == EffectSelectorKind::Enemies && selector.count > 0)
            result = "隨機" + result;
        else
            result += "（隨機決定同順位）";
    }
    return result;
}

std::string_view damageKindLabel(BattleDamageKind kind)
{
    switch (kind)
    {
    case BattleDamageKind::Physical: return "物理";
    case BattleDamageKind::Skill: return "招式";
    case BattleDamageKind::Pure: return "純粹";
    case BattleDamageKind::Poison: return "中毒";
    case BattleDamageKind::Bleed: return "流血";
    case BattleDamageKind::Effect: return "特效";
    case BattleDamageKind::Execute: return "處決";
    }
    assert(false);
    return {};
}

std::string_view statusSourceMatchLabel(StatusSourceMatch source)
{
    switch (source)
    {
    case StatusSourceMatch::Any: return {};
    case StatusSourceMatch::EffectOwner: return "僅效果擁有者套用";
    case StatusSourceMatch::EffectBinding: return "僅同一效果綁定";
    case StatusSourceMatch::CurrentContribution: return "僅此狀態貢獻";
    }
    assert(false);
    return {};
}

std::string numberLabel(const EffectNumber& number)
{
    std::string base;
    const bool attackScaledByMissingHp = number.base == EffectNumberBase::SourceAttack
        && number.multiplierBase == EffectNumberBase::SourceMissingHpRatio;
    if (attackScaledByMissingHp)
    {
        base = std::format("目前攻擊×已損生命比例×{}%", number.percent);
    }
    else switch (effectNumberEvaluationKind(number.base))
    {
    case EffectNumberEvaluationKind::Constant:
        break;
    case EffectNumberEvaluationKind::SourceStar:
        base = number.percent % 100 == 0
            ? std::format("星級×{}", number.percent / 100)
            : std::format("星級×{}%", number.percent);
        break;
    case EffectNumberEvaluationKind::SourceAttack: base = std::format("攻擊的{}%", number.percent); break;
    case EffectNumberEvaluationKind::SourceMaxHp: base = std::format("效果持有者最大生命的{}%", number.percent); break;
    case EffectNumberEvaluationKind::SourceMissingHpRatio: base = std::format("已損生命比例的{}%", number.percent); break;
    case EffectNumberEvaluationKind::SourceCurrentMpRatio: base = std::format("目前內力比例的{}%", number.percent); break;
    case EffectNumberEvaluationKind::TargetMaxHp: base = std::format("目標最大生命的{}%", number.percent); break;
    case EffectNumberEvaluationKind::TargetCurrentHp: base = std::format("目標目前生命的{}%", number.percent); break;
    case EffectNumberEvaluationKind::TargetCurrentShield: base = std::format("目標目前護盾的{}%", number.percent); break;
    case EffectNumberEvaluationKind::TargetCurrentCooldown: base = std::format("目標目前冷卻的{}%", number.percent); break;
    case EffectNumberEvaluationKind::FinalHpDamage: base = std::format("實際生命傷害的{}%", number.percent); break;
    case EffectNumberEvaluationKind::SourceStatusQuantity:
        assert(number.status);
        assert(!statusQuantityMeasure(statusCatalogEntry(*number.status).quantity).empty());
        base = number.percent == 100
            ? std::format("{}{}", battleStatusLabel(*number.status),
                statusQuantityMeasure(statusCatalogEntry(*number.status).quantity))
            : std::format("{}{}的{}%", battleStatusLabel(*number.status),
                statusQuantityMeasure(statusCatalogEntry(*number.status).quantity),
                number.percent);
        if (number.statusSource != StatusSourceMatch::Any)
            base += std::format("（{}）", statusSourceMatchLabel(number.statusSource));
        break;
    case EffectNumberEvaluationKind::CurrentContributionQuantity:
        base = number.percent == 100
            ? "此狀態貢獻數量"
            : std::format("此狀態貢獻數量的{}%", number.percent);
        break;
    case EffectNumberEvaluationKind::StoredStateValue: base = std::format("狀態槽值的{}%", number.percent); break;
    case EffectNumberEvaluationKind::ApplicationTargetMaxHp:
        base = std::format("套用目標最大生命的{}%", number.percent);
        break;
    case EffectNumberEvaluationKind::BoundRatio:
        base = std::format("已綁定數值{}/{}的{}%",
            number.boundNumerator,
            number.boundDenominator,
            number.percent);
        break;
    case EffectNumberEvaluationKind::Count:
        assert(false);
        break;
    }

    if (number.multiplierBase && !attackScaledByMissingHp)
    {
        switch (effectNumberEvaluationKind(*number.multiplierBase))
        {
        case EffectNumberEvaluationKind::Constant: base += "×固定基準"; break;
        case EffectNumberEvaluationKind::SourceStar: base += "×星級"; break;
        case EffectNumberEvaluationKind::SourceAttack: base += "×來源攻擊"; break;
        case EffectNumberEvaluationKind::SourceMaxHp: base += "×來源最大生命"; break;
        case EffectNumberEvaluationKind::SourceMissingHpRatio: base += "×來源已損生命比例"; break;
        case EffectNumberEvaluationKind::SourceCurrentMpRatio: base += "×來源目前內力比例"; break;
        case EffectNumberEvaluationKind::TargetMaxHp: base += "×目標最大生命"; break;
        case EffectNumberEvaluationKind::TargetCurrentHp: base += "×目標目前生命"; break;
        case EffectNumberEvaluationKind::TargetCurrentShield: base += "×目標目前護盾"; break;
        case EffectNumberEvaluationKind::TargetCurrentCooldown: base += "×目標目前冷卻"; break;
        case EffectNumberEvaluationKind::FinalHpDamage: base += "×實際生命傷害"; break;
        case EffectNumberEvaluationKind::SourceStatusQuantity:
            assert(number.status);
            assert(!statusQuantityMeasure(statusCatalogEntry(*number.status).quantity).empty());
            base += std::format("×{}{}", battleStatusLabel(*number.status),
                statusQuantityMeasure(statusCatalogEntry(*number.status).quantity));
            if (number.statusSource != StatusSourceMatch::Any)
                base += std::format("（{}）", statusSourceMatchLabel(number.statusSource));
            break;
        case EffectNumberEvaluationKind::CurrentContributionQuantity:
            base += "×此狀態貢獻數量";
            break;
        case EffectNumberEvaluationKind::StoredStateValue: base += "×狀態槽值"; break;
        case EffectNumberEvaluationKind::ApplicationTargetMaxHp: base += "×套用目標最大生命"; break;
        case EffectNumberEvaluationKind::BoundRatio:
            base += std::format("×已綁定數值{}/{}",
                number.boundNumerator,
                number.boundDenominator);
            break;
        case EffectNumberEvaluationKind::Count:
            assert(false);
            break;
        }
    }
    if (number.base == EffectNumberBase::Constant)
    {
        return std::to_string(number.flat);
    }
    if (number.flat == 0)
    {
        return base;
    }
    return number.flat > 0
        ? std::format("{}＋{}", base, number.flat)
        : std::format("{}{}", base, number.flat);
}

std::string boundedNumberLabel(const EffectNumber& number)
{
    auto result = numberLabel(number);
    if (number.rounding == EffectRounding::Ceil)
    {
        result += "（向上取整）";
    }
    else if (number.rounding == EffectRounding::Floor)
    {
        result += "（向下取整）";
    }
    else if (number.rounding == EffectRounding::Nearest)
    {
        result += "（四捨五入）";
    }
    if (number.minimum)
    {
        result += std::format("，至少{}", *number.minimum);
    }
    if (number.maximum)
    {
        result += std::format("，至多{}", *number.maximum);
    }
    if (number.statusScale == StatusNumberScale::PerContributionLayer)
        result = "每層" + result;
    return result;
}

std::string descriptionNumberLabel(
    const EffectNumber& number,
    EffectDescriptionStyle style)
{
    if (style == EffectDescriptionStyle::Detailed)
        return boundedNumberLabel(number);

    auto result = [&]
    {
        if (const auto constant = effectiveConstantEffectNumberValue(number))
            return std::to_string(*constant);
        return numberLabel(number);
    }();
    if (style == EffectDescriptionStyle::Compact)
    {
        if (number.minimum) result += std::format("，至少{}", *number.minimum);
        if (number.maximum) result += std::format("，至多{}", *number.maximum);
    }
    else if (number.minimum || number.maximum)
    {
        result += "（";
        if (number.minimum) result += std::format("至少{}", *number.minimum);
        if (number.minimum && number.maximum) result += "，";
        if (number.maximum) result += std::format("至多{}", *number.maximum);
        result += "）";
    }
    if (number.statusScale == StatusNumberScale::PerContributionLayer)
        result = "每層" + result;
    return result;
}

std::string statusDurationLabel(
    const ApplyStatusAction& status,
    EffectDescriptionStyle style)
{
    if (status.duration)
        return descriptionNumberLabel(*status.duration, style);
    if (status.durationFrames > 0)
        return std::to_string(status.durationFrames);
    return {};
}

std::string_view borrowedRuleActionCategoryLabel(
    BorrowedRuleActionCategory category)
{
    switch (category)
    {
    case BorrowedRuleActionCategory::AttributeModifier: return "屬性修正";
    case BorrowedRuleActionCategory::DamageModifier: return "傷害修正";
    case BorrowedRuleActionCategory::ResourceChange: return "資源變更";
    case BorrowedRuleActionCategory::HealTransactionModifier: return "治療交易修正";
    case BorrowedRuleActionCategory::Status: return "狀態";
    case BorrowedRuleActionCategory::Damage: return "傷害";
    case BorrowedRuleActionCategory::Attack: return "攻擊";
    case BorrowedRuleActionCategory::ForcedMovement: return "強制移動";
    case BorrowedRuleActionCategory::Area: return "區域";
    case BorrowedRuleActionCategory::Cast: return "修改施放";
    case BorrowedRuleActionCategory::StateValue: return "狀態值";
    case BorrowedRuleActionCategory::DamageMemory: return "傷害記憶";
    case BorrowedRuleActionCategory::DamageAbsorption: return "傷害吸收";
    case BorrowedRuleActionCategory::StatusDamageSettlement: return "狀態傷害結算";
    }
    assert(false);
    return {};
}

std::string borrowedRuleFilterLabel(const BorrowedRuleFilter& filter)
{
    std::string result;
    for (const auto category : filter.allowedActionCategories)
    {
        if (!result.empty())
        {
            result += "、";
        }
        result += borrowedRuleActionCategoryLabel(category);
    }
    return result;
}

std::string_view copiedMagicConditionLabel(CopiedMagicCondition condition)
{
    switch (condition)
    {
    case CopiedMagicCondition::HasUltimateAttackDefinition:
        return "有絕招攻擊定義";
    case CopiedMagicCondition::ExcludesRecursiveEffects:
        return "排除複製與借用遞迴";
    }
    assert(false);
    return {};
}

std::string copiedMagicFilterLabel(const CopiedMagicFilter& filter)
{
    std::string result;
    for (const auto condition : filter.conditions)
    {
        if (!result.empty())
        {
            result += "、";
        }
        result += copiedMagicConditionLabel(condition);
    }
    return result;
}

std::string attributeLabel(BattleAttribute attribute, bool compact)
{
    switch (attribute)
    {
    case BattleAttribute::MaxHp: return compact ? "生命" : "最大生命";
    case BattleAttribute::Attack: return compact ? "攻" : "攻擊";
    case BattleAttribute::Defence: return compact ? "防" : "防禦";
    case BattleAttribute::Speed: return "速度";
    case BattleAttribute::CriticalChance: return compact ? "暴擊" : "暴擊率";
    case BattleAttribute::CriticalDamage: return "暴擊傷害";
    case BattleAttribute::GuaranteedHit: return "必中（無視閃避與格擋）";
    case BattleAttribute::DodgeChance: return compact ? "閃避" : "閃避率";
    case BattleAttribute::BlockChance: return compact ? "格擋" : "格擋率";
    case BattleAttribute::DamageReduction: return "傷害減免";
    case BattleAttribute::SkillDamage: return "技能傷害";
    case BattleAttribute::ProjectilePressureDamage: return compact ? "彈壓傷" : "彈道壓制傷害";
    case BattleAttribute::CooldownReduction: return "冷卻縮減";
    case BattleAttribute::MpRecoveryBonus: return compact ? "回內加成" : "內力回復加成";
    case BattleAttribute::StaggerResistance: return compact ? "僵抗" : "僵直抗性";
    case BattleAttribute::ProjectileReflectChance: return compact ? "彈反" : "彈道反射率";
    case BattleAttribute::SkillReflectPercent: return "技能反彈百分比";
    case BattleAttribute::CounterUltimateBlockChance: return compact ? "格擋反招" : "格擋絕招反擊率";
    case BattleAttribute::CriticalAfterDodge: return "閃避後暴擊";
    case BattleAttribute::DashChance: return "滑步機率";
    case BattleAttribute::OutgoingCooldownExtensionChance: return "攻擊冷卻延長率";
    case BattleAttribute::OutgoingCooldownExtensionPercent: return "攻擊冷卻延長百分比";
    case BattleAttribute::IncomingCooldownExtensionChance: return "受擊冷卻延長反擊率";
    case BattleAttribute::IncomingCooldownExtensionPercent: return "受擊冷卻延長百分比";
    }
    assert(false);
    return {};
}

std::string attackPatternLabel(
    const AttackPattern& pattern,
    bool compact)
{
    switch (pattern.kind)
    {
    case AttackPatternKind::Preserve: return compact ? "原彈道" : "沿用彈道樣式";
    case AttackPatternKind::Fan: return "扇形";
    case AttackPatternKind::Flanks: return "側翼";
    case AttackPatternKind::SamePointSequence: return "同落點延遲";
    case AttackPatternKind::MultiTarget: return "多目標";
    case AttackPatternKind::EchoNearestOthers: return "殘影";
    }
    std::unreachable();
}

std::string joinedDescriptionLabels(
    std::span<const std::string> labels,
    bool compact)
{
    std::string result;
    for (const auto& label : labels)
    {
        if (!result.empty()) result += compact ? "；" : "、";
        result += label;
    }
    return result;
}

std::string conditionLabel(
    const EffectCondition& condition,
    bool compact,
    std::optional<EffectEvent> event)
{
    return std::visit(
        [compact, event](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, IsUltimateCondition>) return "為絕招";
            else if constexpr (std::is_same_v<T, CastUsesEffectSourceMagicCondition>) return "施放武功為效果來源";
            else if constexpr (std::is_same_v<T, IsMainProjectileCondition>) return "為主彈道";
            else if constexpr (std::is_same_v<T, IsRootAttackCondition>) return "為根攻擊";
            else if constexpr (std::is_same_v<T, SourceHpRatioAtMostCondition>) return compact
                ? std::format("持有者生命不高於{}%", typed.percent)
                : std::format("效果持有者生命不高於{}%", typed.percent);
            else if constexpr (std::is_same_v<T, SourceHpRatioBelowCondition>) return compact
                ? std::format("持有者生命低於{}%", typed.percent)
                : std::format("效果持有者生命低於{}%", typed.percent);
            else if constexpr (std::is_same_v<T, SourceIsLastAliveCondition>) return compact ? "持有者為最後存活單位" : "效果持有者為最後存活單位";
            else if constexpr (std::is_same_v<T, TargetHpRatioAtMostCondition>) return std::format("目標生命不高於{}%", typed.percent);
            else if constexpr (std::is_same_v<T, TargetNotInvincibleCondition>) return "目標非無敵";
            else if constexpr (std::is_same_v<T, SourceHasStateCondition>) return compact
                ? std::format("持有者有{}", battleStatusLabel(typed.state))
                : std::format("效果持有者有{}", battleStatusLabel(typed.state));
            else if constexpr (std::is_same_v<T, TargetHasStateCondition>) return compact
                ? std::format("目標有{}", battleStatusLabel(typed.state))
                : std::format("目標有{}", battleStatusLabel(typed.state));
            else if constexpr (std::is_same_v<T, TargetHasStateFromEffectOwnerCondition>) return compact
                ? std::format("目標有持有者施加的{}", battleStatusLabel(typed.state))
                : std::format("目標有由效果持有者施加的{}", battleStatusLabel(typed.state));
            else if constexpr (std::is_same_v<T, SourceStackAtLeastCondition>) return compact
                ? std::format("持有者{}至少{}層", battleStatusLabel(typed.stack), typed.count)
                : std::format("效果持有者的{}至少{}層", battleStatusLabel(typed.stack), typed.count);
            else if constexpr (std::is_same_v<T, OtherLivingAllyUsesBoundMagicCondition>) return "另一名同武功友軍存活";
            else if constexpr (std::is_same_v<T, CastDistinctTargetCountAtLeastCondition>) return std::format("本次命中至少{}名不同敵人", typed.count);
            else if constexpr (std::is_same_v<T, AttackOrdinalEqualsCondition>) return std::format("為第{}道攻擊", typed.ordinal + 1);
            else if constexpr (std::is_same_v<T, HealKindInCondition>)
                return std::format("治療種類為{}", joinedDescriptionLabels(typed.kinds, compact));
            else if constexpr (std::is_same_v<T, DamageOriginIsAttackCondition>) return compact ? "為招式傷害" : "傷害來自招式";
            else if constexpr (std::is_same_v<T, DamageKilledTargetCondition>) return compact ? "傷害致死" : "該次傷害造成死亡";
            else if constexpr (std::is_same_v<T, AcceptedHitCondition>)
            {
                std::string result = "已接受命中";
                if (typed.requirePositiveDamage) result += "且傷害為正";
                return result;
            }
            else if constexpr (std::is_same_v<T, EventTargetBelongsToBoundSourceCondition>)
            {
                assert(!event || *event == EffectEvent::AllyDied
                    || *event == EffectEvent::UnitDied);
                return compact
                    ? "死亡單位屬於此羈絆"
                    : "死亡單位屬於此羈絆來源";
            }
            else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>)
                return typed.perspective == DamagePerspective::Dealt
                    ? compact ? "持有者造成傷害" : "效果持有者造成傷害"
                    : compact ? "持有者承受傷害" : "效果持有者承受傷害";
            else if constexpr (std::is_same_v<T, DamageKindInCondition>)
                return std::format("傷害種類為{}", joinedDescriptionLabels(typed.kinds, compact));
            else if constexpr (std::is_same_v<T, TargetMpWasFullBeforeCastCondition>) return "受益者施放前內力已滿";
            else if constexpr (std::is_same_v<T, RandomSelectionAvailableCondition>) return "有合法隨機目標";
            else if constexpr (std::is_same_v<T, TargetIsStatusHolderCondition>) return "目標為狀態持有者";
            else static_assert(false, "Unhandled effect condition");
        },
        condition);
}

bool descriptionQualifierIsSuppressed(
    std::span<const DescriptionQualifier> suppressed,
    const DescriptionQualifier& qualifier)
{
    return std::ranges::find(suppressed, qualifier) != suppressed.end();
}

std::string_view stackPolicyLabel(
    EffectStackPolicy policy,
    bool compact,
    bool status)
{
    switch (policy)
    {
    case EffectStackPolicy::Independent:
        return status ? "各次狀態獨立存在" : "各次效果獨立存在";
    case EffectStackPolicy::Refresh:
        return status
            ? compact ? "刷新" : "重複施加時只刷新持續時間"
            : compact ? "刷新" : "重複套用時只刷新持續時間";
    case EffectStackPolicy::Replace:
        return status
            ? compact ? "取代既有狀態" : "重複施加時會取代既有狀態"
            : compact ? "取代既有效果" : "重複套用時會取代既有效果";
    case EffectStackPolicy::KeepStrongest:
        return status
            ? compact ? "只留最強" : "重複施加時只保留最強狀態"
            : compact ? "只留最強" : "重複套用時只保留最強效果";
    case EffectStackPolicy::AddStack:
        return status
            ? compact ? "增加層數" : "重複施加時會增加層數"
            : compact ? "增加層數" : "重複套用時會增加層數";
    }
    assert(false);
    return {};
}

std::string_view damageChannelLabel(DamageChannel channel)
{
    switch (channel)
    {
    case DamageChannel::Skill: return "招式傷害";
    case DamageChannel::Dot: return "持續傷害";
    case DamageChannel::Effect: return "特效傷害";
    case DamageChannel::All: return "所有傷害";
    }
    assert(false);
    return {};
}

std::string_view damageChannelSourceLabel(DamageChannel channel)
{
    switch (channel)
    {
    case DamageChannel::Skill: return "招式";
    case DamageChannel::Dot: return "持續效果";
    case DamageChannel::Effect: return "特效";
    case DamageChannel::All: return "任意來源";
    }
    assert(false);
    return {};
}

std::string_view damageStageLabel(DamageModifierStage stage)
{
    switch (stage)
    {
    case DamageModifierStage::BeforeDefense: return "防禦結算前";
    case DamageModifierStage::AfterDefense: return "防禦結算後";
    case DamageModifierStage::Final: return "最終結算";
    }
    assert(false);
    return {};
}

std::string_view healKindLabel(EffectHealKind kind)
{
    switch (kind)
    {
    case EffectHealKind::Direct: return "直接治療";
    case EffectHealKind::Team: return "隊伍治療";
    case EffectHealKind::Aura: return "治療光環";
    case EffectHealKind::OnHit: return "命中治療";
    case EffectHealKind::KillReward: return "擊殺治療";
    case EffectHealKind::DeathMedical: return "死亡醫療";
    case EffectHealKind::Rescue: return "救援治療";
    case EffectHealKind::Regeneration: return "持續回復";
    case EffectHealKind::Lifesteal: return "吸血";
    case EffectHealKind::Count: break;
    }
    assert(false);
    return {};
}

void appendTimedStackQualifiers(
    std::string& result,
    int durationFrames,
    EffectStackPolicy stack,
    const std::optional<int>& stackLimit,
    EffectStackScope stackScope,
    EffectDescriptionStyle style,
    bool status,
    std::span<const DescriptionQualifier> suppressed)
{
    const bool detailed = style == EffectDescriptionStyle::Detailed;
    const bool compact = style == EffectDescriptionStyle::Compact;
    const DescriptionQualifier duration = DescriptionDurationFramesQualifier{ durationFrames };
    const DescriptionQualifier stackPolicy = DescriptionStackPolicyQualifier{ stack };
    const DescriptionQualifier limit = DescriptionStackLimitQualifier{
        stackLimit.value_or(0)};
    const bool combinedCompactStackCap = compact
        && stack == EffectStackPolicy::AddStack
        && stackLimit
        && !descriptionQualifierIsSuppressed(suppressed, stackPolicy)
        && !descriptionQualifierIsSuppressed(suppressed, limit);
    if (combinedCompactStackCap)
    {
        result += std::format("，可疊至{}層", *stackLimit);
    }
    else if (stack != EffectStackPolicy::Independent
        && !descriptionQualifierIsSuppressed(suppressed, stackPolicy))
    {
        result += compact ? "，" : detailed ? "；" : "，";
        result += stackPolicyLabel(stack, compact, status);
    }

    if (stackLimit)
    {
        if (!combinedCompactStackCap
            && !descriptionQualifierIsSuppressed(suppressed, limit))
            result += std::format("，最多{}層", *stackLimit);
    }
    if (stackScope == EffectStackScope::EventSource)
    {
        const DescriptionQualifier qualifier = DescriptionStackScopeQualifier{ stackScope };
        if (!descriptionQualifierIsSuppressed(suppressed, qualifier))
            result += compact ? "，分來源疊加" : "，各事件來源分別疊加";
    }
    if (durationFrames > 0 && !descriptionQualifierIsSuppressed(suppressed, duration))
        result += compact
            ? std::format("，{}幀", durationFrames)
            : detailed
            ? std::format("，{}幀", durationFrames)
            : std::format("，持續{}幀", durationFrames);
}

bool usesStackingOutgoingSkillDamagePhrase(const ModifyDamageAction& action)
{
    const auto amount = effectiveConstantEffectNumberValue(action.amount);
    return action.perspective == DamageModifierPerspective::Outgoing
        && action.channel == DamageChannel::Skill
        && action.stage == DamageModifierStage::AfterDefense
        && action.operation == DamageModifierOperation::PercentAdd
        && amount
        && *amount > 0
        && action.durationFrames > 0
        && action.stack == EffectStackPolicy::AddStack
        && action.stackLimit.has_value()
        && action.stackScope == EffectStackScope::Shared;
}

std::string renderDescriptionActionArgument(
    const EffectActionValue& action,
    EffectDescriptionStyle style,
    std::span<const DescriptionQualifier> suppressed = {},
    const EffectDescriptionPresentationContext& context = {});

std::optional<std::string> renderCanonicalPoisonApplication(
    const ApplyStatusAction& application,
    EffectDescriptionStyle style,
    const EffectDescriptionPresentationContext& context)
{
    if (application.status != BattleStatusKind::Poison
        || !application.behavior)
    {
        return std::nullopt;
    }
    const auto* charges = std::get_if<SetStatusTriggerCharges>(
        &application.quantity);
    if (!charges) return std::nullopt;
    const auto capability = canonicalPoisonDamageCapability(*application.behavior);
    if (!capability
        || application.behavior->rules.size() != 1
        || capability->rule->actions.size() != 2) return std::nullopt;
    const auto* periodicRule = capability->rule;
    const auto* poisonDamage = capability->damage;

    const bool compact = style == EffectDescriptionStyle::Compact;
    auto damage = poisonDamage->amount.flat == 0
            && poisonDamage->amount.percent > 0
        ? std::format("目前生命{}%", poisonDamage->amount.percent)
        : descriptionNumberLabel(poisonDamage->amount, style);
    if (poisonDamage->amount.minimum > 0)
    {
        if (context.compactPolicy != EffectDescriptionCompactPolicy::PlayerCard)
        {
            damage += compact
                ? std::format("、最低{}", *poisonDamage->amount.minimum)
                : std::format("（最低{}點）", *poisonDamage->amount.minimum);
        }
    }
    const auto duration = application.duration
        ? descriptionNumberLabel(*application.duration, style)
        : std::to_string(application.durationFrames);

    std::string result = compact
        ? context.compactPolicy == EffectDescriptionCompactPolicy::PlayerCard
            ? std::format(
                "中毒（{}次、{}幀；每{}幀造成{}傷害{}；每次消耗1次）",
                charges->count,
                duration,
                periodicRule->intervalFrames,
                damage,
                poisonDamage->amount.minimum
                    ? std::format("，最低{}", *poisonDamage->amount.minimum)
                    : std::string{})
            : std::format(
                "中毒（{}次、{}幀；每{}幀造成{}傷害）",
                charges->count,
                duration,
                periodicRule->intervalFrames,
                damage)
        : std::format(
            "施加可觸發{}次的中毒，持續{}幀；每{}幀造成目標{}的中毒傷害並消耗1次",
            charges->count,
            duration,
            periodicRule->intervalFrames,
            damage);

    if (application.reapplication == StatusReapplicationPolicy::KeepHigherDamage)
    {
        result += application.poisonSameEventMerge
                == PoisonSameEventMerge::SumDamagePercent
            ? compact
                ? "；同效果同事件相容毒傷合計後取較高"
                : "；同一效果擁有者在同一事件對同一目標施加相容中毒時，先合計傷害百分比，再與現有中毒保留較高傷害"
            : compact ? "；再施加取較高" : "；再次施加時保留較高傷害";
    }
    else if (application.reapplication
        == StatusReapplicationPolicy::ReplaceExistingPoison)
    {
        result += compact
            ? "；再施加取代現有中毒"
            : "；再次施加時取代現有中毒";
    }
    return result;
}

std::optional<std::string> renderPlayerCardShadowlessBehavior(
    const StatusBehaviorDefinition& behavior)
{
    if (behavior.rules.size() != 1) return std::nullopt;
    const auto& rule = behavior.rules.front();
    const EffectSelector expectedSelector{
        .kind = EffectSelectorKind::NearestEnemies,
        .count = 3,
    };
    const auto* condition = rule.conditions.size() == 1
        ? std::get_if<IsRootAttackCondition>(&rule.conditions.front())
        : nullptr;
    const auto* attack = rule.actions.size() == 1
        ? std::get_if<ModifyAttackAction>(&rule.actions.front().value)
        : nullptr;
    const AttackPattern expectedPattern{
        .kind = AttackPatternKind::EchoNearestOthers,
        .projectileCount = 2,
    };
    if (rule.event != EffectEvent::AttackSpawned
        || rule.observation != EffectObservationScope::StatusHolderEventSource
        || rule.castMatch != EffectCastMatch::BoundMagic
        || rule.selector != expectedSelector
        || !condition
        || !attack
        || attack->pattern != expectedPattern
        || attack->strengthPct != 50
        || attack->through
        || attack->tracking
        || attack->mainProjectile
        || attack->sameTargetHitLimit != 0
        || attack->projectileClearRadiusPct != 0
        || attack->targets != AttackTargetPolicy::SelectedTargets
        || attack->propagation != CastPropagationPolicy::NoEffectRules
        || !attack->addToBaseAttack
        || attack->source
        || attack->damageOverride
        || attack->damageKind
        || !std::holds_alternative<std::monostate>(attack->runtimeBehavior)
        || rule.chancePct != 100
        || rule.maxActivations != 0
        || rule.sharedCooldownFrames != 0
        || rule.intervalFrames != 0
        || rule.everyNthEvent != 0
        || rule.activationLimit
        || rule.repetitionCount)
        return std::nullopt;
    return "原始攻擊對最近3名敵人追加殘影×2，每道50%傷害；非主彈，不觸發效果";
}

std::string renderStatusBehaviorDefinition(
    const StatusBehaviorDefinition& behavior,
    EffectDescriptionStyle style,
    const EffectDescriptionPresentationContext& context)
{
    const bool compact = style == EffectDescriptionStyle::Compact;

    if (compact
        && context.compactPolicy == EffectDescriptionCompactPolicy::PlayerCard)
    {
        if (const auto shadowless = renderPlayerCardShadowlessBehavior(behavior))
            return *shadowless;
    }

    std::string result;
    for (const auto& rule : behavior.rules)
    {
        std::string prefix;
        if (rule.event == EffectEvent::StatusPersistent)
        {
            prefix = compact ? "持續時" : "狀態期間";
        }
        else if (rule.event == EffectEvent::FrameAdvanced && rule.intervalFrames > 0)
        {
            prefix = std::format("每{}幀", rule.intervalFrames);
        }
        else if (rule.event == EffectEvent::HitBeforeDamage)
        {
            switch (rule.observation)
            {
            case EffectObservationScope::StatusHolderEventSource:
                prefix = compact ? "持有者命中" : "狀態持有者命中時";
                break;
            case EffectObservationScope::StatusHolderEventTarget:
                prefix = compact ? "持有者受擊" : "狀態持有者受到攻擊時";
                break;
            case EffectObservationScope::StatusSourceEventSource:
                prefix = compact ? "來源命中" : "狀態來源命中時";
                break;
            case EffectObservationScope::SourceOwnerTeamEventSource:
                prefix = compact
                    ? "來源方友軍命中"
                    : "來源效果擁有者的任一友軍命中時";
                break;
            default:
                prefix = ruleEventLabel(rule.event, style);
                break;
            }
        }
        else if (rule.event == EffectEvent::UnitDied
            && rule.observation == EffectObservationScope::StatusHolderEventTarget)
        {
            prefix = compact ? "持有者死亡" : "狀態持有者死亡時";
        }
        else
        {
            prefix = ruleEventLabel(rule.event, style);
        }

        if (!rule.conditions.empty())
        {
            prefix += compact ? "且" : "，且";
            for (std::size_t index = 0; index < rule.conditions.size(); ++index)
            {
                if (index > 0) prefix += "且";
                prefix += conditionLabel(rule.conditions[index], compact, rule.event);
            }
        }

        std::string actions;
        for (const auto& action : rule.actions)
        {
            if (!actions.empty()) actions += compact ? "、" : "，並";
            actions += renderDescriptionActionArgument(action.value, style, {}, context);
        }
        if (actions.empty()) continue;

        if (!result.empty()) result += "；";
        result += prefix;
        if (rule.selector.kind != EffectSelectorKind::StatusHolder)
            result += std::format("，對{}", selectorLabel(rule.selector, compact));
        result += std::format("，{}", actions);
    }
    return result;
}

std::string_view detailedStatusObservationLabel(EffectObservationScope observation)
{
    switch (observation)
    {
    case EffectObservationScope::Owner: return "效果擁有者";
    case EffectObservationScope::OwnerTeamEventSource: return "效果擁有者同隊的事件來源";
    case EffectObservationScope::EventTarget: return "事件目標";
    case EffectObservationScope::StatusHolderEventSource: return "狀態持有者作為事件來源";
    case EffectObservationScope::StatusHolderEventTarget: return "狀態持有者作為事件目標";
    case EffectObservationScope::StatusSourceEventSource: return "狀態來源單位作為事件來源";
    case EffectObservationScope::SourceOwnerTeamEventSource: return "來源效果擁有者同隊的事件來源";
    }
    std::unreachable();
}

std::string renderDescriptionActionArgument(
    const EffectActionValue& action,
    EffectDescriptionStyle style,
    std::span<const DescriptionQualifier> suppressed,
    const EffectDescriptionPresentationContext& context)
{
    const bool detailed = style == EffectDescriptionStyle::Detailed;
    const bool compact = style == EffectDescriptionStyle::Compact;
    const std::string_view qualifierSeparator = compact || detailed ? "，" : "，";
    return std::visit(
        [style, detailed, compact, qualifierSeparator, suppressed, context](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, ModifyAttributeAction>)
            {
                const auto attribute = compact
                    && context.compactPolicy
                        == EffectDescriptionCompactPolicy::PlayerCard
                    && typed.attribute == BattleAttribute::DodgeChance
                    ? std::string("閃避率")
                    : attributeLabel(typed.attribute, compact);
                const auto amount = descriptionNumberLabel(typed.amount, style);
                const auto attributeUnit = battleAttributeUsesPercentagePoints(typed.attribute)
                    ? "%"
                    : "";
                const auto constantAmount = effectiveConstantEffectNumberValue(typed.amount);
                const bool hasBounds = typed.amount.minimum || typed.amount.maximum;
                const bool useConstantPhrase = constantAmount && (!detailed || !hasBounds);
                std::string result;
                switch (typed.operation)
                {
                case AttributeOperation::FlatAdd:
                    result = useConstantPhrase
                        ? std::format(
                            "{}{:+}{}",
                            attribute,
                            *constantAmount,
                            attributeUnit)
                        : std::format("{}增加{}{}", attribute, amount, attributeUnit);
                    break;
                case AttributeOperation::PercentAdd:
                    if (!detailed
                        && typed.attribute == BattleAttribute::DamageReduction
                        && constantAmount)
                    {
                        result = std::format("減傷{:+}%", *constantAmount);
                    }
                    else if (useConstantPhrase)
                        result = std::format("{}{:+}%", attribute, *constantAmount);
                    else if (constantAmount)
                        result = std::format("{}增加{}%", attribute, amount);
                    else
                        result = std::format("{}增加{}", attribute, amount);
                    break;
                case AttributeOperation::PercentagePointAdd:
                    result = useConstantPhrase
                        ? std::format("{}{:+}%", attribute, *constantAmount)
                        : std::format("{}增加{}%", attribute, amount);
                    break;
                case AttributeOperation::Override:
                    result = typed.attribute == BattleAttribute::GuaranteedHit
                        && constantAmount == 1 && !detailed
                        ? "獲得必中（無視閃避與格擋，仍受無敵與護盾限制）"
                        : std::format("{}改為{}{}", attribute, amount, attributeUnit);
                    break;
                case AttributeOperation::Multiply:
                    result = typed.amount.base == EffectNumberBase::Constant
                        ? std::format("{}乘以{}%", attribute, amount)
                        : std::format("{}乘以{}", attribute, amount);
                    break;
                case AttributeOperation::AtLeast:
                    result = std::format("{}至少為{}{}", attribute, amount, attributeUnit);
                    break;
                }
                appendTimedStackQualifiers(
                    result,
                    typed.durationFrames,
                    typed.stack,
                    typed.stackLimit,
                    typed.stackScope,
                    style,
                    false,
                    suppressed);
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                const auto constantAmount = effectiveConstantEffectNumberValue(typed.amount);
                if (!detailed && usesStackingOutgoingSkillDamagePhrase(typed))
                {
                    if (compact)
                    {
                        return std::format(
                            "增傷{}%×{}層，{}幀",
                            *constantAmount,
                            *typed.stackLimit,
                            typed.durationFrames);
                    }
                    return std::format(
                        "使招式傷害{:+}%，最多{}層；最後一次命中{}幀後清除層數",
                        *constantAmount,
                        *typed.stackLimit,
                        typed.durationFrames);
                }

                const auto percentAmount = [&]
                {
                    auto amount = descriptionNumberLabel(typed.amount, style);
                    if (typed.amount.base == EffectNumberBase::Constant) amount += "%";
                    return amount;
                };
                std::string result;
                const bool reviewedIncoming = !detailed
                    && typed.perspective == DamageModifierPerspective::Incoming
                    && typed.channel == DamageChannel::All
                    && typed.stage == DamageModifierStage::BeforeDefense
                    && typed.operation == DamageModifierOperation::PercentAdd
                    && constantAmount;
                const bool reviewedOutgoing = !detailed
                    && typed.perspective == DamageModifierPerspective::Outgoing
                    && typed.channel == DamageChannel::Skill
                    && typed.stage == DamageModifierStage::AfterDefense
                    && typed.operation == DamageModifierOperation::PercentAdd
                    && constantAmount;
                if (reviewedIncoming)
                {
                    result = *constantAmount < 0
                        ? std::format(
                            "減傷{}%",
                            -static_cast<std::int64_t>(*constantAmount))
                        : std::format("受傷{:+}%", *constantAmount);
                }
                else if (reviewedOutgoing)
                {
                    result = *constantAmount > 0
                        ? std::format("增傷{}%", *constantAmount)
                        : std::format("造成傷害{:+}%", *constantAmount);
                }
                else
                {
                    const auto perspective = typed.perspective == DamageModifierPerspective::Outgoing
                        ? compact ? "造成" : "造成的"
                        : compact ? "承受" : "承受的";
                    const auto damageContext = compact
                        ? std::format("{}{}", perspective, damageChannelLabel(typed.channel))
                        : std::format(
                            "{}{}（{}）",
                            perspective,
                            damageChannelLabel(typed.channel),
                            damageStageLabel(typed.stage));
                    if (typed.operation == DamageModifierOperation::IgnoreDefensePercent)
                        result = std::format("{}忽略{}防禦", damageContext, percentAmount());
                    else if (typed.operation == DamageModifierOperation::CapSingleHitAtMaxHpPercent)
                        result = compact
                            ? std::format("每次承傷≤最大生命{}", percentAmount())
                            : std::format("每次承傷不超過最大生命的{}", percentAmount());
                    else if (typed.operation == DamageModifierOperation::CapSingleHitAtValue)
                        result = compact
                            ? std::format("下次承傷≤{}", descriptionNumberLabel(typed.amount, style))
                            : std::format("下次承傷不超過{}", descriptionNumberLabel(typed.amount, style));
                    else if (typed.operation == DamageModifierOperation::ExecuteBelowMaxHpPercent)
                        result = std::format("{}普通傷害後生命低於{}最大生命時處決", damageContext, percentAmount());
                    else
                    {
                        auto amount = descriptionNumberLabel(typed.amount, style);
                        if ((typed.operation == DamageModifierOperation::PercentAdd
                                || typed.operation == DamageModifierOperation::Multiply)
                            && typed.amount.base == EffectNumberBase::Constant)
                        {
                            amount += "%";
                        }
                        if (typed.operation == DamageModifierOperation::FlatAdd)
                        {
                            result = constantAmount
                                ? std::format("{}固定加算{:+}點", damageContext, *constantAmount)
                                : std::format("{}固定加算{}點", damageContext, amount);
                        }
                        else if (compact
                            && typed.operation == DamageModifierOperation::PercentAdd
                            && constantAmount)
                        {
                            result = std::format("{}{:+}%", damageContext, *constantAmount);
                        }
                        else
                        {
                            const auto operation = typed.operation == DamageModifierOperation::PercentAdd
                                ? compact ? "加" : "百分比加算"
                                : compact ? "×" : "乘以";
                            result = std::format("{}{}{}", damageContext, operation, amount);
                        }
                    }
                    if (compact)
                    {
                        result += typed.stage == DamageModifierStage::BeforeDefense
                            ? "，防前"
                            : typed.stage == DamageModifierStage::AfterDefense
                            ? "，防後"
                            : "，最終";
                    }
                }
                appendTimedStackQualifiers(
                    result,
                    typed.durationFrames,
                    typed.stack,
                    typed.stackLimit,
                    typed.stackScope,
                    style,
                    false,
                    suppressed);
                return result;
            }
            else if constexpr (std::is_same_v<T, ChangeResourceAction>)
            {
                const auto resource = typed.resource == BattleResource::Hp ? "生命"
                    : typed.resource == BattleResource::Mp ? "內力"
                    : typed.resource == BattleResource::Shield ? "護盾"
                    : typed.resource == BattleResource::StatusShield ? "狀態護盾"
                    : typed.resource == BattleResource::StaggerShield ? "僵直護盾"
                    : typed.resource == BattleResource::ActiveCooldown ? "目前冷卻"
                    : typed.resource == BattleResource::ControlImmunityFrames ? "僵直吸收幀數"
                    : "無敵幀數";
                const auto verb = typed.kind == ResourceChangeKind::Restore ? "回復"
                    : typed.kind == ResourceChangeKind::Drain ? "奪取"
                    : typed.kind == ResourceChangeKind::Grant ? "獲得"
                    : typed.kind == ResourceChangeKind::Remove ? "移除"
                    : typed.kind == ResourceChangeKind::Transfer ? "轉移"
                    : "至少刷新至";
                const auto amountUsesBase = [&](EffectNumberBase base)
                {
                    return typed.amount.base == base
                        || typed.amount.multiplierBase == base;
                };
                const bool numberIncludesResource = (typed.resource == BattleResource::Hp
                    && (amountUsesBase(EffectNumberBase::SourceMaxHp)
                        || amountUsesBase(EffectNumberBase::TargetMaxHp)
                        || amountUsesBase(EffectNumberBase::TargetCurrentHp)
                        || amountUsesBase(EffectNumberBase::FinalHpDamage)))
                    || (typed.resource == BattleResource::Shield
                        && amountUsesBase(EffectNumberBase::TargetCurrentShield))
                    || (typed.resource == BattleResource::ActiveCooldown
                        && amountUsesBase(EffectNumberBase::TargetCurrentCooldown));
                auto result = std::format(
                    "{}{}{}",
                    verb,
                    descriptionNumberLabel(typed.amount, style),
                    numberIncludesResource ? "" : resource);
                if (compact
                    && context.compactPolicy
                        == EffectDescriptionCompactPolicy::PlayerCard
                    && !numberIncludesResource
                    && (typed.kind == ResourceChangeKind::Restore
                        || typed.kind == ResourceChangeKind::Grant)
                    && effectiveConstantEffectNumberValue(typed.amount))
                {
                    result = std::format(
                        "{}+{}",
                        resource,
                        *effectiveConstantEffectNumberValue(typed.amount));
                }
                if (typed.kind == ResourceChangeKind::Transfer && typed.transferDestination)
                    result += std::format("至{}", selectorLabel(*typed.transferDestination, compact));
                if (typed.resource == BattleResource::Hp
                    && typed.kind == ResourceChangeKind::Restore)
                {
                    result += std::format("{}{}", qualifierSeparator, healKindLabel(typed.healKind));
                    if (typed.healRequiresFullMp)
                        result += std::format("{}僅結算時滿內力才回復", qualifierSeparator);
                    if (typed.healSourcePolicy == EffectHealSourcePolicy::AllowDead)
                        result += std::format("{}來源死亡仍可生效", qualifierSeparator);
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                auto result = typed.operation == HealModifierOperation::Block
                    ? "無法受到治療"
                    : std::format("受到的治療改為{}%", typed.percent);
                result += std::format("{}限{}", qualifierSeparator, joinedDescriptionLabels(typed.kinds, compact));
                return result;
            }
            else if constexpr (std::is_same_v<T, ApplyStatusAction>)
            {
                if (const auto poison = renderCanonicalPoisonApplication(
                        typed, style, context))
                {
                    return *poison;
                }
                const auto status = battleStatusLabel(typed.status);
                const auto quantityModel = statusCatalogEntry(typed.status).quantity;
                const auto quantityCounter = statusQuantityCounter(quantityModel);
                std::string result;
                std::visit([&](const auto& quantity)
                {
                    using Q = std::decay_t<decltype(quantity)>;
                    if constexpr (std::is_same_v<Q, NoStatusQuantity>)
                        result = std::format("施加{}", status);
                    else if constexpr (std::is_same_v<Q, AddStatusLayers>)
                        result = compact
                            ? std::format("{}+{}{}", status, quantity.count, quantityCounter)
                            : std::format("獲得{}{}{}", quantity.count, quantityCounter, status);
                    else if constexpr (std::is_same_v<Q, AddSharedStatusLayers>)
                        result = std::format("施加{}層{}", quantity.count, status);
                    else if constexpr (std::is_same_v<Q, SetStatusMarks>)
                        result = std::format("將{}{}設為{}{}", status,
                            statusQuantityObjectSuffix(quantityModel),
                            quantity.count, quantityCounter);
                    else if constexpr (std::is_same_v<Q, AddDamageBlockCharges>)
                        result = compact
                            ? std::format("{}+{}{}", status, quantity.count, quantityCounter)
                            : std::format("增加{}{}{}", quantity.count, quantityCounter, status);
                    else if constexpr (std::is_same_v<Q, SetDamageBlockCharges>)
                        result = std::format("將{}設為{}{}{}", status,
                            quantity.count, quantityCounter,
                            statusQuantityObjectSuffix(quantityModel));
                    else if constexpr (std::is_same_v<Q, SetStatusTriggerCharges>)
                        result = compact
                            ? std::format("{}（{}{}）", status, quantity.count, quantityCounter)
                            : std::format("施加可觸發{}{}的{}",
                                quantity.count, quantityCounter, status);
                }, typed.quantity);

                const auto durationSuppressed = typed.durationFrames > 0
                    && descriptionQualifierIsSuppressed(
                        suppressed,
                        DescriptionDurationFramesQualifier{ typed.durationFrames });
                if (typed.duration)
                    result += std::format("{}持續{}幀", qualifierSeparator,
                        descriptionNumberLabel(*typed.duration, style));
                else if (typed.durationFrames > 0 && !durationSuppressed)
                    result += compact
                        ? std::format("，{}幀", typed.durationFrames)
                        : std::format("，持續{}幀", typed.durationFrames);

                const auto effectText = typed.behavior
                    ? renderStatusBehaviorDefinition(*typed.behavior, style, context)
                    : std::string{};
                const bool poisonSumsSameEventDamage = typed.poisonSameEventMerge
                    == PoisonSameEventMerge::SumDamagePercent;

                const auto layerLimit = std::visit([](const auto& quantity) -> std::optional<int>
                {
                    using Q = std::decay_t<decltype(quantity)>;
                    if constexpr (std::is_same_v<Q, AddStatusLayers>
                        || std::is_same_v<Q, AddDamageBlockCharges>)
                        return quantity.limit;
                    return std::nullopt;
                }, typed.quantity);
                const auto targetTotalLimit = std::visit([](const auto& quantity) -> std::optional<int>
                {
                    using Q = std::decay_t<decltype(quantity)>;
                    if constexpr (std::is_same_v<Q, AddSharedStatusLayers>)
                        return quantity.targetTotalLimit;
                    return std::nullopt;
                }, typed.quantity);
                const bool damageBlockLimit = std::holds_alternative<AddDamageBlockCharges>(
                    typed.quantity);
                if (compact && (!effectText.empty() || layerLimit || targetTotalLimit))
                {
                    result += "（";
                    if (!effectText.empty()) result += effectText;
                    if (layerLimit)
                    {
                        if (!effectText.empty()) result += "，";
                        result += damageBlockLimit
                            ? std::format("此來源最多{}次抵擋", *layerLimit)
                            : std::format("此來源最多{}層", *layerLimit);
                    }
                    if (targetTotalLimit)
                    {
                        if (!effectText.empty() || layerLimit) result += "，";
                        result += std::format("目標總上限{}層", *targetTotalLimit);
                    }
                    result += "）";
                }
                else
                {
                    if (layerLimit)
                    {
                        result += damageBlockLimit
                            ? std::format("；此效果提供的{}最多{}次抵擋",
                                status, *layerLimit)
                            : std::format("；此效果提供的{}最多{}層",
                                status, *layerLimit);
                    }
                    if (targetTotalLimit)
                        result += std::format("；目標共享上限{}層", *targetTotalLimit);
                    if (!effectText.empty()) result += std::format("；{}", effectText);
                }

                const auto appendReapplication = [&](std::string_view text)
                {
                    result += compact
                        ? std::format("；再施加時{}", text)
                        : std::format("；再次施加會{}", text);
                };
                switch (typed.reapplication)
                {
                case StatusReapplicationPolicy::Implicit:
                    if (std::holds_alternative<SetStatusMarks>(typed.quantity))
                        appendReapplication("重設印記與持續時間");
                    break;
                case StatusReapplicationPolicy::ExtendDuration:
                    appendReapplication(statusReapplicationPolicyLabel(typed.reapplication));
                    break;
                case StatusReapplicationPolicy::KeepLongerDuration:
                    if (compact)
                        result += "（再施加取較長）";
                    else
                        appendReapplication(statusReapplicationPolicyLabel(typed.reapplication));
                    break;
                case StatusReapplicationPolicy::RefreshDuration:
                    appendReapplication(statusReapplicationPolicyLabel(typed.reapplication));
                    break;
                case StatusReapplicationPolicy::KeepHigherDamage:
                    appendReapplication(statusReapplicationPolicyLabel(typed.reapplication));
                    break;
                case StatusReapplicationPolicy::ReplaceExistingPoison:
                    appendReapplication(statusReapplicationPolicyLabel(typed.reapplication));
                    break;
                case StatusReapplicationPolicy::Count:
                    std::unreachable();
                }
                if (poisonSumsSameEventDamage)
                {
                    result += compact
                        ? "；同源同事件先合計相容毒傷%"
                        : "；同一效果擁有者在同一事件對同一目標施加相容中毒時，先合計傷害百分比再比較較高傷害";
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
            {
                const auto quantityModel = statusCatalogEntry(typed.status).quantity;
                const auto quantityCounter = statusQuantityCounter(quantityModel);
                assert(!quantityCounter.empty());
                auto result = std::format("消耗{}{}{}{}",
                    typed.quantity,
                    quantityCounter,
                    battleStatusLabel(typed.status),
                    statusQuantityObjectSuffix(quantityModel));
                if (typed.source != StatusSourceMatch::Any)
                    result += std::format("{}{}", qualifierSeparator,
                        statusSourceMatchLabel(typed.source));
                assert(!typed.whenDepleted
                    && "depleted status branches must be rendered through DescriptionConditional");
                return result;
            }
            else if constexpr (std::is_same_v<T, ConsumeThisStatusAction>)
            {
                auto result = std::format("消耗此狀態{}份數量", typed.quantity);
                if (typed.whenDepleted)
                {
                    const auto& depleted = *typed.whenDepleted;
                    result += std::format("；耗盡時施加{}", battleStatusLabel(depleted.status));
                    if (depleted.duration)
                        result += std::format("{}持續{}幀", qualifierSeparator,
                            descriptionNumberLabel(*depleted.duration, style));
                    else if (depleted.durationFrames > 0)
                        result += std::format("{}持續{}幀", qualifierSeparator,
                            depleted.durationFrames);
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, RemoveStatusAction>)
            {
                std::string result;
                if (typed.controlOnly) result = "清除全部控制狀態";
                else if (typed.negativeOnly) result = typed.count == 0
                    ? "清除全部負面效果"
                    : std::format("清除{}個負面效果", typed.count);
                else if (!typed.statuses.empty())
                {
                    result = "移除";
                    for (const auto status : typed.statuses)
                    {
                        if (!result.ends_with("移除")) result += "、";
                        result += battleStatusLabel(status);
                    }
                    if (typed.count > 0) result += std::format("中的{}個", typed.count);
                }
                if (typed.count > 0)
                {
                    result += typed.order == StatusRemovalOrder::LongestRemaining
                        ? std::format("{}優先最長剩餘", qualifierSeparator)
                        : typed.order == StatusRemovalOrder::Oldest
                        ? std::format("{}優先最早套用", qualifierSeparator)
                        : std::format("{}優先最新套用", qualifierSeparator);
                }
                if (typed.clearCurrentActionStagger)
                {
                    if (!result.empty()) result += "並";
                    result += "解除目前動作僵直（保留位置與動作）";
                }
                if (typed.source != StatusSourceMatch::Any)
                    result += std::format("{}{}", qualifierSeparator,
                        statusSourceMatchLabel(typed.source));
                return result;
            }
            else if constexpr (std::is_same_v<T, SuppressCurrentCastContactsAction>)
            {
                auto result = std::string("使本次施放的目前及剩餘攻擊落空");
                if (typed.originalTargetShield)
                    result += std::format("{}原攻擊目標獲得{}護盾",
                        qualifierSeparator,
                        descriptionNumberLabel(*typed.originalTargetShield, style));
                return result;
            }
            else if constexpr (std::is_same_v<T, MakeIncomingAttackMissAction>)
                return "使本次受到的攻擊落空";
            else if constexpr (std::is_same_v<T, BlockPositiveDamageAction>)
                return "抵擋本次非處決正傷害";
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                const auto kind = std::format("{}傷害", damageKindLabel(typed.kind));
                const auto area = typed.area.kind == DamageAreaKind::Square
                    ? std::format("{}×{}範圍", typed.area.squareSideTiles, typed.area.squareSideTiles)
                    : typed.area.kind == DamageAreaKind::Circle
                    ? std::format("{}格範圍", typed.area.radiusTiles)
                    : std::string{};
                auto result = std::format("{}造成{}{}", area, descriptionNumberLabel(typed.amount, style), kind);
                if (typed.transactionCount)
                    result += std::format("{}獨立{}次", qualifierSeparator, descriptionNumberLabel(*typed.transactionCount, style));
                if (!typed.appliesDamageModifiers)
                    result += std::format("{}不套用傷害修正", qualifierSeparator);
                if (!typed.triggersHurtInvincibility)
                    result += std::format("{}不觸發受傷無敵", qualifierSeparator);
                if (typed.perCast.perTargetLimit > 0)
                    result += compact
                        ? std::format("{}每施放同目標最多{}次", qualifierSeparator, typed.perCast.perTargetLimit)
                        : std::format("{}每次施放對同一目標最多命中{}次", qualifierSeparator, typed.perCast.perTargetLimit);
                if (typed.areaProjectiles)
                {
                    const auto& delivery = *typed.areaProjectiles;
                    result += std::format(
                        "{}{}區域追蹤彈{}{}格{}最多{}目標",
                        qualifierSeparator,
                        delivery.visual == AreaProjectileVisual::DeathBlast
                            ? "死亡爆炸"
                            : "護盾爆炸",
                        qualifierSeparator,
                        delivery.rangeTiles,
                        qualifierSeparator,
                        delivery.maximumTargets);
                    if (delivery.trackEventSource)
                        result += std::format("{}{}", qualifierSeparator,
                            compact ? "含存活事件來源" : "必含存活事件來源");
                    if (delivery.stunFrames > 0)
                        result += std::format("{}眩暈{}幀", qualifierSeparator, delivery.stunFrames);
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
            {
                if (compact
                    && context.compactPolicy
                        == EffectDescriptionCompactPolicy::PlayerCard
                    && std::holds_alternative<std::monostate>(typed.runtimeBehavior))
                {
                    const auto pattern = [&]
                    {
                        switch (typed.pattern.kind)
                        {
                        case AttackPatternKind::Preserve: return std::string("原彈道");
                        case AttackPatternKind::Fan: return std::string("扇形攻擊");
                        case AttackPatternKind::Flanks: return std::string("側翼攻擊");
                        case AttackPatternKind::SamePointSequence: return std::string("同落點追加");
                        case AttackPatternKind::MultiTarget: return std::string("多目標攻擊");
                        case AttackPatternKind::EchoNearestOthers: return std::string("殘影");
                        }
                        std::unreachable();
                    }();
                    std::string result = std::format(
                        "{}×{}",
                        pattern,
                        typed.pattern.projectileCount);
                    std::vector<std::string> clauses;
                    clauses.push_back(std::format(
                        "每道{}%傷害",
                        typed.strengthPct));
                    clauses.push_back(typed.mainProjectile ? "主彈" : "非主彈");
                    if (typed.pattern.spreadDegrees > 0)
                        clauses.push_back(std::format(
                            "展開{}°",
                            typed.pattern.spreadDegrees));
                    if (typed.pattern.intervalFrames > 0)
                        clauses.push_back(std::format(
                            "間隔{}幀",
                            typed.pattern.intervalFrames));
                    if (typed.targets == AttackTargetPolicy::SelectedTargets)
                        clauses.push_back("選擇目標");
                    else if (typed.targets == AttackTargetPolicy::SamePoint
                        && typed.pattern.kind != AttackPatternKind::SamePointSequence)
                        clauses.push_back("同落點");
                    else if (typed.targets == AttackTargetPolicy::SameTarget)
                        clauses.push_back("同目標");
                    if (typed.through)
                        clauses.push_back(*typed.through ? "貫穿" : "不貫穿");
                    if (typed.tracking)
                        clauses.push_back(*typed.tracking ? "追蹤" : "不追蹤");
                    if (typed.sameTargetHitLimit > 0)
                        clauses.push_back(std::format(
                            "同目標最多{}次",
                            typed.sameTargetHitLimit));
                    if (typed.projectileClearRadiusPct > 0)
                        clauses.push_back(std::format(
                            "沿途清除敵方彈道（含絕招，半徑{}%）",
                            typed.projectileClearRadiusPct));
                    if (typed.propagation == CastPropagationPolicy::SourceHitRulesOnly)
                        clauses.push_back("僅來源命中");
                    else if (typed.propagation == CastPropagationPolicy::SuppressUltimateRules)
                        clauses.push_back("不觸發大招效果");
                    else if (typed.propagation == CastPropagationPolicy::BorrowedUltimateRules)
                        clauses.push_back("觸發借用大招效果");
                    else if (typed.propagation == CastPropagationPolicy::NoEffectRules)
                        clauses.push_back("不觸發效果");
                    if (typed.addToBaseAttack)
                        clauses.push_back("追加攻擊");
                    if (typed.source)
                        clauses.push_back("由" + selectorLabel(*typed.source, true) + "出手");
                    if (typed.damageOverride)
                    {
                        auto overrideText = "傷害改為"
                            + descriptionNumberLabel(*typed.damageOverride, style);
                        if (typed.damageKind)
                            overrideText += std::string(damageKindLabel(*typed.damageKind)) + "傷害";
                        else
                            overrideText += "傷害（沿用種類）";
                        clauses.push_back(std::move(overrideText));
                    }
                    else if (typed.damageKind)
                    {
                        clauses.push_back(
                            "傷害種類改為" + std::string(damageKindLabel(*typed.damageKind)));
                    }
                    for (std::size_t index = 0; index < clauses.size(); ++index)
                    {
                        result += index == 0 ? "（" : "；";
                        result += clauses[index];
                    }
                    result += "）";
                    return result;
                }

                const auto pattern = attackPatternLabel(typed.pattern, compact);
                auto result = std::format("{}{}×{}，{}%傷害",
                    pattern,
                    typed.mainProjectile ? "主彈" : "非主彈",
                    typed.pattern.projectileCount,
                    typed.strengthPct);
                if (!compact && !detailed)
                    result = std::format("{}{}×{}，{}%傷害",
                        pattern,
                        typed.mainProjectile ? "主彈" : "非主彈",
                        typed.pattern.projectileCount,
                        typed.strengthPct);
                if (typed.pattern.spreadDegrees > 0)
                    result += std::format("{}展開{}度", qualifierSeparator, typed.pattern.spreadDegrees);
                if (typed.through) result += std::format("{}{}", qualifierSeparator, *typed.through ? "貫穿" : "不貫穿");
                if (typed.tracking) result += std::format("{}{}", qualifierSeparator, *typed.tracking ? "追蹤" : "不追蹤");
                if (typed.sameTargetHitLimit > 0) result += std::format("{}同目標{}次", qualifierSeparator, typed.sameTargetHitLimit);
                if (typed.projectileClearRadiusPct > 0) result += std::format("{}沿途清除所有敵方彈道（含絕招，偵測半徑為命中半徑{}%）", qualifierSeparator, typed.projectileClearRadiusPct);
                if (typed.pattern.intervalFrames > 0) result += std::format("{}間隔{}幀", qualifierSeparator, typed.pattern.intervalFrames);
                if (typed.propagation == CastPropagationPolicy::SourceHitRulesOnly)
                    result += std::format("{}{}", qualifierSeparator,
                        compact ? "僅來源命中" : "僅觸發來源命中規則");
                else if (typed.propagation == CastPropagationPolicy::SuppressUltimateRules) result += std::format("{}不觸發大招效果", qualifierSeparator);
                else if (typed.propagation == CastPropagationPolicy::BorrowedUltimateRules)
                    result += std::format("{}{}", qualifierSeparator,
                        compact ? "觸發借用大招效果" : "觸發借用的大招規則");
                else if (typed.propagation == CastPropagationPolicy::NoEffectRules)
                    result += std::format("{}{}", qualifierSeparator,
                        compact ? "不觸發效果" : "不觸發任何效果規則");
                if (typed.addToBaseAttack)
                    result += std::format("{}{}", qualifierSeparator,
                        compact ? "追加攻擊" : "作為追加攻擊");
                if (typed.targets == AttackTargetPolicy::SelectedTargets) result += std::format("{}選擇目標", qualifierSeparator);
                else if (typed.targets == AttackTargetPolicy::SamePoint) result += std::format("{}同落點", qualifierSeparator);
                else if (typed.targets == AttackTargetPolicy::SameTarget) result += std::format("{}同目標", qualifierSeparator);
                if (typed.source) result += std::format("{}由{}出手", qualifierSeparator, selectorLabel(*typed.source, true));
                if (typed.damageOverride)
                {
                    result += std::format("{}{}", qualifierSeparator, descriptionNumberLabel(*typed.damageOverride, style));
                    if (typed.damageKind)
                        result += std::format("{}傷害", damageKindLabel(*typed.damageKind));
                    else
                        result += "傷害（沿用原傷害種類）";
                }
                else if (typed.damageKind)
                {
                    result += std::format("{}傷害種類改為{}", qualifierSeparator, damageKindLabel(*typed.damageKind));
                }
                std::visit(
                    [&](const auto& behavior)
                    {
                        using B = std::decay_t<decltype(behavior)>;
                        if constexpr (std::is_same_v<B, ProjectileBounceAttackBehavior>)
                        {
                            result += std::format(
                                "{}彈射追加命中{}次{}{}%{}{}像素",
                                qualifierSeparator,
                                behavior.additionalHits,
                                qualifierSeparator,
                                behavior.chancePct,
                                qualifierSeparator,
                                behavior.rangePixels);
                        }
                        else if constexpr (std::is_same_v<B, NearbyTrackingAttackBehavior>)
                        {
                            result += std::format(
                                "{}{}像素內產生{}%傷害追蹤彈",
                                qualifierSeparator,
                                behavior.rangePixels,
                                behavior.damagePct);
                        }
                        else if constexpr (std::is_same_v<B, DelayedAlternateAttackBehavior>)
                        {
                            result += std::format(
                                "{}延遲{}幀，以{}%傷害追擊最近的其他敵人"
                                "（附近無其他敵人則追擊原攻擊目標）{}{}%機率獲得1次傷害抵擋（最多1次）",
                                qualifierSeparator,
                                behavior.delayFrames,
                                behavior.damagePct,
                                qualifierSeparator,
                                behavior.attackerBlockGainChancePct);
                        }
                        else if constexpr (std::is_same_v<B, ExpandingSpiralAttackBehavior>)
                        {
                            result += std::format(
                                "{}擴張螺旋彈×{}{}流血{}層",
                                qualifierSeparator,
                                behavior.projectileCount,
                                qualifierSeparator,
                                behavior.bleedStacks);
                        }
                    },
                    typed.runtimeBehavior);
                return result;
            }
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
            {
                const auto direction = typed.direction == ForceMoveDirection::AwayFromSource
                    ? "擊退"
                    : typed.direction == ForceMoveDirection::TowardSource
                    ? "拉近"
                    : "移向指定點";
                auto result = typed.distancePixels > 0
                    ? compact
                        ? std::format("{}{}像素，鎖{}幀", direction, typed.distancePixels, typed.lockFrames)
                        : std::format("{}{}像素並鎖定{}幀", direction, typed.distancePixels, typed.lockFrames)
                    : compact
                        ? std::format("{}{}格，鎖{}幀", direction, typed.distanceTiles, typed.lockFrames)
                        : std::format("{}{}格並鎖定{}幀", direction, typed.distanceTiles, typed.lockFrames);
                result += std::format("{}{}", qualifierSeparator,
                    typed.collision == ForceMoveCollision::StopBeforeOccupied
                        ? compact ? "遇佔位停止" : "在佔位前停止"
                        : compact ? "遇障停止" : "在阻擋地形前停止");
                result += std::format("{}{}", qualifierSeparator,
                    typed.blocked == ForceMoveBlockedResult::Shorten
                        ? compact ? "受阻縮短" : "受阻時縮短位移"
                        : compact ? "受阻取消" : "受阻時取消位移");
                return result;
            }
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
            {
                if (!detailed)
                {
                    const auto relationLabel = [compact](EffectTeamFilter relation)
                    {
                        if (compact)
                            return relation == EffectTeamFilter::Ally ? std::string("友")
                                : relation == EffectTeamFilter::Enemy ? std::string("敵")
                                : std::string("全體");
                        return relation == EffectTeamFilter::Ally ? std::string("友方")
                            : relation == EffectTeamFilter::Enemy ? std::string("敵人")
                            : std::string("所有單位");
                    };
                    std::string result;
                    if (compact)
                    {
                        result = typed.shape == AreaShape::Circle
                            ? std::format("區域{}格，{}幀", typed.radiusTiles, typed.durationFrames)
                            : std::format("區域{}×{}，{}幀", typed.squareSideTiles, typed.squareSideTiles, typed.durationFrames);
                    }
                    else
                    {
                        const auto location = typed.anchor == AreaAnchor::HitPosition
                            ? "在命中位置"
                            : "以來源單位為中心";
                        result = typed.shape == AreaShape::Circle
                            ? std::format("{}建立半徑{}格的區域，持續{}幀", location, typed.radiusTiles, typed.durationFrames)
                            : std::format("{}建立{}×{}格的區域，持續{}幀", location, typed.squareSideTiles, typed.squareSideTiles, typed.durationFrames);
                    }

                    if (typed.sourceDeath == AreaSourceDeathPolicy::RemoveImmediately)
                        result += compact ? "，來源死即消" : "，來源死亡時立即移除";
                    if (typed.merge == AreaMergePolicy::Independent)
                        result += compact ? "，區域獨立" : "，各區域獨立";
                    else if (typed.merge == AreaMergePolicy::ReplaceSameSource)
                        result += compact ? "，同源取代" : "，同來源取代舊區域";

                    struct PlayerAreaModifierLabel
                    {
                        EffectTeamFilter relation{};
                        std::string text{};
                    };
                    struct PlayerAreaAttackPercentLabel
                    {
                        EffectTeamFilter relation{};
                        std::string text{};
                        int amount{};
                    };
                    std::vector<PlayerAreaModifierLabel> modifierLabels;
                    std::vector<PlayerAreaAttackPercentLabel> attackPercentLabels;
                    std::vector<PlayerAreaModifierLabel> trackingLabels;
                    for (const auto& modifier : typed.modifiers)
                    {
                        if (modifier.kind == AreaModifierKind::Attribute)
                        {
                            const auto constantAmount = effectiveConstantEffectNumberValue(modifier.amount);
                            const bool useConstantPhrase = constantAmount
                                && (!detailed || (!modifier.amount.minimum && !modifier.amount.maximum));
                            const auto amount = useConstantPhrase
                                ? std::format("{:+}%", *constantAmount)
                                : std::format("增加{}%", descriptionNumberLabel(modifier.amount, style));
                            modifierLabels.push_back({
                                modifier.relation,
                                std::format(
                                    "{}{}",
                                    compact && modifier.attribute == BattleAttribute::Speed
                                        ? "速"
                                        : attributeLabel(modifier.attribute, compact),
                                    amount),
                            });
                        }
                        else if (modifier.kind == AreaModifierKind::PeriodicDamage)
                        {
                            modifierLabels.push_back({modifier.relation,
                                std::format("每{}幀受到{}效果傷害", modifier.intervalFrames,
                                    descriptionNumberLabel(modifier.amount, style))});
                        }
                        else if (modifier.kind == AreaModifierKind::DamageRedirect)
                        {
                            modifierLabels.push_back({modifier.relation,
                                std::format("傷害由來源承擔，轉移減傷{}%（不含來源自身，不連鎖）", modifier.percent)});
                        }
                        else if (modifier.kind == AreaModifierKind::OutgoingDamage)
                        {
                            modifierLabels.push_back({
                                modifier.relation,
                                std::format(
                                    "造成的{}{:+}%",
                                    compact && modifier.damageChannel == DamageChannel::All
                                        ? "全傷"
                                        : damageChannelLabel(modifier.damageChannel),
                                    modifier.percent),
                            });
                        }
                        else if (modifier.kind == AreaModifierKind::AttackSpawn)
                        {
                            if (modifier.tracking)
                            {
                                trackingLabels.push_back({
                                    modifier.relation,
                                    *modifier.tracking
                                        ? "彈道可追蹤"
                                        : compact ? "禁追蹤" : "彈道無法追蹤",
                                });
                            }
                            if (modifier.speedPct)
                                attackPercentLabels.push_back({
                                    modifier.relation,
                                    "彈速",
                                    *modifier.speedPct,
                                });
                            if (modifier.projectilePressurePct)
                                attackPercentLabels.push_back({
                                    modifier.relation,
                                    compact ? "壓制" : "壓制傷害",
                                    *modifier.projectilePressurePct,
                                });
                        }
                        else
                        {
                            const auto direction = modifier.blockedDirection == ForceMoveDirection::AwayFromSource
                                ? "擊退"
                                : modifier.blockedDirection == ForceMoveDirection::TowardSource
                                ? "拉近"
                                : "指定點位移";
                            modifierLabels.push_back({
                                modifier.relation,
                                std::format("免疫{}", direction),
                            });
                        }
                    }
                    if (!compact)
                        modifierLabels.insert(
                            modifierLabels.end(),
                            trackingLabels.begin(),
                            trackingLabels.end());
                    while (!attackPercentLabels.empty())
                    {
                        const auto relation = attackPercentLabels.front().relation;
                        const int amount = attackPercentLabels.front().amount;
                        std::string labels;
                        for (auto it = attackPercentLabels.begin(); it != attackPercentLabels.end();)
                        {
                            if (it->relation != relation || it->amount != amount)
                            {
                                ++it;
                                continue;
                            }
                            if (!labels.empty()) labels += compact ? "；" : "及";
                            labels += it->text;
                            it = attackPercentLabels.erase(it);
                        }
                        modifierLabels.push_back({
                            relation,
                            std::format("{}{:+}%", labels, amount),
                        });
                    }
                    if (compact)
                        modifierLabels.insert(
                            modifierLabels.end(),
                            trackingLabels.begin(),
                            trackingLabels.end());
                    const bool commonRelation = std::ranges::all_of(
                        typed.modifiers,
                        [&](const AreaModifier& modifier)
                        {
                            return modifier.relation == typed.modifiers.front().relation;
                        });
                    bool firstModifier = true;
                    for (const auto& label : modifierLabels)
                    {
                        result += compact ? "，" : firstModifier ? "；" : "，";
                        if (!commonRelation || firstModifier)
                        {
                            if (!compact) result += "區域內";
                            result += relationLabel(label.relation);
                        }
                        result += label.text;
                        firstModifier = false;
                    }
                    return result;
                }

                auto result = typed.shape == AreaShape::Circle
                    ? std::format("建立半徑{}格、持續{}幀的區域", typed.radiusTiles, typed.durationFrames)
                    : std::format("建立{}×{}、持續{}幀的區域", typed.squareSideTiles, typed.squareSideTiles, typed.durationFrames);
                result += typed.anchor == AreaAnchor::HitPosition ? "，固定於命中位置" : "，跟隨來源單位";
                result += typed.sourceDeath == AreaSourceDeathPolicy::PersistUntilExpiry
                    ? "，來源死亡後持續至到期"
                    : "，來源死亡時立即移除";
                result += typed.merge == AreaMergePolicy::Independent
                    ? "，各區域獨立"
                    : typed.merge == AreaMergePolicy::RefreshSameSource
                    ? "，同來源刷新時間"
                    : "，同來源取代舊區域";
                const auto relationLabel = [](EffectTeamFilter relation)
                {
                    return relation == EffectTeamFilter::Ally ? "友方"
                        : relation == EffectTeamFilter::Enemy ? "敵方"
                        : "所有單位";
                };
                const auto overlapLabel = [](AreaOverlapPolicy overlap)
                {
                    return overlap == AreaOverlapPolicy::Add ? "相加"
                        : overlap == AreaOverlapPolicy::KeepStrongest ? "取最強"
                        : "任一成立";
                };
                for (const auto& modifier : typed.modifiers)
                {
                    result += compact ? "，" : "；";
                    if (modifier.kind == AreaModifierKind::Attribute)
                    {
                        const auto constantAmount = effectiveConstantEffectNumberValue(modifier.amount);
                        const bool useConstantPhrase = constantAmount
                            && (!detailed || (!modifier.amount.minimum && !modifier.amount.maximum));
                        const auto amount = useConstantPhrase
                            ? std::format("{:+}", *constantAmount)
                            : std::format("增加{}", descriptionNumberLabel(modifier.amount, style));
                        result += std::format("區域內{}{}{}",
                            relationLabel(modifier.relation),
                            attributeLabel(modifier.attribute, compact),
                            amount);
                    }
                    else if (modifier.kind == AreaModifierKind::PeriodicDamage)
                    {
                        result += std::format("區域內敵方每{}幀受到{}效果傷害", modifier.intervalFrames,
                            descriptionNumberLabel(modifier.amount, style));
                    }
                    else if (modifier.kind == AreaModifierKind::DamageRedirect)
                    {
                        result += std::format("區域內其他友方的傷害由來源承擔，轉移減傷{}%（不連鎖）", modifier.percent);
                    }
                    else if (modifier.kind == AreaModifierKind::OutgoingDamage)
                    {
                        result += std::format("區域內{}造成的{}{}%",
                            relationLabel(modifier.relation),
                            damageChannelLabel(modifier.damageChannel),
                            modifier.percent >= 0 ? std::format("+{}", modifier.percent) : std::to_string(modifier.percent));
                    }
                    else if (modifier.kind == AreaModifierKind::AttackSpawn)
                    {
                        result += std::format("區域內{}的新彈道", relationLabel(modifier.relation));
                        if (modifier.tracking) result += *modifier.tracking ? "，可追蹤" : "，不可追蹤";
                        if (modifier.speedPct) result += std::format("，彈速{}%", *modifier.speedPct);
                        if (modifier.projectilePressurePct) result += std::format("，彈道壓制傷害{}%", *modifier.projectilePressurePct);
                    }
                    else
                    {
                        const auto direction = modifier.blockedDirection == ForceMoveDirection::AwayFromSource
                            ? "擊退"
                            : modifier.blockedDirection == ForceMoveDirection::TowardSource
                            ? "拉近"
                            : "移向指定點";
                        result += std::format("區域內{}免疫{}", relationLabel(modifier.relation), direction);
                    }
                    result += std::format("，重疊{}", overlapLabel(modifier.overlap));
                    if (modifier.trackingOverlap)
                        result += std::format("，追蹤重疊{}", overlapLabel(*modifier.trackingOverlap));
                    if (modifier.speedOverlap)
                        result += std::format("，彈速重疊{}", overlapLabel(*modifier.speedOverlap));
                    if (modifier.projectilePressureOverlap)
                        result += std::format("，彈壓重疊{}", overlapLabel(*modifier.projectilePressureOverlap));
                }
                return result;
            }
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
            {
                std::string result;
                const auto append = [&](std::string fragment)
                {
                    if (!result.empty()) result += compact ? "，" : "，";
                    result += fragment;
                };
                if (typed.mpCost) append(std::format("實際消耗{}內力", descriptionNumberLabel(*typed.mpCost, style)));
                if (typed.rangeMode == CastRangeMode::Ranged) append("武功遠程化");
                else if (typed.rangeMode == CastRangeMode::Preserve) append("保留原射程模式");
                if (typed.projectileSpeedPct > 0)
                    append(std::format("彈道速度{}%", typed.projectileSpeedPct));
                if (typed.minimumSelectDistance > 0)
                    append(std::format("最小選擇距離{}", typed.minimumSelectDistance));
                if (typed.additionalProjectiles > 0)
                    append(std::format("追加彈道{}枚", typed.additionalProjectiles));
                if (typed.mobility == CastMobilityPolicy::DashAttack) append("啟用滑步攻擊");
                else if (typed.mobility == CastMobilityPolicy::BlinkAttack) append("啟用閃擊");
                if (typed.autoUltimate)
                {
                    append(std::format(
                        "自動施放絕招（{}內力，{}公告）",
                        typed.autoUltimate->consumeMp ? "消耗" : "不消耗",
                        typed.autoUltimate->announce ? "顯示" : "不顯示"));
                }
                if (typed.replacementPattern)
                {
                    const auto& pattern = *typed.replacementPattern;
                    auto replacement = std::format(
                        "替換攻擊樣式為{}×{}",
                        attackPatternLabel(pattern, compact),
                        pattern.projectileCount);
                    if (pattern.spreadDegrees > 0) replacement += std::format("{}展開{}度", qualifierSeparator, pattern.spreadDegrees);
                    if (pattern.intervalFrames > 0) replacement += std::format("{}間隔{}幀", qualifierSeparator, pattern.intervalFrames);
                    append(std::move(replacement));
                }
                if (typed.freeAdditionalCast) append("免費追加相同施放");
                if (typed.propagation == CastPropagationPolicy::SourceHitRulesOnly)
                    append(compact ? "僅來源命中" : "僅觸發來源命中規則");
                else if (typed.propagation == CastPropagationPolicy::SuppressUltimateRules)
                    append("不再次觸發大招效果");
                else if (typed.propagation == CastPropagationPolicy::BorrowedUltimateRules)
                    append(compact ? "觸發借用大招效果" : "觸發借用的大招規則");
                else if (typed.propagation == CastPropagationPolicy::NoEffectRules)
                    append(compact ? "不觸發效果" : "不觸發任何效果規則");
                return result;
            }
            else if constexpr (std::is_same_v<T, StateMachineAction>)
            {
                return std::visit(
                    [style, compact, qualifierSeparator](const auto& machine) -> std::string
                    {
                        using M = std::decay_t<decltype(machine)>;
                        if constexpr (std::is_same_v<M, ChangeStateValueAction>)
                        {
                            auto result = std::format("狀態值{:+}", machine.delta);
                            if (machine.minimum) result += std::format("{}下限{}", qualifierSeparator, *machine.minimum);
                            if (machine.maximum) result += std::format("{}上限{}", qualifierSeparator, *machine.maximum);
                            return result;
                        }
                        else if constexpr (std::is_same_v<M, TransferStateValueAction>)
                            return "轉移記錄值至另一狀態槽並清空來源";
                        else if constexpr (std::is_same_v<M, RecordMaximumDamageAction>)
                            return std::format("記錄最大單次{}生命傷害", damageChannelSourceLabel(machine.channel));
                        else if constexpr (std::is_same_v<M, ConsumeRecordedMaximumAction>)
                        {
                            const auto amount = machine.percent == 100 ? "等量" : std::format("{}%", machine.percent);
                            auto result = machine.clearAfterConsume
                                ? (machine.destination == StateValueDestination::DamageAmount
                                    ? std::format("消耗記錄值並附加{}純粹傷害", amount)
                                    : std::format("消耗記錄值並獲得{}護盾", amount))
                                : (machine.destination == StateValueDestination::DamageAmount
                                    ? std::format("讀取記錄值並附加{}純粹傷害", amount)
                                    : std::format("讀取記錄值並獲得{}護盾", amount));
                            return result;
                        }
                        else if constexpr (std::is_same_v<M, StartDamageAbsorptionAction>)
                        {
                            assert(false && "damage absorption must be rendered through DescriptionStateCycle");
                            return {};
                        }
                        else if constexpr (std::is_same_v<M, SettleDamageAbsorptionAction>)
                        {
                            auto result = std::format(
                                "將累計吸收值的{}%以{}傷害結算給{}",
                                machine.returnedPct,
                                damageKindLabel(machine.damageKind),
                                selectorLabel(machine.target, compact));
                            if (!machine.clearAfterSettle) result += std::format("{}保留累計值", qualifierSeparator);
                            return result;
                        }
                        else if constexpr (std::is_same_v<M, BorrowEffectRulesAction>)
                        {
                            return std::format(
                                "借用{}的大招規則{}數量{}{}允許類別[{}]{}不含複製與借用遞迴{}傳播借用規則",
                                selectorLabel(machine.sourceUnits, compact),
                                qualifierSeparator,
                                descriptionNumberLabel(machine.sourceCount, style),
                                qualifierSeparator,
                                borrowedRuleFilterLabel(machine.filter),
                                qualifierSeparator,
                                qualifierSeparator);
                        }
                        else if constexpr (std::is_same_v<M, CopyAttackDefinitionAction>)
                        {
                            return std::format(
                                "複製{}的絕招武功攻擊{}數量{}{}條件[{}]{}不傳播大招規則",
                                selectorLabel(machine.sourceUnits, compact),
                                qualifierSeparator,
                                machine.copyCount,
                                qualifierSeparator,
                                copiedMagicFilterLabel(machine.filter),
                                qualifierSeparator);
                        }
                        else if constexpr (std::is_same_v<M, SettleRemainingStatusDamageAction>)
                        {
                            return std::format(
                                "立即結算剩餘{}傷害",
                                battleStatusLabel(machine.status));
                        }
                        else if constexpr (std::is_same_v<M, GenerateClonesAction>)
                            return std::format("生成{}個分身", machine.count);
                        else if constexpr (std::is_same_v<M, PreventDeathAction>)
                            return std::format("首次致命傷害鎖定1生命並獲得{}幀無敵", machine.invincibilityFrames);
                        else if constexpr (std::is_same_v<M, ConfigureRescueRepositionAction>)
                            return std::format(
                                "每場可觸發{}次{}",
                                machine.activations,
                                machine.mode == RescueRepositionMode::Protect ? "保護挪移" : "處決挪移");
                        else static_assert(false, "Unhandled state machine action");
                    },
                    typed);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
            {
                assert(false && "conditional actions must be rendered through DescriptionConditional");
                return {};
            }
            else static_assert(false, "Unhandled effect action");
        },
        action);
}

std::vector<DescriptionQualifier> actionDescriptionQualifiers(
    const EffectActionValue& action)
{
    std::vector<DescriptionQualifier> qualifiers;
    const auto appendTimed = [&](
        int durationFrames,
        EffectStackPolicy stack,
        const std::optional<int>& stackLimit,
        EffectStackScope stackScope)
    {
        if (durationFrames > 0)
            qualifiers.emplace_back(DescriptionDurationFramesQualifier{ durationFrames });
        if (stack != EffectStackPolicy::Independent)
            qualifiers.emplace_back(DescriptionStackPolicyQualifier{ stack });
        if (stackLimit)
            qualifiers.emplace_back(DescriptionStackLimitQualifier{ *stackLimit });
        if (stackScope == EffectStackScope::EventSource)
            qualifiers.emplace_back(DescriptionStackScopeQualifier{ stackScope });
    };

    if (const auto* modifier = std::get_if<ModifyAttributeAction>(&action))
    {
        appendTimed(
            modifier->durationFrames,
            modifier->stack,
            modifier->stackLimit,
            modifier->stackScope);
    }
    else if (const auto* modifier = std::get_if<ModifyDamageAction>(&action))
    {
        appendTimed(
            modifier->durationFrames,
            modifier->stack,
            modifier->stackLimit,
            modifier->stackScope);
    }
    else if (const auto* status = std::get_if<ApplyStatusAction>(&action))
    {
        if (!status->duration && status->durationFrames > 0)
            qualifiers.emplace_back(
                DescriptionDurationFramesQualifier{ status->durationFrames });
    }
    return qualifiers;
}

std::vector<DescriptionQualifier> sharedActionDescriptionQualifiers(
    std::span<const EffectAction> actions)
{
    assert(actions.size() > 1);
    const auto* first = std::get_if<ModifyAttributeAction>(&actions.front().value);
    std::optional<int> firstAmount;
    if (first) firstAmount = effectiveConstantEffectNumberValue(first->amount);
    if (!first
        || !firstAmount
        || attributeModifierIsNegative(first->operation, *firstAmount)
        || first->durationFrames <= 0
        || (first->stack != EffectStackPolicy::Independent
            && first->stack != EffectStackPolicy::Refresh)
        || first->stackLimit
        || first->stackScope != EffectStackScope::Shared)
    {
        return {};
    }

    for (const auto& action : actions.subspan(1))
    {
        const auto* modifier = std::get_if<ModifyAttributeAction>(&action.value);
        std::optional<int> amount;
        if (modifier) amount = effectiveConstantEffectNumberValue(modifier->amount);
        if (!modifier
            || !amount
            || attributeModifierIsNegative(modifier->operation, *amount)
            || modifier->operation != first->operation
            || modifier->durationFrames != first->durationFrames
            || modifier->stack != first->stack
            || modifier->stackLimit
            || modifier->stackScope != EffectStackScope::Shared)
        {
            return {};
        }
    }

    // Sibling runtime stack domains remain distinct because actionOrder is part
    // of their keys.  Only the display-unobservable common duration is promoted;
    // caps, policies, scopes, counters, and winner semantics stay per action.
    return { DescriptionDurationFramesQualifier{ first->durationFrames } };
}

bool statusActionsDependOnOrder(
    const EffectActionValue& first,
    const EffectActionValue& second)
{
    const auto statusOf = [](const EffectActionValue& action)
        -> std::optional<BattleStatusKind>
    {
        if (const auto* apply = std::get_if<ApplyStatusAction>(&action)) return apply->status;
        if (const auto* consume = std::get_if<ConsumeStatusAction>(&action)) return consume->status;
        if (const auto* state = std::get_if<StateMachineAction>(&action))
        {
            if (const auto* settle = std::get_if<SettleRemainingStatusDamageAction>(state))
                return settle->status;
        }
        return std::nullopt;
    };
    const auto removalAffects = [](const RemoveStatusAction& removal, BattleStatusKind status)
    {
        return removal.negativeOnly
            || removal.controlOnly
            || std::ranges::contains(removal.statuses, status);
    };

    const auto firstStatus = statusOf(first);
    const auto secondStatus = statusOf(second);
    if (firstStatus && secondStatus && *firstStatus == *secondStatus) return true;
    if (const auto* removal = std::get_if<RemoveStatusAction>(&first))
        if (secondStatus && removalAffects(*removal, *secondStatus)) return true;
    if (const auto* removal = std::get_if<RemoveStatusAction>(&second))
        if (firstStatus && removalAffects(*removal, *firstStatus)) return true;
    return false;
}

bool actionPairDependsOnOrder(
    const EffectActionValue& first,
    const EffectActionValue& second)
{
    if (std::holds_alternative<std::shared_ptr<ConditionalEffectAction>>(first)
        || std::holds_alternative<std::shared_ptr<ConditionalEffectAction>>(second))
        return true;
    if (std::holds_alternative<StateMachineAction>(first)
        || std::holds_alternative<StateMachineAction>(second))
        return true;
    if (std::holds_alternative<DealDamageAction>(first)
        || std::holds_alternative<DealDamageAction>(second))
        return true;
    if (statusActionsDependOnOrder(first, second)) return true;

    if (const auto* lhs = std::get_if<ModifyAttributeAction>(&first))
        if (const auto* rhs = std::get_if<ModifyAttributeAction>(&second))
            return lhs->attribute == rhs->attribute;
    if (const auto* lhs = std::get_if<ModifyDamageAction>(&first))
        if (const auto* rhs = std::get_if<ModifyDamageAction>(&second))
            return lhs->perspective == rhs->perspective
                && lhs->stage == rhs->stage
                && lhs->channel == rhs->channel;
    if (const auto* lhs = std::get_if<ChangeResourceAction>(&first))
        if (const auto* rhs = std::get_if<ChangeResourceAction>(&second))
            return lhs->resource == rhs->resource
                || (lhs->resource == BattleResource::Mp && rhs->healRequiresFullMp)
                || (rhs->resource == BattleResource::Mp && lhs->healRequiresFullMp);
    if (std::holds_alternative<ModifyAttackAction>(first)
        && std::holds_alternative<ModifyAttackAction>(second))
        return true;
    if (std::holds_alternative<ModifyCastAction>(first)
        && std::holds_alternative<ModifyCastAction>(second))
        return true;
    return false;
}

bool actionsDependOnOrder(std::span<const EffectAction> actions)
{
    for (std::size_t lhs = 0; lhs < actions.size(); ++lhs)
    {
        for (std::size_t rhs = lhs + 1; rhs < actions.size(); ++rhs)
        {
            if (actionPairDependsOnOrder(actions[lhs].value, actions[rhs].value))
                return true;
        }
    }
    return false;
}

struct DescriptionRenderContext
{
    EffectDescriptionStyle style{};
    std::optional<EffectEvent> event;
    std::span<const DescriptionQualifier> suppressedActionQualifiers;
    EffectDescriptionPresentationContext presentation{};
    int intervalFrames{};
    bool ordinaryOutgoingAcceptedHit{};
    bool sameComboAllyDeath{};
    bool stackingOutgoingSkillDamage{};
    bool topLevelTriggerHidden{};
};

std::string renderRuleQualifier(
    const DescriptionQualifier& qualifier,
    EffectDescriptionStyle style,
    bool leadingSeparator = true)
{
    const bool compact = style == EffectDescriptionStyle::Compact;
    const std::string_view separator = leadingSeparator
        ? compact ? "，" : "；"
        : "";
    return std::visit(
        [style, compact, leadingSeparator, separator](const auto& typed) -> std::string
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, DescriptionMaxActivationsQualifier>)
                return compact
                    ? std::format("{}最多{}次", separator, typed.count)
                    : std::format("{}最多啟用{}次", separator, typed.count);
            else if constexpr (std::is_same_v<T, DescriptionSharedCooldownQualifier>)
                return compact
                    ? std::format("{}同來源冷卻{}幀", separator, typed.frames)
                    : std::format("{}同來源共用{}幀冷卻", separator, typed.frames);
            else if constexpr (std::is_same_v<T, DescriptionIntervalQualifier>)
                return compact
                    ? std::format("{}每{}幀", separator, typed.frames)
                    : std::format("{}每{}幀一次", separator, typed.frames);
            else if constexpr (std::is_same_v<T, DescriptionEveryNthEventQualifier>)
                return compact
                    ? std::format("{}每{}次", separator, typed.count)
                    : std::format("{}每{}次符合事件啟用一次", separator, typed.count);
            else if constexpr (std::is_same_v<T, DescriptionRepetitionCountQualifier>)
                return compact
                    ? std::format("{}依序×{}", separator, descriptionNumberLabel(typed.count, style))
                    : std::format("{}依序重複{}次", separator, descriptionNumberLabel(typed.count, style));
            else if constexpr (std::is_same_v<T, DescriptionActivationLimitQualifier>)
            {
                assert(typed.scope == EffectActivationScope::PerCastPerTarget);
                return compact
                    ? std::format("{}每施放每目標{}次", separator, typed.count)
                    : std::format("{}每次施放對同一目標最多判定{}次", separator, typed.count);
            }
            else return {};
        },
        qualifier);
}

std::vector<std::string> ruleQualifierDescriptions(
    const DescriptionRuleQualifiersFact& qualifiers,
    EffectDescriptionStyle style,
    EffectEvent event)
{
    std::vector<std::string> result;
    const auto append = [&](DescriptionQualifier qualifier)
    {
        auto text = renderRuleQualifier(qualifier, style, false);
        if (!text.empty())
            result.push_back(std::move(text));
    };

    if (qualifiers.intervalFrames > 0 && event != EffectEvent::FrameAdvanced)
        append(DescriptionIntervalQualifier{qualifiers.intervalFrames});
    if (qualifiers.everyNthEvent > 0)
        append(DescriptionEveryNthEventQualifier{qualifiers.everyNthEvent});
    if (qualifiers.repetitionCount)
        append(DescriptionRepetitionCountQualifier{*qualifiers.repetitionCount});
    if (qualifiers.maxActivations > 0)
        append(DescriptionMaxActivationsQualifier{qualifiers.maxActivations});
    if (qualifiers.sharedCooldownFrames > 0)
        append(DescriptionSharedCooldownQualifier{qualifiers.sharedCooldownFrames});
    if (qualifiers.activationLimit)
    {
        append(DescriptionActivationLimitQualifier{
            qualifiers.activationLimit->scope,
            qualifiers.activationLimit->maxEvaluations,
        });
    }
    return result;
}

std::string renderGuardQualifier(
    const DescriptionQualifier& qualifier,
    EffectDescriptionStyle style,
    bool leadingSeparator)
{
    const bool compact = style == EffectDescriptionStyle::Compact;
    const std::string_view separator = leadingSeparator
        ? compact ? "，" : "；"
        : "";
    if (const auto* interval = std::get_if<DescriptionIntervalQualifier>(&qualifier))
        return compact
            ? std::format("{}每{}幀", separator, interval->frames)
            : std::format("{}每{}幀一次", separator, interval->frames);
    if (const auto* nth = std::get_if<DescriptionEveryNthEventQualifier>(&qualifier))
        return compact
            ? std::format("{}每{}次", separator, nth->count)
            : std::format("{}每{}次符合事件啟用一次", separator, nth->count);
    return renderRuleQualifier(qualifier, style, leadingSeparator);
}

std::string renderSharedActionQualifiers(
    std::span<const DescriptionQualifier> qualifiers,
    bool compact)
{
    std::string result;
    for (const auto& qualifier : qualifiers)
    {
        std::visit(
            [&](const auto& typed)
            {
                using T = std::decay_t<decltype(typed)>;
                if constexpr (std::is_same_v<T, DescriptionDurationFramesQualifier>)
                    result += compact ? std::format("，{}幀", typed.frames) : std::format("，持續{}幀", typed.frames);
                else if constexpr (std::is_same_v<T, DescriptionStackPolicyQualifier>)
                {
                    result += compact ? "，" : "；";
                    result += stackPolicyLabel(typed.policy, compact, false);
                }
                else if constexpr (std::is_same_v<T, DescriptionStackLimitQualifier>)
                    result += std::format("，最多{}層", typed.count);
                else if constexpr (std::is_same_v<T, DescriptionStackScopeQualifier>)
                    result += "，各事件來源分別疊加";
            },
            qualifier);
    }
    return result;
}

std::string renderEffectActions(
    std::span<const EffectAction> actions,
    DescriptionRenderContext context,
    bool coalesce);

std::string renderConditionalConditions(
    std::span<const EffectCondition> conditions,
    bool compact)
{
    std::string result;
    for (const auto& condition : conditions)
    {
        if (!result.empty()) result += "且";
        result += conditionLabel(condition, compact);
    }
    return result;
}

std::string renderEffectAction(
    const EffectAction& action,
    DescriptionRenderContext context,
    bool coalesce)
{
    const bool detailed = context.style == EffectDescriptionStyle::Detailed;
    const bool compact = context.style == EffectDescriptionStyle::Compact;
    const auto sequenceSeparator = detailed ? "，接著" : "，再";

    if (const auto* conditional = std::get_if<
            std::shared_ptr<ConditionalEffectAction>>(&action.value))
    {
        assert(*conditional);
        const auto conditions = renderConditionalConditions(
            (*conditional)->conditions,
            compact);
        const auto whenTrue = renderEffectActions(
            (*conditional)->whenTrue,
            context,
            coalesce);
        const auto whenFalse = renderEffectActions(
            (*conditional)->whenFalse,
            context,
            coalesce);
        if (whenFalse.empty())
            return std::format("若{}，{}", conditions, whenTrue);
        return compact
            ? std::format("若{}，{}；否則{}", conditions, whenTrue, whenFalse)
            : std::format("若{}則{}，否則{}", conditions, whenTrue, whenFalse);
    }

    if (const auto* consume = std::get_if<ConsumeStatusAction>(&action.value);
        consume && consume->whenDepleted)
    {
        auto base = *consume;
        const auto depleted = *base.whenDepleted;
        base.whenDepleted.reset();
        const auto baseText = renderDescriptionActionArgument(
            EffectActionValue{std::move(base)},
            context.style,
            context.suppressedActionQualifiers,
            context.presentation);
        const auto depletedText = renderEffectAction(
            EffectAction{EffectActionValue{depleted}},
            context,
            coalesce);
        return std::format(
            "{}{}若{}耗盡，{}",
            baseText,
            sequenceSeparator,
            battleStatusLabel(consume->status),
            depletedText);
    }

    if (const auto* state = std::get_if<StateMachineAction>(&action.value))
    {
        if (const auto* absorption = std::get_if<StartDamageAbsorptionAction>(state))
        {
            auto settlement = std::format(
                "將累計吸收值的{}%以{}傷害結算給{}",
                absorption->returnedPct,
                damageKindLabel(absorption->settlementDamageKind),
                selectorLabel(absorption->settlementTarget, compact));
            if (absorption->settleOnSourceDeath)
                settlement += compact ? "，來源死亡時立即結算" : "；來源死亡時立即結算";
            const auto initial = compact ? "，初始0" : "；首次記錄值為0";
            return std::format(
                "{}幀內記錄並吸收所受傷害的{}%{}{}{}清空記錄{}",
                absorption->durationFrames,
                absorption->absorbedPct,
                sequenceSeparator,
                settlement,
                sequenceSeparator,
                initial);
        }

        auto result = renderDescriptionActionArgument(
            action.value,
            context.style,
            context.suppressedActionQualifiers,
            context.presentation);
        if (std::holds_alternative<RecordMaximumDamageAction>(*state))
            result += compact ? "，初始0" : "；首次記錄值為0";
        else if (const auto* consume = std::get_if<ConsumeRecordedMaximumAction>(state);
                 consume && consume->clearAfterConsume)
            result += std::string(sequenceSeparator) + (compact ? "清空" : "清空記錄");
        else if (const auto* settle = std::get_if<SettleDamageAbsorptionAction>(state);
                 settle && settle->clearAfterSettle)
            result += std::string(sequenceSeparator) + (compact ? "清空" : "清空記錄");
        return result;
    }

    return renderDescriptionActionArgument(
        action.value,
        context.style,
        context.suppressedActionQualifiers,
        context.presentation);
}

std::string renderEffectActions(
    std::span<const EffectAction> actions,
    DescriptionRenderContext context,
    bool coalesce)
{
    if (actions.empty()) return {};
    if (actions.size() == 1)
        return renderEffectAction(actions.front(), context, coalesce);

    const bool detailed = context.style == EffectDescriptionStyle::Detailed;
    const bool compact = context.style == EffectDescriptionStyle::Compact;
    if (actionsDependOnOrder(actions))
    {
        std::string result;
        const auto separator = detailed ? "，接著" : "，再";
        for (const auto& action : actions)
        {
            if (!result.empty()) result += separator;
            result += renderEffectAction(action, context, coalesce);
        }
        return result;
    }

    const auto renderSimultaneous = [&](
        std::span<const EffectAction> group,
        std::span<const DescriptionQualifier> shared)
    {
        auto nested = context;
        nested.suppressedActionQualifiers = shared;
        const auto separator = compact && !shared.empty() ? "；" : compact ? "，" : "、";
        std::string result;
        for (const auto& action : group)
        {
            if (!result.empty()) result += separator;
            result += renderEffectAction(action, nested, coalesce);
        }
        result += renderSharedActionQualifiers(shared, compact);
        return result;
    };

    if (!coalesce) return renderSimultaneous(actions, {});

    std::string result;
    for (std::size_t begin = 0; begin < actions.size();)
    {
        std::size_t end = begin + 1;
        std::vector<DescriptionQualifier> shared;
        while (end < actions.size())
        {
            auto candidate = sharedActionDescriptionQualifiers(
                actions.subspan(begin, end - begin + 1));
            if (candidate.empty()) break;
            shared = std::move(candidate);
            ++end;
        }
        if (!result.empty()) result += compact ? "，" : "、";
        result += renderSimultaneous(
            actions.subspan(begin, end - begin),
            shared);
        begin = end;
    }
    return result;
}

std::string renderActionDescription(
    const EffectAction& action,
    EffectDescriptionStyle style,
    EffectEvent event,
    bool coalesce,
    const EffectDescriptionPresentationContext& context)
{
    return renderEffectAction(
        action,
        DescriptionRenderContext{
            .style = style,
            .event = event,
            .presentation = context,
        },
        coalesce);
}

std::vector<DescriptionActionPhraseRow> renderPlayerActionDescriptionRows(
    const EffectAction& action,
    EffectDescriptionStyle style,
    EffectEvent event,
    bool coalesce,
    const EffectDescriptionPresentationContext& context)
{
    assert(style != EffectDescriptionStyle::Detailed);
    if (const auto* status = std::get_if<ApplyStatusAction>(&action.value))
    {
        if (style == EffectDescriptionStyle::Compact
            && context.compactPolicy == EffectDescriptionCompactPolicy::PlayerCard
            && status->status == BattleStatusKind::Shadowless
            && status->behavior
            && status->reapplication == StatusReapplicationPolicy::RefreshDuration)
        {
            if (const auto behavior = renderPlayerCardShadowlessBehavior(
                    *status->behavior))
            {
                return {{std::format(
                    "無影（{}幀，刷新）：{}",
                    statusDurationLabel(*status, style),
                    *behavior)}};
            }
        }

        const auto* layers = std::get_if<AddStatusLayers>(&status->quantity);
        if (status->status == BattleStatusKind::Bleed)
        {
            const auto* sharedLayers = std::get_if<AddSharedStatusLayers>(
                &status->quantity);
            if (sharedLayers && statusBehaviorHasExactShape(*status, { 1 }))
            {
                if (style == EffectDescriptionStyle::Compact)
                {
                    return {{std::format(
                        "施加{}層流血（目標總上限{}層）",
                        sharedLayers->count,
                        sharedLayers->targetTotalLimit)}};
                }
                return {{std::format(
                    "施加{}層流血，目標共享上限{}層；每10幀造成一次目標最大生命1% × 流血總層數的流血傷害",
                    sharedLayers->count,
                    sharedLayers->targetTotalLimit)}};
            }
        }

        if (status->status == BattleStatusKind::ColdPoison
            && statusBehaviorHasExactShape(*status, { 2 }))
        {
            const auto* healing = findStatusBehaviorAction<ModifyHealTransactionAction>(
                *status);
            const auto* speed = findStatusBehaviorAction<ModifyAttributeAction>(
                *status);
            if (healing && speed)
            {
                const auto speedDelta = effectiveConstantEffectNumberValue(
                    speed->amount);
                assert(speedDelta && *speedDelta <= 0);
                const auto duration = statusDurationLabel(
                    *status,
                    style);
                if (style == EffectDescriptionStyle::Compact)
                {
                    return {{std::format(
                        "寒毒{}幀（禁療、速度-{}%；再施加取代剩餘時間）",
                        duration,
                        -*speedDelta)}};
                }
                return {{std::format(
                    "施加寒毒{}幀；狀態期間無法受到治療、速度降低{}%；再次施加會以新的持續時間取代剩餘時間",
                    duration,
                    -*speedDelta)}};
            }
        }

        if (status->status == BattleStatusKind::MpBlocked
            && !status->behavior)
        {
            const auto duration = statusDurationLabel(*status, style);
            if (!duration.empty())
            {
                if (style == EffectDescriptionStyle::Compact)
                    return {{std::format(
                        "封內{}幀（再施加保留較長持續時間）",
                        duration)}};
                return {{std::format(
                    "施加封內{}幀；再次施加時保留較長持續時間",
                    duration)}};
            }
        }

        if (status->status == BattleStatusKind::NeutralizeForce
            && statusBehaviorHasExactShape(*status, { 2 }))
        {
            const auto* charges = std::get_if<SetStatusTriggerCharges>(
                &status->quantity);
            const auto* recovery = findStatusBehaviorAction<ChangeResourceAction>(*status);
            if (charges && recovery)
            {
                const auto amount = descriptionNumberLabel(
                    recovery->amount,
                    style);
                if (style == EffectDescriptionStyle::Compact)
                {
                    return {
                        {std::format(
                            "化勁{}次：下次命中為目標回內",
                            charges->count)},
                        {std::format(
                            "命中目標恢復{}內力",
                            amount), 1},
                        {"再次施加：完整取代現有化勁", 1},
                    };
                }
                return {
                    {std::format("施加可觸發{}次的化勁", charges->count)},
                    {std::format(
                        "狀態持有者下次命中時，命中目標恢復{}內力並消耗1次化勁",
                        amount), 1},
                    {"再次施加會完整取代現有化勁", 1},
                };
            }
        }

        if (status->status == BattleStatusKind::Blinded
            && statusBehaviorHasExactShape(*status, { 1 }))
        {
            const auto* charges = std::get_if<SetStatusTriggerCharges>(
                &status->quantity);
            const auto* suppression = findStatusBehaviorAction<
                SuppressCurrentCastContactsAction>(*status);
            if (charges && suppression)
            {
                if (style == EffectDescriptionStyle::Compact)
                {
                    return {
                        {std::format("刺目{}次：使下次施放落空", charges->count)},
                        {"再次施加：完整取代現有刺目", 1},
                    };
                }
                return {
                    {std::format("施加可觸發{}次的刺目", charges->count)},
                    {"觸發時使該次施放目前及剩餘攻擊落空", 1},
                    {"再次施加會完整取代現有刺目", 1},
                };
            }
        }

        if (status->status == BattleStatusKind::BattleSpirit
            && layers
            && statusBehaviorHasExactShape(*status, { 2 }))
        {
            const auto* outgoing = findStatusBehaviorAction<ModifyDamageAction>(
                *status,
                [](const ModifyDamageAction& modifier)
                {
                    return modifier.perspective == DamageModifierPerspective::Outgoing
                        && modifier.channel == DamageChannel::Skill
                        && modifier.operation == DamageModifierOperation::PercentAdd;
                });
            const auto* incoming = findStatusBehaviorAction<ModifyDamageAction>(
                *status,
                [](const ModifyDamageAction& modifier)
                {
                    return modifier.perspective == DamageModifierPerspective::Incoming
                        && modifier.channel == DamageChannel::All
                        && modifier.operation == DamageModifierOperation::PercentAdd;
                });
            if (outgoing && incoming)
            {
                const auto outgoingPerLayer = effectiveConstantEffectNumberValue(
                    outgoing->amount);
                const auto incomingPerLayer = effectiveConstantEffectNumberValue(
                    incoming->amount);
                assert(outgoingPerLayer && incomingPerLayer && *incomingPerLayer <= 0);
                const auto reductionPerLayer = -*incomingPerLayer;
                if (style == EffectDescriptionStyle::Compact)
                {
                    return {{std::format(
                        "獲得{}層戰意（此來源上限{}層；每層招式傷害+{}%、受到傷害減少{}%）",
                        layers->count,
                        layers->limit,
                        *outgoingPerLayer,
                        reductionPerLayer)}};
                }
                return {{std::format(
                    "獲得{}層戰意，此來源上限{}層；每層使招式傷害+{}%、受到傷害減少{}%",
                    layers->count,
                    layers->limit,
                    *outgoingPerLayer,
                    reductionPerLayer)}};
            }
        }

        if (status->status == BattleStatusKind::WitheredBone
            && statusBehaviorHasExactShape(*status, { 2 }))
        {
            const auto* damage = findStatusBehaviorAction<ModifyDamageAction>(
                *status,
                [](const ModifyDamageAction& modifier)
                {
                    return modifier.perspective == DamageModifierPerspective::Incoming
                        && modifier.channel == DamageChannel::All
                        && modifier.operation == DamageModifierOperation::PercentAdd;
                });
            const auto* healing = findStatusBehaviorAction<ModifyHealTransactionAction>(
                *status,
                [](const ModifyHealTransactionAction& modifier)
                {
                    return modifier.operation == HealModifierOperation::MultiplyReceived;
                });
            if (damage && healing)
            {
                const auto damageIncrease = effectiveConstantEffectNumberValue(damage->amount);
                assert(damageIncrease);
                const auto duration = statusDurationLabel(
                    *status,
                    style);
                if (style == EffectDescriptionStyle::Compact)
                {
                    return {{std::format(
                        "枯骨{}幀（受傷+{}%、受療-{}%；再施加取代剩餘時間）",
                        duration,
                        *damageIncrease,
                        100 - healing->percent)}};
                }
                return {
                    {std::format(
                        "施加枯骨狀態{}幀；再次施加會以新的持續時間取代剩餘時間",
                        duration)},
                    {std::format(
                        "狀態期間，受到的傷害提高{}%、受到的治療降低{}%",
                        *damageIncrease,
                        100 - healing->percent)},
                };
            }
        }

        if (status->status == BattleStatusKind::DamageBlockLayer
            && statusBehaviorHasExactShape(*status, { 1 }))
        {
            const auto* charges = std::get_if<AddDamageBlockCharges>(
                &status->quantity);
            const auto* block = findStatusBehaviorAction<BlockPositiveDamageAction>(
                *status);
            if (charges && block)
            {
                if (style == EffectDescriptionStyle::Compact)
                {
                    return {{std::format(
                        "傷害抵擋+{}次（此來源最多{}次抵擋；每次抵擋一次非處決正傷害）",
                        charges->count,
                        charges->limit)}};
                }
                return {{std::format(
                    "增加{}次傷害抵擋；此效果提供的傷害抵擋最多{}次。每次抵擋一次非處決正傷害",
                    charges->count,
                    charges->limit)}};
            }
        }

        if (status->status == BattleStatusKind::TrueQi
            && layers
            && statusBehaviorHasExactShape(*status, { 1 }))
        {
            const auto* damage = findStatusBehaviorAction<DealDamageAction>(*status);
            if (damage)
            {
                auto perLayer = damage->amount;
                perLayer.statusScale = StatusNumberScale::Once;
                const auto amount = descriptionNumberLabel(perLayer, style);
                if (style == EffectDescriptionStyle::Compact)
                {
                    return {
                        {std::format("真氣+{}層", layers->count)},
                        {std::format(
                            "此來源最多{}層；每層命中附加{}{}傷害",
                            layers->limit,
                            amount,
                            damageKindLabel(damage->kind)),
                            1},
                    };
                }
                return {
                    {std::format(
                        "獲得{}層真氣；此效果提供的真氣最多{}層",
                        layers->count,
                        layers->limit)},
                    {std::format(
                        "持有者命中時，每層真氣對命中目標附加{}點{}傷害",
                        amount,
                        damageKindLabel(damage->kind))},
                };
            }
        }

        if (status->status == BattleStatusKind::SingleHitCapLayer
            && statusBehaviorHasExactShape(*status, { 1 }))
        {
            const auto* cap = findStatusBehaviorAction<ModifyDamageAction>(
                *status,
                [](const ModifyDamageAction& modifier)
                {
                    return modifier.operation == DamageModifierOperation::CapSingleHitAtValue;
                });
            if (cap)
            {
                auto amount = descriptionNumberLabel(cap->amount, style);
                if (cap->amount.base == EffectNumberBase::ApplicationTargetMaxHp
                    && amount.starts_with("套用目標"))
                {
                    amount.erase(0, std::string_view("套用").size());
                }
                return {{std::format(
                    "下次承傷不超過{}",
                    amount)}};
            }
        }
    }

    const auto* attack = std::get_if<ModifyAttackAction>(&action.value);
    if (!attack)
    {
        return {{renderActionDescription(action, style, event, coalesce, context)}};
    }
    if (hasOrdinaryAttackModification(*attack))
    {
        return {{renderActionDescription(action, style, event, coalesce, context)}};
    }

    return std::visit(
        [&](const auto& behavior) -> std::vector<DescriptionActionPhraseRow>
        {
            using B = std::decay_t<decltype(behavior)>;
            if constexpr (std::is_same_v<B, ProjectileBounceAttackBehavior>)
            {
                return {{std::format(
                    "命中時有{}%機率在{}像素內彈射，最多追加命中{}次",
                    behavior.chancePct,
                    behavior.rangePixels,
                    behavior.additionalHits)}};
            }
            else if constexpr (std::is_same_v<B, NearbyTrackingAttackBehavior>)
            {
                return {{std::format(
                    "命中後在{}像素內產生{}%傷害追蹤彈",
                    behavior.rangePixels,
                    behavior.damagePct)}};
            }
            else if constexpr (std::is_same_v<B, DelayedAlternateAttackBehavior>)
            {
                if (style == EffectDescriptionStyle::Compact)
                {
                    return {
                        {std::format(
                            "{}幀後以{}%傷害追擊最近的其他敵人；附近無其他敵人則追擊原攻擊目標",
                            behavior.delayFrames,
                            behavior.damagePct)},
                        {std::format(
                            "追擊出手時：{}%機率獲得1次傷害抵擋（最多1次）",
                            behavior.attackerBlockGainChancePct),
                            1,
                            EffectDescriptionSemanticBreak::ActionGroup},
                    };
                }
                return {
                    {std::format(
                        "{}幀後以{}%傷害追擊最近的其他敵人；附近沒有其他敵人時，則追擊原本的攻擊目標",
                        behavior.delayFrames,
                        behavior.damagePct)},
                    {std::format(
                        "追擊出手時，有{}%機率獲得1次傷害抵擋，最多持有1次",
                        behavior.attackerBlockGainChancePct),
                        1,
                        EffectDescriptionSemanticBreak::ActionGroup},
                };
            }
            else
            {
                return {{renderActionDescription(action, style, event, coalesce, context)}};
            }
        },
        attack->runtimeBehavior);
}

std::vector<DescriptionActionPhraseRow> renderDetailedStatusBehaviorRows(
    const ApplyStatusAction& status)
{
    if (!status.behavior) return {};

    if (status.status == BattleStatusKind::BattleSpirit
        && statusBehaviorHasExactShape(status, { 2 }))
    {
        const auto* outgoing = findStatusBehaviorAction<ModifyDamageAction>(
            status,
            [](const ModifyDamageAction& modifier)
            {
                return modifier.perspective == DamageModifierPerspective::Outgoing
                    && modifier.channel == DamageChannel::Skill
                    && modifier.operation == DamageModifierOperation::PercentAdd;
            });
        const auto* incoming = findStatusBehaviorAction<ModifyDamageAction>(
            status,
            [](const ModifyDamageAction& modifier)
            {
                return modifier.perspective == DamageModifierPerspective::Incoming
                    && modifier.channel == DamageChannel::All
                    && modifier.operation == DamageModifierOperation::PercentAdd;
            });
        if (outgoing && incoming)
        {
            const auto outgoingValue = effectiveConstantEffectNumberValue(outgoing->amount);
            const auto incomingValue = effectiveConstantEffectNumberValue(incoming->amount);
            assert(outgoingValue && incomingValue && *incomingValue <= 0);
            return {
                {std::format("每層生效：招式傷害增加{}%", *outgoingValue)},
                {std::format("每層生效：傷害減免{}%", -*incomingValue)},
                {"來源隔離：其他生產者可提供自己的戰意，但不共用此來源的層數上限或每層數值"},
                {"管線位置：造成與承受修正沿用既有狀態貢獻的傷害結算位置"},
            };
        }
    }

    if (status.status == BattleStatusKind::WitheredBone
        && statusBehaviorHasExactShape(status, { 2 }))
    {
        const auto* damage = findStatusBehaviorAction<ModifyDamageAction>(status);
        const auto* healing = findStatusBehaviorAction<ModifyHealTransactionAction>(status);
        if (damage && healing)
        {
            const auto damageValue = effectiveConstantEffectNumberValue(damage->amount);
            assert(damageValue);
            return {
                {std::format("持續生效：受到傷害增加{}%", *damageValue)},
                {std::format("持續生效：受到治療減少{}%", 100 - healing->percent)},
            };
        }
    }

    if (status.status == BattleStatusKind::Bleed
        && statusBehaviorHasExactShape(status, { 1 }))
    {
        return {
            {"群組規則：所有來源在此目標共享流血層數上限與10幀計時"},
            {"重新套用：增加層數不重置計時；較高的目標總上限只在目前群組存續期間向上調整"},
            {"結算：每10幀造成一筆目標最大生命1% × 流血總層數的合併流血傷害，向零取整且至少1點"},
            {"生命週期：結算不消耗層數；完整移除後，層數、上限、計時與來源一併重置"},
        };
    }

    if (status.status == BattleStatusKind::SevenStarMark
        && statusBehaviorHasExactShape(status, { 2 }))
    {
        const auto* damage = findStatusBehaviorAction<ModifyDamageAction>(status);
        const auto* consume = findStatusBehaviorAction<ConsumeThisStatusAction>(status);
        assert(damage && consume && consume->whenDepleted);
        const auto ignoredDefense = effectiveConstantEffectNumberValue(damage->amount);
        assert(ignoredDefense);
        return {
            {"群組規則：目標只保留一個完整七星狀態包"},
            {"重新套用：以新套用的印記、持續時間、來源與綁定數值原子取代現有七星；新印記數可以較少"},
            {"觸發：施加方任一友軍以招式命中狀態持有者"},
            {std::format(
                "動作：該次招式忽略{}%防禦並消耗{}枚七星印記",
                *ignoredDefense,
                consume->quantity)},
            {std::format(
                "耗盡時：使狀態持有者{}{}幀",
                battleStatusLabel(consume->whenDepleted->status),
                statusDurationLabel(
                    *consume->whenDepleted,
                    EffectDescriptionStyle::Detailed)), 1},
        };
    }

    if (status.status == BattleStatusKind::NeutralizeForce
        && statusBehaviorHasExactShape(status, { 2 }))
    {
        assert(status.neutralizeMpRecovery);
        return {
            {"群組規則：目標只保留一個完整化勁充能包；再次施加會完整取代"},
            {"觸發：狀態持有者成功命中時"},
            {"動作：消耗1次化勁，命中目標恢復內力"},
            {"命中回內：命中目標恢復"
                + descriptionNumberLabel(
                    *status.neutralizeMpRecovery,
                    EffectDescriptionStyle::Detailed)
                + "內力"},
        };
    }

    if (status.status == BattleStatusKind::TrueQi
        && statusBehaviorHasExactShape(status, { 1 }))
    {
        const auto* damage = findStatusBehaviorAction<DealDamageAction>(status);
        if (damage)
        {
            auto perLayer = damage->amount;
            perLayer.statusScale = StatusNumberScale::Once;
            return {
                {"觸發：狀態持有者命中"},
                {"目標：命中目標"},
                {std::format(
                    "動作：每層造成{}點{}傷害",
                    descriptionNumberLabel(perLayer, EffectDescriptionStyle::Detailed),
                    damageKindLabel(damage->kind))},
                {"傷害範圍：單體（省略欄位後的預設值）"},
            };
        }
    }

    if (status.status == BattleStatusKind::SingleHitCapLayer
        && statusBehaviorHasExactShape(status, { 1 }))
    {
        const auto* cap = findStatusBehaviorAction<ModifyDamageAction>(status);
        if (cap)
        {
            return {{"持續生效：下次承傷不超過"
                + descriptionNumberLabel(cap->amount, EffectDescriptionStyle::Detailed)}};
        }
    }

    std::vector<DescriptionActionPhraseRow> result;
    for (const auto& rule : status.behavior->rules)
    {
        const auto trigger = rule.event == EffectEvent::HitBeforeDamage
                && rule.observation == EffectObservationScope::SourceOwnerTeamEventSource
            ? std::string{"來源效果擁有者的任一友軍命中時"}
            : rule.event == EffectEvent::UnitDied
                && rule.observation == EffectObservationScope::StatusHolderEventTarget
            ? std::string{"狀態持有者死亡時"}
            : rule.event == EffectEvent::StatusPersistent
            ? std::string{"狀態期間持續生效"}
            : ruleEventLabel(rule.event, EffectDescriptionStyle::Detailed);
        result.push_back({"狀態行為/觸發：" + trigger});
        result.push_back({std::string{"狀態行為/觀察範圍："}
            + std::string(detailedStatusObservationLabel(rule.observation))});
        result.push_back({"狀態行為/目標：" + selectorLabel(rule.selector, false)});
        for (const auto& condition : rule.conditions)
            result.push_back({"狀態行為/條件："
                + conditionLabel(condition, false, rule.event)});
        if (rule.actions.size() > 1)
            result.push_back({actionsDependOnOrder(rule.actions)
                ? "動作關係：依序"
                : "動作關係：同時"});
        for (const auto& action : rule.actions)
        {
            if (const auto* consume = std::get_if<ConsumeThisStatusAction>(&action.value))
            {
                const auto& quantity = statusCatalogEntry(status.status).quantity;
                result.push_back({std::format(
                    "動作：消耗此來源{}{}{}{}",
                    consume->quantity,
                    statusQuantityCounter(quantity),
                    battleStatusLabel(status.status),
                    statusQuantityObjectSuffix(quantity))});
                if (consume->whenDepleted)
                {
                    const auto& depleted = *consume->whenDepleted;
                    auto summary = depleted;
                    summary.behavior.reset();
                    result.push_back({
                        "耗盡時：" + renderDescriptionActionArgument(
                            summary,
                            EffectDescriptionStyle::Detailed),
                        1 });
                    auto nestedRows = renderDetailedStatusBehaviorRows(depleted);
                    for (auto& row : nestedRows)
                    {
                        row.indent += 2;
                        result.push_back(std::move(row));
                    }
                }
                continue;
            }
            if (const auto* nested = std::get_if<ApplyStatusAction>(&action.value))
            {
                auto summary = *nested;
                summary.behavior.reset();
                result.push_back({"動作：" + renderDescriptionActionArgument(
                    summary,
                    EffectDescriptionStyle::Detailed)});
                auto nestedRows = renderDetailedStatusBehaviorRows(*nested);
                for (auto& row : nestedRows)
                {
                    ++row.indent;
                    result.push_back(std::move(row));
                }
                continue;
            }
            result.push_back({"動作：" + renderDescriptionActionArgument(
                action.value,
                EffectDescriptionStyle::Detailed)});
            if (const auto* damage = std::get_if<DealDamageAction>(&action.value);
                damage && damage->area.kind == DamageAreaKind::SingleTarget)
            {
                result.push_back({"傷害範圍：單體（省略欄位後的預設值）", 1});
            }
        }
    }
    return result;
}

}  // namespace KysChess::EffectDescriptionDetail
