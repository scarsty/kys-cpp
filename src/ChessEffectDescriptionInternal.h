#pragma once

#include "ChessEffectDescription.h"

#include <algorithm>
#include <initializer_list>
#include <span>

namespace KysChess::EffectDescriptionDetail
{

std::string ruleEventLabel(EffectEvent event, EffectDescriptionStyle style);
std::string selectorLabel(const EffectSelector& selector, bool compact);
std::string descriptionNumberLabel(
    const EffectNumber& number,
    EffectDescriptionStyle style);
// Returns the authored duration using the formula when present, otherwise the
// fixed frame count.  Keeping this selection in one place prevents specialised
// status phrases from silently rendering formula durations as 0幀.
std::string statusDurationLabel(
    const ApplyStatusAction& status,
    EffectDescriptionStyle style);
std::string renderStatusBehaviorDefinition(
    const StatusBehaviorDefinition& behavior,
    EffectDescriptionStyle style);

inline bool statusBehaviorHasExactShape(
    const ApplyStatusAction& status,
    std::initializer_list<std::size_t> actionCounts)
{
    if (!status.behavior || status.behavior->rules.size() != actionCounts.size())
        return false;
    return std::ranges::equal(
        status.behavior->rules,
        actionCounts,
        {},
        [](const EffectRule& rule) { return rule.actions.size(); });
}

template<typename Action, typename Predicate>
const Action* findStatusBehaviorAction(
    const ApplyStatusAction& status,
    Predicate predicate)
{
    if (!status.behavior) return nullptr;
    for (const auto& rule : status.behavior->rules)
    {
        for (const auto& action : rule.actions)
        {
            const auto* typed = std::get_if<Action>(&action.value);
            if (typed && predicate(*typed)) return typed;
        }
    }
    return nullptr;
}

template<typename Action>
const Action* findStatusBehaviorAction(const ApplyStatusAction& status)
{
    return findStatusBehaviorAction<Action>(
        status,
        [](const Action&) { return true; });
}

std::string borrowedRuleFilterLabel(const BorrowedRuleFilter& filter);
std::string copiedMagicFilterLabel(const CopiedMagicFilter& filter);
std::string conditionLabel(
    const EffectCondition& condition,
    bool compact,
    std::optional<EffectEvent> event = std::nullopt);
std::string_view damageKindLabel(BattleDamageKind kind);
std::vector<std::string> ruleQualifierDescriptions(
    const DescriptionRuleQualifiersFact& qualifiers,
    EffectDescriptionStyle style,
    EffectEvent event);
bool actionsDependOnOrder(std::span<const EffectAction> actions);
std::string renderActionDescription(
    const EffectAction& action,
    EffectDescriptionStyle style,
    EffectEvent event,
    bool coalesce);

struct DescriptionActionPhraseRow
{
    std::string text;
    int indent{};
    EffectDescriptionSemanticBreak breakBefore{
        EffectDescriptionSemanticBreak::ActionGroup};
};

std::vector<DescriptionActionPhraseRow> renderPlayerActionDescriptionRows(
    const EffectAction& action,
    EffectDescriptionStyle style,
    EffectEvent event,
    bool coalesce);
std::vector<DescriptionActionPhraseRow> renderDetailedStatusBehaviorRows(
    const ApplyStatusAction& status);

DescriptionSourceField descriptionSource(
    const EffectRule& rule,
    DescriptionSourceFieldKind kind,
    std::size_t variantIndex,
    std::string path);
EffectDescriptionFact descriptionFact(
    DescriptionFactValue value,
    DescriptionSourceField source,
    DescriptionFactLevel level,
    DescriptionPlayerFact playerFact);
DescriptionTriggerFact::SubjectRole descriptionSubjectRole(
    EffectObservationScope observation);
DescriptionSelectorFact resolveDescriptionSelector(
    const EffectSelector& selector);
DescriptionAction makeDescriptionAction(
    const EffectRule& rule,
    const EffectAction& action,
    std::string path);
void setDescriptionRuleOrder(
    EffectDescriptionBlock& block,
    std::size_t ruleOrder);
void appendCoverage(
    DescriptionCoverage& coverage,
    const DescriptionSourceField& source,
    DescriptionFieldDisposition disposition,
    DescriptionFactLevel level,
    DescriptionPlayerProjection projection,
    std::optional<DescriptionPlayerFact> playerFact,
    std::string auditValue,
    std::string reason);
void appendAuditCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    DescriptionSourceFieldKind kind,
    std::size_t variantIndex,
    std::string path,
    DescriptionFieldDisposition disposition,
    std::string auditValue,
    std::optional<DescriptionPlayerFact> playerFact = std::nullopt);
void appendEffectNumberCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    DescriptionSourceFieldKind kind,
    std::size_t variantIndex,
    const EffectNumber& number,
    std::string_view path,
    std::optional<DescriptionPlayerFact> playerFact = std::nullopt);
void appendSelectorCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    DescriptionSourceFieldKind kind,
    std::size_t variantIndex,
    const EffectSelector& selector,
    std::string_view path);
void appendConditionCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    const EffectCondition& condition,
    std::string_view path);
void appendActionCoverage(
    DescriptionCoverage& coverage,
    const EffectRule& rule,
    const EffectAction& action,
    std::string path);

const ApplyStatusAction* applyStatusAction(
    const EffectDescriptionBlock& block);
const ConsumeStatusAction* consumeStatusAction(
    const EffectDescriptionBlock& block);
const DescriptionRuleQualifiersFact& ruleQualifiers(
    const EffectDescriptionBlock& block);
const DescriptionTriggerFact& descriptionTrigger(
    const EffectDescriptionBlock& block);
const DescriptionSelectorFact& descriptionTarget(
    const EffectDescriptionBlock& block);
std::vector<const EffectCondition*> descriptionConditions(
    const EffectDescriptionBlock& block);
std::vector<const EffectAction*> descriptionActions(
    const EffectDescriptionBlock& block);
bool hasDefaultRuleQualifiers(const DescriptionRuleQualifiersFact& qualifiers);
const BorrowEffectRulesAction* borrowRulesAction(
    const EffectDescriptionBlock& block);
const CopyAttackDefinitionAction* copyAttackAction(
    const EffectDescriptionBlock& block);
bool matchesBorrowRulesArchetype(const EffectDescriptionBlock& block);
bool matchesCopyAttackArchetype(const EffectDescriptionBlock& block);
bool matchesConditionalAttackArchetype(const EffectDescriptionBlock& block);
bool matchesStatusLifecycleArchetype(const EffectDescriptionBlock& block);
bool matchesStackExplosionArchetype(const EffectDescriptionBlock& block);
DescriptionArchetype descriptionArchetype(
    const EffectDescriptionBlock& block);
void classifyArchetypeCoverage(EffectDescriptionBlock& block);
void applyArchetypeProjection(EffectDescriptionBlock& block);
void classifyPhraseAbsorptions(EffectDescriptionBlock& block);

}  // namespace KysChess::EffectDescriptionDetail
