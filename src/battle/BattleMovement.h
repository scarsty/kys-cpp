#pragma once

#include "BattleGeometry.h"

#include <functional>
#include <map>

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
};

struct BattleMovementPhysicsConfig
{
    float gravity = -4.0f;
    float friction = 0.1f;
    int postDashSpreadFrames = 0;
};

struct BattleMovementPhysicsInput
{
    BattleMovementPhysicsState state;
    BattleMovementPhysicsConfig config;
    bool actionDashActive = false;
    std::function<bool(Pointf, int)> canMove;
};

class BattleMovementPhysicsSystem
{
public:
    BattleMovementPhysicsState advance(const BattleMovementPhysicsInput& input) const;
};

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
