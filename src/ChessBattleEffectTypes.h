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
};

// Battle effects are described by orthogonal event, target, condition and
// action values. All battle-effect sources use this schema.
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
};

inline constexpr std::string_view battleStatusLabel(BattleStatusKind status)
{
    switch (status)
    {
    case BattleStatusKind::Poison: return "中毒";
    case BattleStatusKind::Bleed: return "流血";
    case BattleStatusKind::Stun: return "眩暈";
    case BattleStatusKind::MpBlocked: return "封內";
    case BattleStatusKind::ColdPoison: return "寒毒";
    case BattleStatusKind::WitheredBone: return "枯骨";
    case BattleStatusKind::SevenStarMark: return "七星";
    case BattleStatusKind::NeutralizeForce: return "化勁";
    case BattleStatusKind::Blinded: return "刺目";
    case BattleStatusKind::NextAttackMiss: return "下一次攻擊落空";
    case BattleStatusKind::DamageBlockLayer: return "傷害抵擋";
    case BattleStatusKind::SingleHitCapLayer: return "單次承傷上限";
    case BattleStatusKind::BattleSpirit: return "戰意";
    case BattleStatusKind::TrueQi: return "真氣";
    case BattleStatusKind::PoisonExplosion: return "毒爆";
    case BattleStatusKind::Shadowless: return "無影";
    case BattleStatusKind::NextAttackCritical: return "下一次攻擊必定暴擊";
    }
    return {};
}

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
    AccumulatedStateValue,
    SourceStatusPotency,
    SourceStatusStacks,
    StoredStateValue,
};

enum class EffectRounding
{
    TowardZero,
    Floor,
    Ceil,
    Nearest,
};

struct EffectNumber
{
    EffectNumberBase base = EffectNumberBase::Constant;
    std::optional<EffectNumberBase> multiplierBase;
    std::optional<BattleStatusKind> status;
    std::optional<EffectStateSlot> stateSlot;
    int flat = 0;
    int percent = 0;
    EffectRounding rounding = EffectRounding::TowardZero;
    std::optional<int> minimum;
    std::optional<int> maximum;

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
    RandomSelectionAvailableCondition>;

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
};


enum class AttributeOperation
{
    FlatAdd,
    PercentAdd,
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
    bool perStack = false;
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

struct ApplyStatusAction
{
    BattleStatusKind status{};
    int durationFrames = 0;
    std::optional<EffectNumber> duration;
    std::optional<EffectNumber> applicationCount;
    int stacks = 1;
    EffectNumber potency;
    EffectNumber secondaryPotency;
    EffectStackPolicy stack = EffectStackPolicy::Independent;
    std::optional<int> stackLimit;
    bool aggregatePotencyWithinEvent = false;
};

enum class StatusSourceMatch
{
    Any,
    EffectOwner,
};

struct ConsumeStatusAction
{
    BattleStatusKind status{};
    int stacks = 1;
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
    bool negativeOnly = false;
    bool controlOnly = false;
    bool clearCurrentActionStagger = false;
    int count = 0;
    StatusRemovalOrder order = StatusRemovalOrder::LongestRemaining;
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

struct ModifyAttackAction
{
    AttackPattern pattern;
    int strengthPct = 100;
    std::optional<bool> through;
    std::optional<bool> tracking;
    bool mainProjectile = true;
    int sameTargetHitLimit = 0;
    AttackTargetPolicy targets = AttackTargetPolicy::Preserve;
    CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules;
    bool addToBaseAttack = false;
    std::optional<EffectSelector> source;
    std::optional<EffectNumber> damageOverride;
    std::optional<BattleDamageKind> damageKind;
    AttackRuntimeBehavior runtimeBehavior;

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

enum class EffectActivationScope
{
    PerCastPerTarget,
};

enum class EffectObservationScope
{
    Owner,
    OwnerTeamEventSource,
    EventTarget,
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
struct ChessMagicEffectDefinition
{
    int magicId = -1;
    std::string name;
    std::vector<EffectRule> rules;
    std::string purpose;
};

}  // namespace KysChess
