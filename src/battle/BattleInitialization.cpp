#include "BattleInitialization.h"

#include "ChessComboResolver.h"
#include "BattleLogSegments.h"
#include "../BattleStarStats.h"
#include "../ChessBattleEffects.h"
#include "../Find.h"

#include <algorithm>
#include <array>
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

struct TeamResolvedSetup
{
    std::map<int, RoleComboState> baseStatesByRealRoleId;
    std::vector<std::pair<ComboEffect, int>> teamwideEffects;
};

struct OpponentTopDebuffSummary
{
    int liveOwners{};
    int topTargets{};
    int perOwnerValue{};
};

int applyPercentBonus(int value, double pct)
{
    if (pct == 0)
    {
        return value;
    }

    return static_cast<int>(value * (1.0 + pct / 100.0));
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

std::vector<ChessComboResolverUnit> comboResolverUnits(
    const std::vector<BattleSetupRosterUnit>& roster)
{
    std::vector<ChessComboResolverUnit> result;
    result.reserve(roster.size());
    for (const auto& unit : roster)
    {
        result.push_back({
            unit.realRoleId,
            unit.star,
            unit.cost,
            unit.weaponId,
            unit.armorId,
            unit.unitId,
        });
    }
    return result;
}

std::vector<ChessComboResolverEquipmentRule> comboResolverEquipmentRules(
    const BattleRuntimeSetupSeed& setup)
{
    std::vector<ChessComboResolverEquipmentRule> result;
    for (const auto& equipment : setup.equipmentDefinitions)
    {
        if (!equipment.actAsComboNames.empty())
        {
            result.push_back({equipment.itemId, {}, equipment.actAsComboNames});
        }
    }
    for (const auto& synergy : setup.equipmentSynergies)
    {
        if (!synergy.actAsComboNames.empty())
        {
            result.push_back({synergy.equipmentId, synergy.roleIds, synergy.actAsComboNames});
        }
    }
    return result;
}

std::vector<ChessComboResolverDefinition> comboResolverDefinitions(
    const BattleRuntimeSetupSeed& setup)
{
    std::vector<ChessComboResolverDefinition> result;
    result.reserve(setup.comboDefinitions.size());
    for (const auto& combo : setup.comboDefinitions)
    {
        ChessComboResolverDefinition definition;
        definition.id = combo.id;
        definition.name = combo.name;
        definition.memberRoleIds = combo.memberRoleIds;
        definition.isAntiCombo = combo.isAntiCombo;
        definition.starSynergyBonus = combo.starSynergyBonus;
        for (const auto& threshold : combo.thresholds)
        {
            definition.thresholdCounts.push_back(threshold.count);
        }
        result.push_back(std::move(definition));
    }
    return result;
}

std::vector<ResolvedChessCombo> resolveBattleSetupCombosImpl(
    const std::vector<BattleSetupRosterUnit>& roster,
    const BattleRuntimeSetupSeed& setup)
{
    return resolveChessCombos(
        comboResolverUnits(roster),
        comboResolverEquipmentRules(setup),
        comboResolverDefinitions(setup));
}

TeamResolvedSetup resolveTeamSetup(
    const std::vector<BattleSetupRosterUnit>& roster,
    const BattleRuntimeSetupSeed& setup)
{
    TeamResolvedSetup resolved;
    for (const auto& active : resolveBattleSetupCombosImpl(roster, setup))
    {
        if (active.activeThresholdIndex < 0)
        {
            continue;
        }

        const auto& comboDefinition = requireById(setup.comboDefinitions, active.id);
        const auto& threshold = comboDefinition.thresholds[active.activeThresholdIndex];

        for (int roleId : active.memberRoleIds)
        {
            auto& state = resolved.baseStatesByRealRoleId[roleId];
            for (const auto& effect : threshold.effects)
            {
                if (isTeamwideComboEffect(effect.type))
                {
                    continue;
                }
                state.applyConfiguredEffect(effect, active.id);
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
            combo.applyConfiguredEffect(effect);
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
            combo.applyConfiguredEffect(effect);
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
            combo.applyConfiguredEffect(effect);
        }
    }
}

BattleUnitSkillEffectState makeSkillEffectState(
    int magicId,
    const BattleRuntimeSetupSeed& setup)
{
    BattleUnitSkillEffectState state;
    if (magicId < 0)
    {
        return state;
    }

    state.magicId = magicId;
    const auto* definition = tryFindBy(
        setup.magicEffectDefinitions,
        magicId,
        &ChessMagicEffectDefinition::magicId);
    if (!definition)
    {
        return state;
    }

    for (const auto& effect : definition->effects)
    {
        state.effects.applyConfiguredEffect(effect);
    }
    return state;
}

BattleUnitMagicEffectRuntime makeSkillEffectsFromActionPlan(
    const BattleActionPlanSeed* actionPlan,
    const BattleRuntimeSetupSeed& setup)
{
    BattleUnitMagicEffectRuntime runtime;
    if (!actionPlan)
    {
        return runtime;
    }

    runtime.ultimate = makeSkillEffectState(actionPlan->ultimateSkill.id, setup);
    return runtime;
}

void rebuildSpawnSkillEffects(
    BattleRuntimeUnitSpawn& spawn,
    const BattleRuntimeSetupSeed& setup)
{
    spawn.skillEffects = makeSkillEffectsFromActionPlan(spawn.actionPlan(), setup);
}

Pointf positionForCloneCell(const BattleGridTransform& gridTransform, int x, int y)
{
    return {
        static_cast<float>(-y * gridTransform.tileWidth + x * gridTransform.tileWidth + gridTransform.coordCount * gridTransform.tileWidth),
        static_cast<float>(y * gridTransform.tileWidth + x * gridTransform.tileWidth),
        0.0f,
    };
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
    std::array<OpponentTopDebuffSummary, 2> summaries;
    for (const auto& spawn : spawns)
    {
        const auto& owner = spawn.unit;
        if (!owner.alive)
        {
            continue;
        }
        assert(owner.team == 0 || owner.team == 1);

        const auto* topDebuff = (spawn.combo).firstAlways(EffectType::EnemyTopDebuff);
        if (!topDebuff || topDebuff->value <= 0)
        {
            continue;
        }

        auto& summary = summaries[owner.team];
        summary.liveOwners++;
        summary.topTargets = std::max(summary.topTargets, topDebuff->value);
        summary.perOwnerValue = std::max(summary.perOwnerValue, topDebuff->value2);
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

    std::vector<BattleInitializationEnemyTopDebuffDelta> deltas;
    for (int ownerTeam = 0; ownerTeam < static_cast<int>(summaries.size()); ++ownerTeam)
    {
        const auto& summary = summaries[ownerTeam];
        const int targetTeam = 1 - ownerTeam;
        std::vector<BattleRuntimeUnitSpawn*> targetOrder;
        for (auto& spawn : spawns)
        {
            const auto& unit = spawn.unit;
            if (unit.team == targetTeam && unit.alive)
            {
                targetOrder.push_back(&spawn);
            }
        }

        std::stable_sort(
            targetOrder.begin(),
            targetOrder.end(),
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

        int assignedTargets = 0;
        for (auto* targetSpawn : targetOrder)
        {
            auto& target = targetSpawn->unit;
            auto& combo = targetSpawn->combo;

            int desired = 0;
            if (assignedTargets < summary.topTargets && summary.liveOwners > 0 && summary.perOwnerValue > 0)
            {
                desired = summary.perOwnerValue * summary.liveOwners;
                ++assignedTargets;
            }

            const int delta = combo.setEnemyTopDebuffApplied(desired);
            if (delta == 0)
            {
                continue;
            }

            target.stats.attack = std::max(0, target.stats.attack - delta);
            target.stats.defence = std::max(0, target.stats.defence - delta);
            deltas.push_back({
                target.id,
                -delta,
                -delta,
                desired,
            });
            BattleLogEvent log;
            log.type = BattleLogEventType::Status;
            log.frame = frame;
            log.targetUnitId = target.id;
            log.amount = -delta;
            log.previousAmount = -(desired - delta);
            log.newAmount = -desired;
            log.statusId = BattleStatusSemanticId::EnemyTopDebuff;
            log.semanticSourceTeam = ownerTeam;
            log.semanticSourceKind = "combo";
            log.semanticSourceName = "陰險";
            log.segments = logSegments<BattleLogTextTone::SkillName>(
                "陰險：前",
                std::pair{ BattleLogTextTone::ResourceValue, summary.topTargets },
                "名攻防",
                std::pair{ delta > 0 ? BattleLogTextTone::Negative : BattleLogTextTone::Positive, delta > 0 ? "-" : "+" },
                std::pair{ delta > 0 ? BattleLogTextTone::Negative : BattleLogTextTone::Positive, std::abs(delta) },
                "（",
                std::pair{ BattleLogTextTone::ResourceValue, summary.liveOwners },
                "名存活）");
            logEvents.push_back(std::move(log));
        }
    }

    return deltas;
}

class BattleStartInitializationRun
{
public:
    BattleStartInitializationRun(std::vector<BattleRuntimeUnitSpawn> spawns,
                                 const BattleRuntimeSetupSeed& setup,
                                 BattleInitializationContext context);

    BattleInitializationOutput run() &&;

private:
    void initializeSkillEffectStates();
    void initializeSeededUnits();
    void applyTeamFlatShields();
    void summonClones();
    void applyEnemyTopDebuffs();
    void appendSeededRoleDeltas();

    decltype(auto) spawn(this auto& self, int unitId)
    {
        assert(unitId >= 0);
        const auto spawnIt = self.spawnIndexByUnitId_.find(unitId);
        assert(spawnIt != self.spawnIndexByUnitId_.end());
        assert(spawnIt->second < self.spawns_.size());
        return (self.spawns_[spawnIt->second]);
    }

    void appendSpawn(BattleRuntimeUnitSpawn spawn);

    int teamFlatShield(int team) const;
    std::map<int, int> cloneCountByTeam() const;

    const TeamResolvedSetup& resolvedForTeam(int team) const;
    const std::vector<BattleSetupRosterUnit>& rosterForTeam(int team) const;

    std::vector<BattleRuntimeUnitSpawn> spawns_;
    const BattleRuntimeSetupSeed& setup_;
    BattleInitializationContext context_;
    TeamResolvedSetup allyResolved_;
    TeamResolvedSetup enemyResolved_;
    BattleInitializationResult result_;
    std::unordered_map<int, std::size_t> spawnIndexByUnitId_;
    std::vector<int> seededUnitIds_;
    std::map<int, StarBoostedStats> starStatsByUnitId_;
};

BattleStartInitializationRun::BattleStartInitializationRun(
    std::vector<BattleRuntimeUnitSpawn> spawns,
    const BattleRuntimeSetupSeed& setup,
    BattleInitializationContext context)
    : spawns_(std::move(spawns))
    , setup_(setup)
    , context_(context)
    , allyResolved_(resolveTeamSetup(setup_.allyRoster, setup_))
    , enemyResolved_(resolveTeamSetup(setup_.enemyRoster, setup_))
{
    spawnIndexByUnitId_.reserve(spawns_.size());
    seededUnitIds_.reserve(setup_.units.size());
    for (std::size_t index = 0; index < spawns_.size(); ++index)
    {
        const int unitId = spawns_[index].unit.id;
        assert(unitId >= 0);
        assert(!spawnIndexByUnitId_.contains(unitId));
        spawnIndexByUnitId_.emplace(unitId, index);
    }
}

BattleInitializationOutput BattleStartInitializationRun::run() &&
{
    initializeSkillEffectStates();
    initializeSeededUnits();
    applyTeamFlatShields();
    summonClones();
    applyEnemyTopDebuffs();
    appendSeededRoleDeltas();

    return {
        std::move(spawns_),
        std::move(result_),
    };
}

void BattleStartInitializationRun::appendSpawn(BattleRuntimeUnitSpawn spawn)
{
    const int unitId = spawn.unit.id;
    assert(unitId >= 0);
    assert(!spawnIndexByUnitId_.contains(unitId));
    spawnIndexByUnitId_.emplace(unitId, spawns_.size());
    spawns_.push_back(std::move(spawn));
}

void BattleStartInitializationRun::initializeSkillEffectStates()
{
    for (auto& spawn : spawns_)
    {
        rebuildSpawnSkillEffects(spawn, setup_);
    }
}

const TeamResolvedSetup& BattleStartInitializationRun::resolvedForTeam(int team) const
{
    return team == 0 ? allyResolved_ : enemyResolved_;
}

const std::vector<BattleSetupRosterUnit>& BattleStartInitializationRun::rosterForTeam(int team) const
{
    return team == 0 ? setup_.allyRoster : setup_.enemyRoster;
}

void BattleStartInitializationRun::initializeSeededUnits()
{
    for (const auto& seed : setup_.units)
    {
        auto& spawn = this->spawn(seed.unitId);
        auto& unit = spawn.unit;
        auto& combo = spawn.combo;

        const auto& resolved = resolvedForTeam(seed.team);
        const auto& roster = rosterForTeam(seed.team);
        const auto* rosterUnit = tryFindBy(roster, seed.unitId, &BattleSetupRosterUnit::unitId);
        int extraFightWinGrowthHP{};
        int extraFightWinGrowthATK{};
        int extraFightWinGrowthDEF{};
        if (const auto baseStateIt = resolved.baseStatesByRealRoleId.find(seed.realRoleId);
            baseStateIt != resolved.baseStatesByRealRoleId.end())
        {
            const auto& baseState = baseStateIt->second;
            for (RoleComboEffectId effectId : baseState.effectIdsInAppendOrder())
            {
                const auto& effect = baseState.effect(effectId);
                if (effect.origin != RoleComboEffectOrigin::Configured)
                {
                    continue;
                }
                combo.applyConfiguredEffect(effect, effect.sourceComboId);
            }
            const auto& baseBonuses = baseState.statBonuses();
            extraFightWinGrowthHP = baseBonuses.fightWinGrowthHP;
            extraFightWinGrowthATK = baseBonuses.fightWinGrowthATK;
            extraFightWinGrowthDEF = baseBonuses.fightWinGrowthDEF;
        }
        for (const auto& [effect, sourceComboId] : resolved.teamwideEffects)
        {
            combo.applyConfiguredEffect(effect, sourceComboId);
        }
        applyEquipmentEffects(
            combo,
            setup_,
            seed,
            roster);
        applyObtainedNeigongEffects(combo, setup_, seed.team);

        const int normalizedStar = normalizeBattleStar(rosterUnit ? rosterUnit->star : seed.star);
        const int fightsWon = rosterUnit ? rosterUnit->fightsWon : 0;
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
            setup_.starGrowth,
            normalizedStar,
            fightsWon,
            extraFightWinGrowthHP,
            extraFightWinGrowthATK,
            extraFightWinGrowthDEF);
        starStatsByUnitId_[seed.unitId] = starBoostedStats;

        const auto& comboStatBonuses = combo.statBonuses();
        unit.vitals.maxHp = starBoostedStats.hp + comboStatBonuses.flatHP;
        unit.stats.attack = starBoostedStats.atk + comboStatBonuses.flatATK;
        unit.stats.defence = starBoostedStats.def + comboStatBonuses.flatDEF;
        unit.stats.speed = starBoostedStats.spd + comboStatBonuses.flatSPD;
        unit.realRoleId = seed.realRoleId;
        unit.team = seed.team;
        unit.star = seed.star;
        unit.cost = seed.cost;

        unit.vitals.maxHp = applyPercentBonus(unit.vitals.maxHp, comboStatBonuses.pctHP);
        unit.stats.attack = applyPercentBonus(unit.stats.attack, comboStatBonuses.pctATK);
        unit.stats.defence = applyPercentBonus(unit.stats.defence, comboStatBonuses.pctDEF);
        unit.stats.speed = applyPercentBonus(unit.stats.speed, comboStatBonuses.pctSPD);
        unit.vitals.hp = unit.vitals.maxHp;

        const int shieldPctMaxHP = combo.sumAlways(EffectType::ShieldPctMaxHP);
        if (shieldPctMaxHP > 0)
        {
            const int shield = unit.vitals.maxHp * shieldPctMaxHP / 100;
            result_.logEvents.push_back(
                {
                    BattleLogEventType::Status,
                    context_.frame,
                    seed.unitId,
                    -1,
                    shield,
                    BattleLogCategory::Status,
                    BattleLogPerspective::Targeted,
                    battleLogText(shieldLogText("獲取", shield), BattleLogTextTone::ShieldValue),
                });
        }

        combo.seedAutoUltimateFrameTimers();
        refreshRuntimeUnitSpawnDerivedState(spawn);
        seededUnitIds_.push_back(seed.unitId);
    }
}

int BattleStartInitializationRun::teamFlatShield(int team) const
{
    int totalShield = 0;
    std::set<int> seenComboIds;
    for (const auto& seed : setup_.units)
    {
        if (seed.team != team)
        {
            continue;
        }

        const auto& combo = spawn(seed.unitId).combo;
        for (RoleComboEffectId effectId : combo.effectIds(Trigger::Always, EffectType::FlatShield))
        {
            const auto& effect = combo.effect(effectId);
            if (effect.origin != RoleComboEffectOrigin::Configured)
            {
                continue;
            }
            if (effect.value <= 0)
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

void BattleStartInitializationRun::applyTeamFlatShields()
{
    std::map<int, int> teamFlatShieldByTeam;
    for (const auto& seed : setup_.units)
    {
        if (teamFlatShieldByTeam.contains(seed.team))
        {
            continue;
        }

        teamFlatShieldByTeam.emplace(seed.team, teamFlatShield(seed.team));
    }

    for (const auto& seed : setup_.units)
    {
        const auto teamShieldIt = teamFlatShieldByTeam.find(seed.team);
        assert(teamShieldIt != teamFlatShieldByTeam.end());
        const int teamShield = teamShieldIt->second;
        if (teamShield <= 0)
        {
            continue;
        }

        auto& spawn = this->spawn(seed.unitId);
        refreshRuntimeUnitSpawnDerivedState(spawn);
        spawn.unit.shield += teamShield;
        result_.logEvents.push_back(
            {
                BattleLogEventType::Status,
                context_.frame,
                seed.unitId,
                -1,
                teamShield,
                BattleLogCategory::Status,
                BattleLogPerspective::Targeted,
                battleLogText(shieldLogText("全隊獲取", teamShield), BattleLogTextTone::ShieldValue),
            });
    }
}

std::map<int, int> BattleStartInitializationRun::cloneCountByTeam() const
{
    std::map<int, int> countByTeam;
    for (const auto& spawn : spawns_)
    {
        const int count = (spawn.combo).maxAlways(EffectType::CloneSummon);
        if (count <= 0)
        {
            continue;
        }
        countByTeam[spawn.unit.team] = std::max(countByTeam[spawn.unit.team], count);
    }
    return countByTeam;
}

void BattleStartInitializationRun::summonClones()
{
    const auto countByTeam = cloneCountByTeam();
    if (countByTeam.empty() || setup_.cloneSources.empty())
    {
        return;
    }

    std::vector<BattleInitializationCloneSource> cloneSources = setup_.cloneSources;
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

    int nextRuntimeUnitId{};
    for (const auto& spawn : spawns_)
    {
        nextRuntimeUnitId = std::max(nextRuntimeUnitId, spawn.unit.id + 1);
    }
    std::set<std::pair<int, int>> usedCloneCells;
    for (const auto& [team, count] : countByTeam)
    {
        std::set<int> usedInstanceIds;
        std::vector<BattleInitializationCloneSource> cloneCandidates;
        std::vector<BattleInitializationCloneSource> fallbackCandidates;
        for (const auto& source : cloneSources)
        {
            const auto& sourceSpawn = spawn(source.sourceUnitId);
            if (sourceSpawn.unit.team != team)
            {
                continue;
            }
            if (sourceSpawn.combo.maxAlways(EffectType::CloneSummon) <= 0)
            {
                continue;
            }
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

        int spawned = 0;
        for (const auto& cell : setup_.cloneCells)
        {
            if (spawned >= count || cloneCandidates.empty())
            {
                break;
            }
            if (cell.team >= 0 && cell.team != team)
            {
                continue;
            }
            if (!cell.walkable || cell.occupied)
            {
                continue;
            }
            if (!usedCloneCells.insert({ cell.x, cell.y }).second)
            {
                continue;
            }

            const auto& source = cloneCandidates[spawned % cloneCandidates.size()];
            const auto& sourceSpawn = spawn(source.sourceUnitId);
            const auto& sourceUnit = sourceSpawn.unit;

            auto cloneCombo = KysChess::ChessBattleEffects::makeSummonedCloneState(sourceSpawn.combo);
            auto cloneUnit = makeCloneRuntimeUnit(
                sourceUnit,
                nextRuntimeUnitId,
                context_.gridTransform,
                cell);

            std::optional<BattleActionPlanSeed> cloneActionPlan;
            if (const auto* sourcePlan = sourceSpawn.actionPlan())
            {
                cloneActionPlan = *sourcePlan;
            }
            auto cloneSpawn = makeRuntimeUnitSpawn(
                std::move(cloneUnit),
                std::move(cloneCombo),
                std::move(cloneActionPlan));
            rebuildSpawnSkillEffects(cloneSpawn, setup_);

            result_.roleDeltas.push_back(makeRoleDelta(
                nextRuntimeUnitId,
                cloneSpawn.unit.star,
                cloneSpawn.unit.vitals,
                cloneSpawn.unit.stats));
            result_.logEvents.push_back({
                BattleLogEventType::Status,
                context_.frame,
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
            appendSpawn(std::move(cloneSpawn));

            ++nextRuntimeUnitId;
            ++spawned;
        }
    }
}

void BattleStartInitializationRun::applyEnemyTopDebuffs()
{
    result_.enemyTopDebuffs = applyEnemyTopDebuff(spawns_, context_.frame, result_.logEvents);
}

void BattleStartInitializationRun::appendSeededRoleDeltas()
{
    for (int unitId : seededUnitIds_)
    {
        const auto& unit = spawn(unitId).unit;
        const auto starStatsIt = starStatsByUnitId_.find(unitId);
        assert(starStatsIt != starStatsByUnitId_.end());
        result_.roleDeltas.push_back(makeRoleDelta(
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
}

}  // namespace

std::vector<ResolvedChessCombo> resolveBattleSetupCombos(
    const std::vector<BattleSetupRosterUnit>& roster,
    const BattleRuntimeSetupSeed& setup)
{
    return resolveBattleSetupCombosImpl(roster, setup);
}

BattleStartInitializer::BattleStartInitializer(
    std::vector<BattleRuntimeUnitSpawn> spawns,
    const BattleRuntimeSetupSeed& setup,
    BattleInitializationContext context)
    : spawns_(std::move(spawns))
    , setup_(setup)
    , context_(context)
{
}

BattleInitializationOutput BattleStartInitializer::initialize() &&
{
    return BattleStartInitializationRun(
        std::move(spawns_),
        setup_,
        std::move(context_))
        .run();
}


}  // namespace KysChess::Battle
