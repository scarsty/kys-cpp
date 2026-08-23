#pragma once

#include "battle/BattleEffectSystem.h"
#include "battle/BattleRuntimeUnitSpawn.h"
#include "battle/BattleRuntimeUnits.h"

#include <initializer_list>
#include <utility>

namespace KysChess::Battle::Test
{

inline void appendRuntimeRecord(
    BattleRuntimeUnits& records,
    BattleRuntimeUnit unit,
    BattleComboRuntimeFacts comboFacts = {})
{
    auto record = makeRuntimeUnitSpawn(
        std::move(unit),
        std::move(comboFacts)).makeRecord();
    records.append(std::move(record));
}

inline BattleRuntimeUnits runtimeRecords(std::initializer_list<BattleRuntimeUnit> units)
{
    BattleRuntimeUnits records;
    records.reserve(units.size());
    for (auto unit : units)
    {
        appendRuntimeRecord(records, std::move(unit));
    }
    return records;
}

inline void appendDeathBlastRule(
    BattleEffectRuleStore& store,
    EffectSourceBinding binding,
    int damagePct,
    int maximumTargets,
    int stunFrames,
    EffectRuleId ruleId = { 1 })
{
    DealDamageAction damage;
    damage.amount.base = EffectNumberBase::SourceMaxHp;
    damage.amount.percent = damagePct;
    damage.amount.minimum = 1;
    damage.kind = BattleDamageKind::Physical;
    damage.appliesDamageModifiers = false;
    damage.triggersHurtInvincibility = false;
    damage.areaProjectiles = AreaProjectileDamageDelivery{
        .rangeTiles = 7,
        .maximumTargets = maximumTargets,
        .stunFrames = stunFrames,
        .trackEventSource = true,
        .visual = AreaProjectileVisual::DeathBlast,
    };

    EffectRule rule;
    rule.id = ruleId;
    rule.event = EffectEvent::UnitDied;
    rule.selector.kind = EffectSelectorKind::Self;
    rule.actions = { EffectAction{ EffectActionValue{ std::move(damage) } } };
    store.append(binding, rule);
}

}  // namespace KysChess::Battle::Test
