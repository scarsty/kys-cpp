#pragma once

#include "BattleCore.h"

#include <optional>

namespace KysChess::Battle
{

struct BattleRuntimeUnitSpawn
{
    BattleRuntimeUnit unit;
    RoleComboState combo;
    BattleStatusRuntimeUnit status;
    BattleDamageRuntimeUnit damage;
    BattleMovementAgentState movement;
    std::optional<BattleActionPlanSeed> actionPlan;

    BattleRuntimeUnitRecord makeRecord() &&;
};

BattleStatusRuntimeUnit makeInitialStatusRuntimeUnit(
    const BattleRuntimeUnit& unit,
    const RoleComboState& combo);

BattleDamageRuntimeUnit makeInitialDamageRuntimeUnit(
    int unitId,
    const RoleComboState& combo);

BattleMovementAgentState makeInitialMovementAgent(
    const BattleRuntimeUnit& unit);

void refreshRuntimeUnitSpawnStores(BattleRuntimeUnitSpawn& spawn);

BattleRuntimeUnitSpawn makeRuntimeUnitSpawn(
    BattleRuntimeUnit unit,
    RoleComboState combo,
    std::optional<BattleActionPlanSeed> actionPlan = std::nullopt);

void appendRuntimeUnit(BattleRuntimeState& runtime, BattleRuntimeUnitSpawn spawn);

}  // namespace KysChess::Battle
