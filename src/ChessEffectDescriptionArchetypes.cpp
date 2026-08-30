#include "ChessEffectDescriptionInternal.h"
#include "ChessBattleEffectSemantics.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <ranges>

namespace KysChess::EffectDescriptionDetail
{
namespace
{
struct ArchetypeProjectionDescriptor
{
    DescriptionArchetype archetype{};
    std::array<DescriptionPlayerFact, 3> fullAuditFacts{};
    std::size_t fullAuditFactCount{};
    std::array<DescriptionPlayerFact, 3> compactAuditFacts{};
    std::size_t compactAuditFactCount{};
};

constexpr std::array archetypeProjectionDescriptors{
    ArchetypeProjectionDescriptor{
        DescriptionArchetype::Generic,
        { DescriptionPlayerFact::Action },
        1,
        { DescriptionPlayerFact::Action },
        1,
    },
    ArchetypeProjectionDescriptor{
        DescriptionArchetype::StatusLifecycle,
        { DescriptionPlayerFact::Action },
        1,
        { DescriptionPlayerFact::Action },
        1,
    },
    ArchetypeProjectionDescriptor{
        DescriptionArchetype::StackExplosion,
        { DescriptionPlayerFact::Action },
        1,
        { DescriptionPlayerFact::Action },
        1,
    },
    ArchetypeProjectionDescriptor{ DescriptionArchetype::ConditionalAttack },
    ArchetypeProjectionDescriptor{
        DescriptionArchetype::CopyAttack,
        { DescriptionPlayerFact::CopiedMagicConditions,
          DescriptionPlayerFact::StateMachinePropagation },
        2,
        { DescriptionPlayerFact::CopiedMagicConditions,
          DescriptionPlayerFact::StateMachinePropagation },
        2,
    },
    ArchetypeProjectionDescriptor{
        DescriptionArchetype::BorrowRules,
        { DescriptionPlayerFact::BorrowedRuleActionCategories,
          DescriptionPlayerFact::StateMachinePropagation },
        2,
        { DescriptionPlayerFact::BorrowedRuleActionCategories,
          DescriptionPlayerFact::StateMachinePropagation },
        2,
    },
};

constexpr std::array fullCoreFacts{
    DescriptionPlayerFact::Trigger,
    DescriptionPlayerFact::Target,
    DescriptionPlayerFact::Action,
};

constexpr std::array fullDecisionFacts{
    DescriptionPlayerFact::Trigger,
    DescriptionPlayerFact::Condition,
    DescriptionPlayerFact::RuleQualifiers,
};

constexpr std::array compactMandatoryFacts{
    DescriptionPlayerFact::Trigger,
    DescriptionPlayerFact::Target,
    DescriptionPlayerFact::Condition,
    DescriptionPlayerFact::RuleQualifiers,
    DescriptionPlayerFact::Action,
};

const ArchetypeProjectionDescriptor& projectionDescriptor(
    DescriptionArchetype archetype)
{
    const auto descriptor = std::ranges::find(
        archetypeProjectionDescriptors,
        archetype,
        &ArchetypeProjectionDescriptor::archetype);
    assert(descriptor != archetypeProjectionDescriptors.end());
    return *descriptor;
}

template<std::size_t Size>
bool containsPlayerFact(
    const std::array<DescriptionPlayerFact, Size>& facts,
    std::size_t count,
    DescriptionPlayerFact fact)
{
    return std::ranges::find(facts.begin(), facts.begin() + count, fact)
        != facts.begin() + count;
}

DescriptionPlayerProjection projectionForFact(
    const ArchetypeProjectionDescriptor& descriptor,
    DescriptionFactLevel level,
    DescriptionPlayerFact fact)
{
    switch (level)
    {
    case DescriptionFactLevel::Core:
        return {
            std::ranges::contains(fullCoreFacts, fact),
            std::ranges::contains(compactMandatoryFacts, fact),
        };
    case DescriptionFactLevel::Decision:
        return {
            std::ranges::contains(fullDecisionFacts, fact),
            std::ranges::contains(compactMandatoryFacts, fact),
        };
    case DescriptionFactLevel::Audit:
        return {
            containsPlayerFact(
                descriptor.fullAuditFacts,
                descriptor.fullAuditFactCount,
                fact),
            containsPlayerFact(
                descriptor.compactAuditFacts,
                descriptor.compactAuditFactCount,
                fact),
        };
    }
    std::unreachable();
}
}  // namespace

template<typename T>
const T* stateMachineAction(const EffectAction& action)
{
    const auto* machine = std::get_if<StateMachineAction>(&action.value);
    return machine ? std::get_if<T>(machine) : nullptr;
}

const ApplyStatusAction* applyStatusAction(const EffectDescriptionBlock& block)
{
    for (const auto& group : block.actions)
    {
        for (const auto& action : group.actions)
        {
            const auto* leaf = descriptionEffectAction(action);
            if (!leaf) continue;
            if (const auto* status = std::get_if<ApplyStatusAction>(&leaf->value))
                return status;
        }
    }
    return nullptr;
}

const ConsumeStatusAction* consumeStatusAction(const EffectDescriptionBlock& block)
{
    for (const auto& group : block.actions)
    {
        for (const auto& action : group.actions)
        {
            const auto* leaf = descriptionEffectAction(action);
            if (!leaf) continue;
            if (const auto* status = std::get_if<ConsumeStatusAction>(&leaf->value))
                return status;
        }
    }
    return nullptr;
}

const DescriptionRuleQualifiersFact& ruleQualifiers(const EffectDescriptionBlock& block)
{
    for (const auto& fact : block.conditions)
        if (const auto* qualifiers = std::get_if<DescriptionRuleQualifiersFact>(&fact.value))
            return *qualifiers;
    assert(false && "effect description block has no rule qualifiers");
    std::unreachable();
}

const DescriptionTriggerFact& descriptionTrigger(const EffectDescriptionBlock& block)
{
    assert(block.trigger.size() == 1);
    return std::get<DescriptionTriggerFact>(block.trigger.front().value);
}

const DescriptionSelectorFact& descriptionTarget(const EffectDescriptionBlock& block)
{
    assert(block.targets.size() == 1);
    return std::get<DescriptionSelectorFact>(block.targets.front().value);
}

std::vector<const EffectCondition*> descriptionConditions(
    const EffectDescriptionBlock& block)
{
    std::vector<const EffectCondition*> result;
    for (const auto& fact : block.conditions)
    {
        if (const auto* condition = std::get_if<DescriptionConditionFact>(&fact.value))
            result.push_back(&condition->condition);
    }
    return result;
}

std::vector<const EffectAction*> descriptionActions(
    const EffectDescriptionBlock& block)
{
    std::vector<const EffectAction*> result;
    for (const auto& group : block.actions)
    {
        for (const auto& action : group.actions)
        {
            const auto* leaf = descriptionEffectAction(action);
            if (!leaf) return {};
            result.push_back(leaf);
        }
    }
    return result;
}

const BorrowEffectRulesAction* borrowRulesAction(
    const EffectDescriptionBlock& block)
{
    const auto actions = descriptionActions(block);
    return actions.size() == 1
        ? stateMachineAction<BorrowEffectRulesAction>(*actions.front())
        : nullptr;
}

const CopyAttackDefinitionAction* copyAttackAction(
    const EffectDescriptionBlock& block)
{
    const auto actions = descriptionActions(block);
    return actions.size() == 1
        ? stateMachineAction<CopyAttackDefinitionAction>(*actions.front())
        : nullptr;
}

bool hasDefaultRuleQualifiers(const DescriptionRuleQualifiersFact& qualifiers)
{
    return qualifiers.chancePct == 100
        && qualifiers.maxActivations == 0
        && qualifiers.sharedCooldownFrames == 0
        && qualifiers.intervalFrames == 0
        && qualifiers.everyNthEvent == 0
        && !qualifiers.activationLimit
        && !qualifiers.repetitionCount;
}

bool hasStandardBorrowedRuleFilter(const BorrowEffectRulesAction& action)
{
    static constexpr std::array expected{
        BorrowedRuleActionCategory::AttributeModifier,
        BorrowedRuleActionCategory::DamageModifier,
        BorrowedRuleActionCategory::ResourceChange,
        BorrowedRuleActionCategory::HealTransactionModifier,
        BorrowedRuleActionCategory::Status,
        BorrowedRuleActionCategory::Damage,
        BorrowedRuleActionCategory::Attack,
        BorrowedRuleActionCategory::ForcedMovement,
        BorrowedRuleActionCategory::Area,
        BorrowedRuleActionCategory::Cast,
        BorrowedRuleActionCategory::StateValue,
        BorrowedRuleActionCategory::DamageMemory,
        BorrowedRuleActionCategory::DamageAbsorption,
        BorrowedRuleActionCategory::StatusDamageSettlement,
    };
    return action.propagation == CastPropagationPolicy::BorrowedUltimateRules
        && std::ranges::equal(action.filter.allowedActionCategories, expected);
}

bool hasDefaultEffectNumber(const EffectNumber& number)
{
    return number == EffectNumber{};
}

EffectSelector selectorOfKind(EffectSelectorKind kind)
{
    EffectSelector result;
    result.kind = kind;
    return result;
}

bool matchesConditionalAttackBranch(
    const ModifyAttackAction& action,
    int strengthPct,
    bool mainProjectile,
    const std::optional<EffectSelector>& source)
{
    const AttackPattern expectedPattern{};
    return action.pattern.kind == expectedPattern.kind
        && action.pattern.projectileCount == expectedPattern.projectileCount
        && action.pattern.spreadDegrees == expectedPattern.spreadDegrees
        && action.pattern.intervalFrames == expectedPattern.intervalFrames
        && action.strengthPct == strengthPct
        && !action.through
        && !action.tracking
        && action.mainProjectile == mainProjectile
        && action.sameTargetHitLimit == 0
        && action.targets == AttackTargetPolicy::SameTarget
        && action.propagation == CastPropagationPolicy::SuppressUltimateRules
        && action.addToBaseAttack
        && ((!action.source && !source)
            || (action.source && source && *action.source == *source))
        && !action.damageOverride
        && !action.damageKind
        && std::holds_alternative<std::monostate>(action.runtimeBehavior);
}

bool matchesBorrowRulesArchetype(const EffectDescriptionBlock& block)
{
    const auto& trigger = descriptionTrigger(block);
    const auto conditions = descriptionConditions(block);
    const auto actions = descriptionActions(block);
    if (trigger.event != EffectEvent::CastPlanned
        || trigger.observation != EffectObservationScope::Owner
        || trigger.castMatch != EffectCastMatch::BoundMagic
        || descriptionTarget(block).selector != selectorOfKind(EffectSelectorKind::Self)
        || !conditions.empty()
        || !hasDefaultRuleQualifiers(ruleQualifiers(block))
        || actions.size() != 1)
        return false;

    const auto* action = stateMachineAction<BorrowEffectRulesAction>(*actions.front());
    if (!action || !hasStandardBorrowedRuleFilter(*action)) return false;
    auto expectedSource = selectorOfKind(EffectSelectorKind::Enemies);
    expectedSource.tieBreak = EffectTieBreak::BattleRandom;
    EffectNumber expectedCount;
    expectedCount.base = EffectNumberBase::SourceStar;
    expectedCount.percent = 50;
    expectedCount.rounding = EffectRounding::Ceil;
    expectedCount.minimum = 1;
    expectedCount.maximum = 2;
    return action->sourceUnits == expectedSource
        && action->sourceCount == expectedCount;
}

bool matchesCopyAttackArchetype(const EffectDescriptionBlock& block)
{
    const auto& trigger = descriptionTrigger(block);
    const auto conditions = descriptionConditions(block);
    const auto actions = descriptionActions(block);
    if (trigger.event != EffectEvent::UltimateCommitted
        || trigger.observation != EffectObservationScope::Owner
        || trigger.castMatch != EffectCastMatch::BoundMagic
        || descriptionTarget(block).selector != selectorOfKind(EffectSelectorKind::Self)
        || !conditions.empty()
        || !hasDefaultRuleQualifiers(ruleQualifiers(block))
        || actions.size() != 1)
        return false;

    const auto* action = stateMachineAction<CopyAttackDefinitionAction>(*actions.front());
    if (!action
        || action->copyCount != 1
        || action->propagation != CastPropagationPolicy::SuppressUltimateRules
        || action->filter.conditions != std::vector{
            CopiedMagicCondition::HasUltimateAttackDefinition,
            CopiedMagicCondition::ExcludesRecursiveEffects })
        return false;
    auto expectedSource = selectorOfKind(EffectSelectorKind::AllLivingUnits);
    expectedSource.tieBreak = EffectTieBreak::BattleRandom;
    expectedSource.excludeOwner = true;
    return action->sourceUnits == expectedSource;
}

bool matchesConditionalAttackArchetype(const EffectDescriptionBlock& block)
{
    const auto& trigger = descriptionTrigger(block);
    if (trigger.event != EffectEvent::AttackCommitted
        || trigger.observation != EffectObservationScope::Owner
        || trigger.castMatch != EffectCastMatch::BoundMagic
        || descriptionTarget(block).selector
            != selectorOfKind(EffectSelectorKind::OriginalAttackTarget)
        || !descriptionConditions(block).empty()
        || !hasDefaultRuleQualifiers(ruleQualifiers(block))
        || block.actions.size() != 1
        || block.actions.front().actions.size() != 1)
        return false;

    const auto* branchPointer = std::get_if<std::shared_ptr<DescriptionBranch>>(
        &block.actions.front().actions.front().value);
    if (!branchPointer || !*branchPointer) return false;
    const auto& branch = **branchPointer;
    if (branch.conditions.size() != 1
        || !std::holds_alternative<OtherLivingAllyUsesBoundMagicCondition>(
            std::get<DescriptionConditionFact>(
                branch.conditions.front().value).condition)
        || branch.whenTrue.size() != 1
        || branch.whenFalse.size() != 1
        || branch.whenTrue.front().actions.size() != 1
        || branch.whenFalse.front().actions.size() != 1)
        return false;
    const auto* whenTrue = std::get_if<ModifyAttackAction>(
        &descriptionEffectAction(branch.whenTrue.front().actions.front())->value);
    const auto* whenFalse = std::get_if<ModifyAttackAction>(
        &descriptionEffectAction(branch.whenFalse.front().actions.front())->value);
    if (!whenTrue || !whenFalse) return false;
    auto allySource = selectorOfKind(EffectSelectorKind::Allies);
    allySource.count = 1;
    allySource.excludeOwner = true;
    allySource.requiredBoundMagic = true;
    return matchesConditionalAttackBranch(*whenTrue, 100, true, allySource)
        && matchesConditionalAttackBranch(*whenFalse, 50, false, std::nullopt);
}

DescriptionArchetype descriptionArchetype(const EffectDescriptionBlock& block)
{
    if (matchesBorrowRulesArchetype(block)) return DescriptionArchetype::BorrowRules;
    if (matchesCopyAttackArchetype(block)) return DescriptionArchetype::CopyAttack;
    if (matchesConditionalAttackArchetype(block)) return DescriptionArchetype::ConditionalAttack;
    return DescriptionArchetype::Generic;
}

void classifyArchetypeCoverage(EffectDescriptionBlock& block)
{
    const auto absorb = [&](DescriptionPlayerFact fact, std::string_view reason)
    {
        for (auto& entry : block.coverage.fields)
        {
            if (entry.playerFact != fact) continue;
            entry.disposition = DescriptionFieldDisposition::AbsorbedByPhrase;
            entry.reason = std::string(reason);
        }
    };
    if (block.archetype == DescriptionArchetype::BorrowRules)
    {
        absorb(DescriptionPlayerFact::BorrowedRuleActionCategories,
            "「大招效果」吸收已審核的封閉 action 類別集合");
        absorb(DescriptionPlayerFact::StateMachinePropagation,
            "「借用大招效果」吸收借用規則傳播政策");
    }
    else if (block.archetype == DescriptionArchetype::CopyAttack)
    {
        absorb(DescriptionPlayerFact::CopiedMagicConditions,
            "「絕招攻擊且不遞迴」吸收複製候選的封閉條件集合");
        absorb(DescriptionPlayerFact::StateMachinePropagation,
            "「不複製大招效果」吸收抑制大招規則的傳播政策");
    }
}

void applyArchetypeProjection(EffectDescriptionBlock& block)
{
    const auto& descriptor = projectionDescriptor(block.archetype);
    const auto applyFacts = [&](auto& facts)
    {
        for (auto& fact : facts)
            fact.projection = projectionForFact(
                descriptor,
                fact.level,
                fact.playerFact);
    };
    applyFacts(block.trigger);
    applyFacts(block.conditions);
    applyFacts(block.targets);
    const auto applyActions = [&](const auto& self, auto& groups) -> void
    {
        for (auto& group : groups)
        {
            for (auto& action : group.actions)
            {
                action.projection = projectionForFact(
                    descriptor,
                    action.level,
                    action.playerFact);
                if (auto* branch = std::get_if<std::shared_ptr<DescriptionBranch>>(
                        &action.value))
                {
                    assert(*branch);
                    applyFacts((*branch)->conditions);
                    self(self, (*branch)->whenTrue);
                    self(self, (*branch)->whenFalse);
                }
            }
        }
    };
    applyActions(applyActions, block.actions);

    for (auto& entry : block.coverage.fields)
    {
        if (!entry.playerFact) continue;
        if (entry.disposition == DescriptionFieldDisposition::SchemaDefault)
        {
            entry.projection = {};
            continue;
        }
        entry.projection = projectionForFact(
            descriptor,
            entry.level,
            *entry.playerFact);
        if (entry.level == DescriptionFactLevel::Audit
            && (entry.projection.full || entry.projection.compact))
        {
            assert(entry.disposition == DescriptionFieldDisposition::Visible
                || entry.disposition == DescriptionFieldDisposition::AbsorbedByPhrase);
            entry.reason = "archetype descriptor 以 closed player fact 宣告玩家詞組吸收";
        }
    }
}

void classifyPhraseAbsorptions(EffectDescriptionBlock& block)
{
    const auto& trigger = descriptionTrigger(block);
    std::vector<EffectDescriptionFact*> conditions;
    for (auto& fact : block.conditions)
    {
        if (std::holds_alternative<DescriptionConditionFact>(fact.value))
            conditions.push_back(&fact);
    }
    const auto conditionIs = [](const EffectDescriptionFact& fact, auto predicate)
    {
        return predicate(std::get<DescriptionConditionFact>(fact.value).condition);
    };

    for (auto* fact : conditions)
    {
        if (trigger.event == EffectEvent::UltimateCommitted
            && conditionIs(*fact, [](const EffectCondition& condition)
            {
                return std::holds_alternative<IsUltimateCondition>(condition);
            }))
        {
            fact->absorption = DescriptionPhraseAbsorption::UltimateEvent;
        }
    }

    const bool ordinaryAcceptedHit = trigger.event == EffectEvent::DamageResolved
        && std::ranges::any_of(conditions, [&](const EffectDescriptionFact* fact)
        {
            return conditionIs(*fact, [](const EffectCondition& condition)
            {
                const auto* accepted = std::get_if<AcceptedHitCondition>(&condition);
                return accepted && !accepted->requirePositiveDamage;
            });
        })
        && std::ranges::any_of(conditions, [&](const EffectDescriptionFact* fact)
        {
            return conditionIs(*fact, [](const EffectCondition& condition)
            {
                const auto* perspective = std::get_if<DamagePerspectiveCondition>(&condition);
                return perspective && perspective->perspective == DamagePerspective::Dealt;
            });
        });
    if (ordinaryAcceptedHit)
    {
        for (auto* fact : conditions)
        {
            if (conditionIs(*fact, [](const EffectCondition& condition)
                {
                    return std::holds_alternative<AcceptedHitCondition>(condition)
                        || std::holds_alternative<DamagePerspectiveCondition>(condition);
                }))
            {
                fact->absorption = DescriptionPhraseAbsorption::OrdinaryHitTrigger;
            }
        }
    }

    if (trigger.event == EffectEvent::AllyDied)
    {
        for (auto* fact : conditions)
        {
            if (conditionIs(*fact, [](const EffectCondition& condition)
                {
                    return std::holds_alternative<
                        EventTargetBelongsToBoundSourceCondition>(condition);
                }))
            {
                fact->absorption =
                    DescriptionPhraseAbsorption::SameComboAllyDeathTrigger;
            }
        }
    }

    const bool hasDamagePerspective = std::ranges::any_of(
        conditions,
        [&](const EffectDescriptionFact* fact)
        {
            return conditionIs(*fact, [](const EffectCondition& condition)
            {
                return std::holds_alternative<DamagePerspectiveCondition>(condition);
            });
        });
    if (hasDamagePerspective)
    {
        for (auto* fact : conditions)
        {
            if (conditionIs(*fact, [](const EffectCondition& condition)
                {
                    const auto* accepted = std::get_if<AcceptedHitCondition>(&condition);
                    return accepted && accepted->requirePositiveDamage;
                }))
            {
                fact->absorption =
                    DescriptionPhraseAbsorption::CompactPositiveDamagePerspective;
            }
        }
    }
}

std::optional<EffectRule> lifecycleRule(const EffectDescriptionBlock& block)
{
    if (block.trigger.size() != 1 || block.targets.size() != 1)
        return std::nullopt;
    const auto conditions = descriptionConditions(block);
    const auto actions = descriptionActions(block);
    if (actions.empty()) return std::nullopt;

    const auto& trigger = descriptionTrigger(block);
    const auto& target = descriptionTarget(block);
    const auto& qualifiers = ruleQualifiers(block);
    EffectRule rule;
    rule.id = block.sourceRuleId;
    rule.event = trigger.event;
    rule.observation = trigger.observation;
    rule.castMatch = trigger.castMatch;
    rule.selector = target.selector;
    for (const auto* condition : conditions) rule.conditions.push_back(*condition);
    rule.chancePct = qualifiers.chancePct;
    rule.maxActivations = qualifiers.maxActivations;
    rule.sharedCooldownFrames = qualifiers.sharedCooldownFrames;
    rule.intervalFrames = qualifiers.intervalFrames;
    rule.everyNthEvent = qualifiers.everyNthEvent;
    rule.activationLimit = qualifiers.activationLimit;
    rule.repetitionCount = qualifiers.repetitionCount;
    for (const auto* action : actions) rule.actions.push_back(*action);
    return rule;
}

bool matchesStatusLifecycleProducer(
    const EffectDescriptionBlock& block,
    BattleStatusKind status)
{
    const auto rule = lifecycleRule(block);
    return status == BattleStatusKind::SevenStarMark
        && rule
        && matchesSevenStarLifecycleProducer(*rule);
}

bool matchesStatusLifecycleConsumer(
    const EffectDescriptionBlock& block,
    BattleStatusKind status)
{
    const auto rule = lifecycleRule(block);
    return status == BattleStatusKind::SevenStarMark
        && rule
        && matchesSevenStarLifecycleConsumer(*rule);
}

bool matchesStackExplosionProducer(
    const EffectDescriptionBlock& block,
    BattleStatusKind status)
{
    const auto rule = lifecycleRule(block);
    return status == BattleStatusKind::PoisonExplosion
        && rule
        && matchesPoisonExplosionLifecycleProducer(*rule);
}

bool matchesStackExplosionConsumer(
    const EffectDescriptionBlock& block,
    BattleStatusKind status)
{
    const auto rule = lifecycleRule(block);
    return status == BattleStatusKind::PoisonExplosion
        && rule
        && matchesPoisonExplosionLifecycleConsumer(*rule);
}

void markLifecycleArchetype(
    EffectDescriptionBlock& block,
    DescriptionArchetype archetype)
{
    block.archetype = archetype;
    block.coverage.genericFallback = false;
    block.coverage.unmatchedShapeSignatures.clear();
}

void linkStatusLifecycles(EffectDescriptionDocument& document)
{
    std::vector<EffectDescriptionBlock*> blocks;
    for (auto& section : document.sections)
        for (auto& block : section.blocks)
            blocks.push_back(&block);

    for (auto* consumer : blocks)
    {
        const auto* consumed = consumeStatusAction(*consumer);
        if (consumed)
        {
            std::vector<EffectDescriptionBlock*> compatibleProducers;
            for (auto* producer : blocks)
            {
                if (producer == consumer) continue;
                if (matchesStatusLifecycleProducer(
                        *producer, consumed->status)
                    && matchesStatusLifecycleConsumer(
                        *consumer, consumed->status))
                    compatibleProducers.push_back(producer);
            }
            if (compatibleProducers.size() == 1)
            {
                markLifecycleArchetype(
                    *compatibleProducers.front(),
                    DescriptionArchetype::StatusLifecycle);
                markLifecycleArchetype(
                    *consumer,
                    DescriptionArchetype::StatusLifecycle);
            }
        }

        const auto& qualifiers = ruleQualifiers(*consumer);
        if (qualifiers.repetitionCount
            && qualifiers.repetitionCount->status)
        {
            const auto status = *qualifiers.repetitionCount->status;
            std::vector<EffectDescriptionBlock*> compatibleProducers;
            for (auto* producer : blocks)
            {
                if (producer == consumer) continue;
                if (matchesStackExplosionProducer(*producer, status)
                    && matchesStackExplosionConsumer(*consumer, status))
                    compatibleProducers.push_back(producer);
            }
            if (compatibleProducers.size() == 1)
            {
                markLifecycleArchetype(
                    *compatibleProducers.front(),
                    DescriptionArchetype::StackExplosion);
                markLifecycleArchetype(
                    *consumer,
                    DescriptionArchetype::StackExplosion);
            }
        }
    }
}


}  // namespace KysChess::EffectDescriptionDetail
