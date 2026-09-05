#include "BattleRuntimeEffects.h"

#include "BattleRuntimeUnits.h"
#include "BattleAreaEffectSystem.h"
#include "BattleDamageSystem.h"
#include "BattleMath.h"
#include "BattleStatusSystem.h"

#include <algorithm>
#include <cassert>
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

void appendMagicId(std::set<int>& magicIds, int magicId)
{
    if (magicId >= 0)
    {
        magicIds.insert(magicId);
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

void appendComboIds(const BattleComboRuntimeFacts& comboFacts, std::set<int>& comboIds)
{
    comboIds.insert(
        comboFacts.memberComboIds.begin(),
        comboFacts.memberComboIds.end());
    comboIds.insert(
        comboFacts.appliedComboIds.begin(),
        comboFacts.appliedComboIds.end());
}

void appendBoundMagicIds(
    const BattleRuntimeState& runtime,
    int ownerUnitId,
    std::set<int>& magicIds)
{
    for (const auto& bound : runtime.effectRules.rules())
    {
        if (bound.binding.kind == EffectSourceKind::Magic
            && bound.binding.ownerUnitId == ownerUnitId
            && !bound.castScope)
        {
            appendMagicId(magicIds, bound.binding.sourceId);
        }
    }
}

void populateEffectStatusSnapshot(
    EffectUnitSnapshot& result,
    const BattleStatusEffectState& effects)
{
    result.statusDetails.clear();
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
    return BattleEffectCommandSystem::queryAttribute(
        runtime,
        {
            .unitId = unitId,
            .attribute = attribute,
            .baseValue = baseValue,
            .frame = runtime.movement.frame,
            .eventSourceUnitId = eventSourceUnitId,
        });
}

int areaAttributeDelta(
    const BattleRuntimeState& runtime,
    int unitId,
    BattleAttribute attribute)
{
    const auto modifiers = BattleAreaEffectSystem::collectAreaUnitModifiers(
        runtime.areas,
        runtime.gridTransform,
        runtime.units,
        unitId,
        runtime.movement.frame,
        BattleAreaQueryPhase::UnitAttribute);
    int delta{};
    for (const auto& applied : modifiers.modifiers)
    {
        if (applied.modifier.attribute != attribute)
        {
            continue;
        }
        assert(applied.modifier.amount.base == EffectNumberBase::Constant);
        assert(!applied.modifier.amount.multiplierBase);
        assert(applied.modifier.amount.percent == 0);
        delta += applied.modifier.amount.flat;
    }
    return delta;
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
    const auto status = BattleStatusSystem({}).snapshot(
        state.units.require(unitId).statusDamageState());
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
    const BattleActionPlanSeed* actionPlan)
{
    EffectUnitSnapshot result;
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
    result.defence = unit.stats.defence;
    result.speed = unit.stats.speed;
    result.position = unit.motion.position;
    result.martialCategory = selectedMartialCategory(actionPlan);

    if (actionPlan)
    {
        appendMagicId(result.magicIds, actionPlan->normalSkill.id);
        appendMagicId(result.magicIds, actionPlan->ultimateSkill.id);
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

EffectUnitSnapshot makeEffectUnitSnapshot(
    const BattleRuntimeState& runtime,
    const BattleRuntimeUnitRecord& record)
{
    const auto& unit = record.core;
    auto result = makeEffectUnitSnapshot(
        unit,
        record.comboFacts,
        record.status.effects,
        record.actionPlan());
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
    result.defence = effectAdjustedAttribute(
        runtime,
        unit.id,
        BattleAttribute::Defence,
        unit.stats.defence);
    result.speed = effectAndAreaAdjustedSpeed(runtime, record.id(), record.core.stats.speed);
    appendBoundMagicIds(runtime, record.id(), result.magicIds);
    return result;
}

std::vector<EffectUnitSnapshot> makeEffectUnitSnapshots(const BattleRuntimeState& runtime)
{
    std::vector<EffectUnitSnapshot> result;
    result.reserve(runtime.units.size());
    for (const auto& record : runtime.units.all())
    {
        result.push_back(makeEffectUnitSnapshot(runtime, record));
    }
    std::ranges::sort(result, {}, &EffectUnitSnapshot::id);
    return result;
}

BattleEffectRuntimeSnapshot::BattleEffectRuntimeSnapshot(const BattleRuntimeState& runtime)
    : units_(makeEffectUnitSnapshots(runtime))
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
    return BattleEffectReadView(units_, tileWidth_);
}

}  // namespace KysChess::Battle
