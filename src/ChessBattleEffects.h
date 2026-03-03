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
    OnHit,  // Proc on attack hit
};

enum class EffectType
{
    // Stat buffs (pre-battle)
    FlatHP, FlatATK, FlatDEF, FlatSPD,
    PctHP, PctATK, PctDEF, NegPctDEF,

    // Trigger effects (runtime)
    FlatDmgReduction,
    BlockChance,
    DodgeChance,
    DodgeThenCrit,
    CritChance,
    CritMultiplier,
    EveryNthDouble,
    ArmorPenChance,  // DEPRECATED: use ArmorPen with OnHit trigger
    ArmorPenPct,     // DEPRECATED: use ArmorPen with OnHit trigger
    ArmorPen,        // Unified: trigger=OnHit, value=chance, value2=pen%
    StunChance,      // DEPRECATED: use Stun with OnHit trigger
    Stun,            // Unified: trigger=OnHit, value=chance, value2=duration
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
    RampingDmg,
    // Triggered heal
    HealBurst,
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
    double pctHP = 0, pctATK = 0, pctDEF = 0;

    // Trigger values
    int flatDmgReduction = 0;
    int blockChancePct = 0;
    int dodgeChancePct = 0;
    bool dodgeThenCrit = false;
    int critChancePct = 0;
    int critMultiplier = 150;
    int everyNthDouble = 0;
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
    int adaptationPctPerStack = 0;
    int adaptationMaxStacks = 0;
    int rampingDmgPct = 0;
    int rampingDmgMaxStacks = 0;

    // Mutable runtime state
    int hitCounter = 0;
    bool dodgedLast = false;
    int shield = 0;
    int poisonTimer = 0;
    int poisonTickDmg = 0;
    std::map<Trigger, int> triggerTimers;
    bool lastAliveFlag = false;
    std::map<int, int> effectActivationCounts;  // effect index → count
    std::map<int, int> adaptationStacks;
    int rampingStacks = 0;
    int rampingIdleTimer = 0;
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
