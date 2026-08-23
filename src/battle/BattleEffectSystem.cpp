#include "BattleEffectSystem.h"

#include "BattleRuntimeRandom.h"
#include "BattleUnitValues.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace KysChess::Battle
{
namespace
{

template<class... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};

template<class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

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

std::vector<const BoundEffectRule*> orderedBoundRules(
    std::span<const BoundEffectRule> rules,
    std::span<const std::size_t> indices)
{
    std::vector<const BoundEffectRule*> ordered;
    ordered.reserve(indices.size());
    for (const auto index : indices)
    {
        if (index >= rules.size())
        {
            throw std::out_of_range("效果規則索引超出範圍");
        }
        ordered.push_back(&rules[index]);
    }
    std::ranges::stable_sort(ordered, [](const auto* lhs, const auto* rhs)
    {
        const int lhsSource = effectSourcePrecedence(lhs->binding.kind);
        const int rhsSource = effectSourcePrecedence(rhs->binding.kind);
        return lhsSource != rhsSource
            ? lhsSource < rhsSource
            : lhs->order < rhs->order;
    });
    return ordered;
}

EffectSharedCooldownKey sharedCooldownKey(
    const EffectSourceBinding& binding,
    EffectRuleId ruleId)
{
    return {
        .sourceKind = binding.kind,
        .sourceId = binding.sourceId,
        .sourceTeam = binding.sourceTeam,
        .sourceInstanceId = binding.runtimeInstanceId,
        .ruleId = ruleId,
    };
}

EffectActivationScopeKey activationScopeKey(const EffectSourceBinding& binding,
                                             EffectRuleId ruleId,
                                             BattleCastId castId,
                                             int targetUnitId)
{
    return {
        .rule = runtimeKey(binding, ruleId),
        .castId = castId,
        .targetUnitId = targetUnitId,
    };
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

std::int64_t roundRatio(
    std::int64_t numerator,
    std::int64_t denominator,
    EffectRounding rounding)
{
    assert(denominator > 0);
    std::int64_t quotient = numerator / denominator;
    const std::int64_t remainder = numerator % denominator;
    if (remainder == 0)
    {
        return quotient;
    }

    switch (rounding)
    {
    case EffectRounding::TowardZero:
        return quotient;
    case EffectRounding::Floor:
        return numerator < 0 ? quotient - 1 : quotient;
    case EffectRounding::Ceil:
        return numerator > 0 ? quotient + 1 : quotient;
    case EffectRounding::Nearest:
        if (std::abs(remainder) * 2 >= denominator)
        {
            quotient += numerator > 0 ? 1 : -1;
        }
        return quotient;
    }
    assert(false);
    return quotient;
}

int clampToInt(std::int64_t value)
{
    return static_cast<int>(std::clamp(
        value,
        static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

const BattleCastProvenance* castProvenance(const EffectEventContext& context)
{
    const auto fromOrigin = [](const EffectDamageOrigin& origin) -> const BattleCastProvenance*
    {
        if (const auto* attack = std::get_if<EffectAttackDamageOrigin>(&origin))
        {
            return &attack->provenance.cast;
        }
        return nullptr;
    };
    return std::visit(Overloaded{
        [](const CastPlanEventData& data) { return &data.provenance; },
        [](const CastCommitEventData& data) { return &data.provenance; },
        [](const AttackEventData& data) { return &data.provenance.cast; },
        [](const HitEventData& data) { return &data.provenance.cast; },
        [](const CastAggregateEventData& data) { return &data.provenance; },
        [&](const DamageResultEventData& data) { return fromOrigin(data.origin); },
        [&](const ShieldBreakEventData& data) { return fromOrigin(data.cause); },
        [&](const DeathEventData& data) { return fromOrigin(data.cause); },
        [](const auto&) -> const BattleCastProvenance* { return nullptr; },
    }, context.payload);
}

const BattleAttackProvenance* attackProvenance(const EffectEventContext& context)
{
    const auto fromOrigin = [](const EffectDamageOrigin& origin) -> const BattleAttackProvenance*
    {
        if (const auto* attack = std::get_if<EffectAttackDamageOrigin>(&origin))
        {
            return &attack->provenance;
        }
        return nullptr;
    };
    return std::visit(Overloaded{
        [](const AttackEventData& data) { return &data.provenance; },
        [](const HitEventData& data) { return &data.provenance; },
        [&](const DamageResultEventData& data) { return fromOrigin(data.origin); },
        [&](const ShieldBreakEventData& data) { return fromOrigin(data.cause); },
        [&](const DeathEventData& data) { return fromOrigin(data.cause); },
        [](const auto&) -> const BattleAttackProvenance* { return nullptr; },
    }, context.payload);
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
        [](const HitEventData& data) { return &data.defenderBefore; },
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
        [](const AttackEventData& data) { return &data.attacker; },
        [](const HitEventData& data) { return &data.attackerBefore; },
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
        [&context](const auto&) -> const EffectUnitSnapshot* { return &context.header.owner; },
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
    if (context.header.owner.id == unitId)
    {
        return &context.header.owner;
    }
    return context.header.battle.findUnit(unitId);
}

std::optional<int> sourceUnitId(const EffectEventContext& context)
{
    if (const auto* source = sourceSnapshot(context))
    {
        return source->id;
    }
    if (const auto* cast = castProvenance(context))
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
        return &context.header.owner;
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
            return context.header.owner.position;
        },
        [&context](const CastCommitEventData& data)
        {
            if (const auto* target = context.header.battle.findUnit(data.targetUnitId))
            {
                return target->position;
            }
            return context.header.owner.position;
        },
        [&context](const auto&) { return context.header.owner.position; },
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
    auto findResource = [unitId](const std::vector<EffectUnitResourceBeforeCast>& resources)
        -> std::optional<int>
    {
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
    auto findResource = [unitId](const std::vector<EffectUnitResourceBeforeCast>& resources)
        -> std::optional<int>
    {
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
    case EffectTeamFilter::Ally: return unit.team == context.header.owner.team;
    case EffectTeamFilter::Enemy: return unit.team != context.header.owner.team;
    }
    assert(false);
    return false;
}

bool matchesSelectorRequirements(const EffectUnitSnapshot& unit,
                                 const EffectSelector& selector,
                                 const EffectEventContext& context)
{
    if (selector.excludeOwner && unit.id == context.header.owner.id)
    {
        return false;
    }
    if (!matchesTeamFilter(unit, context, selector.team))
    {
        return false;
    }
    if (selector.requiredMagicId >= 0 && !unit.usesMagic(selector.requiredMagicId))
    {
        return false;
    }
    return selector.requiredWeaponType < 0 || unit.weaponType == selector.requiredWeaponType;
}

bool matchesSelectorMembership(const EffectUnitSnapshot& unit,
                               const EffectSelector& selector,
                               const EffectEventContext& context)
{
    switch (selector.kind)
    {
    case EffectSelectorKind::ComboMembers:
        return unit.comboIds.contains(context.header.binding.sourceId)
            || unit.comboIds.contains(selector.requiredMagicId);
    case EffectSelectorKind::Allies:
    case EffectSelectorKind::LowestHpAllies:
    case EffectSelectorKind::LowestMpAllies:
        return unit.team == context.header.owner.team;
    case EffectSelectorKind::Enemies:
    case EffectSelectorKind::HighestMpEnemy:
    case EffectSelectorKind::StrongestEnemies:
    case EffectSelectorKind::NearestEnemies:
    case EffectSelectorKind::FarthestEnemy:
        return unit.team != context.header.owner.team;
    case EffectSelectorKind::AlliesUsingWeapon:
        return unit.team == context.header.owner.team
            && unit.weaponType == selector.requiredWeaponType;
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

void randomizeTieGroups(std::vector<const EffectUnitSnapshot*>& candidates,
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
    case BattleDamageKind::Reflected: return label == "反射" || label == "Reflected";
    case BattleDamageKind::Execute: return label == "處決" || label == "Execute";
    }
    return false;
}

bool matchesHealKindLabel(BattleHealKind kind, std::string_view label)
{
    switch (kind)
    {
    case BattleHealKind::Direct: return label == "直接" || label == "Direct";
    case BattleHealKind::Team: return label == "隊伍" || label == "Team";
    case BattleHealKind::Aura: return label == "光環" || label == "Aura";
    case BattleHealKind::OnHit: return label == "命中" || label == "OnHit";
    case BattleHealKind::KillReward: return label == "擊殺" || label == "KillReward";
    case BattleHealKind::DeathMedical: return label == "死亡醫療" || label == "DeathMedical";
    case BattleHealKind::Rescue: return label == "救援" || label == "Rescue";
    case BattleHealKind::Regeneration: return label == "再生" || label == "Regeneration";
    case BattleHealKind::Lifesteal: return label == "吸血" || label == "Lifesteal";
    }
    return false;
}

bool conditionSatisfied(const EffectCondition& condition,
                        const EffectEventContext& context,
                        const EffectUnitSnapshot& target,
                        bool selectionAvailable)
{
    return std::visit(Overloaded{
        [&](const IsUltimateCondition&)
        {
            const auto* cast = castProvenance(context);
            return cast && cast->ultimate;
        },
        [&](const MagicIdEqualsCondition& value)
        {
            const auto* cast = castProvenance(context);
            return cast && cast->magicId == value.magicId;
        },
        [&](const IsMainProjectileCondition&)
        {
            const auto* attack = attackProvenance(context);
            return attack && attack->mainProjectile;
        },
        [&](const IsRootAttackCondition&)
        {
            const auto* attack = attackProvenance(context);
            return attack && attack->rootAttack;
        },
        [&](const SourceHpRatioAtMostCondition& value)
        {
            return context.header.owner.maxHp > 0 &&
                   static_cast<std::int64_t>(context.header.owner.hp) * 100 <=
                       static_cast<std::int64_t>(context.header.owner.maxHp) * value.percent;
        },
        [&](const SourceHpRatioBelowCondition& value)
        {
            return context.header.owner.maxHp > 0 &&
                   static_cast<std::int64_t>(context.header.owner.hp) * 100 <
                       static_cast<std::int64_t>(context.header.owner.maxHp) * value.percent;
        },
        [&](const SourceIsLastAliveCondition&)
        {
            return context.header.owner.alive
                && std::ranges::count_if(
                    context.header.battle.units(),
                    [&](const EffectUnitSnapshot& unit)
                    {
                        return unit.alive && unit.team == context.header.owner.team;
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
            return context.header.owner.hasState(value.state);
        },
        [&](const TargetHasStateCondition& value)
        {
            return target.hasState(value.state);
        },
        [&](const TargetHasStateFromEffectOwnerCondition& value)
        {
            return target.hasStateFromSource(
                value.state,
                context.header.binding.ownerUnitId);
        },
        [&](const SourceStackAtLeastCondition& value)
        {
            return context.header.owner.stackCount(value.stack) >= value.count;
        },
        [&](const OtherLivingAllyUsesMagicCondition& value)
        {
            return std::ranges::any_of(context.header.battle.units(), [&](const EffectUnitSnapshot& unit)
            {
                return unit.alive && unit.id != context.header.owner.id &&
                       unit.team == context.header.owner.team && unit.usesMagic(value.magicId);
            });
        },
        [&](const CastDistinctTargetCountAtLeastCondition& value)
        {
            const auto* aggregate = std::get_if<CastAggregateEventData>(&context.payload);
            return aggregate && static_cast<int>(aggregate->aggregate.distinctHitUnitIds.size()) >= value.count;
        },
        [&](const AttackOrdinalEqualsCondition& value)
        {
            const auto* attack = attackProvenance(context);
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
            return !value.excludeReflected
                || damage->damageKind != BattleDamageKind::Reflected;
        },
        [&](const EventTargetBelongsToBoundSourceCondition&)
        {
            if (context.header.binding.kind != EffectSourceKind::Combo)
            {
                return false;
            }
            const auto* target = transactionTargetSnapshot(context);
            return target
                && target->comboIds.contains(context.header.binding.sourceId);
        },
        [&](const DamagePerspectiveCondition& value)
        {
            const auto* damage = std::get_if<DamageResultEventData>(&context.payload);
            if (!damage)
            {
                return false;
            }
            return value.perspective == DamagePerspective::Dealt
                ? damage->attackerBefore && damage->attackerBefore->id == context.header.owner.id
                : damage->defenderBefore.id == context.header.owner.id;
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
    if (const auto* attack = attackProvenance(context))
    {
        return attack->propagation;
    }
    if (const auto* cast = castProvenance(context))
    {
        return cast->propagation;
    }
    return std::nullopt;
}

bool isOwnerObservation(const BoundEffectRule& bound,
                        const EffectEventContext& context)
{
    if (const auto* hit = std::get_if<HitEventData>(&context.payload);
        hit && hit->defenderBefore.id == bound.binding.ownerUnitId)
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

bool ruleObservesEvent(const BoundEffectRule& bound,
                       const EffectEventContext& context)
{
    switch (bound.rule.observation)
    {
    case EffectObservationScope::Owner:
        return bound.binding.ownerUnitId < 0
            || bound.binding.ownerUnitId == context.header.owner.id;
    case EffectObservationScope::OwnerTeamEventSource:
    {
        const auto* source = sourceSnapshot(context);
        return source && source->team == bound.binding.sourceTeam;
    }
    case EffectObservationScope::EventTarget:
    {
        const auto* target = transactionTargetSnapshot(context);
        return target && target->id == bound.binding.ownerUnitId;
    }
    }
    assert(false);
    return false;
}

void rewriteObservedOwner(const BoundEffectRule& bound,
                          EffectEventContext& context)
{
    if (bound.rule.observation == EffectObservationScope::Owner)
    {
        return;
    }
    const auto* owner = context.header.battle.findUnit(bound.binding.ownerUnitId);
    if (!owner)
    {
        throw std::logic_error("觀察規則的效果擁有者不存在");
    }
    context.header.owner = *owner;
}

bool castScopeMatches(const BoundEffectRule& bound,
                      const EffectEventContext& context)
{
    if (!bound.castScope)
    {
        return true;
    }
    const auto* cast = castProvenance(context);
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
                         const BoundEffectRule& bound)
{
    assert(bound.castScope);
    assert(cast.castId == *bound.castScope);
    cast.magicId = bound.binding.sourceId;
    cast.origin = CastOriginKind::BorrowedEffect;
    cast.propagation = bound.scopedPropagation;
}

void rewriteBorrowedAttack(BattleAttackProvenance& attack,
                           const BoundEffectRule& bound)
{
    rewriteBorrowedCast(attack.cast, bound);
    attack.propagation = bound.scopedPropagation;
}

void rewriteScopedRuleContext(const BoundEffectRule& bound,
                              EffectEventContext& context)
{
    context.header.binding = bound.binding;
    if (!bound.castScope)
    {
        return;
    }
    const auto* cast = castProvenance(context);
    if (!cast || cast->castId != *bound.castScope)
    {
        return;
    }

    const auto rewriteOrigin = [&](EffectDamageOrigin& origin)
    {
        if (auto* attack = std::get_if<EffectAttackDamageOrigin>(&origin))
        {
            rewriteBorrowedAttack(attack->provenance, bound);
        }
    };
    std::visit(Overloaded{
        [&](CastPlanEventData& data) { rewriteBorrowedCast(data.provenance, bound); },
        [&](CastCommitEventData& data) { rewriteBorrowedCast(data.provenance, bound); },
        [&](AttackEventData& data) { rewriteBorrowedAttack(data.provenance, bound); },
        [&](HitEventData& data) { rewriteBorrowedAttack(data.provenance, bound); },
        [&](CastAggregateEventData& data) { rewriteBorrowedCast(data.provenance, bound); },
        [&](DamageResultEventData& data) { rewriteOrigin(data.origin); },
        [&](ShieldBreakEventData& data) { rewriteOrigin(data.cause); },
        [&](DeathEventData& data) { rewriteOrigin(data.cause); },
        [](auto&) {},
    }, context.payload);
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

bool ruleAllowedByPropagation(const BoundEffectRule& bound,
                              const EffectEventContext& context)
{
    const auto policy = propagationPolicy(context);
    if (!policy)
    {
        return true;
    }
    if (isOwnerObservation(bound, context))
    {
        return true;
    }
    if (bound.rule.observation != EffectObservationScope::Owner)
    {
        return *policy != CastPropagationPolicy::NoEffectRules;
    }
    switch (*policy)
    {
    case CastPropagationPolicy::SourceRules:
        return true;
    case CastPropagationPolicy::SourceHitRulesOnly:
        return isHitRuleEvent(context.event);
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

bool ruleMatchesMagicCast(const BoundEffectRule& bound,
                          const EffectEventContext& context)
{
    if (bound.binding.kind != EffectSourceKind::Magic)
    {
        return true;
    }
    const auto* cast = castProvenance(context);
    if (!cast)
    {
        return true;
    }
    if (bound.rule.observation != EffectObservationScope::Owner)
    {
        return true;
    }
    if (isOwnerObservation(bound, context))
    {
        // Defender/death observation rules subscribe to the bound owner, not
        // to the incoming attacker's selected magic.
        return true;
    }
    if (bound.rule.castMatch == EffectCastMatch::OwnerAnyCast)
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
        [](const RemoveStatusAction&) { return std::optional{ BorrowedRuleActionCategory::Status }; },
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

bool copiedMagicMatchesFilter(
    const BattleEffectRuleStore& store,
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
            if (std::ranges::any_of(store.rules(), [&](const auto& bound)
                {
                    return !bound.castScope
                        && bound.binding.kind == EffectSourceKind::Magic
                        && bound.binding.ownerUnitId == unit.id
                        && bound.binding.sourceId == unit.ultimateMagicId
                        && std::ranges::any_of(
                            bound.rule.actions,
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
    const auto* cast = castProvenance(context);
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
    case BattleDamageKind::Reflected:
        return DamageChannel::Reflected;
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

std::int64_t percentOf(std::int64_t value, int percent)
{
    return roundRatio(
        saturatingMultiply(value, percent),
        100,
        EffectRounding::TowardZero);
}

struct CommandEmitter
{
    BattleEffectRuleStore& store;
    const BoundEffectRule& bound;
    const EffectEventContext& context;
    BattleRuntimeRandom& random;
    std::vector<EffectCommand>& commands;
    std::uint64_t& nextCommandOrdinal;

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
        stateContext.header.formulaInputs.storedStateValue = store.stateValue(
            bound.binding,
            *number.stateSlot,
            stateScopeId(*number.stateSlot, context));
        return BattleEffectSystem::evaluateNumber(number, stateContext, target);
    }

    void emit(const EffectAction& effectAction,
              const EffectUnitSnapshot& target,
              std::uint32_t actionOrder,
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
            for (const auto& nested : branch)
            {
                emit(nested, target, actionOrder, targetOrder);
            }
            return;
        }

        auto metadata = EffectCommandMetadata{
            .binding = bound.binding,
            .ruleId = bound.rule.id,
            .event = context.event,
            .ruleOrder = bound.order,
            .actionOrder = actionOrder,
            .targetOrder = targetOrder,
            .commandOrdinal = nextCommandOrdinal++,
            .targetUnitId = target.id,
            .eventSourceUnitId = sourceUnitId(context).value_or(-1),
        };

        std::visit(Overloaded{
            [&](const ModifyAttributeAction& action)
            {
                commands.push_back({ metadata, ModifyAttributeEffectCommand{
                    action,
                    evaluate(action.amount, target),
                } });
            },
            [&](const ModifyDamageAction& action)
            {
                commands.push_back({ metadata, ModifyDamageEffectCommand{
                    action,
                    evaluate(action.amount, target),
                } });
            },
            [&](const ChangeResourceAction& action)
            {
                auto destinations = action.transferDestination
                    ? BattleEffectSystem::selectTargets(*action.transferDestination, context, random)
                    : std::vector<int>{};
                commands.push_back({ metadata, ChangeResourceEffectCommand{
                    action,
                    evaluate(action.amount, target),
                    std::move(destinations),
                } });
            },
            [&](const ModifyHealTransactionAction& action)
            {
                commands.push_back({ metadata, ModifyHealTransactionEffectCommand{ action } });
            },
            [&](const ApplyStatusAction& action)
            {
                const int applicationCount = action.applicationCount
                    ? evaluate(*action.applicationCount, target)
                    : 1;
                assert(applicationCount > 0);
                for (int application = 0; application < applicationCount; ++application)
                {
                    auto applicationMetadata = metadata;
                    if (application > 0)
                    {
                        applicationMetadata.commandOrdinal = nextCommandOrdinal++;
                    }
                    commands.push_back({ applicationMetadata, ApplyStatusEffectCommand{
                        action,
                        evaluate(action.potency, target),
                        evaluate(action.secondaryPotency, target),
                        action.duration
                            ? std::optional<int>{ evaluate(*action.duration, target) }
                            : std::nullopt,
                    } });
                }
            },
            [&](const ConsumeStatusAction& action)
            {
                std::optional<ApplyStatusEffectCommand> whenDepleted;
                if (action.whenDepleted)
                {
                    whenDepleted = ApplyStatusEffectCommand{
                        *action.whenDepleted,
                        evaluate(action.whenDepleted->potency, target),
                        evaluate(action.whenDepleted->secondaryPotency, target),
                        action.whenDepleted->duration
                            ? std::optional<int>{ evaluate(*action.whenDepleted->duration, target) }
                            : std::nullopt,
                    };
                }
                commands.push_back({ metadata, ConsumeStatusEffectCommand{
                    action,
                    std::move(whenDepleted),
                } });
            },
            [&](const RemoveStatusAction& action)
            {
                commands.push_back({ metadata, RemoveStatusEffectCommand{ action } });
            },
            [&](const DealDamageAction& action)
            {
                commands.push_back({ metadata, DealDamageEffectCommand{
                    action,
                    evaluate(action.amount, target),
                    action.transactionCount
                        ? evaluate(*action.transactionCount, target)
                        : 1,
                } });
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
                    };
                }
                commands.push_back({ metadata, ModifyAttackEffectCommand{
                    action,
                    action.damageOverride
                        ? std::optional<int>{ evaluate(*action.damageOverride, target) }
                        : std::nullopt,
                    source,
                } });
            },
            [&](const ForceMoveAction& action)
            {
                commands.push_back({ metadata, ForceMoveEffectCommand{ action } });
            },
            [&](const CreateAreaAction& action)
            {
                std::vector<int> amounts;
                amounts.reserve(action.modifiers.size());
                for (const auto& modifier : action.modifiers)
                {
                    amounts.push_back(evaluate(modifier.amount, target));
                }
                commands.push_back({ metadata, CreateAreaEffectCommand{ action, std::move(amounts) } });
            },
            [&](const ModifyCastAction& action)
            {
                commands.push_back({ metadata, ModifyCastEffectCommand{
                    action,
                    action.mpCost
                        ? std::optional<int>{ evaluate(*action.mpCost, target) }
                        : std::nullopt,
                } });
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

                StateMachineEffectCommand command;
                std::optional<ChangeResourceEffectCommand> routedResourceCommand;
                command.action = action;
                std::visit(Overloaded{
                    [&](const ChangeStateValueAction& value)
                    {
                        const auto scope = stateScopeId(value.slot, context);
                        command.stateValueBefore = store.stateValue(bound.binding, value.slot, scope);
                        command.stateValueAfter = saturatingAdd(
                            command.stateValueBefore,
                            value.delta);
                        if (value.minimum)
                        {
                            command.stateValueAfter = std::max(
                                command.stateValueAfter,
                                *value.minimum);
                        }
                        if (value.maximum)
                        {
                            command.stateValueAfter = std::min(
                                command.stateValueAfter,
                                *value.maximum);
                        }
                        command.outputValue = command.stateValueAfter;
                        store.setStateValue(
                            bound.binding,
                            value.slot,
                            command.stateValueAfter,
                            scope);
                    },
                    [&](const TransferStateValueAction& value)
                    {
                        const auto sourceScope = stateScopeId(value.sourceSlot, context);
                        const auto destinationScope = stateScopeId(
                            value.destinationSlot,
                            context);
                        command.stateValueBefore = store.stateValue(
                            bound.binding,
                            value.sourceSlot,
                            sourceScope);
                        command.stateValueAfter = 0;
                        command.outputValue = command.stateValueBefore;
                        store.setStateValue(
                            bound.binding,
                            value.sourceSlot,
                            0,
                            sourceScope);
                        store.setStateValue(
                            bound.binding,
                            value.destinationSlot,
                            command.outputValue,
                            destinationScope);
                    },
                    [&](const RecordMaximumDamageAction& value)
                    {
                        const auto scope = stateScopeId(value.slot, context);
                        command.stateValueBefore = store.stateValue(bound.binding, value.slot, scope);
                        const auto observed = channelMatches(value.channel, currentDamageChannel(context))
                            ? finalHpDamage(context).value_or(0)
                            : 0;
                        command.stateValueAfter = std::max(command.stateValueBefore,
                                                           static_cast<std::int64_t>(observed));
                        command.outputValue = observed;
                        store.setStateValue(bound.binding, value.slot, command.stateValueAfter, scope);
                    },
                    [&](const ConsumeRecordedMaximumAction& value)
                    {
                        const auto scope = stateScopeId(value.slot, context);
                        command.stateValueBefore = store.stateValue(bound.binding, value.slot, scope);
                        command.outputValue = percentOf(command.stateValueBefore, value.percent);
                        command.stateValueAfter = value.clearAfterConsume ? 0 : command.stateValueBefore;
                        store.setStateValue(bound.binding, value.slot, command.stateValueAfter, scope);
                        if (value.destination == StateValueDestination::ShieldAmount
                            && command.outputValue > 0)
                        {
                            assert(command.outputValue <= std::numeric_limits<int>::max());
                            const int amount = static_cast<int>(command.outputValue);
                            ChangeResourceAction grantShield;
                            grantShield.resource = BattleResource::Shield;
                            grantShield.kind = ResourceChangeKind::Grant;
                            grantShield.amount.flat = amount;
                            routedResourceCommand = ChangeResourceEffectCommand{
                                .action = std::move(grantShield),
                                .amount = amount,
                            };
                        }
                    },
                    [&](const StartDamageAbsorptionAction& value)
                    {
                        const auto scope = stateScopeId(value.slot, context);
                        command.stateValueBefore = store.stateValue(bound.binding, value.slot, scope);
                        command.stateValueAfter = 0;
                        store.setStateValue(bound.binding, value.slot, 0, scope);
                    },
                    [&](const SettleDamageAbsorptionAction& value)
                    {
                        const auto scope = stateScopeId(value.slot, context);
                        command.stateValueBefore = store.stateValue(bound.binding, value.slot, scope);
                        command.outputValue = percentOf(command.stateValueBefore, value.returnedPct);
                        command.stateValueAfter = value.clearAfterSettle ? 0 : command.stateValueBefore;
                        store.setStateValue(bound.binding, value.slot, command.stateValueAfter, scope);
                        command.selectedSourceUnitIds = BattleEffectSystem::selectTargets(value.target, context, random);
                    },
                    [&](const BorrowEffectRulesAction& value)
                    {
                        command.selectedSourceUnitIds = BattleEffectSystem::selectTargets(value.sourceUnits, context, random);
                        const auto desired = std::max(0, evaluate(value.sourceCount, target));
                        if (static_cast<int>(command.selectedSourceUnitIds.size()) > desired)
                        {
                            command.selectedSourceUnitIds.resize(static_cast<std::size_t>(desired));
                        }
                        command.outputValue = static_cast<std::int64_t>(command.selectedSourceUnitIds.size());
                    },
                    [&](const CopyAttackDefinitionAction& value)
                    {
                        command.selectedSourceUnitIds = BattleEffectSystem::selectTargets(
                            value.sourceUnits,
                            context,
                            random,
                            [&](const EffectUnitSnapshot& candidate)
                            {
                                return copiedMagicMatchesFilter(
                                    store,
                                    candidate,
                                    value.filter);
                            });
                        if (static_cast<int>(command.selectedSourceUnitIds.size()) > value.copyCount)
                        {
                            command.selectedSourceUnitIds.resize(static_cast<std::size_t>(value.copyCount));
                        }
                        command.outputValue = static_cast<std::int64_t>(command.selectedSourceUnitIds.size());
                    },
                    [&](const SettleRemainingStatusDamageAction&)
                    {
                    },
                    [&](const GenerateClonesAction& value)
                    {
                        command.outputValue = value.count;
                    },
                    [&](const PreventDeathAction& value)
                    {
                        command.outputValue = value.invincibilityFrames;
                    },
                    [&](const ConfigureRescueRepositionAction& value)
                    {
                        command.outputValue = value.activations;
                    },
                }, action);
                if (routedResourceCommand)
                {
                    commands.push_back({ metadata, std::move(*routedResourceCommand) });
                }
                else
                {
                    commands.push_back({ metadata, std::move(command) });
                }
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

bool EffectUnitSnapshot::hasState(const std::string& state) const
{
    return states.contains(state) || stackCount(state) > 0;
}

bool EffectUnitSnapshot::hasStateFromSource(const std::string& state,
                                            int sourceUnitId) const
{
    return std::ranges::any_of(statusDetails, [&](const auto& status)
    {
        return status.state == state
            && status.sourceUnitId == sourceUnitId
            && status.stacks > 0;
    });
}

int EffectUnitSnapshot::stackCount(const std::string& stack) const
{
    const auto it = stacks.find(stack);
    return it != stacks.end() ? it->second : 0;
}

int EffectUnitSnapshot::statusPotency(const std::string& state) const
{
    const auto status = std::ranges::find(statusDetails, state, &EffectStatusSnapshot::state);
    return status != statusDetails.end() ? status->potency : 0;
}

bool EffectUnitSnapshot::usesMagic(int magicId) const
{
    return magicIds.contains(magicId);
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
    runtimeByRule_.clear();
    sharedCooldownUntilFrame_.clear();
    activationEvaluations_.clear();
    stateValues_.clear();
    blinkAttackWeakestTargetByOwner_.clear();
    nextRuntimeInstanceId_ = 1;
}

std::size_t BattleEffectRuleStore::append(EffectSourceBinding binding, const EffectRule& rule)
{
    std::string validationError;
    if (!validateEffectRule(rule, validationError))
    {
        throw std::invalid_argument(validationError);
    }
    if (rule.castMatch == EffectCastMatch::OwnerAnyCast
        && binding.kind != EffectSourceKind::Magic)
    {
        throw std::invalid_argument("效果擁有者任意施放匹配只支援武功效果來源");
    }
    const auto key = runtimeKey(binding, rule.id);
    if (runtimeByRule_.contains(key))
    {
        throw std::invalid_argument("同一效果來源不可有重複的 EffectRuleId");
    }

    const auto index = rules_.size();
    rules_.push_back({ binding, rule, static_cast<std::uint32_t>(index) });
    ruleIndicesByEvent_[static_cast<std::size_t>(rule.event)].push_back(index);
    runtimeByRule_.emplace(key, EffectRuleRuntimeState{
        .intervalFramesRemaining = rule.intervalFrames,
    });
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
            || bound.rule.event == EffectEvent::BattleInitialized
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
        append(binding, bound.rule);
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
            || bound.rule.event == EffectEvent::BattleInitialized
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
        append(binding, bound.rule);
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
                || !ruleAllowedByBorrowFilter(bound.rule, filter))
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
                .rule = source.rule,
                .order = static_cast<std::uint32_t>(index),
                .castScope = castId,
                .scopedPropagation = propagation,
            });
            ruleIndicesByEvent_[static_cast<std::size_t>(source.rule.event)].push_back(index);
            runtimeByRule_.emplace(
                runtimeKey(binding, source.rule.id),
                EffectRuleRuntimeState{
                    .intervalFramesRemaining = source.rule.intervalFrames,
                });
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
    if (removedInstances.empty())
    {
        return;
    }

    std::erase_if(rules_, [&](const BoundEffectRule& bound)
    {
        return bound.castScope == castId;
    });
    std::erase_if(runtimeByRule_, [&](const auto& entry)
    {
        return removedInstances.contains(entry.first.sourceInstanceId);
    });
    std::erase_if(activationEvaluations_, [&](const auto& entry)
    {
        return entry.first.castId == castId
            || removedInstances.contains(entry.first.rule.sourceInstanceId);
    });
    std::erase_if(stateValues_, [&](const auto& entry)
    {
        return removedInstances.contains(entry.first.sourceInstanceId);
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
    for (auto& indices : ruleIndicesByEvent_)
    {
        indices.clear();
    }
    for (std::size_t index = 0; index < rules_.size(); ++index)
    {
        auto& bound = rules_[index];
        bound.order = static_cast<std::uint32_t>(index);
        ruleIndicesByEvent_[static_cast<std::size_t>(bound.rule.event)].push_back(index);
    }
}

const EffectRuleRuntimeState& BattleEffectRuleStore::runtime(
    const EffectSourceBinding& binding,
    EffectRuleId ruleId) const
{
    return runtimeByRule_.at(runtimeKey(binding, ruleId));
}

int BattleEffectRuleStore::activationCount(const EffectSourceBinding& binding,
                                           EffectRuleId ruleId) const
{
    const auto it = runtimeByRule_.find(runtimeKey(binding, ruleId));
    return it != runtimeByRule_.end() ? it->second.activationCount : 0;
}

int BattleEffectRuleStore::activationEvaluationCount(const EffectSourceBinding& binding,
                                                      EffectRuleId ruleId,
                                                      BattleCastId castId,
                                                      int targetUnitId) const
{
    const auto it = activationEvaluations_.find(
        activationScopeKey(binding, ruleId, castId, targetUnitId));
    return it != activationEvaluations_.end() ? it->second : 0;
}

bool BattleEffectRuleStore::canActivateRuntimeRule(
    const EffectSourceBinding& binding,
    EffectRuleId ruleId,
    int frame) const
{
    assert(frame >= 0);
    const auto runtimeIt = runtimeByRule_.find(runtimeKey(binding, ruleId));
    if (runtimeIt == runtimeByRule_.end())
    {
        return false;
    }
    const auto bound = std::ranges::find_if(rules_, [&](const BoundEffectRule& candidate)
    {
        return runtimeKey(candidate.binding, candidate.rule.id) == runtimeKey(binding, ruleId);
    });
    if (bound == rules_.end())
    {
        return false;
    }
    if (bound->rule.maxActivations > 0
        && runtimeIt->second.activationCount >= bound->rule.maxActivations)
    {
        return false;
    }
    if (bound->rule.sharedCooldownFrames <= 0)
    {
        return true;
    }
    const auto cooldown = sharedCooldownUntilFrame_.find(
        sharedCooldownKey(binding, ruleId));
    return cooldown == sharedCooldownUntilFrame_.end()
        || frame >= cooldown->second;
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
    const auto bound = std::ranges::find_if(rules_, [&](const BoundEffectRule& candidate)
    {
        return runtimeKey(candidate.binding, candidate.rule.id) == runtimeKey(binding, ruleId);
    });
    assert(bound != rules_.end());
    if (!random.chance(bound->rule.chancePct))
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
    auto& runtime = runtimeByRule_.at(runtimeKey(binding, ruleId));
    ++runtime.activationCount;
    const auto bound = std::ranges::find_if(rules_, [&](const BoundEffectRule& candidate)
    {
        return runtimeKey(candidate.binding, candidate.rule.id) == runtimeKey(binding, ruleId);
    });
    assert(bound != rules_.end());
    if (bound->rule.sharedCooldownFrames > 0)
    {
        sharedCooldownUntilFrame_[sharedCooldownKey(binding, ruleId)] =
            static_cast<std::int64_t>(frame) + bound->rule.sharedCooldownFrames;
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
    const auto it = stateValues_.find(stateKey(binding, slot, scopeId));
    return it != stateValues_.end() ? it->second : 0;
}

void BattleEffectRuleStore::setStateValue(const EffectSourceBinding& binding,
                                          EffectStateSlot slot,
                                          std::int64_t value,
                                          std::uint64_t scopeId)
{
    stateValues_[stateKey(binding, slot, scopeId)] = value;
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
        switch (base)
        {
        case EffectNumberBase::Constant: return 0;
        case EffectNumberBase::SourceStar: return context.header.owner.star;
        case EffectNumberBase::SourceAttack: return context.header.owner.attack;
        case EffectNumberBase::SourceMaxHp: return context.header.owner.maxHp;
        case EffectNumberBase::SourceMissingHpRatio:
            assert(context.header.owner.maxHp > 0);
            return std::clamp(
                context.header.owner.maxHp - context.header.owner.hp,
                0,
                context.header.owner.maxHp);
        case EffectNumberBase::SourceCurrentMpRatio:
            assert(context.header.owner.maxMp > 0);
            return std::clamp(
                context.header.owner.mp,
                0,
                context.header.owner.maxMp);
        case EffectNumberBase::TargetMaxHp: return target.maxHp;
        case EffectNumberBase::TargetCurrentHp: return target.hp;
        case EffectNumberBase::TargetCurrentShield: return target.shield;
        case EffectNumberBase::TargetCurrentCooldown: return target.activeCooldown;
        case EffectNumberBase::FinalHpDamage:
            if (const auto value = finalHpDamage(context))
            {
                return *value;
            }
            throw std::logic_error("實際生命傷害公式需要 DamageResolved payload");
        case EffectNumberBase::AccumulatedStateValue:
            if (context.header.formulaInputs.accumulatedStateValue)
            {
                return *context.header.formulaInputs.accumulatedStateValue;
            }
            throw std::logic_error("累積狀態公式缺少 typed input");
        case EffectNumberBase::SourceStatusPotency:
            return context.header.owner.statusPotency(number.status);
        case EffectNumberBase::SourceStatusStacks:
            return context.header.owner.stackCount(number.status);
        case EffectNumberBase::StoredStateValue:
            if (context.header.formulaInputs.storedStateValue)
            {
                return *context.header.formulaInputs.storedStateValue;
            }
            throw std::logic_error("狀態槽公式缺少 typed input");
        }
        throw std::logic_error("未知 EffectNumberBase");
    };

    const auto baseDenominator = [&](EffectNumberBase base) -> std::int64_t
    {
        if (base == EffectNumberBase::SourceMissingHpRatio)
        {
            assert(context.header.owner.maxHp > 0);
            return context.header.owner.maxHp;
        }
        if (base == EffectNumberBase::SourceCurrentMpRatio)
        {
            assert(context.header.owner.maxMp > 0);
            return context.header.owner.maxMp;
        }
        return 1;
    };

    std::int64_t value{};
    value = baseValue(number.base);
    auto denominator = baseDenominator(number.base);
    if (number.multiplierBase)
    {
        value = saturatingMultiply(value, baseValue(*number.multiplierBase));
        denominator = saturatingMultiply(
            denominator,
            baseDenominator(*number.multiplierBase));
    }
    value = roundRatio(
        saturatingMultiply(value, number.percent),
        saturatingMultiply(denominator, 100),
        number.rounding);
    value = saturatingAdd(value, number.flat);
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

std::vector<int> BattleEffectSystem::selectTargets(const EffectSelector& selector,
                                                   const EffectEventContext& context,
                                                   BattleRuntimeRandom& random,
                                                   const std::function<bool(
                                                       const EffectUnitSnapshot&)>& candidateFilter)
{
    std::vector<const EffectUnitSnapshot*> candidates;
    SelectorOrdering ordering = SelectorOrdering::UnitId;
    const Pointf center = selector.kind == EffectSelectorKind::NearestEnemies ||
                                  selector.kind == EffectSelectorKind::FarthestEnemy
        ? context.header.owner.position
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
            candidates.push_back(unit);
        }
    };

    switch (selector.kind)
    {
    case EffectSelectorKind::Self:
        addDirect(&context.header.owner);
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
    case EffectSelectorKind::AlliesUsingWeapon:
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

    std::vector<int> result;
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

namespace
{

void aggregateEventStatusPotency(std::vector<EffectCommand>& commands)
{
    std::vector<EffectCommand> aggregated;
    aggregated.reserve(commands.size());
    for (auto& command : commands)
    {
        auto* status = std::get_if<ApplyStatusEffectCommand>(&command.value);
        if (!status || !status->action.aggregatePotencyWithinEvent)
        {
            aggregated.push_back(std::move(command));
            continue;
        }

        assert(status->action.status == BattleStatusKind::Poison);
        assert(status->action.stack == EffectStackPolicy::KeepStrongest);
        const auto sameAggregate = [&](EffectCommand& candidate)
        {
            const auto* existing = std::get_if<ApplyStatusEffectCommand>(&candidate.value);
            return existing
                && existing->action.aggregatePotencyWithinEvent
                && candidate.metadata.targetUnitId == command.metadata.targetUnitId
                && candidate.metadata.binding.ownerUnitId == command.metadata.binding.ownerUnitId;
        };
        const auto existingCommand = std::find_if(
            aggregated.begin(), aggregated.end(), sameAggregate);
        if (existingCommand == aggregated.end())
        {
            aggregated.push_back(std::move(command));
            continue;
        }

        auto& existing = std::get<ApplyStatusEffectCommand>(existingCommand->value);
        const auto combinedPotency = static_cast<long long>(existing.potency) + status->potency;
        assert(combinedPotency <= std::numeric_limits<int>::max());
        existing.potency = static_cast<int>(combinedPotency);
        const int existingDuration = existing.evaluatedDurationFrames.value_or(
            existing.action.durationFrames);
        const int addedDuration = status->evaluatedDurationFrames.value_or(
            status->action.durationFrames);
        existing.evaluatedDurationFrames = std::max(existingDuration, addedDuration);
        existing.action.stacks = std::max(
            existing.action.stacks,
            status->action.stacks);
        assert(existing.action.stackLimit);
        assert(status->action.stackLimit);
        existing.action.stackLimit = std::max(
            *existing.action.stackLimit,
            *status->action.stackLimit);
    }

    for (auto& command : aggregated)
    {
        if (auto* status = std::get_if<ApplyStatusEffectCommand>(&command.value);
            status && status->action.aggregatePotencyWithinEvent)
        {
            status->action.aggregatePotencyWithinEvent = false;
        }
    }
    commands = std::move(aggregated);
}

}  // namespace

std::vector<EffectExactRuntimeRuleMatch> BattleEffectSystem::queryExactRuntimeRules(
    const BattleEffectRuleStore& store,
    const EffectEventContext& context,
    BattleRuntimeRandom& random) const
{
    if (!eventPayloadMatches(context))
    {
        throw std::invalid_argument("EffectEvent 與 typed payload 不相符");
    }

    const auto& indices = store.ruleIndicesByEvent_[static_cast<std::size_t>(context.event)];
    const auto ordered = orderedBoundRules(store.rules_, indices);

    std::vector<EffectExactRuntimeRuleMatch> result;
    for (const auto* bound : ordered)
    {
        if (!requiresExactRuntimePhaseQuery(bound->rule)
            || !ruleObservesEvent(*bound, context)
            || !castScopeMatches(*bound, context))
        {
            continue;
        }

        auto ruleContext = context;
        rewriteObservedOwner(*bound, ruleContext);
        rewriteScopedRuleContext(*bound, ruleContext);
        if (!ruleAllowedByPropagation(*bound, ruleContext)
            || !ruleMatchesMagicCast(*bound, ruleContext)
            || !store.canActivateRuntimeRule(
                bound->binding,
                bound->rule.id,
                context.header.frame))
        {
            continue;
        }

        const auto selectedIds = selectTargets(bound->rule.selector, ruleContext, random);
        EffectExactRuntimeRuleMatch match{ .bound = bound };
        match.targetUnitIds.reserve(selectedIds.size());
        for (const auto unitId : selectedIds)
        {
            const auto* target = snapshotForTarget(ruleContext, unitId);
            if (!target)
            {
                throw std::logic_error("selector 傳回了 read view 中不存在的單位");
            }
            if (conditionsSatisfied(bound->rule.conditions, ruleContext, *target, true))
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

BattleEffectDispatchResult BattleEffectSystem::dispatchRuleIndices(
    BattleEffectRuleStore& store,
    const EffectEventContext& context,
    BattleRuntimeRandom& random,
    std::span<const std::size_t> ruleIndices) const
{
    if (!eventPayloadMatches(context))
    {
        throw std::invalid_argument("EffectEvent 與 typed payload 不相符");
    }

    BattleEffectDispatchResult result;
    std::uint64_t nextCommandOrdinal{};
    for (const auto* boundRule : orderedBoundRules(store.rules_, ruleIndices))
    {
        const auto& bound = *boundRule;
        if (bound.rule.event != context.event
            || !ruleObservesEvent(bound, context)
            || !castScopeMatches(bound, context))
        {
            continue;
        }

        auto ruleContext = context;
        rewriteObservedOwner(bound, ruleContext);
        rewriteScopedRuleContext(bound, ruleContext);
        if (!ruleAllowedByPropagation(bound, ruleContext)
            || !ruleMatchesMagicCast(bound, ruleContext))
        {
            continue;
        }
        if (requiresExactRuntimePhaseQuery(bound.rule))
        {
            continue;
        }

        auto& runtime = store.runtimeByRule_.at(runtimeKey(bound.binding, bound.rule.id));
        if (bound.rule.maxActivations > 0 && runtime.activationCount >= bound.rule.maxActivations)
        {
            continue;
        }
        if (bound.rule.sharedCooldownFrames > 0)
        {
            const auto cooldown = store.sharedCooldownUntilFrame_.find(
                sharedCooldownKey(bound.binding, bound.rule.id));
            if (cooldown != store.sharedCooldownUntilFrame_.end()
                && context.header.frame < cooldown->second)
            {
                continue;
            }
        }
        if (bound.rule.intervalFrames > 0)
        {
            assert(context.event == EffectEvent::FrameAdvanced);
            const auto& tick = std::get<FrameTickEventData>(context.payload);
            assert(tick.deltaFrames > 0);
            assert(runtime.intervalFramesRemaining > 0);
            runtime.intervalFramesRemaining -= tick.deltaFrames;
            if (runtime.intervalFramesRemaining > 0)
            {
                continue;
            }
            runtime.intervalFramesRemaining = bound.rule.intervalFrames;
        }

        const auto selectedIds = selectTargets(bound.rule.selector, ruleContext, random);
        if (selectedIds.empty())
        {
            continue;
        }

        std::vector<const EffectUnitSnapshot*> eligibleTargets;
        eligibleTargets.reserve(selectedIds.size());
        for (const auto unitId : selectedIds)
        {
            const auto* target = snapshotForTarget(ruleContext, unitId);
            if (!target)
            {
                throw std::logic_error("selector 傳回了 read view 中不存在的單位");
            }
            if (conditionsSatisfied(bound.rule.conditions, ruleContext, *target, true))
            {
                eligibleTargets.push_back(target);
            }
        }
        if (eligibleTargets.empty())
        {
            continue;
        }
        if (bound.rule.everyNthEvent > 0)
        {
            ++runtime.eligibleEventCount;
            if (runtime.eligibleEventCount % bound.rule.everyNthEvent != 0)
            {
                continue;
            }
        }

        std::vector<const EffectUnitSnapshot*> activationTargets;
        if (bound.rule.activationLimit)
        {
            const auto* cast = castProvenance(ruleContext);
            if (!cast)
            {
                throw std::logic_error("施放範圍觸發限制需要 cast provenance");
            }
            activationTargets.reserve(eligibleTargets.size());
            for (const auto* target : eligibleTargets)
            {
                switch (bound.rule.activationLimit->scope)
                {
                case EffectActivationScope::PerCastPerTarget:
                {
                    auto& evaluationCount = store.activationEvaluations_[activationScopeKey(
                        bound.binding,
                        bound.rule.id,
                        cast->castId,
                        target->id)];
                    if (evaluationCount >= bound.rule.activationLimit->maxEvaluations)
                    {
                        continue;
                    }
                    // 先佔用本次判定，機率失敗後同一施放不得重擲。
                    ++evaluationCount;
                    break;
                }
                }
                if (random.chance(bound.rule.chancePct))
                {
                    activationTargets.push_back(target);
                }
            }
        }
        else
        {
            if (!random.chance(bound.rule.chancePct))
            {
                continue;
            }
            activationTargets = eligibleTargets;
        }
        if (activationTargets.empty())
        {
            continue;
        }

        ++runtime.activationCount;
        if (bound.rule.sharedCooldownFrames > 0)
        {
            store.sharedCooldownUntilFrame_[sharedCooldownKey(
                bound.binding,
                bound.rule.id)] = static_cast<std::int64_t>(context.header.frame)
                    + bound.rule.sharedCooldownFrames;
        }
        EffectRuleActivation activation{
            .binding = bound.binding,
            .ruleId = bound.rule.id,
        };
        for (const auto* target : activationTargets)
        {
            activation.targetUnitIds.push_back(target->id);
        }
        result.activations.push_back(std::move(activation));

        CommandEmitter emitter{
            .store = store,
            .bound = bound,
            .context = ruleContext,
            .random = random,
            .commands = result.commands,
            .nextCommandOrdinal = nextCommandOrdinal,
        };
        for (std::uint32_t actionOrder = 0;
             actionOrder < static_cast<std::uint32_t>(bound.rule.actions.size());
             ++actionOrder)
        {
            for (std::uint32_t targetOrder = 0;
                 targetOrder < static_cast<std::uint32_t>(activationTargets.size());
                 ++targetOrder)
            {
                emitter.emit(bound.rule.actions[actionOrder],
                             *activationTargets[targetOrder],
                             actionOrder,
                             targetOrder);
            }
        }
    }
    if (context.event == EffectEvent::CastSettled)
    {
        const auto* cast = castProvenance(context);
        assert(cast);
        std::erase_if(store.activationEvaluations_, [&](const auto& entry)
        {
            return entry.first.castId == cast->castId;
        });
    }
    aggregateEventStatusPotency(result.commands);
    return result;
}

}  // namespace KysChess::Battle
