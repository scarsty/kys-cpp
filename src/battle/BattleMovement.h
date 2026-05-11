#pragma once

#include "BattleGeometry.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace KysChess::Battle
{

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

struct BattleMovementPhysicsConfig
{
    float gravity = -4.0f;
    float friction = 0.1f;
    int postDashSpreadFrames = 0;
};

struct BattleMovementPhysicsCollisionUnitSnapshot
{
    int id = -1;
    bool alive = true;
    Pointf position;
};

struct BattleMovementPhysicsCollisionWorld
{
    double tileWidth = 0.0;
    int coordCount = 0;
    double defaultSeparationDistance = 0.0;
    std::vector<BattleMovementPhysicsCollisionUnitSnapshot> units;
    std::vector<std::uint8_t> walkableByCell;
};

std::size_t movementPhysicsCellIndex(const BattleMovementPhysicsCollisionWorld& world, int x, int y);
bool movementPhysicsCellWalkable(const BattleMovementPhysicsCollisionWorld& world, Point cell);

struct BattleMovementPhysicsInput
{
    BattleMovementPhysicsState state;
    BattleMovementPhysicsConfig config;
    const BattleMovementPhysicsCollisionWorld* collisionWorld = nullptr;
    int unitId = -1;
    Pointf currentPosition;
    bool actionDashActive = false;
};

struct BattleFrameMovementPhysicsUnitResult
{
    int unitId{};
    int frozenFrames{};
    bool physicsAdvanced = false;
    BattleMovementPhysicsState state;
};

class BattleMovementPhysicsSystem
{
public:
    BattleMovementPhysicsState advance(const BattleMovementPhysicsInput& input) const;
};

bool canMoveInPhysicsSnapshot(
    const BattleMovementPhysicsCollisionWorld& world,
    int unitId,
    Pointf currentPosition,
    Pointf nextPosition,
    int separationDistance);

class BattleMovementPlanner
{
public:
    explicit BattleMovementPlanner(BattleWorldState& world);

    BattleTickResult tick();
    MoveProbe probeMove(const BattleUnitState& unit,
                        Pointf nextPosition,
                        bool ignoreUnits,
                        const std::map<int, Pointf>& reservations = {}) const;

private:
    BattleWorldState& world_;
};

class BattleMovementSimulator
{
public:
    explicit BattleMovementSimulator(BattleWorldState world);

    MovementRunResult run(int frames, unsigned int seed);

private:
    BattleWorldState world_;
};

}  // namespace KysChess::Battle
