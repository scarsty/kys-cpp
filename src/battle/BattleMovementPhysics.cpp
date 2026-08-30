#include "BattleMovementPhysics.h"

#include "BattleMath.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace KysChess::Battle
{

namespace
{

constexpr double AirborneTerrainClearanceTileFactor = 3.0;

bool movementPhysicsSegmentWalkable(
    const BattleMovementPhysicsCollisionWorld& world,
    Pointf currentPosition,
    Pointf nextPosition)
{
    const int steps = std::max(
        1,
        battleTravelFrames2d(
            currentPosition,
            nextPosition,
            std::max(1.0, world.tileWidth / 4.0)));
    for (int step = 1; step <= steps; ++step)
    {
        const auto probe = currentPosition + (nextPosition - currentPosition) * (static_cast<double>(step) / steps);
        if (!movementPhysicsCellWalkable(world, movementPhysicsCell(world, probe)))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

Point movementPhysicsCell(const BattleMovementPhysicsCollisionWorld& world, Pointf position)
{
    assert(world.tileWidth > 0.0);
    assert(world.coordCount > 0);
    return battleIsometricGridPosition(position, world.coordCount, world.tileWidth);
}

std::size_t movementPhysicsCellIndex(const BattleMovementPhysicsCollisionWorld& world, int x, int y)
{
    assert(world.coordCount > 0);
    assert(x >= 0);
    assert(y >= 0);
    assert(x < world.coordCount);
    assert(y < world.coordCount);
    return static_cast<std::size_t>(y * world.coordCount + x);
}

bool movementPhysicsCellWalkable(const BattleMovementPhysicsCollisionWorld& world, Point cell)
{
    if (cell.x < 0 || cell.y < 0 || cell.x >= world.coordCount || cell.y >= world.coordCount)
    {
        return false;
    }
    const auto index = movementPhysicsCellIndex(world, cell.x, cell.y);
    const auto& walkableCells = world.walkableCells();
    assert(index < walkableCells.size());
    return walkableCells[index] != 0;
}

bool canMoveInPhysicsSnapshot(
    const BattleMovementPhysicsCollisionWorld& world,
    int unitId,
    Pointf currentPosition,
    Pointf nextPosition,
    int separationDistance,
    bool ignoreUnitCollision)
{
    if (currentPosition.z <= 1.0f && !ignoreUnitCollision)
    {
        const double separation = separationDistance == -1
            ? world.defaultSeparationDistance
            : static_cast<double>(separationDistance);
        for (const auto& unit : world.units)
        {
            if (!unit.alive || unit.id == unitId)
            {
                continue;
            }
            const double nextDistance = distance2d(nextPosition, unit.position);
            if (nextDistance < separation)
            {
                const double currentDistance = distance2d(currentPosition, unit.position);
                if (currentDistance >= nextDistance)
                {
                    return false;
                }
            }
        }
    }

    if (std::min(currentPosition.z, nextPosition.z) >= world.tileWidth * AirborneTerrainClearanceTileFactor)
    {
        return true;
    }

    return movementPhysicsSegmentWalkable(world, currentPosition, nextPosition);
}

BattleMovementPhysicsState BattleMovementPhysicsSystem::advance(const BattleMovementPhysicsInput& input) const
{
    assert(input.collisionWorld);
    assert(input.unitId >= 0);

    auto state = input.state;
    auto canMove = [&](Pointf position, int separationDistance)
    {
        return canMoveInPhysicsSnapshot(
            *input.collisionWorld,
            input.unitId,
            state.position,
            position,
            separationDistance,
            input.ignoreUnitCollision);
    };

    const auto startPosition = state.position;
    const auto startVelocity = state.velocity;
    const bool movementDashActive = state.movementDashFrames > 0;
    const bool movementDashEnding = state.movementDashFrames == 1;
    const bool postDashRetreatActive = state.postDashRetreatFrames > 0;
    const bool knockbackActive = state.knockbackFrames > 0;
    if (postDashRetreatActive && !knockbackActive && !input.actionDashActive && !movementDashActive)
    {
        state.velocity = state.postDashRetreatVelocity;
    }
    const int separationDistance = input.actionDashActive || movementDashActive || postDashRetreatActive || knockbackActive ? 1 : -1;
    const auto velocity = state.velocity;
    const double velocityNorm = velocity.norm();
    const double stepDistance = knockbackActive
        ? std::max(1.0, input.collisionWorld->tileWidth / 4.0)
        : std::max(velocityNorm, 1.0);
    const int stepCount = knockbackActive
        ? std::max(1, battleTravelFrames3d({}, velocity, stepDistance))
        : 1;
    Pointf appliedVelocity;
    bool blocked = false;
    for (int step = 1; step <= stepCount; ++step)
    {
        auto nextPosition = state.position + velocity * (1.0 / stepCount);
        if (canMove(nextPosition, separationDistance))
        {
            appliedVelocity += nextPosition - state.position;
            state.position = nextPosition;
            continue;
        }

        bool canSlide = false;
        auto xOnly = state.position;
        xOnly.x = nextPosition.x;
        if (!knockbackActive && canMove(xOnly, separationDistance))
        {
            appliedVelocity += xOnly - state.position;
            state.position = xOnly;
            state.velocity.y = 0;
            canSlide = true;
        }
        auto yOnly = state.position;
        yOnly.y = nextPosition.y;
        if (!knockbackActive && !canSlide && canMove(yOnly, separationDistance))
        {
            appliedVelocity += yOnly - state.position;
            state.position = yOnly;
            state.velocity.x = 0;
            canSlide = true;
        }

        if (!canSlide)
        {
            blocked = true;
            break;
        }
    }
    if (blocked)
    {
        state.velocity = { 0, 0, 0 };
        state.movementDashFrames = 0;
        state.postDashRetreatFrames = 0;
        state.knockbackFrames = 0;
        state.knockbackControlFrames = 0;
        state.knockbackVelocity = {};
    }
    else if (knockbackActive)
    {
        state.velocity = appliedVelocity;
    }

    if (state.knockbackFrames > 0)
    {
        --state.knockbackFrames;
    }
    state.knockbackVelocity = state.knockbackFrames > 0 ? state.velocity : Pointf{};
    if (state.knockbackControlFrames > 0)
    {
        --state.knockbackControlFrames;
    }

    if (state.movementDashFrames > 0)
    {
        --state.movementDashFrames;
    }
    if (!movementDashActive && state.postDashRetreatFrames > 0)
    {
        --state.postDashRetreatFrames;
    }
    if (movementDashEnding)
    {
        state.movementDashSpreadFrames = input.config.postDashSpreadFrames;
    }
    else if (!movementDashActive && state.movementDashSpreadFrames > 0)
    {
        --state.movementDashSpreadFrames;
    }
    if (state.movementDashCooldown > 0)
    {
        --state.movementDashCooldown;
    }
    if (state.position.z < 0)
    {
        state.position.z = 0;
    }
    if (state.position.z == 0 && state.velocity.norm() != 0)
    {
        auto friction = -state.velocity;
        friction.normTo(input.config.friction);
        state.acceleration = { friction.x, friction.y, input.config.gravity };
    }
    else
    {
        state.acceleration = { 0, 0, input.config.gravity };
    }
    state.velocity += state.acceleration;
    if (state.position.z == 0)
    {
        state.velocity.z = 0;
    }
    if (state.velocity.norm() < 0.1)
    {
        state.velocity.x = 0;
        state.velocity.y = 0;
    }
    return state;
}

double deathKickSpeed(int committedHpDamage)
{
    constexpr double MaxDeathKickSpeed = 75.0;

    return std::clamp(committedHpDamage / 3.0 + 5.0, 0.0, MaxDeathKickSpeed);
}

Pointf deathKickVelocity(Pointf direction, int committedHpDamage)
{
    const double speed = deathKickSpeed(committedHpDamage);
    const double verticalSpeed = std::min(6.0, speed * 0.35);
    const double horizontalSpeed = std::sqrt(std::max(0.0, speed * speed - verticalSpeed * verticalSpeed));

    direction.z = 0.0f;
    if (direction.norm() <= 0.01)
    {
        direction = { 1, 0, 0 };
    }
    direction.normTo(static_cast<float>(horizontalSpeed));
    direction.z = static_cast<float>(verticalSpeed);
    return direction;
}

bool needsCorpsePhysics(bool alive, Pointf position, Pointf velocity)
{
    return !alive && (position.z > 0.0f || velocity.norm() > 0.01);
}

}  // namespace KysChess::Battle
