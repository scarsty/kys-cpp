#include "BattleDamageApplicationSystem.h"

#include <algorithm>
#include <cassert>
#include <format>
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

std::string formatStatusFrames(const char* label, int frames)
{
    if (frames <= 0)
    {
        return label;
    }
    return std::format("{}（{}幀）", label, frames);
}

BattleLogEvent makeDeathPreventionLog(const BattleDamageEvent& event)
{
    BattleLogEvent log;
    log.type = BattleLogEventType::Status;
    log.sourceUnitId = event.sourceUnitId;
    log.targetUnitId = event.targetUnitId;
    log.amount = event.value;
    log.text = "死亡庇護";
    return log;
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
    result.gameplayEvents.push_back({
        BattleGameplayEventType::BattleEnded,
        input.frame,
        -1,
        -1,
        result.winningTeam,
    });
    result.logEvents.push_back({
        BattleLogEventType::BattleEnded,
        input.frame,
        -1,
        -1,
        result.winningTeam,
    });
}

std::vector<BattleDamageTransactionInput> aggregatePendingTransactions(
    const std::vector<BattleDamageTransactionInput>& pendingTransactions)
{
    std::vector<BattleDamageTransactionInput> aggregated;
    std::map<int, std::size_t> indexByDefender;
    for (const auto& pending : pendingTransactions)
    {
        assert(pending.request.defenderUnitId >= 0);
        auto it = indexByDefender.find(pending.request.defenderUnitId);
        if (it == indexByDefender.end())
        {
            indexByDefender[pending.request.defenderUnitId] = aggregated.size();
            aggregated.push_back(pending);
            continue;
        }

        auto& current = aggregated[it->second];
        const auto accumulatedDamage = current.request.baseDamage + pending.request.baseDamage;
        current = pending;
        current.request.baseDamage = accumulatedDamage;
        current.request.preResolvedDamage = current.request.preResolvedDamage || pending.request.preResolvedDamage;
    }
    return aggregated;
}

std::vector<BattleDamagePresentationInput> aggregatePendingPresentation(
    const std::vector<BattleDamageTransactionInput>& pendingTransactions,
    const std::vector<BattleDamagePresentationInput>& pendingPresentation)
{
    assert(pendingPresentation.empty() || pendingPresentation.size() == pendingTransactions.size());
    if (pendingPresentation.empty())
    {
        return {};
    }

    std::vector<BattleDamagePresentationInput> aggregated;
    std::map<int, std::size_t> indexByDefender;
    for (std::size_t i = 0; i < pendingTransactions.size(); ++i)
    {
        const auto defenderUnitId = pendingTransactions[i].request.defenderUnitId;
        assert(defenderUnitId >= 0);
        auto it = indexByDefender.find(defenderUnitId);
        if (it == indexByDefender.end())
        {
            indexByDefender[defenderUnitId] = aggregated.size();
            aggregated.push_back(pendingPresentation[i]);
            continue;
        }

        if (!aggregated[it->second].enabled || pendingPresentation[i].enabled)
        {
            aggregated[it->second] = pendingPresentation[i];
        }
    }
    return aggregated;
}

int committedHpDamage(const BattleDamageTransactionResult& transaction)
{
    int damage = 0;
    for (const auto& event : transaction.events)
    {
        if (event.type == BattleDamageEventType::DamageApplied)
        {
            damage += event.value;
        }
    }
    return damage;
}

BattlePresentationColor selectDamageColor(const BattleDamagePresentationInput& presentation)
{
    return (presentation.critical || presentation.ultimate)
        ? presentation.emphasizedDamageColor
        : presentation.normalDamageColor;
}

int selectDamageTextSize(const BattleDamagePresentationInput& presentation)
{
    return (presentation.critical || presentation.ultimate)
        ? presentation.emphasizedDamageTextSize
        : presentation.normalDamageTextSize;
}

void appendDamageOutputEvents(BattleDamageApplicationResult& result,
                              const BattleDamagePresentationInput& presentation,
                              const BattleDamageTransactionResult& transaction)
{
    if (!presentation.enabled)
    {
        return;
    }

    const int hpDamage = committedHpDamage(transaction);
    if (hpDamage <= 0)
    {
        return;
    }

    if (presentation.executed)
    {
        BattleVisualEvent executedText;
        executedText.type = BattleVisualEventType::FloatingText;
        executedText.targetUnitId = transaction.defender.id;
        executedText.text = "處決！";
        executedText.color = presentation.executeTextColor;
        executedText.textSize = presentation.executeTextSize;
        result.visualEvents.push_back(std::move(executedText));
    }
    else
    {
        BattleVisualEvent number;
        number.type = BattleVisualEventType::DamageNumber;
        number.targetUnitId = transaction.defender.id;
        number.amount = hpDamage;
        number.color = selectDamageColor(presentation);
        number.textSize = selectDamageTextSize(presentation);
        result.visualEvents.push_back(std::move(number));
    }

    BattleLogEvent damageLog;
    damageLog.type = BattleLogEventType::Damage;
    damageLog.sourceUnitId = transaction.attacker.id;
    damageLog.targetUnitId = transaction.defender.id;
    damageLog.amount = hpDamage;
    damageLog.skillName = presentation.skillName;
    damageLog.detailText = presentation.detailText;
    result.logEvents.push_back(std::move(damageLog));
}

void appendDamageTransactionLogEvents(BattleDamageApplicationResult& result,
                                      const BattleDamageTransactionResult& transaction)
{
    if (transaction.hurtInvincGranted && transaction.defenderDelta.invincibleDelta > 0)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = transaction.defender.id;
        log.targetUnitId = transaction.defender.id;
        log.amount = transaction.defenderDelta.invincibleDelta;
        log.text = formatStatusFrames("受傷無敵", transaction.defenderDelta.invincibleDelta);
        result.logEvents.push_back(std::move(log));
    }

    if (!transaction.killed)
    {
        return;
    }

    if (transaction.attackerDelta.hpDelta > 0)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Heal;
        log.sourceUnitId = transaction.attacker.id;
        log.targetUnitId = transaction.attacker.id;
        log.amount = transaction.attackerDelta.hpDelta;
        log.text = transaction.attacker.killHealPct > 0
            ? std::format("擊殺回復 {}%", transaction.attacker.killHealPct)
            : "擊殺回復";
        result.logEvents.push_back(std::move(log));
    }
    if (transaction.attackerDelta.invincibleDelta > 0)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = transaction.attacker.id;
        log.targetUnitId = transaction.attacker.id;
        log.amount = transaction.attackerDelta.invincibleDelta;
        log.text = formatStatusFrames("擊殺無敵", transaction.attackerDelta.invincibleDelta);
        result.logEvents.push_back(std::move(log));
    }
    if (transaction.attackerDelta.attackDelta > 0)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = transaction.attacker.id;
        log.targetUnitId = transaction.attacker.id;
        log.amount = transaction.attackerDelta.attackDelta;
        log.text = std::format("嗜血（+{}攻）", transaction.attackerDelta.attackDelta);
        result.logEvents.push_back(std::move(log));
    }
}

}  // namespace

BattleDamageApplicationResult BattleDamageApplicationSystem::apply(
    const BattleDamageApplicationInput& input) const
{
    assert(input.frame >= 0);

    BattleDamageApplicationResult result;
    result.deathEffects = input.deathEffects;
    auto units = input.units;
    auto pendingTransactions = input.aggregatePendingTransactionsByDefender
        ? aggregatePendingTransactions(input.pendingTransactions)
        : input.pendingTransactions;
    auto pendingPresentation = input.aggregatePendingTransactionsByDefender
        ? aggregatePendingPresentation(input.pendingTransactions, input.pendingPresentation)
        : input.pendingPresentation;
    assert(pendingPresentation.empty() || pendingPresentation.size() == pendingTransactions.size());

    for (std::size_t i = 0; i < pendingTransactions.size(); ++i)
    {
        const auto& pending = pendingTransactions[i];
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
                result.logEvents.push_back(makeDeathPreventionLog(event));
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
            result.gameplayEvents.push_back({
                BattleGameplayEventType::UnitDied,
                input.frame,
                event.sourceUnitId,
                event.targetUnitId,
                event.value,
            });
            result.logEvents.push_back({
                BattleLogEventType::UnitDied,
                input.frame,
                event.sourceUnitId,
                event.targetUnitId,
                event.value,
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

        if (!pendingPresentation.empty())
        {
            appendDamageOutputEvents(result, pendingPresentation[i], transaction);
        }
        appendDamageTransactionLogEvents(result, transaction);
        result.transactions.push_back(std::move(transaction));
    }

    updateBattleResult(result, input, units);
    auto followUpContext = input.projectileFollowUps;
    auto followUps = expandBattleProjectileFollowUpCommands(result.commands, followUpContext);
    result.commands = std::move(followUps.commands);
    result.visualEvents.insert(
        result.visualEvents.end(),
        followUps.visualEvents.begin(),
        followUps.visualEvents.end());
    result.logEvents.insert(
        result.logEvents.end(),
        followUps.logEvents.begin(),
        followUps.logEvents.end());
    return result;
}

}  // namespace KysChess::Battle
