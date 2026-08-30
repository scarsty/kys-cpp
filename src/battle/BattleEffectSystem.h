#pragma once

#include "../ChessBattleEffectTypes.h"
#include "../Point.h"
#include "BattleCastLifecycle.h"
#include "BattleHealSystem.h"

#include <array>
#include <compare>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace KysChess::Battle
{

class BattleRuntimeRandom;

struct EffectStatusSnapshot
{
    BattleStatusKind state{};
    int sourceUnitId = -1;
    int stacks = 1;
    int potency{};
    int secondaryPotency{};
};

struct EffectUnitSnapshot
{
    int id = -1;
    int team{};
    int star = 1;
    int cost{};
    bool alive = true;
    int hp{};
    int maxHp{};
    int mp{};
    int maxMp{};
    int shield{};
    int activeCooldown{};
    bool invincible = false;
    int statusShield{};
    int staggerShield{};
    int attack{};
    int defence{};
    int speed{};
    Pointf position;
    EffectMartialCategory martialCategory = EffectMartialCategory::None;
    int ultimateMagicId = -1;
    std::set<int> magicIds;
    std::set<int> comboIds;
    std::vector<EffectStatusSnapshot> statusDetails;

    bool hasState(BattleStatusKind state) const;
    bool hasStateFromSource(BattleStatusKind state, int sourceUnitId) const;
    int stackCount(BattleStatusKind stack) const;
    bool usesMagic(int magicId) const;
};

class BattleEffectReadView
{
public:
    BattleEffectReadView() = default;
    explicit BattleEffectReadView(std::span<const EffectUnitSnapshot> units,
                                  float tileWidth = 1.0f);

    std::span<const EffectUnitSnapshot> units() const;
    const EffectUnitSnapshot* findUnit(int unitId) const;
    float tileWidth() const;

private:
    std::span<const EffectUnitSnapshot> units_;
    float tileWidth_ = 1.0f;
};

struct EffectUnitResourceBeforeCast
{
    int unitId = -1;
    int mp{};
    int maxMp{};
};

class EffectResourcesBeforeCastSnapshot
{
public:
    EffectResourcesBeforeCastSnapshot() = default;

    explicit EffectResourcesBeforeCastSnapshot(
        std::vector<EffectUnitResourceBeforeCast> resources)
        : resources_(std::make_shared<const std::vector<EffectUnitResourceBeforeCast>>(
            std::move(resources)))
    {
    }

    EffectResourcesBeforeCastSnapshot(
        std::initializer_list<EffectUnitResourceBeforeCast> resources)
        : EffectResourcesBeforeCastSnapshot(
            std::vector<EffectUnitResourceBeforeCast>(resources))
    {
    }

    std::span<const EffectUnitResourceBeforeCast> values() const
    {
        return resources_ ? std::span<const EffectUnitResourceBeforeCast>(*resources_)
                          : std::span<const EffectUnitResourceBeforeCast>{};
    }

    std::size_t size() const { return values().size(); }
    bool empty() const { return values().empty(); }

private:
    std::shared_ptr<const std::vector<EffectUnitResourceBeforeCast>> resources_;
};

struct EffectFormulaInputs
{
    std::optional<std::int64_t> storedStateValue;
};

struct InitializationEventData
{
    int battleRuleId{};
};

struct FrameTickEventData
{
    int deltaFrames = 1;
    std::uint64_t periodOrdinal{};
};

struct UltimateCooldownFinishedEventData
{
    int magicId = -1;
};

struct CastPlanEventData
{
    BattleCastProvenance provenance;
    int preferredTargetUnitId = -1;
    int mpBefore{};
    int baseMpCost{};
    CastRangeMode baseRangeMode = CastRangeMode::Preserve;
    AttackPattern baseAttackPattern;
    EffectResourcesBeforeCastSnapshot resourcesBeforeCast;
};

struct CastCommitEventData
{
    BattleCastProvenance provenance;
    int targetUnitId = -1;
    int mpBefore{};
    int mpPaid{};
    CastRangeMode rangeMode = CastRangeMode::Preserve;
    AttackPattern attackPattern;
    EffectResourcesBeforeCastSnapshot resourcesBeforeCast;
};

struct AttackEventData
{
    BattleAttackProvenance provenance;
    int originalTargetUnitId = -1;
    Pointf spawnPosition;
    Pointf velocity;
    int skillId = -1;
    int baseDamage{};
    BattleDamageKind damageKind = BattleDamageKind::Physical;
};

struct HitEventData
{
    BattleAttackProvenance provenance;
    int targetUnitId = -1;
    int originalTargetUnitId = -1;
    Pointf contactPosition;
    bool acceptedHit = true;
    int preDefenseDamage{};
    BattleDamageKind damageKind = BattleDamageKind::Physical;
};

struct EffectAttackDamageOrigin
{
    BattleAttackProvenance provenance;
};

struct EffectStatusDamageOrigin
{
    BattleStatusKind status{};
    int sourceUnitId = -1;
};

struct EffectRuleDamageOrigin
{
    EffectRuleId ruleId;
    EffectSourceBinding binding;
};

struct EffectEnvironmentDamageOrigin {};

using EffectDamageOrigin = std::variant<
    EffectAttackDamageOrigin,
    EffectStatusDamageOrigin,
    EffectRuleDamageOrigin,
    EffectEnvironmentDamageOrigin>;

struct DamageResultEventData
{
    std::uint64_t transactionId{};
    EffectDamageOrigin origin;
    std::optional<EffectUnitSnapshot> attackerBefore;
    EffectUnitSnapshot defenderBefore;
    EffectUnitSnapshot defenderAfter;
    int rawDamage{};
    int resolvedDamage{};
    int shieldAbsorbed{};
    int finalHpDamage{};
    int finalMpDamage{};
    BattleDamageKind damageKind = BattleDamageKind::Physical;
    bool blocked = false;
    bool executed = false;
    bool killed = false;
};

struct HealRequestEventData
{
    HealTransactionId transactionId;
    BattleHealKind kind = BattleHealKind::Direct;
    EffectUnitSnapshot sourceBefore;
    EffectUnitSnapshot targetBefore;
    int calculatedAmount{};
    std::optional<BattleCastId> castId;
};

struct HealResultEventData
{
    HealRequestEventData request;
    EffectUnitSnapshot targetAfter;
    int modifiedAmount{};
    int appliedAmount{};
};

struct CastAggregateEventData
{
    BattleCastProvenance provenance;
    int originalTargetUnitId = -1;
    CastAggregate aggregate;
    EffectResourcesBeforeCastSnapshot resourcesBeforeCast;
};

struct ShieldBreakEventData
{
    EffectUnitSnapshot targetBefore;
    EffectUnitSnapshot targetAfter;
    int brokenAmount{};
    EffectDamageOrigin cause;
};

struct DeathEventData
{
    EffectUnitSnapshot deadBefore;
    EffectUnitSnapshot deadAfter;
    std::optional<EffectUnitSnapshot> killer;
    EffectDamageOrigin cause;
    std::uint64_t deathOrdinal{};
    bool allyOfOwner = false;
};

using EffectEventPayload = std::variant<
    InitializationEventData,
    FrameTickEventData,
    UltimateCooldownFinishedEventData,
    CastPlanEventData,
    CastCommitEventData,
    AttackEventData,
    HitEventData,
    DamageResultEventData,
    HealRequestEventData,
    HealResultEventData,
    CastAggregateEventData,
    ShieldBreakEventData,
    DeathEventData>;

struct EffectEventHeader
{
    int frame{};
    std::uint64_t eventOrdinal{};
    EffectSourceBinding binding;
    const EffectUnitSnapshot* owner = nullptr;
    BattleEffectReadView battle;
    EffectFormulaInputs formulaInputs;
};

struct EffectEventContext
{
    EffectEvent event{};
    EffectEventHeader header;
    EffectEventPayload payload;
};

struct EffectCommandMetadata
{
    EffectSourceBinding binding;
    EffectRuleId ruleId;
    EffectEvent event{};
    std::uint32_t ruleOrder{};
    std::uint32_t actionOrder{};
    std::uint32_t targetOrder{};
    std::uint64_t commandOrdinal{};
    int targetUnitId = -1;
    int eventSourceUnitId = -1;
};

struct ModifyAttributeEffectCommand
{
    ModifyAttributeAction action;
    int amount{};
};

struct ModifyDamageEffectCommand
{
    ModifyDamageAction action;
    int amount{};
};

struct ChangeResourceEffectCommand
{
    ChangeResourceAction action;
    int amount{};
    std::vector<int> transferDestinationUnitIds;
};

struct ModifyHealTransactionEffectCommand
{
    ModifyHealTransactionAction action;
};

struct ApplyStatusEffectCommand
{
    ApplyStatusAction action;
    int potency{};
    int secondaryPotency{};
    std::optional<int> evaluatedDurationFrames;
};

template <typename Evaluator>
std::pair<int, int> evaluateStatusRuntimeValues(
    const ApplyStatusAction& action,
    Evaluator&& evaluate)
{
    return std::visit([&](const auto& effects) -> std::pair<int, int>
    {
        using T = std::decay_t<decltype(effects)>;
        if constexpr (std::is_same_v<T, PoisonStatusEffects>)
            return { evaluate(effects.currentHpDamagePercent), 0 };
        else if constexpr (std::is_same_v<T, BleedStatusEffects>)
            return { evaluate(effects.maxHpDamagePercent), 0 };
        else if constexpr (std::is_same_v<T, ColdPoisonStatusEffects>)
            return { evaluate(effects.speedReductionPercent), 0 };
        else if constexpr (std::is_same_v<T, WitheredBoneStatusEffects>)
            return {
                evaluate(effects.damageTakenIncreasePercent),
                evaluate(effects.healingReductionPercent),
            };
        else if constexpr (std::is_same_v<T, NeutralizeForceStatusEffects>)
            return { evaluate(effects.originalTargetShield), 0 };
        else if constexpr (std::is_same_v<T, SingleHitCapStatusEffects>)
            return { evaluate(effects.damageCap), 0 };
        else if constexpr (std::is_same_v<T, BattleSpiritStatusEffects>)
            return {
                evaluate(effects.skillDamageIncreasePercent),
                evaluate(effects.damageReductionPercent),
            };
        else if constexpr (std::is_same_v<T, TrueQiStatusEffects>)
            return { evaluate(effects.pureDamagePerHit), 0 };
        else if constexpr (std::is_same_v<T, PoisonExplosionStatusEffects>)
            return { evaluate(effects.deathPureDamage), 0 };
        else
            return { 0, 0 };
    }, action.effects);
}

struct ConsumeStatusEffectCommand
{
    ConsumeStatusAction action;
    std::optional<ApplyStatusEffectCommand> whenDepleted;
};

struct RemoveStatusEffectCommand
{
    RemoveStatusAction action;
};

struct DealDamageEffectCommand
{
    DealDamageAction action;
    int amount{};
    int transactionCount = 1;
};

struct ResolvedEffectAttackSource
{
    int unitId = -1;
    Pointf position;
};

struct ModifyAttackEffectCommand
{
    ModifyAttackAction action;
    std::optional<int> damageOverride;
    std::optional<ResolvedEffectAttackSource> source;
};

struct ForceMoveEffectCommand
{
    ForceMoveAction action;
};

struct CreateAreaEffectCommand
{
    CreateAreaAction action;
    std::vector<int> modifierAmounts;
};

struct ModifyCastEffectCommand
{
    ModifyCastAction action;
    std::optional<int> mpCost;
};

struct StateMachineEffectCommand
{
    StateMachineAction action;
    std::vector<int> selectedSourceUnitIds;
    std::int64_t stateValueBefore{};
    std::int64_t stateValueAfter{};
    std::int64_t outputValue{};
};

using EffectCommandValue = std::variant<
    ModifyAttributeEffectCommand,
    ModifyDamageEffectCommand,
    ChangeResourceEffectCommand,
    ModifyHealTransactionEffectCommand,
    ApplyStatusEffectCommand,
    ConsumeStatusEffectCommand,
    RemoveStatusEffectCommand,
    DealDamageEffectCommand,
    ModifyAttackEffectCommand,
    ForceMoveEffectCommand,
    CreateAreaEffectCommand,
    ModifyCastEffectCommand,
    StateMachineEffectCommand>;

struct EffectCommand
{
    EffectCommandMetadata metadata;
    EffectCommandValue value;
};

struct BoundEffectRule
{
    EffectSourceBinding binding;
    EffectRule rule;
    std::uint32_t order{};
    std::optional<BattleCastId> castScope;
    CastPropagationPolicy scopedPropagation = CastPropagationPolicy::SourceRules;
};

struct EffectRuleRuntimeKey
{
    int ownerUnitId = -1;
    EffectSourceKind sourceKind{};
    int sourceId{};
    std::uint64_t sourceInstanceId{};
    EffectRuleId ruleId;

    auto operator<=>(const EffectRuleRuntimeKey&) const = default;
};

struct EffectRuleRuntimeState
{
    int activationCount{};
    int eligibleEventCount{};
    int intervalFramesRemaining{};
};

struct EffectSharedCooldownKey
{
    EffectSourceKind sourceKind{};
    int sourceId{};
    int sourceTeam{};
    std::uint64_t sourceInstanceId{};
    EffectRuleId ruleId;

    auto operator<=>(const EffectSharedCooldownKey&) const = default;
};

struct EffectActivationScopeKey
{
    EffectRuleRuntimeKey rule;
    BattleCastId castId;
    int targetUnitId = -1;

    auto operator<=>(const EffectActivationScopeKey&) const = default;
};

struct EffectStateKey
{
    int ownerUnitId = -1;
    EffectSourceKind sourceKind{};
    int sourceId{};
    std::uint64_t sourceInstanceId{};
    EffectStateSlot slot{};
    std::uint64_t scopeId{};

    auto operator<=>(const EffectStateKey&) const = default;
};

class BattleEffectRuleStore
{
public:
    void clear();
    std::size_t append(EffectSourceBinding binding, const EffectRule& rule);
    void append(EffectSourceBinding binding, std::span<const EffectRule> rules);
    void appendClonedOwnerRules(int sourceOwnerUnitId,
                                int cloneOwnerUnitId,
                                int cloneTeam);
    void appendAntiComboTransferredRules(int sourceOwnerUnitId,
                                         int targetOwnerUnitId,
                                         int targetTeam,
                                         int comboId);

    std::span<const BoundEffectRule> rules() const;
    std::vector<std::size_t> bindBorrowedUltimateRules(
        BattleCastId castId,
        int ownerUnitId,
        int ownerTeam,
        std::span<const int> sourceUnitIds,
        const BorrowedRuleFilter& filter,
        CastPropagationPolicy propagation);
    void removeCastScopedRules(BattleCastId castId);
    std::size_t castScopedRuleCount(BattleCastId castId) const;
    const EffectRuleRuntimeState& runtime(const EffectSourceBinding& binding,
                                          EffectRuleId ruleId) const;
    int activationCount(const EffectSourceBinding& binding, EffectRuleId ruleId) const;
    int activationEvaluationCount(const EffectSourceBinding& binding,
                                  EffectRuleId ruleId,
                                  BattleCastId castId,
                                  int targetUnitId) const;
    bool canActivateRuntimeRule(const EffectSourceBinding& binding,
                                EffectRuleId ruleId,
                                int frame) const;
    bool tryActivateRuntimeRule(const EffectSourceBinding& binding,
                                EffectRuleId ruleId,
                                int frame,
                                BattleRuntimeRandom& random);
    void recordRuntimeRuleActivation(const EffectSourceBinding& binding,
                                     EffectRuleId ruleId,
                                     int frame);
    bool blinkAttackUsesWeakestTarget(int ownerUnitId) const;
    void advanceBlinkAttackTargetMode(int ownerUnitId);
    std::int64_t stateValue(const EffectSourceBinding& binding,
                            EffectStateSlot slot,
                            std::uint64_t scopeId = 0) const;
    void setStateValue(const EffectSourceBinding& binding,
                       EffectStateSlot slot,
                       std::int64_t value,
                       std::uint64_t scopeId = 0);

private:
    static constexpr std::size_t EventCount =
        static_cast<std::size_t>(EffectEvent::AllyDied) + 1;

    std::vector<BoundEffectRule> rules_;
    std::array<std::vector<std::size_t>, EventCount> ruleIndicesByEvent_;
    std::map<EffectRuleRuntimeKey, EffectRuleRuntimeState> runtimeByRule_;
    std::map<EffectSharedCooldownKey, std::int64_t> sharedCooldownUntilFrame_;
    std::map<EffectActivationScopeKey, int> activationEvaluations_;
    std::map<EffectStateKey, std::int64_t> stateValues_;
    std::map<int, bool> blinkAttackWeakestTargetByOwner_;
    std::uint64_t nextRuntimeInstanceId_ = 1;

    void rebuildEventIndices();

    friend class BattleEffectSystem;
};

struct EffectRuleActivation
{
    EffectSourceBinding binding;
    EffectRuleId ruleId;
    std::vector<int> targetUnitIds;
};

// Timing-sensitive actions are consumed by their owning battle system at the
// exact runtime phase where the required live inputs exist.  Matching remains
// centralized here so those consumers do not reimplement effect eligibility.
// The bound pointer remains valid until the rule store is mutated.
struct EffectExactRuntimeRuleMatch
{
    const BoundEffectRule* bound{};
    std::vector<int> targetUnitIds;
};

struct BattleEffectDispatchResult
{
    std::vector<EffectRuleActivation> activations;
    std::vector<EffectCommand> commands;
};

class BattleEffectSystem
{
public:
    std::vector<EffectExactRuntimeRuleMatch> queryExactRuntimeRules(
        const BattleEffectRuleStore& store,
        const EffectEventContext& context,
        BattleRuntimeRandom& random) const;
    BattleEffectDispatchResult dispatch(BattleEffectRuleStore& store,
                                        const EffectEventContext& context,
                                        BattleRuntimeRandom& random) const;
    BattleEffectDispatchResult dispatchRuleIndices(
        BattleEffectRuleStore& store,
        const EffectEventContext& context,
        BattleRuntimeRandom& random,
        std::span<const std::size_t> ruleIndices) const;

    static bool eventPayloadMatches(const EffectEventContext& context);
    static int evaluateNumber(const EffectNumber& number,
                              const EffectEventContext& context,
                              const EffectUnitSnapshot& target);
    static std::vector<int> selectTargets(const EffectSelector& selector,
                                          const EffectEventContext& context,
                                          BattleRuntimeRandom& random,
                                          const std::function<bool(
                                              const EffectUnitSnapshot&)>& candidateFilter = {});
};

}  // namespace KysChess::Battle
