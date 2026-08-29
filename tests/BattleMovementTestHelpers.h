#pragma once

#include "battle/BattleGeometry.h"
#include "battle/BattleMath.h"
#include "battle/BattleMovement.h"
#include "Find.h"

#include <map>
#include <utility>
#include <vector>

namespace KysChess::Battle::Test
{

constexpr double SceneTileWidth = 36.0;
constexpr double MaxEffectiveBattleReach = 480.0;
constexpr double TestMinimumVectorNorm = 0.0001;
constexpr int BattleCoordCount = 64;

inline BattleMovementConfig testConfig()
{
    BattleMovementGeometry geometry;
    geometry.tileWidth = SceneTileWidth;
    geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    geometry.meleeAttackHitRadius = SceneTileWidth * 2.0;
    geometry.dashFrames = 5;
    geometry.dashCooldownFrames = 18;
    geometry.maxRangedReach = MaxEffectiveBattleReach;
    return BattleGeometry(geometry).movementConfig();
}

inline BattleUnitState unit(int id, int team, Pointf position, CombatStyle style = CombatStyle::Melee)
{
    BattleUnitState state;
    state.id = id;
    state.team = team;
    state.position = position;
    state.speed = 5.0;
    state.reach = style == CombatStyle::Ranged ? 400.0 : 137.5;
    state.style = style;
    return state;
}

inline BattleMovementPlanInput worldWith(std::vector<BattleUnitState> units)
{
    BattleMovementPlanInput world;
    world.config = testConfig();
    world.units.reserve(units.size());
    for (auto& unit : units)
    {
        world.units.push_back(std::move(unit));
    }
    return world;
}

struct MovementStats
{
    int consecutiveBlockedFrames = 0;
    int consecutiveAllyBlockedFrames = 0;
    int consecutiveWallBlockedFrames = 0;
    int consecutiveNoProgressFrames = 0;
    int totalAllyBlockedFrames = 0;
    int targetSwitches = 0;
    int slotSwitches = 0;
    int dashCount = 0;
    double lastDashDistance = 0.0;
    int attackReadyFrames = 0;
    int directionReversalCount = 0;
    int directionSharpTurnCount = 0;
    MoveBlockReason lastBlockReason = MoveBlockReason::None;
};

struct MovementRunResult
{
    BattleMovementPlanInput world;
    std::vector<BattleEvent> events;
    std::map<int, MovementStats> stats;
};

inline double dot2d(Pointf lhs, Pointf rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

inline void applyMovementDecisionToPlanWorld(BattleMovementPlanInput& world, const MovementDecision& decision)
{
    auto& unit = requireById(world.units, decision.unitId);
    unit.targetId = decision.targetId;
    unit.assignedSlot = decision.slot;
    unit.slotSwitchCooldownRemaining = decision.slotSwitchCooldownRemaining;
    unit.position = decision.destination;
    unit.velocity = decision.velocity;
    unit.dashFramesRemaining = decision.dashFramesRemaining;
    unit.dashCooldownRemaining = decision.dashCooldownRemaining;
    unit.movementDashSpreadFramesRemaining = decision.movementDashSpreadFramesRemaining;
    unit.postDashRetreatFramesRemaining = decision.postDashRetreatFramesRemaining;
    unit.postDashChaosFramesRemaining = decision.postDashChaosFramesRemaining;
    unit.knockbackFramesRemaining = decision.knockbackFramesRemaining;
    unit.knockbackControlFramesRemaining = decision.knockbackControlFramesRemaining;
}

inline void applyMovementTickToPlanWorld(BattleMovementPlanInput& world, const BattleTickResult& result)
{
    world.frame = result.frame;
    world.movementReservations = result.movementReservations;
    world.yieldRequests = result.yieldRequests;
    world.detourRequests = result.detourRequests;
    for (const auto& entry : result.decisions)
    {
        applyMovementDecisionToPlanWorld(world, entry.second);
    }
}

inline MovementRunResult runMovementPlanForFrames(BattleMovementPlanInput world, int frames)
{
    MovementRunResult run;
    std::map<int, Pointf> lastPositions;
    std::map<int, Pointf> lastDirections;
    std::map<int, int> lastTargets;
    std::map<int, int> lastSlots;
    for (const auto& unit : world.units)
    {
        lastPositions[unit.id] = unit.position;
        lastDirections[unit.id] = {};
        lastTargets[unit.id] = unit.targetId;
        lastSlots[unit.id] = unit.assignedSlot;
    }

    for (int i = 0; i < frames; ++i)
    {
        auto tickResult = BattleMovementPlanner(world).tick();
        applyMovementTickToPlanWorld(world, tickResult);
        run.events.insert(run.events.end(), tickResult.events.begin(), tickResult.events.end());
        for (const auto& event : tickResult.events)
        {
            if (event.type == BattleEventType::DashStart)
            {
                auto& stats = run.stats[event.unitId];
                stats.dashCount++;
                stats.lastDashDistance = event.value;
            }
        }
        for (const auto& unit : world.units)
        {
            auto& stats = run.stats[unit.id];
            auto dit = tickResult.decisions.find(unit.id);
            MovementDecision decision = dit != tickResult.decisions.end() ? dit->second : MovementDecision{ unit.id };
            if (decision.blockReason != MoveBlockReason::None
                || decision.action == MovementAction::Hold)
            {
                if (decision.blockReason != MoveBlockReason::None)
                {
                    stats.consecutiveBlockedFrames++;
                    stats.lastBlockReason = decision.blockReason;
                }
                if (decision.blockReason == MoveBlockReason::Ally
                    || decision.blockReason == MoveBlockReason::Reservation)
                {
                    stats.consecutiveAllyBlockedFrames++;
                    stats.totalAllyBlockedFrames++;
                }
                else
                {
                    stats.consecutiveAllyBlockedFrames = 0;
                }
                if (decision.blockReason == MoveBlockReason::Wall)
                {
                    stats.consecutiveWallBlockedFrames++;
                }
                else
                {
                    stats.consecutiveWallBlockedFrames = 0;
                }
            }
            else
            {
                stats.consecutiveBlockedFrames = 0;
                stats.consecutiveAllyBlockedFrames = 0;
                stats.consecutiveWallBlockedFrames = 0;
                stats.lastBlockReason = MoveBlockReason::None;
            }

            const bool madeTacticalProgress = decision.action == MovementAction::Move
                || decision.action == MovementAction::Dash
                || decision.action == MovementAction::AttackReady
                || pointDistance(unit.position, lastPositions[unit.id]) >= world.config.engagementDeadband / 4.0;
            if (!madeTacticalProgress)
            {
                stats.consecutiveNoProgressFrames++;
            }
            else
            {
                stats.consecutiveNoProgressFrames = 0;
            }
            if (lastTargets[unit.id] != unit.targetId)
            {
                stats.targetSwitches++;
                lastTargets[unit.id] = unit.targetId;
            }
            if (lastSlots[unit.id] != unit.assignedSlot)
            {
                stats.slotSwitches++;
                lastSlots[unit.id] = unit.assignedSlot;
            }
            if (decision.action == MovementAction::AttackReady)
            {
                stats.attackReadyFrames++;
            }
            if (pointDistance(unit.velocity, {}) > 0.01 && pointDistance(lastDirections[unit.id], {}) > 0.01)
            {
                auto currentDir = normalizedTo(unit.velocity, 1.0, 0.01);
                auto lastDir = normalizedTo(lastDirections[unit.id], 1.0, 0.01);
                const double turnDot = dot2d(currentDir, lastDir);
                if (turnDot < 0.5)
                {
                    stats.directionSharpTurnCount++;
                }
                if (turnDot < -0.5)
                {
                    stats.directionReversalCount++;
                }
            }
            lastDirections[unit.id] = unit.velocity;
            lastPositions[unit.id] = unit.position;
        }
    }
    run.world = std::move(world);
    return run;
}

}  // namespace KysChess::Battle::Test
