#include "BattleComboTriggerSystem.h"

#include "BattleDamageSystem.h"
#include "BattleRuntimeRandom.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <span>
#include <utility>

namespace KysChess::Battle
{
namespace
{

bool passesChance(BattleRuntimeRandom& random, int chancePct)
{
    if (chancePct <= 0)
    {
        return false;
    }
    if (chancePct >= 100)
    {
        return true;
    }
    return random.chance(chancePct);
}

std::span<const Trigger> triggersForHook(BattleComboTriggerHook hook)
{
    static constexpr std::array<Trigger, 3> frameTriggers = {
        Trigger::WhileLowHP,
        Trigger::AllyLowHPBurst,
        Trigger::LastAlive,
    };
    static constexpr std::array<Trigger, 2> castTriggers = {
        Trigger::OnCast,
        Trigger::OnUltimate,
    };
    static constexpr std::array<Trigger, 1> onHitTriggers = { Trigger::OnHit };
    static constexpr std::array<Trigger, 1> onBeingHitTriggers = { Trigger::OnBeingHit };
    static constexpr std::array<Trigger, 1> shieldBreakTriggers = { Trigger::OnShieldBreak };
    static constexpr std::array<Trigger, 0> noTriggers = {};

    switch (hook)
    {
    case BattleComboTriggerHook::FrameTick:
        return frameTriggers;
    case BattleComboTriggerHook::AfterSkillCast:
        return castTriggers;
    case BattleComboTriggerHook::ProjectileHitEnemy:
    case BattleComboTriggerHook::ProjectileHitAllyOrSource:
    case BattleComboTriggerHook::DamageDealt:
        return onHitTriggers;
    case BattleComboTriggerHook::DamageTaken:
        return onBeingHitTriggers;
    case BattleComboTriggerHook::ShieldBreak:
        return shieldBreakTriggers;
    default:
        return noTriggers;
    }
}

std::vector<RoleComboEffectId> sortedCandidateIds(
    const RoleComboState& state,
    std::span<const Trigger> triggers,
    std::initializer_list<EffectType> effectTypes)
{
    std::vector<RoleComboEffectId> ids;
    for (Trigger trigger : triggers)
    {
        for (EffectType type : effectTypes)
        {
            for (RoleComboEffectId id : state.effectIds(trigger, type))
            {
                ids.push_back(id);
            }
        }
    }
    std::ranges::sort(ids, {}, &RoleComboEffectId::value);
    return ids;
}

std::vector<RoleComboEffectId> sortedCandidateIds(
    const RoleComboState& state,
    Trigger trigger,
    std::initializer_list<EffectType> effectTypes)
{
    std::array triggers = { trigger };
    return sortedCandidateIds(state, std::span<const Trigger>{ triggers }, effectTypes);
}

ComboTriggerTimerKey triggerTimerKeyFor(const ComboEffectSnapshot& effect)
{
    assert(effect.trigger == Trigger::AllyLowHPBurst);
    return { effect.trigger, effect.sourceComboId };
}

bool isActiveFrameTrigger(const RoleComboState& state, const BattleComboFrameUnit& unit, const ComboEffectSnapshot& effect)
{
    assert(unit.maxHp > 0);

    if (effect.trigger == Trigger::Always)
    {
        return true;
    }
    if (effect.trigger == Trigger::WhileLowHP)
    {
        return unit.hp * 100 < unit.maxHp * effect.triggerValue;
    }
    if (effect.trigger == Trigger::LastAlive)
    {
        return unit.lastAlive;
    }
    if (effect.trigger == Trigger::AllyLowHPBurst)
    {
        return state.hasActiveTriggerTimer(triggerTimerKeyFor(effect));
    }
    return false;
}

std::vector<RoleComboStackChange> applyDamageAdaptationStacks(
    RoleComboState& state,
    int attackerUnitId)
{
    assert(attackerUnitId >= 0);
    std::vector<RoleComboStackChange> changes;
    for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::Adaptation))
    {
        const auto& effect = state.effect(id);
        changes.push_back(state.recordEffectStackAgainst(id, attackerUnitId, effect.value2, effect.value));
    }
    return changes;
}

std::vector<RoleComboStackChange> applyDodgeAdaptationStacks(
    RoleComboState& state,
    int attackerUnitId)
{
    assert(attackerUnitId >= 0);
    std::vector<RoleComboStackChange> changes;
    for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::DodgeAdaptation))
    {
        const auto& effect = state.effect(id);
        changes.push_back(state.recordEffectStackAgainst(id, attackerUnitId, effect.value2, effect.value));
    }
    return changes;
}

std::vector<RoleComboStackChange> applyRampingStacks(RoleComboState& state)
{
    std::vector<RoleComboStackChange> changes;
    for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::RampingDmg))
    {
        const auto& effect = state.effect(id);
        auto change = state.recordEffectStack(id, effect.value2, effect.value);
        state.setEffectIdleTimer(id, 90);
        changes.push_back(change);
    }
    return changes;
}

}  // namespace

bool BattleComboTriggerSystem::canActivate(const RoleComboState& state, RoleComboEffectId effectId) const
{
    return state.canActivateTriggeredEffect(effectId);
}

void BattleComboTriggerSystem::recordActivation(RoleComboState& state, RoleComboEffectId effectId) const
{
    state.recordTriggeredEffectActivation(effectId);
}

std::vector<BattleComboTriggerAction> BattleComboTriggerSystem::updateFrameTriggers(
    RoleComboState& state,
    const BattleComboFrameUnit& unit) const
{
    assert(unit.maxHp > 0);

    std::vector<BattleComboTriggerAction> actions;
    std::vector<RoleComboEffectId> ids;
    for (Trigger trigger : triggersForHook(BattleComboTriggerHook::FrameTick))
    {
        for (RoleComboEffectId id : state.effectIdsInAppendOrder())
        {
            const auto& effect = state.effect(id);
            if (effect.trigger == trigger)
            {
                ids.push_back(id);
            }
        }
    }
    std::ranges::sort(ids, {}, &RoleComboEffectId::value);

    for (RoleComboEffectId id : ids)
    {
        const auto& effect = state.effect(id);
        if (!canActivate(state, id))
        {
            continue;
        }

        bool active = false;
        if (effect.trigger == Trigger::WhileLowHP)
        {
            active = unit.hp * 100 < unit.maxHp * effect.triggerValue;
        }
        else if (effect.trigger == Trigger::LastAlive)
        {
            active = unit.lastAlive;
        }
        else if (effect.trigger == Trigger::AllyLowHPBurst)
        {
            const auto timerKey = triggerTimerKeyFor(effect);
            if (unit.hp * 100 < unit.maxHp * effect.triggerValue
                && !state.hasActiveTriggerTimer(timerKey))
            {
                recordActivation(state, id);
                actions.push_back({
                    BattleComboTriggerActionType::BroadcastTriggerTimer,
                    effect.trigger,
                    timerKey,
                    id,
                    effect.value,
                    effect.duration,
                });
            }
            continue;
        }

        if (active && effect.type == EffectType::HealBurst)
        {
            recordActivation(state, id);
            actions.push_back({
                BattleComboTriggerActionType::HealPercentSelf,
                effect.trigger,
                {},
                id,
                effect.value,
                effect.duration,
            });
        }
    }
    return actions;
}

std::vector<BattleComboFrameRuntimeEvent> BattleComboTriggerSystem::advanceFrameRuntime(
    RoleComboState& state,
    const BattleComboFrameRuntimeInput& input) const
{
    assert(input.frame >= 0);
    assert(input.maxHp > 0);

    std::vector<BattleComboFrameRuntimeEvent> events;
    state.setLastAliveForComboRuntime(input.lastAlive);

    if (input.alive)
    {
        for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::AutoUltimateAfterFrames))
        {
            const auto& effect = state.effect(id);
            if (effect.value > 0 && state.advanceAutoUltimateFrameTimer(id, effect.value))
            {
                events.push_back({ BattleComboFrameRuntimeEventType::AutoUltimateReady });
            }
        }

        for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::HPRegenPct))
        {
            const auto& effect = state.effect(id);
            if (effect.value > 0 && effect.value2 > 0 && input.frame % effect.value2 == 0)
            {
                events.push_back({
                    BattleComboFrameRuntimeEventType::SelfHpRegen,
                    effect.trigger,
                    {},
                    id,
                    effect.value,
                });
            }
        }
    }

    int healAuraFlat = 0;
    int healAuraPct = 0;
    int healAuraInterval = 0;
    int healedBoostPct = 0;
    for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::HealAuraFlat))
    {
        const auto& effect = state.effect(id);
        healAuraFlat = std::max(healAuraFlat, effect.value);
        healAuraInterval = effect.value2 > 0 ? effect.value2 : healAuraInterval;
    }
    for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::HealAuraPct))
    {
        const auto& effect = state.effect(id);
        healAuraPct = std::max(healAuraPct, effect.value);
        healAuraInterval = effect.value2 > 0 ? effect.value2 : healAuraInterval;
    }
    for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::HealedATKSPDBoost))
    {
        healedBoostPct += state.effect(id).value;
    }
    if (input.alive && (healAuraPct > 0 || healAuraFlat > 0) && healAuraInterval > 0 && input.frame % healAuraInterval == 0)
    {
        events.push_back({
            BattleComboFrameRuntimeEventType::HealAura,
            Trigger::Always,
            {},
            {},
            healAuraFlat,
            healAuraPct,
            healedBoostPct,
        });
    }

    if (input.alive)
    {
        for (const auto& action : updateFrameTriggers(
                 state,
                 { input.hp, input.maxHp, input.lastAlive }))
        {
            if (action.type == BattleComboTriggerActionType::HealPercentSelf)
            {
                events.push_back({
                    BattleComboFrameRuntimeEventType::HealPercentSelf,
                    action.trigger,
                    {},
                    action.effectId,
                    action.value,
                    0,
                    action.durationFrames,
                });
            }
            else
            {
                assert(action.type == BattleComboTriggerActionType::BroadcastTriggerTimer);
                events.push_back({
                    BattleComboFrameRuntimeEventType::BroadcastTriggerTimer,
                    action.trigger,
                    action.timerKey,
                    action.effectId,
                    action.value,
                    0,
                    action.durationFrames,
                });
            }
        }
    }

    std::vector<RoleComboEffectId> rampingIds;
    for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::RampingDmg))
    {
        rampingIds.push_back(id);
    }
    state.advanceEffectIdleTimers(rampingIds);
    state.advanceTriggerTimersOneFrame();

    return events;
}

BattleTriggeredTeamHeal BattleComboTriggerSystem::collectTeamHeal(
    RoleComboState& state,
    Trigger trigger,
    BattleRuntimeRandom& random) const
{
    BattleTriggeredTeamHeal result;
    for (RoleComboEffectId id : sortedCandidateIds(
             state,
             trigger,
             { EffectType::OnSkillTeamHeal, EffectType::OnSkillTeamHealPct }))
    {
        const auto& effect = state.effect(id);
        if (!canActivate(state, id))
        {
            continue;
        }
        if (trigger == Trigger::OnHit && effect.triggerValue > 0 && !passesChance(random, effect.triggerValue))
        {
            continue;
        }

        if (effect.type == EffectType::OnSkillTeamHeal)
        {
            result.flatHeal += effect.value;
        }
        else
        {
            assert(effect.type == EffectType::OnSkillTeamHealPct);
            result.pctHeal += effect.value;
        }
        recordActivation(state, id);
        result.activatedEffectIds.push_back(id);
    }
    return result;
}

BattleTriggeredTeamHeal BattleComboTriggerSystem::collectTriggeredTeamHeal(
    RoleComboState& state,
    const BattleComboTriggerInput& input,
    BattleRuntimeRandom& random) const
{
    auto events = collectTriggerEvents(
        state,
        input,
        { EffectType::OnSkillTeamHeal, EffectType::OnSkillTeamHealPct },
        random);

    BattleTriggeredTeamHeal result;
    for (const auto& event : events)
    {
        if (event.effect.type == EffectType::OnSkillTeamHeal)
        {
            result.flatHeal += event.effect.value;
        }
        else
        {
            assert(event.effect.type == EffectType::OnSkillTeamHealPct);
            result.pctHeal += event.effect.value;
        }
        result.activatedEffectIds.push_back(event.effectId);
    }
    return result;
}

BattleTriggeredTeamHeal BattleComboTriggerSystem::collectPendingSkillTeamHeal(
    RoleComboState& state,
    const BattleComboTriggerInput& input,
    BattleRuntimeRandom& random) const
{
    BattleTriggeredTeamHeal result;
    if (!state.consumeTypePending(EffectType::OnSkillTeamHeal))
    {
        return result;
    }

    result.flatHeal = state.sumAlways(EffectType::OnSkillTeamHeal);
    result.pctHeal = state.sumAlways(EffectType::OnSkillTeamHealPct);
    auto triggered = collectTriggeredTeamHeal(state, input, random);
    result.flatHeal += triggered.flatHeal;
    result.pctHeal += triggered.pctHeal;
    result.activatedEffectIds = std::move(triggered.activatedEffectIds);
    return result;
}

std::vector<BattleActivatedComboEffect> BattleComboTriggerSystem::collectChanceEffects(
    RoleComboState& state,
    Trigger trigger,
    std::initializer_list<EffectType> effectTypes,
    BattleRuntimeRandom& random,
    BattleComboActivationRecording recording) const
{
    assert(effectTypes.size() > 0);

    std::vector<BattleActivatedComboEffect> result;
    for (RoleComboEffectId id : sortedCandidateIds(state, trigger, effectTypes))
    {
        const auto& effect = state.effect(id);
        if (!canActivate(state, id))
        {
            continue;
        }
        if (effect.triggerValue > 0 && !passesChance(random, effect.triggerValue))
        {
            continue;
        }

        if (recording == BattleComboActivationRecording::RecordOnCollect)
        {
            recordActivation(state, id);
        }
        result.push_back({ id, effect });
    }
    return result;
}

std::vector<BattleComboTriggerEvent> BattleComboTriggerSystem::collectTriggerEvents(
    RoleComboState& state,
    const BattleComboTriggerInput& input,
    std::initializer_list<EffectType> effectTypes,
    BattleRuntimeRandom& random,
    BattleComboActivationRecording recording) const
{
    assert(effectTypes.size() > 0);

    std::vector<BattleComboTriggerEvent> result;
    for (RoleComboEffectId id : sortedCandidateIds(state, triggersForHook(input.hook), effectTypes))
    {
        const auto& effect = state.effect(id);
        if (!canActivate(state, id))
        {
            continue;
        }
        if (effect.triggerValue > 0 && !passesChance(random, effect.triggerValue))
        {
            continue;
        }

        if (recording == BattleComboActivationRecording::RecordOnCollect)
        {
            recordActivation(state, id);
        }
        result.push_back({
            input.hook,
            input.sourceUnitId,
            input.targetUnitId,
            id,
            effect,
        });
    }
    return result;
}

std::vector<BattleComboTriggerEvent> BattleComboTriggerSystem::matchingTriggerEffects(
    const RoleComboState& state,
    const BattleComboTriggerInput& input,
    std::initializer_list<EffectType> effectTypes) const
{
    assert(effectTypes.size() > 0);

    std::vector<BattleComboTriggerEvent> result;
    for (RoleComboEffectId id : sortedCandidateIds(state, triggersForHook(input.hook), effectTypes))
    {
        result.push_back({
            input.hook,
            input.sourceUnitId,
            input.targetUnitId,
            id,
            state.effect(id),
        });
    }
    return result;
}

std::vector<BattleComboTriggerEvent> BattleComboTriggerSystem::activeFrameTriggerEffects(
    const RoleComboState& state,
    const BattleComboFrameUnit& unit,
    std::initializer_list<EffectType> effectTypes) const
{
    assert(effectTypes.size() > 0);
    assert(unit.maxHp > 0);

    std::vector<BattleComboTriggerEvent> result;
    std::vector<RoleComboEffectId> ids;
    for (RoleComboEffectId id : sortedCandidateIds(state, triggersForHook(BattleComboTriggerHook::FrameTick), effectTypes))
    {
        ids.push_back(id);
    }
    std::ranges::sort(ids, {}, &RoleComboEffectId::value);

    for (RoleComboEffectId id : ids)
    {
        const auto& effect = state.effect(id);
        if (!isActiveFrameTrigger(state, unit, effect))
        {
            continue;
        }

        result.push_back({
            BattleComboTriggerHook::FrameTick,
            -1,
            -1,
            id,
            effect,
        });
    }
    return result;
}

BattleDodgeResolution BattleComboTriggerSystem::resolveDodge(
    const RoleComboState& state,
    int attackerUnitId,
    double rollPercent) const
{
    assert(attackerUnitId >= 0);
    assert(rollPercent >= 0.0 && rollPercent < 100.0);

    int dodgeChancePct = state.sumAlways(EffectType::DodgeChance);
    dodgeChancePct += state.dodgeAdaptationBonusAgainst(attackerUnitId);

    BattleDodgeResolution result;
    result.chancePct = std::clamp(dodgeChancePct, 0, 100);
    result.dodged = result.chancePct > 0 && rollPercent < result.chancePct;
    return result;
}

BattleAttackerHitDamageResult BattleComboTriggerSystem::shapeAttackerHitDamage(
    RoleComboState& state,
    const BattleAttackerHitDamageInput& input,
    BattleRuntimeRandom& random) const
{
    assert(input.maxHp > 0);

    BattleAttackerHitDamageResult result;
    result.damage = input.damage;

    for (const auto& event : activeFrameTriggerEffects(
             state,
             { input.hp, input.maxHp, input.lastAlive },
             { EffectType::PctATK }))
    {
        result.damage *= (1.0 + event.effect.value / 100.0);
    }

    bool critted = state.hasAlways(EffectType::DodgeThenCrit)
        && state.consumeTypePending(EffectType::DodgeThenCrit);
    const int critChancePct = state.sumAlways(EffectType::CritChance);
    if (!critted && passesChance(random, critChancePct))
    {
        critted = true;
    }
    if (critted)
    {
        const int critMultiplier = std::max(150, state.maxAlways(EffectType::CritMultiplier));
        result.damage *= critMultiplier / 100.0;
        result.events.push_back({
            BattleAttackerHitDamageEventType::Crit,
            critMultiplier,
        });
    }

    for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::EveryNthDouble))
    {
        const auto& effect = state.effect(id);
        if (effect.value > 0 && state.advanceEffectCounter(id, effect.value))
        {
            result.damage *= 2.0;
        }
    }

    for (const auto& ramping : applyRampingStacks(state))
    {
        result.damage *= (1.0 + ramping.stacks * ramping.pctPerStack / 100.0);
        if (ramping.increased)
        {
            result.events.push_back({
                BattleAttackerHitDamageEventType::RampingStack,
                ramping.pctPerStack,
                ramping.stacks,
            });
        }
    }

    return result;
}

BattleDefenderHitDamageResult BattleComboTriggerSystem::shapeDefenderHitDamage(
    RoleComboState& state,
    const BattleDefenderHitDamageInput& input) const
{
    assert(input.maxHp > 0);
    assert(input.attackerUnitId >= 0);

    BattleDefenderHitDamageResult result;
    result.damage = input.damage;

    for (const auto& event : activeFrameTriggerEffects(
             state,
             { input.hp, input.maxHp, input.lastAlive },
             { EffectType::DmgReductionPct }))
    {
        result.damage *= (1.0 - event.effect.value / 100.0);
    }

    for (const auto& adaptation : applyDamageAdaptationStacks(state, input.attackerUnitId))
    {
        result.damage *= (1.0 - adaptation.stacks * adaptation.pctPerStack / 100.0);
        if (adaptation.increased)
        {
            result.events.push_back({
                BattleDefenderHitDamageEventType::DamageAdaptationStack,
                adaptation.pctPerStack,
                adaptation.stacks,
            });
        }
    }

    for (const auto& adaptation : applyDodgeAdaptationStacks(state, input.attackerUnitId))
    {
        if (adaptation.increased)
        {
            result.events.push_back({
                BattleDefenderHitDamageEventType::DodgeAdaptationStack,
                adaptation.pctPerStack,
                adaptation.stacks,
            });
        }
    }

    return result;
}

BattleExecuteComboResult BattleComboTriggerSystem::resolveExecuteCombo(
    RoleComboState& state,
    const BattleExecuteComboInput& input,
    BattleRuntimeRandom& random) const
{
    assert(input.attackerUnitId >= 0);
    assert(input.targetUnitId >= 0);

    BattleExecuteComboResult result;
    for (const auto& triggerEvent : matchingTriggerEffects(
             state,
             { BattleComboTriggerHook::DamageDealt, input.attackerUnitId, input.targetUnitId },
             { EffectType::Execute }))
    {
        const auto& effect = triggerEvent.effect;
        assert(effect.type == EffectType::Execute);
        if (effect.triggerValue <= 0 || effect.value <= 0)
        {
            continue;
        }

        if (BattleDamageSystem().shouldExecute({
                input.projectedHpBeforeDamage,
                input.maxHp,
                input.pendingDamage,
                input.appliesHpDamage,
                effect.value,
            })
            && passesChance(random, effect.triggerValue))
        {
            recordActivation(state, triggerEvent.effectId);
            result.executed = true;
            result.thresholdPct = effect.value;
            result.effectId = triggerEvent.effectId;
            break;
        }
    }
    return result;
}

bool BattleComboTriggerSystem::resolveProjectileReflect(
    const RoleComboState& state,
    bool rangedProjectile,
    BattleRuntimeRandom& random) const
{
    return rangedProjectile && passesChance(random, state.sumAlways(EffectType::ProjectileReflect));
}

BattleProjectileBouncePrime BattleComboTriggerSystem::collectProjectileBouncePrime(
    const RoleComboState& state,
    const BattleProjectileBouncePrimeInput& input) const
{
    assert(input.attackerUnitId >= 0);
    assert(input.rollPct >= 0 && input.rollPct < 100);
    assert(input.defaultRange > 0);

    BattleProjectileBouncePrime prime;
    prime.rollPct = input.rollPct;
    for (const auto& event : matchingTriggerEffects(
             state,
             { BattleComboTriggerHook::ProjectileHitEnemy, input.attackerUnitId, -1 },
             { EffectType::ProjectileBounce }))
    {
        const auto& effect = event.effect;
        if (effect.value > 0)
        {
            prime.count += effect.value;
        }
        if (effect.triggerValue > 0)
        {
            prime.chancePct = std::min(100, prime.chancePct + effect.triggerValue);
        }
        if (effect.value2 > 0)
        {
            prime.range = std::max(prime.range, effect.value2);
        }
    }
    if (prime.count > 0 && prime.range <= 0)
    {
        prime.range = input.defaultRange;
    }
    return prime;
}

int BattleComboTriggerSystem::collectExtraProjectileCount(
    RoleComboState& state,
    const BattleComboTriggerInput& input,
    int baseCount,
    BattleRuntimeRandom& random) const
{
    assert(baseCount >= 0);
    int count = baseCount;
    for (const auto& event : collectTriggerEvents(
             state,
             input,
             { EffectType::UltimateExtraProjectiles },
             random))
    {
        assert(event.effect.value > 0);
        count += event.effect.value;
    }
    return count;
}

bool BattleComboTriggerSystem::hasExecuteCombo(
    const RoleComboState& state,
    int attackerUnitId) const
{
    assert(attackerUnitId >= 0);
    auto effects = matchingTriggerEffects(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, attackerUnitId, -1 },
        { EffectType::Execute });
    return std::any_of(effects.begin(), effects.end(), [](const auto& event)
        {
            return event.effect.triggerValue > 0 && event.effect.value > 0;
        });
}

BattleArmorPenetrationResult BattleComboTriggerSystem::resolveArmorPenetratedDefense(
    const RoleComboState& state,
    const BattleArmorPenetrationInput& input,
    BattleRuntimeRandom& random) const
{
    assert(input.attackerUnitId >= 0);
    assert(input.targetUnitId >= 0);
    assert(input.defense >= 0.0);

    BattleArmorPenetrationResult result;
    result.defense = input.defense;
    const int armorPenChancePct = state.sumAlways(EffectType::ArmorPenChance);
    if (passesChance(random, armorPenChancePct))
    {
        result.defense *= (1.0 - state.maxAlways(EffectType::ArmorPenPct) / 100.0);
    }

    for (const auto& event : matchingTriggerEffects(
             state,
             { BattleComboTriggerHook::DamageDealt, input.attackerUnitId, input.targetUnitId },
             { EffectType::ArmorPen }))
    {
        if (passesChance(random, event.effect.triggerValue))
        {
            result.defense *= (1.0 - event.effect.value / 100.0);
        }
    }
    return result;
}

}  // namespace KysChess::Battle
