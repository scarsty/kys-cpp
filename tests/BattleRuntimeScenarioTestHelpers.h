#pragma once

#include "battle/BattleCore.h"
#include "battle/BattleRuntimeRules.h"
#include "battle/BattleRuntimeSession.h"
#include "BattleLogTestHelpers.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace KysChess::Battle::Test
{

constexpr double ScenarioTileWidth = 36.0;
constexpr int ScenarioCoordCount = 18;
constexpr double ScenarioMinimumVectorNorm = 0.0001;

struct BattleScenarioFrameDigest
{
    int frame{};
    bool battleEnded{};
    int winningTeam = -1;
    std::vector<int> aliveUnitIds;
    std::map<int, int> hpByUnitId;
    std::map<int, int> mpByUnitId;
    std::map<int, int> shieldByUnitId;
    std::size_t activeAttackCount{};
    std::size_t pendingAttackSpawnCount{};
    std::size_t pendingCastCount{};
    std::map<int, bool> haveActionByUnitId;
    std::map<int, BattleOperationType> operationTypeByUnitId;
    std::vector<BattleGameplayEventType> gameplayTypes;
    std::vector<BattleLogEventType> logTypes;
    std::vector<std::string> logTexts;
    std::vector<int> damageDefenderIds;
    std::vector<int> committedHpDamage;
    std::vector<BattleGameplayEventType> attackTypes;
    std::vector<int> activeAttackIds;
    std::map<int, int> projectileCancelWeakenByAttackId;
    std::vector<int> roleEffectTargetUnitIds;
};

inline BattleRuntimeRulesConfig scenarioRules()
{
    auto rules = makeHadesBattleRuntimeRules(ScenarioTileWidth, ScenarioCoordCount);
    rules.movementCollisionWorld.walkableByCell.assign(ScenarioCoordCount * ScenarioCoordCount, 1);
    rules.minimumVectorNorm = ScenarioMinimumVectorNorm;
    return rules;
}

inline BattleRuntimeUnit scenarioRuntimeUnit(int id, int team, int hp, Pointf position)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.realRoleId = id;
    unit.name = std::to_string(id);
    unit.team = team;
    unit.alive = hp > 0;
    unit.vitals = { hp, 100, 20, 100 };
    unit.stats = { 30, 5, 0 };
    unit.motion.position = position;
    unit.motion.facing = { 1, 0, 0 };
    return unit;
}

inline BattleSetupUnitInput scenarioSetupUnit(int id, int team, int hp, Pointf position)
{
    BattleSetupUnitInput unit;
    unit.unitId = id;
    unit.realRoleId = id;
    unit.name = std::to_string(id);
    unit.team = team;
    unit.alive = hp > 0;
    unit.vitals = { hp, 100, 20, 100 };
    unit.stats = { 30, 5, 0 };
    unit.motion.position = position;
    unit.motion.facing = { 1, 0, 0 };
    return unit;
}

inline BattleStatusRuntimeUnit scenarioStatusUnit(const BattleRuntimeUnit& unit)
{
    BattleStatusUnitState status;
    status.id = unit.id;
    status.alive = unit.alive;
    status.hp = unit.vitals.hp;
    status.maxHp = unit.vitals.maxHp;
    return makeBattleStatusRuntimeUnit(status);
}

inline BattleAttackState scenarioAttackState()
{
    BattleAttackState state;
    state.hitRadius = ScenarioTileWidth * 2.0;
    state.minimumVectorNorm = ScenarioMinimumVectorNorm;
    state.bounceSpawnDistance = ScenarioTileWidth * 1.5;
    state.defaultProjectileSpeed = ScenarioTileWidth / 3.0;
    state.spendNonThroughOnHit = true;
    return state;
}

inline void seedScenarioRuntimeStores(BattleRuntimeState& state, std::vector<BattleRuntimeUnit> units)
{
    state.unitStore.gridTransform = { ScenarioTileWidth, ScenarioCoordCount };
    state.movement.frame = 0;
    state.movement.config = scenarioRules().movementConfig;
    state.attacks = scenarioAttackState();
    state.unitStore.units = std::move(units);
    state.combo.units.clear();
    state.status.units.clear();
    state.damage.unitExtras.clear();
    state.deathEffects.store.units.clear();
    state.movement.agents.clear();

    for (const auto& unit : state.unitStore.units)
    {
        state.combo.units.emplace(unit.id, RoleComboState{});
        state.status.units.push_back(scenarioStatusUnit(unit));
        state.damage.unitExtras.push_back(makeBattleDamageRuntimeUnit(
            makeBattleDamageUnitState(unit, static_cast<const BattleDamageRuntimeUnit*>(nullptr))));
        state.deathEffects.store.units.push_back({ .id = unit.id });

        if (unit.alive)
        {
            BattleMovementAgentState agent;
            agent.physics.position = unit.motion.position;
            agent.physics.velocity = unit.motion.velocity;
            agent.physics.acceleration = unit.motion.acceleration;
            state.movement.agents.emplace(unit.id, agent);
        }
    }
}

inline BattlePendingDamageIntent scenarioPreResolvedDamage(int attackerUnitId, int defenderUnitId, int damage)
{
    BattlePendingDamageIntent intent;
    intent.request.attackerUnitId = attackerUnitId;
    intent.request.defenderUnitId = defenderUnitId;
    intent.request.baseDamage = damage;
    intent.request.preResolvedDamage = true;
    intent.presentation.enabled = true;
    intent.presentation.normalDamageTextSize = 30;
    intent.presentation.emphasizedDamageTextSize = 44;
    intent.presentation.normalDamageColor = { 255, 90, 79, 255 };
    intent.presentation.emphasizedDamageColor = { 255, 45, 85, 255 };
    return intent;
}

inline BattleAttackInstance scenarioCancelProjectile(int id, int attackerUnitId, int cancelDamage)
{
    BattleAttackInstance attack;
    attack.id = id;
    attack.state.attackerUnitId = attackerUnitId;
    attack.frame = 5;
    attack.state.totalFrame = 30;
    attack.state.position = { 500, 500, 0 };
    attack.state.operationType = BattleOperationType::RangedProjectile;
    attack.state.projectileCancelDamage = cancelDamage;
    return attack;
}

inline BattleRescueCellSnapshot scenarioRescueCell(int x, int y, bool walkable = true, bool occupied = false)
{
    BattleRescueCellSnapshot cell;
    cell.x = x;
    cell.y = y;
    cell.walkable = walkable;
    cell.occupied = occupied;
    cell.position = {
        static_cast<float>(x * ScenarioTileWidth),
        static_cast<float>(y * ScenarioTileWidth),
        0.0f,
    };
    return cell;
}

inline BattleScenarioFrameDigest digestScenarioFrame(
    const BattleRuntimeState& runtime,
    const BattleFrameResult& result)
{
    BattleScenarioFrameDigest digest;
    digest.frame = result.frame.frame;
    digest.battleEnded = runtime.result.ended;
    digest.winningTeam = runtime.result.winningTeam;
    digest.activeAttackCount = runtime.attacks.attacks.size();
    digest.pendingAttackSpawnCount = runtime.pendingAttackSpawns.size();
    digest.pendingCastCount = runtime.action.pendingCasts.size();

    for (const auto& unit : runtime.unitStore.units)
    {
        if (unit.alive)
        {
            digest.aliveUnitIds.push_back(unit.id);
        }
        digest.hpByUnitId.emplace(unit.id, unit.vitals.hp);
        digest.mpByUnitId.emplace(unit.id, unit.vitals.mp);
        digest.shieldByUnitId.emplace(unit.id, unit.shield);
        digest.haveActionByUnitId.emplace(unit.id, unit.haveAction);
        digest.operationTypeByUnitId.emplace(unit.id, unit.operationType);
    }
    std::ranges::sort(digest.aliveUnitIds);

    for (const auto& attack : runtime.attacks.attacks)
    {
        digest.activeAttackIds.push_back(attack.id);
        digest.projectileCancelWeakenByAttackId.emplace(
            attack.id,
            attack.state.projectileCancelWeaken);
    }
    std::ranges::sort(digest.activeAttackIds);

    for (const auto& event : result.frame.gameplayEvents)
    {
        digest.gameplayTypes.push_back(event.type);
        switch (event.type)
        {
        case BattleGameplayEventType::AttackSpawned:
        case BattleGameplayEventType::ProjectileMoved:
        case BattleGameplayEventType::ProjectileHit:
        case BattleGameplayEventType::ProjectileExpired:
        case BattleGameplayEventType::ProjectileCancelled:
            digest.attackTypes.push_back(event.type);
            break;
        default:
            break;
        }
    }
    for (const auto& event : result.frame.logEvents)
    {
        digest.logTypes.push_back(event.type);
        digest.logTexts.push_back(BattleLogTest::textOf(event));
    }
    for (const auto& transaction : result.damageTransactions)
    {
        if (transaction.finalHpDamage > 0)
        {
            digest.damageDefenderIds.push_back(transaction.defender.id);
            digest.committedHpDamage.push_back(transaction.finalHpDamage);
        }
    }
    for (const auto& event : result.frame.visualEvents)
    {
        if (event.type == BattleVisualEventType::RoleEffect)
        {
            digest.roleEffectTargetUnitIds.push_back(event.targetUnitId);
        }
    }

    return digest;
}

inline std::vector<BattleScenarioFrameDigest> runScenarioFrames(BattleRuntimeSession& session, int frameCount)
{
    assert(frameCount >= 0);

    std::vector<BattleScenarioFrameDigest> digests;
    digests.reserve(static_cast<std::size_t>(frameCount));
    for (int i = 0; i < frameCount; ++i)
    {
        auto result = session.runFrame();
        digests.push_back(digestScenarioFrame(session.runtime(), result));
    }
    return digests;
}

inline bool digestHasLogText(const BattleScenarioFrameDigest& digest, const std::string& text)
{
    return std::ranges::find(digest.logTexts, text) != digest.logTexts.end();
}

}  // namespace KysChess::Battle::Test
