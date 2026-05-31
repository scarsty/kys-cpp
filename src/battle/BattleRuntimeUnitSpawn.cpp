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
    for (RoleComboEffectId effectId : combo.effectIds(Trigger::Always, EffectType::FlatShield))
    {
        const auto& effect = combo.effect(effectId);
        if (effect.origin != RoleComboEffectOrigin::Configured)
        {
            continue;
        }
        if (effect.sourceComboId < 0)
        {
            shield += effect.value;
        }
    }
    const int shieldPct = combo.sumAlways(EffectType::ShieldPctMaxHP);
    if (shieldPct > 0)
    {
        shield += unit.vitals.maxHp * shieldPct / 100;
    }
    return shield;
}

int sumAlwaysEffectCharges(const RoleComboState& combo, EffectType type)
{
    int total = 0;
    for (RoleComboEffectId effectId : combo.effectIds(Trigger::Always, type))
    {
        const auto& effect = combo.effect(effectId);
        if (effect.origin != RoleComboEffectOrigin::Configured)
        {
            continue;
        }
        total += std::max(1, effect.value);
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

BattleDamageRuntimeUnit makeInitialDamageRuntimeUnit(const RoleComboState& combo)
{
    BattleDamageRuntimeUnit damage;
    damage.hurtInvincFrames = combo.maxAlways(EffectType::HurtInvincFrames);
    damage.blockFirstHitsRemaining = combo.sumAlways(EffectType::BlockFirstHits);
    damage.deathPrevention = combo.maxAlways(EffectType::DeathPrevention) > 0;
    damage.deathPreventionFrames = combo.maxAlways(EffectType::DeathPrevention);
    damage.killHealPct = combo.sumAlways(EffectType::KillHealPct);
    damage.killInvincFrames = combo.maxAlways(EffectType::KillInvincFrames);
    damage.bloodlustAttackPerKill = combo.sumAlways(EffectType::Bloodlust);
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

void refreshRuntimeUnitSpawnDerivedState(BattleRuntimeUnitSpawn& spawn)
{
    assert(spawn.unit.id >= 0);

    spawn.unit.shield = initialShieldFor(spawn.unit, spawn.combo);
    spawn.status = makeInitialStatusRuntimeUnit(spawn.unit, spawn.combo);
    spawn.damage = makeInitialDamageRuntimeUnit(spawn.combo);
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
    refreshRuntimeUnitSpawnDerivedState(spawn);
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
    record.deathEffects = {};
    record.rescue = {
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
    if (spawn.actionPlan)
    {
        assert(spawn.actionPlan->unitId == unitId);
    }

    auto record = std::move(spawn).makeRecord();

    runtime.damage.presentationStylesByDefender.emplace(
        unitId,
        makeDamagePresentationStyle(record.core.team));
    runtime.units.append(std::move(record));
}

}  // namespace KysChess::Battle
