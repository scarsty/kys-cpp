#include "BattleRuntimeSession.h"

#include "BattleRuntimeUnitSpawn.h"

#include "../Find.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <ranges>
#include <utility>

namespace KysChess::Battle
{
namespace
{
constexpr int ProjectileGraceFrames = 5;

int getComboLookupId(const BattleRuntimeUnit& unit)
{
    if (unit.cloneSourceUnitId >= 0)
    {
        return -1;
    }
    return unit.realRoleId;
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
    BattleRuntimeUnits& records,
    const BattleRuntimeSetupSeed& setup)
{
    BattleDeathEffectStore store;
    for (const auto& combo : setup.comboDefinitions)
    {
        if (!combo.isAntiCombo)
        {
            store.regularSynergyComboIds.insert(combo.id);
        }
    }

    for (auto& record : records.all())
    {
        const auto& unit = record.core;
        auto& extras = record.deathEffects;
        extras.shieldPctMaxHp = (record.combo).sumAlways(EffectType::ShieldPctMaxHP);
        extras.appliedEffects.clear();
        for (KysChess::RoleComboEffectId effectId : record.combo.effectIdsInAppendOrder())
        {
            const auto& effect = record.combo.effect(effectId);
            if (effect.origin != RoleComboEffectOrigin::Configured)
            {
                continue;
            }
            extras.appliedEffects.push_back(effect);
        }

        const int comboLookupId = getComboLookupId(unit);
        if (comboLookupId >= 0)
        {
            for (const auto& combo : setup.comboDefinitions)
            {
                if (std::ranges::find(combo.memberRoleIds, comboLookupId) != combo.memberRoleIds.end())
                {
                    extras.comboIds.push_back(combo.id);
                }
            }
        }

    }

    return store;
}

std::vector<BattleRuntimeUnitSpawn> buildCanonicalSpawns(
    BattleRuntimeSessionCreationInput& input);

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
    BattleRuntimeSessionCreationInput& input)
{
    const auto actionPlans = makeActionPlanSeedMap(input.actionPlanSeeds);

    std::vector<BattleRuntimeUnitSpawn> spawns;
    spawns.reserve(input.units.size());
    for (auto& setup : input.units)
    {
        std::optional<BattleActionPlanSeed> actionPlan;
        if (const auto actionIt = actionPlans.find(setup.unitId);
            actionIt != actionPlans.end())
        {
            actionPlan = actionIt->second;
        }

        auto spawn = makeRuntimeUnitSpawn(
            makeRuntimeUnit(setup),
            std::move(setup.baseCombo),
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
    runtime.gridTransform = input.rules.gridTransform;
    runtime.random = BattleRuntimeRandom(input.randomSeed);
    runtime.units.reserve(spawns.size());

    for (auto& spawn : spawns)
    {
        appendRuntimeUnit(runtime, std::move(spawn));
    }
    return runtime;
}

void deriveRuntimeState(
    BattleRuntimeState& runtime,
    BattleRuntimeSessionCreationInput input)
{
    runtime.movement.frame = input.battleFrame;
    runtime.movement.config = input.rules.movementConfig;
    runtime.movement.terrainCells = std::move(input.terrainCells);

    configureAttackWorld(runtime.attacks, input.rules);
    runtime.teamEffects.healAuraRadius = input.rules.teamEffectHealAuraRadius;
    runtime.deathEffects.store = makeDeathEffectStore(runtime.units, input.setup);

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
    runtime.maximumFrames = input.rules.maximumFrames;

    runtime.damage.sortPendingDamageByDefenderMagnitude = true;
    runtime.profiling = input.profiling;
}

struct BattleRuntimeSetupResult
{
    BattleRuntimeState runtime;
    BattleInitializationResult initialization;
};

BattleRuntimeSetupResult setupBattleRuntime(BattleRuntimeSessionCreationInput input)
{
    auto initialized = BattleStartInitializer(
        buildCanonicalSpawns(input),
        input.setup,
        BattleInitializationContext{ input.rules.gridTransform, input.battleFrame })
        .initialize();
    auto runtime = buildRuntimeFromSpawns(input, std::move(initialized.spawns));
    deriveRuntimeState(runtime, std::move(input));

    return {
        std::move(runtime),
        std::move(initialized.result),
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
    auto frame = runner_.runFrame(runtime_);
    if (!runtime_.result.ended && runtime_.movement.frame >= runtime_.maximumFrames)
    {
        runtime_.result.ended = true;
        runtime_.result.winningTeam = 1;
        runtime_.result.endedFrame = runtime_.maximumFrames;
        runtime_.result.eventEmitted = true;
        runtime_.result.outcome = BattleOutcome::Timeout;
        frame.gameplayEvents.push_back({
            BattleGameplayEventType::BattleEnded,
            runtime_.maximumFrames,
            -1,
            -1,
            runtime_.result.winningTeam,
        });
        frame.logEvents.push_back({
            BattleLogEventType::BattleEnded,
            runtime_.maximumFrames,
            -1,
            -1,
            runtime_.result.winningTeam,
        });
    }
    return frame;
}


void BattleRuntimeSession::swapSetupUnitPositions(int firstUnitId, int secondUnitId)
{
    assert(!frameStarted_);
    auto& first = runtime_.units.requireCore(firstUnitId);
    auto& second = runtime_.units.requireCore(secondUnitId);
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
    return runtime_.units.requireCore(unitId);
}

}  // namespace KysChess::Battle
