#pragma once

#include "BattleCore.h"

#include <optional>

namespace KysChess::Battle
{

struct BattleRuntimeUnitSpawn
{
    BattleRuntimeUnit unit;
    BattleComboRuntimeFacts comboFacts;
    BattleStatusRuntimeUnit status;
    BattleDamageRuntimeUnit damage;
    BattleRescueUnitRuntime rescue;
    BattleMovementAgentState movement;
    std::optional<BattleActionPlanSeed> actionPlanSeed;

    BattleRuntimeUnitRecord makeRecord() &&;

    const BattleActionPlanSeed* actionPlan() const
    {
        return actionPlanSeed ? &*actionPlanSeed : nullptr;
    }
};

BattleStatusRuntimeUnit makeInitialStatusRuntimeUnit(
    const BattleRuntimeUnit& unit);

BattleDamageRuntimeUnit makeInitialDamageRuntimeUnit();

BattleMovementAgentState makeInitialMovementAgent(
    const BattleRuntimeUnit& unit);

void refreshRuntimeUnitSpawnDerivedState(BattleRuntimeUnitSpawn& spawn);

BattleRuntimeUnitSpawn makeRuntimeUnitSpawn(
    BattleRuntimeUnit unit,
    BattleComboRuntimeFacts comboFacts = {},
    std::optional<BattleActionPlanSeed> actionPlan = std::nullopt);

void appendRuntimeUnit(BattleRuntimeState& runtime, BattleRuntimeUnitSpawn spawn);

}  // namespace KysChess::Battle
