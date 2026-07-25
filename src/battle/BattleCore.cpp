#include "BattleCore.h"

#include "../ChessEftIds.h"
#include "../Find.h"
#include "BattleCombatIntent.h"
#include "BattleLogSegments.h"
#include "BattleMath.h"
#include "BattleResourceRules.h"

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

BattleProjectileBouncePrime collectFrameProjectileBouncePrime(
    const BattleEffectSources& sources,
    int attackerUnitId,
    int rollPct,
    int defaultRange);
int collectFrameExtraProjectileCount(
    const BattleEffectSources& sources,
    BattleRuntimeRandom& random,
    int unitId,
    int baseCount,
    bool ultimate);
int collectFramePostSkillInvincFrames(
    const BattleEffectSources& sources,
    BattleRuntimeRandom& random,
    int unitId,
    bool ultimate);
bool frameComboHasExecute(const KysChess::RoleComboState& state, int attackerUnitId);
BattleFixed resolveFrameArmorPenetratedDefense(
    const BattleEffectSources& sources,
    int attackerUnitId,
    int targetUnitId,
    BattleFixed defense,
    bool ultimate,
    bool mainProjectile,
    BattleRuntimeRandom& random);
BattleEffectSources makeBattleEffectSources(
    BattleRuntimeState& state,
    int unitId,
    BattleSkillEffectRef skillRef);

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
bool isLastAliveInTeam(const BattleRuntimeUnits& units, const BattleRuntimeUnit& unit);
void appendTeamEffectVisualEvents(
    std::vector<BattleVisualEvent>& visualEvents,
    const std::vector<BattleTeamEffectEvent>& events);
void appendTeamEffectLogEvents(
    std::vector<BattleLogEvent>& logEvents,
    const std::vector<BattleTeamEffectEvent>& events,
    const std::string& reason);
struct BattleTeamEffectCommandApplication
{
    std::vector<BattleTeamEffectEvent> events;
    std::vector<BattleLogEvent> logEvents;
};

BattleTeamEffectCommandApplication applyBattleTeamHealCommand(
    BattleRuntimeState& state,
    const BattleTeamHealCommand& command);

std::vector<BattleEffectTriggerEvent> collectFrameCastScopedComboEvents(
    const BattleEffectSources& sources,
    BattleRuntimeRandom& random,
    int unitId,
    bool ultimate);

struct BattleFrameCastScopedComboEffects
{
    int unitId{};
    double projectileSpeed{};
    std::vector<BattleEffectTriggerEvent> events;
};

struct BattleFrameMpRestore
{
    int unitId{};
    int amount{};
    std::string reason;
};

class BattleFrameContext;

template <typename T>
using BattleFrameVector = std::pmr::vector<T>;

void applyFrameCastScopedComboEffects(
    BattleRuntimeState& state,
    const BattleFrameCastScopedComboEffects& effects,
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents);

void applyFrameCastScopedComboEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame);

void applyKnockbackImpulse(
    BattleRuntimeState& state,
    const BattleKnockbackCommand& knockback);

struct BattleRuntimeUnitFrameCommit
{
    explicit BattleRuntimeUnitFrameCommit(std::pmr::memory_resource* memoryResource)
        : comboEvents(memoryResource)
    {
    }

    int unitId = -1;
    BattleFrameVector<BattleComboFrameRuntimeEvent> comboEvents;
};

struct BattleSkillFinishedTeamHeal
{
    int sourceUnitId{};
    int flatHeal{};
    int pctHeal{};
};

struct BattleRuntimeUnitsAdvanceResult
{
    explicit BattleRuntimeUnitsAdvanceResult(std::pmr::memory_resource* memoryResource)
        : runtimeCommits(memoryResource)
        , skillFinishedTeamHeals(memoryResource)
    {
    }

    BattleFrameVector<BattleRuntimeUnitFrameCommit> runtimeCommits;
    BattleFrameVector<BattleSkillFinishedTeamHeal> skillFinishedTeamHeals;
};

void appendAttackSpawnRequests(
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    const std::vector<BattleAttackSpawnRequest>& requests)
{
    attackSpawns.insert(attackSpawns.end(), requests.begin(), requests.end());
}

bool isMpBlocked(BattleRuntimeState& state, int unitId)
{
    return state.units.require(unitId).mpBlocked();
}

int mpRecoveryBonusPct(BattleRuntimeState& state, int unitId)
{
    return state.units.require(unitId).sumAlways(EffectType::MPRecoveryBonus);
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

long long enemyTopDebuffSortScore(const BattleRuntimeUnit& unit)
{
    if (unit.cost <= 0)
    {
        return 0;
    }

    long long score = 1;
    for (int index = 0; index < std::max(0, unit.star); ++index)
    {
        score *= unit.cost;
    }
    return score;
}

struct OpponentTopDebuffRuntimeSummary
{
    int liveOwners{};
    int topTargets{};
    int perOwnerValue{};
};

void appendEnemyTopDebuffUpdates(BattleRuntimeState& state,
                                 std::vector<BattleLogEvent>& logEvents)
{
    std::array<OpponentTopDebuffRuntimeSummary, 2> summaries;
    for (const auto& ownerRecord : state.units.live())
    {
        const auto& owner = ownerRecord.core;
        assert(owner.team == 0 || owner.team == 1);

        const auto& combo = ownerRecord.combo;
        const auto* topDebuff = combo.firstAlways(EffectType::EnemyTopDebuff);
        if (!topDebuff || topDebuff->value <= 0)
        {
            continue;
        }

        auto& summary = summaries[owner.team];
        summary.liveOwners++;
        summary.topTargets = std::max(summary.topTargets, topDebuff->value);
        summary.perOwnerValue = std::max(summary.perOwnerValue, topDebuff->value2);
    }

    for (int ownerTeam = 0; ownerTeam < static_cast<int>(summaries.size()); ++ownerTeam)
    {
        const auto& summary = summaries[ownerTeam];
        const int targetTeam = 1 - ownerTeam;
        std::vector<BattleRuntimeUnit*> targetOrder;
        for (auto& unitRecord : state.units.live())
        {
            auto& unit = unitRecord.core;
            if (unit.team == targetTeam)
            {
                targetOrder.push_back(&unit);
            }
        }

        std::stable_sort(
            targetOrder.begin(),
            targetOrder.end(),
            [](const BattleRuntimeUnit* left, const BattleRuntimeUnit* right)
            {
                const long long leftScore = enemyTopDebuffSortScore(*left);
                const long long rightScore = enemyTopDebuffSortScore(*right);
                if (leftScore != rightScore)
                {
                    return leftScore > rightScore;
                }
                return left->vitals.maxHp > right->vitals.maxHp;
            });

        int assignedTargets = 0;
        for (auto* target : targetOrder)
        {
            auto& combo = state.units.require(target->id).combo;

            int desired = 0;
            if (assignedTargets < summary.topTargets && summary.liveOwners > 0 && summary.perOwnerValue > 0)
            {
                desired = summary.perOwnerValue * summary.liveOwners;
                ++assignedTargets;
            }

            const int delta = combo.setEnemyTopDebuffApplied(desired);
            if (delta == 0)
            {
                continue;
            }

            target->stats.attack = std::max(0, target->stats.attack - delta);
            target->stats.defence = std::max(0, target->stats.defence - delta);
            BattleLogEvent log;
            log.type = BattleLogEventType::Status;
            log.sourceUnitId = -1;
            log.targetUnitId = target->id;
            log.amount = -delta;
            log.previousAmount = -(desired - delta);
            log.newAmount = -desired;
            log.statusId = BattleStatusSemanticId::EnemyTopDebuff;
            log.semanticSourceTeam = ownerTeam;
            log.semanticSourceKind = "combo";
            log.semanticSourceName = "陰險";
            log.segments = battleLogText(std::format(
                "陰險：前{}名攻防{}{}（{}名存活）",
                summary.topTargets,
                delta > 0 ? "-" : "+",
                std::abs(delta),
                summary.liveOwners), BattleLogTextTone::SkillName);
            logEvents.push_back(std::move(log));
        }
    }
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

int sharedBleedMaxStacks(BattleRuntimeState& state, const BattleAttackEvent& event)
{
    if (event.scriptedBleedStacks <= 0)
    {
        return 1;
    }
    auto summary = resolveBattleBleedEffectSummary(
        makeBattleEffectSources(state, event.sourceUnitId, event.skillEffectRef));
    return summary.hasBleedChance ? summary.maxStacks : 1;
}

int resolveHitMagicBaseDamage(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    const BattleRuntimeUnit& attacker,
    const BattleRuntimeUnit& defender)
{
    BattleFixed defence = defender.stats.defence;
    auto attackerSources = makeBattleEffectSources(state, attacker.id, event.skillEffectRef);
    defence = resolveFrameArmorPenetratedDefense(
        attackerSources,
        attacker.id,
        defender.id,
        defence,
        event.ultimate,
        event.mainProjectile,
        state.random);

    return BattleDamageSystem().resolveMagicBaseDamage({
        attacker.stats.attack,
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
    if (attack.state.skillId < 0)
    {
        return currentDamage;
    }

    const auto& attacker = state.units.requireCore(attack.state.attackerUnitId);
    const auto& defender = state.units.requireCore(otherAttack.state.attackerUnitId);

    BattleAttackEvent event;
    event.sourceUnitId = attack.state.attackerUnitId;
    event.skillId = attack.state.skillId;
    event.skillMagicPower = attack.state.skillMagicPower;
    return scaleProjectileCancelDamage(
        resolveHitMagicBaseDamage(state, event, attacker, defender),
        attack.state.operationType);
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
        presentation.sourceUnitId = attack->state.attackerUnitId;
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
        gameplay.sourceUnitId = attack->state.attackerUnitId;
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
            presentation.impactSceneShake = event.ultimate ? 10 : 0;
            presentation.impactUnitShake = event.ultimate ? 10 : 5;
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
            gameplay.sourceUnitId = sourceAttack->state.attackerUnitId;
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
                attack ? attack->state.attackerUnitId : -1,
                attack && attack->spawnedFromAttackId >= 0
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
                attack ? attack->state.attackerUnitId : -1,
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

BattleRuntimeUnitsAdvanceResult advanceRuntimeUnits(
    BattleRuntimeState& state,
    std::pmr::memory_resource* frameMemoryResource)
{
    BattleComboTriggerSystem comboSystem;
    BattleRuntimeUnitsAdvanceResult result(frameMemoryResource);
    result.runtimeCommits.reserve(state.units.size());
    result.skillFinishedTeamHeals.reserve(state.units.size());
    for (auto& unitRecord : state.units.live())
    {
        auto& unit = unitRecord.core;
        assert(unit.id >= 0);
        const bool lastAlive = isLastAliveInTeam(state.units, unit);

        BattleRuntimeUnitFrameCommit committed(frameMemoryResource);
        committed.unitId = unit.id;
        auto tick = unitRecord.advanceFrameTick({
            state.movement.frame,
            3,
            3,
        });

        auto& combo = unitRecord.combo;
        auto frameEvents = comboSystem.advanceFrameRuntime(
            combo,
            {
                state.movement.frame,
                unit.vitals.hp,
                unit.vitals.maxHp,
                unit.alive,
                lastAlive,
            },
            frameMemoryResource);
        committed.comboEvents = std::move(frameEvents);
        if (tick.skillFinished)
        {
            const auto cooldownSkillRef = unitRecord.skillCooldownSource();
            auto teamHeal = comboSystem.collectPendingSkillTeamHeal(
                makeBattleEffectSources(state, unit.id, cooldownSkillRef),
                {
                    BattleComboTriggerHook::AfterSkillCast,
                    unit.id,
                    -1,
                    cooldownSkillRef.slot == BattleSkillSlot::Ultimate,
                    true,
                },
                state.random);
            unitRecord.clearSkillCooldownSource();
            if (teamHeal.flatHeal > 0 || teamHeal.pctHeal > 0)
            {
                result.skillFinishedTeamHeals.push_back({
                    unit.id,
                    teamHeal.flatHeal,
                    teamHeal.pctHeal,
                });
            }
        }
        result.runtimeCommits.push_back(std::move(committed));
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
    const BattleAttackEvent& event)
{
    const auto& attacker = state.units.require(event.sourceUnitId);
    const auto& defender = state.units.require(event.unitId);

    BattleHitResolutionInput input;
    input.attackEvent = event;
    input.attacker = makeHitUnitSnapshot(attacker.core);
    input.defender = makeHitUnitSnapshot(defender.core);
    if ((input.attackEvent.position - input.defender.motion.position).norm() == 0.0)
    {
        assert(input.defender.motion.facing.norm() > 0.0);
        input.attackEvent.position = input.defender.motion.position + input.defender.motion.facing;
    }
    input.sharedBleedMaxStacks = sharedBleedMaxStacks(state, event);
    input.randomDamageVariance = state.random.symmetricInt(10);

    if (event.skillId >= 0)
    {
        input.skill = makeHitSkillSnapshot(
            event,
            attacker.core,
            defender.core,
            resolveHitMagicBaseDamage(state, event, attacker.core, defender.core));
    }
    input.attackerStatusEffects = attacker.statusEffects();
    input.defenderStatusEffects = defender.statusEffects();
    return input;
}

BattleEffectSources makeBattleEffectSources(
    BattleRuntimeState& state,
    int unitId,
    BattleSkillEffectRef skillRef = {})
{
    auto& record = state.units.require(unitId);
    BattleEffectSources sources;
    sources.combo = { { BattleEffectSourceKind::Combo, BattleSkillSlot::None }, &record.combo };
    if (skillRef.slot != BattleSkillSlot::None)
    {
        assert(skillRef.unitId == unitId);
        sources.skill = {
            { BattleEffectSourceKind::Skill, skillRef.slot },
            &record.skillEffects.slot(skillRef.slot).effects,
        };
    }
    return sources;
}

BattleSkillEffectRef skillEffectRefForCast(int unitId, bool ultimate)
{
    return {
        unitId,
        ultimate ? BattleSkillSlot::Ultimate : BattleSkillSlot::Normal,
    };
}

BattleEffectSources makeSelectedCastEffectSources(
    BattleRuntimeState& state,
    int unitId,
    bool ultimate)
{
    return makeBattleEffectSources(state, unitId, skillEffectRefForCast(unitId, ultimate));
}

void markSelectedCastFinishPending(
    BattleRuntimeState& state,
    int unitId,
    bool ultimate)
{
    if (!ultimate)
    {
        return;
    }

    auto sources = makeSelectedCastEffectSources(state, unitId, true);
    if (sources.skill.state)
    {
        sources.skill.state->setTypePending(EffectType::OnSkillTeamHeal, true);
    }
}

bool tryResolveDodgeHit(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    const double roll = state.random.nextPercent();
    auto& defenderCombo = state.units.require(event.unitId).combo;
    auto dodge = BattleComboTriggerSystem().resolveDodge(defenderCombo, event.sourceUnitId, roll);
    if (!dodge.dodged)
    {
        return false;
    }

    defenderCombo.setTypePending(EffectType::DodgeThenCrit, true);

    logEvents.push_back(dodgeStatusEvent(event.unitId, event.sourceUnitId));
    visualEvents.push_back(roleEffectEvent(event.unitId, KysChess::EFT_EVADE, CoreRoleStatusEffectFrames));
    return true;
}

void resolveHitEvents(
    BattleRuntimeState& state,
    std::span<const BattleAttackEvent> events,
    std::pmr::vector<BattleGameplayCommand>& commands,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    for (const auto& event : events)
    {
        if (event.type != BattleAttackEventType::Hit)
        {
            continue;
        }

        if (event.scriptedDamage <= 0 && tryResolveDodgeHit(state, event, logEvents, visualEvents))
        {
            continue;
        }

        auto input = makeHitResolutionInput(state, event);
        auto attackerSources = makeBattleEffectSources(state, event.sourceUnitId, event.skillEffectRef);
        auto defenderSources = makeBattleEffectSources(state, event.unitId);
        auto result = BattleHitResolver().resolve(input, attackerSources, defenderSources, state.random);
        auto followUps = expandBattleProjectileFollowUpCommands(
            result.commands,
            state.projectileFollowUps,
            state.units);
        result.commands = std::move(followUps.commands);
        result.visualEvents.insert(
            result.visualEvents.end(),
            followUps.visualEvents.begin(),
            followUps.visualEvents.end());
        commands.insert(commands.end(), result.commands.begin(), result.commands.end());
        logEvents.insert(
            logEvents.end(),
            result.logEvents.begin(),
            result.logEvents.end());
        visualEvents.insert(
            visualEvents.end(),
            result.visualEvents.begin(),
            result.visualEvents.end());
    }
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
    const BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed);

void refreshMovementSkillProfile(
    BattleUnitState& movementUnit,
    const BattleRuntimeUnit& runtimeUnit,
    const BattleRuntimeState& state)
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
    const auto skill = makeRuntimeCastSkillProfile(
        state,
        runtimeUnit,
        useUltimate ? seed->ultimateSkill : seed->normalSkill);
    movementUnit.reach = skill.reach > 0.0 ? skill.reach : state.action.actionRules.meleeAttackReach;
    movementUnit.style = skill.rangedStyle ? CombatStyle::Ranged : CombatStyle::Melee;
    const auto& combo = state.units.require(runtimeUnit.id).combo;
    movementUnit.taXue = combo.hasAlways(EffectType::DashAttack);
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

    bool firstHitBlockActiveForFrame(int unitId) const
    {
        return firstHitBlockActiveDefenders_.contains(unitId);
    }

    void activateFirstHitBlockForFrame(int unitId)
    {
        assert(unitId >= 0);
        firstHitBlockActiveDefenders_.insert(unitId);
    }

    void queueCommand(BattleGameplayCommand command)
    {
        frameCommands_.push_back(std::move(command));
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
        , firstHitBlockActiveDefenders_(&frameMemoryResource_)
        , frameStartMotion_(makeUnitMotionSnapshot(state.units, &frameMemoryResource_))
        , gameplayEvents(std::move(recycledPresentation.gameplayEvents))
        , logEvents(std::move(recycledPresentation.logEvents))
        , visualEvents(std::move(recycledPresentation.visualEvents))
        , attackSoundIds(std::move(recycledPresentation.attackSoundIds))
        , rumbles(std::move(recycledPresentation.rumbles))
        , attackEvents(&frameMemoryResource_)
        , castScopedComboEffects(&frameMemoryResource_)
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
    std::pmr::set<int> firstHitBlockActiveDefenders_;
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
    BattleFrameVector<BattleFrameCastScopedComboEffects> castScopedComboEffects;
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
        const auto& combo = record.combo;
        movementUnit.taXue = combo.hasAlways(EffectType::DashAttack);
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
    const BattleActionRulesConfig& actionRules,
    const BattleCastGeometry& geometry)
{
    if (skill.id < 0)
    {
        return actionRules.tileWidth * 3.0;
    }
    if (runtimeForcedRangedMagic(skill, forceRanged))
    {
        return std::max(
            actionRules.tileWidth * 3.0,
            static_cast<double>(std::max(1, forcedRangedMinSelectDistance)) * actionRules.tileWidth);
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
        return std::min(actionRules.maxEffectiveBattleReach, reach - 10.0);
    }
    return std::max(actionRules.tileWidth * 3.0, static_cast<double>(skill.selectDistance) * actionRules.tileWidth);
}

double runtimeEffectiveBattleReach(
    const BattleActionSkillSeed& skill,
    bool forceRanged,
    int forcedRangedMinSelectDistance,
    int projectileSpeedMultiplierPct,
    const BattleActionRulesConfig& actionRules,
    const BattleCastGeometry& geometry)
{
    if (skill.id < 0)
    {
        return actionRules.tileWidth * 2.0;
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
        const double rangedAttackSafetyMargin = actionRules.meleeAttackHitRadius - actionRules.tileWidth / 2.0;
        return std::max(actionRules.tileWidth * 2.0, projectileReach - rangedAttackSafetyMargin);
    }
    if (skill.attackAreaType == 3)
    {
        return actionRules.heavyAttackReach;
    }
    return actionRules.meleeAttackReach;
}

RuntimeCastSkillProfile makeRuntimeCastSkillProfile(
    const BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed)
{
    RuntimeCastSkillProfile profile;
    if (seed.id < 0)
    {
        return profile;
    }

    constexpr int DefaultForcedRangedMinSelectDistance = 6;
    const auto& combo = state.units.require(unit.id).combo;
    const auto* forceRangedEffect = combo.firstAlways(EffectType::ForceRangedAttack);
    profile.forceRanged = forceRangedEffect != nullptr;
    const int forcedRangedMinSelectDistance = forceRangedEffect && forceRangedEffect->value2 > 0
        ? std::max(1, forceRangedEffect->value2)
        : DefaultForcedRangedMinSelectDistance;
    const bool forcedRangedMagic = runtimeForcedRangedMagic(seed, profile.forceRanged);
    profile.effectiveSelectDistance = runtimeEffectiveProjectileSelectDistance(
        seed,
        forcedRangedMagic,
        forcedRangedMinSelectDistance);
    profile.projectileSpeedMultiplierPct = forceRangedEffect && forceRangedEffect->value > 0
        ? forceRangedEffect->value
        : 100;
    profile.reach = std::min(
        runtimeEffectiveBattleReach(
            seed,
            profile.forceRanged,
            forcedRangedMinSelectDistance,
            profile.projectileSpeedMultiplierPct,
            state.action.actionRules,
            state.action.castGeometry),
        state.action.actionRules.maxEffectiveBattleReach);
    profile.rangedStyle = runtimeBattleRangedStyle(seed, profile.forceRanged);
    profile.blinkReach = runtimeBattleBlinkReach(
        seed,
        profile.forceRanged,
        forcedRangedMinSelectDistance,
        state.action.actionRules,
        state.action.castGeometry);
    return profile;
}

BattleCastSkillState makeRuntimeCastSkillState(
    const BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed,
    bool ultimate)
{
    BattleCastSkillState skill;
    if (seed.id < 0)
    {
        return skill;
    }

    const auto profile = makeRuntimeCastSkillProfile(state, unit, seed);
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
        snapshot.unit.forcePullProtect = unit.hasAlways(EffectType::ForcePullProtect);
        snapshot.unit.forcePullExecute = unit.hasAlways(EffectType::ForcePullExecute);
        snapshot.unit.forcePullProtectRemaining = unit.forcePullProtectRemaining();
        snapshot.unit.forcePullExecuteRemaining = unit.forcePullExecuteRemaining();
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
    input.unit.speed = source.core.stats.speed;
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

void refreshRuntimeCastSkillBonuses(BattleRuntimeState& state, BattleCastInput& input)
{
    if (input.ultimateSkill.id >= 0 && input.unit.mp == input.unit.maxMp)
    {
        input.ultimateSkill.extraProjectileCount = collectFrameExtraProjectileCount(
            makeSelectedCastEffectSources(state, input.unit.id, true),
            state.random,
            input.unit.id,
            0,
            true);
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
    input.unit.speed = unit.core.stats.speed;
    input.unit.operationCount = unit.core.operationCount;
    input.unit.meleeAttackReach = state.action.actionRules.meleeAttackReach;
    input.unit.dashAttackReach = state.action.actionRules.dashAttackMeleeReach;
    input.unit.hasEquippedSkill = seed.hasEquippedSkill;
    input.unit.movementDashActive = movementDashActive;
    input.unit.frozen = unit.frozen();
    input.unit.dashAttackEnabled = unit.hasAlways(EffectType::DashAttack);

    input.unit.dashVelocity = unit.core.motion.facing;
    if (input.unit.dashVelocity.norm() > 0.01)
    {
        input.unit.dashVelocity.normTo(
            static_cast<float>(
                state.action.actionRules.meleeAttackHitRadius / state.action.actionRules.dashMomentumFrames));
    }

    const bool ultimateReady = unit.core.vitals.maxMp > 0 && unit.core.vitals.mp >= unit.core.vitals.maxMp;
    const bool useUltimate = ultimateReady && seed.ultimateSkill.id >= 0;
    input.unit.cooldownReductionPct = BattleEffectReader().sumAlways(
        makeSelectedCastEffectSources(state, unit.id(), useUltimate),
        EffectType::CDR);
    input.normalSkill = makeRuntimeCastSkillState(
        state,
        unit.core,
        seed.normalSkill,
        false);
    input.ultimateSkill = makeRuntimeCastSkillState(
        state,
        unit.core,
        seed.ultimateSkill,
        true);
    const auto& selectedSeed = useUltimate
        ? seed.ultimateSkill
        : seed.normalSkill;
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

    double dashDistance = state.action.actionRules.meleeAttackHitRadius
        / state.action.actionRules.dashMomentumFrames;

    if (selectedSkill.rangedStyle)
    {
        const double attackRange = std::min(selectedSkill.reach, state.action.actionRules.maxEffectiveBattleReach);
        const double forwardGap = std::max(0.0, input.targetDistance - attackRange);
        dashDistance = state.action.actionRules.meleeAttackHitRadius
            / state.action.actionRules.dashMomentumFrames;
        if (forwardGap > state.movement.config.engagementDeadband)
        {
            dashDistance = std::min(dashDistance, forwardGap / state.action.actionRules.dashMomentumFrames);
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
            - state.action.actionRules.meleeAttackReach
            + state.movement.config.engagementDeadband;
        dashDistance = std::clamp(
            usefulAdvance,
            0.0,
            state.movement.config.maxDashDistance)
            / state.action.actionRules.dashMomentumFrames;
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

BattlePendingCastAction makePendingCastAction(const BattleCastInput& castInput,
                                              const BattleCastResult& cast,
                                              int castFrame);
BattleActionCommitInput makeCommittedCastActionInput(BattleRuntimeState& state,
                                                     const BattleRuntimeUnit& unit,
                                                     const BattleCastInput& castInput,
                                                     const BattleCastSkillState& selectedSkill,
                                                     const BattleCastResult& cast);
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

    auto& combo = state.units.require(unit.id).combo;
    if (combo.hasAlways(EffectType::BlinkAttack))
    {
        const double blinkReach = selectedSkill.blinkReach > 0.0 ? selectedSkill.blinkReach : selectedSkill.reach;
        actionInput.blinkGeometry = makeRuntimeBlinkGeometry(state, unit, blinkReach);
    }
}

void applyCastPostSkillInvincibility(
    BattleRuntimeState& state,
    int sourceUnitId,
    int frames,
    std::vector<BattleLogEvent>& logEvents);

bool tryCommitAutoUltimate(
    BattleRuntimeState& state,
    int unitId,
    bool consumeMp,
    bool announceAutoUltimate,
    std::pmr::memory_resource* frameMemoryResource,
    std::vector<int>& attackSoundIds,
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
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
    refreshRuntimeCastSkillBonuses(state, castInput);

    const auto operationType =
        BattleCombatIntentPlanner().operationTypeForAttackArea(castInput.ultimateSkill.attackAreaType);
    if (operationType == BattleOperationType::None)
    {
        return true;
    }

    refreshRuntimeDashAttackDetails(
        state,
        unit,
        castInput,
        true,
        operationType == BattleOperationType::Dash);
    auto cast = BattleCastPlanner().commitSelectedCast(castInput, true, operationType);
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
        cast);
    auto& combo = state.units.require(unitId).combo;
    auto actionResult = BattleActionCommitSystem().commit(actionInput, combo, state.units);
    markSelectedCastFinishPending(state, unitId, true);
    auto castSources = makeSelectedCastEffectSources(state, unitId, true);
    applyCastPostSkillInvincibility(
        state,
        unitId,
        collectFramePostSkillInvincFrames(castSources, state.random, unitId, true),
        logEvents);
    appendAttackSpawnRequests(attackSpawns, actionResult.attackSpawnRequests);
    logEvents.insert(
        logEvents.end(),
        actionResult.logEvents.begin(),
        actionResult.logEvents.end());
    visualEvents.insert(
        visualEvents.end(),
        actionResult.visualEvents.begin(),
        actionResult.visualEvents.end());
    BattleFrameCastScopedComboEffects castScopedEffects{
        unitId,
        committedCastProjectileSpeed(actionResult, state.projectileFollowUps.projectileSpeed),
        collectFrameCastScopedComboEvents(castSources, state.random, unitId, true),
    };
    unit.operationCount = actionResult.operationCount;
    if (consumeMp)
    {
        unit.vitals.mp -= unit.vitals.maxMp;
    }
    applyFrameCastScopedComboEffects(
        state,
        castScopedEffects,
        attackSpawns,
        pendingDamage,
        logEvents,
        visualEvents);
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
                                              int castFrame)
{
    assert(castFrame > 0);
    BattlePendingCastAction pending;
    pending.unitId = cast.decision.unitId;
    pending.targetUnitId = cast.decision.targetUnitId;
    pending.ultimate = cast.decision.ultimate;
    pending.operationType = cast.decision.operationType;
    pending.castFrame = castFrame;
    pending.normalAttackActType = castInput.normalSkill.magicType;
    pending.dashVelocity = castInput.unit.dashVelocity;
    pending.skill = selectedCastSkill(castInput, cast);
    return pending;
}

int pendingCastCommitTargetUnitId(const BattleRuntimeUnits& units, const BattlePendingCastAction& pending)
{
    const auto& source = units.requireCore(pending.unitId);
    if (!source.alive)
    {
        return -1;
    }

    assert(pending.targetUnitId >= 0);
    const auto& target = units.requireCore(pending.targetUnitId);
    if (target.alive && target.team != source.team)
    {
        return target.id;
    }

    return findNearestEnemyUnitId(units, pending.unitId);
}

std::optional<BattleCastInput> tryMakeRuntimeCastInputForPendingCast(
    BattleRuntimeState& state,
    const BattlePendingCastAction& pending,
    std::pmr::memory_resource* frameMemoryResource)
{
    const auto& unit = state.units.requireCore(pending.unitId);
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
    input.unit.speed = unit.stats.speed;
    input.unit.operationCount = unit.operationCount;
    input.unit.meleeAttackReach = state.action.actionRules.meleeAttackReach;
    input.unit.dashAttackReach = state.action.actionRules.dashAttackMeleeReach;
    input.unit.hasEquippedSkill = true;
    input.unit.movementDashActive = actionMovementDashActive(state, unit.id);
    auto& combo = state.units.require(unit.id).combo;
    input.unit.cooldownReductionPct = BattleEffectReader().sumAlways(
        makeSelectedCastEffectSources(state, unit.id, pending.ultimate),
        EffectType::CDR);
    input.unit.dashAttackEnabled = combo.hasAlways(EffectType::DashAttack);
    input.unit.dashVelocity = unit.motion.facing;
    if (input.unit.dashVelocity.norm() > 0.01)
    {
        assert(state.action.actionRules.dashMomentumFrames > 0);
        input.unit.dashVelocity.normTo(
            static_cast<float>(
                state.action.actionRules.meleeAttackHitRadius
                / state.action.actionRules.dashMomentumFrames));
    }
    input.unit.emitDashFollowUpSkillAttack = input.unit.dashAttackEnabled && pending.skill.id >= 0;
    input.unit.dashFollowUpOperationType = pending.skill.id >= 0
        ? BattleCombatIntentPlanner().operationTypeForAttackArea(pending.skill.attackAreaType)
        : BattleOperationType::None;
    input.normalSkill = pending.skill;
    input.ultimateSkill = pending.skill;

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
    const BattleCastResult& cast)
{
    BattleActionCommitInput actionInput;
    actionInput.hasCast = cast.decision.canCast;
    actionInput.cast = cast;
    actionInput.blinkRandomRoll = state.random.nextInt(std::numeric_limits<int>::max());
    actionInput.blinkCellRandomRoll = state.random.nextInt(std::numeric_limits<int>::max());
    actionInput.blinkReach = selectedSkill.blinkReach > 0.0 ? selectedSkill.blinkReach : selectedSkill.reach;
    actionInput.blinkWeakTargetDefWeight = state.action.blinkWeakTargetDefWeight;
    actionInput.strengthenedMeleeOperationCountThreshold =
        state.action.strengthenedMeleeOperationCountThreshold;
    actionInput.normalAttackActType = castInput.normalSkill.magicType;
    populateActionCommitLiveInput(state, unit, castInput, selectedSkill, actionInput);

    auto prime = collectFrameProjectileBouncePrime(
        makeSelectedCastEffectSources(state, unit.id, cast.decision.ultimate),
        unit.id,
        state.random.nextInt(100),
        state.action.projectileBounceRange);
    actionInput.projectileBouncePrime = {
        prime.count,
        prime.chancePct,
        prime.rollPct,
        prime.range,
    };
    return actionInput;
}

std::optional<BattleActionCommitInput> tryMakeRuntimeActionCommitInput(
    BattleRuntimeState& state,
    const BattleTickResult& movement,
    const BattlePendingCastAction& pending,
    std::pmr::memory_resource* frameMemoryResource)
{
    (void)movement;

    const auto& unit = state.units.requireCore(pending.unitId);
    auto castInput = tryMakeRuntimeCastInputForPendingCast(state, pending, frameMemoryResource);
    if (!castInput)
    {
        return std::nullopt;
    }

    auto selectedSkill = pending.skill;
    if (pending.ultimate)
    {
        selectedSkill.extraProjectileCount = collectFrameExtraProjectileCount(
            makeSelectedCastEffectSources(state, unit.id, true),
            state.random,
            unit.id,
            0,
            true);
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
        pending.ultimate,
        pending.operationType);
    auto actionInput = makeCommittedCastActionInput(state, unit, *castInput, selectedSkill, cast);
    actionInput.normalAttackActType = pending.normalAttackActType;
    return actionInput;
}

std::string toStatusText(const BattleDamageEvent& event)
{
    switch (event.type)
    {
    case BattleDamageEventType::BlockedByFirstHit:
        return "格擋了首輪傷害";
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
    case BattleDamageStatusType::DamageReduceDebuff:
        return withValue("破防");
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
    case BattleDamageEventType::KillRewardApplied:
        gameplay.resourceId = BattleResourceSemanticId::Attack;
        gameplay.type = BattleGameplayEventType::ResourceChanged;
        break;
    case BattleDamageEventType::BlockedByInvincible:
        gameplay.statusId = BattleStatusSemanticId::BlockedByInvincible;
        gameplay.type = BattleGameplayEventType::StatusApplied;
        gameplay.text = toStatusText(event);
        break;
    case BattleDamageEventType::BlockedByFirstHit:
        gameplay.statusId = BattleStatusSemanticId::BlockedByFirstHit;
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
        damage.blockFirstHitsRemaining = runtime->blockFirstHitsRemaining;
        damage.dualWieldBlocksRemaining = runtime->dualWieldBlocksRemaining;
        damage.deathPrevention = runtime->deathPrevention;
        damage.deathPreventionUsed = runtime->deathPreventionUsed;
        damage.deathPreventionFrames = runtime->deathPreventionFrames;
        damage.killHealPct = runtime->killHealPct;
        damage.killInvincFrames = runtime->killInvincFrames;
        damage.bloodlustAttackPerKill = runtime->bloodlustAttackPerKill;
    }
    return damage;
}

void writeBattleDamageRuntimeUnitImpl(BattleDamageRuntimeUnit& runtime, const BattleDamageUnitState& unit)
{
    runtime.hurtInvincFrames = unit.hurtInvincFrames;
    runtime.blockFirstHitsRemaining = unit.blockFirstHitsRemaining;
    runtime.dualWieldBlocksRemaining = unit.dualWieldBlocksRemaining;
    runtime.deathPrevention = unit.deathPrevention;
    runtime.deathPreventionUsed = unit.deathPreventionUsed;
    runtime.deathPreventionFrames = unit.deathPreventionFrames;
    runtime.killHealPct = unit.killHealPct;
    runtime.killInvincFrames = unit.killInvincFrames;
    runtime.bloodlustAttackPerKill = unit.bloodlustAttackPerKill;
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
    const BattleRuntimeState& state,
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
    request.initial.attackerUnitId = command.attackerUnitId;
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

    if (result.heal.amount > 0)
    {
        assert(result.heal.targetUnitId == pulled.id);
        pulled.vitals.hp = std::min(pulled.vitals.maxHp, pulled.vitals.hp + result.heal.amount);
        visualEvents.push_back(roleEffectEvent(
            pulled.id,
            KysChess::EFT_HEAL,
            CoreRoleStatusEffectFrames));
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
    logEvents.insert(
        logEvents.end(),
        result.logEvents.begin(),
        result.logEvents.end());
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
            if (record.hasAlways(EffectType::ForcePullExecute)
                && record.forcePullExecuteRemaining() > 0)
            {
                return true;
            }
        }
        else if (record.hasAlways(EffectType::ForcePullProtect)
            && record.forcePullProtectRemaining() > 0)
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
    bool canTriggerExecute = false,
    bool canTriggerDefenderBlock = false,
    BattleSkillEffectRef skillEffectRef = {})
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
    intent.canTriggerExecute = canTriggerExecute;
    intent.canTriggerDefenderBlock = canTriggerDefenderBlock;
    intent.skillEffectRef = skillEffectRef;
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
    presentation.ultimate = command.ultimate;
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
    request.preResolvedDamage = true;
    request.hitstunFrames = command.frozenFrames;
    request.triggersDefenseEffects = command.triggersDefenseEffects;

    appendFramePendingDamage(
        state,
        pendingDamage,
        std::move(request),
        makeFrameDamagePresentation(state, command),
        command.canTriggerExecute,
        command.canTriggerDefenderBlock,
        command.skillEffectRef);
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
        command.canTriggerDefenderBlock);
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

    appendFramePendingDamage(state, pendingDamage, std::move(request));
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

bool applyFrameTeamEffectCommand(
    BattleRuntimeState& state,
    const BattleTeamHealCommand& command,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    if (state.units.empty())
    {
        return false;
    }

    const int sourceUnitId = command.sourceUnitId;
    assert(sourceUnitId >= 0);

    const auto& source = state.units.requireCore(sourceUnitId);
    if (!source.alive)
    {
        return true;
    }

    auto application = applyBattleTeamHealCommand(state, command);
    logEvents.insert(
        logEvents.end(),
        application.logEvents.begin(),
        application.logEvents.end());
    appendTeamEffectVisualEvents(visualEvents, application.events);
    return true;
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

bool applyFrameTempAttackBuffCommand(
    BattleRuntimeState& state,
    const BattleTempAttackBuffCommand& command,
    std::vector<BattleLogEvent>& logEvents)
{
    auto& unit = state.units.requireCore(command.unitId);

    unit.stats.attack += command.attackBonus;
    unit.stats.defence += command.defenceBonus;
    if (!command.permanent)
    {
        if (command.attackBonus != 0 && command.durationFrames > 0)
        {
            state.units.require(command.unitId).addTempAttackBuff(command.attackBonus, command.durationFrames);
        }
    }
    if (!command.reason.empty())
    {
        appendStatusEventLog(
            logEvents,
            command.unitId,
            command.unitId,
            command.permanent
                ? std::format("{}（攻防+{}）", command.reason, command.attackBonus)
                : command.reason);
    }
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

bool reduceFrameGameplayCommand(
    BattleRuntimeState& state,
    const BattleGameplayCommand& command,
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
    if (const auto* teamHeal = std::get_if<BattleTeamHealCommand>(&command))
    {
        return applyFrameTeamEffectCommand(state, *teamHeal, sinks.logEvents, sinks.visualEvents);
    }
    if (const auto* projectile = std::get_if<BattleProjectileSpawnCommand>(&command))
    {
        sinks.attackSpawns.push_back(projectile->request);
        return true;
    }
    if (std::holds_alternative<BattleNearbyTrackingProjectilesCommand>(command))
    {
        auto followUps = expandBattleProjectileFollowUpCommands(
            std::span(&command, 1),
            state.projectileFollowUps,
            state.units);
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
            autoUltimate->unitId,
            autoUltimate->consumeMp,
            autoUltimate->announce,
            pending.get_allocator().resource(),
            attackSoundIds,
            sinks.attackSpawns,
            sinks.pendingDamage,
            sinks.gameplayEvents,
            sinks.logEvents,
            sinks.visualEvents);
    }
    if (const auto* knockback = std::get_if<BattleKnockbackCommand>(&command))
    {
        applyKnockbackImpulse(state, *knockback);
        return true;
    }
    if (const auto* tempAttack = std::get_if<BattleTempAttackBuffCommand>(&command))
    {
        return applyFrameTempAttackBuffCommand(state, *tempAttack, sinks.logEvents);
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
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        currentFrameSinks(frame));
}

void reduceCommandsBeforeAttacks(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        currentFrameSinks(frame));
}

void reduceCommandsAfterAttackHits(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        afterAttackHitSinks(state, frame));
}

void reduceCommandsAfterDamageLifecycle(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        afterDamageLifecycleSinks(state, frame));
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
    case BattleDamageStatusType::DamageReduceDebuff:
        return withValue("破防");
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

    const int currentStacks = std::max(event.value, transaction.defenderStatus.effects.bleedStacks);
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

void appendFrameDeathAoeProjectiles(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleDamageTransactionResult& transaction,
    int deadUnitId)
{
    const auto& dead = state.units.require(deadUnitId);
    const auto* deathAoe = dead.firstAlways(EffectType::DeathAOE);
    if (!deathAoe || deathAoe->value <= 0)
    {
        return;
    }

    const int damage = std::max(1, transaction.defender.vitals.maxHp * deathAoe->value / 100);
    frame.mutableAreaProjectileFollowUps().push_back({
        deadUnitId,
        7,
        transaction.attacker.id,
        deathAoe->value2,
        KysChess::EFT_DEATH_BLAST,
        damage,
        deathAoe->value,
        deathAoe->duration,
        "殉爆",
        std::format("殉爆{}%（{}幀）", deathAoe->value, deathAoe->duration),
    });
}

void appendFrameDeathEffectOutputs(BattleFrameContext& frame, const std::vector<BattleDeathEffectEvent>& events)
{
    for (const auto& event : events)
    {
        switch (event.type)
        {
        case BattleDeathEffectEventType::AllyStatBoost:
            appendStatusEventLog(
                frame.logEvents,
                event.targetUnitId,
                event.targetUnitId,
                std::format("同袍之死（攻防+{}）", event.value));
            break;
        case BattleDeathEffectEventType::DeathMedicalHeal:
            appendHealEventLog(
                frame.logEvents,
                event.sourceUnitId,
                event.targetUnitId,
                event.value,
                "死亡醫療");
            frame.visualEvents.push_back(roleEffectEvent(
                event.targetUnitId,
                KysChess::EFT_HEAL,
                CoreRoleStatusEffectFrames));
            break;
        case BattleDeathEffectEventType::ShieldOnAllyDeath:
            appendStatusEventLog(
                frame.logEvents,
                event.sourceUnitId,
                event.targetUnitId,
                formatStatusValue("護盾重獲", event.value, "護盾"));
            break;
        }
    }
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
    modifier.poisonTimer = effects.poisonTimer;
    modifier.outgoingDamageReduceDebuffs.clear();
    for (const auto& debuff : effects.damageReduceDebuffs)
    {
        modifier.outgoingDamageReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
    }
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
    const BattleDamageRequest& request,
    bool blockFirstHitWithoutConsuming = false)
{
    assert(request.defenderUnitId >= 0);

    BattleDamageTransactionInput transaction;
    transaction.request = request;
    transaction.blockFirstHitWithoutConsuming = blockFirstHitWithoutConsuming;

    const auto& defender = state.units.require(request.defenderUnitId);
    transaction.defender = defender.damageState();
    transaction.defenderModifiers = defender.damageModifiers();
    transaction.defenderStatus = defender.statusDamageState();
    applyLiveStatusToDamageModifier(defender.statusEffects(), transaction.defenderModifiers);
    transaction.defenderCooldown = makeBattleFrameCooldownState(defender.core);

    if (request.attackerUnitId != OptionalDamageAttackerUnitId)
    {
        assert(request.attackerUnitId >= 0);
        const auto& attacker = state.units.require(request.attackerUnitId);
        transaction.attacker = attacker.damageState();
        transaction.attackerModifiers = attacker.damageModifiers();
        applyLiveStatusToDamageModifier(attacker.statusEffects(), transaction.attackerModifiers);
    }
    else
    {
        transaction.attacker.id = OptionalDamageAttackerUnitId;
    }

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

    if (transaction.blockedByFirstHit)
    {
        appendAttackBlock("格擋了首輪傷害", BattleStatusSemanticId::BlockedByFirstHit);
    }
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

        appendFrameDeathAoeProjectiles(state, frame, transaction, event.targetUnitId);
        auto deathEvents = state.deathEffects.store.applyAllyDeathEffects(
            state.units,
            event.targetUnitId);
        appendFrameDeathEffectOutputs(frame, deathEvents);
    }
    return deadUnitIds;
}

void appendFrameDamageKillRewardLogEvents(
    BattleFrameContext& frame,
    const BattleDamageTransactionResult& transaction)
{
    if (!transaction.killed)
    {
        return;
    }

    if (transaction.attackerDelta.hpDelta > 0)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Heal;
        log.sourceUnitId = transaction.attacker.id;
        log.targetUnitId = transaction.attacker.id;
        log.amount = transaction.attackerDelta.hpDelta;
        log.segments = transaction.attacker.killHealPct > 0
            ? logSegments<BattleLogTextTone::Positive>(
                "擊殺回復 ",
                std::pair{ BattleLogTextTone::HealValue, std::format("{}%", transaction.attacker.killHealPct) })
            : battleLogText("擊殺回復", BattleLogTextTone::Positive);
        frame.logEvents.push_back(std::move(log));
    }
    if (transaction.attackerDelta.invincibleDelta > 0)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = transaction.attacker.id;
        log.targetUnitId = transaction.attacker.id;
        log.amount = transaction.attackerDelta.invincibleDelta;
        log.segments = logStatusFrames<BattleLogTextTone::Positive>(
            "擊殺無敵",
            transaction.attackerDelta.invincibleDelta);
        frame.logEvents.push_back(std::move(log));
    }
    if (transaction.attackerDelta.attackDelta > 0)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = transaction.attacker.id;
        log.targetUnitId = transaction.attacker.id;
        log.amount = transaction.attackerDelta.attackDelta;
        log.segments = logSegments<BattleLogTextTone::Positive>(
            "嗜血（+",
            std::pair{ BattleLogTextTone::DamageValue, transaction.attackerDelta.attackDelta },
            "攻）");
        frame.logEvents.push_back(std::move(log));
    }
}

void appendFrameShieldBreakCommands(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleDamageTransactionResult& transaction)
{
    if (!(transaction.shieldAbsorbed > 0
            && transaction.defenderDelta.shieldDelta < 0
            && transaction.defender.shield == 0))
    {
        return;
    }

    auto& defenderCombo = state.units.require(transaction.defender.id).combo;
    int shieldExplosionPct = 0;
    auto shieldBreakEvents = BattleComboTriggerSystem().collectTriggerEvents(
        defenderCombo,
        { BattleComboTriggerHook::ShieldBreak, transaction.defender.id, transaction.defender.id },
        {
            EffectType::ShieldExplosion,
            EffectType::AutoUltimate,
            EffectType::TempFlatATK,
            EffectType::MPRestore,
        },
        state.random,
        BattleComboActivationRecording::CallerRecords);
    for (const auto& event : shieldBreakEvents)
    {
        bool activated = true;
        switch (event.effect.type)
        {
        case EffectType::ShieldExplosion:
            shieldExplosionPct = std::max(shieldExplosionPct, event.effect.value);
            break;
        case EffectType::AutoUltimate:
            frame.queueCommand(BattleAutoUltimateCommand{ transaction.defender.id, false });
            break;
        case EffectType::TempFlatATK:
            frame.queueCommand(BattleTempAttackBuffCommand{
                transaction.defender.id,
                event.effect.value,
                event.effect.duration,
                std::format("護盾爆炸（臨時攻+{}，{}幀）", event.effect.value, event.effect.duration),
            });
            break;
        case EffectType::MPRestore:
        {
            const auto& unit = state.units.requireCore(transaction.defender.id);
            int restored = std::min(
                event.effect.value,
                std::max(0, unit.vitals.maxMp - unit.vitals.mp));
            if (restored > 0)
            {
                frame.mutableLateMpRestores().push_back({
                    transaction.defender.id,
                    restored,
                    std::format("護盾爆炸·回內力+{}", restored),
                });
            }
            else
            {
                activated = false;
            }
            break;
        }
        default:
            assert(false);
        }

        if (activated)
        {
            BattleComboTriggerSystem().recordActivation(
                defenderCombo,
                event.effectId);
        }
    }
    if (shieldExplosionPct > 0)
    {
        frame.visualEvents.push_back(roleEffectEvent(
            transaction.defender.id,
            KysChess::EFT_SHIELD_BLAST,
            CoreRoleStatusEffectFrames));
        const int defenderShieldPct = defenderCombo.sumAlways(EffectType::ShieldPctMaxHP);
        int explosionDamage = std::max(
            1,
            defenderShieldPct * transaction.defender.vitals.maxHp / 100 * shieldExplosionPct / 100);
        frame.mutableAreaProjectileFollowUps().push_back({
            transaction.defender.id,
            5,
            -1,
            0,
            KysChess::EFT_SHIELD_BLAST,
            explosionDamage,
            0,
            0,
            "護盾爆炸",
            std::format("護盾爆炸（{}傷害）", explosionDamage),
        });
    }
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
    appendProjectileFollowUpsToFrame(
        frame,
        expandBattleProjectileFollowUpCommands(
            pendingCommands,
            state.projectileFollowUps,
            state.units));
    auto areaFollowUps = frame.drainAreaProjectileFollowUps();
    for (const auto& followUp : areaFollowUps)
    {
        appendProjectileFollowUpsToFrame(
            frame,
            expandBattleAreaProjectileFollowUp(
                followUp,
                state.projectileFollowUps,
                state.units));
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

bool comboEffectIsApplied(const KysChess::RoleComboState& state, int comboId)
{
    return state.hasComboApplied(comboId);
}

bool unitBelongsToCombo(const BattleDeathEffectExtras& extras, int comboId)
{
    return std::ranges::find(extras.comboIds, comboId) != extras.comboIds.end();
}

bool isAntiComboId(const BattleDeathEffectStore& store, int comboId)
{
    return comboId >= 0 && store.regularSynergyComboIds.count(comboId) == 0;
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
        const auto& extras = state.units.require(candidate.id).deathEffects;
        if (!unitBelongsToCombo(extras, comboId))
        {
            continue;
        }
        const auto& combo = state.units.require(candidate.id).combo;
        if (comboEffectIsApplied(combo, comboId))
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
    int deadUnitId,
    std::vector<BattleLogEvent>& logEvents)
{
    const auto& deadExtras = state.units.require(deadUnitId).deathEffects;
    const auto& deadCombo = state.units.require(deadUnitId).combo;
    const auto& dead = state.units.require(deadUnitId);
    for (int comboId : deadExtras.comboIds)
    {
        if (!isAntiComboId(state.deathEffects.store, comboId)
            || !dead.hasComboApplied(comboId))
        {
            continue;
        }

        auto* target = findAntiComboTransferTarget(state, deadUnitId, comboId);
        if (!target)
        {
            continue;
        }

        auto& targetRecord = state.units.require(target->id);
        for (RoleComboEffectId effectId : deadCombo.idsFromCombo(comboId))
        {
            const auto& effect = deadCombo.effect(effectId);
            targetRecord.grantRuntimeComboEffect(effect, comboId);
            state.units.require(target->id).transferDeathAppliedEffect(effect);
        }
        logEvents.push_back(makeAntiComboTransferLog(deadUnitId, target->id));
    }
}

void applyRuntimeDeathComboConsequences(
    BattleRuntimeState& state,
    const std::vector<int>& deadUnitIds,
    std::vector<BattleLogEvent>& logEvents)
{
    for (int deadUnitId : deadUnitIds)
    {
        state.units.require(deadUnitId).clearAllPending();
        applyRuntimeAntiComboTransfer(state, deadUnitId, logEvents);
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
        request.preResolvedDamage = true;
        BattleDamagePresentationInput presentation;
        presentation.segments = battleLogText(event.reason, BattleLogTextTone::SkillName);
        applyStatusTickDamagePresentation(state, event.type, event.unitId, presentation);
        appendFramePendingDamage(state, pendingDamage, std::move(request), std::move(presentation));
    }
    return std::move(statusTick.events);
}

void appendTeamEffectLogEvents(
    std::vector<BattleLogEvent>& logEvents,
    const std::vector<BattleTeamEffectEvent>& events,
    const std::string& reason)
{
    for (const auto& event : events)
    {
        BattleLogEvent log;
        log.sourceUnitId = event.sourceUnitId;
        log.targetUnitId = event.targetUnitId;
        log.amount = event.value;
        log.segments = battleLogText(reason, BattleLogTextTone::SkillName);
        switch (event.type)
        {
        case BattleTeamEffectEventType::Heal:
            log.type = BattleLogEventType::Heal;
            log.segments = battleLogText(reason, BattleLogTextTone::SkillName);
            break;
        case BattleTeamEffectEventType::MpRestore:
            log.type = BattleLogEventType::Status;
            log.segments = battleLogText(std::format("{}+{}MP", reason, event.value), BattleLogTextTone::SkillName);
            break;
        case BattleTeamEffectEventType::MpDamage:
            log.type = BattleLogEventType::Status;
            log.segments = battleLogText(std::format("{}-{}MP", reason, event.value), BattleLogTextTone::SkillName);
            break;
        case BattleTeamEffectEventType::ShieldGain:
            log.type = BattleLogEventType::Status;
            log.segments = battleLogText(formatStatusValue(reason, event.value, "護盾"), BattleLogTextTone::SkillName);
            break;
        case BattleTeamEffectEventType::CooldownReduced:
            log.type = BattleLogEventType::Status;
            log.segments = battleLogText(formatStatusValue(reason, event.value, "冷卻"), BattleLogTextTone::SkillName);
            break;
        }
        logEvents.push_back(std::move(log));
    }
}

void appendTeamEffectVisualEvents(
    std::vector<BattleVisualEvent>& visualEvents,
    const std::vector<BattleTeamEffectEvent>& events)
{
    for (const auto& event : events)
    {
        switch (event.type)
        {
        case BattleTeamEffectEventType::Heal:
            visualEvents.push_back(roleEffectEvent(
                event.targetUnitId,
                KysChess::EFT_HEAL,
                CoreRoleStatusEffectFrames));
            break;
        case BattleTeamEffectEventType::MpRestore:
        case BattleTeamEffectEventType::MpDamage:
        case BattleTeamEffectEventType::ShieldGain:
        case BattleTeamEffectEventType::CooldownReduced:
            break;
        }
    }
}

void applyRuntimeTeamEvents(
    BattleRuntimeState& state,
    int sourceUnitId,
    const BattleComboFrameRuntimeEvent& event,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    if (state.units.empty())
    {
        return;
    }

    const auto& source = state.units.requireCore(sourceUnitId);
    if (!source.alive)
    {
        return;
    }

    BattleTeamEffectSystem system;
    std::vector<BattleTeamEffectEvent> events;
    std::string reason;
    switch (event.type)
    {
    case BattleComboFrameRuntimeEventType::SelfHpRegen:
        events = system.applySelfHeal(state.units, sourceUnitId, event.value);
        reason = "生命回復";
        break;
    case BattleComboFrameRuntimeEventType::HealAura:
        assert(state.teamEffects.healAuraRadius > 0.0);
        events = system.applyHealAura(
            state.units,
            sourceUnitId,
            event.value,
            event.value2,
            state.teamEffects.healAuraRadius,
            event.durationFrames);
        reason = "治療光環";
        break;
    case BattleComboFrameRuntimeEventType::HealPercentSelf:
        events = system.applySelfHeal(state.units, sourceUnitId, event.value);
        reason = "爆發治療";
        break;
    default:
        assert(false);
        break;
    }

    appendTeamEffectLogEvents(logEvents, events, reason);
    appendTeamEffectVisualEvents(visualEvents, events);
}


void applyBroadcastTriggerTimer(BattleRuntimeState& state, int sourceUnitId, const BattleComboFrameRuntimeEvent& event)
{
    assert(event.durationFrames > 0);
    assert(event.type == BattleComboFrameRuntimeEventType::BroadcastTriggerTimer);
    assert(event.timerKey.trigger == event.trigger);

    const auto& source = state.units.requireCore(sourceUnitId);
    if (!source.alive)
    {
        return;
    }

    for (auto& record : state.units.live())
    {
        const auto& unit = record.core;
        if (unit.id == sourceUnitId || unit.team != source.team)
        {
            continue;
        }
        if (!record.combo.ownsTriggerTimer(event.timerKey))
        {
            continue;
        }
        record.combo.extendTriggerTimer(event.timerKey, event.durationFrames);
    }
}

void applyCastPostSkillInvincibility(
    BattleRuntimeState& state,
    int sourceUnitId,
    int frames,
    std::vector<BattleLogEvent>& logEvents)
{
    if (frames <= 0)
    {
        return;
    }

    auto& unit = state.units.requireCore(sourceUnitId);
    const int before = unit.invincible;
    unit.invincible = std::max(unit.invincible, frames);
    const int added = unit.invincible - before;
    if (added <= 0)
    {
        return;
    }

    BattleLogEvent log;
    log.type = BattleLogEventType::Status;
    log.sourceUnitId = sourceUnitId;
    log.targetUnitId = sourceUnitId;
    log.amount = added;
    log.segments = logStatusFrames("技能後無敵", added);
    logEvents.push_back(std::move(log));
}

BattleFrameVector<BattleGameplayCommand> applyRuntimeComboEvents(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::span<const BattleRuntimeUnitFrameCommit> runtimeCommits)
{
    BattleFrameVector<BattleGameplayCommand> deferredCommands(frame.frameMemoryResource());
    for (const auto& result : runtimeCommits)
    {
        bool autoUltimateReady = false;
        for (const auto& event : result.comboEvents)
        {
            switch (event.type)
            {
            case BattleComboFrameRuntimeEventType::SelfHpRegen:
            case BattleComboFrameRuntimeEventType::HealAura:
            case BattleComboFrameRuntimeEventType::HealPercentSelf:
                applyRuntimeTeamEvents(
                    state,
                    result.unitId,
                    event,
                    frame.logEvents,
                    frame.visualEvents);
                break;
            case BattleComboFrameRuntimeEventType::BroadcastTriggerTimer:
                applyBroadcastTriggerTimer(state, result.unitId, event);
                break;
            case BattleComboFrameRuntimeEventType::AutoUltimateReady:
                autoUltimateReady = true;
                break;
            }
        }
        if (autoUltimateReady)
        {
            deferredCommands.push_back(BattleAutoUltimateCommand{ result.unitId, false, true });
        }
    }
    return deferredCommands;
}

void applySkillFinishedTeamHeals(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::span<const BattleSkillFinishedTeamHeal> heals)
{
    BattleTeamEffectSystem system;
    for (const auto& heal : heals)
    {
        const auto& source = state.units.requireCore(heal.sourceUnitId);
        assert(source.alive);
        auto events = system.applyTeamHeal(
            state.units,
            heal.sourceUnitId,
            heal.flatHeal,
            heal.pctHeal);
        appendTeamEffectLogEvents(frame.logEvents, events, "技能群療");
        appendTeamEffectVisualEvents(frame.visualEvents, events);
    }
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
    if (record.combo.hasAlways(EffectType::DashAttack))
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
            assert(operation >= 0 && operation < static_cast<int>(state.movementPhysics.actionCastFrames.size()));
            const int dashStartFrame = state.movementPhysics.actionCastFrames[operation];
            const int dashEndFrame = dashStartFrame + state.movementPhysics.dashMomentumFrames;
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
        movementSnapshot.taXue = record.combo.hasAlways(EffectType::DashAttack);
        movementSnapshot.velocity = result.state.velocity;
        movementSnapshot.dashFramesRemaining = result.state.movementDashFrames;
        movementSnapshot.dashCooldownRemaining = result.state.movementDashCooldown;
        movementSnapshot.movementDashSpreadFramesRemaining = result.state.movementDashSpreadFrames;
        movementSnapshot.postDashRetreatFramesRemaining = result.state.postDashRetreatFrames;
        movementSnapshot.postDashChaosFramesRemaining = result.state.postDashChaosFrames;
        movementSnapshot.knockbackFramesRemaining = result.state.knockbackFrames;
        movementSnapshot.knockbackControlFramesRemaining = result.state.knockbackControlFrames;
        physicsInput.ignoreUnitCollision = !unit.alive || battleMovementTaXueUnstable(movementSnapshot);

        result.state = BattleMovementPhysicsSystem().advance(physicsInput);
        result.physicsAdvanced = true;

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
    auto actionState = makeActionRuntimeState(unit.core);
    resetActionFrameState(actionState);
    commitActionFrameStateToRuntime(unit.core, actionState);
    unit.clearAllPending();
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
    assert(static_cast<std::size_t>(operationIndex) < state.action.castFrames.size());
    return state.action.castFrames[operationIndex];
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
    return operationType == BattleOperationType::Dash
        ? state.action.dashRecoveryFrames
        : state.action.actionRecoveryFrames;
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
            && !actionMovementDashActive(state, unit.id))
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

        if (!actionState.haveAction && runtimeCastPlan)
        {
            auto castInput = refreshedCastInput(state, unitRecord, movement, std::move(*runtimeCastPlan));
            castInput.unit.canStartAttack = castInput.unit.canStartAttack
                && unit.animation.cooldown == 0;
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
                unitRecord.setPendingCast(makePendingCastAction(castInput, cast, castFrame));
                unitRecord.setSkillCooldownUltimate(cast.decision.ultimate);
                if (cast.decision.ultimate)
                {
                    unitRecord.markUltimateCaster();
                }
                unitRecord.movement.physics.movementDashSpreadFrames = 0;
            }
        }
        else if (actionState.haveAction && pendingCast)
        {
            assert(pendingCast->castFrame > 0);
            const int castFrame = pendingCast->castFrame;
            if (actionState.actFrame == castFrame)
            {
                actionCommitted = true;
                auto maybeActionInput = tryMakeRuntimeActionCommitInput(
                    state,
                    movement,
                    *pendingCast,
                    frame.frameMemoryResource());
                auto& combo = unitRecord.combo;
                unitRecord.clearPendingCast();
                if (maybeActionInput)
                {
                    actionInput = std::move(*maybeActionInput);
                    unit.motion.facing = actionInput.committedFacing;
                    actionResult = BattleActionCommitSystem().commit(actionInput, combo, state.units);
                    if (actionInput.hasCast)
                    {
                        markSelectedCastFinishPending(
                            state,
                            unit.id,
                            actionInput.cast.decision.ultimate);
                    }
                }
                else
                {
                    actionResult.operationCount = unit.operationCount;
                    resetActionFrameState(actionState);
                    cancelledAction = true;
                }
                if (actionInput.hasCast)
                {
                    auto castSources = makeSelectedCastEffectSources(
                        state,
                        unit.id,
                        actionInput.cast.decision.ultimate);
                    schedulePostDashRetreat(state, unit.id, actionInput.cast);
                    applyCastPostSkillInvincibility(
                        state,
                        unit.id,
                        collectFramePostSkillInvincFrames(
                            castSources,
                            state.random,
                            unit.id,
                            actionInput.cast.decision.ultimate),
                        logEvents);
                }
                if (actionInput.hasCast && actionInput.cast.decision.soundId >= 0)
                {
                    frame.attackSoundIds.push_back(actionInput.cast.decision.soundId);
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
                if (actionInput.hasCast)
                {
                    auto castSources = makeSelectedCastEffectSources(
                        state,
                        unit.id,
                        actionInput.cast.decision.ultimate);
                    auto events = collectFrameCastScopedComboEvents(
                        castSources,
                        state.random,
                        unit.id,
                        actionInput.cast.decision.ultimate);
                    if (!events.empty())
                    {
                        frame.castScopedComboEffects.push_back({
                            unit.id,
                            committedCastProjectileSpeed(actionResult, state.projectileFollowUps.projectileSpeed),
                            std::move(events),
                        });
                    }
                }
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
    assert(request.initial.attackerUnitId >= 0);
    assert(!request.initial.skillName.empty());
    assert(request.attackerDualWieldBlockGainChancePct <= 100);

    auto& attacker = state.units.require(request.initial.attackerUnitId);
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

void advanceAttacksAndResolveHits(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    auto& attackEvents = frame.attackEvents;
    auto& frameCommands = frame.mutableCommandsForReducer();
    auto& logEvents = frame.logEvents;
    auto& visualEvents = frame.visualEvents;

    state.attacks.frame = state.movement.frame;
    auto attackSpawns = frame.drainCurrentFrameAttacks();
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
        applyAttackSpawnAttackerDualWieldBlockGain(state, request, logEvents);
        attackEvents.push_back(state.attacks.spawn(request));
    }
    state.nextFrame.recycleAttacks(std::move(attackSpawns));
    state.attacks.tick(state.units, attackEvents);
    applyProjectileCancelDamageResults(state, attackEvents);
    appendProjectileCancellationLogEvents(state.attacks, attackEvents, logEvents, false);
    resolveHitEvents(
        state,
        attackEvents,
        frameCommands,
        logEvents,
        visualEvents);
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

    auto attackerSources = makeBattleEffectSources(state, request.attackerUnitId, intent.skillEffectRef);
    const auto& defender = state.units.requireCore(request.defenderUnitId);
    const BattleExecuteComboInput executeInput{
            request.attackerUnitId,
            request.defenderUnitId,
            defender.vitals.hp,
            defender.vitals.maxHp,
            request.baseDamage,
            true,
    };
    const auto execute = BattleComboTriggerSystem().resolveExecuteCombo(
        attackerSources,
        executeInput,
        state.random);
    if (!execute.executed)
    {
        return false;
    }

    request.canExecute = true;
    request.executeThresholdPct = execute.thresholdPct;
    presentation.executed = true;
    appendDamagePresentationDetail(presentation, "處決");
    appendStatusEventLog(
        frame.logEvents,
        request.attackerUnitId,
        request.defenderUnitId,
        formatExecuteStatus(execute.thresholdPct));
    return true;
}

bool applyFrameDefenderBlockCommands(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleDamageRequest& request)
{
    assert(request.attackerUnitId >= 0);
    assert(request.defenderUnitId >= 0);

    const auto& defenderCombo = state.units.require(request.defenderUnitId).combo;
    const int counterUltimateBlockChancePct = defenderCombo.maxAlways(EffectType::CounterUltimateBlock);
    const bool counterUltimateBlock = counterUltimateBlockChancePct > 0
        && (counterUltimateBlockChancePct >= 100 || state.random.chance(counterUltimateBlockChancePct));
    const int blockChancePct = defenderCombo.sumAlways(EffectType::BlockChance);
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
    const bool executed = intent.canTriggerExecute
        && applyFrameExecuteReaction(state, frame, intent, request, presentation);
    if (!executed
        && intent.canTriggerDefenderBlock
        && applyFrameDefenderBlockCommands(state, frame, request))
    {
        return false;
    }
    return true;
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
    auto pendingDamageIndexes = orderedFramePendingDamageIndexes(
        pendingDamage,
        state.damage.sortPendingDamageByDefenderMagnitude,
        frame.frameMemoryResource());
    for (const auto pendingIndex : pendingDamageIndexes)
    {
        const auto& intent = pendingDamage[pendingIndex];
        if (!state.units.requireCore(intent.request.defenderUnitId).alive)
        {
            continue;
        }

        auto request = intent.request;
        auto presentation = intent.presentation;
        if (!applyFramePendingHitReactions(state, frame, intent, request, presentation))
        {
            continue;
        }

        auto transaction = BattleDamageSystem().resolveTransaction(
            makeFrameDamageTransactionInput(
                state,
                request,
                frame.firstHitBlockActiveForFrame(request.defenderUnitId)));
        if (transaction.blockedByFirstHit)
        {
            frame.activateFirstHitBlockForFrame(transaction.defender.id);
        }
        applyFrameDamageTakenMpGain(transaction);
        applyDamageResultToFrameState(state, transaction, frameStartMotion);
        appendFrameShieldBreakCommands(state, frame, transaction);
        appendFrameDamageOutputEvents(frame, presentation, transaction);
        appendFrameDamagePreDeathLogEvents(frame, transaction);
        appendFrameDamageResourceLogEvents(frame, transaction);
        appendFrameDamageGameplayEvents(frame, transaction, presentation.skillId);
        auto transactionDeadUnitIds = appendFrameDamageLifecycle(state, frame, transaction);
        appendFrameDamageKillRewardLogEvents(frame, transaction);
        applyRescueRepositionForDamage(state, transaction, logEvents, visualEvents);

        if (!transactionDeadUnitIds.empty())
        {
            unitDied = true;
            deadUnitIds.insert(
                deadUnitIds.end(),
                transactionDeadUnitIds.begin(),
                transactionDeadUnitIds.end());
        }
    }

    if (unitDied)
    {
        applyRuntimeDeathComboConsequences(state, deadUnitIds, logEvents);
        cancelDeadRuntimeActions(state);
        appendEnemyTopDebuffUpdates(state, logEvents);
    }

    updateFrameBattleResultAfterDamage(state, frame);
    expandFrameDamageFollowUpCommands(state, frame);
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

void pruneFinishedRuntimeAttacks(BattleRuntimeState& state)
{
    state.attacks.attacks.erase(
        std::remove_if(
            state.attacks.attacks.begin(),
            state.attacks.attacks.end(),
            [](const BattleAttackInstance& attack)
            {
                return attack.frame >= attack.state.totalFrame;
            }),
        state.attacks.attacks.end());
}
}  // namespace

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

BattleTeamEffectCommandApplication applyBattleTeamHealCommand(
    BattleRuntimeState& state,
    const BattleTeamHealCommand& command)
{
    BattleTeamEffectCommandApplication result;
    BattleTeamEffectSystem system;
    assert(command.sourceUnitId >= 0);
    result.events = system.applyTeamHeal(
        state.units,
        command.sourceUnitId,
        command.flatHeal,
        command.pctHeal);
    appendTeamEffectLogEvents(result.logEvents, result.events, command.reason);
    return result;
}

BattleProjectileBouncePrime collectFrameProjectileBouncePrime(
    const BattleEffectSources& sources,
    int attackerUnitId,
    int rollPct,
    int defaultRange)
{
    return BattleComboTriggerSystem().collectProjectileBouncePrime(
        sources,
        { attackerUnitId, rollPct, defaultRange });
}

int collectFrameExtraProjectileCount(
    const BattleEffectSources& sources,
    BattleRuntimeRandom& random,
    int unitId,
    int baseCount,
    bool ultimate)
{
    return BattleComboTriggerSystem().collectExtraProjectileCount(
        sources,
        { BattleComboTriggerHook::AfterSkillCast, unitId, -1, ultimate, true },
        baseCount,
        random);
}

int collectFramePostSkillInvincFrames(
    const BattleEffectSources& sources,
    BattleRuntimeRandom& random,
    int unitId,
    bool ultimate)
{
    int frames = BattleEffectReader().maxAlways(sources, EffectType::PostSkillInvincFrames);
    for (const auto& event : BattleEffectReader().collectTriggerEvents(
             sources,
             { BattleComboTriggerHook::AfterSkillCast, unitId, -1, ultimate, true },
             { EffectType::PostSkillInvincFrames },
             random))
    {
        frames = std::max(frames, event.effect.value);
    }
    return frames;
}

bool frameComboHasExecute(const KysChess::RoleComboState& state, int attackerUnitId)
{
    return BattleComboTriggerSystem().hasExecuteCombo(state, attackerUnitId);
}

BattleFixed resolveFrameArmorPenetratedDefense(
    const BattleEffectSources& sources,
    int attackerUnitId,
    int targetUnitId,
    BattleFixed defense,
    bool ultimate,
    bool mainProjectile,
    BattleRuntimeRandom& random)
{
    return BattleComboTriggerSystem().resolveArmorPenetratedDefense(
        sources,
        { attackerUnitId, targetUnitId, defense, ultimate, mainProjectile },
        random).defense;
}

void applyCurrentHpBlastCastEffect(
    BattleRuntimeState& state,
    int sourceUnitId,
    int damagePct,
    const std::string& reason,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::vector<BattleLogEvent>&,
    std::vector<BattleVisualEvent>&)
{
    const auto& source = state.units.requireCore(sourceUnitId);
    for (const auto& record : state.units.live())
    {
        const auto& unit = record.core;
        if (unit.team == source.team)
        {
            continue;
        }
        tryAppendFrameDamageTransaction(
            state,
            pendingDamage,
            BattleHpDamageCommand{
                sourceUnitId,
                unit.id,
                std::max(1, unit.vitals.hp * damagePct / 100),
                false,
                false,
                false,
                false,
                0,
                "",
                battleLogText(reason, BattleLogTextTone::SkillName),
                false,
            });
    }
}

BattleSkillEffectRef skillEffectRefForTriggerEvent(const BattleEffectTriggerEvent& event)
{
    if (event.effectRef.store.kind != BattleEffectSourceKind::Skill)
    {
        return {};
    }
    assert(event.effectRef.store.slot != BattleSkillSlot::None);
    return { event.sourceUnitId, event.effectRef.store.slot };
}

void applySpiralBleedCastEffect(
    BattleRuntimeState& state,
    int sourceUnitId,
    BattleSkillEffectRef skillEffectRef,
    int bleedStacks,
    int projectileCount,
    double projectileSpeed,
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    std::vector<BattleLogEvent>&,
    std::vector<BattleVisualEvent>&)
{
    const auto& source = state.units.requireCore(sourceUnitId);
    const auto sourcePosition = source.motion.position;
    const int sharedHitGroupId = state.projectileFollowUps.nextSharedHitGroupId++;
    const int count = std::max(1, projectileCount);
    const double speed = projectileSpeed > 0.0
        ? projectileSpeed
        : state.projectileFollowUps.projectileSpeed;
    for (int i = 0; i < count; ++i)
    {
        BattleAttackSpawnRequest request;
        request.initial.attackerUnitId = sourceUnitId;
        request.initial.operationType = BattleOperationType::RangedProjectile;
        request.initial.visualEffectId = 48;
        request.initial.position = sourcePosition;
        request.initial.totalFrame = 35;
        request.initial.scriptedBleedStacks = bleedStacks;
        request.initial.skillEffectRef = skillEffectRef;
        request.initial.sharedHitGroupId = sharedHitGroupId;
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

std::vector<BattleEffectTriggerEvent> collectFrameCastScopedComboEvents(
    const BattleEffectSources& sources,
    BattleRuntimeRandom& random,
    int unitId,
    bool ultimate)
{
    assert(unitId >= 0);

    return BattleEffectReader().collectTriggerEvents(
        sources,
        { BattleComboTriggerHook::AfterSkillCast, unitId, -1, ultimate, true },
        {
            KysChess::EffectType::CurrentHPPctBlast,
            KysChess::EffectType::TeamMPRestore,
            KysChess::EffectType::EnemyMpDamageAll,
            KysChess::EffectType::FlatShield,
            KysChess::EffectType::SpiralBleedProjectile,
            KysChess::EffectType::MPRestore,
            KysChess::EffectType::ControlImmunityFrames,
            KysChess::EffectType::LowestAllyHeal,
        },
        random);
}

void applyFrameCastScopedComboEffects(
    BattleRuntimeState& state,
    const BattleFrameCastScopedComboEffects& effects,
    std::vector<BattleAttackSpawnRequest>& attackSpawns,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    assert(effects.unitId >= 0);
    assert(effects.projectileSpeed >= 0.0);

    BattleTeamEffectSystem teamEffects;
    for (const auto& event : effects.events)
    {
        assert(event.effect.value > 0);
        switch (event.effect.type)
        {
        case KysChess::EffectType::CurrentHPPctBlast:
            applyCurrentHpBlastCastEffect(
                state,
                effects.unitId,
                event.effect.value,
                "當前生命傷害",
                pendingDamage,
                logEvents,
                visualEvents);
            break;
        case KysChess::EffectType::TeamMPRestore:
        {
            auto teamEvents = teamEffects.applyTeamMp(
                state.units,
                effects.unitId,
                event.effect.value);
            appendTeamEffectLogEvents(logEvents, teamEvents, "琴棋書畫");
            appendTeamEffectVisualEvents(visualEvents, teamEvents);
            break;
        }
        case KysChess::EffectType::EnemyMpDamageAll:
        {
            auto teamEvents = teamEffects.applyEnemyMpDamageAll(
                state.units,
                effects.unitId,
                event.effect.value);
            appendTeamEffectLogEvents(logEvents, teamEvents, "全場殺內");
            appendTeamEffectVisualEvents(visualEvents, teamEvents);
            break;
        }
        case KysChess::EffectType::FlatShield:
        {
            auto teamEvents = teamEffects.applyTeamShield(
                state.units,
                effects.unitId,
                event.effect.value,
                true);
            appendTeamEffectLogEvents(logEvents, teamEvents, "全隊護盾重整");
            appendTeamEffectVisualEvents(visualEvents, teamEvents);
            break;
        }
        case KysChess::EffectType::SpiralBleedProjectile:
            applySpiralBleedCastEffect(
                state,
                effects.unitId,
                skillEffectRefForTriggerEvent(event),
                event.effect.value,
                event.effect.value2 > 0 ? event.effect.value2 : 6,
                effects.projectileSpeed,
                attackSpawns,
                logEvents,
                visualEvents);
            break;
        case KysChess::EffectType::MPRestore:
        {
            auto& unit = state.units.requireCore(effects.unitId);
            const int before = unit.vitals.mp;
            applyRuntimeUnitMpDelta(state, unit, event.effect.value);
            if (unit.vitals.mp > before)
            {
                std::vector<BattleTeamEffectEvent> teamEvents = {
                    { BattleTeamEffectEventType::MpRestore,
                      effects.unitId,
                      effects.unitId,
                      unit.vitals.mp - before,
                      before,
                      unit.vitals.mp },
                };
                appendTeamEffectLogEvents(logEvents, teamEvents, "武功回內");
                appendTeamEffectVisualEvents(visualEvents, teamEvents);
            }
            break;
        }
        case KysChess::EffectType::ControlImmunityFrames:
        {
            auto& record = state.units.require(effects.unitId);
            const int before = record.status.effects.controlImmunityFrames;
            record.status.effects.controlImmunityFrames = std::max(before, event.effect.value);
            if (record.status.effects.controlImmunityFrames > before)
            {
                appendStatusEventLog(
                    logEvents,
                    effects.unitId,
                    effects.unitId,
                    std::format("僵直吸收{}幀", event.effect.value));
                visualEvents.push_back(roleEffectEvent(
                    effects.unitId,
                    KysChess::EFT_BLOCK,
                    CoreRoleStatusEffectFrames));
            }
            break;
        }
        case KysChess::EffectType::LowestAllyHeal:
        {
            auto teamEvents = teamEffects.applyLowestAllyHeal(
                state.units,
                effects.unitId,
                event.effect.value,
                event.effect.value2);
            appendTeamEffectLogEvents(logEvents, teamEvents, "最低友方治療");
            appendTeamEffectVisualEvents(visualEvents, teamEvents);
            break;
        }
        default:
            assert(false);
        }
    }
}

void applyFrameCastScopedComboEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    for (const auto& effects : frame.castScopedComboEffects)
    {
        applyFrameCastScopedComboEffects(
            state,
            effects,
            frame.currentFrameAttacks(),
            frame.currentFrameDamage(),
            frame.logEvents,
            frame.visualEvents);
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

    auto frame = BattleFrameContext::begin(
        state,
        std::move(recycledPresentation),
        frameMemoryStorage_.data(),
        frameMemoryStorage_.size());

    // Tick status timers and queue status damage, e.g. poison or bleed damage transactions.
    advanceStatus(state, frame.currentFrameDamage());
    // Tick unit cooldown/action/MP timers and collect frame combo events, e.g. skill-finished triggers.
    auto runtimeAdvance = advanceRuntimeUnits(state, frame.frameMemoryResource());
    // Apply combo timer events to runtime state, deferring auto-ultimate commands until late frame.
    auto deferredCommands = applyRuntimeComboEvents(state, frame, runtimeAdvance.runtimeCommits);
    // Apply skill-finished team heals whose source finished cooldown this frame.
    applySkillFinishedTeamHeals(state, frame, runtimeAdvance.skillFinishedTeamHeals);
    // Reduce early gameplay commands into concrete queues/state; currently mostly a pre-movement drain point.
    reduceCommandsBeforeMovement(state, frame);
    // Advance and commit motion, e.g. physics and tactical movement.
    auto movement = advanceMotionFrame(state, frame.frameMemoryResource());
    // Start or commit unit actions, e.g. cast startup, attack spawn requests, blink teleports, action sounds.
    advanceActionFrameUnits(state, frame, movement);
    // Apply cast-release effects after all units selected/committed their frame actions.
    applyFrameCastScopedComboEffects(state, frame);
    // Reduce cast-release effects, e.g. 出手回內、全隊盾、當前生命傷害, before attacks/damage apply.
    reduceCommandsBeforeAttacks(state, frame);
    // Spawn/tick attacks and resolve hits; hit commands are reduced immediately into damage/effect queues.
    advanceAttacksAndResolveHits(state, frame);
    // Apply queued damage and lifecycle effects, e.g. HP loss, death, rescue, death AOE, battle end.
    applyDamageAndLifecycle(state, frame);
    state.nextFrame.recycleDamage(frame.drainCurrentFrameDamage());
    // Chain terminal logs are emitted after damage so the projectile visibly lands before the chain result.
    appendProjectileCancellationLogEvents(state.attacks, frame.attackEvents, frame.logEvents, true);
    applyLateFrameMpRestores(state, frame);
    for (auto& command : deferredCommands)
    {
        frame.queueCommand(std::move(command));
    }
    // Reduce late commands from damage/combo lifecycle, e.g. auto-ultimate or death-triggered projectiles.
    reduceCommandsAfterDamageLifecycle(state, frame);
    assert(frame.drainCommands().empty());
    // Convert accumulated gameplay/log/visual events into the presentation frame consumed by the scene.
    emitPresentationFrame(state, frame);
    // Runtime maintenance: remove projectiles/melee attacks whose animation lifetime has finished.
    pruneFinishedRuntimeAttacks(state);
    return consumeBattleFrameContext(std::move(frame));
}

}  // namespace KysChess::Battle
