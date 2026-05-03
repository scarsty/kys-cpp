#include "BattleComboTriggerSystem.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace KysChess::Battle
{
namespace
{

bool hookMatchesLegacyTrigger(BattleComboTriggerHook hook, Trigger trigger)
{
    switch (hook)
    {
    case BattleComboTriggerHook::FrameTick:
        return trigger == Trigger::Always
            || trigger == Trigger::WhileLowHP
            || trigger == Trigger::AllyLowHPBurst
            || trigger == Trigger::LastAlive;
    case BattleComboTriggerHook::AfterSkillCast:
        return trigger == Trigger::OnUltimate;
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

    if (input.alive && state.autoUltimateAfterFrames > 0)
    {
        if (state.autoUltimateTimer > 0)
        {
            --state.autoUltimateTimer;
        }
        if (state.autoUltimateTimer <= 0)
        {
            events.push_back({ BattleComboFrameRuntimeEventType::AutoUltimateReady });
            state.autoUltimateTimer = state.autoUltimateAfterFrames;
        }
    }

    if (input.alive
        && state.hpRegenPct > 0
        && state.hpRegenInterval > 0
        && input.frame % state.hpRegenInterval == 0)
    {
        events.push_back({
            BattleComboFrameRuntimeEventType::SelfHpRegen,
            Trigger::Always,
            -1,
            state.hpRegenPct,
        });
    }

    if (input.alive
        && (state.healAuraPct > 0 || state.healAuraFlat > 0)
        && state.healAuraInterval > 0
        && input.frame % state.healAuraInterval == 0)
    {
        events.push_back({
            BattleComboFrameRuntimeEventType::HealAura,
            Trigger::Always,
            -1,
            state.healAuraFlat,
            state.healAuraPct,
            state.healedATKSPDBoostPct,
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

std::vector<BattleComboFrameRuntimeEvent> BattleComboTriggerSystem::collectSkillFinishedRuntimeEvents(
    const RoleComboState& state,
    bool alive) const
{
    std::vector<BattleComboFrameRuntimeEvent> events;
    if (alive && state.postSkillInvincFrames > 0)
    {
        events.push_back({
            BattleComboFrameRuntimeEventType::PostSkillInvincibility,
            Trigger::OnUltimate,
            -1,
            state.postSkillInvincFrames,
        });
    }
    return events;
}

BattleTriggeredTeamHeal BattleComboTriggerSystem::collectTeamHeal(
    RoleComboState& state,
    Trigger trigger,
    const std::function<double()>& rollPercent) const
{
    assert(static_cast<bool>(rollPercent));

    BattleTriggeredTeamHeal result;
    for (size_t i = 0; i < state.triggeredEffects.size(); ++i)
    {
        const auto& effect = state.triggeredEffects[i];
        if (effect.trigger != trigger || !canActivate(state, i))
        {
            continue;
        }
        if (trigger == Trigger::OnHit && effect.triggerValue > 0 && rollPercent() >= effect.triggerValue)
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
    const std::function<double()>& rollPercent) const
{
    auto events = collectTriggerEvents(
        state,
        input,
        { EffectType::OnSkillTeamHeal, EffectType::OnSkillTeamHealPct },
        rollPercent);

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
    const std::function<double()>& rollPercent) const
{
    BattleTriggeredTeamHeal result;
    if (!state.onSkillTeamHealPending)
    {
        return result;
    }

    result.flatHeal = state.onSkillTeamHeal;
    result.pctHeal = state.onSkillTeamHealPct;
    auto triggered = collectTriggeredTeamHeal(state, input, rollPercent);
    result.flatHeal += triggered.flatHeal;
    result.pctHeal += triggered.pctHeal;
    result.activatedEffectIndices = std::move(triggered.activatedEffectIndices);
    state.onSkillTeamHealPending = false;
    return result;
}

std::vector<BattleOnHitComboCommand> BattleComboTriggerSystem::collectOnHitComboCommands(
    RoleComboState& state,
    const BattleComboTriggerInput& input,
    bool suppressNearbyTrackingProjectiles,
    const std::function<double()>& rollPercent) const
{
    auto events = suppressNearbyTrackingProjectiles
        ? collectTriggerEvents(
            state,
            input,
            {
                EffectType::MPBlock,
                EffectType::CurrentHPPctBlast,
                EffectType::TeamMPRestore,
                EffectType::FlatShield,
                EffectType::SpiralBleedProjectile,
            },
            rollPercent)
        : collectTriggerEvents(
            state,
            input,
            {
                EffectType::MPBlock,
                EffectType::CurrentHPPctBlast,
                EffectType::TeamMPRestore,
                EffectType::FlatShield,
                EffectType::SpiralBleedProjectile,
                EffectType::NearbyTrackingProjectiles,
            },
            rollPercent);

    std::vector<BattleOnHitComboCommand> commands;
    for (const auto& event : events)
    {
        assert(event.effect.triggerValue > 0);
        assert(event.effect.value > 0);

        BattleOnHitComboCommand command;
        command.value = event.effect.value;
        command.value2 = event.effect.value2;
        switch (event.effect.type)
        {
        case EffectType::MPBlock:
            command.type = BattleOnHitComboCommandType::MpBlock;
            break;
        case EffectType::CurrentHPPctBlast:
            command.type = BattleOnHitComboCommandType::CurrentHpPctBlast;
            break;
        case EffectType::TeamMPRestore:
            command.type = BattleOnHitComboCommandType::TeamMpRestore;
            break;
        case EffectType::FlatShield:
            command.type = BattleOnHitComboCommandType::FlatShield;
            break;
        case EffectType::SpiralBleedProjectile:
            command.type = BattleOnHitComboCommandType::SpiralBleedProjectile;
            break;
        case EffectType::NearbyTrackingProjectiles:
            command.type = BattleOnHitComboCommandType::NearbyTrackingProjectiles;
            break;
        default:
            assert(false);
        }
        commands.push_back(command);
    }
    return commands;
}

std::vector<BattleShieldBreakCommand> BattleComboTriggerSystem::collectShieldBreakCommands(
    RoleComboState& state,
    const BattleComboTriggerInput& input,
    const std::function<double()>& rollPercent) const
{
    auto events = collectTriggerEvents(
        state,
        input,
        {
            EffectType::ShieldExplosion,
            EffectType::AutoUltimate,
            EffectType::TempFlatATK,
            EffectType::MPRestore,
        },
        rollPercent,
        BattleComboActivationRecording::CallerRecords);

    std::vector<BattleShieldBreakCommand> commands;
    for (const auto& event : events)
    {
        BattleShieldBreakCommand command;
        command.value = event.effect.value;
        command.durationFrames = event.effect.duration;
        command.effectIndex = event.effectIndex;
        switch (event.effect.type)
        {
        case EffectType::ShieldExplosion:
            assert(event.effect.value > 0);
            command.type = BattleShieldBreakCommandType::ShieldExplosion;
            break;
        case EffectType::AutoUltimate:
            command.type = BattleShieldBreakCommandType::AutoUltimate;
            break;
        case EffectType::TempFlatATK:
            assert(event.effect.value != 0);
            assert(event.effect.duration > 0);
            command.type = BattleShieldBreakCommandType::TempFlatAttack;
            break;
        case EffectType::MPRestore:
            assert(event.effect.value > 0);
            command.type = BattleShieldBreakCommandType::MpRestore;
            break;
        default:
            assert(false);
        }
        commands.push_back(command);
    }
    return commands;
}

std::vector<BattleActivatedComboEffect> BattleComboTriggerSystem::collectChanceEffects(
    RoleComboState& state,
    Trigger trigger,
    std::initializer_list<EffectType> effectTypes,
    const std::function<double()>& rollPercent,
    BattleComboActivationRecording recording) const
{
    assert(effectTypes.size() > 0);
    assert(static_cast<bool>(rollPercent));

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
        if (effect.triggerValue > 0 && rollPercent() >= effect.triggerValue)
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
    const std::function<double()>& rollPercent,
    BattleComboActivationRecording recording) const
{
    assert(effectTypes.size() > 0);
    assert(static_cast<bool>(rollPercent));

    std::vector<BattleComboTriggerEvent> result;
    for (size_t i = 0; i < state.triggeredEffects.size(); ++i)
    {
        const auto& effect = state.triggeredEffects[i];
        if (!hookMatchesLegacyTrigger(input.hook, effect.trigger) || !canActivate(state, i))
        {
            continue;
        }
        if (std::find(effectTypes.begin(), effectTypes.end(), effect.type) == effectTypes.end())
        {
            continue;
        }
        if (effect.triggerValue > 0 && rollPercent() >= effect.triggerValue)
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
        if (!hookMatchesLegacyTrigger(input.hook, effect.trigger))
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

    int dodgeChancePct = state.dodgeChancePct;
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
    const std::function<double()>& rollPercent) const
{
    assert(input.maxHp > 0);
    assert(static_cast<bool>(rollPercent));
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
    if (state.dodgedLast && state.dodgeThenCrit)
    {
        critted = true;
        state.dodgedLast = false;
    }
    if (!critted && state.critChancePct > 0 && rollPercent() < state.critChancePct)
    {
        critted = true;
    }
    if (critted)
    {
        result.damage *= state.critMultiplier / 100.0;
        result.events.push_back({
            BattleAttackerHitDamageEventType::Crit,
            state.critMultiplier,
        });
    }

    for (int n : state.everyNthDoubles)
    {
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

}  // namespace KysChess::Battle
