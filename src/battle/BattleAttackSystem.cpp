#include "BattleAttackSystem.h"

#include "../Find.h"
#include "BattleMath.h"
#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <unordered_set>
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

bool isProjectileOperation(BattleOperationType operationType)
{
    return operationType == BattleOperationType::RangedProjectile
        || operationType == BattleOperationType::TrackingProjectile;
}

double pointSegmentDistance(Pointf point, Pointf segmentStart, Pointf segmentEnd)
{
    const double dx = static_cast<double>(segmentEnd.x) - segmentStart.x;
    const double dy = static_cast<double>(segmentEnd.y) - segmentStart.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.0)
    {
        return pointDistance(point, segmentEnd);
    }

    const double px = static_cast<double>(point.x) - segmentStart.x;
    const double py = static_cast<double>(point.y) - segmentStart.y;
    const double t = std::clamp((px * dx + py * dy) / lengthSquared, 0.0, 1.0);
    Pointf closest{
        static_cast<float>(segmentStart.x + dx * t),
        static_cast<float>(segmentStart.y + dy * t),
        0.0f,
    };
    return pointDistance(point, closest);
}

double cross2d(Pointf origin, Pointf lhs, Pointf rhs)
{
    return (static_cast<double>(lhs.x) - origin.x) * (static_cast<double>(rhs.y) - origin.y)
        - (static_cast<double>(lhs.y) - origin.y) * (static_cast<double>(rhs.x) - origin.x);
}

bool rangesOverlap(double lhsStart, double lhsEnd, double rhsStart, double rhsEnd)
{
    if (lhsStart > lhsEnd)
    {
        std::swap(lhsStart, lhsEnd);
    }
    if (rhsStart > rhsEnd)
    {
        std::swap(rhsStart, rhsEnd);
    }
    return std::max(lhsStart, rhsStart) <= std::min(lhsEnd, rhsEnd);
}

bool segmentsIntersect(Pointf lhsStart, Pointf lhsEnd, Pointf rhsStart, Pointf rhsEnd)
{
    constexpr double Epsilon = 0.000001;
    const double lhsCrossStart = cross2d(lhsStart, lhsEnd, rhsStart);
    const double lhsCrossEnd = cross2d(lhsStart, lhsEnd, rhsEnd);
    const double rhsCrossStart = cross2d(rhsStart, rhsEnd, lhsStart);
    const double rhsCrossEnd = cross2d(rhsStart, rhsEnd, lhsEnd);

    if (std::abs(lhsCrossStart) <= Epsilon
        && std::abs(lhsCrossEnd) <= Epsilon
        && std::abs(rhsCrossStart) <= Epsilon
        && std::abs(rhsCrossEnd) <= Epsilon)
    {
        return rangesOverlap(lhsStart.x, lhsEnd.x, rhsStart.x, rhsEnd.x)
            && rangesOverlap(lhsStart.y, lhsEnd.y, rhsStart.y, rhsEnd.y);
    }

    return lhsCrossStart * lhsCrossEnd <= Epsilon
        && rhsCrossStart * rhsCrossEnd <= Epsilon;
}

double segmentSegmentDistance(Pointf lhsStart, Pointf lhsEnd, Pointf rhsStart, Pointf rhsEnd)
{
    if (segmentsIntersect(lhsStart, lhsEnd, rhsStart, rhsEnd))
    {
        return 0.0;
    }

    return std::min({
        pointSegmentDistance(lhsStart, rhsStart, rhsEnd),
        pointSegmentDistance(lhsEnd, rhsStart, rhsEnd),
        pointSegmentDistance(rhsStart, lhsStart, lhsEnd),
        pointSegmentDistance(rhsEnd, lhsStart, lhsEnd),
    });
}

void applyAttackPayload(BattleAttackEvent& event, const BattleAttackPayload& state)
{
    event.sourceUnitId = state.attackerUnitId;
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
    event.scriptedStunFrames = state.scriptedStunFrames;
    event.scriptedBleedStacks = state.scriptedBleedStacks;
    event.executeCanHitInvincible = state.executeCanHitInvincible;
    event.track = state.track;
    event.through = state.through;
    event.ultimate = state.ultimate;
    event.strengthMultiplier = state.strengthMultiplier;
    event.suppressNearbyTrackingProjectileProc = state.suppressNearbyTrackingProjectileProc;
    event.mainProjectile = state.mainProjectile;
    event.sharedHitGroupId = state.sharedHitGroupId;
    event.projectileCancelDamage = state.projectileCancelWeaken;
    event.position = state.position;
    event.velocity = state.velocity;
    event.totalFrame = state.totalFrame;
}

BattleAttackEvent makeProjectileCancelEvent(const BattleAttackInstance& lhs, const BattleAttackInstance& rhs)
{
    BattleAttackEvent event;
    event.type = BattleAttackEventType::ProjectileCancel;
    event.attackId = lhs.id;
    event.otherAttackId = rhs.id;
    event.sourceUnitId = lhs.state.attackerUnitId;
    event.otherSourceUnitId = rhs.state.attackerUnitId;
    event.projectileCancelDamage = scaleProjectileCancelDamage(
        lhs.state.projectileCancelDamage,
        lhs.state.operationType);
    event.otherProjectileCancelDamage = scaleProjectileCancelDamage(
        rhs.state.projectileCancelDamage,
        rhs.state.operationType);
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

bool canResolveContactFromDefeatedSource(const BattleAttackPayload& attack)
{
    return isProjectileOperation(attack.operationType);
}

}  // namespace

double projectileOperationDamageMultiplier(BattleOperationType operationType)
{
    return battleOperationDamageMultiplier(operationType);
}

int scaleProjectileCancelDamage(int damage, BattleOperationType operationType)
{
    if (damage <= 0)
    {
        return 0;
    }

    return std::max(1, static_cast<int>(std::ceil(damage * projectileOperationDamageMultiplier(operationType))));
}

void applyProjectileBouncePrime(BattleAttackSpawnRequest& request, BattleAttackBouncePrime prime)
{
    assert(request.initial.attackerUnitId >= 0);
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
    assert(request.initial.attackerUnitId >= 0);
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

BattleAttackEvent BattleAttackSystem::spawn(
    BattleAttackState& world,
    const BattleAttackSpawnRequest& request) const
{
    assert(request.initial.attackerUnitId >= 0);
    assert(request.initialFrame >= 0);
    assert(request.initial.totalFrame > 0);
    assert(request.initial.bounceRemaining >= 0);
    assert(request.initial.bounceRange >= 0);
    assert(request.initial.bounceChancePct >= 0 && request.initial.bounceChancePct <= 100);
    assert(request.initial.bounceRollPct >= 0 && request.initial.bounceRollPct < 100);

    BattleAttackInstance attack;
    attack.id = allocateAttackId(world);
    attack.state = request.initial;
    attack.previousPosition = request.initial.position;
    attack.frame = request.initialFrame;
    attack.acceleration = request.acceleration;
    attack.spiralMotion = request.spiralMotion;
    attack.spiralCenter = request.spiralCenter;
    attack.spiralRadius = request.spiralRadius;
    attack.spiralRadiusGrowth = request.spiralRadiusGrowth;
    attack.spiralAngle = request.spiralAngle;
    attack.spiralAngularVelocity = request.spiralAngularVelocity;
    world.attacks.push_back(attack);

    BattleAttackEvent event;
    event.type = BattleAttackEventType::AttackSpawned;
    event.attackId = attack.id;
    event.unitId = attack.state.preferredTargetUnitId;
    applyAttackPayload(event, attack.state);
    event.position = attack.state.position;
    event.velocity = attack.state.velocity;
    event.totalFrame = attack.state.totalFrame;
    return event;
}

std::vector<BattleAttackEvent> BattleAttackSystem::tick(
    BattleAttackState& world,
    const BattleRuntimeUnits& units) const
{
    assert(world.hitRadius > 0.0);
    assert(world.minimumVectorNorm > 0.0);
    assert(world.projectileGraceFrames >= 0);
    assert(world.bounceSpawnDistance > 0.0);
    assert(world.defaultProjectileSpeed > 0.0);
    assert(world.minimumBounceTotalFrame > 0);

    std::vector<BattleAttackEvent> events;
    std::vector<PendingBounce> pendingBounces;

    const size_t initialAttackCount = world.attacks.size();
    for (size_t i = 0; i < initialAttackCount; ++i)
    {
        auto& attack = world.attacks[i];
        assert(attack.id >= 0);
        assert(attack.state.totalFrame > 0);
        assert(attack.state.bounceRemaining >= 0);
        assert(attack.state.bounceRange >= 0);
        assert(attack.state.bounceChancePct >= 0 && attack.state.bounceChancePct <= 100);
        assert(attack.state.bounceRollPct >= 0 && attack.state.bounceRollPct < 100);

        ++attack.frame;
        moveAttack(attack);
        events.push_back({ BattleAttackEventType::Moved, attack.id });

        const auto* target = selectTarget(world, units, attack);
        if (!target && attack.state.requirePreferredTarget)
        {
            attack.noHurt = true;
            attack.frame = std::max(attack.state.totalFrame - 5, attack.frame);
            events.push_back({ BattleAttackEventType::TargetLost, attack.id });
        }

        if (target && attack.state.track && attack.hitUnitIds.empty())
        {
            trackTarget(attack, *target, world.minimumVectorNorm);
        }

        if (target && contactBlockedByInvincible(world, units, attack, *target))
        {
            markInvincibleBlocked(attack, target->id);
            BattleAttackEvent blocked;
            blocked.type = BattleAttackEventType::BlockedByInvincible;
            blocked.attackId = attack.id;
            blocked.unitId = target->id;
            applyAttackPayload(blocked, attack.state);
            blocked.frame = attack.frame;
            events.push_back(blocked);
        }
        else if (target && canHit(world, units, attack, *target))
        {
            markHit(world, attack, target->id);
            BattleAttackEvent hit;
            hit.type = BattleAttackEventType::Hit;
            hit.attackId = attack.id;
            hit.unitId = target->id;
            applyAttackPayload(hit, attack.state);
            hit.frame = attack.frame;
            events.push_back(hit);

            if (attack.spawnedFromAttackId >= 0 && attack.state.bounceRemaining == 0)
            {
                events.push_back({ BattleAttackEventType::ChainEnded, attack.id, -1, target->id });
            }
            else if (attack.state.bounceRemaining > 0)
            {
                auto bounceSource = attack;
                const auto* nextTarget = attack.state.bounceChancePct > 0
                    && attack.state.bounceRollPct < attack.state.bounceChancePct
                    ? selectBounceTarget(world, units, attack, *target)
                    : nullptr;
                attack.state.bounceRemaining = 0;
                attack.noHurt = true;
                attack.frame = std::max(attack.state.totalFrame - 15, attack.frame);
                if (nextTarget)
                {
                    const int bounceAttackId = allocateAttackId(world);
                    pendingBounces.push_back({
                        makeBounceAttack(world, bounceSource, *target, *nextTarget, bounceAttackId),
                        attack.id,
                        nextTarget->id,
                    });
                }
                else
                {
                    events.push_back({ BattleAttackEventType::ChainNoTargetInRange, attack.id, -1, target->id });
                }
            }

            if (world.spendNonThroughOnHit && !attack.state.through)
            {
                attack.noHurt = true;
                attack.frame = std::max(attack.state.totalFrame - 15, attack.frame);
            }
        }

        if (attack.frame >= attack.state.totalFrame)
        {
            events.push_back({ BattleAttackEventType::Expired, attack.id });
        }
    }

    for (const auto& pending : pendingBounces)
    {
        world.attacks.push_back(pending.attack);
        events.push_back({
            BattleAttackEventType::Bounce,
            pending.sourceAttackId,
            pending.attack.id,
            pending.targetUnitId,
        });
    }

    collectProjectileCancelEvents(world, units, events);
    return events;
}

void BattleAttackSystem::applyProjectileCancelDamage(
    BattleAttackState& world,
    const BattleAttackEvent& event) const
{
    assert(event.type == BattleAttackEventType::ProjectileCancel);
    assert(event.attackId >= 0);
    assert(event.otherAttackId >= 0);
    assert(event.projectileCancelDamage >= 0);
    assert(event.otherProjectileCancelDamage >= 0);

    auto& lhs = requireById(world.attacks, event.attackId);
    auto& rhs = requireById(world.attacks, event.otherAttackId);
    lhs.state.projectileCancelWeaken += event.otherProjectileCancelDamage;
    rhs.state.projectileCancelWeaken += event.projectileCancelDamage;
    if (lhs.state.projectileCancelWeaken > event.projectileCancelDamage)
    {
        lhs.noHurt = true;
        lhs.frame = std::max(lhs.state.totalFrame - 5, lhs.frame);
    }
    if (rhs.state.projectileCancelWeaken > event.otherProjectileCancelDamage)
    {
        rhs.noHurt = true;
        rhs.frame = std::max(rhs.state.totalFrame - 5, rhs.frame);
    }
}

int BattleAttackSystem::allocateAttackId(BattleAttackState& world) const
{
    assert(world.nextAttackId >= 0);
    if (world.nextAttackId == 0 && !world.attacks.empty())
    {
        auto maxId = world.attacks.front().id;
        for (const auto& attack : world.attacks)
        {
            assert(attack.id >= 0);
            maxId = std::max(maxId, attack.id);
        }
        world.nextAttackId = maxId + 1;
    }
    return world.nextAttackId++;
}

const BattleRuntimeUnit* BattleAttackSystem::selectTarget(
    const BattleAttackState& world,
    const BattleRuntimeUnits& units,
    const BattleAttackInstance& attack) const
{
    const auto& attacker = units.requireCore(attack.state.attackerUnitId);
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
    double bestDistance = 0.0;
    for (const auto& unitRecord : units.live())
    {
        const auto& unit = unitRecord.core;
        if (unit.team == attacker.team)
        {
            continue;
        }
        const double candidateDistance = pointDistance(unit.motion.position, attack.state.position);
        if (!best || candidateDistance < bestDistance)
        {
            best = &unit;
            bestDistance = candidateDistance;
        }
    }
    return best;
}

bool BattleAttackSystem::hasHitUnit(const BattleAttackInstance& attack, int unitId) const
{
    return std::find(attack.hitUnitIds.begin(), attack.hitUnitIds.end(), unitId) != attack.hitUnitIds.end();
}

bool BattleAttackSystem::hasInvincibleBlockedUnit(const BattleAttackInstance& attack, int unitId) const
{
    return std::find(
        attack.invincibleBlockedUnitIds.begin(),
        attack.invincibleBlockedUnitIds.end(),
        unitId) != attack.invincibleBlockedUnitIds.end();
}

bool BattleAttackSystem::hasSharedHit(
    const BattleAttackState& world,
    int sharedHitGroupId,
    int unitId) const
{
    if (sharedHitGroupId <= 0)
    {
        return false;
    }
    auto it = world.sharedHitGroupTargets.find(sharedHitGroupId);
    if (it == world.sharedHitGroupTargets.end())
    {
        return false;
    }
    return std::find(it->second.begin(), it->second.end(), unitId) != it->second.end();
}

void BattleAttackSystem::markHit(BattleAttackState& world, BattleAttackInstance& attack, int unitId) const
{
    assert(unitId >= 0);
    assert(!hasHitUnit(attack, unitId));
    attack.hitUnitIds.push_back(unitId);
    if (attack.state.sharedHitGroupId > 0)
    {
        auto& sharedHits = world.sharedHitGroupTargets[attack.state.sharedHitGroupId];
        assert(std::find(sharedHits.begin(), sharedHits.end(), unitId) == sharedHits.end());
        sharedHits.push_back(unitId);
    }
}

void BattleAttackSystem::markInvincibleBlocked(BattleAttackInstance& attack, int unitId) const
{
    assert(unitId >= 0);
    assert(!hasInvincibleBlockedUnit(attack, unitId));
    attack.invincibleBlockedUnitIds.push_back(unitId);
}

void BattleAttackSystem::moveAttack(BattleAttackInstance& attack) const
{
    const Pointf positionBeforeMove = attack.state.position;
    if (attack.spiralMotion)
    {
        attack.spiralRadius += attack.spiralRadiusGrowth;
        attack.spiralAngle += attack.spiralAngularVelocity;
        attack.state.position = attack.spiralCenter + Pointf{
            static_cast<float>(std::cos(attack.spiralAngle) * attack.spiralRadius),
            static_cast<float>(std::sin(attack.spiralAngle) * attack.spiralRadius),
            0.0f,
        };
        attack.state.velocity = {
            static_cast<float>(std::cos(attack.spiralAngle) * attack.spiralRadiusGrowth),
            static_cast<float>(std::sin(attack.spiralAngle) * attack.spiralRadiusGrowth),
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

void BattleAttackSystem::trackTarget(
    BattleAttackInstance& attack,
    const BattleRuntimeUnit& target,
    double minimumVectorNorm) const
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

bool BattleAttackSystem::canContactTarget(
    const BattleAttackState& world,
    const BattleRuntimeUnits& units,
    const BattleAttackInstance& attack,
    const BattleRuntimeUnit& target) const
{
    if (attack.noHurt
        || !target.alive
        || hasHitUnit(attack, target.id)
        || hasSharedHit(world, attack.state.sharedHitGroupId, target.id))
    {
        return false;
    }

    const auto& attacker = units.requireCore(attack.state.attackerUnitId);
    if ((!attacker.alive && !canResolveContactFromDefeatedSource(attack.state)) || attacker.team == target.team)
    {
        return false;
    }

    return pointSegmentDistance(
        target.motion.position,
        attack.previousPosition,
        attack.state.position) <= world.hitRadius;
}

bool BattleAttackSystem::contactBlockedByInvincible(
    const BattleAttackState& world,
    const BattleRuntimeUnits& units,
    const BattleAttackInstance& attack,
    const BattleRuntimeUnit& target) const
{
    return target.invincible > 0
        && !attack.state.executeCanHitInvincible
        && !hasInvincibleBlockedUnit(attack, target.id)
        && canContactTarget(world, units, attack, target);
}

bool BattleAttackSystem::canHit(
    const BattleAttackState& world,
    const BattleRuntimeUnits& units,
    const BattleAttackInstance& attack,
    const BattleRuntimeUnit& target) const
{
    if (target.invincible > 0 && !attack.state.executeCanHitInvincible)
    {
        return false;
    }

    return canContactTarget(world, units, attack, target);
}

const BattleRuntimeUnit* BattleAttackSystem::selectBounceTarget(
    const BattleAttackState& world,
    const BattleRuntimeUnits& units,
    const BattleAttackInstance& attack,
    const BattleRuntimeUnit& hitTarget) const
{
    assert(attack.state.bounceRemaining > 0);
    assert(attack.state.bounceRange > 0);

    const auto& attacker = units.requireCore(attack.state.attackerUnitId);

    const BattleRuntimeUnit* best = nullptr;
    double bestDistance = static_cast<double>(attack.state.bounceRange);
    for (const auto& unitRecord : units.live())
    {
        const auto& unit = unitRecord.core;
        if (unit.team == attacker.team
            || unit.id == hitTarget.id
            || hasHitUnit(attack, unit.id)
            || hasSharedHit(world, attack.state.sharedHitGroupId, unit.id))
        {
            continue;
        }

        const double candidateDistance = pointDistance(hitTarget.motion.position, unit.motion.position);
        if (candidateDistance > bestDistance)
        {
            continue;
        }
        if (!best || candidateDistance < bestDistance)
        {
            best = &unit;
            bestDistance = candidateDistance;
        }
    }
    return best;
}

BattleAttackInstance BattleAttackSystem::makeBounceAttack(
    const BattleAttackState& world,
    const BattleAttackInstance& source,
    const BattleRuntimeUnit& hitTarget,
    const BattleRuntimeUnit& nextTarget,
    int attackId) const
{
    assert(attackId >= 0);

    auto bounce = source;
    bounce.id = attackId;
    bounce.spawnedFromAttackId = source.id;
    bounce.state.preferredTargetUnitId = nextTarget.id;
    bounce.state.requirePreferredTarget = true;
    bounce.state.track = true;
    bounce.state.through = false;
    bounce.noHurt = false;
    bounce.state.ignoreProjectileCancel = true;
    bounce.frame = 0;
    bounce.state.bounceRemaining = std::max(0, source.state.bounceRemaining - 1);

    double speed = source.state.velocity.norm();
    if (speed <= world.minimumVectorNorm)
    {
        speed = world.defaultProjectileSpeed;
    }

    auto direction = normalizedTo(nextTarget.motion.position - hitTarget.motion.position, 1.0, world.minimumVectorNorm);
    if (direction.norm() <= world.minimumVectorNorm)
    {
        direction = normalizedTo(nextTarget.motion.position - source.state.position, 1.0, world.minimumVectorNorm);
    }
    if (direction.norm() <= world.minimumVectorNorm)
    {
        direction = { 1.0f, 0.0f, 0.0f };
    }

    const auto spawnDistance = static_cast<float>(world.bounceSpawnDistance);
    bounce.state.position = hitTarget.motion.position + Pointf{
        direction.x * spawnDistance,
        direction.y * spawnDistance,
        direction.z * spawnDistance,
    };
    bounce.state.velocity = normalizedTo(nextTarget.motion.position - bounce.state.position, speed, world.minimumVectorNorm);
    if (bounce.state.velocity.norm() <= world.minimumVectorNorm)
    {
        bounce.state.velocity = normalizedTo(direction, speed, world.minimumVectorNorm);
    }
    bounce.state.totalFrame = std::max(
        world.minimumBounceTotalFrame,
        static_cast<int>(std::ceil(pointDistance(nextTarget.motion.position, bounce.state.position) / speed)) + 20);
    return bounce;
}

void BattleAttackSystem::collectProjectileCancelEvents(
    const BattleAttackState& world,
    const BattleRuntimeUnits& units,
    std::vector<BattleAttackEvent>& events) const
{
    std::vector<ProjectileCancelCandidate> candidates;
    for (size_t i = 0; i + 1 < world.attacks.size(); ++i)
    {
        const auto& lhs = world.attacks[i];
        if (lhs.noHurt || lhs.state.ignoreProjectileCancel || lhs.frame < world.projectileGraceFrames)
        {
            continue;
        }
        const auto& lhsAttacker = units.requireCore(lhs.state.attackerUnitId);

        for (size_t j = i + 1; j < world.attacks.size(); ++j)
        {
            const auto& rhs = world.attacks[j];
            if (rhs.noHurt || rhs.state.ignoreProjectileCancel || rhs.frame < world.projectileGraceFrames)
            {
                continue;
            }
            const auto& rhsAttacker = units.requireCore(rhs.state.attackerUnitId);
            if (lhsAttacker.team == rhsAttacker.team)
            {
                continue;
            }
            if (lhs.state.ultimate || rhs.state.ultimate)
            {
                continue;
            }
            if (segmentSegmentDistance(
                    lhs.previousPosition,
                    lhs.state.position,
                    rhs.previousPosition,
                    rhs.state.position) < world.hitRadius)
            {
                candidates.push_back(makeProjectileCancelCandidate(lhs, rhs));
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), betterProjectileCancelCandidate);
    std::unordered_set<int> usedAttackIds;
    usedAttackIds.reserve(candidates.size() * 2);
    for (const auto& candidate : candidates)
    {
        if (usedAttackIds.contains(candidate.event.attackId)
            || usedAttackIds.contains(candidate.event.otherAttackId))
        {
            continue;
        }
        usedAttackIds.insert(candidate.event.attackId);
        usedAttackIds.insert(candidate.event.otherAttackId);
        events.push_back(candidate.event);
    }
}

}  // namespace KysChess::Battle
