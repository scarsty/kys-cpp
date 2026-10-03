#include "BattleEffectSystem.h"

#include "../ChessBattleEffectSemantics.h"
#include "../ChessBattleEffectValidation.h"
#include "BattleEffectCommandSystem.h"
#include "BattleRuntimeRandom.h"
#include "BattleStatusSystem.h"
#include "BattleUnitValues.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <format>
#include <iterator>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace KysChess::Battle
{
int effectSourcePrecedence(EffectSourceKind kind)
{
    switch (kind)
    {
    case EffectSourceKind::Combo: return 0;
    case EffectSourceKind::Equipment: return 1;
    case EffectSourceKind::EquipmentSynergy: return 2;
    case EffectSourceKind::Neigong: return 3;
    case EffectSourceKind::Magic: return 4;
    }
    assert(false);
    return 0;
}

bool effectSourceRuleOrderLess(
    EffectSourceKind lhsSource,
    std::uint32_t lhsOrder,
    EffectSourceKind rhsSource,
    std::uint32_t rhsOrder)
{
    const int lhsPrecedence = effectSourcePrecedence(lhsSource);
    const int rhsPrecedence = effectSourcePrecedence(rhsSource);
    return lhsPrecedence != rhsPrecedence
        ? lhsPrecedence < rhsPrecedence
        : lhsOrder < rhsOrder;
}

EffectDamageOrigin makeEffectDamageOrigin(
    EffectSourceBinding binding,
    EffectRuleId ruleId,
    std::uint32_t authoredActionOrder,
    std::optional<EffectStatusContributionContext> statusContribution,
    std::optional<BattleCastProvenance> triggeringCast,
    std::optional<BattleAttackProvenance> triggeringAttack)
{
    if (statusContribution)
    {
        return EffectStatusDamageOrigin{
            .binding = std::move(binding),
            .contribution = std::move(*statusContribution),
            .behaviorActionOrder = authoredActionOrder,
            .triggeringCast = std::move(triggeringCast),
            .triggeringAttack = std::move(triggeringAttack),
        };
    }
    return EffectRuleDamageOrigin{
        .ruleId = ruleId,
        .binding = std::move(binding),
        .actionOrder = authoredActionOrder,
        .triggeringCast = std::move(triggeringCast),
        .triggeringAttack = std::move(triggeringAttack),
    };
}

const BattleAttackProvenance* effectDamageAttackProvenance(
    const EffectDamageOrigin& origin)
{
    if (const auto* attack = std::get_if<EffectAttackDamageOrigin>(&origin))
    {
        return &attack->provenance;
    }
    if (const auto* status = std::get_if<EffectStatusDamageOrigin>(&origin))
    {
        return status->triggeringAttack ? &*status->triggeringAttack : nullptr;
    }
    if (const auto* rule = std::get_if<EffectRuleDamageOrigin>(&origin))
    {
        return rule->triggeringAttack ? &*rule->triggeringAttack : nullptr;
    }
    return nullptr;
}

const BattleCastProvenance* effectDamageCastProvenance(
    const EffectDamageOrigin& origin)
{
    if (const auto* attack = effectDamageAttackProvenance(origin))
    {
        return &attack->cast;
    }
    if (const auto* status = std::get_if<EffectStatusDamageOrigin>(&origin))
    {
        return status->triggeringCast ? &*status->triggeringCast : nullptr;
    }
    if (const auto* rule = std::get_if<EffectRuleDamageOrigin>(&origin))
    {
        return rule->triggeringCast ? &*rule->triggeringCast : nullptr;
    }
    return nullptr;
}

EffectExecutionOrderKey effectExecutionOrderKey(
    const EffectCommandMetadata& metadata)
{
    auto result = EffectExecutionOrderKey{
        .sourcePrecedence = effectSourcePrecedence(metadata.binding.kind),
        .producerRuleOrder = metadata.ruleOrder,
        .lane = metadata.executionLane,
        .producerActionOrder = metadata.producerActionOrder,
        .behaviorRuleOrder = metadata.behaviorRuleOrder,
        .contributionSequence = metadata.statusContribution
            ? metadata.statusContribution->appliedSequence
            : std::uint64_t{},
        .actionOrder = metadata.actionOrder,
        .targetOrder = metadata.targetOrder,
        .commandOrdinal = metadata.commandOrdinal,
    };
    if (metadata.statusContribution)
    {
        result = statusBehaviorExecutionOrderKey(
            metadata.binding,
            metadata.ruleOrder,
            metadata.producerActionOrder,
            metadata.behaviorRuleOrder,
            metadata.statusContribution->holderUnitId,
            metadata.statusContribution->appliedSequence,
            metadata.actionOrder);
        result.targetOrder = metadata.targetOrder;
        result.commandOrdinal = metadata.commandOrdinal;
    }
    return result;
}

EffectExecutionOrderKey statusBehaviorExecutionOrderKey(
    const EffectSourceBinding& binding,
    std::uint32_t producerRuleOrder,
    std::uint32_t producerActionOrder,
    std::uint32_t behaviorRuleOrder,
    int holderUnitId,
    std::uint64_t contributionSequence,
    std::uint32_t actionOrder)
{
    return {
        .sourcePrecedence = effectSourcePrecedence(binding.kind),
        .producerRuleOrder = producerRuleOrder,
        .lane = EffectExecutionLane::StatusBehavior,
        .producerActionOrder = producerActionOrder,
        .behaviorRuleOrder = behaviorRuleOrder,
        .holderUnitId = holderUnitId,
        .contributionSequence = contributionSequence,
        .actionOrder = actionOrder,
    };
}

namespace
{

template<class... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};

template<class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

void appendDispatchResult(BattleEffectDispatchResult& destination,
                          BattleEffectDispatchResult source)
{
    std::uint64_t nextCommandOrdinal{};
    for (const auto& command : destination.commands)
    {
        assert(command.metadata.commandOrdinal
            < std::numeric_limits<std::uint64_t>::max());
        nextCommandOrdinal = std::max(
            nextCommandOrdinal,
            command.metadata.commandOrdinal + 1);
    }
    for (auto& command : source.commands)
    {
        assert(command.metadata.commandOrdinal
            <= std::numeric_limits<std::uint64_t>::max() - nextCommandOrdinal);
        command.metadata.commandOrdinal += nextCommandOrdinal;
    }
    destination.activations.insert(
        destination.activations.end(),
        std::make_move_iterator(source.activations.begin()),
        std::make_move_iterator(source.activations.end()));
    destination.commands.insert(
        destination.commands.end(),
        std::make_move_iterator(source.commands.begin()),
        std::make_move_iterator(source.commands.end()));
}

EffectRuleRuntimeKey runtimeKey(const EffectSourceBinding& binding, EffectRuleId ruleId)
{
    return {
        .ownerUnitId = binding.ownerUnitId,
        .sourceKind = binding.kind,
        .sourceId = binding.sourceId,
        .sourceInstanceId = binding.runtimeInstanceId,
        .ruleId = ruleId,
    };
}

bool ruleActivationAvailable(const EffectRule& rule, const EffectRuleRuntimeState& runtime, int frame)
{
    return (rule.maxActivations <= 0 || runtime.activationCount < rule.maxActivations)
        && (rule.sharedCooldownFrames <= 0 || frame >= runtime.sharedCooldownUntilFrame);
}

std::pmr::vector<std::size_t> orderedRuleIndices(
    std::span<const BoundEffectRule> rules,
    std::span<const std::size_t> indices,
    std::pmr::memory_resource* memoryResource)
{
    std::pmr::vector<std::size_t> ordered(memoryResource);
    ordered.reserve(indices.size());
    for (const auto index : indices)
    {
        if (index >= rules.size())
        {
            throw std::out_of_range("效果規則索引超出範圍");
        }
        ordered.push_back(index);
    }
    std::ranges::stable_sort(ordered, [&](std::size_t lhsIndex, std::size_t rhsIndex)
    {
        const auto& lhs = rules[lhsIndex];
        const auto& rhs = rules[rhsIndex];
        return effectSourceRuleOrderLess(
            lhs.binding.kind,
            lhs.order,
            rhs.binding.kind,
            rhs.order);
    });
    return ordered;
}

int& activationEvaluation(
    EffectRuleRuntimeState& runtime,
    BattleCastId castId,
    int targetUnitId)
{
    const auto found = std::ranges::find_if(
        runtime.activationEvaluations,
        [&](const auto& entry)
        {
            return entry.castId == castId.value()
                && entry.targetUnitId == targetUnitId;
        });
    if (found != runtime.activationEvaluations.end()) return found->count;
    runtime.activationEvaluations.push_back({
        .castId = castId.value(),
        .targetUnitId = targetUnitId,
    });
    return runtime.activationEvaluations.back().count;
}

EffectStateKey stateKey(const EffectSourceBinding& binding,
                        EffectStateSlot slot,
                        std::uint64_t scopeId)
{
    return {
        .ownerUnitId = binding.ownerUnitId,
        .sourceKind = binding.kind,
        .sourceId = binding.sourceId,
        .sourceInstanceId = binding.runtimeInstanceId,
        .slot = slot,
        .scopeId = scopeId,
    };
}

std::int64_t saturatingMultiply(std::int64_t lhs, std::int64_t rhs)
{
    if (lhs == 0 || rhs == 0)
    {
        return 0;
    }
    if (lhs == -1 && rhs == std::numeric_limits<std::int64_t>::min())
    {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (rhs == -1 && lhs == std::numeric_limits<std::int64_t>::min())
    {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (lhs > 0)
    {
        if (rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() / rhs)
        {
            return std::numeric_limits<std::int64_t>::max();
        }
        if (rhs < 0 && rhs < std::numeric_limits<std::int64_t>::min() / lhs)
        {
            return std::numeric_limits<std::int64_t>::min();
        }
    }
    else
    {
        if (rhs > 0 && lhs < std::numeric_limits<std::int64_t>::min() / rhs)
        {
            return std::numeric_limits<std::int64_t>::min();
        }
        if (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::max() / rhs)
        {
            return std::numeric_limits<std::int64_t>::max();
        }
    }
    return lhs * rhs;
}

std::int64_t saturatingAdd(std::int64_t lhs, std::int64_t rhs)
{
    if (rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() - rhs)
    {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::min() - rhs)
    {
        return std::numeric_limits<std::int64_t>::min();
    }
    return lhs + rhs;
}

int clampToInt(std::int64_t value)
{
    return static_cast<int>(std::clamp(
        value,
        static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

const EffectDamageOrigin* damageOrigin(const EffectEventContext& context)
{
    return std::visit(Overloaded{
        [](const DamageResultEventData& data) -> const EffectDamageOrigin* { return &data.origin; },
        [](const ShieldBreakEventData& data) -> const EffectDamageOrigin* { return &data.cause; },
        [](const DeathEventData& data) -> const EffectDamageOrigin* { return &data.cause; },
        [](const auto&) -> const EffectDamageOrigin* { return nullptr; },
    }, context.payload);
}

std::optional<BattleDamageKind> damageKind(const EffectEventContext& context)
{
    return std::visit(Overloaded{
        [](const AttackEventData& data) -> std::optional<BattleDamageKind> { return data.damageKind; },
        [](const HitEventData& data) -> std::optional<BattleDamageKind> { return data.damageKind; },
        [](const DamageResultEventData& data) -> std::optional<BattleDamageKind> { return data.damageKind; },
        [](const auto&) -> std::optional<BattleDamageKind> { return std::nullopt; },
    }, context.payload);
}

std::optional<BattleHealKind> healKind(const EffectEventContext& context)
{
    return std::visit(Overloaded{
        [](const HealRequestEventData& data) -> std::optional<BattleHealKind> { return data.kind; },
        [](const HealResultEventData& data) -> std::optional<BattleHealKind> { return data.request.kind; },
        [](const auto&) -> std::optional<BattleHealKind> { return std::nullopt; },
    }, context.payload);
}

const EffectUnitSnapshot* transactionTargetSnapshot(const EffectEventContext& context)
{
    return std::visit(Overloaded{
        [&context](const HitEventData& data)
        {
            const auto* target = context.header.battle.findUnit(data.targetUnitId);
            assert(target);
            return target;
        },
        [](const DamageResultEventData& data) { return &data.defenderBefore; },
        [](const HealRequestEventData& data) { return &data.targetBefore; },
        [](const HealResultEventData& data) { return &data.request.targetBefore; },
        [](const ShieldBreakEventData& data) { return &data.targetBefore; },
        [](const DeathEventData& data) { return &data.deadBefore; },
        [](const auto&) -> const EffectUnitSnapshot* { return nullptr; },
    }, context.payload);
}

const EffectUnitSnapshot* sourceSnapshot(const EffectEventContext& context)
{
    return std::visit(Overloaded{
        [&context](const AttackEventData& data)
        {
            const auto* source = context.header.battle.findUnit(
                data.provenance.cast.sourceUnitId);
            assert(source);
            return source;
        },
        [&context](const HitEventData& data)
        {
            const auto* source = context.header.battle.findUnit(
                data.provenance.cast.sourceUnitId);
            assert(source);
            return source;
        },
        [](const DamageResultEventData& data) -> const EffectUnitSnapshot*
        {
            return data.attackerBefore ? &*data.attackerBefore : nullptr;
        },
        [](const HealRequestEventData& data) { return &data.sourceBefore; },
        [](const HealResultEventData& data) { return &data.request.sourceBefore; },
        [](const DeathEventData& data) -> const EffectUnitSnapshot*
        {
            return data.killer ? &*data.killer : nullptr;
        },
        [&context](const auto&) -> const EffectUnitSnapshot* { return context.scope.owner; },
    }, context.payload);
}

const EffectUnitSnapshot* snapshotForTarget(const EffectEventContext& context, int unitId)
{
    if (const auto* transaction = transactionTargetSnapshot(context);
        transaction && transaction->id == unitId)
    {
        return transaction;
    }
    if (const auto* source = sourceSnapshot(context); source && source->id == unitId)
    {
        return source;
    }
    if (context.scope.owner->id == unitId)
    {
        return context.scope.owner;
    }
    return context.header.battle.findUnit(unitId);
}

std::optional<int> sourceUnitId(const EffectEventContext& context)
{
    if (const auto* source = sourceSnapshot(context))
    {
        return source->id;
    }
    if (const auto* cast = effectCastProvenance(context))
    {
        return cast->sourceUnitId;
    }
    return std::nullopt;
}

std::optional<int> originalTargetUnitId(const EffectEventContext& context)
{
    return std::visit(Overloaded{
        [](const CastPlanEventData& data) -> std::optional<int> { return data.preferredTargetUnitId; },
        [](const CastCommitEventData& data) -> std::optional<int> { return data.targetUnitId; },
        [](const AttackEventData& data) -> std::optional<int> { return data.originalTargetUnitId; },
        [](const HitEventData& data) -> std::optional<int> { return data.originalTargetUnitId; },
        [](const CastAggregateEventData& data) -> std::optional<int> { return data.originalTargetUnitId; },
        [](const auto&) -> std::optional<int> { return std::nullopt; },
    }, context.payload);
}

const EffectUnitSnapshot* requiredTargetSnapshot(EffectRequiredTarget requiredTarget,
                                                 const EffectEventContext& context)
{
    switch (requiredTarget)
    {
    case EffectRequiredTarget::Self:
        return context.scope.owner;
    case EffectRequiredTarget::SourceUnit:
        return sourceSnapshot(context);
    case EffectRequiredTarget::TransactionTarget:
    case EffectRequiredTarget::HitTarget:
        return transactionTargetSnapshot(context);
    case EffectRequiredTarget::OriginalAttackTarget:
        if (const auto unitId = originalTargetUnitId(context))
        {
            return snapshotForTarget(context, *unitId);
        }
        return nullptr;
    }
    assert(false);
    return nullptr;
}

Pointf selectorCenter(const EffectEventContext& context)
{
    return std::visit(Overloaded{
        [](const AttackEventData& data) { return data.spawnPosition; },
        [](const HitEventData& data) { return data.contactPosition; },
        [&context](const CastPlanEventData& data)
        {
            if (const auto* target = context.header.battle.findUnit(data.preferredTargetUnitId))
            {
                return target->position;
            }
            return context.scope.owner->position;
        },
        [&context](const CastCommitEventData& data)
        {
            if (const auto* target = context.header.battle.findUnit(data.targetUnitId))
            {
                return target->position;
            }
            return context.scope.owner->position;
        },
        [&context](const auto&) { return context.scope.owner->position; },
    }, context.payload);
}

std::optional<int> finalHpDamage(const EffectEventContext& context)
{
    if (const auto* data = std::get_if<DamageResultEventData>(&context.payload))
    {
        return data->finalHpDamage;
    }
    return std::nullopt;
}

std::optional<int> targetMpBeforeCast(const EffectEventContext& context, int unitId)
{
    auto findResource = [unitId](const EffectResourcesBeforeCastSnapshot& snapshot)
        -> std::optional<int>
    {
        const auto resources = snapshot.values();
        const auto it = std::find_if(resources.begin(), resources.end(), [unitId](const auto& resource)
        {
            return resource.unitId == unitId;
        });
        return it != resources.end() ? std::optional<int>{ it->mp } : std::nullopt;
    };

    return std::visit(Overloaded{
        [&](const CastPlanEventData& data) -> std::optional<int>
        {
            if (const auto value = findResource(data.resourcesBeforeCast))
            {
                return value;
            }
            return unitId == data.provenance.sourceUnitId ? std::optional<int>{ data.mpBefore } : std::nullopt;
        },
        [&](const CastCommitEventData& data) -> std::optional<int>
        {
            if (const auto value = findResource(data.resourcesBeforeCast))
            {
                return value;
            }
            return unitId == data.provenance.sourceUnitId ? std::optional<int>{ data.mpBefore } : std::nullopt;
        },
        [&](const CastAggregateEventData& data) { return findResource(data.resourcesBeforeCast); },
        [](const auto&) -> std::optional<int> { return std::nullopt; },
    }, context.payload);
}

std::optional<int> targetMaxMpBeforeCast(const EffectEventContext& context, int unitId)
{
    auto findResource = [unitId](const EffectResourcesBeforeCastSnapshot& snapshot)
        -> std::optional<int>
    {
        const auto resources = snapshot.values();
        const auto it = std::find_if(resources.begin(), resources.end(), [unitId](const auto& resource)
        {
            return resource.unitId == unitId;
        });
        return it != resources.end() ? std::optional<int>{ it->maxMp } : std::nullopt;
    };

    return std::visit(Overloaded{
        [&](const CastPlanEventData& data) { return findResource(data.resourcesBeforeCast); },
        [&](const CastCommitEventData& data) { return findResource(data.resourcesBeforeCast); },
        [&](const CastAggregateEventData& data) { return findResource(data.resourcesBeforeCast); },
        [](const auto&) -> std::optional<int> { return std::nullopt; },
    }, context.payload);
}

bool isAttackDamageOrigin(const EffectEventContext& context)
{
    const auto* origin = damageOrigin(context);
    return origin && std::holds_alternative<EffectAttackDamageOrigin>(*origin);
}

bool ratioLess(int lhsValue, int lhsMaximum, int rhsValue, int rhsMaximum)
{
    assert(lhsMaximum > 0);
    assert(rhsMaximum > 0);
    return static_cast<std::int64_t>(lhsValue) * rhsMaximum <
           static_cast<std::int64_t>(rhsValue) * lhsMaximum;
}

bool ratioEqual(int lhsValue, int lhsMaximum, int rhsValue, int rhsMaximum)
{
    assert(lhsMaximum > 0);
    assert(rhsMaximum > 0);
    return static_cast<std::int64_t>(lhsValue) * rhsMaximum ==
           static_cast<std::int64_t>(rhsValue) * lhsMaximum;
}

double distanceSquared(Pointf lhs, Pointf rhs)
{
    const double dx = static_cast<double>(lhs.x) - rhs.x;
    const double dy = static_cast<double>(lhs.y) - rhs.y;
    return dx * dx + dy * dy;
}

bool matchesTeamFilter(const EffectUnitSnapshot& unit,
                       const EffectEventContext& context,
                       EffectTeamFilter filter)
{
    switch (filter)
    {
    case EffectTeamFilter::Any: return true;
    case EffectTeamFilter::Ally: return unit.team == context.scope.owner->team;
    case EffectTeamFilter::Enemy: return unit.team != context.scope.owner->team;
    }
    assert(false);
    return false;
}

bool matchesSelectorRequirements(const EffectUnitSnapshot& unit,
                                 const EffectSelector& selector,
                                 const EffectEventContext& context)
{
    if (selector.excludeOwner && unit.id == context.scope.owner->id)
    {
        return false;
    }
    if (!matchesTeamFilter(unit, context, selector.team))
    {
        return false;
    }
    if (selector.requiredBoundMagic
        && !unit.usesMagic(context.scope.binding.sourceId))
    {
        return false;
    }
    return selector.requiredMartialCategory == EffectMartialCategory::None
        || unit.martialCategory == selector.requiredMartialCategory;
}

bool matchesSelectorMembership(const EffectUnitSnapshot& unit,
                               const EffectSelector& selector,
                               const EffectEventContext& context)
{
    switch (selector.kind)
    {
    case EffectSelectorKind::ComboMembers:
        return std::ranges::contains(unit.comboIds, context.scope.binding.sourceId);
    case EffectSelectorKind::Allies:
    case EffectSelectorKind::LowestHpAllies:
    case EffectSelectorKind::LowestMpAllies:
        return unit.team == context.scope.owner->team;
    case EffectSelectorKind::Enemies:
    case EffectSelectorKind::HighestMpEnemy:
    case EffectSelectorKind::StrongestEnemies:
    case EffectSelectorKind::NearestEnemies:
    case EffectSelectorKind::FarthestEnemy:
        return unit.team != context.scope.owner->team;
    case EffectSelectorKind::AlliesUsingMartialCategory:
        return unit.team == context.scope.owner->team
            && unit.martialCategory == selector.requiredMartialCategory;
    case EffectSelectorKind::Self:
    case EffectSelectorKind::SourceUnit:
    case EffectSelectorKind::TransactionTarget:
    case EffectSelectorKind::HitTarget:
    case EffectSelectorKind::OriginalAttackTarget:
    case EffectSelectorKind::AllLivingUnits:
    case EffectSelectorKind::UnitsInRadius:
    case EffectSelectorKind::UnitsInSquare:
        return true;
    }
    assert(false);
    return false;
}

enum class SelectorOrdering
{
    UnitId,
    LowestHpRatio,
    LowestMp,
    HighestMp,
    Strongest,
    Nearest,
    Farthest,
};

void randomizeTieGroups(std::span<const EffectUnitSnapshot*> candidates,
                        SelectorOrdering ordering,
                        Pointf center,
                        BattleRuntimeRandom& random)
{
    auto tied = [&](const EffectUnitSnapshot& lhs, const EffectUnitSnapshot& rhs)
    {
        switch (ordering)
        {
        case SelectorOrdering::UnitId: return true;
        case SelectorOrdering::LowestHpRatio:
            return ratioEqual(lhs.hp, lhs.maxHp, rhs.hp, rhs.maxHp);
        case SelectorOrdering::LowestMp:
        case SelectorOrdering::HighestMp:
            return lhs.mp == rhs.mp;
        case SelectorOrdering::Strongest:
        {
            const auto score = [](const EffectUnitSnapshot& unit)
            {
                std::int64_t result = unit.cost > 0 ? 1 : 0;
                for (int index = 0; index < unit.star; ++index)
                {
                    result = saturatingMultiply(result, unit.cost);
                }
                return result;
            };
            return score(lhs) == score(rhs) && lhs.maxHp == rhs.maxHp;
        }
        case SelectorOrdering::Nearest:
        case SelectorOrdering::Farthest:
            return distanceSquared(lhs.position, center) == distanceSquared(rhs.position, center);
        }
        return false;
    };

    for (std::size_t begin = 0; begin < candidates.size();)
    {
        std::size_t end = begin + 1;
        while (end < candidates.size() && tied(*candidates[begin], *candidates[end]))
        {
            ++end;
        }
        for (std::size_t index = end; index > begin + 1; --index)
        {
            const auto swapIndex = begin + static_cast<std::size_t>(
                random.nextInt(static_cast<int>(index - begin)));
            std::swap(candidates[index - 1], candidates[swapIndex]);
        }
        begin = end;
    }
}

bool matchesDamageKindLabel(BattleDamageKind kind, std::string_view label)
{
    if (label == "全部" || label == "All")
    {
        return true;
    }
    switch (kind)
    {
    case BattleDamageKind::Physical: return label == "物理" || label == "Physical";
    case BattleDamageKind::Skill: return label == "招式" || label == "技能" || label == "Skill";
    case BattleDamageKind::Pure: return label == "純粹" || label == "Pure";
    case BattleDamageKind::Poison: return label == "中毒" || label == "持續" || label == "Poison";
    case BattleDamageKind::Bleed: return label == "流血" || label == "持續" || label == "Bleed";
    case BattleDamageKind::Effect: return label == "效果" || label == "Effect";
    case BattleDamageKind::Execute: return label == "處決" || label == "Execute";
    }
    return false;
}

bool matchesHealKindLabel(BattleHealKind kind, std::string_view label)
{
    assert(kind != BattleHealKind::Count);
    return label == effectHealKindAuthorLabel(kind);
}

bool conditionSatisfied(const EffectCondition& condition,
                        const EffectEventContext& context,
                        const EffectUnitSnapshot& target,
                        bool selectionAvailable)
{
    return std::visit(Overloaded{
        [&](const IsUltimateCondition&)
        {
            const auto* cast = effectCastProvenance(context);
            return cast && cast->ultimate;
        },
        [&](const CastUsesEffectSourceMagicCondition&)
        {
            const auto* cast = effectCastProvenance(context);
            return cast && cast->magicId == context.scope.binding.sourceId;
        },
        [&](const IsMainProjectileCondition&)
        {
            const auto* attack = effectAttackProvenance(context);
            return attack && attack->mainProjectile;
        },
        [&](const IsRootAttackCondition&)
        {
            const auto* attack = effectAttackProvenance(context);
            return attack && attack->rootAttack;
        },
        [&](const SourceHpRatioAtMostCondition& value)
        {
            return context.scope.owner->maxHp > 0 &&
                   static_cast<std::int64_t>(context.scope.owner->hp) * 100 <=
                       static_cast<std::int64_t>(context.scope.owner->maxHp) * value.percent;
        },
        [&](const SourceHpRatioBelowCondition& value)
        {
            return context.scope.owner->maxHp > 0 &&
                   static_cast<std::int64_t>(context.scope.owner->hp) * 100 <
                       static_cast<std::int64_t>(context.scope.owner->maxHp) * value.percent;
        },
        [&](const SourceIsLastAliveCondition&)
        {
            return context.scope.owner->alive
                && std::ranges::count_if(
                    context.header.battle.units(),
                    [&](const EffectUnitSnapshot& unit)
                    {
                        return unit.alive && unit.team == context.scope.owner->team;
                    }) == 1;
        },
        [&](const TargetHpRatioAtMostCondition& value)
        {
            return target.maxHp > 0 &&
                   static_cast<std::int64_t>(target.hp) * 100 <=
                       static_cast<std::int64_t>(target.maxHp) * value.percent;
        },
        [&](const TargetNotInvincibleCondition&)
        {
            return !target.invincible;
        },
        [&](const SourceHasStateCondition& value)
        {
            return context.scope.owner->hasState(value.state);
        },
        [&](const TargetHasStateCondition& value)
        {
            return target.hasState(value.state);
        },
        [&](const TargetHasStateFromEffectOwnerCondition& value)
        {
            return target.hasStateFromSource(
                value.state,
                context.scope.binding.ownerUnitId);
        },
        [&](const SourceStackAtLeastCondition& value)
        {
            return context.scope.owner->stackCount(value.stack) >= value.count;
        },
        [&](const OtherLivingAllyUsesBoundMagicCondition&)
        {
            return std::ranges::any_of(context.header.battle.units(), [&](const EffectUnitSnapshot& unit)
            {
                return unit.alive && unit.id != context.scope.owner->id &&
                       unit.team == context.scope.owner->team
                       && unit.usesMagic(context.scope.binding.sourceId);
            });
        },
        [&](const CastDistinctTargetCountAtLeastCondition& value)
        {
            const auto* aggregate = std::get_if<CastAggregateEventData>(&context.payload);
            return aggregate && static_cast<int>(aggregate->aggregate.distinctHitUnitIds.size()) >= value.count;
        },
        [&](const AttackOrdinalEqualsCondition& value)
        {
            const auto* attack = effectAttackProvenance(context);
            return attack && attack->attackOrdinal == value.ordinal;
        },
        [&](const HealKindInCondition& value)
        {
            const auto kind = healKind(context);
            return kind && std::ranges::any_of(value.kinds, [&](const std::string& label)
            {
                return matchesHealKindLabel(*kind, label);
            });
        },
        [&](const DamageOriginIsAttackCondition&) { return isAttackDamageOrigin(context); },
        [&](const DamageKilledTargetCondition&)
        {
            const auto* damage = std::get_if<DamageResultEventData>(&context.payload);
            return damage && damage->killed;
        },
        [&](const AcceptedHitCondition& value)
        {
            const auto* damage = std::get_if<DamageResultEventData>(&context.payload);
            if (!damage
                || damage->blocked
                || !std::holds_alternative<EffectAttackDamageOrigin>(damage->origin))
            {
                return false;
            }
            if (value.requirePositiveDamage && damage->finalHpDamage <= 0)
            {
                return false;
            }
            return true;
        },
        [&](const EventTargetBelongsToBoundSourceCondition&)
        {
            if (context.scope.binding.kind != EffectSourceKind::Combo)
            {
                return false;
            }
            const auto* target = transactionTargetSnapshot(context);
            return target
                && std::ranges::contains(target->comboIds, context.scope.binding.sourceId);
        },
        [&](const DamagePerspectiveCondition& value)
        {
            const auto* damage = std::get_if<DamageResultEventData>(&context.payload);
            if (!damage)
            {
                return false;
            }
            return value.perspective == DamagePerspective::Dealt
                ? damage->attackerBefore && damage->attackerBefore->id == context.scope.owner->id
                : damage->defenderBefore.id == context.scope.owner->id;
        },
        [&](const DamageKindInCondition& value)
        {
            const auto kind = damageKind(context);
            return kind && std::ranges::any_of(value.kinds, [&](const std::string& label)
            {
                return matchesDamageKindLabel(*kind, label);
            });
        },
        [&](const TargetMpWasFullBeforeCastCondition&)
        {
            const auto mp = targetMpBeforeCast(context, target.id);
            const auto maxMp = targetMaxMpBeforeCast(context, target.id);
            if (mp && maxMp)
            {
                return *mp >= *maxMp;
            }
            return target.maxMp > 0 && target.mp >= target.maxMp;
        },
        [&](const RandomSelectionAvailableCondition&) { return selectionAvailable; },
        [&](const TargetIsStatusHolderCondition&)
        {
            return context.scope.statusContribution
                && target.id == context.scope.statusContribution->holderUnitId;
        },
    }, condition);
}

bool conditionsSatisfied(std::span<const EffectCondition> conditions,
                         const EffectEventContext& context,
                         const EffectUnitSnapshot& target,
                         bool selectionAvailable)
{
    return std::ranges::all_of(conditions, [&](const EffectCondition& condition)
    {
        return conditionSatisfied(condition, context, target, selectionAvailable);
    });
}

std::optional<CastPropagationPolicy> propagationPolicy(const EffectEventContext& context)
{
    if (const auto* attack = effectAttackProvenance(context))
    {
        return attack->propagation;
    }
    if (const auto* cast = effectCastProvenance(context))
    {
        return cast->propagation;
    }
    return std::nullopt;
}

struct BoundEffectRuleView
{
    const EffectSourceBinding& binding;
    const EffectRule& definition;
    const EffectRule& rule() const { return definition; }
    std::uint32_t order{};
    std::optional<BattleCastId> castScope;
    CastPropagationPolicy scopedPropagation = CastPropagationPolicy::SourceRules;

    BoundEffectRuleView(const BoundEffectRule& bound)
        : binding(bound.binding), definition(bound.rule()), order(bound.order)
        , castScope(bound.castScope), scopedPropagation(bound.scopedPropagation) {}
    BoundEffectRuleView(const EffectSourceBinding& binding, const EffectRule& rule,
                       std::uint32_t order)
        : binding(binding), definition(rule), order(order) {}
};

bool isOwnerObservation(const BoundEffectRuleView& bound,
                        const EffectEventContext& context)
{
    if (const auto* hit = std::get_if<HitEventData>(&context.payload);
        hit && hit->targetUnitId == bound.binding.ownerUnitId)
    {
        return true;
    }
    if (const auto* damage = std::get_if<DamageResultEventData>(&context.payload);
        damage && damage->defenderBefore.id == bound.binding.ownerUnitId)
    {
        return true;
    }
    if (const auto* death = std::get_if<DeathEventData>(&context.payload);
        death && (death->deadBefore.id == bound.binding.ownerUnitId
            || death->allyOfOwner))
    {
        return true;
    }
    if (const auto* shield = std::get_if<ShieldBreakEventData>(&context.payload);
        shield && shield->targetBefore.id == bound.binding.ownerUnitId)
    {
        return true;
    }
    return false;
}

bool ruleObservesEvent(const BoundEffectRuleView& bound,
                       const EffectEventContext& context)
{
    switch (bound.rule().observation)
    {
    case EffectObservationScope::Owner:
        return bound.binding.ownerUnitId < 0
            || bound.binding.ownerUnitId == context.scope.owner->id;
    case EffectObservationScope::OwnerTeamEventSource:
    {
        const auto* source = sourceSnapshot(context);
        return source && source->team == bound.binding.sourceTeam;
    }
    case EffectObservationScope::ComboMemberEventSource:
    {
        const auto* source = sourceSnapshot(context);
        const auto* owner = context.header.battle.findUnit(bound.binding.ownerUnitId);
        assert(bound.binding.kind == EffectSourceKind::Combo);
        assert(owner && source);
        return owner->alive && source->alive
            && source->team == bound.binding.sourceTeam
            && std::ranges::contains(source->comboIds, bound.binding.sourceId);
    }
    case EffectObservationScope::EventTarget:
    {
        const auto* target = transactionTargetSnapshot(context);
        return target && target->id == bound.binding.ownerUnitId;
    }
    case EffectObservationScope::StatusHolderEventSource:
        return context.scope.statusContribution
            && (context.event == EffectEvent::FrameAdvanced
                || sourceUnitId(context)
                    == context.scope.statusContribution->holderUnitId);
    case EffectObservationScope::StatusHolderEventTarget:
    {
        const auto* target = transactionTargetSnapshot(context);
        return context.scope.statusContribution
            && target
            && target->id == context.scope.statusContribution->holderUnitId;
    }
    case EffectObservationScope::StatusSourceEventSource:
        return context.scope.statusContribution
            && sourceUnitId(context) == context.scope.statusContribution->sourceUnitId;
    case EffectObservationScope::SourceOwnerTeamEventSource:
    {
        const auto* source = sourceSnapshot(context);
        return source && source->team == bound.binding.sourceTeam;
    }
    }
    assert(false);
    return false;
}

void rewriteObservedOwner(const BoundEffectRuleView& bound,
                          EffectEventContext& context)
{
    if (bound.rule().observation == EffectObservationScope::Owner)
    {
        return;
    }
    const auto* owner = context.header.battle.findUnit(bound.binding.ownerUnitId);
    if (!owner)
    {
        throw std::logic_error(std::format(
            "觀察規則的效果擁有者 {} 不存在",
            bound.binding.ownerUnitId));
    }
    context.scope.owner = owner;
}

bool castScopeMatches(const BoundEffectRuleView& bound,
                      const EffectEventContext& context)
{
    if (!bound.castScope)
    {
        return true;
    }
    const auto* cast = effectCastProvenance(context);
    if (cast)
    {
        return cast->castId == *bound.castScope
            || isOwnerObservation(bound, context);
    }
    // Frame, non-attack damage and other owner events have no cast provenance;
    // the alias lifetime itself defines their temporary availability.
    return true;
}

void rewriteBorrowedCast(BattleCastProvenance& cast,
                         const BoundEffectRuleView& bound)
{
    assert(bound.castScope);
    assert(cast.castId == *bound.castScope);
    cast.magicId = bound.binding.sourceId;
    cast.origin = CastOriginKind::BorrowedEffect;
    cast.propagation = bound.scopedPropagation;
}

void rewriteBorrowedAttack(BattleAttackProvenance& attack,
                           const BoundEffectRuleView& bound)
{
    rewriteBorrowedCast(attack.cast, bound);
    attack.propagation = bound.scopedPropagation;
}

void rewriteScopedRuleContext(const BoundEffectRuleView& bound,
                              EffectEventContext& context)
{
    context.scope.binding = bound.binding;
    if (!bound.castScope)
    {
        return;
    }
    const auto* cast = effectCastProvenance(context);
    if (!cast || cast->castId != *bound.castScope)
    {
        return;
    }

    if (const auto* attack = effectAttackProvenance(context))
    {
        context.scope.attack = *attack;
        rewriteBorrowedAttack(*context.scope.attack, bound);
    }
    context.scope.cast = *cast;
    rewriteBorrowedCast(*context.scope.cast, bound);
}

bool isHitRuleEvent(EffectEvent event)
{
    switch (event)
    {
    case EffectEvent::MainProjectileBeforeDamage:
    case EffectEvent::HitBeforeDamage:
    case EffectEvent::DamageResolved:
    case EffectEvent::ShieldBroken:
    case EffectEvent::UnitDied:
        return true;
    default:
        return false;
    }
}

bool ruleAllowedByPropagation(const BoundEffectRuleView& bound,
                              const EffectEventContext& context)
{
    const auto policy = propagationPolicy(context);
    if (!policy)
    {
        return true;
    }
    if (*policy == CastPropagationPolicy::SourceHitRulesOnly
        && !isHitRuleEvent(context.event))
    {
        return false;
    }
    if (isOwnerObservation(bound, context))
    {
        return true;
    }
    if (bound.rule().observation != EffectObservationScope::Owner)
    {
        return *policy != CastPropagationPolicy::NoEffectRules;
    }
    switch (*policy)
    {
    case CastPropagationPolicy::SourceRules:
        return true;
    case CastPropagationPolicy::SourceHitRulesOnly:
        return true;
    case CastPropagationPolicy::SuppressUltimateRules:
        return bound.binding.kind != EffectSourceKind::Magic;
    case CastPropagationPolicy::BorrowedUltimateRules:
        return true;
    case CastPropagationPolicy::NoEffectRules:
        return false;
    }
    assert(false);
    return false;
}

bool ruleMatchesMagicCast(const BoundEffectRuleView& bound,
                          const EffectEventContext& context)
{
    if (bound.binding.kind != EffectSourceKind::Magic)
    {
        return true;
    }
    const auto* cast = effectCastProvenance(context);
    if (!cast)
    {
        return true;
    }
    if (cast->origin == CastOriginKind::Reflection)
    {
        // 反射 child cast 沒有武功身分；來襲武功只保留在 attack payload
        // 作為呈現與威力 metadata，任何武功綁定規則都不可觀察此 synthetic cast。
        return false;
    }
    if (bound.rule().observation != EffectObservationScope::Owner)
    {
        return true;
    }
    if (isOwnerObservation(bound, context))
    {
        // Defender/death observation rules subscribe to the bound owner, not
        // to the incoming attacker's selected magic.
        return true;
    }
    if (bound.rule().castMatch == EffectCastMatch::OwnerAnyCast)
    {
        return cast->sourceUnitId == bound.binding.ownerUnitId;
    }
    if (cast->propagation == CastPropagationPolicy::BorrowedUltimateRules)
    {
        return true;
    }
    return cast->ultimate && cast->magicId == bound.binding.sourceId;
}

bool stateMachineAllowedByPropagation(const StateMachineAction& action,
                                      const EffectEventContext& context)
{
    const auto propagation = propagationPolicy(context);
    if (propagation != CastPropagationPolicy::BorrowedUltimateRules
        && propagation != CastPropagationPolicy::SuppressUltimateRules)
    {
        return true;
    }
    return !std::holds_alternative<BorrowEffectRulesAction>(action) &&
           !std::holds_alternative<CopyAttackDefinitionAction>(action);
}

bool actionContainsRecursiveRuleTransfer(const EffectAction& action)
{
    if (const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
            &action.value))
    {
        assert(*conditional);
        const auto contains = [](const std::vector<EffectAction>& actions)
        {
            return std::ranges::any_of(actions, actionContainsRecursiveRuleTransfer);
        };
        return contains((*conditional)->whenTrue)
            || contains((*conditional)->whenFalse);
    }
    const auto* machine = std::get_if<StateMachineAction>(&action.value);
    return machine
        && (std::holds_alternative<BorrowEffectRulesAction>(*machine)
            || std::holds_alternative<CopyAttackDefinitionAction>(*machine));
}

std::optional<BorrowedRuleActionCategory> borrowedActionCategory(
    const EffectAction& action)
{
    return std::visit(Overloaded{
        [](const ModifyAttributeAction&) { return std::optional{ BorrowedRuleActionCategory::AttributeModifier }; },
        [](const ModifyDamageAction&) { return std::optional{ BorrowedRuleActionCategory::DamageModifier }; },
        [](const ChangeResourceAction&) { return std::optional{ BorrowedRuleActionCategory::ResourceChange }; },
        [](const ModifyHealTransactionAction&) { return std::optional{ BorrowedRuleActionCategory::HealTransactionModifier }; },
        [](const ApplyStatusAction&) { return std::optional{ BorrowedRuleActionCategory::Status }; },
        [](const ConsumeStatusAction&) { return std::optional{ BorrowedRuleActionCategory::Status }; },
        [](const ConsumeThisStatusAction&)
            -> std::optional<BorrowedRuleActionCategory> { return std::nullopt; },
        [](const RemoveStatusAction&) { return std::optional{ BorrowedRuleActionCategory::Status }; },
        [](const SuppressCurrentCastContactsAction&)
            -> std::optional<BorrowedRuleActionCategory> { return std::nullopt; },
        [](const MakeIncomingAttackMissAction&)
            -> std::optional<BorrowedRuleActionCategory> { return std::nullopt; },
        [](const BlockPositiveDamageAction&)
            -> std::optional<BorrowedRuleActionCategory> { return std::nullopt; },
        [](const DealDamageAction&) { return std::optional{ BorrowedRuleActionCategory::Damage }; },
        [](const ModifyAttackAction&) { return std::optional{ BorrowedRuleActionCategory::Attack }; },
        [](const ForceMoveAction&) { return std::optional{ BorrowedRuleActionCategory::ForcedMovement }; },
        [](const CreateAreaAction&) { return std::optional{ BorrowedRuleActionCategory::Area }; },
        [](const ModifyCastAction&) { return std::optional{ BorrowedRuleActionCategory::Cast }; },
        [](const StateMachineAction& machine) -> std::optional<BorrowedRuleActionCategory>
        {
            return std::visit(Overloaded{
                [](const ChangeStateValueAction&) { return std::optional{ BorrowedRuleActionCategory::StateValue }; },
                [](const TransferStateValueAction&) { return std::optional{ BorrowedRuleActionCategory::DamageMemory }; },
                [](const RecordMaximumDamageAction&) { return std::optional{ BorrowedRuleActionCategory::DamageMemory }; },
                [](const ConsumeRecordedMaximumAction&) { return std::optional{ BorrowedRuleActionCategory::DamageMemory }; },
                [](const StartDamageAbsorptionAction&) { return std::optional{ BorrowedRuleActionCategory::DamageAbsorption }; },
                [](const SettleDamageAbsorptionAction&) { return std::optional{ BorrowedRuleActionCategory::DamageAbsorption }; },
                [](const SettleRemainingStatusDamageAction&) { return std::optional{ BorrowedRuleActionCategory::StatusDamageSettlement }; },
                [](const BorrowEffectRulesAction&) -> std::optional<BorrowedRuleActionCategory> { return std::nullopt; },
                [](const CopyAttackDefinitionAction&) -> std::optional<BorrowedRuleActionCategory> { return std::nullopt; },
                [](const GenerateClonesAction&) -> std::optional<BorrowedRuleActionCategory> { return std::nullopt; },
                [](const PreventDeathAction&) -> std::optional<BorrowedRuleActionCategory> { return std::nullopt; },
                [](const ConfigureRescueRepositionAction&) -> std::optional<BorrowedRuleActionCategory> { return std::nullopt; },
            }, machine);
        },
        [](const std::shared_ptr<ConditionalEffectAction>&)
            -> std::optional<BorrowedRuleActionCategory> { return std::nullopt; },
    }, action.value);
}

bool actionAllowedByBorrowFilter(
    const EffectAction& action,
    const BorrowedRuleFilter& filter)
{
    if (const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
            &action.value))
    {
        assert(*conditional);
        const auto allowed = [&](const std::vector<EffectAction>& actions)
        {
            return std::ranges::all_of(actions, [&](const auto& nested)
            {
                return actionAllowedByBorrowFilter(nested, filter);
            });
        };
        return allowed((*conditional)->whenTrue)
            && allowed((*conditional)->whenFalse);
    }
    const auto category = borrowedActionCategory(action);
    return category
        && std::ranges::contains(filter.allowedActionCategories, *category);
}

bool ruleAllowedByBorrowFilter(
    const EffectRule& rule,
    const BorrowedRuleFilter& filter)
{
    return std::ranges::all_of(rule.actions, [&](const auto& action)
    {
        return actionAllowedByBorrowFilter(action, filter);
    });
}

template<class Rules>
bool copiedMagicMatchesFilter(
    const Rules& rules,
    const EffectUnitSnapshot& unit,
    const CopiedMagicFilter& filter)
{
    for (const auto condition : filter.conditions)
    {
        switch (condition)
        {
        case CopiedMagicCondition::HasUltimateAttackDefinition:
            if (unit.ultimateMagicId < 0)
            {
                return false;
            }
            break;
        case CopiedMagicCondition::ExcludesRecursiveEffects:
            if (std::ranges::any_of(rules, [&](const auto& bound)
                {
                    return !bound.castScope
                        && bound.binding.kind == EffectSourceKind::Magic
                        && bound.binding.ownerUnitId == unit.id
                        && bound.binding.sourceId == unit.ultimateMagicId
                        && std::ranges::any_of(
                            bound.rule().actions,
                            actionContainsRecursiveRuleTransfer);
                }))
            {
                return false;
            }
            break;
        }
    }
    return true;
}

std::uint64_t stateScopeId(EffectStateSlot slot, const EffectEventContext& context)
{
    if (slot != EffectStateSlot::CastMaximumHpDamage)
    {
        return 0;
    }
    const auto* cast = effectCastProvenance(context);
    if (!cast)
    {
        throw std::logic_error("本次施放狀態槽需要 cast provenance");
    }
    return cast->castId.value();
}

DamageChannel currentDamageChannel(const EffectEventContext& context)
{
    const auto kind = damageKind(context);
    if (!kind)
    {
        return DamageChannel::All;
    }
    switch (*kind)
    {
    case BattleDamageKind::Physical:
    case BattleDamageKind::Skill:
        return isAttackDamageOrigin(context) ? DamageChannel::Skill : DamageChannel::Effect;
    case BattleDamageKind::Poison:
    case BattleDamageKind::Bleed:
        return DamageChannel::Dot;
    case BattleDamageKind::Pure:
    case BattleDamageKind::Effect:
    case BattleDamageKind::Execute:
        return DamageChannel::Effect;
    }
    return DamageChannel::All;
}

bool channelMatches(DamageChannel expected, DamageChannel actual)
{
    return expected == DamageChannel::All || expected == actual;
}

bool requiresExactRuntimePhaseQuery(const EffectRule& rule)
{
    return std::ranges::any_of(rule.actions, [](const EffectAction& action)
    {
        if (const auto* attack = std::get_if<ModifyAttackAction>(&action.value))
        {
            return !std::holds_alternative<std::monostate>(attack->runtimeBehavior)
                && !std::holds_alternative<ExpandingSpiralAttackBehavior>(
                    attack->runtimeBehavior);
        }
        if (const auto* movement = std::get_if<ForceMoveAction>(&action.value))
        {
            return movement->distancePixels > 0;
        }
        if (const auto* cast = std::get_if<ModifyCastAction>(&action.value))
        {
            return cast->rangeMode == CastRangeMode::Ranged
                || cast->projectileSpeedPct > 0
                || cast->minimumSelectDistance > 0
                || cast->additionalProjectiles > 0
                || cast->mobility != CastMobilityPolicy::Preserve;
        }
        return false;
    });
}

bool actionContainsExecute(const EffectAction& action)
{
    if (const auto* damage = std::get_if<DealDamageAction>(&action.value))
    {
        return damage->kind == BattleDamageKind::Execute;
    }
    if (const auto* modifier = std::get_if<ModifyDamageAction>(&action.value))
    {
        return modifier->operation == DamageModifierOperation::ExecuteBelowMaxHpPercent;
    }
    const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
        &action.value);
    if (!conditional)
    {
        return false;
    }
    assert(*conditional);
    const auto containsExecute = [](const std::vector<EffectAction>& actions)
    {
        return std::ranges::any_of(actions, actionContainsExecute);
    };
    return containsExecute((*conditional)->whenTrue)
        || containsExecute((*conditional)->whenFalse);
}

bool ruleContainsExecute(const EffectRule& rule)
{
    return std::ranges::any_of(rule.actions, actionContainsExecute);
}

std::uint64_t authoredActionLeafCount(const EffectAction& action)
{
    const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
        &action.value);
    if (!conditional) return 1;
    assert(*conditional);
    const auto branchCount = [](const std::vector<EffectAction>& actions)
    {
        return std::accumulate(
            actions.begin(), actions.end(), std::uint64_t{},
            [](std::uint64_t count, const EffectAction& nested)
            {
                return saturatingAdd(count, authoredActionLeafCount(nested));
            });
    };
    return saturatingAdd(
        branchCount((*conditional)->whenTrue),
        branchCount((*conditional)->whenFalse));
}

std::int64_t percentOf(std::int64_t value, int percent)
{
    return roundEffectRatio(
        saturatingMultiply(value, percent),
        100,
        EffectRounding::TowardZero);
}

std::int64_t effectStateValue(const std::pmr::map<EffectStateKey, std::int64_t>& values,
    const EffectSourceBinding& binding, EffectStateSlot slot, std::uint64_t scope)
{
    const auto found = values.find(stateKey(binding, slot, scope));
    return found == values.end() ? 0 : found->second;
}

void setEffectStateValue(std::pmr::map<EffectStateKey, std::int64_t>& values,
    const EffectSourceBinding& binding, EffectStateSlot slot, std::int64_t value,
    std::uint64_t scope)
{
    values[stateKey(binding, slot, scope)] = value;
}

struct EffectStateAccess
{
    std::pmr::map<EffectStateKey, std::int64_t>& values;
    std::int64_t stateValue(const EffectSourceBinding& binding, EffectStateSlot slot,
                            std::uint64_t scope = 0) const
    {
        return effectStateValue(values, binding, slot, scope);
    }
    void setStateValue(const EffectSourceBinding& binding, EffectStateSlot slot,
                       std::int64_t value, std::uint64_t scope = 0)
    {
        setEffectStateValue(values, binding, slot, value, scope);
    }
};

EffectExecutionInputs executionInputs(const EffectEventContext& context)
{
    EffectExecutionInputs inputs{
        .frame = context.header.executionFrame.value_or(context.header.frame),
    };
    if (const auto* cast = effectEventCastProvenance(context.payload)) inputs.cast = *cast;
    if (const auto* attack = effectEventAttackProvenance(context.payload)) inputs.attack = *attack;
    if (const auto* hit = std::get_if<HitEventData>(&context.payload); hit && hit->acceptedHit)
        inputs.hitDamageCredit = EffectHitDamageCredit{ hit->provenance, hit->targetUnitId };
    inputs.retainCastUntilDamageDescendants = context.header.retainCastUntilDamageDescendants
        && context.event != EffectEvent::CastSettled;
    return inputs;
}

int areaTeamDomain(const EffectEventContext& context)
{
    return std::visit(Overloaded{
        [&](const HitEventData& hit) { return context.header.battle.findUnit(hit.targetUnitId)->team; },
        [&](const AttackEventData& attack) { return context.header.battle.findUnit(attack.provenance.cast.sourceUnitId)->team; },
        [&](const HealRequestEventData& heal) { return heal.targetBefore.team; },
        [&](const HealResultEventData& heal) { return heal.request.targetBefore.team; },
        [&](const ShieldBreakEventData& broken) { return broken.targetAfter.team; },
        [&](const DeathEventData& death) { return death.deadAfter.team; },
        [&](const auto&) { return context.header.owner->team; },
    }, context.payload);
}

struct CommandEmitter
{
    EffectStateAccess state;
    std::span<const BoundEffectRule> rules;
    const BoundEffectRuleView& bound;
    const EffectEventContext& context;
    BattleRuntimeRandom& random;
    std::vector<EffectCommand>& commands;
    std::uint64_t& nextCommandOrdinal;

    void append(const EffectCommandMetadata& metadata, EffectCommandValue value)
    {
        commands.push_back({ metadata, std::move(value), executionInputs(context) });
    }

    int evaluate(const EffectNumber& number,
                 const EffectUnitSnapshot& target) const
    {
        const bool needsStoredState = number.base == EffectNumberBase::StoredStateValue
            || number.multiplierBase == EffectNumberBase::StoredStateValue;
        if (!needsStoredState)
        {
            return BattleEffectSystem::evaluateNumber(number, context, target);
        }
        assert(number.stateSlot);
        auto stateContext = context;
        stateContext.scope.formulaInputs.storedStateValue = state.stateValue(
            bound.binding,
            *number.stateSlot,
            stateScopeId(*number.stateSlot, context));
        return BattleEffectSystem::evaluateNumber(number, stateContext, target);
    }

    std::pair<std::int64_t, std::int64_t> applicationBaseRatio(
        const EffectNumber& number,
        EffectNumberBase base,
        const EffectUnitSnapshot& target) const
    {
        assert(statusNumberBindingPhase(base)
            == StatusNumberBindingPhase::ApplicationBound);
        switch (applicationBaseBindingKind(base))
        {
        case ApplicationBaseBindingKind::SourceStar:
            return { context.scope.owner->star, 1 };
        case ApplicationBaseBindingKind::SourceAttack:
            return { context.scope.owner->attack, 1 };
        case ApplicationBaseBindingKind::SourceMaxHp:
            return { context.scope.owner->maxHp, 1 };
        case ApplicationBaseBindingKind::SourceMissingHpRatio:
            assert(context.scope.owner->maxHp > 0);
            return {
                std::clamp(
                    context.scope.owner->maxHp - context.scope.owner->hp,
                    0,
                    context.scope.owner->maxHp),
                context.scope.owner->maxHp,
            };
        case ApplicationBaseBindingKind::SourceCurrentMpRatio:
            assert(context.scope.owner->maxMp > 0);
            return {
                std::clamp(context.scope.owner->mp, 0, context.scope.owner->maxMp),
                context.scope.owner->maxMp,
            };
        case ApplicationBaseBindingKind::SourceStatusQuantity:
        {
            assert(number.status);
            return {
                context.scope.owner->stackCount(
                    *number.status,
                    resolveStatusContributionFilter(
                        number.statusSource,
                        bound.binding,
                        context.scope.statusContribution)),
                1,
            };
        }
        case ApplicationBaseBindingKind::StoredStateValue:
            assert(number.stateSlot);
            return {
                state.stateValue(
                    bound.binding,
                    *number.stateSlot,
                    stateScopeId(*number.stateSlot, context)),
                1,
            };
        case ApplicationBaseBindingKind::ApplicationTargetMaxHp:
            return { target.maxHp, 1 };
        case ApplicationBaseBindingKind::None:
            break;
        }
        std::unreachable();
    }

    void bindStatusNumber(
        EffectNumber& number,
        const EffectUnitSnapshot& target) const
    {
        const bool bindBase = statusNumberBindingPhase(number.base)
            == StatusNumberBindingPhase::ApplicationBound;
        const bool bindMultiplier = number.multiplierBase
            && statusNumberBindingPhase(*number.multiplierBase)
                == StatusNumberBindingPhase::ApplicationBound;
        if (!bindBase && !bindMultiplier) return;

        if (bindBase && bindMultiplier)
        {
            const auto lhs = applicationBaseRatio(number, number.base, target);
            const auto rhs = applicationBaseRatio(number, *number.multiplierBase, target);
            number.base = EffectNumberBase::BoundRatio;
            number.multiplierBase.reset();
            number.boundNumerator = saturatingMultiply(lhs.first, rhs.first);
            number.boundDenominator = saturatingMultiply(lhs.second, rhs.second);
        }
        else
        {
            const auto base = bindBase ? number.base : *number.multiplierBase;
            const auto captured = applicationBaseRatio(number, base, target);
            if (bindBase)
                number.base = EffectNumberBase::BoundRatio;
            else
                number.multiplierBase = EffectNumberBase::BoundRatio;
            number.boundNumerator = captured.first;
            number.boundDenominator = captured.second;
        }

    }

    std::shared_ptr<const StatusBehaviorDefinition> bindStatusBehavior(
        const std::shared_ptr<const StatusBehaviorDefinition>& source,
        const EffectUnitSnapshot& target) const
    {
        if (!source) return {};
        auto result = std::make_shared<StatusBehaviorDefinition>(*source);
        for (auto& rule : result->rules)
        {
            forEachEffectNumber(rule, [&](EffectNumber& number)
            {
                bindStatusNumber(number, target);
            });
        }
        return result;
    }

    void emit(const EffectAction& effectAction,
              const EffectUnitSnapshot& target,
              std::uint32_t actionOrder,
              std::uint32_t authoredActionOrder,
              std::uint32_t targetOrder)
    {
        if (const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
                &effectAction.value))
        {
            if (!*conditional)
            {
                throw std::logic_error("條件分支 action 不可為空");
            }
            const auto& branch = conditionsSatisfied(
                (*conditional)->conditions, context, target, true)
                ? (*conditional)->whenTrue
                : (*conditional)->whenFalse;
            std::uint64_t nestedOffset = &branch == &(*conditional)->whenTrue
                ? 0
                : std::accumulate(
                    (*conditional)->whenTrue.begin(),
                    (*conditional)->whenTrue.end(),
                    std::uint64_t{},
                    [](std::uint64_t count, const EffectAction& nested)
                    {
                        return saturatingAdd(
                            count,
                            authoredActionLeafCount(nested));
                    });
            for (const auto& nested : branch)
            {
                assert(nestedOffset <= std::numeric_limits<std::uint32_t>::max());
                assert(static_cast<std::uint64_t>(actionOrder) + nestedOffset
                    <= std::numeric_limits<std::uint32_t>::max());
                assert(static_cast<std::uint64_t>(authoredActionOrder) + nestedOffset
                    <= std::numeric_limits<std::uint32_t>::max());
                emit(
                    nested,
                    target,
                    actionOrder + static_cast<std::uint32_t>(nestedOffset),
                    authoredActionOrder + static_cast<std::uint32_t>(nestedOffset),
                    targetOrder);
                nestedOffset = saturatingAdd(
                    nestedOffset,
                    authoredActionLeafCount(nested));
            }
            return;
        }

        auto metadata = EffectCommandMetadata{
            .binding = bound.binding,
            .ruleId = bound.rule().id,
            .event = context.event,
            .ruleOrder = bound.order,
            .authoredActionOrder = authoredActionOrder,
            .actionOrder = actionOrder,
            .targetOrder = targetOrder,
            .commandOrdinal = nextCommandOrdinal++,
            .targetUnitId = target.id,
            .eventSourceUnitId = sourceUnitId(context).value_or(-1),
        };
        if (context.scope.statusContribution)
        {
            metadata.ruleId = context.scope.statusContribution->producerRuleId;
            metadata.ruleOrder = context.scope.statusContribution->producerRuleOrder;
            metadata.executionLane = EffectExecutionLane::StatusBehavior;
            metadata.producerActionOrder =
                context.scope.statusContribution->producerActionOrder;
            metadata.behaviorRuleOrder =
                context.scope.statusContribution->behaviorRuleOrder;
            metadata.statusContribution = context.scope.statusContribution;
        }

        const auto prepareApplication = [&](const ApplyStatusAction& application)
        {
            auto behavior = bindStatusBehavior(application.behavior, target);
            const int duration = application.duration
                ? evaluate(*application.duration, target) : application.durationFrames;
            return prepareStatusApplication(application, duration, std::move(behavior));
        };
        const auto prepareDepletion = [&](const std::optional<ApplyStatusAction>& application)
            -> std::optional<ApplyStatusEffectCommand>
        {
            if (!application) return std::nullopt;
            return prepareApplication(*application);
        };
        std::visit(Overloaded{
            [&](const ModifyAttributeAction& action)
            {
                append(metadata, prepareModifyAttribute(action,
                    evaluate(action.amount, target)));
            },
            [&](const ModifyDamageAction& action)
            {
                append(metadata, prepareModifyDamage(action,
                    evaluate(action.amount, target)));
            },
            [&](const ChangeResourceAction& action)
            {
                auto destinations = action.transferDestination
                    ? BattleEffectSystem::selectTargets(*action.transferDestination, context, random)
                    : std::vector<int>{};
                ResourceEffectAmount amount = context.event == EffectEvent::BattleInitialized
                    ? ResourceEffectAmount{ InitializationResourceAmount{ action.amount, action.additionalAmount } }
                    : ResourceEffectAmount{ sumResourceAmounts(evaluate(action.amount, target),
                        action.additionalAmount ? evaluate(*action.additionalAmount, target) : 0) };
                append(metadata, prepareChangeResource(action,
                    std::move(amount), std::move(destinations)));
            },
            [&](const ModifyHealTransactionAction& action)
            {
                append(metadata, ModifyHealTransactionEffectCommand{ action });
            },
            [&](const ApplyStatusAction& action)
            {
                append(metadata, prepareApplication(action));
            },
            [&](const ConsumeStatusAction& action)
            {
                append(metadata, ConsumeStatusEffectCommand{
                    .request = {
                        .kind = action.status,
                        .stacks = action.quantity,
                        .filter = resolveStatusContributionFilter(
                            action.source, metadata.binding, metadata.statusContribution),
                    },
                    .whenDepleted = prepareDepletion(action.whenDepleted),
                });
            },
            [&](const ConsumeThisStatusAction& action)
            {
                assert(context.scope.statusContribution);
                append(metadata, ConsumeThisStatusEffectCommand{
                    .request = {
                        .kind = context.scope.statusContribution->kind,
                        .stacks = action.quantity,
                        .filter = resolveStatusContributionFilter(
                            StatusSourceMatch::CurrentContribution,
                            metadata.binding, metadata.statusContribution),
                    },
                    .whenDepleted = prepareDepletion(action.whenDepleted),
                });
            },
            [&](const RemoveStatusAction& action)
            {
                append(metadata, prepareStatusRemoval(action, metadata));
            },
            [&](const SuppressCurrentCastContactsAction& action)
            {
                assert(context.scope.statusContribution);
                append(metadata, SuppressCurrentCastContactsEffectCommand{
                    .originalTargetShield = action.originalTargetShield
                        ? evaluate(*action.originalTargetShield, target)
                        : 0,
                });
            },
            [&](const MakeIncomingAttackMissAction&)
            {
                assert(context.scope.statusContribution);
                append(metadata, MakeIncomingAttackMissEffectCommand{});
            },
            [&](const BlockPositiveDamageAction&)
            {
                throw std::logic_error("抵擋非處決正傷害只能在已綁定的狀態行為中執行");
            },
            [&](const DealDamageAction& action)
            {
                append(metadata, prepareDealDamage(action,
                    evaluate(action.amount, target),
                    action.transactionCount
                        ? evaluate(*action.transactionCount, target)
                        : 1));
            },
            [&](const ModifyAttackAction& action)
            {
                std::optional<ResolvedEffectAttackSource> source;
                if (action.source)
                {
                    const auto sourceUnitIds = BattleEffectSystem::selectTargets(
                        *action.source,
                        context,
                        random);
                    if (sourceUnitIds.empty())
                    {
                        return;
                    }
                    assert(sourceUnitIds.size() == 1);
                    const auto* selected = snapshotForTarget(context, sourceUnitIds.front());
                    assert(selected);
                    source = ResolvedEffectAttackSource{
                        .unitId = selected->id,
                        .position = selected->position,
                        .attack = selected->attack,
                    };
                }
                append(metadata, prepareModifyAttack(action,
                    action.damageOverride
                        ? std::optional<int>{ evaluate(*action.damageOverride, target) }
                        : std::nullopt,
                    source));
            },
            [&](const ForceMoveAction& action)
            {
                append(metadata, ForceMoveEffectCommand{ action });
            },
            [&](const CreateAreaAction& action)
            {
                BattleAreaCreateRequest request{
                    .source = metadata.binding,
                    .ruleId = metadata.ruleId,
                    .targetTeamDomain = areaTeamDomain(context),
                    .geometry = { action.shape, action.radiusTiles, action.squareSideTiles },
                    .currentFrame = context.header.executionFrame.value_or(context.header.frame),
                    .durationFrames = action.durationFrames,
                    .sourceDeath = action.sourceDeath,
                    .merge = action.merge,
                    .modifiers = action.modifiers,
                };
                if (action.anchor == AreaAnchor::FollowSourceUnit)
                    request.anchor = { BattleAreaAnchorKind::FollowSourceUnit, {}, metadata.binding.ownerUnitId };
                else
                {
                    const auto* hit = std::get_if<HitEventData>(&context.payload);
                    assert(hit);
                    request.anchor = { BattleAreaAnchorKind::FixedWorldPosition, hit->contactPosition, -1 };
                }
                for (auto& modifier : request.modifiers)
                {
                    const int amount = evaluate(modifier.amount, target);
                    if (modifier.kind == AreaModifierKind::Attribute
                        || modifier.kind == AreaModifierKind::PeriodicDamage)
                    {
                        modifier.amount = {};
                        modifier.amount.flat = amount;
                    }
                }
                append(metadata, CreateAreaEffectCommand{ std::move(request) });
            },
            [&](const ModifyCastAction& action)
            {
                append(metadata, prepareModifyCast(action,
                    action.mpCost
                        ? std::optional<int>{ evaluate(*action.mpCost, target) }
                        : std::nullopt));
            },
            [&](const StateMachineAction& action)
            {
                if ((bound.castScope
                        && (std::holds_alternative<BorrowEffectRulesAction>(action)
                            || std::holds_alternative<CopyAttackDefinitionAction>(action)))
                    || !stateMachineAllowedByPropagation(action, context))
                {
                    return;
                }

                std::visit(Overloaded{
                    [&](const ChangeStateValueAction& value)
                    {
                        const auto scope = stateScopeId(value.slot, context);
                        auto after = saturatingAdd(state.stateValue(bound.binding, value.slot, scope), value.delta);
                        if (value.minimum) after = std::max(after, *value.minimum);
                        if (value.maximum) after = std::min(after, *value.maximum);
                        state.setStateValue(bound.binding, value.slot, after, scope);
                        append(metadata, StateMachineEffectCommand{ EvaluatedStateEffectCommand{} });
                    },
                    [&](const TransferStateValueAction& value)
                    {
                        const auto sourceScope = stateScopeId(value.sourceSlot, context);
                        const auto destinationScope = stateScopeId(value.destinationSlot, context);
                        const auto amount = state.stateValue(bound.binding, value.sourceSlot, sourceScope);
                        state.setStateValue(bound.binding, value.sourceSlot, 0, sourceScope);
                        state.setStateValue(bound.binding, value.destinationSlot, amount, destinationScope);
                        append(metadata, StateMachineEffectCommand{ EvaluatedStateEffectCommand{} });
                    },
                    [&](const RecordMaximumDamageAction& value)
                    {
                        const auto scope = stateScopeId(value.slot, context);
                        const auto observed = channelMatches(value.channel, currentDamageChannel(context))
                            ? finalHpDamage(context).value_or(0) : 0;
                        const auto after = std::max(state.stateValue(bound.binding, value.slot, scope),
                            static_cast<std::int64_t>(observed));
                        state.setStateValue(bound.binding, value.slot, after, scope);
                        append(metadata, StateMachineEffectCommand{ EvaluatedStateEffectCommand{} });
                    },
                    [&](const ConsumeRecordedMaximumAction& value)
                    {
                        const auto scope = stateScopeId(value.slot, context);
                        const auto before = state.stateValue(bound.binding, value.slot, scope);
                        const auto amount = percentOf(before, value.percent);
                        state.setStateValue(bound.binding, value.slot, value.clearAfterConsume ? 0 : before, scope);
                        if (amount <= 0)
                        {
                            append(metadata, StateMachineEffectCommand{ EvaluatedStateEffectCommand{} });
                            return;
                        }
                        assert(amount <= std::numeric_limits<int>::max());
                        if (value.destination == StateValueDestination::ShieldAmount)
                        {
                            ChangeResourceEffectCommand grant;
                            grant.resource = BattleResource::Shield;
                            grant.kind = ResourceChangeKind::Grant;
                            grant.amount = static_cast<int>(amount);
                            append(metadata, std::move(grant));
                        }
                        else
                        {
                            append(metadata, StateMachineEffectCommand{ StateDamageEffectCommand{
                                static_cast<int>(amount), BattleDamageKind::Pure, { target.id } } });
                        }
                    },
                    [&](const StartDamageAbsorptionAction& value)
                    {
                        const auto scope = stateScopeId(value.slot, context);
                        state.setStateValue(bound.binding, value.slot, 0, scope);
                        append(metadata, StateMachineEffectCommand{ value });
                    },
                    [&](const SettleDamageAbsorptionAction& value)
                    {
                        const auto scope = stateScopeId(value.slot, context);
                        const auto before = state.stateValue(bound.binding, value.slot, scope);
                        const auto amount = percentOf(before, value.returnedPct);
                        state.setStateValue(bound.binding, value.slot, value.clearAfterSettle ? 0 : before, scope);
                        auto targets = BattleEffectSystem::selectTargets(value.target, context, random);
                        if (amount <= 0)
                        {
                            append(metadata, StateMachineEffectCommand{ EvaluatedStateEffectCommand{} });
                            return;
                        }
                        assert(amount <= std::numeric_limits<int>::max());
                        append(metadata, StateMachineEffectCommand{ StateDamageEffectCommand{
                            static_cast<int>(amount), value.damageKind, std::move(targets) } });
                    },
                    [&](const BorrowEffectRulesAction& value)
                    {
                        auto sources = BattleEffectSystem::selectTargets(value.sourceUnits, context, random);
                        const auto desired = std::max(0, evaluate(value.sourceCount, target));
                        if (static_cast<int>(sources.size()) > desired)
                            sources.resize(static_cast<std::size_t>(desired));
                        append(metadata, StateMachineEffectCommand{ BorrowEffectRulesCommand{
                            std::move(sources), value.filter, value.propagation } });
                    },
                    [&](const CopyAttackDefinitionAction& value)
                    {
                        auto sources = BattleEffectSystem::selectTargets(value.sourceUnits, context, random,
                            [&](const EffectUnitSnapshot& candidate)
                            {
                                if (context.scope.statusContribution)
                                    return copiedMagicMatchesFilter(std::array{ bound }, candidate, value.filter);
                                return copiedMagicMatchesFilter(rules, candidate, value.filter);
                            });
                        if (static_cast<int>(sources.size()) > value.copyCount)
                            sources.resize(static_cast<std::size_t>(value.copyCount));
                        append(metadata, StateMachineEffectCommand{ CopyAttackDefinitionCommand{
                            std::move(sources), value.propagation } });
                    },
                    [&](const SettleRemainingStatusDamageAction& value)
                    {
                        append(metadata, StateMachineEffectCommand{ value });
                    },
                    [&](const GenerateClonesAction& value)
                    {
                        append(metadata, StateMachineEffectCommand{ value });
                    },
                    [&](const PreventDeathAction& value)
                    {
                        append(metadata, StateMachineEffectCommand{ value });
                    },
                    [&](const ConfigureRescueRepositionAction& value)
                    {
                        append(metadata, StateMachineEffectCommand{ value });
                    },
                }, action);
            },
            [&](const std::shared_ptr<ConditionalEffectAction>& conditional)
            {
                (void)conditional;
                assert(false);
            },
        }, effectAction.value);
    }
};

}  // namespace

bool effectHealKindMatchesLabels(
    BattleHealKind kind,
    std::span<const std::string> labels)
{
    return std::ranges::any_of(labels, [&](const auto& label)
    {
        return matchesHealKindLabel(kind, label);
    });
}

StatusContributionFilter resolveStatusContributionFilter(
    StatusSourceMatch match,
    const EffectSourceBinding& binding,
    const std::optional<EffectStatusContributionContext>& contribution)
{
    switch (match)
    {
    case StatusSourceMatch::Any:
        return {};
    case StatusSourceMatch::EffectOwner:
        return { .sourceUnitId = binding.ownerUnitId };
    case StatusSourceMatch::EffectBinding:
        return { .producerBinding = binding };
    case StatusSourceMatch::CurrentContribution:
        assert(contribution);
        return {
            .holderUnitId = contribution->holderUnitId,
            .appliedSequence = contribution->appliedSequence,
        };
    }
    std::unreachable();
}

bool EffectUnitSnapshot::hasState(BattleStatusKind state) const
{
    return stackCount(state) > 0;
}

bool EffectUnitSnapshot::hasStateFromSource(BattleStatusKind state,
                                            int sourceUnitId) const
{
    return std::ranges::any_of(statusDetails, [&](const auto& status)
    {
        return status.state == state
            && status.sourceUnitId == sourceUnitId
            && status.stacks > 0;
    });
}

int EffectUnitSnapshot::stackCount(
    BattleStatusKind stack,
    const StatusContributionFilter& filter) const
{
    std::int64_t total{};
    for (const auto& status : statusDetails)
    {
        if (status.state != stack
            || (filter.holderUnitId && id != *filter.holderUnitId)
            || (filter.sourceUnitId
                && status.sourceUnitId != *filter.sourceUnitId)
            || (filter.producerBinding
                && status.producerBinding != filter.producerBinding)
            || (filter.appliedSequence
                && status.appliedSequence != *filter.appliedSequence)) continue;
        total = std::min<std::int64_t>(
            std::numeric_limits<int>::max(),
            total + status.stacks);
    }
    return static_cast<int>(total);
}

bool EffectUnitSnapshot::usesMagic(int magicId) const
{
    return std::ranges::contains(magicIds, magicId);
}

BattleEffectReadView::BattleEffectReadView(std::span<const EffectUnitSnapshot> units,
                                           float tileWidth)
    : units_(units)
    , tileWidth_(tileWidth)
{
    assert(tileWidth > 0.0f);
}

std::span<const EffectUnitSnapshot> BattleEffectReadView::units() const
{
    return units_;
}

const EffectUnitSnapshot* BattleEffectReadView::findUnit(int unitId) const
{
    const auto it = std::find_if(units_.begin(), units_.end(), [unitId](const auto& unit)
    {
        return unit.id == unitId;
    });
    return it != units_.end() ? &*it : nullptr;
}

float BattleEffectReadView::tileWidth() const
{
    return tileWidth_;
}

void BattleEffectRuleStore::clear()
{
    rules_.clear();
    for (auto& indices : ruleIndicesByEvent_)
    {
        indices.clear();
    }
    ruleIndexByKey_.clear();
    stateValues_.clear();
    blinkAttackWeakestTargetByOwner_.clear();
    nextRuntimeInstanceId_ = 1;
    nextRuleOrder_ = 0;
}

std::uint32_t BattleEffectRuleStore::allocateRuleOrder()
{
    assert(nextRuleOrder_ < std::numeric_limits<std::uint32_t>::max());
    return nextRuleOrder_++;
}

std::size_t BattleEffectRuleStore::append(
    EffectSourceBinding binding,
    const EffectRule& rule,
    EffectRuleAuthoringContext context)
{
    std::string validationError;
    if (!validateEffectRule(rule, validationError, context))
    {
        throw std::invalid_argument(validationError);
    }
    if (rule.castMatch == EffectCastMatch::OwnerAnyCast
        && binding.kind != EffectSourceKind::Magic)
    {
        throw std::invalid_argument("效果擁有者任意施放匹配只支援武功效果來源");
    }
    if (rule.observation == EffectObservationScope::ComboMemberEventSource
        && binding.kind != EffectSourceKind::Combo)
    {
        throw std::invalid_argument("同門出招觀察只支援羈絆效果來源");
    }
    const auto key = runtimeKey(binding, rule.id);
    if (ruleIndexByKey_.contains(key))
    {
        throw std::invalid_argument("同一效果來源不可有重複的 EffectRuleId");
    }

    const auto index = rules_.size();
    rules_.push_back({
        .binding = binding,
        .definition = std::make_shared<const EffectRule>(rule),
        .order = allocateRuleOrder(),
        .runtime = { .intervalFramesRemaining = rule.intervalFrames },
    });
    ruleIndicesByEvent_[static_cast<std::size_t>(rule.event)].push_back(index);
    ruleIndexByKey_.emplace(key, index);
    return index;
}

void BattleEffectRuleStore::append(EffectSourceBinding binding,
                                   std::span<const EffectRule> rules)
{
    for (const auto& rule : rules)
    {
        append(binding, rule);
    }
}

void BattleEffectRuleStore::appendClonedOwnerRules(
    int sourceOwnerUnitId,
    int cloneOwnerUnitId,
    int cloneTeam)
{
    assert(sourceOwnerUnitId >= 0);
    assert(cloneOwnerUnitId >= 0);
    std::vector<BoundEffectRule> inherited;
    for (const auto& bound : rules_)
    {
        if (bound.binding.ownerUnitId != sourceOwnerUnitId
            || bound.rule().event == EffectEvent::BattleInitialized
            || bound.castScope)
        {
            continue;
        }
        inherited.push_back(bound);
    }
    for (const auto& bound : inherited)
    {
        auto binding = bound.binding;
        binding.ownerUnitId = cloneOwnerUnitId;
        binding.sourceTeam = cloneTeam;
        binding.runtimeInstanceId = 0;
        const auto index = append(binding, bound.rule());
        if (bound.rule().observation == EffectObservationScope::ComboMemberEventSource)
        {
            rules_[index].runtime.eligibleEventCount
                = runtime(bound.binding, bound.rule().id).eligibleEventCount;
        }
    }
}

void BattleEffectRuleStore::appendAntiComboTransferredRules(
    int sourceOwnerUnitId,
    int targetOwnerUnitId,
    int targetTeam,
    int comboId)
{
    assert(sourceOwnerUnitId >= 0);
    assert(targetOwnerUnitId >= 0);
    std::vector<BoundEffectRule> transferred;
    for (const auto& bound : rules_)
    {
        if (bound.binding.ownerUnitId != sourceOwnerUnitId
            || bound.binding.kind != EffectSourceKind::Combo
            || bound.binding.sourceId != comboId
            || bound.rule().event == EffectEvent::BattleInitialized
            || bound.castScope)
        {
            continue;
        }
        transferred.push_back(bound);
    }
    for (const auto& bound : transferred)
    {
        auto binding = bound.binding;
        binding.ownerUnitId = targetOwnerUnitId;
        binding.sourceTeam = targetTeam;
        binding.runtimeInstanceId = 0;
        append(binding, bound.rule());
    }
}

std::span<const BoundEffectRule> BattleEffectRuleStore::rules() const
{
    return rules_;
}

std::vector<std::size_t> BattleEffectRuleStore::bindBorrowedUltimateRules(
    BattleCastId castId,
    int ownerUnitId,
    int ownerTeam,
    std::span<const int> sourceUnitIds,
    const BorrowedRuleFilter& filter,
    CastPropagationPolicy propagation)
{
    assert(castId.valid());
    assert(ownerUnitId >= 0);
    assert(propagation == CastPropagationPolicy::BorrowedUltimateRules);

    struct SourceRuleGroup
    {
        std::vector<BoundEffectRule> rules;
    };
    std::vector<SourceRuleGroup> groups;
    groups.reserve(sourceUnitIds.size());
    for (const int sourceUnitId : sourceUnitIds)
    {
        SourceRuleGroup group;
        for (const auto& bound : rules_)
        {
            if (bound.castScope
                || bound.binding.kind != EffectSourceKind::Magic
                || bound.binding.ownerUnitId != sourceUnitId
                || !ruleAllowedByBorrowFilter(bound.rule(), filter))
            {
                continue;
            }
            group.rules.push_back(bound);
        }
        if (!group.rules.empty())
        {
            groups.push_back(std::move(group));
        }
    }

    std::vector<std::size_t> addedIndices;
    for (const auto& group : groups)
    {
        const std::uint64_t instanceId = nextRuntimeInstanceId_++;
        for (const auto& source : group.rules)
        {
            auto binding = source.binding;
            binding.ownerUnitId = ownerUnitId;
            binding.sourceTeam = ownerTeam;
            binding.runtimeInstanceId = instanceId;

            const std::size_t index = rules_.size();
            rules_.push_back({
                .binding = binding,
                .definition = source.definition,
                .order = allocateRuleOrder(),
                .castScope = castId,
                .scopedPropagation = propagation,
                .runtime = { .intervalFramesRemaining = source.rule().intervalFrames },
            });
            ruleIndicesByEvent_[static_cast<std::size_t>(source.rule().event)].push_back(index);
            ruleIndexByKey_.emplace(runtimeKey(binding, source.rule().id), index);
            addedIndices.push_back(index);
        }
    }
    return addedIndices;
}

void BattleEffectRuleStore::removeCastScopedRules(BattleCastId castId)
{
    assert(castId.valid());
    std::set<std::uint64_t> removedInstances;
    for (const auto& bound : rules_)
    {
        if (bound.castScope == castId)
        {
            assert(bound.binding.runtimeInstanceId != 0);
            removedInstances.insert(bound.binding.runtimeInstanceId);
        }
    }
    for (auto& bound : rules_)
    {
        if (removedInstances.contains(bound.binding.runtimeInstanceId)) continue;
        std::erase_if(bound.runtime.activationEvaluations, [&](const auto& entry)
        {
            return entry.castId == castId.value();
        });
    }
    std::erase_if(stateValues_, [&](const auto& entry)
    {
        return entry.first.scopeId == castId.value()
            || removedInstances.contains(entry.first.sourceInstanceId);
    });
    if (removedInstances.empty())
    {
        return;
    }

    std::erase_if(rules_, [&](const BoundEffectRule& bound)
    {
        return bound.castScope == castId;
    });
    rebuildEventIndices();
}

std::size_t BattleEffectRuleStore::castScopedRuleCount(BattleCastId castId) const
{
    return static_cast<std::size_t>(std::ranges::count_if(rules_, [&](const auto& bound)
    {
        return bound.castScope == castId;
    }));
}

void BattleEffectRuleStore::rebuildEventIndices()
{
    ruleIndexByKey_.clear();
    for (auto& indices : ruleIndicesByEvent_)
    {
        indices.clear();
    }
    for (std::size_t index = 0; index < rules_.size(); ++index)
    {
        const auto& bound = rules_[index];
        ruleIndicesByEvent_[static_cast<std::size_t>(bound.rule().event)].push_back(index);
        ruleIndexByKey_.emplace(runtimeKey(bound.binding, bound.rule().id), index);
    }
}

const EffectRuleRuntimeState& BattleEffectRuleStore::runtime(
    const EffectSourceBinding& binding,
    EffectRuleId ruleId) const
{
    return rules_[ruleIndexByKey_.at(runtimeKey(binding, ruleId))].runtime;
}

int BattleEffectRuleStore::activationCount(const EffectSourceBinding& binding,
                                           EffectRuleId ruleId) const
{
    const auto it = ruleIndexByKey_.find(runtimeKey(binding, ruleId));
    return it != ruleIndexByKey_.end() ? rules_[it->second].runtime.activationCount : 0;
}

int BattleEffectRuleStore::activationEvaluationCount(const EffectSourceBinding& binding,
                                                      EffectRuleId ruleId,
                                                      BattleCastId castId,
                                                      int targetUnitId) const
{
    const auto index = ruleIndexByKey_.find(runtimeKey(binding, ruleId));
    if (index == ruleIndexByKey_.end()) return 0;
    const auto& runtime = rules_[index->second].runtime;
    const auto found = std::ranges::find_if(
        runtime.activationEvaluations,
        [&](const auto& entry)
        {
            return entry.castId == castId.value()
                && entry.targetUnitId == targetUnitId;
        });
    return found != runtime.activationEvaluations.end()
        ? found->count
        : 0;
}

bool BattleEffectRuleStore::canActivateRuntimeRule(
    const EffectSourceBinding& binding,
    EffectRuleId ruleId,
    int frame) const
{
    assert(frame >= 0);
    const auto index = ruleIndexByKey_.find(runtimeKey(binding, ruleId));
    if (index == ruleIndexByKey_.end())
    {
        return false;
    }
    const auto& bound = rules_[index->second];
    return ruleActivationAvailable(bound.rule(), bound.runtime, frame);
}

bool BattleEffectRuleStore::tryActivateRuntimeRule(
    const EffectSourceBinding& binding,
    EffectRuleId ruleId,
    int frame,
    BattleRuntimeRandom& random)
{
    if (!canActivateRuntimeRule(binding, ruleId, frame))
    {
        return false;
    }
    const auto& bound = rules_[ruleIndexByKey_.at(runtimeKey(binding, ruleId))];
    if (!random.chance(bound.rule().chancePct))
    {
        return false;
    }
    recordRuntimeRuleActivation(binding, ruleId, frame);
    return true;
}

void BattleEffectRuleStore::recordRuntimeRuleActivation(
    const EffectSourceBinding& binding,
    EffectRuleId ruleId,
    int frame)
{
    assert(canActivateRuntimeRule(binding, ruleId, frame));
    auto& bound = rules_[ruleIndexByKey_.at(runtimeKey(binding, ruleId))];
    auto& runtime = bound.runtime;
    ++runtime.activationCount;
    if (bound.rule().sharedCooldownFrames > 0)
    {
        runtime.sharedCooldownUntilFrame =
            static_cast<std::int64_t>(frame) + bound.rule().sharedCooldownFrames;
    }
}

bool BattleEffectRuleStore::blinkAttackUsesWeakestTarget(int ownerUnitId) const
{
    assert(ownerUnitId >= 0);
    const auto it = blinkAttackWeakestTargetByOwner_.find(ownerUnitId);
    return it != blinkAttackWeakestTargetByOwner_.end() && it->second;
}

void BattleEffectRuleStore::advanceBlinkAttackTargetMode(int ownerUnitId)
{
    assert(ownerUnitId >= 0);
    auto& useWeakest = blinkAttackWeakestTargetByOwner_[ownerUnitId];
    useWeakest = !useWeakest;
}

std::int64_t BattleEffectRuleStore::stateValue(const EffectSourceBinding& binding,
                                               EffectStateSlot slot,
                                               std::uint64_t scopeId) const
{
    return effectStateValue(stateValues_, binding, slot, scopeId);
}

void BattleEffectRuleStore::setStateValue(const EffectSourceBinding& binding,
                                          EffectStateSlot slot,
                                          std::int64_t value,
                                          std::uint64_t scopeId)
{
    setEffectStateValue(stateValues_, binding, slot, value, scopeId);
}

bool BattleEffectSystem::eventPayloadMatches(const EffectEventContext& context)
{
    switch (context.event)
    {
    case EffectEvent::BattleInitialized:
        return std::holds_alternative<InitializationEventData>(context.payload);
    case EffectEvent::FrameAdvanced:
        return std::holds_alternative<FrameTickEventData>(context.payload);
    case EffectEvent::UltimateCooldownFinished:
        return std::holds_alternative<UltimateCooldownFinishedEventData>(context.payload);
    case EffectEvent::CastPlanned:
        return std::holds_alternative<CastPlanEventData>(context.payload);
    case EffectEvent::AttackCommitted:
        return std::holds_alternative<CastCommitEventData>(context.payload);
    case EffectEvent::UltimateCommitted:
    {
        const auto* data = std::get_if<CastCommitEventData>(&context.payload);
        return data && data->provenance.ultimate;
    }
    case EffectEvent::AttackSpawned:
        return std::holds_alternative<AttackEventData>(context.payload);
    case EffectEvent::MainProjectileBeforeDamage:
    {
        const auto* data = std::get_if<HitEventData>(&context.payload);
        return data && data->provenance.mainProjectile;
    }
    case EffectEvent::HitBeforeDamage:
        return std::holds_alternative<HitEventData>(context.payload);
    case EffectEvent::DamageResolved:
        return std::holds_alternative<DamageResultEventData>(context.payload);
    case EffectEvent::HealAttempted:
        return std::holds_alternative<HealRequestEventData>(context.payload);
    case EffectEvent::HealApplied:
    {
        const auto* data = std::get_if<HealResultEventData>(&context.payload);
        return data && data->appliedAmount > 0;
    }
    case EffectEvent::CastContinuation:
    case EffectEvent::CastSettled:
        return std::holds_alternative<CastAggregateEventData>(context.payload);
    case EffectEvent::ShieldBroken:
        return std::holds_alternative<ShieldBreakEventData>(context.payload);
    case EffectEvent::UnitDied:
    case EffectEvent::AllyDied:
        return std::holds_alternative<DeathEventData>(context.payload);
    }
    return false;
}

int BattleEffectSystem::evaluateNumber(const EffectNumber& number,
                                       const EffectEventContext& context,
                                       const EffectUnitSnapshot& target)
{
    auto baseValue = [&](EffectNumberBase base) -> std::int64_t
    {
        switch (effectNumberEvaluationKind(base))
        {
        case EffectNumberEvaluationKind::Constant: return 0;
        case EffectNumberEvaluationKind::SourceStar: return context.scope.owner->star;
        case EffectNumberEvaluationKind::SourceAttack: return context.scope.owner->attack;
        case EffectNumberEvaluationKind::SourceMaxHp: return context.scope.owner->maxHp;
        case EffectNumberEvaluationKind::SourceMissingHpRatio:
            assert(context.scope.owner->maxHp > 0);
            return std::clamp(
                context.scope.owner->maxHp - context.scope.owner->hp,
                0,
                context.scope.owner->maxHp);
        case EffectNumberEvaluationKind::SourceCurrentMpRatio:
            assert(context.scope.owner->maxMp > 0);
            return std::clamp(
                context.scope.owner->mp,
                0,
                context.scope.owner->maxMp);
        case EffectNumberEvaluationKind::TargetMaxHp: return target.maxHp;
        case EffectNumberEvaluationKind::TargetCurrentHp: return target.hp;
        case EffectNumberEvaluationKind::TargetCurrentShield: return target.shield;
        case EffectNumberEvaluationKind::TargetCurrentCooldown: return target.activeCooldown;
        case EffectNumberEvaluationKind::FinalHpDamage:
            if (const auto value = finalHpDamage(context))
            {
                return *value;
            }
            throw std::logic_error("實際生命傷害公式需要 DamageResolved payload");
        case EffectNumberEvaluationKind::SourceStatusQuantity:
        {
            assert(number.status);
            return context.scope.owner->stackCount(
                *number.status,
                resolveStatusContributionFilter(
                    number.statusSource,
                    context.scope.binding,
                    context.scope.statusContribution));
        }
        case EffectNumberEvaluationKind::CurrentContributionQuantity:
            assert(context.scope.statusContribution);
            assert(context.scope.statusContribution->quantity > 0);
            return context.scope.statusContribution->quantity;
        case EffectNumberEvaluationKind::StoredStateValue:
            if (context.scope.formulaInputs.storedStateValue)
            {
                return *context.scope.formulaInputs.storedStateValue;
            }
            throw std::logic_error("狀態槽公式缺少 typed input");
        case EffectNumberEvaluationKind::ApplicationTargetMaxHp:
            return target.maxHp;
        case EffectNumberEvaluationKind::BoundRatio:
            return number.boundNumerator;
        case EffectNumberEvaluationKind::Count:
            break;
        }
        throw std::logic_error("未知 EffectNumberBase");
    };

    const auto baseDenominator = [&](EffectNumberBase base) -> std::int64_t
    {
        const auto evaluation = effectNumberEvaluationKind(base);
        if (evaluation == EffectNumberEvaluationKind::SourceMissingHpRatio)
        {
            assert(context.scope.owner->maxHp > 0);
            return context.scope.owner->maxHp;
        }
        if (evaluation == EffectNumberEvaluationKind::SourceCurrentMpRatio)
        {
            assert(context.scope.owner->maxMp > 0);
            return context.scope.owner->maxMp;
        }
        if (evaluation == EffectNumberEvaluationKind::BoundRatio)
        {
            assert(number.boundDenominator > 0);
            return number.boundDenominator;
        }
        return 1;
    };

    std::int64_t value = baseValue(number.base);
    auto denominator = baseDenominator(number.base);
    if (number.multiplierBase)
    {
        value = saturatingMultiply(value, baseValue(*number.multiplierBase));
        denominator = saturatingMultiply(
            denominator,
            baseDenominator(*number.multiplierBase));
    }
    const auto percentageDenominator = saturatingMultiply(denominator, 100);
    if (number.statusScale == StatusNumberScale::PerContributionLayer)
    {
        assert(context.scope.statusContribution);
        assert(context.scope.statusContribution->quantity > 0);
        const auto percentageNumerator = saturatingMultiply(value, number.percent);
        const auto flatNumerator = saturatingMultiply(
            number.flat,
            percentageDenominator);
        value = roundEffectRatio(
            saturatingMultiply(
                saturatingAdd(percentageNumerator, flatNumerator),
                context.scope.statusContribution->quantity),
            percentageDenominator,
            number.rounding);
    }
    else
    {
        value = roundEffectRatio(
            saturatingMultiply(value, number.percent),
            percentageDenominator,
            number.rounding);
        value = saturatingAdd(value, number.flat);
    }
    if (number.minimum)
    {
        value = std::max(value, static_cast<std::int64_t>(*number.minimum));
    }
    if (number.maximum)
    {
        value = std::min(value, static_cast<std::int64_t>(*number.maximum));
    }
    return clampToInt(value);
}

namespace
{

std::pmr::vector<int> selectTargetsWithResource(const EffectSelector& selector,
    const EffectEventContext& context,
    BattleRuntimeRandom& random,
    const std::function<bool(const EffectUnitSnapshot&)>& candidateFilter,
    std::pmr::memory_resource* memoryResource)
{
    std::pmr::vector<const EffectUnitSnapshot*> candidates(memoryResource);
    SelectorOrdering ordering = SelectorOrdering::UnitId;
    const Pointf center = selector.kind == EffectSelectorKind::NearestEnemies ||
                                  selector.kind == EffectSelectorKind::FarthestEnemy
        ? context.scope.owner->position
        : selectorCenter(context);
    const EffectUnitSnapshot* requiredTarget = selector.requiredTarget
        ? requiredTargetSnapshot(*selector.requiredTarget, context)
        : nullptr;
    if (selector.requiredTarget
        && (!requiredTarget
            || !requiredTarget->alive
            || (candidateFilter && !candidateFilter(*requiredTarget))
            || !matchesSelectorRequirements(*requiredTarget, selector, context)
            || !matchesSelectorMembership(*requiredTarget, selector, context)))
    {
        return {};
    }

    auto addDirect = [&](const EffectUnitSnapshot* unit)
    {
        if (unit
            && (!candidateFilter || candidateFilter(*unit))
            && matchesSelectorRequirements(*unit, selector, context))
        {
            candidates.reserve(1);
            candidates.push_back(unit);
        }
    };

    switch (selector.kind)
    {
    case EffectSelectorKind::Self:
        addDirect(context.scope.owner);
        break;
    case EffectSelectorKind::StatusHolder:
        assert(context.scope.statusContribution);
        addDirect(context.header.battle.findUnit(
            context.scope.statusContribution->holderUnitId));
        break;
    case EffectSelectorKind::SourceUnit:
        addDirect(sourceSnapshot(context));
        break;
    case EffectSelectorKind::TransactionTarget:
    case EffectSelectorKind::HitTarget:
        addDirect(transactionTargetSnapshot(context));
        break;
    case EffectSelectorKind::OriginalAttackTarget:
        if (const auto unitId = originalTargetUnitId(context))
        {
            addDirect(snapshotForTarget(context, *unitId));
        }
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
        candidates.reserve(context.header.battle.units().size());
        for (const auto& unit : context.header.battle.units())
        {
            if (!unit.alive
                || (candidateFilter && !candidateFilter(unit))
                || !matchesSelectorRequirements(unit, selector, context)
                || !matchesSelectorMembership(unit, selector, context))
            {
                continue;
            }

            switch (selector.kind)
            {
            case EffectSelectorKind::UnitsInRadius:
            {
                const double radius = static_cast<double>(selector.radiusTiles) * context.header.battle.tileWidth();
                if (distanceSquared(unit.position, center) > radius * radius)
                {
                    continue;
                }
                break;
            }
            case EffectSelectorKind::UnitsInSquare:
            {
                const double halfSide = static_cast<double>(selector.squareSideTiles) *
                                        context.header.battle.tileWidth() / 2.0;
                if (std::abs(static_cast<double>(unit.position.x) - center.x) > halfSide
                    || std::abs(static_cast<double>(unit.position.y) - center.y) > halfSide)
                {
                    continue;
                }
                break;
            }
            default:
                break;
            }
            candidates.push_back(&unit);
        }
        break;
    }

    if (requiredTarget)
    {
        std::erase_if(candidates, [requiredTarget](const EffectUnitSnapshot* candidate)
        {
            return candidate->id == requiredTarget->id;
        });
    }

    switch (selector.kind)
    {
    case EffectSelectorKind::LowestHpAllies: ordering = SelectorOrdering::LowestHpRatio; break;
    case EffectSelectorKind::LowestMpAllies: ordering = SelectorOrdering::LowestMp; break;
    case EffectSelectorKind::HighestMpEnemy: ordering = SelectorOrdering::HighestMp; break;
    case EffectSelectorKind::StrongestEnemies: ordering = SelectorOrdering::Strongest; break;
    case EffectSelectorKind::NearestEnemies: ordering = SelectorOrdering::Nearest; break;
    case EffectSelectorKind::FarthestEnemy: ordering = SelectorOrdering::Farthest; break;
    default: ordering = SelectorOrdering::UnitId; break;
    }

    std::ranges::sort(candidates, [&](const auto* lhs, const auto* rhs)
    {
        bool lhsFirst = false;
        bool tied = false;
        switch (ordering)
        {
        case SelectorOrdering::UnitId:
            tied = true;
            break;
        case SelectorOrdering::LowestHpRatio:
            tied = ratioEqual(lhs->hp, lhs->maxHp, rhs->hp, rhs->maxHp);
            lhsFirst = ratioLess(lhs->hp, lhs->maxHp, rhs->hp, rhs->maxHp);
            break;
        case SelectorOrdering::LowestMp:
            tied = lhs->mp == rhs->mp;
            lhsFirst = lhs->mp < rhs->mp;
            break;
        case SelectorOrdering::HighestMp:
            tied = lhs->mp == rhs->mp;
            lhsFirst = lhs->mp > rhs->mp;
            break;
        case SelectorOrdering::Strongest:
        {
            const auto score = [](const EffectUnitSnapshot& unit)
            {
                std::int64_t result = unit.cost > 0 ? 1 : 0;
                for (int index = 0; index < unit.star; ++index)
                {
                    result = saturatingMultiply(result, unit.cost);
                }
                return result;
            };
            const auto lhsScore = score(*lhs);
            const auto rhsScore = score(*rhs);
            tied = lhsScore == rhsScore && lhs->maxHp == rhs->maxHp;
            lhsFirst = lhsScore != rhsScore
                ? lhsScore > rhsScore
                : lhs->maxHp > rhs->maxHp;
            break;
        }
        case SelectorOrdering::Nearest:
            tied = distanceSquared(lhs->position, center) == distanceSquared(rhs->position, center);
            lhsFirst = distanceSquared(lhs->position, center) < distanceSquared(rhs->position, center);
            break;
        case SelectorOrdering::Farthest:
            tied = distanceSquared(lhs->position, center) == distanceSquared(rhs->position, center);
            lhsFirst = distanceSquared(lhs->position, center) > distanceSquared(rhs->position, center);
            break;
        }
        return tied ? lhs->id < rhs->id : lhsFirst;
    });

    if (selector.tieBreak == EffectTieBreak::BattleRandom && candidates.size() > 1)
    {
        randomizeTieGroups(candidates, ordering, center, random);
    }
    const int targetCount = selector.kind == EffectSelectorKind::HighestMpEnemy ||
            selector.kind == EffectSelectorKind::FarthestEnemy
        ? 1
        : selector.count;
    const int remainingTargetCount = targetCount > 0
        ? std::max(0, targetCount - (requiredTarget ? 1 : 0))
        : 0;
    if (targetCount > 0 && candidates.size() > static_cast<std::size_t>(remainingTargetCount))
    {
        candidates.resize(static_cast<std::size_t>(remainingTargetCount));
    }

    std::pmr::vector<int> result(memoryResource);
    result.reserve(candidates.size() + (requiredTarget ? 1 : 0));
    if (requiredTarget)
    {
        result.push_back(requiredTarget->id);
    }
    for (const auto* unit : candidates)
    {
        result.push_back(unit->id);
    }
    return result;
}

}  // namespace

std::vector<int> BattleEffectSystem::selectTargets(const EffectSelector& selector,
    const EffectEventContext& context,
    BattleRuntimeRandom& random,
    const std::function<bool(const EffectUnitSnapshot&)>& candidateFilter)
{
    std::array<std::byte, 4096> buffer;
    std::pmr::monotonic_buffer_resource memory(buffer.data(), buffer.size());
    const auto targets = selectTargetsWithResource(selector, context, random, candidateFilter, &memory);
    // 公開結果可被命令持有；不可將查詢的短期 arena 帶出此邊界。
    return { targets.begin(), targets.end() };
}

namespace
{

EffectNumber& canonicalPoisonDamageNumber(StatusBehaviorDefinition& behavior)
{
    const auto capability = canonicalPoisonDamageCapability(behavior);
    assert(capability && capability->damage);
    const auto* canonicalNumber = &capability->damage->amount;
    for (auto& rule : behavior.rules)
    {
        for (auto& action : rule.actions)
        {
            auto* damage = std::get_if<DealDamageAction>(&action.value);
            if (damage && &damage->amount == canonicalNumber)
                return damage->amount;
        }
    }
    std::unreachable();
}

void normalizeNestedStatusBehaviorIdentity(EffectAction& action);

void normalizeStatusBehaviorIdentity(StatusBehaviorDefinition& behavior)
{
    for (std::size_t ruleIndex = 0; ruleIndex < behavior.rules.size(); ++ruleIndex)
    {
        auto& rule = behavior.rules[ruleIndex];
        rule.id = EffectRuleId{ ruleIndex + 1 };
        for (auto& action : rule.actions)
            normalizeNestedStatusBehaviorIdentity(action);
    }
}

void normalizeNestedApplicationBehavior(ApplyStatusAction& application)
{
    if (!application.behavior) return;
    auto normalized = std::make_shared<StatusBehaviorDefinition>(
        *application.behavior);
    normalizeStatusBehaviorIdentity(*normalized);
    application.behavior = std::move(normalized);
}

void normalizeNestedStatusBehaviorIdentity(EffectAction& action)
{
    std::visit(
        [](auto& typed)
        {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, ApplyStatusAction>)
            {
                normalizeNestedApplicationBehavior(typed);
            }
            else if constexpr (std::is_same_v<T, ConsumeStatusAction>
                || std::is_same_v<T, ConsumeThisStatusAction>)
            {
                if (typed.whenDepleted)
                    normalizeNestedApplicationBehavior(*typed.whenDepleted);
            }
            else if constexpr (std::is_same_v<T,
                std::shared_ptr<ConditionalEffectAction>>)
            {
                assert(typed);
                auto normalized = std::make_shared<ConditionalEffectAction>(*typed);
                for (auto& nested : normalized->whenTrue)
                    normalizeNestedStatusBehaviorIdentity(nested);
                for (auto& nested : normalized->whenFalse)
                    normalizeNestedStatusBehaviorIdentity(nested);
                typed = std::move(normalized);
            }
        },
        action.value);
}

std::shared_ptr<const StatusBehaviorDefinition> normalizedPoisonMergeBehavior(
    const StatusBehaviorDefinition& behavior)
{
    auto normalized = std::make_shared<StatusBehaviorDefinition>(behavior);
    normalizeStatusBehaviorIdentity(*normalized);
    canonicalPoisonDamageNumber(*normalized).percent = 0;
    return normalized;
}

bool poisonMergeBehaviorsEquivalent(
    const StatusBehaviorDefinition& lhs,
    const StatusBehaviorDefinition& rhs)
{
    return statusBehaviorsEquivalent(
        normalizedPoisonMergeBehavior(lhs),
        normalizedPoisonMergeBehavior(rhs));
}

bool sumsSameEventPoisonDamage(const ApplyStatusEffectCommand& command)
{
    return command.status == BattleStatusKind::Poison
        && command.poisonSameEventMerge
            == PoisonSameEventMerge::SumDamagePercent;
}

void aggregateEventPoisonDamagePercent(std::vector<EffectCommand>& commands)
{
    std::ranges::stable_sort(commands, [](const auto& lhs, const auto& rhs)
    {
        return effectExecutionOrderKey(lhs.metadata)
            < effectExecutionOrderKey(rhs.metadata);
    });
    std::vector<EffectCommand> aggregated;
    aggregated.reserve(commands.size());
    for (auto& command : commands)
    {
        auto* status = std::get_if<ApplyStatusEffectCommand>(&command.value);
        if (!status || !sumsSameEventPoisonDamage(*status))
        {
            aggregated.push_back(std::move(command));
            continue;
        }

        assert(status->status == BattleStatusKind::Poison);
        assert(status->stack == EffectStackPolicy::KeepStrongest);
        const auto sameAggregateTarget = [&](EffectCommand& candidate)
        {
            const auto* existing = std::get_if<ApplyStatusEffectCommand>(&candidate.value);
            return existing
                && sumsSameEventPoisonDamage(*existing)
                && candidate.metadata.targetUnitId == command.metadata.targetUnitId
                && candidate.metadata.binding.ownerUnitId == command.metadata.binding.ownerUnitId
                && existing->behavior
                && status->behavior
                && poisonMergeBehaviorsEquivalent(
                    *existing->behavior,
                    *status->behavior);
        };
        const auto existingCommand = std::find_if(
            aggregated.begin(), aggregated.end(), sameAggregateTarget);
        if (existingCommand == aggregated.end())
        {
            aggregated.push_back(std::move(command));
            continue;
        }

        auto& existing = std::get<ApplyStatusEffectCommand>(existingCommand->value);
        assert(existing.behavior);
        assert(status->behavior);
        auto combinedBehavior = std::make_shared<StatusBehaviorDefinition>(
            *existing.behavior);
        auto& existingDamage = canonicalPoisonDamageNumber(*combinedBehavior);
        auto addedBehavior = *status->behavior;
        const auto& addedDamage = canonicalPoisonDamageNumber(addedBehavior);
        existingDamage.percent = clampToInt(saturatingAdd(
            existingDamage.percent,
            addedDamage.percent));
        existing.behavior = std::move(combinedBehavior);
        existing.durationFrames = std::max(existing.durationFrames, status->durationFrames);
        existing.stacks = std::max(existing.stacks, status->stacks);
    }

    for (auto& command : aggregated)
    {
        if (auto* status = std::get_if<ApplyStatusEffectCommand>(&command.value);
            status && sumsSameEventPoisonDamage(*status))
        {
            status->poisonSameEventMerge = PoisonSameEventMerge::None;
        }
    }
    commands = std::move(aggregated);
}

}  // namespace

ModifyAttributeEffectCommand prepareModifyAttribute(
    const ModifyAttributeAction& action, int amount)
{
    return {
        .attribute = action.attribute,
        .amount = amount,
        .operation = action.operation,
        .durationFrames = action.durationFrames,
        .stack = action.stack,
        .stackLimit = action.stackLimit,
        .stackScope = action.stackScope,
    };
}

ModifyDamageEffectCommand prepareModifyDamage(
    const ModifyDamageAction& action, int amount)
{
    return {
        .perspective = action.perspective,
        .stage = action.stage,
        .channel = action.channel,
        .amount = amount,
        .operation = action.operation,
        .durationFrames = action.durationFrames,
        .stack = action.stack,
        .stackLimit = action.stackLimit,
        .stackScope = action.stackScope,
    };
}

ChangeResourceEffectCommand prepareChangeResource(
    const ChangeResourceAction& action, ResourceEffectAmount amount,
    std::vector<int> destinations)
{
    return {
        .resource = action.resource,
        .amount = std::move(amount),
        .kind = action.kind,
        .healKind = action.healKind,
        .healSourcePolicy = action.healSourcePolicy,
        .healRequiresFullMp = action.healRequiresFullMp,
        .transferDestinationUnitIds = std::move(destinations),
    };
}

ModifyAttackEffectCommand prepareModifyAttack(
    const ModifyAttackAction& action, std::optional<int> damageOverride,
    std::optional<ResolvedEffectAttackSource> source)
{
    return {
        .pattern = action.pattern,
        .strengthPct = action.strengthPct,
        .through = action.through,
        .tracking = action.tracking,
        .mainProjectile = action.mainProjectile,
        .sameTargetHitLimit = action.sameTargetHitLimit,
        .projectileClearRadiusPct = action.projectileClearRadiusPct,
        .targets = action.targets,
        .propagation = action.propagation,
        .addToBaseAttack = action.addToBaseAttack,
        .source = source,
        .damageOverride = damageOverride,
        .damageKind = action.damageKind,
        .runtimeBehavior = action.runtimeBehavior,
        .independentProjectile = action.independentProjectile,
        .activationLog = action.activationLog,
    };
}

ModifyCastEffectCommand prepareModifyCast(
    const ModifyCastAction& action, std::optional<int> mpCost)
{
    return {
        .mpCost = mpCost,
        .rangeMode = action.rangeMode,
        .projectileSpeedPct = action.projectileSpeedPct,
        .minimumSelectDistance = action.minimumSelectDistance,
        .additionalProjectiles = action.additionalProjectiles,
        .mobility = action.mobility,
        .autoUltimate = action.autoUltimate,
        .replacementPattern = action.replacementPattern,
        .freeAdditionalCast = action.freeAdditionalCast,
        .propagation = action.propagation,
    };
}

DealDamageEffectCommand prepareDealDamage(
    const DealDamageAction& action, int amount, int transactionCount)
{
    EffectDamageDelivery delivery{
        .area = action.area,
        .perCast = action.perCast,
        .areaProjectiles = action.areaProjectiles,
    };
    if (action.areaProjectiles && action.amount.base == EffectNumberBase::SourceMaxHp)
    {
        delivery.projectileSourceMaxHpPercent = action.amount.percent;
        if (!action.amount.multiplierBase && action.amount.flat == 0 && action.amount.percent > 0)
            delivery.displayedSourceMaxHpPercent = action.amount.percent;
    }
    return {
        .amount = amount,
        .transactionCount = transactionCount,
        .kind = action.kind,
        .appliesDamageModifiers = action.appliesDamageModifiers,
        .triggersHurtInvincibility = action.triggersHurtInvincibility,
        .canExecute = action.kind == BattleDamageKind::Execute,
        .delivery = std::move(delivery),
    };
}

ApplyStatusEffectCommand prepareStatusApplication(
    const ApplyStatusAction& action,
    int durationFrames,
    std::shared_ptr<const StatusBehaviorDefinition> behavior)
{
    ApplyStatusEffectCommand command;
    command.status = action.status;
    command.durationFrames = durationFrames;
    command.behavior = std::move(behavior);
    command.poisonSameEventMerge = action.poisonSameEventMerge;
    const auto quantity = lowerStatusQuantity(action);
    command.stacks = quantity.stacks;
    command.stack = lowerStatusReapplication(action);
    command.stackLimit = quantity.stackLimit;
    const auto storage = statusCatalogEntry(action.status).storage;
    if (storage == StatusStorageModel::SharedLayerDebuff)
    {
        command.targetTotalLimit = command.stackLimit;
        command.stackLimit.reset();
    }
    else if (storage != StatusStorageModel::ProducerOwnedContributions)
    {
        command.stackLimit.reset();
    }
    if (action.status == BattleStatusKind::Poison)
    {
        // Poison is governed by its explicit strongest/same-event group
        // reducer. Trigger charges describe the winning group clock rather
        // than a producer-family capacity and therefore do not participate in
        // family-limit identity.
        command.stackLimit.reset();
    }
    if (std::holds_alternative<NoStatusQuantity>(action.quantity)
        && command.behavior
        && statusCatalogEntry(action.status).storage
            == StatusStorageModel::ProducerOwnedContributions)
    {
        // Duration-only authored behaviors still need a finite holder-local
        // producer-family allocation. A runtime alias or a newly bound value
        // can refresh the existing immutable generation's duration, but cannot
        // accumulate another active copy or replace its bound behavior values.
        command.stackLimit = 1;
    }
    return command;
}

RemoveStatusEffectCommand prepareStatusRemoval(
    const RemoveStatusAction& action, const EffectCommandMetadata& metadata)
{
    return { {
        .statuses = action.statuses,
        .filter = resolveStatusContributionFilter(
            action.source, metadata.binding, metadata.statusContribution),
        .negativeOnly = action.negativeOnly,
        .controlOnly = action.controlOnly,
        .clearCurrentActionStagger = action.clearCurrentActionStagger,
        .count = action.count,
        .order = action.order,
    } };
}

const BattleCastProvenance* effectEventCastProvenance(const EffectEventPayload& payload)
{
    return std::visit(Overloaded{
        [](const CastPlanEventData& data) { return &data.provenance; },
        [](const CastCommitEventData& data) { return &data.provenance; },
        [](const AttackEventData& data) { return &data.provenance.cast; },
        [](const HitEventData& data) { return &data.provenance.cast; },
        [](const CastAggregateEventData& data) { return &data.provenance; },
        [](const HealRequestEventData& data) { return data.cast ? &*data.cast : nullptr; },
        [](const HealResultEventData& data) { return data.request.cast ? &*data.request.cast : nullptr; },
        [&](const DamageResultEventData& data) {
            return effectDamageCastProvenance(data.origin);
        },
        [&](const ShieldBreakEventData& data) {
            return effectDamageCastProvenance(data.cause);
        },
        [&](const DeathEventData& data) {
            return effectDamageCastProvenance(data.cause);
        },
        [](const auto&) -> const BattleCastProvenance* { return nullptr; },
    }, payload);
}

const BattleCastProvenance* effectCastProvenance(const EffectEventContext& context)
{
    if (context.scope.cast) return &*context.scope.cast;
    // 治療的施放來源只用於後續命令歸因，不限制受療者的武功規則。
    if (std::holds_alternative<HealRequestEventData>(context.payload)
        || std::holds_alternative<HealResultEventData>(context.payload)) return nullptr;
    return effectEventCastProvenance(context.payload);
}

const BattleAttackProvenance* effectEventAttackProvenance(const EffectEventPayload& payload)
{
    return std::visit(Overloaded{
        [](const AttackEventData& data) { return &data.provenance; },
        [](const HitEventData& data) { return &data.provenance; },
        [&](const DamageResultEventData& data) {
            return effectDamageAttackProvenance(data.origin);
        },
        [&](const ShieldBreakEventData& data) {
            return effectDamageAttackProvenance(data.cause);
        },
        [&](const DeathEventData& data) {
            return effectDamageAttackProvenance(data.cause);
        },
        [](const auto&) -> const BattleAttackProvenance* { return nullptr; },
    }, payload);
}

const BattleAttackProvenance* effectAttackProvenance(const EffectEventContext& context)
{
    if (context.scope.attack) return &*context.scope.attack;
    return effectEventAttackProvenance(context.payload);
}

void BattleEffectSystem::finalizeEventDispatch(
    BattleEffectRuleStore& store,
    const EffectEventContext& context,
    BattleEffectDispatchResult& result) const
{
    if (context.event == EffectEvent::CastSettled)
    {
        const auto* cast = effectCastProvenance(context);
        assert(cast);
        for (auto& bound : store.rules_)
        {
            std::erase_if(bound.runtime.activationEvaluations, [&](const auto& entry)
            {
                return entry.castId == cast->castId.value();
            });
        }
    }
    aggregateEventPoisonDamagePercent(result.commands);
    std::ranges::stable_sort(result.commands, [](const auto& lhs, const auto& rhs)
    {
        return effectExecutionOrderKey(lhs.metadata)
            < effectExecutionOrderKey(rhs.metadata);
    });
    for (std::uint64_t ordinal = 0; ordinal < result.commands.size(); ++ordinal)
        result.commands[ordinal].metadata.commandOrdinal = ordinal;
}

namespace
{

bool exactRuntimeRuleAvailable(
    const BoundEffectRule& bound,
    const EffectEventContext& context)
{
    if (!requiresExactRuntimePhaseQuery(bound.rule())
        || !castScopeMatches(bound, context)) return false;
    auto ruleContext = context;
    rewriteScopedRuleContext(bound, ruleContext);
    return ruleAllowedByPropagation(bound, ruleContext)
        && ruleMatchesMagicCast(bound, ruleContext)
        && ruleActivationAvailable(bound.rule(), bound.runtime, context.header.frame);
}

}  // namespace

bool BattleEffectSystem::hasExactRuntimeRuleCandidates(
    const BattleEffectRuleStore& store,
    EffectEvent event,
    int ownerUnitId) const
{
    const auto& indices = store.ruleIndicesByEvent_[static_cast<std::size_t>(event)];
    return std::ranges::any_of(indices, [&](std::size_t index)
    {
        const auto& bound = store.rules_[index];
        return (bound.rule().observation != EffectObservationScope::Owner
                || bound.binding.ownerUnitId < 0
                || bound.binding.ownerUnitId == ownerUnitId)
            && requiresExactRuntimePhaseQuery(bound.rule());
    });
}

bool BattleEffectSystem::hasExactRuntimeRuleCandidates(
    const BattleEffectRuleStore& store,
    const EffectEventData& event,
    int ownerUnitId) const
{
    const auto& indices = store.ruleIndicesByEvent_[static_cast<std::size_t>(event.event)];
    const EffectEventContext context(event);
    return std::ranges::any_of(indices, [&](std::size_t index)
    {
        const auto& bound = store.rules_[index];
        return (bound.rule().observation != EffectObservationScope::Owner
                || bound.binding.ownerUnitId < 0
                || bound.binding.ownerUnitId == ownerUnitId)
            && exactRuntimeRuleAvailable(bound, context);
    });
}

std::pmr::vector<EffectExactRuntimeRuleMatch> BattleEffectSystem::queryExactRuntimeRules(
    const BattleEffectRuleStore& store,
    const EffectEventContext& context,
    BattleRuntimeRandom& random,
    std::pmr::memory_resource* memoryResource) const
{
    if (!eventPayloadMatches(context))
    {
        throw std::invalid_argument("EffectEvent 與 typed payload 不相符");
    }

    const auto& indices = store.ruleIndicesByEvent_[static_cast<std::size_t>(context.event)];
    std::array<std::byte, 4096> buffer;
    std::pmr::monotonic_buffer_resource memory(buffer.data(), buffer.size());
    const auto ordered = orderedRuleIndices(store.rules_, indices, &memory);

    std::pmr::vector<EffectExactRuntimeRuleMatch> result(memoryResource);
    result.reserve(ordered.size());
    for (const auto index : ordered)
    {
        const auto* bound = &store.rules_[index];
        if (!requiresExactRuntimePhaseQuery(bound->rule())
            || !ruleObservesEvent(*bound, context)
            || !exactRuntimeRuleAvailable(*bound, context))
        {
            continue;
        }

        auto ruleContext = context;
        rewriteObservedOwner(*bound, ruleContext);
        rewriteScopedRuleContext(*bound, ruleContext);
        const auto selectedIds = selectTargetsWithResource(bound->rule().selector, ruleContext, random, {}, &memory);
        EffectExactRuntimeRuleMatch match{
            .bound = bound,
            .targetUnitIds = std::pmr::vector<int>{ memoryResource },
        };
        match.targetUnitIds.reserve(selectedIds.size());
        for (const auto unitId : selectedIds)
        {
            const auto* target = snapshotForTarget(ruleContext, unitId);
            if (!target)
            {
                throw std::logic_error("selector 傳回了 read view 中不存在的單位");
            }
            if (conditionsSatisfied(bound->rule().conditions, ruleContext, *target, true))
            {
                match.targetUnitIds.push_back(unitId);
            }
        }
        if (!match.targetUnitIds.empty())
        {
            result.push_back(std::move(match));
        }
    }
    return result;
}

bool BattleEffectSystem::hasInvincibilityPiercingExecuteRule(
    const BattleEffectRuleStore& store,
    const EffectEventContext& context) const
{
    // 接觸判定早於命中規則；此處只辨識目前施放可觀察到的處決規則，
    // 實際目標條件、次數限制與機率仍在命中事件中照常判定。
    for (const auto& bound : store.rules_)
    {
        if ((bound.rule().event != EffectEvent::MainProjectileBeforeDamage
                && bound.rule().event != EffectEvent::HitBeforeDamage)
            || !ruleContainsExecute(bound.rule())
            || !ruleObservesEvent(bound, context)
            || !castScopeMatches(bound, context))
        {
            continue;
        }

        auto ruleContext = context;
        rewriteObservedOwner(bound, ruleContext);
        rewriteScopedRuleContext(bound, ruleContext);
        if (ruleAllowedByPropagation(bound, ruleContext)
            && ruleMatchesMagicCast(bound, ruleContext))
        {
            return true;
        }
    }
    return false;
}

BattleEffectDispatchResult BattleEffectSystem::dispatch(BattleEffectRuleStore& store,
                                                        const EffectEventContext& context,
                                                        BattleRuntimeRandom& random) const
{
    if (!eventPayloadMatches(context))
    {
        throw std::invalid_argument("EffectEvent 與 typed payload 不相符");
    }
    const auto& indices = store.ruleIndicesByEvent_[static_cast<std::size_t>(context.event)];
    return dispatchRuleIndices(store, context, random, indices);
}

namespace
{
void evaluateOrdinaryRule(
    EffectStateAccess state,
    std::span<const BoundEffectRule> rules,
    const BoundEffectRuleView& bound,
    EffectRuleRuntimeState& runtime,
    const EffectEventContext& context,
    BattleRuntimeRandom& random,
    BattleEffectDispatchResult& result,
    std::uint64_t& nextCommandOrdinal)
{
    if (bound.rule().event != context.event
        || !ruleObservesEvent(bound, context)
        || !castScopeMatches(bound, context))
    {
        return;
    }

    const EffectEventContext* ruleContext = &context;
    std::optional<EffectEventContext> rewrittenContext;
    if (bound.rule().observation != EffectObservationScope::Owner
        || bound.castScope)
    {
        rewrittenContext.emplace(context);
        rewriteObservedOwner(bound, *rewrittenContext);
        rewriteScopedRuleContext(bound, *rewrittenContext);
        ruleContext = &*rewrittenContext;
    }
    if (!ruleAllowedByPropagation(bound, *ruleContext)
        || !ruleMatchesMagicCast(bound, *ruleContext))
    {
        return;
    }
    if (requiresExactRuntimePhaseQuery(bound.rule()))
    {
        return;
    }

    if (!ruleActivationAvailable(bound.rule(), runtime, context.header.frame)) return;
    if (bound.rule().intervalFrames > 0)
    {
        assert(context.event == EffectEvent::FrameAdvanced);
        const auto& tick = std::get<FrameTickEventData>(context.payload);
        assert(tick.deltaFrames > 0);
        assert(runtime.intervalFramesRemaining > 0);
        runtime.intervalFramesRemaining -= tick.deltaFrames;
        if (runtime.intervalFramesRemaining > 0)
        {
            return;
        }
        runtime.intervalFramesRemaining = bound.rule().intervalFrames;
    }

    std::array<std::byte, 4096> buffer;
    std::pmr::monotonic_buffer_resource memory(buffer.data(), buffer.size());
    const auto selectedIds = selectTargetsWithResource(bound.rule().selector, *ruleContext, random, {}, &memory);
    if (selectedIds.empty())
    {
        return;
    }

    std::pmr::vector<const EffectUnitSnapshot*> eligibleTargets(&memory);
    eligibleTargets.reserve(selectedIds.size());
    for (const auto unitId : selectedIds)
    {
        const auto* target = snapshotForTarget(*ruleContext, unitId);
        if (!target)
        {
            throw std::logic_error("selector 傳回了 read view 中不存在的單位");
        }
        if (conditionsSatisfied(bound.rule().conditions, *ruleContext, *target, true))
        {
            eligibleTargets.push_back(target);
        }
    }
    if (eligibleTargets.empty())
    {
        return;
    }
    if (bound.rule().everyNthEvent > 0)
    {
        ++runtime.eligibleEventCount;
        if (runtime.eligibleEventCount % bound.rule().everyNthEvent != 0)
        {
            return;
        }
    }

    std::pmr::vector<const EffectUnitSnapshot*> activationTargets(&memory);
    activationTargets.reserve(eligibleTargets.size());
    if (bound.rule().activationLimit)
    {
        const auto* cast = effectCastProvenance(*ruleContext);
        if (!cast)
        {
            throw std::logic_error("施放範圍觸發限制需要 cast provenance");
        }
        activationTargets.reserve(eligibleTargets.size());
        for (const auto* target : eligibleTargets)
        {
            switch (bound.rule().activationLimit->scope)
            {
            case EffectActivationScope::PerCastPerTarget:
            {
                auto& evaluationCount = activationEvaluation(
                    runtime,
                    cast->castId,
                    target->id);
                if (evaluationCount >= bound.rule().activationLimit->maxEvaluations)
                {
                    continue;
                }
                // 先佔用本次判定，機率失敗後同一施放不得重擲。
                ++evaluationCount;
                break;
            }
            }
            if (random.chance(bound.rule().chancePct))
            {
                activationTargets.push_back(target);
            }
        }
    }
    else
    {
        if (!random.chance(bound.rule().chancePct))
        {
            return;
        }
        activationTargets = eligibleTargets;
    }
    if (activationTargets.empty())
    {
        return;
    }

    ++runtime.activationCount;
    if (bound.rule().sharedCooldownFrames > 0)
    {
        runtime.sharedCooldownUntilFrame =
            static_cast<std::int64_t>(context.header.frame)
            + bound.rule().sharedCooldownFrames;
    }
    EffectRuleActivation activation{
        .binding = bound.binding,
        .ruleId = bound.rule().id,
    };
    activation.targetUnitIds.reserve(activationTargets.size());
    for (const auto* target : activationTargets)
    {
        activation.targetUnitIds.push_back(target->id);
    }
    result.activations.push_back(std::move(activation));

    CommandEmitter emitter{
        .state = state,
        .rules = rules,
        .bound = bound,
        .context = *ruleContext,
        .random = random,
        .commands = result.commands,
        .nextCommandOrdinal = nextCommandOrdinal,
    };
    const int repetitionCount = bound.rule().repetitionCount
        ? emitter.evaluate(*bound.rule().repetitionCount, *ruleContext->scope.owner)
        : 1;
    assert(repetitionCount > 0);
    const auto authoredLeafCount = std::accumulate(
        bound.rule().actions.begin(),
        bound.rule().actions.end(),
        std::uint64_t{},
        [](std::uint64_t count, const EffectAction& action)
        {
            return saturatingAdd(count, authoredActionLeafCount(action));
        });
    const auto emittedActionCount = saturatingMultiply(
        static_cast<std::uint64_t>(repetitionCount),
        authoredLeafCount);
    assert(emittedActionCount <= std::numeric_limits<std::uint32_t>::max());
    for (int repetition = 0; repetition < repetitionCount; ++repetition)
    {
        std::uint64_t authoredActionOffset{};
        for (const auto& action : bound.rule().actions)
        {
            const auto actionOrder = static_cast<std::uint32_t>(repetition)
                * static_cast<std::uint32_t>(authoredLeafCount)
                + static_cast<std::uint32_t>(authoredActionOffset);
            for (std::uint32_t targetOrder = 0;
                 targetOrder < static_cast<std::uint32_t>(activationTargets.size());
                 ++targetOrder)
            {
                emitter.emit(action,
                             *activationTargets[targetOrder],
                             actionOrder,
                             static_cast<std::uint32_t>(authoredActionOffset),
                             targetOrder);
            }
            authoredActionOffset = saturatingAdd(
                authoredActionOffset,
                authoredActionLeafCount(action));
        }
    }
}
}  // namespace

BattleEffectDispatchResult BattleEffectSystem::dispatchRuleIndices(
    BattleEffectRuleStore& store,
    const EffectEventContext& context,
    BattleRuntimeRandom& random,
    std::span<const std::size_t> ruleIndices,
    bool finalizeEvent) const
{
    if (!eventPayloadMatches(context))
    {
        throw std::invalid_argument("EffectEvent 與 typed payload 不相符");
    }

    BattleEffectDispatchResult result;
    std::uint64_t nextCommandOrdinal{};
    const auto evaluate = [&](BoundEffectRule& bound)
    {
        evaluateOrdinaryRule(
            { store.stateValues_ }, store.rules_, bound,
            bound.runtime,
            context, random, result, nextCommandOrdinal);
    };
    if (ruleIndices.size() == 1)
    {
        const auto index = ruleIndices.front();
        if (index >= store.rules_.size())
            throw std::out_of_range("效果規則索引超出範圍");
        evaluate(store.rules_[index]);
    }
    else if (!ruleIndices.empty())
    {
        std::array<std::byte, 4096> buffer;
        std::pmr::monotonic_buffer_resource memory(buffer.data(), buffer.size());
        for (const auto index : orderedRuleIndices(store.rules_, ruleIndices, &memory)) evaluate(store.rules_[index]);
    }
    if (finalizeEvent) finalizeEventDispatch(store, context, result);
    return result;
}

bool BattleEffectSystem::statusBehaviorRuleMatchesEvent(
    const EffectRule& rule,
    EffectEvent event,
    StatusBehaviorDispatchFilter filter)
{
    if (rule.event != event || event == EffectEvent::StatusPersistent) return false;
    if (filter == StatusBehaviorDispatchFilter::All) return true;
    const auto actionContains = [&]<typename Action>(
                                    const auto& self,
                                    const EffectAction& action) -> bool
    {
        if (std::holds_alternative<Action>(action.value))
            return true;
        const auto* conditional = std::get_if<std::shared_ptr<ConditionalEffectAction>>(
            &action.value);
        if (!conditional) return false;
        assert(*conditional);
        const auto contains = [&](const std::vector<EffectAction>& actions)
        {
            return std::ranges::any_of(actions, [&](const auto& nested)
            {
                return self.template operator()<Action>(self, nested);
            });
        };
        return contains((*conditional)->whenTrue)
            || contains((*conditional)->whenFalse);
    };
    const bool suppressesOutgoingCast = std::ranges::any_of(
        rule.actions,
        [&](const auto& action)
        {
            return actionContains.template operator()<
                SuppressCurrentCastContactsAction>(actionContains, action);
        });
    const bool makesIncomingAttackMiss = std::ranges::any_of(
        rule.actions,
        [&](const auto& action)
        {
            return actionContains.template operator()<
                MakeIncomingAttackMissAction>(actionContains, action);
        });
    const bool attackInterceptor = suppressesOutgoingCast || makesIncomingAttackMiss;
    switch (filter)
    {
    case StatusBehaviorDispatchFilter::All: return true;
    case StatusBehaviorDispatchFilter::AttackInterceptorsOnly: return attackInterceptor;
    case StatusBehaviorDispatchFilter::OutgoingCastSuppressorsOnly: return suppressesOutgoingCast;
    case StatusBehaviorDispatchFilter::IncomingAttackMissOnly: return makesIncomingAttackMiss;
    case StatusBehaviorDispatchFilter::ExcludeAttackInterceptors: return !attackInterceptor;
    }
    assert(false);
    return false;
}

bool BattleEffectSystem::rulesNeedStatusPrediction(
    const BattleEffectRuleStore& store,
    const EffectEventContext& context,
    std::span<const ActiveStatusBehaviorView> behaviors,
    BattleRuntimeRandom& random,
    bool includeAllFrameOwners) const
{
    assert(!includeAllFrameOwners || context.event == EffectEvent::FrameAdvanced);
    const auto* tick = std::get_if<FrameTickEventData>(&context.payload);
    std::array<std::byte, 4096> buffer;
    std::pmr::monotonic_buffer_resource memory(buffer.data(), buffer.size());
    const auto couldEmit = [&](const EffectRule& rule, const EffectRuleRuntimeState& runtime)
    {
        if (rule.event != context.event) return false;
        assert(rule.intervalFrames <= 0 || tick);
        return !requiresExactRuntimePhaseQuery(rule)
            && ruleActivationAvailable(rule, runtime, context.header.frame)
            && (rule.intervalFrames <= 0 || runtime.intervalFramesRemaining <= tick->deltaFrames)
            && std::ranges::any_of(rule.actions, BattleEffectCommandSystem::actionMayAffectStatusLiveness);
    };
    const auto hasEligibleTarget = [&](const BoundEffectRuleView& bound, EffectEventContext ruleContext)
    {
        if (!ruleObservesEvent(bound, ruleContext)) return false;
        rewriteObservedOwner(bound, ruleContext);
        rewriteScopedRuleContext(bound, ruleContext);
        if (!ruleAllowedByPropagation(bound, ruleContext) || !ruleMatchesMagicCast(bound, ruleContext))
            return false;
        // 隨機選擇保守地視為可能執行；預檢不可消耗 RNG 或改變規則計時器。
        if (bound.rule().selector.tieBreak == EffectTieBreak::BattleRandom) return true;
        const auto drawsBefore = random.rawDrawCount();
        const auto targets = selectTargetsWithResource(bound.rule().selector, ruleContext, random, {}, &memory);
        assert(random.rawDrawCount() == drawsBefore);
        (void)drawsBefore;
        return std::ranges::any_of(targets, [&](int unitId)
        {
            const auto* target = snapshotForTarget(ruleContext, unitId);
            assert(target);
            return conditionsSatisfied(bound.rule().conditions, ruleContext, *target, true);
        });
    };
    for (const auto index : store.ruleIndicesByEvent_[static_cast<std::size_t>(context.event)])
    {
        const auto& bound = store.rules_[index];
        if (!couldEmit(bound.rule(), bound.runtime)) continue;
        auto ruleContext = context;
        if (includeAllFrameOwners)
        {
            const auto* owner = context.header.battle.findUnit(bound.binding.ownerUnitId);
            if (!owner || !owner->alive) continue;
            ruleContext.scope.owner = owner;
        }
        if (hasEligibleTarget(bound, ruleContext)) return true;
    }
    for (const auto& behavior : behaviors)
    {
        if (!behavior.behavior || behavior.quantity <= 0) continue;
        assert(behavior.runtime);
        assert(behavior.runtime->size() == behavior.behavior->rules.size());
        for (std::uint32_t index = 0; index < behavior.behavior->rules.size(); ++index)
        {
            const auto& rule = behavior.behavior->rules[index];
            if (!couldEmit(rule, (*behavior.runtime)[index])) continue;
            auto statusContext = context;
            statusContext.scope.statusContribution = EffectStatusContributionContext{
                .holderUnitId = behavior.holderUnitId,
                .sourceUnitId = behavior.sourceUnitId,
                .kind = behavior.kind,
                .quantity = behavior.quantity,
                .appliedSequence = behavior.appliedSequence,
                .producerRuleId = behavior.producerRuleId,
                .producerRuleOrder = behavior.producerRuleOrder,
                .producerActionOrder = behavior.producerActionOrder,
                .behaviorRuleOrder = index,
            };
            if (hasEligibleTarget({ behavior.binding, rule, behavior.producerRuleOrder }, statusContext))
                return true;
        }
    }
    return false;
}

BattleEffectDispatchResult BattleEffectSystem::dispatchMerged(
    BattleEffectRuleStore& store,
    const EffectEventContext& context,
    BattleRuntimeRandom& random,
    std::span<const ActiveStatusBehaviorView> behaviors,
    StatusBehaviorDispatchFilter filter,
    bool includeAllFrameOwners,
    const StatusBehaviorDispatchLiveness* reducerLiveness) const
{
    assert(!includeAllFrameOwners || context.event == EffectEvent::FrameAdvanced);
    assert(behaviors.empty() || reducerLiveness);
    assert(!reducerLiveness
        || (reducerLiveness->contributionQuantity
            && reducerLiveness->reduceRuleCommands));
    struct PendingRule
    {
        std::optional<std::size_t> configuredRuleIndex;
        const ActiveStatusBehaviorView* behavior{};
        std::uint32_t behaviorRuleOrder{};
        EffectExecutionOrderKey order;
        std::size_t insertionOrder{};
        std::optional<BattleEffectDispatchResult> precomputed;
    };
    std::array<std::byte, 8192> buffer;
    std::pmr::monotonic_buffer_resource memory(buffer.data(), buffer.size());
    std::pmr::vector<PendingRule> pendingRules(&memory);
    const auto& configuredIndices =
        store.ruleIndicesByEvent_[static_cast<std::size_t>(context.event)];
    auto pendingCapacity = configuredIndices.size();
    for (const auto& behavior : behaviors)
    {
        if (behavior.behavior && behavior.quantity > 0)
            pendingCapacity += behavior.behavior->rules.size();
    }
    pendingRules.reserve(pendingCapacity);
    // 明確保留相同 order 的插入順序，排序不再需要 stable_sort 的暫存配置。
    const auto pendingOrderLess = [](const PendingRule& lhs, const PendingRule& rhs)
    {
        return std::tie(lhs.order, lhs.insertionOrder) < std::tie(rhs.order, rhs.insertionOrder);
    };
    const auto addConfiguredRule = [&](std::size_t ruleIndex)
    {
        if (ruleIndex >= store.rules_.size())
            throw std::out_of_range("效果規則索引超出範圍");
        const auto& bound = store.rules_[ruleIndex];
        if (bound.rule().event != context.event) return;
        if (context.event == EffectEvent::FrameAdvanced
            && includeAllFrameOwners)
        {
            const auto* owner = context.header.battle.findUnit(
                bound.binding.ownerUnitId);
            if (!owner || !owner->alive) return;
        }
        pendingRules.push_back({
            .configuredRuleIndex = ruleIndex,
            .order = {
                .sourcePrecedence = effectSourcePrecedence(bound.binding.kind),
                .producerRuleOrder = bound.order,
                .lane = EffectExecutionLane::Configured,
            },
            .insertionOrder = pendingRules.size(),
        });
    };
    for (const auto ruleIndex : configuredIndices) addConfiguredRule(ruleIndex);

    for (const auto& behavior : behaviors)
    {
        if (!behavior.behavior || behavior.quantity <= 0) continue;
        if (context.event == EffectEvent::FrameAdvanced)
        {
            const auto* holder = context.header.battle.findUnit(
                behavior.holderUnitId);
            if (!holder || !holder->alive) continue;
        }
        assert(behavior.runtime);
        assert(behavior.runtime->size() == behavior.behavior->rules.size());
        for (std::uint32_t ruleOrder = 0;
             ruleOrder < behavior.behavior->rules.size();
             ++ruleOrder)
        {
            const auto& rule = behavior.behavior->rules[ruleOrder];
            if (!statusBehaviorRuleMatchesEvent(rule, context.event, filter))
                continue;

            pendingRules.push_back({
                .behavior = &behavior,
                .behaviorRuleOrder = ruleOrder,
                .order = statusBehaviorExecutionOrderKey(
                    behavior.binding,
                    behavior.producerRuleOrder,
                    behavior.producerActionOrder,
                    ruleOrder,
                    behavior.holderUnitId,
                    behavior.appliedSequence,
                    0),
                .insertionOrder = pendingRules.size(),
            });
        }
    }
    std::ranges::sort(pendingRules, pendingOrderLess);

    const auto isPoisonMergePreflightRule = [&](const PendingRule& pending)
    {
        if (!pending.configuredRuleIndex) return false;
        const auto& rule = store.rules_[*pending.configuredRuleIndex].rule();
        if (rule.actions.size() != 1) return false;
        const auto* application = std::get_if<ApplyStatusAction>(
            &rule.actions.front().value);
        return application
            && application->status == BattleStatusKind::Poison
            && application->poisonSameEventMerge
                == PoisonSameEventMerge::SumDamagePercent;
    };
    std::vector<EffectCommand> poisonMergeCommands;
    for (auto& pending : pendingRules)
    {
        if (!isPoisonMergePreflightRule(pending)) continue;
        const std::array ruleIndices{ *pending.configuredRuleIndex };
        pending.precomputed = dispatchRuleIndices(
            store,
            context,
            random,
            ruleIndices,
            false);
        poisonMergeCommands.insert(
            poisonMergeCommands.end(),
            std::make_move_iterator(pending.precomputed->commands.begin()),
            std::make_move_iterator(pending.precomputed->commands.end()));
        pending.precomputed->commands.clear();
    }
    aggregateEventPoisonDamagePercent(poisonMergeCommands);
    for (auto& command : poisonMergeCommands)
    {
        const auto pending = std::ranges::find_if(pendingRules, [&](const auto& candidate)
        {
            if (!candidate.configuredRuleIndex) return false;
            const auto& bound = store.rules_[*candidate.configuredRuleIndex];
            return bound.binding == command.metadata.binding
                && bound.rule().id == command.metadata.ruleId;
        });
        assert(pending != pendingRules.end());
        assert(pending->precomputed);
        pending->precomputed->commands.push_back(std::move(command));
    }

    const auto currentContributionQuantity = [&](const ActiveStatusBehaviorView& behavior)
        -> std::optional<int>
    {
        assert(reducerLiveness);
        const auto quantity = reducerLiveness->contributionQuantity(
            behavior.holderUnitId, behavior.appliedSequence, behavior.kind);
        if (!quantity) return std::nullopt;
        // 共用層數存活時沿用事件開始的數量；本次新增層數不能追溯觸發既有 tick。
        if (statusCatalogEntry(behavior.kind).storage == StatusStorageModel::SharedLayerDebuff)
            return behavior.quantity;
        return quantity;
    };

    BattleEffectDispatchResult result;
    std::size_t nextPendingRule{};
    while (nextPendingRule < pendingRules.size())
    {
        auto pending = std::move(pendingRules[nextPendingRule++]);
        BattleEffectDispatchResult dispatched;
        if (pending.precomputed)
        {
            dispatched = std::move(*pending.precomputed);
        }
        else if (pending.configuredRuleIndex)
        {
            const std::array ruleIndices{ *pending.configuredRuleIndex };
            auto configuredContext = context;
            if (context.event == EffectEvent::FrameAdvanced
                && includeAllFrameOwners)
            {
                const auto& bound = store.rules_[*pending.configuredRuleIndex];
                const auto* owner = context.header.battle.findUnit(
                    bound.binding.ownerUnitId);
                assert(owner && owner->alive);
                configuredContext.scope.owner = owner;
            }
            dispatched = dispatchRuleIndices(
                store,
                configuredContext,
                random,
                ruleIndices,
                false);
        }
        else
        {
            assert(pending.behavior);
            const auto& behavior = *pending.behavior;
            const auto currentQuantity = currentContributionQuantity(behavior);
            if (!currentQuantity) continue;
            const auto ruleOrder = pending.behaviorRuleOrder;
            const auto& rule = behavior.behavior->rules[ruleOrder];

            // 狀態行為的啟用狀態屬於 contribution；狀態槽維持原本每次規則求值的生命期。
            std::pmr::map<EffectStateKey, std::int64_t> statusStateValues(&memory);
            auto& statusRuntime = (*behavior.runtime)[ruleOrder];
            auto statusContext = context;
            statusContext.scope.statusContribution = EffectStatusContributionContext{
                .holderUnitId = behavior.holderUnitId,
                .sourceUnitId = behavior.sourceUnitId,
                .kind = behavior.kind,
                .quantity = *currentQuantity,
                .appliedSequence = behavior.appliedSequence,
                .producerRuleId = behavior.producerRuleId,
                .producerRuleOrder = behavior.producerRuleOrder,
                .producerActionOrder = behavior.producerActionOrder,
                .behaviorRuleOrder = ruleOrder,
            };
            std::uint64_t nextCommandOrdinal{};
            evaluateOrdinaryRule(
                { statusStateValues }, {},
                { behavior.binding, rule, behavior.producerRuleOrder },
                statusRuntime, statusContext, random, dispatched, nextCommandOrdinal);
            if (context.event == EffectEvent::CastSettled)
            {
                const auto* cast = effectCastProvenance(context);
                assert(cast);
                std::erase_if(statusRuntime.activationEvaluations, [&](const auto& entry)
                {
                    return entry.castId == cast->castId.value();
                });
            }
        }

        const bool resolvesInterceptor = std::ranges::any_of(
            dispatched.commands,
            [](const EffectCommand& command)
            {
                return std::holds_alternative<SuppressCurrentCastContactsEffectCommand>(
                        command.value)
                    || std::holds_alternative<MakeIncomingAttackMissEffectCommand>(
                        command.value);
            });
        if (reducerLiveness)
        {
            reducerLiveness->reduceRuleCommands(dispatched.commands);
        }

        std::pmr::vector<std::size_t> addedRuleIndices(&memory);
        for (const auto& command : dispatched.commands)
        {
            const auto* stateMachine = std::get_if<StateMachineEffectCommand>(
                &command.value);
            if (!stateMachine) continue;
            const auto* borrow = std::get_if<BorrowEffectRulesCommand>(
                &stateMachine->value);
            if (!borrow) continue;
            const auto* cast = effectCastProvenance(context);
            if (!cast)
                throw std::logic_error("借用效果規則需要 cast provenance");
            assert(borrow->propagation == CastPropagationPolicy::BorrowedUltimateRules);
            auto added = store.bindBorrowedUltimateRules(
                cast->castId,
                context.scope.owner->id,
                context.scope.owner->team,
                borrow->sourceUnitIds,
                borrow->filter,
                borrow->propagation);
            addedRuleIndices.insert(
                addedRuleIndices.end(),
                added.begin(),
                added.end());
        }
        appendDispatchResult(result, std::move(dispatched));
        if ((filter == StatusBehaviorDispatchFilter::AttackInterceptorsOnly
                || filter == StatusBehaviorDispatchFilter::IncomingAttackMissOnly)
            && resolvesInterceptor)
            break;

        if (!addedRuleIndices.empty())
        {
            for (const auto ruleIndex : addedRuleIndices)
                addConfiguredRule(ruleIndex);
            std::sort(
                pendingRules.begin() + static_cast<std::ptrdiff_t>(nextPendingRule),
                pendingRules.end(),
                pendingOrderLess);
        }
    }
    finalizeEventDispatch(store, context, result);
    return result;
}

BattleEffectDispatchResult BattleEffectSystem::dispatchStatusBehaviors(
    const EffectEventContext& context,
    BattleRuntimeRandom& random,
    std::span<const ActiveStatusBehaviorView> behaviors,
    StatusBehaviorDispatchFilter filter,
    const StatusBehaviorDispatchLiveness* reducerLiveness) const
{
    BattleEffectRuleStore emptyStore;
    return dispatchMerged(
        emptyStore,
        context,
        random,
        behaviors,
        filter,
        false,
        reducerLiveness);
}

}  // namespace KysChess::Battle
