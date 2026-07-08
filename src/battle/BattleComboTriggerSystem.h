#pragma once

#include "ChessBattleEffects.h"

#include <array>
#include <initializer_list>
#include <optional>
#include <vector>

namespace KysChess::Battle
{

class BattleRuntimeRandom;

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
};

struct BattleComboFrameRuntimeEvent
{
    BattleComboFrameRuntimeEventType type = BattleComboFrameRuntimeEventType::SelfHpRegen;
    Trigger trigger = Trigger::Always;
    ComboTriggerTimerKey timerKey;
    RoleComboEffectId effectId;
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
    bool ultimate = false;
    bool mainProjectile = true;
};

enum class BattleEffectSourceKind
{
    Combo,
    Skill,
};

enum class BattleSkillSlot
{
    None,
    Normal,
    Ultimate,
};

struct BattleEffectStoreRef
{
    BattleEffectSourceKind kind = BattleEffectSourceKind::Combo;
    BattleSkillSlot slot = BattleSkillSlot::None;

    auto operator<=>(const BattleEffectStoreRef&) const = default;
};

struct BattleEffectRef
{
    BattleEffectStoreRef store;
    RoleComboEffectId localId;
};

struct BattleEffectSource
{
    BattleEffectStoreRef ref;
    BattleEffectState* state = nullptr;
};

struct BattleEffectSources
{
    BattleEffectSource combo;
    BattleEffectSource skill;
};

std::array<BattleEffectSource, 2> orderedBattleEffectSources(const BattleEffectSources& sources);
BattleEffectSource battleEffectSourceForStore(
    const BattleEffectSources& sources,
    BattleEffectStoreRef store);

struct BattleSkillEffectRef
{
    int unitId = -1;
    BattleSkillSlot slot = BattleSkillSlot::None;
};

struct BattleComboTriggerAction
{
    BattleComboTriggerActionType type{};
    Trigger trigger{};
    ComboTriggerTimerKey timerKey;
    RoleComboEffectId effectId;
    int value{};
    int durationFrames{};
};

struct BattleTriggeredTeamHeal
{
    int flatHeal = 0;
    int pctHeal = 0;
    std::vector<RoleComboEffectId> activatedEffectIds;
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
    RoleComboEffectId effectId;
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
    bool ultimate = false;
    bool mainProjectile = true;
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

struct BattleActivatedComboEffect
{
    RoleComboEffectId effectId;
    ComboEffectSnapshot effect;
};

struct BattleComboTriggerEvent
{
    BattleComboTriggerHook hook{};
    int sourceUnitId{};
    int targetUnitId{};
    RoleComboEffectId effectId;
    ComboEffectSnapshot effect;
};

struct BattleEffectTriggerEvent
{
    BattleComboTriggerHook hook{};
    int sourceUnitId{};
    int targetUnitId{};
    BattleEffectRef effectRef;
    ComboEffectSnapshot effect;
};

class BattleEffectReader
{
public:
    int sumAlways(const BattleEffectSources& sources, EffectType type) const;
    int maxAlways(const BattleEffectSources& sources, EffectType type) const;
    int maxAlwaysValue2(const BattleEffectSources& sources, EffectType type) const;
    bool hasAlways(const BattleEffectSources& sources, EffectType type) const;
    const RoleComboEffectInstance* firstAlways(const BattleEffectSources& sources, EffectType type) const;
    BattleEffectRef firstAlwaysRef(const BattleEffectSources& sources, EffectType type) const;
    std::optional<ComboEffectSnapshot> maxAlwaysPair(const BattleEffectSources& sources, EffectType type) const;

    std::vector<BattleEffectTriggerEvent> collectTriggerEvents(
        const BattleEffectSources& sources,
        const BattleComboTriggerInput& input,
        std::initializer_list<EffectType> effectTypes,
        BattleRuntimeRandom& random,
        BattleComboActivationRecording recording = BattleComboActivationRecording::RecordOnCollect) const;

    std::vector<BattleEffectTriggerEvent> matchingTriggerEvents(
        const BattleEffectSources& sources,
        const BattleComboTriggerInput& input,
        std::initializer_list<EffectType> effectTypes) const;
};

class BattleEffectCommands
{
public:
    void recordActivation(const BattleEffectSources& sources, BattleEffectRef effectRef) const;
    bool consumeTypePending(const BattleEffectSources& sources, EffectType type) const;
    bool advanceEffectCounter(const BattleEffectSources& sources, BattleEffectRef effectRef, int threshold) const;
};

class BattleComboTriggerSystem
{
public:
    std::vector<BattleComboTriggerAction> updateFrameTriggers(RoleComboState& state,
                                                             const BattleComboFrameUnit& unit) const;

    std::vector<BattleComboFrameRuntimeEvent> advanceFrameRuntime(
        RoleComboState& state,
        const BattleComboFrameRuntimeInput& input) const;

    BattleTriggeredTeamHeal collectTeamHeal(RoleComboState& state,
                                            Trigger trigger,
                                            BattleRuntimeRandom& random) const;

    BattleTriggeredTeamHeal collectTriggeredTeamHeal(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
        BattleRuntimeRandom& random) const;

    BattleTriggeredTeamHeal collectPendingSkillTeamHeal(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
        BattleRuntimeRandom& random) const;

    BattleTriggeredTeamHeal collectPendingSkillTeamHeal(
        const BattleEffectSources& sources,
        const BattleComboTriggerInput& input,
        BattleRuntimeRandom& random) const;

    std::vector<BattleActivatedComboEffect> collectChanceEffects(
        RoleComboState& state,
        Trigger trigger,
        std::initializer_list<EffectType> effectTypes,
        BattleRuntimeRandom& random,
        BattleComboActivationRecording recording = BattleComboActivationRecording::RecordOnCollect) const;

    std::vector<BattleComboTriggerEvent> collectTriggerEvents(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
        std::initializer_list<EffectType> effectTypes,
        BattleRuntimeRandom& random,
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
        BattleRuntimeRandom& random) const;

    BattleDefenderHitDamageResult shapeDefenderHitDamage(
        RoleComboState& state,
        const BattleDefenderHitDamageInput& input) const;

    BattleExecuteComboResult resolveExecuteCombo(
        const BattleEffectSources& sources,
        const BattleExecuteComboInput& input,
        BattleRuntimeRandom& random) const;

    BattleExecuteComboResult resolveExecuteCombo(
        RoleComboState& state,
        const BattleExecuteComboInput& input,
        BattleRuntimeRandom& random) const;

    bool resolveProjectileReflect(const RoleComboState& state,
                                  bool rangedProjectile,
                                  BattleRuntimeRandom& random) const;

    BattleProjectileBouncePrime collectProjectileBouncePrime(
        const BattleEffectSources& sources,
        const BattleProjectileBouncePrimeInput& input) const;

    BattleProjectileBouncePrime collectProjectileBouncePrime(
        const RoleComboState& state,
        const BattleProjectileBouncePrimeInput& input) const;

    int collectExtraProjectileCount(
        const BattleEffectSources& sources,
        const BattleComboTriggerInput& input,
        int baseCount,
        BattleRuntimeRandom& random) const;

    int collectExtraProjectileCount(
        RoleComboState& state,
        const BattleComboTriggerInput& input,
        int baseCount,
        BattleRuntimeRandom& random) const;

    bool hasExecuteCombo(const RoleComboState& state, int attackerUnitId) const;

    BattleArmorPenetrationResult resolveArmorPenetratedDefense(
        const BattleEffectSources& sources,
        const BattleArmorPenetrationInput& input,
        BattleRuntimeRandom& random) const;

    BattleArmorPenetrationResult resolveArmorPenetratedDefense(
        const RoleComboState& state,
        const BattleArmorPenetrationInput& input,
        BattleRuntimeRandom& random) const;

    void recordActivation(RoleComboState& state, RoleComboEffectId effectId) const;

private:
    bool canActivate(const RoleComboState& state, RoleComboEffectId effectId) const;
};

}  // namespace KysChess::Battle
