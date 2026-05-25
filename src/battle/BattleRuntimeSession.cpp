#include "BattleRuntimeSession.h"

#include "BattleRuntimeUnitSpawn.h"

#include "../ChessCombo.h"
#include "../ChessEquipment.h"
#include "../ChessNeigong.h"
#include "../Find.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <utility>

namespace KysChess::Battle
{
namespace
{
constexpr int ProjectileGraceFrames = 5;

int getComboLookupId(int realRoleId, const RoleComboState& state)
{
    if (state.isSummonedClone)
    {
        return -1;
    }
    return realRoleId;
}

void configureAttackWorld(
    BattleAttackState& world,
    const BattleRuntimeRulesConfig& rules)
{
    world.hitRadius = rules.action.meleeAttackHitRadius;
    world.minimumVectorNorm = rules.minimumVectorNorm;
    world.projectileGraceFrames = ProjectileGraceFrames;
    world.bounceSpawnDistance = rules.projectileFollowUps.areaSpawnDistance;
    world.defaultProjectileSpeed = rules.projectileFollowUps.projectileSpeed;
    world.spendNonThroughOnHit = true;
}

BattleRuntimeUnit makeRuntimeUnit(const BattleSetupUnitInput& setup)
{
    assert(setup.unitId >= 0);

    BattleRuntimeUnit unit;
    unit.id = setup.unitId;
    unit.realRoleId = setup.realRoleId;
    unit.name = setup.name;
    unit.headId = setup.headId;
    unit.fightFrames = setup.fightFrames;
    unit.skillNames = setup.skillNames;
    unit.team = setup.team;
    unit.alive = setup.alive;
    unit.vitals = setup.vitals;
    unit.stats = setup.stats;
    unit.motion = setup.motion;
    unit.animation = setup.animation;
    unit.haveAction = setup.haveAction;
    unit.operationType = setup.operationType;
    unit.operationCount = setup.operationCount;
    unit.physicalPower = setup.physicalPower;
    unit.invincible = setup.invincible;
    unit.star = setup.star;
    unit.cost = setup.cost;
    unit.weaponId = setup.weaponId;
    unit.armorId = setup.armorId;
    unit.chessInstanceId = setup.chessInstanceId;
    for (int magicType = 0; magicType <= 4; ++magicType)
    {
        unit.actPropertiesByMagicType.emplace(magicType, setup.actPropertiesByMagicType[magicType]);
    }
    unit.grid = { setup.gridX, setup.gridY };
    return unit;
}

BattleDeathEffectStore makeDeathEffectStore(
    const BattleUnitStore& units,
    const std::map<int, RoleComboState>& states)
{
    BattleDeathEffectStore store;
    const auto& allCombos = KysChess::ChessCombo::getAllCombos();
    for (int comboId = 0; comboId < static_cast<int>(allCombos.size()); ++comboId)
    {
        if (!allCombos[comboId].isAntiCombo)
        {
            store.regularSynergyComboIds.insert(comboId);
        }
    }

    for (const auto& unit : units.units)
    {
        const auto stateIt = states.find(unit.id);
        assert(stateIt != states.end());

        BattleDeathEffectExtras extras;
        extras.id = unit.id;
        extras.shieldPctMaxHp = sumAlwaysEffectValue(stateIt->second, EffectType::ShieldPctMaxHP);
        extras.shieldOnAllyDeathTracker = stateIt->second.shieldOnAllyDeathTracker;
        extras.appliedEffects = stateIt->second.appliedEffects;

        const int comboLookupId = getComboLookupId(unit.realRoleId, stateIt->second);
        if (comboLookupId >= 0)
        {
            extras.comboIds = KysChess::ChessCombo::getCombosForRole(comboLookupId);
        }

        store.units.push_back(std::move(extras));
    }

    return store;
}

std::vector<BattleSetupComboDefinition> makeBattleSetupComboDefinitions()
{
    std::vector<BattleSetupComboDefinition> definitions;
    for (const auto& combo : KysChess::ChessCombo::getAllCombos())
    {
        BattleSetupComboDefinition definition;
        definition.id = combo.id;
        definition.name = combo.name;
        definition.memberRoleIds = combo.memberRoleIds;
        definition.isAntiCombo = combo.isAntiCombo;
        definition.starSynergyBonus = combo.starSynergyBonus;
        definition.thresholds.reserve(combo.thresholds.size());
        for (const auto& threshold : combo.thresholds)
        {
            definition.thresholds.push_back({ threshold.count, threshold.effects });
        }
        definitions.push_back(std::move(definition));
    }
    return definitions;
}

std::vector<BattleSetupEquipmentDefinition> makeBattleSetupEquipmentDefinitions()
{
    std::vector<BattleSetupEquipmentDefinition> definitions;
    for (const auto& equipment : KysChess::ChessEquipment::getAll())
    {
        definitions.push_back({
            equipment.itemId,
            equipment.equipType,
            equipment.effects,
            equipment.actAsComboNames,
        });
    }
    return definitions;
}

std::vector<BattleSetupEquipmentSynergyDefinition> makeBattleSetupEquipmentSynergyDefinitions()
{
    std::vector<BattleSetupEquipmentSynergyDefinition> definitions;
    for (const auto& synergy : KysChess::ChessEquipment::getAllSynergies())
    {
        definitions.push_back({
            synergy.roleIds,
            synergy.equipmentId,
            synergy.effects,
            synergy.actAsComboNames,
        });
    }
    return definitions;
}

std::vector<BattleSetupNeigongDefinition> makeBattleSetupNeigongDefinitions()
{
    std::vector<BattleSetupNeigongDefinition> definitions;
    for (const auto& neigong : KysChess::ChessNeigong::getPool())
    {
        definitions.push_back({ neigong.magicId, neigong.effects });
    }
    return definitions;
}

void populateBattleRuntimeSetupDefinitions(BattleRuntimeSetupSeed& setup)
{
    setup.comboDefinitions = makeBattleSetupComboDefinitions();
    setup.equipmentDefinitions = makeBattleSetupEquipmentDefinitions();
    setup.equipmentSynergies = makeBattleSetupEquipmentSynergyDefinitions();
    setup.neigongDefinitions = makeBattleSetupNeigongDefinitions();
}

void applyCreationComboStatesToSetup(BattleRuntimeSessionCreationInput& input)
{
    for (auto& seed : input.setup.units)
    {
        if (const auto comboIt = input.comboStates.find(seed.unitId);
            comboIt != input.comboStates.end())
        {
            seed.baseCombo = comboIt->second;
        }
    }
}

std::vector<BattleRuntimeUnitSpawn> buildCanonicalSpawns(
    const BattleRuntimeSessionCreationInput& input);

BattleRuntimeState buildRuntimeFromSpawns(
    const BattleRuntimeSessionCreationInput& input,
    std::vector<BattleRuntimeUnitSpawn> spawns);

std::map<int, BattleActionPlanSeed> makeActionPlanSeedMap(
    const std::vector<BattleActionPlanSeed>& seeds)
{
    std::map<int, BattleActionPlanSeed> result;
    for (const auto& seed : seeds)
    {
        result.emplace(seed.unitId, seed);
    }
    return result;
}

std::vector<BattleRuntimeUnitSpawn> buildCanonicalSpawns(
    const BattleRuntimeSessionCreationInput& input)
{
    const auto actionPlans = makeActionPlanSeedMap(input.actionPlanSeeds);

    std::vector<BattleRuntimeUnitSpawn> spawns;
    spawns.reserve(input.units.size());
    for (const auto& setup : input.units)
    {
        RoleComboState combo;
        if (const auto comboIt = input.comboStates.find(setup.unitId);
            comboIt != input.comboStates.end())
        {
            combo = comboIt->second;
        }

        std::optional<BattleActionPlanSeed> actionPlan;
        if (const auto actionIt = actionPlans.find(setup.unitId);
            actionIt != actionPlans.end())
        {
            actionPlan = actionIt->second;
        }

        auto spawn = makeRuntimeUnitSpawn(
            makeRuntimeUnit(setup),
            std::move(combo),
            std::move(actionPlan));
        spawn.status.effects.frozenTimer = setup.frozen;
        spawn.status.effects.frozenMaxTimer = setup.frozenMax;
        spawns.push_back(std::move(spawn));
    }
    return spawns;
}

BattleRuntimeState buildRuntimeFromSpawns(
    const BattleRuntimeSessionCreationInput& input,
    std::vector<BattleRuntimeUnitSpawn> spawns)
{
    BattleRuntimeState runtime;
    runtime.unitStore.gridTransform = input.rules.gridTransform;
    runtime.random = BattleRuntimeRandom(input.randomSeed);
    runtime.unitStore.units.reserve(spawns.size());
    runtime.status.units.reserve(spawns.size());
    runtime.damage.unitExtras.reserve(spawns.size());

    for (auto& spawn : spawns)
    {
        appendRuntimeUnit(runtime, std::move(spawn));
    }
    return runtime;
}

void refreshDeathAoeDamageEffects(BattleRuntimeState& runtime)
{
    runtime.damage.unitEffects.clear();
    for (const auto& unit : runtime.unitStore.units)
    {
        const auto stateIt = runtime.combo.units.find(unit.id);
        if (stateIt == runtime.combo.units.end())
        {
            continue;
        }

        const auto* deathAoe = firstAlwaysEffect(stateIt->second, EffectType::DeathAOE);
        if (deathAoe && deathAoe->value > 0)
        {
            runtime.damage.unitEffects.emplace(
                unit.id,
                BattleDamageApplicationUnitEffects{
                    deathAoe->value,
                    deathAoe->duration,
                    deathAoe->value2,
                });
        }
    }
}

void deriveRuntimeStores(
    BattleRuntimeState& runtime,
    BattleRuntimeSessionCreationInput input)
{
    runtime.movement.frame = input.battleFrame;
    runtime.movement.config = input.rules.movementConfig;
    runtime.movement.terrainCells = std::move(input.terrainCells);

    configureAttackWorld(runtime.attacks, input.rules);
    runtime.teamEffects.healAuraRadius = input.rules.teamEffectHealAuraRadius;
    runtime.deathEffects.store = makeDeathEffectStore(runtime.unitStore, runtime.combo.units);

    runtime.rescue.cells = std::move(input.rescueCells);
    runtime.rescue.executeUnattendedRadius = input.rules.rescueExecuteUnattendedRadius;
    runtime.rescue.counterAttack = input.rules.rescueCounterAttack;
    runtime.rescue.counterAttack.skillId = input.rescueCounterAttackSkillId;

    runtime.movementPhysics.config = input.rules.movementPhysicsConfig;
    runtime.movementPhysics.terrain.tileWidth = input.rules.movementCollisionWorld.tileWidth;
    runtime.movementPhysics.terrain.coordCount = input.rules.movementCollisionWorld.coordCount;
    runtime.movementPhysics.terrain.defaultSeparationDistance =
        input.rules.movementCollisionWorld.defaultSeparationDistance;
    runtime.movementPhysics.terrain.walkableByCell = input.rules.movementCollisionWorld.walkableByCell;
    runtime.movementPhysics.actionCastFrames.assign(
        input.rules.castConfig.castFrames.begin(),
        input.rules.castConfig.castFrames.end());
    runtime.movementPhysics.dashMomentumFrames = input.rules.movementPhysicsDashMomentumFrames;

    runtime.action.castFrames.assign(
        input.rules.castConfig.castFrames.begin(),
        input.rules.castConfig.castFrames.end());
    runtime.action.actionRecoveryFrames = input.rules.action.actionRecoveryFrames;
    runtime.action.dashRecoveryFrames = input.rules.action.dashRecoveryFrames;
    runtime.action.blinkWeakTargetDefWeight = input.rules.action.blinkWeakTargetDefWeight;
    runtime.action.strengthenedMeleeOperationCountThreshold = input.rules.action.strengthenedMeleeOperationCountThreshold;
    runtime.action.projectileBounceRange = input.rules.action.projectileBounceRange;
    runtime.action.castConfig = input.rules.castConfig;
    runtime.action.castGeometry = input.rules.castGeometry;
    runtime.action.actionRules = input.rules.action;

    runtime.projectileFollowUps = input.rules.projectileFollowUps;

    runtime.damage.sortPendingDamageByDefenderMagnitude = true;
    refreshDeathAoeDamageEffects(runtime);
}

struct BattleRuntimeSetupResult
{
    BattleRuntimeState runtime;
    BattleInitializationResult initialization;
};

BattleRuntimeSetupResult setupBattleRuntime(BattleRuntimeSessionCreationInput input)
{
    populateBattleRuntimeSetupDefinitions(input.setup);
    applyCreationComboStatesToSetup(input);

    auto spawns = buildCanonicalSpawns(input);
    auto initialization = BattleInitializationSystem().initialize(
        spawns,
        input.setup,
        BattleInitializationContext{ input.rules.gridTransform, input.battleFrame });
    auto runtime = buildRuntimeFromSpawns(input, std::move(spawns));
    deriveRuntimeStores(runtime, std::move(input));

    return {
        std::move(runtime),
        std::move(initialization),
    };
}

}  // namespace

BattleRuntimeSession::BattleRuntimeSession(BattleRuntimeState runtime)
    : runtime_(std::move(runtime))
{
}

BattleRuntimeSessionCreationResult BattleRuntimeSession::createInitialized(BattleRuntimeSessionCreationInput input)
{
    auto setup = setupBattleRuntime(std::move(input));

    return {
        BattleRuntimeSession(std::move(setup.runtime)),
        std::move(setup.initialization),
    };
}

BattlePresentationFrame BattleRuntimeSession::runFrame()
{
    frameStarted_ = true;
    // BattleFrameRunner is the single writer for runtime state; BattlePresentationFrame is the presentation report.
    return runner_.runFrame(runtime_);
}


void BattleRuntimeSession::swapSetupUnitPositions(int firstUnitId, int secondUnitId)
{
    assert(!frameStarted_);
    auto& first = runtime_.unitStore.requireUnit(firstUnitId);
    auto& second = runtime_.unitStore.requireUnit(secondUnitId);
    std::swap(first.grid, second.grid);
    std::swap(first.motion.position, second.motion.position);
    first.motion.velocity = { 0, 0, 0 };
    second.motion.velocity = { 0, 0, 0 };
}

const BattleRuntimeState& BattleRuntimeSession::runtime() const
{
    return runtime_;
}

const BattleRuntimeUnit& BattleRuntimeSession::requireRuntimeUnit(int unitId) const
{
    return runtime_.unitStore.requireUnit(unitId);
}

std::span<const BattleRuntimeUnit> BattleRuntimeSession::runtimeUnits() const
{
    return runtime_.unitStore.units;
}

}  // namespace KysChess::Battle
