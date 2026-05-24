#include "BattleCore.h"

#include "../ChessEftIds.h"
#include "../Find.h"
#include "BattleCombatIntent.h"
#include "BattleLogSegments.h"
#include "BattleMath.h"
#include "BattleResourceRules.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
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

    const double x = position.x - coordCount * tileWidth;
    Point grid;
    grid.x = static_cast<int>(std::round((x / tileWidth + position.y / tileWidth) / 2.0));
    grid.y = static_cast<int>(std::round((-x / tileWidth + position.y / tileWidth) / 2.0));
    return grid;
}

BattleRuntimeRandom::BattleRuntimeRandom(unsigned int seed)
    : seed_(seed), rand_(seed)
{
}

unsigned int BattleRuntimeRandom::seed() const
{
    return seed_;
}

double BattleRuntimeRandom::nextPercent()
{
    return static_cast<double>(rand_() % 10000u) / 100.0;
}

int BattleRuntimeRandom::nextInt(int upperBound)
{
    assert(upperBound > 0);
    return static_cast<int>(rand_() % static_cast<std::uint32_t>(upperBound));
}

bool BattleRuntimeRandom::chance(int chancePct)
{
    assert(chancePct >= 0);
    assert(chancePct <= 100);
    if (chancePct <= 0)
    {
        return false;
    }
    if (chancePct >= 100)
    {
        return true;
    }
    return nextPercent() < static_cast<double>(chancePct);
}

int BattleRuntimeRandom::symmetricInt(int exclusiveBound)
{
    assert(exclusiveBound > 0);
    return nextInt(exclusiveBound) - nextInt(exclusiveBound);
}

BattleRuntimeUnit* BattleUnitStore::findUnit(int unitId)
{
    return tryDenseById(units, unitId);
}

const BattleRuntimeUnit* BattleUnitStore::findUnit(int unitId) const
{
    return tryDenseById(units, unitId);
}

BattleRuntimeUnit& BattleUnitStore::requireUnit(int unitId)
{
    return requireDenseById(units, unitId);
}

const BattleRuntimeUnit& BattleUnitStore::requireUnit(int unitId) const
{
    return requireDenseById(units, unitId);
}

void BattleUnitStore::writeDamageUnit(const BattleDamageUnitState& source)
{
    auto& unit = requireUnit(source.id);
    unit.alive = source.alive;
    unit.vitals = source.vitals;
    unit.stats.attack = source.attack;
    unit.invincible = source.invincible;
    unit.shield = source.shield;
    unit.mpBlocked = source.mpBlocked;
    unit.mpRecoveryBonusPct = source.mpRecoveryBonusPct;
}

void BattleUnitStore::setPosition(int unitId, Pointf position)
{
    auto& unit = requireUnit(unitId);
    unit.motion.position = position;
    unit.grid = gridTransform.toGrid(position);
}

void BattleUnitStore::setMotion(int unitId, Pointf position, Pointf velocity, Pointf acceleration)
{
    auto& unit = requireUnit(unitId);
    unit.motion.position = position;
    unit.motion.velocity = velocity;
    unit.motion.acceleration = acceleration;
    if (velocity.norm() > 0.01f)
    {
        unit.motion.facing = velocity;
        unit.motion.facing.normTo(1.0f);
    }
    unit.grid = gridTransform.toGrid(position);
}

int findNearestEnemyUnitId(const BattleUnitStore& units, int sourceUnitId)
{
    const auto& source = units.requireUnit(sourceUnitId);
    int targetUnitId = -1;
    double bestDistance = 0.0;
    for (const auto& candidate : units.units)
    {
        if (!candidate.alive || candidate.team == source.team)
        {
            continue;
        }

        const double distance = (candidate.motion.position - source.motion.position).norm();
        if (targetUnitId < 0 || distance < bestDistance)
        {
            targetUnitId = candidate.id;
            bestDistance = distance;
        }
    }
    return targetUnitId;
}

int findFarthestEnemyUnitId(const BattleUnitStore& units, int sourceUnitId)
{
    const auto& source = units.requireUnit(sourceUnitId);
    int targetUnitId = -1;
    double bestDistance = 0.0;
    for (const auto& candidate : units.units)
    {
        if (!candidate.alive || candidate.team == source.team)
        {
            continue;
        }

        const double distance = (candidate.motion.position - source.motion.position).norm();
        if (targetUnitId < 0 || distance > bestDistance)
        {
            targetUnitId = candidate.id;
            bestDistance = distance;
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
    return unit;
}

namespace
{
bool isLastAliveInTeam(const BattleUnitStore& store, const BattleRuntimeUnit& unit);
void appendTeamEffectVisualEvents(
    std::vector<BattleVisualEvent>& visualEvents,
    const std::vector<BattleTeamEffectEvent>& events);
BattleUnitFrameTickInput makeRuntimeUnitTickInput(
    const BattleRuntimeState& state,
    const BattleRuntimeUnit& unit);

struct BattleRuntimeUnitFrameCommit
{
    int unitId = -1;
    BattleUnitFrameTickResult tick;
    std::vector<BattleComboFrameRuntimeEvent> comboEvents;
    RoleComboState comboState;
};

void applyRuntimeUnitMpDelta(BattleRuntimeUnit& unit, int mpDelta)
{
    if (mpDelta > 0)
    {
        if (unit.mpBlocked)
        {
            return;
        }
        unit.vitals.mp += mpDelta * (100 + unit.mpRecoveryBonusPct) / 100;
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

std::vector<BattleLogTextSegment> formatEffectCommandLogSegments(const BattleEffectCommand& command)
{
    switch (command.type)
    {
    case BattleEffectCommandType::AddShield:
        return command.value > 0
            ? logSegments<BattleLogTextTone::SkillName>(
                command.label,
                "（",
                std::pair{ BattleLogTextTone::ShieldValue, command.value },
                "護盾）")
            : battleLogText(command.label, BattleLogTextTone::SkillName);
    case BattleEffectCommandType::AddInvincibility:
        return logStatusFrames(command.label.c_str(), command.value);
    case BattleEffectCommandType::ModifyResource:
        return command.value > 0
            ? logSegments<BattleLogTextTone::SkillName>(
                command.label,
                std::pair{ BattleLogTextTone::Positive, "+" },
                std::pair{ BattleLogTextTone::ResourceValue, command.value },
                std::pair{ BattleLogTextTone::ResourceValue, "MP" })
            : battleLogText(command.label, BattleLogTextTone::SkillName);
    case BattleEffectCommandType::ModifyCooldown:
        return command.value != 0
            ? logSegments<BattleLogTextTone::SkillName>(
                command.label,
                "（",
                std::pair{ BattleLogTextTone::DurationValue, command.value },
                "冷卻）")
            : battleLogText(command.label, BattleLogTextTone::SkillName);
    case BattleEffectCommandType::Heal:
    case BattleEffectCommandType::DedicatedEffect:
        return battleLogText(command.label, BattleLogTextTone::SkillName);
    }
    assert(false);
    return battleLogText(command.label, BattleLogTextTone::SkillName);
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

void appendEnemyTopDebuffUpdates(BattleRuntimeState& state,
                                 std::vector<BattleLogEvent>& logEvents)
{
    int liveAllies = 0;
    int topTargets = 0;
    int perMemberValue = 0;
    for (const auto& ally : state.unitStore.units)
    {
        if (!ally.alive || ally.team != 0)
        {
            continue;
        }

        const auto comboIt = state.combo.units.find(ally.id);
        if (comboIt == state.combo.units.end() || comboIt->second.enemyTopDebuffCount <= 0)
        {
            continue;
        }

        liveAllies++;
        topTargets = std::max(topTargets, comboIt->second.enemyTopDebuffCount);
        perMemberValue = std::max(perMemberValue, comboIt->second.enemyTopDebuffValue);
    }

    std::vector<BattleRuntimeUnit*> enemyOrder;
    for (auto& unit : state.unitStore.units)
    {
        if (unit.team == 1 && unit.alive)
        {
            enemyOrder.push_back(&unit);
        }
    }

    std::stable_sort(
        enemyOrder.begin(),
        enemyOrder.end(),
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
    for (auto* enemy : enemyOrder)
    {
        auto comboIt = state.combo.units.find(enemy->id);
        if (comboIt == state.combo.units.end())
        {
            continue;
        }

        int desired = 0;
        if (assignedTargets < topTargets && liveAllies > 0 && perMemberValue > 0)
        {
            desired = perMemberValue * liveAllies;
            ++assignedTargets;
        }

        const int delta = desired - comboIt->second.enemyTopDebuffApplied;
        if (delta == 0)
        {
            continue;
        }

        enemy->stats.attack = std::max(0, enemy->stats.attack - delta);
        enemy->stats.defence = std::max(0, enemy->stats.defence - delta);
        comboIt->second.enemyTopDebuffApplied = desired;
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = -1;
        log.targetUnitId = enemy->id;
        log.segments = battleLogText(std::format(
            "陰險：前{}名攻防{}{}（{}名存活）",
            topTargets,
            delta > 0 ? "-" : "+",
            std::abs(delta),
            liveAllies), BattleLogTextTone::SkillName);
        logEvents.push_back(std::move(log));
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

int sharedBleedMaxStacks(const BattleRuntimeState& state, const BattleAttackEvent& event)
{
    if (event.scriptedBleedStacks <= 0)
    {
        return 1;
    }
    auto it = state.combo.units.find(event.sourceUnitId);
    if (it == state.combo.units.end() || it->second.bleedChancePct <= 0)
    {
        return 1;
    }
    return std::max(1, it->second.bleedMaxStacks);
}

int resolveHitMagicBaseDamage(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    const BattleRuntimeUnit& attacker,
    const BattleRuntimeUnit& defender)
{
    double defence = defender.stats.defence;
    auto comboIt = state.combo.units.find(attacker.id);
    if (comboIt != state.combo.units.end())
    {
        defence = resolveFrameArmorPenetratedDefense(
            comboIt->second,
            attacker.id,
            defender.id,
            defence,
            state.random);
    }

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

    const auto& attacker = state.unitStore.requireUnit(attack.state.attackerUnitId);
    const auto& defender = state.unitStore.requireUnit(otherAttack.state.attackerUnitId);

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
        presentation.operationKind = toLegacyOperationType(attack->state.operationType);
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

std::vector<BattleVisualEvent> toVisualEvents(
    const BattleAttackEvent& event,
    const BattleAttackState& world)
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
        presentation.operationKind = toLegacyOperationType(event.operationType);
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
        return {};
    case BattleAttackEventType::Bounce:
        presentation.type = BattleVisualEventType::ProjectileBounced;
        presentation.targetUnitId = event.unitId;
        presentation.amount = event.otherAttackId;
        break;
    }
    std::vector<BattleVisualEvent> presentations;
    presentations.push_back(std::move(presentation));
    if (event.type == BattleAttackEventType::Bounce)
    {
        presentations.push_back(toProjectileSpawnPresentationEvent(world, event.otherAttackId));
    }
    return presentations;
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
    const std::vector<BattleAttackEvent>& events,
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

void advanceRuntimeUnits(
    BattleRuntimeState& state,
    std::vector<BattleGameplayCommand>& commands,
    std::vector<BattleRuntimeUnitFrameCommit>& runtimeCommits)
{
    BattleComboTriggerSystem comboSystem;
    runtimeCommits.clear();
    for (auto& unit : state.unitStore.units)
    {
        assert(unit.id >= 0);
        if (!unit.alive)
        {
            continue;
        }
        auto input = makeRuntimeUnitTickInput(state, unit);
        const bool lastAlive = isLastAliveInTeam(state.unitStore, unit);

        BattleRuntimeUnitFrameCommit committed;
        committed.unitId = unit.id;
        committed.tick = BattleUnitFrameTickSystem().advance(input);

        unit.animation.cooldown = committed.tick.state.cooldown;
        unit.animation.actFrame = committed.tick.state.actFrame;
        unit.haveAction = committed.tick.state.haveAction;
        unit.operationType = committed.tick.state.operationType;
        unit.animation.actType = committed.tick.state.actType;
        unit.physicalPower = committed.tick.state.physicalPower;
        applyRuntimeUnitMpDelta(unit, committed.tick.mpDelta);
        if (committed.tick.resetDashVelocity)
        {
            unit.motion.velocity = { 0, 0, 0 };
        }

        auto comboIt = state.combo.units.find(unit.id);
        if (comboIt != state.combo.units.end())
        {
            auto frameEvents = comboSystem.advanceFrameRuntime(
                comboIt->second,
                {
                    state.movement.frame,
                    unit.vitals.hp,
                    unit.vitals.maxHp,
                    unit.alive,
                    lastAlive,
                });
            committed.comboEvents.insert(
                committed.comboEvents.end(),
                frameEvents.begin(),
                frameEvents.end());
        }
        if (committed.tick.skillFinished && comboIt != state.combo.units.end())
        {
            auto teamHeal = comboSystem.collectPendingSkillTeamHeal(
                comboIt->second,
                { BattleComboTriggerHook::AfterSkillCast, unit.id, -1 },
                state.random);
            if (teamHeal.flatHeal > 0 || teamHeal.pctHeal > 0)
            {
                BattleTeamHealCommand command{
                    unit.id,
                    teamHeal.flatHeal,
                    teamHeal.pctHeal,
                    "技能群療",
                };
                state.teamEffects.pendingCommands.push_back(std::move(command));
            }
        }
        if (comboIt != state.combo.units.end())
        {
            committed.comboState = comboIt->second;
        }
        runtimeCommits.push_back(std::move(committed));
    }
}

void applyProjectileCancelDamageResults(
    BattleRuntimeState& state,
    std::vector<BattleAttackEvent>& events,
    std::vector<BattleGameplayCommand>& commands)
{
    BattleAttackSystem attackSystem;
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

        BattleProjectileCancelDamageCommand command;
        command.attackId = event.attackId;
        command.otherAttackId = event.otherAttackId;
        command.sourceUnitId = event.sourceUnitId;
        command.otherSourceUnitId = event.otherSourceUnitId;
        command.damage = event.projectileCancelDamage;
        command.otherDamage = event.otherProjectileCancelDamage;
        commands.push_back(command);
        attackSystem.applyProjectileCancelDamage(state.attacks, event);
    }
}

void updateCommittedHitCombos(BattleRuntimeState& state, const BattleHitResolutionResult& result)
{
    if (auto it = state.combo.units.find(result.attackerUnitId); it != state.combo.units.end())
    {
        it->second = result.attackerCombo;
    }
    if (auto it = state.combo.units.find(result.defenderUnitId); it != state.combo.units.end())
    {
        it->second = result.defenderCombo;
    }
}

BattleHitResolutionInput makeHitResolutionInput(
    BattleRuntimeState& state,
    const BattleAttackEvent& event)
{
    const auto& attacker = state.unitStore.requireUnit(event.sourceUnitId);
    const auto& defender = state.unitStore.requireUnit(event.unitId);

    BattleHitResolutionInput input;
    input.attackEvent = event;
    input.attacker = makeHitUnitSnapshot(attacker);
    input.defender = makeHitUnitSnapshot(defender);
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
            attacker,
            defender,
            resolveHitMagicBaseDamage(state, event, attacker, defender));
    }
    if (auto comboIt = state.combo.units.find(event.sourceUnitId); comboIt != state.combo.units.end())
    {
        input.attackerCombo = comboIt->second;
    }
    if (auto comboIt = state.combo.units.find(event.unitId); comboIt != state.combo.units.end())
    {
        input.defenderCombo = comboIt->second;
    }
    return input;
}

bool tryResolveDodgeHit(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    auto defenderComboIt = state.combo.units.find(event.unitId);
    if (defenderComboIt == state.combo.units.end())
    {
        return false;
    }

    const double roll = state.random.nextPercent();
    auto dodge = BattleComboTriggerSystem().resolveDodge(defenderComboIt->second, event.sourceUnitId, roll);
    if (!dodge.dodged)
    {
        return false;
    }

    defenderComboIt->second.dodgedLast = true;

    logEvents.push_back(dodgeStatusEvent(event.unitId, event.sourceUnitId));
    visualEvents.push_back(roleEffectEvent(event.unitId, KysChess::EFT_EVADE, CoreRoleStatusEffectFrames));
    return true;
}

void resolveHitEvents(
    BattleRuntimeState& state,
    const std::vector<BattleAttackEvent>& events,
    std::vector<BattleGameplayCommand>& commands,
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
        auto result = BattleHitResolver().resolve(input, state.random);
        auto followUps = expandBattleProjectileFollowUpCommands(
            result.commands,
            state.projectileFollowUps,
            state.unitStore);
        result.commands = std::move(followUps.commands);
        result.visualEvents.insert(
            result.visualEvents.end(),
            followUps.visualEvents.begin(),
            followUps.visualEvents.end());
        updateCommittedHitCombos(state, result);
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

BattleCastSkillState makeRuntimeCastSkillState(
    const BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed,
    bool ultimate,
    bool consumeFrameSkillBonuses);

void refreshMovementSkillProfile(
    BattleUnitState& movementUnit,
    const BattleRuntimeUnit& runtimeUnit,
    const BattleRuntimeState& state)
{
    auto seedIt = state.action.planSeeds.find(runtimeUnit.id);
    if (seedIt == state.action.planSeeds.end())
    {
        if (runtimeUnit.reach > 0.0)
        {
            movementUnit.reach = runtimeUnit.reach;
            movementUnit.style = runtimeUnit.style;
        }
        return;
    }

    const auto& seed = seedIt->second;
    const bool useUltimate = runtimeUnit.vitals.maxMp > 0
        && runtimeUnit.vitals.mp >= runtimeUnit.vitals.maxMp
        && seed.ultimateSkill.id >= 0;
    const auto skill = makeRuntimeCastSkillState(
        state,
        runtimeUnit,
        useUltimate ? seed.ultimateSkill : seed.normalSkill,
        useUltimate,
        false);
    movementUnit.reach = skill.reach > 0.0 ? skill.reach : state.action.actionRules.meleeAttackReach;
    movementUnit.style = skill.rangedStyle ? CombatStyle::Ranged : CombatStyle::Melee;
    const auto comboIt = state.combo.units.find(runtimeUnit.id);
    const bool dashAttackEnabled = comboIt != state.combo.units.end() && comboIt->second.dashAttack;
    movementUnit.taXue = dashAttackEnabled;
}

using PostPhysicsMotionMap = std::map<int, BattleMovementPhysicsState>;
using UnitMotionSnapshotMap = std::map<int, BattleUnitMotion>;

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
    direction.normTo(horizontalSpeed);
    direction.z = static_cast<float>(verticalSpeed);
    return direction;
}

Pointf deathKickDirection(const BattleRuntimeState& state, const BattleRuntimeUnit& defender, int attackerUnitId)
{
    if (const auto* attacker = state.unitStore.findUnit(attackerUnitId))
    {
        if (attacker->id != defender.id)
        {
            return defender.motion.position - attacker->motion.position;
        }
    }
    return { 1, 0, 0 };
}

UnitMotionSnapshotMap makeUnitMotionSnapshot(const BattleUnitStore& units)
{
    UnitMotionSnapshotMap snapshots;
    for (const auto& unit : units.units)
    {
        snapshots.emplace(unit.id, unit.motion);
    }
    return snapshots;
}

// One runFrame() transaction ledger. Persistent gameplay lives in BattleRuntimeState.
// The only public frame output is result; every other member is frame-local scratch.
// Keep this type private to BattleCore.cpp and do not pass it to subsystem classes.
struct BattleFrameContext
{
    BattlePresentationFrame result;
    std::vector<BattleGameplayCommand> frameCommands;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<int> attackSoundIds;
    std::vector<BattleFrameRumbleEvent> rumbles;
    int blinkSoundCount{};
    std::vector<BattleGameplayCommand> deferredCommands;
    std::vector<BattleRuntimeUnitFrameCommit> runtimeCommits;
    std::vector<BattleAttackEvent> attackEvents;
    BattleTickResult movement;
    std::vector<BattleTeamEffectEvent> teamEffectEvents;
    std::vector<BattleEffectCommand> effectCommands;
    UnitMotionSnapshotMap frameStartMotion;
};

BattleFrameContext makeBattleFrameContext(const BattleRuntimeState& state)
{
    BattleFrameContext frame;
    frame.frameStartMotion = makeUnitMotionSnapshot(state.unitStore);
    return frame;
}

BattlePresentationFrame consumeBattleFrameContext(BattleFrameContext&& frame)
{
    assert(frame.frameCommands.empty());
    return std::move(frame.result);
}

const BattleUnitMotion& motionSnapshotForUnit(
    const UnitMotionSnapshotMap& snapshots,
    const BattleRuntimeUnit& fallback)
{
    const auto snapshotIt = snapshots.find(fallback.id);
    if (snapshotIt != snapshots.end())
    {
        return snapshotIt->second;
    }
    return fallback.motion;
}

void prepareMovementAgents(BattleRuntimeState& state)
{
    for (const auto& unit : state.unitStore.units)
    {
        if (!unit.alive)
        {
            continue;
        }
        auto [it, inserted] = state.movement.agents.emplace(unit.id, BattleMovementAgentState{});
        if (inserted)
        {
            it->second.physics.position = unit.motion.position;
            it->second.physics.velocity = unit.motion.velocity;
            it->second.physics.acceleration = unit.motion.acceleration;
        }
    }

    for (auto it = state.movement.agents.begin(); it != state.movement.agents.end();)
    {
        const auto& unit = state.unitStore.requireUnit(it->first);
        if (!unit.alive && !needsCorpsePhysics(unit))
        {
            state.movement.movementReservations.erase(it->first);
            it = state.movement.agents.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = state.movement.movementReservations.begin(); it != state.movement.movementReservations.end();)
    {
        const auto& unit = state.unitStore.requireUnit(it->first);
        if (!unit.alive)
        {
            it = state.movement.movementReservations.erase(it);
            continue;
        }
        ++it;
    }
}

BattleMovementPlanInput makeFrameMovementPlanInput(
    const BattleRuntimeState& state,
    const PostPhysicsMotionMap& postPhysics)
{
    BattleMovementPlanInput input;
    input.frame = state.movement.frame;
    input.config = state.movement.config;
    input.terrainCells = state.movement.terrainCells;
    input.movementReservations = state.movement.movementReservations;
    input.units.reserve(state.unitStore.units.size());

    for (const auto& runtimeUnit : state.unitStore.units)
    {
        if (!runtimeUnit.alive)
        {
            continue;
        }

        BattleUnitState movementUnit = makeBattleMovementPlanUnit(runtimeUnit, BattleRuntimeMoveSpeedDivisor);
        auto postPhysicsIt = postPhysics.find(runtimeUnit.id);
        if (postPhysicsIt != postPhysics.end())
        {
            movementUnit.position = postPhysicsIt->second.position;
            movementUnit.velocity = postPhysicsIt->second.velocity;
        }
        movementUnit.canAttack = runtimeUnit.animation.cooldown == 0;
        if (movementUnit.speed <= 0.0 && runtimeUnit.stats.speed > 0)
        {
            movementUnit.speed = runtimeUnit.stats.speed;
        }
        refreshMovementSkillProfile(movementUnit, runtimeUnit, state);
        const auto& agent = requireMappedById(state.movement.agents, runtimeUnit.id);
        const auto& physics = postPhysicsIt != postPhysics.end()
            ? postPhysicsIt->second
            : agent.physics;
        movementUnit.targetId = agent.targetId;
        movementUnit.assignedSlot = agent.assignedSlot;
        movementUnit.slotSwitchCooldownRemaining = agent.slotSwitchCooldownRemaining;
        movementUnit.dashFramesRemaining = physics.movementDashFrames;
        movementUnit.dashCooldownRemaining = physics.movementDashCooldown;
        movementUnit.postDashRetreatFramesRemaining = physics.postDashRetreatFrames;
        movementUnit.postDashChaosFramesRemaining = physics.postDashChaosFrames;
        movementUnit.movementDashSpreadFramesRemaining = physics.movementDashSpreadFrames;
        input.units.push_back(std::move(movementUnit));
    }

    return input;
}

bool isLastAliveInTeam(const BattleUnitStore& store, const BattleRuntimeUnit& unit)
{
    for (const auto& other : store.units)
    {
        if (other.id != unit.id && other.team == unit.team && other.alive)
        {
            return false;
        }
    }
    return true;
}

bool runtimeRoleForcesRangedMagic(const BattleRuntimeState& state, int unitId)
{
    auto it = state.combo.units.find(unitId);
    return it != state.combo.units.end() && it->second.forceRangedAttack;
}

int runtimeForcedRangedMinSelectDistance(const BattleRuntimeState& state, int unitId)
{
    constexpr int DefaultForcedRangedMinSelectDistance = 6;
    auto it = state.combo.units.find(unitId);
    if (it == state.combo.units.end() || it->second.forceRangedMinSelectDistance <= 0)
    {
        return DefaultForcedRangedMinSelectDistance;
    }
    return std::max(1, it->second.forceRangedMinSelectDistance);
}

int runtimeProjectileSpeedMultiplierPct(const BattleRuntimeState& state, int unitId)
{
    auto it = state.combo.units.find(unitId);
    if (it == state.combo.units.end())
    {
        return 100;
    }
    return std::max(100, it->second.projectileSpeedMultiplierPct);
}

bool runtimeForcedRangedMagic(const BattleActionSkillSeed& skill, bool forceRanged)
{
    return forceRanged && skill.attackAreaType == 0;
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
    if (forcedRanged && skill.attackAreaType == 0)
    {
        selectDistance = std::max(selectDistance, std::max(1, forcedRangedMinSelectDistance));
    }
    return selectDistance;
}

double runtimeBattleBlinkReach(
    const BattleActionSkillSeed& skill,
    const BattleActionRulesConfig& actionRules,
    const BattleCastGeometry& geometry)
{
    if (skill.id < 0)
    {
        return actionRules.tileWidth * 3.0;
    }
    if (skill.attackAreaType == 3)
    {
        return 180.0;
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
    if (skill.attackAreaType == 3)
    {
        return 180.0;
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
    return actionRules.meleeAttackReach;
}

BattleCastSkillState makeRuntimeCastSkillState(
    const BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionSkillSeed& seed,
    bool ultimate,
    bool consumeFrameSkillBonuses)
{
    (void)consumeFrameSkillBonuses;

    BattleCastSkillState skill;
    if (seed.id < 0)
    {
        return skill;
    }

    const bool forceRanged = runtimeRoleForcesRangedMagic(state, unit.id);
    const bool forcedRangedMagic = runtimeForcedRangedMagic(seed, forceRanged);
    const int forcedRangedMinSelectDistance = runtimeForcedRangedMinSelectDistance(state, unit.id);
    const int effectiveSelectDistance = runtimeEffectiveProjectileSelectDistance(
        seed,
        forcedRangedMagic,
        forcedRangedMinSelectDistance);
    const int speedMultiplierPct = runtimeProjectileSpeedMultiplierPct(state, unit.id);
    skill.id = seed.id;
    skill.name = seed.name;
    skill.soundId = seed.soundId;
    skill.hurtType = seed.hurtType;
    skill.attackAreaType = seed.attackAreaType;
    skill.magicType = seed.magicType;
    skill.visualEffectId = seed.visualEffectId;
    skill.selectDistance = effectiveSelectDistance;
    skill.projectileSpeedMultiplierPct = speedMultiplierPct;
    skill.actProperty = seed.actProperty;
    skill.magicPower = seed.magicPower;
    skill.meleeSplashCount = ultimate && seed.attackAreaType == 0 ? 1 : 0;
    skill.extraProjectileCount = 0;
    skill.reach = std::min(
        runtimeEffectiveBattleReach(
            seed,
            forceRanged,
            forcedRangedMinSelectDistance,
            speedMultiplierPct,
            state.action.actionRules,
            state.action.castGeometry),
        state.action.actionRules.maxEffectiveBattleReach);
    skill.forceRanged = forceRanged;
    skill.rangedStyle = runtimeBattleRangedStyle(seed, forceRanged);
    skill.blinkReach = runtimeBattleBlinkReach(seed, state.action.actionRules, state.action.castGeometry);
    return skill;
}

BattleUnitFrameTickInput makeRuntimeUnitTickInput(
    const BattleRuntimeState& state,
    const BattleRuntimeUnit& unit)
{
    BattleUnitFrameTickInput input;
    input.state.cooldown = unit.animation.cooldown;
    input.state.actFrame = unit.animation.actFrame;
    input.state.actType = unit.animation.actType;
    input.state.operationType = unit.operationType;
    input.state.haveAction = unit.haveAction;
    input.state.physicalPower = unit.physicalPower;
    input.frame = state.movement.frame;
    input.mpRegenIntervalFrames = 3;
    input.physicalPowerRegenIntervalFrames = 3;
    if (const auto* status = tryFindById(state.status.units, unit.id))
    {
        input.frozen = status->effects.frozenTimer > 0;
    }
    return input;
}

std::pair<int, int> rescueCellKey(int x, int y)
{
    return { x, y };
}

std::vector<BattleFrameRescueUnitSnapshot> makeRescueUnitSnapshots(const BattleRuntimeState& state)
{
    std::vector<BattleFrameRescueUnitSnapshot> snapshots;
    snapshots.reserve(state.unitStore.units.size());
    for (const auto& unit : state.unitStore.units)
    {
        BattleFrameRescueUnitSnapshot snapshot;
        snapshot.unit.id = unit.id;
        snapshot.unit.team = unit.team;
        snapshot.unit.alive = unit.alive;
        snapshot.unit.hp = unit.vitals.hp;
        snapshot.unit.maxHp = unit.vitals.maxHp;
        snapshot.unit.invincible = unit.invincible;
        snapshot.unit.cell = unit.grid;
        snapshot.position = unit.motion.position;
        auto comboIt = state.combo.units.find(unit.id);
        if (comboIt != state.combo.units.end())
        {
            snapshot.unit.isSummonedClone = comboIt->second.isSummonedClone;
            snapshot.unit.forcePullProtect = comboIt->second.forcePullProtect;
            snapshot.unit.forcePullProtectRemaining = comboIt->second.forcePullProtectRemaining;
            snapshot.unit.forcePullExecute = comboIt->second.forcePullExecute;
            snapshot.unit.forcePullExecuteRemaining = comboIt->second.forcePullExecuteRemaining;
        }
        snapshots.push_back(std::move(snapshot));
    }
    return snapshots;
}

std::vector<BattleRescueCellSnapshot> makeRescueCellSnapshots(const BattleRuntimeState& state)
{
    std::map<std::pair<int, int>, int> occupantByCell;
    for (const auto& unit : state.unitStore.units)
    {
        if (!unit.alive)
        {
            continue;
        }
        occupantByCell[rescueCellKey(unit.grid.x, unit.grid.y)] = unit.id;
    }

    auto cells = state.rescue.cells;
    for (auto& cell : cells)
    {
        cell.occupied = false;
        cell.occupantUnitId = -1;
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

BattleCastInput refreshedCastInput(const BattleRuntimeState& state,
                                   const BattleTickResult& movement,
                                   BattleCastInput input)
{
    (void)movement;
    const auto& source = state.unitStore.requireUnit(input.unit.id);
    input.unit.position = source.motion.position;
    input.unit.facing = source.motion.facing;
    input.unit.alive = source.alive;
    input.unit.canStartAttack = source.canAttack;
    input.unit.mp = source.vitals.mp;
    input.unit.maxMp = source.vitals.maxMp;
    input.unit.speed = source.stats.speed;
    input.unit.operationCount = source.operationCount;
    if (const auto* status = tryFindById(state.status.units, input.unit.id))
    {
        input.unit.frozen = status->effects.frozenTimer > 0;
    }
    if (input.targetUnitId < 0)
    {
        input.targetUnitId = findNearestEnemyUnitId(state.unitStore, input.unit.id);
    }
    if (input.targetUnitId >= 0)
    {
        const auto& target = state.unitStore.requireUnit(input.targetUnitId);
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
    const auto& sourceUnit = state.unitStore.requireUnit(input.unit.id);
    for (const auto& candidate : state.unitStore.units)
    {
        if (!candidate.alive || candidate.team == sourceUnit.team)
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
        auto& combo = requireMappedById(state.combo.units, input.unit.id);
        input.ultimateSkill.extraProjectileCount = collectFrameExtraProjectileCount(
            combo,
            state.random,
            input.unit.id,
            std::max(0, combo.ultimateExtraProjectiles));
    }
}

BattleCastInput makeRuntimeCastInputFromSeed(
    BattleRuntimeState& state,
    const BattleRuntimeUnit& unit,
    const BattleActionPlanSeed& seed,
    bool canStartAttack,
    bool movementDashActive,
    bool consumeFrameSkillBonuses)
{
    BattleCastInput input;
    input.config = state.action.castConfig;
    input.geometry = state.action.castGeometry;
    input.unit.id = unit.id;
    input.unit.position = unit.motion.position;
    input.unit.facing = unit.motion.facing;
    input.unit.alive = unit.alive;
    input.unit.canStartAttack = canStartAttack;
    input.unit.mp = unit.vitals.mp;
    input.unit.maxMp = unit.vitals.maxMp;
    input.unit.speed = unit.stats.speed;
    input.unit.operationCount = unit.operationCount;
    input.unit.meleeAttackReach = state.action.actionRules.meleeAttackReach;
    input.unit.dashAttackReach = state.action.actionRules.dashAttackMeleeReach;
    input.unit.hasEquippedSkill = seed.hasEquippedSkill;
    input.unit.movementDashActive = movementDashActive;
    if (const auto* status = tryFindById(state.status.units, unit.id))
    {
        input.unit.frozen = status->effects.frozenTimer > 0;
    }

    auto comboIt = state.combo.units.find(unit.id);
    if (comboIt != state.combo.units.end())
    {
        input.unit.cooldownReductionPct = comboIt->second.cdrPct;
        input.unit.dashAttackEnabled = comboIt->second.dashAttack;
    }

    input.unit.dashVelocity = unit.motion.facing;
    if (input.unit.dashVelocity.norm() > 0.01)
    {
        input.unit.dashVelocity.normTo(
            static_cast<float>(
                state.action.actionRules.meleeAttackHitRadius / state.action.actionRules.dashMomentumFrames));
    }

    const bool ultimateReady = unit.vitals.maxMp > 0 && unit.vitals.mp >= unit.vitals.maxMp;
    const auto& selectedSeed = ultimateReady && seed.ultimateSkill.id >= 0
        ? seed.ultimateSkill
        : seed.normalSkill;
    input.unit.dashHitCount = 1;
    input.unit.emitDashFollowUpSkillAttack = input.unit.dashAttackEnabled && selectedSeed.id >= 0;
    input.unit.dashFollowUpOperationType = selectedSeed.id >= 0
        ? BattleCombatIntentPlanner().operationTypeForAttackArea(selectedSeed.attackAreaType)
        : BattleOperationType::None;
    input.normalSkill = makeRuntimeCastSkillState(state, unit, seed.normalSkill, false, consumeFrameSkillBonuses);
    input.ultimateSkill = makeRuntimeCastSkillState(state, unit, seed.ultimateSkill, true, consumeFrameSkillBonuses);
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
        const double attackRange = selectedSkill.attackAreaType == 3
            ? 180.0
            : std::min(selectedSkill.reach, state.action.actionRules.maxEffectiveBattleReach);
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

    const auto& unit = state.unitStore.requireUnit(unitId);
    Pointf retreatVelocity = cast.postDashRetreatVelocity;
    int retreatFrames = cast.postDashRetreatFrames;
    int chaosFrames = 0;
    if (unit.style == CombatStyle::Melee)
    {
        retreatVelocity = taXueMeleeRetreatVelocity(state, retreatVelocity);
        retreatFrames += 6;
        chaosFrames = state.movement.config.dashFrames + 1;
    }

    auto& physics = requireMappedById(state.movement.agents, unitId).physics;
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
    const auto movementIt = state.movement.agents.find(unitId);
    return movementIt != state.movement.agents.end()
        && movementIt->second.physics.movementDashFrames > 0;
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
                                              const BattleCastResult& cast);
BattleActionCommitInput makeCommittedCastActionInput(BattleRuntimeState& state,
                                                     const BattleRuntimeUnit& unit,
                                                     const BattleCastInput& castInput,
                                                     const BattleCastSkillState& selectedSkill,
                                                     const BattleCastResult& cast);
std::optional<BattleActionCommitInput> tryMakeRuntimeActionCommitInput(BattleRuntimeState& state,
                                                                        const BattleTickResult& movement,
                                                                        const BattlePendingCastAction& pending);
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

    auto& combo = requireMappedById(state.combo.units, unit.id);
    if (combo.blinkAttack)
    {
        const double blinkReach = selectedSkill.blinkReach > 0.0 ? selectedSkill.blinkReach : selectedSkill.reach;
        actionInput.blinkGeometry = makeRuntimeBlinkGeometry(state, unit, blinkReach);
    }
}

void applyCastPostSkillInvincibility(
    BattleRuntimeState& state,
    int sourceUnitId,
    int frames,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleLogEvent>& logEvents);

bool tryCommitAutoUltimate(
    BattleRuntimeState& state,
    int unitId,
    bool consumeMp,
    bool announceAutoUltimate,
    std::vector<int>& attackSoundIds,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleGameplayCommand>& pendingCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    auto& unit = state.unitStore.requireUnit(unitId);
    if (!unit.alive)
    {
        return true;
    }

    auto seedIt = state.action.planSeeds.find(unitId);
    if (seedIt == state.action.planSeeds.end()
        || seedIt->second.ultimateSkill.id < 0)
    {
        return true;
    }

    auto castInput = refreshedCastInput(
        state,
        {},
        makeRuntimeCastInputFromSeed(
            state,
            unit,
            seedIt->second,
            true,
            actionMovementDashActive(state, unitId),
            true));
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
    auto& combo = requireMappedById(state.combo.units, unitId);
    auto actionResult = BattleActionCommitSystem().commit(actionInput, combo, state.unitStore);
    applyCastPostSkillInvincibility(
        state,
        unitId,
        actionResult.combo.postSkillInvincFrames,
        effectCommands,
        logEvents);
    auto castScopedCommands = collectFrameCastScopedComboCommands(
        actionResult.combo,
        state.random,
        unitId,
        committedCastProjectileSpeed(actionResult, state.projectileFollowUps.projectileSpeed));
    pendingCommands.insert(
        pendingCommands.end(),
        std::make_move_iterator(castScopedCommands.begin()),
        std::make_move_iterator(castScopedCommands.end()));
    state.pendingAttackSpawns.insert(
        state.pendingAttackSpawns.end(),
        actionResult.attackSpawnRequests.begin(),
        actionResult.attackSpawnRequests.end());
    logEvents.insert(
        logEvents.end(),
        actionResult.logEvents.begin(),
        actionResult.logEvents.end());
    visualEvents.insert(
        visualEvents.end(),
        actionResult.visualEvents.begin(),
        actionResult.visualEvents.end());
    combo = actionResult.combo;
    unit.operationCount = actionResult.operationCount;
    if (consumeMp)
    {
        unit.vitals.mp -= unit.vitals.maxMp;
    }
    return true;
}

Pointf positionForRuntimeGridCell(const BattleRuntimeState& state, int x, int y)
{
    const int coordCount = state.unitStore.gridTransform.coordCount;
    const double tileWidth = state.unitStore.gridTransform.tileWidth;
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
    const int coordCount = state.unitStore.gridTransform.coordCount;
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

BattleBlinkGeometryInput makeRuntimeBlinkGeometry(const BattleRuntimeState& state,
                                                  const BattleRuntimeUnit& source,
                                                  double reach)
{
    BattleBlinkGeometryInput geometry;
    geometry.currentGridX = source.grid.x;
    geometry.currentGridY = source.grid.y;

    const double tileWidth = state.unitStore.gridTransform.tileWidth;
    assert(tileWidth > 0.0);
    int gridReach = std::max(1, static_cast<int>(reach / tileWidth) + 1);
    std::set<std::pair<int, int>> visited;
    for (const auto& target : state.unitStore.units)
    {
        if (target.id == source.id || !target.alive || target.team == source.team)
        {
            continue;
        }

        for (int dx = -gridReach; dx <= gridReach; ++dx)
        {
            for (int dy = -gridReach; dy <= gridReach; ++dy)
            {
                const int x = target.grid.x + dx;
                const int y = target.grid.y + dy;
                if (!visited.emplace(x, y).second)
                {
                    continue;
                }

                bool occupied = false;
                for (const auto& other : state.unitStore.units)
                {
                    if (other.id == source.id || !other.alive)
                    {
                        continue;
                    }
                    if (other.grid.x == x && other.grid.y == y)
                    {
                        occupied = true;
                        break;
                    }
                }

                geometry.cells.push_back({
                    x,
                    y,
                    positionForRuntimeGridCell(state, x, y),
                    runtimeGridCellWalkable(state, x, y),
                    occupied,
                });
            }
        }
    }
    return geometry;
}

BattlePendingCastAction makePendingCastAction(const BattleCastInput& castInput,
                                              const BattleCastResult& cast)
{
    BattlePendingCastAction pending;
    pending.unitId = cast.decision.unitId;
    pending.targetUnitId = cast.decision.targetUnitId;
    pending.ultimate = cast.decision.ultimate;
    pending.operationType = cast.decision.operationType;
    pending.skill = selectedCastSkill(castInput, cast);
    return pending;
}

int pendingCastCommitTargetUnitId(const BattleUnitStore& units, const BattlePendingCastAction& pending)
{
    const auto& source = units.requireUnit(pending.unitId);
    if (!source.alive)
    {
        return -1;
    }

    assert(pending.targetUnitId >= 0);
    const auto& target = units.requireUnit(pending.targetUnitId);
    if (target.alive && target.team != source.team)
    {
        return target.id;
    }

    return findNearestEnemyUnitId(units, pending.unitId);
}

std::optional<BattleCastInput> tryMakeRuntimeCastInputForPendingCast(
    BattleRuntimeState& state,
    const BattlePendingCastAction& pending)
{
    const auto& unit = state.unitStore.requireUnit(pending.unitId);
    const int targetUnitId = pendingCastCommitTargetUnitId(state.unitStore, pending);
    if (targetUnitId < 0)
    {
        return std::nullopt;
    }
    const auto& target = state.unitStore.requireUnit(targetUnitId);

    BattleCastInput input;
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
    auto& combo = requireMappedById(state.combo.units, unit.id);
    input.unit.cooldownReductionPct = combo.cdrPct;
    input.unit.dashAttackEnabled = combo.dashAttack;
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

    const auto& sourceUnit = state.unitStore.requireUnit(input.unit.id);
    for (const auto& candidate : state.unitStore.units)
    {
        if (!candidate.alive || candidate.team == sourceUnit.team)
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
    populateActionCommitLiveInput(state, unit, castInput, selectedSkill, actionInput);

    auto& combo = requireMappedById(state.combo.units, unit.id);
    auto prime = collectFrameProjectileBouncePrime(
        combo,
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
    const BattlePendingCastAction& pending)
{
    (void)movement;

    const auto& unit = state.unitStore.requireUnit(pending.unitId);
    auto castInput = tryMakeRuntimeCastInputForPendingCast(state, pending);
    if (!castInput)
    {
        return std::nullopt;
    }

    auto selectedSkill = pending.skill;
    if (pending.ultimate)
    {
        auto& combo = requireMappedById(state.combo.units, unit.id);
        selectedSkill.extraProjectileCount = collectFrameExtraProjectileCount(
            combo,
            state.random,
            unit.id,
            std::max(0, combo.ultimateExtraProjectiles));
    }
    castInput->normalSkill = selectedSkill;
    castInput->ultimateSkill = selectedSkill;

    if (pending.operationType == BattleOperationType::Dash)
    {
        castInput->unit.dashHitCount = rollRuntimeDashHitCount(state, unit, selectedSkill);
        castInput->unit.dashVelocity = runtimeDashAttackVelocity(
            state,
            unit,
            *castInput,
            selectedSkill);
    }

    auto cast = BattleCastPlanner().commitSelectedCast(
        *castInput,
        selectedSkill,
        pending.ultimate,
        pending.operationType);
    return makeCommittedCastActionInput(state, unit, *castInput, selectedSkill, cast);
}

std::string toStatusText(const BattleDamageEvent& event)
{
    switch (event.type)
    {
    case BattleDamageEventType::BlockedByFirstHit:
        return "格擋了首輪傷害";
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
    case BattleDamageStatusType::Frozen:
        return withValue("受擊硬直");
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
    case BattleDamageEventType::MpDamageApplied:
    case BattleDamageEventType::ShieldAbsorbed:
        gameplay.type = BattleGameplayEventType::DamageApplied;
        break;
    case BattleDamageEventType::UnitDied:
        gameplay.type = BattleGameplayEventType::UnitDied;
        break;
    case BattleDamageEventType::StatusApplied:
        gameplay.type = BattleGameplayEventType::StatusApplied;
        gameplay.text = toStatusText(event);
        break;
    case BattleDamageEventType::HpRestored:
    case BattleDamageEventType::MpRestored:
    case BattleDamageEventType::MpDrained:
    case BattleDamageEventType::CooldownExtended:
    case BattleDamageEventType::KillRewardApplied:
        gameplay.type = BattleGameplayEventType::ResourceChanged;
        break;
    case BattleDamageEventType::BlockedByInvincible:
    case BattleDamageEventType::BlockedByFirstHit:
    case BattleDamageEventType::DeathPrevented:
    case BattleDamageEventType::ExecuteTriggered:
        gameplay.type = BattleGameplayEventType::StatusApplied;
        gameplay.text = toStatusText(event);
        break;
    }
    return gameplay;
}

BattleStatusUnitState makeFallbackStatusUnit(const BattleDamageUnitState& unit)
{
    BattleStatusUnitState status;
    status.id = unit.id;
    status.alive = unit.alive;
    status.hp = unit.vitals.hp;
    status.maxHp = unit.vitals.maxHp;
    status.invincible = unit.invincible;
    return status;
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
    damage.mpBlocked = unit.mpBlocked;
    damage.mpRecoveryBonusPct = unit.mpRecoveryBonusPct;
    if (runtime)
    {
        damage.hurtInvincFrames = runtime->hurtInvincFrames;
        damage.blockFirstHitsRemaining = runtime->blockFirstHitsRemaining;
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
    state.unitStore.writeDamageUnit(unit);
    writeBattleDamageRuntimeUnit(requireById(state.damage.unitExtras, unit.id), unit);
}

void commitDamageDefenderStatusToRuntime(
    BattleRuntimeState& state,
    const BattleDamageTransactionResult& transaction)
{
    writeBattleStatusRuntimeUnit(
        requireById(state.status.units, transaction.defender.id),
        transaction.defenderStatus);
}

void commitDamageCooldownToRuntime(BattleRuntimeState& state, const BattleDamageTransactionResult& transaction)
{
    auto& unit = state.unitStore.requireUnit(transaction.defender.id);
    unit.animation.cooldown = transaction.defenderCooldown.cooldown;
}

void applyDamageResultToFrameState(
    BattleRuntimeState& state,
    const BattleDamageTransactionResult& transaction,
    const UnitMotionSnapshotMap& frameStartMotion)
{
    const auto& preDamageDefender = state.unitStore.requireUnit(transaction.defender.id);
    const auto& defenderStartMotion = motionSnapshotForUnit(frameStartMotion, preDamageDefender);
    const auto* attacker = state.unitStore.findUnit(transaction.attacker.id);
    Pointf preDamageDeathKickDirection = { 1, 0, 0 };
    if (attacker && attacker->id != preDamageDefender.id)
    {
        preDamageDeathKickDirection = defenderStartMotion.position - motionSnapshotForUnit(frameStartMotion, *attacker).position;
    }
    commitDamageUnitCoreToRuntime(state, transaction.attacker);
    commitDamageUnitCoreToRuntime(state, transaction.defender);
    commitDamageCooldownToRuntime(state, transaction);
    auto& unit = state.unitStore.requireUnit(transaction.defender.id);
    if (transaction.killed)
    {
        unit.motion = defenderStartMotion;
        unit.motion.position.z += DeathKickImpactHeight;
        unit.motion.velocity = deathKickVelocity(
            preDamageDeathKickDirection,
            transaction.finalHpDamage);
        unit.motion.acceleration = { 0, 0, state.movementPhysics.config.gravity };
        unit.frozen = 0;
        unit.frozenMax = 0;
        if (auto* status = tryFindById(state.status.units, unit.id))
        {
            status->effects.frozenTimer = 0;
            status->effects.frozenMaxTimer = 0;
        }
        if (auto agentIt = state.movement.agents.find(unit.id); agentIt != state.movement.agents.end())
        {
            agentIt->second.physics.position = unit.motion.position;
            agentIt->second.physics.velocity = unit.motion.velocity;
            agentIt->second.physics.acceleration = unit.motion.acceleration;
        }
    }
    commitDamageDefenderStatusToRuntime(state, transaction);
}

void applyRescueDamageToUnitStore(BattleRuntimeState& state, int unitId, int hp, int invincible)
{
    auto& unit = state.unitStore.requireUnit(unitId);
    unit.vitals.hp = hp;
    unit.invincible = invincible;
}

void applyRescuePositionToUnitStore(BattleRuntimeState& state, int unitId, Pointf position)
{
    state.unitStore.setPosition(unitId, position);
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

    const auto& attackerUnit = state.unitStore.requireUnit(command.attackerUnitId);
    const auto& targetUnit = state.unitStore.requireUnit(command.targetUnitId);

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
        static_cast<int>(std::ceil(distance2d(targetUnit.motion.position, request.initial.position) / config.projectileSpeed))
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

    auto& pulled = state.unitStore.requireUnit(result.teleport->unitId);
    pulled.grid = result.teleport->destinationCell;
    applyRescuePositionToUnitStore(state, pulled.id, result.teleport->destinationPosition);

    if (result.counterDelta.unitId >= 0)
    {
        auto comboIt = state.combo.units.find(result.counterDelta.unitId);
        if (comboIt != state.combo.units.end())
        {
            comboIt->second.forcePullProtectRemaining = std::max(
                0,
                comboIt->second.forcePullProtectRemaining + result.counterDelta.protectRemainingDelta);
            comboIt->second.forcePullExecuteRemaining = std::max(
                0,
                comboIt->second.forcePullExecuteRemaining + result.counterDelta.executeRemainingDelta);
        }
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
    applyRescueDamageToUnitStore(state, pulled.id, pulled.vitals.hp, pulled.invincible);

    if (result.basicCounterAttack)
    {
        state.pendingAttackSpawns.push_back(makeRescueCounterAttackSpawn(state, *result.basicCounterAttack));
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

void applyRescueRepositionForDamage(
    BattleRuntimeState& state,
    const BattleDamageTransactionResult& transaction,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    if (state.unitStore.units.empty() || transaction.defender.vitals.maxHp <= 0 || !transaction.defender.alive)
    {
        return;
    }

    auto rescueUnits = makeRescueUnitSnapshots(state);
    const auto& pulled = requireBy(rescueUnits, transaction.defender.id, rescueSnapshotUnitId);
    const auto* attacker = tryFindBy(rescueUnits, transaction.attacker.id, rescueSnapshotUnitId);

    const int hpBefore = transaction.defender.vitals.hp - transaction.defenderDelta.hpDelta;
    if (attacker
        && attacker->unit.alive
        && attacker->unit.team != pulled.unit.team
        && hpBefore * 4 > transaction.defender.vitals.maxHp
        && transaction.defender.vitals.hp * 4 <= transaction.defender.vitals.maxHp)
    {
        tryApplyRescue(state,
                       rescueUnits,
                       BattleRescuePullMode::Protect,
                       transaction.defender.id,
                       pulled.unit.team,
                       logEvents,
                       visualEvents);
        rescueUnits = makeRescueUnitSnapshots(state);
    }

    const auto& currentPulled = state.unitStore.requireUnit(transaction.defender.id);
    if (hpBefore * 100 > transaction.defender.vitals.maxHp * 15
        && currentPulled.vitals.hp * 100 <= transaction.defender.vitals.maxHp * 15
        && state.rescue.executeUnattendedRadius > 0.0
        && rescueUnitUnattendedByTeam(state, rescueUnits, transaction.defender.id, 1 - currentPulled.team))
    {
        tryApplyRescue(state,
                       rescueUnits,
                       BattleRescuePullMode::Execute,
                       transaction.defender.id,
                       1 - currentPulled.team,
                       logEvents,
                       visualEvents);
    }
}

void appendFramePendingDamage(
    BattleRuntimeState& state,
    BattleDamageRequest request,
    std::optional<BattleDamagePresentationInput> presentation = std::nullopt,
    bool canTriggerExecute = false,
    bool canTriggerDefenderBlock = false)
{
    assert(request.defenderUnitId >= 0);

    state.unitStore.requireUnit(request.defenderUnitId);
    if (request.attackerUnitId >= 0)
    {
        state.unitStore.requireUnit(request.attackerUnitId);
    }

    BattlePendingDamageIntent intent;
    intent.request = std::move(request);
    if (presentation)
    {
        intent.presentation = std::move(*presentation);
    }
    intent.canTriggerExecute = canTriggerExecute;
    intent.canTriggerDefenderBlock = canTriggerDefenderBlock;
    state.damage.pendingDamage.push_back(std::move(intent));
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
    presentation.ultimate = command.ultimate;
    presentation.skillName = command.skillName;
    presentation.segments = command.segments;
    applyFrameDamagePresentationStyle(state, command.targetUnitId, presentation);
    return presentation;
}

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    const BattleHpDamageCommand& command)
{
    BattleDamageRequest request;
    request.attackerUnitId = command.sourceUnitId;
    request.defenderUnitId = command.targetUnitId;
    request.baseDamage = command.damage;
    request.preResolvedDamage = true;
    request.frozenFrames = command.frozenFrames;
    request.triggersDefenseEffects = command.triggersDefenseEffects;

    appendFramePendingDamage(
        state,
        std::move(request),
        makeFrameDamagePresentation(state, command),
        command.canTriggerExecute,
        command.canTriggerDefenderBlock);
    return true;
}

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    const BattleMpDamageCommand& command)
{
    auto request = command.damage;
    request.attackerUnitId = command.sourceUnitId;
    request.defenderUnitId = command.targetUnitId;

    appendFramePendingDamage(
        state,
        std::move(request),
        std::nullopt,
        false,
        command.canTriggerDefenderBlock);
    return true;
}

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    const BattleAcceptedHitSideEffectCommand& command)
{
    auto request = command.damage;
    request.attackerUnitId = command.sourceUnitId;
    request.defenderUnitId = command.targetUnitId;
    request.acceptedHit = true;

    appendFramePendingDamage(state, std::move(request));
    return true;
}

void appendStatusEventLog(
    std::vector<BattleLogEvent>& logEvents,
    int sourceUnitId,
    int targetUnitId,
    std::string text)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Status;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.segments = battleLogText(std::move(text), BattleLogTextTone::SkillName);
    logEvents.push_back(std::move(event));
}

void appendHealEventLog(
    std::vector<BattleLogEvent>& logEvents,
    int sourceUnitId,
    int targetUnitId,
    int amount,
    std::string text)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Heal;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.amount = amount;
    event.segments = battleLogText(std::move(text), BattleLogTextTone::SkillName);
    logEvents.push_back(std::move(event));
}

bool applyFrameTeamEffectCommand(
    BattleRuntimeState& state,
    const BattleGameplayCommand& command,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    if (state.unitStore.units.empty())
    {
        return false;
    }

    const int sourceUnitId = std::visit([](const auto& typedCommand) -> int
        {
            using Command = std::decay_t<decltype(typedCommand)>;
            if constexpr (std::is_same_v<Command, BattleTeamHealCommand>
                || std::is_same_v<Command, BattleTeamMpRestoreCommand>
                || std::is_same_v<Command, BattleTeamShieldCommand>)
            {
                return typedCommand.sourceUnitId;
            }
            return -1;
    }, command);
    assert(sourceUnitId >= 0);

    const auto& source = state.unitStore.requireUnit(sourceUnitId);
    if (!source.alive)
    {
        return true;
    }

    auto application = applyBattleTeamEffectCommand(state.unitStore, command);
    logEvents.insert(
        logEvents.end(),
        application.logEvents.begin(),
        application.logEvents.end());
    appendTeamEffectVisualEvents(visualEvents, application.events);
    teamEffectEvents.insert(
        teamEffectEvents.end(),
        application.events.begin(),
        application.events.end());
    return true;
}

bool applyFrameMpRestoreCommand(
    BattleRuntimeState& state,
    const BattleMpRestoreCommand& command,
    std::vector<BattleLogEvent>& logEvents)
{
    auto& unit = state.unitStore.requireUnit(command.unitId);

    const int restored = std::min(command.amount, std::max(0, unit.vitals.maxMp - unit.vitals.mp));
    if (restored <= 0)
    {
        return true;
    }

    unit.vitals.mp += restored;
    appendStatusEventLog(logEvents, command.unitId, command.unitId, command.reason);
    return true;
}

bool applyFrameUnitHealCommand(
    BattleRuntimeState& state,
    const BattleUnitHealCommand& command,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    auto& unit = state.unitStore.requireUnit(command.targetUnitId);

    const int before = unit.vitals.hp;
    unit.vitals.hp = std::min(unit.vitals.maxHp, unit.vitals.hp + command.amount);
    const int healed = unit.vitals.hp - before;
    if (healed <= 0)
    {
        return true;
    }

    appendHealEventLog(logEvents, command.sourceUnitId, command.targetUnitId, healed, command.reason);
    visualEvents.push_back(roleEffectEvent(
        command.targetUnitId,
        KysChess::EFT_HEAL,
        CoreRoleStatusEffectFrames));
    return true;
}

bool applyFrameUnitShieldCommand(
    BattleRuntimeState& state,
    const BattleUnitShieldCommand& command,
    std::vector<BattleLogEvent>& logEvents)
{
    auto& runtimeUnit = state.unitStore.requireUnit(command.targetUnitId);
    runtimeUnit.shield += command.amount;
    appendStatusEventLog(
        logEvents,
        command.sourceUnitId,
        command.targetUnitId,
        formatStatusValue(command.reason, command.amount, "護盾"));
    return true;
}

bool applyFrameTempAttackBuffCommand(
    BattleRuntimeState& state,
    const BattleTempAttackBuffCommand& command,
    std::vector<BattleLogEvent>& logEvents)
{
    auto& unit = state.unitStore.requireUnit(command.unitId);

    unit.stats.attack += command.attackBonus;
    unit.stats.defence += command.defenceBonus;
    if (!command.permanent)
    {
        if (command.attackBonus != 0 && command.durationFrames > 0)
        {
            auto& status = requireById(state.status.units, command.unitId);
            status.effects.tempAttackBuffs.push_back({ command.attackBonus, command.durationFrames });
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

bool reduceFrameGameplayCommand(
    BattleRuntimeState& state,
    const BattleGameplayCommand& command,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleGameplayCommand>& pending,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    if (const auto* hp = std::get_if<BattleHpDamageCommand>(&command))
    {
        return tryAppendFrameDamageTransaction(state, *hp);
    }
    if (const auto* mp = std::get_if<BattleMpDamageCommand>(&command))
    {
        return tryAppendFrameDamageTransaction(state, *mp);
    }
    if (const auto* sideEffect = std::get_if<BattleAcceptedHitSideEffectCommand>(&command))
    {
        return tryAppendFrameDamageTransaction(state, *sideEffect);
    }
    if (std::holds_alternative<BattleTeamHealCommand>(command)
        || std::holds_alternative<BattleTeamMpRestoreCommand>(command)
        || std::holds_alternative<BattleTeamShieldCommand>(command))
    {
        return applyFrameTeamEffectCommand(state, command, teamEffectEvents, logEvents, visualEvents);
    }
    if (const auto* projectile = std::get_if<BattleProjectileSpawnCommand>(&command))
    {
        state.pendingAttackSpawns.push_back(projectile->request);
        return true;
    }
    if (std::holds_alternative<BattleCurrentHpBlastCommand>(command)
        || std::holds_alternative<BattleSpiralBleedProjectileCommand>(command)
        || std::holds_alternative<BattleNearbyTrackingProjectilesCommand>(command)
        || std::holds_alternative<BattleHitExtraProjectilesCommand>(command)
        || std::holds_alternative<BattleShieldExplosionCommand>(command)
        || std::holds_alternative<BattleDeathAoeProjectileCommand>(command))
    {
        auto followUps = expandBattleProjectileFollowUpCommands(
            { command },
            state.projectileFollowUps,
            state.unitStore);
        pending.insert(
            pending.end(),
            std::make_move_iterator(followUps.commands.begin()),
            std::make_move_iterator(followUps.commands.end()));
        visualEvents.insert(
            visualEvents.end(),
            std::make_move_iterator(followUps.visualEvents.begin()),
            std::make_move_iterator(followUps.visualEvents.end()));
        return true;
    }
    if (const auto* mpRestore = std::get_if<BattleMpRestoreCommand>(&command))
    {
        return applyFrameMpRestoreCommand(state, *mpRestore, logEvents);
    }
    if (const auto* autoUltimate = std::get_if<BattleAutoUltimateCommand>(&command))
    {
        return tryCommitAutoUltimate(
            state,
            autoUltimate->unitId,
            autoUltimate->consumeMp,
            autoUltimate->announce,
            attackSoundIds,
            effectCommands,
            pending,
            gameplayEvents,
            logEvents,
            visualEvents);
    }
    if (const auto* knockback = std::get_if<BattleKnockbackCommand>(&command))
    {
        auto& unit = state.unitStore.requireUnit(knockback->targetUnitId);
        unit.motion.velocity += knockback->velocityDelta;
        if (knockback->velocityCap > 0.0 && unit.motion.velocity.norm() > knockback->velocityCap)
        {
            unit.motion.velocity.normTo(static_cast<float>(knockback->velocityCap));
        }
        requireMappedById(state.movement.agents, knockback->targetUnitId).physics.velocity = unit.motion.velocity;
        return true;
    }
    if (const auto* tempAttack = std::get_if<BattleTempAttackBuffCommand>(&command))
    {
        return applyFrameTempAttackBuffCommand(state, *tempAttack, logEvents);
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
    if (std::holds_alternative<BattleProjectileCancelDamageCommand>(command))
    {
        return true;
    }
    if (const auto* heal = std::get_if<BattleUnitHealCommand>(&command))
    {
        return applyFrameUnitHealCommand(state, *heal, logEvents, visualEvents);
    }
    if (const auto* shield = std::get_if<BattleUnitShieldCommand>(&command))
    {
        return applyFrameUnitShieldCommand(state, *shield, logEvents);
    }
    assert(false);
    return false;
}

void reduceFrameGameplayCommandsImpl(
    BattleRuntimeState& state,
    std::vector<BattleGameplayCommand>& commands,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    std::vector<BattleGameplayCommand> pending = std::move(commands);
    std::vector<BattleGameplayCommand> unreduced;
    for (std::size_t i = 0; i < pending.size(); ++i)
    {
        if (!reduceFrameGameplayCommand(
            state,
            pending[i],
            attackSoundIds,
            rumbles,
            effectCommands,
            pending,
            teamEffectEvents,
            gameplayEvents,
            logEvents,
            visualEvents))
        {
            unreduced.push_back(std::move(pending[i]));
        }
    }
    commands = std::move(unreduced);
}

void reduceFrameGameplayCommands(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.frameCommands,
        frame.attackSoundIds,
        frame.rumbles,
        frame.effectCommands,
        frame.teamEffectEvents,
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents);
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
    case BattleDamageStatusType::Frozen:
        return logStatusFrames<BattleLogTextTone::Negative>("受擊硬直", event.value);
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

void appendFrameDeathAoeCommand(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleDamageTransactionResult& transaction,
    int deadUnitId)
{
    auto effectIt = state.damage.unitEffects.find(deadUnitId);
    if (effectIt == state.damage.unitEffects.end() || effectIt->second.deathAoePct <= 0)
    {
        return;
    }

    BattleDeathAoeProjectileCommand command;
    command.sourceUnitId = deadUnitId;
    command.trackedTargetUnitId = transaction.attacker.id;
    command.damage = std::max(1, transaction.defender.vitals.maxHp * effectIt->second.deathAoePct / 100);
    command.damagePct = effectIt->second.deathAoePct;
    command.stunFrames = effectIt->second.deathAoeStunFrames;
    command.maxTargets = effectIt->second.deathAoeMaxTargets;
    frame.frameCommands.push_back(command);
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

    std::set<int> aliveTeams;
    for (const auto& unit : state.unitStore.units)
    {
        if (unit.alive)
        {
            aliveTeams.insert(unit.team);
        }
    }
    if (aliveTeams.size() != 1)
    {
        return;
    }

    state.result.ended = true;
    state.result.winningTeam = *aliveTeams.begin();
    state.result.endedFrame = state.movement.frame;
    state.result.eventEmitted = true;

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
    const BattleStatusRuntimeUnit* status,
    BattleDamageModifierState& modifier)
{
    if (!status)
    {
        return;
    }

    modifier.poisonTimer = status->effects.poisonTimer;
    modifier.outgoingDamageReduceDebuffs.clear();
    for (const auto& debuff : status->effects.damageReduceDebuffs)
    {
        modifier.outgoingDamageReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
    }
}

std::vector<std::size_t> orderedFramePendingDamageIndexes(
    const std::vector<BattlePendingDamageIntent>& pendingDamage,
    bool sortByDefenderMagnitude)
{
    std::vector<std::size_t> indexes(pendingDamage.size());
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

    const auto& defenderUnit = state.unitStore.requireUnit(request.defenderUnitId);
    auto& defenderExtras = requireById(state.damage.unitExtras, request.defenderUnitId);
    transaction.defender = makeBattleDamageUnitState(defenderUnit, &defenderExtras);
    auto& defenderCombo = requireMappedById(state.combo.units, request.defenderUnitId);
    transaction.defenderModifiers = makeBattleDamageModifierState(&defenderCombo);

    const auto& defenderStatus = requireById(state.status.units, request.defenderUnitId);
    transaction.defenderStatus = makeBattleStatusUnitState(defenderStatus, defenderUnit);
    applyLiveStatusToDamageModifier(&defenderStatus, transaction.defenderModifiers);
    transaction.defenderCooldown = makeBattleFrameCooldownState(defenderUnit);

    if (request.attackerUnitId >= 0)
    {
        const auto& attackerUnit = state.unitStore.requireUnit(request.attackerUnitId);
        auto& attackerExtras = requireById(state.damage.unitExtras, request.attackerUnitId);
        transaction.attacker = makeBattleDamageUnitState(attackerUnit, &attackerExtras);

        auto& attackerCombo = requireMappedById(state.combo.units, request.attackerUnitId);
        transaction.attackerModifiers = makeBattleDamageModifierState(&attackerCombo);
        applyLiveStatusToDamageModifier(
            tryFindById(state.status.units, request.attackerUnitId),
            transaction.attackerModifiers);
    }
    else
    {
        transaction.attacker.id = request.attackerUnitId;
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
        log.segments = formatAppliedStatusLog(transaction, event);
        frame.logEvents.push_back(std::move(log));
    }

    if (transaction.blockedByFirstHit)
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
        log.segments = battleLogText("格擋了首輪傷害", BattleLogTextTone::Positive);
        frame.logEvents.push_back(std::move(log));
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

        appendFrameDeathAoeCommand(state, frame, transaction, event.targetUnitId);
        auto deathEvents = BattleDeathEffectSystem().applyAllyDeathEffects(
            state.unitStore,
            state.deathEffects.store,
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

    auto& defenderCombo = requireMappedById(state.combo.units, transaction.defender.id);
    int shieldExplosionPct = 0;
    auto shieldBreakCommands = BattleComboTriggerSystem().collectShieldBreakCommands(
        defenderCombo,
        { BattleComboTriggerHook::ShieldBreak, transaction.defender.id, transaction.defender.id },
        state.random);
    for (const auto& shieldBreak : shieldBreakCommands)
    {
        bool activated = true;
        switch (shieldBreak.type)
        {
        case BattleShieldBreakCommandType::ShieldExplosion:
            shieldExplosionPct = std::max(shieldExplosionPct, shieldBreak.value);
            break;
        case BattleShieldBreakCommandType::AutoUltimate:
            frame.frameCommands.push_back(BattleAutoUltimateCommand{ transaction.defender.id, false });
            break;
        case BattleShieldBreakCommandType::TempFlatAttack:
            frame.frameCommands.push_back(BattleTempAttackBuffCommand{
                transaction.defender.id,
                shieldBreak.value,
                shieldBreak.durationFrames,
                std::format("護盾爆炸（臨時攻+{}，{}幀）", shieldBreak.value, shieldBreak.durationFrames),
            });
            break;
        case BattleShieldBreakCommandType::MpRestore:
        {
            int restored = std::min(
                shieldBreak.value,
                std::max(0, transaction.defender.vitals.maxMp - transaction.defender.vitals.mp));
            if (restored > 0)
            {
                frame.frameCommands.push_back(BattleMpRestoreCommand{
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
                static_cast<size_t>(shieldBreak.effectIndex));
        }
    }
    if (shieldExplosionPct > 0)
    {
        int explosionDamage = std::max(
            1,
            defenderCombo.shieldPctMaxHP * transaction.defender.vitals.maxHp / 100 * shieldExplosionPct / 100);
        frame.frameCommands.push_back(BattleShieldExplosionCommand{
            transaction.defender.id,
            5,
            KysChess::EFT_SHIELD_BLAST,
            explosionDamage,
            "護盾爆炸",
        });
    }
}

void appendFrameDamageGameplayEvents(
    BattleFrameContext& frame,
    const BattleDamageTransactionResult& transaction)
{
    for (const auto& event : transaction.events)
    {
        if (event.type == BattleDamageEventType::UnitDied)
        {
            continue;
        }
        frame.gameplayEvents.push_back(toGameplayEvent(event));
    }
}

void expandFrameDamageFollowUpCommands(BattleRuntimeState& state, BattleFrameContext& frame)
{
    auto followUps = expandBattleProjectileFollowUpCommands(
        frame.frameCommands,
        state.projectileFollowUps,
        state.unitStore);
    frame.frameCommands = std::move(followUps.commands);
    frame.visualEvents.insert(
        frame.visualEvents.end(),
        std::make_move_iterator(followUps.visualEvents.begin()),
        std::make_move_iterator(followUps.visualEvents.end()));
    frame.logEvents.insert(
        frame.logEvents.end(),
        std::make_move_iterator(followUps.logEvents.begin()),
        std::make_move_iterator(followUps.logEvents.end()));
}

bool comboEffectIsApplied(const KysChess::RoleComboState& state, int comboId)
{
    return std::ranges::any_of(
        state.appliedEffects,
        [comboId](const KysChess::AppliedEffectInstance& effect)
        {
            return effect.sourceComboId == comboId;
        });
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
    const auto& dead = state.unitStore.requireUnit(deadUnitId);
    BattleRuntimeUnit* best = nullptr;
    for (auto& candidate : state.unitStore.units)
    {
        if (!candidate.alive || candidate.id == dead.id || candidate.team != dead.team)
        {
            continue;
        }
        const auto* extras = tryFindById(state.deathEffects.store.units, candidate.id);
        if (!extras || !unitBelongsToCombo(*extras, comboId))
        {
            continue;
        }
        const auto comboIt = state.combo.units.find(candidate.id);
        assert(comboIt != state.combo.units.end());
        if (comboEffectIsApplied(comboIt->second, comboId))
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
    const auto& deadExtras = requireById(state.deathEffects.store.units, deadUnitId);
    const auto& deadCombo = requireMappedById(state.combo.units, deadUnitId);
    for (int comboId : deadExtras.comboIds)
    {
        if (!isAntiComboId(state.deathEffects.store, comboId)
            || !comboEffectIsApplied(deadCombo, comboId))
        {
            continue;
        }

        auto* target = findAntiComboTransferTarget(state, deadUnitId, comboId);
        if (!target)
        {
            continue;
        }

        auto& targetCombo = requireMappedById(state.combo.units, target->id);
        auto& targetExtras = requireById(state.deathEffects.store.units, target->id);
        for (const auto& effect : deadCombo.appliedEffects)
        {
            if (effect.sourceComboId != comboId)
            {
                continue;
            }
            KysChess::ChessBattleEffects::applyEffect(targetCombo, effect, comboId);
            targetExtras.appliedEffects.push_back(effect);
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
        auto& combo = requireMappedById(state.combo.units, deadUnitId);
        combo.onSkillTeamHealPending = false;
        applyRuntimeAntiComboTransfer(state, deadUnitId, logEvents);
    }
}

std::vector<BattleStatusEvent> advanceStatus(BattleRuntimeState& state)
{
    state.status.config.frame = state.movement.frame;
    auto statusTick = BattleStatusSystem(state.status.config).tick(state.unitStore, state.status.units);
    for (const auto& event : statusTick.events)
    {
        if (event.type != BattleStatusEventType::PoisonDamage
            && event.type != BattleStatusEventType::BleedDamage)
        {
            continue;
        }

        state.unitStore.requireUnit(event.sourceUnitId);
        state.unitStore.requireUnit(event.unitId);
        requireById(state.status.units, event.unitId);

        BattleDamageRequest request;
        request.attackerUnitId = event.sourceUnitId;
        request.defenderUnitId = event.unitId;
        request.baseDamage = event.value;
        request.preResolvedDamage = true;
        BattleDamagePresentationInput presentation;
        presentation.segments = battleLogText(event.reason, BattleLogTextTone::SkillName);
        applyStatusTickDamagePresentation(state, event.type, event.unitId, presentation);
        appendFramePendingDamage(state, std::move(request), std::move(presentation));
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
        case BattleTeamEffectEventType::ShieldGain:
        case BattleTeamEffectEventType::CooldownReduced:
            break;
        }
    }
}

void appendEffectCommandLogEvents(
    std::vector<BattleLogEvent>& logEvents,
    const std::vector<BattleEffectCommand>& commands)
{
    for (const auto& command : commands)
    {
        BattleLogEvent log;
        log.sourceUnitId = command.sourceUnitId;
        log.targetUnitId = command.targetUnitId;
        log.amount = command.value;
        log.segments = formatEffectCommandLogSegments(command);
        switch (command.type)
        {
        case BattleEffectCommandType::Heal:
            log.type = BattleLogEventType::Heal;
            break;
        case BattleEffectCommandType::AddShield:
        case BattleEffectCommandType::AddInvincibility:
        case BattleEffectCommandType::ModifyResource:
        case BattleEffectCommandType::ModifyCooldown:
        case BattleEffectCommandType::DedicatedEffect:
            log.type = BattleLogEventType::Status;
            break;
        }
        logEvents.push_back(std::move(log));
    }
}

void applyRuntimeTeamEvents(
    BattleRuntimeState& state,
    int sourceUnitId,
    const BattleComboFrameRuntimeEvent& event,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    if (state.unitStore.units.empty())
    {
        return;
    }

    const auto& source = state.unitStore.requireUnit(sourceUnitId);
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
        events = system.applySelfHeal(state.unitStore, sourceUnitId, event.value);
        reason = "生命回復";
        break;
    case BattleComboFrameRuntimeEventType::HealAura:
        assert(state.teamEffects.healAuraRadius > 0.0);
        events = system.applyHealAura(
            state.unitStore,
            sourceUnitId,
            event.value,
            event.value2,
            state.teamEffects.healAuraRadius,
            event.durationFrames);
        reason = "治療光環";
        break;
    case BattleComboFrameRuntimeEventType::HealPercentSelf:
        events = system.applySelfHeal(state.unitStore, sourceUnitId, event.value);
        reason = "爆發治療";
        break;
    default:
        assert(false);
        break;
    }

    appendTeamEffectLogEvents(logEvents, events, reason);
    appendTeamEffectVisualEvents(visualEvents, events);
    teamEffectEvents.insert(
        teamEffectEvents.end(),
        events.begin(),
        events.end());
}

void applyBroadcastTriggerTimer(BattleRuntimeState& state, int sourceUnitId, const BattleComboFrameRuntimeEvent& event)
{
    assert(event.durationFrames > 0);
    const auto& source = state.unitStore.requireUnit(sourceUnitId);
    if (!source.alive)
    {
        return;
    }
    for (const auto& unit : state.unitStore.units)
    {
        if (!unit.alive || unit.id == sourceUnitId || unit.team != source.team)
        {
            continue;
        }
        auto comboIt = state.combo.units.find(unit.id);
        assert(comboIt != state.combo.units.end());
        comboIt->second.triggerTimers[event.trigger] = event.durationFrames;
    }
}

void applyCastPostSkillInvincibility(
    BattleRuntimeState& state,
    int sourceUnitId,
    int frames,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleLogEvent>& logEvents)
{
    if (frames <= 0)
    {
        return;
    }

    auto& unit = state.unitStore.requireUnit(sourceUnitId);
    const int before = unit.invincible;
    unit.invincible = std::max(unit.invincible, frames);
    const int added = unit.invincible - before;
    if (added <= 0)
    {
        return;
    }

    BattleEffectCommand command{
        BattleEffectCommandType::AddInvincibility,
        -1,
        sourceUnitId,
        sourceUnitId,
        added,
        "技能後無敵",
    };
    const std::vector<BattleEffectCommand> logCommands{ command };
    appendEffectCommandLogEvents(logEvents, logCommands);
    effectCommands.push_back(std::move(command));
}

void applyRuntimeComboEvents(BattleRuntimeState& state, BattleFrameContext& frame)
{
    for (const auto& result : frame.runtimeCommits)
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
                    frame.teamEffectEvents,
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
        auto comboIt = state.combo.units.find(result.unitId);
        if (comboIt != state.combo.units.end())
        {
            auto committed = result.comboState;
            committed.triggerTimers = comboIt->second.triggerTimers;
            comboIt->second = std::move(committed);
        }
        if (autoUltimateReady)
        {
            frame.deferredCommands.push_back(BattleAutoUltimateCommand{ result.unitId, false, true });
        }
    }
}

void applyPendingTeamEffects(BattleRuntimeState& state, BattleFrameContext& frame)
{
    if (state.unitStore.units.empty())
    {
        return;
    }

    std::vector<BattleGameplayCommand> unappliedCommands;
    for (const auto& command : state.teamEffects.pendingCommands)
    {
        const int sourceUnitId = std::visit([](const auto& typedCommand) -> int
            {
                using Command = std::decay_t<decltype(typedCommand)>;
                if constexpr (std::is_same_v<Command, BattleTeamHealCommand>
                    || std::is_same_v<Command, BattleTeamMpRestoreCommand>
                    || std::is_same_v<Command, BattleTeamShieldCommand>)
                {
                    return typedCommand.sourceUnitId;
                }
                return -1;
        }, command);
        if (sourceUnitId >= 0)
        {
            const auto& source = state.unitStore.requireUnit(sourceUnitId);
            if (!source.alive)
            {
                continue;
            }
            auto application = applyBattleTeamEffectCommand(state.unitStore, command);
            frame.logEvents.insert(
                frame.logEvents.end(),
                application.logEvents.begin(),
                application.logEvents.end());
            appendTeamEffectVisualEvents(frame.visualEvents, application.events);
            frame.teamEffectEvents.insert(
                frame.teamEffectEvents.end(),
                application.events.begin(),
                application.events.end());
            continue;
        }
        unappliedCommands.push_back(command);
    }
    state.teamEffects.pendingCommands = std::move(unappliedCommands);
}

BattleMovementPhysicsCollisionWorld makeMovementPhysicsCollisionWorld(const BattleRuntimeState& state)
{
    BattleMovementPhysicsCollisionWorld collision;
    collision.tileWidth = state.movementPhysics.terrain.tileWidth;
    collision.coordCount = state.movementPhysics.terrain.coordCount;
    collision.defaultSeparationDistance = state.movementPhysics.terrain.defaultSeparationDistance;
    collision.walkableByCell = state.movementPhysics.terrain.walkableByCell;
    collision.units.reserve(state.unitStore.units.size());
    for (const auto& unit : state.unitStore.units)
    {
        collision.units.push_back({
            unit.id,
            unit.alive,
            unit.motion.position,
        });
    }
    return collision;
}

PostPhysicsMotionMap makePostPhysicsMotionMap(
    const std::vector<BattleFrameMovementPhysicsUnitResult>& physicsResults)
{
    PostPhysicsMotionMap postPhysics;
    for (const auto& result : physicsResults)
    {
        postPhysics.emplace(result.unitId, result.state);
    }
    return postPhysics;
}

std::vector<BattleFrameMovementPhysicsUnitResult> computeMovementPhysics(BattleRuntimeState& state)
{
    std::vector<BattleFrameMovementPhysicsUnitResult> physicsResults;
    if (state.unitStore.units.empty())
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

    auto collision = makeMovementPhysicsCollisionWorld(state);

    for (auto& unit : state.unitStore.units)
    {
        assert(unit.id >= 0);
        if (!unit.alive && !needsCorpsePhysics(unit))
        {
            continue;
        }
        auto& physics = requireMappedById(state.movement.agents, unit.id).physics;

        BattleFrameMovementPhysicsUnitResult result;
        result.unitId = unit.id;
        result.state = physics;
        result.state.position = unit.motion.position;
        result.state.velocity = unit.motion.velocity;
        result.state.acceleration = unit.motion.acceleration;
        if (auto* status = tryFindById(state.status.units, unit.id))
        {
            result.frozenFrames = status->effects.frozenTimer;
        }

        if (result.frozenFrames > 0)
        {
            --result.frozenFrames;
            physicsResults.push_back(std::move(result));
            continue;
        }

        bool actionDashActive = false;
        if (unit.operationType == BattleOperationType::Dash && unit.haveAction)
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
        auto movementSnapshot = makeBattleMovementPlanUnit(unit, BattleRuntimeMoveSpeedDivisor);
        refreshMovementSkillProfile(movementSnapshot, unit, state);
        movementSnapshot.velocity = result.state.velocity;
        movementSnapshot.dashFramesRemaining = result.state.movementDashFrames;
        movementSnapshot.dashCooldownRemaining = result.state.movementDashCooldown;
        movementSnapshot.movementDashSpreadFramesRemaining = result.state.movementDashSpreadFrames;
        movementSnapshot.postDashRetreatFramesRemaining = result.state.postDashRetreatFrames;
        movementSnapshot.postDashChaosFramesRemaining = result.state.postDashChaosFrames;
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

void commitFrameMovement(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const std::vector<BattleFrameMovementPhysicsUnitResult>& physicsResults,
    BattleTickResult movement)
{
    frame.movement = std::move(movement);
    state.movement.frame = frame.movement.frame;
    state.movement.movementReservations = frame.movement.movementReservations;
    for (const auto& [unitId, decision] : frame.movement.decisions)
    {
        auto& agent = requireMappedById(state.movement.agents, unitId);
        agent.targetId = decision.targetId;
        agent.assignedSlot = decision.slot;
        agent.slotSwitchCooldownRemaining = decision.slotSwitchCooldownRemaining;
    }

    for (const auto& physicsResult : physicsResults)
    {
        auto& unit = state.unitStore.requireUnit(physicsResult.unitId);
        auto& physics = requireMappedById(state.movement.agents, physicsResult.unitId).physics;

        unit.motion.position = physicsResult.state.position;
        unit.motion.velocity = physicsResult.state.velocity;
        unit.motion.acceleration = physicsResult.state.acceleration;
        physics = physicsResult.state;
        if (auto* status = tryFindById(state.status.units, physicsResult.unitId))
        {
            status->effects.frozenTimer = physicsResult.frozenFrames;
        }
    }

    if (state.movementPhysics.terrain.walkableByCell.empty())
    {
        return;
    }

    assert(state.unitStore.gridTransform.tileWidth > 0.0);
    assert(state.unitStore.gridTransform.coordCount > 0);

    for (const auto& [unitId, decision] : frame.movement.decisions)
    {
        auto& runtimeUnit = state.unitStore.requireUnit(unitId);
        const auto action = decision.action;

        Pointf syncedVelocity = decision.velocity;
        if (action == MovementAction::Move)
        {
            syncedVelocity = { 0.0f, 0.0f, decision.velocity.z };
        }

        const auto acceleration = runtimeUnit.motion.acceleration;
        const Pointf syncedPosition = action == MovementAction::Hold
            ? runtimeUnit.motion.position
            : decision.destination;
        state.unitStore.setMotion(
            unitId,
            syncedPosition,
            syncedVelocity,
            acceleration);

        auto& physics = requireMappedById(state.movement.agents, unitId).physics;
        physics.position = syncedPosition;
        physics.velocity = syncedVelocity;
        physics.acceleration = acceleration;
        physics.movementDashFrames = decision.dashFramesRemaining;
        physics.movementDashCooldown = decision.dashCooldownRemaining;
        physics.movementDashSpreadFrames = decision.movementDashSpreadFramesRemaining;
        physics.postDashRetreatFrames = decision.postDashRetreatFramesRemaining;
        physics.postDashChaosFrames = decision.postDashChaosFramesRemaining;
    }
}

void advanceMotionFrame(BattleRuntimeState& state, BattleFrameContext& frame)
{
    prepareMovementAgents(state);
    auto physicsResults = computeMovementPhysics(state);
    auto movementInput = makeFrameMovementPlanInput(state, makePostPhysicsMotionMap(physicsResults));
    auto movement = BattleMovementPlanner(std::move(movementInput)).tick();
    commitFrameMovement(state, frame, physicsResults, std::move(movement));
}

void commitActionFrameStateToRuntime(BattleRuntimeUnit& unit, const BattleUnitFrameTickState& state)
{
    unit.animation.cooldown = state.cooldown;
    unit.animation.actFrame = state.actFrame;
    unit.animation.actType = state.actType;
    unit.operationType = state.operationType;
    unit.haveAction = state.haveAction;
}

void clearActionFrameState(BattleUnitFrameTickState& state)
{
    state.cooldown = 0;
    state.actFrame = 0;
    state.actType = -1;
    state.operationType = BattleOperationType::None;
    state.haveAction = false;
}

void clearRuntimePendingAction(BattleRuntimeState& state, int unitId)
{
    auto& unit = state.unitStore.requireUnit(unitId);
    unit.animation.cooldown = 0;
    unit.animation.actFrame = 0;
    unit.animation.actType = -1;
    unit.operationType = BattleOperationType::None;
    unit.haveAction = false;
    state.action.pendingCasts.erase(unitId);
    state.ultimateCasters.erase(unitId);
}

void clearDeadRuntimePendingActions(BattleRuntimeState& state)
{
    for (const auto& unit : state.unitStore.units)
    {
        if (!unit.alive)
        {
            clearRuntimePendingAction(state, unit.id);
        }
    }
}

int actionCastFrame(const BattleRuntimeState& state, BattleOperationType operationType)
{
    const int legacyOperation = toLegacyOperationType(operationType);
    if (legacyOperation < 0)
    {
        return 0;
    }
    assert(static_cast<std::size_t>(legacyOperation) < state.action.castFrames.size());
    return state.action.castFrames[legacyOperation];
}

int actionRecoveryFrames(const BattleRuntimeState& state, BattleOperationType operationType)
{
    return operationType == BattleOperationType::Dash
        ? state.action.dashRecoveryFrames
        : state.action.actionRecoveryFrames;
}

BattleUnitFrameTickState makeActionRuntimeState(const BattleRuntimeUnit& unit)
{
    BattleUnitFrameTickState state;
    state.cooldown = unit.animation.cooldown;
    state.actFrame = unit.animation.actFrame;
    state.actType = unit.animation.actType;
    state.operationType = unit.operationType;
    state.haveAction = unit.haveAction;
    state.physicalPower = unit.physicalPower;
    return state;
}

void advanceActionFrameUnits(BattleRuntimeState& state, BattleFrameContext& frame)
{
    const auto& movement = frame.movement;
    auto& effectCommands = frame.effectCommands;
    auto& frameCommands = frame.frameCommands;
    auto& gameplayEvents = frame.gameplayEvents;
    auto& logEvents = frame.logEvents;
    auto& visualEvents = frame.visualEvents;

    for (auto& unit : state.unitStore.units)
    {
        assert(unit.id >= 0);
        if (!unit.alive)
        {
            clearRuntimePendingAction(state, unit.id);
            continue;
        }

        const BattleCastInput* castPlanInput = nullptr;
        std::optional<BattleCastInput> runtimeCastPlan;
        auto runtimePlanSeedIt = state.action.planSeeds.find(unit.id);
        bool usingRuntimeCastPlan = false;
        if (runtimePlanSeedIt != state.action.planSeeds.end()
            && !unit.haveAction
            && !actionMovementDashActive(state, unit.id))
        {
            runtimeCastPlan = makeRuntimeCastInputFromSeed(
                state,
                unit,
                runtimePlanSeedIt->second,
                unit.animation.cooldown == 0,
                false,
                false);
            castPlanInput = &*runtimeCastPlan;
            usingRuntimeCastPlan = true;
        }
        auto pendingCastIt = state.action.pendingCasts.find(unit.id);
        BattleFrameActionUnitResult result;
        result.unitId = unit.id;
        auto actionState = makeActionRuntimeState(unit);
        const bool wasActionActive = actionState.haveAction;
        bool cancelledAction = false;

        if (!actionState.haveAction && castPlanInput)
        {
            auto castInput = refreshedCastInput(state, movement, *castPlanInput);
            if (usingRuntimeCastPlan)
            {
                castInput.unit.canStartAttack = castInput.unit.canStartAttack
                    && unit.animation.cooldown == 0;
            }
            auto cast = BattleCastPlanner().plan(castInput);
            gameplayEvents.insert(gameplayEvents.end(), cast.gameplayEvents.begin(), cast.gameplayEvents.end());
            logEvents.insert(logEvents.end(), cast.logEvents.begin(), cast.logEvents.end());
            visualEvents.insert(visualEvents.end(), cast.visualEvents.begin(), cast.visualEvents.end());
            if (cast.decision.canCast)
            {
                state.unitStore.requireUnit(unit.id).motion.facing = runtimeCastFacing(state, unit, castInput);
                result.castStarted = true;
                actionState.haveAction = true;
                actionState.actFrame = 0;
                actionState.actType = cast.decision.ultimate
                    ? castInput.ultimateSkill.magicType
                    : castInput.normalSkill.magicType;
                actionState.operationType = cast.decision.operationType;
                actionState.cooldown = cast.animation.cooldownFrames;
                state.action.pendingCasts[unit.id] = makePendingCastAction(castInput, cast);
                requireMappedById(state.movement.agents, unit.id).physics.movementDashSpreadFrames = 0;
            }
            result.castResult = std::move(cast);
        }
        else if (actionState.haveAction && pendingCastIt != state.action.pendingCasts.end())
        {
            const int castFrame = actionCastFrame(state, actionState.operationType);
            if (actionState.actFrame == castFrame)
            {
                result.actionCommitted = true;
                auto actionInput = tryMakeRuntimeActionCommitInput(state, movement, pendingCastIt->second);
                auto& combo = requireMappedById(state.combo.units, unit.id);
                state.action.pendingCasts.erase(pendingCastIt);
                if (actionInput)
                {
                    result.actionInput = std::move(*actionInput);
                    result.castCommitted = true;
                    state.unitStore.requireUnit(unit.id).motion.facing = result.actionInput.committedFacing;
                    result.actionResult = BattleActionCommitSystem().commit(result.actionInput, combo, state.unitStore);
                }
                else
                {
                    result.castCommitted = false;
                    result.actionResult.combo = combo;
                    result.actionResult.operationCount = unit.operationCount;
                    clearActionFrameState(actionState);
                    cancelledAction = true;
                }
                if (result.actionInput.hasCast)
                {
                    schedulePostDashRetreat(state, unit.id, result.actionInput.cast);
                    applyCastPostSkillInvincibility(
                        state,
                        unit.id,
                        result.actionResult.combo.postSkillInvincFrames,
                        effectCommands,
                        logEvents);
                    auto castScopedCommands = collectFrameCastScopedComboCommands(
                        result.actionResult.combo,
                        state.random,
                        unit.id,
                        committedCastProjectileSpeed(result.actionResult, state.projectileFollowUps.projectileSpeed));
                    frameCommands.insert(
                        frameCommands.end(),
                        std::make_move_iterator(castScopedCommands.begin()),
                        std::make_move_iterator(castScopedCommands.end()));
                }
                if (result.actionInput.hasCast && result.actionInput.cast.decision.soundId >= 0)
                {
                    frame.attackSoundIds.push_back(result.actionInput.cast.decision.soundId);
                }
                state.pendingAttackSpawns.insert(
                    state.pendingAttackSpawns.end(),
                    result.actionResult.attackSpawnRequests.begin(),
                    result.actionResult.attackSpawnRequests.end());
                logEvents.insert(
                    logEvents.end(),
                    result.actionResult.logEvents.begin(),
                    result.actionResult.logEvents.end());
                visualEvents.insert(
                    visualEvents.end(),
                    result.actionResult.visualEvents.begin(),
                    result.actionResult.visualEvents.end());
                frame.blinkSoundCount += static_cast<int>(result.actionResult.blinkTeleports.size());
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
                state.action.pendingCasts.erase(unit.id);
            }
        }

        if (result.actionCommitted)
        {
            unit.operationCount = result.actionResult.operationCount;
            if (result.actionInput.hasCast)
            {
                applyRuntimeUnitMpDelta(unit, result.actionInput.cast.mpDelta);
                requireMappedById(state.combo.units, unit.id) = result.actionResult.combo;
                state.ultimateCasters.erase(unit.id);
            }
        }
        commitActionFrameStateToRuntime(unit, actionState);
    }
}

void advanceAttacksAndResolveHits(BattleRuntimeState& state, BattleFrameContext& frame)
{
    auto& attackEvents = frame.attackEvents;
    auto& frameCommands = frame.frameCommands;
    auto& logEvents = frame.logEvents;
    auto& visualEvents = frame.visualEvents;

    state.attacks.frame = state.movement.frame;
    BattleAttackSystem attackSystem;
    for (const auto& request : state.pendingAttackSpawns)
    {
        attackEvents.push_back(attackSystem.spawn(state.attacks, request));
    }
    state.pendingAttackSpawns.clear();
    auto tickEvents = attackSystem.tick(state.attacks, state.unitStore);
    attackEvents.insert(
        attackEvents.end(),
        std::make_move_iterator(tickEvents.begin()),
        std::make_move_iterator(tickEvents.end()));
    applyProjectileCancelDamageResults(
        state,
        attackEvents,
        frameCommands);
    appendProjectileCancellationLogEvents(state.attacks, attackEvents, logEvents, false);
    resolveHitEvents(
        state,
        attackEvents,
        frameCommands,
        logEvents,
        visualEvents);
    reduceFrameGameplayCommands(state, frame);
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
    BattleDamageRequest& request,
    BattleDamagePresentationInput& presentation)
{
    assert(request.attackerUnitId >= 0);
    assert(request.defenderUnitId >= 0);

    auto& attackerCombo = requireMappedById(state.combo.units, request.attackerUnitId);
    const auto& defender = state.unitStore.requireUnit(request.defenderUnitId);
    auto execute = BattleComboTriggerSystem().resolveExecuteCombo(
        attackerCombo,
        {
            request.attackerUnitId,
            request.defenderUnitId,
            defender.vitals.hp,
            defender.vitals.maxHp,
            request.baseDamage,
            true,
        },
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

    const auto& defenderCombo = requireMappedById(state.combo.units, request.defenderUnitId);
    auto blockCommands = BattleComboTriggerSystem().collectDefenderBlockCommands(
        defenderCombo,
        { false, false },
        state.random);
    if (blockCommands.empty())
    {
        return false;
    }

    for (auto blockCommand : blockCommands)
    {
        frame.visualEvents.push_back(roleEffectEvent(
            request.defenderUnitId,
            KysChess::EFT_BLOCK,
            CoreRoleStatusEffectFrames));
        switch (blockCommand)
        {
        case BattleDefenderBlockCommand::CounterUltimateBlock:
            appendStatusEventLog(
                frame.logEvents,
                request.defenderUnitId,
                request.attackerUnitId,
                "格擋後釋放絕招");
            frame.frameCommands.push_back(BattleAutoUltimateCommand{ request.defenderUnitId, false });
            break;
        case BattleDefenderBlockCommand::Block:
            appendStatusEventLog(
                frame.logEvents,
                request.defenderUnitId,
                request.attackerUnitId,
                "格擋了本次攻擊");
            break;
        default:
            assert(false);
        }
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
        && applyFrameExecuteReaction(state, frame, request, presentation);
    if (!executed
        && intent.canTriggerDefenderBlock
        && applyFrameDefenderBlockCommands(state, frame, request))
    {
        return false;
    }
    return true;
}

void applyDamageAndLifecycle(BattleRuntimeState& state, BattleFrameContext& frame)
{
    const auto& frameStartMotion = frame.frameStartMotion;
    auto& logEvents = frame.logEvents;
    auto& visualEvents = frame.visualEvents;

    if (state.result.ended && state.damage.pendingDamage.empty())
    {
        return;
    }

    bool unitDied = false;

    std::vector<int> deadUnitIds;
    for (const auto pendingIndex : orderedFramePendingDamageIndexes(
             state.damage.pendingDamage,
             state.damage.sortPendingDamageByDefenderMagnitude))
    {
        const auto& intent = state.damage.pendingDamage[pendingIndex];
        if (!state.unitStore.requireUnit(intent.request.defenderUnitId).alive)
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
            makeFrameDamageTransactionInput(state, request));
        applyFrameDamageTakenMpGain(transaction);
        applyDamageResultToFrameState(state, transaction, frameStartMotion);
        appendFrameShieldBreakCommands(state, frame, transaction);
        appendFrameDamageOutputEvents(frame, presentation, transaction);
        appendFrameDamagePreDeathLogEvents(frame, transaction);
        appendFrameDamageGameplayEvents(frame, transaction);
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
        clearDeadRuntimePendingActions(state);
        appendEnemyTopDebuffUpdates(state, logEvents);
    }

    updateFrameBattleResultAfterDamage(state, frame);
    expandFrameDamageFollowUpCommands(state, frame);
    state.damage.pendingDamage.clear();
}

void emitPresentationFrame(BattleRuntimeState& state, BattleFrameContext& frame)
{
    auto& result = frame.result;
    auto& gameplayEvents = frame.gameplayEvents;
    auto& logEvents = frame.logEvents;
    auto& visualEvents = frame.visualEvents;

    BattlePresentationRecorder recorder;
    recorder.beginFrame(state.movement.frame);
    for (auto event : gameplayEvents)
    {
        recorder.recordGameplay(std::move(event));
    }
    for (auto event : visualEvents)
    {
        recorder.recordVisual(std::move(event));
    }
    for (auto event : logEvents)
    {
        recorder.recordLog(std::move(event));
    }
    for (const auto& event : frame.attackEvents)
    {
        recorder.recordGameplay(toGameplayEvent(event, state.attacks));
        for (auto presentation : toVisualEvents(event, state.attacks))
        {
            recorder.recordVisual(std::move(presentation));
        }
    }
    auto presentationFrame = recorder.consumeFrame();
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

BattleTeamEffectCommandApplication applyBattleTeamEffectCommand(
    BattleUnitStore& units,
    const BattleGameplayCommand& command)
{
    BattleTeamEffectCommandApplication result;
    BattleTeamEffectSystem system;
    if (const auto* heal = std::get_if<BattleTeamHealCommand>(&command))
    {
        assert(heal->sourceUnitId >= 0);
        result.events = system.applyTeamHeal(
            units,
            heal->sourceUnitId,
            heal->flatHeal,
            heal->pctHeal);
        appendTeamEffectLogEvents(result.logEvents, result.events, heal->reason);
        return result;
    }
    if (const auto* mp = std::get_if<BattleTeamMpRestoreCommand>(&command))
    {
        assert(mp->sourceUnitId >= 0);
        result.events = system.applyTeamMp(
            units,
            mp->sourceUnitId,
            mp->amount);
        appendTeamEffectLogEvents(result.logEvents, result.events, mp->reason);
        return result;
    }
    if (const auto* shield = std::get_if<BattleTeamShieldCommand>(&command))
    {
        assert(shield->sourceUnitId >= 0);
        result.events = system.applyTeamShield(
            units,
            shield->sourceUnitId,
            shield->amount,
            shield->refreshOnly);
        appendTeamEffectLogEvents(result.logEvents, result.events, shield->reason);
        return result;
    }
    assert(false);
    return result;
}

BattleDamageRuntimeUnit makeBattleDamageRuntimeUnit(const BattleDamageUnitState& unit)
{
    BattleDamageRuntimeUnit runtime;
    runtime.id = unit.id;
    runtime.hurtInvincFrames = unit.hurtInvincFrames;
    runtime.blockFirstHitsRemaining = unit.blockFirstHitsRemaining;
    runtime.deathPrevention = unit.deathPrevention;
    runtime.deathPreventionUsed = unit.deathPreventionUsed;
    runtime.deathPreventionFrames = unit.deathPreventionFrames;
    runtime.killHealPct = unit.killHealPct;
    runtime.killInvincFrames = unit.killInvincFrames;
    runtime.bloodlustAttackPerKill = unit.bloodlustAttackPerKill;
    return runtime;
}

BattleProjectileBouncePrime collectFrameProjectileBouncePrime(
    const KysChess::RoleComboState& state,
    int attackerUnitId,
    int rollPct,
    int defaultRange)
{
    return BattleComboTriggerSystem().collectProjectileBouncePrime(
        state,
        {
            attackerUnitId,
            rollPct,
            defaultRange,
        });
}

int collectFrameExtraProjectileCount(
    KysChess::RoleComboState& state,
    BattleRuntimeRandom& random,
    int unitId,
    int baseCount)
{
    return BattleComboTriggerSystem().collectExtraProjectileCount(
        state,
        { BattleComboTriggerHook::AfterSkillCast, unitId, -1 },
        baseCount,
        random);
}

std::vector<BattleGameplayCommand> collectFrameCastScopedComboCommands(
    KysChess::RoleComboState& state,
    BattleRuntimeRandom& random,
    int unitId,
    double projectileSpeed)
{
    assert(unitId >= 0);
    assert(projectileSpeed >= 0.0);

    auto events = BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::AfterSkillCast, unitId, -1 },
        {
            KysChess::EffectType::CurrentHPPctBlast,
            KysChess::EffectType::TeamMPRestore,
            KysChess::EffectType::FlatShield,
            KysChess::EffectType::SpiralBleedProjectile,
        },
        random);

    std::vector<BattleGameplayCommand> commands;
    for (const auto& event : events)
    {
        assert(event.effect.value > 0);
        switch (event.effect.type)
        {
        case KysChess::EffectType::CurrentHPPctBlast:
            commands.push_back(BattleCurrentHpBlastCommand{
                unitId,
                event.effect.value,
                "當前生命傷害",
            });
            break;
        case KysChess::EffectType::TeamMPRestore:
            commands.push_back(BattleTeamMpRestoreCommand{
                unitId,
                event.effect.value,
                "琴棋書畫",
            });
            break;
        case KysChess::EffectType::FlatShield:
            commands.push_back(BattleTeamShieldCommand{
                unitId,
                event.effect.value,
                true,
                "全隊護盾重整",
            });
            break;
        case KysChess::EffectType::SpiralBleedProjectile:
            commands.push_back(BattleSpiralBleedProjectileCommand{
                unitId,
                event.effect.value,
                event.effect.value2 > 0 ? event.effect.value2 : 6,
                projectileSpeed,
            });
            break;
        default:
            assert(false);
        }
    }
    return commands;
}

bool frameComboHasExecute(const KysChess::RoleComboState& state, int attackerUnitId)
{
    return BattleComboTriggerSystem().hasExecuteCombo(state, attackerUnitId);
}

double resolveFrameArmorPenetratedDefense(
    const KysChess::RoleComboState& state,
    int attackerUnitId,
    int targetUnitId,
    double defense,
    BattleRuntimeRandom& random)
{
    return BattleComboTriggerSystem().resolveArmorPenetratedDefense(
        state,
        { attackerUnitId, targetUnitId, defense },
        random).defense;
}

BattleUnitFrameTickResult BattleUnitFrameTickSystem::advance(
    const BattleUnitFrameTickInput& input) const
{
    assert(input.frame >= 0);
    assert(input.mpRegenIntervalFrames > 0);
    assert(input.physicalPowerRegenIntervalFrames > 0);
    assert(input.state.cooldown >= 0);
    assert(input.state.physicalPower >= 0);

    BattleUnitFrameTickResult result;
    result.state = input.state;

    const int previousCooldown = result.state.cooldown;
    if (!input.frozen && result.state.cooldown > 0)
    {
        --result.state.cooldown;
    }
    result.skillFinished = previousCooldown > 0 && result.state.cooldown == 0;

    if (result.state.cooldown == 0)
    {
        if (input.frame % input.physicalPowerRegenIntervalFrames == 0)
        {
            ++result.state.physicalPower;
        }
        result.state.actFrame = 0;
        result.resetDashVelocity = result.state.operationType == BattleOperationType::Dash;
        result.state.operationType = BattleOperationType::None;
        result.state.actType = -1;
        result.state.haveAction = false;
    }

    if (input.frame % input.mpRegenIntervalFrames == 0)
    {
        result.mpDelta = 1;
    }

    return result;
}

BattlePresentationFrame BattleFrameRunner::runFrame(BattleRuntimeState& state) const
{
    assert(!state.unitStore.units.empty());

    auto frame = makeBattleFrameContext(state);

    // Tick status timers and queue status damage, e.g. poison or bleed damage transactions.
    advanceStatus(state);
    // Tick unit cooldown/action/MP timers and collect frame combo events, e.g. skill-finished triggers.
    advanceRuntimeUnits(state, frame.frameCommands, frame.runtimeCommits);
    // Apply combo timer events to runtime state, deferring auto-ultimate commands until late frame.
    applyRuntimeComboEvents(state, frame);
    // Apply queued team effects whose source exists, e.g. delayed skill team heal.
    applyPendingTeamEffects(state, frame);
    // Reduce early gameplay commands into concrete queues/state; currently mostly a pre-movement drain point.
    reduceFrameGameplayCommands(state, frame);
    // Advance and commit motion, e.g. physics and tactical movement.
    advanceMotionFrame(state, frame);
    // Start or commit unit actions, e.g. cast startup, attack spawn requests, blink teleports, action sounds.
    advanceActionFrameUnits(state, frame);
    // Reduce cast-release effects, e.g. 出手回內、全隊盾、當前生命傷害, before attacks/damage apply.
    reduceFrameGameplayCommands(state, frame);
    // Spawn/tick attacks and resolve hits; hit commands are reduced immediately into damage/effect queues.
    advanceAttacksAndResolveHits(state, frame);
    // Apply queued damage and lifecycle effects, e.g. HP loss, death, rescue, death AOE, battle end.
    applyDamageAndLifecycle(state, frame);
    // Chain terminal logs are emitted after damage so the projectile visibly lands before the chain result.
    appendProjectileCancellationLogEvents(state.attacks, frame.attackEvents, frame.logEvents, true);
    frame.frameCommands.insert(
        frame.frameCommands.end(),
        std::make_move_iterator(frame.deferredCommands.begin()),
        std::make_move_iterator(frame.deferredCommands.end()));
    // Reduce late commands from damage/combo lifecycle, e.g. auto-ultimate or death-triggered projectiles.
    reduceFrameGameplayCommands(state, frame);
    assert(frame.frameCommands.empty());
    // Convert accumulated gameplay/log/visual events into the presentation frame consumed by the scene.
    emitPresentationFrame(state, frame);
    // Runtime maintenance: remove projectiles/melee attacks whose animation lifetime has finished.
    pruneFinishedRuntimeAttacks(state);
    return consumeBattleFrameContext(std::move(frame));
}

}  // namespace KysChess::Battle
