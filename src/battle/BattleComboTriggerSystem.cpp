#include "BattleComboTriggerSystem.h"

#include <algorithm>
#include <cassert>

namespace KysChess::Battle
{

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

}  // namespace KysChess::Battle
