#pragma once

#include "BattleOperation.h"
#include "BattleStatusSystem.h"
#include "BattleUnitValues.h"
#include "../Point.h"

#include <vector>

namespace KysChess
{
struct RoleComboState;
}

namespace KysChess::Battle
{

struct BattleRuntimeUnit;

struct BattleDamageUnitState
{
    int id = -1;
    bool alive = true;
    BattleUnitVitals vitals;
    int attack = 0;
    int invincible = 0;
    int hurtInvincFrames = 0;

    int shield = 0;
    int blockFirstHitsRemaining = 0;

    bool deathPrevention = false;
    bool deathPreventionUsed = false;
    int deathPreventionFrames = 0;

    int killHealPct = 0;
    int killInvincFrames = 0;
    int bloodlustAttackPerKill = 0;

    bool mpBlocked = false;
    int mpRecoveryBonusPct = 0;
};

struct BattleDamageRuntimeUnit
{
    int id = -1;
    int hurtInvincFrames = 0;
    int blockFirstHitsRemaining = 0;
    bool deathPrevention = false;
    bool deathPreventionUsed = false;
    int deathPreventionFrames = 0;
    int killHealPct = 0;
    int killInvincFrames = 0;
    int bloodlustAttackPerKill = 0;
};

struct BattleDamageModifierState
{
    int flatDamageIncrease = 0;
    int skillDamagePct = 0;
    int poisonDamageAmpPct = 0;
    std::vector<DamageReduceDebuff> outgoingDamageReduceDebuffs;

    int flatDamageReduction = 0;
    int damageReductionPct = 0;
    int poisonTimer = 0;
    int maxHitPctMaxHp = 0;
};

struct BattleDamageModifierInput
{
    double damage = 0.0;
    bool usingSkill = false;
    bool ignoreDefense = false;
    BattleDamageModifierState attacker;
    BattleDamageModifierState defender;
    BattleDamageUnitState defenderUnit;
};

struct BattleDamageModifierResult
{
    double damage = 0.0;
    bool maxHitCapped = false;
    int maxHitPct = 0;
};

struct BattleMagicBaseDamageInput
{
    int attackerAttack = 0;
    int magicPower = 0;
    double defenderDefense = 0.0;
    int randomVariance = 0;
};

struct BattleLegacyHitShapeInput
{
    double baseDamage = 0.0;
    int projectileCancelDamage = 0;
    double strengthMultiplier = 1.0;
    int frame = 0;
    int totalFrame = 1;
    Pointf impactPosition;
    Pointf defenderPosition;
    Pointf defenderFacing;
    BattleOperationType operationType = BattleOperationType::None;
    bool usingSkill = false;
    int attackerActProperty = 0;
    int defenderActProperty = 0;
};

struct BattleLegacyHitShapeResult
{
    double damage = 0.0;
    int frozenFrames = 0;
    double knockbackStrength = 0.0;
    double knockbackVelocityCap = 0.0;
};

struct BattleScriptedHitRequestInput
{
    int attackerUnitId = -1;
    int defenderUnitId = -1;
    int stunFrames = 0;
    int bleedStacks = 0;
    int bleedMaxStacks = 0;
};

struct BattleDamageDefenseInput
{
    double damage = 0.0;
    bool executed = false;
    bool reflected = false;
    bool defenderWasInvincible = false;
    BattleDamageUnitState defender;
};

struct BattleDamageDefenseResult
{
    double damage = 0.0;
    BattleDamageUnitState defender;
    int shieldAbsorbed = 0;
    bool blockedByInvincible = false;
    bool blockedByFirstHit = false;
    bool shieldBroken = false;
};

struct BattleDamageTakenResult
{
    BattleDamageUnitState defender;
    bool hurtInvincGranted = false;
    bool deathPrevented = false;
    bool died = false;
    int invincibilityGranted = 0;
};

struct BattleKillRewardInput
{
    BattleDamageUnitState killer;
};

struct BattleKillRewardResult
{
    BattleDamageUnitState killer;
    int healed = 0;
    int invincibilityGranted = 0;
    int attackGranted = 0;
};

struct BattleCooldownState
{
    bool alive = true;
    int cooldown = 0;
    int cooldownMax = 0;
    bool haveAction = false;
    BattleOperationType operationType = BattleOperationType::None;
    int actType = -1;
};

struct BattleCooldownIncreaseResult
{
    BattleCooldownState unit;
    bool increased = false;
    int before = 0;
    int after = 0;
};

struct BattleExecuteInput
{
    int projectedHpBeforeDamage = 0;
    int maxHp = 0;
    double pendingDamage = 0.0;
    bool appliesHpDamage = true;
    int thresholdPct = 0;
};

struct BattleResourceUnitState
{
    int id = -1;
    bool alive = true;
    BattleUnitVitals vitals;
    bool mpBlocked = false;
    int mpRecoveryBonusPct = 0;
};

struct BattleOnHitResourceInput
{
    BattleResourceUnitState attacker;
    BattleResourceUnitState target;
    int mpOnHit = 0;
    int hpOnHit = 0;
    int mpDrain = 0;
};

struct BattleOnHitResourceResult
{
    BattleResourceUnitState attacker;
    BattleResourceUnitState target;
    int mpRestored = 0;
    int hpHealed = 0;
    int mpDrained = 0;
};

struct BattlePoisonApplyInput
{
    BattleStatusUnitState target;
    int sourceUnitId = -1;
    int poisonPct = 0;
    int durationFrames = 0;
};

struct BattleStatusApplyResult
{
    BattleStatusUnitState target;
    bool applied = false;
    int value = 0;
};

enum class BattleDamageEventType
{
    DamageApplied,
    MpDamageApplied,
    ShieldAbsorbed,
    BlockedByInvincible,
    BlockedByFirstHit,
    DeathPrevented,
    UnitDied,
    KillRewardApplied,
    ExecuteTriggered,
    HpRestored,
    MpRestored,
    MpDrained,
    CooldownExtended,
    StatusApplied,
};

enum class BattleDamageStatusType
{
    None,
    Frozen,
    Poison,
    Bleed,
    DamageReduceDebuff,
    MpBlocked,
};

struct BattleDamageEvent
{
    BattleDamageEventType type{};
    BattleDamageStatusType statusType{};
    int sourceUnitId{};
    int targetUnitId{};
    int value{};
    int maxValue{};
};

struct BattleDamageRequest
{
    int attackerUnitId = -1;
    int defenderUnitId = -1;
    double baseDamage = 0.0;
    int mpDamage = 0;
    bool acceptedHit = false;
    bool preResolvedDamage = false;
    bool usingSkill = false;
    bool ignoreDefense = false;
    bool reflected = false;
    bool canExecute = false;
    int executeThresholdPct = 0;

    int mpOnHit = 0;
    int hpOnHit = 0;
    int mpDrain = 0;
    int cooldownExtendPct = 0;

    int frozenFrames = 0;
    int frozenLowHpImmunityPct = 25;
    int poisonPct = 0;
    int poisonDurationFrames = 0;
    int bleedStacks = 0;
    int bleedMaxStacks = 0;
    int damageReduceDebuffDurationFrames = 0;
    int damageReduceDebuffPct = 0;
    int mpBlockFrames = 0;
    bool triggersDefenseEffects = true;
};

struct BattleUnitDelta
{
    int unitId{};
    int hpDelta{};
    int mpDelta{};
    int shieldDelta{};
    int invincibleDelta{};
    int attackDelta{};
    bool aliveChanged{};
    bool alive{};
};

struct BattleDamageTransactionInput
{
    BattleDamageRequest request;
    BattleDamageUnitState attacker;
    BattleDamageUnitState defender;
    BattleDamageModifierState attackerModifiers;
    BattleDamageModifierState defenderModifiers;
    BattleStatusUnitState defenderStatus;
    BattleCooldownState defenderCooldown;
};

struct BattleDamageTransactionResult
{
    BattleDamageUnitState attacker;
    BattleDamageUnitState defender;
    BattleUnitDelta attackerDelta;
    BattleUnitDelta defenderDelta;
    BattleStatusUnitState defenderStatus;
    BattleCooldownState defenderCooldown;
    std::vector<BattleDamageEvent> events;
    int finalHpDamage = 0;
    int finalMpDamage = 0;
    int cooldownDelta = 0;
    int shieldAbsorbed = 0;
    bool executed = false;
    bool killed = false;
    bool hurtInvincGranted = false;
    bool deathPrevented = false;
    bool blockedByInvincible = false;
    bool blockedByFirstHit = false;
    int invincibilityGranted = 0;
};

class BattleDamageSystem
{
public:
    BattleDamageTransactionResult resolveTransaction(const BattleDamageTransactionInput& input) const;
    BattleDamageModifierResult applyModifiers(const BattleDamageModifierInput& input) const;
    BattleDamageDefenseResult resolveDefense(const BattleDamageDefenseInput& input) const;
    BattleDamageTakenResult applyDamageTaken(BattleDamageUnitState defender, int damage, bool triggersDefenseEffects = true) const;
    BattleKillRewardResult applyKillReward(const BattleKillRewardInput& input) const;
    BattleCooldownIncreaseResult extendActiveCooldown(BattleCooldownState unit, int pct) const;
    bool shouldExecute(const BattleExecuteInput& input) const;
    BattleOnHitResourceResult applyOnHitResources(const BattleOnHitResourceInput& input) const;
    BattleStatusApplyResult applyPoisonIfStronger(const BattlePoisonApplyInput& input) const;
    BattleStatusApplyResult applyBleed(BattleStatusUnitState target, int sourceUnitId, int stacks, int maxStacks) const;
    BattleStatusApplyResult applyDamageReduceDebuff(BattleStatusUnitState target, int durationFrames, int pct) const;
    int resolveMagicBaseDamage(const BattleMagicBaseDamageInput& input) const;
    BattleLegacyHitShapeResult shapeLegacyHitDamage(const BattleLegacyHitShapeInput& input) const;
    BattleDamageRequest makeScriptedHitRequest(const BattleScriptedHitRequestInput& input) const;
};

BattleDamageModifierState makeBattleDamageModifierState(const RoleComboState* state);
BattleDamageUnitState makeBattleDamageUnitState(const BattleRuntimeUnit& unit, const BattleDamageRuntimeUnit* runtime);
void writeBattleDamageRuntimeUnit(BattleDamageRuntimeUnit& runtime, const BattleDamageUnitState& unit);
BattleCooldownState makeBattleFrameCooldownState(const BattleRuntimeUnit& unit);

}  // namespace KysChess::Battle
