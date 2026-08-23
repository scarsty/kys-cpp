#include "BattleRuntimeUnitSpawn.h"

#include "BattleStatusSystem.h"

#include <cassert>
#include <utility>

namespace KysChess::Battle
{

namespace
{

constexpr int NormalDamageTextSize = 30;
constexpr int UltDamageTextSize = 44;

BattlePresentationColor damageTextColor(int team, bool emphasized)
{
    if (team == 0)
    {
        return emphasized
            ? BattlePresentationColor{ 255, 45, 85, 255 }
            : BattlePresentationColor{ 255, 90, 79, 255 };
    }
    return emphasized
        ? BattlePresentationColor{ 47, 128, 255, 255 }
        : BattlePresentationColor{ 102, 207, 255, 255 };
}

BattleDamagePresentationStyle makeDamagePresentationStyle(int team)
{
    BattleDamagePresentationStyle style;
    style.normalDamageColor = damageTextColor(team, false);
    style.emphasizedDamageColor = damageTextColor(team, true);
    style.executeTextColor = { 255, 136, 48, 255 };
    style.normalDamageTextSize = NormalDamageTextSize;
    style.emphasizedDamageTextSize = UltDamageTextSize;
    style.executeTextSize = UltDamageTextSize;
    return style;
}

}  // namespace

BattleStatusRuntimeUnit makeInitialStatusRuntimeUnit(
    const BattleRuntimeUnit& unit)
{
    return makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(unit));
}

BattleDamageRuntimeUnit makeInitialDamageRuntimeUnit()
{
    return {};
}

BattleMovementAgentState makeInitialMovementAgent(
    const BattleRuntimeUnit& unit)
{
    BattleMovementAgentState agent;
    agent.active = unit.alive;
    agent.physics.position = unit.motion.position;
    agent.physics.velocity = unit.motion.velocity;
    agent.physics.acceleration = unit.motion.acceleration;
    return agent;
}

void refreshRuntimeUnitSpawnDerivedState(BattleRuntimeUnitSpawn& spawn)
{
    assert(spawn.unit.id >= 0);

    spawn.status = makeInitialStatusRuntimeUnit(spawn.unit);
    spawn.damage = makeInitialDamageRuntimeUnit();
    spawn.rescue = {};
    spawn.movement = makeInitialMovementAgent(spawn.unit);
    if (spawn.actionPlanSeed)
    {
        spawn.actionPlanSeed->unitId = spawn.unit.id;
    }
}

BattleRuntimeUnitSpawn makeRuntimeUnitSpawn(
    BattleRuntimeUnit unit,
    BattleComboRuntimeFacts comboFacts,
    std::optional<BattleActionPlanSeed> actionPlan)
{
    BattleRuntimeUnitSpawn spawn;
    spawn.unit = std::move(unit);
    spawn.comboFacts = std::move(comboFacts);
    spawn.actionPlanSeed = std::move(actionPlan);
    refreshRuntimeUnitSpawnDerivedState(spawn);
    return spawn;
}

BattleRuntimeUnitRecord BattleRuntimeUnitSpawn::makeRecord() &&
{
    BattleRuntimeUnitRecord record;
    record.core = std::move(unit);
    record.comboFacts = std::move(comboFacts);
    record.status = std::move(status);
    record.damage = std::move(damage);
    record.rescue = std::move(rescue);
    record.movement = std::move(movement);
    if (actionPlanSeed)
    {
        record.setActionPlan(std::move(*actionPlanSeed));
    }
    return record;
}

void appendRuntimeUnit(BattleRuntimeState& runtime, BattleRuntimeUnitSpawn spawn)
{
    const int unitId = spawn.unit.id;
    assert(unitId >= 0);
    if (spawn.actionPlanSeed)
    {
        assert(spawn.actionPlanSeed->unitId == unitId);
    }

    auto record = std::move(spawn).makeRecord();

    runtime.damage.presentationStylesByDefender.emplace(
        unitId,
        makeDamagePresentationStyle(record.core.team));
    runtime.units.append(std::move(record));
}

}  // namespace KysChess::Battle
