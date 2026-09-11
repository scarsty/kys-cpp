#include "ChessEffectDescriptionInternal.h"
#include "ChessReplayHash.h"

#include <cassert>
#include <format>
#include <iterator>
#include <ranges>

namespace KysChess
{
namespace
{

void appendSignatureToken(std::string& target, std::string_view token)
{
    target += std::to_string(token.size());
    target += ':';
    target += token;
}

bool isPresentationCoverageRoot(std::string_view path)
{
    if (path == "id" || path == "selector" || path == "ruleQualifiers")
        return true;
    const bool indexedRoot = path.starts_with("conditions[")
        || path.starts_with("actions[");
    return indexedRoot && path.ends_with(']') && !path.contains("].");
}

std::string genericFallbackShapeSignature(
    const DescriptionCoverage& coverage)
{
    std::string canonical;
    for (const auto& entry : coverage.fields)
    {
        if (isPresentationCoverageRoot(entry.source.path)) continue;
        canonical += std::format("{}:{}:{}:",
            static_cast<int>(entry.source.kind),
            entry.source.variantIndex,
            static_cast<int>(entry.disposition));
        appendSignatureToken(canonical, entry.source.path);
        appendSignatureToken(canonical, entry.auditValue);
    }
    return "v2:" + chessSha256Hex(chessSha256(canonical));
}

}  // namespace

using namespace EffectDescriptionDetail;

EffectDescriptionDocument buildEffectDescriptionDocument(
    const EffectDescriptionInput& input)
{
    EffectDescriptionDocument result{
        .kind = input.kind,
        .cardSummary = {input.cardSummary.begin(), input.cardSummary.end()},
    };
    for (std::size_t ruleOrder = 0; ruleOrder < input.rules.size(); ++ruleOrder)
    {
        const auto& rule = input.rules[ruleOrder];
        EffectDescriptionBlock block{
            .archetype = DescriptionArchetype::Generic,
            .sourceRuleId = rule.id,
        };

        appendAuditCoverage(block.coverage, rule,
            DescriptionSourceFieldKind::Rule, 0, "id",
            DescriptionFieldDisposition::Visible,
            std::to_string(rule.id.value));
        appendAuditCoverage(block.coverage, rule,
            DescriptionSourceFieldKind::Rule, 0, "conditions.count",
            DescriptionFieldDisposition::Visible,
            std::to_string(rule.conditions.size()));
        appendAuditCoverage(block.coverage, rule,
            DescriptionSourceFieldKind::Rule, 0, "actions.count",
            DescriptionFieldDisposition::Visible,
            std::to_string(rule.actions.size()));

        const auto triggerSource = descriptionSource(
            rule, DescriptionSourceFieldKind::Trigger, 0, "event");
        block.trigger.push_back(descriptionFact(
            DescriptionTriggerFact{
                .event = rule.event,
                .observation = rule.observation,
                .castMatch = rule.castMatch,
                .subject = descriptionSubjectRole(rule.observation),
            },
            triggerSource,
            DescriptionFactLevel::Core,
            DescriptionPlayerFact::Trigger));
        appendCoverage(block.coverage, triggerSource,
            DescriptionFieldDisposition::Visible,
            DescriptionFactLevel::Core,
            { true, true },
            DescriptionPlayerFact::Trigger,
            std::to_string(static_cast<int>(rule.event)),
            "觸發事件產生 Core fact");
        appendCoverage(block.coverage,
            descriptionSource(rule, DescriptionSourceFieldKind::Trigger, 0, "observation"),
            rule.observation == EffectObservationScope::Owner
                ? DescriptionFieldDisposition::SchemaDefault
                : DescriptionFieldDisposition::Visible,
            rule.observation == EffectObservationScope::Owner
                ? DescriptionFactLevel::Audit
                : DescriptionFactLevel::Decision,
            rule.observation == EffectObservationScope::Owner
                ? DescriptionPlayerProjection{}
                : DescriptionPlayerProjection{ true, true },
            DescriptionPlayerFact::Trigger,
            std::to_string(static_cast<int>(rule.observation)),
            "觀察範圍由 role resolver 消費");
        appendCoverage(block.coverage,
            descriptionSource(rule, DescriptionSourceFieldKind::Trigger, 0, "castMatch"),
            rule.castMatch == EffectCastMatch::BoundMagic
                ? DescriptionFieldDisposition::SchemaDefault
                : DescriptionFieldDisposition::Visible,
            rule.castMatch == EffectCastMatch::BoundMagic
                ? DescriptionFactLevel::Audit
                : DescriptionFactLevel::Decision,
            rule.castMatch == EffectCastMatch::BoundMagic
                ? DescriptionPlayerProjection{}
                : DescriptionPlayerProjection{ true, true },
            DescriptionPlayerFact::Trigger,
            std::to_string(static_cast<int>(rule.castMatch)),
            "非預設任意施放是 Decision fact");

        const auto selectorSource = descriptionSource(
            rule, DescriptionSourceFieldKind::Selector,
            static_cast<std::size_t>(rule.selector.kind), "selector");
        block.targets.push_back(descriptionFact(
            resolveDescriptionSelector(rule.selector),
            selectorSource,
            DescriptionFactLevel::Core,
            DescriptionPlayerFact::Target));
        appendCoverage(block.coverage, selectorSource,
            DescriptionFieldDisposition::Visible,
            DescriptionFactLevel::Core,
            { true, true },
            DescriptionPlayerFact::Target,
            std::format("variant={}", static_cast<int>(rule.selector.kind)),
            "typed selector 解析為目標角色");
        appendSelectorCoverage(block.coverage, rule,
            DescriptionSourceFieldKind::Selector,
            static_cast<std::size_t>(rule.selector.kind),
            rule.selector,
            "selector");

        for (std::size_t index = 0; index < rule.conditions.size(); ++index)
        {
            const auto source = descriptionSource(
                rule, DescriptionSourceFieldKind::Condition,
                rule.conditions[index].index(),
                std::format("conditions[{}]", index));
            block.conditions.push_back(descriptionFact(
                DescriptionConditionFact{ rule.conditions[index] },
                source,
                DescriptionFactLevel::Decision,
                DescriptionPlayerFact::Condition));
            appendCoverage(block.coverage, source,
                DescriptionFieldDisposition::Visible,
                DescriptionFactLevel::Decision,
                { true, true },
                DescriptionPlayerFact::Condition,
                std::format("variant={}", rule.conditions[index].index()),
                "條件產生 Decision fact");
            appendConditionCoverage(block.coverage, rule, rule.conditions[index],
                std::format("conditions[{}]", index));
        }

        const auto qualifierSource = descriptionSource(
            rule, DescriptionSourceFieldKind::Qualifier, 0, "ruleQualifiers");
        block.conditions.push_back(descriptionFact(
            DescriptionRuleQualifiersFact{
                .chancePct = rule.chancePct,
                .maxActivations = rule.maxActivations,
                .sharedCooldownFrames = rule.sharedCooldownFrames,
                .intervalFrames = rule.intervalFrames,
                .everyNthEvent = rule.everyNthEvent,
                .activationLimit = rule.activationLimit,
                .repetitionCount = rule.repetitionCount,
            },
            qualifierSource,
            DescriptionFactLevel::Decision,
            DescriptionPlayerFact::RuleQualifiers));
        appendCoverage(block.coverage, qualifierSource,
            DescriptionFieldDisposition::Visible,
            DescriptionFactLevel::Decision,
            { true, true },
            DescriptionPlayerFact::RuleQualifiers,
            std::format("機率{}%;上限{};冷卻{};間隔{};每N{};觸發限制{};重複{}",
                rule.chancePct,
                rule.maxActivations,
                rule.sharedCooldownFrames,
                rule.intervalFrames,
                rule.everyNthEvent,
                rule.activationLimit ? "有" : "無",
                rule.repetitionCount ? "有" : "無"),
            "規則限制的預設值也保留於 Detailed audit");
        const auto appendQualifierCoverage = [&](
            std::string_view field,
            bool isDefault,
            std::string value)
        {
            appendAuditCoverage(
                block.coverage,
                rule,
                DescriptionSourceFieldKind::Qualifier,
                0,
                std::format("ruleQualifiers.{}", field),
                isDefault
                    ? DescriptionFieldDisposition::SchemaDefault
                    : DescriptionFieldDisposition::Visible,
                std::move(value));
        };
        appendQualifierCoverage("chancePct", rule.chancePct == 100,
            std::to_string(rule.chancePct));
        appendQualifierCoverage("maxActivations", rule.maxActivations == 0,
            std::to_string(rule.maxActivations));
        appendQualifierCoverage("sharedCooldownFrames", rule.sharedCooldownFrames == 0,
            std::to_string(rule.sharedCooldownFrames));
        appendQualifierCoverage("intervalFrames", rule.intervalFrames == 0,
            std::to_string(rule.intervalFrames));
        appendQualifierCoverage("everyNthEvent", rule.everyNthEvent == 0,
            std::to_string(rule.everyNthEvent));
        appendQualifierCoverage("activationLimit", !rule.activationLimit,
            rule.activationLimit ? "present" : "absent");
        appendQualifierCoverage("repetitionCount", !rule.repetitionCount,
            rule.repetitionCount ? "present" : "absent");
        if (rule.activationLimit)
        {
            appendAuditCoverage(block.coverage, rule,
                DescriptionSourceFieldKind::Qualifier, 0,
                "ruleQualifiers.activationLimit.value.scope",
                DescriptionFieldDisposition::Visible,
                std::to_string(static_cast<int>(rule.activationLimit->scope)));
            appendAuditCoverage(block.coverage, rule,
                DescriptionSourceFieldKind::Qualifier, 0,
                "ruleQualifiers.activationLimit.value.maxEvaluations",
                DescriptionFieldDisposition::Visible,
                std::to_string(rule.activationLimit->maxEvaluations));
        }
        if (rule.repetitionCount)
            appendEffectNumberCoverage(block.coverage, rule,
                DescriptionSourceFieldKind::Qualifier, 0,
                *rule.repetitionCount,
                "ruleQualifiers.repetitionCount.value");

        DescriptionActionGroup group;
        group.sequential = actionsDependOnOrder(rule.actions);
        for (std::size_t index = 0; index < rule.actions.size(); ++index)
        {
            const auto source = descriptionSource(
                rule, DescriptionSourceFieldKind::Action,
                rule.actions[index].value.index(),
                std::format("actions[{}]", index));
            appendCoverage(block.coverage, source,
                DescriptionFieldDisposition::Visible,
                DescriptionFactLevel::Core,
                { true, true },
                DescriptionPlayerFact::Action,
                std::format("variant={}", rule.actions[index].value.index()),
                "動作 payload 保留在 typed Core fact");
            appendActionCoverage(block.coverage, rule, rule.actions[index],
                std::format("actions[{}]", index));
            auto action = makeDescriptionAction(
                rule,
                rule.actions[index],
                std::format("actions[{}]", index));
            group.actions.push_back(std::move(action));
        }
        if (!group.actions.empty()) block.actions.push_back(std::move(group));
        block.archetype = descriptionArchetype(block);
        classifyArchetypeCoverage(block);
        block.coverage.genericFallback = block.archetype == DescriptionArchetype::Generic;
        if (block.coverage.genericFallback)
        {
            block.coverage.unmatchedShapeSignatures.push_back(
                genericFallbackShapeSignature(block.coverage));
        }
        setDescriptionRuleOrder(block, ruleOrder);

        auto section = std::ranges::find(
            result.sections,
            std::optional<EffectEvent>{ rule.event },
            &EffectDescriptionSection::event);
        if (section == result.sections.end())
        {
            result.sections.push_back({ .event = rule.event });
            section = std::prev(result.sections.end());
        }
        section->blocks.push_back(std::move(block));
    }
    for (auto& section : result.sections)
        for (auto& block : section.blocks)
        {
            applyArchetypeProjection(block);
            classifyPhraseAbsorptions(block);
        }
    return result;
}


}  // namespace KysChess
