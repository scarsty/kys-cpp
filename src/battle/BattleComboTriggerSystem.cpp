#include "BattleComboTriggerSystem.h"

#include "BattleDamageSystem.h"
#include "BattleRuntimeRandom.h"

#include <algorithm>
#include <cassert>
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

bool hookMatchesConfiguredTrigger(BattleComboTriggerHook hook, Trigger trigger)
{
    switch (hook)
    {
    case BattleComboTriggerHook::FrameTick:
        return trigger == Trigger::Always
            || trigger == Trigger::WhileLowHP
            || trigger == Trigger::AllyLowHPBurst
            || trigger == Trigger::LastAlive;
    case BattleComboTriggerHook::AfterSkillCast:
        return trigger == Trigger::OnCast
            || trigger == Trigger::OnUltimate;
    case BattleComboTriggerHook::ProjectileHitEnemy:
    case BattleComboTriggerHook::ProjectileHitAllyOrSource:
    case BattleComboTriggerHook::DamageDealt:
        return trigger == Trigger::OnHit;
    case BattleComboTriggerHook::DamageTaken:
        return trigger == Trigger::OnBeingHit;
    case BattleComboTriggerHook::ShieldBreak:
        return trigger == Trigger::OnShieldBreak;
    default:
        return false;
    }
}

bool isActiveFrameTrigger(const RoleComboState& state, const BattleComboFrameUnit& unit, const AppliedEffectInstance& effect)
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
        auto timer = state.triggerTimers.find(effect.trigger);
        return timer != state.triggerTimers.end() && timer->second > 0;
    }
    return false;
}

}  // namespace

bool BattleComboTriggerSystem::canActivate(const RoleComboState& state, size_t effectIndex) const
{
    assert(effectIndex < state.triggeredEffects.size());

    const auto& effect = state.triggeredEffects[effectIndex];
    auto count = state.effectActivationCounts.find(static_cast<int>(effectIndex));
    int activated = count == state.effectActivationCounts.end() ? 0 : count->second;
    return effect.maxCount <= 0 || activated < effect.maxCount;
}

void BattleComboTriggerSystem::recordActivation(RoleComboState& state, size_t effectIndex) const
{
    assert(effectIndex < state.triggeredEffects.size());
    state.effectActivationCounts[static_cast<int>(effectIndex)]++;
}

std::vector<BattleComboTriggerAction> BattleComboTriggerSystem::updateFrameTriggers(
    RoleComboState& state,
    const BattleComboFrameUnit& unit) const
{
    assert(unit.maxHp > 0);

    std::vector<BattleComboTriggerAction> actions;
    for (size_t i = 0; i < state.triggeredEffects.size(); ++i)
    {
        const auto& effect = state.triggeredEffects[i];
        if (!canActivate(state, i))
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
            if (unit.hp * 100 < unit.maxHp * effect.triggerValue
                && state.triggerTimers[effect.trigger] <= 0)
            {
                recordActivation(state, i);
                actions.push_back({
                    BattleComboTriggerActionType::BroadcastTriggerTimer,
                    effect.trigger,
                    static_cast<int>(i),
                    effect.value,
                    effect.duration,
                });
            }
            continue;
        }

        if (active && effect.type == EffectType::HealBurst)
        {
            recordActivation(state, i);
            actions.push_back({
                BattleComboTriggerActionType::HealPercentSelf,
                effect.trigger,
                static_cast<int>(i),
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
    assert(state.rampingStacks.size() == state.rampingIdleTimers.size());
    assert(state.rampings.empty() || state.rampings.size() == state.rampingStacks.size());

    std::vector<BattleComboFrameRuntimeEvent> events;
    state.lastAliveFlag = input.lastAlive;

    for (int effectIndex = 0; effectIndex < static_cast<int>(state.appliedEffects.size()); ++effectIndex)
    {
        const auto& effect = state.appliedEffects[effectIndex];
        if (!input.alive || effect.trigger != Trigger::Always)
        {
            continue;
        }
        if (effect.type == EffectType::AutoUltimateAfterFrames && effect.value > 0)
        {
            int& timer = state.effectFrameTimers[effectIndex];
            if (timer <= 0)
            {
                timer = effect.value;
            }
            --timer;
            if (timer <= 0)
            {
                events.push_back({ BattleComboFrameRuntimeEventType::AutoUltimateReady });
                timer = effect.value;
            }
        }
        else if (effect.type == EffectType::HPRegenPct && effect.value > 0 && effect.value2 > 0 && input.frame % effect.value2 == 0)
        {
            events.push_back({
                BattleComboFrameRuntimeEventType::SelfHpRegen,
                effect.trigger,
                effectIndex,
                effect.value,
            });
        }
    }

    int healAuraFlat = 0;
    int healAuraPct = 0;
    int healAuraInterval = 0;
    int healedBoostPct = 0;
    for (const auto& effect : state.appliedEffects)
    {
        if (effect.trigger != Trigger::Always)
        {
            continue;
        }
        if (effect.type == EffectType::HealAuraFlat)
        {
            healAuraFlat = std::max(healAuraFlat, effect.value);
            healAuraInterval = effect.value2 > 0 ? effect.value2 : healAuraInterval;
        }
        else if (effect.type == EffectType::HealAuraPct)
        {
            healAuraPct = std::max(healAuraPct, effect.value);
            healAuraInterval = effect.value2 > 0 ? effect.value2 : healAuraInterval;
        }
        else if (effect.type == EffectType::HealedATKSPDBoost)
        {
            healedBoostPct += effect.value;
        }
    }
    if (input.alive && (healAuraPct > 0 || healAuraFlat > 0) && healAuraInterval > 0 && input.frame % healAuraInterval == 0)
    {
        events.push_back({
            BattleComboFrameRuntimeEventType::HealAura,
            Trigger::Always,
            -1,
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
                    action.effectIndex,
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
                    action.effectIndex,
                    action.value,
                    0,
                    action.durationFrames,
                });
            }
        }
    }

    for (size_t i = 0; i < state.rampingStacks.size(); ++i)
    {
        if (state.rampingIdleTimers[i] > 0)
        {
            --state.rampingIdleTimers[i];
        }
        else
        {
            state.rampingStacks[i] = 0;
        }
    }

    for (auto& [trigger, timer] : state.triggerTimers)
    {
        if (timer > 0)
        {
            --timer;
        }
    }

    return events;
}

BattleTriggeredTeamHeal BattleComboTriggerSystem::collectTeamHeal(
    RoleComboState& state,
    Trigger trigger,
    BattleRuntimeRandom& random) const
{
    BattleTriggeredTeamHeal result;
    for (size_t i = 0; i < state.triggeredEffects.size(); ++i)
    {
        const auto& effect = state.triggeredEffects[i];
        if (effect.trigger != trigger || !canActivate(state, i))
        {
            continue;
        }
        if (trigger == Trigger::OnHit && effect.triggerValue > 0 && !passesChance(random, effect.triggerValue))
        {
            continue;
        }

        bool activated = false;
        if (effect.type == EffectType::OnSkillTeamHeal)
        {
            result.flatHeal += effect.value;
            activated = true;
        }
        else if (effect.type == EffectType::OnSkillTeamHealPct)
        {
            result.pctHeal += effect.value;
            activated = true;
        }

        if (activated)
        {
            recordActivation(state, i);
            result.activatedEffectIndices.push_back(static_cast<int>(i));
        }
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
        result.activatedEffectIndices.push_back(event.effectIndex);
    }
    return result;
}

BattleTriggeredTeamHeal BattleComboTriggerSystem::collectPendingSkillTeamHeal(
    RoleComboState& state,
    const BattleComboTriggerInput& input,
    BattleRuntimeRandom& random) const
{
    BattleTriggeredTeamHeal result;
    if (!state.onSkillTeamHealPending)
    {
        return result;
    }

    result.flatHeal = sumAlwaysEffectValue(state, EffectType::OnSkillTeamHeal);
    result.pctHeal = sumAlwaysEffectValue(state, EffectType::OnSkillTeamHealPct);
    auto triggered = collectTriggeredTeamHeal(state, input, random);
    result.flatHeal += triggered.flatHeal;
    result.pctHeal += triggered.pctHeal;
    result.activatedEffectIndices = std::move(triggered.activatedEffectIndices);
    state.onSkillTeamHealPending = false;
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
    for (size_t i = 0; i < state.triggeredEffects.size(); ++i)
    {
        const auto& effect = state.triggeredEffects[i];
        if (effect.trigger != trigger || !canActivate(state, i))
        {
            continue;
        }
        if (std::find(effectTypes.begin(), effectTypes.end(), effect.type) == effectTypes.end())
        {
            continue;
        }
        if (effect.triggerValue > 0 && !passesChance(random, effect.triggerValue))
        {
            continue;
        }

        if (recording == BattleComboActivationRecording::RecordOnCollect)
        {
            recordActivation(state, i);
        }
        result.push_back({ static_cast<int>(i), effect });
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
    for (size_t i = 0; i < state.triggeredEffects.size(); ++i)
    {
        const auto& effect = state.triggeredEffects[i];
        if (!hookMatchesConfiguredTrigger(input.hook, effect.trigger) || !canActivate(state, i))
        {
            continue;
        }
        if (std::find(effectTypes.begin(), effectTypes.end(), effect.type) == effectTypes.end())
        {
            continue;
        }
        if (effect.triggerValue > 0 && !passesChance(random, effect.triggerValue))
        {
            continue;
        }

        if (recording == BattleComboActivationRecording::RecordOnCollect)
        {
            recordActivation(state, i);
        }
        result.push_back({
            input.hook,
            input.sourceUnitId,
            input.targetUnitId,
            static_cast<int>(i),
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
    for (size_t i = 0; i < state.triggeredEffects.size(); ++i)
    {
        const auto& effect = state.triggeredEffects[i];
        if (!hookMatchesConfiguredTrigger(input.hook, effect.trigger))
        {
            continue;
        }
        if (std::find(effectTypes.begin(), effectTypes.end(), effect.type) == effectTypes.end())
        {
            continue;
        }

        result.push_back({
            input.hook,
            input.sourceUnitId,
            input.targetUnitId,
            static_cast<int>(i),
            effect,
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
    for (size_t i = 0; i < state.triggeredEffects.size(); ++i)
    {
        const auto& effect = state.triggeredEffects[i];
        if (!isActiveFrameTrigger(state, unit, effect))
        {
            continue;
        }
        if (std::find(effectTypes.begin(), effectTypes.end(), effect.type) == effectTypes.end())
        {
            continue;
        }

        result.push_back({
            BattleComboTriggerHook::FrameTick,
            -1,
            -1,
            static_cast<int>(i),
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

    int dodgeChancePct = sumAlwaysEffectValue(state, EffectType::DodgeChance);
    for (size_t i = 0; i < state.dodgeAdaptations.size(); ++i)
    {
        assert(i < state.dodgeAdaptationStacks.size());
        const auto& evade = state.dodgeAdaptations[i];
        auto stackIt = state.dodgeAdaptationStacks[i].find(attackerUnitId);
        if (stackIt != state.dodgeAdaptationStacks[i].end())
        {
            dodgeChancePct += stackIt->second * evade.pctPerStack;
        }
    }

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
    assert(state.rampingStacks.size() == state.rampingIdleTimers.size());
    assert(state.rampings.empty() || state.rampings.size() == state.rampingStacks.size());

    BattleAttackerHitDamageResult result;
    result.damage = input.damage;

    for (const auto& event : activeFrameTriggerEffects(
             state,
             { input.hp, input.maxHp, input.lastAlive },
             { EffectType::PctATK }))
    {
        result.damage *= (1.0 + event.effect.value / 100.0);
    }

    bool critted = false;
    if (state.dodgedLast && firstAlwaysEffect(state, EffectType::DodgeThenCrit) != nullptr)
    {
        critted = true;
        state.dodgedLast = false;
    }
    const int critChancePct = sumAlwaysEffectValue(state, EffectType::CritChance);
    if (!critted && passesChance(random, critChancePct))
    {
        critted = true;
    }
    if (critted)
    {
        const int critMultiplier = std::max(150, maxAlwaysEffectValue(state, EffectType::CritMultiplier));
        result.damage *= critMultiplier / 100.0;
        result.events.push_back({
            BattleAttackerHitDamageEventType::Crit,
            critMultiplier,
        });
    }

    for (const auto& effect : state.appliedEffects)
    {
        if (effect.type != EffectType::EveryNthDouble || effect.trigger != Trigger::Always || effect.value <= 0)
        {
            continue;
        }
        const int n = effect.value;
        auto& counter = state.everyNthCounters[n];
        counter++;
        if (counter >= n)
        {
            result.damage *= 2.0;
            counter = 0;
        }
    }

    for (size_t i = 0; i < state.rampings.size(); ++i)
    {
        const auto& ramp = state.rampings[i];
        int beforeStacks = state.rampingStacks[i];
        state.rampingStacks[i] = std::min(state.rampingStacks[i] + 1, ramp.maxStacks);
        state.rampingIdleTimers[i] = 90;
        result.damage *= (1.0 + state.rampingStacks[i] * ramp.pctPerStack / 100.0);
        if (state.rampingStacks[i] > beforeStacks)
        {
            result.events.push_back({
                BattleAttackerHitDamageEventType::RampingStack,
                ramp.pctPerStack,
                state.rampingStacks[i],
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
    assert(state.adaptationStacks.size() == state.adaptations.size());
    assert(state.dodgeAdaptationStacks.size() == state.dodgeAdaptations.size());

    BattleDefenderHitDamageResult result;
    result.damage = input.damage;

    for (const auto& event : activeFrameTriggerEffects(
             state,
             { input.hp, input.maxHp, input.lastAlive },
             { EffectType::DmgReductionPct }))
    {
        result.damage *= (1.0 - event.effect.value / 100.0);
    }

    for (size_t i = 0; i < state.adaptations.size(); ++i)
    {
        const auto& adapt = state.adaptations[i];
        int& stacks = state.adaptationStacks[i][input.attackerUnitId];
        int beforeStacks = stacks;
        stacks = std::min(stacks + 1, adapt.maxStacks);
        result.damage *= (1.0 - stacks * adapt.pctPerStack / 100.0);
        if (stacks > beforeStacks)
        {
            result.events.push_back({
                BattleDefenderHitDamageEventType::DamageAdaptationStack,
                adapt.pctPerStack,
                stacks,
            });
        }
    }

    for (size_t i = 0; i < state.dodgeAdaptations.size(); ++i)
    {
        const auto& evade = state.dodgeAdaptations[i];
        int& stacks = state.dodgeAdaptationStacks[i][input.attackerUnitId];
        int beforeStacks = stacks;
        stacks = std::min(stacks + 1, evade.maxStacks);
        if (stacks > beforeStacks)
        {
            result.events.push_back({
                BattleDefenderHitDamageEventType::DodgeAdaptationStack,
                evade.pctPerStack,
                stacks,
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
            recordActivation(state, static_cast<size_t>(triggerEvent.effectIndex));
            result.executed = true;
            result.thresholdPct = effect.value;
            result.effectIndex = triggerEvent.effectIndex;
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
    return rangedProjectile && passesChance(random, sumAlwaysEffectValue(state, EffectType::ProjectileReflect));
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
    const int armorPenChancePct = sumAlwaysEffectValue(state, EffectType::ArmorPenChance);
    if (passesChance(random, armorPenChancePct))
    {
        result.defense *= (1.0 - maxAlwaysEffectValue(state, EffectType::ArmorPenPct) / 100.0);
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
