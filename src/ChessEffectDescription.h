#pragma once

#include "ChessBattleEffectTypes.h"
#include "DisplayText.h"

namespace KysChess
{

enum class EffectDescriptionStyle
{
    Detailed,
    Full,
    Compact,
};

enum class EffectDescriptionContainerKind
{
    Magic,
    Equipment,
    EquipmentSynergy,
    Neigong,
    ComboThreshold,
};

enum class EffectDescriptionCompactPolicy
{
    Default,
    PlayerCard,
};

struct EffectDescriptionInput
{
    EffectDescriptionContainerKind kind{};
    std::span<const EffectRule> rules{};
    std::span<const std::string> cardSummary{};
};

struct EffectDescriptionPresentationContext
{
    std::optional<EffectEvent> enclosingDefaultEvent{};
    EffectDescriptionCompactPolicy compactPolicy{};
};

enum class DescriptionFieldDisposition
{
    Visible,
    AbsorbedByPhrase,
    SchemaDefault,
    DeterminismOnly,
    SafetyInvariant,
};

enum class DescriptionFactLevel
{
    Core,
    Decision,
    Audit,
};

struct DescriptionPlayerProjection
{
    bool full{};
    bool compact{};

    bool operator==(const DescriptionPlayerProjection&) const = default;
};

enum class DescriptionPlayerFact
{
    Trigger,
    Target,
    Condition,
    RuleQualifiers,
    Action,
    BorrowedRuleActionCategories,
    CopiedMagicConditions,
    StateMachinePropagation,
};

enum class DescriptionPhraseAbsorption
{
    None,
    UltimateEvent,
    OrdinaryHitTrigger,
    SameComboAllyDeathTrigger,
    CompactPositiveDamagePerspective,
};

enum class DescriptionSourceFieldKind
{
    Rule,
    Trigger,
    Selector,
    Condition,
    Action,
    Qualifier,
};

struct DescriptionSourceField
{
    EffectRuleId ruleId{};
    std::size_t ruleOrder{};
    DescriptionSourceFieldKind kind{};
    std::size_t variantIndex{};
    std::string path;
};

struct DescriptionCoverageEntry
{
    DescriptionSourceField source;
    DescriptionFieldDisposition disposition{};
    DescriptionFactLevel level = DescriptionFactLevel::Audit;
    DescriptionPlayerProjection projection;
    std::optional<DescriptionPlayerFact> playerFact;
    std::string auditValue;
    std::string reason;
};

struct DescriptionCoverage
{
    std::vector<DescriptionCoverageEntry> fields;
    bool genericFallback = false;
    std::vector<std::string> unmatchedShapeSignatures;
};

enum class DescriptionArchetype
{
    Generic,
    StatusLifecycle,
    StackExplosion,
    ConditionalAttack,
    CopyAttack,
    BorrowRules,
};

struct DescriptionTriggerFact
{
    EffectEvent event{};
    EffectObservationScope observation{};
    EffectCastMatch castMatch{};
    enum class SubjectRole
    {
        EffectOwner,
        EventSource,
        EventTarget,
    } subject{};
};

enum class DescriptionTargetRole
{
    EffectOwner,
    EventSource,
    EventTarget,
    OriginalAttackTarget,
    SelectedUnits,
};

struct DescriptionSelectorFact
{
    EffectSelector selector;
    DescriptionTargetRole role{};
};

struct DescriptionConditionFact
{
    EffectCondition condition;
};

struct DescriptionRuleQualifiersFact
{
    int chancePct = 100;
    int maxActivations{};
    int sharedCooldownFrames{};
    int intervalFrames{};
    int everyNthEvent{};
    std::optional<EffectActivationLimit> activationLimit;
    std::optional<EffectNumber> repetitionCount;
};

using DescriptionFactValue = std::variant<
    DescriptionTriggerFact,
    DescriptionSelectorFact,
    DescriptionConditionFact,
    DescriptionRuleQualifiersFact>;

struct EffectDescriptionFact
{
    DescriptionFactValue value;
    std::vector<DescriptionSourceField> sources;
    DescriptionFactLevel level{};
    DescriptionPlayerProjection projection;
    DescriptionPlayerFact playerFact{};
    DescriptionPhraseAbsorption absorption{};
};

struct DescriptionBranch;

struct DescriptionStatusApplication
{
    EffectAction semanticAction;
};

struct DescriptionAction
{
    std::variant<
        EffectAction,
        DescriptionStatusApplication,
        std::shared_ptr<DescriptionBranch>> value;
    std::vector<DescriptionSourceField> sources;
    DescriptionFactLevel level = DescriptionFactLevel::Core;
    DescriptionPlayerProjection projection{ true, true };
    DescriptionPlayerFact playerFact = DescriptionPlayerFact::Action;
};

inline const EffectAction* descriptionEffectAction(const DescriptionAction& action)
{
    if (const auto* leaf = std::get_if<EffectAction>(&action.value)) return leaf;
    if (const auto* status = std::get_if<DescriptionStatusApplication>(&action.value))
        return &status->semanticAction;
    return nullptr;
}

inline const ApplyStatusAction* descriptionStatusApplication(
    const DescriptionAction& action)
{
    const auto* semantic = std::get_if<DescriptionStatusApplication>(&action.value);
    return semantic
        ? std::get_if<ApplyStatusAction>(&semantic->semanticAction.value)
        : nullptr;
}

struct DescriptionActionGroup
{
    std::vector<DescriptionAction> actions;
    bool sequential = false;
};

struct DescriptionBranch
{
    std::vector<EffectDescriptionFact> conditions;
    std::vector<DescriptionActionGroup> whenTrue;
    std::vector<DescriptionActionGroup> whenFalse;
};

struct EffectDescriptionBlock
{
    DescriptionArchetype archetype{};
    EffectRuleId sourceRuleId{};
    std::size_t sourceRuleOrder{};
    std::vector<EffectDescriptionFact> trigger;
    std::vector<EffectDescriptionFact> conditions;
    std::vector<EffectDescriptionFact> targets;
    std::vector<DescriptionActionGroup> actions;
    DescriptionCoverage coverage;
};

struct EffectDescriptionSection
{
    std::optional<EffectEvent> event;
    std::vector<EffectDescriptionBlock> blocks;
};

struct EffectDescriptionDocument
{
    EffectDescriptionContainerKind kind{};
    std::vector<EffectDescriptionSection> sections;
    std::vector<std::string> cardSummary;
};

enum class EffectDescriptionRowKind
{
    Field,
    ListItem,
    Heading,
    Prose,
    Summary,
};

enum class EffectDescriptionSemanticBreak
{
    None,
    Block,
    Branch,
    Sequence,
    ActionGroup,
    Qualifier,
};

struct RenderedEffectDescriptionRow
{
    EffectDescriptionRowKind kind{};
    std::string text;
    int indent{};
    EffectDescriptionSemanticBreak breakBefore{};
    DisplayTextWrapping wrapping{};
};

struct RenderedEffectDescriptionBlock
{
    std::vector<RenderedEffectDescriptionRow> rows;
};

struct RenderedEffectDescriptionSection
{
    std::optional<std::string> heading;
    std::vector<RenderedEffectDescriptionBlock> blocks;
};

struct RenderedEffectDescription
{
    std::vector<RenderedEffectDescriptionSection> sections;
};

EffectDescriptionDocument buildEffectDescriptionDocument(
    const EffectDescriptionInput& input);

RenderedEffectDescription renderEffectDescription(
    const EffectDescriptionDocument& document,
    EffectDescriptionStyle style,
    const EffectDescriptionPresentationContext& context);

std::vector<std::string> effectDescriptionTextRows(
    const RenderedEffectDescription& rendered);

std::string joinEffectDescriptionRows(
    const RenderedEffectDescription& rendered,
    std::string_view separator = "\n");

}  // namespace KysChess
