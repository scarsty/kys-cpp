#include "BattleCastLifecycle.h"

#include <algorithm>
#include <cassert>
#include <limits>

namespace KysChess::Battle
{

namespace
{
void assertCastProvenance(const BattleCastProvenance& provenance)
{
    assert(provenance.valid());
    assert(provenance.sourceUnitId >= 0);
    assert(provenance.magicId >= -1);
    if (provenance.parentCastId)
    {
        assert(provenance.parentCastId->valid());
        assert(provenance.castId != provenance.rootCastId);
    }
    else
    {
        assert(provenance.castId == provenance.rootCastId);
    }
}

void updateDamageAggregate(CastAttackAggregate& attack, CastAggregate& cast, int actualHpDamage)
{
    assert(actualHpDamage >= 0);
    attack.highestActualHpDamage = std::max(attack.highestActualHpDamage, actualHpDamage);
    attack.totalActualHpDamage += actualHpDamage;
    cast.highestActualHpDamage = std::max(cast.highestActualHpDamage, actualHpDamage);
    cast.totalActualHpDamage += actualHpDamage;
}
}  // namespace

BattleAttackProvenance completeAttackProvenance(
    const BattlePendingAttackProvenance& pending,
    BattleAttackId attackId)
{
    assertCastProvenance(pending.cast);
    assert(attackId.valid());
    assert(pending.attackOrdinal >= 0);
    assert(pending.sharedHitGroupId >= 0);
    if (pending.parentAttackId)
    {
        assert(pending.parentAttackId->valid());
        assert(!pending.rootAttack);
    }

    return {
        pending.cast,
        pending.propagation,
        pending.origin,
        attackId,
        pending.parentAttackId,
        pending.attackOrdinal,
        pending.rootAttack,
        pending.mainProjectile,
        pending.sharedHitGroupId,
    };
}

BattleAttackId battleAttackIdFromRuntimeId(int runtimeAttackId)
{
    assert(runtimeAttackId >= 0);
    assert(static_cast<std::uint64_t>(runtimeAttackId) < std::numeric_limits<std::uint64_t>::max());
    return BattleAttackId{ static_cast<std::uint64_t>(runtimeAttackId) + 1 };
}

BattleCastStart BattleCastLifecycle::beginRootCast(const BattleRootCastRequest& request)
{
    assert(!execution_.battleEndedFrame_);
    assert(request.sourceUnitId >= 0);
    assert(request.magicId >= -1);

    const auto castId = allocateCastId();
    BattleCastProvenance provenance{
        castId,
        castId,
        std::nullopt,
        request.sourceUnitId,
        request.magicId,
        request.ultimate,
        request.origin,
        request.propagation,
    };
    assertCastProvenance(provenance);

    RuntimeRecord record;
    record.runtime.provenance = provenance;
    const auto [it, inserted] = execution_.casts_.emplace(castId, std::move(record));
    assert(inserted);
    (void)it;

    return { provenance, reserveWork(castId, CastWorkKind::CommitBarrier) };
}

BattleCastStart BattleCastLifecycle::beginChildCast(
    BattleCastId parentCastId,
    const BattleChildCastRequest& request)
{
    assert(!execution_.battleEndedFrame_);
    assert(request.sourceUnitId >= 0);
    assert(request.magicId >= -1);
    auto& parent = requireRuntime(parentCastId);
    assertMayCreateTrackedWork(parent);

    const auto parentWork = reserveWork(parentCastId, CastWorkKind::ChildCast);
    const auto castId = allocateCastId();
    BattleCastProvenance provenance{
        parent.runtime.provenance.rootCastId,
        castId,
        parentCastId,
        request.sourceUnitId,
        request.magicId,
        request.ultimate,
        request.origin,
        request.propagation,
    };
    assertCastProvenance(provenance);

    RuntimeRecord record;
    record.runtime.provenance = provenance;
    record.parentChildWork = parentWork;
    const auto [it, inserted] = execution_.casts_.emplace(castId, std::move(record));
    assert(inserted);
    (void)it;

    return { provenance, reserveWork(castId, CastWorkKind::CommitBarrier) };
}

void BattleCastLifecycle::cancelPlannedCast(const BattleCastStart& start, int frame)
{
    assert(!execution_.battleEndedFrame_);
    assert(frame >= 0);
    assertCastProvenance(start.provenance);
    assert(start.commitBarrier.valid());
    assert(start.commitBarrier.castId == start.provenance.castId);

    auto& cast = requireRuntime(start.provenance.castId);
    assert(cast.runtime.provenance.rootCastId == start.provenance.rootCastId);
    assert(cast.runtime.provenance.parentCastId == start.provenance.parentCastId);
    assert(cast.runtime.provenance.sourceUnitId == start.provenance.sourceUnitId);
    assert(cast.runtime.provenance.magicId == start.provenance.magicId);
    assert(cast.runtime.provenance.ultimate == start.provenance.ultimate);
    assert(cast.runtime.provenance.origin == start.provenance.origin);
    assert(cast.runtime.provenance.propagation == start.provenance.propagation);
    assert(cast.runtime.outstandingWork == 1);
    assert(!cast.rootAttackReserved);
    assert(!cast.runtime.cancelledBeforeCommit);
    assert(!cast.runtime.continuationDispatched);
    assert(!cast.runtime.settlementQueued);
    assert(!cast.runtime.settledDispatched);
    assert(!cast.runtime.continuationFrame);
    assert(!cast.runtime.settledFrame);
    assert(!cast.runtime.cancelledFrame);
    assert(!cast.runtime.terminalReason);

    auto& barrier = requireWork(start.commitBarrier);
    assert(barrier.kind == CastWorkKind::CommitBarrier);
    --cast.runtime.outstandingWork;
    assert(cast.runtime.outstandingWork == 0);
    cast.runtime.cancelledBeforeCommit = true;
    cast.runtime.cancelledFrame = frame;
    cast.runtime.terminalReason = BattleCastTerminalReason::PlannedCastCancelled;
    const auto erased = execution_.work_.erase(start.commitBarrier.id);
    assert(erased == 1);
    completeParentChildWork(cast);
}

void BattleCastLifecycle::cancelOutstandingForBattleEnd(int frame)
{
    assert(frame >= 0);
    assert(!execution_.battleEndedFrame_);
    execution_.battleEndedFrame_ = frame;

    std::map<BattleCastId, int> workCountByCast;
    for (const auto& [workId, work] : execution_.work_)
    {
        (void)workId;
        ++workCountByCast[work.token.castId];
        if (work.kind != CastWorkKind::QueuedAttack
            && work.kind != CastWorkKind::LiveAttack)
        {
            continue;
        }
        if (work.kind == CastWorkKind::LiveAttack)
        {
            assert(work.attackId);
            assert(execution_.liveAttackWork_.at(*work.attackId) == work.token.id);
        }
        auto& cast = requireRuntime(work.token.castId);
        recordAttackWorkFinished(
            cast,
            work,
            AttackFinishReason::BattleEnded);
    }
    for (const auto& [attackId, workId] : execution_.liveAttackWork_)
    {
        const auto& work = execution_.work_.at(workId);
        assert(work.kind == CastWorkKind::LiveAttack);
        assert(work.attackId);
        assert(*work.attackId == attackId);
    }
    for (const auto& [castId, record] : execution_.casts_)
    {
        const auto count = workCountByCast.contains(castId)
            ? workCountByCast.at(castId)
            : 0;
        assert(record.runtime.outstandingWork == count);
    }

    execution_.work_.clear();
    execution_.liveAttackWork_.clear();

    std::vector<BattleCastId> castIds;
    castIds.reserve(execution_.casts_.size());
    for (auto& [castId, record] : execution_.casts_)
    {
        castIds.push_back(castId);
        record.runtime.outstandingWork = 0;
        record.parentChildWork.reset();
        record.continuationWindowOpen = false;
        if (!record.runtime.cancelledBeforeCommit)
        {
            assert(!record.runtime.terminalReason);
            assert(!record.runtime.continuationFrame
                || frame >= *record.runtime.continuationFrame);
            record.runtime.cancelledFrame = frame;
            record.runtime.terminalReason = BattleCastTerminalReason::BattleEnded;
        }
    }
    for (const auto castId : castIds)
    {
        retireCast(castId);
    }
}

BattleAttackReservation BattleCastLifecycle::reserveAttack(
    BattleCastId castId,
    const BattleAttackReservationRequest& request)
{
    auto& record = requireRuntime(castId);
    assertMayCreateTrackedWork(record);
    assert(request.sharedHitGroupId >= 0);
    if (request.parentAttackId)
    {
        assert(request.parentAttackId->valid());
        assert(!request.rootAttack);
        const auto parent = execution_.liveAttackWork_.find(*request.parentAttackId);
        assert(parent != execution_.liveAttackWork_.end());
        const auto parentCastId = execution_.work_.at(parent->second).token.castId;
        assert(parentCastId == castId
            || record.runtime.provenance.parentCastId == parentCastId);
    }
    if (request.rootAttack)
    {
        assert(record.nextAttackOrdinal == 0);
        assert(!record.rootAttackReserved);
        assert(!request.parentAttackId);
        record.rootAttackReserved = true;
    }

    BattlePendingAttackProvenance provenance;
    provenance.cast = record.runtime.provenance;
    provenance.propagation = request.propagation.value_or(
        record.runtime.provenance.propagation);
    provenance.origin = request.rootAttack
        ? BattleAttackOriginKind::Initial
        : request.origin.value_or(BattleAttackOriginKind::CastDerived);
    provenance.parentAttackId = request.parentAttackId;
    provenance.attackOrdinal = record.nextAttackOrdinal++;
    provenance.rootAttack = request.rootAttack;
    provenance.mainProjectile = request.mainProjectile;
    provenance.sharedHitGroupId = request.sharedHitGroupId;

    auto token = reserveWork(castId, CastWorkKind::QueuedAttack);
    auto& work = requireWork(token);
    work.attackOrdinal = provenance.attackOrdinal;
    return { provenance, token };
}

CastWorkToken BattleCastLifecycle::reserveDelayedEffectCommand(BattleCastId castId)
{
    auto& record = requireRuntime(castId);
    assertMayCreateTrackedWork(record);
    return reserveWork(castId, CastWorkKind::DelayedEffectCommand);
}

void BattleCastLifecycle::transferToLiveAttack(CastWorkToken token, BattleAttackId attackId)
{
    assert(attackId.valid());
    auto& work = requireWork(token);
    assert(work.kind == CastWorkKind::QueuedAttack);
    assert(!work.attackId);
    assert(!execution_.liveAttackWork_.contains(attackId));

    work.kind = CastWorkKind::LiveAttack;
    work.attackId = attackId;
    const auto [it, inserted] = execution_.liveAttackWork_.emplace(attackId, token.id);
    assert(inserted);
    (void)it;
}

void BattleCastLifecycle::completeWork(CastWorkToken token, CastWorkResult result)
{
    auto& work = requireWork(token);
    const bool attackWork = work.kind == CastWorkKind::QueuedAttack
        || work.kind == CastWorkKind::LiveAttack;
    assert(attackWork == result.attackFinishReason.has_value());

    auto& cast = requireRuntime(token.castId);
    assert(cast.runtime.outstandingWork > 0);

    if (work.kind == CastWorkKind::LiveAttack)
    {
        assert(work.attackId);
        const auto erased = execution_.liveAttackWork_.erase(*work.attackId);
        assert(erased == 1);
    }
    if (attackWork)
    {
        recordAttackWorkFinished(cast, work, *result.attackFinishReason);
    }

    --cast.runtime.outstandingWork;
    assert(cast.runtime.outstandingWork >= 0);
    const auto erased = execution_.work_.erase(token.id);
    assert(erased == 1);
}

void BattleCastLifecycle::recordHit(
    const BattleAttackProvenance& provenance,
    int targetUnitId)
{
    assert(provenance.valid());
    assert(targetUnitId >= 0);
    auto& cast = requireRuntime(provenance.cast.castId);
    assert(cast.runtime.provenance.rootCastId == provenance.cast.rootCastId);

    const auto live = execution_.liveAttackWork_.find(provenance.attackId);
    assert(live != execution_.liveAttackWork_.end());
    const auto& work = execution_.work_.at(live->second);
    assert(work.token.castId == provenance.cast.castId);
    assert(work.attackOrdinal == provenance.attackOrdinal);

    cast.runtime.aggregate.distinctHitUnitIds.insert(targetUnitId);
    auto& aggregate = cast.runtime.aggregate.attacksByOrdinal[provenance.attackOrdinal];
    aggregate.attackId = provenance.attackId;
    aggregate.attackOrdinal = provenance.attackOrdinal;
    aggregate.hitUnitIds.insert(targetUnitId);
}

void BattleCastLifecycle::recordActualHpDamage(
    const BattleAttackProvenance& provenance,
    int targetUnitId,
    int actualHpDamage)
{
    assert(provenance.valid());
    assert(targetUnitId >= 0);
    assert(actualHpDamage >= 0);
    auto& cast = requireRuntime(provenance.cast.castId);
    const auto live = execution_.liveAttackWork_.find(provenance.attackId);
    assert(live != execution_.liveAttackWork_.end());
    const auto& work = execution_.work_.at(live->second);
    assert(work.token.castId == provenance.cast.castId);
    assert(work.attackOrdinal == provenance.attackOrdinal);

    auto& aggregate = cast.runtime.aggregate.attacksByOrdinal[provenance.attackOrdinal];
    assert(aggregate.hitUnitIds.contains(targetUnitId));
    aggregate.attackId = provenance.attackId;
    aggregate.attackOrdinal = provenance.attackOrdinal;
    updateDamageAggregate(aggregate, cast.runtime.aggregate, actualHpDamage);
}

std::vector<BattleCastLifecycleEvent> BattleCastLifecycle::drainReadyEvents(int dispatchFrame)
{
    assert(dispatchFrame >= 0);
    if (execution_.battleEndedFrame_)
    {
        return {};
    }

    std::vector<BattleCastId> cancelledCastIds;
    for (auto& [castId, record] : execution_.casts_)
    {
        record.continuationWindowOpen = false;
        if (record.runtime.cancelledBeforeCommit)
        {
            assert(record.runtime.outstandingWork == 0);
            cancelledCastIds.push_back(castId);
        }
    }

    std::vector<BattleCastLifecycleEvent> events;
    for (auto& [castId, record] : execution_.casts_)
    {
        (void)castId;
        if (record.runtime.cancelledBeforeCommit)
        {
            assert(record.runtime.outstandingWork == 0);
            continue;
        }
        if (record.runtime.outstandingWork == 0
            && !record.runtime.continuationDispatched)
        {
            record.runtime.continuationDispatched = true;
            record.runtime.continuationFrame = dispatchFrame;
            record.continuationWindowOpen = true;
            events.push_back({
                BattleCastLifecycleEventType::CastContinuation,
                record.runtime.provenance,
                record.runtime.aggregate,
                dispatchFrame,
            });
        }
    }
    if (!events.empty())
    {
        for (const auto castId : cancelledCastIds)
        {
            retireCast(castId);
        }
        return events;
    }

    std::vector<BattleCastId> settledCastIds;
    for (auto& [castId, record] : execution_.casts_)
    {
        if (record.runtime.cancelledBeforeCommit)
        {
            assert(record.runtime.outstandingWork == 0);
            continue;
        }
        if (record.runtime.outstandingWork == 0
            && record.runtime.continuationDispatched
            && !record.runtime.settlementQueued
            && !record.runtime.settledDispatched)
        {
            record.runtime.settlementQueued = true;
            settledCastIds.push_back(castId);
            events.push_back({
                BattleCastLifecycleEventType::CastSettled,
                record.runtime.provenance,
                record.runtime.aggregate,
                dispatchFrame,
            });
        }
    }

    for (const auto castId : settledCastIds)
    {
        auto& record = requireRuntime(castId);
        assert(record.runtime.settlementQueued);
        assert(!record.runtime.settledDispatched);
        assert(record.runtime.continuationFrame);
        assert(dispatchFrame >= *record.runtime.continuationFrame);
        record.runtime.settledDispatched = true;
        record.runtime.settledFrame = dispatchFrame;
        record.runtime.terminalReason = BattleCastTerminalReason::Settled;
        completeParentChildWork(record);
    }
    for (const auto castId : settledCastIds)
    {
        retireCast(castId);
    }
    for (const auto castId : cancelledCastIds)
    {
        retireCast(castId);
    }
    return events;
}

const BattleCastRuntime& BattleCastLifecycle::runtime(BattleCastId castId) const
{
    return requireRuntime(castId).runtime;
}

int BattleCastLifecycle::outstandingWork(BattleCastId castId) const
{
    return runtime(castId).outstandingWork;
}

CastWorkKind BattleCastLifecycle::workKind(CastWorkToken token) const
{
    return requireWork(token).kind;
}

bool BattleCastLifecycle::containsCast(BattleCastId castId) const
{
    return execution_.casts_.contains(castId);
}

std::size_t BattleCastLifecycle::activeCastCount() const
{
    return execution_.casts_.size();
}

std::size_t BattleCastLifecycle::trackedWorkCount() const
{
    return execution_.work_.size();
}

BattleCastLifecycleSnapshot BattleCastLifecycle::snapshot() const
{
    if (execution_.battleEndedFrame_)
    {
        assert(execution_.casts_.empty());
        assert(execution_.work_.empty());
        assert(execution_.liveAttackWork_.empty());
    }
    BattleCastLifecycleSnapshot result;
    result.terminalState = execution_.battleEndedFrame_
        ? BattleCastLifecycleTerminalState::BattleEnded
        : BattleCastLifecycleTerminalState::Running;
    result.battleEndedFrame = execution_.battleEndedFrame_;
    result.nextCastId = execution_.nextCastId_;
    result.nextWorkId = execution_.nextWorkId_;
    result.activeCasts.reserve(execution_.casts_.size());
    for (const auto& [castId, record] : execution_.casts_)
    {
        (void)castId;
        result.activeCasts.push_back({
            record.runtime,
            record.nextAttackOrdinal,
            record.rootAttackReserved,
            record.parentChildWork,
            record.continuationWindowOpen,
        });
    }
    result.retiredCasts = retiredCasts_;
    std::ranges::sort(
        result.retiredCasts,
        {},
        [](const BattleCastRuntime& runtime)
        {
            return runtime.provenance.castId;
        });
    result.work.reserve(execution_.work_.size());
    for (const auto& [workId, record] : execution_.work_)
    {
        (void)workId;
        result.work.push_back({
            record.token,
            record.kind,
            record.attackId,
            record.attackOrdinal,
        });
    }
    return result;
}

BattleCastId BattleCastLifecycle::allocateCastId()
{
    assert(execution_.nextCastId_ > 0);
    assert(execution_.nextCastId_ < std::numeric_limits<std::uint64_t>::max());
    return BattleCastId{ execution_.nextCastId_++ };
}

CastWorkToken BattleCastLifecycle::reserveWork(BattleCastId castId, CastWorkKind kind)
{
    auto& cast = requireRuntime(castId);
    assertMayCreateTrackedWork(cast);
    assert(execution_.nextWorkId_ > 0);
    assert(execution_.nextWorkId_ < std::numeric_limits<std::uint64_t>::max());

    CastWorkToken token{ BattleCastWorkId{ execution_.nextWorkId_++ }, castId };
    WorkRecord record;
    record.token = token;
    record.kind = kind;
    const auto [it, inserted] = execution_.work_.emplace(token.id, std::move(record));
    assert(inserted);
    (void)it;
    ++cast.runtime.outstandingWork;
    assert(cast.runtime.outstandingWork > 0);
    return token;
}

BattleCastLifecycle::RuntimeRecord& BattleCastLifecycle::requireRuntime(BattleCastId castId)
{
    assert(castId.valid());
    const auto it = execution_.casts_.find(castId);
    assert(it != execution_.casts_.end());
    return it->second;
}

const BattleCastLifecycle::RuntimeRecord& BattleCastLifecycle::requireRuntime(BattleCastId castId) const
{
    assert(castId.valid());
    const auto it = execution_.casts_.find(castId);
    assert(it != execution_.casts_.end());
    return it->second;
}

BattleCastLifecycle::WorkRecord& BattleCastLifecycle::requireWork(CastWorkToken token)
{
    assert(token.valid());
    const auto it = execution_.work_.find(token.id);
    assert(it != execution_.work_.end());
    assert(it->second.token.castId == token.castId);
    return it->second;
}

const BattleCastLifecycle::WorkRecord& BattleCastLifecycle::requireWork(CastWorkToken token) const
{
    assert(token.valid());
    const auto it = execution_.work_.find(token.id);
    assert(it != execution_.work_.end());
    assert(it->second.token.castId == token.castId);
    return it->second;
}

void BattleCastLifecycle::assertMayCreateTrackedWork(const RuntimeRecord& record) const
{
    assert(!execution_.battleEndedFrame_);
    assert(!record.runtime.cancelledBeforeCommit);
    assert(!record.runtime.settledDispatched);
    assert(!record.runtime.settlementQueued);
    assert(!record.runtime.continuationDispatched || record.continuationWindowOpen);
}

void BattleCastLifecycle::completeParentChildWork(RuntimeRecord& child)
{
    if (!child.parentChildWork)
    {
        return;
    }
    const auto parentWork = *child.parentChildWork;
    child.parentChildWork.reset();
    completeWork(parentWork);
}

void BattleCastLifecycle::recordAttackWorkFinished(
    RuntimeRecord& cast,
    const WorkRecord& work,
    AttackFinishReason reason)
{
    assert(work.kind == CastWorkKind::QueuedAttack
        || work.kind == CastWorkKind::LiveAttack);
    assert(work.attackOrdinal);
    auto& aggregate = cast.runtime.aggregate.attacksByOrdinal[*work.attackOrdinal];
    aggregate.attackOrdinal = *work.attackOrdinal;
    if (work.attackId)
    {
        aggregate.attackId = *work.attackId;
    }
    assert(!aggregate.finishReason);
    aggregate.finishReason = reason;
}

void BattleCastLifecycle::retireCast(BattleCastId castId)
{
    const auto cast = execution_.casts_.find(castId);
    assert(cast != execution_.casts_.end());
    assert(cast->second.runtime.outstandingWork == 0);
    assert(cast->second.runtime.cancelledBeforeCommit
        || cast->second.runtime.settledDispatched
        || (cast->second.runtime.terminalReason
            && *cast->second.runtime.terminalReason
                == BattleCastTerminalReason::BattleEnded));
    assert(!cast->second.parentChildWork);
    assert(std::ranges::none_of(execution_.work_, [castId](const auto& entry)
    {
        return entry.second.token.castId == castId;
    }));
    assert(cast->second.runtime.terminalReason);
    switch (*cast->second.runtime.terminalReason)
    {
    case BattleCastTerminalReason::Settled:
        assert(cast->second.runtime.settledFrame);
        break;
    case BattleCastTerminalReason::PlannedCastCancelled:
    case BattleCastTerminalReason::BattleEnded:
        assert(cast->second.runtime.cancelledFrame);
        break;
    }
    retiredCasts_.push_back(std::move(cast->second.runtime));
    execution_.casts_.erase(cast);
}

}  // namespace KysChess::Battle
