#pragma once

#include "../Point.h"
#include "BattleOperation.h"

#include <string>
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

enum class BattleAttackCastSubrequestKind
{
    None,
    SkillHit,
    DashHit,
    DashFollowUpSkill,
    MeleeSplash,
    ExtraProjectile,
};

struct BattleAttackState
{
    int attackerUnitId = -1;
    int skillId = -1;
    std::string skillName;
    int skillHurtType = 0;
    int skillMagicType = 0;
    int skillEffectId = -1;
    int skillAttackerActProperty = 0;
    int skillMagicPower = 0;
    int preferredTargetUnitId = -1;
    bool requirePreferredTarget = false;
    int totalFrame = 1;
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
    int visualEffectId = -1;
    BattleOperationType operationType = BattleOperationType::None;
    int scriptedDamage = 0;
    int scriptedStunFrames = 0;
    int scriptedBleedStacks = 0;
    int projectileCancelDamage = 0;
    int projectileCancelWeaken = 0;
    BattleAttackCastSubrequestKind castSubrequestKind = BattleAttackCastSubrequestKind::None;
    float strengthMultiplier = 1.0f;
    bool suppressNearbyTrackingProjectileProc = false;
    bool mainProjectile = true;
    Pointf position;
    Pointf velocity;
};

struct BattleAttackInstance
{
    int id = -1;
    BattleAttackState state;
    int frame = 0;
    bool noHurt = false;
    int spawnedFromAttackId = -1;
    std::vector<int> hitUnitIds;
    Pointf previousPosition;
    Pointf acceleration;
    bool spiralMotion = false;
    Pointf spiralCenter;
    float spiralRadius = 0.0f;
    float spiralRadiusGrowth = 0.0f;
    float spiralAngle = 0.0f;
    float spiralAngularVelocity = 0.0f;
};

struct BattleAttackSpawnRequest
{
    BattleAttackState initial;
    int initialFrame = 0;
    Pointf acceleration;
    bool spiralMotion = false;
    Pointf spiralCenter;
    float spiralRadius = 0.0f;
    float spiralRadiusGrowth = 0.0f;
    float spiralAngle = 0.0f;
    float spiralAngularVelocity = 0.0f;
};

struct BattleAttackBouncePrime
{
    int count = 0;
    int chancePct = 0;
    int rollPct = 0;
    int range = 0;
};

void applyProjectileBouncePrime(BattleAttackSpawnRequest& request, BattleAttackBouncePrime prime);
bool tryApplyProjectileBouncePrime(BattleAttackSpawnRequest& request, BattleAttackBouncePrime prime);

enum class BattleAttackEventType
{
    AttackSpawned,
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
    int sourceUnitId = -1;
    int otherSourceUnitId = -1;
    int skillId = -1;
    std::string skillName;
    int skillHurtType = 0;
    int skillMagicType = 0;
    int skillEffectId = -1;
    int skillAttackerActProperty = 0;
    int skillMagicPower = 0;
    BattleOperationType operationType = BattleOperationType::None;
    int visualEffectId = -1;
    int scriptedDamage = 0;
    int scriptedStunFrames = 0;
    int scriptedBleedStacks = 0;
    bool executeCanHitInvincible = false;
    bool track = false;
    bool through = false;
    bool ultimate = false;
    float strengthMultiplier = 1.0f;
    bool suppressNearbyTrackingProjectileProc = false;
    bool mainProjectile = true;
    int sharedHitGroupId = 0;
    int projectileCancelDamage = 0;
    int otherProjectileCancelDamage = 0;
    Pointf position;
    Pointf velocity;
    int frame = 0;
    int totalFrame = 0;
};

struct BattleAttackWorld
{
    int frame = 0;
    double hitRadius{};
    double minimumVectorNorm{};
    int projectileGraceFrames = 5;
    int nextAttackId = 0;
    double bounceSpawnDistance{};
    double defaultProjectileSpeed{};
    int minimumBounceTotalFrame = 20;
    bool spendNonThroughOnHit = true;
    std::vector<BattleAttackUnit> units;
    std::vector<BattleAttackInstance> attacks;
    std::unordered_map<int, std::vector<int>> sharedHitGroupTargets;
};

class BattleAttackSystem
{
public:
    BattleAttackEvent spawn(BattleAttackWorld& world, const BattleAttackSpawnRequest& request) const;
    std::vector<BattleAttackEvent> tick(BattleAttackWorld& world) const;
    void applyProjectileCancelDamage(BattleAttackWorld& world, const BattleAttackEvent& event) const;

private:
    int allocateAttackId(BattleAttackWorld& world) const;
    const BattleAttackUnit* selectTarget(const BattleAttackWorld& world, const BattleAttackInstance& attack) const;
    bool hasHitUnit(const BattleAttackInstance& attack, int unitId) const;
    bool hasSharedHit(const BattleAttackWorld& world, int sharedHitGroupId, int unitId) const;
    void markHit(BattleAttackWorld& world, BattleAttackInstance& attack, int unitId) const;
    void moveAttack(BattleAttackInstance& attack) const;
    void trackTarget(BattleAttackInstance& attack, const BattleAttackUnit& target, double minimumVectorNorm) const;
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

double projectileOperationDamageMultiplier(BattleOperationType operationType);
int scaleProjectileCancelDamage(int damage, BattleOperationType operationType);

}  // namespace KysChess::Battle
