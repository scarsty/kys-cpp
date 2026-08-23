#include "BattleAttackSystem.h"

#include "../Find.h"
#include "BattleMath.h"
#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>
#include <unordered_set>
#include <utility>
#include <vector>

namespace KysChess::Battle
{

namespace
{
struct PendingBounce
{
    BattleAttackInstance attack;
    int sourceAttackId = -1;
    int targetUnitId = -1;
};

struct ProjectileCancelCandidate
{
    BattleAttackEvent event;
    int totalDamage = 0;
    int strongestDamage = 0;
    int weakestDamage = 0;
};

struct ProjectileCancelSearchCandidate
{
    const BattleAttackInstance* attack{};
    int team{};
    double minX{};
    double maxX{};
    double minY{};
    double maxY{};
};

bool isProjectileOperation(BattleOperationType operationType)
{
    return operationType == BattleOperationType::RangedProjectile
        || operationType == BattleOperationType::TrackingProjectile;
}

bool segmentBoundsCanOverlap(
    const ProjectileCancelSearchCandidate& lhs,
    const ProjectileCancelSearchCandidate& rhs)
{
    return lhs.minX <= rhs.maxX
        && rhs.minX <= lhs.maxX
        && lhs.minY <= rhs.maxY
        && rhs.minY <= lhs.maxY;
}

void applyAttackPayload(BattleAttackEvent& event, const BattleAttackPayload& state)
{
    event.preferredTargetUnitId = state.preferredTargetUnitId;
    event.sourceUnitId = state.attackSourceUnitId;
    event.skillId = state.skillId;
    event.skillName = state.skillName;
    event.skillHurtType = state.skillHurtType;
    event.skillMagicType = state.skillMagicType;
    event.skillEffectId = state.skillEffectId;
    event.skillAttackerActProperty = state.skillAttackerActProperty;
    event.skillMagicPower = state.skillMagicPower;
    event.operationType = state.operationType;
    event.visualEffectId = state.visualEffectId;
    event.scriptedDamage = state.scriptedDamage;
    event.scriptedDamageAppliesModifiers = state.scriptedDamageAppliesModifiers;
    event.scriptedDamageTriggersDefenseEffects = state.scriptedDamageTriggersDefenseEffects;
    event.scriptedStunFrames = state.scriptedStunFrames;
    event.scriptedBleedStacks = state.scriptedBleedStacks;
    event.executeCanHitInvincible = state.executeCanHitInvincible;
    event.track = state.track;
    event.through = state.through;
    event.strengthPct = state.strengthPct;
    event.suppressNearbyTrackingProjectileProc = state.suppressNearbyTrackingProjectileProc;
    event.projectileCancelDamage = state.projectileCancelWeaken;
    event.castSubrequestKind = state.castSubrequestKind;
    event.roleAttackEchoActType = state.roleAttackEchoActType;
    event.damageKind = state.damageKind;
    event.position = state.position;
    event.velocity = state.velocity;
    event.totalFrame = state.totalFrame;
}

void applyAttackInstance(BattleAttackEvent& event, const BattleAttackInstance& attack)
{
    assert(attack.provenance.valid());
    event.provenance = attack.provenance;
    applyAttackPayload(event, attack.state);
}

BattleAttackEvent makeProjectileCancelEvent(const BattleAttackInstance& lhs, const BattleAttackInstance& rhs)
{
    BattleAttackEvent event;
    event.type = BattleAttackEventType::ProjectileCancel;
    event.attackId = lhs.id;
    event.otherAttackId = rhs.id;
    event.preferredTargetUnitId = lhs.state.preferredTargetUnitId;
    event.sourceUnitId = lhs.state.attackSourceUnitId;
    event.otherSourceUnitId = rhs.state.attackSourceUnitId;
    event.projectileCancelDamage = scaleProjectileCancelDamage(
        lhs.state.projectileCancelDamage,
        lhs.state.operationType);
    event.otherProjectileCancelDamage = scaleProjectileCancelDamage(
        rhs.state.projectileCancelDamage,
        rhs.state.operationType);
    assert(lhs.provenance.valid());
    assert(rhs.provenance.valid());
    event.provenance = lhs.provenance;
    event.otherProvenance = rhs.provenance;
    return event;
}

BattleAttackEvent makeAttackEvent(
    BattleAttackEventType type,
    const BattleAttackInstance& attack,
    int unitId = -1)
{
    BattleAttackEvent event;
    event.type = type;
    event.attackId = attack.id;
    event.unitId = unitId;
    applyAttackInstance(event, attack);
    event.frame = attack.frame;
    return event;
}

ProjectileCancelCandidate makeProjectileCancelCandidate(
    const BattleAttackInstance& lhs,
    const BattleAttackInstance& rhs)
{
    ProjectileCancelCandidate candidate;
    candidate.event = makeProjectileCancelEvent(lhs, rhs);
    candidate.totalDamage = candidate.event.projectileCancelDamage + candidate.event.otherProjectileCancelDamage;
    candidate.strongestDamage = std::max(
        candidate.event.projectileCancelDamage,
        candidate.event.otherProjectileCancelDamage);
    candidate.weakestDamage = std::min(
        candidate.event.projectileCancelDamage,
        candidate.event.otherProjectileCancelDamage);
    return candidate;
}

bool betterProjectileCancelCandidate(
    const ProjectileCancelCandidate& lhs,
    const ProjectileCancelCandidate& rhs)
{
    if (lhs.totalDamage != rhs.totalDamage)
    {
        return lhs.totalDamage > rhs.totalDamage;
    }
    if (lhs.strongestDamage != rhs.strongestDamage)
    {
        return lhs.strongestDamage > rhs.strongestDamage;
    }
    if (lhs.weakestDamage != rhs.weakestDamage)
    {
        return lhs.weakestDamage > rhs.weakestDamage;
    }
    if (lhs.event.attackId != rhs.event.attackId)
    {
        return lhs.event.attackId < rhs.event.attackId;
    }
    return lhs.event.otherAttackId < rhs.event.otherAttackId;
}

ProjectileCancelSearchCandidate makeProjectileCancelSearchCandidate(
    const BattleAttackInstance& attack,
    int team,
    double hitRadius)
{
    ProjectileCancelSearchCandidate candidate;
    candidate.attack = &attack;
    candidate.team = team;
    candidate.minX = std::min(attack.previousPosition.x, attack.state.position.x) - hitRadius;
    candidate.maxX = std::max(attack.previousPosition.x, attack.state.position.x) + hitRadius;
    candidate.minY = std::min(attack.previousPosition.y, attack.state.position.y) - hitRadius;
    candidate.maxY = std::max(attack.previousPosition.y, attack.state.position.y) + hitRadius;
    return candidate;
}

bool projectileCancelSearchCandidateLess(
    const ProjectileCancelSearchCandidate& lhs,
    const ProjectileCancelSearchCandidate& rhs)
{
    if (lhs.minX != rhs.minX)
    {
        return lhs.minX < rhs.minX;
    }
    return lhs.attack->id < rhs.attack->id;
}

bool canResolveContactFromDefeatedSource(const BattleAttackPayload& attack)
{
    return isProjectileOperation(attack.operationType);
}

}  // namespace

int scaleProjectileCancelDamage(int damage, BattleOperationType operationType)
{
    if (damage <= 0)
    {
        return 0;
    }

    return std::max(1, (damage * battleOperationDamagePct(operationType) + 99) / 100);
}

void applyProjectileBouncePrime(BattleAttackSpawnRequest& request, BattleAttackBouncePrime prime)
{
    assert(request.initial.attackSourceUnitId >= 0);
    assert(request.initial.bounceRemaining == 0);
    assert(prime.count > 0);
    assert(prime.chancePct >= 0 && prime.chancePct <= 100);
    assert(prime.rollPct >= 0 && prime.rollPct < 100);
    assert(prime.range > 0);

    request.initial.bounceRemaining = prime.count;
    request.initial.bounceChancePct = prime.chancePct;
    request.initial.bounceRollPct = prime.rollPct;
    request.initial.bounceRange = prime.range;
}

bool tryApplyProjectileBouncePrime(BattleAttackSpawnRequest& request, BattleAttackBouncePrime prime)
{
    assert(request.initial.attackSourceUnitId >= 0);
    assert(request.initial.bounceRemaining == 0);
    assert(prime.count > 0);
    assert(prime.chancePct >= 0 && prime.chancePct <= 100);
    assert(prime.rollPct >= 0 && prime.rollPct < 100);
    assert(prime.range > 0);

    const auto& attack = request.initial;
    const bool eligible = attack.scriptedDamage == 0
        && (attack.track
            || attack.operationType == BattleOperationType::RangedProjectile);
    if (!eligible)
    {
        return false;
    }

    applyProjectileBouncePrime(request, prime);
    return true;
}

bool attackSpawnDelayElapsed(BattleAttackSpawnRequest& request)
{
    assert(request.provenance.valid());
    assert(request.castWork.valid());
    assert(request.castWork.castId == request.provenance.cast.castId);
    assert(request.spawnDelayFrames >= 0);
    if (request.spawnDelayFrames == 0)
    {
        return true;
    }
    --request.spawnDelayFrames;
    return false;
}

BattleAttackEvent BattleAttackState::spawn(
    BattleAttackSpawnRequest&& request,
    BattleCastLifecycle& castLifecycle)
{
    assert(request.initial.attackSourceUnitId >= 0);
    assert(request.initialFrame >= 0);
    assert(request.spawnDelayFrames == 0);
    assert(request.initial.totalFrame > 0);
    assert(request.initial.bounceRemaining >= 0);
    assert(request.initial.bounceRange >= 0);
    assert(request.initial.bounceChancePct >= 0 && request.initial.bounceChancePct <= 100);
    assert(request.initial.bounceRollPct >= 0 && request.initial.bounceRollPct < 100);
    assert(request.initial.projectilePressurePct >= 0);
    assert(request.provenance.valid());
    assert(request.castWork.valid());
    assert(request.castWork.castId == request.provenance.cast.castId);

    BattleAttackInstance attack;
    attack.id = allocateAttackId();
    attack.state = std::move(request.initial);
    attack.castWork = std::exchange(request.castWork, {});
    attack.provenance = completeAttackProvenance(
        request.provenance,
        battleAttackIdFromRuntimeId(attack.id));
    request.provenance = {};
    attack.contactsSuppressed = castContactsSuppressed(
        attack.provenance.cast.castId);
    castLifecycle.transferToLiveAttack(
        attack.castWork,
        attack.provenance.attackId);
    attack.previousPosition = attack.state.position;
    attack.frame = request.initialFrame;
    attack.acceleration = request.acceleration;
    attack.spiralMotion = request.spiralMotion;
    attack.spiralCenter = request.spiralCenter;
    attack.spiralRadius = request.spiralRadius;
    attack.spiralRadiusGrowth = request.spiralRadiusGrowth;
    attack.spiralAngle = request.spiralAngle;
    attack.spiralAngularVelocity = request.spiralAngularVelocity;
    attacks.push_back(std::move(attack));
    const auto& spawned = attacks.back();

    BattleAttackEvent event;
    event.type = BattleAttackEventType::AttackSpawned;
    event.attackId = spawned.id;
    event.unitId = spawned.state.preferredTargetUnitId;
    applyAttackInstance(event, spawned);
    event.position = spawned.state.position;
    event.velocity = spawned.state.velocity;
    event.totalFrame = spawned.state.totalFrame;
    return event;
}

std::pmr::vector<BattleAttackEvent> BattleAttackState::tick(
    const BattleRuntimeUnits& units,
    BattleCastLifecycle& castLifecycle,
    std::pmr::memory_resource* memoryResource)
{
    std::pmr::vector<BattleAttackEvent> events(memoryResource);
    tick(units, castLifecycle, events);
    return events;
}

void BattleAttackState::tick(
    const BattleRuntimeUnits& units,
    BattleCastLifecycle& castLifecycle,
    std::pmr::vector<BattleAttackEvent>& events)
{
    assert(hitRadius > 0.0);
    assert(minimumVectorNorm > 0.0);
    assert(projectileGraceFrames >= 0);
    assert(bounceSpawnDistance > 0.0);
    assert(defaultProjectileSpeed > 0.0);
    assert(minimumBounceTotalFrame > 0);

    auto* memoryResource = events.get_allocator().resource();
    std::pmr::vector<PendingBounce> pendingBounces(memoryResource);

    const size_t initialAttackCount = attacks.size();
    for (size_t i = 0; i < initialAttackCount; ++i)
    {
        auto& attack = attacks[i];
        assert(attack.id >= 0);
        assert(attack.state.totalFrame > 0);
        assert(attack.state.bounceRemaining >= 0);
        assert(attack.state.bounceRange >= 0);
        assert(attack.state.bounceChancePct >= 0 && attack.state.bounceChancePct <= 100);
        assert(attack.state.bounceRollPct >= 0 && attack.state.bounceRollPct < 100);
        assert(attack.provenance.valid());
        assert(attack.castWork.valid());
        assert(attack.provenance.attackId == battleAttackIdFromRuntimeId(attack.id));

        ++attack.frame;
        moveAttack(attack);
        events.push_back(makeAttackEvent(BattleAttackEventType::Moved, attack));

        const auto* target = selectTarget(units, attack);
        if (!target && attack.state.requirePreferredTarget)
        {
            attack.noHurt = true;
            attack.frame = std::max(attack.state.totalFrame - 5, attack.frame);
            if (!attack.scheduledFinishReason)
            {
                attack.scheduledFinishReason = AttackFinishReason::TargetLost;
                events.push_back(makeAttackEvent(BattleAttackEventType::TargetLost, attack));
            }
        }

        if (target && attack.state.track && attack.hitUnitIds.empty())
        {
            trackTarget(attack, *target);
        }

        if (target && contactBlockedByInvincible(units, attack, *target))
        {
            markInvincibleBlocked(attack, target->id);
            BattleAttackEvent blocked;
            blocked.type = BattleAttackEventType::BlockedByInvincible;
            blocked.attackId = attack.id;
            blocked.unitId = target->id;
            applyAttackInstance(blocked, attack);
            blocked.frame = attack.frame;
            events.push_back(std::move(blocked));
        }
        else if (target && canHit(units, attack, *target))
        {
            markHit(attack, target->id);
            BattleAttackEvent hit;
            hit.type = BattleAttackEventType::Hit;
            hit.attackId = attack.id;
            hit.unitId = target->id;
            applyAttackInstance(hit, attack);
            hit.frame = attack.frame;
            events.push_back(std::move(hit));

            const bool bounceAttack = attack.provenance.origin == BattleAttackOriginKind::Bounce;
            if (bounceAttack && attack.state.bounceRemaining == 0)
            {
                attack.scheduledFinishReason = AttackFinishReason::ChainEnded;
                events.push_back(makeAttackEvent(
                    BattleAttackEventType::ChainEnded,
                    attack,
                    target->id));
            }
            else if (attack.state.bounceRemaining > 0)
            {
                auto bounceSource = attack;
                const auto* nextTarget = attack.state.bounceChancePct > 0
                    && attack.state.bounceRollPct < attack.state.bounceChancePct
                    ? selectBounceTarget(units, attack, *target)
                    : nullptr;
                attack.state.bounceRemaining = 0;
                attack.noHurt = true;
                attack.frame = std::max(attack.state.totalFrame - 15, attack.frame);
                if (nextTarget)
                {
                    const int bounceAttackId = allocateAttackId();
                    pendingBounces.push_back({
                        makeBounceAttack(
                            bounceSource,
                            *target,
                            *nextTarget,
                            bounceAttackId,
                            castLifecycle),
                        attack.id,
                        nextTarget->id,
                    });
                    attack.scheduledFinishReason = AttackFinishReason::SpentOnHit;
                }
                else
                {
                    attack.scheduledFinishReason = AttackFinishReason::NoBounceTarget;
                    events.push_back(makeAttackEvent(
                        BattleAttackEventType::ChainNoTargetInRange,
                        attack,
                        target->id));
                }
            }

            if (spendNonThroughOnHit && !attack.state.through)
            {
                attack.noHurt = true;
                attack.frame = std::max(attack.state.totalFrame - 15, attack.frame);
                if (!attack.scheduledFinishReason)
                {
                    attack.scheduledFinishReason = AttackFinishReason::SpentOnHit;
                }
            }
        }

        if (attack.frame >= attack.state.totalFrame)
        {
            if (!attack.scheduledFinishReason)
            {
                attack.scheduledFinishReason = AttackFinishReason::Expired;
            }
            events.push_back(makeAttackEvent(BattleAttackEventType::Expired, attack));
        }
    }

    for (auto& pending : pendingBounces)
    {
        const int attackId = pending.attack.id;
        attacks.push_back(std::move(pending.attack));
        auto& spawned = attacks.back();
        auto event = makeAttackEvent(
            BattleAttackEventType::Bounce,
            requireById(attacks, pending.sourceAttackId),
            pending.targetUnitId);
        event.otherAttackId = attackId;
        assert(spawned.provenance.valid());
        event.otherProvenance = spawned.provenance;
        events.push_back(std::move(event));
    }

    collectProjectileCancelEvents(units, events);
}

void BattleAttackState::applyProjectileCancelDamage(const BattleAttackEvent& event)
{
    assert(event.type == BattleAttackEventType::ProjectileCancel);
    assert(event.attackId >= 0);
    assert(event.otherAttackId >= 0);
    assert(event.projectileCancelDamage >= 0);
    assert(event.otherProjectileCancelDamage >= 0);

    auto& lhs = requireById(attacks, event.attackId);
    auto& rhs = requireById(attacks, event.otherAttackId);
    lhs.state.projectileCancelWeaken += event.otherProjectileCancelDamage;
    rhs.state.projectileCancelWeaken += event.projectileCancelDamage;
    if (lhs.state.projectileCancelWeaken > event.projectileCancelDamage)
    {
        lhs.noHurt = true;
        lhs.frame = std::max(lhs.state.totalFrame - 5, lhs.frame);
        lhs.scheduledFinishReason = AttackFinishReason::ProjectileCancelled;
    }
    if (rhs.state.projectileCancelWeaken > event.otherProjectileCancelDamage)
    {
        rhs.noHurt = true;
        rhs.frame = std::max(rhs.state.totalFrame - 5, rhs.frame);
        rhs.scheduledFinishReason = AttackFinishReason::ProjectileCancelled;
    }
}

bool BattleAttackState::contactsSuppressed(int attackId) const
{
    assert(attackId >= 0);
    return requireById(attacks, attackId).contactsSuppressed;
}

void BattleAttackState::suppressContacts(int attackId)
{
    assert(attackId >= 0);
    requireById(attacks, attackId).contactsSuppressed = true;

    // Bounce attacks can already have been created by tick() before Core
    // accepts or suppresses the contact that produced them. Propagate across
    // the live chain here; makeBounceAttack() carries the state to any later
    // descendants.
    bool changed;
    do
    {
        changed = false;
        for (auto& attack : attacks)
        {
            if (attack.contactsSuppressed || !attack.provenance.parentAttackId)
            {
                continue;
            }
            const auto parent = std::ranges::find_if(
                attacks,
                [&attack](const BattleAttackInstance& candidate)
                {
                    return candidate.provenance.attackId
                        == *attack.provenance.parentAttackId;
                });
            if (parent != attacks.end() && parent->contactsSuppressed)
            {
                attack.contactsSuppressed = true;
                changed = true;
            }
        }
    } while (changed);
}

bool BattleAttackState::castContactsSuppressed(BattleCastId castId) const
{
    assert(castId.valid());
    return suppressedContactCastIds.contains(castId);
}

void BattleAttackState::suppressContactsForCast(BattleCastId castId)
{
    assert(castId.valid());
    suppressedContactCastIds.insert(castId);
    for (auto& attack : attacks)
    {
        if (attack.provenance.cast.castId == castId)
        {
            attack.contactsSuppressed = true;
        }
    }
}

void BattleAttackState::releaseCastContactSuppression(BattleCastId castId)
{
    assert(castId.valid());
    suppressedContactCastIds.erase(castId);
}

void BattleAttackState::clearCastContactSuppressions()
{
    suppressedContactCastIds.clear();
}

void BattleAttackState::completeFinished(BattleCastLifecycle& castLifecycle)
{
    for (auto& attack : attacks)
    {
        if (attack.frame < attack.state.totalFrame)
        {
            continue;
        }
        assert(attack.scheduledFinishReason);
        finishAttack(attack, *attack.scheduledFinishReason, castLifecycle);
    }
}

void BattleAttackState::eraseFinished()
{
    attacks.erase(
        std::remove_if(
            attacks.begin(),
            attacks.end(),
            [](const BattleAttackInstance& attack)
            {
                if (attack.frame < attack.state.totalFrame)
                {
                    assert(attack.provenance.valid());
                    assert(attack.castWork.valid());
                    return false;
                }
                assert(attack.provenance.valid());
                assert(attack.finishReason);
                assert(!attack.castWork.valid());
                return true;
            }),
        attacks.end());
}

void BattleAttackState::pruneFinished(BattleCastLifecycle& castLifecycle)
{
    completeFinished(castLifecycle);
    eraseFinished();
}

void BattleAttackState::cancelAllForBattleEnd(BattleCastLifecycle& castLifecycle)
{
    for (auto& attack : attacks)
    {
        if (attack.finishReason)
        {
            assert(!attack.castWork.valid());
            continue;
        }
        finishAttack(attack, AttackFinishReason::BattleEnded, castLifecycle);
    }
    attacks.clear();
    sharedHitGroupTargets.clear();
}

int BattleAttackState::allocateAttackId()
{
    assert(nextAttackId >= 0);
    if (nextAttackId == 0 && !attacks.empty())
    {
        auto maxId = attacks.front().id;
        for (const auto& attack : attacks)
        {
            assert(attack.id >= 0);
            maxId = std::max(maxId, attack.id);
        }
        nextAttackId = maxId + 1;
    }
    return nextAttackId++;
}

const BattleRuntimeUnit* BattleAttackState::selectTarget(
    const BattleRuntimeUnits& units,
    const BattleAttackInstance& attack) const
{
    const auto& attacker = units.requireCore(attack.state.attackSourceUnitId);
    if (!attacker.alive && !canResolveContactFromDefeatedSource(attack.state))
    {
        return nullptr;
    }

    if (attack.state.preferredTargetUnitId != OptionalPreferredTargetUnitId)
    {
        assert(attack.state.preferredTargetUnitId >= 0);
        const auto& preferred = units.requireCore(attack.state.preferredTargetUnitId);
        if (preferred.alive && preferred.team != attacker.team)
        {
            return &preferred;
        }
        if (attack.state.requirePreferredTarget)
        {
            return nullptr;
        }
    }

    const BattleRuntimeUnit* best = nullptr;
    std::uint64_t bestDistanceSquared{};
    for (const auto& unitRecord : units.live())
    {
        const auto& unit = unitRecord.core;
        if (unit.team == attacker.team)
        {
            continue;
        }
        const std::uint64_t candidateDistanceSquared = battleDistanceSquared2d(
            unit.motion.position,
            attack.state.position);
        if (!best
            || candidateDistanceSquared < bestDistanceSquared
            || (candidateDistanceSquared == bestDistanceSquared && unit.id < best->id))
        {
            best = &unit;
            bestDistanceSquared = candidateDistanceSquared;
        }
    }
    return best;
}

bool BattleAttackState::hasHitUnit(const BattleAttackInstance& attack, int unitId) const
{
    return std::find(attack.hitUnitIds.begin(), attack.hitUnitIds.end(), unitId) != attack.hitUnitIds.end();
}

bool BattleAttackState::hasInvincibleBlockedUnit(const BattleAttackInstance& attack, int unitId) const
{
    return std::find(
        attack.invincibleBlockedUnitIds.begin(),
        attack.invincibleBlockedUnitIds.end(),
        unitId) != attack.invincibleBlockedUnitIds.end();
}

bool BattleAttackState::hasSharedHit(
    int sharedHitGroupId,
    int unitId) const
{
    if (sharedHitGroupId <= 0)
    {
        return false;
    }
    auto it = sharedHitGroupTargets.find(sharedHitGroupId);
    if (it == sharedHitGroupTargets.end())
    {
        return false;
    }
    return std::find(it->second.begin(), it->second.end(), unitId) != it->second.end();
}

void BattleAttackState::markHit(BattleAttackInstance& attack, int unitId)
{
    assert(unitId >= 0);
    assert(!hasHitUnit(attack, unitId));
    attack.hitUnitIds.push_back(unitId);
    const auto sharedHitGroupId = attack.provenance.sharedHitGroupId;
    if (sharedHitGroupId > 0)
    {
        auto& sharedHits = sharedHitGroupTargets[sharedHitGroupId];
        assert(std::find(sharedHits.begin(), sharedHits.end(), unitId) == sharedHits.end());
        sharedHits.push_back(unitId);
    }
}

void BattleAttackState::markInvincibleBlocked(BattleAttackInstance& attack, int unitId) const
{
    assert(unitId >= 0);
    assert(!hasInvincibleBlockedUnit(attack, unitId));
    attack.invincibleBlockedUnitIds.push_back(unitId);
}

void BattleAttackState::moveAttack(BattleAttackInstance& attack) const
{
    const Pointf positionBeforeMove = attack.state.position;
    if (attack.spiralMotion)
    {
        attack.spiralRadius += attack.spiralRadiusGrowth;
        attack.spiralAngle += attack.spiralAngularVelocity;
        const auto [sine, cosine] = deterministicSinCos(attack.spiralAngle);
        attack.state.position = attack.spiralCenter + Pointf{
            static_cast<float>(cosine * attack.spiralRadius),
            static_cast<float>(sine * attack.spiralRadius),
            0.0f,
        };
        attack.state.velocity = {
            static_cast<float>(cosine * attack.spiralRadiusGrowth),
            static_cast<float>(sine * attack.spiralRadiusGrowth),
            0.0f,
        };
        attack.previousPosition = positionBeforeMove;
        return;
    }

    attack.state.velocity += attack.acceleration;
    attack.state.position += attack.state.velocity;
    attack.previousPosition = positionBeforeMove;
    if (attack.frame == 1
        && attack.state.preferredTargetUnitId >= 0
        && isProjectileOperation(attack.state.operationType))
    {
        attack.previousPosition = positionBeforeMove - attack.state.velocity;
    }
}

void BattleAttackState::trackTarget(
    BattleAttackInstance& attack,
    const BattleRuntimeUnit& target) const
{
    const double speed = attack.state.velocity.norm();
    if (speed <= minimumVectorNorm)
    {
        return;
    }
    auto correction = normalizedTo(target.motion.position - attack.state.position, speed / 20.0, minimumVectorNorm);
    attack.state.velocity += correction;
    attack.state.velocity = normalizedTo(attack.state.velocity, speed, minimumVectorNorm);
}

bool BattleAttackState::canContactTarget(
    const BattleRuntimeUnits& units,
    const BattleAttackInstance& attack,
    const BattleRuntimeUnit& target) const
{
    if (attack.noHurt
        || !target.alive
        || hasHitUnit(attack, target.id)
        || hasSharedHit(attack.provenance.sharedHitGroupId, target.id))
    {
        return false;
    }

    const auto& attacker = units.requireCore(attack.state.attackSourceUnitId);
    if ((!attacker.alive && !canResolveContactFromDefeatedSource(attack.state)) || attacker.team == target.team)
    {
        return false;
    }

    return battlePointSegmentWithinRadius(
        target.motion.position,
        attack.previousPosition,
        attack.state.position,
        hitRadius);
}

bool BattleAttackState::contactBlockedByInvincible(
    const BattleRuntimeUnits& units,
    const BattleAttackInstance& attack,
    const BattleRuntimeUnit& target) const
{
    return target.invincible > 0
        && !attack.state.executeCanHitInvincible
        && !hasInvincibleBlockedUnit(attack, target.id)
        && canContactTarget(units, attack, target);
}

bool BattleAttackState::canHit(
    const BattleRuntimeUnits& units,
    const BattleAttackInstance& attack,
    const BattleRuntimeUnit& target) const
{
    if (target.invincible > 0 && !attack.state.executeCanHitInvincible)
    {
        return false;
    }

    return canContactTarget(units, attack, target);
}

const BattleRuntimeUnit* BattleAttackState::selectBounceTarget(
    const BattleRuntimeUnits& units,
    const BattleAttackInstance& attack,
    const BattleRuntimeUnit& hitTarget) const
{
    assert(attack.state.bounceRemaining > 0);
    assert(attack.state.bounceRange > 0);

    const auto& attacker = units.requireCore(attack.state.attackSourceUnitId);

    const BattleRuntimeUnit* best = nullptr;
    const std::uint64_t maximumDistanceSquared = battleDistanceSquared2d(
        {},
        {static_cast<float>(attack.state.bounceRange), 0.0f, 0.0f});
    std::uint64_t bestDistanceSquared = maximumDistanceSquared;
    for (const auto& unitRecord : units.live())
    {
        const auto& unit = unitRecord.core;
        if (unit.team == attacker.team
            || unit.id == hitTarget.id
            || hasHitUnit(attack, unit.id)
            || hasSharedHit(attack.provenance.sharedHitGroupId, unit.id))
        {
            continue;
        }

        const std::uint64_t candidateDistanceSquared = battleDistanceSquared2d(
            hitTarget.motion.position,
            unit.motion.position);
        if (candidateDistanceSquared > maximumDistanceSquared)
        {
            continue;
        }
        if (!best
            || candidateDistanceSquared < bestDistanceSquared
            || (candidateDistanceSquared == bestDistanceSquared && unit.id < best->id))
        {
            best = &unit;
            bestDistanceSquared = candidateDistanceSquared;
        }
    }
    return best;
}

BattleAttackInstance BattleAttackState::makeBounceAttack(
    const BattleAttackInstance& source,
    const BattleRuntimeUnit& hitTarget,
    const BattleRuntimeUnit& nextTarget,
    int attackId,
    BattleCastLifecycle& castLifecycle) const
{
    assert(attackId >= 0);

    auto bounce = source;
    bounce.id = attackId;
    bounce.provenance = {};
    bounce.castWork = {};
    bounce.scheduledFinishReason.reset();
    bounce.finishReason.reset();
    assert(source.provenance.valid());
    assert(source.castWork.valid());
    BattleAttackReservationRequest request;
    request.parentAttackId = source.provenance.attackId;
    request.origin = BattleAttackOriginKind::Bounce;
    request.mainProjectile = source.provenance.mainProjectile;
    request.sharedHitGroupId = source.provenance.sharedHitGroupId;
    request.propagation = source.provenance.propagation
            == CastPropagationPolicy::SourceRules
        ? CastPropagationPolicy::SourceHitRulesOnly
        : source.provenance.propagation;
    const auto reservation = castLifecycle.reserveAttack(
        source.provenance.cast.castId,
        request);
    bounce.provenance = completeAttackProvenance(
        reservation.provenance,
        battleAttackIdFromRuntimeId(attackId));
    bounce.castWork = reservation.work;
    castLifecycle.transferToLiveAttack(
        reservation.work,
        bounce.provenance.attackId);
    bounce.state.preferredTargetUnitId = nextTarget.id;
    bounce.state.requirePreferredTarget = true;
    bounce.state.track = true;
    bounce.state.through = false;
    bounce.noHurt = false;
    bounce.contactsSuppressed = source.contactsSuppressed;
    bounce.state.ignoreProjectileCancel = true;
    bounce.frame = 0;
    bounce.state.bounceRemaining = std::max(0, source.state.bounceRemaining - 1);

    double speed = source.state.velocity.norm();
    if (speed <= minimumVectorNorm)
    {
        speed = defaultProjectileSpeed;
    }

    auto direction = normalizedTo(nextTarget.motion.position - hitTarget.motion.position, 1.0, minimumVectorNorm);
    if (direction.norm() <= minimumVectorNorm)
    {
        direction = normalizedTo(nextTarget.motion.position - source.state.position, 1.0, minimumVectorNorm);
    }
    if (direction.norm() <= minimumVectorNorm)
    {
        direction = { 1.0f, 0.0f, 0.0f };
    }

    const auto spawnDistance = static_cast<float>(bounceSpawnDistance);
    bounce.state.position = hitTarget.motion.position + Pointf{
        direction.x * spawnDistance,
        direction.y * spawnDistance,
        direction.z * spawnDistance,
    };
    bounce.state.velocity = normalizedTo(nextTarget.motion.position - bounce.state.position, speed, minimumVectorNorm);
    if (bounce.state.velocity.norm() <= minimumVectorNorm)
    {
        bounce.state.velocity = normalizedTo(direction, speed, minimumVectorNorm);
    }
    bounce.state.totalFrame = std::max(
        minimumBounceTotalFrame,
        battleTravelFrames2d(bounce.state.position, nextTarget.motion.position, speed) + 20);
    return bounce;
}

void BattleAttackState::finishAttack(
    BattleAttackInstance& attack,
    AttackFinishReason reason,
    BattleCastLifecycle& castLifecycle)
{
    assert(attack.id >= 0);
    assert(attack.provenance.valid());
    assert(attack.provenance.attackId == battleAttackIdFromRuntimeId(attack.id));
    assert(attack.castWork.valid());
    assert(attack.castWork.castId == attack.provenance.cast.castId);
    assert(!attack.finishReason);

    attack.finishReason = reason;
    castLifecycle.completeWork(
        attack.castWork,
        CastWorkResult::attackFinished(reason));
    attack.castWork = {};
}

void BattleAttackState::collectProjectileCancelEvents(
    const BattleRuntimeUnits& units,
    std::pmr::vector<BattleAttackEvent>& events) const
{
    auto* memoryResource = events.get_allocator().resource();
    std::pmr::vector<ProjectileCancelSearchCandidate> searchCandidates(memoryResource);
    searchCandidates.reserve(attacks.size());
    for (const auto& attack : attacks)
    {
        if (attack.noHurt
            || attack.state.ignoreProjectileCancel
            || attack.frame < projectileGraceFrames
            || attack.provenance.cast.ultimate)
        {
            continue;
        }
        const auto& attacker = units.requireCore(attack.state.attackSourceUnitId);
        searchCandidates.push_back(makeProjectileCancelSearchCandidate(attack, attacker.team, hitRadius));
    }
    if (searchCandidates.size() < 2)
    {
        return;
    }

    std::sort(searchCandidates.begin(), searchCandidates.end(), projectileCancelSearchCandidateLess);

    std::pmr::vector<ProjectileCancelCandidate> candidates(memoryResource);
    for (size_t i = 0; i + 1 < searchCandidates.size(); ++i)
    {
        const auto& lhs = searchCandidates[i];
        for (size_t j = i + 1; j < searchCandidates.size(); ++j)
        {
            const auto& rhs = searchCandidates[j];
            if (rhs.minX > lhs.maxX)
            {
                break;
            }
            if (lhs.team == rhs.team)
            {
                continue;
            }
            if (!segmentBoundsCanOverlap(lhs, rhs))
            {
                continue;
            }
            if (battleSegmentsWithinRadius(
                    lhs.attack->previousPosition,
                    lhs.attack->state.position,
                    rhs.attack->previousPosition,
                    rhs.attack->state.position,
                    hitRadius,
                    false))
            {
                candidates.push_back(makeProjectileCancelCandidate(*lhs.attack, *rhs.attack));
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), betterProjectileCancelCandidate);
    std::pmr::unordered_set<int> usedAttackIds(memoryResource);
    usedAttackIds.reserve(candidates.size() * 2);
    for (auto& candidate : candidates)
    {
        if (usedAttackIds.contains(candidate.event.attackId)
            || usedAttackIds.contains(candidate.event.otherAttackId))
        {
            continue;
        }
        usedAttackIds.insert(candidate.event.attackId);
        usedAttackIds.insert(candidate.event.otherAttackId);
        events.push_back(std::move(candidate.event));
    }
}

}  // namespace KysChess::Battle
