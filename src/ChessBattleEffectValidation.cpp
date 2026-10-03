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

template<class>
inline constexpr bool AlwaysFalse = false;
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
            else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>
                || std::is_same_v<T, EventTargetHasNegativeStatusCondition>)
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

bool validateEffectNumberAtEvent(
    const EffectNumber& number,
    EffectEvent event,
    EffectRuleAuthoringContext context,
    std::string& error)
{
    const auto usesBase = [&](EffectNumberBase base)
    {
        return number.base == base || number.multiplierBase == base;
    };
    const bool statusBehavior = context != EffectRuleAuthoringContext::Configured;
    for (const auto base : { number.base, number.multiplierBase.value_or(number.base) })
    {
        if (!effectNumberBaseAllowedInAuthoringContext(base, statusBehavior))
        {
            error = std::format(
                "數值基準「{}」只能用於狀態效果規則",
                effectNumberBaseCatalogEntry(base).authorLabel);
            return false;
        }
    }
    if (number.statusScale == StatusNumberScale::PerContributionLayer
        && !statusBehavior)
    {
        error = "「每層數值」只能用於層數狀態的效果規則";
        return false;
    }
    const bool usesStatusQuantity = usesBase(EffectNumberBase::SourceStatusQuantity);
    const bool usesStatus = usesStatusQuantity;
    if (usesStatus != number.status.has_value())
    {
        error = usesStatus
            ? "來源狀態數值基準需要指定狀態"
            : "只有來源狀態數值基準可指定狀態";
        return false;
    }
    if (!usesStatusQuantity && number.statusSource != StatusSourceMatch::Any)
    {
        error = "只有來源狀態數量可指定狀態來源";
        return false;
    }
    if (number.statusSource == StatusSourceMatch::CurrentContribution)
    {
        error = "來源狀態數量請使用「不限」、「效果擁有者」或「效果綁定」；目前貢獻請改用「此狀態貢獻數量」基準";
        return false;
    }
    const bool usesCurrentContribution = usesBase(
        EffectNumberBase::CurrentContributionQuantity);
    if (usesCurrentContribution
        && context == EffectRuleAuthoringContext::Configured)
    {
        error = "「此狀態貢獻數量」只能用於狀態效果規則";
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
    if (event == EffectEvent::StatusPersistent
        && (statusNumberBindingPhase(number.base) == StatusNumberBindingPhase::EventLive
            || (number.multiplierBase
                && statusNumberBindingPhase(*number.multiplierBase)
                    == StatusNumberBindingPhase::EventLive)))
    {
        error = "狀態持續效果的數值不可使用事件即時基準";
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

bool effectNumberCannotBePositive(const EffectNumber& number)
{
    if (const auto constant = effectiveConstantEffectNumberValue(number))
        return *constant <= 0;
    if (number.minimum && *number.minimum > 0) return false;
    if (number.maximum && *number.maximum <= 0) return true;

    const bool scaledValueCanBePositive = number.base != EffectNumberBase::Constant
        && number.percent > 0;
    return !scaledValueCanBePositive && number.flat <= 0;
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
    return !bound
        || (*bound >= std::numeric_limits<int>::min() / multiplier
            && *bound <= std::numeric_limits<int>::max() / multiplier);
}

bool isStatusHolderRule(const EffectRule& rule, EffectEvent event)
{
    return rule.event == event
        && rule.selector.kind == EffectSelectorKind::StatusHolder;
}

template <typename Condition>
bool hasCondition(const EffectRule& rule)
{
    return std::ranges::any_of(rule.conditions, [](const EffectCondition& condition)
    {
        return std::holds_alternative<Condition>(condition);
    });
}

bool coversEveryHealKind(const ModifyHealTransactionAction& action)
{
    std::set<std::string_view> expected;
    for (const auto& entry : effectHealKindCatalog)
        expected.insert(entry.authorLabel);
    const std::set<std::string_view> actual(action.kinds.begin(), action.kinds.end());
    return actual == expected;
}

bool isPositiveNumber(const EffectNumber& number)
{
    return effectNumberMustBePositive(number);
}

bool isNegativeConstantNumber(const EffectNumber& number)
{
    const auto value = effectiveConstantEffectNumberValue(number);
    return value && *value < 0;
}

bool isPeriodicStatusRule(const EffectRule& rule)
{
    return isStatusHolderRule(rule, EffectEvent::FrameAdvanced)
        && rule.observation == EffectObservationScope::StatusHolderEventSource
        && rule.intervalFrames > 0;
}

bool hasDefaultStatusTriggerAccounting(const EffectRule& rule)
{
    return rule.chancePct == 100
        && rule.maxActivations == 0
        && rule.sharedCooldownFrames == 0
        && rule.everyNthEvent == 0
        && !rule.activationLimit
        && !rule.repetitionCount;
}

bool isUngatedStatusBehaviorRule(const EffectRule& rule)
{
    return rule.conditions.empty()
        && hasDefaultStatusTriggerAccounting(rule);
}

enum class ExactRuntimeActionClass
{
    Ordinary,
    Exact,
    UnsupportedComposition,
};

ExactRuntimeActionClass exactRuntimeActionClass(const EffectAction& action);

bool actionUsesStatusContext(const EffectAction& action)
{
    if (std::holds_alternative<ConsumeThisStatusAction>(action.value)
        || std::holds_alternative<SuppressCurrentCastContactsAction>(action.value)
        || std::holds_alternative<MakeIncomingAttackMissAction>(action.value)
        || std::holds_alternative<BlockPositiveDamageAction>(action.value))
    {
        return true;
    }
    const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
        &action.value);
    if (!conditional) return false;
    assert(*conditional);
    const auto uses = [](const std::vector<EffectAction>& actions)
    {
        return std::ranges::any_of(actions, actionUsesStatusContext);
    };
    return uses((*conditional)->whenTrue) || uses((*conditional)->whenFalse);
}

bool ruleUsesStatusContext(const EffectRule& rule)
{
    return rule.event == EffectEvent::StatusPersistent
        || rule.observation == EffectObservationScope::StatusHolderEventSource
        || rule.observation == EffectObservationScope::StatusHolderEventTarget
        || rule.observation == EffectObservationScope::StatusSourceEventSource
        || rule.observation == EffectObservationScope::SourceOwnerTeamEventSource
        || rule.selector.kind == EffectSelectorKind::StatusHolder
        || std::ranges::any_of(rule.conditions, [](const EffectCondition& condition)
        {
            return std::holds_alternative<TargetIsStatusHolderCondition>(condition);
        })
        || std::ranges::any_of(rule.actions, actionUsesStatusContext);
}

bool statusObservationAllowed(EffectObservationScope observation)
{
    return observation == EffectObservationScope::StatusHolderEventSource
        || observation == EffectObservationScope::StatusHolderEventTarget
        || observation == EffectObservationScope::StatusSourceEventSource
        || observation == EffectObservationScope::SourceOwnerTeamEventSource;
}

bool validatePersistentStatusAction(const EffectAction& action, std::string& error)
{
    const auto reject = [&](std::string message)
    {
        error = std::move(message);
        return false;
    };
    return std::visit(
        [&](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, ModifyAttributeAction>)
            {
                if (typed.attribute != BattleAttribute::Speed
                    || typed.operation != AttributeOperation::PercentAdd)
                    return reject("狀態持續效果的屬性修正只支援速度百分比加算");
                if (typed.durationFrames != 0
                    || typed.stack != EffectStackPolicy::Independent
                    || typed.stackLimit
                    || typed.stackScope != EffectStackScope::Shared)
                    return reject("狀態持續效果不可另設修正持續時間或疊加政策");
                return true;
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                if (typed.durationFrames != 0
                    || typed.stack != EffectStackPolicy::Independent
                    || typed.stackLimit
                    || typed.stackScope != EffectStackScope::Shared)
                    return reject("狀態持續效果不可另設修正持續時間或疊加政策");
                if (typed.operation == DamageModifierOperation::CapSingleHitAtValue)
                {
                    if (typed.perspective == DamageModifierPerspective::Incoming
                        && typed.stage == DamageModifierStage::Final
                        && typed.channel == DamageChannel::All) return true;
                    return reject("狀態持續效果的單次承傷上限只支援承受方、最終階段與全部傷害");
                }
                if (typed.operation != DamageModifierOperation::PercentAdd)
                    return reject("狀態持續效果只支援百分比加算或單次承傷上限");
                if (typed.perspective == DamageModifierPerspective::Outgoing)
                {
                    if (typed.stage == DamageModifierStage::BeforeDefense
                        && typed.channel == DamageChannel::Skill) return true;
                    return reject("狀態持續效果的造成傷害修正只支援防禦前招式傷害");
                }
                if (typed.stage == DamageModifierStage::BeforeDefense)
                {
                    if (typed.channel != DamageChannel::All)
                        return reject("狀態持續效果的防禦前承傷修正只支援全部傷害");
                    if (!effectNumberCannotBePositive(typed.amount))
                        return reject("狀態持續效果的防禦前承傷修正必須保證不大於零");
                    return true;
                }
                if (typed.stage == DamageModifierStage::Final
                    && typed.channel == DamageChannel::All) return true;
                return reject("狀態持續效果的承受傷害修正只支援防禦前減免或最終全部傷害");
            }
            else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>
                || std::is_same_v<T, BlockPositiveDamageAction>)
            {
                return true;
            }
            else
            {
                return reject("狀態持續效果包含查詢快照不支援的動作");
            }
        },
        action.value);
}

bool validatePersistentStatusRule(const EffectRule& rule, std::string& error)
{
    EffectSelector holder;
    holder.kind = EffectSelectorKind::StatusHolder;
    if (rule.observation != EffectObservationScope::StatusHolderEventSource
        || rule.castMatch != EffectCastMatch::BoundMagic
        || rule.selector != holder)
    {
        error = "狀態持續效果必須使用預設狀態持有者觀察與單一狀態持有者目標";
        return false;
    }
    if (!rule.conditions.empty()
        || rule.chancePct != 100
        || rule.maxActivations != 0
        || rule.sharedCooldownFrames != 0
        || rule.intervalFrames != 0
        || rule.everyNthEvent != 0
        || rule.activationLimit
        || rule.repetitionCount)
    {
        error = "狀態持續效果是無條件查詢快照，不可設定條件、機率或觸發記帳欄位";
        return false;
    }
    return std::ranges::all_of(rule.actions, [&](const EffectAction& action)
    {
        return validatePersistentStatusAction(action, error);
    });
}

bool validateMinimumStatusBehaviorProfile(
    const ApplyStatusAction& application,
    StatusBehaviorProfile profile,
    std::string& missingCapability)
{
    assert(application.behavior);
    const auto& rules = application.behavior->rules;
    const auto fail = [&](std::string_view capability)
    {
        missingCapability = capability;
        return false;
    };
    const auto anyRule = [&](const auto& predicate)
    {
        return std::ranges::any_of(rules, predicate);
    };

    switch (profile)
    {
    case StatusBehaviorProfile::None:
        return true;
    case StatusBehaviorProfile::Poison:
        if (!canonicalPoisonDamageCapability(*application.behavior))
            return fail("唯一且規格化的週期目前生命中毒傷害及其逐次消耗");
        return true;
    case StatusBehaviorProfile::Bleed:
        if (!anyRule([](const EffectRule& rule)
            {
                return isPeriodicStatusRule(rule)
                    && isUngatedStatusBehaviorRule(rule)
                    && std::ranges::any_of(rule.actions, [](const EffectAction& action)
                    {
                        const auto* damage = std::get_if<DealDamageAction>(&action.value);
                        return damage
                            && damage->kind == BattleDamageKind::Bleed
                            && damage->amount.base == EffectNumberBase::TargetMaxHp
                            && !damage->amount.multiplierBase
                            && damage->amount.statusScale
                                == StatusNumberScale::PerContributionLayer
                            && isPositiveNumber(damage->amount);
                    });
            })) return fail("依貢獻層數縮放的週期最大生命流血傷害");
        return true;
    case StatusBehaviorProfile::ColdPoison:
    {
        const bool blocksHealing = anyRule([](const EffectRule& rule)
        {
            return isStatusHolderRule(rule, EffectEvent::StatusPersistent)
                && std::ranges::any_of(rule.actions, [](const EffectAction& action)
                {
                    const auto* modifier = std::get_if<ModifyHealTransactionAction>(
                        &action.value);
                    return modifier
                        && modifier->operation == HealModifierOperation::Block
                        && coversEveryHealKind(*modifier);
                });
        });
        if (!blocksHealing) return fail("阻止全部治療種類");
        const bool reducesSpeed = anyRule([](const EffectRule& rule)
        {
            return isStatusHolderRule(rule, EffectEvent::StatusPersistent)
                && std::ranges::any_of(rule.actions, [](const EffectAction& action)
                {
                    const auto* modifier = std::get_if<ModifyAttributeAction>(&action.value);
                    return modifier
                        && modifier->attribute == BattleAttribute::Speed
                        && modifier->operation == AttributeOperation::PercentAdd
                        && isNegativeConstantNumber(modifier->amount);
                });
        });
        if (!reducesSpeed) return fail("速度降低");
        return true;
    }
    case StatusBehaviorProfile::WitheredBone:
        if (!anyRule([](const EffectRule& rule)
            {
                if (!isStatusHolderRule(rule, EffectEvent::StatusPersistent)) return false;
                for (std::size_t damageIndex = 0; damageIndex < rule.actions.size(); ++damageIndex)
                {
                    const auto* damage = std::get_if<ModifyDamageAction>(
                        &rule.actions[damageIndex].value);
                    if (!damage
                        || damage->perspective != DamageModifierPerspective::Incoming
                        || damage->operation != DamageModifierOperation::PercentAdd
                        || !isPositiveNumber(damage->amount)) continue;
                    for (std::size_t healIndex = damageIndex + 1;
                         healIndex < rule.actions.size(); ++healIndex)
                    {
                        const auto* heal = std::get_if<ModifyHealTransactionAction>(
                            &rule.actions[healIndex].value);
                        if (heal
                            && heal->operation == HealModifierOperation::MultiplyReceived
                            && heal->percent < 100
                            && coversEveryHealKind(*heal)) return true;
                    }
                }
                return false;
            })) return fail("依序增加承受傷害並降低全部受到治療");
        return true;
    case StatusBehaviorProfile::SevenStar:
        if (!anyRule([](const EffectRule& rule)
            {
                if (rule.event != EffectEvent::HitBeforeDamage
                    || rule.observation != EffectObservationScope::SourceOwnerTeamEventSource
                    || rule.selector.kind != EffectSelectorKind::HitTarget
                    || !hasDefaultStatusTriggerAccounting(rule)
                    || rule.conditions.size() != 1
                    || !hasCondition<TargetIsStatusHolderCondition>(rule)) return false;
                for (std::size_t index = 0; index + 1 < rule.actions.size(); ++index)
                {
                    const auto* damage = std::get_if<ModifyDamageAction>(
                        &rule.actions[index].value);
                    const auto* consume = std::get_if<ConsumeThisStatusAction>(
                        &rule.actions[index + 1].value);
                    if (!damage
                        || damage->perspective != DamageModifierPerspective::Outgoing
                        || damage->stage != DamageModifierStage::BeforeDefense
                        || damage->channel != DamageChannel::Skill
                        || damage->operation != DamageModifierOperation::IgnoreDefensePercent
                        || !isPositiveNumber(damage->amount)
                        || !consume
                        || consume->quantity != 1
                        || !consume->whenDepleted) continue;
                    const auto& depleted = *consume->whenDepleted;
                    if (depleted.status == BattleStatusKind::Stun
                        && (depleted.durationFrames > 0
                            || (depleted.duration && isPositiveNumber(*depleted.duration))))
                    {
                        return true;
                    }
                }
                return false;
            })) return fail("友軍命中時忽略防禦、消耗一枚印記並在耗盡時眩暈");
        return true;
    case StatusBehaviorProfile::NeutralizeForce:
        if (!anyRule([](const EffectRule& rule)
            {
                if (rule.event != EffectEvent::HitBeforeDamage
                    || rule.selector.kind != EffectSelectorKind::HitTarget
                    || rule.observation != EffectObservationScope::StatusHolderEventSource
                    || !isUngatedStatusBehaviorRule(rule)
                    || rule.actions.size() != 2) return false;
                const auto* recovery = std::get_if<ChangeResourceAction>(&rule.actions[0].value);
                const auto* consume = std::get_if<ConsumeThisStatusAction>(&rule.actions[1].value);
                return recovery && recovery->resource == BattleResource::Mp
                    && recovery->kind == ResourceChangeKind::Restore
                    && isPositiveNumber(recovery->amount)
                    && consume && consume->quantity == 1 && !consume->whenDepleted;
            })) return fail("命中目標恢復內力並消耗一次化勁");
        return true;
    case StatusBehaviorProfile::Blinded:
        if (!anyRule([](const EffectRule& rule)
            {
                return isStatusHolderRule(rule, EffectEvent::HitBeforeDamage)
                    && rule.observation
                        == EffectObservationScope::StatusHolderEventSource
                    && isUngatedStatusBehaviorRule(rule)
                    && std::ranges::any_of(rule.actions, [](const EffectAction& action)
                    {
                        return std::holds_alternative<SuppressCurrentCastContactsAction>(
                            action.value);
                    });
            })) return fail("阻止本次施放接觸");
        return true;
    case StatusBehaviorProfile::NextIncomingAttackMiss:
        if (!anyRule([](const EffectRule& rule)
            {
                return isStatusHolderRule(rule, EffectEvent::HitBeforeDamage)
                    && rule.observation == EffectObservationScope::StatusHolderEventTarget
                    && isUngatedStatusBehaviorRule(rule)
                    && std::ranges::any_of(rule.actions, [](const EffectAction& action)
                    {
                        return std::holds_alternative<MakeIncomingAttackMissAction>(
                            action.value);
                    });
            })) return fail("使狀態持有者下一次受到的攻擊落空");
        return true;
    case StatusBehaviorProfile::DamageBlock:
        if (!anyRule([](const EffectRule& rule)
            {
                return isStatusHolderRule(rule, EffectEvent::StatusPersistent)
                    && std::ranges::any_of(rule.actions, [](const EffectAction& action)
                    {
                        return std::holds_alternative<BlockPositiveDamageAction>(action.value);
                    });
            })) return fail("抵擋非處決正傷害");
        return true;
    case StatusBehaviorProfile::SingleHitCap:
        if (!anyRule([](const EffectRule& rule)
            {
                return isStatusHolderRule(rule, EffectEvent::StatusPersistent)
                    && std::ranges::any_of(rule.actions, [](const EffectAction& action)
                    {
                        const auto* modifier = std::get_if<ModifyDamageAction>(&action.value);
                        return modifier
                            && modifier->perspective == DamageModifierPerspective::Incoming
                            && modifier->stage == DamageModifierStage::Final
                            && modifier->channel == DamageChannel::All
                            && modifier->operation == DamageModifierOperation::CapSingleHitAtValue
                            && isPositiveNumber(modifier->amount);
                    });
            })) return fail("承受方最終階段的單次傷害上限");
        return true;
    case StatusBehaviorProfile::BattleSpirit:
    {
        const auto scaled = [](const EffectNumber& number)
        {
            return number.statusScale == StatusNumberScale::PerContributionLayer;
        };
        const bool outgoing = anyRule([&](const EffectRule& rule)
        {
            return isStatusHolderRule(rule, EffectEvent::StatusPersistent)
                && std::ranges::any_of(rule.actions, [&](const EffectAction& action)
                {
                    const auto* modifier = std::get_if<ModifyDamageAction>(&action.value);
                    return modifier
                        && modifier->perspective == DamageModifierPerspective::Outgoing
                        && modifier->channel == DamageChannel::Skill
                        && modifier->operation == DamageModifierOperation::PercentAdd
                        && scaled(modifier->amount)
                        && isPositiveNumber(modifier->amount);
                });
        });
        if (!outgoing) return fail("每個貢獻層各自增加造成的招式傷害");
        const bool incoming = anyRule([&](const EffectRule& rule)
        {
            return isStatusHolderRule(rule, EffectEvent::StatusPersistent)
                && std::ranges::any_of(rule.actions, [&](const EffectAction& action)
                {
                    const auto* modifier = std::get_if<ModifyDamageAction>(&action.value);
                    return modifier
                        && modifier->perspective == DamageModifierPerspective::Incoming
                        && modifier->stage == DamageModifierStage::BeforeDefense
                        && modifier->channel == DamageChannel::All
                        && modifier->operation == DamageModifierOperation::PercentAdd
                        && scaled(modifier->amount)
                        && isNegativeConstantNumber(modifier->amount);
                });
        });
        if (!incoming) return fail("每個貢獻層各自降低承受傷害");
        return true;
    }
    case StatusBehaviorProfile::TrueQi:
        if (!anyRule([](const EffectRule& rule)
            {
                return rule.event == EffectEvent::HitBeforeDamage
                    && rule.observation == EffectObservationScope::StatusHolderEventSource
                    && rule.selector.kind == EffectSelectorKind::HitTarget
                    && isUngatedStatusBehaviorRule(rule)
                    && std::ranges::any_of(rule.actions, [](const EffectAction& action)
                    {
                        const auto* damage = std::get_if<DealDamageAction>(&action.value);
                        return damage
                            && damage->kind == BattleDamageKind::Pure
                            && damage->amount.statusScale
                                == StatusNumberScale::PerContributionLayer
                            && isPositiveNumber(damage->amount);
                    });
            })) return fail("狀態持有者命中時依貢獻層數造成純粹傷害");
        return true;
    case StatusBehaviorProfile::PoisonExplosion:
        if (!anyRule([](const EffectRule& rule)
            {
                if (rule.event != EffectEvent::UnitDied
                    || rule.observation != EffectObservationScope::StatusHolderEventTarget)
                    return false;
                if (rule.selector.kind != EffectSelectorKind::UnitsInRadius
                    || rule.selector.team != EffectTeamFilter::Enemy)
                    return false;
                if (!isUngatedStatusBehaviorRule(rule)) return false;
                for (std::size_t damageIndex = 0; damageIndex < rule.actions.size(); ++damageIndex)
                {
                    const auto* damage = std::get_if<DealDamageAction>(
                        &rule.actions[damageIndex].value);
                    if (!damage
                        || damage->kind != BattleDamageKind::Pure
                        || damage->amount.statusScale
                            != StatusNumberScale::PerContributionLayer
                        || !isPositiveNumber(damage->amount)) continue;
                    return std::ranges::any_of(
                        rule.actions.begin() + static_cast<std::ptrdiff_t>(damageIndex + 1),
                        rule.actions.end(),
                        [](const EffectAction& action)
                        {
                            const auto* poison = std::get_if<ApplyStatusAction>(&action.value);
                            return poison && poison->status == BattleStatusKind::Poison;
                        });
                }
                return false;
            })) return fail("狀態持有者死亡時對半徑內敵軍依貢獻層數造成純粹傷害並施加中毒");
        return true;
    }
    assert(false);
    return false;
}

bool validateStatusApplication(
    const ApplyStatusAction& action,
    EffectEvent event,
    EffectRuleAuthoringContext context,
    std::string& error)
{
    const auto reject = [&](std::string message)
    {
        error = std::move(message);
        return false;
    };
    const auto rejectStatus = [&](std::string_view message)
    {
        return reject(std::format(
            "狀態「{}」{}",
            battleStatusLabel(action.status),
            message));
    };
    const auto& catalog = statusCatalogEntry(action.status);
    if (!catalog.authorable) return reject("執行期狀態不可由效果直接套用");

    if (catalog.duration == StatusDurationModel::RequiredPositive)
    {
        if ((action.durationFrames > 0) == action.duration.has_value())
            return rejectStatus("必須擇一指定固定或公式「持續幀數」");
        if (action.duration)
        {
            if (!validateEffectNumberAtEvent(*action.duration, event, context, error)) return false;
            if (!effectNumberMustBePositive(*action.duration))
                return rejectStatus("的持續幀數公式必須保證為正數");
        }
    }
    else if (action.durationFrames != 0 || action.duration)
    {
        return rejectStatus("不可指定「持續幀數」");
    }

    const auto positive = [&](int value, std::string_view field)
    {
        if (value > 0) return true;
        error = std::format("「{}」必須為正數", field);
        return false;
    };
    const auto positiveQuantityField = [&](int value, StatusQuantityFieldId field)
    {
        return positive(value, statusQuantityFieldLabel(field));
    };
    switch (catalog.quantity)
    {
    case StatusQuantityModel::None:
        if (!std::holds_alternative<NoStatusQuantity>(action.quantity))
            return rejectStatus("不可指定數量");
        break;
    case StatusQuantityModel::Layers:
    {
        const auto fields = statusQuantityOperationFields(
            StatusQuantityOperationId::AddLayers);
        assert(fields.size() == 2);
        const auto* quantity = std::get_if<AddStatusLayers>(&action.quantity);
        if (!quantity)
        {
            return rejectStatus(std::format(
                "必須使用「{}」與「{}」",
                statusQuantityFieldLabel(fields[0]),
                statusQuantityFieldLabel(fields[1])));
        }
        if (!positiveQuantityField(quantity->count, fields[0])
            || !positiveQuantityField(quantity->limit, fields[1])) return false;
        break;
    }
    case StatusQuantityModel::SharedLayers:
    {
        const auto fields = statusQuantityOperationFields(
            StatusQuantityOperationId::AddSharedLayers);
        assert(fields.size() == 2);
        const auto* quantity = std::get_if<AddSharedStatusLayers>(&action.quantity);
        if (!quantity)
        {
            return rejectStatus(std::format(
                "必須使用「{}」與「{}」",
                statusQuantityFieldLabel(fields[0]),
                statusQuantityFieldLabel(fields[1])));
        }
        if (!positiveQuantityField(quantity->count, fields[0])
            || !positiveQuantityField(quantity->targetTotalLimit, fields[1])) return false;
        break;
    }
    case StatusQuantityModel::TriggerCharges:
    {
        const auto fields = statusQuantityOperationFields(
            StatusQuantityOperationId::SetTriggerCharges);
        assert(fields.size() == 1);
        const auto* quantity = std::get_if<SetStatusTriggerCharges>(&action.quantity);
        if (!quantity)
            return rejectStatus(std::format(
                "必須使用「{}」", statusQuantityFieldLabel(fields.front())));
        if (!positiveQuantityField(quantity->count, fields.front())) return false;
        break;
    }
    case StatusQuantityModel::Marks:
    {
        const auto fields = statusQuantityOperationFields(
            StatusQuantityOperationId::SetMarks);
        assert(fields.size() == 1);
        const auto* quantity = std::get_if<SetStatusMarks>(&action.quantity);
        if (!quantity)
            return rejectStatus(std::format(
                "必須使用「{}」", statusQuantityFieldLabel(fields.front())));
        if (!positiveQuantityField(quantity->count, fields.front())) return false;
        break;
    }
    case StatusQuantityModel::DamageBlockCharges:
    {
        const auto addFields = statusQuantityOperationFields(
            StatusQuantityOperationId::AddDamageBlocks);
        const auto setFields = statusQuantityOperationFields(
            StatusQuantityOperationId::SetDamageBlocks);
        assert(addFields.size() == 2);
        assert(setFields.size() == 1);
        if (const auto* quantity = std::get_if<AddDamageBlockCharges>(&action.quantity))
        {
            if (!positiveQuantityField(quantity->count, addFields[0])
                || !positiveQuantityField(quantity->limit, addFields[1])) return false;
        }
        else if (const auto* quantity = std::get_if<SetDamageBlockCharges>(&action.quantity))
        {
            if (!positiveQuantityField(quantity->count, setFields.front())) return false;
        }
        else return rejectStatus("必須使用抵擋次數數量動詞");
        break;
    }
    case StatusQuantityModel::Internal:
        return rejectStatus("是執行期狀態，不可指定數量");
    }

    if (statusReapplicationPolicyRequired(action.status)
        && action.reapplication == StatusReapplicationPolicy::Implicit)
        return rejectStatus("需要「重複套用」");
    if (!statusReapplicationPolicyAllowed(action.status, action.reapplication))
        return rejectStatus("使用了不允許的「重複套用」方式");

    for (const auto& field : statusNamedNumberFieldCatalog)
    {
        const auto& value = statusNamedNumberField(action, field.id);
        if (!value) continue;
        if (!statusHasNamedNumberField(action.status, field.id))
            return rejectStatus(std::format("不可指定「{}」", field.label));
        if (!validateEffectNumberAtEvent(*value, event, context, error)) return false;
        switch (field.constraint)
        {
        case StatusNamedNumberConstraint::Positive:
            if (!effectNumberMustBePositive(*value))
            {
                return rejectStatus(std::format(
                    "的「{}」必須保證為正數", field.label));
            }
            break;
        }
    }
    for (const auto fieldId : statusNamedNumberFields(action.status))
    {
        const auto& field = statusNamedNumberFieldCatalogEntry(fieldId);
        if (field.required && !statusNamedNumberField(action, fieldId))
            return rejectStatus(std::format("需要「{}」", field.label));
    }

    if ((catalog.behaviorClassification == StatusBehaviorClassification::Profiled
            || catalog.behaviorClassification == StatusBehaviorClassification::OpenMarker)
        && !action.behavior)
        return rejectStatus("需要「效果」以完整描述狀態行為");
    if (catalog.behaviorClassification == StatusBehaviorClassification::Intrinsic
        && action.behavior)
        return rejectStatus("由內建群組語意決定行為，不可指定「效果」");
    if (catalog.behaviorClassification == StatusBehaviorClassification::CatalogOwned)
    {
        if (!action.behavior
            || !statusBehaviorsEquivalent(
                action.behavior,
                makeCatalogOwnedStatusBehavior(action)))
        {
            return rejectStatus("必須使用目錄決定的完整行為");
        }
    }

    if (action.behavior)
    {
        if (action.behavior->rules.empty())
            return rejectStatus("的「效果」不可為空");
        for (const auto& behaviorRule : action.behavior->rules)
        {
            if (!validateEffectRule(
                    behaviorRule,
                    error,
                    EffectRuleAuthoringContext::StatusBehavior))
                return false;
            bool numbersValid = true;
            const auto inspect = [&](const EffectNumber& number)
            {
                if (!numbersValid
                    || number.statusScale != StatusNumberScale::PerContributionLayer)
                    return;
                if (catalog.quantity != StatusQuantityModel::Layers
                    && catalog.quantity != StatusQuantityModel::SharedLayers)
                {
                    error = std::format(
                        "狀態「{}」不是層數狀態，不可使用「每層數值」",
                        battleStatusLabel(action.status));
                    numbersValid = false;
                    return;
                }
                const int limit = std::visit([](const auto& quantity) -> int
                {
                    using T = std::decay_t<decltype(quantity)>;
                    if constexpr (std::is_same_v<T, AddStatusLayers>)
                        return quantity.limit;
                    else if constexpr (std::is_same_v<T, AddSharedStatusLayers>)
                        return quantity.targetTotalLimit;
                    else
                        return 0;
                }, action.quantity);
                assert(limit > 0);
                if (!effectNumberProductFitsInt(number, limit))
                {
                    error = "「每層數值」乘上層數上限後可能超出整數範圍";
                    numbersValid = false;
                }
            };
            forEachDirectEffectNumber(behaviorRule, inspect);
            if (!numbersValid) return false;
        }
    }

    if (catalog.behaviorClassification == StatusBehaviorClassification::Profiled
        && catalog.behaviorProfile != StatusBehaviorProfile::None)
    {
        std::string missingCapability;
        if (!validateMinimumStatusBehaviorProfile(
                action, catalog.behaviorProfile, missingCapability))
        {
            return rejectStatus(std::format(
                "的效果缺少必要能力「{}」", missingCapability));
        }
    }

    if (action.status == BattleStatusKind::Poison)
    {
        if (action.reapplication == StatusReapplicationPolicy::KeepHigherDamage
            && action.poisonSameEventMerge != PoisonSameEventMerge::SumDamagePercent)
            return reject("保留較高傷害的中毒必須合計同事件傷害百分比");
        if (action.reapplication == StatusReapplicationPolicy::ReplaceExistingPoison
            && action.poisonSameEventMerge != PoisonSameEventMerge::None)
            return reject("取代現有中毒不可合併同事件傷害");
    }
    else if (action.poisonSameEventMerge != PoisonSameEventMerge::None)
    {
        return reject("只有中毒可指定同事件合併方式");
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

bool validateActionPayload(
    const EffectAction& action,
    EffectEvent event,
    EffectRuleAuthoringContext context,
    std::string& error)
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
                if (!validateEffectNumberAtEvent(typed.amount, event, context, error)) return false;
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
                if (!validateEffectNumberAtEvent(typed.amount, event, context, error)) return false;
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
                if (typed.operation == DamageModifierOperation::CapSingleHitAtValue
                    && (event != EffectEvent::StatusPersistent
                        || typed.perspective != DamageModifierPerspective::Incoming
                        || typed.stage != DamageModifierStage::Final
                        || typed.channel != DamageChannel::All))
                    return reject("單次承傷上限只支援狀態持續效果的承受方、最終階段與全部傷害");
                if (typed.operation == DamageModifierOperation::CapSingleHitAtValue
                    && !effectNumberMustBePositive(typed.amount))
                    return reject("單次承傷上限必須保證為正數");
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
                if (!validateEffectNumberAtEvent(typed.amount, event, context, error)) return false;
                if (!effectNumberCannotBeNegative(typed.amount))
                    return reject("資源變更數值不可產生負數");
                if (typed.additionalAmount)
                {
                    if (!validateEffectNumberAtEvent(*typed.additionalAmount, event, context, error)) return false;
                    if (!effectNumberCannotBeNegative(*typed.additionalAmount))
                        return reject("資源變更附加數值不可產生負數");
                }
                if ((typed.kind == ResourceChangeKind::Transfer) != typed.transferDestination.has_value())
                    return reject("只有轉移資源需要且必須提供轉移目標");
                if (typed.sourceShieldMaxHpPct)
                {
                    if (typed.resource != BattleResource::Shield || typed.kind != ResourceChangeKind::Grant)
                        return reject("來源護盾上限只支援增加護盾");
                    if (*typed.sourceShieldMaxHpPct <= 0)
                        return reject("來源護盾上限百分比必須為正數");
                    if (event == EffectEvent::BattleInitialized)
                        return reject("來源護盾上限不支援開場資源");
                }
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
                if (typed.healRequiresFullMp
                    && (typed.resource != BattleResource::Hp
                        || typed.kind != ResourceChangeKind::Restore))
                    return reject("僅滿內力時只支援回復生命");
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
                return validateStatusApplication(typed, event, context, error);
            }
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
            {
                if (typed.quantity <= 0) return reject("消耗狀態數量必須為正數");
                if (typed.source == StatusSourceMatch::CurrentContribution)
                    return reject("一般消耗狀態不支援「此狀態貢獻」來源；狀態效果請使用「消耗此狀態」");
                if (typed.whenDepleted)
                {
                    EffectAction nested{ EffectActionValue{ *typed.whenDepleted } };
                    if (!validateActionPayload(nested, event, context, error)) return false;
                }
            }
            else if constexpr (std::is_same_v<T, ConsumeThisStatusAction>)
            {
                if (typed.quantity <= 0) return reject("消耗此狀態數量必須為正數");
                if (typed.whenDepleted)
                {
                    EffectAction nested{ EffectActionValue{ *typed.whenDepleted } };
                    if (!validateActionPayload(nested, event, context, error)) return false;
                }
            }
            else if constexpr (std::is_same_v<T, SuppressCurrentCastContactsAction>)
            {
                if (typed.originalTargetShield)
                {
                    if (!validateEffectNumberAtEvent(
                            *typed.originalTargetShield, event, context, error)) return false;
                    if (!effectNumberMustBePositive(*typed.originalTargetShield))
                        return reject("原攻擊目標獲得護盾必須保證為正數");
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
                if (typed.source == StatusSourceMatch::CurrentContribution)
                    return reject("一般移除狀態不支援「此狀態貢獻」來源");
            }
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                if (!validateEffectNumberAtEvent(typed.amount, event, context, error)) return false;
                if (typed.transactionCount
                    && (!validateEffectNumberAtEvent(*typed.transactionCount, event, context, error)
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
                if (typed.independentProjectile)
                {
                    const auto& projectile = *typed.independentProjectile;
                    if (projectile.visualEffectId < 0 || projectile.speed <= 0 || projectile.lifetimeFrames <= 0
                        || projectile.magicPower < 0
                        || !typed.addToBaseAttack || !typed.source || typed.damageOverride
                        || typed.targets != AttackTargetPolicy::SelectedTargets)
                        return reject("獨立彈體需要有效外觀、正速度與壽命，非負武功威力、追加攻擊、來源和指定目標，不可覆寫直接傷害");
                }

                if (typed.pattern.projectileCount <= 0) return reject("攻擊彈道數量必須為正數");
                if (typed.pattern.intervalFrames < 0) return reject("攻擊間隔幀數不可為負數");
                if (typed.strengthPct < 0 || typed.sameTargetHitLimit < 0) return reject("攻擊倍率與同目標上限不可為負數");
                if (typed.projectileClearRadiusPct != 0 && typed.projectileClearRadiusPct < 100)
                    return reject("清除彈道半徑百分比須為零或至少100");
                if (typed.source)
                {
                    if (!validateSelectorSchema(*typed.source, error)
                        || !validateSelectorAtEvent(*typed.source, event, error)) return false;
                    if (!selectorHasSingleResult(*typed.source))
                        return reject("攻擊來源選擇器必須只選取一個單位");
                }
                if (typed.damageOverride && !validateEffectNumberAtEvent(*typed.damageOverride, event, context, error)) return false;
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
                                if (behavior.projectileCount > 0 && behavior.bleedStacks > 0
                                    && behavior.baseFrames > 0 && behavior.framesPerStar > 0) return true;
                                return reject("擴張螺旋需要正彈道數量、正流血層數、正基礎幀數與正每星幀數");
                            }
                            else
                            {
                                static_assert(AlwaysFalse<B>,
                                    "未處理的攻擊執行行為 payload 驗證");
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
                {
                    if (!validateEffectNumberAtEvent(modifier.amount, event, context, error)) return false;
                    if (modifier.kind == AreaModifierKind::PeriodicDamage
                        && (modifier.intervalFrames <= 0 || !effectNumberCannotBeNegative(modifier.amount)
                            || modifier.relation != EffectTeamFilter::Enemy || modifier.overlap != AreaOverlapPolicy::Add))
                        return reject("週期傷害需要非負數值、正間隔、敵方關係與相加重疊");
                    if (modifier.kind == AreaModifierKind::DamageRedirect
                        && (modifier.percent < 0 || modifier.percent > 100
                            || modifier.relation != EffectTeamFilter::Ally || modifier.overlap != AreaOverlapPolicy::KeepStrongest
                            || typed.anchor != AreaAnchor::FollowSourceUnit
                            || typed.sourceDeath != AreaSourceDeathPolicy::RemoveImmediately))
                        return reject("傷害轉移需要 0 至 100 減傷、友方取最強、跟隨來源且來源死亡立即移除");
                }
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
                if (typed.mpCost && !validateEffectNumberAtEvent(*typed.mpCost, event, context, error)) return false;
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
                            if (!validateEffectNumberAtEvent(machine.sourceCount, event, context, error)
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
                        else if constexpr (std::is_same_v<M, RecordMaximumDamageAction>)
                        {
                            // 只有封閉 enum 欄位，沒有額外數值限制。
                        }
                        else
                        {
                            static_assert(AlwaysFalse<M>,
                                "未處理的狀態機 payload 驗證");
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
                    if (!validateActionPayload(nested, event, context, error)) return false;
                for (const auto& nested : typed->whenFalse)
                    if (!validateActionPayload(nested, event, context, error)) return false;
            }
            else if constexpr (std::is_same_v<T, MakeIncomingAttackMissAction>
                || std::is_same_v<T, BlockPositiveDamageAction>)
            {
                // 純旗標動作，沒有 payload 可驗證。
            }
            else
            {
                static_assert(AlwaysFalse<T>, "未處理的效果動作 payload 驗證");
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
        case 14: return reject("使本次施放攻擊落空只允許命中傷害前事件");
        case 15: return reject("使本次受到攻擊落空只允許命中傷害前事件");
        case 16: return reject("抵擋非處決正傷害只允許狀態持續事件");
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
        case EffectNumberBase::SourceStatusQuantity:
        case EffectNumberBase::CurrentContributionQuantity:
        case EffectNumberBase::StoredStateValue:
        case EffectNumberBase::ApplicationTargetMaxHp:
        case EffectNumberBase::BoundRatio:
        case EffectNumberBase::Count:
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
            if (typed.additionalAmount && !validateBattleInitializedNumber(*typed.additionalAmount, error)) return false;
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

bool containsAttackInterceptor(const EffectAction& action)
{
    if (std::holds_alternative<SuppressCurrentCastContactsAction>(action.value)
        || std::holds_alternative<MakeIncomingAttackMissAction>(action.value))
    {
        return true;
    }
    const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
        &action.value);
    if (!conditional) return false;
    assert(*conditional);
    const auto contains = [](const std::vector<EffectAction>& actions)
    {
        return std::ranges::any_of(actions, containsAttackInterceptor);
    };
    return contains((*conditional)->whenTrue)
        || contains((*conditional)->whenFalse);
}

bool containsPoisonSameEventMerge(const EffectAction& action)
{
    return std::visit(
        [](const auto& typed) -> bool
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, ApplyStatusAction>)
            {
                return typed.status == BattleStatusKind::Poison
                    && typed.poisonSameEventMerge
                        == PoisonSameEventMerge::SumDamagePercent;
            }
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>
                || std::is_same_v<T, ConsumeThisStatusAction>)
            {
                return typed.whenDepleted
                    && typed.whenDepleted->status == BattleStatusKind::Poison
                    && typed.whenDepleted->poisonSameEventMerge
                        == PoisonSameEventMerge::SumDamagePercent;
            }
            else if constexpr (std::is_same_v<T,
                std::shared_ptr<ConditionalEffectAction>>)
            {
                assert(typed);
                const auto contains = [](const std::vector<EffectAction>& actions)
                {
                    return std::ranges::any_of(
                        actions, containsPoisonSameEventMerge);
                };
                return contains(typed->whenTrue) || contains(typed->whenFalse);
            }
            return false;
        },
        action.value);
}

bool isDirectPoisonSameEventMerge(const EffectAction& action)
{
    const auto* application = std::get_if<ApplyStatusAction>(&action.value);
    return application
        && application->status == BattleStatusKind::Poison
        && application->poisonSameEventMerge
            == PoisonSameEventMerge::SumDamagePercent;
}

}  // namespace

bool validateEffectRule(
    const EffectRule& rule,
    std::string& error,
    EffectRuleAuthoringContext context)
{
    if (rule.observation == EffectObservationScope::ComboMemberEventSource && rule.everyNthEvent <= 0)
    {
        error = "同門出招觀察需要正出招次數";
        return false;
    }

    error.clear();
    const bool intrinsicRule = isIntrinsicEffectRuleId(rule.id);
    const bool runtimeIntrinsic =
        context == EffectRuleAuthoringContext::RuntimeIntrinsicStatusBehavior;
    if (intrinsicRule != runtimeIntrinsic)
    {
        error = intrinsicRule
            ? "作者規則 ID 不可使用保留的內建狀態識別空間"
            : "內建狀態效果驗證只接受保留的內建規則 ID";
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
    const bool statusBehavior = effectObservationScopeUsesStatusContext(
        rule.observation)
        || rule.event == EffectEvent::StatusPersistent
        || rule.selector.kind == EffectSelectorKind::StatusHolder;
    if (!effectObservationScopeAllowedAtEvent(
            rule.observation,
            rule.event,
            statusBehavior))
    {
        error = "觀察範圍不支援此時機或作者脈絡";
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
        if (!validateEffectNumberAtEvent(
                *rule.repetitionCount, rule.event, context, error))
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
            || !validateActionPayload(action, rule.event, context, error)) return false;
    }
    const bool hasPoisonSameEventMerge = std::ranges::any_of(
        rule.actions, containsPoisonSameEventMerge);
    if (hasPoisonSameEventMerge)
    {
        if (context != EffectRuleAuthoringContext::Configured)
        {
            error = "中毒「同事件合併」只能寫在頂層設定規則";
            return false;
        }
        if (rule.actions.size() != 1
            || !isDirectPoisonSameEventMerge(rule.actions.front()))
        {
            error = "中毒「同事件合併」必須是規則中唯一且直接指定的動作";
            return false;
        }
        if (rule.event != EffectEvent::HitBeforeDamage)
        {
            error = "中毒「同事件合併」只支援「命中」時機";
            return false;
        }
        if (rule.selector.kind != EffectSelectorKind::HitTarget)
        {
            error = "中毒「同事件合併」必須使用「命中目標」";
            return false;
        }
        if (!rule.conditions.empty()
            || rule.chancePct != 100
            || rule.maxActivations != 0
            || rule.sharedCooldownFrames != 0
            || rule.intervalFrames != 0
            || rule.everyNthEvent != 0
            || rule.activationLimit
            || rule.repetitionCount)
        {
            error = "中毒「同事件合併」是事件前置合併規則，不可設定條件、機率或觸發記帳欄位";
            return false;
        }
    }
    if (rule.event == EffectEvent::StatusPersistent
        && !validatePersistentStatusRule(rule, error)) return false;
    const bool hasAttackInterceptor = std::ranges::any_of(
        rule.actions,
        containsAttackInterceptor);
    const bool isExclusiveDirectAttackInterceptor = rule.actions.size() == 1
        && (std::holds_alternative<SuppressCurrentCastContactsAction>(
                rule.actions.front().value)
            || std::holds_alternative<MakeIncomingAttackMissAction>(
                rule.actions.front().value));
    if (hasAttackInterceptor && !isExclusiveDirectAttackInterceptor)
    {
        error = "攻擊落空攔截動作必須是規則中唯一且直接指定的動作；成功後效果請寫在攔截動作內";
        return false;
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
    if (context == EffectRuleAuthoringContext::Configured)
    {
        if (ruleUsesStatusContext(rule))
        {
            error = "狀態持有者、持續時機與狀態生命週期動作只能寫在「套用狀態.效果」內";
            return false;
        }
    }
    else
    {
        if (!statusObservationAllowed(rule.observation))
        {
            error = "狀態效果必須使用狀態生命週期的「觀察範圍」";
            return false;
        }
        if (hasExactRuntimeAction || hasUnsupportedComposition)
        {
            error = "狀態效果不可包含精確 runtime 階段動作；此類動作目前只能寫在頂層效果規則";
            return false;
        }
    }
    return true;
}

bool validateEffectRules(std::span<const EffectRule> rules, std::string& error)
{
    for (const auto& rule : rules)
    {
        if (!validateEffectRule(rule, error)) return false;
    }
    return true;
}

}  // namespace KysChess
