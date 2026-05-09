#include "BattleDeathEffectSystem.h"

#include "BattleCore.h"
#include "BattleFind.h"

#include <algorithm>
#include <cassert>

namespace KysChess::Battle
{

bool BattleDeathEffectSystem::comboAppliesToUnit(const BattleDeathEffectStore& effects,
                                                const BattleDeathEffectExtras& extras,
                                                int comboId) const
{
    if (comboId < 0 || effects.regularSynergyComboIds.count(comboId) == 0)
    {
        return false;
    }
    return std::find(extras.comboIds.begin(), extras.comboIds.end(), comboId) != extras.comboIds.end();
}

std::vector<BattleDeathEffectEvent> BattleDeathEffectSystem::applyAllyDeathEffects(BattleUnitStore& units,
                                                                                   BattleDeathEffectStore& effects,
                                                                                   int deadUnitId) const
{
    auto& dead = units.requireUnit(deadUnitId);
    auto& deadExtras = requireById(effects.units, deadUnitId);
    assert(!dead.alive);

    std::vector<BattleDeathEffectEvent> events;
    for (auto& allyExtras : effects.units)
    {
        auto& ally = units.requireUnit(allyExtras.id);
        if (!ally.alive || ally.id == dead.id || ally.team != dead.team)
        {
            continue;
        }

        for (const auto& effect : allyExtras.appliedEffects)
        {
            if (effect.type != EffectType::AllyDeathStatBoost
                || !comboAppliesToUnit(effects, deadExtras, effect.sourceComboId))
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

        for (const auto& effect : deadExtras.appliedEffects)
        {
            if (effect.type != EffectType::DeathMedical
                || !comboAppliesToUnit(effects, allyExtras, effect.sourceComboId))
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

        for (const auto& effect : allyExtras.appliedEffects)
        {
            if (effect.type != EffectType::ShieldOnAllyDeath
                || !comboAppliesToUnit(effects, deadExtras, effect.sourceComboId))
            {
                continue;
            }
            assert(effect.value > 0);

            allyExtras.shieldOnAllyDeathTracker++;
            if (allyExtras.shieldOnAllyDeathTracker < effect.value)
            {
                continue;
            }

            allyExtras.shieldOnAllyDeathTracker = 0;
            if (allyExtras.shieldPctMaxHp <= 0)
            {
                continue;
            }

            int before = ally.shield;
            ally.shield += ally.maxHp * allyExtras.shieldPctMaxHp / 100;
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
