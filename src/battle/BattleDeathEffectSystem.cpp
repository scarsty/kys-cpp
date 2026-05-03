#include "BattleDeathEffectSystem.h"

#include <algorithm>
#include <cassert>

namespace KysChess::Battle
{

BattleDeathEffectUnit& BattleDeathEffectSystem::unitById(BattleDeathEffectWorld& world, int unitId) const
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleDeathEffectUnit& unit)
        {
            return unit.id == unitId;
        });
    assert(it != world.units.end());
    return *it;
}

bool BattleDeathEffectSystem::comboAppliesToUnit(const BattleDeathEffectWorld& world,
                                                const BattleDeathEffectUnit& unit,
                                                int comboId) const
{
    if (comboId < 0 || world.regularSynergyComboIds.count(comboId) == 0)
    {
        return false;
    }
    return std::find(unit.comboIds.begin(), unit.comboIds.end(), comboId) != unit.comboIds.end();
}

std::vector<BattleDeathEffectEvent> BattleDeathEffectSystem::applyAllyDeathEffects(BattleDeathEffectWorld& world,
                                                                                   int deadUnitId) const
{
    auto& dead = unitById(world, deadUnitId);
    assert(!dead.alive);

    std::vector<BattleDeathEffectEvent> events;
    for (auto& ally : world.units)
    {
        if (!ally.alive || ally.id == dead.id || ally.team != dead.team)
        {
            continue;
        }

        for (const auto& effect : ally.appliedEffects)
        {
            if (effect.type != EffectType::AllyDeathStatBoost
                || !comboAppliesToUnit(world, dead, effect.sourceComboId))
            {
                continue;
            }
            assert(effect.value > 0);

            ally.attack += effect.value;
            ally.defence += effect.value;
            events.push_back({
                BattleDeathEffectEventType::AllyStatBoost,
                dead.id,
                ally.id,
                effect.value,
                effect.sourceComboId,
            });
        }

        for (const auto& effect : dead.appliedEffects)
        {
            if (effect.type != EffectType::DeathMedical
                || !comboAppliesToUnit(world, ally, effect.sourceComboId))
            {
                continue;
            }
            assert(effect.value > 0);

            int before = ally.hp;
            int heal = std::max(1, ally.maxHp * effect.value / 100);
            ally.hp = std::min(ally.maxHp, ally.hp + heal);
            if (ally.hp > before)
            {
                events.push_back({
                    BattleDeathEffectEventType::DeathMedicalHeal,
                    dead.id,
                    ally.id,
                    ally.hp - before,
                    effect.sourceComboId,
                });
            }
        }

        for (const auto& effect : ally.appliedEffects)
        {
            if (effect.type != EffectType::ShieldOnAllyDeath
                || !comboAppliesToUnit(world, dead, effect.sourceComboId))
            {
                continue;
            }
            assert(effect.value > 0);

            ally.shieldOnAllyDeathTracker++;
            if (ally.shieldOnAllyDeathTracker < effect.value)
            {
                continue;
            }

            ally.shieldOnAllyDeathTracker = 0;
            if (ally.shieldPctMaxHp <= 0)
            {
                continue;
            }

            int before = ally.shield;
            ally.shield += ally.maxHp * ally.shieldPctMaxHp / 100;
            if (ally.shield > before)
            {
                events.push_back({
                    BattleDeathEffectEventType::ShieldOnAllyDeath,
                    dead.id,
                    ally.id,
                    ally.shield - before,
                    effect.sourceComboId,
                });
            }
        }
    }
    return events;
}

}  // namespace KysChess::Battle
