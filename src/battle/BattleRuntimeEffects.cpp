#include "BattleRuntimeEffects.h"

#include "BattleRuntimeUnits.h"
#include "BattleAreaEffectSystem.h"
#include "BattleDamageSystem.h"
#include "BattleMath.h"
#include "BattleStatusSystem.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <iterator>
#include <ranges>

namespace KysChess::Battle
{

namespace
{

const ChessMagicEffectDefinition* findMagicDefinition(
    std::span<const ChessMagicEffectDefinition> definitions,
    int magicId)
{
    const auto definition = std::ranges::find(
        definitions,
        magicId,
        &ChessMagicEffectDefinition::magicId);
    return definition != definitions.end() ? &*definition : nullptr;
}

void appendMagicId(std::pmr::vector<int>& magicIds, int magicId)
{
    if (magicId >= 0 && !std::ranges::contains(magicIds, magicId))
    {
        magicIds.push_back(magicId);
    }
}

EffectMartialCategory selectedMartialCategory(const BattleActionPlanSeed* actionPlan)
{
    if (!actionPlan || actionPlan->normalSkill.magicType <= 0)
    {
        return EffectMartialCategory::None;
    }
    switch (actionPlan->normalSkill.magicType)
    {
    case 1: return EffectMartialCategory::Fist;
    case 2: return EffectMartialCategory::Sword;
    case 3: return EffectMartialCategory::Knife;
    case 4: return EffectMartialCategory::Unusual;
    default: assert(false && "武功類別必須是1至4"); std::unreachable();
    }
}

void appendComboIds(const BattleComboRuntimeFacts& comboFacts, std::pmr::vector<int>& comboIds)
{
    comboIds.reserve(comboFacts.memberComboIds().size() + comboFacts.appliedComboIds().size());
    std::ranges::set_union(
        comboFacts.memberComboIds(),
        comboFacts.appliedComboIds(),
        std::back_inserter(comboIds));
}

bool isPersistentMagicRule(const BoundEffectRule& bound)
{
    return bound.binding.kind == EffectSourceKind::Magic
        && !bound.castScope;
}

void appendBoundMagicIds(
    const BattleRuntimeState& runtime,
    int ownerUnitId,
    std::pmr::vector<int>& magicIds)
{
    for (const auto& bound : runtime.effectRules.rules())
    {
        if (isPersistentMagicRule(bound)
            && bound.binding.ownerUnitId == ownerUnitId)
        {
            appendMagicId(magicIds, bound.binding.sourceId);
        }
    }
    std::ranges::sort(magicIds);
}

void populateEffectStatusSnapshot(
    EffectUnitSnapshot& result,
    const BattleStatusEffectState& effects)
{
    result.statusDetails.clear();
    result.statusDetails.reserve(effects.statuses.size());
    result.statusShield = effects.statusShield;
    result.staggerShield = effects.staggerShield;
    for (const auto& instance : effects.statuses)
    {
        assert(instance.stacks > 0);
        result.statusDetails.push_back({
            .state = instance.kind,
            .sourceUnitId = instance.sourceUnitId,
            .producerBinding = instance.producer
                ? std::optional{ instance.producer->binding }
                : std::nullopt,
            .appliedSequence = instance.appliedSequence,
            .stacks = instance.stacks,
        });
    }
}


}  // namespace

int effectAdjustedAttribute(
    const BattleRuntimeState& runtime,
    int unitId,
    BattleAttribute attribute,
    int baseValue,
    int eventSourceUnitId)
{
    const int adjusted = BattleEffectCommandSystem::queryAttribute(
        runtime,
        {
            .unitId = unitId,
            .attribute = attribute,
            .baseValue = baseValue,
            .frame = runtime.movement.frame,
            .eventSourceUnitId = eventSourceUnitId,
        });
    // 攻防減益可以超過基礎值；完整結算修正後，實際攻防最低為零。
    return attribute == BattleAttribute::Attack || attribute == BattleAttribute::Defence
        ? std::max(0, adjusted) : adjusted;
}

int areaAttributeDelta(
    const BattleRuntimeState& runtime,
    int unitId,
    BattleAttribute attribute)
{
    return BattleAreaEffectSystem::attributeDelta(
        runtime.areas,
        runtime.gridTransform,
        runtime.units,
        unitId,
        runtime.movement.frame,
        attribute);
}

int areaAdjustedSpeed(const BattleRuntimeState& state, int unitId, int baseSpeed)
{
    const auto factor = std::max<std::int64_t>(
        0,
        static_cast<std::int64_t>(100)
            + areaAttributeDelta(state, unitId, BattleAttribute::Speed));
    return std::max(0, battleSaturatedInt(
        static_cast<std::int64_t>(baseSpeed) * factor / 100));
}

int effectAndAreaAdjustedRateAttribute(
    const BattleRuntimeState& state,
    int unitId,
    BattleAttribute attribute,
    int baseValue)
{
    return effectAdjustedAttribute(state, unitId, attribute, baseValue)
        + areaAttributeDelta(state, unitId, attribute);
}

int effectAndAreaAdjustedSpeed(const BattleRuntimeState& state, int unitId, int baseSpeed)
{
    const auto status = BattleStatusSystem({}).persistentModifiers(
        state.units.require(unitId).status.effects);
    const int effectAdjusted = effectAdjustedAttribute(
        state, unitId, BattleAttribute::Speed, baseSpeed);
    const auto statusFactor = std::max<std::int64_t>(
        0,
        static_cast<std::int64_t>(100) + status.speedPctDelta);
    const int statusAdjusted = std::max(0, battleSaturatedInt(
        static_cast<std::int64_t>(effectAdjusted) * statusFactor / 100));
    return areaAdjustedSpeed(
        state,
        unitId,
        statusAdjusted);
}

bool areaDamageChannelMatches(BattleDamageKind kind, DamageChannel channel)
{
    if (channel == DamageChannel::All)
    {
        return true;
    }
    switch (kind)
    {
    case BattleDamageKind::Physical:
    case BattleDamageKind::Skill:
        return channel == DamageChannel::Skill;
    case BattleDamageKind::Poison:
    case BattleDamageKind::Bleed:
        return channel == DamageChannel::Dot;
    case BattleDamageKind::Pure:
    case BattleDamageKind::Effect:
    case BattleDamageKind::Execute:
        return channel == DamageChannel::Effect;
    }
    assert(false);
    return false;
}

int areaOutgoingDamagePctDelta(
    const BattleRuntimeState& state,
    int sourceUnitId,
    BattleDamageKind damageKind)
{
    if (sourceUnitId == OptionalDamageAttackerUnitId)
    {
        return 0;
    }
    const auto modifiers = BattleAreaEffectSystem::collectAreaUnitModifiers(
        state.areas,
        state.gridTransform,
        state.units,
        sourceUnitId,
        state.movement.frame,
        BattleAreaQueryPhase::OutgoingDamage);
    int delta{};
    for (const auto& applied : modifiers.modifiers)
    {
        if (areaDamageChannelMatches(damageKind, applied.modifier.damageChannel))
        {
            delta += applied.modifier.percent;
        }
    }
    return delta;
}


EffectUnitSnapshot makeEffectUnitSnapshot(
    const BattleRuntimeUnit& unit,
    const BattleComboRuntimeFacts& comboFacts,
    const BattleStatusEffectState& statusEffects,
    const BattleActionPlanSeed* actionPlan,
    std::pmr::memory_resource* memoryResource)
{
    EffectUnitSnapshot result{
        .magicIds = std::pmr::vector<int>{ memoryResource },
        .comboIds = std::pmr::vector<int>{ memoryResource },
        .statusDetails = std::pmr::vector<EffectStatusSnapshot>{ memoryResource },
    };
    result.id = unit.id;
    result.team = unit.team;
    result.star = unit.star;
    result.cost = unit.cost;
    result.alive = unit.alive;
    result.hp = unit.vitals.hp;
    result.maxHp = unit.vitals.maxHp;
    result.mp = unit.vitals.mp;
    result.maxMp = unit.vitals.maxMp;
    result.shield = unit.shield;
    result.activeCooldown = unit.animation.cooldown;
    result.invincible = unit.invincible > 0;
    result.attack = unit.stats.attack;
    result.position = unit.motion.position;
    result.martialCategory = selectedMartialCategory(actionPlan);

    if (actionPlan)
    {
        result.magicIds.reserve(2);
        appendMagicId(result.magicIds, actionPlan->normalSkill.id);
        appendMagicId(result.magicIds, actionPlan->ultimateSkill.id);
        std::ranges::sort(result.magicIds);
        result.ultimateMagicId = actionPlan->ultimateSkill.id;
    }
    appendComboIds(comboFacts, result.comboIds);
    refreshEffectStatusSnapshot(result, statusEffects);
    return result;
}

void refreshEffectStatusSnapshot(
    EffectUnitSnapshot& snapshot,
    const BattleStatusEffectState& effects)
{
    populateEffectStatusSnapshot(snapshot, effects);
}

void appendRuntimeMagicEffectRules(
    BattleRuntimeState& runtime,
    std::span<const ChessMagicEffectDefinition> definitions)
{
    for (const auto& record : runtime.units.all())
    {
        const auto* actionPlan = record.actionPlan();
        if (!actionPlan || actionPlan->ultimateSkill.id < 0)
        {
            continue;
        }

        const int magicId = actionPlan->ultimateSkill.id;
        const auto* definition = findMagicDefinition(definitions, magicId);
        if (!definition)
        {
            continue;
        }

        runtime.effectSourceNames.emplace(
            std::pair{ EffectSourceKind::Magic, magicId },
            definition->name);

        runtime.effectRules.append(
            {
                .kind = EffectSourceKind::Magic,
                .sourceId = magicId,
                .ownerUnitId = record.id(),
                .sourceTeam = record.core.team,
            },
            definition->rules);
    }
}

namespace
{

EffectUnitSnapshot makeEffectUnitAttributeSnapshot(
    const BattleRuntimeState& runtime,
    const BattleRuntimeUnitRecord& record,
    std::pmr::memory_resource* memoryResource)
{
    const auto& unit = record.core;
    auto result = makeEffectUnitSnapshot(
        unit,
        record.comboFacts,
        record.status.effects,
        record.actionPlan(),
        memoryResource);
    result.maxHp = effectAdjustedAttribute(
        runtime,
        unit.id,
        BattleAttribute::MaxHp,
        unit.vitals.maxHp);
    result.attack = effectAdjustedAttribute(
        runtime,
        unit.id,
        BattleAttribute::Attack,
        unit.stats.attack);
    return result;
}

}  // namespace

EffectUnitSnapshot makeEffectUnitSnapshot(
    const BattleRuntimeState& runtime,
    const BattleRuntimeUnitRecord& record,
    std::pmr::memory_resource* memoryResource)
{
    auto result = makeEffectUnitAttributeSnapshot(runtime, record, memoryResource);
    appendBoundMagicIds(runtime, record.id(), result.magicIds);
    return result;
}

std::pmr::vector<EffectUnitSnapshot> makeEffectUnitSnapshots(
    const BattleRuntimeState& runtime,
    std::pmr::memory_resource* memoryResource)
{
    std::pmr::vector<EffectUnitSnapshot> result(memoryResource);
    result.reserve(runtime.units.size());
    for (const auto& record : runtime.units.all())
    {
        result.push_back(makeEffectUnitAttributeSnapshot(runtime, record, memoryResource));
    }
    if (!std::ranges::is_sorted(result, {}, &EffectUnitSnapshot::id))
        std::ranges::sort(result, {}, &EffectUnitSnapshot::id);
    // 全場快照只掃描一次規則；單位已按 ID 排序，可直接定位擁有者。
    for (const auto& bound : runtime.effectRules.rules())
    {
        if (!isPersistentMagicRule(bound)) continue;
        const auto owner = std::ranges::lower_bound(
            result, bound.binding.ownerUnitId, {}, &EffectUnitSnapshot::id);
        if (owner != result.end() && owner->id == bound.binding.ownerUnitId)
        {
            appendMagicId(owner->magicIds, bound.binding.sourceId);
        }
    }
    for (auto& snapshot : result)
    {
        std::ranges::sort(snapshot.magicIds);
    }
    return result;
}

BattleEffectRuntimeSnapshot::BattleEffectRuntimeSnapshot(const BattleRuntimeState& runtime)
    : units_(makeEffectUnitSnapshots(runtime, &memory_))
    , tileWidth_(static_cast<float>(runtime.gridTransform.tileWidth))
{
    assert(tileWidth_ > 0.0f);
}

std::span<const EffectUnitSnapshot> BattleEffectRuntimeSnapshot::units() const
{
    return units_;
}

BattleEffectReadView BattleEffectRuntimeSnapshot::readView() const
{
    return BattleEffectReadView(units(), tileWidth_);
}

}  // namespace KysChess::Battle
