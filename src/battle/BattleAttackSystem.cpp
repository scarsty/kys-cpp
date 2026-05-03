#include "BattleAttackSystem.h"

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

double distance(const Pointf& lhs, const Pointf& rhs)
{
    const double dx = static_cast<double>(lhs.x) - rhs.x;
    const double dy = static_cast<double>(lhs.y) - rhs.y;
    return std::sqrt(dx * dx + dy * dy);
}

double norm(const Pointf& point)
{
    return std::sqrt(static_cast<double>(point.x) * point.x
        + static_cast<double>(point.y) * point.y
        + static_cast<double>(point.z) * point.z);
}

Pointf normalizedTo(Pointf point, double length)
{
    const double current = norm(point);
    if (current <= 0.0001)
    {
        return {};
    }
    point.x = static_cast<float>(point.x * length / current);
    point.y = static_cast<float>(point.y * length / current);
    point.z = static_cast<float>(point.z * length / current);
    return point;
}
}  // namespace

BattleAttackEvent BattleAttackSystem::spawn(
    BattleAttackWorld& world,
    const BattleAttackSpawnRequest& request) const
{
    assert(request.attackerUnitId >= 0);
    assert(request.totalFrame > 0);
    assert(request.bounceRemaining >= 0);
    assert(request.bounceRange >= 0);
    assert(request.bounceChancePct >= 0 && request.bounceChancePct <= 100);
    assert(request.bounceRollPct >= 0 && request.bounceRollPct < 100);

    BattleAttackInstance attack;
    attack.id = allocateAttackId(world);
    attack.attackerUnitId = request.attackerUnitId;
    attack.skillId = request.skillId;
    attack.operationKind = request.operationType;
    attack.visualEffectId = request.visualEffectId;
    attack.preferredTargetUnitId = request.preferredTargetUnitId;
    attack.position = request.position;
    attack.velocity = request.velocity;
    attack.totalFrame = request.totalFrame;
    attack.through = request.through;
    attack.track = request.track;
    attack.requirePreferredTarget = request.requirePreferredTarget;
    attack.executeCanHitInvincible = request.executeCanHitInvincible;
    attack.ignoreProjectileCancel = request.ignoreProjectileCancel;
    attack.sharedHitGroupId = request.sharedHitGroupId;
    attack.bounceRemaining = request.bounceRemaining;
    attack.bounceRange = request.bounceRange;
    attack.bounceChancePct = request.bounceChancePct;
    attack.bounceRollPct = request.bounceRollPct;
    world.attacks.push_back(attack);

    BattleAttackEvent event;
    event.type = BattleAttackEventType::AttackSpawned;
    event.attackId = attack.id;
    event.unitId = attack.preferredTargetUnitId;
    event.sourceUnitId = attack.attackerUnitId;
    event.skillId = attack.skillId;
    event.operationType = request.operationType;
    event.visualEffectId = attack.visualEffectId;
    event.position = attack.position;
    event.velocity = attack.velocity;
    event.totalFrame = attack.totalFrame;
    return event;
}

std::vector<BattleAttackEvent> BattleAttackSystem::tick(BattleAttackWorld& world) const
{
    assert(world.hitRadius > 0.0);
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
        assert(attack.totalFrame > 0);
        assert(attack.bounceRemaining >= 0);
        assert(attack.bounceRange >= 0);
        assert(attack.bounceChancePct >= 0 && attack.bounceChancePct <= 100);
        assert(attack.bounceRollPct >= 0 && attack.bounceRollPct < 100);

        ++attack.frame;
        moveAttack(attack);
        events.push_back({ BattleAttackEventType::Moved, attack.id });

        const auto* target = selectTarget(world, attack);
        if (!target && attack.requirePreferredTarget)
        {
            attack.noHurt = true;
            attack.frame = std::max(attack.totalFrame - 5, attack.frame);
            events.push_back({ BattleAttackEventType::TargetLost, attack.id });
        }

        if (target && attack.track)
        {
            trackTarget(attack, *target);
        }

        if (target && canHit(world, attack, *target))
        {
            markHit(world, attack, target->id);
            events.push_back({ BattleAttackEventType::Hit, attack.id, -1, target->id });

            if (attack.bounceRemaining > 0)
            {
                auto bounceSource = attack;
                const auto* nextTarget = attack.bounceChancePct > 0
                    && attack.bounceRollPct < attack.bounceChancePct
                    ? selectBounceTarget(world, attack, *target)
                    : nullptr;
                attack.bounceRemaining = 0;
                attack.noHurt = true;
                attack.frame = std::max(attack.totalFrame - 15, attack.frame);
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

            if (world.spendNonThroughOnHit && !attack.through)
            {
                attack.noHurt = true;
                attack.frame = std::max(attack.totalFrame - 15, attack.frame);
            }
        }

        if (attack.frame >= attack.totalFrame)
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
    const auto* attacker = unitById(world, attack.attackerUnitId);
    if (!attacker || !attacker->alive)
    {
        return nullptr;
    }

    if (attack.preferredTargetUnitId >= 0)
    {
        const auto* preferred = unitById(world, attack.preferredTargetUnitId);
        if (preferred && preferred->alive && preferred->team != attacker->team)
        {
            return preferred;
        }
        if (attack.requirePreferredTarget)
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
        const double candidateDistance = distance(unit.position, attack.position);
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
    if (attack.sharedHitGroupId > 0)
    {
        auto& sharedHits = world.sharedHitGroupTargets[attack.sharedHitGroupId];
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
        attack.position = attack.spiralCenter + Pointf{
            static_cast<float>(std::cos(attack.spiralAngle) * attack.spiralRadius),
            static_cast<float>(std::sin(attack.spiralAngle) * attack.spiralRadius),
            0.0f,
        };
        attack.velocity = {
            static_cast<float>(std::cos(attack.spiralAngle) * attack.spiralRadiusGrowth),
            static_cast<float>(std::sin(attack.spiralAngle) * attack.spiralRadiusGrowth),
            0.0f,
        };
        return;
    }

    attack.velocity += attack.acceleration;
    attack.position += attack.velocity;
}

void BattleAttackSystem::trackTarget(BattleAttackInstance& attack, const BattleAttackUnit& target) const
{
    const double speed = norm(attack.velocity);
    if (speed <= 0.0001)
    {
        return;
    }
    auto correction = normalizedTo(target.position - attack.position, speed / 20.0);
    attack.velocity += correction;
    attack.velocity = normalizedTo(attack.velocity, speed);
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
        || hasSharedHit(world, attack.sharedHitGroupId, target.id))
    {
        return false;
    }

    const auto* attacker = unitById(world, attack.attackerUnitId);
    if (!attacker || !attacker->alive || attacker->team == target.team)
    {
        return false;
    }

    if (target.invincible && !attack.executeCanHitInvincible)
    {
        return false;
    }

    return distance(target.position, attack.position) <= world.hitRadius;
}

const BattleAttackUnit* BattleAttackSystem::selectBounceTarget(
    const BattleAttackWorld& world,
    const BattleAttackInstance& attack,
    const BattleAttackUnit& hitTarget) const
{
    assert(attack.bounceRemaining > 0);
    assert(attack.bounceRange > 0);

    const auto* attacker = unitById(world, attack.attackerUnitId);
    if (!attacker || !attacker->alive)
    {
        return nullptr;
    }

    const BattleAttackUnit* best = nullptr;
    double bestDistance = static_cast<double>(attack.bounceRange);
    for (const auto& unit : world.units)
    {
        if (!unit.alive
            || unit.team == attacker->team
            || unit.id == hitTarget.id
            || hasHitUnit(attack, unit.id)
            || hasSharedHit(world, attack.sharedHitGroupId, unit.id))
        {
            continue;
        }

        const double candidateDistance = distance(hitTarget.position, unit.position);
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
    bounce.preferredTargetUnitId = nextTarget.id;
    bounce.requirePreferredTarget = true;
    bounce.track = true;
    bounce.through = false;
    bounce.noHurt = false;
    bounce.ignoreProjectileCancel = true;
    bounce.frame = 0;
    bounce.bounceRemaining = std::max(0, source.bounceRemaining - 1);

    double speed = norm(source.velocity);
    if (speed <= 0.0001)
    {
        speed = world.defaultProjectileSpeed;
    }

    auto direction = normalizedTo(nextTarget.position - hitTarget.position, 1.0);
    if (norm(direction) <= 0.0001)
    {
        direction = normalizedTo(nextTarget.position - source.position, 1.0);
    }
    if (norm(direction) <= 0.0001)
    {
        direction = { 1.0f, 0.0f, 0.0f };
    }

    const auto spawnDistance = static_cast<float>(world.bounceSpawnDistance);
    bounce.position = hitTarget.position + Pointf{
        direction.x * spawnDistance,
        direction.y * spawnDistance,
        direction.z * spawnDistance,
    };
    bounce.velocity = normalizedTo(nextTarget.position - bounce.position, speed);
    if (norm(bounce.velocity) <= 0.0001)
    {
        bounce.velocity = normalizedTo(direction, speed);
    }
    bounce.totalFrame = std::max(
        world.minimumBounceTotalFrame,
        static_cast<int>(std::ceil(distance(nextTarget.position, bounce.position) / speed)) + 20);
    return bounce;
}

void BattleAttackSystem::collectProjectileCancelEvents(
    const BattleAttackWorld& world,
    std::vector<BattleAttackEvent>& events) const
{
    for (size_t i = 0; i + 1 < world.attacks.size(); ++i)
    {
        const auto& lhs = world.attacks[i];
        if (lhs.noHurt || lhs.ignoreProjectileCancel || lhs.frame < world.projectileGraceFrames)
        {
            continue;
        }
        const auto* lhsAttacker = unitById(world, lhs.attackerUnitId);
        if (!lhsAttacker || !lhsAttacker->alive)
        {
            continue;
        }

        for (size_t j = i + 1; j < world.attacks.size(); ++j)
        {
            const auto& rhs = world.attacks[j];
            if (rhs.noHurt || rhs.ignoreProjectileCancel || rhs.frame < world.projectileGraceFrames)
            {
                continue;
            }
            const auto* rhsAttacker = unitById(world, rhs.attackerUnitId);
            if (!rhsAttacker || !rhsAttacker->alive || lhsAttacker->team == rhsAttacker->team)
            {
                continue;
            }
            if (lhs.ultimate || rhs.ultimate)
            {
                continue;
            }
            if (distance(lhs.position, rhs.position) < world.hitRadius)
            {
                events.push_back({ BattleAttackEventType::ProjectileCancel, lhs.id, rhs.id });
            }
        }
    }
}

}  // namespace KysChess::Battle
