#pragma once

#include "../ChessBattleEffectSemantics.h"
#include "../ChessBattleEffectValidation.h"
#include "../Point.h"
#include "BattleCastLifecycle.h"
#include "BattleStatusSystem.h"
#include "BattleHealSystem.h"
#include "BattleAreaEffectSystem.h"

#include <array>
#include <cassert>
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

struct BattleStatusEffectState;

class BattleRuntimeRandom;

int effectSourcePrecedence(EffectSourceKind kind);
bool effectSourceRuleOrderLess(
    EffectSourceKind lhsSource,
    std::uint32_t lhsOrder,
    EffectSourceKind rhsSource,
    std::uint32_t rhsOrder);
bool effectHealKindMatchesLabels(
    BattleHealKind kind,
    std::span<const std::string> labels);

struct EffectStatusSnapshot
{
    BattleStatusKind state{};
    int sourceUnitId = -1;
    std::optional<EffectSourceBinding> producerBinding;
    std::uint64_t appliedSequence{};
    int stacks = 1;
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
    int stackCount(
        BattleStatusKind stack,
        const StatusContributionFilter& filter = {}) const;
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

struct EffectStatusContributionContext
{
    int holderUnitId = -1;
    int sourceUnitId = -1;
    BattleStatusKind kind{};
    int quantity{};
    std::uint64_t appliedSequence{};
    EffectRuleId producerRuleId;
    std::uint32_t producerRuleOrder{};
    std::uint32_t producerActionOrder{};
    std::uint32_t behaviorRuleOrder{};

    bool operator==(const EffectStatusContributionContext&) const = default;
};

StatusContributionFilter resolveStatusContributionFilter(
    StatusSourceMatch match,
    const EffectSourceBinding& binding,
    const std::optional<EffectStatusContributionContext>& contribution);

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
    EffectSourceBinding binding;
    EffectStatusContributionContext contribution;
    std::uint32_t behaviorActionOrder{};
    std::optional<BattleCastProvenance> triggeringCast;
    std::optional<BattleAttackProvenance> triggeringAttack;
};

struct EffectRuleDamageOrigin
{
    EffectRuleId ruleId;
    EffectSourceBinding binding;
    std::uint32_t actionOrder{};
    std::optional<BattleCastProvenance> triggeringCast;
    std::optional<BattleAttackProvenance> triggeringAttack;
};

struct EffectEnvironmentDamageOrigin {};

using EffectDamageOrigin = std::variant<
    EffectAttackDamageOrigin,
    EffectStatusDamageOrigin,
    EffectRuleDamageOrigin,
    EffectEnvironmentDamageOrigin>;

EffectDamageOrigin makeEffectDamageOrigin(
    EffectSourceBinding binding,
    EffectRuleId ruleId,
    std::uint32_t authoredActionOrder,
    std::optional<EffectStatusContributionContext> statusContribution,
    std::optional<BattleCastProvenance> triggeringCast,
    std::optional<BattleAttackProvenance> triggeringAttack);
const BattleCastProvenance* effectDamageCastProvenance(
    const EffectDamageOrigin& origin);
const BattleAttackProvenance* effectDamageAttackProvenance(
    const EffectDamageOrigin& origin);

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
    std::optional<BattleCastProvenance> cast;
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
    std::optional<EffectStatusContributionContext> statusContribution;
    // 事件可攜帶已結算的施放來源，但不能替已退役的施放保留工作。
    bool retainCastUntilDamageDescendants = true;
    std::optional<int> executionFrame;
};

enum class EffectExecutionLane
{
    Configured,
    StatusBehavior,
};

struct EffectEventData
{
    EffectEvent event{};
    EffectEventHeader header;
    EffectEventPayload payload;
};

struct EffectEvaluationScope
{
    EffectSourceBinding binding;
    const EffectUnitSnapshot* owner{};
    EffectFormulaInputs formulaInputs;
    std::optional<EffectStatusContributionContext> statusContribution;
    std::optional<BattleCastProvenance> cast;
    std::optional<BattleAttackProvenance> attack;
};

// 借用不可變事件；只有規則的觀察者、公式輸入與有效來源可被覆寫。
struct EffectEventContext
{
    EffectEventContext(const EffectEventData& data)
        : event(data.event), header(data.header), payload(data.payload)
        , scope{ data.header.binding, data.header.owner,
                 data.header.formulaInputs, data.header.statusContribution }
    {
    }
    const EffectEvent& event;
    const EffectEventHeader& header;
    const EffectEventPayload& payload;
    EffectEvaluationScope scope;
};

const BattleCastProvenance* effectEventCastProvenance(const EffectEventPayload& payload);
const BattleAttackProvenance* effectEventAttackProvenance(const EffectEventPayload& payload);
const BattleCastProvenance* effectCastProvenance(const EffectEventContext& context);
const BattleAttackProvenance* effectAttackProvenance(const EffectEventContext& context);

// 只有已接受的接觸可授予命中傷害記帳；觸發來源本身不代表命中。
struct EffectHitDamageCredit
{
    BattleAttackProvenance provenance;
    int targetUnitId{};
};

// 求值後由命令持有；來源事件的 lineage 與規則 binding 各自保留原本語義。
struct EffectExecutionInputs
{
    int frame{};
    std::optional<BattleCastProvenance> cast;
    std::optional<BattleAttackProvenance> attack;
    std::optional<EffectHitDamageCredit> hitDamageCredit;
    bool retainCastUntilDamageDescendants = true;
    BattleHealModifierState healModifiers;
    int controlLowHpImmunityPct = 25;
    bool bypassStatusShield = false;
};

struct EffectCommandMetadata
{
    EffectSourceBinding binding;
    EffectRuleId ruleId;
    EffectEvent event{};
    std::uint32_t ruleOrder{};
    // Stable identity of the authored action within the rule. Unlike
    // actionOrder, this value does not expand when the rule repeats.
    std::uint32_t authoredActionOrder{};
    std::uint32_t actionOrder{};
    std::uint32_t targetOrder{};
    std::uint64_t commandOrdinal{};
    int targetUnitId = -1;
    int eventSourceUnitId = -1;
    EffectExecutionLane executionLane = EffectExecutionLane::Configured;
    std::uint32_t producerActionOrder{};
    std::uint32_t behaviorRuleOrder{};
    std::optional<EffectStatusContributionContext> statusContribution;
};

// One structured order key is shared by configured rules, status-owned
// behavior commands, and status interceptors.  The explicit source precedence
// deliberately does not depend on EffectSourceKind's enum ordinal.
struct EffectExecutionOrderKey
{
    int sourcePrecedence{};
    std::uint32_t producerRuleOrder{};
    EffectExecutionLane lane = EffectExecutionLane::Configured;
    std::uint32_t producerActionOrder{};
    std::uint32_t behaviorRuleOrder{};
    int holderUnitId = -1;
    std::uint64_t contributionSequence{};
    std::uint32_t actionOrder{};
    std::uint32_t targetOrder{};
    std::uint64_t commandOrdinal{};

    auto operator<=>(const EffectExecutionOrderKey&) const = default;
};

EffectExecutionOrderKey effectExecutionOrderKey(
    const EffectCommandMetadata& metadata);
EffectExecutionOrderKey statusBehaviorExecutionOrderKey(
    const EffectSourceBinding& binding,
    std::uint32_t producerRuleOrder,
    std::uint32_t producerActionOrder,
    std::uint32_t behaviorRuleOrder,
    int holderUnitId,
    std::uint64_t contributionSequence,
    std::uint32_t actionOrder);

struct ModifyAttributeEffectCommand
{
    BattleAttribute attribute{};
    int amount{};
    AttributeOperation operation{};
    int durationFrames = 0;
    EffectStackPolicy stack = EffectStackPolicy::Independent;
    std::optional<int> stackLimit;
    EffectStackScope stackScope = EffectStackScope::Shared;
};

ModifyAttributeEffectCommand prepareModifyAttribute(
    const ModifyAttributeAction& action, int amount);

struct ModifyDamageEffectCommand
{
    DamageModifierPerspective perspective = DamageModifierPerspective::Outgoing;
    DamageModifierStage stage{};
    DamageChannel channel{};
    int amount{};
    DamageModifierOperation operation{};
    int durationFrames = 0;
    EffectStackPolicy stack = EffectStackPolicy::Independent;
    std::optional<int> stackLimit;
    EffectStackScope stackScope = EffectStackScope::Shared;
};

ModifyDamageEffectCommand prepareModifyDamage(
    const ModifyDamageAction& action, int amount);

// 初始化資源須在基礎屬性完成後才求值；與一般命令的固定數值互斥。
struct InitializationResourceAmount
{
    EffectNumber formula;
    std::optional<EffectNumber> additionalFormula;
};

using ResourceEffectAmount = std::variant<int, InitializationResourceAmount>;

struct ChangeResourceEffectCommand
{
    BattleResource resource{};
    ResourceEffectAmount amount{};
    ResourceChangeKind kind{};
    EffectHealKind healKind = EffectHealKind::Direct;
    EffectHealSourcePolicy healSourcePolicy = EffectHealSourcePolicy::RequireAlive;
    bool healRequiresFullMp = false;
    std::vector<int> transferDestinationUnitIds;

    int resolvedAmount() const { return std::get<int>(amount); }
};

ChangeResourceEffectCommand prepareChangeResource(
    const ChangeResourceAction& action, ResourceEffectAmount amount,
    std::vector<int> destinations = {});

struct ModifyHealTransactionEffectCommand
{
    ModifyHealTransactionAction action;
};

struct ApplyStatusEffectCommand
{
    BattleStatusKind status{};
    int durationFrames{};
    int stacks{};
    EffectStackPolicy stack{};
    std::optional<int> stackLimit;
    std::optional<int> targetTotalLimit;
    std::shared_ptr<const StatusBehaviorDefinition> behavior;
    PoisonSameEventMerge poisonSameEventMerge{};
};

ApplyStatusEffectCommand prepareStatusApplication(
    const ApplyStatusAction& action,
    int durationFrames,
    std::shared_ptr<const StatusBehaviorDefinition> behavior);

struct ConsumeStatusEffectCommand
{
    BattleStatusConsumeRequest request;
    std::optional<ApplyStatusEffectCommand> whenDepleted;
};

struct ConsumeThisStatusEffectCommand
{
    BattleStatusConsumeRequest request;
    std::optional<ApplyStatusEffectCommand> whenDepleted;
};

struct RemoveStatusEffectCommand
{
    BattleStatusRemoveRequest request;
};

RemoveStatusEffectCommand prepareStatusRemoval(
    const RemoveStatusAction& action, const EffectCommandMetadata& metadata);

struct EffectDamageDelivery
{
    DamageArea area;
    PerCastHitPolicy perCast;
    std::optional<AreaProjectileDamageDelivery> areaProjectiles;
    int projectileSourceMaxHpPercent{};
    std::optional<int> displayedSourceMaxHpPercent;
    bool statusTickPresentation = true;
    std::optional<std::vector<int>> targetUnitIds;
};

struct DealDamageEffectCommand
{
    int amount{};
    int transactionCount = 1;
    BattleDamageKind kind{};
    bool appliesDamageModifiers = true;
    bool triggersHurtInvincibility = true;
    bool canExecute{};
    bool applyDefenderTypedStatuses{};
    EffectDamageDelivery delivery;
};

DealDamageEffectCommand prepareDealDamage(
    const DealDamageAction& action, int amount, int transactionCount = 1);

struct SuppressCurrentCastContactsEffectCommand
{
    int originalTargetShield{};
};

struct MakeIncomingAttackMissEffectCommand {};

struct ResolvedEffectAttackSource
{
    int unitId = -1;
    Pointf position;
    int attack{};
};

struct ModifyAttackEffectCommand
{
    AttackPattern pattern;
    int strengthPct = 100;
    std::optional<bool> through;
    std::optional<bool> tracking;
    bool mainProjectile = true;
    int sameTargetHitLimit = 0;
    int projectileClearRadiusPct = 0;
    AttackTargetPolicy targets = AttackTargetPolicy::Preserve;
    CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules;
    bool addToBaseAttack = false;
    std::optional<ResolvedEffectAttackSource> source;
    std::optional<int> damageOverride;
    std::optional<BattleDamageKind> damageKind;
    AttackRuntimeBehavior runtimeBehavior;
    std::optional<IndependentProjectile> independentProjectile;
    std::string activationLog;
};

ModifyAttackEffectCommand prepareModifyAttack(
    const ModifyAttackAction& action, std::optional<int> damageOverride = {},
    std::optional<ResolvedEffectAttackSource> source = {});

struct ForceMoveEffectCommand
{
    ForceMoveAction action;
};

struct CreateAreaEffectCommand
{
    BattleAreaCreateRequest request;
};

struct ModifyCastEffectCommand
{
    std::optional<int> mpCost;
    std::optional<CastRangeMode> rangeMode;
    int projectileSpeedPct = 0;
    int minimumSelectDistance = 0;
    int additionalProjectiles = 0;
    CastMobilityPolicy mobility = CastMobilityPolicy::Preserve;
    std::optional<AutoUltimateCastRequest> autoUltimate;
    std::optional<AttackPattern> replacementPattern;
    bool freeAdditionalCast = false;
    CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules;
};

ModifyCastEffectCommand prepareModifyCast(
    const ModifyCastAction& action, std::optional<int> mpCost = {});

// 保留已完成求值的命令位置，維持排序、借用規則及 reduction 回執的邊界。
struct EvaluatedStateEffectCommand {};

struct StateDamageEffectCommand
{
    int amount{};
    BattleDamageKind kind{};
    std::vector<int> targetUnitIds;
};

struct BorrowEffectRulesCommand
{
    std::vector<int> sourceUnitIds;
    BorrowedRuleFilter filter;
    CastPropagationPolicy propagation{};
};

struct CopyAttackDefinitionCommand
{
    std::vector<int> sourceUnitIds;
    CastPropagationPolicy propagation{};
};

using StateMachineExecution = std::variant<
    EvaluatedStateEffectCommand,
    StateDamageEffectCommand,
    StartDamageAbsorptionAction,
    BorrowEffectRulesCommand,
    CopyAttackDefinitionCommand,
    SettleRemainingStatusDamageAction,
    GenerateClonesAction,
    PreventDeathAction,
    ConfigureRescueRepositionAction>;

struct StateMachineEffectCommand
{
    StateMachineExecution value;
};

using EffectCommandValue = std::variant<
    ModifyAttributeEffectCommand,
    ModifyDamageEffectCommand,
    ChangeResourceEffectCommand,
    ModifyHealTransactionEffectCommand,
    ApplyStatusEffectCommand,
    ConsumeStatusEffectCommand,
    ConsumeThisStatusEffectCommand,
    RemoveStatusEffectCommand,
    DealDamageEffectCommand,
    SuppressCurrentCastContactsEffectCommand,
    MakeIncomingAttackMissEffectCommand,
    ModifyAttackEffectCommand,
    ForceMoveEffectCommand,
    CreateAreaEffectCommand,
    ModifyCastEffectCommand,
    StateMachineEffectCommand>;

struct EffectCommand
{
    EffectCommandMetadata metadata;
    EffectCommandValue value;
    EffectExecutionInputs execution;
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
    std::size_t append(
        EffectSourceBinding binding,
        const EffectRule& rule,
        EffectRuleAuthoringContext context = EffectRuleAuthoringContext::Configured);
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
        static_cast<std::size_t>(EffectEvent::StatusPersistent) + 1;

    std::vector<BoundEffectRule> rules_;
    std::array<std::vector<std::size_t>, EventCount> ruleIndicesByEvent_;
    std::map<EffectRuleRuntimeKey, EffectRuleRuntimeState> runtimeByRule_;
    std::map<EffectStateKey, std::int64_t> stateValues_;
    std::map<int, bool> blinkAttackWeakestTargetByOwner_;
    std::uint64_t nextRuntimeInstanceId_ = 1;
    std::uint32_t nextRuleOrder_{};

    std::uint32_t allocateRuleOrder();
    void rebuildEventIndices();

    friend class BattleEffectSystem;
};

struct EffectRuleActivation
{
    EffectSourceBinding binding;
    EffectRuleId ruleId;
    std::vector<int> targetUnitIds;
};

struct ActiveStatusBehaviorView
{
    EffectSourceBinding binding;
    EffectRuleId producerRuleId;
    std::uint32_t producerRuleOrder{};
    std::uint32_t producerActionOrder{};
    int holderUnitId = -1;
    int sourceUnitId = -1;
    BattleStatusKind kind{};
    int quantity{};
    std::uint64_t appliedSequence{};
    std::shared_ptr<const StatusBehaviorDefinition> behavior;
    std::vector<EffectRuleRuntimeState>* runtime = nullptr;
};

enum class StatusBehaviorDispatchFilter
{
    All,
    AttackInterceptorsOnly,
    OutgoingCastSuppressorsOnly,
    IncomingAttackMissOnly,
    ExcludeAttackInterceptors,
};

// 需要狀態存活判定的 dispatch 必須提供共用的 reducer-backed prediction。
struct StatusBehaviorDispatchLiveness
{
    std::function<std::optional<int>(
        int holderUnitId,
        std::uint64_t appliedSequence,
        BattleStatusKind kind)> contributionQuantity;
    std::function<void(std::span<const EffectCommand>)> reduceRuleCommands;
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
    // 只排除事件或擁有者不可能匹配的規則；完整條件仍由實際查詢判定。
    bool hasExactRuntimeRuleCandidates(
        const BattleEffectRuleStore& store,
        EffectEvent event,
        int ownerUnitId) const;
    bool hasInvincibilityPiercingExecuteRule(
        const BattleEffectRuleStore& store,
        const EffectEventContext& context) const;
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
        std::span<const std::size_t> ruleIndices,
        bool finalizeEvent = true) const;
    BattleEffectDispatchResult dispatchMerged(
        BattleEffectRuleStore& store,
        const EffectEventContext& context,
        BattleRuntimeRandom& random,
        std::span<const ActiveStatusBehaviorView> behaviors,
        StatusBehaviorDispatchFilter filter = StatusBehaviorDispatchFilter::All,
        bool includeAllFrameOwners = false,
        const StatusBehaviorDispatchLiveness* reducerLiveness = nullptr) const;
    BattleEffectDispatchResult dispatchStatusBehaviors(
        const EffectEventContext& context,
        BattleRuntimeRandom& random,
        std::span<const ActiveStatusBehaviorView> behaviors,
        StatusBehaviorDispatchFilter filter = StatusBehaviorDispatchFilter::All,
        const StatusBehaviorDispatchLiveness* reducerLiveness = nullptr) const;

    static bool eventPayloadMatches(const EffectEventContext& context);
    static bool statusBehaviorRuleMatchesEvent(
        const EffectRule& rule,
        EffectEvent event,
        StatusBehaviorDispatchFilter filter);
    static int evaluateNumber(const EffectNumber& number,
                              const EffectEventContext& context,
                              const EffectUnitSnapshot& target);
    static std::vector<int> selectTargets(const EffectSelector& selector,
                                          const EffectEventContext& context,
                                          BattleRuntimeRandom& random,
                                          const std::function<bool(
                                              const EffectUnitSnapshot&)>& candidateFilter = {});

private:
    void finalizeEventDispatch(
        BattleEffectRuleStore& store,
        const EffectEventContext& context,
        BattleEffectDispatchResult& result) const;
};

}  // namespace KysChess::Battle
