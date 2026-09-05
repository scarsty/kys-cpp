#include "BattleCoreDetail.h"
#include "BattleCombatIntent.h"
#include "BattleEffectAttackCastSystem.h"
#include "BattleEffectEventBridge.h"
#include "BattleFrameContext.h"
#include "BattleLogSegments.h"
#include "BattleMath.h"
#include "BattleMovementPhysics.h"
#include "BattleRuntimeEffects.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <format>
#include <iterator>
#include <limits>
#include <memory_resource>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>



namespace KysChess::Battle
{


namespace
{

struct BattleCopiedAttackDefinitionRequest
{
    int definitionOwnerUnitId = -1;
    CastPropagationPolicy propagation = CastPropagationPolicy::SuppressUltimateRules;
};

constexpr int ActionCastFrameJitterRadius = 1;
constexpr int ActionCastFrameJitterChoices = ActionCastFrameJitterRadius * 2 + 1;

struct RuntimeCastPolicies
{
    std::optional<ModifyCastAction> forceRanged;
    bool dashAttack{};
    bool blinkAttack{};
};

struct RuntimeCastSkillProfile
{
    int effectiveSelectDistance{};
    int projectileSpeedMultiplierPct = 100;
    double reach{};
    double blinkReach{};
    bool forceRanged = false;
    bool rangedStyle = false;
};

struct BattleActionFrameState
{
    int cooldown{};
    int cooldownMax{};
    int actFrame{};
    int actType = -1;
    BattleOperationType operationType = BattleOperationType::None;
    bool haveAction{};
};


RuntimeCastSkillProfile makeRuntimeCastSkillProfile(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed,
    bool ultimate,
    const RuntimeCastPolicies* precomputedPolicies);
BattleBlinkGeometryInput makeRuntimeBlinkGeometry(const BattleRuntimeState& state, const BattleRuntimeUnit& unit, double reach);
const BattleCastSkillState& selectedCastSkill(const BattleCastInput& castInput, const BattleCastResult& cast);

RuntimeCastPolicies runtimeCastPolicies(
    std::span<const EffectExactRuntimeRuleMatch> matches)
{
    RuntimeCastPolicies result;
    for (const auto& match : matches)
    {
        assert(match.bound);
        const auto* bound = match.bound;
        for (const auto& effectAction : bound->rule.actions)
        {
            const auto* action = std::get_if<ModifyCastAction>(&effectAction.value);
            if (!action)
            {
                continue;
            }
            if (!result.forceRanged
                && action->rangeMode == CastRangeMode::Ranged)
            {
                result.forceRanged = *action;
            }
            result.dashAttack = result.dashAttack
                || action->mobility == CastMobilityPolicy::DashAttack;
            result.blinkAttack = result.blinkAttack
                || action->mobility == CastMobilityPolicy::BlinkAttack;
        }
    }
    return result;
}

BattleAttackBouncePrime collectRuntimeProjectileBouncePrime(
    std::span<const EffectExactRuntimeRuleMatch> matches,
    int rollPct,
    int defaultRange)
{
    BattleAttackBouncePrime result;
    result.rollPct = rollPct;
    result.range = defaultRange;
    for (const auto& match : matches)
    {
        assert(match.bound);
        const auto* bound = match.bound;
        for (const auto& effectAction : bound->rule.actions)
        {
            const auto* action = std::get_if<ModifyAttackAction>(&effectAction.value);
            if (!action)
            {
                continue;
            }
            const auto* bounce = std::get_if<ProjectileBounceAttackBehavior>(
                &action->runtimeBehavior);
            if (!bounce)
            {
                continue;
            }
            result.count += bounce->additionalHits;
            result.chancePct = std::min(100, result.chancePct + bounce->chancePct);
            result.range = std::max(result.range, bounce->rangePixels);
        }
    }
    return result;
}

int collectRuntimeUltimateExtraProjectileCount(
    BattleRuntimeState& state,
    std::span<const EffectExactRuntimeRuleMatch> matches,
    int baseCount)
{
    int result = baseCount;
    for (const auto& match : matches)
    {
        assert(match.bound);
        const auto* bound = match.bound;
        for (const auto& effectAction : bound->rule.actions)
        {
            const auto* action = std::get_if<ModifyCastAction>(&effectAction.value);
            if (!action || action->additionalProjectiles <= 0)
            {
                continue;
            }
            if (state.effectRules.tryActivateRuntimeRule(
                    bound->binding,
                    bound->rule.id,
                    state.movement.frame,
                    state.random))
            {
                result += action->additionalProjectiles;
            }
        }
    }
    return result;
}

std::optional<DelayedAlternateAttackBehavior> runtimeDelayedAlternateAttack(
    std::span<const EffectExactRuntimeRuleMatch> matches)
{
    for (const auto& match : matches)
    {
        assert(match.bound);
        const auto* bound = match.bound;
        for (const auto& effectAction : bound->rule.actions)
        {
            const auto* action = std::get_if<ModifyAttackAction>(&effectAction.value);
            if (!action)
            {
                continue;
            }
            if (const auto* alternate = std::get_if<DelayedAlternateAttackBehavior>(
                    &action->runtimeBehavior))
            {
                return *alternate;
            }
        }
    }
    return std::nullopt;
}

void appendAttackSpawnRequests(
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    std::vector<BattleAttackSpawnRequest>& requests)
{
    attackSpawns.insert(
        attackSpawns.end(),
        std::make_move_iterator(requests.begin()),
        std::make_move_iterator(requests.end()));
    requests.clear();
}

template <class Payload>
std::vector<EffectExactRuntimeRuleMatch> queryExactRuntimeRules(
    BattleRuntimeState& state,
    int ownerUnitId,
    EffectEvent event,
    Payload payload)
{
    const auto owned = BattleEffectEventBridge().makeEvent(
        state,
        {
            .frame = state.movement.frame,
            .eventOrdinal = state.effectIntegration.nextEventOrdinal,
            .ownerUnitId = ownerUnitId,
        },
        event,
        std::move(payload));
    return BattleEffectSystem().queryExactRuntimeRules(
        state.effectRules,
        owned.context(),
        state.random);
}

RuntimeCastPolicies runtimeCastPoliciesForSkill(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    int magicId,
    bool ultimate,
    int preferredTargetUnitId = -1,
    const BattleCastProvenance* provenance = nullptr)
{
    if (magicId < 0)
    {
        return {};
    }
    const auto resources = CoreDetail::snapshotEffectResourcesBeforeCast(state);
    auto payload = CoreDetail::makeCastPlanEventData(
        unit.id,
        magicId,
        ultimate,
        preferredTargetUnitId,
        unit.vitals.mp,
        unit.vitals.maxMp,
        state.action.castConfig.normalCastMpDelta,
        false,
        resources,
        provenance);
    return runtimeCastPolicies(queryExactRuntimeRules(
        state,
        unit.id,
        EffectEvent::CastPlanned,
        std::move(payload)));
}


std::vector<BattleCopiedAttackDefinitionRequest> collectCopiedAttackDefinitionRequests(
    std::span<const BattleEffectDispatchResult> committedEffects)
{
    std::vector<BattleCopiedAttackDefinitionRequest> result;
    for (const auto& dispatched : committedEffects)
    {
        for (const auto& command : dispatched.commands)
        {
            const auto* stateMachine = std::get_if<StateMachineEffectCommand>(&command.value);
            if (!stateMachine)
            {
                continue;
            }
            const auto* copy = std::get_if<CopyAttackDefinitionAction>(&stateMachine->action);
            if (!copy)
            {
                continue;
            }
            for (int unitId : stateMachine->selectedSourceUnitIds)
            {
                result.push_back({ unitId, copy->propagation });
            }
        }
    }
    return result;
}

void refreshMovementSkillProfile(
    BattleUnitState& movementUnit,
    const BattleRuntimeUnit& runtimeUnit,
    BattleRuntimeState& state)
{
    const auto* seed = state.units.require(runtimeUnit.id).actionPlan();
    if (!seed)
    {
        if (runtimeUnit.reach > 0.0)
        {
            movementUnit.reach = runtimeUnit.reach;
            movementUnit.style = runtimeUnit.style;
        }
        return;
    }

    const bool useUltimate = runtimeUnit.vitals.maxMp > 0
        && runtimeUnit.vitals.mp >= runtimeUnit.vitals.maxMp
        && seed->ultimateSkill.id >= 0;
    const auto& selectedSeed = useUltimate ? seed->ultimateSkill : seed->normalSkill;
    const auto policies = runtimeCastPoliciesForSkill(
        state,
        runtimeUnit,
        selectedSeed.id,
        useUltimate);
    const auto skill = makeRuntimeCastSkillProfile(
        state,
        runtimeUnit,
        selectedSeed,
        useUltimate,
        &policies);
    movementUnit.reach = skill.reach > 0.0
        ? skill.reach
        : state.movement.config.meleeAttackReach;
    movementUnit.style = skill.rangedStyle ? CombatStyle::Ranged : CombatStyle::Melee;
    movementUnit.taXue = policies.dashAttack;
}


bool runtimeForcedRangedMagic(const BattleActionSkillSeed& skill, bool forceRanged)
{
    return forceRanged && (skill.attackAreaType == 0 || skill.attackAreaType == 3);
}

bool runtimeProjectileStyleMagic(const BattleActionSkillSeed& skill, bool forceRanged)
{
    return skill.id >= 0
        && (skill.attackAreaType == 1
            || skill.attackAreaType == 2
            || runtimeForcedRangedMagic(skill, forceRanged));
}

bool runtimeBattleRangedStyle(const BattleActionSkillSeed& skill, bool forceRanged)
{
    return skill.id >= 0
        && (skill.attackAreaType == 1
            || skill.attackAreaType == 2
            || skill.attackAreaType == 3
            || runtimeForcedRangedMagic(skill, forceRanged));
}

int runtimeEffectiveProjectileSelectDistance(
    const BattleActionSkillSeed& skill,
    bool forcedRanged,
    int forcedRangedMinSelectDistance)
{
    int selectDistance = std::max(1, skill.selectDistance);
    if (forcedRanged && (skill.attackAreaType == 0 || skill.attackAreaType == 3))
    {
        selectDistance = std::max(selectDistance, std::max(1, forcedRangedMinSelectDistance));
    }
    return selectDistance;
}

double runtimeBattleBlinkReach(
    const BattleActionSkillSeed& skill,
    bool forceRanged,
    int forcedRangedMinSelectDistance,
    const BattleMovementConfig& movementConfig,
    const BattleActionRulesConfig& actionRules,
    const BattleCastGeometry& geometry)
{
    if (skill.id < 0)
    {
        return movementConfig.tileWidth * 3.0;
    }
    if (runtimeForcedRangedMagic(skill, forceRanged))
    {
        return std::max(
            movementConfig.tileWidth * 3.0,
            static_cast<double>(std::max(1, forcedRangedMinSelectDistance))
                * movementConfig.tileWidth);
    }
    if (skill.attackAreaType == 3)
    {
        return actionRules.heavyAttackReach;
    }
    if (skill.attackAreaType == 1 || skill.attackAreaType == 2)
    {
        const double reach = geometry.projectileSpawnOffset
            + geometry.projectileBaseTravel
            + (skill.selectDistance - 1) * geometry.projectileTravelPerSelectDistance;
        return std::min(movementConfig.maxRangedReach, reach - 10.0);
    }
    return std::max(
        movementConfig.tileWidth * 3.0,
        static_cast<double>(skill.selectDistance) * movementConfig.tileWidth);
}

double runtimeEffectiveBattleReach(
    const BattleActionSkillSeed& skill,
    bool forceRanged,
    int forcedRangedMinSelectDistance,
    int projectileSpeedMultiplierPct,
    const BattleMovementConfig& movementConfig,
    double meleeAttackHitRadius,
    const BattleActionRulesConfig& actionRules,
    const BattleCastGeometry& geometry)
{
    if (skill.id < 0)
    {
        return movementConfig.tileWidth * 2.0;
    }
    if (runtimeProjectileStyleMagic(skill, forceRanged))
    {
        const int selectDistance = runtimeEffectiveProjectileSelectDistance(
            skill,
            runtimeForcedRangedMagic(skill, forceRanged),
            forcedRangedMinSelectDistance);
        const double projectileReach = geometry.projectileSpawnOffset
            + (geometry.projectileBaseTravel + (selectDistance - 1) * geometry.projectileTravelPerSelectDistance)
                * projectileSpeedMultiplierPct / 100.0;
        const double rangedAttackSafetyMargin = meleeAttackHitRadius
            - movementConfig.tileWidth / 2.0;
        return std::max(
            movementConfig.tileWidth * 2.0,
            projectileReach - rangedAttackSafetyMargin);
    }
    if (skill.attackAreaType == 3)
    {
        return actionRules.heavyAttackReach;
    }
    return movementConfig.meleeAttackReach;
}

RuntimeCastSkillProfile makeRuntimeCastSkillProfile(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed,
    bool ultimate,
    const RuntimeCastPolicies* precomputedPolicies)
{
    RuntimeCastSkillProfile profile;
    if (seed.id < 0)
    {
        return profile;
    }

    constexpr int DefaultForcedRangedMinSelectDistance = 6;
    const auto queriedPolicies = precomputedPolicies
        ? RuntimeCastPolicies{}
        : runtimeCastPoliciesForSkill(state, unit, seed.id, ultimate);
    const auto& policies = precomputedPolicies ? *precomputedPolicies : queriedPolicies;
    profile.forceRanged = policies.forceRanged.has_value();
    const int forcedRangedMinSelectDistance = policies.forceRanged
            && policies.forceRanged->minimumSelectDistance > 0
        ? std::max(1, policies.forceRanged->minimumSelectDistance)
        : DefaultForcedRangedMinSelectDistance;
    const bool forcedRangedMagic = runtimeForcedRangedMagic(seed, profile.forceRanged);
    profile.effectiveSelectDistance = runtimeEffectiveProjectileSelectDistance(
        seed,
        forcedRangedMagic,
        forcedRangedMinSelectDistance);
    profile.projectileSpeedMultiplierPct = policies.forceRanged
            && policies.forceRanged->projectileSpeedPct > 0
        ? policies.forceRanged->projectileSpeedPct
        : 100;
    profile.reach = std::min(
        runtimeEffectiveBattleReach(
            seed,
            profile.forceRanged,
            forcedRangedMinSelectDistance,
            profile.projectileSpeedMultiplierPct,
            state.movement.config,
            state.attacks.hitRadius,
            state.action.actionRules,
            state.action.castGeometry),
        state.movement.config.maxRangedReach);
    profile.rangedStyle = runtimeBattleRangedStyle(seed, profile.forceRanged);
    profile.blinkReach = runtimeBattleBlinkReach(
        seed,
        profile.forceRanged,
        forcedRangedMinSelectDistance,
        state.movement.config,
        state.action.actionRules,
        state.action.castGeometry);
    return profile;
}

BattleCastSkillState makeRuntimeCastSkillState(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed,
    bool ultimate,
    const RuntimeCastPolicies* precomputedPolicies = nullptr)
{
    BattleCastSkillState skill;
    if (seed.id < 0)
    {
        return skill;
    }

    const auto profile = makeRuntimeCastSkillProfile(
        state,
        unit,
        seed,
        ultimate,
        precomputedPolicies);
    skill.id = seed.id;
    skill.name = seed.name;
    skill.soundId = seed.soundId;
    skill.hurtType = seed.hurtType;
    skill.attackAreaType = seed.attackAreaType;
    skill.magicType = seed.magicType;
    skill.visualEffectId = seed.visualEffectId;
    skill.selectDistance = profile.effectiveSelectDistance;
    skill.projectileSpeedMultiplierPct = profile.projectileSpeedMultiplierPct;
    skill.actProperty = seed.actProperty;
    skill.magicPower = seed.magicPower;
    skill.meleeSplashCount = ultimate && seed.attackAreaType == 0 ? 1 : 0;
    skill.extraProjectileCount = 0;
    skill.reach = profile.reach;
    skill.forceRanged = profile.forceRanged;
    skill.rangedStyle = profile.rangedStyle;
    skill.blinkReach = profile.blinkReach;
    return skill;
}

void refreshPlannedCastExactRuntimePolicies(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed,
    bool ultimate,
    const BattleCastProvenance& provenance,
    BattleCastInput& input)
{
    assert(provenance.valid());
    assert(provenance.sourceUnitId == unit.id);
    assert(provenance.magicId == seed.id);
    assert(provenance.ultimate == ultimate);

    const auto policies = runtimeCastPoliciesForSkill(
        state,
        unit,
        seed.id,
        ultimate,
        input.targetUnitId,
        &provenance);
    auto refreshedSkill = makeRuntimeCastSkillState(
        state,
        unit,
        seed,
        ultimate,
        &policies);
    auto& selectedSkill = ultimate ? input.ultimateSkill : input.normalSkill;
    refreshedSkill.extraProjectileCount = selectedSkill.extraProjectileCount;
    selectedSkill = std::move(refreshedSkill);

    input.unit.dashAttackEnabled = policies.dashAttack;
    input.unit.blinkAttackEnabled = policies.blinkAttack;
    input.unit.emitDashFollowUpSkillAttack = policies.dashAttack && seed.id >= 0;
    input.unit.dashFollowUpOperationType = seed.id >= 0
        ? (runtimeForcedRangedMagic(seed, selectedSkill.forceRanged)
            ? BattleOperationType::RangedProjectile
            : BattleCombatIntentPlanner().operationTypeForAttackArea(
                selectedSkill.attackAreaType))
        : BattleOperationType::None;
}

void refreshCastTarget(BattleCastInput& input, int targetUnitId, Pointf targetPosition)
{
    input.targetUnitId = targetUnitId;
    input.targetPosition = targetPosition;
    input.targetDistance = battleDistance2d(input.unit.position, targetPosition);
    if (input.geometry.dashVelocityMagnitude > 0.0)
    {
        auto dashVelocity = targetPosition - input.unit.position;
        if (dashVelocity.norm() > 0.01)
        {
            dashVelocity.normTo(static_cast<float>(input.geometry.dashVelocityMagnitude));
        }
        input.unit.dashVelocity = dashVelocity;
    }
}

BattleCastInput refreshedCastInput(BattleRuntimeState& state,
                                   const BattleRuntimeUnitRecord& source,
                                   const BattleTickResult& movement,
                                   BattleCastInput input)
{
    input.unit.position = source.core.motion.position;
    input.unit.facing = source.core.motion.facing;
    input.unit.alive = source.alive();
    input.unit.canStartAttack = source.core.canAttack;
    input.unit.mp = source.core.vitals.mp;
    input.unit.maxMp = source.core.vitals.maxMp;
    input.unit.speed = effectAndAreaAdjustedSpeed(state, source.id(), source.core.stats.speed);
    input.unit.operationCount = source.core.operationCount;
    input.unit.frozen = source.frozen();
    if (input.targetUnitId < 0)
    {
        const auto movementDecision = movement.decisions.find(input.unit.id);
        if (movementDecision != movement.decisions.end()
            && movementDecision->second.targetId >= 0
            && state.units.requireCore(movementDecision->second.targetId).alive)
        {
            input.targetUnitId = movementDecision->second.targetId;
        }
        if (input.targetUnitId < 0)
        {
            input.targetUnitId = findNearestEnemyUnitId(state.units, input.unit.id);
        }
    }
    if (input.targetUnitId >= 0)
    {
        const auto& target = state.units.requireCore(input.targetUnitId);
        if (target.alive)
        {
            refreshCastTarget(input, target.id, target.motion.position);
        }
        else
        {
            input.targetUnitId = -1;
        }
    }

    input.projectileSpreadTargets.clear();
    input.projectileSpreadTargets.reserve(state.units.size());
    const auto& sourceUnit = source.core;
    for (const auto& candidateRecord : state.units.live())
    {
        const auto& candidate = candidateRecord.core;
        if (candidate.team == sourceUnit.team)
        {
            continue;
        }
        input.projectileSpreadTargets.push_back({
            candidate.id,
            candidate.motion.position,
        });
    }

    return input;
}

void refreshRuntimeCastSkillBonuses(
    BattleRuntimeState& state,
    BattleCastInput& input,
    const BattleCastProvenance* provenance = nullptr)
{
    if (input.ultimateSkill.id >= 0 && input.unit.mp == input.unit.maxMp)
    {
        const auto resources = CoreDetail::snapshotEffectResourcesBeforeCast(state);
        const auto matches = queryExactRuntimeRules(
            state,
            input.unit.id,
            EffectEvent::CastPlanned,
            CoreDetail::makeCastPlanEventData(input, true, resources, provenance));
        input.ultimateSkill.extraProjectileCount =
            collectRuntimeUltimateExtraProjectileCount(
                state,
                matches,
                0);
    }
}

BattleCastInput makeRuntimeCastInputFromSeed(
    BattleRuntimeState& state,
    const BattleRuntimeUnitRecord& unit,
    const BattleActionPlanSeed& seed,
    bool canStartAttack,
    bool movementDashActive,
    std::pmr::memory_resource* frameMemoryResource)
{
    BattleCastInput input(frameMemoryResource);
    input.config = state.action.castConfig;
    input.geometry = state.action.castGeometry;
    input.unit.id = unit.id();
    input.unit.position = unit.core.motion.position;
    input.unit.facing = unit.core.motion.facing;
    input.unit.alive = unit.alive();
    input.unit.canStartAttack = canStartAttack;
    input.unit.mp = unit.core.vitals.mp;
    input.unit.maxMp = unit.core.vitals.maxMp;
    input.unit.speed = effectAndAreaAdjustedSpeed(state, unit.id(), unit.core.stats.speed);
    input.unit.operationCount = unit.core.operationCount;
    input.unit.meleeAttackReach = state.movement.config.meleeAttackReach;
    input.unit.dashAttackReach = state.movement.config.meleeAttackReach
        + state.movement.config.meleeLocalTargetRadius;
    input.unit.hasEquippedSkill = seed.hasEquippedSkill;
    input.unit.movementDashActive = movementDashActive;
    input.unit.frozen = unit.frozen();
    const bool ultimateReady = unit.core.vitals.maxMp > 0
        && unit.core.vitals.mp >= unit.core.vitals.maxMp;
    const bool useUltimate = ultimateReady && seed.ultimateSkill.id >= 0;
    const auto& selectedSeed = useUltimate
        ? seed.ultimateSkill
        : seed.normalSkill;
    const auto castPolicies = runtimeCastPoliciesForSkill(
        state,
        unit.core,
        selectedSeed.id,
        useUltimate);
    input.unit.dashAttackEnabled = castPolicies.dashAttack;
    input.unit.blinkAttackEnabled = castPolicies.blinkAttack;

    input.unit.dashVelocity = unit.core.motion.facing;
    if (input.unit.dashVelocity.norm() > 0.01)
    {
        input.unit.dashVelocity.normTo(
            static_cast<float>(
                state.attacks.hitRadius / state.movement.config.dashFrames));
    }

    input.unit.cooldownReductionPct = effectAdjustedAttribute(
        state,
        unit.id(),
        BattleAttribute::CooldownReduction,
        0);
    input.normalSkill = makeRuntimeCastSkillState(
        state,
        unit.core,
        seed.normalSkill,
        false,
        useUltimate ? nullptr : &castPolicies);
    input.ultimateSkill = makeRuntimeCastSkillState(
        state,
        unit.core,
        seed.ultimateSkill,
        true,
        useUltimate ? &castPolicies : nullptr);
    const auto& selectedSkill = useUltimate
        ? input.ultimateSkill
        : input.normalSkill;
    input.unit.dashHitCount = 1;
    input.unit.emitDashFollowUpSkillAttack = input.unit.dashAttackEnabled && selectedSkill.id >= 0;
    input.unit.dashFollowUpOperationType = selectedSkill.id >= 0
        ? (runtimeForcedRangedMagic(selectedSeed, selectedSkill.forceRanged)
            ? BattleOperationType::RangedProjectile
            : BattleCombatIntentPlanner().operationTypeForAttackArea(selectedSkill.attackAreaType))
        : BattleOperationType::None;
    return input;
}

double nextRuntimeUnitRoll(BattleRuntimeState& state)
{
    return state.random.nextPercent() / 100.0;
}

int rollRuntimeDashHitCount(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleCastSkillState& selectedSkill)
{
    int dashHitCount = 1;
    if (selectedSkill.id < 0)
    {
        return dashHitCount;
    }

    const double multiHitScore = (unit.stats.speed + selectedSkill.actProperty) / 180.0;
    if (nextRuntimeUnitRoll(state) < multiHitScore)
    {
        ++dashHitCount;
    }
    if (nextRuntimeUnitRoll(state) < multiHitScore * 0.5)
    {
        ++dashHitCount;
    }
    return dashHitCount;
}

Pointf runtimeDashAttackVelocity(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill)
{
    auto direction = input.targetPosition - unit.motion.position;
    if (direction.norm() <= state.action.castConfig.minimumFacingNorm)
    {
        direction = unit.motion.facing;
    }
    assert(direction.norm() > state.action.castConfig.minimumFacingNorm);
    direction = normalizedTo(direction, 1.0, state.action.castConfig.minimumFacingNorm);

    double dashDistance = state.attacks.hitRadius
        / state.movement.config.dashFrames;

    if (selectedSkill.rangedStyle)
    {
        const double attackRange = std::min(
            selectedSkill.reach,
            state.movement.config.maxRangedReach);
        const double forwardGap = std::max(0.0, input.targetDistance - attackRange);
        dashDistance = state.attacks.hitRadius
            / state.movement.config.dashFrames;
        if (forwardGap > state.movement.config.engagementDeadband)
        {
            dashDistance = std::min(
                dashDistance,
                forwardGap / state.movement.config.dashFrames);
        }
        else
        {
            auto away = unit.motion.position - input.targetPosition;
            if (away.norm() > state.action.castConfig.minimumFacingNorm)
            {
                away = normalizedTo(away, 1.0, state.action.castConfig.minimumFacingNorm);
                Pointf side{ -away.y, away.x, 0 };
                if (state.random.nextPercent() < 50.0)
                {
                    side = scaled(side, -1.0);
                }
                side = normalizedTo(side, 1.0, state.action.castConfig.minimumFacingNorm);
                direction = side + scaled(
                    away,
                    std::clamp((attackRange - input.targetDistance) / std::max(attackRange, 1.0), 0.0, 1.0));
            }
        }
    }
    else if (selectedSkill.attackAreaType == 0)
    {
        const double usefulAdvance = input.targetDistance
            - state.movement.config.meleeAttackReach
            + state.movement.config.engagementDeadband;
        dashDistance = std::clamp(
            usefulAdvance,
            0.0,
            state.movement.config.maxDashDistance)
            / state.movement.config.dashFrames;
    }

    dashDistance *= 0.8;

    return normalizedTo(direction, dashDistance, state.action.castConfig.minimumFacingNorm);
}

Pointf committedRuntimeDashAttackVelocity(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleCastInput& input,
    const BattleCastSkillState& selectedSkill,
    const BattlePendingCastAction& pending)
{
    auto velocity = runtimeDashAttackVelocity(state, unit, input, selectedSkill);
    if (velocity.norm() > state.action.castConfig.minimumFacingNorm)
    {
        return velocity;
    }

    assert(pending.dashVelocity.norm() > state.action.castConfig.minimumFacingNorm);
    return pending.dashVelocity;
}

Pointf runtimeCastFacing(
    const BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleCastInput& input)
{
    assert(input.config.minimumFacingNorm > 0.0);
    auto facing = input.targetPosition - unit.motion.position;
    if (facing.norm() <= input.config.minimumFacingNorm)
    {
        facing = unit.motion.facing;
    }
    assert(facing.norm() > input.config.minimumFacingNorm);
    return normalizedTo(facing, 1.0, input.config.minimumFacingNorm);
}


Pointf taXueMeleeRetreatVelocity(BattleRuntimeState& state, Pointf retreatVelocity)
{
    const double retreatSpeed = retreatVelocity.norm();
    if (retreatSpeed <= state.action.castConfig.minimumFacingNorm)
    {
        return retreatVelocity;
    }

    auto backward = normalizedTo(retreatVelocity, 1.0, state.action.castConfig.minimumFacingNorm);
    Pointf side{ -backward.y, backward.x, 0 };
    if (state.random.nextPercent() < 50.0)
    {
        side = scaled(side, -1.0);
    }

    return normalizedTo(
        scaled(backward, 0.65) + scaled(side, 0.35),
        retreatSpeed,
        state.action.castConfig.minimumFacingNorm);
}

void schedulePostDashRetreat(BattleRuntimeState& state, int unitId, const BattleCastResult& cast)
{
    if (cast.postDashRetreatFrames <= 0)
    {
        return;
    }

    const auto& unit = state.units.requireCore(unitId);
    Pointf retreatVelocity = cast.postDashRetreatVelocity;
    int retreatFrames = cast.postDashRetreatFrames;
    int chaosFrames = 0;
    if (unit.style == CombatStyle::Melee)
    {
        retreatVelocity = taXueMeleeRetreatVelocity(state, retreatVelocity);
        retreatFrames += 6;
        chaosFrames = state.movement.config.dashFrames + 1;
    }

    auto& physics = state.units.require(unitId).movement.physics;
    physics.postDashRetreatVelocity = retreatVelocity;
    physics.postDashRetreatFrames = retreatFrames;
    physics.postDashChaosFrames = chaosFrames;
    physics.movementDashSpreadFrames = 0;
}

void refreshRuntimeDashAttackDetails(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    BattleCastInput& input,
    bool ultimate,
    bool dashOperation)
{
    if (!dashOperation)
    {
        return;
    }
    const auto& skill = CoreDetail::selectedCastSkill(input, ultimate);
    input.unit.dashHitCount = rollRuntimeDashHitCount(state, unit, skill);
    input.unit.dashVelocity = runtimeDashAttackVelocity(state, unit, input, skill);
}

bool actionMovementDashActive(const BattleRuntimeState& state, int unitId)
{
    const auto& movement = state.units.require(unitId).movement;
    return movement.active && movement.physics.movementDashFrames > 0;
}

double committedCastProjectileSpeed(const BattleActionCommitResult& result, double fallbackSpeed)
{
    assert(fallbackSpeed >= 0.0);
    for (const auto& request : result.attackSpawnRequests)
    {
        const double speed = request.initial.velocity.norm();
        if (speed > 0.01)
        {
            return speed;
        }
    }
    return fallbackSpeed;
}

const BattleCastSkillState& selectedCastSkill(const BattleCastInput& input, const BattleCastResult& cast)
{
    return cast.decision.ultimate ? input.ultimateSkill : input.normalSkill;
}


BattleCastSkillState materializePendingCastSkill(const BattlePendingCastAction& pending)
{
    assert(pending.effectCast.provenance.valid());
    assert(pending.skillPlan.id == -1);
    auto skill = pending.skillPlan;
    skill.id = pending.effectCast.provenance.magicId;
    return skill;
}

void populateActionCommitLiveInput(BattleRuntimeState& state,
                                   const BattleRuntimeUnit& unit,
                                   const BattleCastInput& castInput,
                                   const BattleCastSkillState& selectedSkill,
    BattleActionCommitInput& actionInput)
{
    actionInput.sourceUnitId = unit.id;
    actionInput.committedFacing = runtimeCastFacing(state, unit, castInput);

    if (castInput.unit.blinkAttackEnabled)
    {
        const double blinkReach = selectedSkill.blinkReach > 0.0 ? selectedSkill.blinkReach : selectedSkill.reach;
        actionInput.blinkGeometry = makeRuntimeBlinkGeometry(state, unit, blinkReach);
    }
}


Pointf positionForRuntimeGridCell(const BattleRuntimeState& state, int x, int y)
{
    const int coordCount = state.gridTransform.coordCount;
    const double tileWidth = state.gridTransform.tileWidth;
    assert(coordCount > 0);
    assert(tileWidth > 0.0);
    return {
        static_cast<float>(-y * tileWidth + x * tileWidth + coordCount * tileWidth),
        static_cast<float>(y * tileWidth + x * tileWidth),
        0.0f,
    };
}

bool runtimeGridCellWalkable(const BattleRuntimeState& state, int x, int y)
{
    const int coordCount = state.gridTransform.coordCount;
    if (x < 0 || y < 0 || x >= coordCount || y >= coordCount)
    {
        return false;
    }
    const auto index = static_cast<std::size_t>(x * coordCount + y);
    if (index >= state.movement.terrainCells.size())
    {
        return true;
    }
    return state.movement.terrainCells[index].walkable;
}

bool runtimeGridCellInBounds(const BattleRuntimeState& state, int x, int y)
{
    const int coordCount = state.gridTransform.coordCount;
    return x >= 0 && y >= 0 && x < coordCount && y < coordCount;
}

std::size_t runtimeGridCellIndex(int coordCount, int x, int y)
{
    assert(coordCount > 0);
    assert(x >= 0);
    assert(y >= 0);
    assert(x < coordCount);
    assert(y < coordCount);
    return static_cast<std::size_t>(x) * static_cast<std::size_t>(coordCount)
        + static_cast<std::size_t>(y);
}

BattleBlinkGeometryInput makeRuntimeBlinkGeometry(const BattleRuntimeState& state,
                                                  const BattleRuntimeUnit& source,
                                                  double reach)
{
    BattleBlinkGeometryInput geometry;
    geometry.currentGridX = source.grid.x;
    geometry.currentGridY = source.grid.y;

    const double tileWidth = state.gridTransform.tileWidth;
    const int coordCount = state.gridTransform.coordCount;
    assert(tileWidth > 0.0);
    assert(coordCount > 0);
    int gridReach = std::max(1, static_cast<int>(reach / tileWidth) + 1);
    const auto cellCount = static_cast<std::size_t>(coordCount) * static_cast<std::size_t>(coordCount);
    std::vector<unsigned char> visited(cellCount);
    std::vector<unsigned char> occupied(cellCount);
    for (const auto& otherRecord : state.units.live())
    {
        const auto& other = otherRecord.core;
        if (other.id == source.id || !runtimeGridCellInBounds(state, other.grid.x, other.grid.y))
        {
            continue;
        }
        occupied[runtimeGridCellIndex(coordCount, other.grid.x, other.grid.y)] = 1;
    }

    for (const auto& targetRecord : state.units.live())
    {
        const auto& target = targetRecord.core;
        if (target.id == source.id || target.team == source.team)
        {
            continue;
        }

        for (int dx = -gridReach; dx <= gridReach; ++dx)
        {
            for (int dy = -gridReach; dy <= gridReach; ++dy)
            {
                const int x = target.grid.x + dx;
                const int y = target.grid.y + dy;
                if (!runtimeGridCellInBounds(state, x, y))
                {
                    continue;
                }

                const auto index = runtimeGridCellIndex(coordCount, x, y);
                if (visited[index] != 0)
                {
                    continue;
                }
                visited[index] = 1;

                geometry.cells.push_back({
                    x,
                    y,
                    positionForRuntimeGridCell(state, x, y),
                    runtimeGridCellWalkable(state, x, y),
                    occupied[index] != 0,
                });
            }
        }
    }
    return geometry;
}

BattlePendingCastAction makePendingCastAction(const BattleCastInput& castInput,
                                              const BattleCastResult& cast,
                                              const BattleCastStart& trackedCast,
                                              int castFrame)
{
    assert(castFrame > 0);
    assert(trackedCast.provenance.valid());
    assert(trackedCast.provenance.sourceUnitId == cast.decision.unitId);
    assert(trackedCast.provenance.magicId == selectedCastSkill(castInput, cast).id);
    assert(trackedCast.provenance.ultimate == cast.decision.ultimate);
    BattlePendingCastAction pending;
    pending.targetUnitId = cast.decision.targetUnitId;
    pending.operationType = cast.decision.operationType;
    pending.castFrame = castFrame;
    pending.dashVelocity = castInput.unit.dashVelocity;
    pending.skillPlan = CoreDetail::makePendingCastSkillPlan(selectedCastSkill(castInput, cast));
    pending.effectCast = trackedCast;
    return pending;
}

int castCommitTargetUnitId(
    const BattleRuntimeUnits& units,
    int sourceUnitId,
    int targetUnitId)
{
    assert(sourceUnitId >= 0);
    const auto& source = units.requireCore(sourceUnitId);
    if (!source.alive)
    {
        return -1;
    }

    assert(targetUnitId >= 0);
    const auto& target = units.requireCore(targetUnitId);
    if (target.alive && target.team != source.team)
    {
        return target.id;
    }

    return findNearestEnemyUnitId(units, sourceUnitId);
}

int pendingCastCommitTargetUnitId(const BattleRuntimeUnits& units, const BattlePendingCastAction& pending)
{
    assert(pending.effectCast.provenance.valid());
    return castCommitTargetUnitId(
        units,
        pending.effectCast.provenance.sourceUnitId,
        pending.targetUnitId);
}


BattleActionCommitInput makeCommittedCastActionInput(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleCastInput& castInput,
    const BattleCastSkillState& selectedSkill,
    const BattleCastResult& cast,
    const BattleCastProvenance& provenance,
    const BattleEffectCastPreparation& effectPreparation,
    const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast)
{
    CastCommitEventData payload;
    assert(provenance.valid());
    assert(provenance.sourceUnitId == unit.id);
    assert(provenance.magicId == selectedSkill.id);
    assert(provenance.ultimate == cast.decision.ultimate);
    payload.provenance = provenance;
    payload.targetUnitId = cast.decision.targetUnitId;
    const auto resources = resourcesBeforeCast.values();
    const auto ownerResource = std::ranges::find(
        resources,
        unit.id,
        &EffectUnitResourceBeforeCast::unitId);
    payload.mpBefore = ownerResource != resources.end()
        ? ownerResource->mp
        : castInput.unit.mp;
    payload.mpPaid = std::max(0, -cast.mpDelta);
    payload.rangeMode = effectPreparation.rangeMode.value_or(CastRangeMode::Preserve);
    payload.attackPattern = cast.attackPattern;
    payload.resourcesBeforeCast = resourcesBeforeCast;
    const auto exactMatches = queryExactRuntimeRules(
        state,
        unit.id,
        EffectEvent::AttackCommitted,
        std::move(payload));

    BattleActionCommitInput actionInput;
    actionInput.hasCast = cast.decision.canCast;
    actionInput.cast = cast;
    actionInput.blinkRandomRoll = state.random.nextInt(std::numeric_limits<int>::max());
    actionInput.blinkCellRandomRoll = state.random.nextInt(std::numeric_limits<int>::max());
    actionInput.mobility = castInput.unit.blinkAttackEnabled
        ? CastMobilityPolicy::BlinkAttack
        : CastMobilityPolicy::Preserve;
    actionInput.blinkUseWeakestTarget =
        state.effectRules.blinkAttackUsesWeakestTarget(unit.id);
    actionInput.blinkReach = selectedSkill.blinkReach > 0.0 ? selectedSkill.blinkReach : selectedSkill.reach;
    actionInput.blinkWeakTargetDefWeight = state.action.actionRules.blinkWeakTargetDefWeight;
    actionInput.strengthenedMeleeOperationCountThreshold =
        state.action.castConfig.strengthenedMeleeOperationCountThreshold;
    actionInput.normalAttackActType = castInput.normalSkill.magicType;
    actionInput.delayedAlternateAttack = runtimeDelayedAlternateAttack(exactMatches);
    populateActionCommitLiveInput(state, unit, castInput, selectedSkill, actionInput);

    actionInput.projectileBouncePrime = collectRuntimeProjectileBouncePrime(
        exactMatches,
        state.random.nextInt(100),
        state.action.actionRules.projectileBounceRange);
    return actionInput;
}

std::optional<BattleActionCommitInput> tryMakeRuntimeActionCommitInput(
    BattleRuntimeState& state,
    const BattleTickResult& movement,
    const BattlePendingCastAction& pending,
    std::pmr::memory_resource* frameMemoryResource)
{
    (void)movement;

    assert(pending.effectCast.provenance.valid());
    const auto& provenance = pending.effectCast.provenance;
    const auto& unit = state.units.requireCore(provenance.sourceUnitId);
    auto castInput = CoreDetail::tryMakeRuntimeCastInputForPendingCast(
        state,
        pending,
        frameMemoryResource);
    if (!castInput)
    {
        return std::nullopt;
    }
    const int normalAttackActType = castInput->normalSkill.magicType;

    auto selectedSkill = materializePendingCastSkill(pending);
    if (provenance.ultimate)
    {
        const auto matches = queryExactRuntimeRules(
            state,
            unit.id,
            EffectEvent::CastPlanned,
            CoreDetail::makeCastPlanEventData(
                *castInput,
                true,
                pending.effectResourcesBeforeCast,
                &provenance));
        selectedSkill.extraProjectileCount =
            collectRuntimeUltimateExtraProjectileCount(
                state,
                matches,
                0);
    }
    castInput->normalSkill = selectedSkill;
    castInput->ultimateSkill = selectedSkill;

    if (pending.operationType == BattleOperationType::Dash)
    {
        castInput->unit.dashHitCount = rollRuntimeDashHitCount(state, unit, selectedSkill);
        castInput->unit.dashVelocity = committedRuntimeDashAttackVelocity(
            state,
            unit,
            *castInput,
            selectedSkill,
            pending);
    }

    auto cast = BattleCastPlanner().commitSelectedCast(
        *castInput,
        selectedSkill,
        provenance.ultimate,
        pending.operationType);
    BattleEffectAttackApplyState effectAttackState{
        .nextSharedHitGroupId = state.effectIntegration.nextSharedHitGroupId,
    };
    auto preparedEffects = BattleEffectAttackCastSystem().applyPreparedCast(
        *castInput,
        cast,
        pending.effectPreparation,
        effectAttackState);
    CoreDetail::applyEffectAttackDirectives(cast.attackSpawnRequests, preparedEffects);
    auto plannedAttackEffects = BattleEffectAttackCastSystem().applyAttackCommands(
        *castInput,
        cast,
        pending.plannedAttackEffectCommands,
        effectAttackState);
    CoreDetail::applyEffectAttackDirectives(cast.attackSpawnRequests, plannedAttackEffects);
    state.effectIntegration.nextSharedHitGroupId = effectAttackState.nextSharedHitGroupId;
    auto actionInput = makeCommittedCastActionInput(
        state,
        unit,
        *castInput,
        selectedSkill,
        cast,
        provenance,
        pending.effectPreparation,
        pending.effectResourcesBeforeCast);
    actionInput.normalAttackActType = normalAttackActType;
    return actionInput;
}

void applyBlinkTeleportToRuntimeUnit(BattleRuntimeState& state, const BattleBlinkTeleportDelta& teleport)
{
    state.units.setPosition(teleport.unitId, teleport.position, state.gridTransform);
    auto& record = state.units.require(teleport.unitId);
    auto& unit = record.core;
    unit.grid = { teleport.gridX, teleport.gridY };
    unit.motion.velocity = {};
    unit.motion.acceleration = {};
    unit.motion.facing = teleport.facing;
    record.movement.physics.position = teleport.position;
    record.movement.physics.velocity = {};
    record.movement.physics.acceleration = {};
    state.movement.movementReservations.erase(teleport.unitId);
}

void commitActionFrameStateToRuntime(BattleRuntimeUnit& unit, const BattleActionFrameState& state)
{
    unit.animation.cooldown = state.cooldown;
    unit.animation.cooldownMax = state.cooldownMax;
    unit.animation.actFrame = state.actFrame;
    unit.animation.actType = state.actType;
    unit.operationType = state.operationType;
    unit.haveAction = state.haveAction;
}

BattleActionFrameState makeActionRuntimeState(const BattleRuntimeUnit& unit)
{
    BattleActionFrameState state;
    state.cooldown = unit.animation.cooldown;
    state.cooldownMax = unit.animation.cooldownMax;
    state.actFrame = unit.animation.actFrame;
    state.actType = unit.animation.actType;
    state.operationType = unit.operationType;
    state.haveAction = unit.haveAction;
    return state;
}

void resetActionFrameState(BattleActionFrameState& state)
{
    state.cooldown = 0;
    state.cooldownMax = 0;
    state.actFrame = 0;
    state.actType = -1;
    state.operationType = BattleOperationType::None;
    state.haveAction = false;
}

void cancelRuntimeAction(BattleRuntimeState& state, int unitId)
{
    auto& unit = state.units.require(unitId);
    if (const auto* pending = unit.pendingCast())
    {
        CoreDetail::cancelEffectRootCast(state, pending->effectCast);
    }
    auto actionState = makeActionRuntimeState(unit.core);
    resetActionFrameState(actionState);
    commitActionFrameStateToRuntime(unit.core, actionState);
    unit.clearActionOwners();
}


int actionCastFrame(const BattleRuntimeState& state, BattleOperationType operationType)
{
    if (!isBattleOperation(operationType))
    {
        return 0;
    }
    const int operationIndex = battleOperationIndex(operationType);
    assert(static_cast<std::size_t>(operationIndex) < state.action.castConfig.castFrames.size());
    return state.action.castConfig.castFrames[operationIndex];
}

int jitteredActionCastFrame(BattleRuntimeState& state, BattleOperationType operationType)
{
    const int baseCastFrame = actionCastFrame(state, operationType);
    assert(baseCastFrame > ActionCastFrameJitterRadius);
    return baseCastFrame
        + state.random.nextInt(ActionCastFrameJitterChoices)
        - ActionCastFrameJitterRadius;
}

int actionRecoveryFrames(const BattleRuntimeState& state, BattleOperationType operationType)
{
    const int operationIndex = battleOperationIndex(operationType);
    assert(static_cast<std::size_t>(operationIndex)
           < state.action.castConfig.recoveryFrames.size());
    return state.action.castConfig.recoveryFrames[operationIndex];
}

BattleOperationType copiedAttackOperationType(const BattleCastSkillState& skill)
{
    if (skill.forceRanged
        && (skill.attackAreaType == 0 || skill.attackAreaType == 3))
    {
        return BattleOperationType::RangedProjectile;
    }
    return BattleCombatIntentPlanner().operationTypeForAttackArea(
        skill.attackAreaType);
}

void queueCopiedAttackDefinitionChildCast(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleCastProvenance& parent,
    int parentTargetUnitId,
    const BattleCopiedAttackDefinitionRequest& request,
    std::pmr::memory_resource* frameMemoryResource)
{
    assert(parent.valid());
    assert(parent.sourceUnitId >= 0);
    assert(request.definitionOwnerUnitId >= 0);
    assert(request.definitionOwnerUnitId != parent.sourceUnitId);
    assert(request.propagation == CastPropagationPolicy::SuppressUltimateRules);

    const auto& definitionOwner = state.units.require(
        request.definitionOwnerUnitId);
    const auto* definitionPlan = definitionOwner.actionPlan();
    if (!definitionOwner.alive()
        || !definitionPlan
        || definitionPlan->ultimateSkill.id < 0)
    {
        return;
    }

    const auto& copier = state.units.require(parent.sourceUnitId);
    const auto copiedSkill = makeRuntimeCastSkillState(
        state,
        copier.core,
        definitionPlan->ultimateSkill,
        true);
    const auto operationType = copiedAttackOperationType(copiedSkill);
    if (operationType == BattleOperationType::None)
    {
        return;
    }

    if (castCommitTargetUnitId(
            state.units,
            parent.sourceUnitId,
            parentTargetUnitId) < 0)
    {
        return;
    }
    const auto child = state.castLifecycle.beginChildCast(
        parent.castId,
        {
            .sourceUnitId = parent.sourceUnitId,
            .magicId = copiedSkill.id,
            .ultimate = true,
            .origin = CastOriginKind::CopiedAttack,
            .propagation = request.propagation,
        });
    BattlePendingCastAction pending;
    pending.targetUnitId = parentTargetUnitId;
    pending.operationType = operationType;
    pending.skillPlan = CoreDetail::makePendingCastSkillPlan(copiedSkill);
    pending.effectResourcesBeforeCast = CoreDetail::snapshotEffectResourcesBeforeCast(state);
    pending.effectCast = child;
    auto input = CoreDetail::tryMakeRuntimeCastInputForPendingCast(
        state,
        pending,
        frameMemoryResource);
    assert(input);
    refreshRuntimeDashAttackDetails(
        state,
        copier.core,
        *input,
        true,
        operationType == BattleOperationType::Dash);

    auto cast = BattleCastPlanner().commitSelectedCast(
        *input,
        copiedSkill,
        true,
        operationType);
    cast.mpDelta = 0;
    auto committedEffects = CoreDetail::dispatchCastCommittedEffects(
        state,
        pending,
        cast);
    BattleEffectAttackApplyState effectAttackState{
        .nextSharedHitGroupId = state.effectIntegration.nextSharedHitGroupId,
    };
    for (const auto& committedEffect : committedEffects)
    {
        const auto applied = BattleEffectAttackCastSystem().applyAttackCommands(
            *input,
            cast.attackSpawnRequests,
            committedEffect.commands,
            effectAttackState);
        CoreDetail::applyEffectAttackDirectives(cast.attackSpawnRequests, applied);
    }
    state.effectIntegration.nextSharedHitGroupId =
        effectAttackState.nextSharedHitGroupId;

    for (auto& attack : cast.attackSpawnRequests)
    {
        attack.provenance.propagation = request.propagation;
        attack.provenance.origin = BattleAttackOriginKind::CastDerived;
    }
    CoreDetail::reserveEffectRootCastAttacks(
        state.castLifecycle,
        child,
        cast.attackSpawnRequests);
    state.effectIntegration.casts[child.provenance.castId] = {
        .originalTargetUnitId = cast.decision.targetUnitId,
        .resourcesBeforeCast = pending.effectResourcesBeforeCast,
        .skill = copiedSkill,
        .operationType = operationType,
    };
    const BattleEffectCommandContext context{
        .frame = state.movement.frame,
        .cast = child.provenance,
        .areaTargetTeamDomain = copier.core.team,
    };
    for (auto& committedEffect : committedEffects)
    {
        frame.queueEffectCommands(
            std::move(committedEffect.commands),
            context);
    }
    frame.queueCastCommitBarrier(child.commitBarrier);
    for (auto& attack : cast.attackSpawnRequests)
    {
        state.nextFrame.queueAttack(std::move(attack));
    }

    frame.gameplayEvents.insert(
        frame.gameplayEvents.end(),
        cast.gameplayEvents.begin(),
        cast.gameplayEvents.end());
    frame.logEvents.insert(
        frame.logEvents.end(),
        cast.logEvents.begin(),
        cast.logEvents.end());
    frame.visualEvents.insert(
        frame.visualEvents.end(),
        cast.visualEvents.begin(),
        cast.visualEvents.end());
    if (copiedSkill.soundId >= 0)
    {
        frame.attackSoundIds.push_back(copiedSkill.soundId);
    }
}

void queueCopiedAttackDefinitionChildCasts(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleCastProvenance& parent,
    int parentTargetUnitId,
    std::span<const BattleCopiedAttackDefinitionRequest> requests,
    std::pmr::memory_resource* frameMemoryResource)
{
    for (const auto& request : requests)
    {
        queueCopiedAttackDefinitionChildCast(
            state,
            frame,
            parent,
            parentTargetUnitId,
            request,
            frameMemoryResource);
    }
}

void applySpiralBleedCastEffect(
    BattleRuntimeState& state,
    int sourceUnitId,
    const BattleCastSkillState& skill,
    const EffectCommandMetadata& producerMetadata,
    int bleedStacks,
    int projectileCount,
    double projectileSpeed,
    std::vector<BattleAttackSpawnRequest>& attackSpawns)
{
    const auto& source = state.units.requireCore(sourceUnitId);
    const auto sourcePosition = source.motion.position;
    const int sharedHitGroupId = state.effectIntegration.nextSharedHitGroupId++;
    const int count = std::max(1, projectileCount);
    const double speed = projectileSpeed > 0.0
        ? projectileSpeed
        : state.projectileFollowUps.projectileSpeed;
    for (int i = 0; i < count; ++i)
    {
        BattleAttackSpawnRequest request{ BattleAttackPayload(
            BattleAttackDelivery::projectile(),
            BattleProjectilePayloadClass::scriptedControl(),
            BattleAttackReflectionLineageKind::Ordinary) };
        request.initial.attackSourceUnitId = sourceUnitId;
        request.initial.skillId = skill.id;
        request.initial.skillName = skill.name;
        request.initial.skillHurtType = skill.hurtType;
        request.initial.skillMagicType = skill.magicType;
        request.initial.skillAttackerActProperty = skill.actProperty;
        request.initial.skillMagicPower = skill.magicPower;
        request.initial.operationType = BattleOperationType::RangedProjectile;
        request.initial.visualEffectId = 48;
        request.provenance.rootAttack = false;
        request.provenance.mainProjectile = false;
        request.provenance.sharedHitGroupId = sharedHitGroupId;
        request.provenance.origin = BattleAttackOriginKind::CastDerived;
        request.provenance.propagation = CastPropagationPolicy::SourceHitRulesOnly;
        request.initial.position = sourcePosition;
        request.initial.totalFrame = 35;
        request.initial.scriptedBleedStacks = bleedStacks;
        request.initial.scriptedBleedProducer =
            BattleEffectCommandSystem::statusProducerProvenance(producerMetadata);
        request.initial.ignoreProjectileCancel = true;
        request.initial.through = true;
        request.spiralMotion = true;
        request.spiralCenter = sourcePosition;
        request.spiralRadius = 0.0f;
        request.spiralRadiusGrowth = static_cast<float>(speed * 0.9);
        request.spiralAngle = static_cast<float>(2.0 * BattlePi * i / count);
        request.spiralAngularVelocity = 0.42f;
        attackSpawns.push_back(std::move(request));
    }
}

void appendRuntimeSpiralBleedCastEffects(
    BattleRuntimeState& state,
    int sourceUnitId,
    const BattleCastSkillState& skill,
    double projectileSpeed,
    std::span<const BattleEffectDispatchResult> committedEffects,
    std::vector<BattleAttackSpawnRequest>& attackSpawns)
{
    for (const auto& dispatched : committedEffects)
    {
        for (const auto& command : dispatched.commands)
        {
            const auto* modifyAttack = std::get_if<ModifyAttackEffectCommand>(
                &command.value);
            if (!modifyAttack)
            {
                continue;
            }
            const auto* spiral = std::get_if<ExpandingSpiralAttackBehavior>(
                &modifyAttack->action.runtimeBehavior);
            if (!spiral)
            {
                continue;
            }
            assert(command.metadata.binding.ownerUnitId == sourceUnitId);
            applySpiralBleedCastEffect(
                state,
                sourceUnitId,
                skill,
                command.metadata,
                spiral->bleedStacks,
                spiral->projectileCount,
                projectileSpeed,
                attackSpawns);
        }
    }
}

}  // namespace

namespace CoreDetail
{


void advanceActionFrameUnits(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleTickResult& movement)
{
    auto& gameplayEvents = frame.gameplayEvents;
    auto& logEvents = frame.logEvents;
    auto& visualEvents = frame.visualEvents;

    for (auto& unitRecord : state.units.all())
    {
        auto& unit = unitRecord.core;
        assert(unit.id >= 0);
        if (!unit.alive)
        {
            cancelRuntimeAction(state, unit.id);
            continue;
        }

        std::optional<BattleCastInput> runtimeCastPlan;
        const auto* runtimePlanSeed = unitRecord.actionPlan();
        if (runtimePlanSeed
            && !unit.haveAction
            && !actionMovementDashActive(state, unit.id)
            && findNearestEnemyUnitId(state.units, unit.id) >= 0)
        {
            runtimeCastPlan = makeRuntimeCastInputFromSeed(
                state,
                unitRecord,
                *runtimePlanSeed,
                unit.animation.cooldown == 0,
                false,
                frame.frameMemoryResource());
        }
        auto* pendingCast = unitRecord.pendingCast();
        bool actionCommitted = false;
        BattleActionCommitInput actionInput;
        BattleActionCommitResult actionResult;
        auto actionState = makeActionRuntimeState(unit);
        const bool wasActionActive = actionState.haveAction;
        bool cancelledAction = false;

        if (!actionState.haveAction
            && runtimeCastPlan
            && unit.canAttack
            && unit.animation.cooldown == 0
            && !unitRecord.frozen())
        {
            auto castInput = refreshedCastInput(state, unitRecord, movement, std::move(*runtimeCastPlan));
            castInput.unit.canStartAttack = castInput.unit.canStartAttack
                && unit.animation.cooldown == 0;
            auto effectResourcesBeforeCast = snapshotEffectResourcesBeforeCast(state);
            const bool plannedUltimate = castInput.ultimateSkill.id >= 0
                && castInput.unit.mp == castInput.unit.maxMp;
            assert(runtimePlanSeed);
            const auto& plannedSkillSeed = plannedUltimate
                ? runtimePlanSeed->ultimateSkill
                : runtimePlanSeed->normalSkill;
            const auto trackedCast = beginEffectRootCast(
                state.castLifecycle,
                unit.id,
                plannedSkillSeed.id,
                plannedUltimate);
            auto plannedEffects = dispatchCastPlannedEffects(
                state,
                castInput,
                plannedUltimate,
                effectResourcesBeforeCast,
                trackedCast.provenance);
            refreshPlannedCastExactRuntimePolicies(
                state,
                unit,
                plannedSkillSeed,
                plannedUltimate,
                trackedCast.provenance,
                castInput);
            auto effectPreparation = BattleEffectAttackCastSystem().prepareCast(
                castInput,
                plannedUltimate,
                plannedEffects.commands);
            auto cast = BattleCastPlanner().plan(castInput);
            gameplayEvents.insert(gameplayEvents.end(), cast.gameplayEvents.begin(), cast.gameplayEvents.end());
            logEvents.insert(logEvents.end(), cast.logEvents.begin(), cast.logEvents.end());
            visualEvents.insert(visualEvents.end(), cast.visualEvents.begin(), cast.visualEvents.end());
            if (cast.decision.canCast)
            {
                unit.motion.facing = runtimeCastFacing(state, unit, castInput);
                actionState.haveAction = true;
                actionState.actFrame = 0;
                actionState.actType = cast.decision.ultimate
                    ? castInput.ultimateSkill.magicType
                    : castInput.normalSkill.magicType;
                actionState.operationType = cast.decision.operationType;
                actionState.cooldown = cast.animation.cooldownFrames;
                actionState.cooldownMax = cast.animation.cooldownFrames;
                const int castFrame = jitteredActionCastFrame(state, cast.decision.operationType);
                auto pending = makePendingCastAction(
                    castInput,
                    cast,
                    trackedCast,
                    castFrame);
                pending.effectPreparation = std::move(effectPreparation);
                pending.plannedAttackEffectCommands = std::move(plannedEffects.commands);
                pending.effectResourcesBeforeCast = std::move(effectResourcesBeforeCast);
                unitRecord.setPendingCast(std::move(pending));
                unitRecord.setSkillCooldownUltimate(cast.decision.ultimate);
                if (cast.decision.ultimate)
                {
                    unitRecord.markUltimateCaster();
                }
                unitRecord.movement.physics.movementDashSpreadFrames = 0;
            }
            else
            {
                cancelEffectRootCast(state, trackedCast);
            }
        }
        else if (actionState.haveAction && pendingCast)
        {
            assert(pendingCast->castFrame > 0);
            const int castFrame = pendingCast->castFrame;
            if (actionState.actFrame == castFrame)
            {
                actionCommitted = true;
                auto committedPending = unitRecord.takePendingCast();
                auto maybeActionInput = tryMakeRuntimeActionCommitInput(
                    state,
                    movement,
                    committedPending,
                    frame.frameMemoryResource());
                if (maybeActionInput && maybeActionInput->hasCast)
                {
                    actionInput = std::move(*maybeActionInput);
                    unit.motion.facing = actionInput.committedFacing;
                    actionResult = BattleActionCommitSystem().commit(actionInput, state.units);
                    if (actionResult.advanceBlinkTargetMode)
                    {
                        state.effectRules.advanceBlinkAttackTargetMode(unit.id);
                    }
                }
                else
                {
                    cancelEffectRootCast(state, committedPending.effectCast);
                    actionResult.operationCount = unit.operationCount;
                    resetActionFrameState(actionState);
                    cancelledAction = true;
                }
                if (actionInput.hasCast)
                {
                    schedulePostDashRetreat(state, unit.id, actionInput.cast);
                }
                if (actionInput.hasCast && actionInput.cast.decision.soundId >= 0)
                {
                    frame.attackSoundIds.push_back(actionInput.cast.decision.soundId);
                }
                if (actionInput.hasCast)
                {
                    const auto& trackedCast = committedPending.effectCast;
                    const auto committedSkill = materializePendingCastSkill(
                        committedPending);
                    auto committedEffects = dispatchCastCommittedEffects(
                        state,
                        committedPending,
                        actionInput.cast);
                    const auto copiedAttackRequests = collectCopiedAttackDefinitionRequests(
                        committedEffects);

                    auto effectCastInput = tryMakeRuntimeCastInputForPendingCast(
                        state,
                        committedPending,
                        frame.frameMemoryResource());
                    assert(effectCastInput);
                    BattleEffectAttackCastSystem().prepareCast(
                        *effectCastInput,
                        committedPending.plannedAttackEffectCommands);
                    BattleEffectAttackApplyState effectAttackState{
                        .nextSharedHitGroupId = state.effectIntegration.nextSharedHitGroupId,
                    };
                    for (const auto& committedEffect : committedEffects)
                    {
                        auto committedAttackEffects = BattleEffectAttackCastSystem().applyAttackCommands(
                            *effectCastInput,
                            actionResult.attackSpawnRequests,
                            committedEffect.commands,
                            effectAttackState);
                        applyEffectAttackDirectives(
                            actionResult.attackSpawnRequests,
                            committedAttackEffects);
                    }
                    state.effectIntegration.nextSharedHitGroupId =
                        effectAttackState.nextSharedHitGroupId;
                    appendRuntimeSpiralBleedCastEffects(
                        state,
                        unit.id,
                        committedSkill,
                        committedCastProjectileSpeed(
                            actionResult,
                            state.projectileFollowUps.projectileSpeed),
                        committedEffects,
                        actionResult.attackSpawnRequests);

                    reserveEffectRootCastAttacks(
                        state.castLifecycle,
                        trackedCast,
                        actionResult.attackSpawnRequests);
                    state.effectIntegration.casts[trackedCast.provenance.castId] = {
                        .originalTargetUnitId = actionInput.cast.decision.targetUnitId,
                        .resourcesBeforeCast = committedPending.effectResourcesBeforeCast,
                        .skill = committedSkill,
                        .operationType = actionInput.cast.decision.operationType,
                    };
                    const BattleEffectCommandContext committedEffectContext{
                        .frame = state.movement.frame,
                        .cast = trackedCast.provenance,
                        .areaTargetTeamDomain = unit.team,
                    };
                    for (auto& committedEffect : committedEffects)
                    {
                        frame.queueEffectCommands(
                            std::move(committedEffect.commands),
                            committedEffectContext);
                    }
                    queueCopiedAttackDefinitionChildCasts(
                        state,
                        frame,
                        trackedCast.provenance,
                        actionInput.cast.decision.targetUnitId,
                        copiedAttackRequests,
                        frame.frameMemoryResource());
                    frame.queueCastCommitBarrier(trackedCast.commitBarrier);
                }
                appendAttackSpawnRequests(frame.currentFrameAttacks(), actionResult.attackSpawnRequests);
                for (const auto& teleport : actionResult.blinkTeleports)
                {
                    applyBlinkTeleportToRuntimeUnit(state, teleport);
                }
                logEvents.insert(
                    logEvents.end(),
                    actionResult.logEvents.begin(),
                    actionResult.logEvents.end());
                visualEvents.insert(
                    visualEvents.end(),
                    actionResult.visualEvents.begin(),
                    actionResult.visualEvents.end());
                frame.blinkSoundCount += static_cast<int>(actionResult.blinkTeleports.size());
            }
        }

        if (wasActionActive && !cancelledAction)
        {
            ++actionState.actFrame;
            const int castFrame = actionCastFrame(state, actionState.operationType);
            if (actionState.cooldown > 0
                && actionState.actType >= 0
                && actionState.operationType != BattleOperationType::None
                && actionState.actFrame > castFrame + actionRecoveryFrames(state, actionState.operationType))
            {
                actionState.haveAction = false;
                actionState.operationType = BattleOperationType::None;
                actionState.actType = -1;
                unitRecord.clearActionOwners();
            }
        }

        if (actionCommitted)
        {
            unit.operationCount = actionResult.operationCount;
            if (actionInput.hasCast)
            {
                applyRuntimeUnitMpDelta(state, unit, actionInput.cast.mpDelta);
                unitRecord.clearUltimateCaster();
            }
        }
        commitActionFrameStateToRuntime(unit, actionState);
    }
}

CastPlanEventData makeCastPlanEventData(
    int sourceUnitId,
    int magicId,
    bool ultimate,
    int preferredTargetUnitId,
    int mpBefore,
    int maxMp,
    int normalCastMpDelta,
    bool forceRanged,
    const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast,
    const BattleCastProvenance* provenance)
{
    CastPlanEventData payload;
    payload.provenance = provenance
        ? *provenance
        : plannedEffectCastProvenance(sourceUnitId, magicId, ultimate);
    payload.preferredTargetUnitId = preferredTargetUnitId;
    payload.mpBefore = mpBefore;
    payload.baseMpCost = ultimate
        ? maxMp
        : std::max(0, -normalCastMpDelta);
    payload.baseRangeMode = forceRanged
        ? CastRangeMode::Ranged
        : CastRangeMode::Preserve;
    payload.resourcesBeforeCast = resourcesBeforeCast;
    return payload;
}

bool tryCommitAutoUltimate(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    int unitId,
    bool consumeMp,
    bool announceAutoUltimate,
    std::pmr::memory_resource* frameMemoryResource,
    std::vector<int>& attackSoundIds,
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    auto& unitRecord = state.units.require(unitId);
    auto& unit = unitRecord.core;
    if (!unit.alive)
    {
        return true;
    }

    const auto* seed = unitRecord.actionPlan();
    if (!seed || seed->ultimateSkill.id < 0)
    {
        return true;
    }

    auto castInput = refreshedCastInput(
        state,
        unitRecord,
        BattleTickResult{},
        makeRuntimeCastInputFromSeed(
            state,
            unitRecord,
            *seed,
            true,
            actionMovementDashActive(state, unitId),
            frameMemoryResource));
    if (castInput.targetUnitId < 0)
    {
        return true;
    }
    castInput.unit.canStartAttack = true;
    const auto trackedCast = beginEffectRootCast(
        state.castLifecycle,
        unitId,
        seed->ultimateSkill.id,
        true);

    auto effectResourcesBeforeCast = snapshotEffectResourcesBeforeCast(state);
    auto plannedEffects = dispatchCastPlannedEffects(
        state,
        castInput,
        true,
        effectResourcesBeforeCast,
        trackedCast.provenance);
    refreshPlannedCastExactRuntimePolicies(
        state,
        unit,
        seed->ultimateSkill,
        true,
        trackedCast.provenance,
        castInput);
    refreshRuntimeCastSkillBonuses(
        state,
        castInput,
        &trackedCast.provenance);
    auto effectPreparation = BattleEffectAttackCastSystem().prepareCast(
        castInput,
        true,
        plannedEffects.commands);
    const auto operationType = BattleCombatIntentPlanner().operationTypeForAttackArea(
        castInput.ultimateSkill.attackAreaType);
    if (operationType == BattleOperationType::None)
    {
        cancelEffectRootCast(state, trackedCast);
        return true;
    }

    refreshRuntimeDashAttackDetails(
        state,
        unit,
        castInput,
        true,
        operationType == BattleOperationType::Dash);
    auto cast = BattleCastPlanner().commitSelectedCast(castInput, true, operationType);
    if (!cast.decision.canCast)
    {
        cancelEffectRootCast(state, trackedCast);
        return true;
    }
    BattleEffectAttackApplyState effectAttackState{
        .nextSharedHitGroupId = state.effectIntegration.nextSharedHitGroupId,
    };
    auto preparedAttackEffects = BattleEffectAttackCastSystem().applyPreparedCast(
        castInput,
        cast,
        effectPreparation,
        effectAttackState);
    applyEffectAttackDirectives(cast.attackSpawnRequests, preparedAttackEffects);
    auto plannedAttackEffects = BattleEffectAttackCastSystem().applyAttackCommands(
        castInput,
        cast,
        plannedEffects.commands,
        effectAttackState);
    applyEffectAttackDirectives(cast.attackSpawnRequests, plannedAttackEffects);
    state.effectIntegration.nextSharedHitGroupId = effectAttackState.nextSharedHitGroupId;
    gameplayEvents.insert(gameplayEvents.end(), cast.gameplayEvents.begin(), cast.gameplayEvents.end());
    logEvents.insert(logEvents.end(), cast.logEvents.begin(), cast.logEvents.end());
    visualEvents.insert(visualEvents.end(), cast.visualEvents.begin(), cast.visualEvents.end());
    if (castInput.ultimateSkill.soundId >= 0)
    {
        attackSoundIds.push_back(castInput.ultimateSkill.soundId);
    }
    if (announceAutoUltimate)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = unitId;
        log.targetUnitId = unitId;
        log.segments = battleLogText(std::format("自動絕招·{}", castInput.ultimateSkill.name), BattleLogTextTone::SkillName);
        logEvents.push_back(std::move(log));
    }

    auto actionInput = makeCommittedCastActionInput(
        state,
        unit,
        castInput,
        selectedCastSkill(castInput, true),
        cast,
        trackedCast.provenance,
        effectPreparation,
        effectResourcesBeforeCast);
    if (!actionInput.hasCast)
    {
        cancelEffectRootCast(state, trackedCast);
        return true;
    }
    auto actionResult = BattleActionCommitSystem().commit(actionInput, state.units);
    if (actionResult.advanceBlinkTargetMode)
    {
        state.effectRules.advanceBlinkAttackTargetMode(unitId);
    }
    BattlePendingCastAction effectPending;
    effectPending.targetUnitId = actionInput.cast.decision.targetUnitId;
    effectPending.operationType = operationType;
    effectPending.skillPlan = makePendingCastSkillPlan(castInput.ultimateSkill);
    effectPending.effectCast = trackedCast;
    effectPending.effectPreparation = std::move(effectPreparation);
    effectPending.plannedAttackEffectCommands = std::move(plannedEffects.commands);
    effectPending.effectResourcesBeforeCast = std::move(effectResourcesBeforeCast);
    auto committedCast = actionInput.cast;
    if (!consumeMp)
    {
        committedCast.mpDelta = 0;
    }
    auto committedEffects = dispatchCastCommittedEffects(
        state,
        effectPending,
        committedCast);
    const auto copiedAttackRequests = collectCopiedAttackDefinitionRequests(
        committedEffects);
    for (const auto& committedEffect : committedEffects)
    {
        auto committedAttackEffects = BattleEffectAttackCastSystem().applyAttackCommands(
            castInput,
            actionResult.attackSpawnRequests,
            committedEffect.commands,
            effectAttackState);
        applyEffectAttackDirectives(
            actionResult.attackSpawnRequests,
            committedAttackEffects);
    }
    state.effectIntegration.nextSharedHitGroupId =
        effectAttackState.nextSharedHitGroupId;
    appendRuntimeSpiralBleedCastEffects(
        state,
        unitId,
        castInput.ultimateSkill,
        committedCastProjectileSpeed(
            actionResult,
            state.projectileFollowUps.projectileSpeed),
        committedEffects,
        actionResult.attackSpawnRequests);
    reserveEffectRootCastAttacks(
        state.castLifecycle,
        trackedCast,
        actionResult.attackSpawnRequests);
    state.effectIntegration.casts[trackedCast.provenance.castId] = {
        .originalTargetUnitId = actionInput.cast.decision.targetUnitId,
        .resourcesBeforeCast = effectPending.effectResourcesBeforeCast,
        .skill = castInput.ultimateSkill,
        .operationType = operationType,
    };
    const BattleEffectCommandContext committedEffectContext{
        .frame = state.movement.frame,
        .cast = trackedCast.provenance,
        .areaTargetTeamDomain = unit.team,
    };
    for (auto& committedEffect : committedEffects)
    {
        frame.queueEffectCommands(
            std::move(committedEffect.commands),
            committedEffectContext);
    }
    queueCopiedAttackDefinitionChildCasts(
        state,
        frame,
        trackedCast.provenance,
        actionInput.cast.decision.targetUnitId,
        copiedAttackRequests,
        frameMemoryResource);
    frame.queueCastCommitBarrier(trackedCast.commitBarrier);
    appendAttackSpawnRequests(attackSpawns, actionResult.attackSpawnRequests);
    logEvents.insert(
        logEvents.end(),
        actionResult.logEvents.begin(),
        actionResult.logEvents.end());
    visualEvents.insert(
        visualEvents.end(),
        actionResult.visualEvents.begin(),
        actionResult.visualEvents.end());
    unit.operationCount = actionResult.operationCount;
    if (consumeMp)
    {
        applyRuntimeUnitMpDelta(state, unit, actionInput.cast.mpDelta);
    }
    return true;
}

void refreshRuntimeMovementProfiles(BattleRuntimeState& state)
{
    for (auto& record : state.units.live())
    {
        auto& runtimeUnit = record.core;
        auto movementUnit = makeBattleMovementPlanUnit(runtimeUnit, BattleRuntimeMoveSpeedDivisor);
        refreshMovementSkillProfile(movementUnit, runtimeUnit, state);
        runtimeUnit.reach = movementUnit.reach;
        runtimeUnit.style = movementUnit.style;
    }
}

BattleCastSkillState makePendingCastSkillPlan(BattleCastSkillState skill)
{
    skill.id = -1;
    return skill;
}

std::optional<BattleCastInput> tryMakeRuntimeCastInputForPendingCast(
    BattleRuntimeState& state,
    const BattlePendingCastAction& pending,
    std::pmr::memory_resource* frameMemoryResource)
{
    const auto& provenance = pending.effectCast.provenance;
    assert(provenance.valid());
    const auto& unit = state.units.requireCore(provenance.sourceUnitId);
    const auto skill = materializePendingCastSkill(pending);
    const int targetUnitId = pendingCastCommitTargetUnitId(state.units, pending);
    if (targetUnitId < 0)
    {
        return std::nullopt;
    }
    const auto& target = state.units.requireCore(targetUnitId);

    BattleCastInput input(frameMemoryResource);
    input.config = state.action.castConfig;
    input.geometry = state.action.castGeometry;
    input.unit.id = unit.id;
    input.unit.position = unit.motion.position;
    input.unit.facing = unit.motion.facing;
    input.unit.alive = unit.alive;
    input.unit.canStartAttack = true;
    input.unit.mp = unit.vitals.mp;
    input.unit.maxMp = unit.vitals.maxMp;
    input.unit.speed = effectAndAreaAdjustedSpeed(state, unit.id, unit.stats.speed);
    input.unit.operationCount = unit.operationCount;
    input.unit.meleeAttackReach = state.movement.config.meleeAttackReach;
    input.unit.dashAttackReach = state.movement.config.meleeAttackReach
        + state.movement.config.meleeLocalTargetRadius;
    input.unit.hasEquippedSkill = true;
    input.unit.movementDashActive = actionMovementDashActive(state, unit.id);
    input.unit.cooldownReductionPct = effectAdjustedAttribute(
        state,
        unit.id,
        BattleAttribute::CooldownReduction,
        0);
    const auto castPolicies = runtimeCastPoliciesForSkill(
        state,
        unit,
        provenance.magicId,
        provenance.ultimate,
        targetUnitId,
        &provenance);
    input.unit.dashAttackEnabled = castPolicies.dashAttack;
    input.unit.blinkAttackEnabled = castPolicies.blinkAttack;
    input.unit.dashVelocity = unit.motion.facing;
    if (input.unit.dashVelocity.norm() > 0.01)
    {
        assert(state.movement.config.dashFrames > 0);
        input.unit.dashVelocity.normTo(
            static_cast<float>(
                state.attacks.hitRadius
                / state.movement.config.dashFrames));
    }
    input.unit.emitDashFollowUpSkillAttack = input.unit.dashAttackEnabled && provenance.magicId >= 0;
    input.unit.dashFollowUpOperationType = provenance.magicId >= 0
        ? BattleCombatIntentPlanner().operationTypeForAttackArea(skill.attackAreaType)
        : BattleOperationType::None;
    input.normalSkill = skill;
    input.ultimateSkill = skill;

    input.targetUnitId = target.id;
    refreshCastTarget(input, target.id, target.motion.position);

    const auto& sourceUnit = state.units.requireCore(input.unit.id);
    for (const auto& candidateRecord : state.units.live())
    {
        const auto& candidate = candidateRecord.core;
        if (candidate.team == sourceUnit.team)
        {
            continue;
        }
        input.projectileSpreadTargets.push_back({
            candidate.id,
            candidate.motion.position,
        });
    }
    return input;
}

const BattleCastSkillState& selectedCastSkill(const BattleCastInput& input, bool ultimate)
{
    return ultimate ? input.ultimateSkill : input.normalSkill;
}

void cancelDeadRuntimeActions(BattleRuntimeState& state)
{
    for (const auto& unit : state.units.dead())
    {
        cancelRuntimeAction(state, unit.id());
    }
}

bool runtimeDashAttackEnabled(BattleRuntimeState& state, int ownerUnitId)
{
    const auto& record = state.units.require(ownerUnitId);
    const auto* plan = record.actionPlan();
    if (!plan)
    {
        return false;
    }
    const bool ultimate = record.core.vitals.maxMp > 0
        && record.core.vitals.mp >= record.core.vitals.maxMp
        && plan->ultimateSkill.id >= 0;
    const auto& skill = ultimate ? plan->ultimateSkill : plan->normalSkill;
    return runtimeCastPoliciesForSkill(
        state,
        record.core,
        skill.id,
        ultimate).dashAttack;
}

}  // namespace CoreDetail

}  // namespace KysChess::Battle
