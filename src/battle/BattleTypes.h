#pragma once

#include "../Point.h"

#include <map>
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
    double tileWidth{};
    int reservationHorizonFrames = 12;
    int dashFrames = 5;
    int dashCooldownFrames = 18;
    int slotSwitchCooldownFrames = 12;
    double bodyRadius{};
    double engagementDeadband{};
    double engagementArriveDistance{};
    double meleeAttackReach{};
    double meleeLocalTargetRadius{};
    double maxDashDistance{};
    double maxRangedReach{};
    double movementDashDistanceMultiplier{};
};

struct BattleMovementGeometry
{
    double tileWidth{};
    int reservationHorizonFrames = 12;
    int dashFrames = 5;
    int dashCooldownFrames = 18;
    int slotSwitchCooldownFrames = 12;
    double maxRangedReach{};
    double meleeAttackEffectOffset{};
    double meleeAttackHitRadius{};
};

struct BattleUnitState
{
    int id = -1;
    int team = 0;
    bool alive = true;
    Pointf position;
    Pointf velocity;
    double speed = 0.0;
    double reach = 0.0;
    CombatStyle style = CombatStyle::Melee;
    bool taXue = false;
    bool canAttack = true;
    int targetId = -1;
    int assignedSlot = 0;
    int slotSwitchCooldownRemaining = 0;
    int dashFramesRemaining = 0;
    int dashCooldownRemaining = 0;
    int movementDashSpreadFramesRemaining = 0;
    int postDashRetreatFramesRemaining = 0;
    int postDashChaosFramesRemaining = 0;
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
    int slotSwitchCooldownRemaining = 0;
    int dashFramesRemaining = 0;
    int dashCooldownRemaining = 0;
    int movementDashSpreadFramesRemaining = 0;
    int postDashRetreatFramesRemaining = 0;
    int postDashChaosFramesRemaining = 0;
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

struct BattleTerrainCell
{
    Pointf position;
    bool walkable = true;
};

struct BattleMovementReservation
{
    int unitId = -1;
    Pointf position;
    double radius = 0.0;
    int expiresFrame = 0;
};

struct BattleMovementPhysicsState
{
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    int movementDashFrames = 0;
    int movementDashCooldown = 0;
    int movementDashSpreadFrames = 0;
    Pointf postDashRetreatVelocity;
    int postDashRetreatFrames = 0;
    int postDashChaosFrames = 0;
};

struct BattleMovementAgentState
{
    bool active = true;
    int targetId = -1;
    int assignedSlot = 0;
    int slotSwitchCooldownRemaining = 0;
    BattleMovementPhysicsState physics;
};

struct BattleTickResult
{
    int frame = 0;
    std::vector<BattleEvent> events;
    std::map<int, MovementDecision> decisions;
    std::map<int, BattleMovementReservation> movementReservations;
};

struct BattleMovementPlanInput
{
    int frame = 0;
    BattleMovementConfig config;
    std::vector<BattleUnitState> units;
    std::vector<BattleTerrainCell> terrainCells;
    std::map<int, BattleMovementReservation> movementReservations;
};

struct BattleMovementState
{
    int frame = 0;
    unsigned int seed = 1;
    BattleMovementConfig config;
    std::vector<BattleTerrainCell> terrainCells;
    std::map<int, BattleMovementReservation> movementReservations;
};

}  // namespace KysChess::Battle
