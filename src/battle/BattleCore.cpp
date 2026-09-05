#include "BattleCore.h"
#include "../Find.h"
#include "BattleAreaEffectSystem.h"
#include "BattleEffectEventBridge.h"
#include "BattleFrameContext.h"
#include "BattleProjectileEvents.h"
#include "BattleCoreDetail.h"
#include "BattleMovementPhysics.h"
#include "BattleRuntimeEffects.h"
#include <algorithm>
#include <cassert>
#include <memory_resource>
#include <optional>
#include <span>
#include <utility>



namespace KysChess::Battle
{


namespace
{


void prepareMovementAgents(BattleRuntimeState& state)
{
    for (auto& record : state.units.all())
    {
        const auto& unit = record.core;
        auto& agent = record.movement;
        agent.active = unit.alive || needsCorpsePhysics(unit.alive, unit.motion.position, unit.motion.velocity);
        if (!unit.alive)
        {
            state.movement.movementReservations.erase(unit.id);
        }
    }

    for (auto it = state.movement.movementReservations.begin(); it != state.movement.movementReservations.end();)
    {
        const auto& unit = state.units.requireCore(it->first);
        if (!unit.alive)
        {
            it = state.movement.movementReservations.erase(it);
            continue;
        }
        ++it;
    }
}

BattleMovementPlanInput makeFrameMovementPlanInput(
    BattleRuntimeState& state,
    std::span<const BattleFrameMovementPhysicsUnitResult> physicsResults,
    std::pmr::memory_resource* frameMemoryResource)
{
    BattleMovementPlanInput input(frameMemoryResource);
    input.frame = state.movement.frame;
    input.config = state.movement.config;
    input.terrainCellSource = &state.movement.terrainCells;
    input.terrainLayout = state.movement.terrainLayout;
    input.pathState = &state.movement.pathState;
    input.movementReservations = std::move(state.movement.movementReservations);
    input.yieldRequests = std::move(state.movement.yieldRequests);
    input.detourRequests = std::move(state.movement.detourRequests);
    input.units.reserve(state.units.size());

    for (const auto& record : state.units.live())
    {
        const auto& runtimeUnit = record.core;

        BattleUnitState movementUnit = makeBattleMovementPlanUnit(runtimeUnit, BattleRuntimeMoveSpeedDivisor);
        movementUnit.speed = static_cast<double>(
            effectAndAreaAdjustedSpeed(state, runtimeUnit.id, runtimeUnit.stats.speed))
            / BattleRuntimeMoveSpeedDivisor;
        const auto postPhysicsIt = std::find_if(
            physicsResults.begin(),
            physicsResults.end(),
            [&](const BattleFrameMovementPhysicsUnitResult& result)
            {
                return result.unitId == runtimeUnit.id;
            });
        if (postPhysicsIt != physicsResults.end())
        {
            movementUnit.position = postPhysicsIt->state.position;
            movementUnit.velocity = postPhysicsIt->state.velocity;
        }
        movementUnit.canAttack = runtimeUnit.animation.cooldown == 0;
        if (movementUnit.speed <= 0.0 && runtimeUnit.stats.speed > 0)
        {
            movementUnit.speed = runtimeUnit.stats.speed;
        }
        movementUnit.taXue = CoreDetail::runtimeDashAttackEnabled(state, runtimeUnit.id);
        const auto& agent = record.movement;
        const auto& physics = postPhysicsIt != physicsResults.end()
            ? postPhysicsIt->state
            : agent.physics;
        movementUnit.targetId = agent.targetId;
        movementUnit.assignedSlot = agent.assignedSlot;
        movementUnit.slotSwitchCooldownRemaining = agent.slotSwitchCooldownRemaining;
        movementUnit.dashFramesRemaining = physics.movementDashFrames;
        movementUnit.dashCooldownRemaining = physics.movementDashCooldown;
        movementUnit.postDashRetreatFramesRemaining = physics.postDashRetreatFrames;
        movementUnit.postDashChaosFramesRemaining = physics.postDashChaosFrames;
        movementUnit.movementDashSpreadFramesRemaining = physics.movementDashSpreadFrames;
        movementUnit.knockbackFramesRemaining = physics.knockbackFrames;
        movementUnit.knockbackControlFramesRemaining = physics.knockbackControlFrames;
        input.units.push_back(std::move(movementUnit));
    }

    return input;
}


bool hasCommittedReflectedReturn(const BattleRuntimeState& state)
{
    const bool liveReturn = std::ranges::any_of(
        state.attacks.attacks,
        [](const BattleAttackInstance& attack)
        {
            return attack.state.reflectionLineage
                    == BattleAttackReflectionLineageKind::ReflectedReturn
                && !attack.noHurt
                && !attack.finishReason
                && attack.frame < attack.state.totalFrame;
        });
    if (liveReturn)
    {
        return true;
    }

    return std::ranges::any_of(
        state.nextFrame.queuedAttacks(),
        [](const BattleAttackSpawnRequest& request)
        {
            return request.initial.reflectionLineage
                == BattleAttackReflectionLineageKind::ReflectedReturn;
        });
}


BattleMovementPhysicsCollisionWorld makeMovementPhysicsCollisionWorld(
    const BattleRuntimeState& state,
    std::pmr::memory_resource* frameMemoryResource)
{
    BattleMovementPhysicsCollisionWorld collision(frameMemoryResource);
    collision.tileWidth = state.movementPhysics.terrain.tileWidth;
    collision.coordCount = state.movementPhysics.terrain.coordCount;
    collision.defaultSeparationDistance = state.movementPhysics.terrain.defaultSeparationDistance;
    collision.walkableCellSource = &state.movementPhysics.terrain.walkableByCell;
    collision.units.reserve(state.units.size());
    for (const auto& record : state.units.all())
    {
        const auto& unit = record.core;
        collision.units.push_back({
            unit.id,
            unit.alive,
            unit.motion.position,
        });
    }
    return collision;
}

bool frozenUnitShouldAdvancePhysics(const BattleMovementPhysicsState& state)
{
    return state.knockbackFrames > 0 || state.knockbackControlFrames > 0;
}


BattleFrameVector<BattleFrameMovementPhysicsUnitResult> computeMovementPhysics(
    BattleRuntimeState& state,
    std::pmr::memory_resource* frameMemoryResource)
{
    BattleFrameVector<BattleFrameMovementPhysicsUnitResult> physicsResults(frameMemoryResource);
    if (state.units.empty())
    {
        return physicsResults;
    }
    if (state.movementPhysics.terrain.walkableByCell.empty())
    {
        return physicsResults;
    }

    assert(state.movementPhysics.terrain.tileWidth > 0.0);
    assert(state.movementPhysics.terrain.coordCount > 0);
    assert(state.movementPhysics.terrain.defaultSeparationDistance > 0.0);

    auto collision = makeMovementPhysicsCollisionWorld(state, frameMemoryResource);
    physicsResults.reserve(state.units.size());

    for (auto& record : state.units.all())
    {
        auto& unit = record.core;
        assert(unit.id >= 0);
        auto& agent = record.movement;
        if (!agent.active)
        {
            continue;
        }
        auto& physics = agent.physics;

        BattleFrameMovementPhysicsUnitResult result;
        result.unitId = unit.id;
        result.state = physics;
        result.state.position = unit.motion.position;
        result.state.velocity = unit.motion.velocity;
        result.state.acceleration = unit.motion.acceleration;
        result.frozenFrames = record.frozenFrames();

        const bool frozenThisFrame = result.frozenFrames > 0;
        if (result.frozenFrames > 0)
        {
            --result.frozenFrames;
            if (!frozenUnitShouldAdvancePhysics(result.state))
            {
                physicsResults.push_back(std::move(result));
                continue;
            }
        }

        bool actionDashActive = false;
        if (!frozenThisFrame && unit.operationType == BattleOperationType::Dash && unit.haveAction)
        {
            const auto operation = static_cast<int>(unit.operationType);
            assert(operation >= 0 && operation < static_cast<int>(state.action.castConfig.castFrames.size()));
            const int dashStartFrame = state.action.castConfig.castFrames[operation];
            const int dashEndFrame = dashStartFrame + state.movement.config.dashFrames;
            actionDashActive = unit.animation.actFrame >= dashStartFrame
                && unit.animation.actFrame <= dashEndFrame;
            if (unit.animation.actFrame > dashEndFrame)
            {
                result.state.velocity = { 0, 0, 0 };
            }
        }

        BattleMovementPhysicsInput physicsInput;
        physicsInput.state = result.state;
        physicsInput.config = state.movementPhysics.config;
        physicsInput.collisionWorld = &collision;
        physicsInput.unitId = unit.id;
        physicsInput.currentPosition = unit.motion.position;
        physicsInput.actionDashActive = actionDashActive;
        physicsInput.unitAlive = unit.alive;
        BattleUnitState movementSnapshot;
        movementSnapshot.taXue = CoreDetail::runtimeDashAttackEnabled(state, unit.id);
        movementSnapshot.velocity = result.state.velocity;
        movementSnapshot.dashFramesRemaining = result.state.movementDashFrames;
        movementSnapshot.dashCooldownRemaining = result.state.movementDashCooldown;
        movementSnapshot.movementDashSpreadFramesRemaining = result.state.movementDashSpreadFrames;
        movementSnapshot.postDashRetreatFramesRemaining = result.state.postDashRetreatFrames;
        movementSnapshot.postDashChaosFramesRemaining = result.state.postDashChaosFrames;
        movementSnapshot.knockbackFramesRemaining = result.state.knockbackFrames;
        movementSnapshot.knockbackControlFramesRemaining = result.state.knockbackControlFrames;
        physicsInput.ignoreUnitCollision = !unit.alive
            || battleMovementTaXueUnstable(movementSnapshot)
            || (result.state.knockbackFrames > 0
                && result.state.knockbackIgnoresUnitCollision);

        const bool cancelForcedMoveWhenBlocked = result.state.knockbackFrames > 0
            && result.state.knockbackCancelsWhenBlocked;
        const auto expectedKnockbackPosition =
            result.state.position + result.state.velocity;
        result.state = BattleMovementPhysicsSystem().advance(physicsInput);
        result.physicsAdvanced = true;
        if (cancelForcedMoveWhenBlocked
            && (result.state.position - expectedKnockbackPosition).norm() > 0.01)
        {
            result.state.position = result.state.knockbackOrigin;
        }

        if (auto* collisionUnit = tryFindById(collision.units, result.unitId))
        {
            collisionUnit->position = result.state.position;
        }
        physicsResults.push_back(std::move(result));
    }
    return physicsResults;
}

BattleTickResult commitFrameMovement(
    BattleRuntimeState& state,
    std::span<const BattleFrameMovementPhysicsUnitResult> physicsResults,
    BattleTickResult movement)
{
    state.movement.frame = movement.frame;
    state.movement.movementReservations = std::move(movement.movementReservations);
    state.movement.yieldRequests = std::move(movement.yieldRequests);
    state.movement.detourRequests = std::move(movement.detourRequests);
    for (const auto& [unitId, decision] : movement.decisions)
    {
        auto& agent = state.units.require(unitId).movement;
        agent.targetId = decision.targetId;
        agent.assignedSlot = decision.slot;
        agent.slotSwitchCooldownRemaining = decision.slotSwitchCooldownRemaining;
    }

    for (const auto& physicsResult : physicsResults)
    {
        auto& unit = state.units.requireCore(physicsResult.unitId);
        auto& physics = state.units.require(physicsResult.unitId).movement.physics;

        unit.motion.position = physicsResult.state.position;
        unit.motion.velocity = physicsResult.state.velocity;
        unit.motion.acceleration = physicsResult.state.acceleration;
        physics = physicsResult.state;
        state.units.require(physicsResult.unitId).commitFrozenPhysicsFrames(physicsResult.frozenFrames);
    }

    if (state.movementPhysics.terrain.walkableByCell.empty())
    {
        return movement;
    }

    assert(state.gridTransform.tileWidth > 0.0);
    assert(state.gridTransform.coordCount > 0);

    for (const auto& [unitId, decision] : movement.decisions)
    {
        auto& runtimeUnit = state.units.requireCore(unitId);
        const auto action = decision.action;

        auto& physics = state.units.require(unitId).movement.physics;
        if (physics.knockbackFrames > 0 || physics.knockbackControlFrames > 0)
        {
            continue;
        }

        Pointf syncedVelocity = decision.velocity;
        if (action == MovementAction::Move)
        {
            syncedVelocity = { 0.0f, 0.0f, decision.velocity.z };
        }

        const auto acceleration = runtimeUnit.motion.acceleration;
        const Pointf syncedPosition = action == MovementAction::Hold
            ? runtimeUnit.motion.position
            : decision.destination;
        state.units.setMotion(
            unitId,
            syncedPosition,
            syncedVelocity,
            acceleration,
            state.gridTransform,
            action == MovementAction::Dash);

        physics.position = syncedPosition;
        physics.velocity = syncedVelocity;
        physics.acceleration = acceleration;
        physics.movementDashFrames = decision.dashFramesRemaining;
        physics.movementDashCooldown = decision.dashCooldownRemaining;
        physics.movementDashSpreadFrames = decision.movementDashSpreadFramesRemaining;
        physics.postDashRetreatFrames = decision.postDashRetreatFramesRemaining;
        physics.postDashChaosFrames = decision.postDashChaosFramesRemaining;
        physics.knockbackFrames = decision.knockbackFramesRemaining;
        physics.knockbackControlFrames = decision.knockbackControlFramesRemaining;
    }
    return movement;
}

BattleTickResult advanceMotionFrame(
    BattleRuntimeState& state,
    std::pmr::memory_resource* frameMemoryResource)
{
    prepareMovementAgents(state);
    CoreDetail::refreshRuntimeMovementProfiles(state);
    auto physicsResults = computeMovementPhysics(state, frameMemoryResource);
    auto movementInput = makeFrameMovementPlanInput(
        state,
        physicsResults,
        frameMemoryResource);
    auto movement = BattleMovementPlanner(std::move(movementInput)).tick();
    return commitFrameMovement(state, physicsResults, std::move(movement));
}


void completeFinishedRuntimeAttackWork(BattleRuntimeState& state)
{
    state.attacks.completeFinished(state.castLifecycle);
}

void eraseFinishedRuntimeAttacks(BattleRuntimeState& state)
{
    state.attacks.eraseFinished();
}

void discardBattleEndFrameContinuationWork(BattleFrameContext& frame)
{
    (void)frame.drainCommands();
    (void)frame.drainCurrentFrameAttacks();
    (void)frame.drainCurrentFrameDamage();
    (void)frame.drainAreaProjectileFollowUps();
    (void)frame.drainCastCommitBarriers();
    (void)frame.drainEffectCommandBatches();
}
}  // namespace

void cancelBattleRuntimeForBattleEnd(BattleRuntimeState& state, int frame)
{
    assert(state.result.ended);
    assert(frame >= 0);
    if (state.castLifecycle.snapshot().terminalState
        == BattleCastLifecycleTerminalState::BattleEnded)
    {
        return;
    }

    state.attacks.completeFinished(state.castLifecycle);
    state.attacks.cancelAllForBattleEnd(state.castLifecycle);
    state.attacks.clearCastContactSuppressions();
    state.nextFrame.cancelForBattleEnd(state.castLifecycle);

    for (const auto& [castId, context] : state.effectIntegration.casts)
    {
        (void)context;
        BattleEffectEventBridge().releaseCastScopedRules(state, castId);
    }
    state.effectIntegration.casts.clear();
    state.effectIntegration.appliedPerCastDamage.clear();
    state.effectIntegration.queuedCommandBatches.clear();
    state.effectIntegration.damageContinuations.clear();
    for (auto& unit : state.units.all())
    {
        unit.clearActionOwners();
    }
    state.castLifecycle.cancelOutstandingForBattleEnd(frame);
}

BattleDamageUnitState makeBattleDamageUnitState(
    const BattleRuntimeUnit& unit,
    const BattleDamageRuntimeUnit* runtime)
{
    return CoreDetail::makeBattleDamageUnitStateFromRuntime(unit, runtime);
}

void writeBattleDamageRuntimeUnit(BattleDamageRuntimeUnit& runtime, const BattleDamageUnitState& unit)
{
    CoreDetail::writeBattleDamageRuntimeUnitImpl(runtime, unit);
}

BattleCooldownState makeBattleFrameCooldownState(const BattleRuntimeUnit& unit)
{
    return CoreDetail::makeBattleFrameCooldownStateImpl(unit);
}

BattleFrameRunner::BattleFrameRunner()
    : frameMemoryStorage_(FrameMemoryBytes)
{
}

BattlePresentationFrame BattleFrameRunner::runFrame(BattleRuntimeState& state) const
{
    return runFrame(state, {});
}

BattlePresentationFrame BattleFrameRunner::runFrame(
    BattleRuntimeState& state,
    BattlePresentationFrame recycledPresentation) const
{
    assert(!state.units.empty());

    BattleAreaEffectSystem::removeExpired(state.areas, state.movement.frame + 1);
    auto frame = BattleFrameContext::begin(
        state,
        std::move(recycledPresentation),
        frameMemoryStorage_.data(),
        frameMemoryStorage_.size());

    const int upcomingFrame = state.movement.frame + 1;
    auto expiredDamageAbsorptions =
        BattleEffectCommandSystem::removeExpiredDamageAbsorptions(
            state,
            upcomingFrame);
    CoreDetail::appendDamageAbsorptionSettlements(
        state,
        frame.currentFrameDamage(),
        expiredDamageAbsorptions,
        upcomingFrame);

    // One frame-start snapshot orders configured and status-owned rules
    // together. Commands are reduced before timers advance, so a configured
    // removal can suppress a later status rule while a surviving contribution
    // still receives its final eligible tick before expiry.
    auto deferredFrameEffectBatches = CoreDetail::dispatchFrameAdvancedEffects(
        state,
        frame,
        upcomingFrame);
    // Tick status timers after the merged per-frame rules have dispatched.
    CoreDetail::advanceStatus(state, frame.currentFrameDamage());
    // Tick unit cooldown/action/MP timers and collect typed skill-finished effects.
    auto runtimeAdvance = CoreDetail::advanceRuntimeUnits(state);
    for (auto& batch : runtimeAdvance.cooldownFinishedEffects)
    {
        frame.queueEffectCommands(
            std::move(batch.commands),
            std::move(batch.context));
    }
    CoreDetail::reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    // Reduce early gameplay commands into concrete queues/state; currently mostly a pre-movement drain point.
    CoreDetail::reduceCommandsBeforeMovement(state, frame);
    // Advance and commit motion, e.g. physics and tactical movement.
    auto movement = advanceMotionFrame(state, frame.frameMemoryResource());
    // Start or commit unit actions, e.g. cast startup, attack spawn requests, blink teleports, action sounds.
    CoreDetail::advanceActionFrameUnits(state, frame, movement);
    // Typed cast-commit effects are delayed until every unit has selected its
    // action for this frame, then reduced before any resulting attack spawns.
    CoreDetail::reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    CoreDetail::completeCastCommitBarriers(state, frame);
    // Reduce cast-release effects, e.g. 出手回內、全隊盾、當前生命傷害, before attacks/damage apply.
    CoreDetail::reduceCommandsBeforeAttacks(state, frame);
    // Spawn/tick attacks and resolve hits; hit commands are reduced immediately into damage/effect queues.
    CoreDetail::advanceAttacksAndResolveHits(state, frame);
    CoreDetail::appendAreaDamagePulses(state, frame);
    // Apply queued damage and lifecycle effects, e.g. HP loss, death, rescue, death AOE, battle end.
    CoreDetail::applyDamageAndLifecycle(state, frame);
    state.nextFrame.recycleDamage(frame.drainCurrentFrameDamage());
    if (state.result.ended)
    {
        appendProjectileCancellationLogEvents(
            state.attacks,
            frame.attackEvents,
            frame.logEvents,
            true);
        CoreDetail::applyLateFrameMpRestores(state, frame);
        discardBattleEndFrameContinuationWork(frame);
        CoreDetail::emitPresentationFrame(state, frame);
        cancelBattleRuntimeForBattleEnd(state, state.result.endedFrame);
        return consumeBattleFrameContext(std::move(frame));
    }
    // Chain terminal logs are emitted after damage so the projectile visibly lands before the chain result.
    appendProjectileCancellationLogEvents(state.attacks, frame.attackEvents, frame.logEvents, true);
    CoreDetail::applyLateFrameMpRestores(state, frame);
    for (auto& batch : deferredFrameEffectBatches)
    {
        frame.queueEffectCommands(
            std::move(batch.commands),
            std::move(batch.context));
    }
    // 週期自動絕招固定在延後效果批次階段執行。
    CoreDetail::reduceEffectCommandBatches(
        state,
        frame,
        state.nextFrame.mutableDamageForReducer());
    // Reduce late commands from damage/combo lifecycle, e.g. auto-ultimate or death-triggered projectiles.
    CoreDetail::reduceCommandsAfterDamageLifecycle(state, frame);
    // Direct auto-ultimate commits share the typed commit pipeline. Reduce their
    // committed effects and any resulting gameplay commands before releasing the
    // commit barrier, including when the auto cast was created late in this frame.
    CoreDetail::reduceEffectCommandBatches(
        state,
        frame,
        state.nextFrame.mutableDamageForReducer());
    CoreDetail::reduceCommandsAfterDamageLifecycle(state, frame);
    CoreDetail::completeCastCommitBarriers(state, frame);
    // A cast may settle only after its final attack's damage/death descendants have
    // had a chance to reserve work. Keep finished attacks alive for presentation.
    completeFinishedRuntimeAttackWork(state);
    if (state.movement.frame < state.maximumFrames)
    {
        CoreDetail::dispatchReadyCastLifecycleEffects(state, frame);
    }
    assert(frame.drainCommands().empty());
    // Convert accumulated gameplay/log/visual events into the presentation frame consumed by the scene.
    CoreDetail::emitPresentationFrame(state, frame);
    // Runtime maintenance: remove projectiles/melee attacks whose animation lifetime has finished.
    eraseFinishedRuntimeAttacks(state);
    return consumeBattleFrameContext(std::move(frame));
}

}  // namespace KysChess::Battle

namespace KysChess::Battle::CoreDetail
{


CastPlanEventData makeCastPlanEventData(
    const BattleCastInput& input,
    bool ultimate,
    const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast,
    const BattleCastProvenance* provenance)
{
    const auto resources = resourcesBeforeCast.values();
    const auto& skill = ultimate ? input.ultimateSkill : input.normalSkill;
    const auto ownerResource = std::ranges::find(
        resources,
        input.unit.id,
        &EffectUnitResourceBeforeCast::unitId);
    const int mpBefore = ownerResource != resources.end()
        ? ownerResource->mp
        : input.unit.mp;
    return makeCastPlanEventData(
        input.unit.id,
        skill.id,
        ultimate,
        input.targetUnitId,
        mpBefore,
        input.unit.maxMp,
        input.config.normalCastMpDelta,
        skill.forceRanged,
        resourcesBeforeCast,
        provenance);
}


void applyKnockbackImpulse(
    BattleRuntimeState& state,
    const BattleKnockbackCommand& knockback)
{
    auto& record = state.units.require(knockback.targetUnitId);
    if (knockback.semanticDirection == ForceMoveDirection::AwayFromSource
        && runtimeDashAttackEnabled(state, knockback.targetUnitId))
    {
        return;
    }
    if (BattleAreaEffectSystem::blocksForcedMovement(
            state.areas,
            state.gridTransform,
            state.units,
            knockback.targetUnitId,
            state.movement.frame,
            knockback.semanticDirection))
    {
        return;
    }

    auto& unit = record.core;
    auto direction = knockback.direction;
    if (direction.norm() <= 0.01f || knockback.distance <= 0.0)
    {
        return;
    }
    direction.normTo(1.0f);
    const int lockFrames = std::max(1, knockback.lockFrames);
    const auto& config = state.movementPhysics.config;

    auto distanceForVelocity = [&config](Pointf velocity, int frames)
    {
        const int activeFrames = std::max(0, frames);
        if (activeFrames == 0)
        {
            return Pointf{};
        }
        const double distance = std::max(
            0.0,
            static_cast<double>(velocity.norm()) * activeFrames
                - config.friction * static_cast<double>(activeFrames * (activeFrames - 1)) / 2.0);
        if (distance <= 0.0)
        {
            return Pointf{};
        }
        velocity.normTo(static_cast<float>(distance));
        return velocity;
    };

    auto remainingDistance = direction;
    remainingDistance.normTo(static_cast<float>(knockback.distance));

    auto& physics = state.units.require(knockback.targetUnitId).movement.physics;
    if (physics.knockbackFrames <= 0)
    {
        physics.knockbackOrigin = unit.motion.position;
    }
    if (physics.knockbackFrames > 0 || physics.knockbackControlFrames > 0)
    {
        remainingDistance += distanceForVelocity(unit.motion.velocity, physics.knockbackFrames);
    }

    const int combinedLockFrames = std::max(physics.knockbackFrames, lockFrames);
    const double frictionDistance = config.friction * static_cast<double>(combinedLockFrames * (combinedLockFrames - 1)) / 2.0;
    auto velocity = remainingDistance;
    velocity.normTo(static_cast<float>((remainingDistance.norm() + frictionDistance) / static_cast<double>(combinedLockFrames)));
    unit.motion.velocity = velocity;

    physics.velocity = velocity;
    physics.knockbackVelocity = velocity;
    physics.knockbackFrames = combinedLockFrames;
    physics.knockbackControlFrames = physics.knockbackFrames + 1;
    physics.knockbackIgnoresUnitCollision =
        knockback.collision == ForceMoveCollision::StopBeforeBlocked;
    physics.knockbackCancelsWhenBlocked =
        knockback.blocked == ForceMoveBlockedResult::Stop;
    physics.postDashRetreatFrames = 0;
    physics.postDashChaosFrames = 0;
    physics.movementDashSpreadFrames = 0;
}


void updateFrameBattleResultAfterDamage(BattleRuntimeState& state, BattleFrameContext& frame)
{
    if (state.result.ended)
    {
        return;
    }

    std::optional<int> aliveTeam;
    for (const auto& record : state.units.live())
    {
        if (aliveTeam && *aliveTeam != record.core.team)
        {
            return;
        }
        aliveTeam = record.core.team;
    }

    // Accepted contact 已承諾的回程彈道仍可能改變最後存活隊伍；即使反射者
    // 已被來襲傷害擊敗，也必須讓該彈道及其延續完成。
    if (hasCommittedReflectedReturn(state))
    {
        return;
    }

    state.result.ended = true;
    state.result.winningTeam = aliveTeam.value_or(0);
    state.result.endedFrame = state.movement.frame;
    state.result.eventEmitted = true;
    state.result.outcome = state.result.winningTeam == 0
        ? BattleOutcome::PlayerVictory
        : BattleOutcome::PlayerDefeat;

    frame.gameplayEvents.push_back({
        BattleGameplayEventType::BattleEnded,
        state.movement.frame,
        -1,
        -1,
        state.result.winningTeam,
    });
    frame.logEvents.push_back({
        BattleLogEventType::BattleEnded,
        state.movement.frame,
        -1,
        -1,
        state.result.winningTeam,
    });
}

}  // namespace KysChess::Battle::CoreDetail
