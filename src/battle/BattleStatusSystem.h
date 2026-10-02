#pragma once

#include "../ChessBattleEffectTypes.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeUnit;
struct BattleRuntimeUnitRecord;
class BattleRuntimeUnits;

inline constexpr int DurationlessNegativeStatusShieldCost = 50;

struct BattleStatusEffectOrigin
{
    EffectSourceBinding binding;
    EffectRuleId ruleId;
    std::uint32_t ruleOrder{};

    bool operator==(const BattleStatusEffectOrigin&) const = default;
};

struct StatusProducerKey
{
    EffectSourceBinding binding;
    EffectRuleId ruleId;
    std::uint32_t actionOrder{};
    std::uint32_t behaviorRuleOrder{};
    std::uint32_t behaviorActionOrder{};

    bool operator==(const StatusProducerKey&) const = default;
};

struct StatusProducerFamilyKey
{
    EffectSourceKind sourceKind{};
    int sourceId{};
    int logicalOwnerUnitId = -1;
    EffectRuleId ruleId;
    std::uint32_t actionOrder{};
    std::uint32_t behaviorRuleOrder{};
    std::uint32_t behaviorActionOrder{};

    bool operator==(const StatusProducerFamilyKey&) const = default;
};

struct BattleStatusProducerProvenance
{
    StatusProducerKey producer;
    StatusProducerFamilyKey producerFamily;
    BattleStatusEffectOrigin origin;
    int sourceUnitId = -1;

    bool operator==(const BattleStatusProducerProvenance&) const = default;
};

BattleStatusProducerProvenance makeBattleStatusProducerProvenance(
    EffectSourceBinding binding,
    EffectRuleId ruleId,
    std::uint32_t ruleOrder = 0,
    std::uint32_t actionOrder = 0,
    std::uint32_t behaviorRuleOrder = 0,
    std::uint32_t behaviorActionOrder = 0);

struct BattleStatusContribution
{
    BattleStatusKind kind{};
    std::optional<StatusProducerKey> producer;
    std::optional<StatusProducerFamilyKey> producerFamily;
    std::optional<int> familyLocalLimit;
    std::optional<int> targetTotalLimit;
    std::shared_ptr<const StatusBehaviorDefinition> behavior;
    std::vector<EffectRuleRuntimeState> behaviorRuntime;
    int sourceUnitId = -1;
    int remainingFrames = 0;
    int maximumFrames = 0;
    int stacks = 1;
    std::optional<BattleStatusEffectOrigin> origin;
    std::uint64_t appliedSequence{};
    std::uint64_t negativeEffectSequence{};

    bool operator==(const BattleStatusContribution&) const = default;
};

// Group states contain stable indices into the holder's contribution range.
// The contribution record owns the packet data; the variant makes the catalog
// storage shape explicit without changing global application-sequence order.
struct ProducerOwnedContributions
{
    std::vector<std::size_t> contributionIndices;

    bool operator==(const ProducerOwnedContributions&) const = default;
};

struct SharedLayerDebuff
{
    std::size_t contributionIndex{};

    bool operator==(const SharedLayerDebuff&) const = default;
};

struct SelectedDebuffInstance
{
    std::size_t contributionIndex{};

    bool operator==(const SelectedDebuffInstance&) const = default;
};

struct SharedDurationControl
{
    std::size_t contributionIndex{};

    bool operator==(const SharedDurationControl&) const = default;
};

using BattleStatusGroupState = std::variant<
    ProducerOwnedContributions,
    SharedLayerDebuff,
    SelectedDebuffInstance,
    SharedDurationControl>;

struct BattleStatusGroupEntry
{
    BattleStatusKind kind{};
    BattleStatusGroupState state;

    bool operator==(const BattleStatusGroupEntry&) const = default;
};

// The public range remains contribution-shaped because active behavior,
// provenance, and positive producer families operate on individual packets.
// Mutations pass through this holder-local group boundary and maintain the
// explicit group-state variant above. Producer-owned groups are the only model
// that may contain several packet indices.
class BattleStatusGroupStorage
{
public:
    using value_type = BattleStatusContribution;
    using container_type = std::vector<value_type>;
    using iterator = container_type::iterator;
    using const_iterator = container_type::const_iterator;

    BattleStatusGroupStorage() = default;
    BattleStatusGroupStorage(std::initializer_list<value_type> values);
    BattleStatusGroupStorage(container_type values);

    BattleStatusGroupStorage& operator=(std::initializer_list<value_type> values);
    BattleStatusGroupStorage& operator=(container_type values);

    bool operator==(const BattleStatusGroupStorage&) const = default;

    iterator begin() { return values_.begin(); }
    const_iterator begin() const { return values_.begin(); }
    const_iterator cbegin() const { return values_.cbegin(); }
    iterator end() { return values_.end(); }
    const_iterator end() const { return values_.end(); }
    const_iterator cend() const { return values_.cend(); }

    bool empty() const { return values_.empty(); }
    std::size_t size() const { return values_.size(); }
    void reserve(std::size_t count) { values_.reserve(count); }
    void clear()
    {
        values_.clear();
        groups_.clear();
        taxonomyKinds_.clear();
    }

    value_type& front() { return values_.front(); }
    const value_type& front() const { return values_.front(); }
    value_type& back() { return values_.back(); }
    const value_type& back() const { return values_.back(); }
    value_type& operator[](std::size_t index) { return values_[index]; }
    const value_type& operator[](std::size_t index) const { return values_[index]; }

    void push_back(const value_type& value);
    void push_back(value_type&& value);
    iterator erase(const_iterator position);

    // Group identity is derived from mutable contribution records.  Refresh at
    // the taxonomy boundary so a caller changing a packet's kind through the
    // public contribution range cannot observe stale group state.  A cached
    // kind sequence avoids invalidating an already returned group pointer on
    // every read; pointers are rebuilt only after an actual identity change.
    std::size_t groupCount() const
    {
        ensureGroupsFresh();
        return groups_.size();
    }
    const BattleStatusGroupState* groupState(BattleStatusKind kind) const;

    template <typename Predicate>
    std::size_t eraseIf(Predicate predicate)
    {
        const auto before = values_.size();
        std::erase_if(values_, std::move(predicate));
        rebuildGroups();
        return before - values_.size();
    }

private:
    void assertCanInsert(BattleStatusKind kind) const;
    void ensureGroupsFresh() const;
    void rebuildGroups() const;

    container_type values_;
    mutable std::vector<BattleStatusGroupEntry> groups_;
    mutable std::vector<BattleStatusKind> taxonomyKinds_;
};

struct BattleStatusEffectState
{
    int freezeReductionPct = 0;
    int shieldFreezeResPct = 0;
    int controlImmunityFrames = 0;

    int statusShield = 0;
    int staggerShield = 0;

    std::uint64_t nextStatusSequence = 1;
    std::uint64_t nextNegativeEffectSequence = 1;
    BattleStatusGroupStorage statuses;

    bool operator==(const BattleStatusEffectState&) const = default;

    BattleStatusContribution* find(BattleStatusKind kind);
    const BattleStatusContribution* find(BattleStatusKind kind) const;
    bool has(BattleStatusKind kind) const;
    int remainingFrames(BattleStatusKind kind) const;
    int maximumFrames(BattleStatusKind kind) const;
    void setFrames(BattleStatusKind kind,
                   int frames,
                   int maximumFrames = 0,
                   int sourceUnitId = -1);
    void clear(BattleStatusKind kind);
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
    int framesUntilNextTick{};
    int remainingFrames{};
    int remainingStacks{};
    int intervalFrames{};
    int currentHp{};
    int damagePct{};
};

int projectRemainingPoisonDamage(
    const BattleRemainingPoisonDamageInput& input);

// 中毒傷害屬於貢獻所攜帶的行為，不再另存匿名 potency。
// 需要顯示或執行中毒語義的路徑必須從同一份已綁定行為讀取。
int poisonDamagePercent(
    const std::shared_ptr<const StatusBehaviorDefinition>& behavior);

// 擴張螺旋的流血由攻擊 runtime 直接產生；這份具名工廠讓該路徑仍建立
// 完整、可獨立排程的貢獻行為，而不是退回匿名欄位。
std::shared_ptr<const StatusBehaviorDefinition> makeRuntimeBleedStatusBehavior();

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
    std::optional<StatusProducerKey> producer;
    std::optional<StatusProducerFamilyKey> producerFamily;
    std::shared_ptr<const StatusBehaviorDefinition> behavior;
    int sourceUnitId = -1;
    int durationFrames = 0;
    int stacks = 1;
    std::optional<BattleStatusEffectOrigin> origin;
    EffectStackPolicy stack = EffectStackPolicy::Independent;
    std::optional<int> stackLimit;
    std::optional<int> targetTotalLimit;
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
    StatusContributionFilter filter;
    bool negativeOnly = false;
    bool controlOnly = false;
    bool clearCurrentActionStagger = false;
    int count = 0;
    StatusRemovalOrder order = StatusRemovalOrder::LongestRemaining;
};

struct BattleStatusRemovalCandidate
{
    BattleStatusKind kind{};
    int remainingFrames{};
    std::uint64_t sequence{};
};

struct BattleStatusRemoveResult
{
    BattleStatusUnitState target;
    int removedCount = 0;
    std::vector<BattleStatusKind> removedStatuses;
    bool currentActionStaggerCleared = false;
};

struct BattleStatusConsumptionReceipt
{
    int targetUnitId{};
    BattleStatusContribution contribution;
    int remainingStacks{};
};

struct BattleStatusConsumeResult
{
    BattleStatusUnitState target;
    bool consumed = false;
    BattleStatusContribution consumedStatus;
    int remainingStacks = 0;
};

struct BattleStatusConsumeRequest
{
    BattleStatusKind kind{};
    int stacks = 1;
    StatusContributionFilter filter;
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

struct BattleStatusPersistentModifiers
{
    // Kept in deterministic contribution/action order. The heal boundary
    // filters by the actual transaction kind and rounds after each multiplier.
    std::vector<ModifyHealTransactionAction> healTransactionModifiers;
    int speedPctDelta = 0;
    int damageTakenPct = 0;
    int damageReductionPct = 0;
    int skillDamagePct = 0;
};

struct BattleStatusQuerySnapshot : BattleStatusPersistentModifiers
{
    int holderUnitId = -1;
    std::vector<BattleStatusContribution> statuses;
    int statusShield = 0;
    int staggerShield = 0;

    bool has(BattleStatusKind kind) const;
    int stacks(
        BattleStatusKind kind,
        const StatusContributionFilter& filter = {}) const;
    std::optional<int> familyCapacity(BattleStatusKind kind) const;
    std::optional<int> targetTotalCapacity(BattleStatusKind kind) const;
};

struct BattleStatusGenerationPresentation
{
    int quantity{};
    std::uint64_t appliedSequence{};
    std::shared_ptr<const StatusBehaviorDefinition> behavior;
};

struct BattleStatusFamilyPresentation
{
    std::optional<StatusProducerFamilyKey> producerFamily;
    int quantity{};
    std::optional<int> capacity;
    std::vector<BattleStatusGenerationPresentation> generations;
};

struct BattleStatusGroupPresentation
{
    BattleStatusKind kind{};
    int quantity{};
    std::optional<int> targetTotalCapacity;
    std::vector<BattleStatusFamilyPresentation> families;
};

BattleStatusGroupPresentation makeBattleStatusGroupPresentation(
    const BattleStatusEffectState& effects,
    BattleStatusKind kind);

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
    std::vector<BattleStatusRemovalCandidate> removalCandidates(
        const BattleStatusUnitState& target,
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
    BattleStatusPersistentModifiers persistentModifiers(const BattleStatusEffectState& effects) const;
    BattleStatusQuerySnapshot snapshot(const BattleStatusEffectState& effects) const;
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
