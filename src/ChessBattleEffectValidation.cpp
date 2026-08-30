#include "ChessBattleEffectValidation.h"
#include "ChessBattleEffectConstraints.h"
#include "ChessBattleEffectSemantics.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <limits>
#include <ranges>
#include <set>
#include <type_traits>

namespace KysChess
{
namespace
{
bool validateSelectorAtEvent(const EffectSelector& selector, EffectEvent event, std::string& error)
{
    if (selector.kind == EffectSelectorKind::HitTarget
        && !effectSelectorKindAllowedAtEvent(selector.kind, event))
    {
        error = "命中目標選擇器需要命中或傷害事件";
        return false;
    }
    if (selector.kind == EffectSelectorKind::TransactionTarget
        && !effectSelectorKindAllowedAtEvent(selector.kind, event))
    {
        error = "交易目標選擇器需要交易事件";
        return false;
    }
    if (selector.kind == EffectSelectorKind::OriginalAttackTarget
        && !effectSelectorKindAllowedAtEvent(selector.kind, event))
    {
        error = "原攻擊目標選擇器需要施放、攻擊、命中、傷害或施放聚合事件";
        return false;
    }
    if (selector.requiredTarget)
    {
        if (!effectRequiredTargetAllowedAtEvent(*selector.requiredTarget, event))
        {
            error = "必含目標不支援此事件：" + error;
            return false;
        }
    }
    return true;
}

bool validateSelectorSchema(const EffectSelector& selector, std::string& error)
{
    if (selector.count < 0)
    {
        error = "目標數量不可為負數";
        return false;
    }
    if (selector.kind == EffectSelectorKind::UnitsInRadius && selector.radiusTiles <= 0)
    {
        error = "半徑選擇器需要正半徑";
        return false;
    }
    if (selector.kind == EffectSelectorKind::UnitsInSquare
        && (selector.squareSideTiles <= 0 || selector.squareSideTiles % 2 == 0))
    {
        error = "方形選擇器邊長必須是正奇數";
        return false;
    }
    if (selector.kind == EffectSelectorKind::AlliesUsingMartialCategory
        && selector.requiredMartialCategory == EffectMartialCategory::None)
    {
        error = "指定武學類別友軍需要武學類別";
        return false;
    }
    if (selector.requiredTarget)
    {
        switch (selector.kind)
        {
        case EffectSelectorKind::Self:
        case EffectSelectorKind::SourceUnit:
        case EffectSelectorKind::TransactionTarget:
        case EffectSelectorKind::HitTarget:
        case EffectSelectorKind::OriginalAttackTarget:
        case EffectSelectorKind::HighestMpEnemy:
        case EffectSelectorKind::FarthestEnemy:
            error = "單一目標選擇器不能再指定必含目標";
            return false;
        case EffectSelectorKind::ComboMembers:
        case EffectSelectorKind::AllLivingUnits:
        case EffectSelectorKind::Allies:
        case EffectSelectorKind::Enemies:
        case EffectSelectorKind::LowestHpAllies:
        case EffectSelectorKind::LowestMpAllies:
        case EffectSelectorKind::StrongestEnemies:
        case EffectSelectorKind::NearestEnemies:
        case EffectSelectorKind::UnitsInRadius:
        case EffectSelectorKind::UnitsInSquare:
        case EffectSelectorKind::AlliesUsingMartialCategory:
            break;
        }
    }
    return true;
}

bool selectorHasSingleResult(const EffectSelector& selector)
{
    switch (selector.kind)
    {
    case EffectSelectorKind::Self:
    case EffectSelectorKind::SourceUnit:
    case EffectSelectorKind::TransactionTarget:
    case EffectSelectorKind::HitTarget:
    case EffectSelectorKind::OriginalAttackTarget:
    case EffectSelectorKind::HighestMpEnemy:
    case EffectSelectorKind::FarthestEnemy:
        return true;
    case EffectSelectorKind::ComboMembers:
    case EffectSelectorKind::AllLivingUnits:
    case EffectSelectorKind::Allies:
    case EffectSelectorKind::Enemies:
    case EffectSelectorKind::LowestHpAllies:
    case EffectSelectorKind::LowestMpAllies:
    case EffectSelectorKind::StrongestEnemies:
    case EffectSelectorKind::NearestEnemies:
    case EffectSelectorKind::UnitsInRadius:
    case EffectSelectorKind::UnitsInSquare:
    case EffectSelectorKind::AlliesUsingMartialCategory:
        return selector.count == 1;
    }
    assert(false);
    return false;
}

bool validateConditionAtEvent(const EffectCondition& condition, EffectEvent event, std::string& error)
{
    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            const auto reject = [&](std::string message)
            {
                error = std::move(message);
                return false;
            };
            if constexpr (std::is_same_v<T, IsUltimateCondition>
                || std::is_same_v<T, CastUsesEffectSourceMagicCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event))
                    return reject("施放條件需要具有施放識別的事件");
            }
            else if constexpr (std::is_same_v<T, SourceHpRatioAtMostCondition>
                || std::is_same_v<T, SourceHpRatioBelowCondition>
                || std::is_same_v<T, TargetHpRatioAtMostCondition>)
            {
                if (typed.percent < 0 || typed.percent > 100) return reject("生命比例條件必須介於 0 與 100");
                if constexpr (std::is_same_v<T, TargetHpRatioAtMostCondition>)
                {
                    if (!effectConditionAllowedAtEvent(condition.index(), event))
                        return reject("目標生命條件需要命中、傷害或治療事件");
                }
            }
            else if constexpr (std::is_same_v<T, IsMainProjectileCondition>
                || std::is_same_v<T, IsRootAttackCondition>
                || std::is_same_v<T, AttackOrdinalEqualsCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event))
                    return reject("攻擊來源條件需要攻擊、命中或傷害事件");
                if constexpr (std::is_same_v<T, AttackOrdinalEqualsCondition>)
                    if (typed.ordinal < 0) return reject("攻擊序號不可為負數");
            }
            else if constexpr (std::is_same_v<T, CastDistinctTargetCountAtLeastCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event)) return reject("不同目標數條件需要施放聚合事件");
                if (typed.count <= 0) return reject("不同目標數門檻必須為正數");
            }
            else if constexpr (std::is_same_v<T, HealKindInCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event)) return reject("治療種類條件需要治療事件");
                if (typed.kinds.empty()) return reject("治療種類條件不可為空");
            }
            else if constexpr (std::is_same_v<T, DamageOriginIsAttackCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event)) return reject("傷害來源條件需要傷害、破盾或死亡事件");
            }
            else if constexpr (std::is_same_v<T, DamageKilledTargetCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event)) return reject("擊殺條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, AcceptedHitCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event)) return reject("接受命中條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, EventTargetBelongsToBoundSourceCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event))
                    return reject("來源成員條件需要死亡事件");
            }
            else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event)) return reject("傷害方位條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, DamageKindInCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event)) return reject("傷害種類條件需要命中或傷害事件");
                if (typed.kinds.empty()) return reject("傷害種類條件不可為空");
            }
            else if constexpr (std::is_same_v<T, TargetMpWasFullBeforeCastCondition>)
            {
                if (!effectConditionAllowedAtEvent(condition.index(), event))
                {
                    return reject("施放前滿內條件需要施放規劃或提交事件");
                }
            }
            else if constexpr (std::is_same_v<T, SourceStackAtLeastCondition>)
            {
                if (typed.count <= 0) return reject("狀態層數門檻必須為正數");
            }
            return true;
        },
        condition);
}

bool validateEffectNumberAtEvent(const EffectNumber& number, EffectEvent event, std::string& error)
{
    const auto usesBase = [&](EffectNumberBase base)
    {
        return number.base == base || number.multiplierBase == base;
    };
    const bool usesStatusEffect = usesBase(EffectNumberBase::SourceStatusEffectValue);
    const bool usesStatusQuantity = usesBase(EffectNumberBase::SourceStatusQuantity);
    const bool usesStatus = usesStatusEffect || usesStatusQuantity;
    if (usesStatus != number.status.has_value())
    {
        error = usesStatus
            ? "來源狀態數值基準需要指定狀態"
            : "只有來源狀態數值基準可指定狀態";
        return false;
    }
    if (usesStatusEffect != number.statusEffect.has_value())
    {
        error = usesStatusEffect
            ? "來源狀態效果值基準需要指定效果名稱"
            : "只有來源狀態效果值基準可指定效果名稱";
        return false;
    }
    if (number.statusEffect
        && !statusEffectValueBelongsToStatus(*number.statusEffect, *number.status))
    {
        error = "來源狀態效果值名稱不屬於指定狀態";
        return false;
    }
    if (usesStatusQuantity)
    {
        const auto quantity = statusCatalogEntry(*number.status).quantity;
        if (quantity == StatusQuantityModel::None
            || quantity == StatusQuantityModel::Internal)
        {
            error = "來源狀態數量只能參照目錄中公開數量語意的狀態";
            return false;
        }
    }
    const bool usesStoredState = usesBase(EffectNumberBase::StoredStateValue);
    if (usesStoredState != number.stateSlot.has_value())
    {
        error = usesStoredState
            ? "狀態槽值基準需要指定狀態槽"
            : "只有狀態槽值基準可指定狀態槽";
        return false;
    }
    if (number.multiplierBase
        && (number.base == EffectNumberBase::Constant || *number.multiplierBase == EffectNumberBase::Constant))
    {
        error = "乘數基準只能與兩個非固定數值基準組合";
        return false;
    }
    if (number.minimum && number.maximum && *number.minimum > *number.maximum)
    {
        error = "數值最小值不可大於最大值";
        return false;
    }
    const auto needsFinalDamage = number.base == EffectNumberBase::FinalHpDamage
        || number.multiplierBase == EffectNumberBase::FinalHpDamage;
    if (needsFinalDamage
        && !effectNumberBaseAllowedAtEvent(EffectNumberBase::FinalHpDamage, event))
    {
        error = "實際生命傷害公式只能用於傷害結算事件";
        return false;
    }
    const auto needsCurrentShield = number.base == EffectNumberBase::TargetCurrentShield
        || number.multiplierBase == EffectNumberBase::TargetCurrentShield;
    if (needsCurrentShield
        && !effectNumberBaseAllowedAtEvent(EffectNumberBase::TargetCurrentShield, event))
    {
        error = "目標目前護盾公式需要命中、傷害或破盾事件";
        return false;
    }
    const auto needsCurrentCooldown = number.base == EffectNumberBase::TargetCurrentCooldown
        || number.multiplierBase == EffectNumberBase::TargetCurrentCooldown;
    if (needsCurrentCooldown
        && !effectNumberBaseAllowedAtEvent(EffectNumberBase::TargetCurrentCooldown, event))
    {
        error = "目標目前冷卻公式需要每幀或傷害結算事件";
        return false;
    }
    return true;
}

bool effectNumberCannotBeNegative(const EffectNumber& number)
{
    if (number.maximum && *number.maximum < 0)
    {
        return false;
    }
    if (number.minimum && *number.minimum >= 0)
    {
        return true;
    }

    const bool scaledValueCanBeNegative = number.base != EffectNumberBase::Constant
        && number.percent < 0;
    return !scaledValueCanBeNegative && number.flat >= 0;
}

bool effectNumberMustBePositive(const EffectNumber& number)
{
    if (const auto constant = effectiveConstantEffectNumberValue(number))
        return *constant > 0;
    if (!effectNumberCannotBeNegative(number)
        || (number.maximum && *number.maximum <= 0))
    {
        return false;
    }
    if (number.flat > 0 || (number.minimum && *number.minimum > 0)) return true;
    if (number.multiplierBase || number.percent <= 0) return false;
    if (number.base != EffectNumberBase::SourceStar) return false;
    return number.rounding == EffectRounding::Ceil || number.percent >= 100;
}

std::optional<std::int64_t> effectNumberGuaranteedMaximum(
    const EffectNumber& number)
{
    if (const auto constant = effectiveConstantEffectNumberValue(number))
        return *constant;
    if (number.maximum) return *number.maximum;
    if (number.multiplierBase) return std::nullopt;
    std::int64_t bound{};
    if (number.percent == 0)
    {
        bound = number.flat;
    }
    else
    {
        if (number.base != EffectNumberBase::SourceStar || number.percent < 0)
            return std::nullopt;
        const auto scaled = (static_cast<std::int64_t>(3) * number.percent + 99) / 100;
        bound = scaled + number.flat;
    }
    if (number.minimum) bound = std::max(bound, static_cast<std::int64_t>(*number.minimum));
    return bound;
}

bool effectNumberCannotExceed(
    const EffectNumber& number,
    int maximum)
{
    const auto bound = effectNumberGuaranteedMaximum(number);
    return bound && *bound <= maximum;
}

bool effectNumberProductFitsInt(const EffectNumber& number, int multiplier)
{
    const auto bound = effectNumberGuaranteedMaximum(number);
    return bound
        && *bound >= 0
        && *bound <= std::numeric_limits<int>::max() / multiplier;
}

bool validateStatusApplication(
    const ApplyStatusAction& action,
    EffectEvent event,
    std::string& error)
{
    const auto reject = [&](std::string message)
    {
        error = std::move(message);
        return false;
    };
    const auto& catalog = statusCatalogEntry(action.status);
    if (!catalog.authorable) return reject("執行期狀態不可由效果直接套用");

    if (catalog.duration == StatusDurationModel::RequiredPositive)
    {
        if ((action.durationFrames > 0) == action.duration.has_value())
            return reject("需要持續時間的狀態必須擇一指定固定或公式持續幀數");
        if (action.duration)
        {
            if (!validateEffectNumberAtEvent(*action.duration, event, error)) return false;
            if (!effectNumberMustBePositive(*action.duration))
                return reject("狀態持續幀數公式必須保證為正數");
        }
    }
    else if (action.durationFrames != 0 || action.duration)
    {
        return reject("此狀態不可指定持續幀數");
    }

    const auto positive = [&](int value, std::string_view field)
    {
        if (value > 0) return true;
        error = std::format("「{}」必須為正數", field);
        return false;
    };
    switch (catalog.quantity)
    {
    case StatusQuantityModel::None:
        if (!std::holds_alternative<NoStatusQuantity>(action.quantity))
            return reject("此狀態不可指定數量");
        break;
    case StatusQuantityModel::Layers:
    {
        const auto* quantity = std::get_if<AddStatusLayers>(&action.quantity);
        if (!quantity) return reject("此狀態必須使用增加層數");
        if (!positive(quantity->count, "增加層數")
            || !positive(quantity->limit, "層數上限")) return false;
        break;
    }
    case StatusQuantityModel::TriggerCharges:
    {
        const auto* quantity = std::get_if<SetStatusTriggerCharges>(&action.quantity);
        if (!quantity) return reject("此狀態必須使用可觸發次數");
        if (!positive(quantity->count, "可觸發次數")) return false;
        break;
    }
    case StatusQuantityModel::Marks:
    {
        const auto* quantity = std::get_if<SetStatusMarks>(&action.quantity);
        if (!quantity) return reject("此狀態必須使用設定印記層數");
        if (!positive(quantity->count, "設定印記層數")) return false;
        break;
    }
    case StatusQuantityModel::DamageBlockCharges:
        if (const auto* quantity = std::get_if<AddDamageBlockCharges>(&action.quantity))
        {
            if (!positive(quantity->count, "增加可抵擋次數")
                || !positive(quantity->limit, "可抵擋次數上限")) return false;
        }
        else if (const auto* quantity = std::get_if<SetDamageBlockCharges>(&action.quantity))
        {
            if (!positive(quantity->count, "設定可抵擋次數")) return false;
        }
        else return reject("傷害抵擋必須使用抵擋次數數量動詞");
        break;
    case StatusQuantityModel::Internal:
        return reject("執行期狀態不可指定數量");
    }

    if (!statusReapplicationPolicyAllowed(action.status, action.reapplication))
        return reject("狀態使用了不允許的重複套用方式");

    const bool payloadMatches = [&]
    {
        switch (action.status)
        {
        case BattleStatusKind::Poison: return std::holds_alternative<PoisonStatusEffects>(action.effects);
        case BattleStatusKind::Bleed: return std::holds_alternative<BleedStatusEffects>(action.effects);
        case BattleStatusKind::ColdPoison: return std::holds_alternative<ColdPoisonStatusEffects>(action.effects);
        case BattleStatusKind::WitheredBone: return std::holds_alternative<WitheredBoneStatusEffects>(action.effects);
        case BattleStatusKind::NeutralizeForce: return std::holds_alternative<NeutralizeForceStatusEffects>(action.effects);
        case BattleStatusKind::Blinded: return std::holds_alternative<BlindedStatusEffects>(action.effects);
        case BattleStatusKind::NextAttackMiss: return std::holds_alternative<NextIncomingAttackMissStatusEffects>(action.effects);
        case BattleStatusKind::DamageBlockLayer: return std::holds_alternative<DamageBlockStatusEffects>(action.effects);
        case BattleStatusKind::SingleHitCapLayer: return std::holds_alternative<SingleHitCapStatusEffects>(action.effects);
        case BattleStatusKind::BattleSpirit: return std::holds_alternative<BattleSpiritStatusEffects>(action.effects);
        case BattleStatusKind::TrueQi: return std::holds_alternative<TrueQiStatusEffects>(action.effects);
        case BattleStatusKind::PoisonExplosion: return std::holds_alternative<PoisonExplosionStatusEffects>(action.effects);
        case BattleStatusKind::Stun:
        case BattleStatusKind::MpBlocked:
        case BattleStatusKind::SevenStarMark:
        case BattleStatusKind::Shadowless:
            return std::holds_alternative<NoStatusEffects>(action.effects);
        case BattleStatusKind::NextAttackCritical: return false;
        }
        return false;
    }();
    if (!payloadMatches) return reject("狀態效果 payload 與狀態目錄不相符");

    bool numbersValid = true;
    forEachStatusEffectNumber(action.effects, [&](const EffectNumber& number)
    {
        if (numbersValid) numbersValid = validateEffectNumberAtEvent(number, event, error);
    });
    if (!numbersValid) return false;

    const auto requirePositiveNumber = [&](const EffectNumber& number, std::string_view field)
    {
        if (effectNumberMustBePositive(number)) return true;
        error = std::format("狀態效果「{}」必須保證為正數", field);
        return false;
    };
    const auto requireNonnegativeNumber = [&](const EffectNumber& number, std::string_view field)
    {
        if (effectNumberCannotBeNegative(number)) return true;
        error = std::format("狀態效果「{}」不可為負數", field);
        return false;
    };
    const auto requireLayerProduct = [&](const EffectNumber& number, std::string_view field)
    {
        const auto* layers = std::get_if<AddStatusLayers>(&action.quantity);
        assert(layers);
        if (effectNumberProductFitsInt(number, layers->limit)) return true;
        error = std::format(
            "狀態效果「{}」必須以公式最大值保證乘上層數上限後不超出整數範圍",
            field);
        return false;
    };

    const bool valuesValid = std::visit([&](const auto& effects)
    {
        using T = std::decay_t<decltype(effects)>;
        if constexpr (std::is_same_v<T, PoisonStatusEffects>)
            return requirePositiveNumber(effects.currentHpDamagePercent, "目前生命傷害百分比");
        else if constexpr (std::is_same_v<T, BleedStatusEffects>)
            return requirePositiveNumber(effects.maxHpDamagePercent, "最大生命傷害百分比")
                && requireLayerProduct(effects.maxHpDamagePercent, "最大生命傷害百分比");
        else if constexpr (std::is_same_v<T, ColdPoisonStatusEffects>)
            return requireNonnegativeNumber(effects.speedReductionPercent, "速度降低百分比");
        else if constexpr (std::is_same_v<T, WitheredBoneStatusEffects>)
            return requireNonnegativeNumber(
                    effects.damageTakenIncreasePercent, "受到傷害增加百分比")
                && requireNonnegativeNumber(
                    effects.healingReductionPercent, "受到治療減少百分比")
                && effectNumberCannotExceed(effects.healingReductionPercent, 100);
        else if constexpr (std::is_same_v<T, NeutralizeForceStatusEffects>)
            return requirePositiveNumber(effects.originalTargetShield, "原攻擊目標獲得護盾");
        else if constexpr (std::is_same_v<T, SingleHitCapStatusEffects>)
            return requirePositiveNumber(effects.damageCap, "傷害上限");
        else if constexpr (std::is_same_v<T, BattleSpiritStatusEffects>)
            return requireNonnegativeNumber(
                    effects.skillDamageIncreasePercent, "招式傷害增加百分比")
                && requireNonnegativeNumber(effects.damageReductionPercent, "傷害減免百分比")
                && requireLayerProduct(
                    effects.skillDamageIncreasePercent, "招式傷害增加百分比")
                && requireLayerProduct(effects.damageReductionPercent, "傷害減免百分比");
        else if constexpr (std::is_same_v<T, TrueQiStatusEffects>)
            return requirePositiveNumber(effects.pureDamagePerHit, "命中附加純粹傷害")
                && requireLayerProduct(effects.pureDamagePerHit, "命中附加純粹傷害");
        else if constexpr (std::is_same_v<T, PoisonExplosionStatusEffects>)
            return requirePositiveNumber(effects.deathPureDamage, "死亡爆炸純粹傷害")
                && requireLayerProduct(effects.deathPureDamage, "死亡爆炸純粹傷害");
        else
            return true;
    }, action.effects);
    if (!valuesValid)
    {
        if (error.empty()) error = "受到治療減少百分比必須保證介於 0 與 100";
        return false;
    }

    if (const auto* effects = std::get_if<ColdPoisonStatusEffects>(&action.effects);
        effects && !effects->blocksHealing)
        return reject("寒毒必須明確禁止受到治療");
    if (const auto* effects = std::get_if<NeutralizeForceStatusEffects>(&action.effects);
        effects && !effects->preventsCast)
        return reject("化勁必須明確阻止本次施放");
    if (const auto* effects = std::get_if<BlindedStatusEffects>(&action.effects);
        effects && !effects->preventsCast)
        return reject("刺目必須明確阻止本次施放");
    if (const auto* effects = std::get_if<NextIncomingAttackMissStatusEffects>(&action.effects);
        effects && !effects->makesIncomingAttackMiss)
        return reject("下一次受到攻擊必定落空必須明確使攻擊落空");
    if (const auto* effects = std::get_if<DamageBlockStatusEffects>(&action.effects);
        effects && !effects->blocksPositiveNonExecuteDamage)
        return reject("傷害抵擋必須明確抵擋非處決正傷害");
    if (const auto* effects = std::get_if<PoisonStatusEffects>(&action.effects))
    {
        if (action.reapplication == StatusReapplicationPolicy::KeepHigherDamage
            && effects->sameEventMerge != PoisonSameEventMerge::SumDamagePercent)
            return reject("保留較高傷害的中毒必須合計同事件傷害百分比");
        if (action.reapplication == StatusReapplicationPolicy::ReplaceAndReset
            && effects->sameEventMerge != PoisonSameEventMerge::None)
            return reject("取代並重設的中毒不可合併同事件傷害");
    }
    return true;
}

template<class Value>
bool isNonEmptyUniqueAllowList(const std::vector<Value>& values)
{
    if (values.empty())
    {
        return false;
    }
    std::set<Value> unique(values.begin(), values.end());
    return unique.size() == values.size();
}

bool validateActionPayload(const EffectAction& action, EffectEvent event, std::string& error)
{
    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            const auto reject = [&](std::string message)
            {
                error = std::move(message);
                return false;
            };
            if constexpr (std::is_same_v<T, ModifyAttributeAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (typed.durationFrames < 0) return reject("屬性修正持續幀數不可為負數");
                if (typed.stack == EffectStackPolicy::AddStack && !typed.stackLimit)
                    return reject("增加層數的屬性修正需要層數上限");
                if (typed.stackLimit && *typed.stackLimit <= 0) return reject("屬性修正層數上限必須為正數");
                if (typed.stackLimit && typed.stack != EffectStackPolicy::AddStack)
                    return reject("只有增加層數的屬性修正可指定層數上限");
                if (typed.operation == AttributeOperation::PercentAdd
                    && battleAttributeUsesPercentagePoints(typed.attribute))
                    return reject("百分點屬性必須使用百分點加算");
                if (typed.operation == AttributeOperation::PercentagePointAdd
                    && !battleAttributeUsesPercentagePoints(typed.attribute))
                    return reject("基準值屬性不可使用百分點加算");
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (typed.durationFrames < 0) return reject("傷害修正持續幀數不可為負數");
                if (typed.stack == EffectStackPolicy::AddStack && !typed.stackLimit)
                    return reject("增加層數的傷害修正需要層數上限");
                if (typed.stackLimit && *typed.stackLimit <= 0)
                    return reject("傷害修正層數上限必須為正數");
                if (typed.stackLimit && typed.stack != EffectStackPolicy::AddStack)
                    return reject("只有增加層數的傷害修正可指定層數上限");
                if (typed.operation == DamageModifierOperation::IgnoreDefensePercent
                    && (typed.perspective != DamageModifierPerspective::Outgoing
                        || typed.stage != DamageModifierStage::BeforeDefense))
                    return reject("忽略防禦百分比只支援造成方的防禦前階段");
                if (typed.operation == DamageModifierOperation::CapSingleHitAtMaxHpPercent
                    && (typed.perspective != DamageModifierPerspective::Incoming
                        || typed.stage != DamageModifierStage::Final
                        || typed.channel != DamageChannel::All))
                    return reject("單次承傷上限只支援承受方、最終階段與全部傷害");
                if (typed.operation == DamageModifierOperation::CapSingleHitAtMaxHpPercent
                    && !effectNumberMustBePositive(typed.amount))
                    return reject("每次承傷最大生命百分比必須保證為正數");
                if (typed.operation == DamageModifierOperation::ExecuteBelowMaxHpPercent
                    && (typed.perspective != DamageModifierPerspective::Outgoing
                        || typed.stage != DamageModifierStage::Final))
                    return reject("處決門檻只支援造成方的最終階段");
                if (typed.operation == DamageModifierOperation::Multiply
                    && typed.perspective == DamageModifierPerspective::Incoming
                    && typed.stage == DamageModifierStage::BeforeDefense)
                    return reject("承受方的防禦前階段不支援乘算傷害修正");
            }
            else if constexpr (std::is_same_v<T, ChangeResourceAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (!effectNumberCannotBeNegative(typed.amount))
                    return reject("資源變更數值不可產生負數");
                if ((typed.kind == ResourceChangeKind::Transfer) != typed.transferDestination.has_value())
                    return reject("只有轉移資源需要且必須提供轉移目標");
                if (typed.kind == ResourceChangeKind::RefreshToAtLeast
                    && typed.resource != BattleResource::Shield
                    && typed.resource != BattleResource::StatusShield
                    && typed.resource != BattleResource::StaggerShield
                    && typed.resource != BattleResource::ControlImmunityFrames
                    && typed.resource != BattleResource::InvincibilityFrames)
                    return reject("至少刷新只支援護盾、狀態護盾、僵直護盾、僵直吸收幀數或無敵幀數");
                if (typed.resource != BattleResource::Hp
                    && (typed.healKind != EffectHealKind::Direct
                        || typed.healSourcePolicy != EffectHealSourcePolicy::RequireAlive))
                    return reject("非生命資源不可指定治療種類或來源政策");
                if (typed.transferDestination)
                {
                    if (!validateSelectorSchema(*typed.transferDestination, error)
                        || !validateSelectorAtEvent(*typed.transferDestination, event, error)) return false;
                    if (!selectorHasSingleResult(*typed.transferDestination))
                        return reject("轉移目標選擇器必須只選取一個單位");
                }
            }
            else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                if (typed.kinds.empty()) return reject("治療交易修正需要治療種類");
                if (typed.operation == HealModifierOperation::MultiplyReceived
                    && (typed.percent < 0 || typed.percent > 100)) return reject("治療乘算百分比必須介於 0 與 100");
            }
            else if constexpr (std::is_same_v<T, ApplyStatusAction>)
            {
                return validateStatusApplication(typed, event, error);
            }
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
            {
                if (typed.quantity <= 0) return reject("消耗狀態數量必須為正數");
                if (typed.whenDepleted)
                {
                    EffectAction nested{ EffectActionValue{ *typed.whenDepleted } };
                    if (!validateActionPayload(nested, event, error)) return false;
                }
            }
            else if constexpr (std::is_same_v<T, RemoveStatusAction>)
            {
                if (typed.statuses.empty()
                    && !typed.negativeOnly
                    && !typed.controlOnly
                    && !typed.clearCurrentActionStagger)
                    return reject("移除狀態需要狀態列表或篩選條件");
                if (typed.count < 0) return reject("移除狀態數量不可為負數");
            }
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, error)) return false;
                if (typed.transactionCount
                    && (!validateEffectNumberAtEvent(*typed.transactionCount, event, error)
                        || !effectNumberCannotBeNegative(*typed.transactionCount))) return false;
                if (typed.area.kind == DamageAreaKind::Circle && typed.area.radiusTiles <= 0)
                    return reject("圓形傷害範圍需要正半徑");
                if (typed.area.kind == DamageAreaKind::Square
                    && (typed.area.squareSideTiles <= 0 || typed.area.squareSideTiles % 2 == 0))
                    return reject("方形傷害範圍邊長必須是正奇數");
                if (typed.perCast.perTargetLimit < 0) return reject("同目標命中上限不可為負數");
                if (typed.areaProjectiles)
                {
                    const auto& delivery = *typed.areaProjectiles;
                    if (delivery.rangeTiles <= 0)
                        return reject("區域投射物範圍格數必須為正數");
                    if (delivery.maximumTargets < 0)
                        return reject("區域投射物最多目標不可為負數");
                    if (delivery.stunFrames < 0)
                        return reject("區域投射物眩暈幀數不可為負數");
                    if (typed.area.kind != DamageAreaKind::SingleTarget)
                        return reject("區域投射物不可再指定立即傷害範圍");
                    if (typed.perCast.perTargetLimit != 0)
                        return reject("區域投射物不可指定同施放命中上限");
                }
            }
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
            {
                if (typed.pattern.projectileCount <= 0) return reject("攻擊彈道數量必須為正數");
                if (typed.pattern.intervalFrames < 0) return reject("攻擊間隔幀數不可為負數");
                if (typed.strengthPct < 0 || typed.sameTargetHitLimit < 0) return reject("攻擊倍率與同目標上限不可為負數");
                if (typed.source)
                {
                    if (!validateSelectorSchema(*typed.source, error)
                        || !validateSelectorAtEvent(*typed.source, event, error)) return false;
                    if (!selectorHasSingleResult(*typed.source))
                        return reject("攻擊來源選擇器必須只選取一個單位");
                }
                if (typed.damageOverride && !validateEffectNumberAtEvent(*typed.damageOverride, event, error)) return false;
                if (!std::visit(
                        [&](const auto& behavior) -> bool
                        {
                            using B = std::decay_t<decltype(behavior)>;
                            if constexpr (std::is_same_v<B, std::monostate>)
                            {
                                return true;
                            }
                            else if constexpr (std::is_same_v<B, ProjectileBounceAttackBehavior>)
                            {
                                if (behavior.additionalHits > 0
                                    && behavior.chancePct >= 0
                                    && behavior.chancePct <= 100
                                    && behavior.rangePixels > 0) return true;
                                return reject("彈道彈射需要正追加命中次數、0 至 100 機率與正像素範圍");
                            }
                            else if constexpr (std::is_same_v<B, NearbyTrackingAttackBehavior>)
                            {
                                if (behavior.rangePixels > 0 && behavior.damagePct > 0) return true;
                                return reject("範圍追蹤需要正像素範圍與正傷害倍率");
                            }
                            else if constexpr (std::is_same_v<B, DelayedAlternateAttackBehavior>)
                            {
                                if (behavior.delayFrames > 0
                                    && behavior.damagePct > 0
                                    && behavior.attackerBlockGainChancePct >= 0
                                    && behavior.attackerBlockGainChancePct <= 100) return true;
                                return reject("延遲替代攻擊需要正延遲、正傷害倍率與 0 至 100 格擋機率");
                            }
                            else if constexpr (std::is_same_v<B, ExpandingSpiralAttackBehavior>)
                            {
                                if (behavior.projectileCount > 0 && behavior.bleedStacks > 0) return true;
                                return reject("擴張螺旋需要正彈道數量與正流血層數");
                            }
                        },
                        typed.runtimeBehavior)) return false;
            }
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
            {
                if (typed.distanceTiles < 0 || typed.distancePixels < 0
                    || (typed.distanceTiles > 0) == (typed.distancePixels > 0))
                    return reject("強制移動必須且只能指定一種正距離");
                if (typed.lockFrames <= 0) return reject("強制移動鎖定幀數必須為正數");
            }
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
            {
                if (typed.durationFrames <= 0 || typed.modifiers.empty()) return reject("區域需要正持續時間與非空修正");
                if (typed.shape == AreaShape::Circle && typed.radiusTiles <= 0) return reject("圓形區域需要正半徑");
                if (typed.shape == AreaShape::GridSquare
                    && (typed.squareSideTiles <= 0 || typed.squareSideTiles % 2 == 0))
                    return reject("棋格方形區域邊長必須是正奇數");
                for (const auto& modifier : typed.modifiers)
                    if (!validateEffectNumberAtEvent(modifier.amount, event, error)) return false;
            }
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
            {
                if (!typed.mpCost
                    && !typed.rangeMode
                    && typed.projectileSpeedPct == 0
                    && typed.minimumSelectDistance == 0
                    && typed.additionalProjectiles == 0
                    && typed.mobility == CastMobilityPolicy::Preserve
                    && !typed.autoUltimate
                    && !typed.replacementPattern
                    && !typed.freeAdditionalCast)
                    return reject("修改施放至少需要一項變更");
                if (typed.mpCost && !validateEffectNumberAtEvent(*typed.mpCost, event, error)) return false;
                if (typed.projectileSpeedPct < 0
                    || typed.minimumSelectDistance < 0
                    || typed.additionalProjectiles < 0)
                    return reject("施放彈道速度、最小選擇距離與追加彈道數不可為負數");
            }
            else if constexpr (std::is_same_v<T, StateMachineAction>)
            {
                return std::visit(
                    [&](const auto& machine) -> bool
                    {
                        using M = std::decay_t<decltype(machine)>;
                        if constexpr (std::is_same_v<M, ChangeStateValueAction>)
                        {
                            if (machine.delta == 0) return reject("狀態值增量不可為零");
                            if (machine.minimum && machine.maximum
                                && *machine.minimum > *machine.maximum)
                                return reject("狀態值最小值不可大於最大值");
                        }
                        else if constexpr (std::is_same_v<M, TransferStateValueAction>)
                        {
                            if (machine.sourceSlot == machine.destinationSlot)
                                return reject("狀態值轉移的來源與目標狀態槽不可相同");
                            if ((machine.sourceSlot == EffectStateSlot::CastMaximumHpDamage
                                    || machine.destinationSlot == EffectStateSlot::CastMaximumHpDamage)
                                && !effectEventHas(event, EffectEventCapability::CastProvenance))
                                return reject("本次施放狀態槽需要具有施放識別的事件");
                        }
                        else if constexpr (std::is_same_v<M, ConsumeRecordedMaximumAction>)
                        {
                            if (machine.percent < 0) return reject("消耗記錄百分比不可為負數");
                        }
                        else if constexpr (std::is_same_v<M, StartDamageAbsorptionAction>)
                        {
                            if (machine.absorbedPct < 0 || machine.absorbedPct > 100 || machine.durationFrames <= 0)
                                return reject("傷害吸收需要 0 至 100 百分比與正持續時間");
                            if (machine.returnedPct < 0)
                                return reject("傷害吸收結算百分比不可為負數");
                            if (!validateSelectorSchema(machine.settlementTarget, error)
                                || !validateSelectorAtEvent(machine.settlementTarget, event, error)) return false;
                        }
                        else if constexpr (std::is_same_v<M, SettleDamageAbsorptionAction>)
                        {
                            if (machine.returnedPct < 0) return reject("傷害吸收結算百分比不可為負數");
                            if (!validateSelectorSchema(machine.target, error)
                                || !validateSelectorAtEvent(machine.target, event, error)) return false;
                        }
                        else if constexpr (std::is_same_v<M, BorrowEffectRulesAction>)
                        {
                            if (machine.propagation != CastPropagationPolicy::BorrowedUltimateRules)
                                return reject("借用效果規則必須使用借用大招規則傳播政策");
                            if (!validateEffectNumberAtEvent(machine.sourceCount, event, error)
                                || !validateSelectorSchema(machine.sourceUnits, error)
                                || !validateSelectorAtEvent(machine.sourceUnits, event, error)) return false;
                            if (!effectNumberCannotBeNegative(machine.sourceCount))
                                return reject("借用效果規則的來源數量不可產生負數");
                            if (!isNonEmptyUniqueAllowList(
                                    machine.filter.allowedActionCategories))
                                return reject("借用效果規則需要非空且不重複的動作類別 allow-list");
                        }
                        else if constexpr (std::is_same_v<M, CopyAttackDefinitionAction>)
                        {
                            if (machine.propagation != CastPropagationPolicy::SuppressUltimateRules)
                                return reject("複製攻擊定義必須使用不傳播大招規則傳播政策");
                            if (!machine.sourceUnits.excludeOwner)
                                return reject("複製攻擊定義的來源選擇器必須排除效果擁有者");
                            if (machine.copyCount <= 0) return reject("複製攻擊數量必須為正數");
                            if (!validateSelectorSchema(machine.sourceUnits, error)
                                || !validateSelectorAtEvent(machine.sourceUnits, event, error)) return false;
                            if (!isNonEmptyUniqueAllowList(machine.filter.conditions))
                                return reject("複製攻擊定義需要非空且不重複的可選武功條件 allow-list");
                            if (!std::ranges::contains(
                                    machine.filter.conditions,
                                    CopiedMagicCondition::HasUltimateAttackDefinition))
                                return reject("複製攻擊定義必須限制為有絕招攻擊定義的武功");
                            if (!std::ranges::contains(
                                    machine.filter.conditions,
                                    CopiedMagicCondition::ExcludesRecursiveEffects))
                                return reject("複製攻擊定義必須排除複製與借用遞迴");
                        }
                        else if constexpr (std::is_same_v<M, SettleRemainingStatusDamageAction>)
                        {
                            if (machine.status != BattleStatusKind::Poison)
                                return reject("剩餘狀態傷害結算目前只支援中毒");
                        }
                        else if constexpr (std::is_same_v<M, GenerateClonesAction>)
                        {
                            if (machine.count <= 0) return reject("生成分身數量必須為正數");
                        }
                        else if constexpr (std::is_same_v<M, PreventDeathAction>)
                        {
                            if (machine.invincibilityFrames <= 0)
                                return reject("死亡庇護無敵幀數必須為正數");
                        }
                        else if constexpr (std::is_same_v<M, ConfigureRescueRepositionAction>)
                        {
                            if (machine.activations <= 0) return reject("挪移次數必須為正數");
                        }
                        return true;
                    },
                    typed);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
            {
                if (!typed || typed->conditions.empty() || typed->whenTrue.empty())
                    return reject("條件分支需要條件與成立動作");
                for (const auto& condition : typed->conditions)
                    if (!validateConditionAtEvent(condition, event, error)) return false;
                for (const auto& nested : typed->whenTrue)
                    if (!validateActionPayload(nested, event, error)) return false;
                for (const auto& nested : typed->whenFalse)
                    if (!validateActionPayload(nested, event, error)) return false;
            }
            return true;
        },
        action.value);
}

bool isActionAllowedAtEvent(const EffectAction& action, EffectEvent event, std::string& error)
{
    const auto reject = [&](std::string message)
    {
        error = std::move(message);
        return false;
    };

    if (!effectActionAllowedAtEvent(action.value.index(), event))
    {
        switch (action.value.index())
        {
        case 1: return reject("傷害修正不支援此事件");
        case 3: return reject("治療交易修正只能用於治療嘗試事件");
        case 7: return reject("造成傷害不支援此事件");
        case 8: return reject("修改攻擊不支援此事件");
        case 9: return reject("強制移動只允許命中傷害前事件");
        case 10: return reject("建立區域不支援此事件");
        case 11: return reject("修改施放不支援此事件");
        default: return reject("治療嘗試事件只允許治療交易修正");
        }
    }

    if (const auto* machine = std::get_if<StateMachineAction>(&action.value);
        machine && !effectStateMachineActionAllowedAtEvent(machine->index(), event))
    {
        switch (machine->index())
        {
        case 2: return reject("記錄最大招式生命傷害只允許用於傷害結算事件");
        case 6: return reject("借用效果規則只允許用於施放規劃事件");
        case 7: return reject("複製攻擊定義只允許用於絕招提交事件");
        case 9: return reject("生成分身只允許用於戰鬥初始化事件");
        case 10: return reject("死亡庇護只允許用於戰鬥初始化事件");
        case 11: return reject("挪移次數只允許用於戰鬥初始化事件");
        default: return reject("狀態機動作不支援此事件");
        }
    }

    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, ModifyCastAction>)
            {
                if (event == EffectEvent::CastPlanned) return true;
                if (event == EffectEvent::CastContinuation
                    && typed.freeAdditionalCast
                    && !typed.mpCost
                    && !typed.rangeMode
                    && typed.projectileSpeedPct == 0
                    && typed.minimumSelectDistance == 0
                    && typed.additionalProjectiles == 0
                    && typed.mobility == CastMobilityPolicy::Preserve
                    && !typed.autoUltimate
                    && !typed.replacementPattern)
                {
                    return true;
                }
                if ((event == EffectEvent::FrameAdvanced || event == EffectEvent::ShieldBroken)
                    && typed.autoUltimate
                    && !typed.mpCost
                    && !typed.rangeMode
                    && typed.projectileSpeedPct == 0
                    && typed.minimumSelectDistance == 0
                    && typed.additionalProjectiles == 0
                    && typed.mobility == CastMobilityPolicy::Preserve
                    && !typed.replacementPattern
                    && !typed.freeAdditionalCast)
                {
                    return true;
                }
                return reject("修改施放只能用於施放規劃；施放延續只允許免費 child cast；每幀與護盾破裂只允許自動絕招");
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
            {
                if (!typed) return reject("條件動作不可為空");
                for (const auto& nested : typed->whenTrue)
                    if (!isActionAllowedAtEvent(nested, event, error)) return false;
                for (const auto& nested : typed->whenFalse)
                    if (!isActionAllowedAtEvent(nested, event, error)) return false;
                return true;
            }
            else
            {
                return true;
            }
        },
        action.value);
}

enum class ExactRuntimeActionClass
{
    Ordinary,
    Exact,
    UnsupportedComposition,
};

ExactRuntimeActionClass exactRuntimeActionClass(const EffectAction& action)
{
    return std::visit([&](const auto& typed)
    {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, ModifyCastAction>)
        {
            const bool exact = typed.rangeMode == CastRangeMode::Ranged
                || typed.projectileSpeedPct > 0
                || typed.minimumSelectDistance > 0
                || typed.additionalProjectiles > 0
                || typed.mobility != CastMobilityPolicy::Preserve;
            if (!exact)
            {
                return ExactRuntimeActionClass::Ordinary;
            }
            const bool hasOrdinaryFields = typed.mpCost.has_value()
                || typed.autoUltimate.has_value()
                || typed.replacementPattern.has_value()
                || typed.freeAdditionalCast
                || typed.propagation != CastPropagationPolicy::SourceRules;
            return hasOrdinaryFields
                ? ExactRuntimeActionClass::UnsupportedComposition
                : ExactRuntimeActionClass::Exact;
        }
        else if constexpr (std::is_same_v<T, ModifyAttackAction>)
        {
            const bool exact = !std::holds_alternative<std::monostate>(typed.runtimeBehavior)
                && !std::holds_alternative<ExpandingSpiralAttackBehavior>(typed.runtimeBehavior);
            if (!exact)
            {
                return ExactRuntimeActionClass::Ordinary;
            }
            return hasOrdinaryAttackModification(typed)
                ? ExactRuntimeActionClass::UnsupportedComposition
                : ExactRuntimeActionClass::Exact;
        }
        else if constexpr (std::is_same_v<T, ForceMoveAction>)
        {
            return typed.distancePixels > 0
                ? ExactRuntimeActionClass::Exact
                : ExactRuntimeActionClass::Ordinary;
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
        {
            assert(typed);
            const auto containsExact = [&](const auto& actions)
            {
                return std::ranges::any_of(actions, [](const EffectAction& nested)
                {
                    return exactRuntimeActionClass(nested)
                        != ExactRuntimeActionClass::Ordinary;
                });
            };
            return containsExact(typed->whenTrue) || containsExact(typed->whenFalse)
                ? ExactRuntimeActionClass::UnsupportedComposition
                : ExactRuntimeActionClass::Ordinary;
        }
        else
        {
            return ExactRuntimeActionClass::Ordinary;
        }
    }, action.value);
}

bool exactRuntimeActionConsumesRuleChance(const EffectAction& action)
{
    if (const auto* movement = std::get_if<ForceMoveAction>(&action.value))
    {
        return movement->distancePixels > 0;
    }
    const auto* attack = std::get_if<ModifyAttackAction>(&action.value);
    return attack
        && std::holds_alternative<NearbyTrackingAttackBehavior>(
            attack->runtimeBehavior);
}

bool validateBattleInitializedNumber(const EffectNumber& number, std::string& error)
{
    const auto supported = [](EffectNumberBase base)
    {
        switch (base)
        {
        case EffectNumberBase::Constant:
        case EffectNumberBase::SourceStar:
        case EffectNumberBase::SourceAttack:
        case EffectNumberBase::SourceMaxHp:
        case EffectNumberBase::SourceCurrentMpRatio:
        case EffectNumberBase::TargetMaxHp:
        case EffectNumberBase::TargetCurrentHp:
            return true;
        case EffectNumberBase::SourceMissingHpRatio:
        case EffectNumberBase::TargetCurrentShield:
        case EffectNumberBase::TargetCurrentCooldown:
        case EffectNumberBase::FinalHpDamage:
        case EffectNumberBase::SourceStatusEffectValue:
        case EffectNumberBase::SourceStatusQuantity:
        case EffectNumberBase::StoredStateValue:
            return false;
        }
        return false;
    };
    if (!supported(number.base)
        || (number.multiplierBase && !supported(*number.multiplierBase)))
    {
        error = "戰鬥初始化數值公式使用了初始化快照未提供的資料";
        return false;
    }
    return true;
}

bool validateBattleInitializedSelector(const EffectSelector& selector, std::string& error)
{
    if (selector.tieBreak != EffectTieBreak::UnitId)
    {
        error = "戰鬥初始化目標選擇必須使用單位 ID 平手規則";
        return false;
    }
    if (selector.requiredMartialCategory != EffectMartialCategory::None)
    {
        error = "戰鬥初始化快照不提供武學類別篩選資料";
        return false;
    }
    if ((selector.kind == EffectSelectorKind::Self
            || selector.kind == EffectSelectorKind::SourceUnit)
        && selector.excludeOwner)
    {
        error = "戰鬥初始化的自身或來源單位不可排除效果擁有者";
        return false;
    }
    switch (selector.kind)
    {
    case EffectSelectorKind::Self:
    case EffectSelectorKind::SourceUnit:
    case EffectSelectorKind::ComboMembers:
    case EffectSelectorKind::AllLivingUnits:
    case EffectSelectorKind::Allies:
    case EffectSelectorKind::Enemies:
    case EffectSelectorKind::LowestHpAllies:
    case EffectSelectorKind::LowestMpAllies:
    case EffectSelectorKind::HighestMpEnemy:
    case EffectSelectorKind::StrongestEnemies:
    case EffectSelectorKind::NearestEnemies:
    case EffectSelectorKind::FarthestEnemy:
    case EffectSelectorKind::UnitsInRadius:
    case EffectSelectorKind::UnitsInSquare:
        return true;
    case EffectSelectorKind::TransactionTarget:
    case EffectSelectorKind::HitTarget:
    case EffectSelectorKind::OriginalAttackTarget:
    case EffectSelectorKind::AlliesUsingMartialCategory:
        error = "戰鬥初始化不支援此目標選擇器";
        return false;
    }
    return false;
}

bool validateBattleInitializedCondition(const EffectCondition& condition, std::string& error)
{
    if (std::holds_alternative<SourceIsLastAliveCondition>(condition)
        || std::holds_alternative<TargetNotInvincibleCondition>(condition))
    {
        return true;
    }
    error = "戰鬥初始化條件使用了初始化事件未提供或固定不變的資料";
    return false;
}

bool initializedCoreAttribute(BattleAttribute attribute)
{
    return attribute == BattleAttribute::MaxHp
        || attribute == BattleAttribute::Attack
        || attribute == BattleAttribute::Defence
        || attribute == BattleAttribute::Speed;
}

bool validateBattleInitializedAction(
    const EffectAction& action,
    const EffectSelector& ruleSelector,
    std::string& error)
{
    const auto reject = [&](std::string message)
    {
        error = std::move(message);
        return false;
    };
    return std::visit([&](const auto& typed) -> bool
    {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, ModifyAttributeAction>)
        {
            if (!validateBattleInitializedNumber(typed.amount, error)) return false;
            if (initializedCoreAttribute(typed.attribute)
                && (typed.operation != AttributeOperation::FlatAdd
                    && typed.operation != AttributeOperation::PercentAdd))
                return reject("初始化核心屬性只支援固定加算或百分比加算");
            if (initializedCoreAttribute(typed.attribute)
                && (typed.durationFrames != 0
                    || typed.stack != EffectStackPolicy::Independent
                    || typed.stackLimit
                    || typed.stackScope != EffectStackScope::Shared))
                return reject("初始化核心屬性必須是永久、共用且獨立的修正");
            return true;
        }
        else if constexpr (std::is_same_v<T, ModifyDamageAction>)
        {
            return validateBattleInitializedNumber(typed.amount, error);
        }
        else if constexpr (std::is_same_v<T, ChangeResourceAction>)
        {
            if (!validateBattleInitializedNumber(typed.amount, error)) return false;
            if (typed.kind == ResourceChangeKind::Drain
                || typed.kind == ResourceChangeKind::Transfer)
                return reject("戰鬥初始化不支援奪取或轉移資源");
            if (typed.resource == BattleResource::Hp
                || typed.resource == BattleResource::ActiveCooldown)
                return reject("戰鬥初始化不可變更生命或目前冷卻");
            return true;
        }
        else if constexpr (std::is_same_v<T, ApplyStatusAction>)
        {
            bool valid = true;
            forEachStatusEffectNumber(typed.effects, [&](const EffectNumber& number)
            {
                if (valid) valid = validateBattleInitializedNumber(number, error);
            });
            if (!valid) return false;
            if (typed.duration
                && !validateBattleInitializedNumber(*typed.duration, error))
                return false;
            return true;
        }
        else if constexpr (std::is_same_v<T, StateMachineAction>)
        {
            return std::visit([&](const auto& machine) -> bool
            {
                using M = std::decay_t<decltype(machine)>;
                if constexpr (std::is_same_v<M, GenerateClonesAction>)
                {
                    return ruleSelector.kind == EffectSelectorKind::Self
                        || reject("生成分身的戰鬥初始化規則必須以自身為目標");
                }
                else if constexpr (std::is_same_v<M, PreventDeathAction>
                    || std::is_same_v<M, ConfigureRescueRepositionAction>)
                {
                    return true;
                }
                else
                {
                    return reject("戰鬥初始化不支援此狀態機機制");
                }
            }, typed);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
        {
            assert(typed);
            for (const auto& condition : typed->conditions)
                if (!validateBattleInitializedCondition(condition, error)) return false;
            for (const auto& nested : typed->whenTrue)
                if (!validateBattleInitializedAction(nested, ruleSelector, error)) return false;
            for (const auto& nested : typed->whenFalse)
                if (!validateBattleInitializedAction(nested, ruleSelector, error)) return false;
            return true;
        }
        else
        {
            return reject("戰鬥初始化不支援此動作類型");
        }
    }, action.value);
}

bool validateBattleInitializedRule(const EffectRule& rule, std::string& error)
{
    if (rule.observation != EffectObservationScope::Owner)
    {
        error = "戰鬥初始化只支援效果擁有者觀察";
        return false;
    }
    if (rule.castMatch != EffectCastMatch::BoundMagic)
    {
        error = "戰鬥初始化不提供施放識別";
        return false;
    }
    if (rule.chancePct != 100
        || rule.maxActivations != 0
        || rule.sharedCooldownFrames != 0
        || rule.intervalFrames != 0
        || rule.everyNthEvent != 0
        || rule.repetitionCount
        || rule.activationLimit)
    {
        error = "戰鬥初始化必須省略機率、次數、冷卻、間隔、重複與觸發限制欄位";
        return false;
    }
    if (!validateBattleInitializedSelector(rule.selector, error)) return false;
    for (const auto& condition : rule.conditions)
        if (!validateBattleInitializedCondition(condition, error)) return false;
    for (const auto& action : rule.actions)
        if (!validateBattleInitializedAction(action, rule.selector, error)) return false;
    return true;
}

template <typename Visitor>
void visitEffectActionTree(const EffectAction& action, Visitor&& visitor)
{
    visitor(action);
    const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
        &action.value);
    if (!conditional) return;
    assert(*conditional);
    for (const auto& nested : (*conditional)->whenTrue)
        visitEffectActionTree(nested, visitor);
    for (const auto& nested : (*conditional)->whenFalse)
        visitEffectActionTree(nested, visitor);
}

template <typename>
inline constexpr bool unsupportedEffectNumberCarrier = false;

template <typename Visitor>
void visitApplyStatusEffectNumbers(
    const ApplyStatusAction& action,
    Visitor& visitor)
{
    if (action.duration) visitor(*action.duration);
    forEachStatusEffectNumber(action.effects, visitor);
}

template <typename Visitor>
void visitEffectActionNumbers(const EffectAction& action, Visitor& visitor)
{
    std::visit([&](const auto& typed)
    {
        using T = std::decay_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, ModifyAttributeAction>
            || std::is_same_v<T, ModifyDamageAction>
            || std::is_same_v<T, ChangeResourceAction>)
        {
            visitor(typed.amount);
        }
        else if constexpr (std::is_same_v<T, ApplyStatusAction>)
        {
            visitApplyStatusEffectNumbers(typed, visitor);
        }
        else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
        {
            if (typed.whenDepleted)
                visitApplyStatusEffectNumbers(*typed.whenDepleted, visitor);
        }
        else if constexpr (std::is_same_v<T, DealDamageAction>)
        {
            visitor(typed.amount);
            if (typed.transactionCount) visitor(*typed.transactionCount);
        }
        else if constexpr (std::is_same_v<T, ModifyAttackAction>)
        {
            if (typed.damageOverride) visitor(*typed.damageOverride);
        }
        else if constexpr (std::is_same_v<T, CreateAreaAction>)
        {
            for (const auto& modifier : typed.modifiers) visitor(modifier.amount);
        }
        else if constexpr (std::is_same_v<T, ModifyCastAction>)
        {
            if (typed.mpCost) visitor(*typed.mpCost);
        }
        else if constexpr (std::is_same_v<T, StateMachineAction>)
        {
            std::visit([&](const auto& stateAction)
            {
                using S = std::decay_t<decltype(stateAction)>;
                if constexpr (std::is_same_v<S, BorrowEffectRulesAction>)
                {
                    visitor(stateAction.sourceCount);
                }
                else if constexpr (std::is_same_v<S, ChangeStateValueAction>
                    || std::is_same_v<S, TransferStateValueAction>
                    || std::is_same_v<S, RecordMaximumDamageAction>
                    || std::is_same_v<S, ConsumeRecordedMaximumAction>
                    || std::is_same_v<S, StartDamageAbsorptionAction>
                    || std::is_same_v<S, SettleDamageAbsorptionAction>
                    || std::is_same_v<S, CopyAttackDefinitionAction>
                    || std::is_same_v<S, SettleRemainingStatusDamageAction>
                    || std::is_same_v<S, GenerateClonesAction>
                    || std::is_same_v<S, PreventDeathAction>
                    || std::is_same_v<S, ConfigureRescueRepositionAction>)
                {
                }
                else
                {
                    static_assert(unsupportedEffectNumberCarrier<S>,
                        "新的狀態機動作必須明確宣告 EffectNumber 走訪方式");
                }
            }, typed);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
        {
            assert(typed);
            for (const auto& nested : typed->whenTrue)
                visitEffectActionNumbers(nested, visitor);
            for (const auto& nested : typed->whenFalse)
                visitEffectActionNumbers(nested, visitor);
        }
        else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>
            || std::is_same_v<T, RemoveStatusAction>
            || std::is_same_v<T, ForceMoveAction>)
        {
        }
        else
        {
            static_assert(unsupportedEffectNumberCarrier<T>,
                "新的效果動作必須明確宣告 EffectNumber 走訪方式");
        }
    }, action.value);
}

bool effectNumberUsesStatusQuantity(
    const EffectNumber& number,
    BattleStatusKind status)
{
    return number.status == status
        && (number.base == EffectNumberBase::SourceStatusQuantity
            || number.multiplierBase == EffectNumberBase::SourceStatusQuantity);
}

bool effectNumberUsesStatusEffect(
    const EffectNumber& number,
    BattleStatusKind status,
    StatusEffectValueKind effect)
{
    return number.status == status
        && number.statusEffect == effect
        && (number.base == EffectNumberBase::SourceStatusEffectValue
            || number.multiplierBase == EffectNumberBase::SourceStatusEffectValue);
}

bool validateExplicitStatusLifecycles(
    std::span<const EffectRule> rules,
    std::string& error)
{
    int sevenStarProducers{};
    int sevenStarConsumers{};
    int poisonExplosionProducers{};
    int poisonExplosionConsumers{};

    for (const auto& rule : rules)
    {
        bool hasPoisonExplosionQuantityReference{};
        bool hasPoisonExplosionValueReference{};
        const auto inspectNumber = [&](const EffectNumber& number)
        {
            hasPoisonExplosionQuantityReference = hasPoisonExplosionQuantityReference
                || effectNumberUsesStatusQuantity(
                    number, BattleStatusKind::PoisonExplosion);
            hasPoisonExplosionValueReference = hasPoisonExplosionValueReference
                || effectNumberUsesStatusEffect(
                    number,
                    BattleStatusKind::PoisonExplosion,
                    StatusEffectValueKind::PoisonExplosionDeathPureDamage);
        };
        if (rule.repetitionCount) inspectNumber(*rule.repetitionCount);
        for (const auto& action : rule.actions)
        {
            visitEffectActionTree(action, [&](const EffectAction& visited)
            {
                if (const auto* apply = std::get_if<ApplyStatusAction>(&visited.value))
                {
                    sevenStarProducers += apply->status == BattleStatusKind::SevenStarMark;
                    poisonExplosionProducers += apply->status
                        == BattleStatusKind::PoisonExplosion;
                }
                if (const auto* consume = std::get_if<ConsumeStatusAction>(&visited.value))
                {
                    if (consume->status == BattleStatusKind::SevenStarMark)
                        ++sevenStarConsumers;
                    if (consume->whenDepleted)
                    {
                        sevenStarProducers += consume->whenDepleted->status
                            == BattleStatusKind::SevenStarMark;
                        poisonExplosionProducers += consume->whenDepleted->status
                            == BattleStatusKind::PoisonExplosion;
                    }
                }
            });
            visitEffectActionNumbers(action, inspectNumber);
        }

        if (hasPoisonExplosionQuantityReference || hasPoisonExplosionValueReference)
            ++poisonExplosionConsumers;
    }

    const auto validateLifecycle = [&](std::string_view status,
                                       int producers,
                                       int consumers,
                                       int compatibleProducers,
                                       int compatibleConsumers)
    {
        if (producers == 0 && consumers == 0) return true;
        if (producers != 1 || compatibleProducers != 1)
        {
            error = std::format(
                "狀態「{}」的顯式生命週期必須有且只有一個相容 producer；目前為 {} 個",
                status,
                producers);
            return false;
        }
        if (consumers != 1 || compatibleConsumers != 1)
        {
            error = std::format(
                "狀態「{}」的顯式生命週期必須有且只有一個相容 consumer；目前為 {} 個",
                status,
                consumers);
            return false;
        }
        return true;
    };

    const int compatibleSevenStarProducers = static_cast<int>(std::ranges::count_if(
        rules, matchesSevenStarLifecycleProducer));
    const int compatibleSevenStarConsumers = static_cast<int>(std::ranges::count_if(
        rules, matchesSevenStarLifecycleConsumer));
    const int compatiblePoisonExplosionProducers = static_cast<int>(
        std::ranges::count_if(rules, matchesPoisonExplosionLifecycleProducer));
    const int compatiblePoisonExplosionConsumers = static_cast<int>(
        std::ranges::count_if(rules, matchesPoisonExplosionLifecycleConsumer));
    return validateLifecycle(
            "七星",
            sevenStarProducers,
            sevenStarConsumers,
            compatibleSevenStarProducers,
            compatibleSevenStarConsumers)
        && validateLifecycle(
            "毒爆",
            poisonExplosionProducers,
            poisonExplosionConsumers,
            compatiblePoisonExplosionProducers,
            compatiblePoisonExplosionConsumers);
}


}  // namespace

bool validateEffectRule(const EffectRule& rule, std::string& error)
{
    error.clear();
    if (isIntrinsicEffectRuleId(rule.id))
    {
        error = "作者規則 ID 不可使用保留的內建狀態識別空間";
        return false;
    }
    if (rule.castMatch == EffectCastMatch::OwnerAnyCast
        && rule.observation != EffectObservationScope::Owner)
    {
        error = "效果擁有者任意施放匹配只支援效果擁有者觀察";
        return false;
    }
    if (rule.castMatch == EffectCastMatch::OwnerAnyCast
        && !effectEventHas(rule.event, EffectEventCapability::CastProvenance))
    {
        error = "效果擁有者任意施放匹配需要具有施放識別的事件";
        return false;
    }
    if (rule.observation == EffectObservationScope::OwnerTeamEventSource
        && rule.event != EffectEvent::AttackSpawned
        && rule.event != EffectEvent::MainProjectileBeforeDamage
        && rule.event != EffectEvent::HitBeforeDamage)
    {
        error = "同隊事件來源觀察目前只支援攻擊生成或命中傷害前事件";
        return false;
    }
    if (rule.chancePct < 0 || rule.chancePct > 100)
    {
        error = "觸發機率必須介於 0 與 100";
        return false;
    }
    if (rule.maxActivations < 0)
    {
        error = "啟用次數上限不可為負數";
        return false;
    }
    if (rule.sharedCooldownFrames < 0)
    {
        error = "同來源冷卻幀數不可為負數";
        return false;
    }
    if (rule.intervalFrames < 0)
    {
        error = "間隔幀數不可為負數";
        return false;
    }
    if (rule.intervalFrames > 0 && rule.event != EffectEvent::FrameAdvanced)
    {
        error = "間隔幀數只允許用於每幀事件";
        return false;
    }
    if (rule.everyNthEvent < 0)
    {
        error = "每N次事件不可為負數";
        return false;
    }
    if (rule.everyNthEvent == 1)
    {
        error = "每N次事件為1沒有意義，請省略此欄位";
        return false;
    }
    if (rule.repetitionCount)
    {
        if (!validateEffectNumberAtEvent(*rule.repetitionCount, rule.event, error))
            return false;
        if (!effectNumberMustBePositive(*rule.repetitionCount))
        {
            error = "規則重複次數必須保證為正數";
            return false;
        }
        const auto targetScoped = [](EffectNumberBase base)
        {
            return base == EffectNumberBase::TargetMaxHp
                || base == EffectNumberBase::TargetCurrentHp
                || base == EffectNumberBase::TargetCurrentShield
                || base == EffectNumberBase::TargetCurrentCooldown;
        };
        if (targetScoped(rule.repetitionCount->base)
            || (rule.repetitionCount->multiplierBase
                && targetScoped(*rule.repetitionCount->multiplierBase)))
        {
            error = "規則重複次數必須使用來源或事件範圍的數值，不可依個別目標而異";
            return false;
        }
    }
    if (rule.activationLimit)
    {
        if (rule.activationLimit->maxEvaluations <= 0)
        {
            error = "觸發限制次數必須大於零";
            return false;
        }
        switch (rule.activationLimit->scope)
        {
        case EffectActivationScope::PerCastPerTarget:
            if (!effectEventHas(rule.event, EffectEventCapability::CastProvenance))
            {
                error = "每次施放每個目標的觸發限制需要具有施放識別的事件";
                return false;
            }
            break;
        default:
            error = "未知的效果觸發限制範圍";
            return false;
        }
    }
    if (rule.actions.empty())
    {
        error = "規則至少需要一個動作";
        return false;
    }
    if (!validateSelectorSchema(rule.selector, error)
        || !validateSelectorAtEvent(rule.selector, rule.event, error)) return false;
    for (const auto& condition : rule.conditions)
    {
        if (!validateConditionAtEvent(condition, rule.event, error)) return false;
    }
    if (rule.event == EffectEvent::DamageResolved
        && std::ranges::any_of(rule.conditions, [](const EffectCondition& condition)
            {
                return std::holds_alternative<IsMainProjectileCondition>(condition);
            })
        && !std::ranges::any_of(rule.conditions, [](const EffectCondition& condition)
            {
                return std::holds_alternative<DamageOriginIsAttackCondition>(condition);
            }))
    {
        error = "傷害結算事件使用主彈道條件時也需要「傷害來自招式」條件";
        return false;
    }
    for (const auto& action : rule.actions)
    {
        if (!isActionAllowedAtEvent(action, rule.event, error)
            || !validateActionPayload(action, rule.event, error)) return false;
    }
    if (rule.event == EffectEvent::BattleInitialized
        && !validateBattleInitializedRule(rule, error)) return false;
    const bool hasExactRuntimeAction = std::ranges::any_of(
        rule.actions,
        [](const EffectAction& action)
        {
            return exactRuntimeActionClass(action) == ExactRuntimeActionClass::Exact;
        });
    const bool hasUnsupportedComposition = std::ranges::any_of(
        rule.actions,
        [](const EffectAction& action)
        {
            return exactRuntimeActionClass(action)
                == ExactRuntimeActionClass::UnsupportedComposition;
        });
    const bool hasOrdinaryAction = std::ranges::any_of(
        rule.actions,
        [](const EffectAction& action)
        {
            return exactRuntimeActionClass(action) == ExactRuntimeActionClass::Ordinary;
        });
    if (hasUnsupportedComposition || (hasExactRuntimeAction && hasOrdinaryAction))
    {
        error = "精確 runtime 階段動作不可與一般動作混合，也不可放在條件分支內；請拆成不同效果規則";
        return false;
    }
    const bool exactRuntimeChanceIsConsumed = rule.actions.size() == 1
        && exactRuntimeActionConsumesRuleChance(rule.actions.front());
    if (hasExactRuntimeAction
        && ((rule.chancePct != 100 && !exactRuntimeChanceIsConsumed)
            || rule.everyNthEvent > 0
            || rule.repetitionCount
            || rule.activationLimit
            || rule.maxActivations > 0
            || rule.sharedCooldownFrames > 0))
    {
        error = "精確 runtime 階段規則不可設定其消費端未直接處理的規則機率、每N次事件、觸發限制、觸發次數或同來源冷卻；runtime 消費端不共用一般規則的觸發記帳";
        return false;
    }
    return true;
}

bool validateEffectRules(std::span<const EffectRule> rules, std::string& error)
{
    for (const auto& rule : rules)
    {
        if (!validateEffectRule(rule, error)) return false;
    }
    return validateExplicitStatusLifecycles(rules, error);
}

}  // namespace KysChess
