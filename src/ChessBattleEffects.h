#pragma once

#include <compare>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace YAML { class Node; }

namespace KysChess
{

enum class Trigger
{
    Always,
    WhileLowHP,
    AllyLowHPBurst,
    LastAlive,
    OnCast,  // Proc when an attack is released
    OnUltimate,
    OnHit,  // Proc on attack hit
    OnBeingHit,  // Proc when being hit (defender)
    OnShieldBreak,  // Proc when shield breaks
};

enum class EffectType
{
    // Stat buffs (pre-battle)
    FlatHP, FlatATK, FlatDEF, FlatSPD,
    PctHP, PctATK, PctDEF, PctSPD, NegPctDEF,
    TeamFlatHP, TeamFlatATK, TeamFlatDEF, TeamFlatSPD,
    TeamPctHP, TeamPctATK, TeamPctDEF, TeamPctSPD,
    ActAsCombo,
    FightWinHP, FightWinATKDEF,

    // Trigger effects (runtime)
    FlatDmgReduction,
    FlatDmgIncrease,
    BlockChance,
    DodgeChance,
    DodgeThenCrit,
    CritChance,
    CritMultiplier,
    EveryNthDouble,
    ArmorPenChance,  // DEPRECATED: use ArmorPen with OnHit trigger
    ArmorPenPct,     // DEPRECATED: use ArmorPen with OnHit trigger
    ArmorPen,        // Unified: trigger=OnHit, value=chance, value2=pen%
    Stun,            // Unified: trigger=OnHit, triggerValue=chance, value=duration
    KnockbackChance,
    PoisonDOT,
    PoisonDmgAmp,
    MPOnHit,
    HPOnHit,
    MPDrain,
    MPRecoveryBonus,
    SkillDmgPct,
    SkillReflectPct,
    CDR,
    FlatShield,
    ShieldPctMaxHP,
    ShieldFreezeRes,
    HealAuraPct,
    HealAuraFlat,
    HealedATKSPDBoost,
    HPRegenPct,
    FreezeReductionPct,
    ControlImmunityFrames,
    KillHealPct,
    KillInvincFrames,
    PostSkillInvincFrames,
    DmgReductionPct,
    // Comeback & Scaling
    Bloodlust,
    Adaptation,
    DodgeAdaptation,
    RampingDmg,
    // Triggered heal
    HealBurst,
    // === New effects for expanded chess pool ===
    BleedChance,
    PostSkillDash,
    EnemyTopDebuff,
    BlinkAttack,
    AllyDeathStatBoost,
    CloneSummon,
    ProjectileReflect,
    ProjectileBounce,
    OnSkillTeamHeal,
    OnSkillTeamHealPct,
    DeathPrevention,
    DeathMedical,
    ForcePullProtect,
    ForcePullExecute,
    Execute,
    MPBlock,
    CharmCDRDebuff,
    OffensiveCharm,
    DeathAOE,
    ShieldExplosion,
    TempFlatATK,
    AutoUltimate,
    MPRestore,
    ShieldOnAllyDeath,
    DamageImmunityAfterFrames,
    AutoUltimateAfterFrames,
    UltimateExtraProjectiles,
    BlockFirstHits,
    GoldCoefficient,
    HurtInvincFrames,
    DashAttack,
    DashChanceBoost,
    MPRatioDmgBoost,
    DmgReduceDebuff,
    CurrentHPPctBlast,
    TeamMPRestore,
    EnemyMpDamageAll,
    SpiralBleedProjectile,
    NearbyTrackingProjectiles,
    ForceRangedAttack,
    CounterUltimateBlock,
    MaxHitPctCurrentHP,
    FreeRefresh,
    BattleMapChoice,
    LowestAllyHeal,
};

struct ComboEffect
{
    EffectType type;
    int value;
    int value2 = 0;
    std::string text;
    Trigger trigger = Trigger::Always;
    int triggerValue = 0;
    int duration = 0;
    int maxCount = 0;
};

std::string comboEffectDesc(const ComboEffect& eff);
std::string comboEffectCompactDesc(const ComboEffect& eff);

struct RoleComboEffectId
{
    int value = -1;

    bool isValid() const { return value >= 0; }
    auto operator<=>(const RoleComboEffectId&) const = default;
};

inline bool operator==(RoleComboEffectId id, int value)
{
    return id.value == value;
}

inline bool operator==(int value, RoleComboEffectId id)
{
    return id.value == value;
}

enum class RoleComboEffectOrigin
{
    Configured,
    RuntimeGrant,
};

struct ComboEffectSnapshot : ComboEffect
{
    int sourceComboId = -1;  // -1 means the effect did not originate from a synergy
};

struct RoleComboEffectInstance : ComboEffectSnapshot
{
    RoleComboEffectId id;
    RoleComboEffectOrigin origin = RoleComboEffectOrigin::Configured;
};

struct ComboTriggerTimerKey
{
    Trigger trigger = Trigger::Always;
    int sourceComboId = -1;

    auto operator<=>(const ComboTriggerTimerKey&) const = default;
};

struct RoleComboEffectLookupKey
{
    Trigger trigger = Trigger::Always;
    EffectType type = EffectType::FlatHP;

    auto operator<=>(const RoleComboEffectLookupKey&) const = default;
};

struct RoleComboAlwaysSummary
{
    int sumValue = 0;
    int maxValue = 0;
    int maxValue2 = 0;
    RoleComboEffectId firstId;
    RoleComboEffectId maxByValueId;
};

struct RoleComboStatBonuses
{
    int flatHP = 0;
    int flatATK = 0;
    int flatDEF = 0;
    int flatSPD = 0;
    double pctHP = 0;
    double pctATK = 0;
    double pctDEF = 0;
    double pctSPD = 0;
    int fightWinGrowthHP = 0;
    int fightWinGrowthATK = 0;
    int fightWinGrowthDEF = 0;
};

struct RoleComboAdaptationDescriptor
{
    int pctPerStack = 0;
    int maxStacks = 0;
};

using RoleComboDodgeAdaptationDescriptor = RoleComboAdaptationDescriptor;

struct RoleComboRampingDescriptor
{
    int pctPerStack = 0;
    int maxStacks = 0;
};

struct RoleComboStackChange
{
    int pctPerStack = 0;
    int stacks = 0;
    bool increased = false;
};

struct RoleComboEffectStore
{
    std::vector<RoleComboEffectInstance> instances;
    std::vector<RoleComboEffectId> idsInAppendOrder;
    std::map<RoleComboEffectLookupKey, std::vector<RoleComboEffectId>> idsByTriggerAndType;
    std::map<int, std::vector<RoleComboEffectId>> idsBySourceComboId;
    std::map<EffectType, RoleComboAlwaysSummary> alwaysByType;
    RoleComboStatBonuses statBonuses;
    std::map<RoleComboEffectId, RoleComboAdaptationDescriptor> adaptations;
    std::map<RoleComboEffectId, RoleComboDodgeAdaptationDescriptor> dodgeAdaptations;
    std::map<RoleComboEffectId, RoleComboRampingDescriptor> rampings;
};

struct RoleComboEffectRuntimeState
{
    int activationCount = 0;
    int frameTimer = 0;
    int counter = 0;
    int stacks = 0;
    int idleTimer = 0;
    std::map<int, int> stacksByUnit;
};

struct RoleComboEffectTypeRuntimeState
{
    bool pending = false;
    bool toggle = false;
};

struct RoleComboRuntimeState
{
    int enemyTopDebuffApplied = 0;
    std::map<ComboTriggerTimerKey, int> triggerTimers;
    bool lastAliveFlag = false;
    std::map<RoleComboEffectId, RoleComboEffectRuntimeState> byEffect;
    std::map<EffectType, RoleComboEffectTypeRuntimeState> byType;
};

class BattleEffectState
{
public:
    RoleComboEffectId applyConfiguredEffect(const ComboEffect& effect, int sourceComboId = -1);
    RoleComboEffectId grantRuntimeEffect(const ComboEffect& effect, int sourceComboId = -1);

    const RoleComboEffectInstance& effect(RoleComboEffectId id) const;
    std::span<const RoleComboEffectId> effectIdsInAppendOrder() const;
    std::span<const RoleComboEffectId> effectIds(Trigger trigger, EffectType type) const;
    std::span<const RoleComboEffectId> idsFromCombo(int sourceComboId) const;
    const RoleComboStatBonuses& statBonuses() const;
    bool isRuntimeGranted(RoleComboEffectId id) const;

    bool canActivateTriggeredEffect(RoleComboEffectId id) const;
    void recordTriggeredEffectActivation(RoleComboEffectId id);
    int triggeredEffectActivationCount(RoleComboEffectId id) const;
    bool hasTriggeredEffectActivations() const;
    bool hasActiveTriggerTimer(ComboTriggerTimerKey key) const;
    bool ownsTriggerTimer(ComboTriggerTimerKey key) const;
    void extendTriggerTimer(ComboTriggerTimerKey key, int durationFrames);
    void advanceTriggerTimersOneFrame();

    void setLastAliveForComboRuntime(bool lastAlive);
    bool lastAliveForComboRuntime() const;
    void seedAutoUltimateFrameTimers();
    bool advanceAutoUltimateFrameTimer(RoleComboEffectId id, int intervalFrames);
    int effectFrameTimerFrames(RoleComboEffectId id) const;
    int triggerTimerFrames(ComboTriggerTimerKey key) const;

    void setTypePending(EffectType type, bool value);
    bool typePending(EffectType type) const;
    bool consumeTypePending(EffectType type);
    bool typeToggle(EffectType type) const;
    bool consumeTypeToggle(EffectType type);

    bool advanceEffectCounter(RoleComboEffectId id, int threshold);
    void setEffectFrameTimer(RoleComboEffectId id, int frames);
    bool advanceEffectFrameTimer(RoleComboEffectId id, int intervalFrames);
    RoleComboStackChange recordEffectStack(RoleComboEffectId id, int maxStacks, int pctPerStack);
    RoleComboStackChange recordEffectStackAgainst(RoleComboEffectId id, int unitId, int maxStacks, int pctPerStack);
    void setEffectIdleTimer(RoleComboEffectId id, int frames);
    void advanceEffectIdleTimers(std::span<const RoleComboEffectId> ids);
    int effectStacks(RoleComboEffectId id) const;
    int effectIdleTimer(RoleComboEffectId id) const;
    int effectStacksAgainst(RoleComboEffectId id, int unitId) const;
    int setEnemyTopDebuffApplied(int desired);

    int dodgeAdaptationBonusAgainst(int attackerUnitId) const;

    int sumAlways(EffectType type) const;
    int maxAlways(EffectType type) const;
    int maxAlwaysValue2(EffectType type) const;
    bool hasAlways(EffectType type) const;
    const RoleComboEffectInstance* firstAlways(EffectType type) const;
    const RoleComboEffectInstance* maxAlwaysByValue(EffectType type) const;
    bool hasComboApplied(int comboId) const;

private:
    RoleComboEffectStore effects_;
    RoleComboRuntimeState runtime_;

    RoleComboEffectId appendEffect(const ComboEffect& effect, RoleComboEffectOrigin origin, int sourceComboId);
};

class RoleComboState : public BattleEffectState
{
};

struct ChessMagicEffectDefinition
{
    int magicId = -1;
    std::string name;
    std::vector<ComboEffect> effects;
    std::string purpose;
};

class ChessBattleEffects
{
public:
    static const std::map<std::string, EffectType>& getEffectTypeMap();
    static bool parseEffect(const YAML::Node& eNode, ComboEffect& out, const std::string& context);
    static bool parseMagicEffects(const YAML::Node& root, std::vector<ChessMagicEffectDefinition>& out, const std::string& context);
    static bool loadMagicEffectsFile(const std::string& path, std::vector<ChessMagicEffectDefinition>& out);
    static bool loadDefaultMagicEffectsFile(std::vector<ChessMagicEffectDefinition>& out);
    static RoleComboState makeSummonedCloneState(const RoleComboState& sourceState);
    static void mergeEffects(std::map<int, RoleComboState>& states,
                             const std::vector<ComboEffect>& effects,
                             const std::vector<int>& roleIds,
                             int sourceComboId = -1);
};

}  // namespace KysChess
