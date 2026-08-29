#include "BattleInitialization.h"

#include "ChessComboResolver.h"
#include "BattleLogSegments.h"
#include "BattleRuntimeEffects.h"
#include "../BattleStarStats.h"
#include "../ChessBattleEffectTypes.h"
#include "../Find.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

namespace KysChess::Battle
{

namespace
{

template<class... Visitors>
struct Overloaded : Visitors...
{
    using Visitors::operator()...;
};

template<class... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

struct TeamResolvedSetup
{
    struct ActiveComboRules
    {
        int comboId = -1;
        bool isAntiCombo = false;
        std::set<int> memberRoleIds;
        std::vector<EffectRule> rules;
    };

    std::map<int, FightWinGrowthRule> fightWinGrowthByRealRoleId;
    std::vector<ActiveComboRules> activeComboRules;
};

std::string shieldLogText(const char* prefix, int shield)
{
    return std::format("{}{}護盾", prefix, shield);
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
        if (!equipment.countsAsComboNames.empty())
        {
            result.push_back({equipment.itemId, {}, equipment.countsAsComboNames});
        }
    }
    for (const auto& synergy : setup.equipmentSynergies)
    {
        if (!synergy.countsAsComboNames.empty())
        {
            result.push_back({synergy.equipmentId, synergy.roleIds, synergy.countsAsComboNames});
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
            auto& growth = resolved.fightWinGrowthByRealRoleId[roleId];
            growth.maxHp += threshold.fightWinGrowth.maxHp;
            growth.attack += threshold.fightWinGrowth.attack;
            growth.defence += threshold.fightWinGrowth.defence;
        }
        resolved.activeComboRules.push_back({
            .comboId = active.id,
            .isAntiCombo = active.isAntiCombo,
            .memberRoleIds = active.memberRoleIds,
            .rules = threshold.rules,
        });
    }

    return resolved;
}

void applyEquipmentEffects(
    BattleEffectRuleStore& rules,
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
        rules.append(
            {
                .kind = EffectSourceKind::Equipment,
                .sourceId = definition->itemId,
                .ownerUnitId = seed.unitId,
                .sourceTeam = seed.team,
            },
            definition->rules);
    };

    applyDefinition(rosterUnit->weaponId);
    applyDefinition(rosterUnit->armorId);
    for (const auto& synergy : setup.equipmentSynergies)
    {
        if (!equipmentSynergyActive(synergy, seed.realRoleId, rosterUnit->weaponId, rosterUnit->armorId))
        {
            continue;
        }
        rules.append(
            {
                .kind = EffectSourceKind::EquipmentSynergy,
                .sourceId = synergy.equipmentId,
                .ownerUnitId = seed.unitId,
                .sourceTeam = seed.team,
            },
            synergy.rules);
    }
}

void applyObtainedNeigongEffects(
    BattleEffectRuleStore& rules,
    const BattleRuntimeSetupSeed& setup,
    int team,
    int unitId)
{
    assert(team >= 0 && team < static_cast<int>(setup.obtainedNeigongMagicIdsByTeam.size()));
    for (int magicId : setup.obtainedNeigongMagicIdsByTeam[team])
    {
        const auto* definition = tryFindBy(setup.neigongDefinitions, magicId, &BattleSetupNeigongDefinition::magicId);
        if (!definition)
        {
            continue;
        }
        rules.append(
            {
                .kind = EffectSourceKind::Neigong,
                .sourceId = definition->magicId,
                .ownerUnitId = unitId,
                .sourceTeam = team,
            },
            definition->rules);
    }
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

BattleRuntimeUnitSpawn makeInitializedCloneSpawn(
    const BattleRuntimeUnitSpawn& initializedSource,
    int cloneUnitId,
    const BattleGridTransform& gridTransform,
    const BattleInitializationCloneSpawnCell& cell)
{
    // 來源是 BattleInitialized 完成後、任何戰鬥 frame 開始前擷取的
    // 不可變快照。先完整複製，新增的 unit runtime 欄位即可自然繼承。
    auto clone = initializedSource;
    clone.unit = makeCloneRuntimeUnit(
        initializedSource.unit,
        cloneUnitId,
        gridTransform,
        cell);
    clone.comboFacts.memberComboIds.clear();
    rewriteBattleStatusSourceUnitId(
        clone.status,
        initializedSource.unit.id,
        cloneUnitId);
    clone.movement = makeInitialMovementAgent(clone.unit);
    return clone;
}

struct BattleInitializedBaseline
{
    std::map<int, BattleRuntimeUnitSpawn> spawnsByUnitId;
};

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

class BattleStartInitializationRun
{
public:
    BattleStartInitializationRun(std::vector<BattleRuntimeUnitSpawn> spawns,
                                 const BattleRuntimeSetupSeed& setup,
                                 BattleInitializationContext context);

    BattleInitializationOutput run() &&;

private:
    void initializeSeededUnits();
    void bindActiveComboRules();
    void dispatchBattleInitializedRules();
    void captureInitializedBaseline();
    void summonClones();
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

    std::map<int, int> cloneCountByTeam() const;

    const TeamResolvedSetup& resolvedForTeam(int team) const;
    const std::vector<BattleSetupRosterUnit>& rosterForTeam(int team) const;

    std::vector<BattleRuntimeUnitSpawn> spawns_;
    const BattleRuntimeSetupSeed& setup_;
    BattleInitializationContext context_;
    TeamResolvedSetup allyResolved_;
    TeamResolvedSetup enemyResolved_;
    BattleInitializationResult result_;
    BattleEffectRuleStore effectRules_;
    BattleEffectCommandRuntimeState effectCommands_;
    std::set<int> activeAntiComboIds_;
    std::map<int, int> cloneCountByTeam_;
    std::set<int> cloneSourceOwnerUnitIds_;
    std::map<int, int> deathPreventionFramesByUnitId_;
    std::map<int, BattleRescueUnitRuntime> rescueByUnitId_;
    std::optional<BattleInitializedBaseline> initializedBaseline_;
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
    initializeSeededUnits();
    bindActiveComboRules();
    dispatchBattleInitializedRules();
    captureInitializedBaseline();
    summonClones();
    appendSeededRoleDeltas();

    return {
        std::move(spawns_),
        std::move(result_),
        std::move(effectRules_),
        std::move(effectCommands_),
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
        auto& comboFacts = spawn.comboFacts;

        for (const auto& combo : setup_.comboDefinitions)
        {
            if (std::ranges::find(combo.memberRoleIds, seed.realRoleId)
                != combo.memberRoleIds.end())
            {
                comboFacts.memberComboIds.insert(combo.id);
            }
        }

        const auto& resolved = resolvedForTeam(seed.team);
        const auto& roster = rosterForTeam(seed.team);
        const auto* rosterUnit = tryFindBy(roster, seed.unitId, &BattleSetupRosterUnit::unitId);
        int extraFightWinGrowthHP{};
        int extraFightWinGrowthATK{};
        int extraFightWinGrowthDEF{};
        if (const auto growth = resolved.fightWinGrowthByRealRoleId.find(seed.realRoleId);
            growth != resolved.fightWinGrowthByRealRoleId.end())
        {
            extraFightWinGrowthHP = growth->second.maxHp;
            extraFightWinGrowthATK = growth->second.attack;
            extraFightWinGrowthDEF = growth->second.defence;
        }
        applyEquipmentEffects(
            effectRules_,
            setup_,
            seed,
            roster);
        applyObtainedNeigongEffects(
            effectRules_,
            setup_,
            seed.team,
            seed.unitId);

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

        unit.vitals.maxHp = starBoostedStats.hp;
        unit.stats.attack = starBoostedStats.atk;
        unit.stats.defence = starBoostedStats.def;
        unit.stats.speed = starBoostedStats.spd;
        unit.realRoleId = seed.realRoleId;
        unit.team = seed.team;
        unit.star = seed.star;
        unit.cost = seed.cost;

        unit.vitals.hp = unit.vitals.maxHp;
        seededUnitIds_.push_back(seed.unitId);
    }
}

void BattleStartInitializationRun::bindActiveComboRules()
{
    const auto bindTeam = [&](int team, const TeamResolvedSetup& resolved)
    {
        for (const auto& active : resolved.activeComboRules)
        {
            if (active.isAntiCombo)
            {
                activeAntiComboIds_.insert(active.comboId);
            }
            std::vector<int> ownerUnitIds;
            for (const auto& seed : setup_.units)
            {
                if (seed.team == team
                    && active.memberRoleIds.contains(seed.realRoleId))
                {
                    ownerUnitIds.push_back(seed.unitId);
                }
            }
            assert(!ownerUnitIds.empty());
            std::ranges::sort(ownerUnitIds);
            for (int ownerUnitId : ownerUnitIds)
            {
                spawn(ownerUnitId).comboFacts.appliedComboIds.insert(
                    active.comboId);
            }

            for (const auto& rule : active.rules)
            {
                const bool bindOnceForTeam = rule.event == EffectEvent::BattleInitialized
                    && rule.selector.kind == EffectSelectorKind::Allies;
                const auto owners = bindOnceForTeam
                    ? std::span<const int>{ ownerUnitIds.data(), 1 }
                    : std::span<const int>{ ownerUnitIds };
                for (int ownerUnitId : owners)
                {
                    effectRules_.append(
                        {
                            .kind = EffectSourceKind::Combo,
                            .sourceId = active.comboId,
                            .ownerUnitId = ownerUnitId,
                            .sourceTeam = team,
                        },
                        rule);
                }
            }
        }
    };

    bindTeam(0, allyResolved_);
    bindTeam(1, enemyResolved_);
}

void BattleStartInitializationRun::dispatchBattleInitializedRules()
{
    const auto makeSnapshots = [&]
    {
        std::vector<EffectUnitSnapshot> result;
        result.reserve(seededUnitIds_.size());
        for (int unitId : seededUnitIds_)
        {
            const auto& seededSpawn = spawn(unitId);
            const auto& unit = seededSpawn.unit;
            result.push_back(makeEffectUnitSnapshot(
                unit,
                seededSpawn.comboFacts,
                seededSpawn.status.effects,
                seededSpawn.actionPlan()));
        }
        std::ranges::sort(result, {}, &EffectUnitSnapshot::id);
        return result;
    };
    auto snapshots = makeSnapshots();

    std::vector<EffectCommand> commands;
    BattleRuntimeRandom random(0);
    const BattleEffectReadView readView(
        snapshots,
        static_cast<float>(context_.gridTransform.tileWidth));
    std::uint64_t eventOrdinal = 1;
    for (const auto& owner : snapshots)
    {
        EffectEventContext event;
        event.event = EffectEvent::BattleInitialized;
        event.header.frame = context_.frame;
        event.header.eventOrdinal = eventOrdinal++;
        event.header.owner = &owner;
        event.header.battle = readView;
        event.payload = InitializationEventData{};
        auto dispatched = BattleEffectSystem().dispatch(
            effectRules_,
            event,
            random);
        commands.insert(
            commands.end(),
            std::make_move_iterator(dispatched.commands.begin()),
            std::make_move_iterator(dispatched.commands.end()));
    }

    struct AttributeTotals
    {
        int flat{};
        int percent{};
    };
    std::map<std::pair<int, BattleAttribute>, AttributeTotals> totals;
    const auto isAntiComboInitialization = [&](const EffectCommand& command)
    {
        return command.metadata.binding.kind == EffectSourceKind::Combo
            && activeAntiComboIds_.contains(command.metadata.binding.sourceId);
    };
    std::vector<std::optional<BattleAntiComboInitializationValue>>
        antiComboInitializationValues(
        commands.size());
    for (std::size_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex)
    {
        const auto& command = commands[commandIndex];
        const auto* attribute = std::get_if<ModifyAttributeEffectCommand>(&command.value);
        if (attribute)
        {
            if (isAntiComboInitialization(command))
            {
                antiComboInitializationValues[commandIndex] = *attribute;
            }
            const bool baseAttribute = attribute->action.attribute == BattleAttribute::MaxHp
                || attribute->action.attribute == BattleAttribute::Attack
                || attribute->action.attribute == BattleAttribute::Defence
                || attribute->action.attribute == BattleAttribute::Speed;
            if (baseAttribute)
            {
                auto& total = totals[{ command.metadata.targetUnitId, attribute->action.attribute }];
                switch (attribute->action.operation)
                {
                case AttributeOperation::FlatAdd:
                    total.flat += attribute->amount;
                    break;
                case AttributeOperation::PercentAdd:
                    total.percent += attribute->amount;
                    break;
                case AttributeOperation::Override:
                case AttributeOperation::Multiply:
                case AttributeOperation::AtLeast:
                    assert(false);
                    break;
                }
            }
            else
            {
                BattleEffectCommandSystem::applyPersistentAttributeModifier(
                    effectCommands_,
                    command.metadata,
                    *attribute,
                    context_.frame);
            }
            continue;
        }

        if (const auto* damage = std::get_if<ModifyDamageEffectCommand>(&command.value))
        {
            if (isAntiComboInitialization(command))
            {
                antiComboInitializationValues[commandIndex] = *damage;
            }
            BattleEffectCommandSystem::applyPersistentDamageModifier(
                effectCommands_,
                command.metadata,
                *damage,
                context_.frame);
            continue;
        }

        if (std::holds_alternative<ApplyStatusEffectCommand>(command.value))
        {
            continue;
        }

        const auto* stateMachine = std::get_if<StateMachineEffectCommand>(&command.value);
        if (!stateMachine)
        {
            assert(std::holds_alternative<ChangeResourceEffectCommand>(command.value));
            continue;
        }
        std::visit(Overloaded{
            [&](const GenerateClonesAction& action)
            {
                auto& teamCount = cloneCountByTeam_[command.metadata.binding.sourceTeam];
                teamCount = std::max(teamCount, action.count);
                cloneSourceOwnerUnitIds_.insert(command.metadata.binding.ownerUnitId);
            },
            [&](const PreventDeathAction& action)
            {
                auto& frames = deathPreventionFramesByUnitId_[command.metadata.targetUnitId];
                frames = std::max(frames, action.invincibilityFrames);
            },
            [&](const ConfigureRescueRepositionAction& action)
            {
                auto& rescue = rescueByUnitId_[command.metadata.targetUnitId];
                auto& remaining = action.mode == RescueRepositionMode::Protect
                    ? rescue.forcePullProtectRemaining
                    : rescue.forcePullExecuteRemaining;
                remaining += action.activations;
            },
            [](const auto&) { assert(false); },
        }, stateMachine->action);
    }

    const auto attributeValue = [&](int unitId, BattleAttribute attribute) -> int&
    {
        auto& unit = spawn(unitId).unit;
        switch (attribute)
        {
        case BattleAttribute::MaxHp: return unit.vitals.maxHp;
        case BattleAttribute::Attack: return unit.stats.attack;
        case BattleAttribute::Defence: return unit.stats.defence;
        case BattleAttribute::Speed: return unit.stats.speed;
        case BattleAttribute::CriticalChance:
        case BattleAttribute::CriticalDamage:
        case BattleAttribute::DodgeChance:
        case BattleAttribute::BlockChance:
        case BattleAttribute::DamageReduction:
        case BattleAttribute::SkillDamage:
        case BattleAttribute::ProjectilePressureDamage:
        case BattleAttribute::CooldownReduction:
        case BattleAttribute::MpRecoveryBonus:
        case BattleAttribute::StaggerResistance:
        case BattleAttribute::ProjectileReflectChance:
        case BattleAttribute::SkillReflectPercent:
        case BattleAttribute::CounterUltimateBlockChance:
        case BattleAttribute::CriticalAfterDodge:
        case BattleAttribute::DashChance:
        case BattleAttribute::OutgoingCooldownExtensionChance:
        case BattleAttribute::OutgoingCooldownExtensionPercent:
        case BattleAttribute::IncomingCooldownExtensionChance:
        case BattleAttribute::IncomingCooldownExtensionPercent:
            break;
        }
        assert(false);
        return unit.stats.attack;
    };

    // 初始化固定值先合計，再以同一份基準合計百分比並只取整一次。
    // 基準與合計會保留到 runtime，供反向羈絆轉移時沿用同一算法。
    constexpr std::array initializedCoreAttributes{
        BattleAttribute::MaxHp,
        BattleAttribute::Attack,
        BattleAttribute::Defence,
        BattleAttribute::Speed,
    };
    for (int unitId : seededUnitIds_)
    {
        const auto& comboFacts = spawn(unitId).comboFacts;
        const bool antiComboTransferEligible = std::ranges::any_of(
            activeAntiComboIds_,
            [&](int comboId)
            {
                return comboFacts.memberComboIds.contains(comboId)
                    || comboFacts.appliedComboIds.contains(comboId);
            });
        for (BattleAttribute attribute : initializedCoreAttributes)
        {
            const auto totalIt = totals.find({ unitId, attribute });
            const AttributeTotals total = totalIt != totals.end()
                ? totalIt->second
                : AttributeTotals{};
            const BattleAntiComboAttributeBasis basis{
                .baseValue = attributeValue(unitId, attribute),
                .flatTotal = total.flat,
                .percentTotal = total.percent,
            };
            attributeValue(unitId, attribute) =
                BattleEffectCommandSystem::antiComboAttributeValue(basis);
            if (antiComboTransferEligible)
            {
                const BattleAntiComboAttributeKey key{ unitId, attribute };
                const auto [_, inserted] =
                    effectCommands_.antiComboAttributeBases.emplace(key, basis);
                assert(inserted);
            }
        }
    }

    for (int unitId : seededUnitIds_)
    {
        auto& seededSpawn = spawn(unitId);
        seededSpawn.unit.vitals.hp = seededSpawn.unit.vitals.maxHp;
        refreshRuntimeUnitSpawnDerivedState(seededSpawn);
        if (const auto prevention = deathPreventionFramesByUnitId_.find(unitId);
            prevention != deathPreventionFramesByUnitId_.end())
        {
            seededSpawn.damage.deathPrevention = true;
            seededSpawn.damage.deathPreventionFrames = prevention->second;
        }
        if (const auto rescue = rescueByUnitId_.find(unitId); rescue != rescueByUnitId_.end())
        {
            seededSpawn.rescue = rescue->second;
        }
    }

    // 百分比初始資源必須讀取完成固定／百分比兩階段後的最大生命；
    // refreshRuntimeUnitSpawnDerivedState 會覆寫護盾與狀態資源，因此也必須先完成。
    snapshots = makeSnapshots();
    const BattleEffectReadView resourceReadView(
        snapshots,
        static_cast<float>(context_.gridTransform.tileWidth));
    for (std::size_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex)
    {
        const auto& command = commands[commandIndex];
        const auto* resource = std::get_if<ChangeResourceEffectCommand>(&command.value);
        if (!resource)
        {
            const auto* status = std::get_if<ApplyStatusEffectCommand>(&command.value);
            if (!status)
            {
                continue;
            }

            const auto* owner = resourceReadView.findUnit(command.metadata.binding.ownerUnitId);
            const auto* target = resourceReadView.findUnit(command.metadata.targetUnitId);
            assert(owner && target);
            EffectEventContext event;
            event.event = EffectEvent::BattleInitialized;
            event.header.frame = context_.frame;
            event.header.binding = command.metadata.binding;
            event.header.owner = owner;
            event.header.battle = resourceReadView;
            event.payload = InitializationEventData{};

            auto initializedStatus = *status;
            initializedStatus.potency = BattleEffectSystem::evaluateNumber(
                status->action.potency,
                event,
                *target);
            initializedStatus.secondaryPotency = BattleEffectSystem::evaluateNumber(
                status->action.secondaryPotency,
                event,
                *target);
            initializedStatus.evaluatedDurationFrames = status->action.duration
                ? std::optional<int>{ BattleEffectSystem::evaluateNumber(
                    *status->action.duration,
                    event,
                    *target) }
                : std::nullopt;

            auto& targetSpawn = spawn(command.metadata.targetUnitId);
            auto targetStatus = makeBattleStatusUnitState(
                targetSpawn.status,
                targetSpawn.unit);
            targetStatus.effects.freezeReductionPct =
                BattleEffectCommandSystem::queryAttribute(
                    effectCommands_,
                    {
                        .unitId = command.metadata.targetUnitId,
                        .attribute = BattleAttribute::StaggerResistance,
                        .baseValue = 0,
                        .frame = context_.frame,
                    });
            auto applied = BattleEffectCommandSystem::applyStatusCommand(
                std::move(targetStatus),
                command.metadata,
                initializedStatus,
                { .frame = context_.frame },
                {},
                targetSpawn.unit.shield > 0);
            writeBattleStatusRuntimeUnit(targetSpawn.status, applied.target);
            if (isAntiComboInitialization(command))
            {
                antiComboInitializationValues[commandIndex] =
                    std::move(initializedStatus);
            }
            continue;
        }
        assert(resource->action.kind != ResourceChangeKind::Drain
            && resource->action.kind != ResourceChangeKind::Transfer);

        const auto* owner = resourceReadView.findUnit(command.metadata.binding.ownerUnitId);
        const auto* target = resourceReadView.findUnit(command.metadata.targetUnitId);
        assert(owner && target);
        EffectEventContext event;
        event.event = EffectEvent::BattleInitialized;
        event.header.frame = context_.frame;
        event.header.binding = command.metadata.binding;
        event.header.owner = owner;
        event.header.battle = resourceReadView;
        event.payload = InitializationEventData{};
        const int amount = BattleEffectSystem::evaluateNumber(
            resource->action.amount,
            event,
            *target);
        assert(amount >= 0);
        auto initializedResource = *resource;
        initializedResource.amount = amount;
        if (isAntiComboInitialization(command))
        {
            antiComboInitializationValues[commandIndex] =
                std::move(initializedResource);
        }

        auto& targetSpawn = spawn(command.metadata.targetUnitId);
        const auto changedValue = [&](int before)
        {
            switch (resource->action.kind)
            {
            case ResourceChangeKind::Restore:
            case ResourceChangeKind::Grant:
                return static_cast<std::int64_t>(before) + amount;
            case ResourceChangeKind::Remove:
                return static_cast<std::int64_t>(before) - amount;
            case ResourceChangeKind::RefreshToAtLeast:
                return static_cast<std::int64_t>(std::max(before, amount));
            case ResourceChangeKind::Drain:
            case ResourceChangeKind::Transfer:
                break;
            }
            assert(false);
            return std::int64_t{};
        };
        const auto clampNonnegativeInt = [](std::int64_t value)
        {
            return static_cast<int>(std::clamp(
                value,
                std::int64_t{},
                static_cast<std::int64_t>(std::numeric_limits<int>::max())));
        };

        int before{};
        int after{};
        switch (resource->action.resource)
        {
        case BattleResource::Hp:
            assert(false && "戰鬥初始化不可變更生命");
            break;
        case BattleResource::Mp:
            before = targetSpawn.unit.vitals.mp;
            targetSpawn.unit.vitals.mp = std::clamp(
                clampNonnegativeInt(changedValue(before)),
                0,
                targetSpawn.unit.vitals.maxMp);
            after = targetSpawn.unit.vitals.mp;
            break;
        case BattleResource::Shield:
            before = targetSpawn.unit.shield;
            targetSpawn.unit.shield = clampNonnegativeInt(changedValue(before));
            after = targetSpawn.unit.shield;
            break;
        case BattleResource::StatusShield:
            before = targetSpawn.status.effects.statusShield;
            targetSpawn.status.effects.statusShield = clampNonnegativeInt(changedValue(before));
            after = targetSpawn.status.effects.statusShield;
            break;
        case BattleResource::StaggerShield:
            before = targetSpawn.status.effects.staggerShield;
            targetSpawn.status.effects.staggerShield = clampNonnegativeInt(changedValue(before));
            after = targetSpawn.status.effects.staggerShield;
            break;
        case BattleResource::ActiveCooldown:
            assert(false && "戰鬥初始化不可變更目前冷卻");
            break;
        case BattleResource::ControlImmunityFrames:
            before = targetSpawn.status.effects.controlImmunityFrames;
            targetSpawn.status.effects.controlImmunityFrames = clampNonnegativeInt(changedValue(before));
            after = targetSpawn.status.effects.controlImmunityFrames;
            break;
        case BattleResource::InvincibilityFrames:
            before = targetSpawn.unit.invincible;
            targetSpawn.unit.invincible = clampNonnegativeInt(changedValue(before));
            after = targetSpawn.unit.invincible;
            break;
        }
        if (resource->action.resource == BattleResource::Shield && after > before)
        {
            result_.logEvents.push_back({
                BattleLogEventType::Status,
                context_.frame,
                command.metadata.binding.ownerUnitId,
                command.metadata.targetUnitId,
                after - before,
                BattleLogCategory::Status,
                BattleLogPerspective::Targeted,
                battleLogText(
                    shieldLogText("獲取", after - before),
                    BattleLogTextTone::ShieldValue),
            });
        }
    }

    for (std::size_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex)
    {
        auto& value = antiComboInitializationValues[commandIndex];
        if (!value)
        {
            continue;
        }
        BattleEffectCommandSystem::recordAntiComboInitialization(
            effectCommands_,
            commands[commandIndex].metadata,
            std::move(*value));
    }
}

void BattleStartInitializationRun::captureInitializedBaseline()
{
    assert(!initializedBaseline_);
    BattleInitializedBaseline baseline;
    for (int unitId : seededUnitIds_)
    {
        const auto [_, inserted] = baseline.spawnsByUnitId.emplace(
            unitId,
            spawn(unitId));
        assert(inserted);
    }
    initializedBaseline_ = std::move(baseline);
}

std::map<int, int> BattleStartInitializationRun::cloneCountByTeam() const
{
    return cloneCountByTeam_;
}

void BattleStartInitializationRun::summonClones()
{
    assert(initializedBaseline_);
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
            if (!cloneSourceOwnerUnitIds_.contains(source.sourceUnitId))
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
            const auto baselineIt = initializedBaseline_->spawnsByUnitId.find(
                source.sourceUnitId);
            assert(baselineIt != initializedBaseline_->spawnsByUnitId.end());
            const auto& initializedSource = baselineIt->second;
            auto cloneSpawn = makeInitializedCloneSpawn(
                initializedSource,
                nextRuntimeUnitId,
                context_.gridTransform,
                cell);
            effectRules_.appendClonedOwnerRules(
                source.sourceUnitId,
                nextRuntimeUnitId,
                team);
            BattleEffectCommandSystem::inheritCloneEffectModifiers(
                effectCommands_,
                source.sourceUnitId,
                nextRuntimeUnitId,
                team);

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
