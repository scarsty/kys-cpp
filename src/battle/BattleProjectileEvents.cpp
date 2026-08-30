#include "BattleProjectileEvents.h"

#include "BattleLogSegments.h"

#include <string>

#include "BattleOperation.h"
#include "../Find.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <utility>

namespace KysChess::Battle
{

namespace
{

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

}  // namespace

void appendVisualEvents(
    const BattleAttackEvent& event,
    const BattleAttackState& world,
    int presentationFrame,
    std::vector<BattleVisualEvent>& output)
{
    const auto append = [&output, presentationFrame](BattleVisualEvent visual)
    {
        assert(visual.frame == BattlePresentationCurrentFrame || visual.frame >= 0);
        if (visual.frame == BattlePresentationCurrentFrame)
        {
            visual.frame = presentationFrame;
        }
        output.push_back(std::move(visual));
    };
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

namespace
{

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

}  // namespace

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

}  // namespace KysChess::Battle
