#pragma once

#include "../Point.h"
#include "BattleAttackDelivery.h"
#include "BattleCastLifecycle.h"
#include "BattleDamageSystem.h"
#include "BattleOperation.h"

#include <memory_resource>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace KysChess::Battle
{

inline constexpr int OptionalPreferredTargetUnitId = -1;

struct BattleRuntimeUnit;
class BattleRuntimeUnits;

struct BattleAttackUnit
{
    int id = -1;
    int team = 0;
    bool alive = true;
    bool invincible = false;
    Pointf position;
};

struct BattleAttackPayload
{
    BattleAttackDelivery delivery;
    BattleProjectilePayloadClass payloadClass;
    BattleAttackReflectionLineageKind reflectionLineage;
    int attackSourceUnitId = -1;
    int skillId = -1;
    std::string skillName;
    int skillHurtType = 0;
    int skillMagicType = 0;
    int skillEffectId = -1;
    int skillAttackerActProperty = 0;
    int skillMagicPower = 0;
    int preferredTargetUnitId = OptionalPreferredTargetUnitId;
    bool requirePreferredTarget = false;
    int totalFrame = 1;
    bool track = false;
    bool through = false;
    bool executeCanHitInvincible = false;
    bool ignoreProjectileCancel = false;
    int bounceRemaining = 0;
    int bounceRange = 0;
    int bounceChancePct = 0;
    int bounceRollPct = 0;
    int visualEffectId = -1;
    BattleOperationType operationType = BattleOperationType::None;
    int scriptedDamage = 0;
    bool scriptedDamageAppliesModifiers = false;
    bool scriptedDamageTriggersDefenseEffects = false;
    int scriptedStunFrames = 0;
    int scriptedBleedStacks = 0;
    std::optional<BattleStatusProducerProvenance> scriptedBleedProducer;
    int projectileCancelDamage = 0;
    int projectileCancelWeaken = 0;
    int projectilePressurePct = 100;
    int projectileClearRadiusPct = 0;
    BattleAttackCastSubrequestKind castSubrequestKind = BattleAttackCastSubrequestKind::None;
    int roleAttackEchoActType = -1;
    int strengthPct = 100;
    bool suppressNearbyTrackingProjectileProc = false;
    BattleDamageKind damageKind = BattleDamageKind::Physical;
    std::optional<BattleAttackPotencySnapshot> potencySnapshot;
    Pointf position;
    Pointf velocity;

    BattleAttackPayload() = delete;
    BattleAttackPayload(
        BattleAttackDelivery delivery,
        BattleProjectilePayloadClass payloadClass,
        BattleAttackReflectionLineageKind reflectionLineage)
        : delivery(delivery)
        , payloadClass(payloadClass)
        , reflectionLineage(reflectionLineage)
    {
    }
};

struct BattlePendingAttackContact
{
    int targetUnitId{};
};

struct BattleAttackInstance
{
    int id = -1;
    BattleAttackProvenance provenance;
    BattleAttackPayload state;
    CastWorkToken castWork;
    int frame = 0;
    bool noHurt = false;
    bool contactsSuppressed{};
    std::vector<int> hitUnitIds;
    std::vector<int> invincibleBlockedUnitIds;
    Pointf previousPosition;
    Pointf acceleration;
    bool spiralMotion = false;
    Pointf spiralCenter;
    float spiralRadius = 0.0f;
    float spiralRadiusGrowth = 0.0f;
    float spiralAngle = 0.0f;
    float spiralAngularVelocity = 0.0f;
    std::optional<AttackFinishReason> scheduledFinishReason;
    std::optional<AttackFinishReason> finishReason;
    std::optional<BattlePendingAttackContact> pendingContact;

    BattleAttackInstance() = delete;
    explicit BattleAttackInstance(BattleAttackPayload state)
        : state(std::move(state))
    {
    }
};

enum class BattleHitContinuation
{
    Normal,
    Reflected,
};

struct BattleHitSettlement
{
    int attackId{};
    int targetUnitId{};
    bool accepted{};
    BattleHitContinuation continuation{};
};

struct BattleProjectilePropertiesSnapshot
{
    BattleAttackPayload payload;
    Pointf acceleration;
    bool spiralMotion{};
    Pointf spiralCenter;
    float spiralRadius{};
    float spiralRadiusGrowth{};
    float spiralAngle{};
    float spiralAngularVelocity{};

    BattleProjectilePropertiesSnapshot() = delete;
    explicit BattleProjectilePropertiesSnapshot(BattleAttackPayload payload)
        : payload(std::move(payload))
    {
    }
};

struct BattleAttackSpawnRequest
{
    BattlePendingAttackProvenance provenance;
    CastWorkToken castWork;
    BattleAttackPayload initial;
    int initialFrame = 0;
    int spawnDelayFrames = 0;
    int attackerDualWieldBlockGainChancePct = 0;
    Pointf acceleration;
    bool spiralMotion = false;
    Pointf spiralCenter;
    float spiralRadius = 0.0f;
    float spiralRadiusGrowth = 0.0f;
    float spiralAngle = 0.0f;
    float spiralAngularVelocity = 0.0f;

    BattleAttackSpawnRequest() = delete;
    explicit BattleAttackSpawnRequest(BattleAttackPayload initial)
        : initial(std::move(initial))
    {
    }
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
bool attackSpawnDelayElapsed(BattleAttackSpawnRequest& request);

enum class BattleAttackEventType
{
    AttackSpawned,
    Moved,
    Hit,
    Expired,
    TargetLost,
    ChainEnded,
    ChainNoTargetInRange,
    ProjectileCancel,
    BlockedByInvincible,
    Bounce
};

struct BattleAttackEvent
{
    BattleAttackEventType type = BattleAttackEventType::Moved;
    int attackId = -1;
    int otherAttackId = -1;
    int unitId = -1;
    int preferredTargetUnitId = OptionalPreferredTargetUnitId;
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
    std::optional<BattleAttackDelivery> delivery;
    std::optional<BattleProjectilePayloadClass> payloadClass;
    std::optional<BattleAttackReflectionLineageKind> reflectionLineage;
    std::optional<BattleAttackPotencySnapshot> potencySnapshot;
    int visualEffectId = -1;
    int scriptedDamage = 0;
    bool scriptedDamageAppliesModifiers = false;
    bool scriptedDamageTriggersDefenseEffects = false;
    int scriptedStunFrames = 0;
    int scriptedBleedStacks = 0;
    std::optional<BattleStatusProducerProvenance> scriptedBleedProducer;
    bool executeCanHitInvincible = false;
    bool track = false;
    bool through = false;
    int strengthPct = 100;
    bool suppressNearbyTrackingProjectileProc = false;
    int projectileCancelDamage = 0;
    int otherProjectileCancelDamage = 0;
    BattleAttackCastSubrequestKind castSubrequestKind = BattleAttackCastSubrequestKind::None;
    int roleAttackEchoActType = -1;
    BattleAttackProvenance provenance;
    std::optional<BattleAttackProvenance> otherProvenance;
    BattleDamageKind damageKind = BattleDamageKind::Physical;
    Pointf position;
    Pointf velocity;
    int frame = 0;
    int totalFrame = 0;
};

struct BattleHitSettlementResult
{
    std::optional<BattleProjectilePropertiesSnapshot> reflectedProjectile;
    std::vector<BattleAttackEvent> events;
};

struct BattleAttackState
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
    std::vector<BattleAttackInstance> attacks;
    std::unordered_map<int, std::vector<int>> sharedHitGroupTargets;
    std::set<BattleCastId> suppressedContactCastIds;

    BattleAttackEvent spawn(
        BattleAttackSpawnRequest&& request,
        BattleCastLifecycle& castLifecycle);
    std::pmr::vector<BattleAttackEvent> tick(
        const BattleRuntimeUnits& units,
        BattleCastLifecycle& castLifecycle,
        std::pmr::memory_resource* memoryResource = std::pmr::get_default_resource());
    void tick(
        const BattleRuntimeUnits& units,
        BattleCastLifecycle& castLifecycle,
        std::pmr::vector<BattleAttackEvent>& events);
    BattleHitSettlementResult settleHit(
        const BattleHitSettlement& settlement,
        const BattleRuntimeUnits& units,
        BattleCastLifecycle& castLifecycle);
    void appendProjectileCancelEvents(
        const BattleRuntimeUnits& units,
        std::pmr::vector<BattleAttackEvent>& events) const;
    void applyProjectileCancelDamage(const BattleAttackEvent& event);
    bool contactsSuppressed(int attackId) const;
    void suppressContacts(int attackId);
    bool castContactsSuppressed(BattleCastId castId) const;
    void suppressContactsForCast(BattleCastId castId);
    void releaseCastContactSuppression(BattleCastId castId);
    void clearCastContactSuppressions();
    void completeFinished(BattleCastLifecycle& castLifecycle);
    void eraseFinished();
    void pruneFinished(BattleCastLifecycle& castLifecycle);
    void cancelAllForBattleEnd(BattleCastLifecycle& castLifecycle);

private:
    void clearProjectilesAlongPaths(const BattleRuntimeUnits& units);
    int allocateAttackId();
    const BattleRuntimeUnit* selectTarget(
        const BattleRuntimeUnits& units,
        const BattleAttackInstance& attack) const;
    bool hasHitUnit(const BattleAttackInstance& attack, int unitId) const;
    bool hasInvincibleBlockedUnit(const BattleAttackInstance& attack, int unitId) const;
    bool hasSharedHit(int sharedHitGroupId, int unitId) const;
    void markHit(BattleAttackInstance& attack, int unitId);
    void markInvincibleBlocked(BattleAttackInstance& attack, int unitId) const;
    void moveAttack(BattleAttackInstance& attack) const;
    void trackTarget(BattleAttackInstance& attack, const BattleRuntimeUnit& target) const;
    bool canContactTarget(
        const BattleRuntimeUnits& units,
        const BattleAttackInstance& attack,
        const BattleRuntimeUnit& target) const;
    bool contactBlockedByInvincible(
        const BattleRuntimeUnits& units,
        const BattleAttackInstance& attack,
        const BattleRuntimeUnit& target) const;
    bool canHit(
        const BattleRuntimeUnits& units,
        const BattleAttackInstance& attack,
        const BattleRuntimeUnit& target) const;
    const BattleRuntimeUnit* selectBounceTarget(
        const BattleRuntimeUnits& units,
        const BattleAttackInstance& attack,
        const BattleRuntimeUnit& hitTarget) const;
    BattleAttackInstance makeBounceAttack(
        const BattleAttackInstance& source,
        const BattleRuntimeUnit& hitTarget,
        const BattleRuntimeUnit& nextTarget,
        int attackId,
        BattleCastLifecycle& castLifecycle) const;
    void finishAttack(
        BattleAttackInstance& attack,
        AttackFinishReason reason,
        BattleCastLifecycle& castLifecycle);
};

int scaleProjectileCancelDamage(int damage, BattleOperationType operationType);

}  // namespace KysChess::Battle
