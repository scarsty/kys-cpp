#pragma once

#include "BattleFixed.h"
#include "BattleLethalRecovery.h"
#include "BattleRuntimeRandom.h"
#include "BattleHealSystem.h"
#include "BattleOperation.h"
#include "BattleStatusSystem.h"
#include "BattleStrengthening.h"
#include "BattleUnitValues.h"
#include "../Point.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace KysChess::Battle
{

inline constexpr int OptionalDamageAttackerUnitId = -1;
inline constexpr int DualWieldBlockMaxStacks = 1;
inline constexpr int FinalDamageReductionCapPct = 80;

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
    int dualWieldBlocksRemaining = 0;
    BattleStrengthening strengthening{};

    bool deathPrevention = false;
    bool deathPreventionUsed = false;
    int deathPreventionFrames = 0;
    std::optional<BattleLethalRecovery> lethalRecovery;

    bool mpBlocked = false;
    int mpRecoveryBonusPct = 0;
};

struct BattleDamageRuntimeUnit
{
    int hurtInvincFrames = 0;
    int dualWieldBlocksRemaining = 0;
    BattleStrengthening strengthening{};
    bool deathPrevention = false;
    bool deathPreventionUsed = false;
    int deathPreventionFrames = 0;
    std::optional<BattleLethalRecovery> lethalRecovery;

    bool operator==(const BattleDamageRuntimeUnit&) const = default;
};

BattleDamageRuntimeUnit makeBattleDamageRuntimeUnit(const BattleDamageUnitState& unit);

struct BattleDamageMultiplier
{
    int percent{};
    int stacks = 1;
};

struct BattleDamageModifierState
{
    int flatDamageIncrease = 0;
    int skillDamagePct = 0;
    int poisonDamageAmpPct = 0;

    int flatDamageReduction = 0;
    int damageReductionPct = 0;
    int damageTakenIncreasePct = 0;
    bool poisoned = false;
    int maxHitPctMaxHp = 0;
    std::vector<BattleDamageMultiplier> percentageMultipliers;
};

struct BattleDamageModifierInput
{
    BattleFixed damage;
    bool usingSkill = false;
    bool ignoreDefense = false;
    BattleDamageModifierState attacker;
    BattleDamageModifierState defender;
    BattleDamageUnitState defenderUnit;
    BattleDamageKind damageKind = BattleDamageKind::Physical;
};

struct BattleDamageModifierResult
{
    BattleFixed damage;
    bool maxHitCapped = false;
    int maxHitPct = 0;
    int combinedDamageReductionBasisPoints = 0;
};

struct BattleMagicBaseDamageInput
{
    int attackerAttack = 0;
    int magicPower = 0;
    BattleFixed defenderDefense;
    int randomVariance = 0;
};

struct BattleAttackPotencySnapshot
{
    int effectiveAttack;
    int magicPower;

    BattleAttackPotencySnapshot() = delete;
    BattleAttackPotencySnapshot(int effectiveAttack, int magicPower)
        : effectiveAttack(effectiveAttack)
        , magicPower(magicPower)
    {
    }

    bool operator==(const BattleAttackPotencySnapshot&) const = default;
};

struct BattleHitShapeInput
{
    BattleFixed baseDamage;
    int projectileCancelDamage = 0;
    int strengthPct = 100;
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

struct BattleHitShapeResult
{
    BattleFixed damage;
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
    std::optional<BattleStatusProducerProvenance> bleedProducer;
};

struct BattleDamageAbsorptionLayer
{
    std::uint64_t sequence{};
    int absorbedPct{};
};

struct BattleDamageAbsorptionReceipt
{
    std::uint64_t sequence{};
    int absorbedDamage{};
};

struct BattleDamageDefenseInput
{
    int damage = 0;
    bool executed = false;
    bool defenderWasInvincible = false;
    BattleDamageUnitState defender;
    bool blockByStatusLayer = false;
    int singleHitCap = 0;
    int remainingDamageBasisPoints = 10'000;
    std::vector<BattleDamageAbsorptionLayer> absorptionLayers;
    int guardReductionPct{};
};

struct BattleDamageDefenseResult
{
    bool guardChargeConsumed{};
    int guardPreventedDamage{};
    int damage = 0;
    BattleDamageUnitState defender;
    int shieldAbsorbed = 0;
    bool blockedByInvincible = false;
    bool blockedByDualWield = false;
    bool blockedByDamageLayer = false;
    bool singleHitCapConsumed = false;
    bool singleHitCapped = false;
    int singleHitCap = 0;
    bool shieldBroken = false;
    int remainingDamageBasisPoints = 10'000;
    std::vector<BattleDamageAbsorptionReceipt> absorptionReceipts;
};

struct BattleDamageTakenResult
{
    BattleDamageUnitState defender;
    bool hurtInvincGranted = false;
    bool deathPrevented = false;
    bool died = false;
    bool recoveryTested = false;
    bool recoverySucceeded = false;
    int invincibilityGranted = 0;
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
    int pendingDamage = 0;
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
    BattleHealModifierState healModifiers;
};

struct BattleOnHitResourceResult
{
    BattleResourceUnitState attacker;
    BattleResourceUnitState target;
    std::optional<BattleHealResult> heal;
    int mpRestored = 0;
    int hpHealed = 0;
    int mpDrained = 0;
};

enum class BattleDamageEventType
{
    DamageApplied,
    MpDamageApplied,
    ShieldAbsorbed,
    BlockedByInvincible,
    BlockedByDualWield,
    BlockedByDamageLayer,
    SingleHitCapped,
    DeathPrevented,
    UnitDied,
    ExecuteTriggered,
    HpRestored,
    MpRestored,
    MpDrained,
    CooldownExtended,
    StatusApplied,
};

enum class BattleDamageStatusType
{
    None = 0,
    Hitstun = 1,
    Stun = 2,
    Poison = 3,
    Bleed = 4,
    MpBlocked = 6,
};

enum class BattlePreResolvedModifierPolicy
{
    None,
    DefenderTypedStatuses,
};

struct BattleDamageEvent
{
    BattleDamageEventType type{};
    BattleDamageStatusType statusType{};
    int sourceUnitId{};
    int targetUnitId{};
    int value{};
    int maxValue{};
    BattleDamageKind damageKind = BattleDamageKind::Physical;
};

struct BattleDamageRequest
{
    int attackerUnitId = OptionalDamageAttackerUnitId;
    int defenderUnitId = -1;
    int baseDamage = 0;
    int mpDamage = 0;
    BattleDamageKind damageKind = BattleDamageKind::Physical;
    bool acceptedHit = false;
    bool preResolvedDamage = false;
    int preResolvedDamageReductionBasisPoints = 0;
    BattlePreResolvedModifierPolicy preResolvedModifierPolicy{};
    bool usingSkill = false;
    bool ignoreDefense = false;
    bool canExecute = false;
    int executeThresholdPct = 0;

    int mpOnHit = 0;
    int hpOnHit = 0;
    int mpDrain = 0;
    int cooldownExtendPct = 0;

    int hitstunFrames = 0;
    int stunFrames = 0;
    int frozenLowHpImmunityPct = 25;
    int bleedStacks = 0;
    int bleedMaxStacks = 0;
    std::optional<BattleStatusProducerProvenance> bleedProducer;
    bool triggersDefenseEffects = true;
    // 轉移傷害已結算原目標減傷，不可再次轉移或重算攻擊加成。
    bool redirected = false;
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
    int guardReductionPct{};
    // 防禦與護盾結算後，以實際可能扣除的生命傷害進行毒傷轉化。
    std::function<BattleHealResult(int)> convertPoisonDamage;
    bool redirectHpDamage = false;
    BattleDamageRequest request;
    BattleDamageUnitState attacker;
    BattleDamageUnitState defender;
    BattleDamageModifierState attackerModifiers;
    BattleDamageModifierState defenderModifiers;
    BattleStatusUnitState attackerStatus;
    BattleStatusUnitState defenderStatus;
    BattleCooldownState defenderCooldown;
    // Signed final outgoing delta.  This is deliberately outside the base
    // modifier pass so live area queries also affect pre-resolved damage.
    int liveOutgoingDamagePctDelta = 0;
    BattleHealModifierState attackerHealModifiers;
    std::vector<BattleDamageAbsorptionLayer> absorptionLayers;
};

struct BattleDamageTransactionResult
{
    bool guardChargeConsumed{};
    int guardPreventedDamage{};
    BattleDamageUnitState attacker;
    BattleDamageUnitState defender;
    BattleUnitDelta attackerDelta;
    BattleUnitDelta defenderDelta;
    BattleStatusUnitState defenderStatus;
    BattleCooldownState defenderCooldown;
    std::vector<BattleDamageEvent> events;
    std::vector<BattleResolvedHealTransaction> resolvedHeals;
    int resolvedDamageBeforeDefense = 0;
    int finalHpDamage = 0;
    int redirectedHpDamage{};
    int finalMpDamage = 0;
    int cooldownDelta = 0;
    int shieldAbsorbed = 0;
    std::vector<BattleDamageAbsorptionReceipt> absorptionReceipts;
    std::optional<BattleStatusConsumptionReceipt> defenseStatusConsumed;
    bool executed = false;
    bool killed = false;
    bool recoveryTested = false;
    bool recoverySucceeded = false;
    bool hurtInvincGranted = false;
    bool deathPrevented = false;
    bool blockedByInvincible = false;
    bool blockedByDualWield = false;
    bool blockedByDamageLayer = false;
    bool singleHitCapConsumed = false;
    bool singleHitCapped = false;
    int combinedDamageReductionBasisPoints = 0;
    BattleDamageKind damageKind = BattleDamageKind::Physical;
    int invincibilityGranted = 0;
};

class BattleDamageSystem
{
public:
    BattleDamageTransactionResult resolveTransaction(const BattleDamageTransactionInput& input, BattleRuntimeRandom* recoveryRandom = nullptr) const;
    BattleDamageModifierResult applyModifiers(const BattleDamageModifierInput& input) const;
    BattleDamageDefenseResult resolveDefense(const BattleDamageDefenseInput& input) const;
    BattleDamageTakenResult applyDamageTaken(BattleDamageUnitState defender, int damage, bool triggersDefenseEffects = true, BattleRuntimeRandom* recoveryRandom = nullptr) const;
    BattleCooldownIncreaseResult extendActiveCooldown(BattleCooldownState unit, int pct) const;
    bool shouldExecute(const BattleExecuteInput& input) const;
    BattleOnHitResourceResult applyOnHitResources(const BattleOnHitResourceInput& input) const;
    BattleStatusApplyResult applyBleed(
        BattleStatusUnitState target,
        const BattleStatusProducerProvenance& provenance,
        int stacks,
        int maxStacks) const;
    int resolveMagicBaseDamage(const BattleMagicBaseDamageInput& input) const;
    BattleAttackPotencySnapshot snapshotAttackPotency(
        int effectiveAttack,
        int magicPower) const;
    int resolveAttackPotencyAgainstDefender(
        const BattleAttackPotencySnapshot& potency,
        BattleFixed defenderDefense,
        int randomVariance) const;
    BattleHitShapeResult shapeHitDamage(const BattleHitShapeInput& input) const;
    BattleDamageRequest makeScriptedHitRequest(const BattleScriptedHitRequestInput& input) const;
};

int combineBattleBlockChancePct(int baseChancePct, int liveAreaChancePct);
int cappedBattleDamageRemainingBasisPoints(int remainingDamageBasisPoints, int reductionPct);
void applyBattleDamageReduction(BattleFixed& damage, int reductionPct, int& remainingDamageBasisPoints);
void applyBattleDamagePercentDelta(BattleFixed& damage, std::int64_t percentDelta, int& remainingDamageBasisPoints);
void applyBattleDamageMultiplier(BattleFixed& damage, const BattleDamageMultiplier& multiplier, int& remainingDamageBasisPoints);
BattleDamageUnitState makeBattleDamageUnitState(const BattleRuntimeUnit& unit, const BattleDamageRuntimeUnit* runtime);
void writeBattleDamageRuntimeUnit(BattleDamageRuntimeUnit& runtime, const BattleDamageUnitState& unit);
BattleCooldownState makeBattleFrameCooldownState(const BattleRuntimeUnit& unit);

}  // namespace KysChess::Battle
