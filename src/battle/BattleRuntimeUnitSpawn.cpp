#include "BattleRuntimeUnitSpawn.h"

#include "BattleStatusSystem.h"

#include <algorithm>
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

int initialShieldFor(const BattleRuntimeUnit& unit, const RoleComboState& combo)
{
    int shield = 0;
    for (const auto& effect : combo.appliedEffects)
    {
        if (effect.type == EffectType::FlatShield && effect.trigger == Trigger::Always && effect.sourceComboId < 0)
        {
            shield += effect.value;
        }
    }
    const int shieldPct = sumAlwaysEffectValue(combo, EffectType::ShieldPctMaxHP);
    if (shieldPct > 0)
    {
        shield += unit.vitals.maxHp * shieldPct / 100;
    }
    return shield;
}

int sumAlwaysEffectCharges(const RoleComboState& combo, EffectType type)
{
    int total = 0;
    for (const auto& effect : combo.appliedEffects)
    {
        if (effect.type == type && effect.trigger == Trigger::Always)
        {
            total += std::max(1, effect.value);
        }
    }
    return total;
}

}  // namespace

BattleStatusRuntimeUnit makeInitialStatusRuntimeUnit(
    const BattleRuntimeUnit& unit,
    const RoleComboState& combo)
{
    return makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(unit, combo));
}

BattleDamageRuntimeUnit makeInitialDamageRuntimeUnit(
    int unitId,
    const RoleComboState& combo)
{
    BattleDamageRuntimeUnit damage;
    damage.id = unitId;
    damage.hurtInvincFrames = maxAlwaysEffectValue(combo, EffectType::HurtInvincFrames);
    damage.blockFirstHitsRemaining = sumAlwaysEffectValue(combo, EffectType::BlockFirstHits);
    damage.deathPrevention = maxAlwaysEffectValue(combo, EffectType::DeathPrevention) > 0;
    damage.deathPreventionFrames = maxAlwaysEffectValue(combo, EffectType::DeathPrevention);
    damage.killHealPct = sumAlwaysEffectValue(combo, EffectType::KillHealPct);
    damage.killInvincFrames = maxAlwaysEffectValue(combo, EffectType::KillInvincFrames);
    damage.bloodlustAttackPerKill = sumAlwaysEffectValue(combo, EffectType::Bloodlust);
    return damage;
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

void refreshRuntimeUnitSpawnStores(BattleRuntimeUnitSpawn& spawn)
{
    assert(spawn.unit.id >= 0);

    spawn.unit.shield = initialShieldFor(spawn.unit, spawn.combo);
    spawn.status = makeInitialStatusRuntimeUnit(spawn.unit, spawn.combo);
    spawn.damage = makeInitialDamageRuntimeUnit(spawn.unit.id, spawn.combo);
    spawn.movement = makeInitialMovementAgent(spawn.unit);
    if (spawn.actionPlan)
    {
        spawn.actionPlan->unitId = spawn.unit.id;
    }
}

BattleRuntimeUnitSpawn makeRuntimeUnitSpawn(
    BattleRuntimeUnit unit,
    RoleComboState combo,
    std::optional<BattleActionPlanSeed> actionPlan)
{
    BattleRuntimeUnitSpawn spawn;
    spawn.unit = std::move(unit);
    spawn.combo = std::move(combo);
    spawn.actionPlan = std::move(actionPlan);
    refreshRuntimeUnitSpawnStores(spawn);
    return spawn;
}

BattleRuntimeUnitRecord BattleRuntimeUnitSpawn::makeRecord() &&
{
    BattleRuntimeUnitRecord record;
    record.core = std::move(unit);
    record.combo = std::move(combo);
    record.status = std::move(status);
    record.damage = std::move(damage);
    record.movement = std::move(movement);
    record.deathEffects = { .id = record.core.id };
    record.rescue = {
        record.core.id,
        sumAlwaysEffectCharges(record.combo, EffectType::ForcePullProtect),
        sumAlwaysEffectCharges(record.combo, EffectType::ForcePullExecute),
    };
    if (actionPlan)
    {
        record.setActionPlan(std::move(*actionPlan));
    }
    return record;
}

void appendRuntimeUnit(BattleRuntimeState& runtime, BattleRuntimeUnitSpawn spawn)
{
    const int unitId = spawn.unit.id;
    assert(unitId >= 0);
    assert(spawn.status.id == unitId);
    assert(spawn.damage.id == unitId);
    if (spawn.actionPlan)
    {
        assert(spawn.actionPlan->unitId == unitId);
    }

    auto record = BattleRuntimeUnitSpawn(spawn).makeRecord();

    runtime.unitStore.units.push_back(std::move(spawn.unit));
    runtime.status.units.push_back(std::move(spawn.status));
    runtime.damage.unitExtras.push_back(std::move(spawn.damage));
    runtime.movement.agents.emplace(unitId, std::move(spawn.movement));
    runtime.combo.units.emplace(unitId, std::move(spawn.combo));
    runtime.deathEffects.store.units.push_back({ .id = unitId });
    runtime.damage.presentationStylesByDefender.emplace(
        unitId,
        makeDamagePresentationStyle(record.core.team));
    runtime.rescue.units.push_back({
        unitId,
        sumAlwaysEffectCharges(runtime.combo.units.at(unitId), EffectType::ForcePullProtect),
        sumAlwaysEffectCharges(runtime.combo.units.at(unitId), EffectType::ForcePullExecute),
    });
    if (spawn.actionPlan)
    {
        runtime.action.setPlanSeed(std::move(*spawn.actionPlan));
    }
    runtime.unitRecords.append(std::move(record));
}

}  // namespace KysChess::Battle
