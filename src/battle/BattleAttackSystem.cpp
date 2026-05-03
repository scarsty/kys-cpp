#include "BattleAttackSystem.h"

#include "BattleMath.h"

#include <algorithm>
#include <cassert>
#include <cmath>

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
}  // namespace

BattleAttackEvent BattleAttackSystem::spawn(
    BattleAttackWorld& world,
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
    attack.frame = request.initialFrame;
    world.attacks.push_back(attack);

    BattleAttackEvent event;
    event.type = BattleAttackEventType::AttackSpawned;
    event.attackId = attack.id;
    event.unitId = attack.state.preferredTargetUnitId;
    event.sourceUnitId = attack.state.attackerUnitId;
    event.skillId = attack.state.skillId;
    event.operationType = attack.state.operationType;
    event.visualEffectId = attack.state.visualEffectId;
    event.position = attack.state.position;
    event.velocity = attack.state.velocity;
    event.totalFrame = attack.state.totalFrame;
    return event;
}

std::vector<BattleAttackEvent> BattleAttackSystem::tick(BattleAttackWorld& world) const
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

        const auto* target = selectTarget(world, attack);
        if (!target && attack.state.requirePreferredTarget)
        {
            attack.noHurt = true;
            attack.frame = std::max(attack.state.totalFrame - 5, attack.frame);
            events.push_back({ BattleAttackEventType::TargetLost, attack.id });
        }

        if (target && attack.state.track)
        {
            trackTarget(attack, *target, world.minimumVectorNorm);
        }

        if (target && canHit(world, attack, *target))
        {
            markHit(world, attack, target->id);
            events.push_back({ BattleAttackEventType::Hit, attack.id, -1, target->id });

            if (attack.state.bounceRemaining > 0)
            {
                auto bounceSource = attack;
                const auto* nextTarget = attack.state.bounceChancePct > 0
                    && attack.state.bounceRollPct < attack.state.bounceChancePct
                    ? selectBounceTarget(world, attack, *target)
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

    collectProjectileCancelEvents(world, events);
    return events;
}

int BattleAttackSystem::allocateAttackId(BattleAttackWorld& world) const
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

const BattleAttackUnit* BattleAttackSystem::unitById(const BattleAttackWorld& world, int unitId) const
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleAttackUnit& unit)
        {
            return unit.id == unitId;
        });
    return it == world.units.end() ? nullptr : &*it;
}

const BattleAttackUnit* BattleAttackSystem::selectTarget(
    const BattleAttackWorld& world,
    const BattleAttackInstance& attack) const
{
    const auto* attacker = unitById(world, attack.state.attackerUnitId);
    if (!attacker || !attacker->alive)
    {
        return nullptr;
    }

    if (attack.state.preferredTargetUnitId >= 0)
    {
        const auto* preferred = unitById(world, attack.state.preferredTargetUnitId);
        if (preferred && preferred->alive && preferred->team != attacker->team)
        {
            return preferred;
        }
        if (attack.state.requirePreferredTarget)
        {
            return nullptr;
        }
    }

    const BattleAttackUnit* best = nullptr;
    double bestDistance = 0.0;
    for (const auto& unit : world.units)
    {
        if (!unit.alive || unit.team == attacker->team)
        {
            continue;
        }
        const double candidateDistance = pointDistance(unit.position, attack.state.position);
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

bool BattleAttackSystem::hasSharedHit(
    const BattleAttackWorld& world,
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

void BattleAttackSystem::markHit(BattleAttackWorld& world, BattleAttackInstance& attack, int unitId) const
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

void BattleAttackSystem::moveAttack(BattleAttackInstance& attack) const
{
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
        return;
    }

    attack.state.velocity += attack.acceleration;
    attack.state.position += attack.state.velocity;
}

void BattleAttackSystem::trackTarget(
    BattleAttackInstance& attack,
    const BattleAttackUnit& target,
    double minimumVectorNorm) const
{
    const double speed = pointNorm(attack.state.velocity);
    if (speed <= minimumVectorNorm)
    {
        return;
    }
    auto correction = normalizedTo(target.position - attack.state.position, speed / 20.0, minimumVectorNorm);
    attack.state.velocity += correction;
    attack.state.velocity = normalizedTo(attack.state.velocity, speed, minimumVectorNorm);
}

bool BattleAttackSystem::canHit(
    const BattleAttackWorld& world,
    const BattleAttackInstance& attack,
    const BattleAttackUnit& target) const
{
    if (attack.noHurt
        || target.hurtFrame
        || !target.alive
        || hasHitUnit(attack, target.id)
        || hasSharedHit(world, attack.state.sharedHitGroupId, target.id))
    {
        return false;
    }

    const auto* attacker = unitById(world, attack.state.attackerUnitId);
    if (!attacker || !attacker->alive || attacker->team == target.team)
    {
        return false;
    }

    if (target.invincible && !attack.state.executeCanHitInvincible)
    {
        return false;
    }

    return pointDistance(target.position, attack.state.position) <= world.hitRadius;
}

const BattleAttackUnit* BattleAttackSystem::selectBounceTarget(
    const BattleAttackWorld& world,
    const BattleAttackInstance& attack,
    const BattleAttackUnit& hitTarget) const
{
    assert(attack.state.bounceRemaining > 0);
    assert(attack.state.bounceRange > 0);

    const auto* attacker = unitById(world, attack.state.attackerUnitId);
    if (!attacker || !attacker->alive)
    {
        return nullptr;
    }

    const BattleAttackUnit* best = nullptr;
    double bestDistance = static_cast<double>(attack.state.bounceRange);
    for (const auto& unit : world.units)
    {
        if (!unit.alive
            || unit.team == attacker->team
            || unit.id == hitTarget.id
            || hasHitUnit(attack, unit.id)
            || hasSharedHit(world, attack.state.sharedHitGroupId, unit.id))
        {
            continue;
        }

        const double candidateDistance = pointDistance(hitTarget.position, unit.position);
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
    const BattleAttackWorld& world,
    const BattleAttackInstance& source,
    const BattleAttackUnit& hitTarget,
    const BattleAttackUnit& nextTarget,
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

    double speed = pointNorm(source.state.velocity);
    if (speed <= world.minimumVectorNorm)
    {
        speed = world.defaultProjectileSpeed;
    }

    auto direction = normalizedTo(nextTarget.position - hitTarget.position, 1.0, world.minimumVectorNorm);
    if (pointNorm(direction) <= world.minimumVectorNorm)
    {
        direction = normalizedTo(nextTarget.position - source.state.position, 1.0, world.minimumVectorNorm);
    }
    if (pointNorm(direction) <= world.minimumVectorNorm)
    {
        direction = { 1.0f, 0.0f, 0.0f };
    }

    const auto spawnDistance = static_cast<float>(world.bounceSpawnDistance);
    bounce.state.position = hitTarget.position + Pointf{
        direction.x * spawnDistance,
        direction.y * spawnDistance,
        direction.z * spawnDistance,
    };
    bounce.state.velocity = normalizedTo(nextTarget.position - bounce.state.position, speed, world.minimumVectorNorm);
    if (pointNorm(bounce.state.velocity) <= world.minimumVectorNorm)
    {
        bounce.state.velocity = normalizedTo(direction, speed, world.minimumVectorNorm);
    }
    bounce.state.totalFrame = std::max(
        world.minimumBounceTotalFrame,
        static_cast<int>(std::ceil(pointDistance(nextTarget.position, bounce.state.position) / speed)) + 20);
    return bounce;
}

void BattleAttackSystem::collectProjectileCancelEvents(
    const BattleAttackWorld& world,
    std::vector<BattleAttackEvent>& events) const
{
    for (size_t i = 0; i + 1 < world.attacks.size(); ++i)
    {
        const auto& lhs = world.attacks[i];
        if (lhs.noHurt || lhs.state.ignoreProjectileCancel || lhs.frame < world.projectileGraceFrames)
        {
            continue;
        }
        const auto* lhsAttacker = unitById(world, lhs.state.attackerUnitId);
        if (!lhsAttacker || !lhsAttacker->alive)
        {
            continue;
        }

        for (size_t j = i + 1; j < world.attacks.size(); ++j)
        {
            const auto& rhs = world.attacks[j];
            if (rhs.noHurt || rhs.state.ignoreProjectileCancel || rhs.frame < world.projectileGraceFrames)
            {
                continue;
            }
            const auto* rhsAttacker = unitById(world, rhs.state.attackerUnitId);
            if (!rhsAttacker || !rhsAttacker->alive || lhsAttacker->team == rhsAttacker->team)
            {
                continue;
            }
            if (lhs.state.ultimate || rhs.state.ultimate)
            {
                continue;
            }
            if (pointDistance(lhs.state.position, rhs.state.position) < world.hitRadius)
            {
                events.push_back({ BattleAttackEventType::ProjectileCancel, lhs.id, rhs.id });
            }
        }
    }
}

}  // namespace KysChess::Battle
