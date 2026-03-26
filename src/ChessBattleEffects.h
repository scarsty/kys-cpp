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
    MPDrain,
    MPRecoveryBonus,
    SkillDmgPct,
    SkillReflectPct,
    CDR,
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
    BleedPersist,
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
};

struct ComboEffect
{
    EffectType type;
    int value;
    int value2 = 0;
    Trigger trigger = Trigger::Always;
    int triggerValue = 0;
    int duration = 0;
    int maxCount = 0;
};

struct RoleComboState
{
    // Stat buffs
    int flatHP = 0, flatATK = 0, flatDEF = 0, flatSPD = 0;
    double pctHP = 0, pctATK = 0, pctDEF = 0, pctSPD = 0;

    // Trigger values
    int flatDmgReduction = 0;
    int flatDmgIncrease = 0;
    int blockChancePct = 0;
    int dodgeChancePct = 0;
    bool dodgeThenCrit = false;
    int critChancePct = 0;
    int critMultiplier = 150;
    std::vector<int> everyNthDoubles;
    int armorPenChancePct = 0;
    int armorPenPct = 0;
    int stunChancePct = 0;
    int stunFrames = 0;
    int knockbackChancePct = 0;
    int poisonDOTPct = 0;
    int poisonDuration = 0;
    int poisonDmgAmpPct = 0;
    int mpOnHit = 0;
    int mpDrain = 0;
    int mpRecoveryBonusPct = 0;
    int skillDmgPct = 0;
    int skillReflectPct = 0;
    int cdrPct = 0;
    int shieldPctMaxHP = 0;
    int shieldFreezeResPct = 0;
    int healAuraPct = 0;
    int healAuraFlat = 0;
    int healAuraInterval = 0;
    int healedATKSPDBoostPct = 0;
    int hpRegenPct = 0;
    int hpRegenInterval = 0;
    // Generic triggered effects (non-Always triggers stored here)
    std::vector<ComboEffect> triggeredEffects;
    int freezeReductionPct = 0;
    int controlImmunityFrames = 0;
    int killHealPct = 0;
    int killInvincFrames = 0;
    int postSkillInvincFrames = 0;
    int dmgReductionPct = 0;
    // Comeback & Scaling
    int bloodlustATKPerKill = 0;
    struct AdaptationInstance { int pctPerStack; int maxStacks; };
    std::vector<AdaptationInstance> adaptations;
    struct DodgeAdaptationInstance { int pctPerStack; int maxStacks; };
    std::vector<DodgeAdaptationInstance> dodgeAdaptations;
    struct RampingInstance { int pctPerStack; int maxStacks; };
    std::vector<RampingInstance> rampings;
    // --- New effects (expanded pool) ---
    int bleedChancePct = 0;
    bool bleedPersist = false;
    int bleedMaxStacks = 5;
    bool postSkillDash = false;
    int postSkillDashFrames = 0;
    bool blinkAttack = false;
    int allyDeathStatBoost = 0;
    int cloneSummonCount = 0;
    int projectileReflectPct = 0;
    int onSkillTeamHeal = 0;
    int onSkillTeamHealPct = 0;
    bool deathPrevention = false;
    int deathPreventionFrames = 0;
    int charmCDRChancePct = 0;
    int charmCDRAmountPct = 0;
    int offensiveCharmChancePct = 0;
    int deathAOEPct = 0;
    int deathAOEStunFrames = 0;
    int shieldOnAllyDeathCount = 0;
    int enemyTopDebuffCount = 0;
    int enemyTopDebuffValue = 0;
    int enemyTopDebuffApplied = 0;
    bool forcePullProtect = false;
    bool forcePullExecute = false;
    int damageImmunityAfterFrames = 0;
    int damageImmunityDuration = 0;
    int autoUltimateAfterFrames = 0;
    int ultimateExtraProjectiles = 0;
    int blockFirstHitsCount = 0;

    // Mutable runtime state
    std::map<int, int> everyNthCounters;  // N value → counter
    bool dodgedLast = false;
    int shield = 0;
    int poisonTimer = 0;
    int poisonTickDmg = 0;
    int poisonSourceId = -1;
    std::map<Trigger, int> triggerTimers;
    bool lastAliveFlag = false;
    std::map<int, int> effectActivationCounts;  // effect index → count
    std::vector<std::map<int, int>> adaptationStacks;  // per instance: enemyID → stacks
    std::vector<std::map<int, int>> dodgeAdaptationStacks;  // per instance: enemyID → stacks
    std::vector<int> rampingStacks;  // per instance
    std::vector<int> rampingIdleTimers;  // per instance
    int bleedStacks = 0;
    int bleedTimer = 0;
    bool bleedPersistFlag = false;
    int bleedSourceId = -1;
    int mpBlockTimer = 0;
    bool deathPreventionUsed = false;
    bool forcePullProtectUsed = false;
    bool forcePullExecuteUsed = false;
    int shieldOnAllyDeathTracker = 0;
    bool onSkillTeamHealPending = false;
    bool postSkillDashPending = false;
    int postSkillDashTimer = 0;
    bool isSummonedClone = false;
    bool blinkAttackUseWeakest = false;
    struct TempAttackBuffInstance { int attackBonus = 0; int remainingFrames = 0; };
    std::vector<TempAttackBuffInstance> tempAttackBuffs;
    int damageImmunityTimer = 0;
    int autoUltimateTimer = 0;
    int blockFirstHitsRemaining = 0;
    int goldCoefficient = 0;
};

class ChessBattleEffects
{
public:
    static const std::map<std::string, EffectType>& getEffectTypeMap();
    static bool parseEffect(const YAML::Node& eNode, ComboEffect& out, const std::string& context);
    static void applyEffect(RoleComboState& s, const ComboEffect& e);
    static void mergeEffects(std::map<int, RoleComboState>& states,
                             const std::vector<ComboEffect>& effects,
                             const std::vector<int>& roleIds);
};

}  // namespace KysChess
