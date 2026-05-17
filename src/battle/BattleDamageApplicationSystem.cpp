#include "BattleDamageApplicationSystem.h"

#include "../ChessEftIds.h"
#include "BattleComboTriggerSystem.h"
#include "../Find.h"
#include "BattleResourceRules.h"
#include "BattleStatusFormat.h"
#include "BattleUnitStore.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <numeric>
#include <set>
#include <tuple>

namespace KysChess::Battle
{

namespace
{

constexpr int RoleStatusEffectFrames = 45;

std::string formatStatusFrames(const char* label, int frames)
{
    if (frames <= 0)
    {
        return label;
    }
    return std::format("{}（{}幀）", label, frames);
}

std::string formatAppliedStatusLog(const BattleDamageEvent& event)
{
    auto withValue = [&](const char* label)
    {
        return event.value > 0
            ? std::format("{}（{}）", label, event.value)
            : std::string(label);
    };
    switch (event.statusType)
    {
    case BattleDamageStatusType::Frozen:
        return formatStatusFrames("受擊硬直", event.value);
    case BattleDamageStatusType::Poison:
        return withValue("中毒");
    case BattleDamageStatusType::Bleed:
        return withValue("流血");
    case BattleDamageStatusType::DamageReduceDebuff:
        return withValue("破防");
    case BattleDamageStatusType::MpBlocked:
        return formatStatusFrames("封內", event.value);
    case BattleDamageStatusType::None:
        return "狀態";
    }
    assert(false);
    return "狀態";
}

std::string formatAppliedStatusLog(
    const BattleDamageTransactionResult& transaction,
    const BattleDamageEvent& event)
{
    if (event.statusType != BattleDamageStatusType::Bleed)
    {
        return formatAppliedStatusLog(event);
    }

    const int currentStacks = std::max(event.value, transaction.defenderStatus.effects.bleedStacks);
    const int maxStacks = std::max(currentStacks, event.maxValue);
    return formatStatusRange("流血", currentStacks, maxStacks, "層");
}

BattleLogEvent makeDeathPreventionLog(const BattleDamageEvent& event)
{
    BattleLogEvent log;
    log.type = BattleLogEventType::Status;
    log.sourceUnitId = event.targetUnitId;
    log.targetUnitId = event.targetUnitId;
    log.amount = event.value;
    log.text = formatStatusFrames("死亡庇護", event.value);
    return log;
}

void appendDeathAoeCommand(BattleDamageApplicationResult& result,
                           const BattleDamageApplicationInput& input,
                           const BattleDamageTransactionResult& transaction,
                           int deadUnitId)
{
    auto effectIt = input.unitEffects->find(deadUnitId);
    if (effectIt == input.unitEffects->end() || effectIt->second.deathAoePct <= 0)
    {
        return;
    }

    BattleDeathAoeProjectileCommand command;
    command.sourceUnitId = deadUnitId;
    command.trackedTargetUnitId = transaction.attacker.id;
    command.damage = std::max(1, transaction.defender.vitals.maxHp * effectIt->second.deathAoePct / 100);
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
                        const BattleUnitStore& units)
{
    std::set<int> aliveTeams;
    for (const auto& unit : units.units)
    {
        if (unit.alive)
        {
            aliveTeams.insert(unit.team);
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

void applyLiveStatusToDamageModifier(
    const BattleStatusRuntimeUnit* status,
    BattleDamageModifierState& modifier)
{
    if (!status)
    {
        return;
    }

    modifier.poisonTimer = status->effects.poisonTimer;
    modifier.outgoingDamageReduceDebuffs.clear();
    for (const auto& debuff : status->effects.damageReduceDebuffs)
    {
        modifier.outgoingDamageReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
    }
}

std::vector<std::size_t> orderedPendingDamageIndexes(
    const std::vector<BattlePendingDamageIntent>& pendingDamage,
    bool sortByDefenderMagnitude)
{
    std::vector<std::size_t> indexes(pendingDamage.size());
    std::iota(indexes.begin(), indexes.end(), std::size_t{ 0 });
    if (!sortByDefenderMagnitude)
    {
        return indexes;
    }

    std::stable_sort(indexes.begin(), indexes.end(), [&](std::size_t lhs, std::size_t rhs)
    {
        const auto& left = pendingDamage[lhs].request;
        const auto& right = pendingDamage[rhs].request;
        return std::tuple{ left.defenderUnitId, -left.baseDamage }
            < std::tuple{ right.defenderUnitId, -right.baseDamage };
    });
    return indexes;
}

void applyDamageTakenMpGain(BattleDamageTransactionResult& transaction)
{
    if (transaction.finalHpDamage <= 0 || transaction.defender.vitals.maxHp <= 0)
    {
        return;
    }

    const int baseGain = static_cast<int>(
        static_cast<double>(transaction.finalHpDamage) / transaction.defender.vitals.maxHp * 75.0);
    const int mpGain = adjustedMpRestore(
        transaction.defender.mpBlocked,
        transaction.defender.mpRecoveryBonusPct,
        baseGain);
    if (mpGain <= 0)
    {
        return;
    }

    const int before = transaction.defender.vitals.mp;
    transaction.defender.vitals.mp = std::min(transaction.defender.vitals.maxMp, transaction.defender.vitals.mp + mpGain);
    transaction.defenderDelta.mpDelta += transaction.defender.vitals.mp - before;
}

void applyResolvedTransactionToLiveState(
    const BattleDamageApplicationInput& input,
    const BattleDamageTransactionResult& transaction)
{
    input.units->writeDamageUnit(transaction.defender);
    auto& defenderExtras = requireById(*input.damageUnitExtras, transaction.defender.id);
    writeBattleDamageRuntimeUnit(defenderExtras, transaction.defender);
    auto& defenderStatus = requireById(*input.statusUnits, transaction.defender.id);
    writeBattleStatusRuntimeUnit(defenderStatus, transaction.defenderStatus);

    if (transaction.attacker.id >= 0)
    {
        input.units->writeDamageUnit(transaction.attacker);
        auto& attackerExtras = requireById(*input.damageUnitExtras, transaction.attacker.id);
        writeBattleDamageRuntimeUnit(attackerExtras, transaction.attacker);
    }
}

BattleDamageTransactionInput makeDamageTransactionInputFromIntent(
    const BattlePendingDamageIntent& intent,
    const BattleDamageApplicationInput& input);

template <typename Callback>
void resolvePendingDamageTransactions(const BattleDamageApplicationInput& input, Callback&& callback)
{
    assert(input.units);
    assert(input.comboUnits);
    assert(input.statusUnits);
    assert(input.damageUnitExtras);
    assert(input.pendingDamage);

    for (const auto pendingIndex : orderedPendingDamageIndexes(
             *input.pendingDamage,
             input.sortPendingDamageByDefenderMagnitude))
    {
        const auto& intent = (*input.pendingDamage)[pendingIndex];
        auto transaction = BattleDamageSystem().resolveTransaction(
            makeDamageTransactionInputFromIntent(intent, input));
        applyDamageTakenMpGain(transaction);
        applyResolvedTransactionToLiveState(input, transaction);
        callback(intent, std::move(transaction));
    }
}

BattleDamageTransactionInput makeDamageTransactionInputFromIntent(
    const BattlePendingDamageIntent& intent,
    const BattleDamageApplicationInput& input)
{
    assert(input.units);
    assert(input.comboUnits);
    assert(input.statusUnits);
    assert(input.damageUnitExtras);
    assert(intent.request.defenderUnitId >= 0);

    BattleDamageTransactionInput transaction;
    transaction.request = intent.request;

    const auto& defenderUnit = input.units->requireUnit(intent.request.defenderUnitId);
    auto& defenderExtras = requireById(*input.damageUnitExtras, intent.request.defenderUnitId);
    transaction.defender = makeBattleDamageUnitState(defenderUnit, &defenderExtras);
    auto& defenderCombo = requireMappedById(*input.comboUnits, intent.request.defenderUnitId);
    transaction.defenderModifiers = makeBattleDamageModifierState(&defenderCombo);

    const auto& defenderStatus = requireById(*input.statusUnits, intent.request.defenderUnitId);
    transaction.defenderStatus = makeBattleStatusUnitState(defenderStatus, defenderUnit);
    applyLiveStatusToDamageModifier(&defenderStatus, transaction.defenderModifiers);
    transaction.defenderCooldown = makeBattleFrameCooldownState(defenderUnit);

    if (intent.request.attackerUnitId >= 0)
    {
        const auto& attackerUnit = input.units->requireUnit(intent.request.attackerUnitId);
        auto& attackerExtras = requireById(*input.damageUnitExtras, intent.request.attackerUnitId);
        transaction.attacker = makeBattleDamageUnitState(attackerUnit, &attackerExtras);

        auto& attackerCombo = requireMappedById(*input.comboUnits, intent.request.attackerUnitId);
        transaction.attackerModifiers = makeBattleDamageModifierState(&attackerCombo);
        applyLiveStatusToDamageModifier(
            tryFindById(*input.statusUnits, intent.request.attackerUnitId),
            transaction.attackerModifiers);
    }
    else
    {
        transaction.attacker.id = intent.request.attackerUnitId;
    }

    return transaction;
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

BattleVisualEvent roleEffectEvent(int targetUnitId, int effectId, int durationFrames)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::RoleEffect;
    event.targetUnitId = targetUnitId;
    event.effectId = effectId;
    event.visualEffectId = effectId;
    event.durationFrames = durationFrames;
    return event;
}

void appendDamageOutputEvents(BattleDamageApplicationResult& result,
                              const BattleDamagePresentationInput& presentation,
                              const BattleDamageTransactionResult& transaction)
{
    const int hpDamage = committedHpDamage(transaction);
    if (hpDamage <= 0)
    {
        return;
    }

    if (presentation.enabled && presentation.executed)
    {
        BattleVisualEvent executedText;
        executedText.type = BattleVisualEventType::FloatingText;
        executedText.targetUnitId = transaction.defender.id;
        executedText.text = "處決！";
        executedText.color = presentation.executeTextColor;
        executedText.textSize = presentation.executeTextSize;
        result.visualEvents.push_back(std::move(executedText));
    }
    else if (presentation.enabled)
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

void appendDamageTransactionPreDeathLogEvents(BattleDamageApplicationResult& result,
                                              const BattleDamageTransactionResult& transaction)
{
    for (const auto& event : transaction.events)
    {
        if (event.type != BattleDamageEventType::StatusApplied)
        {
            continue;
        }

        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = event.sourceUnitId;
        log.targetUnitId = event.targetUnitId;
        log.amount = event.value;
        log.text = formatAppliedStatusLog(transaction, event);
        result.logEvents.push_back(std::move(log));
    }

    if (transaction.blockedByFirstHit)
    {
        result.visualEvents.push_back(roleEffectEvent(transaction.defender.id, KysChess::EFT_BLOCK, RoleStatusEffectFrames));
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = transaction.defender.id;
        log.targetUnitId = transaction.attacker.id;
        log.text = "格擋了首輪傷害";
        result.logEvents.push_back(std::move(log));
    }

    if (transaction.shieldAbsorbed > 0)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = transaction.defender.id;
        log.targetUnitId = transaction.attacker.id;
        log.amount = transaction.shieldAbsorbed;
        log.text = std::format("護盾吸收 {}", transaction.shieldAbsorbed);
        result.logEvents.push_back(std::move(log));
    }

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
}

void appendDamageTransactionKillRewardLogEvents(BattleDamageApplicationResult& result,
                                                const BattleDamageTransactionResult& transaction)
{
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

void appendDamageLifecycleEvents(BattleDamageApplicationResult& result,
                                 const BattleDamageApplicationInput& input,
                                 const BattleDamageTransactionResult& transaction)
{
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
        auto deathEvents = BattleDeathEffectSystem().applyAllyDeathEffects(
            *input.units,
            *input.deathEffects,
            event.targetUnitId);
        for (const auto& deathEvent : deathEvents)
        {
            appendDeathEffectCommand(result, deathEvent);
        }
    }
}

void appendShieldBreakCommands(BattleDamageApplicationResult& result,
                               const BattleDamageApplicationInput& input,
                               const BattleDamageTransactionResult& transaction)
{
    if (!(transaction.shieldAbsorbed > 0
            && transaction.defenderDelta.shieldDelta < 0
            && transaction.defender.shield == 0))
    {
        return;
    }

    auto& defenderCombo = requireMappedById(*input.comboUnits, transaction.defender.id);
    int shieldExplosionPct = 0;
    auto shieldBreakCommands = BattleComboTriggerSystem().collectShieldBreakCommands(
        defenderCombo,
        { BattleComboTriggerHook::ShieldBreak, transaction.defender.id, transaction.defender.id },
        *input.random);
    for (const auto& shieldBreak : shieldBreakCommands)
    {
        bool activated = true;
        switch (shieldBreak.type)
        {
        case BattleShieldBreakCommandType::ShieldExplosion:
            shieldExplosionPct = std::max(shieldExplosionPct, shieldBreak.value);
            break;
        case BattleShieldBreakCommandType::AutoUltimate:
            result.commands.push_back(BattleAutoUltimateCommand{ transaction.defender.id, false });
            break;
        case BattleShieldBreakCommandType::TempFlatAttack:
            result.commands.push_back(BattleTempAttackBuffCommand{
                transaction.defender.id,
                shieldBreak.value,
                shieldBreak.durationFrames,
                std::format("護盾爆炸（臨時攻+{}，{}幀）", shieldBreak.value, shieldBreak.durationFrames),
            });
            break;
        case BattleShieldBreakCommandType::MpRestore:
        {
            int restored = std::min(
                shieldBreak.value,
                std::max(0, transaction.defender.vitals.maxMp - transaction.defender.vitals.mp));
            if (restored > 0)
            {
                result.commands.push_back(BattleMpRestoreCommand{
                    transaction.defender.id,
                    restored,
                    std::format("護盾爆炸·回內力+{}", restored),
                });
            }
            else
            {
                activated = false;
            }
            break;
        }
        default:
            assert(false);
        }

        if (activated)
        {
            BattleComboTriggerSystem().recordActivation(
                defenderCombo,
                static_cast<size_t>(shieldBreak.effectIndex));
        }
    }
    if (shieldExplosionPct > 0)
    {
        int explosionDamage = std::max(
            1,
            defenderCombo.shieldPctMaxHP * transaction.defender.vitals.maxHp / 100 * shieldExplosionPct / 100);
        result.commands.push_back(BattleShieldExplosionCommand{
            transaction.defender.id,
            5,
            KysChess::EFT_SHIELD_BLAST,
            explosionDamage,
            "護盾爆炸",
        });
    }
}

}  // namespace

BattleDamageApplicationResult BattleDamageApplicationSystem::apply(
    const BattleDamageApplicationInput& input) const
{
    assert(input.frame >= 0);
    assert(input.units);
    assert(input.comboUnits);
    assert(input.statusUnits);
    assert(input.damageUnitExtras);
    assert(input.pendingDamage);
    assert(input.unitEffects);
    assert(input.deathEffects);
    assert(input.projectileFollowUps);
    assert(input.random);

    BattleDamageApplicationResult result;
    resolvePendingDamageTransactions(input, [&](const BattlePendingDamageIntent& intent, BattleDamageTransactionResult transaction)
    {
        const auto& presentation = intent.presentation;
        appendShieldBreakCommands(result, input, transaction);
        appendDamageOutputEvents(result, presentation, transaction);
        appendDamageTransactionPreDeathLogEvents(result, transaction);
        appendDamageLifecycleEvents(result, input, transaction);
        appendDamageTransactionKillRewardLogEvents(result, transaction);
        result.transactions.push_back(std::move(transaction));
    });

    updateBattleResult(result, input, *input.units);
    auto followUpContext = *input.projectileFollowUps;
    auto followUps = expandBattleProjectileFollowUpCommands(
        result.commands,
        followUpContext,
        *input.units);
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

int BattleDamageApplicationSystem::previewDefenderPendingHpDamage(
    BattleDamageApplicationInput input,
    int defenderUnitId) const
{
    assert(input.units);
    assert(input.comboUnits);
    assert(input.statusUnits);
    assert(input.damageUnitExtras);
    assert(input.pendingDamage);
    if (input.pendingDamage->empty())
    {
        return 0;
    }

    const int initialHp = input.units->requireUnit(defenderUnitId).vitals.hp;
    resolvePendingDamageTransactions(input, [](const BattlePendingDamageIntent&, BattleDamageTransactionResult&&)
    {
    });
    return std::max(0, initialHp - input.units->requireUnit(defenderUnitId).vitals.hp);
}

}  // namespace KysChess::Battle
