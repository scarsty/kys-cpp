#include "BattleDamageApplicationSystem.h"

#include <algorithm>
#include <cassert>
#include <set>

namespace KysChess::Battle
{

namespace
{

BattleDamageApplicationUnitSnapshot& unitById(
    std::vector<BattleDamageApplicationUnitSnapshot>& units,
    int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleDamageApplicationUnitSnapshot& unit)
        {
            return unit.id == unitId;
        });
    assert(it != units.end());
    return *it;
}

BattleDeathEffectUnit* findDeathEffectUnit(BattleDeathEffectWorld& world, int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleDeathEffectUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != world.units.end() ? &*it : nullptr;
}

void applyDamageUnitToDeathEffectUnit(BattleDeathEffectUnit& deathUnit, const BattleDamageUnitState& damageUnit)
{
    deathUnit.alive = damageUnit.alive;
    deathUnit.hp = damageUnit.hp;
    deathUnit.maxHp = damageUnit.maxHp;
    deathUnit.attack = damageUnit.attack;
    deathUnit.shield = damageUnit.shield;
}

BattlePresentationEvent makeDeathPreventionPresentation(const BattleDamageEvent& event)
{
    BattlePresentationEvent presentation;
    presentation.type = BattlePresentationEventType::StatusLog;
    presentation.sourceUnitId = event.sourceUnitId;
    presentation.targetUnitId = event.targetUnitId;
    presentation.durationFrames = event.value;
    presentation.text = "死亡庇護";
    return presentation;
}

void appendDeathAoeCommand(BattleDamageApplicationResult& result,
                           const BattleDamageApplicationInput& input,
                           const BattleDamageTransactionResult& transaction,
                           int deadUnitId)
{
    auto effectIt = input.unitEffects.find(deadUnitId);
    if (effectIt == input.unitEffects.end() || effectIt->second.deathAoePct <= 0)
    {
        return;
    }

    BattleDeathAoeProjectileCommand command;
    command.sourceUnitId = deadUnitId;
    command.trackedTargetUnitId = transaction.attacker.id;
    command.damage = std::max(1, transaction.defender.maxHp * effectIt->second.deathAoePct / 100);
    command.damagePct = effectIt->second.deathAoePct;
    command.stunFrames = effectIt->second.deathAoeStunFrames;
    command.maxTargets = effectIt->second.deathAoeMaxTargets;
    result.commands.push_back(command);
}

void appendDeathEffectCommand(BattleDamageApplicationResult& result, const BattleDeathEffectEvent& event)
{
    switch (event.type)
    {
    case BattleDeathEffectEventType::AllyStatBoost:
        result.commands.push_back(BattleTempAttackBuffCommand{
            event.targetUnitId,
            event.value,
            0,
            "同袍之死",
            event.value,
            true,
        });
        break;
    case BattleDeathEffectEventType::DeathMedicalHeal:
        result.commands.push_back(BattleUnitHealCommand{
            event.sourceUnitId,
            event.targetUnitId,
            event.value,
            "死亡醫療",
        });
        break;
    case BattleDeathEffectEventType::ShieldOnAllyDeath:
        result.commands.push_back(BattleUnitShieldCommand{
            event.sourceUnitId,
            event.targetUnitId,
            event.value,
            "護盾重獲",
        });
        break;
    }
}

void updateBattleResult(BattleDamageApplicationResult& result,
                        const BattleDamageApplicationInput& input,
                        const std::vector<BattleDamageApplicationUnitSnapshot>& units)
{
    std::set<int> aliveTeams;
    for (const auto& unit : units)
    {
        if (unit.alive)
        {
            aliveTeams.insert(unit.team);
        }
    }
    for (const auto& [team, count] : input.pendingAliveByTeam)
    {
        if (count > 0)
        {
            aliveTeams.insert(team);
        }
    }
    if (aliveTeams.size() != 1)
    {
        return;
    }

    result.battleEnded = true;
    result.winningTeam = *aliveTeams.begin();
    result.lifecycleEvents.push_back({
        BattleDamageLifecycleEventType::BattleEnded,
        -1,
        -1,
        result.winningTeam,
    });
}

}  // namespace

BattleDamageApplicationResult BattleDamageApplicationSystem::apply(
    const BattleDamageApplicationInput& input) const
{
    assert(input.frame >= 0);

    BattleDamageApplicationResult result;
    result.deathEffects = input.deathEffects;
    auto units = input.units;

    for (const auto& pending : input.pendingTransactions)
    {
        auto transaction = BattleDamageSystem().resolveTransaction(pending);
        unitById(units, transaction.defender.id).alive = transaction.defender.alive;
        if (transaction.attacker.id >= 0)
        {
            unitById(units, transaction.attacker.id).alive = transaction.attacker.alive;
        }

        if (auto* deathUnit = findDeathEffectUnit(result.deathEffects, transaction.defender.id))
        {
            applyDamageUnitToDeathEffectUnit(*deathUnit, transaction.defender);
        }
        if (auto* deathUnit = findDeathEffectUnit(result.deathEffects, transaction.attacker.id))
        {
            applyDamageUnitToDeathEffectUnit(*deathUnit, transaction.attacker);
        }

        for (const auto& event : transaction.events)
        {
            if (event.type == BattleDamageEventType::DeathPrevented)
            {
                result.presentationEvents.push_back(makeDeathPreventionPresentation(event));
            }
            if (event.type != BattleDamageEventType::UnitDied)
            {
                continue;
            }

            result.lifecycleEvents.push_back({
                BattleDamageLifecycleEventType::UnitDied,
                event.sourceUnitId,
                event.targetUnitId,
                event.value,
            });
            result.lifecycleEvents.push_back({
                BattleDamageLifecycleEventType::KillRecorded,
                event.sourceUnitId,
                event.targetUnitId,
                0,
            });

            appendDeathAoeCommand(result, input, transaction, event.targetUnitId);
            if (findDeathEffectUnit(result.deathEffects, event.targetUnitId))
            {
                auto deathEvents = BattleDeathEffectSystem().applyAllyDeathEffects(
                    result.deathEffects,
                    event.targetUnitId);
                for (const auto& deathEvent : deathEvents)
                {
                    appendDeathEffectCommand(result, deathEvent);
                }
            }
        }

        result.transactions.push_back(std::move(transaction));
    }

    updateBattleResult(result, input, units);
    return result;
}

}  // namespace KysChess::Battle
