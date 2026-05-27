#include "BattleCastSystem.h"

#include "BattleLogSegments.h"
#include "../Find.h"
#include "BattleMath.h"
#include "BattleCombatIntent.h"
#include "BattleProjectileTargetingSystem.h"
#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace KysChess::Battle
{

int advanceOperationCountAfterCommittedCast(int operationCount,
                                            bool ultimate,
                                            BattleOperationType operationType,
                                            int strengthenedMeleeOperationCountThreshold)
{
    assert(operationCount >= 0);
    assert(strengthenedMeleeOperationCountThreshold > 0);

    if (operationType != BattleOperationType::Melee)
    {
        return operationCount;
    }
    if (ultimate || operationCount >= strengthenedMeleeOperationCountThreshold)
    {
        return 0;
    }
    return operationCount + 1;
}

namespace
{
constexpr double RangedSideProjectileAngle = 3.14159265358979323846 / 12.0;

BattleSkillState toCombatSkill(const BattleCastSkillState& skill)
{
    BattleSkillState combatSkill;
    combatSkill.id = skill.id;
    combatSkill.attackAreaType = skill.attackAreaType;
    combatSkill.magicType = skill.magicType;
    combatSkill.reach = skill.reach;
    combatSkill.forceRanged = skill.forceRanged;
    combatSkill.rangedStyle = skill.rangedStyle;
    return combatSkill;
}

Pointf castFacing(const BattleCastInput& input)
{
    auto facing = input.targetPosition - input.unit.position;
    assert(input.config.minimumFacingNorm > 0.0);
    assert(facing.norm() > input.config.minimumFacingNorm);
    return normalizedTo(facing, 1.0, input.config.minimumFacingNorm);
}

Pointf rotated(Pointf point, double angle)
{
    const double baseAngle = std::atan2(point.y, point.x) + angle;
    const double norm = std::sqrt(static_cast<double>(point.x) * point.x
        + static_cast<double>(point.y) * point.y);
    return {
        static_cast<float>(std::cos(baseAngle) * norm),
        static_cast<float>(std::sin(baseAngle) * norm),
        point.z,
    };
}

double angleDelta(double lhs, double rhs)
{
    return std::abs(std::atan2(std::sin(lhs - rhs), std::cos(lhs - rhs)));
}

std::vector<BattleCastProjectileTarget> orderedAlternateProjectileTargets(
    const BattleCastInput& input,
    Pointf launchPosition,
    Pointf baseDirection,
    double maxTravel)
{
    const double baseAngle = std::atan2(baseDirection.y, baseDirection.x);
    std::vector<BattleCastProjectileTarget> targets;
    for (const auto& target : input.projectileSpreadTargets)
    {
        if (target.unitId == input.targetUnitId)
        {
            continue;
        }
        if (pointDistance(target.position, launchPosition) > maxTravel)
        {
            continue;
        }
        targets.push_back(target);
    }

    std::sort(targets.begin(), targets.end(), [&](const auto& left, const auto& right)
    {
        const auto leftDir = left.position - launchPosition;
        const auto rightDir = right.position - launchPosition;
        const double leftAngle = leftDir.norm() > input.config.minimumFacingNorm
            ? std::atan2(leftDir.y, leftDir.x)
            : baseAngle;
        const double rightAngle = rightDir.norm() > input.config.minimumFacingNorm
            ? std::atan2(rightDir.y, rightDir.x)
            : baseAngle;
        const double leftDelta = angleDelta(leftAngle, baseAngle);
        const double rightDelta = angleDelta(rightAngle, baseAngle);
        if (leftDelta != rightDelta)
        {
            return leftDelta < rightDelta;
        }
        return pointDistance(left.position, launchPosition) < pointDistance(right.position, launchPosition);
    });
    return targets;
}

void assignProjectileTargetOrSpread(
    BattleAttackSpawnRequest& request,
    const BattleCastInput& input,
    const std::vector<BattleCastProjectileTarget>& alternates,
    int projectileIndex,
    int projectileCount,
    double speed)
{
    const BattleCastProjectileTarget* assigned = nullptr;
    request.initial.requirePreferredTarget = false;
    if (projectileIndex == 0 && input.targetUnitId >= 0)
    {
        request.initial.preferredTargetUnitId = input.targetUnitId;
        const auto primaryDirection = input.targetPosition - request.initial.position;
        if (primaryDirection.norm() > input.config.minimumFacingNorm)
        {
            request.initial.velocity = normalizedTo(
                primaryDirection,
                speed,
                input.config.minimumFacingNorm);
        }
        return;
    }

    const int alternateIndex = projectileIndex - 1;
    if (alternateIndex >= 0 && alternateIndex < static_cast<int>(alternates.size()))
    {
        assigned = &alternates[alternateIndex];
    }

    if (assigned)
    {
        request.initial.preferredTargetUnitId = assigned->unitId;
        request.initial.requirePreferredTarget = true;
        const auto assignedDirection = assigned->position - request.initial.position;
        if (assignedDirection.norm() > input.config.minimumFacingNorm)
        {
            request.initial.velocity = normalizedTo(
                assignedDirection,
                speed,
                input.config.minimumFacingNorm);
        }
        return;
    }

    if (projectileCount > 1)
    {
        const auto facing = castFacing(input);
        const double offset = (projectileIndex - (projectileCount - 1) / 2.0) * RangedSideProjectileAngle;
        request.initial.velocity = normalizedTo(
            rotated(facing, offset),
            speed,
            input.config.minimumFacingNorm);
    }
}

void assertCastSharedConfig(const BattleCastConfig& config)
{
    assert(config.maxCooldownSpeed > 0.0);
    assert(config.speedCooldownReductionRatio >= 0.0);
    assert(config.minimumCooldownAfterCastPadding >= 0);
    assert(config.normalCastMpDelta >= 0);
    assert(config.minimumFacingNorm > 0.0);
}

void assertCastOperationConfig(const BattleCastConfig& config, BattleOperationType operationType)
{
    const int operationIndex = battleOperationIndex(operationType);
    assertCastSharedConfig(config);
    assert(config.castFrames[operationIndex] > 0);
    assert(config.baseCooldownFrames[operationIndex] > 0);
    assert(config.minimumCooldownFrames[operationIndex] > 0);
    assert(config.cooldownActPropertyDivisors[operationIndex] >= 0);
    assert(config.recoveryFrames[operationIndex] >= 0);
}

void assertCommittedCastInput(
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill,
    BattleOperationType operationType)
{
    assertCastOperationConfig(input.config, operationType);
    assert(selectedSkill.id >= 0);
}

void assertCastIntentInput(const BattleCastInput& input, const BattleCastSkillState& selectedSkill)
{
    assertCastSharedConfig(input.config);
    assert(input.targetUnitId >= 0);
    assert(input.targetDistance > 0.0);
    assert(input.unit.meleeAttackReach > 0.0);
    assert(!input.unit.dashAttackEnabled || input.unit.dashAttackReach > 0.0);
    assert(selectedSkill.id >= 0);
    assert(selectedSkill.reach > 0.0);
}

int castFrameForOperation(const BattleCastConfig& config, BattleOperationType operationType)
{
    const int operationIndex = battleOperationIndex(operationType);
    assert(config.castFrames[operationIndex] > 0);
    return config.castFrames[operationIndex];
}

int cooldownForOperation(const BattleCastInput& input, const BattleCastSkillState& selectedSkill, BattleOperationType operationType)
{
    const auto& config = input.config;
    const int operationIndex = battleOperationIndex(operationType);
    int cooldown = config.baseCooldownFrames[operationIndex];
    const int actPropertyDivisor = config.cooldownActPropertyDivisors[operationIndex];
    if (actPropertyDivisor > 0)
    {
        cooldown -= selectedSkill.actProperty / actPropertyDivisor;
    }
    cooldown = std::max(config.minimumCooldownFrames[operationIndex], cooldown);
    const int speed = std::min(static_cast<int>(config.maxCooldownSpeed), input.unit.speed);
    cooldown = static_cast<int>(cooldown * (1.0 - config.speedCooldownReductionRatio * speed / config.maxCooldownSpeed));
    cooldown = std::max(castFrameForOperation(config, operationType) + config.minimumCooldownAfterCastPadding, cooldown);
    if (input.unit.cooldownReductionPct > 0)
    {
        cooldown = static_cast<int>(cooldown * (1.0 - input.unit.cooldownReductionPct / 100.0));
        cooldown = std::max(castFrameForOperation(config, operationType) + config.minimumCooldownAfterCastPadding, cooldown);
    }
    return cooldown;
}

int recoveryFramesForOperation(const BattleCastConfig& config, BattleOperationType operationType)
{
    const int operationIndex = battleOperationIndex(operationType);
    assert(config.recoveryFrames[operationIndex] >= 0);
    return config.recoveryFrames[operationIndex];
}

int mpDeltaForCast(const BattleCastInput& input, bool ultimate)
{
    return ultimate ? -input.unit.maxMp : input.config.normalCastMpDelta;
}

int projectileFramesForSelectDistance(const BattleCastGeometry& geometry, int selectDistance)
{
    assert(selectDistance > 0);
    assert(geometry.projectileSpeed > 0.0);
    assert(geometry.projectileSpawnOffset > 0.0);
    assert(geometry.projectileBaseTravel > 0.0);
    assert(geometry.projectileTravelPerSelectDistance > 0.0);
    const double reach = geometry.projectileSpawnOffset
        + geometry.projectileBaseTravel
        + (selectDistance - 1) * geometry.projectileTravelPerSelectDistance;
    return static_cast<int>((reach - geometry.projectileSpawnOffset) / geometry.projectileSpeed);
}

double projectileSpeedForSkill(const BattleCastGeometry& geometry, const BattleCastSkillState& skill)
{
    assert(geometry.projectileSpeed > 0.0);
    assert(skill.projectileSpeedMultiplierPct > 0);
    return geometry.projectileSpeed * skill.projectileSpeedMultiplierPct / 100.0;
}

double strengthenedMeleeSpeed(const BattleCastConfig& config, const BattleCastSkillState& skill)
{
    assert(skill.selectDistance > 0);
    assert(skill.projectileSpeedMultiplierPct > 0);
    assert(config.strengthenedMeleeSelectDistanceDivisor > 0.0);
    return skill.selectDistance / config.strengthenedMeleeSelectDistanceDivisor
        * skill.projectileSpeedMultiplierPct / 100.0;
}

BattlePresentationColor ultimateTextColor()
{
    return { 255, 215, 0, 255 };
}

double spawnOffsetForOperation(const BattleCastGeometry& geometry, BattleOperationType operationType)
{
    if (operationType == BattleOperationType::TrackingProjectile
        || operationType == BattleOperationType::RangedProjectile)
    {
        assert(geometry.projectileSpawnOffset > 0.0);
        return geometry.projectileSpawnOffset;
    }

    assert(geometry.meleeAttackEffectOffset > 0.0);
    return geometry.meleeAttackEffectOffset;
}

BattleAttackSpawnRequest makeBaseRequest(const BattleCastResult& result,
                                         const BattleCastInput& input,
                                         const BattleCastSkillState& selectedSkill,
                                         BattleOperationType operationType,
                                         BattleAttackCastSubrequestKind kind)
{
    auto facing = castFacing(input);

    BattleAttackSpawnRequest request;
    request.initial.attackerUnitId = input.unit.id;
    request.initial.skillId = selectedSkill.id;
    request.initial.skillName = selectedSkill.name;
    request.initial.skillHurtType = selectedSkill.hurtType;
    request.initial.skillMagicType = selectedSkill.magicType;
    request.initial.skillEffectId = selectedSkill.visualEffectId;
    request.initial.skillAttackerActProperty = selectedSkill.actProperty;
    request.initial.skillMagicPower = selectedSkill.magicPower;
    request.initial.operationType = operationType;
    request.initial.visualEffectId = selectedSkill.visualEffectId;
    request.initial.preferredTargetUnitId = input.targetUnitId;
    request.initial.position = input.unit.position + scaled(facing, spawnOffsetForOperation(input.geometry, operationType));
    request.initial.ultimate = result.decision.ultimate;
    request.initial.castSubrequestKind = kind;
    return request;
}

void applyOperationShape(BattleAttackSpawnRequest& request,
                         const BattleCastConfig& config,
                         const BattleCastGeometry& geometry,
                         BattleOperationType operationType,
                         const BattleCastSkillState& selectedSkill,
                         Pointf facing)
{
    switch (operationType)
    {
    case BattleOperationType::Melee:
        request.initial.totalFrame = config.meleeHitTotalFrame;
        break;
    case BattleOperationType::TrackingProjectile:
        request.initial.velocity = normalizedTo(
            facing,
            projectileSpeedForSkill(geometry, selectedSkill),
            config.minimumFacingNorm);
        request.initial.totalFrame = config.trackingProjectileTotalFrame;
        request.initial.track = true;
        break;
    case BattleOperationType::RangedProjectile:
        request.initial.velocity = normalizedTo(
            facing,
            projectileSpeedForSkill(geometry, selectedSkill),
            config.minimumFacingNorm);
        request.initial.totalFrame = projectileFramesForSelectDistance(geometry, selectedSkill.selectDistance);
        request.initial.through = selectedSkill.attackAreaType == 1 || selectedSkill.attackAreaType == 2;
        break;
    case BattleOperationType::Dash:
        request.initial.totalFrame = config.dashHitTotalFrame;
        break;
    default:
        assert(false);
        break;
    }
}

bool isStrengthenedMelee(const BattleCastResult& result,
                         const BattleCastInput& input,
                         const BattleCastSkillState& selectedSkill)
{
    return result.decision.ultimate
        || input.unit.operationCount >= input.config.strengthenedMeleeOperationCountThreshold
        || selectedSkill.strengthenedMelee;
}

BattleAttackSpawnRequest makeOperationRequest(const BattleCastResult& result,
                                              const BattleCastInput& input,
                                              const BattleCastSkillState& selectedSkill,
                                              BattleOperationType operationType,
                                              BattleAttackCastSubrequestKind kind)
{
    auto request = makeBaseRequest(result, input, selectedSkill, operationType, kind);
    applyOperationShape(request, input.config, input.geometry, operationType, selectedSkill, castFacing(input));
    return request;
}

void clearPreferredTarget(BattleAttackSpawnRequest& request)
{
    request.initial.preferredTargetUnitId = -1;
    request.initial.requirePreferredTarget = false;
}

void appendExtraProjectiles(std::vector<BattleAttackSpawnRequest>& requests,
                            const BattleCastInput& input,
                            const BattleCastSkillState& selectedSkill)
{
    assert(selectedSkill.extraProjectileCount >= 0);
    assert(!requests.empty());
    const auto prototype = requests.front();
    const double speed = prototype.initial.velocity.norm() > input.config.minimumFacingNorm
        ? prototype.initial.velocity.norm()
        : projectileSpeedForSkill(input.geometry, selectedSkill);
    const double maxTravel = speed * std::max(1, prototype.initial.totalFrame - prototype.initialFrame)
        + input.geometry.projectileSpawnOffset;
    const auto alternates = orderedAlternateProjectileTargets(
        input,
        prototype.initial.position,
        prototype.initial.velocity,
        maxTravel);
    for (int i = 0; i < selectedSkill.extraProjectileCount; ++i)
    {
        auto extra = prototype;
        extra.initial.castSubrequestKind = BattleAttackCastSubrequestKind::ExtraProjectile;
        extra.initial.mainProjectile = false;
        assignProjectileTargetOrSpread(
            extra,
            input,
            alternates,
            i + 1,
            selectedSkill.extraProjectileCount + 1,
            speed);
        requests.push_back(extra);
    }
}

void appendTrackingProjectileSpread(
    std::vector<BattleAttackSpawnRequest>& requests,
    const BattleCastResult& result,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill)
{
    const int projectileCount = result.decision.ultimate ? 2 : 1;
    assert(projectileCount > 0);

    auto prototype = makeOperationRequest(
        result,
        input,
        selectedSkill,
        BattleOperationType::TrackingProjectile,
        BattleAttackCastSubrequestKind::SkillHit);
    const double speed = projectileSpeedForSkill(input.geometry, selectedSkill);
    const double maxTravel = speed * std::max(1, prototype.initial.totalFrame - prototype.initialFrame)
        + input.geometry.projectileSpawnOffset;
    const auto alternates = orderedAlternateProjectileTargets(
        input,
        prototype.initial.position,
        castFacing(input),
        maxTravel);
    for (int i = 0; i < projectileCount; ++i)
    {
        auto request = prototype;
        request.initial.mainProjectile = i == 0;
        request.initialFrame = result.decision.ultimate ? (i * 5) : 0;
        assignProjectileTargetOrSpread(request, input, alternates, i, projectileCount, speed);
        requests.push_back(request);
    }
}

void appendRangedSideProjectiles(
    std::vector<BattleAttackSpawnRequest>& requests,
    const BattleCastResult& result,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill)
{
    if (selectedSkill.attackAreaType != 1 && selectedSkill.attackAreaType != 2)
    {
        return;
    }

    const int sideCount = result.decision.ultimate ? 3 : 2;
    const float strengthMultiplier = result.decision.ultimate ? 0.35f : 0.2f;
    assert(sideCount > 0);

    const auto facing = castFacing(input);
    for (int i = 0; i < sideCount; ++i)
    {
        const double t = sideCount == 1
            ? 0.0
            : static_cast<double>(i) / (sideCount - 1);
        const double angle = -RangedSideProjectileAngle
            + t * RangedSideProjectileAngle * 2.0;
        const double speed = (5.0 - 0.5 / sideCount * 2.0 * i)
            * selectedSkill.projectileSpeedMultiplierPct / 100.0;

        auto side = makeOperationRequest(
            result,
            input,
            selectedSkill,
            BattleOperationType::RangedProjectile,
            BattleAttackCastSubrequestKind::ExtraProjectile);
        side.initial.velocity = normalizedTo(
            rotated(facing, angle),
            speed,
            input.config.minimumFacingNorm);
        side.initial.through = true;
        side.initial.mainProjectile = false;
        side.initial.strengthMultiplier = strengthMultiplier;
        clearPreferredTarget(side);
        requests.push_back(side);
    }
}

std::vector<BattleAttackSpawnRequest> makeTrackingProjectileRequests(
    const BattleCastResult& result,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill)
{
    std::vector<BattleAttackSpawnRequest> requests;
    appendTrackingProjectileSpread(requests, result, input, selectedSkill);
    appendExtraProjectiles(requests, input, selectedSkill);
    return requests;
}

std::vector<BattleAttackSpawnRequest> makeRangedProjectileRequests(
    const BattleCastResult& result,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill)
{
    std::vector<BattleAttackSpawnRequest> requests = {
        makeOperationRequest(
            result,
            input,
            selectedSkill,
            BattleOperationType::RangedProjectile,
            BattleAttackCastSubrequestKind::SkillHit),
    };
    clearPreferredTarget(requests.front());
    appendExtraProjectiles(requests, input, selectedSkill);
    appendRangedSideProjectiles(requests, result, input, selectedSkill);
    return requests;
}

std::vector<BattleAttackSpawnRequest> makeMeleeRequests(
    const BattleCastResult& result,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill)
{
    auto facing = castFacing(input);
    std::vector<BattleAttackSpawnRequest> requests;
    auto main = makeBaseRequest(
        result,
        input,
        selectedSkill,
        BattleOperationType::Melee,
        BattleAttackCastSubrequestKind::SkillHit);
    if (isStrengthenedMelee(result, input, selectedSkill))
    {
        main.initial.totalFrame = input.config.strengthenedMeleeTotalFrame;
        main.initial.track = true;
        main.initial.velocity = normalizedTo(
            facing,
            strengthenedMeleeSpeed(input.config, selectedSkill),
            input.config.minimumFacingNorm);
        main.initial.strengthMultiplier = input.config.strengthenedMeleeMultiplier;
    }
    else
    {
        main.initial.totalFrame = input.config.meleeHitTotalFrame;
    }
    requests.push_back(main);

    for (int i = 0; i < selectedSkill.meleeSplashCount; ++i)
    {
        auto splash = makeBaseRequest(
            result,
            input,
            selectedSkill,
            BattleOperationType::Melee,
            BattleAttackCastSubrequestKind::MeleeSplash);
        splash.initial.totalFrame = input.config.meleeSplashTotalFrame;
        splash.initial.track = true;
        splash.initialFrame = input.config.meleeSplashInitialFrame;
        assert(input.geometry.meleeSplashProjectileSpeed > 0.0);
        splash.initial.velocity = normalizedTo(
            facing,
            input.geometry.meleeSplashProjectileSpeed,
            input.config.minimumFacingNorm);
        splash.initial.strengthMultiplier = input.config.meleeSplashStrengthMultiplier;
        requests.push_back(splash);
    }

    appendExtraProjectiles(requests, input, selectedSkill);
    return requests;
}

std::vector<BattleAttackSpawnRequest> makeDashRequests(
    const BattleCastResult& result,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill)
{
    assert(input.unit.dashHitCount > 0);
    assert(input.geometry.dashHitPositionSpacing > 0.0);
    assert(input.geometry.dashHitFrameStep > 0);
    assert(!input.unit.emitDashFollowUpSkillAttack
        || (input.unit.dashFollowUpOperationType == BattleOperationType::Melee
            || input.unit.dashFollowUpOperationType == BattleOperationType::TrackingProjectile
            || input.unit.dashFollowUpOperationType == BattleOperationType::RangedProjectile));

    std::vector<BattleAttackSpawnRequest> requests;
    auto base = makeBaseRequest(
        result,
        input,
        selectedSkill,
        BattleOperationType::Dash,
        BattleAttackCastSubrequestKind::DashHit);
    assert(input.unit.dashVelocity.norm() > input.config.minimumFacingNorm);
    const auto dashHitOffset = normalizedTo(
        input.unit.dashVelocity,
        input.geometry.dashHitPositionSpacing,
        input.config.minimumFacingNorm);
    for (int i = 0; i < input.unit.dashHitCount; ++i)
    {
        auto hit = base;
        hit.initial.position = base.initial.position + scaled(dashHitOffset, i - 1);
        hit.initial.velocity = input.unit.dashVelocity;
        hit.initialFrame = (i + 1) * input.geometry.dashHitFrameStep;
        hit.initial.totalFrame = input.config.dashHitTotalFrame;
        requests.push_back(hit);
    }

    if (input.unit.emitDashFollowUpSkillAttack)
    {
        requests.push_back(makeOperationRequest(
            result,
            input,
            selectedSkill,
            input.unit.dashFollowUpOperationType,
            BattleAttackCastSubrequestKind::DashFollowUpSkill));
    }
    return requests;
}

std::vector<BattleAttackSpawnRequest> makeAttackSpawnRequests(
    const BattleCastResult& result,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill)
{
    switch (result.decision.operationType)
    {
    case BattleOperationType::Melee:
        return makeMeleeRequests(result, input, selectedSkill);
    case BattleOperationType::TrackingProjectile:
        return makeTrackingProjectileRequests(result, input, selectedSkill);
    case BattleOperationType::RangedProjectile:
        return makeRangedProjectileRequests(result, input, selectedSkill);
    case BattleOperationType::Dash:
        return makeDashRequests(result, input, selectedSkill);
    default:
        assert(false);
        return {};
    }
}

void appendBlinkTeleportDelta(
    const BattleActionCommitInput& input,
    const BattleBlinkAttackCommand& command,
    const BattleRuntimeUnitRecords& units,
    BattleActionCommitResult& result)
{
    const auto& target = units.requireCore(command.targetUnitId);

    std::vector<const BattleBlinkCell*> candidates;
    for (const auto& cell : input.blinkGeometry.cells)
    {
        if (!cell.walkable || cell.occupied)
        {
            continue;
        }
        if (cell.gridX == input.blinkGeometry.currentGridX
            && cell.gridY == input.blinkGeometry.currentGridY)
        {
            continue;
        }
        if (pointDistance(cell.position, target.motion.position) > command.reach)
        {
            continue;
        }
        candidates.push_back(&cell);
    }

    if (candidates.empty())
    {
        return;
    }

    const int selectedIndex = input.blinkCellRandomRoll % static_cast<int>(candidates.size());
    const auto& cell = *candidates[selectedIndex];
    auto facing = target.motion.position - cell.position;
    if (facing.norm() > 0.01)
    {
        facing = normalizedTo(facing, 1.0, 0.01);
    }

    result.blinkTeleports.push_back({
        input.sourceUnitId,
        command.targetUnitId,
        command.selectedWeakest,
        cell.gridX,
        cell.gridY,
        cell.position,
        facing,
    });
    BattleLogEvent log;
    log.type = BattleLogEventType::Status;
    log.sourceUnitId = input.sourceUnitId;
    log.targetUnitId = command.targetUnitId;
    log.segments = battleLogText(command.selectedWeakest ? "閃擊追殺" : "閃擊突襲", BattleLogTextTone::SkillName);
    result.logEvents.push_back(std::move(log));
}

void appendBlinkAttackCommand(
    const BattleActionCommitInput& input,
    const BattleRuntimeUnitRecords& units,
    BattleActionCommitResult& result)
{
    if (firstAlwaysEffect(result.combo, EffectType::BlinkAttack) == nullptr)
    {
        return;
    }

    const auto& source = units.requireCore(input.sourceUnitId);
    BattleProjectileTargetingSystem targeting;
    const bool useWeakest = result.combo.blinkAttackUseWeakest;
    int targetId = useWeakest
            ? targeting.selectWeakestVulnerableEnemy(
            units,
            source.team,
            input.blinkWeakTargetDefWeight)
        : targeting.selectRandomEnemy(
            units,
            source.team,
            input.blinkRandomRoll);
    if (targetId < 0 && useWeakest)
    {
        targetId = targeting.selectRandomEnemy(
            units,
            source.team,
            input.blinkRandomRoll);
    }
    if (targetId < 0)
    {
        return;
    }

    BattleBlinkAttackCommand command{
        input.sourceUnitId,
        targetId,
        useWeakest,
        input.blinkReach,
    };
    result.blinkCommands.push_back(command);
    appendBlinkTeleportDelta(input, command, units, result);
    result.combo.blinkAttackUseWeakest = !useWeakest;
}

}  // namespace

void appendCastActionStartOutput(BattleCastResult& result,
                                 const BattleCastInput& input,
                                 const BattleCastSkillState& selectedSkill);

BattleCastResult BattleCastPlanner::plan(const BattleCastInput& input) const
{
    assert(input.unit.id >= 0);

    BattleCastResult result;
    auto& decision = result.decision;
    decision.unitId = input.unit.id;
    decision.targetUnitId = input.targetUnitId;

    bool ultimate = false;
    const auto& selectedSkill = selectSkill(input, ultimate);
    decision.ultimate = ultimate;
    decision.skillId = selectedSkill.id;
    decision.soundId = selectedSkill.soundId;

    decision.reason = blockedReason(input);
    if (decision.reason != BattleCastBlockReason::None)
    {
        return result;
    }

    if (selectedSkill.id < 0)
    {
        decision.reason = BattleCastBlockReason::NoSkill;
        return result;
    }

    assertCastIntentInput(input, selectedSkill);

    CombatIntentInput intentInput;
    intentInput.canStartAttack = input.unit.canStartAttack;
    intentInput.hasEquippedSkill = input.unit.hasEquippedSkill;
    intentInput.ultimateReady = ultimate;
    intentInput.movementDashActive = input.unit.movementDashActive;
    intentInput.dashAttackEnabled = input.unit.dashAttackEnabled;
    intentInput.targetDistance = input.targetDistance;
    intentInput.meleeAttackReach = input.unit.meleeAttackReach;
    intentInput.dashAttackReach = input.unit.dashAttackReach;
    intentInput.plannedSkill = toCombatSkill(selectedSkill);

    auto intent = BattleCombatIntentPlanner().select(intentInput);
    decision.equipSkill = intent.equipPlannedSkill;
    decision.announceUltimate = intent.announceUltimate;
    decision.canCast = intent.startAttack;
    decision.operationType = intent.operationType;
    if (!decision.canCast)
    {
        decision.reason = input.unit.movementDashActive
            ? BattleCastBlockReason::MovementDashActive
            : !input.unit.canStartAttack
                ? BattleCastBlockReason::AttackNotReady
                : BattleCastBlockReason::OutOfRange;
        return result;
    }
    appendCastActionStartOutput(result, input, selectedSkill);
    return result;
}

BattleCastResult BattleCastPlanner::commitSelectedCast(
    const BattleCastInput& input,
    bool ultimate,
    BattleOperationType operationType) const
{
    const auto& selectedSkill = ultimate ? input.ultimateSkill : input.normalSkill;
    return commitSelectedCast(input, selectedSkill, ultimate, operationType);
}

BattleCastResult BattleCastPlanner::commitSelectedCast(
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill,
    bool ultimate,
    BattleOperationType operationType) const
{
    assert(input.unit.id >= 0);
    assert(isBattleOperation(operationType));
    assert(selectedSkill.id >= 0);
    assertCastIntentInput(input, selectedSkill);

    BattleCastResult result;
    auto& decision = result.decision;
    decision.canCast = true;
    decision.ultimate = ultimate;
    decision.unitId = input.unit.id;
    decision.targetUnitId = input.targetUnitId;
    decision.skillId = selectedSkill.id;
    decision.soundId = selectedSkill.soundId;
    decision.operationType = operationType;

    appendCommittedCastOutput(result, input, selectedSkill);
    return result;
}

const BattleCastSkillState& BattleCastPlanner::selectSkill(const BattleCastInput& input, bool& ultimate) const
{
    ultimate = input.ultimateSkill.id >= 0 && input.unit.mp == input.unit.maxMp;
    if (ultimate)
    {
        return input.ultimateSkill;
    }
    return input.normalSkill;
}

BattleCastBlockReason BattleCastPlanner::blockedReason(const BattleCastInput& input) const
{
    if (!input.unit.alive)
    {
        return BattleCastBlockReason::Dead;
    }
    if (input.unit.frozen)
    {
        return BattleCastBlockReason::Frozen;
    }
    if (input.unit.stunned)
    {
        return BattleCastBlockReason::Stunned;
    }
    if (input.targetUnitId < 0)
    {
        return BattleCastBlockReason::NoTarget;
    }
    return BattleCastBlockReason::None;
}

void appendCastActionStartOutput(BattleCastResult& result,
                                 const BattleCastInput& input,
                                 const BattleCastSkillState& selectedSkill)
{
    assert(result.decision.canCast);
    assert(isBattleOperation(result.decision.operationType));
    assertCastOperationConfig(input.config, result.decision.operationType);
    assert(selectedSkill.id >= 0);

    result.animation.castFrame = castFrameForOperation(input.config, result.decision.operationType);
    result.animation.cooldownFrames = cooldownForOperation(input, selectedSkill, result.decision.operationType);
    result.animation.recoveryFrames = recoveryFramesForOperation(input.config, result.decision.operationType);
    result.cooldownDelta = result.animation.cooldownFrames;
    result.mpDelta = mpDeltaForCast(input, result.decision.ultimate);

    BattleGameplayEvent gameplayEvent;
    gameplayEvent.type = BattleGameplayEventType::CastStarted;
    gameplayEvent.sourceUnitId = input.unit.id;
    gameplayEvent.targetUnitId = input.targetUnitId;
    gameplayEvent.effectId = selectedSkill.id;
    result.gameplayEvents.push_back(gameplayEvent);

    BattleLogEvent logEvent;
    logEvent.type = BattleLogEventType::Status;
    logEvent.sourceUnitId = input.unit.id;
    logEvent.targetUnitId = input.targetUnitId;
    logEvent.skillName = selectedSkill.name;
    if (selectedSkill.name.empty())
    {
        logEvent.segments = battleLogText("出手");
    }
    else
    {
        logEvent.segments = logSegments(
            "施放",
            std::pair{ BattleLogTextTone::SkillName, selectedSkill.name });
    }
    result.logEvents.push_back(std::move(logEvent));

    if (result.decision.announceUltimate && !selectedSkill.name.empty())
    {
        BattleVisualEvent textEvent;
        textEvent.type = BattleVisualEventType::FloatingText;
        textEvent.targetUnitId = input.unit.id;
        textEvent.text = selectedSkill.name;
        textEvent.skillName = selectedSkill.name;
        textEvent.textSize = 36;
        textEvent.color = ultimateTextColor();
        result.visualEvents.push_back(textEvent);
    }

    if (selectedSkill.visualEffectId >= 0)
    {
        BattleVisualEvent effectEvent;
        effectEvent.type = BattleVisualEventType::RoleEffect;
        effectEvent.targetUnitId = input.unit.id;
        effectEvent.effectId = selectedSkill.visualEffectId;
        effectEvent.visualEffectId = selectedSkill.visualEffectId;
        effectEvent.durationFrames = result.animation.castFrame;
        result.visualEvents.push_back(effectEvent);
    }
}

void BattleCastPlanner::appendCommittedCastOutput(BattleCastResult& result,
                                                  const BattleCastInput& input,
                                                  const BattleCastSkillState& selectedSkill) const
{
    appendCastActionStartOutput(result, input, selectedSkill);
    assertCommittedCastInput(input, selectedSkill, result.decision.operationType);

    if (result.decision.operationType == BattleOperationType::Dash
        && input.unit.dashVelocity.norm() > input.config.minimumFacingNorm)
    {
        result.postDashRetreatVelocity = scaled(input.unit.dashVelocity, -1.0);
        result.postDashRetreatFrames = result.animation.recoveryFrames;
    }

    result.attackSpawnRequests = makeAttackSpawnRequests(result, input, selectedSkill);

}

BattleActionCommitResult BattleActionCommitSystem::commit(
    const BattleActionCommitInput& input,
    const RoleComboState& combo,
    const BattleRuntimeUnitRecords& units) const
{
    assert(input.sourceUnitId >= 0);
    assert(input.blinkRandomRoll >= 0);
    assert(input.strengthenedMeleeOperationCountThreshold > 0);

    const auto& source = units.requireCore(input.sourceUnitId);
    assert(source.operationCount >= 0);

    BattleActionCommitResult result;
    result.combo = combo;
    result.operationCount = source.operationCount;

    if (input.hasCast)
    {
        assert(input.cast.decision.canCast);
        result.attackSpawnRequests.insert(
            result.attackSpawnRequests.end(),
            input.cast.attackSpawnRequests.begin(),
            input.cast.attackSpawnRequests.end());
        if (input.cast.decision.ultimate)
        {
            result.combo.onSkillTeamHealPending = true;
        }
        result.operationCount = advanceOperationCountAfterCommittedCast(
            source.operationCount,
            input.cast.decision.ultimate,
            input.cast.decision.operationType,
            input.strengthenedMeleeOperationCountThreshold);
    }
    if (input.projectileBouncePrime.count > 0)
    {
        for (auto& request : result.attackSpawnRequests)
        {
            tryApplyProjectileBouncePrime(request, input.projectileBouncePrime);
        }
    }

    appendBlinkAttackCommand(input, units, result);
    return result;
}

}  // namespace KysChess::Battle
