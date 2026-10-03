#include "ChessBattleEffectSemantics.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <type_traits>

namespace KysChess
{
namespace
{

template <typename>
inline constexpr bool alwaysFalse = false;

}

std::string_view battleStatusLabel(BattleStatusKind status)
{
    return statusCatalogEntry(status).label;
}

std::int64_t roundEffectRatio(
    std::int64_t numerator,
    std::int64_t denominator,
    EffectRounding rounding)
{
    assert(denominator > 0);
    std::int64_t quotient = numerator / denominator;
    const std::int64_t remainder = numerator % denominator;
    if (remainder == 0) return quotient;

    switch (rounding)
    {
    case EffectRounding::TowardZero: return quotient;
    case EffectRounding::Floor: return numerator < 0 ? quotient - 1 : quotient;
    case EffectRounding::Ceil: return numerator > 0 ? quotient + 1 : quotient;
    case EffectRounding::Nearest:
    {
        const auto magnitude = remainder < 0
            ? static_cast<std::uint64_t>(-(remainder + 1)) + 1
            : static_cast<std::uint64_t>(remainder);
        const auto unsignedDenominator = static_cast<std::uint64_t>(denominator);
        const auto threshold = unsignedDenominator / 2 + unsignedDenominator % 2;
        if (magnitude >= threshold)
            quotient += numerator > 0 ? 1 : -1;
        return quotient;
    }
    }
    assert(false);
    return quotient;
}

int sumResourceAmounts(int first, int second)
{
    return static_cast<int>(std::clamp<std::int64_t>(static_cast<std::int64_t>(first) + second,
        std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
}

std::optional<int> effectiveConstantEffectNumberValue(
    const EffectNumber& number,
    int contributionQuantity)
{
    assert(contributionQuantity > 0);
    if ((number.base != EffectNumberBase::Constant
            && number.base != EffectNumberBase::BoundRatio)
        || number.multiplierBase)
    {
        return std::nullopt;
    }

    const auto saturatingMultiply = [](std::int64_t lhs, std::int64_t rhs)
    {
        if (lhs == 0 || rhs == 0) return std::int64_t{};
        if (lhs == -1 && rhs == std::numeric_limits<std::int64_t>::min())
            return std::numeric_limits<std::int64_t>::max();
        if (rhs == -1 && lhs == std::numeric_limits<std::int64_t>::min())
            return std::numeric_limits<std::int64_t>::max();
        if (lhs > 0)
        {
            if (rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() / rhs)
                return std::numeric_limits<std::int64_t>::max();
            if (rhs < 0 && rhs < std::numeric_limits<std::int64_t>::min() / lhs)
                return std::numeric_limits<std::int64_t>::min();
        }
        else
        {
            if (rhs > 0 && lhs < std::numeric_limits<std::int64_t>::min() / rhs)
                return std::numeric_limits<std::int64_t>::min();
            if (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::max() / rhs)
                return std::numeric_limits<std::int64_t>::max();
        }
        return lhs * rhs;
    };
    const auto saturatingAdd = [](std::int64_t lhs, std::int64_t rhs)
    {
        if (rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() - rhs)
            return std::numeric_limits<std::int64_t>::max();
        if (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::min() - rhs)
            return std::numeric_limits<std::int64_t>::min();
        return lhs + rhs;
    };
    const std::int64_t denominator = number.base == EffectNumberBase::BoundRatio
        ? number.boundDenominator
        : 1;
    assert(denominator > 0);
    const std::int64_t base = number.base == EffectNumberBase::BoundRatio
        ? number.boundNumerator
        : 0;
    const auto percentageDenominator = saturatingMultiply(denominator, 100);
    auto numerator = saturatingAdd(
        saturatingMultiply(base, number.percent),
        saturatingMultiply(number.flat, percentageDenominator));
    if (number.statusScale == StatusNumberScale::PerContributionLayer)
        numerator = saturatingMultiply(numerator, contributionQuantity);
    auto evaluated = roundEffectRatio(
        numerator,
        percentageDenominator,
        number.rounding);
    evaluated = std::clamp<std::int64_t>(
        evaluated,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max());
    int value = static_cast<int>(evaluated);
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

bool statusReapplicationPolicyAllowed(
    BattleStatusKind status,
    StatusReapplicationPolicy policy)
{
    switch (statusCatalogEntry(status).reapplication)
    {
    case StatusReapplicationModel::Implicit:
    case StatusReapplicationModel::CatalogKeepLongerDuration:
    case StatusReapplicationModel::CatalogRefreshDuration:
    case StatusReapplicationModel::CatalogReplaceSelected:
        return policy == StatusReapplicationPolicy::Implicit;
    case StatusReapplicationModel::StunDuration:
        return policy == StatusReapplicationPolicy::ExtendDuration
            || policy == StatusReapplicationPolicy::KeepLongerDuration;
    case StatusReapplicationModel::PoisonDamage:
        return policy == StatusReapplicationPolicy::KeepHigherDamage
            || policy == StatusReapplicationPolicy::ReplaceExistingPoison;
    case StatusReapplicationModel::AuthoredRefreshDuration:
        return policy == StatusReapplicationPolicy::RefreshDuration;
    }
    assert(false);
    return false;
}

bool statusReapplicationPolicyRequired(BattleStatusKind status)
{
    switch (statusCatalogEntry(status).reapplication)
    {
    case StatusReapplicationModel::StunDuration:
    case StatusReapplicationModel::PoisonDamage:
    case StatusReapplicationModel::AuthoredRefreshDuration:
        return true;
    case StatusReapplicationModel::Implicit:
    case StatusReapplicationModel::CatalogKeepLongerDuration:
    case StatusReapplicationModel::CatalogRefreshDuration:
    case StatusReapplicationModel::CatalogReplaceSelected:
        return false;
    }
    assert(false);
    return false;
}

bool statusBehaviorIsCatalogOwned(BattleStatusKind status)
{
    return statusCatalogEntry(status).behaviorClassification
        == StatusBehaviorClassification::CatalogOwned;
}

std::shared_ptr<const StatusBehaviorDefinition> makeCatalogOwnedStatusBehavior(
    const ApplyStatusAction& action)
{
    assert(statusBehaviorIsCatalogOwned(action.status));
    for (const auto& field : statusNamedNumberFieldCatalog)
    {
        const auto& value = statusNamedNumberField(action, field.id);
        assert(!value || statusHasNamedNumberField(action.status, field.id));
    }
    for (const auto fieldId : statusNamedNumberFields(action.status))
    {
        const auto& field = statusNamedNumberFieldCatalogEntry(fieldId);
        assert(!field.required || statusNamedNumberField(action, fieldId));
    }
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.observation = EffectObservationScope::StatusHolderEventSource;
    rule.selector.kind = EffectSelectorKind::StatusHolder;

    const auto append = [&](auto value)
    {
        rule.actions.push_back(EffectAction{ EffectActionValue{ std::move(value) } });
    };
    const auto allHealKinds = []
    {
        std::vector<std::string> result;
        result.reserve(effectHealKindCatalog.size());
        for (const auto& kind : effectHealKindCatalog)
            result.emplace_back(kind.authorLabel);
        return result;
    };

    switch (action.status)
    {
    case BattleStatusKind::Bleed:
    {
        rule.event = EffectEvent::FrameAdvanced;
        rule.intervalFrames = 10;
        DealDamageAction damage;
        damage.amount.base = EffectNumberBase::TargetMaxHp;
        damage.amount.percent = 1;
        damage.amount.minimum = 1;
        damage.amount.statusScale = StatusNumberScale::PerContributionLayer;
        damage.kind = BattleDamageKind::Bleed;
        append(std::move(damage));
        break;
    }
    case BattleStatusKind::ColdPoison:
    {
        rule.event = EffectEvent::StatusPersistent;
        ModifyHealTransactionAction healing;
        healing.operation = HealModifierOperation::Block;
        healing.kinds = allHealKinds();
        append(std::move(healing));

        ModifyAttributeAction speed;
        speed.attribute = BattleAttribute::Speed;
        speed.amount.flat = -25;
        speed.operation = AttributeOperation::PercentAdd;
        append(std::move(speed));
        break;
    }
    case BattleStatusKind::WitheredBone:
    {
        rule.event = EffectEvent::StatusPersistent;
        ModifyDamageAction damage;
        damage.perspective = DamageModifierPerspective::Incoming;
        damage.stage = DamageModifierStage::Final;
        damage.channel = DamageChannel::All;
        damage.amount.flat = 25;
        damage.operation = DamageModifierOperation::PercentAdd;
        append(std::move(damage));

        ModifyHealTransactionAction healing;
        healing.operation = HealModifierOperation::MultiplyReceived;
        healing.kinds = allHealKinds();
        healing.percent = 25;
        append(std::move(healing));
        break;
    }
    case BattleStatusKind::SevenStarMark:
    {
        rule.event = EffectEvent::HitBeforeDamage;
        rule.observation = EffectObservationScope::SourceOwnerTeamEventSource;
        rule.selector.kind = EffectSelectorKind::HitTarget;
        rule.conditions.push_back(TargetIsStatusHolderCondition{});

        ModifyDamageAction damage;
        damage.perspective = DamageModifierPerspective::Outgoing;
        damage.stage = DamageModifierStage::BeforeDefense;
        damage.channel = DamageChannel::Skill;
        damage.amount.flat = 50;
        damage.operation = DamageModifierOperation::IgnoreDefensePercent;
        append(std::move(damage));

        ApplyStatusAction stun;
        stun.status = BattleStatusKind::Stun;
        stun.durationFrames = 30;
        stun.quantity = NoStatusQuantity{};
        stun.reapplication = StatusReapplicationPolicy::KeepLongerDuration;
        ConsumeThisStatusAction consume;
        consume.quantity = 1;
        consume.whenDepleted = std::move(stun);
        append(std::move(consume));
        break;
    }
    case BattleStatusKind::NeutralizeForce:
    {
        assert(action.neutralizeMpRecovery);
        rule.event = EffectEvent::HitBeforeDamage;
        rule.selector.kind = EffectSelectorKind::HitTarget;
        ChangeResourceAction recovery;
        recovery.resource = BattleResource::Mp;
        recovery.kind = ResourceChangeKind::Restore;
        recovery.amount = *action.neutralizeMpRecovery;
        append(std::move(recovery));
        append(ConsumeThisStatusAction{});
        break;
    }
    case BattleStatusKind::Blinded:
    {
        rule.event = EffectEvent::HitBeforeDamage;
        append(SuppressCurrentCastContactsAction{});
        break;
    }
    case BattleStatusKind::Poison:
    case BattleStatusKind::Stun:
    case BattleStatusKind::MpBlocked:
    case BattleStatusKind::NextAttackMiss:
    case BattleStatusKind::DamageBlockLayer:
    case BattleStatusKind::SingleHitCapLayer:
    case BattleStatusKind::BattleSpirit:
    case BattleStatusKind::TrueQi:
    case BattleStatusKind::PoisonExplosion:
    case BattleStatusKind::Shadowless:
    case BattleStatusKind::SwordGuard:
    case BattleStatusKind::NextAttackCritical:
    case BattleStatusKind::Berserk:
        assert(false && "此狀態沒有目錄擁有的行為");
        break;
    }
    behavior->rules.push_back(std::move(rule));
    return behavior;
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
        else if constexpr (std::is_same_v<T, AddSharedStatusLayers>)
            return { quantity.count, EffectStackPolicy::AddStack,
                     quantity.targetTotalLimit };
        else if constexpr (std::is_same_v<T, SetStatusMarks>)
            return { quantity.count, EffectStackPolicy::Replace, quantity.count };
        else if constexpr (std::is_same_v<T, AddDamageBlockCharges>)
            return { quantity.count, EffectStackPolicy::AddStack, quantity.limit };
        else if constexpr (std::is_same_v<T, SetDamageBlockCharges>)
            return { quantity.count, EffectStackPolicy::Replace, quantity.count };
        else if constexpr (std::is_same_v<T, SetStatusTriggerCharges>)
            return { quantity.count, EffectStackPolicy::Replace, quantity.count };
    }, action.quantity);
}

EffectStackPolicy lowerStatusReapplication(const ApplyStatusAction& action)
{
    switch (action.reapplication)
    {
    case StatusReapplicationPolicy::Implicit:
        switch (statusCatalogEntry(action.status).reapplication)
        {
        case StatusReapplicationModel::CatalogKeepLongerDuration:
            return EffectStackPolicy::Refresh;
        case StatusReapplicationModel::CatalogRefreshDuration:
        case StatusReapplicationModel::CatalogReplaceSelected:
            return EffectStackPolicy::Replace;
        case StatusReapplicationModel::Implicit:
            return lowerStatusQuantity(action).stack;
        case StatusReapplicationModel::StunDuration:
        case StatusReapplicationModel::PoisonDamage:
        case StatusReapplicationModel::AuthoredRefreshDuration:
            assert(false && "需要作者策略的狀態不可降低隱含重複套用");
            return EffectStackPolicy::Independent;
        }
        assert(false);
        return EffectStackPolicy::Independent;
    case StatusReapplicationPolicy::ExtendDuration:
        return EffectStackPolicy::Independent;
    case StatusReapplicationPolicy::KeepLongerDuration:
        return EffectStackPolicy::Refresh;
    case StatusReapplicationPolicy::RefreshDuration:
        return EffectStackPolicy::Refresh;
    case StatusReapplicationPolicy::KeepHigherDamage:
        return EffectStackPolicy::KeepStrongest;
    case StatusReapplicationPolicy::ReplaceExistingPoison:
        return EffectStackPolicy::Replace;
    case StatusReapplicationPolicy::Count:
        break;
    }
    assert(false);
    return EffectStackPolicy::Independent;
}

bool battleAttributeUsesPercentagePoints(BattleAttribute attribute)
{
    switch (attribute)
    {
    case BattleAttribute::GuaranteedHit:
    case BattleAttribute::MaxHp:
    case BattleAttribute::Attack:
    case BattleAttribute::Defence:
    case BattleAttribute::Speed:
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

std::optional<CanonicalPoisonDamageCapability> canonicalPoisonDamageCapability(
    const StatusBehaviorDefinition& behavior)
{
    EffectSelector holder;
    holder.kind = EffectSelectorKind::StatusHolder;
    std::optional<CanonicalPoisonDamageCapability> result;
    int poisonDamageActionCount{};
    for (const auto& rule : behavior.rules)
    {
        const bool canonicalRule = rule.event == EffectEvent::FrameAdvanced
            && rule.observation == EffectObservationScope::StatusHolderEventSource
            && rule.castMatch == EffectCastMatch::BoundMagic
            && rule.selector == holder
            && rule.conditions.empty()
            && rule.chancePct == 100
            && rule.maxActivations == 0
            && rule.sharedCooldownFrames == 0
            && rule.intervalFrames == CanonicalPoisonIntervalFrames
            && rule.everyNthEvent == 0
            && !rule.activationLimit
            && !rule.repetitionCount;
        for (std::size_t index = 0; index < rule.actions.size(); ++index)
        {
            const auto* damage = std::get_if<DealDamageAction>(
                &rule.actions[index].value);
            if (!damage || damage->kind != BattleDamageKind::Poison) continue;
            ++poisonDamageActionCount;
            if (!canonicalRule || index + 1 >= rule.actions.size()) continue;
            const auto* consume = std::get_if<ConsumeThisStatusAction>(
                &rule.actions[index + 1].value);
            const auto& number = damage->amount;
            if (!consume
                || consume->quantity != 1
                || consume->whenDepleted
                || number.base != EffectNumberBase::TargetCurrentHp
                || number.multiplierBase
                || number.status
                || number.stateSlot
                || number.flat != 0
                || number.percent <= 0
                || number.rounding != EffectRounding::TowardZero
                || number.minimum != 1
                || number.maximum
                || number.statusScale != StatusNumberScale::Once
                || number.boundNumerator != 0
                || number.boundDenominator != 1
                || damage->transactionCount
                || !damage->appliesDamageModifiers
                || !damage->triggersHurtInvincibility
                || damage->area.kind != DamageAreaKind::SingleTarget
                || damage->area.radiusTiles != 0
                || damage->area.squareSideTiles != 0
                || damage->perCast.perTargetLimit != 0
                || damage->areaProjectiles)
            {
                continue;
            }
            if (result) return std::nullopt;
            result = CanonicalPoisonDamageCapability{
                .rule = &rule,
                .damage = damage,
                .consume = consume,
            };
        }
    }
    if (poisonDamageActionCount != 1) return std::nullopt;
    return result;
}

namespace
{

template <typename Action, typename Visitor>
void visitActionNumbers(Action& action, Visitor& visitor, bool recurseIntoStatusBehaviors);

template <typename Rule, typename Visitor>
void visitRuleNumbers(Rule& rule, Visitor& visitor, bool recurseIntoStatusBehaviors);

template <typename Apply, typename Visitor>
void visitAppliedStatusNumbers(
    Apply& action,
    Visitor& visitor,
    bool recurseIntoStatusBehaviors)
{
    if (action.duration) visitor(*action.duration);
    for (const auto& field : statusNamedNumberFieldCatalog)
    {
        auto& value = statusNamedNumberField(action, field.id);
        if (value) visitor(*value);
    }
    if (!recurseIntoStatusBehaviors || !action.behavior) return;
    // Catalog-owned behavior is a lowering artifact derived from the named
    // parameters above. It is validated through the catalog contract, but it
    // is not a second authored numeric surface and must not be visited twice.
    if (statusBehaviorIsCatalogOwned(action.status)) return;
    if constexpr (std::is_invocable_v<Visitor&, const EffectNumber&>)
    {
        for (const auto& rule : action.behavior->rules)
            visitRuleNumbers(rule, visitor, true);
    }
    else
    {
        assert(false && "可變數值走訪不得遞迴進入不可變的狀態行為");
    }
}

template <typename Action, typename Visitor>
void visitActionNumbers(
    Action& action,
    Visitor& visitor,
    bool recurseIntoStatusBehaviors)
{
    std::visit([&](auto& typed)
    {
        using T = std::remove_cvref_t<decltype(typed)>;
        if constexpr (std::is_same_v<T, ModifyAttributeAction>
            || std::is_same_v<T, ModifyDamageAction>
            || std::is_same_v<T, ChangeResourceAction>)
        {
            visitor(typed.amount);
            if constexpr (std::is_same_v<T, ChangeResourceAction>)
                if (typed.additionalAmount) visitor(*typed.additionalAmount);
        }
        else if constexpr (std::is_same_v<T, ApplyStatusAction>)
        {
            visitAppliedStatusNumbers(
                typed, visitor, recurseIntoStatusBehaviors);
        }
        else if constexpr (std::is_same_v<T, ConsumeStatusAction>
            || std::is_same_v<T, ConsumeThisStatusAction>)
        {
            if (typed.whenDepleted)
                visitAppliedStatusNumbers(
                    *typed.whenDepleted, visitor, recurseIntoStatusBehaviors);
        }
        else if constexpr (std::is_same_v<T, SuppressCurrentCastContactsAction>)
        {
            if (typed.originalTargetShield) visitor(*typed.originalTargetShield);
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
            for (auto& modifier : typed.modifiers) visitor(modifier.amount);
        }
        else if constexpr (std::is_same_v<T, ModifyCastAction>)
        {
            if (typed.mpCost) visitor(*typed.mpCost);
        }
        else if constexpr (std::is_same_v<T, StateMachineAction>)
        {
            std::visit([&](auto& machine)
            {
                using M = std::remove_cvref_t<decltype(machine)>;
                if constexpr (std::is_same_v<M, BorrowEffectRulesAction>)
                    visitor(machine.sourceCount);
                else if constexpr (std::is_same_v<M, ChangeStateValueAction>
                    || std::is_same_v<M, TransferStateValueAction>
                    || std::is_same_v<M, RecordMaximumDamageAction>
                    || std::is_same_v<M, ConsumeRecordedMaximumAction>
                    || std::is_same_v<M, StartDamageAbsorptionAction>
                    || std::is_same_v<M, SettleDamageAbsorptionAction>
                    || std::is_same_v<M, CopyAttackDefinitionAction>
                    || std::is_same_v<M, SettleRemainingStatusDamageAction>
                    || std::is_same_v<M, GenerateClonesAction>
                    || std::is_same_v<M, PreventDeathAction>
                    || std::is_same_v<M, ConfigureRescueRepositionAction>)
                {
                }
                else
                    static_assert(alwaysFalse<M>,
                        "StateMachineAction 數值走訪缺少新型別的明確處理");
            }, typed);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<ConditionalEffectAction>>)
        {
            assert(typed);
            for (auto& nested : typed->whenTrue)
                visitActionNumbers(nested, visitor, recurseIntoStatusBehaviors);
            for (auto& nested : typed->whenFalse)
                visitActionNumbers(nested, visitor, recurseIntoStatusBehaviors);
        }
        else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>
            || std::is_same_v<T, RemoveStatusAction>
            || std::is_same_v<T, ForceMoveAction>
            || std::is_same_v<T, MakeIncomingAttackMissAction>
            || std::is_same_v<T, BlockPositiveDamageAction>)
        {
        }
        else
            static_assert(alwaysFalse<T>,
                "EffectActionValue 數值走訪缺少新型別的明確處理");
    }, action.value);
}

template <typename Rule, typename Visitor>
void visitRuleNumbers(
    Rule& rule,
    Visitor& visitor,
    bool recurseIntoStatusBehaviors)
{
    if (rule.repetitionCount) visitor(*rule.repetitionCount);
    for (auto& action : rule.actions)
        visitActionNumbers(action, visitor, recurseIntoStatusBehaviors);
}

}  // namespace

void forEachEffectNumber(
    EffectRule& rule,
    const std::function<void(EffectNumber&)>& visitor)
{
    // Mutable traversal is used while binding one contribution generation.
    // A nested status gets its own binding snapshot when it is later applied.
    visitRuleNumbers(rule, visitor, false);
}

void forEachEffectNumber(
    const EffectRule& rule,
    const std::function<void(const EffectNumber&)>& visitor)
{
    visitRuleNumbers(rule, visitor, true);
}

void forEachDirectEffectNumber(
    const EffectRule& rule,
    const std::function<void(const EffectNumber&)>& visitor)
{
    visitRuleNumbers(rule, visitor, false);
}

}  // namespace KysChess
