#pragma once

#include <compare>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace KysChess
{

enum class EffectSourceKind
{
    Combo,
    Equipment,
    EquipmentSynergy,
    Neigong,
    Magic,
};

struct EffectSourceBinding
{
    EffectSourceKind kind{};
    int sourceId{};
    int ownerUnitId{};
    int sourceTeam{};
    // Permanent configured bindings use zero. Runtime-only aliases receive a
    // unique instance so identical borrowed magics keep independent state.
    std::uint64_t runtimeInstanceId{};

    bool operator==(const EffectSourceBinding&) const = default;
};

// Empty means the complete status group.  Runtime command emission resolves
// the authored relationship into one of these concrete contribution keys so
// query, consumption, and removal all share the same matching contract.
struct StatusContributionFilter
{
    std::optional<int> holderUnitId;
    std::optional<int> sourceUnitId;
    std::optional<EffectSourceBinding> producerBinding;
    std::optional<std::uint64_t> appliedSequence;

    bool operator==(const StatusContributionFilter&) const = default;
};

// C++ 戰鬥指令：玩法定義在此組合事件、目標、條件與動作。
// 配置與玩家描述使用具名玩法效果，不直接使用此執行層格式。
enum class EffectEvent
{
    BattleInitialized,
    FrameAdvanced,
    UltimateCooldownFinished,
    CastPlanned,
    AttackCommitted,
    UltimateCommitted,
    AttackSpawned,
    MainProjectileBeforeDamage,
    HitBeforeDamage,
    DamageResolved,
    HealAttempted,
    HealApplied,
    CastContinuation,
    CastSettled,
    ShieldBroken,
    UnitDied,
    AllyDied,
    StatusPersistent,
};

enum class EffectSelectorKind
{
    Self,
    SourceUnit,
    TransactionTarget,
    HitTarget,
    OriginalAttackTarget,
    ComboMembers,
    AllLivingUnits,
    Allies,
    Enemies,
    LowestHpAllies,
    LowestMpAllies,
    HighestMpEnemy,
    StrongestEnemies,
    NearestEnemies,
    FarthestEnemy,
    UnitsInRadius,
    UnitsInSquare,
    AlliesUsingMartialCategory,
    StatusHolder,
};

enum class EffectTeamFilter
{
    Any,
    Ally,
    Enemy,
};

enum class EffectTieBreak
{
    UnitId,
    BattleRandom,
};

enum class EffectRequiredTarget
{
    Self,
    SourceUnit,
    TransactionTarget,
    HitTarget,
    OriginalAttackTarget,
};

enum class EffectMartialCategory
{
    None = -1,
    Fist,
    Sword,
    Knife,
    Unusual,
};

struct EffectSelector
{
    EffectSelectorKind kind = EffectSelectorKind::Self;
    int count = 0;
    int radiusTiles = 0;
    int squareSideTiles = 0;
    EffectTeamFilter team = EffectTeamFilter::Any;
    EffectTieBreak tieBreak = EffectTieBreak::UnitId;
    bool excludeOwner = false;
    bool requiredBoundMagic = false;
    EffectMartialCategory requiredMartialCategory = EffectMartialCategory::None;
    std::optional<EffectRequiredTarget> requiredTarget;

    bool operator==(const EffectSelector&) const = default;
};

enum class EffectStateSlot
{
    MaximumSkillHpDamage,
    CastMaximumHpDamage,
    AbsorbedDamage,
    PermanentCastProgress,
};

enum class BattleStatusKind
{
    Poison,
    Bleed,
    Stun,
    MpBlocked,
    ColdPoison,
    WitheredBone,
    SevenStarMark,
    NeutralizeForce,
    Blinded,
    NextAttackMiss,
    DamageBlockLayer,
    SingleHitCapLayer,
    BattleSpirit,
    TrueQi,
    PoisonExplosion,
    Shadowless,
    NextAttackCritical,
    Count,
};

std::string_view battleStatusLabel(BattleStatusKind status);

enum class EffectNumberBase
{
    Constant,
    SourceStar,
    SourceAttack,
    SourceMaxHp,
    SourceMissingHpRatio,
    SourceCurrentMpRatio,
    TargetMaxHp,
    TargetCurrentHp,
    TargetCurrentShield,
    TargetCurrentCooldown,
    FinalHpDamage,
    SourceStatusQuantity,
    CurrentContributionQuantity,
    StoredStateValue,
    ApplicationTargetMaxHp,
    BoundRatio,
    Count,
};

enum class StatusNumberScale
{
    Once,
    PerContributionLayer,
};

enum class EffectRounding
{
    TowardZero,
    Floor,
    Ceil,
    Nearest,
};

enum class StatusSourceMatch
{
    Any,
    EffectOwner,
    EffectBinding,
    CurrentContribution,
};

struct EffectNumber
{
    EffectNumberBase base = EffectNumberBase::Constant;
    std::optional<EffectNumberBase> multiplierBase;
    std::optional<BattleStatusKind> status;
    StatusSourceMatch statusSource = StatusSourceMatch::Any;
    std::optional<EffectStateSlot> stateSlot;
    int flat = 0;
    int percent = 0;
    EffectRounding rounding = EffectRounding::TowardZero;
    std::optional<int> minimum;
    std::optional<int> maximum;
    StatusNumberScale statusScale = StatusNumberScale::Once;
    std::int64_t boundNumerator{};
    std::int64_t boundDenominator = 1;

    bool operator==(const EffectNumber&) const = default;
};

struct IsUltimateCondition {};
struct CastUsesEffectSourceMagicCondition {};
struct IsMainProjectileCondition {};
struct IsRootAttackCondition {};
struct SourceHpRatioAtMostCondition { int percent = 100; };
struct SourceHpRatioBelowCondition { int percent = 100; };
struct SourceIsLastAliveCondition {};
struct TargetHpRatioAtMostCondition { int percent = 100; };
struct TargetNotInvincibleCondition {};
struct SourceHasStateCondition { BattleStatusKind state{}; };
struct TargetHasStateCondition { BattleStatusKind state{}; };
struct TargetHasStateFromEffectOwnerCondition { BattleStatusKind state{}; };
struct SourceStackAtLeastCondition { BattleStatusKind stack{}; int count = 1; };
struct OtherLivingAllyUsesBoundMagicCondition {};
struct CastDistinctTargetCountAtLeastCondition { int count = 1; };
struct AttackOrdinalEqualsCondition { int ordinal = 0; };
struct HealKindInCondition { std::vector<std::string> kinds; };
struct DamageOriginIsAttackCondition {};
struct DamageKilledTargetCondition {};
struct AcceptedHitCondition
{
    bool requirePositiveDamage = false;
};
struct EventTargetBelongsToBoundSourceCondition {};
enum class DamagePerspective
{
    Dealt,
    Received,
};
struct DamagePerspectiveCondition { DamagePerspective perspective{}; };
struct DamageKindInCondition { std::vector<std::string> kinds; };
struct TargetMpWasFullBeforeCastCondition {};
struct RandomSelectionAvailableCondition {};
struct TargetIsStatusHolderCondition {};

using EffectCondition = std::variant<
    IsUltimateCondition,
    CastUsesEffectSourceMagicCondition,
    IsMainProjectileCondition,
    IsRootAttackCondition,
    SourceHpRatioAtMostCondition,
    SourceHpRatioBelowCondition,
    SourceIsLastAliveCondition,
    TargetHpRatioAtMostCondition,
    TargetNotInvincibleCondition,
    SourceHasStateCondition,
    TargetHasStateCondition,
    TargetHasStateFromEffectOwnerCondition,
    SourceStackAtLeastCondition,
    OtherLivingAllyUsesBoundMagicCondition,
    CastDistinctTargetCountAtLeastCondition,
    AttackOrdinalEqualsCondition,
    HealKindInCondition,
    DamageOriginIsAttackCondition,
    DamageKilledTargetCondition,
    AcceptedHitCondition,
    EventTargetBelongsToBoundSourceCondition,
    DamagePerspectiveCondition,
    DamageKindInCondition,
    TargetMpWasFullBeforeCastCondition,
    RandomSelectionAvailableCondition,
    TargetIsStatusHolderCondition>;

enum class BattleAttribute
{
    MaxHp,
    Attack,
    Defence,
    Speed,
    CriticalChance,
    CriticalDamage,
    DodgeChance,
    BlockChance,
    DamageReduction,
    SkillDamage,
    ProjectilePressureDamage,
    CooldownReduction,
    MpRecoveryBonus,
    StaggerResistance,
    ProjectileReflectChance,
    SkillReflectPercent,
    CounterUltimateBlockChance,
    CriticalAfterDodge,
    DashChance,
    OutgoingCooldownExtensionChance,
    OutgoingCooldownExtensionPercent,
    IncomingCooldownExtensionChance,
    IncomingCooldownExtensionPercent,
    GuaranteedHit,
};


enum class AttributeOperation
{
    FlatAdd,
    PercentAdd,
    PercentagePointAdd,
    Override,
    Multiply,
    AtLeast,
};


enum class EffectStackScope
{
    Shared,
    EventSource,
};

enum class EffectStackPolicy
{
    Independent,
    Refresh,
    Replace,
    KeepStrongest,
    AddStack,
};

struct ModifyAttributeAction
{
    BattleAttribute attribute{};
    EffectNumber amount;
    AttributeOperation operation{};
    int durationFrames = 0;
    EffectStackPolicy stack = EffectStackPolicy::Independent;
    std::optional<int> stackLimit;
    EffectStackScope stackScope = EffectStackScope::Shared;
};

enum class DamageModifierStage
{
    BeforeDefense,
    AfterDefense,
    Final,
};

enum class DamageChannel
{
    Skill,
    Dot,
    Effect,
    All,
};

enum class DamageModifierOperation
{
    FlatAdd,
    PercentAdd,
    Multiply,
    IgnoreDefensePercent,
    CapSingleHitAtMaxHpPercent,
    CapSingleHitAtValue,
    ExecuteBelowMaxHpPercent,
};

enum class DamageModifierPerspective
{
    Outgoing,
    Incoming,
};

struct ModifyDamageAction
{
    DamageModifierPerspective perspective = DamageModifierPerspective::Outgoing;
    DamageModifierStage stage{};
    DamageChannel channel{};
    EffectNumber amount;
    DamageModifierOperation operation{};
    int durationFrames = 0;
    EffectStackPolicy stack = EffectStackPolicy::Independent;
    std::optional<int> stackLimit;
    EffectStackScope stackScope = EffectStackScope::Shared;
};

enum class BattleResource
{
    Hp,
    Mp,
    Shield,
    StatusShield,
    StaggerShield,
    ActiveCooldown,
    ControlImmunityFrames,
    InvincibilityFrames,
};

enum class ResourceChangeKind
{
    Restore,
    Drain,
    Grant,
    Remove,
    Transfer,
    RefreshToAtLeast,
};

enum class EffectHealKind
{
    Direct,
    Team,
    Aura,
    OnHit,
    KillReward,
    DeathMedical,
    Rescue,
    Regeneration,
    Lifesteal,
    Count,
};

enum class EffectHealSourcePolicy
{
    RequireAlive,
    AllowDead,
};

struct ChangeResourceAction
{
    BattleResource resource{};
    EffectNumber amount;
    ResourceChangeKind kind{};
    std::optional<EffectSelector> transferDestination;
    EffectHealKind healKind = EffectHealKind::Direct;
    EffectHealSourcePolicy healSourcePolicy = EffectHealSourcePolicy::RequireAlive;
    bool healRequiresFullMp = false;
    std::optional<EffectNumber> additionalAmount;
};

enum class HealModifierOperation
{
    Block,
    MultiplyReceived,
};

struct ModifyHealTransactionAction
{
    HealModifierOperation operation{};
    std::vector<std::string> kinds;
    int percent = 100;
};

struct NoStatusQuantity
{
    bool operator==(const NoStatusQuantity&) const = default;
};

struct AddStatusLayers
{
    int count{};
    int limit{};

    bool operator==(const AddStatusLayers&) const = default;
};

struct AddSharedStatusLayers
{
    int count{};
    int targetTotalLimit{};

    bool operator==(const AddSharedStatusLayers&) const = default;
};

struct SetStatusMarks
{
    int count{};

    bool operator==(const SetStatusMarks&) const = default;
};

struct AddDamageBlockCharges
{
    int count{};
    int limit{};

    bool operator==(const AddDamageBlockCharges&) const = default;
};

struct SetDamageBlockCharges
{
    int count{};

    bool operator==(const SetDamageBlockCharges&) const = default;
};

struct SetStatusTriggerCharges
{
    int count{};

    bool operator==(const SetStatusTriggerCharges&) const = default;
};

using StatusQuantityOperation = std::variant<
    NoStatusQuantity,
    AddStatusLayers,
    AddSharedStatusLayers,
    SetStatusMarks,
    AddDamageBlockCharges,
    SetDamageBlockCharges,
    SetStatusTriggerCharges>;

enum class StatusReapplicationPolicy
{
    Implicit,
    ExtendDuration,
    KeepLongerDuration,
    RefreshDuration,
    KeepHigherDamage,
    ReplaceExistingPoison,
    Count,
};

enum class PoisonSameEventMerge
{
    None,
    SumDamagePercent,
};

struct StatusBehaviorDefinition;

struct ApplyStatusAction
{
    BattleStatusKind status{};
    int durationFrames = 0;
    std::optional<EffectNumber> duration;
    StatusQuantityOperation quantity;
    StatusReapplicationPolicy reapplication = StatusReapplicationPolicy::Implicit;
    PoisonSameEventMerge poisonSameEventMerge = PoisonSameEventMerge::None;
    std::optional<EffectNumber> neutralizeMpRecovery;
    std::shared_ptr<const StatusBehaviorDefinition> behavior;

    bool operator==(const ApplyStatusAction&) const;
};

struct ConsumeStatusAction
{
    BattleStatusKind status{};
    int quantity = 1;
    StatusSourceMatch source = StatusSourceMatch::Any;
    std::optional<ApplyStatusAction> whenDepleted;
};

enum class StatusRemovalOrder
{
    LongestRemaining,
    Oldest,
    Newest,
};

struct RemoveStatusAction
{
    std::vector<BattleStatusKind> statuses;
    StatusSourceMatch source = StatusSourceMatch::Any;
    bool negativeOnly = false;
    bool controlOnly = false;
    bool clearCurrentActionStagger = false;
    int count = 0;
    StatusRemovalOrder order = StatusRemovalOrder::LongestRemaining;
};

struct SuppressCurrentCastContactsAction
{
    std::optional<EffectNumber> originalTargetShield;

    bool operator==(const SuppressCurrentCastContactsAction&) const = default;
};

struct MakeIncomingAttackMissAction
{
    bool operator==(const MakeIncomingAttackMissAction&) const = default;
};

struct BlockPositiveDamageAction
{
    bool operator==(const BlockPositiveDamageAction&) const = default;
};

struct ConsumeThisStatusAction
{
    int quantity = 1;
    std::optional<ApplyStatusAction> whenDepleted;

    bool operator==(const ConsumeThisStatusAction&) const = default;
};

enum class BattleDamageKind
{
    Physical,
    Skill,
    Pure,
    Poison,
    Bleed,
    Effect,
    Execute,
};

enum class DamageAreaKind
{
    SingleTarget,
    Circle,
    Square,
};

struct DamageArea
{
    DamageAreaKind kind = DamageAreaKind::SingleTarget;
    int radiusTiles = 0;
    int squareSideTiles = 0;
};

struct PerCastHitPolicy
{
    int perTargetLimit = 0;
};

enum class AreaProjectileVisual
{
    DeathBlast,
    ShieldBlast,
};

struct AreaProjectileDamageDelivery
{
    int rangeTiles{};
    int maximumTargets{};
    int stunFrames{};
    bool trackEventSource{};
    AreaProjectileVisual visual{};
};

struct DealDamageAction
{
    EffectNumber amount;
    std::optional<EffectNumber> transactionCount;
    BattleDamageKind kind{};
    bool appliesDamageModifiers = true;
    bool triggersHurtInvincibility = true;
    DamageArea area;
    PerCastHitPolicy perCast;
    std::optional<AreaProjectileDamageDelivery> areaProjectiles;
};

enum class AttackPatternKind
{
    Preserve,
    Fan,
    Flanks,
    SamePointSequence,
    MultiTarget,
    EchoNearestOthers,
};

struct AttackPattern
{
    AttackPatternKind kind = AttackPatternKind::Preserve;
    int projectileCount = 1;
    int spreadDegrees = 0;
    int intervalFrames = 0;

    bool operator==(const AttackPattern&) const = default;
};

enum class AttackTargetPolicy
{
    Preserve,
    SelectedTargets,
    SamePoint,
    SameTarget,
};

struct ProjectileBounceAttackBehavior
{
    int additionalHits{};
    int chancePct{};
    int rangePixels{};

    bool operator==(const ProjectileBounceAttackBehavior&) const = default;
};

struct NearbyTrackingAttackBehavior
{
    int rangePixels{};
    int damagePct{};

    bool operator==(const NearbyTrackingAttackBehavior&) const = default;
};

struct DelayedAlternateAttackBehavior
{
    int delayFrames{};
    int damagePct{};
    int attackerBlockGainChancePct{};

    bool operator==(const DelayedAlternateAttackBehavior&) const = default;
};

struct ExpandingSpiralAttackBehavior
{
    int projectileCount{};
    int bleedStacks{};

    bool operator==(const ExpandingSpiralAttackBehavior&) const = default;
};

using AttackRuntimeBehavior = std::variant<
    std::monostate,
    ProjectileBounceAttackBehavior,
    NearbyTrackingAttackBehavior,
    DelayedAlternateAttackBehavior,
    ExpandingSpiralAttackBehavior>;

enum class CastPropagationPolicy
{
    SourceRules,
    SourceHitRulesOnly,
    SuppressUltimateRules,
    BorrowedUltimateRules,
    NoEffectRules,
};

struct IndependentProjectile
{
    int visualEffectId{};
    int speed{};
    int lifetimeFrames{};
    int magicPower{};

    bool operator==(const IndependentProjectile&) const = default;
};

struct ModifyAttackAction
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
    std::optional<EffectSelector> source;
    std::optional<EffectNumber> damageOverride;
    std::optional<BattleDamageKind> damageKind;
    AttackRuntimeBehavior runtimeBehavior;
    std::optional<IndependentProjectile> independentProjectile;
    std::string activationLog;

    bool operator==(const ModifyAttackAction&) const = default;
};

enum class ForceMoveDirection
{
    AwayFromSource,
    TowardSource,
    TowardPoint,
};

enum class ForceMoveCollision
{
    StopBeforeOccupied,
    StopBeforeBlocked,
};

enum class ForceMoveBlockedResult
{
    Stop,
    Shorten,
};

struct ForceMoveAction
{
    ForceMoveDirection direction{};
    int distanceTiles = 0;
    int distancePixels = 0;
    int lockFrames = 1;
    ForceMoveCollision collision{};
    ForceMoveBlockedResult blocked{};
};

enum class AreaShape
{
    Circle,
    GridSquare,
};

enum class AreaAnchor
{
    HitPosition,
    FollowSourceUnit,
};

enum class AreaSourceDeathPolicy
{
    PersistUntilExpiry,
    RemoveImmediately,
};

enum class AreaMergePolicy
{
    Independent,
    RefreshSameSource,
    ReplaceSameSource,
};

enum class AreaModifierKind
{
    Attribute,
    OutgoingDamage,
    AttackSpawn,
    ForcedMoveImmunity,
    PeriodicDamage,
    DamageRedirect,
};

enum class AreaOverlapPolicy
{
    Add,
    KeepStrongest,
    Any,
};

struct AreaModifier
{
    AreaModifierKind kind{};
    EffectTeamFilter relation{};
    BattleAttribute attribute{};
    EffectNumber amount;
    int percent = 0;
    int intervalFrames{};
    DamageChannel damageChannel = DamageChannel::All;
    std::optional<bool> tracking;
    std::optional<int> speedPct;
    std::optional<int> projectilePressurePct;
    std::optional<ForceMoveDirection> blockedDirection;
    AreaOverlapPolicy overlap = AreaOverlapPolicy::KeepStrongest;
    std::optional<AreaOverlapPolicy> trackingOverlap;
    std::optional<AreaOverlapPolicy> speedOverlap;
    std::optional<AreaOverlapPolicy> projectilePressureOverlap;
};

struct CreateAreaAction
{
    AreaShape shape{};
    int radiusTiles = 0;
    int squareSideTiles = 0;
    AreaAnchor anchor{};
    int durationFrames{};
    AreaSourceDeathPolicy sourceDeath{};
    AreaMergePolicy merge{};
    std::vector<AreaModifier> modifiers;
};

enum class CastRangeMode
{
    Preserve,
    Ranged,
};

enum class CastMobilityPolicy
{
    Preserve,
    DashAttack,
    BlinkAttack,
};

struct AutoUltimateCastRequest
{
    bool consumeMp = false;
    bool announce = false;
};

struct ModifyCastAction
{
    std::optional<EffectNumber> mpCost;
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

struct ChangeStateValueAction
{
    EffectStateSlot slot{};
    int delta{};
    std::optional<std::int64_t> minimum;
    std::optional<std::int64_t> maximum;
};

struct TransferStateValueAction
{
    EffectStateSlot sourceSlot{};
    EffectStateSlot destinationSlot{};
};

struct RecordMaximumDamageAction
{
    EffectStateSlot slot{};
    DamageChannel channel = DamageChannel::Skill;
};

enum class StateValueDestination
{
    DamageAmount,
    ShieldAmount,
};

struct ConsumeRecordedMaximumAction
{
    EffectStateSlot slot{};
    StateValueDestination destination{};
    int percent = 100;
    bool clearAfterConsume = true;
};

struct StartDamageAbsorptionAction
{
    EffectStateSlot slot{};
    int absorbedPct{};
    int durationFrames{};
    bool settleOnSourceDeath = false;
    EffectSelector settlementTarget;
    BattleDamageKind settlementDamageKind = BattleDamageKind::Pure;
    int returnedPct = 100;
};

struct SettleDamageAbsorptionAction
{
    EffectStateSlot slot{};
    EffectSelector target;
    BattleDamageKind damageKind = BattleDamageKind::Pure;
    int returnedPct = 100;
    bool clearAfterSettle = true;
};

// 可借用規則的安全 action 類別。複製與借用本身刻意不在
// 這個封閉集合內，因此 filter 無法放行遞迴狀態機。
enum class BorrowedRuleActionCategory
{
    AttributeModifier,
    DamageModifier,
    ResourceChange,
    HealTransactionModifier,
    Status,
    Damage,
    Attack,
    ForcedMovement,
    Area,
    Cast,
    StateValue,
    DamageMemory,
    DamageAbsorption,
    StatusDamageSettlement,
};

struct BorrowedRuleFilter
{
    std::vector<BorrowedRuleActionCategory> allowedActionCategories;
};

enum class CopiedMagicCondition
{
    HasUltimateAttackDefinition,
    ExcludesRecursiveEffects,
};

struct CopiedMagicFilter
{
    std::vector<CopiedMagicCondition> conditions;
};

struct BorrowEffectRulesAction
{
    EffectSelector sourceUnits;
    EffectNumber sourceCount;
    BorrowedRuleFilter filter;
    CastPropagationPolicy propagation = CastPropagationPolicy::BorrowedUltimateRules;
};

struct CopyAttackDefinitionAction
{
    EffectSelector sourceUnits;
    CopiedMagicFilter filter;
    int copyCount = 1;
    CastPropagationPolicy propagation = CastPropagationPolicy::SuppressUltimateRules;
};

struct SettleRemainingStatusDamageAction
{
    BattleStatusKind status{};
};

struct GenerateClonesAction
{
    int count{};
};

struct PreventDeathAction
{
    int invincibilityFrames{};
};

enum class RescueRepositionMode
{
    Protect,
    Execute,
};

struct ConfigureRescueRepositionAction
{
    RescueRepositionMode mode{};
    int activations{};
};

using StateMachineAction = std::variant<
    ChangeStateValueAction,
    TransferStateValueAction,
    RecordMaximumDamageAction,
    ConsumeRecordedMaximumAction,
    StartDamageAbsorptionAction,
    SettleDamageAbsorptionAction,
    BorrowEffectRulesAction,
    CopyAttackDefinitionAction,
    SettleRemainingStatusDamageAction,
    GenerateClonesAction,
    PreventDeathAction,
    ConfigureRescueRepositionAction>;

struct EffectAction;

struct ConditionalEffectAction
{
    std::vector<EffectCondition> conditions;
    std::vector<EffectAction> whenTrue;
    std::vector<EffectAction> whenFalse;
};

using EffectActionValue = std::variant<
    ModifyAttributeAction,
    ModifyDamageAction,
    ChangeResourceAction,
    ModifyHealTransactionAction,
    ApplyStatusAction,
    ConsumeStatusAction,
    RemoveStatusAction,
    DealDamageAction,
    ModifyAttackAction,
    ForceMoveAction,
    CreateAreaAction,
    ModifyCastAction,
    StateMachineAction,
    ConsumeThisStatusAction,
    SuppressCurrentCastContactsAction,
    MakeIncomingAttackMissAction,
    BlockPositiveDamageAction,
    std::shared_ptr<ConditionalEffectAction>>;

struct EffectAction
{
    EffectActionValue value;
};

struct EffectRuleId
{
    std::uint64_t value{};
    auto operator<=>(const EffectRuleId&) const = default;
};

inline constexpr std::uint64_t IntrinsicEffectRuleIdMask = std::uint64_t{1} << 63;

constexpr bool isIntrinsicEffectRuleId(EffectRuleId id)
{
    return (id.value & IntrinsicEffectRuleIdMask) != 0;
}

constexpr EffectRuleId intrinsicStatusEffectRuleId(EffectRuleId producer)
{
    return EffectRuleId{ producer.value | IntrinsicEffectRuleIdMask };
}

enum class EffectActivationScope
{
    PerCastPerTarget,
};

enum class EffectObservationScope
{
    Owner,
    OwnerTeamEventSource,
    EventTarget,
    StatusHolderEventSource,
    StatusHolderEventTarget,
    StatusSourceEventSource,
    SourceOwnerTeamEventSource,
    ComboMemberEventSource,
};

enum class EffectCastMatch
{
    BoundMagic,
    OwnerAnyCast,
};

struct EffectActivationLimit
{
    EffectActivationScope scope{};
    int maxEvaluations{};
};

struct EffectRule
{
    EffectRuleId id;
    EffectEvent event{};
    EffectObservationScope observation = EffectObservationScope::Owner;
    EffectCastMatch castMatch = EffectCastMatch::BoundMagic;
    EffectSelector selector;
    std::vector<EffectCondition> conditions;
    int chancePct = 100;
    int maxActivations = 0;
    // Rules bound once per eligible owner can share a source-scoped cooldown.
    // This preserves group triggers such as one combo member starting a buff
    // window for every member without collapsing per-owner activation counts.
    int sharedCooldownFrames = 0;
    // 週期規則共用再生與光環效果採用的戰鬥幀序號；當 periodOrdinal
    // 可被週期間隔整除時觸發。
    int intervalFrames = 0;
    // Every-N activation counts otherwise eligible observed events per bound
    // rule. Zero means every event; N activates on N, 2N, and so on.
    int everyNthEvent = 0;
    std::optional<EffectActivationLimit> activationLimit;
    // Ordered repetition replays the complete action list. Unlike an action's
    // transaction/application count, later actions settle between repetitions.
    std::optional<EffectNumber> repetitionCount;
    std::vector<EffectAction> actions;
};

struct EffectActivationEvaluationRuntime
{
    std::uint64_t castId{};
    int targetUnitId = -1;
    int count{};

    bool operator==(const EffectActivationEvaluationRuntime&) const = default;
};

struct EffectRuleRuntimeState
{
    int activationCount{};
    int eligibleEventCount{};
    int intervalFramesRemaining{};
    std::int64_t sharedCooldownUntilFrame = -1;
    std::vector<EffectActivationEvaluationRuntime> activationEvaluations;

    bool operator==(const EffectRuleRuntimeState&) const = default;
};

struct StatusBehaviorDefinition
{
    std::vector<EffectRule> rules;
};

inline bool ApplyStatusAction::operator==(const ApplyStatusAction& other) const
{
    return status == other.status
        && durationFrames == other.durationFrames
        && duration == other.duration
        && quantity == other.quantity
        && reapplication == other.reapplication
        && poisonSameEventMerge == other.poisonSameEventMerge
        && neutralizeMpRecovery == other.neutralizeMpRecovery
        && behavior == other.behavior;
}
class GameplayEffectDefinition;
using GameplayEffect = std::shared_ptr<const GameplayEffectDefinition>;

struct ChessMagicEffectDefinition
{
    int magicId = -1;
    std::string name;
    std::vector<EffectRule> rules;
    std::string purpose;
    std::vector<GameplayEffect> effects;
};

}  // namespace KysChess
