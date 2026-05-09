#include "BattleRuntimeRules.h"

#include "BattleGeometry.h"

#include <array>
#include <cassert>

namespace KysChess::Battle
{
namespace
{

constexpr double MINIMUM_VECTOR_NORM = 0.0001;
constexpr int ACTION_RECOVERY_FRAMES = 4;
constexpr int DASH_MOMENTUM_FRAMES = 5;
constexpr int MOVEMENT_DASH_COOLDOWN_FRAMES = 18;
constexpr int POST_DASH_SPREAD_FRAMES = 12;
constexpr int NORMAL_CAST_MP_DELTA = 5;
constexpr int COOLDOWN_AFTER_CAST_PADDING = 2;
constexpr int COOLDOWN_MAX_SPEED = 150;
constexpr double SPEED_COOLDOWN_REDUCTION_RATIO = 0.5;
constexpr int MELEE_HIT_TOTAL_FRAME = 10;
constexpr int STRENGTHENED_MELEE_TOTAL_FRAME = 30;
constexpr double STRENGTHENED_MELEE_SELECT_DISTANCE_DIVISOR = 2.0;
constexpr float STRENGTHENED_MELEE_MULTIPLIER = 2.0f;
constexpr int STRENGTHENED_MELEE_OPERATION_COUNT_THRESHOLD = 2;
constexpr int MELEE_SPLASH_TOTAL_FRAME = 60;
constexpr int MELEE_SPLASH_INITIAL_FRAME = 5;
constexpr double MELEE_SPLASH_PROJECTILE_SPEED = 3.0;
constexpr float MELEE_SPLASH_STRENGTH_MULTIPLIER = 0.5f;
constexpr int TRACKING_PROJECTILE_TOTAL_FRAME = 120;
constexpr int DASH_HIT_TOTAL_FRAME = 30;
constexpr double DASH_HIT_POSITION_SPACING = 2.0;
constexpr int DASH_HIT_FRAME_STEP = 3;
constexpr std::array<int, 4> LEGACY_CAST_FRAMES = { 25, 30, 20, 25 };
constexpr double MAX_EFFECTIVE_BATTLE_REACH = 480.0;
constexpr double BLINK_WEAK_TARGET_DEF_WEIGHT = 4.0;
constexpr int PROJECTILE_BOUNCE_RANGE = 90;

BattleCastConfig makeHadesBattleCastConfig()
{
    BattleCastConfig config;
    config.castFrames = LEGACY_CAST_FRAMES;
    config.baseCooldownFrames = { 105, 185, 115, 45 };
    config.minimumCooldownFrames = { 60, 70, 70, 45 };
    config.cooldownActPropertyDivisors = { 2, 1, 2, 0 };
    config.recoveryFrames = {
        ACTION_RECOVERY_FRAMES,
        ACTION_RECOVERY_FRAMES,
        ACTION_RECOVERY_FRAMES,
        DASH_MOMENTUM_FRAMES,
    };
    config.maxCooldownSpeed = COOLDOWN_MAX_SPEED;
    config.speedCooldownReductionRatio = SPEED_COOLDOWN_REDUCTION_RATIO;
    config.minimumCooldownAfterCastPadding = COOLDOWN_AFTER_CAST_PADDING;
    config.normalCastMpDelta = NORMAL_CAST_MP_DELTA;
    config.minimumFacingNorm = MINIMUM_VECTOR_NORM;
    config.meleeHitTotalFrame = MELEE_HIT_TOTAL_FRAME;
    config.strengthenedMeleeTotalFrame = STRENGTHENED_MELEE_TOTAL_FRAME;
    config.strengthenedMeleeSelectDistanceDivisor = STRENGTHENED_MELEE_SELECT_DISTANCE_DIVISOR;
    config.strengthenedMeleeMultiplier = STRENGTHENED_MELEE_MULTIPLIER;
    config.meleeSplashTotalFrame = MELEE_SPLASH_TOTAL_FRAME;
    config.meleeSplashInitialFrame = MELEE_SPLASH_INITIAL_FRAME;
    config.meleeSplashStrengthMultiplier = MELEE_SPLASH_STRENGTH_MULTIPLIER;
    config.trackingProjectileTotalFrame = TRACKING_PROJECTILE_TOTAL_FRAME;
    config.dashHitTotalFrame = DASH_HIT_TOTAL_FRAME;
    config.strengthenedMeleeOperationCountThreshold = STRENGTHENED_MELEE_OPERATION_COUNT_THRESHOLD;
    return config;
}

BattleCastGeometry makeHadesBattleCastGeometry(
    double tileWidth,
    double meleeAttackEffectOffset,
    double meleeAttackHitRadius)
{
    BattleCastGeometry geometry;
    geometry.meleeAttackEffectOffset = meleeAttackEffectOffset;
    geometry.projectileSpeed = tileWidth / 3.0;
    geometry.projectileSpawnOffset = tileWidth * 2.0;
    geometry.projectileBaseTravel = tileWidth * 5.0;
    geometry.projectileTravelPerSelectDistance = tileWidth;
    geometry.meleeSplashProjectileSpeed = MELEE_SPLASH_PROJECTILE_SPEED;
    geometry.dashHitPositionSpacing = DASH_HIT_POSITION_SPACING;
    geometry.dashVelocityMagnitude = meleeAttackHitRadius / DASH_MOMENTUM_FRAMES;
    geometry.dashHitFrameStep = DASH_HIT_FRAME_STEP;
    return geometry;
}

}  // namespace

BattleRuntimeRulesConfig makeHadesBattleRuntimeRules(double tileWidth, int coordCount)
{
    assert(tileWidth > 0.0);
    assert(coordCount > 0);

    const double engagementCellDeadband = tileWidth / 2.0;
    const double engagementCellArriveDistance = tileWidth - engagementCellDeadband / 2.0;
    const double meleeAttackEffectOffset = tileWidth * 2.0;
    const double meleeAttackHitRadius = tileWidth * 2.0;
    const double meleeAttackSafetyMargin = meleeAttackHitRadius - engagementCellArriveDistance;
    const double meleeAttackReach = meleeAttackEffectOffset + meleeAttackHitRadius - meleeAttackSafetyMargin;
    const double meleeLocalTargetRadius = meleeAttackReach + meleeAttackEffectOffset;
    const double dashAttackMeleeReach = meleeAttackReach + meleeLocalTargetRadius;

    BattleRuntimeRulesConfig rules;
    rules.gridTransform = { tileWidth, coordCount };

    BattleMovementGeometry movementGeometry;
    movementGeometry.tileWidth = tileWidth;
    movementGeometry.maxRangedReach = MAX_EFFECTIVE_BATTLE_REACH;
    movementGeometry.dashFrames = DASH_MOMENTUM_FRAMES;
    movementGeometry.dashCooldownFrames = MOVEMENT_DASH_COOLDOWN_FRAMES;
    movementGeometry.meleeAttackEffectOffset = meleeAttackEffectOffset;
    movementGeometry.meleeAttackHitRadius = meleeAttackHitRadius;
    rules.movementConfig = BattleGeometry(movementGeometry).movementConfig();

    rules.teamEffectHealAuraRadius = tileWidth * 6.0;
    rules.rescueExecuteUnattendedRadius = tileWidth * 3.0;
    rules.minimumVectorNorm = MINIMUM_VECTOR_NORM;
    rules.rescueCounterAttack.visualEffectId = 11;
    rules.rescueCounterAttack.projectileSpeed = tileWidth / 3.0;
    rules.rescueCounterAttack.meleeAttackEffectOffset = meleeAttackEffectOffset;
    rules.rescueCounterAttack.minimumTotalFrames = 20;
    rules.rescueCounterAttack.totalFramePadding = 15;

    rules.castConfig = makeHadesBattleCastConfig();
    rules.castGeometry = makeHadesBattleCastGeometry(tileWidth, meleeAttackEffectOffset, meleeAttackHitRadius);
    rules.movementPhysicsConfig.postDashSpreadFrames = POST_DASH_SPREAD_FRAMES;
    rules.movementPhysicsDashMomentumFrames = DASH_MOMENTUM_FRAMES;

    rules.action.maxEffectiveBattleReach = MAX_EFFECTIVE_BATTLE_REACH;
    rules.action.tileWidth = tileWidth;
    rules.action.meleeAttackHitRadius = meleeAttackHitRadius;
    rules.action.meleeAttackReach = meleeAttackReach;
    rules.action.dashAttackMeleeReach = dashAttackMeleeReach;
    rules.action.blinkWeakTargetDefWeight = BLINK_WEAK_TARGET_DEF_WEIGHT;
    rules.action.dashMomentumFrames = DASH_MOMENTUM_FRAMES;
    rules.action.movementDashCooldownFrames = MOVEMENT_DASH_COOLDOWN_FRAMES;
    rules.action.actionRecoveryFrames = ACTION_RECOVERY_FRAMES;
    rules.action.dashRecoveryFrames = DASH_MOMENTUM_FRAMES;
    rules.action.strengthenedMeleeOperationCountThreshold = STRENGTHENED_MELEE_OPERATION_COUNT_THRESHOLD;
    rules.action.projectileBounceRange = PROJECTILE_BOUNCE_RANGE;
    rules.action.coordCount = coordCount;

    rules.projectileFollowUps.projectileSpeed = tileWidth / 3.0;
    rules.projectileFollowUps.minimumProjectileFrames = 20;
    rules.projectileFollowUps.nearbyProjectileFramePadding = 18;
    rules.projectileFollowUps.areaProjectileFramePadding = 15;
    rules.projectileFollowUps.areaSpawnDistance = tileWidth * 1.5;

    rules.movementCollisionWorld.tileWidth = tileWidth;
    rules.movementCollisionWorld.coordCount = coordCount;
    rules.movementCollisionWorld.defaultSeparationDistance = tileWidth * 1.5;
    return rules;
}

}  // namespace KysChess::Battle
