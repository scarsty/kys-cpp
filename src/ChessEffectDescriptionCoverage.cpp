#include "ChessEffectDescriptionInternal.h"

#include <cassert>
#include <format>
#include <ranges>
#include <type_traits>

namespace KysChess::EffectDescriptionDetail
{
DescriptionSourceField descriptionSource(
    const EffectRule& rule,
    DescriptionSourceFieldKind kind,
    std::size_t variantIndex,
    std::string path)
{
    return {
        .ruleId = rule.id,
        .kind = kind,
        .variantIndex = variantIndex,
        .path = std::move(path),
    };
}

EffectDescriptionFact descriptionFact(
    DescriptionFactValue value,
    DescriptionSourceField source,
    DescriptionFactLevel level,
    DescriptionPlayerFact playerFact)
{
    return {
        .value = std::move(value),
        .sources = { std::move(source) },
        .level = level,
        .playerFact = playerFact,
    };
}

DescriptionTriggerFact::SubjectRole descriptionSubjectRole(
    EffectObservationScope observation)
{
    switch (observation)
    {
    case EffectObservationScope::Owner:
        return DescriptionTriggerFact::SubjectRole::EffectOwner;
    case EffectObservationScope::OwnerTeamEventSource:
        return DescriptionTriggerFact::SubjectRole::EventSource;
    case EffectObservationScope::EventTarget:
        return DescriptionTriggerFact::SubjectRole::EventTarget;
    }
    std::unreachable();
}

DescriptionSelectorFact resolveDescriptionSelector(
    const EffectSelector& selector)
{
    DescriptionTargetRole role{};
    switch (selector.kind)
    {
    case EffectSelectorKind::Self:
        role = DescriptionTargetRole::EffectOwner;
        break;
    case EffectSelectorKind::SourceUnit:
        role = DescriptionTargetRole::EventSource;
        break;
    case EffectSelectorKind::TransactionTarget:
    case EffectSelectorKind::HitTarget:
        role = DescriptionTargetRole::EventTarget;
        break;
    case EffectSelectorKind::OriginalAttackTarget:
        role = DescriptionTargetRole::OriginalAttackTarget;
        break;
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
    case EffectSelectorKind::AlliesUsingMartialCategory:
        role = DescriptionTargetRole::SelectedUnits;
        break;
    }
    return {
        .selector = selector,
        .role = role,
    };
}

DescriptionAction makeDescriptionAction(
    const EffectRule& rule,
    const EffectAction& action,
    std::string path);

DescriptionActionGroup makeDescriptionActionGroup(
    const EffectRule& rule,
    std::span<const EffectAction> actions,
    std::string_view path)
{
    DescriptionActionGroup result{
        .sequential = actionsDependOnOrder(actions),
    };
    for (std::size_t index = 0; index < actions.size(); ++index)
    {
        result.actions.push_back(makeDescriptionAction(
            rule,
            actions[index],
            std::format("{}[{}]", path, index)));
    }
    return result;
}

DescriptionAction makeDescriptionAction(
    const EffectRule& rule,
    const EffectAction& action,
    std::string path)
{
    const auto source = descriptionSource(
        rule,
        DescriptionSourceFieldKind::Action,
        action.value.index(),
        path);
    const auto* conditional = std::get_if<
        std::shared_ptr<ConditionalEffectAction>>(&action.value);
    if (!conditional)
    {
        return {
            .value = action,
            .sources = { source },
        };
    }

    assert(*conditional);
    auto branch = std::make_shared<DescriptionBranch>();
    for (std::size_t index = 0; index < (*conditional)->conditions.size(); ++index)
    {
        const auto& condition = (*conditional)->conditions[index];
        branch->conditions.push_back(descriptionFact(
            DescriptionConditionFact{condition},
            descriptionSource(
                rule,
                DescriptionSourceFieldKind::Condition,
                condition.index(),
                std::format("{}.conditions[{}]", path, index)),
            DescriptionFactLevel::Decision,
            DescriptionPlayerFact::Condition));
    }
    if (!(*conditional)->whenTrue.empty())
        branch->whenTrue.push_back(makeDescriptionActionGroup(
            rule,
            (*conditional)->whenTrue,
            path + ".whenTrue"));
    if (!(*conditional)->whenFalse.empty())
        branch->whenFalse.push_back(makeDescriptionActionGroup(
            rule,
            (*conditional)->whenFalse,
            path + ".whenFalse"));
    return {
        .value = std::move(branch),
        .sources = { source },
    };
}

void setDescriptionRuleOrder(
    EffectDescriptionBlock& block,
    std::size_t ruleOrder)
{
    block.sourceRuleOrder = ruleOrder;
    const auto setFacts = [ruleOrder](auto& facts)
    {
        for (auto& fact : facts)
            for (auto& source : fact.sources)
                source.ruleOrder = ruleOrder;
    };
    setFacts(block.trigger);
    setFacts(block.conditions);
    setFacts(block.targets);
    const auto setActions = [&](const auto& self, auto& groups) -> void
    {
        for (auto& group : groups)
        {
            for (auto& action : group.actions)
            {
                for (auto& source : action.sources)
                    source.ruleOrder = ruleOrder;
                if (auto* branch = std::get_if<
                        std::shared_ptr<DescriptionBranch>>(&action.value))
                {
                    assert(*branch);
                    setFacts((*branch)->conditions);
                    self(self, (*branch)->whenTrue);
                    self(self, (*branch)->whenFalse);
                }
            }
        }
    };
    setActions(setActions, block.actions);
    for (auto& entry : block.coverage.fields)
        entry.source.ruleOrder = ruleOrder;
}

void appendCoverage(
    DescriptionCoverage& coverage,
    const DescriptionSourceField& source,
    DescriptionFieldDisposition disposition,
    DescriptionFactLevel level,
    DescriptionPlayerProjection projection,
    std::optional<DescriptionPlayerFact> playerFact,
    std::string auditValue,
    std::string reason)
{
    coverage.fields.push_back({
        .source = source,
        .disposition = disposition,
        .level = level,
        .projection = projection,
        .playerFact = playerFact,
        .auditValue = std::move(auditValue),
        .reason = std::move(reason),
    });
}

void appendAuditCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    DescriptionSourceFieldKind kind,
    std::size_t variantIndex,
    std::string path,
    DescriptionFieldDisposition disposition,
    std::string auditValue,
    std::optional<DescriptionPlayerFact> playerFact)
{
    appendCoverage(
        coverage,
        descriptionSource(rule, kind, variantIndex, std::move(path)),
        disposition,
        DescriptionFactLevel::Audit,
        {},
        playerFact,
        std::move(auditValue),
        disposition == DescriptionFieldDisposition::SchemaDefault
            ? "正式預設值保留於 document audit trace"
            : "typed 欄位保留於 document fact 或 audit trace");
}

template<typename T>
std::string descriptionAuditValue(const T& value)
{
    if constexpr (std::is_enum_v<T>)
        return std::to_string(static_cast<int>(value));
    else if constexpr (std::is_same_v<T, bool>)
        return value ? "true" : "false";
    else if constexpr (std::is_integral_v<T>)
        return std::to_string(value);
    else if constexpr (std::is_same_v<T, std::string>)
        return value;
    else
        static_assert(false, "unsupported description audit scalar");
}

template<typename T>
std::string descriptionAuditValue(const std::vector<T>& values)
{
    std::string result = "[";
    for (const auto& value : values)
    {
        if (result.size() > 1) result += ",";
        result += descriptionAuditValue(value);
    }
    return result + "]";
}

void appendEffectNumberCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    DescriptionSourceFieldKind kind,
    std::size_t variantIndex,
    const EffectNumber& number,
    std::string_view path,
    std::optional<DescriptionPlayerFact> playerFact)
{
    const auto append = [&](
        std::string_view field,
        bool isDefault,
        std::string value)
    {
        appendAuditCoverage(
            coverage,
            rule,
            kind,
            variantIndex,
            std::format("{}.{}", path, field),
            isDefault
                ? DescriptionFieldDisposition::SchemaDefault
                : DescriptionFieldDisposition::Visible,
            std::move(value),
            playerFact);
    };
    append("base", number.base == EffectNumberBase::Constant,
        std::to_string(static_cast<int>(number.base)));
    append("multiplierBase", !number.multiplierBase,
        number.multiplierBase
            ? std::to_string(static_cast<int>(*number.multiplierBase))
            : "absent");
    append("status", !number.status,
        number.status
            ? std::to_string(static_cast<int>(*number.status))
            : "absent");
    append("stateSlot", !number.stateSlot,
        number.stateSlot
            ? std::to_string(static_cast<int>(*number.stateSlot))
            : "absent");
    append("flat", number.flat == 0, std::to_string(number.flat));
    append("percent", number.percent == 0, std::to_string(number.percent));
    append("rounding", number.rounding == EffectRounding::TowardZero,
        std::to_string(static_cast<int>(number.rounding)));
    append("minimum", !number.minimum,
        number.minimum ? std::to_string(*number.minimum) : "absent");
    append("maximum", !number.maximum,
        number.maximum ? std::to_string(*number.maximum) : "absent");
}

void appendSelectorCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    DescriptionSourceFieldKind kind,
    std::size_t variantIndex,
    const EffectSelector& selector,
    std::string_view path)
{
    const EffectSelector defaults;
    const auto append = [&](
        std::string_view field,
        bool isDefault,
        std::string value,
        DescriptionFieldDisposition nonDefaultDisposition = DescriptionFieldDisposition::Visible)
    {
        appendAuditCoverage(
            coverage,
            rule,
            kind,
            variantIndex,
            std::format("{}.{}", path, field),
            isDefault
                ? DescriptionFieldDisposition::SchemaDefault
                : nonDefaultDisposition,
            std::move(value));
    };
    append("kind", selector.kind == defaults.kind,
        std::to_string(static_cast<int>(selector.kind)));
    append("count", selector.count == defaults.count,
        std::to_string(selector.count));
    append("radiusTiles", selector.radiusTiles == defaults.radiusTiles,
        std::to_string(selector.radiusTiles));
    append("squareSideTiles", selector.squareSideTiles == defaults.squareSideTiles,
        std::to_string(selector.squareSideTiles));
    append("team", selector.team == defaults.team,
        std::to_string(static_cast<int>(selector.team)));
    append("tieBreak", selector.tieBreak == defaults.tieBreak,
        std::to_string(static_cast<int>(selector.tieBreak)));
    append("excludeOwner", selector.excludeOwner == defaults.excludeOwner,
        selector.excludeOwner ? "true" : "false");
    append("requiredBoundMagic", selector.requiredBoundMagic == defaults.requiredBoundMagic,
        selector.requiredBoundMagic ? "true" : "false");
    append("requiredMartialCategory",
        selector.requiredMartialCategory == defaults.requiredMartialCategory,
        selector.requiredMartialCategory == EffectMartialCategory::None
            ? "absent"
            : std::to_string(static_cast<int>(selector.requiredMartialCategory)));
    append("requiredTarget", selector.requiredTarget == defaults.requiredTarget,
        selector.requiredTarget
            ? std::to_string(static_cast<int>(*selector.requiredTarget))
            : "absent");
}

void appendConditionCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    const EffectCondition& condition,
    std::string_view path)
{
    const auto variantIndex = condition.index();
    appendAuditCoverage(
        coverage,
        rule,
        DescriptionSourceFieldKind::Condition,
        variantIndex,
        std::format("{}.variant", path),
        DescriptionFieldDisposition::Visible,
        std::to_string(variantIndex));
    std::visit(
        [&](const auto& typed)
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, SourceHpRatioAtMostCondition>
                || std::is_same_v<T, SourceHpRatioBelowCondition>
                || std::is_same_v<T, TargetHpRatioAtMostCondition>)
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Condition,
                    variantIndex, std::format("{}.percent", path),
                    DescriptionFieldDisposition::Visible, std::to_string(typed.percent));
            else if constexpr (std::is_same_v<T, SourceHasStateCondition>
                || std::is_same_v<T, TargetHasStateCondition>
                || std::is_same_v<T, TargetHasStateFromEffectOwnerCondition>)
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Condition,
                    variantIndex, std::format("{}.state", path),
                    DescriptionFieldDisposition::Visible,
                    std::to_string(static_cast<int>(typed.state)));
            else if constexpr (std::is_same_v<T, SourceStackAtLeastCondition>)
            {
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Condition,
                    variantIndex, std::format("{}.stack", path),
                    DescriptionFieldDisposition::Visible,
                    std::to_string(static_cast<int>(typed.stack)));
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Condition,
                    variantIndex, std::format("{}.count", path),
                    DescriptionFieldDisposition::Visible, std::to_string(typed.count));
            }
            else if constexpr (std::is_same_v<T, CastDistinctTargetCountAtLeastCondition>)
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Condition,
                    variantIndex, std::format("{}.count", path),
                    DescriptionFieldDisposition::Visible, std::to_string(typed.count));
            else if constexpr (std::is_same_v<T, AttackOrdinalEqualsCondition>)
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Condition,
                    variantIndex, std::format("{}.ordinal", path),
                    DescriptionFieldDisposition::Visible, std::to_string(typed.ordinal));
            else if constexpr (std::is_same_v<T, HealKindInCondition>
                || std::is_same_v<T, DamageKindInCondition>)
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Condition,
                    variantIndex, std::format("{}.kinds", path),
                    DescriptionFieldDisposition::Visible,
                    descriptionAuditValue(typed.kinds));
            else if constexpr (std::is_same_v<T, AcceptedHitCondition>)
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Condition,
                    variantIndex, std::format("{}.requirePositiveDamage", path),
                    typed.requirePositiveDamage
                        ? DescriptionFieldDisposition::Visible
                        : DescriptionFieldDisposition::SchemaDefault,
                    typed.requirePositiveDamage ? "true" : "false");
            else if constexpr (std::is_same_v<T, DamagePerspectiveCondition>)
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Condition,
                    variantIndex, std::format("{}.perspective", path),
                    DescriptionFieldDisposition::Visible,
                    std::to_string(static_cast<int>(typed.perspective)));
        },
        condition);
}

void appendApplyStatusCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    std::size_t variantIndex,
    const ApplyStatusAction& action,
    std::string_view path)
{
    const auto append = [&](std::string_view field, bool isDefault, std::string value)
    {
        appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
            variantIndex, std::format("{}.{}", path, field),
            isDefault
                ? DescriptionFieldDisposition::SchemaDefault
                : DescriptionFieldDisposition::Visible,
            std::move(value));
    };
    const ApplyStatusAction defaults;
    appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
        variantIndex, std::format("{}.status", path),
        DescriptionFieldDisposition::Visible,
        std::to_string(static_cast<int>(action.status)));
    append("durationFrames", action.durationFrames == defaults.durationFrames,
        std::to_string(action.durationFrames));
    append("duration", !action.duration, action.duration ? "present" : "absent");
    append("applicationCount", !action.applicationCount,
        action.applicationCount ? "present" : "absent");
    append("stacks", action.stacks == defaults.stacks, std::to_string(action.stacks));
    append("stack", action.stack == defaults.stack,
        std::to_string(static_cast<int>(action.stack)));
    append("stackLimit", !action.stackLimit,
        action.stackLimit ? std::to_string(*action.stackLimit) : "absent");
    append("aggregatePotencyWithinEvent",
        action.aggregatePotencyWithinEvent == defaults.aggregatePotencyWithinEvent,
        action.aggregatePotencyWithinEvent ? "true" : "false");
    if (action.duration)
        appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
            variantIndex, *action.duration, std::format("{}.duration.value", path));
    if (action.applicationCount)
        appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
            variantIndex, *action.applicationCount, std::format("{}.applicationCount.value", path));
    appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
        variantIndex, action.potency, std::format("{}.potency", path),
        DescriptionPlayerFact::StatusPotency);
    appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
        variantIndex, action.secondaryPotency, std::format("{}.secondaryPotency", path),
        DescriptionPlayerFact::StatusSecondaryPotency);
}

void appendAttackPatternCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    std::size_t variantIndex,
    const AttackPattern& pattern,
    std::string_view path)
{
    const AttackPattern defaults;
    const auto append = [&](std::string_view field, const auto& value, const auto& defaultValue)
    {
        appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
            variantIndex, std::format("{}.{}", path, field),
            value == defaultValue
                ? DescriptionFieldDisposition::SchemaDefault
                : DescriptionFieldDisposition::Visible,
            descriptionAuditValue(value));
    };
    append("kind", pattern.kind, defaults.kind);
    append("projectileCount", pattern.projectileCount, defaults.projectileCount);
    append("spreadDegrees", pattern.spreadDegrees, defaults.spreadDegrees);
    append("intervalFrames", pattern.intervalFrames, defaults.intervalFrames);
}

void appendActionCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    const EffectAction& action,
    std::string path);

void appendStateMachineCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    std::size_t actionVariantIndex,
    const StateMachineAction& machine,
    std::string_view path)
{
    appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
        actionVariantIndex, std::format("{}.variant", path),
        DescriptionFieldDisposition::Visible,
        std::to_string(machine.index()));
    std::visit(
        [&](const auto& typed)
        {
            using T = std::decay_t<decltype(typed)>;
            const auto scalarField = [&](std::string_view name,
                const auto& value,
                const auto& defaultValue,
                std::optional<DescriptionPlayerFact> playerFact = std::nullopt)
            {
                static_cast<void>(defaultValue);
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    actionVariantIndex, std::format("{}.{}", path, name),
                    DescriptionFieldDisposition::Visible,
                    descriptionAuditValue(value),
                    playerFact);
            };
            const auto optionalField = [&](std::string_view name,
                const auto& value)
            {
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    actionVariantIndex, std::format("{}.{}", path, name),
                    value
                        ? DescriptionFieldDisposition::Visible
                        : DescriptionFieldDisposition::SchemaDefault,
                    value ? descriptionAuditValue(*value) : "absent");
            };
            if constexpr (std::is_same_v<T, ChangeStateValueAction>)
            {
                const ChangeStateValueAction defaults;
                scalarField("slot", typed.slot, defaults.slot);
                scalarField("delta", typed.delta, defaults.delta);
                optionalField("minimum", typed.minimum);
                optionalField("maximum", typed.maximum);
            }
            else if constexpr (std::is_same_v<T, TransferStateValueAction>)
            {
                const TransferStateValueAction defaults;
                scalarField("sourceSlot", typed.sourceSlot, defaults.sourceSlot);
                scalarField("destinationSlot", typed.destinationSlot,
                    defaults.destinationSlot);
            }
            else if constexpr (std::is_same_v<T, RecordMaximumDamageAction>)
            {
                const RecordMaximumDamageAction defaults;
                scalarField("slot", typed.slot, defaults.slot);
                scalarField("channel", typed.channel, defaults.channel);
            }
            else if constexpr (std::is_same_v<T, ConsumeRecordedMaximumAction>)
            {
                const ConsumeRecordedMaximumAction defaults;
                scalarField("slot", typed.slot, defaults.slot);
                scalarField("destination", typed.destination, defaults.destination);
                scalarField("percent", typed.percent, defaults.percent);
                scalarField("clearAfterConsume", typed.clearAfterConsume,
                    defaults.clearAfterConsume);
            }
            else if constexpr (std::is_same_v<T, StartDamageAbsorptionAction>)
            {
                const StartDamageAbsorptionAction defaults;
                scalarField("slot", typed.slot, defaults.slot);
                scalarField("absorbedPct", typed.absorbedPct, defaults.absorbedPct);
                scalarField("durationFrames", typed.durationFrames, defaults.durationFrames);
                scalarField("settleOnSourceDeath", typed.settleOnSourceDeath,
                    defaults.settleOnSourceDeath);
                scalarField("settlementDamageKind", typed.settlementDamageKind,
                    defaults.settlementDamageKind);
                scalarField("returnedPct", typed.returnedPct, defaults.returnedPct);
                appendSelectorCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    actionVariantIndex, typed.settlementTarget, std::format("{}.settlementTarget", path));
            }
            else if constexpr (std::is_same_v<T, SettleDamageAbsorptionAction>)
            {
                const SettleDamageAbsorptionAction defaults;
                scalarField("slot", typed.slot, defaults.slot);
                scalarField("damageKind", typed.damageKind, defaults.damageKind);
                scalarField("returnedPct", typed.returnedPct, defaults.returnedPct);
                scalarField("clearAfterSettle", typed.clearAfterSettle,
                    defaults.clearAfterSettle);
                appendSelectorCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    actionVariantIndex, typed.target, std::format("{}.target", path));
            }
            else if constexpr (std::is_same_v<T, BorrowEffectRulesAction>)
            {
                const BorrowEffectRulesAction defaults;
                scalarField("filter.allowedActionCategories",
                    typed.filter.allowedActionCategories,
                    defaults.filter.allowedActionCategories,
                    DescriptionPlayerFact::BorrowedRuleActionCategories);
                scalarField("propagation", typed.propagation, defaults.propagation,
                    DescriptionPlayerFact::StateMachinePropagation);
                appendSelectorCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    actionVariantIndex, typed.sourceUnits, std::format("{}.sourceUnits", path));
                appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    actionVariantIndex, typed.sourceCount, std::format("{}.sourceCount", path));
            }
            else if constexpr (std::is_same_v<T, CopyAttackDefinitionAction>)
            {
                const CopyAttackDefinitionAction defaults;
                scalarField("filter.conditions", typed.filter.conditions,
                    defaults.filter.conditions,
                    DescriptionPlayerFact::CopiedMagicConditions);
                scalarField("copyCount", typed.copyCount, defaults.copyCount);
                scalarField("propagation", typed.propagation, defaults.propagation,
                    DescriptionPlayerFact::StateMachinePropagation);
                appendSelectorCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    actionVariantIndex, typed.sourceUnits, std::format("{}.sourceUnits", path));
            }
            else if constexpr (std::is_same_v<T, SettleRemainingStatusDamageAction>)
            {
                const SettleRemainingStatusDamageAction defaults;
                scalarField("status", typed.status, defaults.status);
            }
            else if constexpr (std::is_same_v<T, GenerateClonesAction>)
            {
                const GenerateClonesAction defaults;
                scalarField("count", typed.count, defaults.count);
            }
            else if constexpr (std::is_same_v<T, PreventDeathAction>)
            {
                const PreventDeathAction defaults;
                scalarField("invincibilityFrames", typed.invincibilityFrames,
                    defaults.invincibilityFrames);
            }
            else
            {
                const ConfigureRescueRepositionAction defaults;
                scalarField("mode", typed.mode, defaults.mode);
                scalarField("activations", typed.activations, defaults.activations);
            }
        },
        machine);
}

void appendActionCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    const EffectAction& action,
    std::string path)
{
    const auto variantIndex = action.value.index();
    appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
        variantIndex, std::format("{}.variant", path),
        DescriptionFieldDisposition::Visible,
        std::to_string(variantIndex));
    std::visit(
        [&](const auto& typed)
        {
            using T = std::decay_t<decltype(typed)>;
            const auto scalarField = [&](
                std::string_view name,
                const auto& value,
                const auto& defaultValue)
            {
                appendAuditCoverage(
                    coverage,
                    rule,
                    DescriptionSourceFieldKind::Action,
                    variantIndex,
                    std::format("{}.{}", path, name),
                    value == defaultValue
                        ? DescriptionFieldDisposition::SchemaDefault
                        : DescriptionFieldDisposition::Visible,
                    descriptionAuditValue(value));
            };
            const auto optionalField = [&]( 
                std::string_view name,
                bool present,
                std::string value)
            {
                appendAuditCoverage(
                    coverage,
                    rule,
                    DescriptionSourceFieldKind::Action,
                    variantIndex,
                    std::format("{}.{}", path, name),
                    present
                        ? DescriptionFieldDisposition::Visible
                        : DescriptionFieldDisposition::SchemaDefault,
                    std::move(value));
            };
            const auto requiredField = [&]<typename Value>(
                std::string_view name,
                const Value& value)
            {
                appendAuditCoverage(
                    coverage,
                    rule,
                    DescriptionSourceFieldKind::Action,
                    variantIndex,
                    std::format("{}.{}", path, name),
                    DescriptionFieldDisposition::Visible,
                    descriptionAuditValue(value));
            };
            if constexpr (std::is_same_v<T, ModifyAttributeAction>)
            {
                const ModifyAttributeAction defaults;
                requiredField("attribute", typed.attribute);
                requiredField("operation", typed.operation);
                scalarField("durationFrames", typed.durationFrames, defaults.durationFrames);
                scalarField("stack", typed.stack, defaults.stack);
                optionalField("stackLimit", typed.stackLimit.has_value(),
                    typed.stackLimit ? std::to_string(*typed.stackLimit) : "absent");
                scalarField("perStack", typed.perStack, defaults.perStack);
                scalarField("stackScope", typed.stackScope, defaults.stackScope);
                appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    variantIndex, typed.amount, std::format("{}.amount", path));
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                const ModifyDamageAction defaults;
                scalarField("perspective", typed.perspective, defaults.perspective);
                requiredField("stage", typed.stage);
                requiredField("channel", typed.channel);
                requiredField("operation", typed.operation);
                scalarField("durationFrames", typed.durationFrames, defaults.durationFrames);
                scalarField("stack", typed.stack, defaults.stack);
                optionalField("stackLimit", typed.stackLimit.has_value(),
                    typed.stackLimit ? std::to_string(*typed.stackLimit) : "absent");
                scalarField("stackScope", typed.stackScope, defaults.stackScope);
                appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    variantIndex, typed.amount, std::format("{}.amount", path));
            }
            else if constexpr (std::is_same_v<T, ChangeResourceAction>)
            {
                const ChangeResourceAction defaults;
                requiredField("resource", typed.resource);
                requiredField("kind", typed.kind);
                optionalField("transferDestination", typed.transferDestination.has_value(),
                    typed.transferDestination ? "present" : "absent");
                scalarField("healKind", typed.healKind, defaults.healKind);
                scalarField("healSourcePolicy", typed.healSourcePolicy, defaults.healSourcePolicy);
                appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    variantIndex, typed.amount, std::format("{}.amount", path));
                if (typed.transferDestination)
                    appendSelectorCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                        variantIndex, *typed.transferDestination, std::format("{}.transferDestination.value", path));
            }
            else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                const ModifyHealTransactionAction defaults;
                requiredField("operation", typed.operation);
                requiredField("kinds", typed.kinds);
                scalarField("percent", typed.percent, defaults.percent);
            }
            else if constexpr (std::is_same_v<T, ApplyStatusAction>)
                appendApplyStatusCoverage(coverage, rule, variantIndex, typed, path);
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>)
            {
                const ConsumeStatusAction defaults;
                requiredField("status", typed.status);
                scalarField("stacks", typed.stacks, defaults.stacks);
                scalarField("source", typed.source, defaults.source);
                optionalField("whenDepleted", typed.whenDepleted.has_value(),
                    typed.whenDepleted ? "present" : "absent");
                if (typed.whenDepleted)
                    appendApplyStatusCoverage(coverage, rule, variantIndex,
                        *typed.whenDepleted, std::format("{}.whenDepleted.value", path));
            }
            else if constexpr (std::is_same_v<T, RemoveStatusAction>)
            {
                const RemoveStatusAction defaults;
                scalarField("statuses", typed.statuses, defaults.statuses);
                scalarField("negativeOnly", typed.negativeOnly, defaults.negativeOnly);
                scalarField("controlOnly", typed.controlOnly, defaults.controlOnly);
                scalarField("clearCurrentActionStagger", typed.clearCurrentActionStagger,
                    defaults.clearCurrentActionStagger);
                scalarField("count", typed.count, defaults.count);
                scalarField("order", typed.order, defaults.order);
            }
            else if constexpr (std::is_same_v<T, DealDamageAction>)
            {
                const DealDamageAction defaults;
                optionalField("transactionCount", typed.transactionCount.has_value(),
                    typed.transactionCount ? "present" : "absent");
                requiredField("kind", typed.kind);
                scalarField("appliesDamageModifiers", typed.appliesDamageModifiers,
                    defaults.appliesDamageModifiers);
                scalarField("triggersHurtInvincibility", typed.triggersHurtInvincibility,
                    defaults.triggersHurtInvincibility);
                scalarField("area.kind", typed.area.kind, defaults.area.kind);
                scalarField("area.radiusTiles", typed.area.radiusTiles, defaults.area.radiusTiles);
                scalarField("area.squareSideTiles", typed.area.squareSideTiles,
                    defaults.area.squareSideTiles);
                scalarField("perCast.perTargetLimit", typed.perCast.perTargetLimit,
                    defaults.perCast.perTargetLimit);
                optionalField("areaProjectiles", typed.areaProjectiles.has_value(),
                    typed.areaProjectiles ? "present" : "absent");
                appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    variantIndex, typed.amount, std::format("{}.amount", path));
                if (typed.transactionCount)
                    appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                        variantIndex, *typed.transactionCount, std::format("{}.transactionCount.value", path));
                if (typed.areaProjectiles)
                {
                    const AreaProjectileDamageDelivery defaults;
                    const auto& delivery = *typed.areaProjectiles;
                    requiredField("areaProjectiles.value.rangeTiles", delivery.rangeTiles);
                    requiredField("areaProjectiles.value.maximumTargets", delivery.maximumTargets);
                    requiredField("areaProjectiles.value.stunFrames", delivery.stunFrames);
                    scalarField("areaProjectiles.value.trackEventSource", delivery.trackEventSource,
                        defaults.trackEventSource);
                    requiredField("areaProjectiles.value.visual", delivery.visual);
                }
            }
            else if constexpr (std::is_same_v<T, ModifyAttackAction>)
            {
                const ModifyAttackAction defaults;
                scalarField("strengthPct", typed.strengthPct, defaults.strengthPct);
                optionalField("through", typed.through.has_value(),
                    typed.through ? descriptionAuditValue(*typed.through) : "absent");
                optionalField("tracking", typed.tracking.has_value(),
                    typed.tracking ? descriptionAuditValue(*typed.tracking) : "absent");
                scalarField("mainProjectile", typed.mainProjectile, defaults.mainProjectile);
                scalarField("sameTargetHitLimit", typed.sameTargetHitLimit,
                    defaults.sameTargetHitLimit);
                scalarField("targets", typed.targets, defaults.targets);
                scalarField("propagation", typed.propagation, defaults.propagation);
                scalarField("addToBaseAttack", typed.addToBaseAttack, defaults.addToBaseAttack);
                optionalField("source", typed.source.has_value(),
                    typed.source ? "present" : "absent");
                optionalField("damageOverride", typed.damageOverride.has_value(),
                    typed.damageOverride ? "present" : "absent");
                optionalField("damageKind", typed.damageKind.has_value(),
                    typed.damageKind ? descriptionAuditValue(*typed.damageKind) : "absent");
                appendAuditCoverage(
                    coverage,
                    rule,
                    DescriptionSourceFieldKind::Action,
                    variantIndex,
                    std::format("{}.runtimeBehavior.kind", path),
                    std::holds_alternative<std::monostate>(typed.runtimeBehavior)
                        ? DescriptionFieldDisposition::SchemaDefault
                        : DescriptionFieldDisposition::Visible,
                    std::to_string(typed.runtimeBehavior.index()));
                appendAttackPatternCoverage(coverage, rule, variantIndex,
                    typed.pattern, std::format("{}.pattern", path));
                if (typed.source)
                    appendSelectorCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                        variantIndex, *typed.source, std::format("{}.source.value", path));
                if (typed.damageOverride)
                    appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                        variantIndex, *typed.damageOverride, std::format("{}.damageOverride.value", path));
                std::visit([&](const auto& behavior)
                {
                    using B = std::decay_t<decltype(behavior)>;
                    if constexpr (std::is_same_v<B, ProjectileBounceAttackBehavior>)
                    {
                        const ProjectileBounceAttackBehavior defaults;
                        scalarField("runtimeBehavior.additionalHits", behavior.additionalHits,
                            defaults.additionalHits);
                        scalarField("runtimeBehavior.chancePct", behavior.chancePct,
                            defaults.chancePct);
                        scalarField("runtimeBehavior.rangePixels", behavior.rangePixels,
                            defaults.rangePixels);
                    }
                    else if constexpr (std::is_same_v<B, NearbyTrackingAttackBehavior>)
                    {
                        const NearbyTrackingAttackBehavior defaults;
                        scalarField("runtimeBehavior.rangePixels", behavior.rangePixels,
                            defaults.rangePixels);
                        scalarField("runtimeBehavior.damagePct", behavior.damagePct,
                            defaults.damagePct);
                    }
                    else if constexpr (std::is_same_v<B, DelayedAlternateAttackBehavior>)
                    {
                        const DelayedAlternateAttackBehavior defaults;
                        scalarField("runtimeBehavior.delayFrames", behavior.delayFrames,
                            defaults.delayFrames);
                        scalarField("runtimeBehavior.damagePct", behavior.damagePct,
                            defaults.damagePct);
                        scalarField("runtimeBehavior.attackerBlockGainChancePct",
                            behavior.attackerBlockGainChancePct,
                            defaults.attackerBlockGainChancePct);
                    }
                    else if constexpr (std::is_same_v<B, ExpandingSpiralAttackBehavior>)
                    {
                        const ExpandingSpiralAttackBehavior defaults;
                        scalarField("runtimeBehavior.projectileCount", behavior.projectileCount,
                            defaults.projectileCount);
                        scalarField("runtimeBehavior.bleedStacks", behavior.bleedStacks,
                            defaults.bleedStacks);
                    }
                }, typed.runtimeBehavior);
            }
            else if constexpr (std::is_same_v<T, ForceMoveAction>)
            {
                const ForceMoveAction defaults;
                requiredField("direction", typed.direction);
                scalarField("distanceTiles", typed.distanceTiles, defaults.distanceTiles);
                scalarField("distancePixels", typed.distancePixels, defaults.distancePixels);
                scalarField("lockFrames", typed.lockFrames, defaults.lockFrames);
                requiredField("collision", typed.collision);
                requiredField("blocked", typed.blocked);
            }
            else if constexpr (std::is_same_v<T, CreateAreaAction>)
            {
                const CreateAreaAction defaults;
                requiredField("shape", typed.shape);
                scalarField("radiusTiles", typed.radiusTiles, defaults.radiusTiles);
                scalarField("squareSideTiles", typed.squareSideTiles, defaults.squareSideTiles);
                requiredField("anchor", typed.anchor);
                requiredField("durationFrames", typed.durationFrames);
                requiredField("sourceDeath", typed.sourceDeath);
                requiredField("merge", typed.merge);
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    variantIndex, std::format("{}.modifiers.count", path),
                    DescriptionFieldDisposition::Visible,
                    std::to_string(typed.modifiers.size()));
                for (std::size_t index = 0; index < typed.modifiers.size(); ++index)
                {
                    const auto modifierPath = std::format("{}.modifiers[{}]", path, index);
                    const AreaModifier defaults;
                    const auto& modifier = typed.modifiers[index];
                    const auto modifierScalar = [&](std::string_view fieldName,
                        const auto& value, const auto& defaultValue)
                    {
                        appendAuditCoverage(coverage, rule,
                            DescriptionSourceFieldKind::Action, variantIndex,
                            std::format("{}.{}", modifierPath, fieldName),
                            value == defaultValue
                                ? DescriptionFieldDisposition::SchemaDefault
                                : DescriptionFieldDisposition::Visible,
                            descriptionAuditValue(value));
                    };
                    const auto modifierRequired = [&](std::string_view fieldName,
                        const auto& value)
                    {
                        appendAuditCoverage(coverage, rule,
                            DescriptionSourceFieldKind::Action, variantIndex,
                            std::format("{}.{}", modifierPath, fieldName),
                            DescriptionFieldDisposition::Visible,
                            descriptionAuditValue(value));
                    };
                    const auto modifierOptional = [&](std::string_view fieldName,
                        const auto& value)
                    {
                        appendAuditCoverage(coverage, rule,
                            DescriptionSourceFieldKind::Action, variantIndex,
                            std::format("{}.{}", modifierPath, fieldName),
                            value
                                ? DescriptionFieldDisposition::Visible
                                : DescriptionFieldDisposition::SchemaDefault,
                            value ? descriptionAuditValue(*value) : "absent");
                    };
                    modifierRequired("kind", modifier.kind);
                    modifierRequired("relation", modifier.relation);
                    if (modifier.kind == AreaModifierKind::Attribute)
                        modifierRequired("attribute", modifier.attribute);
                    else
                        modifierScalar("attribute", modifier.attribute, defaults.attribute);
                    if (modifier.kind == AreaModifierKind::OutgoingDamage)
                    {
                        modifierRequired("percent", modifier.percent);
                        modifierRequired("damageChannel", modifier.damageChannel);
                    }
                    else
                    {
                        modifierScalar("percent", modifier.percent, defaults.percent);
                        modifierScalar("damageChannel", modifier.damageChannel,
                            defaults.damageChannel);
                    }
                    modifierOptional("tracking", modifier.tracking);
                    modifierOptional("speedPct", modifier.speedPct);
                    modifierOptional("projectilePressurePct", modifier.projectilePressurePct);
                    modifierOptional("blockedDirection", modifier.blockedDirection);
                    modifierRequired("overlap", modifier.overlap);
                    modifierOptional("trackingOverlap", modifier.trackingOverlap);
                    modifierOptional("speedOverlap", modifier.speedOverlap);
                    modifierOptional("projectilePressureOverlap",
                        modifier.projectilePressureOverlap);
                    appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                        variantIndex, typed.modifiers[index].amount, std::format("{}.amount", modifierPath));
                }
            }
            else if constexpr (std::is_same_v<T, ModifyCastAction>)
            {
                const ModifyCastAction defaults;
                optionalField("mpCost", typed.mpCost.has_value(),
                    typed.mpCost ? "present" : "absent");
                optionalField("rangeMode", typed.rangeMode.has_value(),
                    typed.rangeMode ? descriptionAuditValue(*typed.rangeMode) : "absent");
                scalarField("projectileSpeedPct", typed.projectileSpeedPct,
                    defaults.projectileSpeedPct);
                scalarField("minimumSelectDistance", typed.minimumSelectDistance,
                    defaults.minimumSelectDistance);
                scalarField("additionalProjectiles", typed.additionalProjectiles,
                    defaults.additionalProjectiles);
                scalarField("mobility", typed.mobility, defaults.mobility);
                optionalField("autoUltimate", typed.autoUltimate.has_value(),
                    typed.autoUltimate ? "present" : "absent");
                optionalField("replacementPattern", typed.replacementPattern.has_value(),
                    typed.replacementPattern ? "present" : "absent");
                scalarField("freeAdditionalCast", typed.freeAdditionalCast,
                    defaults.freeAdditionalCast);
                scalarField("propagation", typed.propagation, defaults.propagation);
                if (typed.mpCost)
                    appendEffectNumberCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                        variantIndex, *typed.mpCost, std::format("{}.mpCost.value", path));
                if (typed.autoUltimate)
                {
                    const AutoUltimateCastRequest defaults;
                    scalarField("autoUltimate.value.consumeMp", typed.autoUltimate->consumeMp,
                        defaults.consumeMp);
                    scalarField("autoUltimate.value.announce", typed.autoUltimate->announce,
                        defaults.announce);
                }
                if (typed.replacementPattern)
                    appendAttackPatternCoverage(coverage, rule, variantIndex,
                        *typed.replacementPattern,
                        std::format("{}.replacementPattern.value", path));
            }
            else if constexpr (std::is_same_v<T, StateMachineAction>)
                appendStateMachineCoverage(coverage, rule, variantIndex, typed,
                    std::format("{}.stateMachine", path));
            else
            {
                assert(typed);
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    variantIndex, std::format("{}.conditions.count", path),
                    DescriptionFieldDisposition::Visible,
                    std::to_string(typed->conditions.size()));
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    variantIndex, std::format("{}.whenTrue.count", path),
                    DescriptionFieldDisposition::Visible,
                    std::to_string(typed->whenTrue.size()));
                appendAuditCoverage(coverage, rule, DescriptionSourceFieldKind::Action,
                    variantIndex, std::format("{}.whenFalse.count", path),
                    typed->whenFalse.empty()
                        ? DescriptionFieldDisposition::SchemaDefault
                        : DescriptionFieldDisposition::Visible,
                    std::to_string(typed->whenFalse.size()));
                for (std::size_t index = 0; index < typed->conditions.size(); ++index)
                    appendConditionCoverage(coverage, rule, typed->conditions[index],
                        std::format("{}.conditions[{}]", path, index));
                for (std::size_t index = 0; index < typed->whenTrue.size(); ++index)
                    appendActionCoverage(coverage, rule, typed->whenTrue[index],
                        std::format("{}.whenTrue[{}]", path, index));
                for (std::size_t index = 0; index < typed->whenFalse.size(); ++index)
                    appendActionCoverage(coverage, rule, typed->whenFalse[index],
                        std::format("{}.whenFalse[{}]", path, index));
            }
        },
        action.value);
}


}  // namespace KysChess::EffectDescriptionDetail
