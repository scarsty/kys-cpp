#pragma once

#include "../Point.h"

#include <unordered_map>
#include <vector>

namespace KysChess::Battle
{

struct BattleAttackUnit
{
    int id = -1;
    int team = 0;
    bool alive = true;
    bool invincible = false;
    bool hurtFrame = false;
    Pointf position;
};

struct BattleAttackInstance
{
    int id = -1;
    int attackerUnitId = -1;
    int preferredTargetUnitId = -1;
    bool requirePreferredTarget = false;
    int frame = 0;
    int totalFrame = 1;
    bool noHurt = false;
    bool track = false;
    bool through = false;
    bool ultimate = false;
    bool executeCanHitInvincible = false;
    bool ignoreProjectileCancel = false;
    int sharedHitGroupId = 0;
    int spawnedFromAttackId = -1;
    int bounceRemaining = 0;
    int bounceRange = 0;
    int bounceChancePct = 0;
    int bounceRollPct = 0;
    std::vector<int> hitUnitIds;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    bool spiralMotion = false;
    Pointf spiralCenter;
    float spiralRadius = 0.0f;
    float spiralRadiusGrowth = 0.0f;
    float spiralAngle = 0.0f;
    float spiralAngularVelocity = 0.0f;
};

enum class BattleAttackEventType
{
    Moved,
    Hit,
    Expired,
    TargetLost,
    ProjectileCancel,
    Bounce
};

struct BattleAttackEvent
{
    BattleAttackEventType type = BattleAttackEventType::Moved;
    int attackId = -1;
    int otherAttackId = -1;
    int unitId = -1;
};

struct BattleAttackWorld
{
    int frame = 0;
    double hitRadius = 100.0;
    int projectileGraceFrames = 5;
    int nextAttackId = 0;
    double bounceSpawnDistance = 54.0;
    double defaultProjectileSpeed = 12.0;
    int minimumBounceTotalFrame = 20;
    bool spendNonThroughOnHit = true;
    std::vector<BattleAttackUnit> units;
    std::vector<BattleAttackInstance> attacks;
    std::unordered_map<int, std::vector<int>> sharedHitGroupTargets;
};

class BattleAttackSystem
{
public:
    std::vector<BattleAttackEvent> tick(BattleAttackWorld& world) const;

private:
    const BattleAttackUnit* unitById(const BattleAttackWorld& world, int unitId) const;
    const BattleAttackUnit* selectTarget(const BattleAttackWorld& world, const BattleAttackInstance& attack) const;
    bool hasHitUnit(const BattleAttackInstance& attack, int unitId) const;
    bool hasSharedHit(const BattleAttackWorld& world, int sharedHitGroupId, int unitId) const;
    void markHit(BattleAttackWorld& world, BattleAttackInstance& attack, int unitId) const;
    void moveAttack(BattleAttackInstance& attack) const;
    void trackTarget(BattleAttackInstance& attack, const BattleAttackUnit& target) const;
    bool canHit(const BattleAttackWorld& world, const BattleAttackInstance& attack, const BattleAttackUnit& target) const;
    const BattleAttackUnit* selectBounceTarget(
        const BattleAttackWorld& world,
        const BattleAttackInstance& attack,
        const BattleAttackUnit& hitTarget) const;
    BattleAttackInstance makeBounceAttack(
        const BattleAttackWorld& world,
        const BattleAttackInstance& source,
        const BattleAttackUnit& hitTarget,
        const BattleAttackUnit& nextTarget,
        int attackId) const;
    void collectProjectileCancelEvents(const BattleAttackWorld& world, std::vector<BattleAttackEvent>& events) const;
};

}  // namespace KysChess::Battle
