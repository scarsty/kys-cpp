#pragma once

#include "BattleGeometry.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory_resource>
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
    explicit BattleMovementPhysicsCollisionWorld(
        std::pmr::memory_resource* memoryResource = std::pmr::get_default_resource())
        : units(memoryResource)
    {
    }

    const std::vector<std::uint8_t>& walkableCells() const
    {
        return walkableCellSource ? *walkableCellSource : walkableByCell;
    }

    double tileWidth = 0.0;
    int coordCount = 0;
    double defaultSeparationDistance = 0.0;
    std::pmr::vector<BattleMovementPhysicsCollisionUnitSnapshot> units;
    std::vector<std::uint8_t> walkableByCell;
    const std::vector<std::uint8_t>* walkableCellSource = nullptr;
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
    bool unitAlive = true;
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

BattleMovementTerrainLayout makeBattleMovementTerrainLayout(
    const std::vector<BattleTerrainCell>& terrainCells,
    double tileWidth);

class BattleMovementPlanner
{
public:
    explicit BattleMovementPlanner(BattleMovementPlanInput world);

    BattleTickResult tick();
    MoveProbe probeMove(const BattleUnitState& unit,
                        Pointf nextPosition,
                        bool ignoreUnits,
                        const std::map<int, Pointf>& reservations = {}) const;

private:
    BattleMovementPathState localPathState_;
    BattleMovementPlanInput world_;
};

}  // namespace KysChess::Battle
