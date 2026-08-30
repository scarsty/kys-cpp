#pragma once

#include "BattleGeometry.h"
#include "BattleMath.h"

#include <cstddef>
#include <cstdint>
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

Point movementPhysicsCell(const BattleMovementPhysicsCollisionWorld& world, Pointf position);

inline double distance2d(Pointf a, Pointf b)
{
    return EuclidDis(a.x - b.x, a.y - b.y);
}

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

inline constexpr float DeathKickImpactHeight = 36.0f;

bool needsCorpsePhysics(bool alive, Pointf position, Pointf velocity);
double deathKickSpeed(int committedHpDamage);
Pointf deathKickVelocity(Pointf direction, int committedHpDamage);

}  // namespace KysChess::Battle
