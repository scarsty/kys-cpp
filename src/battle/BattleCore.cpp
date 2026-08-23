#include "BattleCore.h"

#include "../ChessEftIds.h"
#include "../Find.h"
#include "BattleAreaEffectSystem.h"
#include "BattleCombatIntent.h"
#include "BattleEffectAttackCastSystem.h"
#include "BattleEffectEventBridge.h"
#include "BattleLogSegments.h"
#include "BattleMath.h"
#include "BattleResourceRules.h"
#include "BattleRuntimeEffects.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <memory_resource>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace KysChess::Battle
{

namespace
{
constexpr int CoreRoleStatusEffectFrames = 48;
constexpr int ActionCastFrameJitterRadius = 1;
constexpr int ActionCastFrameJitterChoices = ActionCastFrameJitterRadius * 2 + 1;

struct RuntimeCastPolicies
{
    std::optional<ModifyCastAction> forceRanged;
    bool dashAttack{};
    bool blinkAttack{};
};

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

bool runtimeDashAttackEnabled(BattleRuntimeState& state, int ownerUnitId);

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

struct RuntimeMainHitPolicies
{
    std::vector<BattleKnockbackProcDescriptor> knockbackProcs;
    std::vector<BattleNearbyTrackingProcDescriptor> nearbyTrackingProcs;
};

RuntimeMainHitPolicies runtimeMainHitPolicies(
    std::span<const EffectExactRuntimeRuleMatch> matches)
{
    RuntimeMainHitPolicies result;
    for (const auto& match : matches)
    {
        assert(match.bound);
        const auto& bound = *match.bound;
        for (const auto& effectAction : bound.rule.actions)
        {
            if (const auto* movement = std::get_if<ForceMoveAction>(&effectAction.value);
                movement && movement->distancePixels > 0)
            {
                result.knockbackProcs.push_back({
                    .chancePct = bound.rule.chancePct,
                    .action = *movement,
                });
                continue;
            }
            const auto* attack = std::get_if<ModifyAttackAction>(&effectAction.value);
            if (!attack)
            {
                continue;
            }
            if (const auto* nearby = std::get_if<NearbyTrackingAttackBehavior>(
                    &attack->runtimeBehavior))
            {
                result.nearbyTrackingProcs.push_back({
                    .rule = { bound.binding, bound.rule.id },
                    .chancePct = bound.rule.chancePct,
                    .behavior = *nearby,
                });
            }
        }
    }
    return result;
}

int areaAttributeDelta(
    const BattleRuntimeState& state,
    int unitId,
    BattleAttribute attribute)
{
    const auto areaModifiers = BattleAreaEffectSystem::collectAreaUnitModifiers(
        state.areas,
        state.gridTransform,
        state.units,
        unitId,
        state.movement.frame,
        BattleAreaQueryPhase::UnitAttribute);
    int delta{};
    for (const auto& applied : areaModifiers.modifiers)
    {
        if (applied.modifier.attribute != attribute)
        {
            continue;
        }
        assert(applied.modifier.amount.base == EffectNumberBase::Constant);
        assert(!applied.modifier.amount.multiplierBase);
        assert(applied.modifier.amount.percent == 0);
        delta += applied.modifier.amount.flat;
    }
    return delta;
}

int areaAdjustedSpeed(const BattleRuntimeState& state, int unitId, int baseSpeed)
{
    return std::max(
        0,
        baseSpeed * (100 + areaAttributeDelta(state, unitId, BattleAttribute::Speed)) / 100);
}

int effectAdjustedAttribute(
    const BattleRuntimeState& state,
    int unitId,
    BattleAttribute attribute,
    int baseValue,
    int eventSourceUnitId = -1)
{
    return BattleEffectCommandSystem::queryAttribute(
        state,
        {
            .unitId = unitId,
            .attribute = attribute,
            .baseValue = baseValue,
            .frame = state.movement.frame,
            .eventSourceUnitId = eventSourceUnitId,
        });
}

int effectAndAreaAdjustedRateAttribute(
    const BattleRuntimeState& state,
    int unitId,
    BattleAttribute attribute,
    int baseValue)
{
    return effectAdjustedAttribute(state, unitId, attribute, baseValue)
        + areaAttributeDelta(state, unitId, attribute);
}

int effectAndAreaAdjustedSpeed(
    const BattleRuntimeState& state,
    int unitId,
    int baseSpeed)
{
    const auto status = BattleStatusSystem({}).snapshot(
        state.units.require(unitId).statusDamageState());
    const int statusAdjusted = std::max(
        0,
        effectAdjustedAttribute(state, unitId, BattleAttribute::Speed, baseSpeed)
            * (100 + status.speedPctDelta) / 100);
    return areaAdjustedSpeed(
        state,
        unitId,
        statusAdjusted);
}

bool areaDamageChannelMatches(BattleDamageKind kind, DamageChannel channel)
{
    if (channel == DamageChannel::All)
    {
        return true;
    }
    switch (kind)
    {
    case BattleDamageKind::Physical:
    case BattleDamageKind::Skill:
        return channel == DamageChannel::Skill;
    case BattleDamageKind::Poison:
    case BattleDamageKind::Bleed:
        return channel == DamageChannel::Dot;
    case BattleDamageKind::Reflected:
        return channel == DamageChannel::Reflected;
    case BattleDamageKind::Pure:
    case BattleDamageKind::Effect:
    case BattleDamageKind::Execute:
        return channel == DamageChannel::Effect;
    }
    assert(false);
    return false;
}

int areaOutgoingDamagePctDelta(
    const BattleRuntimeState& state,
    int sourceUnitId,
    BattleDamageKind damageKind)
{
    if (sourceUnitId == OptionalDamageAttackerUnitId)
    {
        return 0;
    }
    const auto modifiers = BattleAreaEffectSystem::collectAreaUnitModifiers(
        state.areas,
        state.gridTransform,
        state.units,
        sourceUnitId,
        state.movement.frame,
        BattleAreaQueryPhase::OutgoingDamage);
    int delta{};
    for (const auto& applied : modifiers.modifiers)
    {
        if (areaDamageChannelMatches(damageKind, applied.modifier.damageChannel))
        {
            delta += applied.modifier.percent;
        }
    }
    return delta;
}

BattleHealModifierState statusHealModifiers(const BattleRuntimeUnitRecord& record)
{
    const auto status = BattleStatusSystem({}).snapshot(record.statusDamageState());
    return {
        .blocked = status.healingBlocked,
        .receivedHealPcts = status.receivedHealMultipliersPct,
    };
}

BattleVisualEvent roleEffectEvent(int targetUnitId, int effectId, int durationFrames)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::RoleEffect;
    event.targetUnitId = targetUnitId;
    event.effectId = effectId;
    event.visualEffectId = effectId;
    event.durationFrames = durationFrames;
    return event;
}

BattleLogEvent makeAntiComboTransferLog(int sourceUnitId, int targetUnitId)
{
    BattleLogEvent log;
    log.type = BattleLogEventType::Status;
    log.sourceUnitId = sourceUnitId;
    log.targetUnitId = targetUnitId;
    log.segments = battleLogText("獨行轉移", BattleLogTextTone::SkillName);
    return log;
}
}

Point BattleGridTransform::toGrid(Pointf position) const
{
    assert(tileWidth > 0.0);
    assert(coordCount > 0);
    return battleIsometricGridPosition(position, coordCount, tileWidth);
}

int findNearestEnemyUnitId(const BattleRuntimeUnits& units, int sourceUnitId)
{
    const auto& source = units.requireCore(sourceUnitId);
    int targetUnitId = -1;
    std::uint64_t bestDistanceSquared{};
    for (const auto& candidateRecord : units.live())
    {
        const auto& candidate = candidateRecord.core;
        if (candidate.team == source.team)
        {
            continue;
        }

        const std::uint64_t distanceSquared = battleDistanceSquared3d(
            candidate.motion.position,
            source.motion.position);
        if (targetUnitId < 0
            || distanceSquared < bestDistanceSquared
            || (distanceSquared == bestDistanceSquared && candidate.id < targetUnitId))
        {
            targetUnitId = candidate.id;
            bestDistanceSquared = distanceSquared;
        }
    }
    return targetUnitId;
}

int findFarthestEnemyUnitId(const BattleRuntimeUnits& units, int sourceUnitId)
{
    const auto& source = units.requireCore(sourceUnitId);
    int targetUnitId = -1;
    std::uint64_t bestDistanceSquared{};
    for (const auto& candidateRecord : units.live())
    {
        const auto& candidate = candidateRecord.core;
        if (candidate.team == source.team)
        {
            continue;
        }

        const std::uint64_t distanceSquared = battleDistanceSquared3d(
            candidate.motion.position,
            source.motion.position);
        if (targetUnitId < 0
            || distanceSquared > bestDistanceSquared
            || (distanceSquared == bestDistanceSquared && candidate.id < targetUnitId))
        {
            targetUnitId = candidate.id;
            bestDistanceSquared = distanceSquared;
        }
    }
    return targetUnitId;
}

BattleUnitState makeBattleMovementPlanUnit(const BattleRuntimeUnit& runtimeUnit, double moveSpeedDivisor)
{
    assert(moveSpeedDivisor != 0.0);

    BattleUnitState unit;
    unit.id = runtimeUnit.id;
    unit.team = runtimeUnit.team;
    unit.alive = runtimeUnit.alive;
    unit.position = runtimeUnit.motion.position;
    unit.velocity = runtimeUnit.motion.velocity;
    unit.speed = runtimeUnit.stats.speed / moveSpeedDivisor;
    unit.canAttack = runtimeUnit.animation.cooldown == 0;
    unit.reach = runtimeUnit.reach;
    unit.style = runtimeUnit.style;
    return unit;
}

namespace
{
template<class... Visitors>
struct Overloaded : Visitors...
{
    using Visitors::operator()...;
};

bool isLastAliveInTeam(const BattleRuntimeUnits& units, const BattleRuntimeUnit& unit);

struct BattleFrameMpRestore
{
    int unitId{};
    int amount{};
    std::string reason;
};

struct BattleFrameEffectCommandBatch
{
    std::vector<EffectCommand> commands;
    BattleEffectCommandContext context;
};

class BattleFrameContext;

template <typename T>
using BattleFrameVector = std::pmr::vector<T>;

void appendRuntimeSpiralBleedCastEffects(
    BattleRuntimeState& state,
    int sourceUnitId,
    const BattleCastSkillState& skill,
    double projectileSpeed,
    std::span<const BattleEffectDispatchResult> committedEffects,
    std::vector<BattleAttackSpawnRequest>& attackSpawns);

void applyKnockbackImpulse(
    BattleRuntimeState& state,
    const BattleKnockbackCommand& knockback);

EffectResourcesBeforeCastSnapshot snapshotEffectResourcesBeforeCast(
    const BattleRuntimeState& state);
BattleEffectDispatchResult dispatchCastPlannedEffects(
    BattleRuntimeState& state,
    const BattleCastInput& input,
    bool ultimate,
    const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast,
    const BattleCastProvenance& provenance);
void applyEffectAttackDirectives(
    std::span<BattleAttackSpawnRequest> requests,
    const BattleEffectAttackApplyResult& result);

struct BattleRuntimeUnitsAdvanceResult
{
    std::vector<BattleFrameEffectCommandBatch> cooldownFinishedEffects;
};

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

bool isMpBlocked(BattleRuntimeState& state, int unitId)
{
    return state.units.require(unitId).mpBlocked();
}

int mpRecoveryBonusPct(BattleRuntimeState& state, int unitId)
{
    return effectAdjustedAttribute(
        state,
        unitId,
        BattleAttribute::MpRecoveryBonus,
        0);
}

int adjustedRuntimeMpRestore(BattleRuntimeState& state, int unitId, int amount)
{
    return adjustedMpRestore(
        isMpBlocked(state, unitId),
        mpRecoveryBonusPct(state, unitId),
        amount);
}

void applyRuntimeUnitMpDelta(BattleRuntimeState& state, BattleRuntimeUnit& unit, int mpDelta)
{
    if (mpDelta > 0)
    {
        unit.vitals.mp += adjustedRuntimeMpRestore(state, unit.id, mpDelta);
    }
    else if (mpDelta < 0)
    {
        unit.vitals.mp += mpDelta;
    }
    unit.vitals.mp = std::clamp(unit.vitals.mp, 0, unit.vitals.maxMp);
}

std::string formatStatusValue(const std::string& label, int value, const char* unit)
{
    if (value <= 0)
    {
        return label;
    }
    return std::format("{}（{}{}）", label, value, unit);
}

BattleHitUnitSnapshot makeHitUnitSnapshot(const BattleRuntimeUnit& unit)
{
    BattleHitUnitSnapshot snapshot;
    snapshot.id = unit.id;
    snapshot.team = unit.team;
    snapshot.alive = unit.alive;
    snapshot.vitals = unit.vitals;
    snapshot.stats = unit.stats;
    snapshot.motion = unit.motion;
    snapshot.animation = unit.animation;
    snapshot.invincible = unit.invincible;
    snapshot.haveAction = unit.haveAction;
    snapshot.operationType = unit.operationType;
    return snapshot;
}

int actPropertyForMagicType(const BattleRuntimeUnit& unit, int magicType)
{
    auto it = unit.actPropertiesByMagicType.find(magicType);
    return it != unit.actPropertiesByMagicType.end() ? it->second : 0;
}

BattleHitSkillSnapshot makeHitSkillSnapshot(
    const BattleAttackEvent& event,
    const BattleRuntimeUnit& attacker,
    const BattleRuntimeUnit& defender,
    int resolvedBaseDamage)
{
    BattleHitSkillSnapshot skill;
    skill.id = event.skillId;
    skill.name = event.skillName;
    skill.hurtType = event.skillHurtType;
    skill.magicType = event.skillMagicType;
    skill.effectId = event.skillEffectId;
    skill.attackerActProperty = event.skillAttackerActProperty != 0
        ? event.skillAttackerActProperty
        : actPropertyForMagicType(attacker, event.skillMagicType);
    skill.defenderActProperty = actPropertyForMagicType(defender, event.skillMagicType);
    skill.magicPower = event.skillMagicPower;
    skill.resolvedBaseDamage = resolvedBaseDamage;
    return skill;
}

int sharedBleedMaxStacks(const BattleAttackEvent& event)
{
    return std::max(1, event.scriptedBleedStacks);
}

int resolveHitMagicBaseDamage(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    const BattleRuntimeUnit& attacker,
    const BattleRuntimeUnit& defender,
    int ignoreDefensePct = 0)
{
    assert(ignoreDefensePct >= 0);
    BattleFixed defence = effectAdjustedAttribute(
        state,
        defender.id,
        BattleAttribute::Defence,
        defender.stats.defence);
    defence = defence.scaled(
        100 - std::min(ignoreDefensePct, 100),
        100);

    return BattleDamageSystem().resolveMagicBaseDamage({
        effectAdjustedAttribute(
            state,
            attacker.id,
            BattleAttribute::Attack,
            attacker.stats.attack),
        event.skillMagicPower,
        defence,
        state.random.symmetricInt(10),
    });
}

int resolveProjectileCancelDamage(
    BattleRuntimeState& state,
    const BattleAttackInstance& attack,
    const BattleAttackInstance& otherAttack,
    int currentDamage)
{
    const auto& attacker = state.units.requireCore(attack.state.attackSourceUnitId);
    int damage = currentDamage;
    if (attack.state.skillId >= 0)
    {
        const auto& defender = state.units.requireCore(otherAttack.state.attackSourceUnitId);
        BattleAttackEvent event;
        event.sourceUnitId = attack.state.attackSourceUnitId;
        event.skillId = attack.state.skillId;
        event.skillMagicPower = attack.state.skillMagicPower;
        damage = scaleProjectileCancelDamage(
            resolveHitMagicBaseDamage(state, event, attacker, defender),
            attack.state.operationType);
    }

    damage = std::max(
        0,
        effectAdjustedAttribute(
            state,
            attacker.id,
            BattleAttribute::ProjectilePressureDamage,
            damage));
    return BattleFixed::fromInteger(damage)
        .scaled(attack.state.projectilePressurePct, 100)
        .toInt();
}

BattleLogEvent dodgeStatusEvent(int defenderUnitId, int attackerUnitId)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Status;
    event.sourceUnitId = defenderUnitId;
    event.targetUnitId = attackerUnitId;
    event.perspective = BattleLogPerspective::SourceOnly;
    event.segments = battleLogText("閃避了來襲攻擊", BattleLogTextTone::SkillName);
    return event;
}

void applyAttackContext(BattleVisualEvent& presentation, const BattleAttackState& world, int attackId)
{
    presentation.effectId = attackId;
    if (const auto* attack = tryFindById(world.attacks, attackId))
    {
        presentation.sourceUnitId = attack->state.attackSourceUnitId;
        presentation.targetUnitId = attack->state.preferredTargetUnitId;
        presentation.durationFrames = attack->state.totalFrame;
        presentation.visualEffectId = attack->state.visualEffectId;
        presentation.position = attack->state.position;
        presentation.velocity = attack->state.velocity;
        presentation.operationKind = toPresentationOperationKind(attack->state.operationType);
        presentation.through = attack->state.through;
    }
}

void applyAttackContext(BattleGameplayEvent& gameplay, const BattleAttackState& world, int attackId)
{
    gameplay.effectId = attackId;
    if (const auto* attack = tryFindById(world.attacks, attackId))
    {
        gameplay.sourceUnitId = attack->state.attackSourceUnitId;
        gameplay.position = attack->state.position;
        gameplay.skillId = attack->state.skillId;
    }
}

BattleVisualEvent toProjectileSpawnPresentationEvent(
    const BattleAttackState& world,
    int attackId)
{
    BattleVisualEvent presentation;
    presentation.type = BattleVisualEventType::ProjectileSpawned;
    applyAttackContext(presentation, world, attackId);
    return presentation;
}

template <class Append>
void appendVisualEvents(
    const BattleAttackEvent& event,
    const BattleAttackState& world,
    Append&& append)
{
    BattleVisualEvent presentation;
    applyAttackContext(presentation, world, event.attackId);

    switch (event.type)
    {
    case BattleAttackEventType::AttackSpawned:
        presentation.type = BattleVisualEventType::ProjectileSpawned;
        presentation.effectId = event.attackId;
        presentation.sourceUnitId = event.sourceUnitId;
        presentation.targetUnitId = event.unitId;
        presentation.durationFrames = event.totalFrame;
        presentation.visualEffectId = event.visualEffectId;
        presentation.position = event.position;
        presentation.velocity = event.velocity;
        presentation.operationKind = toPresentationOperationKind(event.operationType);
        break;
    case BattleAttackEventType::Moved:
        presentation.type = BattleVisualEventType::ProjectileMoved;
        break;
    case BattleAttackEventType::Hit:
        presentation.type = BattleVisualEventType::ProjectileHit;
        presentation.targetUnitId = event.unitId;
        presentation.impactEffectSoundId = event.skillEffectId;
        if (event.scriptedDamage > 0 || event.scriptedStunFrames > 0 || event.scriptedBleedStacks > 0)
        {
            presentation.impactUnitShake = 5;
        }
        else
        {
            presentation.impactSceneShake = event.provenance.cast.ultimate ? 10 : 0;
            presentation.impactUnitShake = event.provenance.cast.ultimate ? 10 : 5;
            presentation.impactRumble = event.operationType != BattleOperationType::None;
        }
        break;
    case BattleAttackEventType::Expired:
        presentation.type = BattleVisualEventType::ProjectileExpired;
        break;
    case BattleAttackEventType::TargetLost:
        presentation.type = BattleVisualEventType::ProjectileTargetLost;
        presentation.targetUnitId = event.unitId;
        presentation.amount = -1;
        break;
    case BattleAttackEventType::ChainEnded:
    case BattleAttackEventType::ChainNoTargetInRange:
        presentation.type = BattleVisualEventType::ProjectileExpired;
        presentation.targetUnitId = event.unitId;
        break;
    case BattleAttackEventType::ProjectileCancel:
        presentation.type = BattleVisualEventType::ProjectileCancelled;
        presentation.amount = event.otherAttackId;
        break;
    case BattleAttackEventType::BlockedByInvincible:
        return;
    case BattleAttackEventType::Bounce:
        presentation.type = BattleVisualEventType::ProjectileBounced;
        presentation.targetUnitId = event.unitId;
        presentation.amount = event.otherAttackId;
        break;
    }
    append(std::move(presentation));
    if (event.type == BattleAttackEventType::AttackSpawned
        && event.castSubrequestKind == BattleAttackCastSubrequestKind::DualWieldFollowUp)
    {
        assert(event.roleAttackEchoActType >= 0);
        BattleVisualEvent echo;
        echo.type = BattleVisualEventType::RoleAttackEcho;
        echo.sourceUnitId = event.sourceUnitId;
        echo.targetUnitId = event.unitId;
        echo.animationActType = event.roleAttackEchoActType;
        append(std::move(echo));
    }
    if (event.type == BattleAttackEventType::Bounce)
    {
        append(toProjectileSpawnPresentationEvent(world, event.otherAttackId));
    }
}

BattleGameplayEvent toGameplayEvent(
    const BattleAttackEvent& event,
    const BattleAttackState& world)
{
    BattleGameplayEvent gameplay;
    applyAttackContext(gameplay, world, event.attackId);

    switch (event.type)
    {
    case BattleAttackEventType::AttackSpawned:
        gameplay.type = BattleGameplayEventType::AttackSpawned;
        gameplay.effectId = event.attackId;
        gameplay.sourceUnitId = event.sourceUnitId;
        gameplay.targetUnitId = event.unitId;
        gameplay.position = event.position;
        break;
    case BattleAttackEventType::Moved:
        gameplay.type = BattleGameplayEventType::ProjectileMoved;
        break;
    case BattleAttackEventType::Hit:
        gameplay.type = BattleGameplayEventType::ProjectileHit;
        gameplay.targetUnitId = event.unitId;
        break;
    case BattleAttackEventType::Expired:
        gameplay.type = BattleGameplayEventType::ProjectileExpired;
        break;
    case BattleAttackEventType::TargetLost:
        gameplay.type = BattleGameplayEventType::ProjectileCancelled;
        gameplay.targetUnitId = event.unitId;
        break;
    case BattleAttackEventType::ChainEnded:
    case BattleAttackEventType::ChainNoTargetInRange:
        gameplay.type = BattleGameplayEventType::ProjectileExpired;
        gameplay.targetUnitId = event.unitId;
        break;
    case BattleAttackEventType::ProjectileCancel:
        gameplay.type = BattleGameplayEventType::ProjectileCancelled;
        gameplay.otherAttackId = event.otherAttackId;
        break;
    case BattleAttackEventType::BlockedByInvincible:
        gameplay.type = BattleGameplayEventType::StatusApplied;
        gameplay.targetUnitId = event.unitId;
        gameplay.text = "彈道命中無敵：傷害忽略";
        break;
    case BattleAttackEventType::Bounce:
        gameplay.type = BattleGameplayEventType::AttackSpawned;
        gameplay.effectId = event.otherAttackId;
        gameplay.targetUnitId = event.unitId;
        if (const auto* sourceAttack = tryFindById(world.attacks, event.attackId))
        {
            gameplay.sourceUnitId = sourceAttack->state.attackSourceUnitId;
        }
        if (const auto* spawnedAttack = tryFindById(world.attacks, event.otherAttackId))
        {
            gameplay.position = spawnedAttack->state.position;
        }
        break;
    }
    return gameplay;
}

std::vector<BattleLogTextSegment> formatProjectileCancelLogSegments(
    int leftAttackId,
    int leftDamage,
    int rightAttackId,
    int rightDamage)
{
    const int remaining = leftDamage - rightDamage;
    if (remaining == 0)
    {
        return logSegments<BattleLogTextTone::SkillName>(
            "抵消彈道 ",
            std::pair{ BattleLogTextTone::ProjectileId, std::format("#{}", leftAttackId) },
            " vs ",
            std::pair{ BattleLogTextTone::ProjectileId, std::format("#{}", rightAttackId) },
            "（",
            std::pair{ BattleLogTextTone::DamageValue, leftDamage },
            std::pair{ BattleLogTextTone::FormulaValue, " - " },
            std::pair{ BattleLogTextTone::DamageValue, rightDamage },
            std::pair{ BattleLogTextTone::FormulaValue, " = " },
            std::pair{ BattleLogTextTone::DamageValue, 0 },
            "，雙方互消）");
    }
    return logSegments<BattleLogTextTone::SkillName>(
        "抵消彈道 ",
        std::pair{ BattleLogTextTone::ProjectileId, std::format("#{}", leftAttackId) },
        " vs ",
        std::pair{ BattleLogTextTone::ProjectileId, std::format("#{}", rightAttackId) },
        "（",
        std::pair{ BattleLogTextTone::DamageValue, leftDamage },
        std::pair{ BattleLogTextTone::FormulaValue, " - " },
        std::pair{ BattleLogTextTone::DamageValue, rightDamage },
        std::pair{ BattleLogTextTone::FormulaValue, " = " },
        std::pair{ BattleLogTextTone::DamageValue, remaining },
        "）");
}

enum class ProjectileStopLogReason
{
    TargetLost,
    ChainTargetLost,
    ChainEnded,
    ChainNoTargetInRange,
};

struct ProjectileStopLogBucket
{
    int sourceUnitId = -1;
    ProjectileStopLogReason reason = ProjectileStopLogReason::TargetLost;
    int count = 0;
};

struct ProjectileInvincibleBlockLogBucket
{
    int unitId = -1;
    int count = 0;
};

void addProjectileStopLog(
    std::vector<ProjectileStopLogBucket>& buckets,
    int sourceUnitId,
    ProjectileStopLogReason reason)
{
    auto bucket = std::find_if(
        buckets.begin(),
        buckets.end(),
        [&](const ProjectileStopLogBucket& candidate)
        {
            return candidate.sourceUnitId == sourceUnitId
                && candidate.reason == reason;
        });
    if (bucket == buckets.end())
    {
        buckets.push_back({ sourceUnitId, reason, 1 });
        return;
    }
    ++bucket->count;
}

void addProjectileInvincibleBlockLog(
    std::vector<ProjectileInvincibleBlockLogBucket>& buckets,
    int unitId)
{
    auto bucket = std::find_if(
        buckets.begin(),
        buckets.end(),
        [&](const ProjectileInvincibleBlockLogBucket& candidate)
        {
            return candidate.unitId == unitId;
        });
    if (bucket == buckets.end())
    {
        buckets.push_back({ unitId, 1 });
        return;
    }
    ++bucket->count;
}

std::string formatProjectileStopLogText(const ProjectileStopLogBucket& bucket)
{
    switch (bucket.reason)
    {
    case ProjectileStopLogReason::TargetLost:
        return std::format("彈道停止：{}枚目標遺失", bucket.count);
    case ProjectileStopLogReason::ChainTargetLost:
        return std::format("連鎖彈道停止：{}枚原目標失效", bucket.count);
    case ProjectileStopLogReason::ChainEnded:
        return std::format("連鎖彈道停止：{}枚已達最後一跳", bucket.count);
    case ProjectileStopLogReason::ChainNoTargetInRange:
        return std::format("連鎖彈道停止：{}枚搜尋範圍內無可連鎖目標", bucket.count);
    }
    assert(false);
    return {};
}

void appendProjectileCancellationLogEvents(
    const BattleAttackState& world,
    std::span<const BattleAttackEvent> events,
    std::vector<BattleLogEvent>& logEvents,
    bool chainedProjectileLogs)
{
    std::vector<ProjectileStopLogBucket> stopLogs;
    std::vector<ProjectileInvincibleBlockLogBucket> invincibleBlockLogs;
    for (const auto& event : events)
    {
        switch (event.type)
        {
        case BattleAttackEventType::TargetLost:
        {
            if (chainedProjectileLogs)
            {
                break;
            }
            const BattleAttackInstance* attack = tryFindById(world.attacks, event.attackId);
            addProjectileStopLog(
                stopLogs,
                attack ? attack->state.attackSourceUnitId : -1,
                attack && attack->provenance.parentAttackId.has_value()
                    ? ProjectileStopLogReason::ChainTargetLost
                    : ProjectileStopLogReason::TargetLost);
            break;
        }
        case BattleAttackEventType::ProjectileCancel:
        {
            if (chainedProjectileLogs)
            {
                break;
            }
            const bool otherWins = event.otherProjectileCancelDamage > event.projectileCancelDamage;
            const int leftAttackId = otherWins ? event.otherAttackId : event.attackId;
            const int rightAttackId = otherWins ? event.attackId : event.otherAttackId;
            const int leftSourceUnitId = otherWins ? event.otherSourceUnitId : event.sourceUnitId;
            const int rightSourceUnitId = otherWins ? event.sourceUnitId : event.otherSourceUnitId;
            const int leftDamage = otherWins ? event.otherProjectileCancelDamage : event.projectileCancelDamage;
            const int rightDamage = otherWins ? event.projectileCancelDamage : event.otherProjectileCancelDamage;
            BattleLogEvent log;
            log.type = BattleLogEventType::Status;
            log.sourceUnitId = leftSourceUnitId;
            log.targetUnitId = rightSourceUnitId;
            log.amount = leftDamage;
            log.secondaryAmount = rightDamage;
            log.effectId = leftAttackId;
            log.otherEffectId = rightAttackId;
            log.category = BattleLogCategory::ProjectileCancel;
            log.segments = formatProjectileCancelLogSegments(leftAttackId, leftDamage, rightAttackId, rightDamage);
            logEvents.push_back(std::move(log));
            break;
        }
        case BattleAttackEventType::ChainEnded:
        case BattleAttackEventType::ChainNoTargetInRange:
        {
            if (!chainedProjectileLogs)
            {
                break;
            }
            const auto* attack = tryFindById(world.attacks, event.attackId);
            addProjectileStopLog(
                stopLogs,
                attack ? attack->state.attackSourceUnitId : -1,
                event.type == BattleAttackEventType::ChainEnded
                    ? ProjectileStopLogReason::ChainEnded
                    : ProjectileStopLogReason::ChainNoTargetInRange);
            break;
        }
        case BattleAttackEventType::BlockedByInvincible:
        {
            if (chainedProjectileLogs)
            {
                break;
            }
            addProjectileInvincibleBlockLog(invincibleBlockLogs, event.unitId);
            break;
        }
        case BattleAttackEventType::AttackSpawned:
        case BattleAttackEventType::Moved:
        case BattleAttackEventType::Hit:
        case BattleAttackEventType::Expired:
        case BattleAttackEventType::Bounce:
            break;
        }
    }

    for (const auto& bucket : stopLogs)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = bucket.sourceUnitId;
        log.targetUnitId = -1;
        log.segments = battleLogText(formatProjectileStopLogText(bucket), BattleLogTextTone::SkillName);
        logEvents.push_back(std::move(log));
    }

    for (const auto& bucket : invincibleBlockLogs)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = bucket.unitId;
        log.targetUnitId = -1;
        log.perspective = BattleLogPerspective::SourceOnly;
        log.segments = battleLogText(
            std::format("彈道命中無敵：{}枚傷害忽略", bucket.count),
            BattleLogTextTone::SkillName);
        logEvents.push_back(std::move(log));
    }
}

BattleRuntimeUnitsAdvanceResult advanceRuntimeUnits(BattleRuntimeState& state)
{
    BattleRuntimeUnitsAdvanceResult result;
    result.cooldownFinishedEffects.reserve(state.units.size());
    for (auto& unitRecord : state.units.live())
    {
        auto& unit = unitRecord.core;
        assert(unit.id >= 0);
        auto tick = unitRecord.advanceFrameTick({
            .frame = state.movement.frame,
            .mpRegenIntervalFrames = 3,
            .physicalPowerRegenIntervalFrames = 3,
            .mpRecoveryBonusPct = mpRecoveryBonusPct(state, unit.id),
        });
        if (tick.skillFinished)
        {
            if (unitRecord.isSkillCooldownUltimate())
            {
                const auto* actionPlan = unitRecord.actionPlan();
                assert(actionPlan && actionPlan->ultimateSkill.id >= 0);
                BattleEffectEventHeaderInput header{
                    .frame = state.movement.frame + 1,
                    .eventOrdinal = state.effectIntegration.nextEventOrdinal++,
                    .ownerUnitId = unit.id,
                };
                auto dispatched = BattleEffectEventBridge().dispatch(
                    state,
                    std::move(header),
                    EffectEvent::UltimateCooldownFinished,
                    UltimateCooldownFinishedEventData{
                        .magicId = actionPlan->ultimateSkill.id,
                    });
                result.cooldownFinishedEffects.push_back({
                    .commands = std::move(dispatched.commands),
                    .context = {
                        .frame = state.movement.frame + 1,
                        .effectPosition = unit.motion.position,
                        .areaTargetTeamDomain = unit.team,
                    },
                });
            }
            unitRecord.clearSkillCooldownSource();
        }
    }
    return result;
}

void applyProjectileCancelDamageResults(
    BattleRuntimeState& state,
    std::pmr::vector<BattleAttackEvent>& events)
{
    for (auto& event : events)
    {
        if (event.type != BattleAttackEventType::ProjectileCancel)
        {
            continue;
        }

        assert(event.attackId >= 0);
        assert(event.otherAttackId >= 0);
        const auto& attack = requireById(state.attacks.attacks, event.attackId);
        const auto& otherAttack = requireById(state.attacks.attacks, event.otherAttackId);

        event.projectileCancelDamage = resolveProjectileCancelDamage(
            state,
            attack,
            otherAttack,
            event.projectileCancelDamage);
        event.otherProjectileCancelDamage = resolveProjectileCancelDamage(
            state,
            otherAttack,
            attack,
            event.otherProjectileCancelDamage);

        state.attacks.applyProjectileCancelDamage(event);
    }
}

BattleHitResolutionInput makeHitResolutionInput(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    int ignoreDefensePct,
    const RuntimeMainHitPolicies& runtimePolicies)
{
    const auto& attacker = state.units.require(event.sourceUnitId);
    const auto& defender = state.units.require(event.unitId);

    BattleHitResolutionInput input;
    input.attackEvent = event;
    input.attacker = makeHitUnitSnapshot(attacker.core);
    input.defender = makeHitUnitSnapshot(defender.core);
    input.attacker.stats.attack = effectAdjustedAttribute(
        state,
        attacker.id(),
        BattleAttribute::Attack,
        input.attacker.stats.attack);
    input.attacker.stats.speed = effectAndAreaAdjustedSpeed(
        state,
        attacker.id(),
        input.attacker.stats.speed);
    input.defender.stats.defence = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::Defence,
        input.defender.stats.defence);
    input.defender.stats.speed = effectAndAreaAdjustedSpeed(
        state,
        defender.id(),
        input.defender.stats.speed);
    input.attackerCriticalChancePct = effectAndAreaAdjustedRateAttribute(
        state,
        attacker.id(),
        BattleAttribute::CriticalChance,
        0);
    input.attackerCriticalMultiplierPct = effectAndAreaAdjustedRateAttribute(
        state,
        attacker.id(),
        BattleAttribute::CriticalDamage,
        150);
    input.defenderProjectileReflectChancePct = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::ProjectileReflectChance,
        0,
        attacker.id());
    input.defenderSkillReflectPercent = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::SkillReflectPercent,
        0,
        attacker.id());
    input.attackerCooldownExtensionChancePct = effectAdjustedAttribute(
        state,
        attacker.id(),
        BattleAttribute::OutgoingCooldownExtensionChance,
        0,
        defender.id());
    input.attackerCooldownExtensionPct = effectAdjustedAttribute(
        state,
        attacker.id(),
        BattleAttribute::OutgoingCooldownExtensionPercent,
        0,
        defender.id());
    input.defenderCooldownExtensionChancePct = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::IncomingCooldownExtensionChance,
        0,
        attacker.id());
    input.defenderCooldownExtensionPct = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::IncomingCooldownExtensionPercent,
        0,
        attacker.id());
    input.sharedBleedMaxStacks = sharedBleedMaxStacks(event);
    input.randomDamageVariance = state.random.symmetricInt(10);
    input.knockbackProcs = runtimePolicies.knockbackProcs;
    input.nearbyTrackingProcs = runtimePolicies.nearbyTrackingProcs;

    if (event.skillId >= 0)
    {
        input.skill = makeHitSkillSnapshot(
            event,
            attacker.core,
            defender.core,
            resolveHitMagicBaseDamage(
                state,
                event,
                attacker.core,
                defender.core,
                ignoreDefensePct));
    }
    return input;
}

EffectResourcesBeforeCastSnapshot snapshotEffectResourcesBeforeCast(
    const BattleRuntimeState& state)
{
    std::vector<EffectUnitResourceBeforeCast> result;
    result.reserve(state.units.size());
    for (const auto& record : state.units.all())
    {
        result.push_back({
            .unitId = record.id(),
            .mp = record.core.vitals.mp,
            .maxMp = record.core.vitals.maxMp,
        });
    }
    std::ranges::sort(result, {}, &EffectUnitResourceBeforeCast::unitId);
    return EffectResourcesBeforeCastSnapshot(std::move(result));
}

BattleEffectEventHeaderInput nextEffectEventHeader(
    BattleRuntimeState& state,
    int ownerUnitId,
    EffectFormulaInputs formulaInputs = {})
{
    assert(ownerUnitId >= 0);
    state.units.requireCore(ownerUnitId);
    return {
        .frame = state.movement.frame,
        .eventOrdinal = state.effectIntegration.nextEventOrdinal++,
        .ownerUnitId = ownerUnitId,
        .formulaInputs = std::move(formulaInputs),
    };
}

BattleCastProvenance plannedEffectCastProvenance(
    int sourceUnitId,
    int magicId,
    bool ultimate)
{
    return {
        .sourceUnitId = sourceUnitId,
        .magicId = magicId,
        .ultimate = ultimate,
        .origin = ultimate ? CastOriginKind::Ultimate : CastOriginKind::Normal,
        .propagation = CastPropagationPolicy::SourceRules,
    };
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
    const BattleCastProvenance* provenance = nullptr)
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

CastPlanEventData makeCastPlanEventData(
    const BattleCastInput& input,
    bool ultimate,
    const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast,
    const BattleCastProvenance* provenance = nullptr)
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

template<class Payload>
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
    const auto resources = snapshotEffectResourcesBeforeCast(state);
    auto payload = makeCastPlanEventData(
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

BattleEffectDispatchResult dispatchCastPlannedEffects(
    BattleRuntimeState& state,
    const BattleCastInput& input,
    bool ultimate,
    const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast,
    const BattleCastProvenance& provenance)
{
    const auto& skill = ultimate ? input.ultimateSkill : input.normalSkill;
    if (skill.id < 0)
    {
        return {};
    }

    assert(provenance.valid());
    assert(provenance.sourceUnitId == input.unit.id);
    assert(provenance.magicId == skill.id);
    assert(provenance.ultimate == ultimate);
    auto payload = makeCastPlanEventData(
        input,
        ultimate,
        resourcesBeforeCast,
        &provenance);
    return BattleEffectEventBridge().dispatch(
        state,
        nextEffectEventHeader(state, input.unit.id),
        EffectEvent::CastPlanned,
        std::move(payload));
}

void applyEffectAttackDirectives(
    std::span<BattleAttackSpawnRequest> requests,
    const BattleEffectAttackApplyResult& result)
{
    for (const auto& damage : result.damage)
    {
        assert(damage.requestIndex < requests.size());
        if (damage.damageKind)
        {
            requests[damage.requestIndex].initial.damageKind = *damage.damageKind;
        }
    }
    for (const auto& lifecycle : result.lifecycle)
    {
        assert(lifecycle.requestIndex < requests.size());
        auto& request = requests[lifecycle.requestIndex];
        request.provenance.propagation = lifecycle.propagation;
        request.provenance.origin = lifecycle.origin;
        request.provenance.parentAttackId = lifecycle.parentAttackId;
        request.provenance.rootAttack = lifecycle.rootAttack;
    }
}

BattleCastStart beginEffectRootCast(
    BattleCastLifecycle& lifecycle,
    int sourceUnitId,
    int magicId,
    bool ultimate)
{
    return lifecycle.beginRootCast({
        .sourceUnitId = sourceUnitId,
        .magicId = magicId,
        .ultimate = ultimate,
        .origin = ultimate ? CastOriginKind::Ultimate : CastOriginKind::Normal,
        .propagation = CastPropagationPolicy::SourceRules,
    });
}

void cancelEffectRootCast(
    BattleRuntimeState& state,
    const BattleCastStart& start)
{
    assert(start.provenance.valid());
    BattleEffectEventBridge().releaseCastScopedRules(
        state,
        start.provenance.castId);
    state.castLifecycle.cancelPlannedCast(start, state.movement.frame);
}

void reserveEffectRootCastAttacks(
    BattleCastLifecycle& lifecycle,
    const BattleCastStart& start,
    std::span<BattleAttackSpawnRequest> requests)
{
    assert(start.provenance.valid());
    for (std::size_t index = 0; index < requests.size(); ++index)
    {
        auto& request = requests[index];
        assert(!request.provenance.valid());
        assert(!request.castWork.valid());

        BattleAttackReservationRequest reservationRequest;
        reservationRequest.parentAttackId = request.provenance.parentAttackId;
        reservationRequest.origin = request.provenance.origin;
        reservationRequest.rootAttack = request.provenance.rootAttack;
        reservationRequest.mainProjectile = request.provenance.mainProjectile;
        reservationRequest.sharedHitGroupId = request.provenance.sharedHitGroupId;
        reservationRequest.propagation = request.provenance.propagation;
        const auto reservation = lifecycle.reserveAttack(
            start.provenance.castId,
            reservationRequest);
        request.provenance = reservation.provenance;
        request.castWork = reservation.work;
    }
}

std::vector<BattleEffectDispatchResult> dispatchCastCommittedEffects(
    BattleRuntimeState& state,
    const BattlePendingCastAction& pending,
    const BattleCastResult& cast)
{
    const auto& provenance = pending.effectCast.provenance;
    assert(provenance.valid());
    assert(provenance.sourceUnitId == cast.decision.unitId);
    assert(provenance.ultimate == cast.decision.ultimate);
    CastCommitEventData payload;
    payload.provenance = provenance;
    payload.targetUnitId = cast.decision.targetUnitId;
    const auto resourcesBeforeCast = pending.effectResourcesBeforeCast.values();
    const auto resource = std::ranges::find(
        resourcesBeforeCast,
        provenance.sourceUnitId,
        &EffectUnitResourceBeforeCast::unitId);
    payload.mpBefore = resource != resourcesBeforeCast.end()
        ? resource->mp
        : state.units.requireCore(provenance.sourceUnitId).vitals.mp;
    payload.mpPaid = std::max(0, -cast.mpDelta);
    payload.rangeMode = pending.effectPreparation.rangeMode.value_or(CastRangeMode::Preserve);
    payload.attackPattern = cast.attackPattern;
    payload.resourcesBeforeCast = pending.effectResourcesBeforeCast;
    std::vector<BattleEffectDispatchResult> result;
    result.push_back(BattleEffectEventBridge().dispatch(
        state,
        nextEffectEventHeader(state, provenance.sourceUnitId),
        EffectEvent::AttackCommitted,
        payload));
    if (cast.decision.ultimate)
    {
        result.push_back(BattleEffectEventBridge().dispatch(
            state,
            nextEffectEventHeader(state, provenance.sourceUnitId),
            EffectEvent::UltimateCommitted,
            std::move(payload)));
    }
    return result;
}

struct BattleCopiedAttackDefinitionRequest
{
    int definitionOwnerUnitId = -1;
    CastPropagationPolicy propagation = CastPropagationPolicy::SuppressUltimateRules;
};

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

void queueCopiedAttackDefinitionChildCasts(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleCastProvenance& parent,
    int parentTargetUnitId,
    std::span<const BattleCopiedAttackDefinitionRequest> requests,
    std::pmr::memory_resource* frameMemoryResource);

bool tryResolveDodgeHit(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    const double roll = state.random.nextPercent();
    const int dodgeChancePct = std::clamp(
        effectAdjustedAttribute(
            state,
            event.unitId,
            BattleAttribute::DodgeChance,
            0,
            event.sourceUnitId)
            + areaAttributeDelta(state, event.unitId, BattleAttribute::DodgeChance),
        0,
        100);
    if (dodgeChancePct <= 0 || roll >= dodgeChancePct)
    {
        return false;
    }

    auto& defender = state.units.require(event.unitId);
    const int criticalAfterDodge = effectAdjustedAttribute(
        state,
        event.unitId,
        BattleAttribute::CriticalAfterDodge,
        0,
        event.sourceUnitId);
    if (criticalAfterDodge > 0)
    {
        BattleStatusApplyRequest request;
        request.kind = BattleStatusKind::NextAttackCritical;
        request.sourceUnitId = event.unitId;
        request.stack = EffectStackPolicy::Refresh;
        request.bypassStatusShield = true;
        auto statusConfig = state.status.config;
        statusConfig.frame = state.movement.frame;
        auto applied = BattleStatusSystem(statusConfig).apply(
            defender.statusDamageState(),
            request);
        assert(applied.applied);
        defender.writeStatusDamageResult(applied.target);
    }

    logEvents.push_back(dodgeStatusEvent(event.unitId, event.sourceUnitId));
    visualEvents.push_back(roleEffectEvent(event.unitId, KysChess::EFT_EVADE, CoreRoleStatusEffectFrames));
    return true;
}

bool consumeNextAttackCritical(BattleRuntimeState& state, int attackerUnitId)
{
    auto& attacker = state.units.require(attackerUnitId);
    const auto snapshot = BattleStatusSystem({}).snapshot(attacker.statusDamageState());
    if (!snapshot.has(BattleStatusKind::NextAttackCritical))
    {
        return false;
    }

    auto consumed = BattleStatusSystem({}).consume(
        attacker.statusDamageState(),
        { .kind = BattleStatusKind::NextAttackCritical });
    assert(consumed.consumed);
    attacker.writeStatusDamageResult(consumed.target);
    return true;
}

int rescueSnapshotUnitId(const BattleFrameRescueUnitSnapshot& snapshot)
{
    return snapshot.unit.id;
}

struct RuntimeCastSkillProfile
{
    int effectiveSelectDistance{};
    int projectileSpeedMultiplierPct = 100;
    double reach{};
    double blinkReach{};
    bool forceRanged = false;
    bool rangedStyle = false;
};

RuntimeCastSkillProfile makeRuntimeCastSkillProfile(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed,
    bool ultimate,
    const RuntimeCastPolicies* precomputedPolicies = nullptr);

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

struct UnitMotionSnapshot
{
    int unitId = -1;
    BattleUnitMotion motion;
};

using UnitMotionSnapshotList = std::pmr::vector<UnitMotionSnapshot>;

constexpr float DeathKickImpactHeight = 36.0f;

bool needsCorpsePhysics(const BattleRuntimeUnit& unit)
{
    return !unit.alive
        && (unit.motion.position.z > 0.0f || unit.motion.velocity.norm() > 0.01);
}

double deathKickSpeed(int committedHpDamage)
{
    constexpr double MaxDeathKickSpeed = 75.0;

    return std::clamp(committedHpDamage / 3.0 + 5.0, 0.0, MaxDeathKickSpeed);
}

Pointf deathKickVelocity(Pointf direction, int committedHpDamage)
{
    const double speed = deathKickSpeed(committedHpDamage);
    const double verticalSpeed = std::min(6.0, speed * 0.35);
    const double horizontalSpeed = std::sqrt(std::max(0.0, speed * speed - verticalSpeed * verticalSpeed));

    direction.z = 0.0f;
    if (direction.norm() <= 0.01)
    {
        direction = { 1, 0, 0 };
    }
    direction.normTo(static_cast<float>(horizontalSpeed));
    direction.z = static_cast<float>(verticalSpeed);
    return direction;
}

UnitMotionSnapshotList makeUnitMotionSnapshot(
    const BattleRuntimeUnits& units,
    std::pmr::memory_resource* frameMemoryResource)
{
    UnitMotionSnapshotList snapshots(frameMemoryResource);
    snapshots.reserve(units.size());
    for (const auto& record : units.all())
    {
        const auto& unit = record.core;
        snapshots.push_back({ unit.id, unit.motion });
    }
    return snapshots;
}

// Private runFrame() state. Persistent gameplay lives in BattleRuntimeState.
// Anything consumed within one frame belongs here, not in BattleRuntimeState.
// Keep this type private to BattleCore.cpp and do not pass it to subsystem classes.
class BattleFrameContext
{
public:
    static BattleFrameContext begin(
        BattleRuntimeState& state,
        BattlePresentationFrame recycledPresentation,
        std::byte* frameMemoryStorage,
        std::size_t frameMemoryBytes)
    {
        return BattleFrameContext(
            state,
            std::move(recycledPresentation),
            frameMemoryStorage,
            frameMemoryBytes);
    }

    std::vector<BattleAttackSpawnRequest>& currentFrameAttacks() { return attackSpawns_; }
    std::vector<BattlePendingDamageIntent>& currentFrameDamage() { return pendingDamage_; }

    void queueCommand(BattleGameplayCommand command)
    {
        frameCommands_.push_back(std::move(command));
    }

    void queueEffectCommands(
        std::vector<EffectCommand> commands,
        BattleEffectCommandContext context)
    {
        if (commands.empty())
        {
            return;
        }
        effectCommandBatches_.push_back({
            std::move(commands),
            std::move(context),
        });
    }

    std::vector<BattleFrameEffectCommandBatch> drainEffectCommandBatches()
    {
        return std::exchange(effectCommandBatches_, {});
    }

    BattleFrameVector<BattleGameplayCommand> drainCommands()
    {
        return drainFrameVector(frameCommands_);
    }

    std::vector<BattleAttackSpawnRequest> drainCurrentFrameAttacks()
    {
        return std::exchange(attackSpawns_, {});
    }

    std::vector<BattlePendingDamageIntent> drainCurrentFrameDamage()
    {
        return std::exchange(pendingDamage_, {});
    }

    BattleFrameVector<BattleAreaProjectileFollowUp> drainAreaProjectileFollowUps()
    {
        return drainFrameVector(areaProjectileFollowUps_);
    }

    void queueCastCommitBarrier(CastWorkToken barrier)
    {
        assert(barrier.valid());
        castCommitBarriers_.push_back(barrier);
    }

    std::vector<CastWorkToken> drainCastCommitBarriers()
    {
        return std::exchange(castCommitBarriers_, {});
    }

    BattleFrameVector<BattleFrameMpRestore> drainLateMpRestores()
    {
        return drainFrameVector(lateMpRestores_);
    }

    const UnitMotionSnapshotList& frameStartMotion() const { return frameStartMotion_; }
    std::pmr::memory_resource* frameMemoryResource() { return &frameMemoryResource_; }

    BattleFrameVector<BattleGameplayCommand>& mutableCommandsForReducer() { return frameCommands_; }
    BattleFrameVector<BattleAreaProjectileFollowUp>& mutableAreaProjectileFollowUps() { return areaProjectileFollowUps_; }
    BattleFrameVector<BattleFrameMpRestore>& mutableLateMpRestores() { return lateMpRestores_; }

private:
    explicit BattleFrameContext(
        BattleRuntimeState& state,
        BattlePresentationFrame recycledPresentation,
        std::byte* frameMemoryStorage,
        std::size_t frameMemoryBytes)
        : frameMemoryResource_(frameMemoryStorage, frameMemoryBytes)
        , frameCommands_(&frameMemoryResource_)
        , areaProjectileFollowUps_(&frameMemoryResource_)
        , lateMpRestores_(&frameMemoryResource_)
        , attackSpawns_(state.nextFrame.drainAttacks())
        , pendingDamage_(state.nextFrame.drainDamage())
        , frameStartMotion_(makeUnitMotionSnapshot(state.units, &frameMemoryResource_))
        , gameplayEvents(std::move(recycledPresentation.gameplayEvents))
        , logEvents(std::move(recycledPresentation.logEvents))
        , visualEvents(std::move(recycledPresentation.visualEvents))
        , attackSoundIds(std::move(recycledPresentation.attackSoundIds))
        , rumbles(std::move(recycledPresentation.rumbles))
        , attackEvents(&frameMemoryResource_)
    {
        gameplayEvents.clear();
        logEvents.clear();
        visualEvents.clear();
        attackSoundIds.clear();
        rumbles.clear();
    }

    template <typename T>
    BattleFrameVector<T> drainFrameVector(BattleFrameVector<T>& source)
    {
        BattleFrameVector<T> drained(&frameMemoryResource_);
        drained.swap(source);
        return drained;
    }

    std::pmr::monotonic_buffer_resource frameMemoryResource_;
    BattleFrameVector<BattleGameplayCommand> frameCommands_;
    BattleFrameVector<BattleAreaProjectileFollowUp> areaProjectileFollowUps_;
    BattleFrameVector<BattleFrameMpRestore> lateMpRestores_;
    std::vector<BattleAttackSpawnRequest> attackSpawns_;
    std::vector<BattlePendingDamageIntent> pendingDamage_;
    std::vector<CastWorkToken> castCommitBarriers_;
    std::vector<BattleFrameEffectCommandBatch> effectCommandBatches_;
    UnitMotionSnapshotList frameStartMotion_;

public:
    BattlePresentationFrame result;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<int> attackSoundIds;
    std::vector<BattleFrameRumbleEvent> rumbles;
    int blinkSoundCount{};
    BattleFrameVector<BattleAttackEvent> attackEvents;
};

BattlePresentationFrame consumeBattleFrameContext(BattleFrameContext&& frame)
{
    assert(frame.drainCommands().empty());
    return std::move(frame.result);
}

const BattleUnitMotion& motionSnapshotForUnit(
    const UnitMotionSnapshotList& snapshots,
    const BattleRuntimeUnit& fallback)
{
    const auto snapshotIt = std::find_if(
        snapshots.begin(),
        snapshots.end(),
        [&](const UnitMotionSnapshot& snapshot)
        {
            return snapshot.unitId == fallback.id;
        });
    if (snapshotIt != snapshots.end())
    {
        return snapshotIt->motion;
    }
    return fallback.motion;
}

void prepareMovementAgents(BattleRuntimeState& state)
{
    for (auto& record : state.units.all())
    {
        const auto& unit = record.core;
        auto& agent = record.movement;
        agent.active = unit.alive || needsCorpsePhysics(unit);
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
        movementUnit.taXue = runtimeDashAttackEnabled(state, runtimeUnit.id);
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

bool isLastAliveInTeam(const BattleRuntimeUnits& units, const BattleRuntimeUnit& unit)
{
    for (const auto& otherRecord : units.live())
    {
        const auto& other = otherRecord.core;
        if (other.id != unit.id && other.team == unit.team && other.alive)
        {
            return false;
        }
    }
    return true;
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

std::pair<int, int> rescueCellKey(int x, int y)
{
    return { x, y };
}

std::vector<BattleFrameRescueUnitSnapshot> makeRescueUnitSnapshots(BattleRuntimeState& state)
{
    std::vector<BattleFrameRescueUnitSnapshot> snapshots;
    snapshots.reserve(state.units.size());
    for (const auto& unit : state.units.all())
    {
        BattleFrameRescueUnitSnapshot snapshot;
        snapshot.unit.id = unit.id();
        snapshot.unit.team = unit.core.team;
        snapshot.unit.alive = unit.alive();
        snapshot.unit.hp = unit.core.vitals.hp;
        snapshot.unit.maxHp = unit.core.vitals.maxHp;
        snapshot.unit.invincible = unit.core.invincible;
        snapshot.unit.cell = unit.core.grid;
        snapshot.position = unit.core.motion.position;
        snapshot.unit.isSummonedClone = unit.core.cloneSourceUnitId >= 0;
        snapshot.unit.forcePullProtect = unit.forcePullProtectRemaining() > 0;
        snapshot.unit.forcePullExecute = unit.forcePullExecuteRemaining() > 0;
        snapshot.unit.forcePullProtectRemaining = unit.forcePullProtectRemaining();
        snapshot.unit.forcePullExecuteRemaining = unit.forcePullExecuteRemaining();
        snapshot.unit.healModifiers = statusHealModifiers(unit);
        snapshots.push_back(std::move(snapshot));
    }
    return snapshots;
}

std::vector<BattleRescueCellSnapshot> makeRescueCellSnapshots(const BattleRuntimeState& state)
{
    std::map<std::pair<int, int>, int> occupantByCell;
    for (const auto& record : state.units.live())
    {
        const auto& unit = record.core;
        occupantByCell[rescueCellKey(unit.grid.x, unit.grid.y)] = unit.id;
    }

    auto cells = state.rescue.cells;
    for (auto& cell : cells)
    {
        if (!cell.occupied)
        {
            cell.occupantUnitId = -1;
        }
        const auto occupantIt = occupantByCell.find(rescueCellKey(cell.x, cell.y));
        if (occupantIt == occupantByCell.end())
        {
            continue;
        }
        cell.occupied = true;
        cell.occupantUnitId = occupantIt->second;
    }
    return cells;
}

double distance2d(Pointf lhs, Pointf rhs)
{
    return EuclidDis(lhs.x - rhs.x, lhs.y - rhs.y);
}

void refreshCastTarget(BattleCastInput& input, int targetUnitId, Pointf targetPosition)
{
    input.targetUnitId = targetUnitId;
    input.targetPosition = targetPosition;
    input.targetDistance = distance2d(input.unit.position, targetPosition);
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
        const auto resources = snapshotEffectResourcesBeforeCast(state);
        const auto matches = queryExactRuntimeRules(
            state,
            input.unit.id,
            EffectEvent::CastPlanned,
            makeCastPlanEventData(input, true, resources, provenance));
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

const BattleCastSkillState& selectedCastSkill(const BattleCastInput& input, bool ultimate)
{
    return ultimate ? input.ultimateSkill : input.normalSkill;
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
    const auto& skill = selectedCastSkill(input, ultimate);
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

BattleCastSkillState makePendingCastSkillPlan(BattleCastSkillState skill)
{
    skill.id = -1;
    return skill;
}

BattleCastSkillState materializePendingCastSkill(const BattlePendingCastAction& pending)
{
    assert(pending.effectCast.provenance.valid());
    assert(pending.skillPlan.id == -1);
    auto skill = pending.skillPlan;
    skill.id = pending.effectCast.provenance.magicId;
    return skill;
}

BattlePendingCastAction makePendingCastAction(const BattleCastInput& castInput,
                                              const BattleCastResult& cast,
                                              const BattleCastStart& trackedCast,
                                              int castFrame);
BattleActionCommitInput makeCommittedCastActionInput(BattleRuntimeState& state,
                                                     const BattleRuntimeUnit& unit,
                                                     const BattleCastInput& castInput,
                                                     const BattleCastSkillState& selectedSkill,
                                                     const BattleCastResult& cast,
                                                     const BattleCastProvenance& provenance,
                                                     const BattleEffectCastPreparation& effectPreparation,
                                                     const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast);
std::optional<BattleActionCommitInput> tryMakeRuntimeActionCommitInput(BattleRuntimeState& state,
                                                                        const BattleTickResult& movement,
                                                                        const BattlePendingCastAction& pending,
                                                                        std::pmr::memory_resource* frameMemoryResource);
BattleBlinkGeometryInput makeRuntimeBlinkGeometry(const BattleRuntimeState& state,
                                                  const BattleRuntimeUnit& unit,
                                                  double reach);

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
    pending.skillPlan = makePendingCastSkillPlan(selectedCastSkill(castInput, cast));
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
    auto castInput = tryMakeRuntimeCastInputForPendingCast(
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
            makeCastPlanEventData(
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
    applyEffectAttackDirectives(cast.attackSpawnRequests, preparedEffects);
    auto plannedAttackEffects = BattleEffectAttackCastSystem().applyAttackCommands(
        *castInput,
        cast,
        pending.plannedAttackEffectCommands,
        effectAttackState);
    applyEffectAttackDirectives(cast.attackSpawnRequests, plannedAttackEffects);
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

std::string toStatusText(const BattleDamageEvent& event)
{
    switch (event.type)
    {
    case BattleDamageEventType::BlockedByDualWield:
        return "互搏抵擋了本次傷害";
    default:
        break;
    }

    auto withValue = [&](const char* label)
    {
        return event.value > 0
            ? std::format("{}（{}）", label, event.value)
            : std::string(label);
    };
    switch (event.statusType)
    {
    case BattleDamageStatusType::Hitstun:
        return withValue("受擊硬直");
    case BattleDamageStatusType::Stun:
        return withValue("眩暈");
    case BattleDamageStatusType::Poison:
        return withValue("中毒");
    case BattleDamageStatusType::Bleed:
        return withValue("流血");
    case BattleDamageStatusType::MpBlocked:
        return withValue("封內");
    case BattleDamageStatusType::None:
        return "狀態";
    }
    assert(false);
    return "狀態";
}

BattleGameplayEvent toGameplayEvent(const BattleDamageEvent& event)
{
    BattleGameplayEvent gameplay;
    gameplay.sourceUnitId = event.sourceUnitId;
    gameplay.targetUnitId = event.targetUnitId;
    gameplay.amount = event.value;
    switch (event.type)
    {
    case BattleDamageEventType::DamageApplied:
        gameplay.resourceId = BattleResourceSemanticId::HitPoints;
        gameplay.type = BattleGameplayEventType::DamageApplied;
        break;
    case BattleDamageEventType::MpDamageApplied:
        gameplay.resourceId = BattleResourceSemanticId::MagicPoints;
        gameplay.type = BattleGameplayEventType::DamageApplied;
        break;
    case BattleDamageEventType::ShieldAbsorbed:
        gameplay.resourceId = BattleResourceSemanticId::Shield;
        gameplay.type = BattleGameplayEventType::DamageApplied;
        break;
    case BattleDamageEventType::UnitDied:
        gameplay.type = BattleGameplayEventType::UnitDied;
        break;
    case BattleDamageEventType::StatusApplied:
        gameplay.type = BattleGameplayEventType::StatusApplied;
        gameplay.statusId = static_cast<BattleStatusSemanticId>(static_cast<int>(event.statusType));
        gameplay.text = toStatusText(event);
        break;
    case BattleDamageEventType::HpRestored:
        gameplay.resourceId = BattleResourceSemanticId::HitPoints;
        gameplay.type = BattleGameplayEventType::ResourceChanged;
        break;
    case BattleDamageEventType::MpRestored:
    case BattleDamageEventType::MpDrained:
        gameplay.resourceId = BattleResourceSemanticId::MagicPoints;
        gameplay.type = BattleGameplayEventType::ResourceChanged;
        break;
    case BattleDamageEventType::CooldownExtended:
        gameplay.resourceId = BattleResourceSemanticId::Cooldown;
        gameplay.type = BattleGameplayEventType::ResourceChanged;
        break;
    case BattleDamageEventType::BlockedByInvincible:
        gameplay.statusId = BattleStatusSemanticId::BlockedByInvincible;
        gameplay.type = BattleGameplayEventType::StatusApplied;
        gameplay.text = toStatusText(event);
        break;
    case BattleDamageEventType::BlockedByDualWield:
        gameplay.statusId = BattleStatusSemanticId::BlockedByDualWield;
        gameplay.type = BattleGameplayEventType::StatusApplied;
        gameplay.text = toStatusText(event);
        break;
    case BattleDamageEventType::DeathPrevented:
        gameplay.statusId = BattleStatusSemanticId::DeathPrevented;
        gameplay.type = BattleGameplayEventType::StatusApplied;
        gameplay.text = toStatusText(event);
        break;
    case BattleDamageEventType::ExecuteTriggered:
        gameplay.statusId = BattleStatusSemanticId::ExecuteTriggered;
        gameplay.type = BattleGameplayEventType::StatusApplied;
        gameplay.text = toStatusText(event);
        break;
    }
    return gameplay;
}

BattleDamageUnitState makeBattleDamageUnitStateFromRuntime(
    const BattleRuntimeUnit& unit,
    const BattleDamageRuntimeUnit* runtime)
{
    BattleDamageUnitState damage;
    damage.id = unit.id;
    damage.alive = unit.alive;
    damage.vitals = unit.vitals;
    damage.attack = unit.stats.attack;
    damage.invincible = unit.invincible;
    damage.shield = unit.shield;
    if (runtime)
    {
        damage.hurtInvincFrames = runtime->hurtInvincFrames;
        damage.dualWieldBlocksRemaining = runtime->dualWieldBlocksRemaining;
        damage.deathPrevention = runtime->deathPrevention;
        damage.deathPreventionUsed = runtime->deathPreventionUsed;
        damage.deathPreventionFrames = runtime->deathPreventionFrames;
    }
    return damage;
}

void writeBattleDamageRuntimeUnitImpl(BattleDamageRuntimeUnit& runtime, const BattleDamageUnitState& unit)
{
    runtime.hurtInvincFrames = unit.hurtInvincFrames;
    runtime.dualWieldBlocksRemaining = unit.dualWieldBlocksRemaining;
    runtime.deathPrevention = unit.deathPrevention;
    runtime.deathPreventionUsed = unit.deathPreventionUsed;
    runtime.deathPreventionFrames = unit.deathPreventionFrames;
}

BattleCooldownState makeBattleFrameCooldownStateImpl(const BattleRuntimeUnit& unit)
{
    BattleCooldownState cooldown;
    cooldown.alive = unit.alive;
    cooldown.cooldown = unit.animation.cooldown;
    cooldown.cooldownMax = unit.animation.cooldownMax;
    cooldown.haveAction = unit.haveAction;
    cooldown.operationType = unit.operationType;
    cooldown.actType = unit.animation.actType;
    return cooldown;
}

void commitDamageUnitCoreToRuntime(BattleRuntimeState& state, const BattleDamageUnitState& unit)
{
    state.units.writeDamageUnit(unit);
    state.units.require(unit.id).writeDamageResult(unit);
}

void commitDamageDefenderStatusToRuntime(
    BattleRuntimeState& state,
    const BattleDamageTransactionResult& transaction)
{
    state.units.require(transaction.defender.id).writeStatusDamageResult(transaction.defenderStatus);
}

void commitDamageCooldownToRuntime(BattleRuntimeState& state, const BattleDamageTransactionResult& transaction)
{
    auto& unit = state.units.requireCore(transaction.defender.id);
    unit.animation.cooldown = transaction.defenderCooldown.cooldown;
    unit.animation.cooldownMax = transaction.defenderCooldown.cooldownMax;
}

void applyDamageResultToFrameState(
    BattleRuntimeState& state,
    const BattleDamageTransactionResult& transaction,
    const UnitMotionSnapshotList& frameStartMotion)
{
    const auto& preDamageDefender = state.units.requireCore(transaction.defender.id);
    const auto& defenderStartMotion = motionSnapshotForUnit(frameStartMotion, preDamageDefender);
    Pointf preDamageDeathKickDirection = { 1, 0, 0 };
    if (transaction.attacker.id != OptionalDamageAttackerUnitId)
    {
        assert(transaction.attacker.id >= 0);
        const auto& attacker = state.units.requireCore(transaction.attacker.id);
        if (attacker.id != preDamageDefender.id)
        {
            preDamageDeathKickDirection =
                defenderStartMotion.position - motionSnapshotForUnit(frameStartMotion, attacker).position;
        }
    }
    if (transaction.attacker.id != OptionalDamageAttackerUnitId)
    {
        commitDamageUnitCoreToRuntime(state, transaction.attacker);
    }
    commitDamageUnitCoreToRuntime(state, transaction.defender);
    commitDamageCooldownToRuntime(state, transaction);
    auto& unit = state.units.requireCore(transaction.defender.id);
    if (transaction.killed)
    {
        unit.motion = defenderStartMotion;
        unit.motion.position.z += DeathKickImpactHeight;
        unit.motion.velocity = deathKickVelocity(
            preDamageDeathKickDirection,
            transaction.finalHpDamage);
        unit.motion.acceleration = { 0, 0, state.movementPhysics.config.gravity };
        auto& agent = state.units.require(unit.id).movement;
        agent.physics.position = unit.motion.position;
        agent.physics.velocity = unit.motion.velocity;
        agent.physics.acceleration = unit.motion.acceleration;
    }
    commitDamageDefenderStatusToRuntime(state, transaction);
    if (transaction.killed)
    {
        state.units.require(unit.id).clearFrozen();
    }
}

void applyRescueDamageToRuntimeUnit(BattleRuntimeState& state, int unitId, int hp, int invincible)
{
    auto& unit = state.units.requireCore(unitId);
    unit.vitals.hp = hp;
    unit.invincible = invincible;
}

void applyRescuePositionToRuntimeUnit(BattleRuntimeState& state, int unitId, Pointf position)
{
    state.units.setPosition(unitId, position, state.gridTransform);
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

BattleRescueRepositionInput makeRescueInput(
    const BattleRuntimeState& state,
    const std::vector<BattleFrameRescueUnitSnapshot>& units,
    BattleRescuePullMode mode,
    int pulledUnitId,
    int pullerTeam)
{
    BattleRescueRepositionInput input;
    input.mode = mode;
    input.pulledUnitId = pulledUnitId;
    input.pullerTeam = pullerTeam;
    input.cells = makeRescueCellSnapshots(state);
    input.units.reserve(units.size());
    for (const auto& unit : units)
    {
        input.units.push_back(unit.unit);
    }
    return input;
}

bool rescueUnitUnattendedByTeam(
    const BattleRuntimeState& state,
    const std::vector<BattleFrameRescueUnitSnapshot>& units,
    int targetUnitId,
    int team)
{
    assert(state.rescue.executeUnattendedRadius > 0.0);
    const auto& target = requireBy(units, targetUnitId, rescueSnapshotUnitId);
    for (const auto& unit : units)
    {
        if (!unit.unit.alive || unit.unit.team != team)
        {
            continue;
        }
        if (distance2d(unit.position, target.position) <= state.rescue.executeUnattendedRadius)
        {
            return false;
        }
    }
    return true;
}

BattleAttackSpawnRequest makeRescueCounterAttackSpawn(
    BattleRuntimeState& state,
    const BattleRescueBasicCounterAttackCommand& command)
{
    const auto& config = state.rescue.counterAttack;
    assert(config.skillId >= 0);
    assert(config.visualEffectId >= 0);
    assert(config.projectileSpeed > 0.0);
    assert(config.meleeAttackEffectOffset > 0.0);

    const auto& attackerUnit = state.units.requireCore(command.attackerUnitId);
    const auto& targetUnit = state.units.requireCore(command.targetUnitId);

    auto direction = targetUnit.motion.position - attackerUnit.motion.position;
    if (direction.norm() <= 0.01)
    {
        direction = { 1, 0, 0 };
    }
    direction.normTo(1);

    BattleAttackSpawnRequest request;
    request.initial.attackSourceUnitId = command.attackerUnitId;
    request.initial.skillId = config.skillId;
    request.initial.preferredTargetUnitId = command.targetUnitId;
    request.initial.requirePreferredTarget = true;
    request.initial.track = true;
    request.initial.operationType = BattleOperationType::Melee;
    request.initial.visualEffectId = config.visualEffectId;
    request.initial.position = attackerUnit.motion.position;
    request.initial.position.x += static_cast<float>(config.meleeAttackEffectOffset) * direction.x;
    request.initial.position.y += static_cast<float>(config.meleeAttackEffectOffset) * direction.y;
    request.initial.position.z += static_cast<float>(config.meleeAttackEffectOffset) * direction.z;
    request.initial.velocity = targetUnit.motion.position - request.initial.position;
    if (request.initial.velocity.norm() <= 0.01)
    {
        request.initial.velocity = direction;
    }
    request.initial.velocity.normTo(static_cast<float>(config.projectileSpeed));
    request.initial.totalFrame = std::max(
        config.minimumTotalFrames,
        battleTravelFrames2d(request.initial.position, targetUnit.motion.position, config.projectileSpeed)
            + config.totalFramePadding);

    const auto cast = state.castLifecycle.beginRootCast({
        .sourceUnitId = command.attackerUnitId,
        .magicId = config.skillId,
        .ultimate = false,
        .origin = CastOriginKind::RescueCounter,
        .propagation = CastPropagationPolicy::NoEffectRules,
    });
    const auto attack = state.castLifecycle.reserveAttack(
        cast.provenance.castId,
        {
            .origin = BattleAttackOriginKind::Scripted,
            .rootAttack = true,
            .mainProjectile = false,
            .propagation = CastPropagationPolicy::NoEffectRules,
        });
    request.provenance = attack.provenance;
    request.castWork = attack.work;
    state.castLifecycle.completeWork(cast.commitBarrier);
    return request;
}

void commitRescueResultToRuntime(
    BattleRuntimeState& state,
    const BattleRescueRepositionResult& result,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    assert(result.teleport.has_value());

    auto& pulled = state.units.requireCore(result.teleport->unitId);
    pulled.grid = result.teleport->destinationCell;
    applyRescuePositionToRuntimeUnit(state, pulled.id, result.teleport->destinationPosition);

    if (result.counterDelta.unitId >= 0)
    {
        state.units.require(result.counterDelta.unitId).applyRescueCounterDelta(result.counterDelta);
    }

    int appliedHeal{};
    if (result.heal.request)
    {
        assert(result.heal.targetUnitId == pulled.id);
        auto heal = BattleHealSystem().commit(state, *result.heal.request);
        appliedHeal = heal.appliedAmount;
        if (appliedHeal > 0)
        {
            visualEvents.push_back(roleEffectEvent(
                pulled.id,
                KysChess::EFT_HEAL,
                CoreRoleStatusEffectFrames));
        }
    }
    if (result.invincibility.frames > 0)
    {
        assert(result.invincibility.targetUnitId == pulled.id);
        pulled.invincible += result.invincibility.frames;
    }
    applyRescueDamageToRuntimeUnit(state, pulled.id, pulled.vitals.hp, pulled.invincible);

    if (result.basicCounterAttack)
    {
        state.nextFrame.queueAttack(makeRescueCounterAttackSpawn(state, *result.basicCounterAttack));
    }
    visualEvents.insert(
        visualEvents.end(),
        result.visualEvents.begin(),
        result.visualEvents.end());
    for (auto log : result.logEvents)
    {
        if (log.type == BattleLogEventType::Heal)
        {
            if (appliedHeal <= 0)
            {
                continue;
            }
            log.amount = appliedHeal;
        }
        logEvents.push_back(std::move(log));
    }
}

bool tryApplyRescue(
    BattleRuntimeState& state,
    const std::vector<BattleFrameRescueUnitSnapshot>& units,
    BattleRescuePullMode mode,
    int pulledUnitId,
    int pullerTeam,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    auto rescue = BattleRescueRepositionSystem().resolve(
        makeRescueInput(state, units, mode, pulledUnitId, pullerTeam));
    if (!rescue.teleport)
    {
        return false;
    }
    commitRescueResultToRuntime(state, rescue, logEvents, visualEvents);
    return true;
}

int hpBeforeDamage(const BattleDamageTransactionResult& transaction)
{
    return transaction.defender.vitals.hp - transaction.defenderDelta.hpDelta;
}

bool damageCrossedHpPctThreshold(
    const BattleDamageTransactionResult& transaction,
    int thresholdPct)
{
    assert(thresholdPct > 0);
    return hpBeforeDamage(transaction) * 100 > transaction.defender.vitals.maxHp * thresholdPct
        && transaction.defender.vitals.hp * 100 <= transaction.defender.vitals.maxHp * thresholdPct;
}

bool rescuePullerAvailable(
    const BattleRuntimeState& state,
    BattleRescuePullMode mode,
    int pullerTeam)
{
    for (const auto& record : state.units.all())
    {
        if (!record.core.alive || record.core.team != pullerTeam || record.core.cloneSourceUnitId >= 0)
        {
            continue;
        }

        if (mode == BattleRescuePullMode::Execute)
        {
            if (record.forcePullExecuteRemaining() > 0)
            {
                return true;
            }
        }
        else if (record.forcePullProtectRemaining() > 0)
        {
            return true;
        }
    }
    return false;
}

void applyRescueRepositionForDamage(
    BattleRuntimeState& state,
    const BattleDamageTransactionResult& transaction,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    if (state.units.empty() || transaction.defender.vitals.maxHp <= 0 || !transaction.defender.alive)
    {
        return;
    }

    const bool crossedProtectThreshold = damageCrossedHpPctThreshold(transaction, 25);
    const bool crossedExecuteThreshold = state.rescue.executeUnattendedRadius > 0.0
        && damageCrossedHpPctThreshold(transaction, 15);
    if (!crossedProtectThreshold && !crossedExecuteThreshold)
    {
        return;
    }

    const auto& currentPulledBeforeRescue = state.units.requireCore(transaction.defender.id);
    const auto* attacker = transaction.attacker.id == OptionalDamageAttackerUnitId
        ? nullptr
        : &state.units.requireCore(transaction.attacker.id);
    const bool canProtect = crossedProtectThreshold
        && attacker
        && attacker->alive
        && attacker->team != currentPulledBeforeRescue.team;
    const bool protectPullerAvailable = canProtect
        && rescuePullerAvailable(state, BattleRescuePullMode::Protect, currentPulledBeforeRescue.team);
    const int executePullerTeam = 1 - currentPulledBeforeRescue.team;
    const bool executePullerAvailable = crossedExecuteThreshold
        && rescuePullerAvailable(state, BattleRescuePullMode::Execute, executePullerTeam);
    if (!protectPullerAvailable && !executePullerAvailable)
    {
        return;
    }

    auto rescueUnits = makeRescueUnitSnapshots(state);
    if (protectPullerAvailable)
    {
        const bool rescued = tryApplyRescue(state,
                                            rescueUnits,
                                            BattleRescuePullMode::Protect,
                                            transaction.defender.id,
                                            currentPulledBeforeRescue.team,
                                            logEvents,
                                            visualEvents);
        if (rescued)
        {
            rescueUnits = makeRescueUnitSnapshots(state);
        }
    }

    const auto& currentPulled = state.units.requireCore(transaction.defender.id);
    if (executePullerAvailable
        && currentPulled.vitals.hp * 100 <= transaction.defender.vitals.maxHp * 15
        && rescueUnitUnattendedByTeam(state, rescueUnits, transaction.defender.id, executePullerTeam))
    {
        tryApplyRescue(state,
                       rescueUnits,
                       BattleRescuePullMode::Execute,
                       transaction.defender.id,
                       executePullerTeam,
                       logEvents,
                       visualEvents);
    }
}

void appendFramePendingDamage(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    BattleDamageRequest request,
    std::optional<BattleDamagePresentationInput> presentation = std::nullopt,
    int executeThresholdPct = 0,
    bool canTriggerDefenderBlock = false,
    BattleAttackProvenance provenance = {},
    std::optional<EffectDamageOrigin> effectOrigin = std::nullopt,
    CastWorkToken delayedCastWork = {})
{
    assert(request.defenderUnitId >= 0);

    state.units.requireCore(request.defenderUnitId);
    if (request.attackerUnitId >= 0)
    {
        state.units.requireCore(request.attackerUnitId);
    }

    BattlePendingDamageIntent intent;
    intent.request = std::move(request);
    if (presentation)
    {
        intent.presentation = std::move(*presentation);
    }
    intent.executeThresholdPct = executeThresholdPct;
    intent.canTriggerDefenderBlock = canTriggerDefenderBlock;
    intent.provenance = std::move(provenance);
    intent.delayedCastWork = delayedCastWork;
    if (intent.delayedCastWork.valid())
    {
        assert(!intent.provenance.valid());
    }
    if (effectOrigin)
    {
        intent.effectOrigin = std::move(*effectOrigin);
    }
    else if (intent.provenance.valid())
    {
        intent.effectOrigin = EffectAttackDamageOrigin{ intent.provenance };
    }
    pendingDamage.push_back(std::move(intent));
}

void applyFrameDamagePresentationStyle(
    const BattleRuntimeState& state,
    int targetUnitId,
    BattleDamagePresentationInput& presentation)
{
    const auto styleIt = state.damage.presentationStylesByDefender.find(targetUnitId);
    if (styleIt == state.damage.presentationStylesByDefender.end())
    {
        return;
    }

    presentation.enabled = true;
    presentation.normalDamageColor = styleIt->second.normalDamageColor;
    presentation.emphasizedDamageColor = styleIt->second.emphasizedDamageColor;
    presentation.executeTextColor = styleIt->second.executeTextColor;
    presentation.normalDamageTextSize = styleIt->second.normalDamageTextSize;
    presentation.emphasizedDamageTextSize = styleIt->second.emphasizedDamageTextSize;
    presentation.executeTextSize = styleIt->second.executeTextSize;
}

BattlePresentationColor statusTickDamageTextColor(BattleStatusEventType type)
{
    switch (type)
    {
    case BattleStatusEventType::PoisonDamage:
        return { 0, 200, 0, 255 };
    case BattleStatusEventType::BleedDamage:
        return { 190, 120, 60, 255 };
    default:
        assert(false);
        return {};
    }
}

void applyStatusTickDamagePresentation(
    const BattleRuntimeState& state,
    BattleStatusEventType type,
    int targetUnitId,
    BattleDamagePresentationInput& presentation)
{
    applyFrameDamagePresentationStyle(state, targetUnitId, presentation);
    presentation.enabled = true;
    presentation.normalDamageColor = statusTickDamageTextColor(type);
    presentation.emphasizedDamageColor = presentation.normalDamageColor;
    if (presentation.normalDamageTextSize <= 0)
    {
        presentation.normalDamageTextSize = 30;
    }
    if (presentation.emphasizedDamageTextSize <= 0)
    {
        presentation.emphasizedDamageTextSize = 44;
    }
}

BattleDamagePresentationInput makeFrameDamagePresentation(
    const BattleRuntimeState& state,
    const BattleHpDamageCommand& command)
{
    BattleDamagePresentationInput presentation;
    presentation.critical = command.critical;
    presentation.criticalMultiplier = command.criticalMultiplier;
    presentation.ultimate = command.provenance.valid()
        && command.provenance.cast.ultimate;
    presentation.skillName = command.skillName;
    presentation.skillId = command.skillId;
    presentation.segments = command.segments;
    applyFrameDamagePresentationStyle(state, command.targetUnitId, presentation);
    return presentation;
}

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleHpDamageCommand& command)
{
    BattleDamageRequest request;
    request.attackerUnitId = command.sourceUnitId;
    request.defenderUnitId = command.targetUnitId;
    request.baseDamage = command.damage;
    request.damageKind = command.damageKind;
    request.preResolvedDamage = true;
    request.preResolvedDamageReductionBasisPoints =
        command.combinedDamageReductionBasisPoints;
    request.hitstunFrames = command.frozenFrames;
    request.triggersDefenseEffects = command.triggersDefenseEffects;

    appendFramePendingDamage(
        state,
        pendingDamage,
        std::move(request),
        makeFrameDamagePresentation(state, command),
        command.executeThresholdPct,
        command.canTriggerDefenderBlock,
        command.provenance);
    return true;
}

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleMpDamageCommand& command)
{
    auto request = command.damage;
    request.attackerUnitId = command.sourceUnitId;
    request.defenderUnitId = command.targetUnitId;

    appendFramePendingDamage(
        state,
        pendingDamage,
        std::move(request),
        std::nullopt,
        false,
        command.canTriggerDefenderBlock,
        command.provenance);
    return true;
}

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleAcceptedHitSideEffectCommand& command)
{
    auto request = command.damage;
    request.attackerUnitId = command.sourceUnitId;
    request.defenderUnitId = command.targetUnitId;
    request.acceptedHit = true;

    appendFramePendingDamage(
        state,
        pendingDamage,
        std::move(request),
        std::nullopt,
        false,
        false,
        command.provenance);
    return true;
}

void appendStatusEventLog(
    std::vector<BattleLogEvent>& logEvents,
    int sourceUnitId,
    int targetUnitId,
    std::string text,
    BattleStatusSemanticId statusId = BattleStatusSemanticId::None,
    BattleResourceSemanticId resourceId = BattleResourceSemanticId::None,
    int amount = 0)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Status;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.amount = amount;
    event.statusId = statusId;
    event.resourceId = resourceId;
    event.segments = battleLogText(std::move(text), BattleLogTextTone::SkillName);
    logEvents.push_back(std::move(event));
}

void appendHealEventLog(
    std::vector<BattleLogEvent>& logEvents,
    int sourceUnitId,
    int targetUnitId,
    int amount,
    std::string text,
    BattleResourceSemanticId resourceId = BattleResourceSemanticId::HitPoints)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Heal;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.amount = amount;
    event.resourceId = resourceId;
    event.segments = battleLogText(std::move(text), BattleLogTextTone::SkillName);
    logEvents.push_back(std::move(event));
}

void appendPoisonEffectLogEvents(
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    const ApplyStatusEffectCommand& command,
    const BattleStatusApplyEffectResult& result)
{
    if (command.action.status != BattleStatusKind::Poison)
    {
        return;
    }

    BattleLogEvent payload;
    payload.type = BattleLogEventType::Status;
    payload.sourceUnitId = metadata.binding.ownerUnitId;
    payload.targetUnitId = metadata.targetUnitId;
    payload.amount = command.potency;
    payload.secondaryAmount = command.action.stacks;
    payload.statusId = BattleStatusSemanticId::PoisonPayload;
    payload.segments = battleLogText(
        std::format("中毒負載{}%（預定{}次）", command.potency, command.action.stacks),
        BattleLogTextTone::Negative);
    logEvents.push_back(std::move(payload));

    if (!result.status.applied)
    {
        return;
    }

    BattleLogEvent applied;
    applied.type = BattleLogEventType::Status;
    applied.sourceUnitId = metadata.binding.ownerUnitId;
    applied.targetUnitId = metadata.targetUnitId;
    applied.amount = command.potency;
    applied.statusId = BattleStatusSemanticId::Poison;
    applied.segments = battleLogText(
        std::format("中毒{}%", command.potency),
        BattleLogTextTone::Negative);
    logEvents.push_back(std::move(applied));
}

void appendMpResourceEffectLogEvents(
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    const ChangeResourceEffectCommand& command,
    const BattleResourceEffectResult& result)
{
    if (command.action.resource != BattleResource::Mp
        || (command.action.kind != ResourceChangeKind::Drain
            && command.action.kind != ResourceChangeKind::Transfer))
    {
        return;
    }

    int removed{};
    for (const auto& delta : result.deltas)
    {
        if (delta.unitId == metadata.targetUnitId && delta.after < delta.before)
        {
            removed += delta.before - delta.after;
        }
    }
    if (removed > 0)
    {
        appendStatusEventLog(
            logEvents,
            metadata.binding.ownerUnitId,
            metadata.targetUnitId,
            command.action.kind == ResourceChangeKind::Drain ? "吸取內力" : "轉移內力",
            BattleStatusSemanticId::MagicPointsDrained,
            BattleResourceSemanticId::MagicPoints,
            removed);
    }

    for (const auto& delta : result.deltas)
    {
        const int restored = delta.after - delta.before;
        if (restored <= 0)
        {
            continue;
        }
        appendHealEventLog(
            logEvents,
            metadata.binding.ownerUnitId,
            delta.unitId,
            restored,
            command.action.kind == ResourceChangeKind::Drain ? "吸取內力" : "轉移內力",
            BattleResourceSemanticId::MagicPoints);
    }
}

bool applyFrameMpRestore(
    BattleRuntimeState& state,
    int unitId,
    int amount,
    const std::string& reason,
    std::vector<BattleLogEvent>& logEvents)
{
    auto& unit = state.units.requireCore(unitId);

    const int restored = std::min(amount, std::max(0, unit.vitals.maxMp - unit.vitals.mp));
    if (restored <= 0)
    {
        return true;
    }

    unit.vitals.mp += restored;
    appendStatusEventLog(logEvents, unitId, unitId, reason);
    return true;
}

struct BattleCommandSinks
{
    std::vector<BattleAttackSpawnRequest>& attackSpawns;
    std::vector<BattlePendingDamageIntent>& pendingDamage;
    std::vector<BattleGameplayEvent>& gameplayEvents;
    std::vector<BattleLogEvent>& logEvents;
    std::vector<BattleVisualEvent>& visualEvents;
};

BattleCommandSinks currentFrameSinks(BattleFrameContext& frame)
{
    return {
        frame.currentFrameAttacks(),
        frame.currentFrameDamage(),
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents,
    };
}

BattleCommandSinks afterAttackHitSinks(BattleRuntimeState& state, BattleFrameContext& frame)
{
    return {
        state.nextFrame.mutableAttacksForReducer(),
        frame.currentFrameDamage(),
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents,
    };
}

BattleCommandSinks afterDamageLifecycleSinks(BattleRuntimeState& state, BattleFrameContext& frame)
{
    return {
        state.nextFrame.mutableAttacksForReducer(),
        state.nextFrame.mutableDamageForReducer(),
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents,
    };
}

CastPropagationPolicy derivedAttackPropagation(
    const BattleAttackProvenance& sourceAttack)
{
    assert(sourceAttack.valid());
    return sourceAttack.propagation == CastPropagationPolicy::SourceRules
        ? CastPropagationPolicy::SourceHitRulesOnly
        : sourceAttack.propagation;
}

void reserveTrackedProjectileFollowUps(
    BattleRuntimeState& state,
    BattleProjectileFollowUpExpansion& expansion)
{
    for (auto& command : expansion.commands)
    {
        auto* projectile = std::get_if<BattleProjectileSpawnCommand>(&command);
        if (!projectile)
        {
            continue;
        }
        auto& request = projectile->request;
        if (request.provenance.valid())
        {
            assert(request.castWork.valid());
            continue;
        }
        assert(!request.castWork.valid());
        assert(projectile->sourceAttack);
        const auto& sourceAttack = *projectile->sourceAttack;
        assert(sourceAttack.valid());
        const auto reservation = state.castLifecycle.reserveAttack(
            sourceAttack.cast.castId,
            {
                .parentAttackId = sourceAttack.attackId,
                .origin = BattleAttackOriginKind::FollowUp,
                .rootAttack = false,
                .mainProjectile = false,
                .sharedHitGroupId = sourceAttack.sharedHitGroupId,
                .propagation = derivedAttackPropagation(sourceAttack),
            });
        request.provenance = reservation.provenance;
        request.castWork = reservation.work;
    }
}

bool reduceFrameGameplayCommand(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    BattleGameplayCommand& command,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    BattleFrameVector<BattleGameplayCommand>& pending,
    BattleCommandSinks sinks)
{
    if (const auto* hp = std::get_if<BattleHpDamageCommand>(&command))
    {
        return tryAppendFrameDamageTransaction(state, sinks.pendingDamage, *hp);
    }
    if (const auto* mp = std::get_if<BattleMpDamageCommand>(&command))
    {
        return tryAppendFrameDamageTransaction(state, sinks.pendingDamage, *mp);
    }
    if (const auto* sideEffect = std::get_if<BattleAcceptedHitSideEffectCommand>(&command))
    {
        return tryAppendFrameDamageTransaction(state, sinks.pendingDamage, *sideEffect);
    }
    if (auto* projectile = std::get_if<BattleProjectileSpawnCommand>(&command))
    {
        assert(projectile->request.provenance.valid());
        assert(projectile->request.castWork.valid());
        sinks.attackSpawns.push_back(std::move(projectile->request));
        return true;
    }
    if (std::holds_alternative<BattleNearbyTrackingProjectilesCommand>(command))
    {
        auto followUps = expandBattleProjectileFollowUpCommands(
            std::span(&command, 1),
            state.projectileFollowUps,
            state.units);
        reserveTrackedProjectileFollowUps(state, followUps);
        pending.insert(
            pending.end(),
            std::make_move_iterator(followUps.commands.begin()),
            std::make_move_iterator(followUps.commands.end()));
        sinks.visualEvents.insert(
            sinks.visualEvents.end(),
            std::make_move_iterator(followUps.visualEvents.begin()),
            std::make_move_iterator(followUps.visualEvents.end()));
        return true;
    }
    if (const auto* autoUltimate = std::get_if<BattleAutoUltimateCommand>(&command))
    {
        return tryCommitAutoUltimate(
            state,
            frame,
            autoUltimate->unitId,
            autoUltimate->consumeMp,
            autoUltimate->announce,
            pending.get_allocator().resource(),
            attackSoundIds,
            sinks.attackSpawns,
            sinks.gameplayEvents,
            sinks.logEvents,
            sinks.visualEvents);
    }
    if (const auto* knockback = std::get_if<BattleKnockbackCommand>(&command))
    {
        applyKnockbackImpulse(state, *knockback);
        return true;
    }
    if (const auto* rumble = std::get_if<BattleRumbleCommand>(&command))
    {
        rumbles.push_back({
            rumble->lowFrequency,
            rumble->highFrequency,
            rumble->durationMs,
        });
        return true;
    }
    assert(false);
    return false;
}

void reduceFrameGameplayCommandsImpl(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    BattleFrameVector<BattleGameplayCommand>& commands,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    BattleCommandSinks sinks)
{
    BattleFrameVector<BattleGameplayCommand> pending = std::move(commands);
    BattleFrameVector<BattleGameplayCommand> unreduced(commands.get_allocator().resource());
    for (std::size_t i = 0; i < pending.size(); ++i)
    {
        if (!reduceFrameGameplayCommand(
            state,
            frame,
            pending[i],
            attackSoundIds,
            rumbles,
            pending,
            sinks))
        {
            unreduced.push_back(std::move(pending[i]));
        }
    }
    commands = std::move(unreduced);
}

void reduceCommandsBeforeMovement(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        currentFrameSinks(frame));
}

void reduceCommandsBeforeAttacks(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        currentFrameSinks(frame));
}

void reduceCommandsAfterAttackHits(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        afterAttackHitSinks(state, frame));
}

void reduceCommandsAfterDamageLifecycle(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        afterDamageLifecycleSinks(state, frame));
}

void completeCastCommitBarriers(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    for (CastWorkToken barrier : frame.drainCastCommitBarriers())
    {
        state.castLifecycle.completeWork(barrier);
    }
}

DamageChannel effectDamageChannel(BattleDamageKind kind)
{
    switch (kind)
    {
    case BattleDamageKind::Physical:
    case BattleDamageKind::Skill:
        return DamageChannel::Skill;
    case BattleDamageKind::Poison:
    case BattleDamageKind::Bleed:
        return DamageChannel::Dot;
    case BattleDamageKind::Reflected:
        return DamageChannel::Reflected;
    case BattleDamageKind::Pure:
    case BattleDamageKind::Effect:
    case BattleDamageKind::Execute:
        return DamageChannel::Effect;
    }
    assert(false);
    return DamageChannel::All;
}

std::vector<int> effectDamageTargetIds(
    const BattleRuntimeState& state,
    const BattleEffectDamageRequestOutput& output)
{
    const auto& center = state.units.requireCore(output.request.defenderUnitId);
    if (output.action.area.kind == DamageAreaKind::SingleTarget)
    {
        return { center.id };
    }

    std::vector<int> result;
    for (const auto& record : state.units.live())
    {
        const auto& candidate = record.core;
        if (candidate.team == output.source.sourceTeam)
        {
            continue;
        }

        bool inside = false;
        switch (output.action.area.kind)
        {
        case DamageAreaKind::SingleTarget:
            inside = candidate.id == center.id;
            break;
        case DamageAreaKind::Circle:
        {
            const double radius = output.action.area.radiusTiles * state.gridTransform.tileWidth;
            inside = battlePointSegmentWithinRadius(
                candidate.motion.position,
                center.motion.position,
                center.motion.position,
                radius);
            break;
        }
        case DamageAreaKind::Square:
        {
            const int halfSide = output.action.area.squareSideTiles / 2;
            inside = std::abs(candidate.grid.x - center.grid.x) <= halfSide
                && std::abs(candidate.grid.y - center.grid.y) <= halfSide;
            break;
        }
        }
        if (inside)
        {
            result.push_back(candidate.id);
        }
    }
    std::ranges::sort(result);
    return result;
}

struct AreaProjectilePresentation
{
    int effectId{};
    std::string_view reason;
};

AreaProjectilePresentation areaProjectilePresentation(AreaProjectileVisual visual)
{
    switch (visual)
    {
    case AreaProjectileVisual::DeathBlast:
        return { KysChess::EFT_DEATH_BLAST, "殉爆" };
    case AreaProjectileVisual::ShieldBlast:
        return { KysChess::EFT_SHIELD_BLAST, "護盾爆炸" };
    }
    assert(false);
    return {};
}

std::string areaProjectileLogText(
    const BattleEffectDamageRequestOutput& output,
    std::string_view reason,
    int stunFrames)
{
    const auto& amount = output.action.amount;
    if (amount.base == EffectNumberBase::SourceMaxHp
        && !amount.multiplierBase
        && amount.flat == 0
        && amount.percent > 0)
    {
        return stunFrames > 0
            ? std::format("{}{}%（{}幀）", reason, amount.percent, stunFrames)
            : std::format("{}{}%", reason, amount.percent);
    }
    return stunFrames > 0
        ? std::format("{}（{}傷害，{}幀）", reason, output.request.baseDamage, stunFrames)
        : std::format("{}（{}傷害）", reason, output.request.baseDamage);
}

CastWorkToken reserveEffectDamageDescendantWork(
    BattleRuntimeState& state,
    const BattleEffectCommandContext& context,
    const BattleAttackProvenance& provenance)
{
    if (provenance.valid()
        || !context.cast
        || !context.retainCastUntilDamageDescendants)
    {
        return {};
    }
    assert(context.cast->valid());
    assert(state.castLifecycle.containsCast(context.cast->castId));
    return state.castLifecycle.reserveDelayedEffectCommand(context.cast->castId);
}

void appendAreaProjectileDamageOutput(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleEffectDamageRequestOutput& output,
    const BattleEffectCommandContext& context)
{
    assert(output.action.areaProjectiles);
    assert(output.transactionCount > 0);
    if (output.request.baseDamage <= 0)
    {
        return;
    }

    const auto& delivery = *output.action.areaProjectiles;
    const auto presentation = areaProjectilePresentation(delivery.visual);
    for (int transaction = 0; transaction < output.transactionCount; ++transaction)
    {
        BattleAreaProjectileFollowUp followUp;
        if (output.provenance)
        {
            assert(output.provenance->valid());
            followUp.cast = output.provenance->cast;
            followUp.sourceAttack = *output.provenance;
            followUp.expansionWork = state.castLifecycle.reserveDelayedEffectCommand(
                followUp.cast.castId);
        }
        else if (context.cast && context.retainCastUntilDamageDescendants)
        {
            assert(context.cast->valid());
            followUp.cast = *context.cast;
            followUp.expansionWork = state.castLifecycle.reserveDelayedEffectCommand(
                followUp.cast.castId);
        }
        else
        {
            const auto root = state.castLifecycle.beginRootCast({
                .sourceUnitId = output.request.attackerUnitId,
                .magicId = -1,
                .ultimate = false,
                .origin = CastOriginKind::Echo,
                .propagation = CastPropagationPolicy::NoEffectRules,
            });
            followUp.cast = root.provenance;
            followUp.expansionWork = root.commitBarrier;
            followUp.ownsRootCast = true;
        }
        followUp.sourceUnitId = output.request.attackerUnitId;
        followUp.areaSize = delivery.rangeTiles;
        followUp.trackedTargetUnitId = delivery.trackEventSource
            ? output.eventSourceUnitId
            : -1;
        followUp.maxTargets = delivery.maximumTargets;
        followUp.effectId = presentation.effectId;
        followUp.damage = output.request.baseDamage;
        followUp.damagePct = output.action.amount.base == EffectNumberBase::SourceMaxHp
            ? output.action.amount.percent
            : 0;
        followUp.damageKind = output.action.kind;
        followUp.stunFrames = delivery.stunFrames;
        followUp.reason = presentation.reason;
        followUp.logText = areaProjectileLogText(
            output,
            presentation.reason,
            delivery.stunFrames);
        frame.mutableAreaProjectileFollowUps().push_back(std::move(followUp));
    }
}

void appendEffectDamageOutput(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleEffectDamageRequestOutput& output,
    const BattleEffectCommandContext& context)
{
    if (output.action.areaProjectiles)
    {
        appendAreaProjectileDamageOutput(state, frame, output, context);
        return;
    }
    for (int targetUnitId : effectDamageTargetIds(state, output))
    {
        if (context.cast && output.action.perCast.perTargetLimit > 0)
        {
            const BattleEffectPerCastDamageKey key{
                .castId = context.cast->castId,
                .sourceKind = output.source.kind,
                .sourceId = output.source.sourceId,
                .sourceInstanceId = output.source.runtimeInstanceId,
                .ruleId = output.ruleId,
                .targetUnitId = targetUnitId,
            };
            if (!state.effectIntegration.appliedPerCastDamage.insert(key).second)
            {
                continue;
            }
        }

        assert(output.transactionCount > 0);
        for (int transaction = 0; transaction < output.transactionCount; ++transaction)
        {
            auto request = output.request;
            request.defenderUnitId = targetUnitId;
            auto provenance = output.provenance.value_or(BattleAttackProvenance{});
            appendFramePendingDamage(
                state,
                pendingDamage,
                std::move(request),
                std::nullopt,
                false,
                false,
                provenance,
                EffectRuleDamageOrigin{ output.ruleId, output.source },
                reserveEffectDamageDescendantWork(state, context, provenance));
        }
    }
}

void appendStateMachineOutput(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleEffectReductionEntry& entry,
    const StateMachineEffectCommand& command,
    const BattleEffectCommandContext& context)
{
    std::visit(Overloaded{
        [&](const ChangeStateValueAction&)
        {
            // The dispatcher owns rule state so later rules in the same event
            // observe the committed value deterministically.
        },
        [&](const TransferStateValueAction&)
        {
            // The dispatcher atomically moves both state slots while emitting
            // the command, so later rules observe the transferred value.
        },
        [&](const RecordMaximumDamageAction&)
        {
            // The dispatcher owns the recorded maximum so selection and command
            // generation observe one deterministic value.
        },
        [&](const ConsumeRecordedMaximumAction& action)
        {
            if (command.outputValue <= 0)
            {
                return;
            }
            assert(command.outputValue <= std::numeric_limits<int>::max());
            const int amount = static_cast<int>(command.outputValue);
            if (action.destination == StateValueDestination::ShieldAmount)
            {
                assert(false);
                return;
            }

            BattleDamageRequest request;
            request.attackerUnitId = entry.metadata.binding.ownerUnitId;
            request.defenderUnitId = entry.metadata.targetUnitId;
            request.baseDamage = amount;
            request.damageKind = BattleDamageKind::Pure;
            const auto provenance = context.attack.value_or(
                BattleAttackProvenance{});
            appendFramePendingDamage(
                state,
                pendingDamage,
                std::move(request),
                std::nullopt,
                false,
                false,
                provenance,
                EffectRuleDamageOrigin{
                    entry.metadata.ruleId,
                    entry.metadata.binding,
                },
                reserveEffectDamageDescendantWork(state, context, provenance));
        },
        [&](const StartDamageAbsorptionAction&)
        {
            // Persistent absorption lifetime is consumed at the damage boundary;
            // the state slot was reset by the dispatcher.
        },
        [&](const SettleDamageAbsorptionAction& action)
        {
            if (command.outputValue <= 0)
            {
                return;
            }
            assert(command.outputValue <= std::numeric_limits<int>::max());
            for (int targetUnitId : command.selectedSourceUnitIds)
            {
                BattleDamageRequest request;
                request.attackerUnitId = entry.metadata.binding.ownerUnitId;
                request.defenderUnitId = targetUnitId;
                request.baseDamage = static_cast<int>(command.outputValue);
                request.damageKind = action.damageKind;
                appendFramePendingDamage(
                    state,
                    pendingDamage,
                    std::move(request),
                    std::nullopt,
                    false,
                    false,
                    {},
                    EffectRuleDamageOrigin{
                        entry.metadata.ruleId,
                        entry.metadata.binding,
                    },
                    reserveEffectDamageDescendantWork(state, context, {}));
            }
        },
        [&](const BorrowEffectRulesAction&)
        {
            // Borrowed source bindings are applied while constructing the child
            // attack; no current-frame resource mutation belongs here.
        },
        [&](const CopyAttackDefinitionAction&)
        {
            // The cast coordinator already scheduled the tracked copied child cast
            // before this command reaches the mutation reducer.
        },
        [&](const SettleRemainingStatusDamageAction& action)
        {
            assert(action.status == BattleStatusKind::Poison);
            auto& target = state.units.require(entry.metadata.targetUnitId);
            const auto* poison = target.status.effects.find(BattleStatusKind::Poison);
            int settlementDamage{};
            if (poison)
            {
                assert(poison->remainingFrames > 0);
                assert(poison->potency > 0);
                assert(state.status.config.poisonDamageIntervalFrames > 0);
                settlementDamage = projectRemainingPoisonDamage({
                    .firstFutureFrame = state.movement.frame,
                    .remainingFrames = poison->remainingFrames,
                    .remainingStacks = poison->stacks,
                    .intervalFrames = state.status.config.poisonDamageIntervalFrames,
                    .currentHp = target.core.vitals.hp,
                    .damagePct = poison->potency,
                });
            }
            if (settlementDamage <= 0)
            {
                return;
            }
            BattleDamageRequest request;
            request.attackerUnitId = entry.metadata.binding.ownerUnitId;
            request.defenderUnitId = entry.metadata.targetUnitId;
            request.baseDamage = settlementDamage;
            request.damageKind = BattleDamageKind::Poison;
            request.preResolvedDamage = true;
            request.preResolvedModifierPolicy =
                BattlePreResolvedModifierPolicy::DefenderTypedStatuses;
            appendFramePendingDamage(
                state,
                pendingDamage,
                std::move(request),
                std::nullopt,
                false,
                false,
                {},
                EffectStatusDamageOrigin{
                    action.status,
                    entry.metadata.binding.ownerUnitId,
                },
                reserveEffectDamageDescendantWork(state, context, {}));
        },
        [&](const GenerateClonesAction&)
        {
            // Consumed by BattleStartInitializer before the runtime is built.
        },
        [&](const PreventDeathAction&)
        {
            // Consumed by BattleStartInitializer before the runtime is built.
        },
        [&](const ConfigureRescueRepositionAction&)
        {
            // Consumed by BattleStartInitializer before the runtime is built.
        },
    }, command.action);
}

int damageAbsorptionSettlementAmount(const BattleDamageAbsorptionInstance& absorption)
{
    assert(absorption.accumulatedDamage >= 0);
    assert(absorption.returnedPct >= 0);
    if (absorption.accumulatedDamage == 0 || absorption.returnedPct == 0)
    {
        return 0;
    }

    constexpr auto maximumDamage = static_cast<std::int64_t>(std::numeric_limits<int>::max());
    const auto maximumUnclampedValue = maximumDamage * 100 / absorption.returnedPct;
    if (absorption.accumulatedDamage > maximumUnclampedValue)
    {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(
        absorption.accumulatedDamage * absorption.returnedPct / 100);
}

void appendDamageAbsorptionSettlements(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::span<const BattleDamageAbsorptionInstance> absorptions,
    int settlementFrame)
{
    assert(settlementFrame >= 0);
    std::uint64_t previousSequence{};
    for (const auto& absorption : absorptions)
    {
        assert(absorption.sequence > previousSequence);
        previousSequence = absorption.sequence;
        state.effectRules.setStateValue(absorption.binding, absorption.slot, 0);

        const int damage = damageAbsorptionSettlementAmount(absorption);
        if (damage <= 0)
        {
            continue;
        }

        const auto event = BattleEffectEventBridge().makeEvent(
            state,
            {
                .frame = settlementFrame,
                .eventOrdinal = state.effectIntegration.nextEventOrdinal++,
                .ownerUnitId = absorption.binding.ownerUnitId,
            },
            EffectEvent::FrameAdvanced,
            FrameTickEventData{});
        auto context = event.context();
        context.header.binding = absorption.binding;
        const auto targets = BattleEffectSystem::selectTargets(
            absorption.settlementTarget,
            context,
            state.random);
        for (int targetUnitId : targets)
        {
            BattleDamageRequest request;
            request.attackerUnitId = absorption.binding.ownerUnitId;
            request.defenderUnitId = targetUnitId;
            request.baseDamage = damage;
            request.damageKind = absorption.settlementDamageKind;
            appendFramePendingDamage(
                state,
                pendingDamage,
                std::move(request),
                std::nullopt,
                false,
                false,
                {},
                EffectRuleDamageOrigin{
                    absorption.ruleId,
                    absorption.binding,
                });
        }
    }
}

bool sameEffectRuleCommandSequence(
    const EffectCommandMetadata& lhs,
    const EffectCommandMetadata& rhs)
{
    return lhs.binding.kind == rhs.binding.kind
        && lhs.binding.sourceId == rhs.binding.sourceId
        && lhs.binding.ownerUnitId == rhs.binding.ownerUnitId
        && lhs.binding.sourceTeam == rhs.binding.sourceTeam
        && lhs.binding.runtimeInstanceId == rhs.binding.runtimeInstanceId
        && lhs.ruleId == rhs.ruleId
        && lhs.event == rhs.event
        && lhs.ruleOrder == rhs.ruleOrder
        && lhs.eventSourceUnitId == rhs.eventSourceUnitId;
}

void reduceEffectCommand(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const EffectCommand& command,
    const BattleEffectCommandContext& context)
{
    auto reduction = BattleEffectCommandSystem().reduce(
        state,
        command,
        context);
    assert(reduction.entries.size() == 1);
    const auto& entry = reduction.entries.front();
    if (const auto* damage = std::get_if<BattleEffectDamageRequestOutput>(&entry.value))
    {
        appendEffectDamageOutput(state, frame, pendingDamage, *damage, context);
    }
    else if (const auto* move = std::get_if<
                 BattleRoutedEffectCommand<ForceMoveEffectCommand>>(&entry.value))
    {
        const auto& source = state.units.requireCore(entry.metadata.binding.ownerUnitId);
        const auto& target = state.units.requireCore(entry.metadata.targetUnitId);
        auto direction = target.motion.position - source.motion.position;
        if (move->command.action.direction == ForceMoveDirection::TowardSource)
        {
            direction *= -1.0f;
        }
        if (direction.norm() <= 0.01)
        {
            direction = { 1, 0, 0 };
        }
        frame.queueCommand(BattleKnockbackCommand{
            .targetUnitId = target.id,
            .direction = direction,
            .distance = move->command.action.distancePixels > 0
                ? static_cast<double>(move->command.action.distancePixels)
                : move->command.action.distanceTiles * state.gridTransform.tileWidth,
            .lockFrames = move->command.action.lockFrames,
            .semanticDirection = move->command.action.direction,
            .collision = move->command.action.collision,
            .blocked = move->command.action.blocked,
        });
    }
    else if (const auto* cast = std::get_if<
                 BattleRoutedEffectCommand<ModifyCastEffectCommand>>(&entry.value))
    {
        const auto& request = cast->command.action.autoUltimate;
        if (request)
        {
            frame.queueCommand(BattleAutoUltimateCommand{
                entry.metadata.targetUnitId,
                request->consumeMp,
                request->announce,
            });
        }
    }
    else if (const auto* stateMachine = std::get_if<
                 BattleRoutedEffectCommand<StateMachineEffectCommand>>(&entry.value))
    {
        appendStateMachineOutput(
            state,
            pendingDamage,
            entry,
            stateMachine->command,
            context);
    }
    else if (const auto* heal = std::get_if<BattleResourceEffectResult>(&entry.value))
    {
        const auto* resource = std::get_if<ChangeResourceEffectCommand>(&command.value);
        assert(resource);
        appendMpResourceEffectLogEvents(
            frame.logEvents,
            entry.metadata,
            *resource,
            *heal);
        if (heal->heal && heal->heal->appliedAmount > 0)
        {
            appendHealEventLog(
                frame.logEvents,
                heal->heal->request.sourceUnitId,
                heal->heal->request.targetUnitId,
                heal->heal->appliedAmount,
                "效果治療");
            frame.visualEvents.push_back(roleEffectEvent(
                heal->heal->request.targetUnitId,
                KysChess::EFT_HEAL,
                CoreRoleStatusEffectFrames));
        }
    }
    else if (const auto* status = std::get_if<BattleStatusApplyEffectResult>(&entry.value))
    {
        const auto* apply = std::get_if<ApplyStatusEffectCommand>(&command.value);
        assert(apply);
        appendPoisonEffectLogEvents(
            frame.logEvents,
            entry.metadata,
            *apply,
            *status);
    }
    else if (const auto* deferredHp = std::get_if<BattleDeferredHpResourceOutput>(&entry.value))
    {
        BattleDamageRequest request;
        request.attackerUnitId = entry.metadata.binding.ownerUnitId;
        request.defenderUnitId = entry.metadata.targetUnitId;
        request.baseDamage = deferredHp->command.amount;
        request.damageKind = BattleDamageKind::Effect;
        appendFramePendingDamage(
            state,
            pendingDamage,
            std::move(request),
            std::nullopt,
            false,
            false,
            {},
            EffectRuleDamageOrigin{
                entry.metadata.ruleId,
                entry.metadata.binding,
            },
            reserveEffectDamageDescendantWork(state, context, {}));
    }
}

void registerEffectDamageContinuation(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::size_t firstDamageIndex,
    std::vector<EffectCommand> commands,
    BattleEffectCommandContext context)
{
    assert(firstDamageIndex < pendingDamage.size());
    assert(!commands.empty());
    const auto transactionCount = pendingDamage.size() - firstDamageIndex;
    assert(transactionCount <= static_cast<std::size_t>(std::numeric_limits<int>::max()));

    const std::uint64_t continuationId =
        state.effectIntegration.nextDamageContinuationId++;
    const auto [continuation, inserted] =
        state.effectIntegration.damageContinuations.emplace(
            continuationId,
            BattleEffectDamageContinuationRuntime{
                .remainingDamageTransactions = static_cast<int>(transactionCount),
                .commandBatch = {
                    .commands = std::move(commands),
                    .context = std::move(context),
                },
            });
    assert(inserted);
    (void)continuation;
    for (std::size_t index = firstDamageIndex; index < pendingDamage.size(); ++index)
    {
        assert(pendingDamage[index].effectCommandContinuationId == 0);
        pendingDamage[index].effectCommandContinuationId = continuationId;
    }
}

void reduceEffectCommandBatches(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage)
{
    while (true)
    {
        auto queued = std::exchange(
            state.effectIntegration.queuedCommandBatches,
            {});
        for (auto& batch : queued)
        {
            frame.queueEffectCommands(
                std::move(batch.commands),
                std::move(batch.context));
        }

        auto batches = frame.drainEffectCommandBatches();
        if (batches.empty())
        {
            return;
        }
        for (auto& batch : batches)
        {
            std::size_t ruleBegin{};
            while (ruleBegin < batch.commands.size())
            {
                std::size_t ruleEnd = ruleBegin + 1;
                while (ruleEnd < batch.commands.size()
                       && sameEffectRuleCommandSequence(
                           batch.commands[ruleBegin].metadata,
                           batch.commands[ruleEnd].metadata))
                {
                    ++ruleEnd;
                }

                std::size_t actionBegin = ruleBegin;
                while (actionBegin < ruleEnd)
                {
                    const std::uint32_t actionOrder =
                        batch.commands[actionBegin].metadata.actionOrder;
                    std::size_t actionEnd = actionBegin + 1;
                    while (actionEnd < ruleEnd
                           && batch.commands[actionEnd].metadata.actionOrder == actionOrder)
                    {
                        ++actionEnd;
                    }

                    const std::size_t firstDamageIndex = pendingDamage.size();
                    for (std::size_t index = actionBegin; index < actionEnd; ++index)
                    {
                        reduceEffectCommand(
                            state,
                            frame,
                            pendingDamage,
                            batch.commands[index],
                            batch.context);
                    }
                    if (pendingDamage.size() > firstDamageIndex
                        && actionEnd < ruleEnd)
                    {
                        std::vector<EffectCommand> continuationCommands;
                        continuationCommands.reserve(ruleEnd - actionEnd);
                        for (std::size_t index = actionEnd; index < ruleEnd; ++index)
                        {
                            continuationCommands.push_back(
                                std::move(batch.commands[index]));
                        }
                        registerEffectDamageContinuation(
                            state,
                            pendingDamage,
                            firstDamageIndex,
                            std::move(continuationCommands),
                            batch.context);
                        break;
                    }
                    actionBegin = actionEnd;
                }
                ruleBegin = ruleEnd;
            }
        }
    }
}

void completeEffectDamageContinuation(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::uint64_t continuationId)
{
    if (continuationId == 0)
    {
        return;
    }

    const auto continuation =
        state.effectIntegration.damageContinuations.find(continuationId);
    assert(continuation != state.effectIntegration.damageContinuations.end());
    assert(continuation->second.remainingDamageTransactions > 0);
    if (--continuation->second.remainingDamageTransactions > 0)
    {
        return;
    }

    auto commandBatch = std::move(continuation->second.commandBatch);
    state.effectIntegration.damageContinuations.erase(continuation);
    frame.queueEffectCommands(
        std::move(commandBatch.commands),
        std::move(commandBatch.context));
    reduceEffectCommandBatches(state, frame, pendingDamage);
}

struct EnemyTopDebuffTotals
{
    int attack{};
    int defence{};
    int sourceTeam = -1;
};

bool isEnemyTopDebuffSource(
    const BattleRuntimeState& state,
    const EffectSourceBinding& binding)
{
    if (binding.kind != EffectSourceKind::Combo)
    {
        return false;
    }
    const auto source = state.effectSourceNames.find({ binding.kind, binding.sourceId });
    return source != state.effectSourceNames.end() && source->second == "陰險";
}

void appendEnemyTopDebuffReportEvents(
    BattleRuntimeState& state,
    int frame,
    std::vector<BattleLogEvent>& logEvents)
{
    std::map<int, EnemyTopDebuffTotals> current;
    for (const auto& modifier : state.effectCommands.attributeModifiers)
    {
        if (!isEnemyTopDebuffSource(state, modifier.binding)
            || (modifier.expiresFrameExclusive && frame >= *modifier.expiresFrameExclusive)
            || !state.units.requireCore(modifier.targetUnitId).alive)
        {
            continue;
        }
        assert(modifier.operation == AttributeOperation::FlatAdd);
        assert(modifier.amount <= 0);
        auto& total = current[modifier.targetUnitId];
        if (total.sourceTeam < 0)
        {
            total.sourceTeam = modifier.binding.sourceTeam;
        }
        else
        {
            assert(total.sourceTeam == modifier.binding.sourceTeam);
        }
        const int contribution = modifier.amount * (modifier.perStack ? modifier.stackCount : 1);
        if (modifier.attribute == BattleAttribute::Attack)
        {
            total.attack += contribution;
        }
        else if (modifier.attribute == BattleAttribute::Defence)
        {
            total.defence += contribution;
        }
    }

    std::set<int> targetUnitIds;
    for (const auto& [targetUnitId, _] : current)
    {
        targetUnitIds.insert(targetUnitId);
    }
    for (const auto& [targetUnitId, _] : state.effectIntegration.reportedEnemyTopDebuffs)
    {
        targetUnitIds.insert(targetUnitId);
    }

    std::map<int, BattleEnemyTopDebuffReportState> nextReported;
    for (int targetUnitId : targetUnitIds)
    {
        const auto active = current.find(targetUnitId);
        const auto previous = state.effectIntegration.reportedEnemyTopDebuffs.find(targetUnitId);
        const int previousValue = previous == state.effectIntegration.reportedEnemyTopDebuffs.end()
            ? 0
            : previous->second.value;
        int newValue{};
        int sourceTeam = previous == state.effectIntegration.reportedEnemyTopDebuffs.end()
            ? -1
            : previous->second.sourceTeam;
        if (active != current.end())
        {
            assert(active->second.attack == active->second.defence);
            newValue = active->second.attack;
            sourceTeam = active->second.sourceTeam;
            nextReported.emplace(targetUnitId, BattleEnemyTopDebuffReportState{
                .value = newValue,
                .sourceTeam = sourceTeam,
            });
        }
        if (newValue == previousValue)
        {
            continue;
        }
        if (!state.units.requireCore(targetUnitId).alive && newValue == 0)
        {
            continue;
        }

        BattleLogEvent event;
        event.type = BattleLogEventType::Status;
        event.frame = frame;
        event.targetUnitId = targetUnitId;
        event.amount = newValue - previousValue;
        event.previousAmount = previousValue;
        event.newAmount = newValue;
        event.statusId = BattleStatusSemanticId::EnemyTopDebuff;
        event.semanticSourceTeam = sourceTeam;
        event.semanticSourceKind = "combo";
        event.semanticSourceName = "陰險";
        event.segments = battleLogText(
            std::format("陰險：攻防{:+}", event.amount),
            BattleLogTextTone::Negative);
        logEvents.push_back(std::move(event));
    }
    state.effectIntegration.reportedEnemyTopDebuffs = std::move(nextReported);
}

std::vector<BattleFrameEffectCommandBatch> dispatchFrameAdvancedEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    int upcomingFrame)
{
    assert(upcomingFrame == state.movement.frame + 1);
    std::vector<BattleFrameEffectCommandBatch> deferredAutoUltimateBatches;
    const BattleEffectRuntimeSnapshot snapshot(state);
    const auto readView = snapshot.readView();
    for (const auto& owner : snapshot.units())
    {
        if (!owner.alive)
        {
            continue;
        }

        EffectEventContext event;
        event.event = EffectEvent::FrameAdvanced;
        event.header.frame = upcomingFrame;
        event.header.eventOrdinal = state.effectIntegration.nextEventOrdinal++;
        event.header.binding = {
            .kind = EffectSourceKind::Combo,
            .sourceId = -1,
            .ownerUnitId = owner.id,
            .sourceTeam = owner.team,
        };
        event.header.owner = &owner;
        event.header.battle = readView;
        event.payload = FrameTickEventData{
            .deltaFrames = 1,
            .periodOrdinal = static_cast<std::uint64_t>(upcomingFrame),
        };
        auto dispatched = BattleEffectSystem().dispatch(
            state.effectRules,
            event,
            state.random);
        std::vector<EffectCommand> earlyCommands;
        std::vector<EffectCommand> deferredCommands;
        earlyCommands.reserve(dispatched.commands.size());
        deferredCommands.reserve(dispatched.commands.size());
        for (auto& command : dispatched.commands)
        {
            const auto* modifyCast = std::get_if<ModifyCastEffectCommand>(
                &command.value);
            auto& destination = modifyCast && modifyCast->action.autoUltimate
                ? deferredCommands
                : earlyCommands;
            destination.push_back(std::move(command));
        }
        BattleEffectCommandContext commandContext{
            .frame = upcomingFrame,
            .effectPosition = owner.position,
            .areaTargetTeamDomain = owner.team,
        };
        frame.queueEffectCommands(
            std::move(earlyCommands),
            commandContext);
        if (!deferredCommands.empty())
        {
            deferredAutoUltimateBatches.push_back({
                std::move(deferredCommands),
                std::move(commandContext),
            });
        }
    }
    reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    appendEnemyTopDebuffReportEvents(state, upcomingFrame, frame.logEvents);
    return deferredAutoUltimateBatches;
}

std::vector<BattleLogTextSegment> formatAppliedStatusLog(const BattleDamageEvent& event)
{
    auto withValue = [&](const char* label)
    {
        return event.value > 0
            ? logSegments<BattleLogTextTone::Negative>(
                label,
                "（",
                std::pair{ BattleLogTextTone::ResourceValue, event.value },
                "）")
            : logSegments<BattleLogTextTone::Negative>(label);
    };
    switch (event.statusType)
    {
    case BattleDamageStatusType::Hitstun:
        return logStatusFrames<BattleLogTextTone::Negative>("受擊硬直", event.value);
    case BattleDamageStatusType::Stun:
        return logStatusFrames<BattleLogTextTone::Negative>("眩暈", event.value);
    case BattleDamageStatusType::Poison:
        return withValue("中毒");
    case BattleDamageStatusType::Bleed:
        return withValue("流血");
    case BattleDamageStatusType::MpBlocked:
        return logStatusFrames<BattleLogTextTone::Negative>("封內", event.value);
    case BattleDamageStatusType::None:
        return logSegments<BattleLogTextTone::Negative>("狀態");
    }
    assert(false);
    return logSegments<BattleLogTextTone::Negative>("狀態");
}

std::vector<BattleLogTextSegment> formatAppliedStatusLog(
    const BattleDamageTransactionResult& transaction,
    const BattleDamageEvent& event)
{
    if (event.statusType != BattleDamageStatusType::Bleed)
    {
        return formatAppliedStatusLog(event);
    }

    const auto* bleed = transaction.defenderStatus.effects.find(BattleStatusKind::Bleed);
    const int currentStacks = std::max(event.value, bleed ? bleed->stacks : 0);
    const int maxStacks = std::max(currentStacks, event.maxValue);
    return logStatusRange<BattleLogTextTone::Negative>("流血", currentStacks, maxStacks, "層");
}

std::string formatAppliedCooldownExtension(const BattleDamageEvent& event)
{
    if (event.value > 0)
    {
        return std::format("冷卻延長（+{}幀）", event.value);
    }
    return "冷卻延長";
}

void appendFrameDamageResourceLogEvents(
    BattleFrameContext& frame,
    const BattleDamageTransactionResult& transaction)
{
    for (const auto& event : transaction.events)
    {
        switch (event.type)
        {
        case BattleDamageEventType::HpRestored:
            frame.visualEvents.push_back(roleEffectEvent(
                event.targetUnitId,
                KysChess::EFT_HEAL,
                CoreRoleStatusEffectFrames));
            appendHealEventLog(
                frame.logEvents,
                event.sourceUnitId,
                event.targetUnitId,
                event.value,
                "命中回血");
            break;
        case BattleDamageEventType::CooldownExtended:
            appendStatusEventLog(
                frame.logEvents,
                event.sourceUnitId,
                event.targetUnitId,
                formatAppliedCooldownExtension(event),
                BattleStatusSemanticId::None,
                BattleResourceSemanticId::Cooldown,
                event.value);
            break;
        case BattleDamageEventType::MpRestored:
            appendHealEventLog(
                frame.logEvents,
                event.sourceUnitId,
                event.targetUnitId,
                event.value,
                "回復內力",
                BattleResourceSemanticId::MagicPoints);
            break;
        case BattleDamageEventType::MpDrained:
            appendStatusEventLog(
                frame.logEvents,
                event.sourceUnitId,
                event.targetUnitId,
                "吸取內力",
                BattleStatusSemanticId::MagicPointsDrained,
                BattleResourceSemanticId::MagicPoints,
                event.value);
            break;
        default:
            break;
        }
    }
}

BattleLogEvent makeDeathPreventionLog(const BattleDamageEvent& event)
{
    BattleLogEvent log;
    log.type = BattleLogEventType::Status;
    log.sourceUnitId = event.targetUnitId;
    log.targetUnitId = event.targetUnitId;
    log.amount = event.value;
    log.segments = logStatusFrames<BattleLogTextTone::Positive>("死亡庇護", event.value);
    return log;
}

void appendProjectileFollowUpsToFrame(
    BattleFrameContext& frame,
    BattleProjectileFollowUpExpansion followUps)
{
    for (auto& command : followUps.commands)
    {
        frame.queueCommand(std::move(command));
    }
    frame.visualEvents.insert(
        frame.visualEvents.end(),
        std::make_move_iterator(followUps.visualEvents.begin()),
        std::make_move_iterator(followUps.visualEvents.end()));
    frame.logEvents.insert(
        frame.logEvents.end(),
        std::make_move_iterator(followUps.logEvents.begin()),
        std::make_move_iterator(followUps.logEvents.end()));
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

void applyLiveStatusToDamageModifier(
    const BattleStatusEffectState& effects,
    BattleDamageModifierState& modifier)
{
    modifier.poisoned = effects.has(BattleStatusKind::Poison);
}

BattleDamageModifierState runtimeDamageModifierState(
    const BattleRuntimeState& state,
    int unitId,
    int eventSourceUnitId,
    DamageModifierPerspective perspective,
    const BattleDamageRequest& request)
{
    BattleDamageModifierState result;
    if (perspective == DamageModifierPerspective::Outgoing && request.usingSkill)
    {
        result.skillDamagePct = effectAdjustedAttribute(
            state,
            unitId,
            BattleAttribute::SkillDamage,
            0,
            eventSourceUnitId);
    }
    if (perspective == DamageModifierPerspective::Incoming)
    {
        result.damageReductionPct = effectAdjustedAttribute(
            state,
            unitId,
            BattleAttribute::DamageReduction,
            0,
            eventSourceUnitId);
    }

    const std::array stages{
        DamageModifierStage::BeforeDefense,
        DamageModifierStage::AfterDefense,
        DamageModifierStage::Final,
    };
    for (DamageModifierStage stage : stages)
    {
        for (const auto& modifier : BattleEffectCommandSystem::queryDamageModifiers(
                 state,
                 {
                     .unitId = unitId,
                     .eventSourceUnitId = eventSourceUnitId,
                     .perspective = perspective,
                     .channel = effectDamageChannel(request.damageKind),
                     .stage = stage,
                     .frame = state.movement.frame,
                 }))
        {
            const int amount = modifier.amount * modifier.stackCount;
            switch (modifier.operation)
            {
            case DamageModifierOperation::FlatAdd:
                if (perspective == DamageModifierPerspective::Outgoing)
                {
                    result.flatDamageIncrease += amount;
                }
                else
                {
                    result.flatDamageReduction -= amount;
                }
                break;
            case DamageModifierOperation::PercentAdd:
            case DamageModifierOperation::Multiply:
            {
                const int percentDelta = modifier.operation
                        == DamageModifierOperation::Multiply
                    ? amount - 100
                    : amount;
                if (perspective == DamageModifierPerspective::Outgoing)
                {
                    result.skillDamagePct += percentDelta;
                }
                else if (percentDelta < 0)
                {
                    result.damageReductionPct -= percentDelta;
                }
                else
                {
                    result.damageTakenIncreasePct += percentDelta;
                }
                break;
            }
            case DamageModifierOperation::CapSingleHitAtMaxHpPercent:
                assert(perspective == DamageModifierPerspective::Incoming);
                result.maxHitPctMaxHp = result.maxHitPctMaxHp == 0
                    ? amount
                    : std::min(result.maxHitPctMaxHp, amount);
                break;
            case DamageModifierOperation::IgnoreDefensePercent:
            case DamageModifierOperation::ExecuteBelowMaxHpPercent:
                break;
            }
        }
    }
    return result;
}

BattleFrameVector<std::size_t> orderedFramePendingDamageIndexes(
    const std::vector<BattlePendingDamageIntent>& pendingDamage,
    bool sortByDefenderMagnitude,
    std::pmr::memory_resource* frameMemoryResource)
{
    BattleFrameVector<std::size_t> indexes(pendingDamage.size(), frameMemoryResource);
    std::iota(indexes.begin(), indexes.end(), std::size_t{ 0 });
    if (!sortByDefenderMagnitude)
    {
        return indexes;
    }

    std::stable_sort(indexes.begin(), indexes.end(), [&](std::size_t lhs, std::size_t rhs)
    {
        const auto& left = pendingDamage[lhs].request;
        const auto& right = pendingDamage[rhs].request;
        return std::tuple{ left.defenderUnitId, -left.baseDamage }
            < std::tuple{ right.defenderUnitId, -right.baseDamage };
    });
    return indexes;
}

BattleDamageTransactionInput makeFrameDamageTransactionInput(
    BattleRuntimeState& state,
    const BattleDamageRequest& request)
{
    assert(request.defenderUnitId >= 0);

    BattleDamageTransactionInput transaction;
    transaction.request = request;

    const auto& defender = state.units.require(request.defenderUnitId);
    transaction.defender = defender.damageState(
        mpRecoveryBonusPct(state, defender.id()));
    transaction.defenderModifiers = runtimeDamageModifierState(
        state,
        defender.id(),
        request.attackerUnitId,
        DamageModifierPerspective::Incoming,
        request);
    transaction.defenderStatus = defender.statusDamageState();
    transaction.defenderStatus.effects.freezeReductionPct = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::StaggerResistance,
        0,
        request.attackerUnitId);
    applyLiveStatusToDamageModifier(defender.statusEffects(), transaction.defenderModifiers);
    transaction.defenderCooldown = makeBattleFrameCooldownState(defender.core);

    if (request.attackerUnitId != OptionalDamageAttackerUnitId)
    {
        assert(request.attackerUnitId >= 0);
        const auto& attacker = state.units.require(request.attackerUnitId);
        transaction.attacker = attacker.damageState(
            mpRecoveryBonusPct(state, attacker.id()));
        transaction.attackerModifiers = runtimeDamageModifierState(
            state,
            attacker.id(),
            request.defenderUnitId,
            DamageModifierPerspective::Outgoing,
            request);
        transaction.attackerStatus = attacker.statusDamageState();
        transaction.attackerStatus.effects.freezeReductionPct = effectAdjustedAttribute(
            state,
            attacker.id(),
            BattleAttribute::StaggerResistance,
            0,
            request.defenderUnitId);
        applyLiveStatusToDamageModifier(attacker.statusEffects(), transaction.attackerModifiers);
        transaction.attackerHealModifiers = statusHealModifiers(attacker);
    }
    else
    {
        transaction.attacker.id = OptionalDamageAttackerUnitId;
    }

    auto damageKind = request.damageKind;
    if (damageKind == BattleDamageKind::Physical)
    {
        if (request.reflected)
        {
            damageKind = BattleDamageKind::Reflected;
        }
        else if (request.usingSkill)
        {
            damageKind = BattleDamageKind::Skill;
        }
    }
    transaction.liveOutgoingDamagePctDelta = areaOutgoingDamagePctDelta(
        state,
        request.attackerUnitId,
        damageKind);
    transaction.absorptionLayers = BattleEffectCommandSystem::queryDamageAbsorptions(
        state,
        request.defenderUnitId,
        state.movement.frame);

    return transaction;
}

void applyFrameDamageTakenMpGain(BattleDamageTransactionResult& transaction)
{
    if (transaction.finalHpDamage <= 0 || transaction.defender.vitals.maxHp <= 0)
    {
        return;
    }

    const int baseGain = static_cast<int>(
        static_cast<double>(transaction.finalHpDamage) / transaction.defender.vitals.maxHp * 75.0);
    const int mpGain = adjustedMpRestore(
        transaction.defender.mpBlocked,
        transaction.defender.mpRecoveryBonusPct,
        baseGain);
    if (mpGain <= 0)
    {
        return;
    }

    const int before = transaction.defender.vitals.mp;
    transaction.defender.vitals.mp = std::min(transaction.defender.vitals.maxMp, transaction.defender.vitals.mp + mpGain);
    transaction.defenderDelta.mpDelta += transaction.defender.vitals.mp - before;
}

int committedHpDamage(const BattleDamageTransactionResult& transaction)
{
    int damage = 0;
    for (const auto& event : transaction.events)
    {
        if (event.type == BattleDamageEventType::DamageApplied)
        {
            damage += event.value;
        }
    }
    return damage;
}

BattlePresentationColor selectDamageColor(const BattleDamagePresentationInput& presentation)
{
    return (presentation.critical || presentation.ultimate)
        ? presentation.emphasizedDamageColor
        : presentation.normalDamageColor;
}

int selectDamageTextSize(const BattleDamagePresentationInput& presentation)
{
    return (presentation.critical || presentation.ultimate)
        ? presentation.emphasizedDamageTextSize
        : presentation.normalDamageTextSize;
}

void appendFrameDamageOutputEvents(
    BattleFrameContext& frame,
    const BattleDamagePresentationInput& presentation,
    const BattleDamageTransactionResult& transaction)
{
    const int hpDamage = committedHpDamage(transaction);
    if (hpDamage <= 0)
    {
        return;
    }

    if (presentation.enabled && presentation.executed)
    {
        BattleVisualEvent executedText;
        executedText.type = BattleVisualEventType::FloatingText;
        executedText.targetUnitId = transaction.defender.id;
        executedText.text = "處決！";
        executedText.color = presentation.executeTextColor;
        executedText.textSize = presentation.executeTextSize;
        frame.visualEvents.push_back(std::move(executedText));
    }
    else if (presentation.enabled)
    {
        BattleVisualEvent number;
        number.type = BattleVisualEventType::DamageNumber;
        number.targetUnitId = transaction.defender.id;
        number.amount = hpDamage;
        number.criticalMultiplier = presentation.criticalMultiplier;
        number.color = selectDamageColor(presentation);
        number.textSize = selectDamageTextSize(presentation);
        frame.visualEvents.push_back(std::move(number));
    }

    BattleLogEvent damageLog;
    damageLog.type = BattleLogEventType::Damage;
    damageLog.sourceUnitId = transaction.attacker.id;
    damageLog.targetUnitId = transaction.defender.id;
    damageLog.amount = hpDamage;
    damageLog.skillName = presentation.skillName;
    damageLog.skillId = presentation.skillId;
    damageLog.resourceId = BattleResourceSemanticId::HitPoints;
    damageLog.segments = presentation.segments;
    frame.logEvents.push_back(std::move(damageLog));
}

void appendFrameDamagePreDeathLogEvents(
    BattleFrameContext& frame,
    const BattleDamageTransactionResult& transaction)
{
    for (const auto& event : transaction.events)
    {
        if (event.type != BattleDamageEventType::StatusApplied)
        {
            continue;
        }

        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = event.sourceUnitId;
        log.targetUnitId = event.targetUnitId;
        log.amount = event.value;
        log.statusId = static_cast<BattleStatusSemanticId>(static_cast<int>(event.statusType));
        log.segments = formatAppliedStatusLog(transaction, event);
        frame.logEvents.push_back(std::move(log));
    }

    const auto appendAttackBlock = [&](std::string text, BattleStatusSemanticId statusId)
    {
        frame.visualEvents.push_back(roleEffectEvent(
            transaction.defender.id,
            KysChess::EFT_BLOCK,
            CoreRoleStatusEffectFrames));
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = transaction.defender.id;
        log.targetUnitId = transaction.attacker.id;
        log.perspective = BattleLogPerspective::SourceOnly;
        log.statusId = statusId;
        log.segments = battleLogText(std::move(text), BattleLogTextTone::Positive);
        frame.logEvents.push_back(std::move(log));
    };

    if (transaction.blockedByDualWield)
    {
        appendAttackBlock("互搏抵擋了本次傷害", BattleStatusSemanticId::BlockedByDualWield);
    }

    if (transaction.shieldAbsorbed > 0)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = transaction.defender.id;
        log.targetUnitId = transaction.attacker.id;
        log.amount = transaction.shieldAbsorbed;
        log.perspective = BattleLogPerspective::SourceOnly;
        log.segments = logSegments<BattleLogTextTone::Positive>(
            "護盾吸收 ",
            std::pair{ BattleLogTextTone::ShieldValue, transaction.shieldAbsorbed });
        frame.logEvents.push_back(std::move(log));
    }

    if (transaction.hurtInvincGranted && transaction.defenderDelta.invincibleDelta > 0)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = transaction.defender.id;
        log.targetUnitId = transaction.defender.id;
        log.amount = transaction.defenderDelta.invincibleDelta;
        log.segments = logStatusFrames<BattleLogTextTone::Positive>(
            "受傷無敵",
            transaction.defenderDelta.invincibleDelta);
        frame.logEvents.push_back(std::move(log));
    }
}

std::vector<int> appendFrameDamageLifecycle(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleDamageTransactionResult& transaction)
{
    std::vector<int> deadUnitIds;
    for (const auto& event : transaction.events)
    {
        if (event.type == BattleDamageEventType::DeathPrevented)
        {
            frame.logEvents.push_back(makeDeathPreventionLog(event));
        }
        if (event.type != BattleDamageEventType::UnitDied)
        {
            continue;
        }

        BattleAreaEffectSystem::removeForSourceDeath(state.areas, event.targetUnitId);
        deadUnitIds.push_back(event.targetUnitId);
        frame.gameplayEvents.push_back({
            BattleGameplayEventType::UnitDied,
            state.movement.frame,
            event.sourceUnitId,
            event.targetUnitId,
            event.value,
        });
        frame.logEvents.push_back({
            BattleLogEventType::UnitDied,
            state.movement.frame,
            event.sourceUnitId,
            event.targetUnitId,
            event.value,
        });

    }
    return deadUnitIds;
}

void appendFrameDamageGameplayEvents(
    BattleFrameContext& frame,
    const BattleDamageTransactionResult& transaction,
    int skillId)
{
    for (const auto& event : transaction.events)
    {
        if (event.type == BattleDamageEventType::UnitDied)
        {
            continue;
        }
        auto gameplay = toGameplayEvent(event);
        if (gameplay.type == BattleGameplayEventType::DamageApplied)
        {
            gameplay.skillId = skillId;
        }
        frame.gameplayEvents.push_back(std::move(gameplay));
    }
}

void expandFrameDamageFollowUpCommands(BattleRuntimeState& state, BattleFrameContext& frame)
{
    auto pendingCommands = frame.drainCommands();
    auto projectileExpansion = expandBattleProjectileFollowUpCommands(
        pendingCommands,
        state.projectileFollowUps,
        state.units);
    reserveTrackedProjectileFollowUps(state, projectileExpansion);
    appendProjectileFollowUpsToFrame(frame, std::move(projectileExpansion));
    auto areaFollowUps = frame.drainAreaProjectileFollowUps();
    for (const auto& followUp : areaFollowUps)
    {
        auto expansion = expandBattleAreaProjectileFollowUp(
            followUp,
            state.projectileFollowUps,
            state.units);
        if (expansion.commands.empty())
        {
            if (followUp.ownsRootCast)
            {
                state.castLifecycle.cancelPlannedCast(
                    { followUp.cast, followUp.expansionWork },
                    state.movement.frame);
            }
            else
            {
                state.castLifecycle.completeWork(followUp.expansionWork);
            }
            appendProjectileFollowUpsToFrame(frame, std::move(expansion));
            continue;
        }

        bool rootAttackReserved = false;
        for (auto& command : expansion.commands)
        {
            auto* projectile = std::get_if<BattleProjectileSpawnCommand>(&command);
            assert(projectile);
            auto& request = projectile->request;
            assert(!request.provenance.valid());
            assert(!request.castWork.valid());

            BattleAttackReservationRequest reservationRequest;
            if (followUp.sourceAttack)
            {
                reservationRequest.parentAttackId = followUp.sourceAttack->attackId;
                reservationRequest.sharedHitGroupId =
                    followUp.sourceAttack->sharedHitGroupId;
                reservationRequest.propagation = derivedAttackPropagation(
                    *followUp.sourceAttack);
            }
            else
            {
                reservationRequest.propagation =
                    followUp.cast.propagation == CastPropagationPolicy::SourceRules
                    ? CastPropagationPolicy::SourceHitRulesOnly
                    : followUp.cast.propagation;
            }
            reservationRequest.origin = BattleAttackOriginKind::FollowUp;
            reservationRequest.rootAttack = followUp.ownsRootCast
                && !rootAttackReserved;
            reservationRequest.mainProjectile = false;
            const auto reservation = state.castLifecycle.reserveAttack(
                followUp.cast.castId,
                reservationRequest);
            request.provenance = reservation.provenance;
            request.castWork = reservation.work;
            rootAttackReserved = rootAttackReserved
                || reservationRequest.rootAttack;
        }
        assert(!followUp.ownsRootCast || rootAttackReserved);
        state.castLifecycle.completeWork(followUp.expansionWork);
        appendProjectileFollowUpsToFrame(frame, std::move(expansion));
    }
}

void applyLateFrameMpRestores(BattleRuntimeState& state, BattleFrameContext& frame)
{
    auto restores = frame.drainLateMpRestores();
    for (const auto& restore : restores)
    {
        applyFrameMpRestore(
            state,
            restore.unitId,
            restore.amount,
            restore.reason,
            frame.logEvents);
    }
}

BattleRuntimeUnit* findAntiComboTransferTarget(
    BattleRuntimeState& state,
    int deadUnitId,
    int comboId)
{
    const auto& dead = state.units.requireCore(deadUnitId);
    BattleRuntimeUnit* best = nullptr;
    for (auto& candidateRecord : state.units.live())
    {
        auto& candidate = candidateRecord.core;
        if (candidate.id == dead.id || candidate.team != dead.team)
        {
            continue;
        }
        if (!candidateRecord.comboFacts.memberComboIds.contains(comboId))
        {
            continue;
        }
        if (candidateRecord.comboFacts.appliedComboIds.contains(comboId))
        {
            continue;
        }
        if (!best || candidate.cost > best->cost)
        {
            best = &candidate;
        }
    }
    return best;
}

void applyRuntimeAntiComboTransfer(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    int deadUnitId,
    std::vector<BattleLogEvent>& logEvents)
{
    const auto& deadRecord = state.units.require(deadUnitId);
    // 分身保留已生效羈絆供 selector 與自身 runtime rule 使用，但不是
    // roster 成員，死亡時不可把同一份反羈絆效果再次轉移。
    if (deadRecord.core.cloneSourceUnitId >= 0)
    {
        return;
    }
    for (int comboId : deadRecord.comboFacts.appliedComboIds)
    {
        if (!state.antiComboIds.contains(comboId))
        {
            continue;
        }

        auto* target = findAntiComboTransferTarget(state, deadUnitId, comboId);
        if (!target)
        {
            continue;
        }

        auto& targetRecord = state.units.require(target->id);
        auto antiComboTransfer =
            BattleEffectCommandSystem::transferAntiComboInitialization(
                state.effectCommands,
                deadUnitId,
                target->id,
                target->team,
                comboId);
        for (const auto& attributeDelta : antiComboTransfer.coreAttributeDeltas)
        {
            int* value = nullptr;
            switch (attributeDelta.attribute)
            {
            case BattleAttribute::MaxHp:
                value = &targetRecord.core.vitals.maxHp;
                break;
            case BattleAttribute::Attack:
                value = &targetRecord.core.stats.attack;
                break;
            case BattleAttribute::Defence:
                value = &targetRecord.core.stats.defence;
                break;
            case BattleAttribute::Speed:
                value = &targetRecord.core.stats.speed;
                break;
            case BattleAttribute::CriticalChance:
            case BattleAttribute::CriticalDamage:
            case BattleAttribute::DodgeChance:
            case BattleAttribute::BlockChance:
            case BattleAttribute::DamageReduction:
            case BattleAttribute::SkillDamage:
            case BattleAttribute::ProjectilePressureDamage:
            case BattleAttribute::CooldownReduction:
            case BattleAttribute::MpRecoveryBonus:
            case BattleAttribute::StaggerResistance:
            case BattleAttribute::ProjectileReflectChance:
            case BattleAttribute::SkillReflectPercent:
            case BattleAttribute::CounterUltimateBlockChance:
            case BattleAttribute::CriticalAfterDodge:
            case BattleAttribute::DashChance:
            case BattleAttribute::OutgoingCooldownExtensionChance:
            case BattleAttribute::OutgoingCooldownExtensionPercent:
            case BattleAttribute::IncomingCooldownExtensionChance:
            case BattleAttribute::IncomingCooldownExtensionPercent:
                break;
            }
            assert(value);
            *value += attributeDelta.delta;
            if (attributeDelta.attribute == BattleAttribute::MaxHp)
            {
                targetRecord.core.vitals.hp = std::min(
                    targetRecord.core.vitals.hp,
                    targetRecord.core.vitals.maxHp);
            }
        }
        frame.queueEffectCommands(
            std::move(antiComboTransfer.commands),
            {
                .frame = state.movement.frame,
                .areaTargetTeamDomain = target->team,
            });
        state.effectRules.appendAntiComboTransferredRules(
            deadUnitId,
            target->id,
            target->team,
            comboId);
        targetRecord.comboFacts.appliedComboIds.insert(comboId);
        logEvents.push_back(makeAntiComboTransferLog(deadUnitId, target->id));
    }
}

void applyRuntimeDeathComboConsequences(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const std::vector<int>& deadUnitIds,
    std::vector<BattleLogEvent>& logEvents)
{
    for (int deadUnitId : deadUnitIds)
    {
        applyRuntimeAntiComboTransfer(state, frame, deadUnitId, logEvents);
    }
}

std::vector<BattleStatusEvent> advanceStatus(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage)
{
    state.status.config.frame = state.movement.frame;
    auto statusTick = BattleStatusSystem(state.status.config).tick(state.units);
    for (const auto& event : statusTick.events)
    {
        if (event.type != BattleStatusEventType::PoisonDamage
            && event.type != BattleStatusEventType::BleedDamage)
        {
            continue;
        }

        state.units.require(event.sourceUnitId);
        state.units.require(event.unitId);

        BattleDamageRequest request;
        request.attackerUnitId = event.sourceUnitId;
        request.defenderUnitId = event.unitId;
        request.baseDamage = event.value;
        request.damageKind = event.type == BattleStatusEventType::PoisonDamage
            ? BattleDamageKind::Poison
            : BattleDamageKind::Bleed;
        request.preResolvedDamage = true;
        request.preResolvedModifierPolicy =
            BattlePreResolvedModifierPolicy::DefenderTypedStatuses;
        BattleDamagePresentationInput presentation;
        presentation.segments = battleLogText(event.reason, BattleLogTextTone::SkillName);
        applyStatusTickDamagePresentation(state, event.type, event.unitId, presentation);
        appendFramePendingDamage(
            state,
            pendingDamage,
            std::move(request),
            std::move(presentation),
            false,
            false,
            {},
            EffectStatusDamageOrigin{
                event.type == BattleStatusEventType::PoisonDamage
                    ? BattleStatusKind::Poison
                    : BattleStatusKind::Bleed,
                event.sourceUnitId,
            });
    }
    return std::move(statusTick.events);
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
        movementSnapshot.taXue = runtimeDashAttackEnabled(state, unit.id);
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
    refreshRuntimeMovementProfiles(state);
    auto physicsResults = computeMovementPhysics(state, frameMemoryResource);
    auto movementInput = makeFrameMovementPlanInput(
        state,
        physicsResults,
        frameMemoryResource);
    auto movement = BattleMovementPlanner(std::move(movementInput)).tick();
    return commitFrameMovement(state, physicsResults, std::move(movement));
}

struct BattleActionFrameState
{
    int cooldown{};
    int cooldownMax{};
    int actFrame{};
    int actType = -1;
    BattleOperationType operationType = BattleOperationType::None;
    bool haveAction{};
};

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
        cancelEffectRootCast(state, pending->effectCast);
    }
    auto actionState = makeActionRuntimeState(unit.core);
    resetActionFrameState(actionState);
    commitActionFrameStateToRuntime(unit.core, actionState);
    unit.clearActionOwners();
}

void cancelDeadRuntimeActions(BattleRuntimeState& state)
{
    for (const auto& unit : state.units.dead())
    {
        cancelRuntimeAction(state, unit.id());
    }
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

void applyAttackSpawnAttackerDualWieldBlockGain(
    BattleRuntimeState& state,
    const BattleAttackSpawnRequest& request,
    std::vector<BattleLogEvent>& logEvents)
{
    if (request.attackerDualWieldBlockGainChancePct <= 0)
    {
        return;
    }
    assert(request.initial.attackSourceUnitId >= 0);
    assert(!request.initial.skillName.empty());
    assert(request.attackerDualWieldBlockGainChancePct <= 100);

    auto& attacker = state.units.require(request.initial.attackSourceUnitId);
    if (!attacker.core.alive)
    {
        return;
    }
    assert(attacker.damage.dualWieldBlocksRemaining >= 0);
    assert(attacker.damage.dualWieldBlocksRemaining <= DualWieldBlockMaxStacks);
    if (attacker.damage.dualWieldBlocksRemaining == DualWieldBlockMaxStacks)
    {
        return;
    }
    if (!state.random.chance(request.attackerDualWieldBlockGainChancePct))
    {
        return;
    }

    attacker.damage.dualWieldBlocksRemaining += 1;
    appendStatusEventLog(
        logEvents,
        attacker.core.id,
        attacker.core.id,
        std::format("{}·互搏抵擋+1", request.initial.skillName));
}

void applyAreaAttackSpawnModifiers(
    BattleRuntimeState& state,
    BattleAttackSpawnRequest& request)
{
    if (request.initial.operationType != BattleOperationType::RangedProjectile)
    {
        return;
    }

    const auto modifiers = BattleAreaEffectSystem::collectAreaAttackSpawnModifiers(
        state.areas,
        state.gridTransform,
        state.units,
        request.initial.attackSourceUnitId,
        state.movement.frame);
    if (modifiers.tracking)
    {
        request.initial.track = *modifiers.tracking;
    }
    if (modifiers.speedPct)
    {
        const int scalePct = 100 + *modifiers.speedPct;
        assert(scalePct > 0);
        request.initial.velocity *= static_cast<float>(scalePct) / 100.0f;
        request.initial.totalFrame = std::max(
            1,
            (request.initial.totalFrame * 100 + scalePct - 1) / scalePct);
    }
    if (modifiers.projectilePressurePct)
    {
        const int scalePct = 100 + *modifiers.projectilePressurePct;
        assert(scalePct >= 0);
        request.initial.projectilePressurePct =
            request.initial.projectilePressurePct * scalePct / 100;
    }
}

bool consumeTypedAttackSuppression(
    BattleRuntimeState& state,
    const BattleAttackEvent& event)
{
    assert(event.provenance.valid());
    if (state.attacks.contactsSuppressed(event.attackId))
    {
        return true;
    }

    auto& attacker = state.units.require(event.sourceUnitId);
    const auto attackerStatus = BattleStatusSystem({}).snapshot(
        attacker.statusDamageState());
    bool sourceSuppressed{};
    int neutralizeShield{};
    for (const auto kind : {
             BattleStatusKind::Blinded,
             BattleStatusKind::NeutralizeForce,
         })
    {
        if (!attackerStatus.has(kind))
        {
            continue;
        }
        auto consumed = BattleStatusSystem({}).consume(
            attacker.statusDamageState(),
            { .kind = kind });
        assert(consumed.consumed);
        attacker.writeStatusDamageResult(consumed.target);
        sourceSuppressed = true;
        if (kind == BattleStatusKind::NeutralizeForce)
        {
            neutralizeShield = consumed.consumedStatus.potency;
        }
    }
    if (sourceSuppressed)
    {
        state.attacks.suppressContactsForCast(event.provenance.cast.castId);
        if (neutralizeShield > 0)
        {
            int shieldTargetUnitId = OptionalPreferredTargetUnitId;
            const auto cast = state.effectIntegration.casts.find(
                event.provenance.cast.castId);
            if (cast != state.effectIntegration.casts.end())
            {
                shieldTargetUnitId = cast->second.originalTargetUnitId;
            }
            if (shieldTargetUnitId < 0)
            {
                shieldTargetUnitId = event.preferredTargetUnitId;
            }
            if (shieldTargetUnitId < 0)
            {
                shieldTargetUnitId = event.unitId;
            }
            ChangeResourceAction grantShield;
            grantShield.resource = BattleResource::Shield;
            grantShield.kind = ResourceChangeKind::Grant;
            grantShield.amount.flat = neutralizeShield;
            const EffectCommand grantShieldCommand{
                EffectCommandMetadata{
                    .targetUnitId = shieldTargetUnitId,
                },
                ChangeResourceEffectCommand{
                    .action = std::move(grantShield),
                    .amount = neutralizeShield,
                },
            };
            BattleEffectCommandSystem().reduce(
                state,
                grantShieldCommand,
                { .frame = state.movement.frame });
        }
        return true;
    }

    auto& defender = state.units.require(event.unitId);
    const auto defenderStatus = BattleStatusSystem({}).snapshot(
        defender.statusDamageState());
    const auto nextAttackMiss = std::ranges::find_if(
        defenderStatus.statuses,
        [](const auto& instance)
        {
            return instance.kind == BattleStatusKind::NextAttackMiss;
        });
    if (nextAttackMiss == defenderStatus.statuses.end())
    {
        return false;
    }
    auto consumed = BattleStatusSystem({}).consume(
        defender.statusDamageState(),
        { .kind = BattleStatusKind::NextAttackMiss });
    assert(consumed.consumed);
    defender.writeStatusDamageResult(consumed.target);
    return true;
}

int currentHitIgnoreDefensePct(
    const BattleRuntimeState& state,
    const BattleAttackEvent& event,
    std::span<const EffectCommand> commands)
{
    int result{};
    for (const auto& command : commands)
    {
        const auto* damage = std::get_if<ModifyDamageEffectCommand>(&command.value);
        if (!damage
            || damage->action.durationFrames != 0
            || damage->action.stack != EffectStackPolicy::Independent
            || damage->action.perspective != DamageModifierPerspective::Outgoing
            || damage->action.stage != DamageModifierStage::BeforeDefense
            || (damage->action.channel != DamageChannel::All
                && damage->action.channel != effectDamageChannel(event.damageKind))
            || damage->action.operation != DamageModifierOperation::IgnoreDefensePercent)
        {
            continue;
        }
        result += damage->amount;
    }
    for (const auto& modifier : BattleEffectCommandSystem::queryDamageModifiers(
             state,
             {
                 .unitId = event.sourceUnitId,
                 .eventSourceUnitId = event.unitId,
                 .perspective = DamageModifierPerspective::Outgoing,
                 .channel = effectDamageChannel(event.damageKind),
                 .stage = DamageModifierStage::BeforeDefense,
                 .frame = state.movement.frame,
             }))
    {
        if (modifier.operation == DamageModifierOperation::IgnoreDefensePercent)
        {
            result += modifier.amount * modifier.stackCount;
        }
    }
    return std::clamp(result, 0, 100);
}

void appendHitDamageModifier(
    BattleHitDamageModifierPhases& phases,
    DamageModifierPerspective perspective,
    DamageModifierStage stage,
    BattleHitDamageModifier modifier)
{
    if (perspective == DamageModifierPerspective::Outgoing)
    {
        switch (stage)
        {
        case DamageModifierStage::BeforeDefense:
            phases.outgoingBeforeCritical.push_back(modifier);
            return;
        case DamageModifierStage::AfterDefense:
            phases.outgoingAfterCritical.push_back(modifier);
            return;
        case DamageModifierStage::Final:
            phases.outgoingFinal.push_back(modifier);
            return;
        }
    }

    switch (stage)
    {
    case DamageModifierStage::BeforeDefense:
        phases.incomingBase.push_back(modifier);
        return;
    case DamageModifierStage::AfterDefense:
        phases.incomingAfterBase.push_back(modifier);
        return;
    case DamageModifierStage::Final:
        phases.incomingFinal.push_back(modifier);
        return;
    }
    assert(false);
}

void collectHitDamageModifiers(
    const BattleRuntimeState& state,
    const BattleAttackEvent& event,
    std::span<const EffectCommand> commands,
    BattleHitResolutionInput& input)
{
    const DamageChannel channel = effectDamageChannel(event.damageKind);
    for (const auto& command : commands)
    {
        const auto* modifier = std::get_if<ModifyDamageEffectCommand>(&command.value);
        if (!modifier
            || modifier->action.durationFrames != 0
            || modifier->action.stack != EffectStackPolicy::Independent
            || (modifier->action.channel != DamageChannel::All
                && modifier->action.channel != channel))
        {
            continue;
        }
        appendHitDamageModifier(
            input.damageModifiers,
            modifier->action.perspective,
            modifier->action.stage,
            { modifier->action.operation, modifier->amount });
    }

    const std::array perspectives{
        DamageModifierPerspective::Outgoing,
        DamageModifierPerspective::Incoming,
    };
    const std::array stages{
        DamageModifierStage::BeforeDefense,
        DamageModifierStage::AfterDefense,
        DamageModifierStage::Final,
    };
    for (DamageModifierPerspective perspective : perspectives)
    {
        for (DamageModifierStage stage : stages)
        {
            const int unitId = perspective == DamageModifierPerspective::Outgoing
                ? event.sourceUnitId
                : event.unitId;
            const int eventSourceUnitId = perspective == DamageModifierPerspective::Outgoing
                ? event.unitId
                : event.sourceUnitId;
            for (const auto& modifier : BattleEffectCommandSystem::queryDamageModifiers(
                     state,
                     {
                         .unitId = unitId,
                         .eventSourceUnitId = eventSourceUnitId,
                         .perspective = perspective,
                         .channel = channel,
                         .stage = stage,
                         .frame = state.movement.frame,
                     }))
            {
                appendHitDamageModifier(
                    input.damageModifiers,
                    perspective,
                    stage,
                    { modifier.operation, modifier.amount, modifier.stackCount });
            }
        }
    }

    const auto attackerStatus = BattleStatusSystem({}).snapshot(
        state.units.require(event.sourceUnitId).statusDamageState());
    if (event.skillId >= 0 && attackerStatus.skillDamagePct != 0)
    {
        input.damageModifiers.outgoingBeforeCritical.push_back({
            DamageModifierOperation::PercentAdd,
            attackerStatus.skillDamagePct,
        });
    }

    const auto defenderStatus = BattleStatusSystem({}).snapshot(
        state.units.require(event.unitId).statusDamageState());
    if (defenderStatus.damageReductionPct != 0)
    {
        input.damageModifiers.incomingBase.push_back({
            DamageModifierOperation::PercentAdd,
            -defenderStatus.damageReductionPct,
        });
    }
    if (defenderStatus.damageTakenPct != 0)
    {
        input.damageModifiers.incomingFinal.push_back({
            DamageModifierOperation::PercentAdd,
            defenderStatus.damageTakenPct,
        });
    }
}

BattleEffectOwnedEvent makeHitEffectEvent(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    EffectEvent effectEvent)
{
    assert(effectEvent == EffectEvent::MainProjectileBeforeDamage
        || effectEvent == EffectEvent::HitBeforeDamage);
    assert(event.provenance.valid());
    const auto& attacker = state.units.require(event.sourceUnitId);
    HitEventData payload;
    payload.provenance = event.provenance;
    payload.targetUnitId = event.unitId;
    payload.originalTargetUnitId = event.unitId;
    const auto cast = state.effectIntegration.casts.find(
        event.provenance.cast.castId);
    if (cast != state.effectIntegration.casts.end())
    {
        payload.originalTargetUnitId = cast->second.originalTargetUnitId;
    }
    payload.contactPosition = event.position;
    payload.acceptedHit = true;
    payload.preDefenseDamage = std::max(0, event.scriptedDamage);
    payload.damageKind = event.damageKind;

    const auto attackerStatus = BattleStatusSystem({}).snapshot(
        attacker.statusDamageState());
    EffectFormulaInputs formulaInputs;
    formulaInputs.accumulatedStateValue = attackerStatus.pureDamagePerHit;
    return BattleEffectEventBridge().makeEvent(
        state,
        nextEffectEventHeader(
            state,
            event.sourceUnitId,
            std::move(formulaInputs)),
        effectEvent,
        std::move(payload));
}

BattleEffectDispatchResult dispatchAttackSpawnedEffects(
    BattleRuntimeState& state,
    const BattleAttackEvent& event)
{
    assert(event.provenance.valid());
    AttackEventData payload;
    payload.provenance = event.provenance;
    payload.originalTargetUnitId = event.unitId;
    const auto cast = state.effectIntegration.casts.find(
        event.provenance.cast.castId);
    if (cast != state.effectIntegration.casts.end())
    {
        payload.originalTargetUnitId = cast->second.originalTargetUnitId;
    }
    payload.spawnPosition = event.position;
    payload.velocity = event.velocity;
    payload.skillId = event.skillId;
    payload.baseDamage = std::max(0, event.scriptedDamage);
    payload.damageKind = event.damageKind;
    return BattleEffectEventBridge().dispatch(
        state,
        nextEffectEventHeader(state, event.sourceUnitId),
        EffectEvent::AttackSpawned,
        std::move(payload));
}

std::optional<BattleCastInput> makeEffectAttackCastInput(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    std::pmr::memory_resource* frameMemoryResource)
{
    assert(event.provenance.valid());
    const auto cast = state.effectIntegration.casts.find(event.provenance.cast.castId);
    if (cast == state.effectIntegration.casts.end())
    {
        return std::nullopt;
    }

    BattlePendingCastAction pending;
    pending.targetUnitId = cast->second.originalTargetUnitId;
    pending.operationType = event.operationType;
    pending.skillPlan = makePendingCastSkillPlan(cast->second.skill);
    pending.effectCast.provenance = event.provenance.cast;
    return tryMakeRuntimeCastInputForPendingCast(
        state,
        pending,
        frameMemoryResource);
}

void applyAttackSpawnedEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleAttackEvent& event)
{
    if (event.type != BattleAttackEventType::AttackSpawned)
    {
        return;
    }
    auto dispatched = dispatchAttackSpawnedEffects(state, event);
    if (dispatched.commands.empty())
    {
        return;
    }

    auto effectCastInput = makeEffectAttackCastInput(
        state,
        event,
        frame.frameMemoryResource());
    if (effectCastInput)
    {
        const auto liveAttack = std::ranges::find(
            state.attacks.attacks,
            event.attackId,
            &BattleAttackInstance::id);
        assert(liveAttack != state.attacks.attacks.end());
        BattleAttackSpawnRequest prototype;
        prototype.initial = liveAttack->state;
        prototype.provenance = {
            .cast = event.provenance.cast,
            .propagation = event.provenance.propagation,
            .origin = event.provenance.origin,
            .parentAttackId = event.provenance.parentAttackId,
            .attackOrdinal = event.provenance.attackOrdinal,
            .rootAttack = event.provenance.rootAttack,
            .mainProjectile = event.provenance.mainProjectile,
            .sharedHitGroupId = event.provenance.sharedHitGroupId,
        };
        std::vector<BattleAttackSpawnRequest> requests{ std::move(prototype) };
        BattleEffectAttackApplyState effectAttackState{
            .nextSharedHitGroupId = state.effectIntegration.nextSharedHitGroupId,
            .sourceAttackProvenance = event.provenance,
        };
        auto applied = BattleEffectAttackCastSystem().applyAttackCommands(
            *effectCastInput,
            requests,
            dispatched.commands,
            effectAttackState);
        applyEffectAttackDirectives(requests, applied);
        state.effectIntegration.nextSharedHitGroupId =
            effectAttackState.nextSharedHitGroupId;

        for (auto& request : requests)
        {
            if (request.provenance.valid())
            {
                // This is the already-live prototype retained by addToBaseAttack.
                assert(!request.castWork.valid());
                continue;
            }
            BattleAttackReservationRequest reservationRequest;
            reservationRequest.parentAttackId = request.provenance.parentAttackId
                .value_or(event.provenance.attackId);
            reservationRequest.origin = request.provenance.origin;
            reservationRequest.rootAttack = false;
            reservationRequest.mainProjectile = request.provenance.mainProjectile;
            reservationRequest.sharedHitGroupId = request.provenance.sharedHitGroupId;
            reservationRequest.propagation = request.provenance.propagation;
            const auto reservation = state.castLifecycle.reserveAttack(
                event.provenance.cast.castId,
                reservationRequest);
            request.provenance = reservation.provenance;
            request.castWork = reservation.work;
            frame.currentFrameAttacks().push_back(std::move(request));
        }
    }

    BattleEffectCommandContext context{
        .frame = state.movement.frame,
        .effectPosition = event.position,
        .areaTargetTeamDomain = state.units.requireCore(event.sourceUnitId).team,
    };
    context.cast = event.provenance.cast;
    context.attack = event.provenance;
    frame.queueEffectCommands(std::move(dispatched.commands), std::move(context));
    reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
}

void resolveTypedHitEvent(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleAttackEvent& event)
{
    if (event.type != BattleAttackEventType::Hit)
    {
        return;
    }
    assert(event.provenance.valid());
    if (consumeTypedAttackSuppression(state, event))
    {
        return;
    }
    if (event.scriptedDamage <= 0
        && tryResolveDodgeHit(state, event, frame.logEvents, frame.visualEvents))
    {
        return;
    }
    const bool forceCritical = event.scriptedDamage <= 0
        && consumeNextAttackCritical(state, event.sourceUnitId);
    state.castLifecycle.recordHit(event.provenance, event.unitId);

    std::vector<EffectCommand> hitEffectCommands;
    const auto reduceDispatched = [&](BattleEffectDispatchResult dispatched)
    {
        hitEffectCommands.insert(
            hitEffectCommands.end(),
            dispatched.commands.begin(),
            dispatched.commands.end());
        BattleEffectCommandContext context{
            .frame = state.movement.frame,
            .effectPosition = event.position,
            .areaTargetTeamDomain = state.units.requireCore(event.unitId).team,
        };
        context.cast = event.provenance.cast;
        context.attack = event.provenance;
        frame.queueEffectCommands(std::move(dispatched.commands), std::move(context));
        reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    };

    RuntimeMainHitPolicies mainHitPolicies;
    if (event.provenance.mainProjectile)
    {
        const auto owned = makeHitEffectEvent(
            state,
            event,
            EffectEvent::MainProjectileBeforeDamage);
        const auto exactMatches = BattleEffectSystem().queryExactRuntimeRules(
            state.effectRules,
            owned.context(),
            state.random);
        mainHitPolicies = runtimeMainHitPolicies(exactMatches);
        reduceDispatched(BattleEffectEventBridge().dispatch(state, owned));
    }
    const auto hitEvent = makeHitEffectEvent(
        state,
        event,
        EffectEvent::HitBeforeDamage);
    reduceDispatched(BattleEffectEventBridge().dispatch(state, hitEvent));

    const int ignoreDefensePct = currentHitIgnoreDefensePct(
        state,
        event,
        hitEffectCommands);
    auto input = makeHitResolutionInput(
        state,
        event,
        ignoreDefensePct,
        mainHitPolicies);
    input.forceCritical = forceCritical;
    collectHitDamageModifiers(state, event, hitEffectCommands, input);
    auto result = BattleHitResolver().resolve(input, state.random);
    for (const auto& activated : result.activatedRuntimeRules)
    {
        state.effectRules.recordRuntimeRuleActivation(
            activated.binding,
            activated.ruleId,
            state.movement.frame);
    }
    auto followUps = expandBattleProjectileFollowUpCommands(
        result.commands,
        state.projectileFollowUps,
        state.units);
    reserveTrackedProjectileFollowUps(state, followUps);
    result.commands = std::move(followUps.commands);
    result.visualEvents.insert(
        result.visualEvents.end(),
        followUps.visualEvents.begin(),
        followUps.visualEvents.end());
    frame.mutableCommandsForReducer().insert(
        frame.mutableCommandsForReducer().end(),
        result.commands.begin(),
        result.commands.end());
    frame.logEvents.insert(
        frame.logEvents.end(),
        result.logEvents.begin(),
        result.logEvents.end());
    frame.visualEvents.insert(
        frame.visualEvents.end(),
        result.visualEvents.begin(),
        result.visualEvents.end());
}

void advanceAttacksAndResolveHits(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    auto& attackEvents = frame.attackEvents;
    auto& logEvents = frame.logEvents;

    state.attacks.frame = state.movement.frame;
    // AttackSpawned rules can add zero-delay descendants. Drain until stable so
    // every attack created this frame joins the same tick; explicit delays still queue.
    while (true)
    {
        auto attackSpawns = frame.drainCurrentFrameAttacks();
        if (attackSpawns.empty())
        {
            break;
        }
        attackEvents.reserve(
            attackEvents.size()
            + attackSpawns.size()
            + state.attacks.attacks.size() * 2);
        for (auto& request : attackSpawns)
        {
            if (!attackSpawnDelayElapsed(request))
            {
                state.nextFrame.queueAttack(std::move(request));
                continue;
            }
            applyAreaAttackSpawnModifiers(state, request);
            applyAttackSpawnAttackerDualWieldBlockGain(state, request, logEvents);
            assert(request.provenance.valid());
            assert(request.castWork.valid());
            attackEvents.push_back(state.attacks.spawn(
                std::move(request),
                state.castLifecycle));
            applyAttackSpawnedEffects(state, frame, attackEvents.back());
        }
        state.nextFrame.recycleAttacks(std::move(attackSpawns));
    }
    state.attacks.tick(state.units, state.castLifecycle, attackEvents);
    applyProjectileCancelDamageResults(state, attackEvents);
    appendProjectileCancellationLogEvents(state.attacks, attackEvents, logEvents, false);
    for (const auto& event : attackEvents)
    {
        resolveTypedHitEvent(state, frame, event);
    }
    reduceCommandsAfterAttackHits(state, frame);
}

std::string formatExecuteStatus(int thresholdPct)
{
    if (thresholdPct <= 0)
    {
        return "觸發處決";
    }
    return std::format("觸發處決（斬殺線{}%）", thresholdPct);
}

void appendDamagePresentationDetail(BattleDamagePresentationInput& presentation, std::string text)
{
    if (presentation.segments.empty())
    {
        presentation.segments = battleLogText(std::move(text), BattleLogTextTone::SkillName);
        return;
    }

    presentation.segments.push_back({ "、", BattleLogTextTone::SkillName });
    presentation.segments.push_back({ std::move(text), BattleLogTextTone::SkillName });
}

bool applyFrameExecuteReaction(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattlePendingDamageIntent& intent,
    BattleDamageRequest& request,
    BattleDamagePresentationInput& presentation)
{
    assert(request.attackerUnitId >= 0);
    assert(request.defenderUnitId >= 0);
    assert(intent.executeThresholdPct > 0);

    const auto& defender = state.units.requireCore(request.defenderUnitId);
    if (!BattleDamageSystem().shouldExecute({
            defender.vitals.hp,
            defender.vitals.maxHp,
            request.baseDamage,
            true,
            intent.executeThresholdPct,
        }))
    {
        return false;
    }

    request.canExecute = true;
    request.executeThresholdPct = intent.executeThresholdPct;
    presentation.executed = true;
    appendDamagePresentationDetail(presentation, "處決");
    appendStatusEventLog(
        frame.logEvents,
        request.attackerUnitId,
        request.defenderUnitId,
        formatExecuteStatus(intent.executeThresholdPct));
    return true;
}

bool applyFrameDefenderBlockCommands(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleDamageRequest& request)
{
    assert(request.attackerUnitId >= 0);
    assert(request.defenderUnitId >= 0);

    const int counterUltimateBlockChancePct = effectAdjustedAttribute(
        state,
        request.defenderUnitId,
        BattleAttribute::CounterUltimateBlockChance,
        0,
        request.attackerUnitId);
    const bool counterUltimateBlock = counterUltimateBlockChancePct > 0
        && (counterUltimateBlockChancePct >= 100 || state.random.chance(counterUltimateBlockChancePct));
    const int blockChancePct = combineBattleBlockChancePct(
        effectAdjustedAttribute(
            state,
            request.defenderUnitId,
            BattleAttribute::BlockChance,
            0,
            request.attackerUnitId),
        areaAttributeDelta(state, request.defenderUnitId, BattleAttribute::BlockChance));
    const bool block = blockChancePct > 0
        && (blockChancePct >= 100 || state.random.chance(blockChancePct));
    if (!counterUltimateBlock && !block)
    {
        return false;
    }

    if (counterUltimateBlock)
    {
        frame.visualEvents.push_back(roleEffectEvent(
            request.defenderUnitId,
            KysChess::EFT_BLOCK,
            CoreRoleStatusEffectFrames));
        appendStatusEventLog(
            frame.logEvents,
            request.defenderUnitId,
            request.attackerUnitId,
            "格擋後釋放絕招");
        frame.queueCommand(BattleAutoUltimateCommand{ request.defenderUnitId, false });
    }
    if (block)
    {
        frame.visualEvents.push_back(roleEffectEvent(
            request.defenderUnitId,
            KysChess::EFT_BLOCK,
            CoreRoleStatusEffectFrames));
        appendStatusEventLog(
            frame.logEvents,
            request.defenderUnitId,
            request.attackerUnitId,
            "格擋了本次攻擊");
    }
    return true;
}

bool applyFramePendingHitReactions(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattlePendingDamageIntent& intent,
    BattleDamageRequest& request,
    BattleDamagePresentationInput& presentation)
{
    const bool executed = intent.executeThresholdPct > 0
        && applyFrameExecuteReaction(state, frame, intent, request, presentation);
    if (!executed
        && intent.canTriggerDefenderBlock
        && applyFrameDefenderBlockCommands(state, frame, request))
    {
        return false;
    }
    return true;
}

void queueDamageResolvedEffectCommands(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattlePendingDamageIntent& intent,
    const BattleDamageTransactionResult& transaction,
    const EffectUnitSnapshot& attackerBefore,
    const EffectUnitSnapshot& defenderBefore)
{
    const std::uint64_t transactionId =
        state.effectIntegration.nextDamageTransactionId++;
    const BattleDamageResolvedEffectInput input{
        .transactionId = transactionId,
        .attackerBefore = attackerBefore.id >= 0
            ? std::optional<EffectUnitSnapshot>{ attackerBefore }
            : std::nullopt,
        .defenderBefore = defenderBefore,
        .rawDamage = intent.request.baseDamage,
        .resolvedDamage = transaction.resolvedDamageBeforeDefense,
    };

    BattleEffectCommandContext context{
        .frame = state.movement.frame,
    };
    if (const auto* attack = std::get_if<EffectAttackDamageOrigin>(
            &intent.effectOrigin))
    {
        context.cast = attack->provenance.cast;
        context.attack = attack->provenance;
        context.healKind = BattleHealKind::Lifesteal;
    }

    const std::array owners{
        transaction.attacker.id,
        transaction.defender.id,
    };
    for (int ownerUnitId : owners)
    {
        if (ownerUnitId < 0
            || (ownerUnitId == transaction.defender.id
                && transaction.defender.id == transaction.attacker.id))
        {
            continue;
        }
        auto dispatched = BattleEffectEventBridge().dispatchDamageResolvedEvent(
            state,
            nextEffectEventHeader(state, ownerUnitId),
            transaction,
            intent.effectOrigin,
            input);
        context.areaTargetTeamDomain = state.units.requireCore(ownerUnitId).team;
        frame.queueEffectCommands(
            std::move(dispatched.commands),
            context);
        reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    }
}

void recordResolvedDamageHeals(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattlePendingDamageIntent& intent,
    const BattleDamageTransactionResult& transaction)
{
    BattleHealSystem healSystem;
    for (const auto& heal : transaction.resolvedHeals)
    {
        auto resolved = heal;
        if (const auto* attack = std::get_if<EffectAttackDamageOrigin>(
                &intent.effectOrigin))
        {
            assert(attack->provenance.valid());
            resolved.result.request.castId = attack->provenance.cast.castId.value();
        }
        healSystem.recordResolved(state, std::move(resolved));
        reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    }
}

void queueShieldAndDeathEffectCommands(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattlePendingDamageIntent& intent,
    const BattleDamageTransactionResult& transaction,
    const EffectUnitSnapshot& attackerBefore,
    const EffectUnitSnapshot& defenderBefore)
{
    const EffectDamageOrigin cause = intent.effectOrigin;
    const auto defenderAfter = makeEffectUnitSnapshot(
        state,
        state.units.require(transaction.defender.id));
    BattleEffectCommandContext context{
        .frame = state.movement.frame,
        .areaTargetTeamDomain = transaction.defender.id >= 0
            ? state.units.requireCore(transaction.defender.id).team
            : -1,
    };
    if (intent.provenance.valid())
    {
        context.cast = intent.provenance.cast;
        context.attack = intent.provenance;
    }

    if (defenderBefore.shield > 0
        && defenderAfter.shield == 0
        && transaction.shieldAbsorbed > 0)
    {
        ShieldBreakEventData payload{
            .targetBefore = defenderBefore,
            .targetAfter = defenderAfter,
            .brokenAmount = transaction.shieldAbsorbed,
            .cause = cause,
        };
        auto dispatched = BattleEffectEventBridge().dispatch(
            state,
            nextEffectEventHeader(state, transaction.defender.id),
            EffectEvent::ShieldBroken,
            std::move(payload));
        frame.queueEffectCommands(std::move(dispatched.commands), context);
        reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    }

    if (!transaction.killed)
    {
        return;
    }

    DeathEventData death;
    death.deadBefore = defenderBefore;
    death.deadAfter = defenderAfter;
    if (attackerBefore.id >= 0)
    {
        death.killer = attackerBefore;
    }
    death.cause = cause;
    death.deathOrdinal = state.effectIntegration.nextEventOrdinal;
    auto dispatched = BattleEffectEventBridge().dispatch(
        state,
        nextEffectEventHeader(state, transaction.defender.id),
        EffectEvent::UnitDied,
        death);
    frame.queueEffectCommands(std::move(dispatched.commands), context);
    reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());

    auto deathDamageAbsorptions =
        BattleEffectCommandSystem::removeDamageAbsorptionsForSourceDeath(
            state,
            transaction.defender.id);
    appendDamageAbsorptionSettlements(
        state,
        frame.currentFrameDamage(),
        deathDamageAbsorptions,
        state.movement.frame);

    for (const auto& ally : state.units.all())
    {
        if (!ally.core.alive
            || ally.core.id == transaction.defender.id
            || ally.core.team != defenderBefore.team)
        {
            continue;
        }
        auto allyDeath = death;
        allyDeath.allyOfOwner = true;
        auto allyDispatched = BattleEffectEventBridge().dispatch(
            state,
            nextEffectEventHeader(state, ally.id()),
            EffectEvent::AllyDied,
            std::move(allyDeath));
        auto allyContext = context;
        allyContext.areaTargetTeamDomain = ally.core.team;
        frame.queueEffectCommands(
            std::move(allyDispatched.commands),
            std::move(allyContext));
        reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    }
}

void applyDamageAndLifecycle(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    const auto& frameStartMotion = frame.frameStartMotion();
    auto& logEvents = frame.logEvents;
    auto& visualEvents = frame.visualEvents;
    auto& pendingDamage = frame.currentFrameDamage();

    if (state.result.ended && pendingDamage.empty())
    {
        return;
    }

    bool unitDied = false;

    std::vector<int> deadUnitIds;
    std::vector<bool> processedDamage(pendingDamage.size(), false);
    for (;;)
    {
        if (processedDamage.size() < pendingDamage.size())
        {
            processedDamage.resize(pendingDamage.size(), false);
        }
        auto pendingDamageIndexes = orderedFramePendingDamageIndexes(
            pendingDamage,
            state.damage.sortPendingDamageByDefenderMagnitude,
            frame.frameMemoryResource());
        const auto next = std::ranges::find_if(pendingDamageIndexes, [&](std::size_t index)
        {
            return !processedDamage[index];
        });
        if (next == pendingDamageIndexes.end())
        {
            break;
        }
        const std::size_t pendingIndex = *next;
        processedDamage[pendingIndex] = true;
        const auto intent = pendingDamage[pendingIndex];
        const auto completeIntentDescendantWork = [&]
        {
            completeEffectDamageContinuation(
                state,
                frame,
                pendingDamage,
                intent.effectCommandContinuationId);
            if (intent.delayedCastWork.valid())
            {
                state.castLifecycle.completeWork(intent.delayedCastWork);
            }
        };
        if (!state.units.requireCore(intent.request.defenderUnitId).alive)
        {
            completeIntentDescendantWork();
            continue;
        }

        auto request = intent.request;
        auto presentation = intent.presentation;
        if (!applyFramePendingHitReactions(state, frame, intent, request, presentation))
        {
            completeIntentDescendantWork();
            continue;
        }

        const auto defenderBefore = makeEffectUnitSnapshot(
            state,
            state.units.require(request.defenderUnitId));
        EffectUnitSnapshot attackerBefore;
        if (request.attackerUnitId != OptionalDamageAttackerUnitId)
        {
            attackerBefore = makeEffectUnitSnapshot(
                state,
                state.units.require(request.attackerUnitId));
        }
        auto transaction = BattleDamageSystem().resolveTransaction(
            makeFrameDamageTransactionInput(state, request));
        BattleEffectCommandSystem::accumulateDamageAbsorptions(
            state,
            transaction.absorptionReceipts);
        if (intent.provenance.valid())
        {
            state.castLifecycle.recordActualHpDamage(
                intent.provenance,
                transaction.defender.id,
                transaction.finalHpDamage);
        }
        applyFrameDamageTakenMpGain(transaction);
        applyDamageResultToFrameState(state, transaction, frameStartMotion);
        recordResolvedDamageHeals(state, frame, intent, transaction);
        if (intent.request.baseDamage > 0 || intent.request.mpDamage > 0)
        {
            queueDamageResolvedEffectCommands(
                state,
                frame,
                intent,
                transaction,
                attackerBefore,
                defenderBefore);
        }
        queueShieldAndDeathEffectCommands(
            state,
            frame,
            intent,
            transaction,
            attackerBefore,
            defenderBefore);
        appendFrameDamageOutputEvents(frame, presentation, transaction);
        appendFrameDamagePreDeathLogEvents(frame, transaction);
        appendFrameDamageResourceLogEvents(frame, transaction);
        appendFrameDamageGameplayEvents(frame, transaction, presentation.skillId);
        auto transactionDeadUnitIds = appendFrameDamageLifecycle(state, frame, transaction);
        applyRescueRepositionForDamage(state, transaction, logEvents, visualEvents);

        if (!transactionDeadUnitIds.empty())
        {
            unitDied = true;
            deadUnitIds.insert(
                deadUnitIds.end(),
                transactionDeadUnitIds.begin(),
                transactionDeadUnitIds.end());
        }
        completeIntentDescendantWork();
    }

    if (unitDied)
    {
        applyRuntimeDeathComboConsequences(state, frame, deadUnitIds, logEvents);
        cancelDeadRuntimeActions(state);
    }

    updateFrameBattleResultAfterDamage(state, frame);
    expandFrameDamageFollowUpCommands(state, frame);
}

void queueFreeChildCast(
    BattleRuntimeState& state,
    const BattleCastLifecycleEvent& lifecycleEvent,
    const BattleEffectCastRuntimeContext& parentContext,
    const BattleEffectFreeAdditionalCast& freeCast,
    std::span<const EffectCommand> commands,
    std::pmr::memory_resource* frameMemoryResource)
{
    const auto child = state.castLifecycle.beginChildCast(
        lifecycleEvent.provenance.castId,
        {
            .sourceUnitId = lifecycleEvent.provenance.sourceUnitId,
            .magicId = parentContext.skill.id,
            .ultimate = lifecycleEvent.provenance.ultimate,
            .origin = CastOriginKind::FreeRepeat,
            .propagation = freeCast.propagation,
        });
    BattlePendingCastAction pending;
    pending.targetUnitId = freeCast.targetUnitId >= 0
        ? freeCast.targetUnitId
        : parentContext.originalTargetUnitId;
    pending.operationType = parentContext.operationType;
    pending.skillPlan = makePendingCastSkillPlan(parentContext.skill);
    pending.effectCast = child;
    auto input = tryMakeRuntimeCastInputForPendingCast(
        state,
        pending,
        frameMemoryResource);
    if (!input)
    {
        state.castLifecycle.cancelPlannedCast(child, state.movement.frame);
        return;
    }

    std::vector<EffectCommand> childCommands;
    for (const auto& command : commands)
    {
        const auto& candidate = command.metadata;
        const auto& source = freeCast.metadata;
        if (candidate.binding.kind == source.binding.kind
            && candidate.binding.sourceId == source.binding.sourceId
            && candidate.binding.ownerUnitId == source.binding.ownerUnitId
            && candidate.binding.sourceTeam == source.binding.sourceTeam
            && candidate.binding.runtimeInstanceId == source.binding.runtimeInstanceId
            && candidate.ruleId == source.ruleId
            && candidate.event == source.event
            && candidate.ruleOrder == source.ruleOrder
            && candidate.targetOrder == source.targetOrder
            && candidate.targetUnitId == source.targetUnitId)
        {
            childCommands.push_back(command);
        }
    }
    const auto preparation = BattleEffectAttackCastSystem().prepareCast(
        *input,
        child.provenance.ultimate,
        childCommands);
    const auto& childSkill = selectedCastSkill(*input, child.provenance.ultimate);
    const bool forcedRanged = childSkill.forceRanged
        && (childSkill.attackAreaType == 0 || childSkill.attackAreaType == 3);
    pending.operationType = forcedRanged
        ? BattleOperationType::RangedProjectile
        : parentContext.operationType;
    auto cast = BattleCastPlanner().commitSelectedCast(
        *input,
        childSkill,
        child.provenance.ultimate,
        pending.operationType);
    BattleEffectAttackApplyState effectAttackState{
        .nextSharedHitGroupId = state.effectIntegration.nextSharedHitGroupId,
    };
    const auto preparedAttackEffects = BattleEffectAttackCastSystem().applyPreparedCast(
        *input,
        cast,
        preparation,
        effectAttackState);
    applyEffectAttackDirectives(cast.attackSpawnRequests, preparedAttackEffects);
    const auto childAttackEffects = BattleEffectAttackCastSystem().applyAttackCommands(
        *input,
        cast,
        childCommands,
        effectAttackState);
    applyEffectAttackDirectives(cast.attackSpawnRequests, childAttackEffects);
    state.effectIntegration.nextSharedHitGroupId = effectAttackState.nextSharedHitGroupId;
    for (auto& request : cast.attackSpawnRequests)
    {
        request.provenance.propagation = freeCast.propagation;
        request.provenance.origin = BattleAttackOriginKind::CastDerived;
    }
    reserveEffectRootCastAttacks(
        state.castLifecycle,
        child,
        cast.attackSpawnRequests);
    state.effectIntegration.casts[child.provenance.castId] = {
        .originalTargetUnitId = cast.decision.targetUnitId,
        .resourcesBeforeCast = snapshotEffectResourcesBeforeCast(state),
        .skill = childSkill,
        .operationType = pending.operationType,
    };
    for (auto& request : cast.attackSpawnRequests)
    {
        state.nextFrame.queueAttack(std::move(request));
    }
    state.castLifecycle.completeWork(child.commitBarrier);
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
    pending.skillPlan = makePendingCastSkillPlan(copiedSkill);
    pending.effectResourcesBeforeCast = snapshotEffectResourcesBeforeCast(state);
    pending.effectCast = child;
    auto input = tryMakeRuntimeCastInputForPendingCast(
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
    auto committedEffects = dispatchCastCommittedEffects(
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
        applyEffectAttackDirectives(cast.attackSpawnRequests, applied);
    }
    state.effectIntegration.nextSharedHitGroupId =
        effectAttackState.nextSharedHitGroupId;

    for (auto& attack : cast.attackSpawnRequests)
    {
        attack.provenance.propagation = request.propagation;
        attack.provenance.origin = BattleAttackOriginKind::CastDerived;
    }
    reserveEffectRootCastAttacks(
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

void dispatchReadyCastLifecycleEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    for (;;)
    {
        auto events = state.castLifecycle.drainReadyEvents(state.movement.frame);
        if (events.empty())
        {
            break;
        }
        for (const auto& event : events)
        {
            if (event.type == BattleCastLifecycleEventType::CastSettled)
            {
                state.attacks.releaseCastContactSuppression(
                    event.provenance.castId);
            }
            const auto contextIt = state.effectIntegration.casts.find(
                event.provenance.castId);
            if (contextIt == state.effectIntegration.casts.end())
            {
                assert(event.provenance.propagation
                    == CastPropagationPolicy::NoEffectRules);
                continue;
            }
            const auto& castContext = contextIt->second;
            auto dispatched = BattleEffectEventBridge().dispatchCastLifecycleEvent(
                state,
                nextEffectEventHeader(state, event.provenance.sourceUnitId),
                event,
                {
                    .originalTargetUnitId = castContext.originalTargetUnitId,
                    .resourcesBeforeCast = castContext.resourcesBeforeCast,
                });

            if (event.type == BattleCastLifecycleEventType::CastContinuation)
            {
                BattleCastInput preparationInput(frame.frameMemoryResource());
                preparationInput.config = state.action.castConfig;
                preparationInput.geometry = state.action.castGeometry;
                preparationInput.unit.id = event.provenance.sourceUnitId;
                preparationInput.unit.mp = state.units.requireCore(
                    event.provenance.sourceUnitId).vitals.mp;
                preparationInput.unit.maxMp = state.units.requireCore(
                    event.provenance.sourceUnitId).vitals.maxMp;
                preparationInput.normalSkill = castContext.skill;
                preparationInput.ultimateSkill = castContext.skill;
                const auto preparation = BattleEffectAttackCastSystem().prepareCast(
                    preparationInput,
                    dispatched.commands);
                for (const auto& freeCast : preparation.freeAdditionalCasts)
                {
                    queueFreeChildCast(
                        state,
                        event,
                        castContext,
                        freeCast,
                        dispatched.commands,
                        frame.frameMemoryResource());
                }
            }

            frame.queueEffectCommands(
                std::move(dispatched.commands),
                {
                    .frame = state.movement.frame,
                    .cast = event.provenance,
                    .retainCastUntilDamageDescendants = event.type
                        != BattleCastLifecycleEventType::CastSettled,
                    .areaTargetTeamDomain = state.units.requireCore(
                        event.provenance.sourceUnitId).team,
                });
            reduceEffectCommandBatches(
                state,
                frame,
                state.nextFrame.mutableDamageForReducer());

            if (event.type == BattleCastLifecycleEventType::CastSettled)
            {
                state.effectIntegration.casts.erase(event.provenance.castId);
                std::erase_if(
                    state.effectIntegration.appliedPerCastDamage,
                    [&](const BattleEffectPerCastDamageKey& key)
                    {
                        return key.castId == event.provenance.castId;
                    });
            }
        }
    }
}

void emitPresentationFrame(BattleRuntimeState& state, BattleFrameContext& frame)
{
    auto& result = frame.result;
    auto& gameplayEvents = frame.gameplayEvents;
    auto& logEvents = frame.logEvents;
    auto& visualEvents = frame.visualEvents;

    BattlePresentationFrame presentationFrame;
    presentationFrame.frame = state.movement.frame;
    const auto resolveEventFrame = [snapshotFrame = state.movement.frame](auto& event)
    {
        assert(event.frame == BattlePresentationCurrentFrame || event.frame >= 0);
        if (event.frame == BattlePresentationCurrentFrame)
        {
            event.frame = snapshotFrame;
        }
    };

    for (auto& event : gameplayEvents)
    {
        resolveEventFrame(event);
    }
    for (auto& event : visualEvents)
    {
        resolveEventFrame(event);
    }
    for (auto& event : logEvents)
    {
        resolveEventFrame(event);
    }
    presentationFrame.gameplayEvents = std::move(gameplayEvents);
    presentationFrame.visualEvents = std::move(visualEvents);
    presentationFrame.logEvents = std::move(logEvents);
    presentationFrame.gameplayEvents.reserve(
        presentationFrame.gameplayEvents.size() + frame.attackEvents.size());
    presentationFrame.visualEvents.reserve(
        presentationFrame.visualEvents.size() + frame.attackEvents.size() * 3);

    const auto appendGameplayEvent = [&](BattleGameplayEvent event)
    {
        resolveEventFrame(event);
        presentationFrame.gameplayEvents.push_back(std::move(event));
    };
    const auto appendVisualEvent = [&](BattleVisualEvent event)
    {
        resolveEventFrame(event);
        presentationFrame.visualEvents.push_back(std::move(event));
    };
    for (const auto& event : frame.attackEvents)
    {
        appendGameplayEvent(toGameplayEvent(event, state.attacks));
        appendVisualEvents(event, state.attacks, appendVisualEvent);
    }
    presentationFrame.attackSoundIds = std::move(frame.attackSoundIds);
    presentationFrame.rumbles = std::move(frame.rumbles);
    presentationFrame.blinkSoundCount = frame.blinkSoundCount;
    result = std::move(presentationFrame);
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
    return makeBattleDamageUnitStateFromRuntime(unit, runtime);
}

void writeBattleDamageRuntimeUnit(BattleDamageRuntimeUnit& runtime, const BattleDamageUnitState& unit)
{
    writeBattleDamageRuntimeUnitImpl(runtime, unit);
}

BattleCooldownState makeBattleFrameCooldownState(const BattleRuntimeUnit& unit)
{
    return makeBattleFrameCooldownStateImpl(unit);
}

namespace
{

void applySpiralBleedCastEffect(
    BattleRuntimeState& state,
    int sourceUnitId,
    const BattleCastSkillState& skill,
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
        BattleAttackSpawnRequest request;
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
                spiral->bleedStacks,
                spiral->projectileCount,
                projectileSpeed,
                attackSpawns);
        }
    }
}

}  // namespace

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
    appendDamageAbsorptionSettlements(
        state,
        frame.currentFrameDamage(),
        expiredDamageAbsorptions,
        upcomingFrame);

    // Tick status timers and queue status damage, e.g. poison or bleed damage transactions.
    advanceStatus(state, frame.currentFrameDamage());
    // Tick unit cooldown/action/MP timers and collect typed skill-finished effects.
    auto runtimeAdvance = advanceRuntimeUnits(state);
    for (auto& batch : runtimeAdvance.cooldownFinishedEffects)
    {
        frame.queueEffectCommands(
            std::move(batch.commands),
            std::move(batch.context));
    }
    reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    // Evaluate all typed per-frame rules from one frame-start snapshot before
    // movement or action selection can change their conditions.
    auto deferredFrameEffectBatches = dispatchFrameAdvancedEffects(
        state,
        frame,
        upcomingFrame);
    // Reduce early gameplay commands into concrete queues/state; currently mostly a pre-movement drain point.
    reduceCommandsBeforeMovement(state, frame);
    // Advance and commit motion, e.g. physics and tactical movement.
    auto movement = advanceMotionFrame(state, frame.frameMemoryResource());
    // Start or commit unit actions, e.g. cast startup, attack spawn requests, blink teleports, action sounds.
    advanceActionFrameUnits(state, frame, movement);
    // Typed cast-commit effects are delayed until every unit has selected its
    // action for this frame, then reduced before any resulting attack spawns.
    reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    completeCastCommitBarriers(state, frame);
    // Reduce cast-release effects, e.g. 出手回內、全隊盾、當前生命傷害, before attacks/damage apply.
    reduceCommandsBeforeAttacks(state, frame);
    // Spawn/tick attacks and resolve hits; hit commands are reduced immediately into damage/effect queues.
    advanceAttacksAndResolveHits(state, frame);
    // Apply queued damage and lifecycle effects, e.g. HP loss, death, rescue, death AOE, battle end.
    applyDamageAndLifecycle(state, frame);
    state.nextFrame.recycleDamage(frame.drainCurrentFrameDamage());
    if (state.result.ended)
    {
        appendProjectileCancellationLogEvents(
            state.attacks,
            frame.attackEvents,
            frame.logEvents,
            true);
        applyLateFrameMpRestores(state, frame);
        discardBattleEndFrameContinuationWork(frame);
        emitPresentationFrame(state, frame);
        cancelBattleRuntimeForBattleEnd(state, state.result.endedFrame);
        return consumeBattleFrameContext(std::move(frame));
    }
    // Chain terminal logs are emitted after damage so the projectile visibly lands before the chain result.
    appendProjectileCancellationLogEvents(state.attacks, frame.attackEvents, frame.logEvents, true);
    applyLateFrameMpRestores(state, frame);
    for (auto& batch : deferredFrameEffectBatches)
    {
        frame.queueEffectCommands(
            std::move(batch.commands),
            std::move(batch.context));
    }
    // 週期自動絕招固定在延後效果批次階段執行。
    reduceEffectCommandBatches(
        state,
        frame,
        state.nextFrame.mutableDamageForReducer());
    // Reduce late commands from damage/combo lifecycle, e.g. auto-ultimate or death-triggered projectiles.
    reduceCommandsAfterDamageLifecycle(state, frame);
    // Direct auto-ultimate commits share the typed commit pipeline. Reduce their
    // committed effects and any resulting gameplay commands before releasing the
    // commit barrier, including when the auto cast was created late in this frame.
    reduceEffectCommandBatches(
        state,
        frame,
        state.nextFrame.mutableDamageForReducer());
    reduceCommandsAfterDamageLifecycle(state, frame);
    completeCastCommitBarriers(state, frame);
    // A cast may settle only after its final attack's damage/death descendants have
    // had a chance to reserve work. Keep finished attacks alive for presentation.
    completeFinishedRuntimeAttackWork(state);
    if (state.movement.frame < state.maximumFrames)
    {
        dispatchReadyCastLifecycleEffects(state, frame);
    }
    assert(frame.drainCommands().empty());
    // Convert accumulated gameplay/log/visual events into the presentation frame consumed by the scene.
    emitPresentationFrame(state, frame);
    // Runtime maintenance: remove projectiles/melee attacks whose animation lifetime has finished.
    eraseFinishedRuntimeAttacks(state);
    return consumeBattleFrameContext(std::move(frame));
}

}  // namespace KysChess::Battle
