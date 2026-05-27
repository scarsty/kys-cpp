#include "BattleDeathEffectSystem.h"

#include "../Find.h"
#include "BattleRuntimeUnits.h"
#include "BattleUnitStore.h"

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
                                                                                   BattleRuntimeUnitRecords& records,
                                                                                   BattleDeathEffectStore& effects,
                                                                                   int deadUnitId) const
{
    auto& dead = units.requireUnit(deadUnitId);
    auto& deadExtras = records.require(deadUnitId).deathEffects;
    assert(!dead.alive);

    std::vector<BattleDeathEffectEvent> events;
    for (auto& allyRecord : records.all())
    {
        auto& allyExtras = allyRecord.deathEffects;
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

            ally.stats.attack += effect.value;
            ally.stats.defence += effect.value;
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

            int before = ally.vitals.hp;
            int heal = std::max(1, ally.vitals.maxHp * effect.value / 100);
            ally.vitals.hp = std::min(ally.vitals.maxHp, ally.vitals.hp + heal);
            if (ally.vitals.hp > before)
            {
                events.push_back({
                    BattleDeathEffectEventType::DeathMedicalHeal,
                    dead.id,
                    ally.id,
                    ally.vitals.hp - before,
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
            ally.shield += ally.vitals.maxHp * allyExtras.shieldPctMaxHp / 100;
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
