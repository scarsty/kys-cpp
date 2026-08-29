#pragma once

#include "BattleMovementTestHelpers.h"
#include "battle/BattleAreaEffectSystem.h"
#include "battle/BattleAttackSystem.h"
#include "battle/BattleDamageQueue.h"
#include "battle/BattleEffectCommandSystem.h"
#include "battle/BattleRuntimeUnitSpawn.h"
#include "battle/BattleRuntimeUnits.h"

#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace KysChess::Battle::Test
{

constexpr double SceneAttackHitRadius = SceneTileWidth * 2.0;
constexpr double SceneBounceSpawnDistance = SceneTileWidth * 1.5;
constexpr double SceneProjectileSpeed = SceneTileWidth / 3.0;

inline BattleAttackState attackWorld()
{
    BattleAttackState world;
    world.hitRadius = SceneAttackHitRadius;
    world.minimumVectorNorm = TestMinimumVectorNorm;
    world.bounceSpawnDistance = SceneBounceSpawnDistance;
    world.defaultProjectileSpeed = SceneProjectileSpeed;
    return world;
}

inline BattleRuntimeUnit runtimeUnitSnapshot(int id, int team, int hp, Pointf position = {})
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = hp > 0;
    unit.vitals = { hp, 100, 20, 50 };
    unit.stats = { 30, 5, 20 };
    unit.animation = { 10, 60, 0, 0 };
    unit.physicalPower = 3;
    unit.motion.position = position;
    unit.motion.facing = { 1, 0, 0 };
    return unit;
}

inline void seedRuntimeUnitsFromMovementUnits(
    BattleRuntimeState& state,
    std::span<const BattleUnitState> units,
    int hp = 100)
{
    if (state.gridTransform.tileWidth <= 0.0)
    {
        state.gridTransform = { SceneTileWidth, 64 };
    }
    state.units = {};
    state.damage.presentationStylesByDefender.clear();
    for (const auto& unit : units)
    {
        auto runtime = runtimeUnitSnapshot(unit.id, unit.team, hp, unit.position);
        runtime.alive = unit.alive;
        runtime.motion.velocity = unit.velocity;
        runtime.style = unit.style;
        runtime.grid = state.gridTransform.toGrid(runtime.motion.position);
        appendRuntimeUnit(
            state,
            makeRuntimeUnitSpawn(std::move(runtime)));
    }
}

inline void seedRuntimeUnits(BattleRuntimeState& state, std::vector<BattleRuntimeUnit> units)
{
    state.units = {};
    state.damage.presentationStylesByDefender.clear();

    for (auto& unit : units)
    {
        appendRuntimeUnit(
            state,
            makeRuntimeUnitSpawn(std::move(unit)));
    }
}

inline void configureRuntimeMovement(BattleRuntimeState& state, BattleMovementPlanInput input)
{
    state.movement.frame = input.frame;
    state.movement.config = input.config;
    state.movement.terrainCells = std::move(input.terrainCells);
    state.movement.terrainLayout = makeBattleMovementTerrainLayout(
        state.movement.terrainCells,
        state.movement.config.tileWidth);
    state.movement.movementReservations = std::move(input.movementReservations);
    seedRuntimeUnitsFromMovementUnits(state, input.units);
}

inline void seedRuntimeUnitsFromWorld(BattleRuntimeState& state, int hp = 100)
{
    std::vector<BattleUnitState> units;
    units.reserve(state.units.size());
    for (const auto& runtime : state.units.cores())
    {
        auto unit = makeBattleMovementPlanUnit(runtime, BattleRuntimeMoveSpeedDivisor);
        unit.reach = runtime.reach;
        unit.style = runtime.style;
        units.push_back(unit);
    }
    seedRuntimeUnitsFromMovementUnits(state, units, hp);
}

inline BattlePendingDamageIntent pendingDamageIntent(
    BattleDamageTransactionInput transaction,
    BattleDamagePresentationInput presentation = {},
    std::optional<EffectDamageOrigin> effectOrigin = std::nullopt)
{
    BattlePendingDamageIntent result;
    result.request = transaction.request;
    result.presentation = std::move(presentation);
    if (effectOrigin)
    {
        result.effectOrigin = std::move(*effectOrigin);
    }
    return result;
}

inline void addTypedAttributeModifier(
    BattleRuntimeState& state,
    int unitId,
    BattleAttribute attribute,
    AttributeOperation operation,
    int amount)
{
    state.effectCommands.attributeModifiers.push_back({
        .sequence = state.effectCommands.nextAttributeSequence++,
        .targetUnitId = unitId,
        .attribute = attribute,
        .operation = operation,
        .amount = amount,
        .appliedFrame = state.movement.frame,
    });
}

inline Pointf areaGridWorldPosition(const BattleGridTransform& transform, int x, int y)
{
    return {
        static_cast<float>((-y + x + transform.coordCount) * transform.tileWidth),
        static_cast<float>((y + x) * transform.tileWidth),
        0.0f,
    };
}

inline AreaModifier areaSpeedModifier()
{
    AreaModifier modifier;
    modifier.kind = AreaModifierKind::Attribute;
    modifier.relation = EffectTeamFilter::Enemy;
    modifier.attribute = BattleAttribute::Speed;
    modifier.amount.flat = -25;
    return modifier;
}

inline BattleAreaCreateRequest fixedCircleAreaRequest(
    int ownerUnitId,
    int sourceTeam,
    EffectRuleId ruleId,
    Pointf center,
    int currentFrame,
    int durationFrames,
    AreaMergePolicy merge = AreaMergePolicy::RefreshSameSource)
{
    BattleAreaCreateRequest request;
    request.source = { EffectSourceKind::Magic, 1000 + ownerUnitId, ownerUnitId, sourceTeam };
    request.ruleId = ruleId;
    request.geometry = { AreaShape::Circle, 6, 0 };
    request.anchor = { BattleAreaAnchorKind::FixedWorldPosition, center, -1 };
    request.currentFrame = currentFrame;
    request.durationFrames = durationFrames;
    request.sourceDeath = AreaSourceDeathPolicy::PersistUntilExpiry;
    request.merge = merge;
    request.modifiers = { areaSpeedModifier() };
    return request;
}

}  // namespace KysChess::Battle::Test
