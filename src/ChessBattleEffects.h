#pragma once

#include <map>
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
    SpiralBleedProjectile,
    NearbyTrackingProjectiles,
    ForceRangedAttack,
    CounterUltimateBlock,
    MaxHitPctCurrentHP,
    FreeRefresh,
    BattleMapChoice,
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

struct AppliedEffectInstance : ComboEffect
{
    int sourceComboId = -1;  // -1 means the effect did not originate from a synergy
};

struct RoleComboState
{
    // Stat buffs
    int flatHP = 0, flatATK = 0, flatDEF = 0, flatSPD = 0;
    double pctHP = 0, pctATK = 0, pctDEF = 0, pctSPD = 0;
    int fightWinGrowthHP = 0, fightWinGrowthATK = 0, fightWinGrowthDEF = 0;

    // Trigger values
    int blockChancePct = 0;
    int stunChancePct = 0;
    int stunFrames = 0;
    int cdrPct = 0;
    std::vector<AppliedEffectInstance> appliedEffects;
    // Generic triggered effects (non-Always triggers stored here)
    std::vector<AppliedEffectInstance> triggeredEffects;
    // Comeback & Scaling
    struct AdaptationInstance { int pctPerStack; int maxStacks; };
    std::vector<AdaptationInstance> adaptations;
    struct DodgeAdaptationInstance { int pctPerStack; int maxStacks; };
    std::vector<DodgeAdaptationInstance> dodgeAdaptations;
    struct RampingInstance { int pctPerStack; int maxStacks; };
    std::vector<RampingInstance> rampings;
    // --- New effects (expanded pool) ---
    int onSkillTeamHeal = 0;
    int onSkillTeamHealPct = 0;
    int enemyTopDebuffApplied = 0;

    // Mutable runtime state
    std::map<int, int> everyNthCounters;  // N value → counter
    bool dodgedLast = false;
    std::map<Trigger, int> triggerTimers;
    bool lastAliveFlag = false;
    std::map<int, int> effectActivationCounts;  // effect index → count
    std::map<int, int> effectFrameTimers;  // effect index → frame timer
    std::vector<std::map<int, int>> adaptationStacks;  // per instance: enemyID → stacks
    std::vector<std::map<int, int>> dodgeAdaptationStacks;  // per instance: enemyID → stacks
    std::vector<int> rampingStacks;  // per instance
    std::vector<int> rampingIdleTimers;  // per instance
    bool deathPreventionUsed = false;
    int forcePullProtectRemaining = 0;
    int forcePullExecuteRemaining = 0;
    int shieldOnAllyDeathTracker = 0;
    bool onSkillTeamHealPending = false;
    bool postSkillDashPending = false;
    bool isSummonedClone = false;
    bool blinkAttackUseWeakest = false;
};

int sumAlwaysEffectValue(const RoleComboState& state, EffectType type);
int maxAlwaysEffectValue(const RoleComboState& state, EffectType type);
int maxAlwaysEffectValue2(const RoleComboState& state, EffectType type);
const AppliedEffectInstance* firstAlwaysEffect(const RoleComboState& state, EffectType type);
const AppliedEffectInstance* maxAlwaysEffectByValue(const RoleComboState& state, EffectType type);

class ChessBattleEffects
{
public:
    static const std::map<std::string, EffectType>& getEffectTypeMap();
    static bool parseEffect(const YAML::Node& eNode, ComboEffect& out, const std::string& context);
    static void applyEffect(RoleComboState& s, const ComboEffect& e, int sourceComboId = -1);
    static RoleComboState makeSummonedCloneState(const RoleComboState& sourceState);
    static void mergeEffects(std::map<int, RoleComboState>& states,
                              const std::vector<ComboEffect>& effects,
                             const std::vector<int>& roleIds,
                             int sourceComboId = -1);
};

}  // namespace KysChess
