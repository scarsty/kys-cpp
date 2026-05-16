#pragma once

#include "BattleGeometry.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace KysChess::Battle
{

struct BattleMovementPhysicsConfig
{
    float gravity = -4.0f;
    float friction = 0.1f;
    int postDashSpreadFrames = 0;
};

struct BattleMovementPhysicsTerrain
{
    double tileWidth = 0.0;
    int coordCount = 0;
    double defaultSeparationDistance = 0.0;
    std::vector<std::uint8_t> walkableByCell;
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
    bool ignoreUnitCollision = false;
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
    int separationDistance,
    bool ignoreUnitCollision = false);

bool battleMovementTaXueUnstable(const BattleUnitState& unit);

class BattleMovementPlanner
{
public:
    explicit BattleMovementPlanner(BattleMovementFrameInput& world);

    BattleTickResult tick();
    MoveProbe probeMove(const BattleUnitState& unit,
                        Pointf nextPosition,
                        bool ignoreUnits,
                        const std::map<int, Pointf>& reservations = {}) const;

private:
    BattleMovementFrameInput& world_;
};

class BattleMovementSimulator
{
public:
    explicit BattleMovementSimulator(BattleMovementFrameInput world);

    MovementRunResult run(int frames, unsigned int seed);

private:
    BattleMovementFrameInput world_;
};

}  // namespace KysChess::Battle
