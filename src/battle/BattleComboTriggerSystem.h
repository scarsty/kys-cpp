#pragma once

#include "ChessBattleEffects.h"

#include <functional>
#include <initializer_list>
#include <vector>

namespace KysChess::Battle
{

struct BattleComboFrameUnit
{
    int hp = 0;
    int maxHp = 0;
    bool lastAlive = false;
};

enum class BattleComboTriggerActionType
{
    HealPercentSelf,
    BroadcastTriggerTimer,
};

enum class BattleComboActivationRecording
{
    RecordOnCollect,
    CallerRecords,
};

struct BattleComboTriggerAction
{
    BattleComboTriggerActionType type = BattleComboTriggerActionType::HealPercentSelf;
    Trigger trigger = Trigger::Always;
    int effectIndex = -1;
    int value = 0;
    int durationFrames = 0;
};

struct BattleTriggeredTeamHeal
{
    int flatHeal = 0;
    int pctHeal = 0;
    std::vector<int> activatedEffectIndices;
};

struct BattleActivatedComboEffect
{
    int effectIndex = -1;
    AppliedEffectInstance effect;
};

class BattleComboTriggerSystem
{
public:
    std::vector<BattleComboTriggerAction> updateFrameTriggers(RoleComboState& state,
                                                             const BattleComboFrameUnit& unit) const;

    BattleTriggeredTeamHeal collectTeamHeal(RoleComboState& state,
                                            Trigger trigger,
                                            const std::function<double()>& rollPercent) const;

    std::vector<BattleActivatedComboEffect> collectChanceEffects(
        RoleComboState& state,
        Trigger trigger,
        std::initializer_list<EffectType> effectTypes,
        const std::function<double()>& rollPercent,
        BattleComboActivationRecording recording = BattleComboActivationRecording::RecordOnCollect) const;

    void recordActivation(RoleComboState& state, size_t effectIndex) const;

private:
    bool canActivate(const RoleComboState& state, size_t effectIndex) const;
};

}  // namespace KysChess::Battle
