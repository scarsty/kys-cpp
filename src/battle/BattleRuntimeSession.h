#pragma once

#include "BattleInitialization.h"

#include <optional>
#include <utility>
#include <vector>

namespace KysChess::Battle
{

struct BattleSetupPlacementUnit
{
    int unitId = -1;
    int x = 0;
    int y = 0;
    int faceTowards = 0;
};

struct BattleSetupPlacementInput
{
    std::vector<BattleSetupPlacementUnit> units;
};

struct BattleRuntimeSetupConfiguration
{
    BattleWorldState world;
    BattleAttackWorld attacks;
    BattleRuntimeState::DamageState damage;
    BattleRuntimeState::BattleResultState result;
    BattleRuntimeState::TeamEffectState teamEffects;
    BattleRuntimeState::RescueState rescue;
    BattleRuntimeState::MovementPhysicsState movementPhysics;
    BattleRuntimeState::ActionState action;
    BattleProjectileFollowUpContext projectileFollowUps;
    BattleRuntimeState::DeathEffectState deathEffects;
};

class BattleRuntimeSession
{
public:
    explicit BattleRuntimeSession(BattleRuntimeInit init);

    BattleFrameResult runFrame();
    void commitSetupConfiguration(BattleRuntimeSetupConfiguration config);
    void commitSetupPlacement(const BattleSetupPlacementInput& input);
    BattleInitializationResult releaseInitializationResult();

    const BattleRuntimeState& runtime() const;

private:
    BattleRuntimeState runtime_;
    std::optional<BattleInitializationResult> initialization_result_;
    BattleFrameRunner runner_;
    bool setupConfigured_ = false;
    bool setupPlacementCommitted_ = false;
    bool frameStarted_ = false;
};

}  // namespace KysChess::Battle
