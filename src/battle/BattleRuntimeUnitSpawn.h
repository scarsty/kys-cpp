#pragma once

#include "BattleCore.h"

#include <optional>

namespace KysChess::Battle
{

struct BattleRuntimeUnitSpawn
{
    BattleRuntimeUnit unit;
    RoleComboState combo;
    BattleUnitMagicEffectRuntime skillEffects;
    BattleStatusRuntimeUnit status;
    BattleDamageRuntimeUnit damage;
    BattleMovementAgentState movement;
    std::optional<BattleActionPlanSeed> actionPlanSeed;

    BattleRuntimeUnitRecord makeRecord() &&;

    const BattleActionPlanSeed* actionPlan() const
    {
        return actionPlanSeed ? &*actionPlanSeed : nullptr;
    }
};

BattleStatusRuntimeUnit makeInitialStatusRuntimeUnit(
    const BattleRuntimeUnit& unit,
    const RoleComboState& combo);

BattleDamageRuntimeUnit makeInitialDamageRuntimeUnit(
    const RoleComboState& combo);

BattleMovementAgentState makeInitialMovementAgent(
    const BattleRuntimeUnit& unit);

void refreshRuntimeUnitSpawnDerivedState(BattleRuntimeUnitSpawn& spawn);

BattleRuntimeUnitSpawn makeRuntimeUnitSpawn(
    BattleRuntimeUnit unit,
    RoleComboState combo,
    std::optional<BattleActionPlanSeed> actionPlan = std::nullopt);

void appendRuntimeUnit(BattleRuntimeState& runtime, BattleRuntimeUnitSpawn spawn);

}  // namespace KysChess::Battle
