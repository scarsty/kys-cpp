#include "BattleCore.h"

#include "../ChessEftIds.h"
#include "../Find.h"
#include "BattleCombatIntent.h"
#include "BattleMath.h"
#include "BattleResourceRules.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
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

BattleUnitState makeBattleWorldUnitState(const BattleRuntimeUnit& runtimeUnit, double moveSpeedDivisor)
{
    assert(moveSpeedDivisor != 0.0);

    BattleUnitState unit;
    unit.id = runtimeUnit.id;
    unit.realRoleId = runtimeUnit.realRoleId;
    unit.name = runtimeUnit.name;
    unit.team = runtimeUnit.team;
    unit.alive = runtimeUnit.alive;
    unit.position = runtimeUnit.motion.position;
    unit.velocity = runtimeUnit.motion.velocity;
    unit.speed = runtimeUnit.stats.speed / moveSpeedDivisor;
    unit.star = runtimeUnit.star;
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

std::string formatStatusFrames(const std::string& label, int frames)
{
    if (frames <= 0)
    {
        return label;
    }
    return std::format("{}（{}幀）", label, frames);
}

std::string formatEffectCommandLogText(const BattleEffectCommand& command)
{
    switch (command.type)
    {
    case BattleEffectCommandType::AddShield:
        return formatStatusValue(command.label, command.value, "護盾");
    case BattleEffectCommandType::AddInvincibility:
        return formatStatusFrames(command.label, command.value);
    case BattleEffectCommandType::ModifyResource:
        return command.value > 0
            ? std::format("{}+{}MP", command.label, command.value)
            : command.label;
    case BattleEffectCommandType::ModifyCooldown:
        return command.value != 0
            ? std::format("{}（{}冷卻）", command.label, command.value)
            : command.label;
    case BattleEffectCommandType::Heal:
    case BattleEffectCommandType::DedicatedEffect:
        return command.label;
    }
    assert(false);
    return command.label;
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
        logEvents.push_back({
            BattleLogEventType::Status,
            BattlePresentationCurrentFrame,
            -1,
            enemy->id,
            0,
            std::format("陰險：前{}名攻防{}{}（{}名存活）",
                topTargets,
                delta > 0 ? "-" : "+",
                std::abs(delta),
                liveAllies),
        });
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

int pendingDefenderHpDamage(const BattleRuntimeState& state, int defenderUnitId)
{
    if (state.damage.pendingDamage.empty())
    {
        return 0;
    }

    BattleUnitStore previewUnits = state.unitStore;
    auto previewComboUnits = state.combo.units;
    auto previewStatusUnits = state.status.units;
    auto previewDamageUnitExtras = state.damage.unitExtras;

    BattleDamageApplicationInput previewInput;
    previewInput.frame = state.movement.frame;
    previewInput.sortPendingDamageByDefenderMagnitude = state.damage.sortPendingDamageByDefenderMagnitude;
    previewInput.units = &previewUnits;
    previewInput.comboUnits = &previewComboUnits;
    previewInput.statusUnits = &previewStatusUnits;
    previewInput.damageUnitExtras = &previewDamageUnitExtras;
    previewInput.pendingDamage = &state.damage.pendingDamage;
    return BattleDamageApplicationSystem().previewDefenderPendingHpDamage(previewInput, defenderUnitId);
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
    event.text = "閃避了來襲攻擊";
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
        break;
    case BattleAttackEventType::Expired:
        presentation.type = BattleVisualEventType::ProjectileExpired;
        break;
    case BattleAttackEventType::TargetLost:
        presentation.type = BattleVisualEventType::ProjectileTargetLost;
        presentation.targetUnitId = event.unitId;
        presentation.amount = -1;
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

std::string formatProjectileCancelLogText(int leftAttackId, int leftDamage, int rightAttackId, int rightDamage)
{
    const int remaining = leftDamage - rightDamage;
    if (remaining == 0)
    {
        return std::format(
            "抵消彈道 #{} vs #{}（{} - {} = 0，雙方互消）",
            leftAttackId,
            rightAttackId,
            leftDamage,
            rightDamage);
    }
    return std::format(
        "抵消彈道 #{} vs #{}（{} - {} = {}）",
        leftAttackId,
        rightAttackId,
        leftDamage,
        rightDamage,
        remaining);
}

void appendProjectileCancellationLogEvents(
    const BattleAttackState& world,
    const std::vector<BattleAttackEvent>& events,
    std::vector<BattleLogEvent>& logEvents)
{
    for (const auto& event : events)
    {
        switch (event.type)
        {
        case BattleAttackEventType::TargetLost:
        {
            BattleLogEvent log;
            log.type = BattleLogEventType::Status;
            if (const auto* attack = tryFindById(world.attacks, event.attackId))
            {
                log.sourceUnitId = attack->state.attackerUnitId;
            }
            log.targetUnitId = -1;
            log.text = "彈道取消：目標遺失";
            logEvents.push_back(std::move(log));
            break;
        }
        case BattleAttackEventType::ProjectileCancel:
        {
            const bool otherWins = event.otherProjectileCancelDamage > event.projectileCancelDamage;
            const int leftAttackId = otherWins ? event.otherAttackId : event.attackId;
            const int rightAttackId = otherWins ? event.attackId : event.otherAttackId;
            const int leftDamage = otherWins ? event.otherProjectileCancelDamage : event.projectileCancelDamage;
            const int rightDamage = otherWins ? event.projectileCancelDamage : event.otherProjectileCancelDamage;
            BattleLogEvent log;
            log.type = BattleLogEventType::Status;
            log.sourceUnitId = otherWins ? event.otherSourceUnitId : event.sourceUnitId;
            log.targetUnitId = otherWins ? event.sourceUnitId : event.otherSourceUnitId;
            log.amount = leftDamage;
            log.text = formatProjectileCancelLogText(leftAttackId, leftDamage, rightAttackId, rightDamage);
            logEvents.push_back(std::move(log));
            break;
        }
        case BattleAttackEventType::BlockedByInvincible:
        {
            BattleLogEvent log;
            log.type = BattleLogEventType::Status;
            log.sourceUnitId = event.sourceUnitId;
            log.targetUnitId = event.unitId;
            log.text = "彈道命中無敵：傷害忽略";
            logEvents.push_back(std::move(log));
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
}

void advanceRuntimeUnits(
    BattleRuntimeState& state,
    std::vector<BattleGameplayCommand>& commands,
    std::vector<BattleRuntimeUnitFrameCommit>& runtimeCommits,
    std::vector<BattleFrameUnitApplication>& unitApplications)
{
    BattleComboTriggerSystem comboSystem;
    runtimeCommits.clear();
    unitApplications.clear();
    for (auto& unit : state.unitStore.units)
    {
        assert(unit.id >= 0);
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
        unitApplications.push_back({
            unit.id,
            unit.animation.cooldown,
            unit.animation.actFrame,
            unit.animation.actType,
            unit.vitals.mp,
            committed.tick.resetDashVelocity,
        });

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
    std::vector<BattleProjectileCancelDamageCommand>& projectileCancelDamageCommands,
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
        projectileCancelDamageCommands.push_back(command);
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
    input.pendingDefenderHpDamage = pendingDefenderHpDamage(state, event.unitId);

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
    std::vector<BattleHitResolutionResult>& hitResults,
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

    BattleHitResolutionResult result;
    result.attackerUnitId = event.sourceUnitId;
    result.defenderUnitId = event.unitId;
    if (auto attackerComboIt = state.combo.units.find(event.sourceUnitId); attackerComboIt != state.combo.units.end())
    {
        result.attackerCombo = attackerComboIt->second;
    }
    result.defenderCombo = defenderComboIt->second;
    result.dodged = true;
    result.logEvents.push_back(dodgeStatusEvent(event.unitId, event.sourceUnitId));
    logEvents.insert(
        logEvents.end(),
        result.logEvents.begin(),
        result.logEvents.end());
    visualEvents.push_back(roleEffectEvent(event.unitId, KysChess::EFT_EVADE, CoreRoleStatusEffectFrames));
    hitResults.push_back(std::move(result));
    return true;
}

void resolveHitEvents(
    BattleRuntimeState& state,
    const std::vector<BattleAttackEvent>& events,
    std::vector<BattleHitResolutionResult>& hitResults,
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

        if (tryResolveDodgeHit(state, event, hitResults, logEvents, visualEvents))
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
        hitResults.push_back(std::move(result));
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
    movementUnit.dashAttack = dashAttackEnabled;
}

using PostPhysicsMotionMap = std::map<int, BattleMovementPhysicsState>;

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
        if (!unit.alive)
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

BattleMovementFrameInput makeMovementFrameInput(
    const BattleRuntimeState& state,
    const PostPhysicsMotionMap& postPhysics)
{
    BattleMovementFrameInput input;
    input.frame = state.movement.frame;
    input.seed = state.movement.seed;
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

        BattleUnitState movementUnit = makeBattleWorldUnitState(runtimeUnit, BattleRuntimeMoveSpeedDivisor);
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

void applyMovementFrameState(BattleRuntimeState& state, const BattleMovementFrameInput& input)
{
    state.movement.frame = input.frame;
    state.movement.seed = input.seed;
    state.movement.movementReservations = input.movementReservations;
    for (const auto& movementUnit : input.units)
    {
        auto& agent = requireMappedById(state.movement.agents, movementUnit.id);
        agent.targetId = movementUnit.targetId;
        agent.assignedSlot = movementUnit.assignedSlot;
        agent.slotSwitchCooldownRemaining = movementUnit.slotSwitchCooldownRemaining;
    }
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
    if (pointNorm(direction) <= state.action.castConfig.minimumFacingNorm)
    {
        direction = unit.motion.facing;
    }
    assert(pointNorm(direction) > state.action.castConfig.minimumFacingNorm);
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
            if (pointNorm(away) > state.action.castConfig.minimumFacingNorm)
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
    if (pointNorm(facing) <= input.config.minimumFacingNorm)
    {
        facing = unit.motion.facing;
    }
    assert(pointNorm(facing) > input.config.minimumFacingNorm);
    return normalizedTo(facing, 1.0, input.config.minimumFacingNorm);
}

const BattleCastSkillState& selectedCastSkill(const BattleCastInput& input, bool ultimate)
{
    return ultimate ? input.ultimateSkill : input.normalSkill;
}

Pointf taXueMeleeRetreatVelocity(BattleRuntimeState& state, Pointf retreatVelocity)
{
    const double retreatSpeed = pointNorm(retreatVelocity);
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
        const double speed = pointNorm(request.initial.velocity);
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
    BattleFrameApplications& applications,
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
        applications.attackSoundIds.push_back(castInput.ultimateSkill.soundId);
    }
    if (announceAutoUltimate)
    {
        BattleLogEvent log;
        log.type = BattleLogEventType::Status;
        log.sourceUnitId = unitId;
        log.targetUnitId = unitId;
        log.text = std::format("自動絕招·{}", castInput.ultimateSkill.name);
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
        applications.mpRestores.push_back({ unitId, -unit.vitals.maxMp });
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

void applyDamageResultToFrameState(BattleRuntimeState& state, const BattleDamageTransactionResult& transaction)
{
    state.unitStore.writeDamageUnit(transaction.attacker);
    state.unitStore.writeDamageUnit(transaction.defender);
    auto& unit = state.unitStore.requireUnit(transaction.defender.id);
    unit.animation.cooldown = transaction.defenderCooldown.cooldown;
    writeBattleDamageRuntimeUnit(
        requireById(state.damage.unitExtras, transaction.attacker.id),
        transaction.attacker);
    writeBattleDamageRuntimeUnit(
        requireById(state.damage.unitExtras, transaction.defender.id),
        transaction.defender);
    writeBattleStatusRuntimeUnit(
        requireById(state.status.units, transaction.defender.id),
        transaction.defenderStatus);
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

void applyRescueResultToFrameState(
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
    BattleFrameResult& result,
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
    applyRescueResultToFrameState(state, rescue, logEvents, visualEvents);
    result.rescueResults.push_back(std::move(rescue));
    return true;
}

void applyRescueRepositionForDamage(
    BattleRuntimeState& state,
    BattleFrameResult& result,
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
                       result,
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
                       result,
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
    std::optional<BattleDamagePresentationInput> presentation = std::nullopt)
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
    presentation.executed = command.executed;
    presentation.skillName = command.skillName;
    presentation.detailText = command.detailText;
    applyFrameDamagePresentationStyle(state, command.targetUnitId, presentation);
    return presentation;
}

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    const BattleHpDamageCommand& command)
{
    const auto& defender = state.unitStore.requireUnit(command.targetUnitId);

    BattleDamageRequest request;
    request.attackerUnitId = command.sourceUnitId;
    request.defenderUnitId = command.targetUnitId;
    request.baseDamage = command.executed
        ? std::max(command.damage, defender.vitals.hp)
        : command.damage;
    request.preResolvedDamage = true;
    request.frozenFrames = command.frozenFrames;
    request.triggersDefenseEffects = command.triggersDefenseEffects;

    appendFramePendingDamage(state, std::move(request), makeFrameDamagePresentation(state, command));
    return true;
}

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    const BattleMpDamageCommand& command)
{
    auto request = command.damage;
    request.attackerUnitId = command.sourceUnitId;
    request.defenderUnitId = command.targetUnitId;

    appendFramePendingDamage(state, std::move(request));
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
    event.text = std::move(text);
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
    event.text = std::move(text);
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
    BattleFrameApplications& applications,
    std::vector<BattleLogEvent>& logEvents)
{
    auto& unit = state.unitStore.requireUnit(command.unitId);

    const int restored = std::min(command.amount, std::max(0, unit.vitals.maxMp - unit.vitals.mp));
    if (restored <= 0)
    {
        return true;
    }

    unit.vitals.mp += restored;
    applications.mpRestores.push_back({ command.unitId, restored });
    appendStatusEventLog(logEvents, command.unitId, command.unitId, command.reason);
    return true;
}

bool applyFrameUnitHealCommand(
    BattleRuntimeState& state,
    const BattleUnitHealCommand& command,
    BattleFrameApplications& applications,
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

    applications.unitHeals.push_back({ command.sourceUnitId, command.targetUnitId, healed });
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
    BattleFrameApplications& applications,
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
        return applyFrameMpRestoreCommand(state, *mpRestore, applications, logEvents);
    }
    if (const auto* autoUltimate = std::get_if<BattleAutoUltimateCommand>(&command))
    {
        return tryCommitAutoUltimate(
            state,
            autoUltimate->unitId,
            autoUltimate->consumeMp,
            autoUltimate->announce,
            applications,
            effectCommands,
            pending,
            gameplayEvents,
            logEvents,
            visualEvents);
    }
    if (const auto* knockback = std::get_if<BattleKnockbackCommand>(&command))
    {
        applications.knockbacks.push_back({
            knockback->targetUnitId,
            knockback->velocityDelta,
            knockback->velocityCap,
        });
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
    if (const auto* lastAttacker = std::get_if<BattleLastAttackerCommand>(&command))
    {
        applications.lastAttackers.push_back({ lastAttacker->targetUnitId, lastAttacker->attackerUnitId });
        return true;
    }
    if (const auto* rumble = std::get_if<BattleRumbleCommand>(&command))
    {
        applications.rumbles.push_back({
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
        return applyFrameUnitHealCommand(state, *heal, applications, logEvents, visualEvents);
    }
    if (const auto* shield = std::get_if<BattleUnitShieldCommand>(&command))
    {
        return applyFrameUnitShieldCommand(state, *shield, logEvents);
    }
    assert(false);
    return false;
}

void reduceFrameGameplayCommands(
    BattleRuntimeState& state,
    std::vector<BattleGameplayCommand>& commands,
    BattleFrameApplications& applications,
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
            applications,
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

BattleDamageApplicationInput makeFrameDamageApplicationInput(BattleRuntimeState& state)
{
    BattleDamageApplicationInput input;
    input.frame = state.movement.frame;
    input.sortPendingDamageByDefenderMagnitude = state.damage.sortPendingDamageByDefenderMagnitude;
    input.units = &state.unitStore;
    input.comboUnits = &state.combo.units;
    input.statusUnits = &state.status.units;
    input.damageUnitExtras = &state.damage.unitExtras;
    input.pendingDamage = &state.damage.pendingDamage;
    input.unitEffects = &state.damage.unitEffects;
    input.deathEffects = &state.deathEffects.store;
    input.projectileFollowUps = &state.projectileFollowUps;
    input.random = &state.random;
    return input;
}

std::vector<int> unitDeathsIn(const std::vector<BattleDamageEvent>& events)
{
    std::vector<int> deadUnitIds;
    for (const auto& event : events)
    {
        if (event.type == BattleDamageEventType::UnitDied)
        {
            deadUnitIds.push_back(event.targetUnitId);
        }
    }
    return deadUnitIds;
}

void updateBattleResult(BattleRuntimeState& state, std::vector<BattleGameplayEvent>& gameplayEvents)
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
    if (!state.result.eventEmitted)
    {
        gameplayEvents.push_back({
            BattleGameplayEventType::BattleEnded,
            BattlePresentationCurrentFrame,
            -1,
            -1,
            state.result.winningTeam,
        });
        state.result.eventEmitted = true;
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
        presentation.detailText = event.reason;
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
        log.text = reason;
        switch (event.type)
        {
        case BattleTeamEffectEventType::Heal:
            log.type = BattleLogEventType::Heal;
            log.text = reason;
            break;
        case BattleTeamEffectEventType::MpRestore:
            log.type = BattleLogEventType::Status;
            log.text = std::format("{}+{}MP", reason, event.value);
            break;
        case BattleTeamEffectEventType::ShieldGain:
            log.type = BattleLogEventType::Status;
            log.text = formatStatusValue(reason, event.value, "護盾");
            break;
        case BattleTeamEffectEventType::CooldownReduced:
            log.type = BattleLogEventType::Status;
            log.text = formatStatusValue(reason, event.value, "冷卻");
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
        log.text = formatEffectCommandLogText(command);
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

void applyRuntimeComboEvents(
    BattleRuntimeState& state,
    const std::vector<BattleRuntimeUnitFrameCommit>& runtimeCommits,
    std::vector<BattleGameplayCommand>& deferredCommands,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
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
                applyRuntimeTeamEvents(state, result.unitId, event, teamEffectEvents, logEvents, visualEvents);
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
            deferredCommands.push_back(BattleAutoUltimateCommand{ result.unitId, false, true });
        }
    }
}

void applyPendingTeamEffects(
    BattleRuntimeState& state,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
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
            logEvents.insert(
                logEvents.end(),
                application.logEvents.begin(),
                application.logEvents.end());
            appendTeamEffectVisualEvents(visualEvents, application.events);
            teamEffectEvents.insert(
                teamEffectEvents.end(),
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

std::vector<BattleFrameMovementPhysicsUnitResult> advanceMovementPhysics(BattleRuntimeState& state)
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
        if (!unit.alive)
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
            if (auto* status = tryFindById(state.status.units, unit.id))
            {
                status->effects.frozenTimer = result.frozenFrames;
            }
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
        auto movementSnapshot = makeBattleWorldUnitState(unit, BattleRuntimeMoveSpeedDivisor);
        refreshMovementSkillProfile(movementSnapshot, unit, state);
        movementSnapshot.velocity = result.state.velocity;
        movementSnapshot.dashFramesRemaining = result.state.movementDashFrames;
        movementSnapshot.dashCooldownRemaining = result.state.movementDashCooldown;
        movementSnapshot.movementDashSpreadFramesRemaining = result.state.movementDashSpreadFrames;
        movementSnapshot.postDashRetreatFramesRemaining = result.state.postDashRetreatFrames;
        movementSnapshot.postDashChaosFramesRemaining = result.state.postDashChaosFrames;
        physicsInput.ignoreUnitCollision = battleMovementTaXueUnstable(movementSnapshot);

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

void advanceMovement(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    const PostPhysicsMotionMap& postPhysics)
{
    auto movementInput = makeMovementFrameInput(state, postPhysics);
    result.movement = BattleCore(movementInput).tickMovement();
    applyMovementFrameState(state, movementInput);

    if (state.movementPhysics.terrain.walkableByCell.empty())
    {
        return;
    }

    assert(state.unitStore.gridTransform.tileWidth > 0.0);
    assert(state.unitStore.gridTransform.coordCount > 0);

    for (const auto& movementUnit : result.movement.units)
    {
        auto& runtimeUnit = state.unitStore.requireUnit(movementUnit.id);

        auto decisionIt = result.movement.decisions.find(movementUnit.id);
        assert(decisionIt != result.movement.decisions.end());
        const auto action = decisionIt->second.action;

        Pointf syncedVelocity = movementUnit.velocity;
        if (action == MovementAction::Move)
        {
            syncedVelocity = { 0.0f, 0.0f, movementUnit.velocity.z };
        }

        const auto acceleration = runtimeUnit.motion.acceleration;
        state.unitStore.setMotion(
            movementUnit.id,
            movementUnit.position,
            syncedVelocity,
            acceleration);

        auto& physics = requireMappedById(state.movement.agents, movementUnit.id).physics;
        physics.position = movementUnit.position;
        physics.velocity = syncedVelocity;
        physics.acceleration = acceleration;
        physics.movementDashFrames = movementUnit.dashFramesRemaining;
        physics.movementDashCooldown = movementUnit.dashCooldownRemaining;
        physics.movementDashSpreadFrames = movementUnit.movementDashSpreadFramesRemaining;
        physics.postDashRetreatFrames = movementUnit.postDashRetreatFramesRemaining;
        physics.postDashChaosFrames = movementUnit.postDashChaosFramesRemaining;
    }
}

void publishMovementPresentationResults(const BattleRuntimeState& state, BattleFrameResult& result)
{
    result.movementPresentationResults.clear();
    result.movementPresentationResults.reserve(
        std::max(result.movement.units.size(), result.movementPhysicsResults.size()));

    std::unordered_map<int, std::size_t> indexByUnitId;
    indexByUnitId.reserve(result.movement.units.size() + result.movementPhysicsResults.size());

    for (const auto& unit : result.movement.units)
    {
        BattleFrameMovementPresentationUnitResult presentation;
        presentation.unitId = unit.id;
        presentation.position = unit.position;
        presentation.velocity = unit.velocity;
        const auto& runtimeUnit = state.unitStore.requireUnit(unit.id);
        auto runtimeFacing = runtimeUnit.motion.facing;
        if (runtimeFacing.norm() > 0.01f)
        {
            presentation.facing = runtimeFacing;
        }
        else if (presentation.velocity.norm() > 0.01f)
        {
            presentation.facing = presentation.velocity;
            presentation.facing.normTo(1.0f);
        }
        indexByUnitId.emplace(presentation.unitId, result.movementPresentationResults.size());
        result.movementPresentationResults.push_back(std::move(presentation));
    }

    for (const auto& physics : result.movementPhysicsResults)
    {
        auto indexIt = indexByUnitId.find(physics.unitId);
        if (indexIt == indexByUnitId.end())
        {
            indexIt = indexByUnitId.emplace(physics.unitId, result.movementPresentationResults.size()).first;
            result.movementPresentationResults.push_back({
                .unitId = physics.unitId,
                .position = physics.state.position,
                .velocity = physics.state.velocity,
            });
        }
        auto& presentation = result.movementPresentationResults[indexIt->second];
        auto presentationVelocity = presentation.velocity;
        auto physicsVelocity = physics.state.velocity;
        if (presentationVelocity.norm() <= 0.01 && physicsVelocity.norm() > 0.01)
        {
            presentation.velocity = physics.state.velocity;
        }
        presentation.acceleration = physics.state.acceleration;
        presentation.frozenFrames = physics.frozenFrames;
        if (presentation.facing.norm() <= 0.01f && presentation.velocity.norm() > 0.01f)
        {
            presentation.facing = presentation.velocity;
            presentation.facing.normTo(1.0f);
        }
    }
}

void writeActionStateToUnitStore(BattleRuntimeState& state, const BattleFrameActionUnitResult& result)
{
    auto& unit = state.unitStore.requireUnit(result.unitId);
    unit.animation.cooldown = result.state.cooldown;
    unit.animation.actFrame = result.state.actFrame;
    unit.animation.actType = result.state.actType;
    unit.operationType = result.state.operationType;
    unit.haveAction = result.state.haveAction;
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

void advanceActionFrameUnits(
    BattleRuntimeState& state,
    const BattleTickResult& movement,
    BattleFrameApplications& applications,
    std::vector<BattleFrameActionUnitResult>& actionResults,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleGameplayCommand>& frameCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
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
        result.state = makeActionRuntimeState(unit);
        const bool wasActionActive = result.state.haveAction;
        bool cancelledAction = false;

        if (!result.state.haveAction && castPlanInput)
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
                result.state.haveAction = true;
                result.state.actFrame = 0;
                result.state.actType = cast.decision.ultimate
                    ? castInput.ultimateSkill.magicType
                    : castInput.normalSkill.magicType;
                result.state.operationType = cast.decision.operationType;
                result.state.cooldown = cast.animation.cooldownFrames;
                state.action.pendingCasts[unit.id] = makePendingCastAction(castInput, cast);
                requireMappedById(state.movement.agents, unit.id).physics.movementDashSpreadFrames = 0;
            }
            result.castResult = std::move(cast);
        }
        else if (result.state.haveAction && pendingCastIt != state.action.pendingCasts.end())
        {
            const int castFrame = actionCastFrame(state, result.state.operationType);
            if (result.state.actFrame == castFrame)
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
                    clearActionFrameState(result.state);
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
                    applications.attackSoundIds.push_back(result.actionInput.cast.decision.soundId);
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
            }
        }

        if (wasActionActive && !cancelledAction)
        {
            ++result.state.actFrame;
            const int castFrame = actionCastFrame(state, result.state.operationType);
            if (result.state.cooldown > 0
                && result.state.actType >= 0
                && result.state.operationType != BattleOperationType::None
                && result.state.actFrame > castFrame + actionRecoveryFrames(state, result.state.operationType))
            {
                result.state.haveAction = false;
                result.state.operationType = BattleOperationType::None;
                result.state.actType = -1;
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
        writeActionStateToUnitStore(state, result);
        if (wasActionActive || result.castStarted || result.actionCommitted)
        {
            actionResults.push_back(std::move(result));
        }
    }
}

void advanceAttacksAndResolveHits(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    std::vector<BattleGameplayCommand>& frameCommands,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    state.attacks.frame = state.movement.frame;
    BattleAttackSystem attackSystem;
    for (const auto& request : state.pendingAttackSpawns)
    {
        result.attackEvents.push_back(attackSystem.spawn(state.attacks, request));
    }
    state.pendingAttackSpawns.clear();
    auto tickEvents = attackSystem.tick(state.attacks, state.unitStore);
    result.attackEvents.insert(
        result.attackEvents.end(),
        std::make_move_iterator(tickEvents.begin()),
        std::make_move_iterator(tickEvents.end()));
    applyProjectileCancelDamageResults(
        state,
        result.attackEvents,
        result.projectileCancelDamageCommands,
        frameCommands);
    appendProjectileCancellationLogEvents(state.attacks, result.attackEvents, logEvents);
    resolveHitEvents(
        state,
        result.attackEvents,
        result.hitResults,
        frameCommands,
        logEvents,
        visualEvents);
    reduceFrameGameplayCommands(
        state,
        frameCommands,
        result.applications,
        effectCommands,
        result.teamEffectEvents,
        gameplayEvents,
        logEvents,
        visualEvents);
}

BattleFrameDamageRenderUnit makeBattleFrameDamageRenderUnit(const BattleDamageUnitState& unit)
{
    return {
        unit.id,
        unit.vitals.hp,
        unit.vitals.mp,
        unit.invincible,
        unit.alive,
    };
}

BattleFrameDamageRenderApplication makeBattleFrameDamageRenderApplication(
    const BattleDamageTransactionResult& transaction,
    bool critical)
{
    return {
        makeBattleFrameDamageRenderUnit(transaction.defender),
        makeBattleFrameDamageRenderUnit(transaction.attacker),
        transaction.defenderStatus.effects.frozenTimer,
        transaction.defenderStatus.effects.frozenMaxTimer,
        transaction.defenderCooldown.cooldown,
        transaction.finalHpDamage,
        transaction.killed,
        critical,
    };
}

void applyDamageAndLifecycle(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    std::vector<BattleGameplayCommand>& frameCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    if (state.result.ended && state.damage.pendingDamage.empty())
    {
        return;
    }

    const bool battleEndAlreadyEmitted = state.result.eventEmitted;
    auto application = BattleDamageApplicationSystem().apply(makeFrameDamageApplicationInput(state));
    bool unitDied = false;

    logEvents.insert(
        logEvents.end(),
        application.logEvents.begin(),
        application.logEvents.end());
    visualEvents.insert(
        visualEvents.end(),
        application.visualEvents.begin(),
        application.visualEvents.end());
    frameCommands.insert(
        frameCommands.end(),
        application.commands.begin(),
        application.commands.end());

    std::set<int> criticalDefenderIds;
    for (const auto& hit : result.hitResults)
    {
        if (hit.critical)
        {
            criticalDefenderIds.insert(hit.defenderUnitId);
        }
    }

    for (auto transaction : application.transactions)
    {
        applyDamageResultToFrameState(state, transaction);
        applyRescueRepositionForDamage(state, result, transaction, logEvents, visualEvents);
        for (const auto& event : transaction.events)
        {
            if (event.type == BattleDamageEventType::UnitDied)
            {
                unitDied = true;
                continue;
            }
            gameplayEvents.push_back(toGameplayEvent(event));
        }
        result.damageRenderApplications.push_back(
            makeBattleFrameDamageRenderApplication(
                transaction,
                criticalDefenderIds.contains(transaction.defender.id)));
        result.damageTransactions.push_back(transaction);
    }
    for (const auto& event : application.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded && battleEndAlreadyEmitted)
        {
            continue;
        }
        gameplayEvents.push_back(event);
    }
    if (unitDied)
    {
        clearDeadRuntimePendingActions(state);
        appendEnemyTopDebuffUpdates(state, logEvents);
    }

    if (application.battleEnded)
    {
        state.result.ended = true;
        state.result.winningTeam = application.winningTeam;
        state.result.endedFrame = state.movement.frame;
        state.result.eventEmitted = true;
    }
    state.damage.pendingDamage.clear();
}

void emitPresentationFrame(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
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
    for (const auto& event : result.attackEvents)
    {
        recorder.recordGameplay(toGameplayEvent(event, state.attacks));
        for (auto presentation : toVisualEvents(event, state.attacks))
        {
            recorder.recordVisual(std::move(presentation));
        }
    }
    result.frame = recorder.consumeFrame();
}

BattleFrameComboRenderApplication makeBattleFrameComboRenderApplication(
    const BattleRuntimeUnit& unit,
    const BattleDamageRuntimeUnit* damage)
{
    BattleFrameComboRenderApplication application;
    application.unitId = unit.id;
    application.shield = unit.shield;
    application.blockFirstHitsRemaining = damage ? damage->blockFirstHitsRemaining : 0;
    return application;
}

BattleFrameRenderStatusUnit makeBattleFrameRenderStatusUnit(
    const BattleStatusRuntimeUnit& status,
    const BattleRuntimeUnit& unit)
{
    return {
        status.id,
        unit.invincible,
        status.effects.frozenTimer,
        status.effects.frozenMaxTimer,
    };
}

void publishFrameApplyOutputs(BattleRuntimeState& state, BattleFrameResult& result)
{
    result.stateApplications.comboRenderUnits.clear();
    result.stateApplications.comboRenderUnits.reserve(state.unitStore.units.size());
    for (const auto& unit : state.unitStore.units)
    {
        result.stateApplications.comboRenderUnits.push_back(
            makeBattleFrameComboRenderApplication(
                unit,
                tryFindById(state.damage.unitExtras, unit.id)));
    }
    result.stateApplications.statusRenderUnits.clear();
    result.stateApplications.statusRenderUnits.reserve(state.status.units.size());
    for (const auto& status : state.status.units)
    {
        result.stateApplications.statusRenderUnits.push_back(
            makeBattleFrameRenderStatusUnit(status, state.unitStore.requireUnit(status.id)));
    }
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

BattleCore::BattleCore(BattleMovementFrameInput& world)
    : world_(world)
{
}

BattleTickResult BattleCore::tickMovement()
{
    return BattleMovementPlanner(world_).tick();
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

BattleFrameResult BattleFrameRunner::runFrame(BattleRuntimeState& state) const
{
    assert(!state.unitStore.units.empty());

    BattleFrameResult result;
    std::vector<BattleGameplayCommand> frameCommands;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<BattleGameplayCommand> deferredCommands;
    std::vector<BattleRuntimeUnitFrameCommit> runtimeCommits;

    // Tick status timers and queue status damage, e.g. poison or bleed damage transactions.
    advanceStatus(state);
    // Tick unit cooldown/action/MP timers and collect frame combo events, e.g. skill-finished triggers.
    advanceRuntimeUnits(state, frameCommands, runtimeCommits, result.unitApplications);
    // Apply combo timer events to runtime state, deferring auto-ultimate commands until late frame.
    applyRuntimeComboEvents(
        state,
        runtimeCommits,
        deferredCommands,
        result.teamEffectEvents,
        result.effectCommands,
        logEvents,
        visualEvents);
    // Apply queued team effects whose source exists, e.g. delayed skill team heal.
    applyPendingTeamEffects(state, result.teamEffectEvents, logEvents, visualEvents);
    // Reduce early gameplay commands into concrete queues/state; currently mostly a pre-movement drain point.
    reduceFrameGameplayCommands(
        state,
        frameCommands,
        result.applications,
        result.effectCommands,
        result.teamEffectEvents,
        gameplayEvents,
        logEvents,
        visualEvents);
    // Prepare movement-owned unit state once; frame input builders must stay read-only.
    prepareMovementAgents(state);
    // Advance velocity/acceleration physics, e.g. dash momentum, gravity, friction, frozen gating.
    result.movementPhysicsResults = advanceMovementPhysics(state);
    // Run tactical movement planning on the world snapshot, e.g. approach enemies or hold ranged distance.
    advanceMovement(state, result, makePostPhysicsMotionMap(result.movementPhysicsResults));
    // Start or commit unit actions, e.g. cast startup, attack spawn requests, blink teleports, action sounds.
    advanceActionFrameUnits(
        state,
        result.movement,
        result.applications,
        result.actionResults,
        result.effectCommands,
        frameCommands,
        gameplayEvents,
        logEvents,
        visualEvents);
    // Publish one scene-facing movement payload after action updates so cast-facing is preserved.
    publishMovementPresentationResults(state, result);
    // Reduce cast-release effects, e.g. 出手回內、全隊盾、當前生命傷害, before attacks/damage apply.
    reduceFrameGameplayCommands(
        state,
        frameCommands,
        result.applications,
        result.effectCommands,
        result.teamEffectEvents,
        gameplayEvents,
        logEvents,
        visualEvents);
    // Spawn/tick attacks and resolve hits; hit commands are reduced immediately into damage/effect queues.
    advanceAttacksAndResolveHits(
        state,
        result,
        frameCommands,
        result.effectCommands,
        gameplayEvents,
        logEvents,
        visualEvents);
    // Apply queued damage and lifecycle effects, e.g. HP loss, death, rescue, death AOE, battle end.
    applyDamageAndLifecycle(state, result, frameCommands, gameplayEvents, logEvents, visualEvents);
    frameCommands.insert(
        frameCommands.end(),
        std::make_move_iterator(deferredCommands.begin()),
        std::make_move_iterator(deferredCommands.end()));
    // Reduce late commands from damage/combo lifecycle, e.g. auto-ultimate or death-triggered projectiles.
    reduceFrameGameplayCommands(
        state,
        frameCommands,
        result.applications,
        result.effectCommands,
        result.teamEffectEvents,
        gameplayEvents,
        logEvents,
        visualEvents);
    assert(frameCommands.empty());
    // Copy authoritative runtime state into explicit scene apply payloads, e.g. status/combo mirrors.
    publishFrameApplyOutputs(state, result);
    // Convert accumulated gameplay/log/visual events into the presentation frame consumed by the scene.
    emitPresentationFrame(state, result, gameplayEvents, logEvents, visualEvents);
    // Runtime maintenance: remove projectiles/melee attacks whose animation lifetime has finished.
    pruneFinishedRuntimeAttacks(state);
    return result;
}

}  // namespace KysChess::Battle
