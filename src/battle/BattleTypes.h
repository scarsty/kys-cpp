#pragma once

#include "../Point.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace KysChess::Battle
{

enum class CombatStyle
{
    Melee,
    Ranged,
};

enum class MoveBlockReason
{
    None,
    Wall,
    Ally,
    Enemy,
    Reservation,
};

enum class BattleEventType
{
    Movement,
    BlockedByAlly,
    BlockedByWall,
    SlotChanged,
    DashStart,
    DashEnd,
    AttackReady,
};

enum class MovementAction
{
    Hold,
    Move,
    Dash,
    AttackReady,
};

struct BattleMovementConfig
{
    double tileWidth = 50.0;
    int reservationHorizonFrames = 12;
    int dashFrames = 5;
    int dashCooldownFrames = 18;
    int slotSwitchCooldownFrames = 12;
    double bodyRadius = 75.0;
    double engagementDeadband = 25.0;
    double engagementArriveDistance = 37.5;
    double meleeAttackReach = 137.5;
    double meleeLocalTargetRadius = 237.5;
    double maxDashDistance = 237.5;
    double maxRangedReach = 480.0;
    double movementDashDistanceMultiplier = 2.0;
};

struct BattleMovementGeometry
{
    double tileWidth = 50.0;
    int reservationHorizonFrames = 12;
    int dashFrames = 5;
    int dashCooldownFrames = 18;
    int slotSwitchCooldownFrames = 12;
    double maxRangedReach = 480.0;
    double meleeAttackEffectOffset = 100.0;
    double meleeAttackHitRadius = 100.0;
};

struct BattleUnitState
{
    int id = -1;
    int realRoleId = -1;
    std::string name;
    int team = 0;
    bool alive = true;
    Pointf position;
    Pointf velocity;
    double speed = 0.0;
    int star = 1;
    double reach = 0.0;
    CombatStyle style = CombatStyle::Melee;
    bool taXue = false;
    bool dashAttack = false;
    bool canAttack = true;
    int targetId = -1;
    int assignedSlot = 0;
    int slotSwitchCooldownRemaining = 0;
    int dashFramesRemaining = 0;
    int dashCooldownRemaining = 0;
};

struct MoveProbe
{
    bool canMove = false;
    MoveBlockReason reason = MoveBlockReason::None;
    int blockerId = -1;
};

struct MovementDecision
{
    int unitId = -1;
    MovementAction action = MovementAction::Hold;
    Pointf velocity;
    Pointf destination;
    int targetId = -1;
    int slot = 0;
    MoveBlockReason blockReason = MoveBlockReason::None;
    int blockerId = -1;
    double dashDistance = 0.0;
};

struct BattleEvent
{
    BattleEventType type = BattleEventType::Movement;
    int unitId = -1;
    int targetId = -1;
    int blockerId = -1;
    int slot = 0;
    Pointf from;
    Pointf to;
    double value = 0.0;
};

struct BattleSnapshot
{
    int frame = 0;
    std::vector<BattleUnitState> units;
};

struct BattleTickResult
{
    BattleSnapshot snapshot;
    std::vector<BattleEvent> events;
    std::map<int, MovementDecision> decisions;
};

struct BattleWorldState
{
    int frame = 0;
    unsigned int seed = 1;
    BattleMovementConfig config;
    std::vector<BattleUnitState> units;
    std::function<bool(Pointf)> canStandAt;
};

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
    MoveBlockReason lastBlockReason = MoveBlockReason::None;
};

struct MovementRunResult
{
    BattleWorldState world;
    std::vector<BattleEvent> events;
    std::map<int, MovementStats> stats;
};

}  // namespace KysChess::Battle
