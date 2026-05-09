#include "BattleInitialization.h"

#include "../BattleStarStats.h"
#include "../ChessBattleEffects.h"
#include "BattleFind.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <map>
#include <set>
#include <string>
#include <unordered_map>

namespace KysChess::Battle
{

namespace
{

struct ActiveSetupCombo
{
    int id = -1;
    int memberCount = 0;
    int physicalMemberCount = 0;
    int activeThresholdIdx = -1;
    bool isAntiCombo = false;
    std::set<int> memberRoleIds;
};

struct TeamResolvedSetup
{
    std::map<int, RoleComboState> baseStatesByRealRoleId;
    std::vector<std::pair<ComboEffect, int>> teamwideEffects;
};

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
    return std::format("{}{}護盾", prefix, shield);
}

bool isTeamwideComboEffect(EffectType type)
{
    switch (type)
    {
    case EffectType::TeamFlatHP:
    case EffectType::TeamFlatATK:
    case EffectType::TeamFlatDEF:
    case EffectType::TeamFlatSPD:
    case EffectType::TeamPctHP:
    case EffectType::TeamPctATK:
    case EffectType::TeamPctDEF:
    case EffectType::TeamPctSPD:
        return true;
    default:
        return false;
    }
}

bool equipmentSynergyActive(
    const BattleSetupEquipmentSynergyDefinition& synergy,
    int roleId,
    int weaponId,
    int armorId)
{
    if (synergy.equipmentId != weaponId && synergy.equipmentId != armorId)
    {
        return false;
    }

    return std::find(synergy.roleIds.begin(), synergy.roleIds.end(), roleId) != synergy.roleIds.end();
}

std::vector<std::string> collectActAsComboNames(
    const BattleRuntimeSetupSeed& setup,
    const BattleSetupRosterUnit& unit)
{
    std::vector<std::string> names;

    auto appendEquipmentActAs = [&](int itemId)
    {
        const auto* definition = tryFindBy(setup.equipmentDefinitions, itemId, &BattleSetupEquipmentDefinition::itemId);
        if (!definition)
        {
            return;
        }
        names.insert(names.end(), definition->actAsComboNames.begin(), definition->actAsComboNames.end());
    };

    appendEquipmentActAs(unit.weaponId);
    appendEquipmentActAs(unit.armorId);

    for (const auto& synergy : setup.equipmentSynergies)
    {
        if (!equipmentSynergyActive(synergy, unit.realRoleId, unit.weaponId, unit.armorId))
        {
            continue;
        }

        names.insert(names.end(), synergy.actAsComboNames.begin(), synergy.actAsComboNames.end());
    }

    return names;
}

std::vector<ActiveSetupCombo> detectCombos(
    const std::vector<BattleSetupRosterUnit>& roster,
    const BattleRuntimeSetupSeed& setup)
{
    std::map<int, int> starByRole;
    std::map<int, int> costByRole;
    std::map<std::string, std::set<int>> actAsByComboName;
    for (const auto& unit : roster)
    {
        starByRole[unit.realRoleId] = unit.star;
        costByRole[unit.realRoleId] = unit.cost;
        for (const auto& comboName : collectActAsComboNames(setup, unit))
        {
            actAsByComboName[comboName].insert(unit.realRoleId);
        }
    }

    std::vector<ActiveSetupCombo> result;
    for (const auto& combo : setup.comboDefinitions)
    {
        ActiveSetupCombo active;
        active.id = combo.id;

        auto addMember = [&](int roleId)
        {
            if (!starByRole.contains(roleId))
            {
                return;
            }
            if (active.memberRoleIds.insert(roleId).second)
            {
                active.memberCount++;
            }
        };

        for (int roleId : combo.memberRoleIds)
        {
            addMember(roleId);
        }
        if (const auto actAsIt = actAsByComboName.find(combo.name); actAsIt != actAsByComboName.end())
        {
            for (int roleId : actAsIt->second)
            {
                addMember(roleId);
            }
        }
        if (active.memberCount == 0)
        {
            continue;
        }

        active.physicalMemberCount = active.memberCount;
        if (combo.starSynergyBonus)
        {
            for (int roleId : active.memberRoleIds)
            {
                active.memberCount += starByRole[roleId] - 1;
            }
        }

        if (combo.isAntiCombo)
        {
            int bestRoleId = -1;
            int bestCost = -1;
            for (int roleId : active.memberRoleIds)
            {
                const int cost = costByRole.contains(roleId) ? costByRole[roleId] : -1;
                if (cost > bestCost)
                {
                    bestCost = cost;
                    bestRoleId = roleId;
                }
            }
            active.memberRoleIds.clear();
            if (bestRoleId >= 0)
            {
                active.memberRoleIds.insert(bestRoleId);
                active.memberCount = 1;
                active.physicalMemberCount = 1;
                active.isAntiCombo = true;
                active.activeThresholdIdx = 0;
            }
        }
        else
        {
            for (int thresholdIndex = static_cast<int>(combo.thresholds.size()) - 1; thresholdIndex >= 0; --thresholdIndex)
            {
                if (active.memberCount >= combo.thresholds[thresholdIndex].count)
                {
                    active.activeThresholdIdx = thresholdIndex;
                    break;
                }
            }
        }

        result.push_back(std::move(active));
    }

    return result;
}

TeamResolvedSetup resolveTeamSetup(
    const std::vector<BattleSetupRosterUnit>& roster,
    const BattleRuntimeSetupSeed& setup)
{
    TeamResolvedSetup resolved;
    for (const auto& active : detectCombos(roster, setup))
    {
        if (active.activeThresholdIdx < 0)
        {
            continue;
        }

        const auto& comboDefinition = requireById(setup.comboDefinitions, active.id);
        const auto& threshold = comboDefinition.thresholds[active.activeThresholdIdx];

        for (int roleId : active.memberRoleIds)
        {
            auto& state = resolved.baseStatesByRealRoleId[roleId];
            for (const auto& effect : threshold.effects)
            {
                if (isTeamwideComboEffect(effect.type))
                {
                    continue;
                }
                KysChess::ChessBattleEffects::applyEffect(state, effect, active.id);
            }
        }

        for (const auto& effect : threshold.effects)
        {
            if (!isTeamwideComboEffect(effect.type))
            {
                continue;
            }
            resolved.teamwideEffects.push_back({ effect, active.id });
        }
    }

    return resolved;
}

void applyEquipmentEffects(
    RoleComboState& combo,
    const BattleRuntimeSetupSeed& setup,
    const BattleInitializationUnitSeed& seed,
    const std::vector<BattleSetupRosterUnit>& roster)
{
    const auto* rosterUnit = tryFindBy(roster, seed.unitId, &BattleSetupRosterUnit::unitId);
    if (!rosterUnit)
    {
        return;
    }

    auto applyDefinition = [&](int itemId)
    {
        const auto* definition = tryFindBy(setup.equipmentDefinitions, itemId, &BattleSetupEquipmentDefinition::itemId);
        if (!definition)
        {
            return;
        }
        for (const auto& effect : definition->effects)
        {
            KysChess::ChessBattleEffects::applyEffect(combo, effect);
        }
    };

    applyDefinition(rosterUnit->weaponId);
    applyDefinition(rosterUnit->armorId);
    for (const auto& synergy : setup.equipmentSynergies)
    {
        if (!equipmentSynergyActive(synergy, seed.realRoleId, rosterUnit->weaponId, rosterUnit->armorId))
        {
            continue;
        }
        for (const auto& effect : synergy.effects)
        {
            KysChess::ChessBattleEffects::applyEffect(combo, effect);
        }
    }
}

void applyObtainedNeigongEffects(
    RoleComboState& combo,
    const BattleRuntimeSetupSeed& setup,
    int team)
{
    if (team != 0)
    {
        return;
    }

    for (int magicId : setup.obtainedNeigongMagicIds)
    {
        const auto* definition = tryFindBy(setup.neigongDefinitions, magicId, &BattleSetupNeigongDefinition::magicId);
        if (!definition)
        {
            continue;
        }
        for (const auto& effect : definition->effects)
        {
            KysChess::ChessBattleEffects::applyEffect(combo, effect);
        }
    }
}

int resolveCloneCount(const BattleRuntimeSetupSeed& setup, const std::map<int, RoleComboState>& combos)
{
    if (setup.cloneSummonCount > 0)
    {
        return setup.cloneSummonCount;
    }

    int cloneCount = 0;
    for (const auto& [unitId, combo] : combos)
    {
        (void)unitId;
        cloneCount = std::max(cloneCount, combo.cloneSummonCount);
    }
    return cloneCount;
}

Pointf positionForCloneCell(const BattleGridTransform& gridTransform, int x, int y)
{
    return {
        static_cast<float>(-y * gridTransform.tileWidth + x * gridTransform.tileWidth + gridTransform.coordCount * gridTransform.tileWidth),
        static_cast<float>(y * gridTransform.tileWidth + x * gridTransform.tileWidth),
        0.0f,
    };
}

BattleStatusRuntimeUnit cloneStatusUnit(
    const BattleStatusRuntimeUnit& source,
    int cloneUnitId,
    const RoleComboState& cloneCombo)
{
    auto clone = source;
    clone.id = cloneUnitId;
    clone.freezeReductionPct = cloneCombo.freezeReductionPct;
    clone.shieldFreezeResPct = cloneCombo.shieldFreezeResPct;
    clone.controlImmunityFrames = cloneCombo.controlImmunityFrames;
    clone.damageImmunityAfterFrames = cloneCombo.damageImmunityAfterFrames;
    clone.damageImmunityDuration = cloneCombo.damageImmunityDuration;
    clone.damageImmunityTimer = cloneCombo.damageImmunityTimer;
    return clone;
}

BattleInitializationRoleDelta makeRoleDelta(
    int unitId,
    int star,
    int maxHp,
    int hp,
    int attack,
    int defence,
    int speed,
    int fist = 0,
    int sword = 0,
    int knife = 0,
    int unusual = 0,
    int hiddenWeapon = 0)
{
    BattleInitializationRoleDelta delta;
    delta.unitId = unitId;
    delta.star = star;
    delta.maxHp = maxHp;
    delta.hp = hp;
    delta.attack = attack;
    delta.defence = defence;
    delta.speed = speed;
    delta.fist = fist;
    delta.sword = sword;
    delta.knife = knife;
    delta.unusual = unusual;
    delta.hiddenWeapon = hiddenWeapon;
    return delta;
}

std::vector<BattleInitializationEnemyTopDebuffDelta> applyEnemyTopDebuff(
    BattleRuntimeState& runtime,
    std::vector<BattleLogEvent>& logEvents)
{
    int liveAllies = 0;
    int topTargets = 0;
    int perMemberValue = 0;
    for (const auto& ally : runtime.units.units)
    {
        if (!ally.alive || ally.team != 0)
        {
            continue;
        }

        const auto comboIt = runtime.combo.units.find(ally.id);
        if (comboIt == runtime.combo.units.end() || comboIt->second.enemyTopDebuffCount <= 0)
        {
            continue;
        }

        liveAllies++;
        topTargets = std::max(topTargets, comboIt->second.enemyTopDebuffCount);
        perMemberValue = std::max(perMemberValue, comboIt->second.enemyTopDebuffValue);
    }

    std::vector<const BattleRuntimeUnit*> enemyOrder;
    for (const auto& unit : runtime.units.units)
    {
        if (unit.team == 1 && unit.alive)
        {
            enemyOrder.push_back(&unit);
        }
    }

    auto sortScore = [](const BattleRuntimeUnit& unit)
    {
        if (unit.cost <= 0)
        {
            return 0LL;
        }

        long long score = 1;
        for (int index = 0; index < unit.star; ++index)
        {
            score *= unit.cost;
        }
        return score;
    };

    std::stable_sort(
        enemyOrder.begin(),
        enemyOrder.end(),
        [&sortScore](const BattleRuntimeUnit* left, const BattleRuntimeUnit* right)
        {
            const long long leftScore = sortScore(*left);
            const long long rightScore = sortScore(*right);
            if (leftScore != rightScore)
            {
                return leftScore > rightScore;
            }
            return left->maxHp > right->maxHp;
        });

    std::vector<BattleInitializationEnemyTopDebuffDelta> deltas;
    int assignedTargets = 0;
    for (const auto* enemyRef : enemyOrder)
    {
        auto& enemy = runtime.units.requireUnit(enemyRef->id);
        auto comboIt = runtime.combo.units.find(enemy.id);
        assert(comboIt != runtime.combo.units.end());

        int desired = 0;
        if (assignedTargets < topTargets && liveAllies > 0 && perMemberValue > 0)
        {
            desired = perMemberValue * liveAllies;
            ++assignedTargets;
        }

        const int delta = desired - comboIt->second.enemyTopDebuffApplied;
        if (delta == 0)
        {
            continue;
        }

        enemy.attack = std::max(0, enemy.attack - delta);
        enemy.defence = std::max(0, enemy.defence - delta);
        comboIt->second.enemyTopDebuffApplied = desired;
        deltas.push_back({
            enemy.id,
            -delta,
            -delta,
            desired,
        });
        logEvents.push_back({
            BattleLogEventType::Status,
            BattlePresentationCurrentFrame,
            -1,
            enemy.id,
            0,
            std::format("陰險：前{}名攻防{}{}（{}名存活）",
                topTargets,
                delta > 0 ? "-" : "+",
                std::abs(delta),
                liveAllies),
        });
    }

    return deltas;
}

}  // namespace

BattleInitializationResult BattleInitializationSystem::initialize(BattleRuntimeState& runtime,
                                                                  const BattleRuntimeSetupSeed& setup) const
{
    BattleInitializationResult result;
    const auto allyResolved = resolveTeamSetup(setup.allyRoster, setup);
    const auto enemyResolved = resolveTeamSetup(setup.enemyRoster, setup);
    std::vector<int> seededUnitIds;
    seededUnitIds.reserve(setup.units.size());
    std::map<int, StarBoostedStats> starStatsByUnitId;

    for (const auto& seed : setup.units)
    {
        auto& unit = runtime.units.requireUnit(seed.unitId);
        auto& status = requireById(runtime.status.units, seed.unitId);
        RoleComboState combo = seed.baseCombo;

        const auto& resolved = seed.team == 0 ? allyResolved : enemyResolved;
        const auto& roster = seed.team == 0 ? setup.allyRoster : setup.enemyRoster;
        const auto* rosterUnit = tryFindBy(roster, seed.unitId, &BattleSetupRosterUnit::unitId);
        if (const auto baseStateIt = resolved.baseStatesByRealRoleId.find(seed.realRoleId);
            baseStateIt != resolved.baseStatesByRealRoleId.end())
        {
            for (const auto& effect : baseStateIt->second.appliedEffects)
            {
                KysChess::ChessBattleEffects::applyEffect(combo, effect, effect.sourceComboId);
            }
        }
        for (const auto& [effect, sourceComboId] : resolved.teamwideEffects)
        {
            KysChess::ChessBattleEffects::applyEffect(combo, effect, sourceComboId);
        }
        applyEquipmentEffects(
            combo,
            setup,
            seed,
            roster);
        applyObtainedNeigongEffects(combo, setup, seed.team);

        const int normalizedStar = normalizeBattleStar(rosterUnit ? rosterUnit->star : seed.star);
        const int fightsWon = rosterUnit ? rosterUnit->fightsWon : 0;
        const int extraFightWinGrowthHP = [&]()
        {
            const auto baseStateIt = resolved.baseStatesByRealRoleId.find(seed.realRoleId);
            return baseStateIt != resolved.baseStatesByRealRoleId.end() ? baseStateIt->second.fightWinGrowthHP : 0;
        }();
        const int extraFightWinGrowthATK = [&]()
        {
            const auto baseStateIt = resolved.baseStatesByRealRoleId.find(seed.realRoleId);
            return baseStateIt != resolved.baseStatesByRealRoleId.end() ? baseStateIt->second.fightWinGrowthATK : 0;
        }();
        const int extraFightWinGrowthDEF = [&]()
        {
            const auto baseStateIt = resolved.baseStatesByRealRoleId.find(seed.realRoleId);
            return baseStateIt != resolved.baseStatesByRealRoleId.end() ? baseStateIt->second.fightWinGrowthDEF : 0;
        }();
        const auto starBoostedStats = computeStarBoostedStats(
            {
                seed.baseMaxHp,
                seed.baseAttack,
                seed.baseDefence,
                seed.baseSpeed,
                seed.baseFist,
                seed.baseSword,
                seed.baseKnife,
                seed.baseUnusual,
                seed.baseHiddenWeapon,
            },
            normalizedStar,
            fightsWon,
            extraFightWinGrowthHP,
            extraFightWinGrowthATK,
            extraFightWinGrowthDEF);
        starStatsByUnitId[seed.unitId] = starBoostedStats;

        unit.maxHp = starBoostedStats.hp + combo.flatHP;
        unit.attack = starBoostedStats.atk + combo.flatATK;
        unit.defence = starBoostedStats.def + combo.flatDEF;
        unit.speed = starBoostedStats.spd + combo.flatSPD;
        unit.realRoleId = seed.realRoleId;
        unit.team = seed.team;
        unit.star = seed.star;
        unit.cost = seed.cost;

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
        seededUnitIds.push_back(seed.unitId);
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

    const int cloneCount = resolveCloneCount(setup, runtime.combo.units);
    if (cloneCount > 0 && !setup.cloneSources.empty())
    {
        std::vector<BattleInitializationCloneSource> cloneSources = setup.cloneSources;
        std::sort(
            cloneSources.begin(),
            cloneSources.end(),
            [](const BattleInitializationCloneSource& left, const BattleInitializationCloneSource& right)
            {
                if (left.star != right.star)
                {
                    return left.star > right.star;
                }
                if (left.power != right.power)
                {
                    return left.power > right.power;
                }
                return left.sourceOrder < right.sourceOrder;
            });

        std::set<int> usedInstanceIds;
        std::vector<BattleInitializationCloneSource> cloneCandidates;
        std::vector<BattleInitializationCloneSource> fallbackCandidates;
        for (const auto& source : cloneSources)
        {
            if (source.chessInstanceId >= 0)
            {
                if (!usedInstanceIds.insert(source.chessInstanceId).second)
                {
                    fallbackCandidates.push_back(source);
                    continue;
                }
            }
            cloneCandidates.push_back(source);
        }
        cloneCandidates.insert(cloneCandidates.end(), fallbackCandidates.begin(), fallbackCandidates.end());

        int nextCloneUnitId = setup.nextCloneUnitId;
        int spawned = 0;
        for (const auto& cell : setup.cloneCells)
        {
            if (spawned >= cloneCount || cloneCandidates.empty())
            {
                break;
            }
            if (!cell.walkable || cell.occupied)
            {
                continue;
            }

            const auto& source = cloneCandidates[spawned % cloneCandidates.size()];
            const auto& sourceUnit = runtime.units.requireUnit(source.sourceUnitId);
            const auto& sourceStatus = requireById(runtime.status.units, source.sourceUnitId);
            const auto* sourceWorld = tryFindById(runtime.world.units, source.sourceUnitId);
            const auto sourceComboIt = runtime.combo.units.find(source.sourceUnitId);
            assert(sourceComboIt != runtime.combo.units.end());

            auto cloneUnit = sourceUnit;
            cloneUnit.id = nextCloneUnitId;
            cloneUnit.realRoleId = source.sourceRealRoleId;
            cloneUnit.alive = true;
            cloneUnit.hp = cloneUnit.maxHp;
            cloneUnit.position = positionForCloneCell(runtime.units.gridTransform, cell.x, cell.y);
            cloneUnit.velocity = { 0, 0, 0 };
            cloneUnit.acceleration = { 0, 0, 0 };
            cloneUnit.grid = { cell.x, cell.y, 0 };
            cloneUnit.cooldown = 0;
            cloneUnit.cooldownMax = 0;
            cloneUnit.haveAction = false;
            cloneUnit.actFrame = 0;
            cloneUnit.actType = -1;
            cloneUnit.operationType = BattleOperationType::None;
            cloneUnit.operationCount = 0;
            cloneUnit.invincible = 0;
            cloneUnit.hurtFrame = 0;
            cloneUnit.canAttack = true;

            auto cloneCombo = KysChess::ChessBattleEffects::makeSummonedCloneState(
                sourceComboIt->second,
                cloneUnit.maxHp);
            cloneUnit.shield = cloneCombo.shield;

            runtime.units.units.push_back(cloneUnit);
            runtime.status.units.push_back(cloneStatusUnit(sourceStatus, nextCloneUnitId, cloneCombo));
            runtime.combo.units[nextCloneUnitId] = cloneCombo;

            BattleUnitState cloneWorld;
            if (sourceWorld)
            {
                cloneWorld = *sourceWorld;
            }
            cloneWorld.id = nextCloneUnitId;
            cloneWorld.realRoleId = source.sourceRealRoleId;
            cloneWorld.team = sourceUnit.team;
            cloneWorld.alive = true;
            cloneWorld.position = cloneUnit.position;
            cloneWorld.velocity = { 0, 0, 0 };
            cloneWorld.star = sourceUnit.star;
            runtime.world.units.push_back(cloneWorld);

            result.cloneIntents.push_back({
                source.sourceUnitId,
                nextCloneUnitId,
                cell.x,
                cell.y,
                makeRoleDelta(
                    nextCloneUnitId,
                    cloneUnit.star,
                    cloneUnit.maxHp,
                    cloneUnit.hp,
                    cloneUnit.attack,
                    cloneUnit.defence,
                    cloneUnit.speed),
                cloneCombo,
            });
            result.logEvents.push_back({
                BattleLogEventType::Status,
                BattlePresentationCurrentFrame,
                source.sourceUnitId,
                nextCloneUnitId,
                0,
                std::format("七截分身（落點 {}, {}）", cell.x, cell.y),
            });

            ++nextCloneUnitId;
            ++spawned;
        }
    }

    result.enemyTopDebuffs = applyEnemyTopDebuff(runtime, result.logEvents);

    for (int unitId : seededUnitIds)
    {
        const auto& unit = runtime.units.requireUnit(unitId);
        const auto starStatsIt = starStatsByUnitId.find(unitId);
        assert(starStatsIt != starStatsByUnitId.end());
        result.roleDeltas.push_back(makeRoleDelta(
            unitId,
            normalizeBattleStar(unit.star),
            unit.maxHp,
            unit.hp,
            unit.attack,
            unit.defence,
            unit.speed,
            starStatsIt->second.fist,
            starStatsIt->second.sword,
            starStatsIt->second.knife,
            starStatsIt->second.unusual,
            starStatsIt->second.hidden));
    }

    result.comboStates = runtime.combo.units;

    return result;
}

}  // namespace KysChess::Battle
