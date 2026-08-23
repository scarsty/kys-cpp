#pragma once

#include "../ChessBattleEffects.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeUnit;
struct BattleRuntimeUnitRecord;
class BattleRuntimeUnits;

inline constexpr int DurationlessNegativeStatusShieldCost = 50;

// New battle statuses keep their semantic payload intact.  Poison, bleed,
// stun and MP block continue to use their specialised fields below; the
// common status API adapts those fields into the same query snapshot.
struct BattleTypedStatusInstance
{
    BattleStatusKind kind{};
    int sourceUnitId = -1;
    int remainingFrames = 0;
    int stacks = 1;
    int potency = 0;
    int secondaryPotency = 0;
    std::uint64_t appliedSequence{};

    bool operator==(const BattleTypedStatusInstance&) const = default;
};

struct BattleStatusEffectState
{
    int poisonTimer = 0;
    int poisonStacks = 0;
    int poisonTickPct = 0;
    int poisonSourceId = -1;

    int bleedStacks = 0;
    int bleedTimer = 0;
    int bleedSourceId = -1;

    int frozenTimer = 0;
    int frozenMaxTimer = 0;
    int frozenSourceId = -1;
    std::uint64_t frozenAppliedSequence{};
    int freezeReductionPct = 0;
    int shieldFreezeResPct = 0;
    int controlImmunityFrames = 0;
    int mpBlockTimer = 0;
    int mpBlockSourceId = -1;
    std::uint64_t mpBlockAppliedSequence{};

    int statusShield = 0;
    int staggerShield = 0;

    std::uint64_t poisonAppliedSequence{};
    std::uint64_t bleedAppliedSequence{};
    std::uint64_t nextStatusSequence = 1;
    std::vector<BattleTypedStatusInstance> typedStatuses;

    bool operator==(const BattleStatusEffectState&) const = default;

    void clearStunAndHitstun()
    {
        frozenTimer = 0;
        frozenMaxTimer = 0;
        frozenSourceId = -1;
        frozenAppliedSequence = 0;
    }
};

struct BattleStatusUnitState
{
    int id = -1;
    bool alive = true;
    int hp = 0;
    int maxHp = 0;
    int attack = 0;
    int invincible = 0;

    BattleStatusEffectState effects;
};

struct BattleStatusRuntimeUnit
{
    BattleStatusEffectState effects;

    bool operator==(const BattleStatusRuntimeUnit&) const = default;
};

enum class BattleStatusEventType
{
    PoisonDamage,
    BleedDamage,
    StatusExpired,
};

struct BattleStatusEvent
{
    BattleStatusEventType type{};
    int unitId{};
    int sourceUnitId{};
    int value{};
    std::string reason;
    BattleStatusKind statusKind{};
};

struct BattleStatusTickResult
{
    std::vector<BattleStatusEvent> events;
};

struct BattleStatusSystemConfig
{
    int frame = 0;
    int poisonDamageIntervalFrames = 30;
    int bleedDamageIntervalFrames = 10;
};

struct BattleRemainingPoisonDamageInput
{
    int firstFutureFrame{};
    int remainingFrames{};
    int remainingStacks{};
    int intervalFrames{};
    int currentHp{};
    int damagePct{};
};

int projectRemainingPoisonDamage(
    const BattleRemainingPoisonDamageInput& input);

enum class BattleStatusApplyOutcome
{
    Applied,
    Refreshed,
    Replaced,
    StackChanged,
    BlockedByStatusShield,
    BlockedByStaggerShield,
    BlockedByControlImmunity,
    KeptStronger,
    TargetDead,
};

struct BattleStatusApplyRequest
{
    BattleStatusKind kind{};
    int sourceUnitId = -1;
    int durationFrames = 0;
    int stacks = 1;
    int potency = 0;
    int secondaryPotency = 0;
    EffectStackPolicy stack = EffectStackPolicy::Independent;
    std::optional<int> stackLimit;
    bool targetHasShield = false;
    int controlLowHpImmunityPct = 0;
    bool bypassStatusShield = false;
};

struct BattleStatusApplyResult
{
    BattleStatusUnitState target;
    BattleStatusApplyOutcome outcome{};
    bool applied = false;
    int value = 0;
    int requestedDurationFrames = 0;
    int appliedDurationFrames = 0;
    int statusShieldAbsorbed = 0;
    int staggerShieldAbsorbed = 0;
    int controlImmunityAbsorbed = 0;
};

struct BattleStatusRemoveRequest
{
    std::vector<BattleStatusKind> statuses;
    bool negativeOnly = false;
    bool controlOnly = false;
    bool clearCurrentActionStagger = false;
    int count = 0;
    StatusRemovalOrder order = StatusRemovalOrder::LongestRemaining;
};

struct BattleStatusRemoveResult
{
    BattleStatusUnitState target;
    int removedCount = 0;
    std::vector<BattleStatusKind> removedStatuses;
    bool currentActionStaggerCleared = false;
};

struct BattleStatusConsumeResult
{
    BattleStatusUnitState target;
    bool consumed = false;
    BattleTypedStatusInstance consumedStatus;
    int remainingStacks = 0;
};

struct BattleStatusConsumeRequest
{
    BattleStatusKind kind{};
    int stacks = 1;
    std::optional<int> sourceUnitId;
};

struct BattleStatusProtectionResult
{
    BattleStatusUnitState target;
    BattleResource resource{};
    int before = 0;
    int after = 0;
};

struct BattleNegativeEffectProtectionResult
{
    BattleStatusUnitState target;
    int requestedDurationFrames{};
    int remainingDurationFrames{};
    int statusShieldAbsorbed{};
    bool blocked = false;
};

struct BattleStatusQuerySnapshot
{
    std::vector<BattleTypedStatusInstance> statuses;
    int statusShield = 0;
    int staggerShield = 0;

    bool healingBlocked = false;
    // Kept in deterministic application order so the heal boundary can round
    // after every modifier instead of collapsing them into one scalar.
    std::vector<int> receivedHealMultipliersPct;
    int speedPctDelta = 0;
    int damageTakenPct = 0;
    int damageReductionPct = 0;
    int skillDamagePct = 0;
    int pureDamagePerHit = 0;

    bool has(BattleStatusKind kind) const;
    int stacks(BattleStatusKind kind) const;
    int potency(BattleStatusKind kind) const;
    int secondaryPotency(BattleStatusKind kind) const;
};

bool isNegativeBattleStatus(BattleStatusKind kind);
bool isControlBattleStatus(BattleStatusKind kind);

class BattleStatusSystem
{
public:
    explicit BattleStatusSystem(BattleStatusSystemConfig config);

    BattleStatusTickResult tick(BattleRuntimeUnitRecord& unit) const;
    BattleStatusTickResult tick(BattleRuntimeUnits& records) const;
    BattleStatusApplyResult apply(
        BattleStatusUnitState target,
        const BattleStatusApplyRequest& request) const;
    BattleStatusRemoveResult remove(
        BattleStatusUnitState target,
        const BattleStatusRemoveRequest& request) const;
    BattleStatusConsumeResult consume(
        BattleStatusUnitState target,
        const BattleStatusConsumeRequest& request) const;
    BattleStatusProtectionResult changeProtection(
        BattleStatusUnitState target,
        BattleResource resource,
        int delta) const;
    BattleNegativeEffectProtectionResult protectNegativeEffect(
        BattleStatusUnitState target,
        int durationFrames) const;
    BattleStatusQuerySnapshot snapshot(const BattleStatusUnitState& target) const;

private:
    BattleStatusSystemConfig config_;
};

BattleStatusRuntimeUnit makeBattleStatusRuntimeUnit(const BattleStatusUnitState& unit);
BattleStatusUnitState makeBattleStatusUnitState(const BattleRuntimeUnit& unit);
BattleStatusUnitState makeBattleStatusUnitState(const BattleStatusRuntimeUnit& status, const BattleRuntimeUnit& unit);
void writeBattleStatusRuntimeUnit(BattleStatusRuntimeUnit& status, const BattleStatusUnitState& unit);
void rewriteBattleStatusSourceUnitId(
    BattleStatusRuntimeUnit& status,
    int sourceUnitId,
    int replacementUnitId);

}  // namespace KysChess::Battle
