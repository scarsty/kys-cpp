#include "BattleCastSystem.h"

#include "BattleMath.h"
#include "BattleCombatIntent.h"
#include "BattleProjectileTargetingSystem.h"

#include <algorithm>
#include <cassert>
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
    assert(pointNorm(facing) > input.config.minimumFacingNorm);
    return normalizedTo(facing, 1.0, input.config.minimumFacingNorm);
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

void appendExtraProjectiles(std::vector<BattleAttackSpawnRequest>& requests,
                            const BattleCastSkillState& selectedSkill)
{
    assert(selectedSkill.extraProjectileCount >= 0);
    assert(!requests.empty());
    const auto prototype = requests.front();
    for (int i = 0; i < selectedSkill.extraProjectileCount; ++i)
    {
        auto extra = prototype;
        extra.initial.castSubrequestKind = BattleAttackCastSubrequestKind::ExtraProjectile;
        requests.push_back(extra);
    }
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

    appendExtraProjectiles(requests, selectedSkill);
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
    assert(pointNorm(input.unit.dashVelocity) > input.config.minimumFacingNorm);
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
    case BattleOperationType::RangedProjectile:
    {
        std::vector<BattleAttackSpawnRequest> requests = {
            makeOperationRequest(
                result,
                input,
                selectedSkill,
                result.decision.operationType,
                BattleAttackCastSubrequestKind::SkillHit),
        };
        appendExtraProjectiles(requests, selectedSkill);
        return requests;
    }
    case BattleOperationType::Dash:
        return makeDashRequests(result, input, selectedSkill);
    default:
        assert(false);
        return {};
    }
}

BattleProjectileTargetWorld makeActionTargetWorld(const BattleActionCommitInput& input)
{
    BattleProjectileTargetWorld world;
    world.units.reserve(input.targets.size());
    for (const auto& target : input.targets)
    {
        world.units.push_back({
            target.id,
            target.team,
            target.alive,
            target.hp,
            target.maxHp,
            static_cast<int>(target.defence),
            target.invincible,
            target.position.x,
            target.position.y,
        });
    }
    return world;
}

const BattleActionTargetSnapshot* findActionTarget(const BattleActionCommitInput& input, int targetId)
{
    auto it = std::find_if(
        input.targets.begin(),
        input.targets.end(),
        [targetId](const BattleActionTargetSnapshot& target)
        {
            return target.id == targetId;
        });
    return it != input.targets.end() ? &*it : nullptr;
}

void appendBlinkTeleportDelta(
    const BattleActionCommitInput& input,
    const BattleBlinkAttackCommand& command,
    BattleActionCommitResult& result)
{
    const auto* target = findActionTarget(input, command.targetUnitId);
    if (!target)
    {
        return;
    }

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
        if (pointDistance(cell.position, target->position) > command.reach)
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
    auto facing = target->position - cell.position;
    if (pointNorm(facing) > 0.01)
    {
        facing = normalizedTo(facing, 1.0, 0.01);
    }

    result.blinkTeleports.push_back({
        input.unit.id,
        command.targetUnitId,
        command.selectedWeakest,
        cell.gridX,
        cell.gridY,
        cell.position,
        facing,
    });
}

void appendBlinkAttackCommand(const BattleActionCommitInput& input, BattleActionCommitResult& result)
{
    if (!result.combo.blinkAttack)
    {
        return;
    }

    auto targetWorld = makeActionTargetWorld(input);
    BattleProjectileTargetingSystem targeting;
    const bool useWeakest = result.combo.blinkAttackUseWeakest;
    int targetId = useWeakest
        ? targeting.selectWeakestVulnerableEnemy(
            targetWorld,
            input.unit.team,
            input.blinkWeakTargetDefWeight)
        : targeting.selectRandomEnemy(
            targetWorld,
            input.unit.team,
            input.blinkRandomRoll);
    if (targetId < 0 && useWeakest)
    {
        targetId = targeting.selectRandomEnemy(
            targetWorld,
            input.unit.team,
            input.blinkRandomRoll);
    }
    if (targetId < 0)
    {
        return;
    }

    BattleBlinkAttackCommand command{
        input.unit.id,
        targetId,
        useWeakest,
        input.blinkReach,
    };
    result.blinkCommands.push_back(command);
    appendBlinkTeleportDelta(input, command, result);
    result.combo.blinkAttackUseWeakest = !useWeakest;
}

void appendItemCommands(const BattleActionCommitInput& input, BattleActionCommitResult& result)
{
    if (!input.hasItem)
    {
        return;
    }

    assert(input.item.id >= 0);
    if (input.item.itemType == 3)
    {
        result.itemUseCommands.push_back({
            input.unit.id,
            input.item.id,
        });
    }
    else if (input.item.itemType == 4)
    {
        assert(input.hiddenWeaponTotalFrame > 0);
        BattleAttackSpawnRequest request;
        request.initial.attackerUnitId = input.unit.id;
        request.initial.position = input.unit.position;
        request.initial.velocity = input.hiddenWeaponVelocity;
        request.initial.hiddenWeaponItemId = input.item.id;
        request.initial.visualEffectId = input.item.hiddenWeaponEffectId;
        request.initial.totalFrame = input.hiddenWeaponTotalFrame;
        request.initial.operationType = BattleOperationType::None;
        request.initial.ignoreProjectileCancel = true;
        result.attackSpawnRequests.push_back(request);
    }

    result.itemCountDeltas.push_back({
        input.item.id,
        -1,
    });
}

}  // namespace

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
    appendCommittedCastOutput(result, input, selectedSkill);
    return result;
}

BattleCastResult BattleCastPlanner::commitSelectedCast(
    const BattleCastInput& input,
    bool ultimate,
    BattleOperationType operationType) const
{
    assert(input.unit.id >= 0);
    assert(isBattleOperation(operationType));

    const auto& selectedSkill = ultimate ? input.ultimateSkill : input.normalSkill;
    assert(selectedSkill.id >= 0);
    assertCastIntentInput(input, selectedSkill);

    BattleCastResult result;
    auto& decision = result.decision;
    decision.canCast = true;
    decision.ultimate = ultimate;
    decision.unitId = input.unit.id;
    decision.targetUnitId = input.targetUnitId;
    decision.skillId = selectedSkill.id;
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

void BattleCastPlanner::appendCommittedCastOutput(BattleCastResult& result,
                                                  const BattleCastInput& input,
                                                  const BattleCastSkillState& selectedSkill) const
{
    assert(result.decision.canCast);
    assert(isBattleOperation(result.decision.operationType));
    assertCommittedCastInput(input, selectedSkill, result.decision.operationType);

    result.animation.castFrame = castFrameForOperation(input.config, result.decision.operationType);
    result.animation.cooldownFrames = cooldownForOperation(input, selectedSkill, result.decision.operationType);
    result.animation.recoveryFrames = recoveryFramesForOperation(input.config, result.decision.operationType);
    result.cooldownDelta = result.animation.cooldownFrames;
    result.mpDelta = mpDeltaForCast(input, result.decision.ultimate);

    result.attackSpawnRequests = makeAttackSpawnRequests(result, input, selectedSkill);

    BattleGameplayEvent gameplayEvent;
    gameplayEvent.type = BattleGameplayEventType::CastStarted;
    gameplayEvent.sourceUnitId = input.unit.id;
    gameplayEvent.targetUnitId = input.targetUnitId;
    gameplayEvent.effectId = selectedSkill.id;
    result.gameplayEvents.push_back(gameplayEvent);

    if (!selectedSkill.name.empty())
    {
        BattleVisualEvent textEvent;
        textEvent.type = BattleVisualEventType::FloatingText;
        textEvent.targetUnitId = input.unit.id;
        textEvent.text = selectedSkill.name;
        textEvent.skillName = selectedSkill.name;
        textEvent.textSize = result.decision.ultimate ? 36 : 24;
        textEvent.color = result.decision.ultimate ? ultimateTextColor() : BattlePresentationColor{};
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

    result.effectEvents.push_back({ BattleHook::SkillFinished, input.unit.id, input.targetUnitId });
    if (result.decision.ultimate)
    {
        result.effectEvents.push_back({ BattleHook::UltimateCast, input.unit.id, input.targetUnitId });
    }
}

BattleActionCommitResult BattleActionCommitSystem::commit(const BattleActionCommitInput& input) const
{
    assert(input.unit.id >= 0);
    assert(input.unit.operationCount >= 0);
    assert(input.blinkRandomRoll >= 0);
    assert(input.strengthenedMeleeOperationCountThreshold > 0);

    BattleActionCommitResult result;
    result.combo = input.combo;
    result.operationCount = input.unit.operationCount;

    if (input.hasCast)
    {
        assert(input.cast.decision.canCast);
        result.visualEvents = input.cast.visualEvents;
        result.attackSpawnRequests.insert(
            result.attackSpawnRequests.end(),
            input.cast.attackSpawnRequests.begin(),
            input.cast.attackSpawnRequests.end());
        if (input.cast.decision.ultimate)
        {
            result.combo.onSkillTeamHealPending = true;
        }
        result.operationCount = advanceOperationCountAfterCommittedCast(
            input.unit.operationCount,
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

    appendBlinkAttackCommand(input, result);
    appendItemCommands(input, result);
    return result;
}

}  // namespace KysChess::Battle
