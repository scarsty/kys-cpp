#pragma once

#include "BattleAreaEffectSystem.h"
#include "BattleDamageSystem.h"
#include "BattleEffectSystem.h"
#include "BattleHealSystem.h"
#include "BattleStatusSystem.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeState;

struct BattleAttributeModifierInstance
{
    std::uint64_t sequence{};
    std::uint64_t negativeEffectSequence{};
    EffectSourceBinding binding;
    EffectRuleId ruleId;
    std::uint32_t actionOrder{};
    int targetUnitId = -1;
    BattleAttribute attribute{};
    AttributeOperation operation{};
    int amount{};
    int appliedFrame{};
    std::optional<std::int64_t> expiresFrameExclusive;
    EffectStackPolicy stack{};
    std::optional<int> stackLimit;
    int stackCount = 1;
    int eventSourceUnitId = -1;
    bool negative{};
};

struct BattleDamageModifierInstance
{
    std::uint64_t sequence{};
    std::uint64_t negativeEffectSequence{};
    EffectSourceBinding binding;
    EffectRuleId ruleId;
    std::uint32_t actionOrder{};
    int targetUnitId = -1;
    int eventSourceUnitId = -1;
    DamageModifierPerspective perspective = DamageModifierPerspective::Outgoing;
    DamageModifierStage stage{};
    DamageChannel channel{};
    DamageModifierOperation operation{};
    int amount{};
    int appliedFrame{};
    std::optional<std::int64_t> expiresFrameExclusive;
    EffectStackPolicy stack{};
    std::optional<int> stackLimit;
    int stackCount = 1;
    bool negative{};
};

struct BattleDamageAbsorptionInstance
{
    std::uint64_t sequence{};
    EffectSourceBinding binding;
    EffectRuleId ruleId;
    std::uint32_t actionOrder{};
    std::uint32_t authoredActionOrder{};
    std::optional<EffectStatusContributionContext> statusContribution;
    std::optional<BattleCastProvenance> triggeringCast;
    std::optional<BattleAttackProvenance> triggeringAttack;
    int targetUnitId = -1;
    EffectStateSlot slot{};
    int absorbedPct{};
    int appliedFrame{};
    std::int64_t expiresFrameExclusive{};
    bool settleOnSourceDeath = false;
    EffectSelector settlementTarget;
    BattleDamageKind settlementDamageKind = BattleDamageKind::Pure;
    int returnedPct = 100;
    std::int64_t accumulatedDamage{};
};

struct BattleAntiComboAttributeKey
{
    int unitId = -1;
    BattleAttribute attribute{};

    auto operator<=>(const BattleAntiComboAttributeKey&) const = default;
};

struct BattleAntiComboAttributeBasis
{
    int baseValue{};
    int flatTotal{};
    int percentTotal{};
};

using BattleAntiComboInitializationValue = std::variant<
    ModifyAttributeEffectCommand,
    ModifyDamageEffectCommand,
    ChangeResourceEffectCommand,
    ApplyStatusEffectCommand>;

struct BattleAntiComboInitializationRecord
{
    EffectCommandMetadata metadata;
    BattleAntiComboInitializationValue value;
};

struct BattleAntiComboCoreAttributeDelta
{
    BattleAttribute attribute{};
    int delta{};
};

struct BattleAntiComboTransfer
{
    std::vector<BattleAntiComboCoreAttributeDelta> coreAttributeDeltas;
    std::vector<EffectCommand> commands;
};

struct BattleEffectCommandRuntimeState
{
    std::uint64_t nextAttributeSequence = 1;
    std::uint64_t nextDamageSequence = 1;
    std::uint64_t nextDamageAbsorptionSequence = 1;
    std::vector<BattleAttributeModifierInstance> attributeModifiers;
    std::vector<BattleDamageModifierInstance> damageModifiers;
    std::vector<BattleDamageAbsorptionInstance> damageAbsorptions;
    std::map<BattleAntiComboAttributeKey, BattleAntiComboAttributeBasis>
        antiComboAttributeBases;
    std::vector<BattleAntiComboInitializationRecord> antiComboInitializationRecords;
};

enum class BattleModifierApplyOutcome
{
    Applied,
    Refreshed,
    Replaced,
    KeptStronger,
    StackChanged,
    BlockedByStatusShield,
};

using BattleAttributeModifierApplyOutcome = BattleModifierApplyOutcome;
using BattleDamageModifierApplyOutcome = BattleModifierApplyOutcome;
using BattleDamageAbsorptionApplyOutcome = BattleModifierApplyOutcome;

struct BattleAttributeQuery
{
    int unitId = -1;
    BattleAttribute attribute{};
    int baseValue{};
    int frame{};
    int eventSourceUnitId = -1;
};

struct BattleAttributeEffectResult
{
    BattleAttributeModifierApplyOutcome outcome{};
    BattleAttributeModifierInstance modifier;
    int requestedDurationFrames{};
    int appliedDurationFrames{};
    int statusShieldAbsorbed{};
    bool applied{};
};

struct BattleDamageModifierQuery
{
    int unitId = -1;
    int eventSourceUnitId = -1;
    DamageModifierPerspective perspective = DamageModifierPerspective::Outgoing;
    DamageChannel channel{};
    DamageModifierStage stage{};
    int frame{};
};

struct BattleDamageModifierEffectResult
{
    BattleDamageModifierApplyOutcome outcome{};
    BattleDamageModifierInstance modifier;
    int requestedDurationFrames{};
    int appliedDurationFrames{};
    int statusShieldAbsorbed{};
    bool applied{};
};

struct BattleDamageAbsorptionEffectResult
{
    BattleDamageAbsorptionApplyOutcome outcome{};
    BattleDamageAbsorptionInstance absorption;
};

enum class BattleResourceEffectOutcome
{
    Applied,
    NoChange,
    IneligibleTarget,
};

struct BattleResourceDelta
{
    int unitId = -1;
    BattleResource resource{};
    int before{};
    int after{};
};

struct BattleResourceEffectResult
{
    BattleResourceEffectOutcome outcome{};
    std::vector<BattleResourceDelta> deltas;
    std::optional<BattleHealResult> heal;
};

struct BattleStatusApplyEffectResult
{
    BattleStatusApplyResult status;
};

struct BattleStatusConsumeEffectResult
{
    BattleStatusConsumeResult status;
    std::optional<BattleStatusApplyResult> depletedStatus;
};

struct BattleStatusRemoveEffectResult
{
    BattleStatusRemoveResult status;
    std::vector<BattleAttributeModifierInstance> removedAttributeModifiers;
    std::vector<BattleDamageModifierInstance> removedDamageModifiers;
};

struct BattleAreaEffectResult
{
    BattleAreaCreateResult area;
};

struct BattleEffectDamageRequestOutput
{
    BattleDamageRequest request;
    EffectDamageDelivery delivery;
    EffectSourceBinding source;
    EffectRuleId ruleId;
    std::optional<BattleCastProvenance> triggeringCast;
    std::optional<BattleAttackProvenance> triggeringAttack;
    std::optional<EffectHitDamageCredit> hitDamageCredit;
    std::optional<EffectStatusContributionContext> statusContribution;
    std::uint32_t authoredActionOrder{};
    int transactionCount = 1;
    int eventSourceUnitId = -1;
};

struct BattleSkippedEffectResult {};

template<class Command>
struct BattleRoutedEffectCommand
{
    Command command;
};

using BattleEffectReductionValue = std::variant<
    BattleSkippedEffectResult,
    BattleAttributeEffectResult,
    BattleDamageModifierEffectResult,
    BattleDamageAbsorptionEffectResult,
    BattleResourceEffectResult,
    BattleStatusApplyEffectResult,
    BattleStatusConsumeEffectResult,
    BattleStatusRemoveEffectResult,
    BattleAreaEffectResult,
    BattleEffectDamageRequestOutput,
    BattleRoutedEffectCommand<ModifyDamageEffectCommand>,
    BattleRoutedEffectCommand<ModifyHealTransactionEffectCommand>,
    BattleRoutedEffectCommand<SuppressCurrentCastContactsEffectCommand>,
    BattleRoutedEffectCommand<MakeIncomingAttackMissEffectCommand>,
    BattleRoutedEffectCommand<ModifyAttackEffectCommand>,
    BattleRoutedEffectCommand<ForceMoveEffectCommand>,
    BattleRoutedEffectCommand<ModifyCastEffectCommand>>;

struct BattleEffectReductionEntry
{
    std::size_t inputOrder{};
    EffectCommandMetadata metadata;
    BattleEffectReductionValue value;
};

struct BattleEffectCommandReduction
{
    std::vector<BattleEffectReductionEntry> entries;
};

class BattleEffectCommandSystem
{
public:
    static std::optional<int> contributionQuantity(
        const BattleStatusEffectState& effects, std::uint64_t sequence, BattleStatusKind kind);
    static bool actionMayAffectStatusLiveness(const EffectAction& action);
    static BattleRuntimeState copyDispatchState(const BattleRuntimeState& source);
    static BattleEffectDamageRequestOutput prepareDamageOutput(
        const EffectCommandMetadata& metadata,
        const DealDamageEffectCommand& command,
        const EffectExecutionInputs& inputs);
    static EffectDamageOrigin damageOrigin(
        const BattleEffectDamageRequestOutput& output);
    static BattleStatusProducerProvenance statusProducerProvenance(
        const EffectCommandMetadata& metadata);
    BattleEffectCommandReduction reduce(
        BattleRuntimeState& state,
        std::span<const EffectCommand> commands) const;

    BattleEffectCommandReduction reduce(
        BattleRuntimeState& state,
        const EffectCommand& command) const;

    static int queryAttribute(
        const BattleRuntimeState& state,
        const BattleAttributeQuery& query);

    static int queryAttribute(
        const BattleEffectCommandRuntimeState& runtime,
        const BattleAttributeQuery& query);

    // 戰鬥初始化尚未建立 BattleRuntimeState；持久屬性仍須沿用與
    // runtime 完全相同的疊加與 sequence 規則。
    static BattleAttributeEffectResult applyPersistentAttributeModifier(
        BattleEffectCommandRuntimeState& runtime,
        const EffectCommandMetadata& metadata,
        const ModifyAttributeEffectCommand& command,
        int frame,
        std::uint64_t* nextNegativeEffectSequence = nullptr);

    static BattleDamageModifierEffectResult applyPersistentDamageModifier(
        BattleEffectCommandRuntimeState& runtime,
        const EffectCommandMetadata& metadata,
        const ModifyDamageEffectCommand& command,
        int frame,
        std::uint64_t* nextNegativeEffectSequence = nullptr);

    static BattleStatusApplyResult applyStatusCommand(
        BattleStatusUnitState target,
        const EffectCommandMetadata& metadata,
        const ApplyStatusEffectCommand& command,
        const EffectExecutionInputs& context,
        BattleStatusSystemConfig statusConfig,
        bool targetHasShield);

    static int antiComboAttributeValue(
        const BattleAntiComboAttributeBasis& basis);

    static void recordAntiComboInitialization(
        BattleEffectCommandRuntimeState& runtime,
        const EffectCommandMetadata& metadata,
        BattleAntiComboInitializationValue value);

    static void inheritCloneEffectModifiers(
        BattleEffectCommandRuntimeState& runtime,
        int sourceUnitId,
        int cloneUnitId,
        int cloneTeam,
        std::uint64_t& nextNegativeEffectSequence);

    static BattleAntiComboTransfer transferAntiComboInitialization(
        BattleEffectCommandRuntimeState& runtime,
        int sourceUnitId,
        int targetUnitId,
        int targetTeam,
        int comboId,
        int frame,
        std::uint64_t* nextNegativeEffectSequence = nullptr);

    // 回傳順序固定為 instance sequence；All channel 可符合任何實際傷害種類。
    static std::vector<BattleDamageModifierInstance> queryDamageModifiers(
        const BattleRuntimeState& state,
        const BattleDamageModifierQuery& query);

    static std::vector<BattleAttributeModifierInstance> removeExpiredAttributeModifiers(
        BattleRuntimeState& state,
        int frame);

    static std::vector<BattleDamageModifierInstance> removeExpiredDamageModifiers(
        BattleRuntimeState& state,
        int frame);

    // 查詢與回執皆依穩定 sequence 排序；吸收值是在格擋／單次上限後、護盾前結算。
    static std::vector<BattleDamageAbsorptionLayer> queryDamageAbsorptions(
        const BattleRuntimeState& state,
        int targetUnitId,
        int frame);

    static void accumulateDamageAbsorptions(
        BattleRuntimeState& state,
        std::span<const BattleDamageAbsorptionReceipt> receipts);

    static std::vector<BattleDamageAbsorptionInstance> removeExpiredDamageAbsorptions(
        BattleRuntimeState& state,
        int frame);

    static std::vector<BattleDamageAbsorptionInstance> removeDamageAbsorptionsForSourceDeath(
        BattleRuntimeState& state,
        int sourceUnitId);
};

}  // namespace KysChess::Battle
