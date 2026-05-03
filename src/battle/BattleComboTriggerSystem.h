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

struct BattleComboFrameRuntimeInput
{
    int frame = 0;
    int hp = 0;
    int maxHp = 0;
    bool alive = false;
    bool lastAlive = false;
};

enum class BattleComboTriggerActionType
{
    HealPercentSelf,
    BroadcastTriggerTimer,
};

enum class BattleComboFrameRuntimeEventType
{
    AutoUltimateReady,
    SelfHpRegen,
    HealAura,
    HealPercentSelf,
    BroadcastTriggerTimer,
    PostSkillInvincibility,
};

struct BattleComboFrameRuntimeEvent
{
    BattleComboFrameRuntimeEventType type = BattleComboFrameRuntimeEventType::SelfHpRegen;
    Trigger trigger = Trigger::Always;
    int effectIndex = -1;
    int value = 0;
    int value2 = 0;
    int durationFrames = 0;
};

enum class BattleComboActivationRecording
{
    RecordOnCollect,
    CallerRecords,
};

enum class BattleComboTriggerHook
{
    FrameTick,
    BeforeCast,
    AfterSkillCast,
    AttackLaunched,
    ProjectileHitEnemy,
    ProjectileHitAllyOrSource,
    DamageTaken,
    DamageDealt,
    ShieldBreak,
    UnitDeath,
    AllyDeath,
    BattleStart,
    BattleEnd,
};

struct BattleComboTriggerInput
{
    BattleComboTriggerHook hook = BattleComboTriggerHook::FrameTick;
    int sourceUnitId = -1;
    int targetUnitId = -1;
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

struct BattleDodgeResolution
{
    int chancePct = 0;
    bool dodged = false;
};

struct BattleActivatedComboEffect
{
    int effectIndex = -1;
    AppliedEffectInstance effect;
};

struct BattleComboTriggerEvent
{
    BattleComboTriggerHook hook = BattleComboTriggerHook::FrameTick;
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int effectIndex = -1;
    AppliedEffectInstance effect;
};

class BattleComboTriggerSystem
{
public:
    std::vector<BattleComboTriggerAction> updateFrameTriggers(RoleComboState& state,
                                                             const BattleComboFrameUnit& unit) const;

    std::vector<BattleComboFrameRuntimeEvent> advanceFrameRuntime(
        RoleComboState& state,
        const BattleComboFrameRuntimeInput& input) const;

    std::vector<BattleComboFrameRuntimeEvent> collectSkillFinishedRuntimeEvents(
        const RoleComboState& state,
        bool alive) const;

    BattleTriggeredTeamHeal collectTeamHeal(RoleComboState& state,
                                            Trigger trigger,
                                            const std::function<double()>& rollPercent) const;

    std::vector<BattleActivatedComboEffect> collectChanceEffects(
        RoleComboState& state,
        Trigger trigger,
        std::initializer_list<EffectType> effectTypes,
        const std::function<double()>& rollPercent,
        BattleComboActivationRecording recording = BattleComboActivationRecording::RecordOnCollect) const;

    std::vector<BattleComboTriggerEvent> collectTriggerEvents(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
        std::initializer_list<EffectType> effectTypes,
        const std::function<double()>& rollPercent,
        BattleComboActivationRecording recording = BattleComboActivationRecording::RecordOnCollect) const;

    std::vector<BattleComboTriggerEvent> matchingTriggerEffects(
        const RoleComboState& state,
        const BattleComboTriggerInput& input,
        std::initializer_list<EffectType> effectTypes) const;

    std::vector<BattleComboTriggerEvent> activeFrameTriggerEffects(
        const RoleComboState& state,
        const BattleComboFrameUnit& unit,
        std::initializer_list<EffectType> effectTypes) const;

    BattleDodgeResolution resolveDodge(const RoleComboState& state,
                                       int attackerUnitId,
                                       double rollPercent) const;

    void recordActivation(RoleComboState& state, size_t effectIndex) const;

private:
    bool canActivate(const RoleComboState& state, size_t effectIndex) const;
};

}  // namespace KysChess::Battle
