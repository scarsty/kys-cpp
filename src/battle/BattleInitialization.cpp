#include "BattleInitialization.h"

#include <cassert>
#include <set>
#include <string>

namespace KysChess::Battle
{

namespace
{

BattleStatusRuntimeUnit& requireStatusUnit(BattleRuntimeState& runtime, int unitId)
{
    for (auto& status : runtime.status.units)
    {
        if (status.id == unitId)
        {
            return status;
        }
    }

    assert(false && "battle initialization requires imported status runtime unit");
    return runtime.status.units.front();
}

int applyPercentBonus(int value, double pct)
{
    if (pct == 0)
    {
        return value;
    }

    return static_cast<int>(value * (1.0 + pct / 100.0));
}

int computeTeamFlatShield(const std::map<int, RoleComboState>& comboStates)
{
    int totalShield = 0;
    std::set<int> seenComboIds;
    for (const auto& [unitId, combo] : comboStates)
    {
        (void)unitId;
        for (const auto& effect : combo.appliedEffects)
        {
            if (effect.type != EffectType::FlatShield || effect.trigger != Trigger::Always || effect.value <= 0)
            {
                continue;
            }

            if (effect.sourceComboId >= 0)
            {
                if (!seenComboIds.insert(effect.sourceComboId).second)
                {
                    continue;
                }
            }

            totalShield += effect.value;
        }
    }
    return totalShield;
}

std::string shieldLogText(const char* prefix, int shield)
{
    return std::string(prefix) + std::to_string(shield) + "護盾";
}

}  // namespace

BattleInitializationResult BattleInitializationSystem::initialize(BattleRuntimeState& runtime,
                                                                  const BattleRuntimeSetupSeed& setup) const
{
    BattleInitializationResult result;
    std::map<int, int> teamByUnitId;

    for (const auto& seed : setup.units)
    {
        auto& unit = runtime.units.requireUnit(seed.unitId);
        auto& status = requireStatusUnit(runtime, seed.unitId);

        teamByUnitId[seed.unitId] = seed.team;

        RoleComboState combo = seed.baseCombo;
        unit.maxHp = seed.baseMaxHp + combo.flatHP;
        unit.attack = seed.baseAttack + combo.flatATK;
        unit.defence = seed.baseDefence + combo.flatDEF;
        unit.speed = seed.baseSpeed + combo.flatSPD;

        unit.maxHp = applyPercentBonus(unit.maxHp, combo.pctHP);
        unit.attack = applyPercentBonus(unit.attack, combo.pctATK);
        unit.defence = applyPercentBonus(unit.defence, combo.pctDEF);
        unit.speed = applyPercentBonus(unit.speed, combo.pctSPD);
        unit.hp = unit.maxHp;

        if (combo.shieldPctMaxHP > 0)
        {
            combo.shield = unit.maxHp * combo.shieldPctMaxHP / 100;
            result.logEvents.push_back(
                {
                    BattleLogEventType::Status,
                    BattlePresentationCurrentFrame,
                    seed.unitId,
                    -1,
                    combo.shield,
                    shieldLogText("獲取", combo.shield),
                });
        }

        if (combo.damageImmunityAfterFrames > 0)
        {
            combo.damageImmunityTimer = combo.damageImmunityAfterFrames;
        }
        if (combo.autoUltimateAfterFrames > 0)
        {
            combo.autoUltimateTimer = combo.autoUltimateAfterFrames;
        }
        combo.blockFirstHitsRemaining = combo.blockFirstHitsCount;

        unit.shield = combo.shield;

        status.damageImmunityAfterFrames = combo.damageImmunityAfterFrames;
        status.damageImmunityDuration = combo.damageImmunityDuration;
        status.damageImmunityTimer = combo.damageImmunityTimer;
        status.freezeReductionPct = combo.freezeReductionPct;
        status.shieldFreezeResPct = combo.shieldFreezeResPct;
        status.controlImmunityFrames = combo.controlImmunityFrames;

        runtime.combo.units[seed.unitId] = combo;
        result.roleDeltas.push_back(
            {
                seed.unitId,
                unit.maxHp,
                unit.hp,
                unit.attack,
                unit.defence,
                unit.speed,
            });
    }

    std::map<int, int> teamFlatShieldByTeam;
    for (const auto& seed : setup.units)
    {
        if (teamFlatShieldByTeam.contains(seed.team))
        {
            continue;
        }

        std::map<int, RoleComboState> teamCombos;
        for (const auto& teamSeed : setup.units)
        {
            if (teamSeed.team != seed.team)
            {
                continue;
            }

            auto comboIt = runtime.combo.units.find(teamSeed.unitId);
            assert(comboIt != runtime.combo.units.end());
            teamCombos.emplace(teamSeed.unitId, comboIt->second);
        }
        teamFlatShieldByTeam.emplace(seed.team, computeTeamFlatShield(teamCombos));
    }

    for (const auto& seed : setup.units)
    {
        const auto teamShieldIt = teamFlatShieldByTeam.find(seed.team);
        assert(teamShieldIt != teamFlatShieldByTeam.end());
        const int teamShield = teamShieldIt->second;
        if (teamShield <= 0)
        {
            continue;
        }

        auto& unit = runtime.units.requireUnit(seed.unitId);
        auto comboIt = runtime.combo.units.find(seed.unitId);
        assert(comboIt != runtime.combo.units.end());

        comboIt->second.shield += teamShield;
        unit.shield = comboIt->second.shield;
        result.logEvents.push_back(
            {
                BattleLogEventType::Status,
                BattlePresentationCurrentFrame,
                seed.unitId,
                -1,
                teamShield,
                shieldLogText("全隊獲取", teamShield),
            });
    }

    return result;
}

}  // namespace KysChess::Battle