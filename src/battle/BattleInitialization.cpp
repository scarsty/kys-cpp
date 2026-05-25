#include "BattleInitialization.h"

#include "BattleLogSegments.h"
#include "../BattleStarStats.h"
#include "../ChessBattleEffects.h"
#include "../Find.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

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
    int cloneCount = 0;
    for (const auto& [unitId, combo] : combos)
    {
        (void)unitId;
        cloneCount = std::max(cloneCount, maxAlwaysEffectValue(combo, EffectType::CloneSummon));
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

BattleRuntimeUnitSpawn& requireSpawnByUnitId(std::vector<BattleRuntimeUnitSpawn>& spawns, int unitId)
{
    const auto it = std::find_if(
        spawns.begin(),
        spawns.end(),
        [unitId](const BattleRuntimeUnitSpawn& spawn) { return spawn.unit.id == unitId; });
    assert(it != spawns.end());
    return *it;
}

const BattleRuntimeUnitSpawn& requireSpawnByUnitId(
    const std::vector<BattleRuntimeUnitSpawn>& spawns,
    int unitId)
{
    const auto it = std::find_if(
        spawns.begin(),
        spawns.end(),
        [unitId](const BattleRuntimeUnitSpawn& spawn) { return spawn.unit.id == unitId; });
    assert(it != spawns.end());
    return *it;
}

std::map<int, RoleComboState> comboMapFromSpawns(const std::vector<BattleRuntimeUnitSpawn>& spawns)
{
    std::map<int, RoleComboState> result;
    for (const auto& spawn : spawns)
    {
        result.emplace(spawn.unit.id, spawn.combo);
    }
    return result;
}

BattleRuntimeUnit makeCloneRuntimeUnit(
    const BattleRuntimeUnit& sourceUnit,
    int cloneUnitId,
    const BattleGridTransform& gridTransform,
    const BattleInitializationCloneSpawnCell& cell)
{
    auto clone = sourceUnit;
    clone.id = cloneUnitId;
    clone.cloneSourceUnitId = sourceUnit.id;
    clone.grid = { cell.x, cell.y, 0 };
    clone.motion.position = positionForCloneCell(gridTransform, cell.x, cell.y);
    clone.animation = BattleUnitAnimationState{};
    clone.haveAction = false;
    clone.operationType = BattleOperationType::None;
    clone.operationCount = 0;
    clone.vitals.hp = clone.vitals.maxHp;
    clone.alive = true;
    clone.weaponId = -1;
    clone.armorId = -1;
    clone.chessInstanceId = -1;
    return clone;
}

BattleInitializationRoleDelta makeRoleDelta(
    int unitId,
    int star,
    const BattleUnitVitals& vitals,
    const BattleUnitStats& stats,
    int fist = 0,
    int sword = 0,
    int knife = 0,
    int unusual = 0,
    int hiddenWeapon = 0)
{
    BattleInitializationRoleDelta delta;
    delta.unitId = unitId;
    delta.star = star;
    delta.vitals = vitals;
    delta.stats = stats;
    delta.fist = fist;
    delta.sword = sword;
    delta.knife = knife;
    delta.unusual = unusual;
    delta.hiddenWeapon = hiddenWeapon;
    return delta;
}

std::vector<BattleInitializationEnemyTopDebuffDelta> applyEnemyTopDebuff(
    std::vector<BattleRuntimeUnitSpawn>& spawns,
    int frame,
    std::vector<BattleLogEvent>& logEvents)
{
    int liveAllies = 0;
    int topTargets = 0;
    int perMemberValue = 0;
    for (const auto& spawn : spawns)
    {
        const auto& ally = spawn.unit;
        if (!ally.alive || ally.team != 0)
        {
            continue;
        }

        const auto* topDebuff = firstAlwaysEffect(spawn.combo, EffectType::EnemyTopDebuff);
        if (!topDebuff || topDebuff->value <= 0)
        {
            continue;
        }

        liveAllies++;
        topTargets = std::max(topTargets, topDebuff->value);
        perMemberValue = std::max(perMemberValue, topDebuff->value2);
    }

    std::vector<BattleRuntimeUnitSpawn*> enemyOrder;
    for (auto& spawn : spawns)
    {
        const auto& unit = spawn.unit;
        if (unit.team == 1 && unit.alive)
        {
            enemyOrder.push_back(&spawn);
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
        [&sortScore](const BattleRuntimeUnitSpawn* left, const BattleRuntimeUnitSpawn* right)
        {
            const long long leftScore = sortScore(left->unit);
            const long long rightScore = sortScore(right->unit);
            if (leftScore != rightScore)
            {
                return leftScore > rightScore;
            }
            return left->unit.vitals.maxHp > right->unit.vitals.maxHp;
        });

    std::vector<BattleInitializationEnemyTopDebuffDelta> deltas;
    int assignedTargets = 0;
    for (auto* enemySpawn : enemyOrder)
    {
        auto& enemy = enemySpawn->unit;
        auto& combo = enemySpawn->combo;

        int desired = 0;
        if (assignedTargets < topTargets && liveAllies > 0 && perMemberValue > 0)
        {
            desired = perMemberValue * liveAllies;
            ++assignedTargets;
        }

        const int delta = desired - combo.enemyTopDebuffApplied;
        if (delta == 0)
        {
            continue;
        }

        enemy.stats.attack = std::max(0, enemy.stats.attack - delta);
        enemy.stats.defence = std::max(0, enemy.stats.defence - delta);
        combo.enemyTopDebuffApplied = desired;
        deltas.push_back({
            enemy.id,
            -delta,
            -delta,
            desired,
        });
        logEvents.push_back({
            BattleLogEventType::Status,
            frame,
            -1,
            enemy.id,
            0,
            BattleLogCategory::Status,
            BattleLogPerspective::Targeted,
            logSegments<BattleLogTextTone::SkillName>(
                "陰險：前",
                std::pair{ BattleLogTextTone::ResourceValue, topTargets },
                "名攻防",
                std::pair{ delta > 0 ? BattleLogTextTone::Negative : BattleLogTextTone::Positive, delta > 0 ? "-" : "+" },
                std::pair{ delta > 0 ? BattleLogTextTone::Negative : BattleLogTextTone::Positive, std::abs(delta) },
                "（",
                std::pair{ BattleLogTextTone::ResourceValue, liveAllies },
                "名存活）"),
        });
    }

    return deltas;
}

}  // namespace

BattleInitializationResult BattleInitializationSystem::initialize(
    std::vector<BattleRuntimeUnitSpawn>& spawns,
    const BattleRuntimeSetupSeed& setup,
    const BattleInitializationContext& context) const
{
    BattleInitializationResult result;
    const auto allyResolved = resolveTeamSetup(setup.allyRoster, setup);
    const auto enemyResolved = resolveTeamSetup(setup.enemyRoster, setup);
    std::vector<int> seededUnitIds;
    seededUnitIds.reserve(setup.units.size());
    std::map<int, StarBoostedStats> starStatsByUnitId;

    for (const auto& seed : setup.units)
    {
        auto& spawn = requireSpawnByUnitId(spawns, seed.unitId);
        auto& unit = spawn.unit;
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

        unit.vitals.maxHp = starBoostedStats.hp + combo.flatHP;
        unit.stats.attack = starBoostedStats.atk + combo.flatATK;
        unit.stats.defence = starBoostedStats.def + combo.flatDEF;
        unit.stats.speed = starBoostedStats.spd + combo.flatSPD;
        unit.realRoleId = seed.realRoleId;
        unit.team = seed.team;
        unit.star = seed.star;
        unit.cost = seed.cost;

        unit.vitals.maxHp = applyPercentBonus(unit.vitals.maxHp, combo.pctHP);
        unit.stats.attack = applyPercentBonus(unit.stats.attack, combo.pctATK);
        unit.stats.defence = applyPercentBonus(unit.stats.defence, combo.pctDEF);
        unit.stats.speed = applyPercentBonus(unit.stats.speed, combo.pctSPD);
        unit.vitals.hp = unit.vitals.maxHp;

        const int shieldPctMaxHP = sumAlwaysEffectValue(combo, EffectType::ShieldPctMaxHP);
        if (shieldPctMaxHP > 0)
        {
            const int shield = unit.vitals.maxHp * shieldPctMaxHP / 100;
            result.logEvents.push_back(
                {
                    BattleLogEventType::Status,
                    context.frame,
                    seed.unitId,
                    -1,
                    shield,
                    BattleLogCategory::Status,
                    BattleLogPerspective::Targeted,
                    battleLogText(shieldLogText("獲取", shield), BattleLogTextTone::ShieldValue),
                });
        }

        for (int effectIndex = 0; effectIndex < static_cast<int>(combo.appliedEffects.size()); ++effectIndex)
        {
            const auto& effect = combo.appliedEffects[effectIndex];
            if (effect.type == EffectType::AutoUltimateAfterFrames && effect.trigger == Trigger::Always && effect.value > 0)
            {
                combo.effectFrameTimers[effectIndex] = effect.value;
            }
        }
        spawn.combo = std::move(combo);
        refreshRuntimeUnitSpawnStores(spawn);
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

            const auto& teamSpawn = requireSpawnByUnitId(spawns, teamSeed.unitId);
            teamCombos.emplace(teamSeed.unitId, teamSpawn.combo);
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

        auto& spawn = requireSpawnByUnitId(spawns, seed.unitId);
        refreshRuntimeUnitSpawnStores(spawn);
        spawn.unit.shield += teamShield;
        result.logEvents.push_back(
            {
                BattleLogEventType::Status,
                context.frame,
                seed.unitId,
                -1,
                teamShield,
                BattleLogCategory::Status,
                BattleLogPerspective::Targeted,
                battleLogText(shieldLogText("全隊獲取", teamShield), BattleLogTextTone::ShieldValue),
            });
    }

    const int cloneCount = resolveCloneCount(setup, comboMapFromSpawns(spawns));
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

        int nextRuntimeUnitId = static_cast<int>(spawns.size());
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
            const auto& sourceSpawn = requireSpawnByUnitId(spawns, source.sourceUnitId);
            const auto& sourceUnit = sourceSpawn.unit;

            auto cloneCombo = KysChess::ChessBattleEffects::makeSummonedCloneState(sourceSpawn.combo);
            auto cloneUnit = makeCloneRuntimeUnit(
                sourceUnit,
                nextRuntimeUnitId,
                context.gridTransform,
                cell);

            std::optional<BattleActionPlanSeed> cloneActionPlan;
            if (sourceSpawn.actionPlan)
            {
                cloneActionPlan = sourceSpawn.actionPlan;
            }
            auto cloneSpawn = makeRuntimeUnitSpawn(
                std::move(cloneUnit),
                std::move(cloneCombo),
                std::move(cloneActionPlan));

            result.roleDeltas.push_back(makeRoleDelta(
                nextRuntimeUnitId,
                cloneSpawn.unit.star,
                cloneSpawn.unit.vitals,
                cloneSpawn.unit.stats));
            result.logEvents.push_back({
                BattleLogEventType::Status,
                context.frame,
                source.sourceUnitId,
                nextRuntimeUnitId,
                0,
                BattleLogCategory::Status,
                BattleLogPerspective::Targeted,
                logSegments<BattleLogTextTone::SkillName>(
                    "七截分身（落點 ",
                    std::pair{ BattleLogTextTone::ResourceValue, cell.x },
                    ", ",
                    std::pair{ BattleLogTextTone::ResourceValue, cell.y },
                    "）"),
            });
            spawns.push_back(std::move(cloneSpawn));

            ++nextRuntimeUnitId;
            ++spawned;
        }
    }

    result.enemyTopDebuffs = applyEnemyTopDebuff(spawns, context.frame, result.logEvents);

    for (int unitId : seededUnitIds)
    {
        const auto& unit = requireSpawnByUnitId(spawns, unitId).unit;
        const auto starStatsIt = starStatsByUnitId.find(unitId);
        assert(starStatsIt != starStatsByUnitId.end());
        result.roleDeltas.push_back(makeRoleDelta(
            unitId,
            normalizeBattleStar(unit.star),
            unit.vitals,
            unit.stats,
            starStatsIt->second.fist,
            starStatsIt->second.sword,
            starStatsIt->second.knife,
            starStatsIt->second.unusual,
            starStatsIt->second.hidden));
    }

    result.comboStates = comboMapFromSpawns(spawns);

    return result;
}

}  // namespace KysChess::Battle
