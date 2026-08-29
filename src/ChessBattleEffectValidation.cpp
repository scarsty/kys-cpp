#include "ChessBattleEffectValidation.h"
#include "ChessBattleEffectSemantics.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <set>
#include <type_traits>

namespace KysChess
{
namespace
{
bool eventHasHitPayload(EffectEvent event)
{
    return event == EffectEvent::MainProjectileBeforeDamage
        || event == EffectEvent::HitBeforeDamage;
}

bool eventHasDamagePayload(EffectEvent event)
{
    return event == EffectEvent::DamageResolved;
}

bool eventHasDamageOriginPayload(EffectEvent event)
{
    return eventHasDamagePayload(event)
        || event == EffectEvent::ShieldBroken
        || event == EffectEvent::UnitDied
        || event == EffectEvent::AllyDied;
}

bool eventHasHealPayload(EffectEvent event)
{
    return event == EffectEvent::HealAttempted
        || event == EffectEvent::HealApplied;
}

bool eventHasCastAggregatePayload(EffectEvent event)
{
    return event == EffectEvent::CastContinuation
        || event == EffectEvent::CastSettled;
}

bool eventHasCastProvenance(EffectEvent event)
{
    switch (event)
    {
    case EffectEvent::CastPlanned:
    case EffectEvent::AttackCommitted:
    case EffectEvent::UltimateCommitted:
    case EffectEvent::AttackSpawned:
    case EffectEvent::MainProjectileBeforeDamage:
    case EffectEvent::HitBeforeDamage:
    case EffectEvent::CastContinuation:
    case EffectEvent::CastSettled:
        return true;
    default:
        return false;
    }
}

bool validateSelectorAtEvent(const EffectSelector& selector, EffectEvent event, std::string& error)
{
    if (selector.kind == EffectSelectorKind::HitTarget && !eventHasHitPayload(event) && !eventHasDamagePayload(event))
    {
        error = "命中目標選擇器需要命中或傷害事件";
        return false;
    }
    if (selector.kind == EffectSelectorKind::TransactionTarget
        && !eventHasDamagePayload(event)
        && !eventHasHealPayload(event)
        && event != EffectEvent::ShieldBroken
        && event != EffectEvent::UnitDied
        && event != EffectEvent::AllyDied)
    {
        error = "交易目標選擇器需要交易事件";
        return false;
    }
    if (selector.kind == EffectSelectorKind::OriginalAttackTarget
        && event != EffectEvent::CastPlanned
        && event != EffectEvent::AttackCommitted
        && event != EffectEvent::UltimateCommitted
        && event != EffectEvent::AttackSpawned
        && !eventHasHitPayload(event)
        && !eventHasDamagePayload(event)
        && !eventHasCastAggregatePayload(event))
    {
        error = "原攻擊目標選擇器需要施放、攻擊、命中、傷害或施放聚合事件";
        return false;
    }
    if (selector.requiredTarget)
    {
        EffectSelector required;
        switch (*selector.requiredTarget)
        {
        case EffectRequiredTarget::Self: required.kind = EffectSelectorKind::Self; break;
        case EffectRequiredTarget::SourceUnit: required.kind = EffectSelectorKind::SourceUnit; break;
        case EffectRequiredTarget::TransactionTarget:
            required.kind = EffectSelectorKind::TransactionTarget;
            break;
        case EffectRequiredTarget::HitTarget: required.kind = EffectSelectorKind::HitTarget; break;
        case EffectRequiredTarget::OriginalAttackTarget:
            required.kind = EffectSelectorKind::OriginalAttackTarget;
            break;
        }
        if (!validateSelectorAtEvent(required, event, error))
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
            if constexpr (std::is_same_v<T, SourceHpRatioAtMostCondition>
                || std::is_same_v<T, SourceHpRatioBelowCondition>
                || std::is_same_v<T, TargetHpRatioAtMostCondition>)
            {
                if (typed.percent < 0 || typed.percent > 100) return reject("生命比例條件必須介於 0 與 100");
                if constexpr (std::is_same_v<T, TargetHpRatioAtMostCondition>)
                {
                    if (!eventHasHitPayload(event) && !eventHasDamagePayload(event) && !eventHasHealPayload(event))
                        return reject("目標生命條件需要命中、傷害或治療事件");
                }
            }
            else if constexpr (std::is_same_v<T, IsMainProjectileCondition>
                || std::is_same_v<T, IsRootAttackCondition>
                || std::is_same_v<T, AttackOrdinalEqualsCondition>)
            {
                if (event != EffectEvent::AttackSpawned && !eventHasHitPayload(event) && !eventHasDamagePayload(event))
                    return reject("攻擊來源條件需要攻擊、命中或傷害事件");
                if constexpr (std::is_same_v<T, AttackOrdinalEqualsCondition>)
                    if (typed.ordinal < 0) return reject("攻擊序號不可為負數");
            }
            else if constexpr (std::is_same_v<T, CastDistinctTargetCountAtLeastCondition>)
            {
                if (!eventHasCastAggregatePayload(event)) return reject("不同目標數條件需要施放聚合事件");
                if (typed.count <= 0) return reject("不同目標數門檻必須為正數");
            }
            else if constexpr (std::is_same_v<T, HealKindInCondition>)
            {
                if (!eventHasHealPayload(event)) return reject("治療種類條件需要治療事件");
                if (typed.kinds.empty()) return reject("治療種類條件不可為空");
            }
            else if constexpr (std::is_same_v<T, DamageOriginIsAttackCondition>)
            {
                if (!eventHasDamageOriginPayload(event)) return reject("傷害來源條件需要傷害、破盾或死亡事件");
            }
            else if constexpr (std::is_same_v<T, DamageKilledTargetCondition>)
            {
                if (!eventHasDamagePayload(event)) return reject("擊殺條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, AcceptedHitCondition>)
            {
                if (!eventHasDamagePayload(event)) return reject("接受命中條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, EventTargetBelongsToBoundSourceCondition>)
            {
                if (event != EffectEvent::AllyDied && event != EffectEvent::UnitDied)
                    return reject("來源成員條件需要死亡事件");
            }
            else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>)
            {
                if (!eventHasDamagePayload(event)) return reject("傷害方位條件需要傷害結算事件");
            }
            else if constexpr (std::is_same_v<T, DamageKindInCondition>)
            {
                if (!eventHasHitPayload(event) && !eventHasDamagePayload(event)) return reject("傷害種類條件需要命中或傷害事件");
                if (typed.kinds.empty()) return reject("傷害種類條件不可為空");
            }
            else if constexpr (std::is_same_v<T, TargetMpWasFullBeforeCastCondition>)
            {
                if (event != EffectEvent::CastPlanned
                    && event != EffectEvent::AttackCommitted
                    && event != EffectEvent::UltimateCommitted)
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
    const bool usesStatus = usesBase(EffectNumberBase::SourceStatusPotency)
        || usesBase(EffectNumberBase::SourceStatusStacks);
    if (usesStatus != number.status.has_value())
    {
        error = usesStatus
            ? "來源狀態數值基準需要指定狀態"
            : "只有來源狀態數值基準可指定狀態";
        return false;
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
    if (needsFinalDamage && !eventHasDamagePayload(event))
    {
        error = "實際生命傷害公式只能用於傷害結算事件";
        return false;
    }
    const auto needsCurrentShield = number.base == EffectNumberBase::TargetCurrentShield
        || number.multiplierBase == EffectNumberBase::TargetCurrentShield;
    if (needsCurrentShield
        && !eventHasHitPayload(event)
        && !eventHasDamagePayload(event)
        && event != EffectEvent::ShieldBroken)
    {
        error = "目標目前護盾公式需要命中、傷害或破盾事件";
        return false;
    }
    const auto needsCurrentCooldown = number.base == EffectNumberBase::TargetCurrentCooldown
        || number.multiplierBase == EffectNumberBase::TargetCurrentCooldown;
    if (needsCurrentCooldown
        && event != EffectEvent::FrameAdvanced
        && event != EffectEvent::DamageResolved)
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
    if (!effectNumberCannotBeNegative(number)
        || (number.maximum && *number.maximum <= 0))
    {
        return false;
    }
    return number.flat > 0
        || (number.minimum && *number.minimum > 0);
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
                if (typed.perStack && typed.stack != EffectStackPolicy::AddStack)
                    return reject("只有增加層數的屬性修正可指定每層計算");
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
                        || typed.stage != DamageModifierStage::Final))
                    return reject("單次承傷上限只支援承受方的最終階段");
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
                if (!validateEffectNumberAtEvent(typed.potency, event, error)
                    || !validateEffectNumberAtEvent(typed.secondaryPotency, event, error)) return false;
                if (typed.applicationCount)
                {
                    if (!validateEffectNumberAtEvent(*typed.applicationCount, event, error)) return false;
                    if (!effectNumberMustBePositive(*typed.applicationCount))
                        return reject("狀態套用次數必須保證為正數");
                }
                if (typed.duration
                    && (!validateEffectNumberAtEvent(*typed.duration, event, error)
                        || !effectNumberCannotBeNegative(*typed.duration))) return false;
                if (typed.durationFrames < 0)
                    return reject("狀態持續幀數不可為負數");
                if (typed.stacks <= 0) return reject("狀態層數必須為正數");
                if (typed.stack == EffectStackPolicy::AddStack && !typed.stackLimit)
                    return reject("增加層數的狀態需要層數上限");
                if (typed.stackLimit && *typed.stackLimit <= 0) return reject("狀態層數上限必須為正數");
                if (typed.aggregatePotencyWithinEvent
                    && (typed.status != BattleStatusKind::Poison
                        || typed.stack != EffectStackPolicy::KeepStrongest
                        || typed.secondaryPotency.base != EffectNumberBase::Constant
                        || typed.secondaryPotency.flat != 0
                        || !typed.stackLimit
                        || *typed.stackLimit != typed.stacks))
                {
                    return reject("同事件合計強度只支援保留最強的中毒，且必須明確指定相同的正層數與層數上限，不可指定次要強度");
                }
            }
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
            {
                if (typed.stacks <= 0) return reject("消耗狀態層數必須為正數");
                if (typed.whenDepleted)
                {
                    if (typed.whenDepleted->applicationCount)
                        return reject("消耗最後一層套用的狀態不可指定套用次數");
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
                                && !eventHasCastProvenance(event))
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
                            if (event != EffectEvent::CastPlanned)
                                return reject("借用效果規則只允許用於施放規劃事件");
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
                            if (event != EffectEvent::UltimateCommitted)
                                return reject("複製攻擊定義只允許用於絕招提交事件");
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
                            if (event != EffectEvent::BattleInitialized)
                                return reject("生成分身只允許用於戰鬥初始化事件");
                            if (machine.count <= 0) return reject("生成分身數量必須為正數");
                        }
                        else if constexpr (std::is_same_v<M, PreventDeathAction>)
                        {
                            if (event != EffectEvent::BattleInitialized)
                                return reject("死亡庇護只允許用於戰鬥初始化事件");
                            if (machine.invincibilityFrames <= 0)
                                return reject("死亡庇護無敵幀數必須為正數");
                        }
                        else if constexpr (std::is_same_v<M, ConfigureRescueRepositionAction>)
                        {
                            if (event != EffectEvent::BattleInitialized)
                                return reject("挪移次數只允許用於戰鬥初始化事件");
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
    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            auto reject = [&](std::string message)
            {
                error = std::move(message);
                return false;
            };

            if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                return event == EffectEvent::HealAttempted
                    || reject("治療交易修正只能用於 HealAttempted");
            }
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
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
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
            {
                return event == EffectEvent::CastPlanned
                    || event == EffectEvent::AttackCommitted
                    || event == EffectEvent::UltimateCommitted
                    || event == EffectEvent::AttackSpawned
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || event == EffectEvent::CastContinuation
                    || reject("修改攻擊不支援此事件");
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                return event == EffectEvent::BattleInitialized
                    || event == EffectEvent::UltimateCommitted
                    || event == EffectEvent::HitBeforeDamage
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || event == EffectEvent::DamageResolved
                    || reject("傷害修正不支援此事件");
            }
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
            {
                return event == EffectEvent::HitBeforeDamage
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || reject("強制移動只允許命中傷害前事件");
            }
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
            {
                return event == EffectEvent::AttackCommitted
                    || event == EffectEvent::UltimateCommitted
                    || event == EffectEvent::MainProjectileBeforeDamage
                    || event == EffectEvent::HitBeforeDamage
                    || event == EffectEvent::UnitDied
                    || reject("建立區域不支援此事件");
            }
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                return event != EffectEvent::CastPlanned
                    && event != EffectEvent::AttackSpawned
                    && event != EffectEvent::HealAttempted
                    || reject("造成傷害不支援此事件");
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
                return event != EffectEvent::HealAttempted
                    || reject("HealAttempted 只允許治療交易修正");
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
        case EffectNumberBase::AccumulatedStateValue:
        case EffectNumberBase::SourceStatusPotency:
        case EffectNumberBase::SourceStatusStacks:
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
                    || typed.perStack
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
            if (!validateBattleInitializedNumber(typed.potency, error)
                || !validateBattleInitializedNumber(typed.secondaryPotency, error))
                return false;
            if (typed.duration
                && !validateBattleInitializedNumber(*typed.duration, error))
                return false;
            return !typed.applicationCount
                || validateBattleInitializedNumber(*typed.applicationCount, error);
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


}  // namespace

bool validateEffectRule(const EffectRule& rule, std::string& error)
{
    error.clear();
    if (rule.castMatch == EffectCastMatch::OwnerAnyCast
        && rule.observation != EffectObservationScope::Owner)
    {
        error = "效果擁有者任意施放匹配只支援效果擁有者觀察";
        return false;
    }
    if (rule.castMatch == EffectCastMatch::OwnerAnyCast
        && !eventHasCastProvenance(rule.event))
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
            if (!eventHasCastProvenance(rule.event))
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

}  // namespace KysChess
