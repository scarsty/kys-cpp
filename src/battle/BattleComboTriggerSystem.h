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
    BattleComboTriggerActionType type{};
    Trigger trigger{};
    int effectIndex{};
    int value{};
    int durationFrames{};
};

struct BattleTriggeredTeamHeal
{
    int flatHeal = 0;
    int pctHeal = 0;
    std::vector<int> activatedEffectIndices;
};

struct BattleDodgeResolution
{
    int chancePct{};
    bool dodged{};
};

struct BattleAttackerHitDamageInput
{
    double damage = 0.0;
    int hp = 0;
    int maxHp = 0;
    bool lastAlive = false;
};

enum class BattleAttackerHitDamageEventType
{
    Crit,
    RampingStack,
};

struct BattleAttackerHitDamageEvent
{
    BattleAttackerHitDamageEventType type{};
    int value{};
    int value2{};
};

struct BattleAttackerHitDamageResult
{
    double damage = 0.0;
    std::vector<BattleAttackerHitDamageEvent> events;
};

struct BattleDefenderHitDamageInput
{
    double damage = 0.0;
    int hp = 0;
    int maxHp = 0;
    bool lastAlive = false;
    int attackerUnitId = -1;
};

enum class BattleDefenderHitDamageEventType
{
    DamageAdaptationStack,
    DodgeAdaptationStack,
};

struct BattleDefenderHitDamageEvent
{
    BattleDefenderHitDamageEventType type{};
    int value{};
    int value2{};
};

struct BattleDefenderHitDamageResult
{
    double damage = 0.0;
    std::vector<BattleDefenderHitDamageEvent> events;
};

struct BattleExecuteComboInput
{
    int attackerUnitId = -1;
    int targetUnitId = -1;
    int projectedHpBeforeDamage = 0;
    int maxHp = 0;
    double pendingDamage = 0.0;
    bool appliesHpDamage = true;
};

struct BattleExecuteComboResult
{
    bool executed = false;
    int thresholdPct = 0;
    int effectIndex = -1;
};

struct BattleDefenderBlockInput
{
    bool executed = false;
    bool reflected = false;
};

enum class BattleDefenderBlockCommand
{
    CounterUltimateBlock,
    Block,
};

struct BattleStunCommand
{
    int frames{};
    int effectIndex{};
};

struct BattleProjectileBouncePrimeInput
{
    int attackerUnitId = -1;
    int rollPct = 0;
    int defaultRange = 0;
};

struct BattleProjectileBouncePrime
{
    int count = 0;
    int chancePct = 0;
    int rollPct = 0;
    int range = 0;
};

struct BattleArmorPenetrationInput
{
    int attackerUnitId = -1;
    int targetUnitId = -1;
    double defense = 0.0;
};

struct BattleArmorPenetrationResult
{
    double defense = 0.0;
};

struct BattleBleedProc
{
    bool applies = false;
    int stacks = 0;
    int maxStacks = 0;
};

struct BattleDamageReduceDebuffProc
{
    bool applies = false;
    int pct = 0;
    int durationFrames = 0;
};

enum class BattleOnHitComboCommandType
{
    MpBlock,
    CurrentHpPctBlast,
    TeamMpRestore,
    FlatShield,
    SpiralBleedProjectile,
    NearbyTrackingProjectiles,
};

enum class BattleShieldBreakCommandType
{
    ShieldExplosion,
    AutoUltimate,
    TempFlatAttack,
    MpRestore,
};

struct BattleOnHitComboCommand
{
    BattleOnHitComboCommandType type{};
    int value{};
    int value2{};
};

struct BattleShieldBreakCommand
{
    BattleShieldBreakCommandType type{};
    int value{};
    int durationFrames{};
    int effectIndex{};
};

struct BattleActivatedComboEffect
{
    int effectIndex{};
    AppliedEffectInstance effect;
};

struct BattleComboTriggerEvent
{
    BattleComboTriggerHook hook{};
    int sourceUnitId{};
    int targetUnitId{};
    int effectIndex{};
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

    BattleTriggeredTeamHeal collectTriggeredTeamHeal(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
        const std::function<double()>& rollPercent) const;

    BattleTriggeredTeamHeal collectPendingSkillTeamHeal(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
        const std::function<double()>& rollPercent) const;

    std::vector<BattleOnHitComboCommand> collectOnHitComboCommands(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
        bool suppressNearbyTrackingProjectiles,
        const std::function<double()>& rollPercent) const;

    std::vector<BattleShieldBreakCommand> collectShieldBreakCommands(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
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

    BattleAttackerHitDamageResult shapeAttackerHitDamage(
        RoleComboState& state,
        const BattleAttackerHitDamageInput& input,
        const std::function<double()>& rollPercent) const;

    BattleDefenderHitDamageResult shapeDefenderHitDamage(
        RoleComboState& state,
        const BattleDefenderHitDamageInput& input) const;

    BattleExecuteComboResult resolveExecuteCombo(
        RoleComboState& state,
        const BattleExecuteComboInput& input,
        const std::function<double()>& rollPercent) const;

    bool resolveProjectileReflect(const RoleComboState& state,
                                  bool rangedProjectile,
                                  const std::function<double()>& rollPercent) const;

    std::vector<BattleDefenderBlockCommand> collectDefenderBlockCommands(
        const RoleComboState& state,
        const BattleDefenderBlockInput& input,
        const std::function<double()>& rollPercent) const;

    std::vector<BattleStunCommand> collectStunCommands(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
        const std::function<double()>& rollPercent) const;

    BattleProjectileBouncePrime collectProjectileBouncePrime(
        const RoleComboState& state,
        const BattleProjectileBouncePrimeInput& input) const;

    int collectExtraProjectileCount(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
        int baseCount,
        const std::function<double()>& rollPercent) const;

    bool hasExecuteCombo(const RoleComboState& state, int attackerUnitId) const;

    BattleArmorPenetrationResult resolveArmorPenetratedDefense(
        const RoleComboState& state,
        const BattleArmorPenetrationInput& input,
        const std::function<double()>& rollPercent) const;

    int resolveLegacyStunFrames(const RoleComboState& state, const std::function<double()>& rollPercent) const;
    bool shouldApplyKnockback(const RoleComboState& state, const std::function<double()>& rollPercent) const;
    int resolveOffensiveCooldownExtendPct(const RoleComboState& state, const std::function<double()>& rollPercent) const;
    int resolveDefensiveCooldownExtendPct(const RoleComboState& state, const std::function<double()>& rollPercent) const;
    BattleBleedProc resolveBleedProc(
        const RoleComboState& state,
        bool damagePositive,
        const std::function<double()>& rollPercent) const;
    BattleDamageReduceDebuffProc resolveDamageReduceDebuffProc(
        const RoleComboState& state,
        bool damagePositive,
        const std::function<double()>& rollPercent) const;

    void recordActivation(RoleComboState& state, size_t effectIndex) const;

private:
    bool canActivate(const RoleComboState& state, size_t effectIndex) const;
};

}  // namespace KysChess::Battle
