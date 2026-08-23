#include "BattleRuntimeEffects.h"

#include "BattleRuntimeUnits.h"

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

int selectedWeaponType(const BattleActionPlanSeed* actionPlan)
{
    if (!actionPlan || actionPlan->normalSkill.magicType <= 0)
    {
        return -1;
    }

    // MagicType uses 1=拳、2=劍、3=刀、4=特殊；effect selectors use
    // the corresponding zero-based martial category.
    return actionPlan->normalSkill.magicType - 1;
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
        const auto label = battleStatusLabel(instance.kind);
        assert(!label.empty());
        result.statusDetails.push_back({
            .state = std::string(label),
            .sourceUnitId = instance.sourceUnitId,
            .stacks = instance.stacks,
            .potency = instance.potency,
            .secondaryPotency = instance.secondaryPotency,
        });
    }
}

int effectAdjustedAttribute(
    const BattleRuntimeState& runtime,
    int unitId,
    BattleAttribute attribute,
    int baseValue)
{
    return BattleEffectCommandSystem::queryAttribute(
        runtime,
        {
            .unitId = unitId,
            .attribute = attribute,
            .baseValue = baseValue,
            .frame = runtime.movement.frame,
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

int effectiveSpeed(
    const BattleRuntimeState& runtime,
    const BattleRuntimeUnitRecord& record)
{
    const auto status = BattleStatusSystem({}).snapshot(record.status.effects);
    const int statusAdjusted = std::max(
        0,
        effectAdjustedAttribute(
            runtime,
            record.id(),
            BattleAttribute::Speed,
            record.core.stats.speed)
            * (100 + status.speedPctDelta) / 100);
    return std::max(
        0,
        statusAdjusted
            * (100 + areaAttributeDelta(
                runtime,
                record.id(),
                BattleAttribute::Speed))
            / 100);
}

}  // namespace

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
    result.weaponType = selectedWeaponType(actionPlan);

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
        if (!definition || !definition->enabled)
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
    result.speed = effectiveSpeed(runtime, record);
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
