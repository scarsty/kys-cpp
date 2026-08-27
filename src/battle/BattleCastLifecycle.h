#pragma once

#include "../ChessBattleEffects.h"

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace KysChess::Battle
{

using ::KysChess::CastPropagationPolicy;

template <typename Tag>
class StrongId
{
public:
    constexpr StrongId() = default;
    constexpr explicit StrongId(std::uint64_t value)
        : value_(value)
    {
    }

    constexpr bool valid() const { return value_ != 0; }
    constexpr explicit operator bool() const { return valid(); }
    constexpr std::uint64_t value() const { return value_; }

    auto operator<=>(const StrongId&) const = default;

private:
    std::uint64_t value_{};
};

using BattleCastId = StrongId<struct BattleCastIdTag>;
using BattleAttackId = StrongId<struct BattleAttackIdTag>;
using BattleCastWorkId = StrongId<struct BattleCastWorkIdTag>;

using SharedHitGroupId = int;

enum class CastOriginKind
{
    Normal,
    Ultimate,
    FreeRepeat,
    CopiedAttack,
    BorrowedEffect,
    Echo,
    RescueCounter,
    Reflection,
};

enum class BattleAttackOriginKind
{
    Initial,
    CastDerived,
    Bounce,
    FollowUp,
    Echo,
    Scripted,
    Reflection,
};

struct BattleCastProvenance
{
    BattleCastId rootCastId;
    BattleCastId castId;
    std::optional<BattleCastId> parentCastId;
    int sourceUnitId = -1;
    int magicId = -1;
    bool ultimate = false;
    CastOriginKind origin = CastOriginKind::Normal;
    // 施放層級的預設政策；個別攻擊可在下方 provenance 覆寫。
    CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules;

    bool valid() const { return rootCastId.valid() && castId.valid(); }
};

struct BattlePendingAttackProvenance
{
    BattleCastProvenance cast;
    // 此攻擊的實際政策，不必與 cast 的預設政策相同。
    CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules;
    BattleAttackOriginKind origin = BattleAttackOriginKind::CastDerived;
    std::optional<BattleAttackId> parentAttackId;
    int attackOrdinal{};
    bool rootAttack = false;
    bool mainProjectile = true;
    SharedHitGroupId sharedHitGroupId{};

    bool valid() const { return cast.valid(); }
};

struct BattleAttackProvenance
{
    BattleCastProvenance cast;
    // 此攻擊的實際政策，不必與 cast 的預設政策相同。
    CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules;
    BattleAttackOriginKind origin = BattleAttackOriginKind::CastDerived;
    BattleAttackId attackId;
    std::optional<BattleAttackId> parentAttackId;
    int attackOrdinal{};
    bool rootAttack = false;
    bool mainProjectile = true;
    SharedHitGroupId sharedHitGroupId{};

    bool valid() const { return cast.valid() && attackId.valid(); }
};

BattleAttackProvenance completeAttackProvenance(
    const BattlePendingAttackProvenance& pending,
    BattleAttackId attackId);
BattleAttackId battleAttackIdFromRuntimeId(int runtimeAttackId);

enum class CastWorkKind
{
    CommitBarrier,
    QueuedAttack,
    LiveAttack,
    ChildCast,
    DelayedEffectCommand,
};

enum class AttackFinishReason
{
    SpentOnHit,
    Expired,
    TargetLost,
    ProjectileCancelled,
    ChainEnded,
    NoBounceTarget,
    ExplicitlyCancelled,
    BattleEnded,
    ReflectedAtHit,
};

struct CastWorkToken
{
    BattleCastWorkId id;
    BattleCastId castId;

    bool valid() const { return id.valid() && castId.valid(); }
};

struct CastWorkResult
{
    std::optional<AttackFinishReason> attackFinishReason;

    static CastWorkResult attackFinished(AttackFinishReason reason)
    {
        return { reason };
    }
};

struct CastAttackAggregate
{
    BattleAttackId attackId;
    int attackOrdinal{};
    std::set<int> hitUnitIds;
    int highestActualHpDamage{};
    std::int64_t totalActualHpDamage{};
    std::optional<AttackFinishReason> finishReason;
};

struct CastAggregate
{
    std::set<int> distinctHitUnitIds;
    int highestActualHpDamage{};
    std::int64_t totalActualHpDamage{};
    std::map<int, CastAttackAggregate> attacksByOrdinal;
};

enum class BattleCastTerminalReason
{
    Settled,
    PlannedCastCancelled,
    BattleEnded,
};

struct BattleCastRuntime
{
    BattleCastProvenance provenance;
    int outstandingWork{};
    CastAggregate aggregate;
    bool cancelledBeforeCommit = false;
    bool continuationDispatched = false;
    bool settlementQueued = false;
    bool settledDispatched = false;
    std::optional<int> continuationFrame;
    std::optional<int> settledFrame;
    std::optional<int> cancelledFrame;
    std::optional<BattleCastTerminalReason> terminalReason;
};

struct BattleRootCastRequest
{
    int sourceUnitId = -1;
    int magicId = -1;
    bool ultimate = false;
    CastOriginKind origin = CastOriginKind::Normal;
    CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules;
};

struct BattleChildCastRequest
{
    int sourceUnitId = -1;
    int magicId = -1;
    bool ultimate = false;
    CastOriginKind origin = CastOriginKind::FreeRepeat;
    CastPropagationPolicy propagation = CastPropagationPolicy::SuppressUltimateRules;
};

struct BattleCastStart
{
    BattleCastProvenance provenance;
    CastWorkToken commitBarrier;
};

struct BattleAttackReservationRequest
{
    std::optional<BattleAttackId> parentAttackId;
    std::optional<BattleAttackOriginKind> origin;
    bool rootAttack = false;
    bool mainProjectile = true;
    SharedHitGroupId sharedHitGroupId{};
    std::optional<CastPropagationPolicy> propagation;
};

struct BattleAttackReservation
{
    BattlePendingAttackProvenance provenance;
    CastWorkToken work;
};

enum class BattleCastLifecycleEventType
{
    CastContinuation,
    CastSettled,
};

struct BattleCastLifecycleEvent
{
    BattleCastLifecycleEventType type = BattleCastLifecycleEventType::CastContinuation;
    BattleCastProvenance provenance;
    CastAggregate aggregate;
    int dispatchFrame{};
};

struct BattleCastWorkSnapshot
{
    CastWorkToken token;
    CastWorkKind kind = CastWorkKind::CommitBarrier;
    std::optional<BattleAttackId> attackId;
    std::optional<int> attackOrdinal;
};

struct BattleCastSnapshot
{
    BattleCastRuntime runtime;
    int nextAttackOrdinal{};
    bool rootAttackReserved = false;
    std::optional<CastWorkToken> parentChildWork;
    bool continuationWindowOpen = false;
};

enum class BattleCastLifecycleTerminalState
{
    Running,
    BattleEnded,
};

struct BattleCastLifecycleSnapshot
{
    BattleCastLifecycleTerminalState terminalState = BattleCastLifecycleTerminalState::Running;
    std::optional<int> battleEndedFrame;
    std::uint64_t nextCastId = 1;
    std::uint64_t nextWorkId = 1;
    std::vector<BattleCastSnapshot> activeCasts;
    std::vector<BattleCastRuntime> retiredCasts;
    std::vector<BattleCastWorkSnapshot> work;
};

class BattleCastLifecycle
{
public:
    BattleCastStart beginRootCast(const BattleRootCastRequest& request);
    BattleCastStart beginChildCast(
        BattleCastId parentCastId,
        const BattleChildCastRequest& request);
    void cancelPlannedCast(const BattleCastStart& start, int frame);
    // 攻擊與跨幀佇列應先完成各自 token；此函式會終止其餘工作並永久丟棄事件。
    void cancelOutstandingForBattleEnd(int frame);

    BattleAttackReservation reserveAttack(
        BattleCastId castId,
        const BattleAttackReservationRequest& request = {});
    CastWorkToken reserveDelayedEffectCommand(BattleCastId castId);
    void transferToLiveAttack(CastWorkToken token, BattleAttackId attackId);
    void completeWork(CastWorkToken token, CastWorkResult result = {});

    void recordHit(const BattleAttackProvenance& provenance, int targetUnitId);
    void recordActualHpDamage(
        const BattleAttackProvenance& provenance,
        int targetUnitId,
        int actualHpDamage);

    std::vector<BattleCastLifecycleEvent> drainReadyEvents(int dispatchFrame);

    const BattleCastRuntime& runtime(BattleCastId castId) const;
    int outstandingWork(BattleCastId castId) const;
    CastWorkKind workKind(CastWorkToken token) const;
    bool containsCast(BattleCastId castId) const;
    std::size_t activeCastCount() const;
    std::size_t trackedWorkCount() const;
    BattleCastLifecycleSnapshot snapshot() const;

private:
    struct WorkRecord
    {
        CastWorkToken token;
        CastWorkKind kind = CastWorkKind::CommitBarrier;
        std::optional<BattleAttackId> attackId;
        std::optional<int> attackOrdinal;
    };

    struct RuntimeRecord
    {
        BattleCastRuntime runtime;
        int nextAttackOrdinal{};
        bool rootAttackReserved = false;
        std::optional<CastWorkToken> parentChildWork;
        bool continuationWindowOpen = false;
    };

    BattleCastId allocateCastId();
    CastWorkToken reserveWork(BattleCastId castId, CastWorkKind kind);
    RuntimeRecord& requireRuntime(BattleCastId castId);
    const RuntimeRecord& requireRuntime(BattleCastId castId) const;
    WorkRecord& requireWork(CastWorkToken token);
    const WorkRecord& requireWork(CastWorkToken token) const;
    void assertMayCreateTrackedWork(const RuntimeRecord& record) const;
    void completeParentChildWork(RuntimeRecord& child);
    void recordAttackWorkFinished(
        RuntimeRecord& cast,
        const WorkRecord& work,
        AttackFinishReason reason);
    void retireCast(BattleCastId castId);

    std::uint64_t nextCastId_ = 1;
    std::uint64_t nextWorkId_ = 1;
    std::map<BattleCastId, RuntimeRecord> casts_;
    std::map<BattleCastWorkId, WorkRecord> work_;
    std::map<BattleAttackId, BattleCastWorkId> liveAttackWork_;
    std::vector<BattleCastRuntime> retiredCasts_;
    std::optional<int> battleEndedFrame_;
};

}  // namespace KysChess::Battle
