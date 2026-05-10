#include "BattleCore.h"

#include "../ChessEftIds.h"
#include "BattleCombatIntent.h"
#include "BattleFind.h"
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
bool hasCanonicalUnitStore(const BattleRuntimeState& state);
bool isLastAliveInTeam(const BattleUnitStore& store, const BattleRuntimeUnit& unit);
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

const char* textForMovementEvent(BattleEventType type)
{
    switch (type)
    {
    case BattleEventType::Movement:
        return "movement";
    case BattleEventType::BlockedByAlly:
        return "blocked-by-ally";
    case BattleEventType::BlockedByWall:
        return "blocked-by-wall";
    case BattleEventType::SlotChanged:
        return "slot-changed";
    case BattleEventType::DashStart:
        return "dash-start";
    case BattleEventType::DashEnd:
        return "dash-end";
    case BattleEventType::AttackReady:
        return "attack-ready";
    }
    assert(false);
    return "movement";
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

void appendEnemyTopDebuffUpdates(BattleRuntimeState& state,
                                 std::vector<BattleLogEvent>& logEvents)
{
    int liveAllies = 0;
    int topTargets = 0;
    int perMemberValue = 0;
    for (const auto& ally : state.units.units)
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
    for (auto& unit : state.units.units)
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

BattleLogEvent toLogEvent(const BattleEvent& event)
{
    BattleLogEvent log;
    log.type = BattleLogEventType::Status;
    log.sourceUnitId = event.unitId;
    log.targetUnitId = event.targetId;
    log.text = textForMovementEvent(event.type);
    log.amount = static_cast<int>(event.value);
    return log;
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
    snapshot.hurtFrame = unit.hurtFrame;
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
    int pending = 0;
    for (const auto& transaction : state.damage.pendingTransactions)
    {
        if (transaction.defender.id == defenderUnitId)
        {
            pending += static_cast<int>(transaction.request.baseDamage);
        }
    }
    return pending;
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

    const auto& attacker = state.units.requireUnit(attack.state.attackerUnitId);
    const auto& defender = state.units.requireUnit(otherAttack.state.attackerUnitId);

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

void applyAttackContext(BattleVisualEvent& presentation, const BattleAttackWorld& world, int attackId)
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
    }
}

void applyAttackContext(BattleGameplayEvent& gameplay, const BattleAttackWorld& world, int attackId)
{
    gameplay.effectId = attackId;
    if (const auto* attack = tryFindById(world.attacks, attackId))
    {
        gameplay.sourceUnitId = attack->state.attackerUnitId;
        gameplay.position = attack->state.position;
    }
}

BattleVisualEvent toProjectileSpawnPresentationEvent(
    const BattleAttackWorld& world,
    int attackId)
{
    BattleVisualEvent presentation;
    presentation.type = BattleVisualEventType::ProjectileSpawned;
    applyAttackContext(presentation, world, attackId);
    return presentation;
}

std::vector<BattleVisualEvent> toVisualEvents(
    const BattleAttackEvent& event,
    const BattleAttackWorld& world)
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
    const BattleAttackWorld& world)
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

void syncAttackUnitsFromWorld(BattleRuntimeState& state)
{
    for (auto& attackUnit : state.attacks.units)
    {
        if (const auto* unit = tryFindById(state.world.units, attackUnit.id))
        {
            attackUnit.alive = unit->alive;
            attackUnit.team = unit->team;
            attackUnit.position = unit->position;
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
    for (auto& unit : state.units.units)
    {
        assert(unit.id >= 0);
        auto input = makeRuntimeUnitTickInput(state, unit);
        const bool lastAlive = isLastAliveInTeam(state.units, unit);

        BattleRuntimeUnitFrameCommit committed;
        committed.unitId = unit.id;
        committed.tick = BattleUnitFrameTickSystem().advance(input);

        unit.animation.cooldown = committed.tick.state.cooldown;
        unit.animation.actFrame = committed.tick.state.actFrame;
        unit.haveAction = committed.tick.state.haveAction;
        unit.operationType = committed.tick.state.operationType;
        unit.animation.actType = committed.tick.state.actType;
        unit.physicalPower = committed.tick.state.physicalPower;
        if (committed.tick.mpDelta > 0 && !unit.mpBlocked)
        {
            unit.vitals.mp += committed.tick.mpDelta * (100 + unit.mpRecoveryBonusPct) / 100;
        }
        else if (committed.tick.mpDelta < 0)
        {
            unit.vitals.mp += committed.tick.mpDelta;
        }
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
                    state.world.frame,
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
            auto skillFinishedEvents = comboSystem.collectSkillFinishedRuntimeEvents(
                comboIt->second,
                unit.alive);
            committed.comboEvents.insert(
                committed.comboEvents.end(),
                skillFinishedEvents.begin(),
                skillFinishedEvents.end());

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
    const auto& attacker = state.units.requireUnit(event.sourceUnitId);
    const auto& defender = state.units.requireUnit(event.unitId);

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
    std::vector<BattleLogEvent>& logEvents)
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

        if (tryResolveDodgeHit(state, event, hitResults, logEvents))
        {
            continue;
        }

        auto input = makeHitResolutionInput(state, event);
        auto result = BattleHitResolver().resolve(input, state.random);
        auto followUps = expandBattleProjectileFollowUpCommands(
            result.commands,
            state.projectileFollowUps,
            state.units);
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

void assertFrameAttackWorldConfigured(const BattleAttackWorld& attacks)
{
    assert(attacks.hitRadius > 0.0);
    assert(attacks.minimumVectorNorm > 0.0);
    assert(attacks.projectileGraceFrames >= 0);
    assert(attacks.bounceSpawnDistance > 0.0);
    assert(attacks.defaultProjectileSpeed > 0.0);
    assert(attacks.minimumBounceTotalFrame > 0);
}

void assertFrameMovementConfigConfigured(const BattleMovementConfig& config)
{
    assert(config.tileWidth > 0.0);
    assert(config.reservationHorizonFrames >= 0);
    assert(config.dashFrames > 0);
    assert(config.dashCooldownFrames >= 0);
    assert(config.slotSwitchCooldownFrames >= 0);
    assert(config.bodyRadius > 0.0);
    assert(config.engagementDeadband > 0.0);
    assert(config.engagementArriveDistance > 0.0);
    assert(config.meleeAttackReach > 0.0);
    assert(config.meleeLocalTargetRadius > 0.0);
    assert(config.maxDashDistance > 0.0);
    assert(config.maxRangedReach > 0.0);
    assert(config.movementDashDistanceMultiplier > 0.0);
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

void refreshMovementWorldFromRuntimeUnits(BattleRuntimeState& state)
{
    for (const auto& runtimeUnit : state.units.units)
    {
        auto* movementUnit = tryFindById(state.world.units, runtimeUnit.id);
        if (!movementUnit)
        {
            continue;
        }

        movementUnit->realRoleId = runtimeUnit.realRoleId;
        movementUnit->name = runtimeUnit.name;
        movementUnit->team = runtimeUnit.team;
        movementUnit->alive = runtimeUnit.alive;
        movementUnit->canAttack = runtimeUnit.animation.cooldown == 0;
        if (movementUnit->speed <= 0.0 && runtimeUnit.stats.speed > 0)
        {
            movementUnit->speed = runtimeUnit.stats.speed;
        }
        refreshMovementSkillProfile(*movementUnit, runtimeUnit, state);
    }

    state.world.units.erase(
        std::remove_if(
            state.world.units.begin(),
            state.world.units.end(),
            [&](const BattleUnitState& unit)
            {
                const auto* runtimeUnit = state.units.findUnit(unit.id);
                return !runtimeUnit || !runtimeUnit->alive;
            }),
        state.world.units.end());
}

bool hasCanonicalUnitStore(const BattleRuntimeState& state)
{
    return !state.units.units.empty();
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
    const int speedMultiplierPct = runtimeProjectileSpeedMultiplierPct(state, unit.id);
    skill.id = seed.id;
    skill.name = seed.name;
    skill.soundId = seed.soundId;
    skill.hurtType = seed.hurtType;
    skill.attackAreaType = seed.attackAreaType;
    skill.magicType = seed.magicType;
    skill.visualEffectId = seed.visualEffectId;
    skill.selectDistance = seed.selectDistance;
    skill.projectileSpeedMultiplierPct = speedMultiplierPct;
    skill.actProperty = seed.actProperty;
    skill.magicPower = seed.magicPower;
    skill.meleeSplashCount = ultimate && seed.attackAreaType == 0 ? 1 : 0;
    skill.extraProjectileCount = 0;
    skill.reach = std::min(
        runtimeEffectiveBattleReach(
            seed,
            forceRanged,
            runtimeForcedRangedMinSelectDistance(state, unit.id),
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
    input.frame = state.world.frame;
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

void syncRescueStateFromRuntimeUnits(BattleRuntimeState& state)
{
    state.rescue.units.clear();
    state.rescue.units.reserve(state.units.units.size());
    for (const auto& unit : state.units.units)
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
        state.rescue.units.push_back(std::move(snapshot));
    }

    std::map<std::pair<int, int>, int> occupantByCell;
    for (const auto& unit : state.units.units)
    {
        if (!unit.alive)
        {
            continue;
        }
        occupantByCell[rescueCellKey(unit.grid.x, unit.grid.y)] = unit.id;
    }

    for (auto& cell : state.rescue.cells)
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
    if (const auto* source = state.units.findUnit(input.unit.id))
    {
        input.unit.position = source->motion.position;
        input.unit.facing = source->motion.facing;
        input.unit.alive = source->alive;
        input.unit.canStartAttack = source->canAttack;
        input.unit.mp = source->vitals.mp;
        input.unit.maxMp = source->vitals.maxMp;
        input.unit.speed = source->stats.speed;
        input.unit.operationCount = source->operationCount;
    }
    else if (const auto* source = tryFindById(state.world.units, input.unit.id))
    {
        input.unit.position = source->position;
        input.unit.alive = source->alive;
        input.unit.canStartAttack = source->canAttack;
        if (auto decision = movement.decisions.find(source->id); decision != movement.decisions.end())
        {
            input.unit.canStartAttack = decision->second.action == MovementAction::AttackReady;
        }
    }
    if (const auto* status = tryFindById(state.status.units, input.unit.id))
    {
        input.unit.frozen = status->effects.frozenTimer > 0;
    }
    if (input.targetUnitId < 0 && hasCanonicalUnitStore(state))
    {
        input.targetUnitId = findNearestEnemyUnitId(state.units, input.unit.id);
    }
    if (const auto* target = state.units.findUnit(input.targetUnitId))
    {
        if (target->alive)
        {
            refreshCastTarget(input, target->id, target->motion.position);
        }
        else
        {
            input.targetUnitId = -1;
        }
    }
    else if (const auto* target = tryFindById(state.world.units, input.targetUnitId))
    {
        if (target->alive)
        {
            refreshCastTarget(input, target->id, target->position);
        }
        else
        {
            input.targetUnitId = -1;
        }
    }
    else
    {
        input.targetUnitId = -1;
    }
    return input;
}

void refreshRuntimeCastSkillBonuses(BattleRuntimeState& state, BattleCastInput& input)
{
    auto comboIt = state.combo.units.find(input.unit.id);
    if (comboIt == state.combo.units.end())
    {
        return;
    }
    if (input.ultimateSkill.id >= 0 && input.unit.mp == input.unit.maxMp)
    {
        input.ultimateSkill.extraProjectileCount = collectFrameExtraProjectileCount(
            comboIt->second,
            state.random,
            input.unit.id,
            std::max(0, comboIt->second.ultimateExtraProjectiles));
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
    input.unit.stunned = unit.hurtFrame > 0;

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
            state.action.actionRules.meleeAttackHitRadius / state.action.actionRules.dashMomentumFrames);
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

const BattleCastSkillState& selectedCastSkill(const BattleCastInput& input, bool ultimate)
{
    return ultimate ? input.ultimateSkill : input.normalSkill;
}

bool castInputWouldUseDash(const BattleCastInput& input, const BattleCastSkillState& selectedSkill)
{
    return input.unit.dashAttackEnabled
        && input.targetDistance <= input.unit.dashAttackReach
        && (input.targetDistance > input.unit.meleeAttackReach || selectedSkill.rangedStyle);
}

void refreshRuntimeDashHitCount(
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
    input.unit.dashHitCount = rollRuntimeDashHitCount(
        state,
        unit,
        selectedCastSkill(input, ultimate));
}

bool actionMovementDashActive(const BattleRuntimeState& state, int unitId)
{
    auto movementIt = state.movementRuntime.find(unitId);
    return movementIt != state.movementRuntime.end()
        && movementIt->second.movementDashFrames > 0;
}

const BattleCastSkillState& selectedCastSkill(const BattleCastInput& input, const BattleCastResult& cast)
{
    return cast.decision.ultimate ? input.ultimateSkill : input.normalSkill;
}

BattleActionCommitInput makePendingCastActionInput(BattleRuntimeState& state,
                                                   const BattleRuntimeUnit& unit,
                                                   const BattleCastInput& castInput,
                                                   const BattleCastResult& cast);

bool tryCommitAutoUltimate(
    BattleRuntimeState& state,
    int unitId,
    bool consumeMp,
    bool announceAutoUltimate,
    BattleFrameApplications& applications,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    auto* unit = state.units.findUnit(unitId);
    if (!unit || !unit->alive)
    {
        return false;
    }

    auto seedIt = state.action.planSeeds.find(unitId);
    if (seedIt == state.action.planSeeds.end()
        || seedIt->second.ultimateSkill.id < 0)
    {
        return false;
    }

    auto castInput = refreshedCastInput(
        state,
        {},
        makeRuntimeCastInputFromSeed(
            state,
            *unit,
            seedIt->second,
            true,
            actionMovementDashActive(state, unitId),
            true));
    if (castInput.targetUnitId < 0)
    {
        return false;
    }
    castInput.unit.canStartAttack = true;
    refreshRuntimeCastSkillBonuses(state, castInput);

    const auto operationType =
        BattleCombatIntentPlanner().operationTypeForAttackArea(castInput.ultimateSkill.attackAreaType);
    if (operationType == BattleOperationType::None)
    {
        return false;
    }

    refreshRuntimeDashHitCount(
        state,
        *unit,
        castInput,
        true,
        operationType == BattleOperationType::Dash);
    auto cast = BattleCastPlanner().commitSelectedCast(castInput, true, operationType);
    gameplayEvents.insert(gameplayEvents.end(), cast.gameplayEvents.begin(), cast.gameplayEvents.end());
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

    auto actionInput = makePendingCastActionInput(state, *unit, castInput, cast);
    auto actionResult = BattleActionCommitSystem().commit(actionInput);
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
    auto comboIt = state.combo.units.find(unitId);
    if (comboIt != state.combo.units.end())
    {
        comboIt->second = actionResult.combo;
    }
    unit->operationCount = actionResult.operationCount;
    if (consumeMp)
    {
        unit->vitals.mp -= unit->vitals.maxMp;
        applications.mpRestores.push_back({ unitId, -unit->vitals.maxMp });
    }
    return true;
}

Pointf positionForRuntimeGridCell(const BattleRuntimeState& state, int x, int y)
{
    const int coordCount = state.units.gridTransform.coordCount;
    const double tileWidth = state.units.gridTransform.tileWidth;
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
    const int coordCount = state.units.gridTransform.coordCount;
    if (x < 0 || y < 0 || x >= coordCount || y >= coordCount)
    {
        return false;
    }
    const auto index = static_cast<std::size_t>(x * coordCount + y);
    if (index >= state.world.terrainCells.size())
    {
        return true;
    }
    return state.world.terrainCells[index].walkable;
}

BattleBlinkGeometryInput makeRuntimeBlinkGeometry(const BattleRuntimeState& state,
                                                  const BattleRuntimeUnit& source,
                                                  double reach)
{
    BattleBlinkGeometryInput geometry;
    geometry.currentGridX = source.grid.x;
    geometry.currentGridY = source.grid.y;

    const double tileWidth = state.units.gridTransform.tileWidth;
    assert(tileWidth > 0.0);
    int gridReach = std::max(1, static_cast<int>(reach / tileWidth) + 1);
    std::set<std::pair<int, int>> visited;
    for (const auto& target : state.units.units)
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
                for (const auto& other : state.units.units)
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

BattleActionCommitInput makePendingCastActionInput(BattleRuntimeState& state,
                                                   const BattleRuntimeUnit& unit,
                                                   const BattleCastInput& castInput,
                                                   const BattleCastResult& cast)
{
    const auto& selectedSkill = selectedCastSkill(castInput, cast);
    BattleActionCommitInput actionInput;
    actionInput.unit.id = unit.id;
    actionInput.unit.team = unit.team;
    actionInput.unit.position = unit.motion.position;
    actionInput.unit.facing = unit.motion.facing;
    actionInput.unit.operationCount = unit.operationCount;
    actionInput.hasCast = true;
    actionInput.cast = cast;
    actionInput.blinkRandomRoll = state.random.nextInt(std::numeric_limits<int>::max());
    actionInput.blinkCellRandomRoll = state.random.nextInt(std::numeric_limits<int>::max());
    actionInput.blinkReach = selectedSkill.blinkReach > 0.0 ? selectedSkill.blinkReach : selectedSkill.reach;
    actionInput.blinkWeakTargetDefWeight = state.action.blinkWeakTargetDefWeight;
    actionInput.strengthenedMeleeOperationCountThreshold =
        state.action.strengthenedMeleeOperationCountThreshold;
    for (const auto& target : state.units.units)
    {
        actionInput.targets.push_back({
            target.id,
            target.team,
            target.alive,
            target.vitals.hp,
            target.vitals.maxHp,
            static_cast<double>(target.stats.defence),
            target.invincible,
            target.motion.position,
        });
    }

    auto comboIt = state.combo.units.find(unit.id);
    if (comboIt != state.combo.units.end())
    {
        actionInput.combo = comboIt->second;
        auto prime = collectFrameProjectileBouncePrime(
            actionInput.combo,
            unit.id,
            state.random.nextInt(100),
            state.action.projectileBounceRange);
        actionInput.projectileBouncePrime = {
            prime.count,
            prime.chancePct,
            prime.rollPct,
            prime.range,
        };
        if (actionInput.combo.blinkAttack)
        {
            actionInput.blinkGeometry = makeRuntimeBlinkGeometry(state, unit, actionInput.blinkReach);
        }
    }
    return actionInput;
}

std::string toStatusText(const BattleDamageEvent& event)
{
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

BattleDamageUnitState makeBattleDamageUnitState(
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

void writeBattleDamageRuntimeUnit(BattleDamageRuntimeUnit& runtime, const BattleDamageUnitState& unit)
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

BattleCooldownState makeBattleFrameCooldownState(const BattleRuntimeUnit& unit)
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

void applyDamageTakenMpGain(BattleDamageTransactionResult& transaction)
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

void applyDamageResultToFrameState(BattleRuntimeState& state, const BattleDamageTransactionResult& transaction)
{
    if (hasCanonicalUnitStore(state))
    {
        state.units.writeDamageUnit(transaction.attacker);
        state.units.writeDamageUnit(transaction.defender);
        auto& unit = state.units.requireUnit(transaction.defender.id);
        unit.animation.cooldown = transaction.defenderCooldown.cooldown;
    }
    if (auto* attacker = tryFindById(state.damage.unitExtras, transaction.attacker.id))
    {
        writeBattleDamageRuntimeUnit(*attacker, transaction.attacker);
    }
    if (auto* defender = tryFindById(state.damage.unitExtras, transaction.defender.id))
    {
        writeBattleDamageRuntimeUnit(*defender, transaction.defender);
    }
    if (auto* attacker = tryFindById(state.world.units, transaction.attacker.id))
    {
        attacker->alive = transaction.attacker.alive;
    }
    if (auto* defender = tryFindById(state.world.units, transaction.defender.id))
    {
        defender->alive = transaction.defender.alive;
    }
    if (auto* status = tryFindById(state.status.units, transaction.defender.id))
    {
        writeBattleStatusRuntimeUnit(*status, transaction.defenderStatus);
    }
}

void syncRescueDamageUnit(BattleRuntimeState& state, int unitId, int hp, int invincible)
{
    if (hasCanonicalUnitStore(state))
    {
        auto& unit = state.units.requireUnit(unitId);
        unit.vitals.hp = hp;
        unit.invincible = invincible;
    }
}

void syncRescuePosition(BattleRuntimeState& state, int unitId, Pointf position)
{
    if (hasCanonicalUnitStore(state))
    {
        state.units.setPosition(unitId, position);
    }
    if (auto* worldUnit = tryFindById(state.world.units, unitId))
    {
        worldUnit->position = position;
    }
    if (auto* attackUnit = tryFindById(state.attacks.units, unitId))
    {
        attackUnit->position = position;
    }
}

BattleRescueRepositionInput makeRescueInput(
    const BattleRuntimeState& state,
    BattleRescuePullMode mode,
    int pulledUnitId,
    int pullerTeam)
{
    BattleRescueRepositionInput input;
    input.mode = mode;
    input.pulledUnitId = pulledUnitId;
    input.pullerTeam = pullerTeam;
    input.cells = state.rescue.cells;
    input.units.reserve(state.rescue.units.size());
    for (const auto& unit : state.rescue.units)
    {
        input.units.push_back(unit.unit);
    }
    return input;
}

bool rescueUnitUnattendedByTeam(const BattleRuntimeState& state, int targetUnitId, int team)
{
    assert(state.rescue.executeUnattendedRadius > 0.0);
    const auto& target = requireBy(state.rescue.units, targetUnitId, rescueSnapshotUnitId);
    for (const auto& unit : state.rescue.units)
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

    const auto& attacker = requireBy(state.rescue.units, command.attackerUnitId, rescueSnapshotUnitId);
    const auto& target = requireBy(state.rescue.units, command.targetUnitId, rescueSnapshotUnitId);

    auto direction = target.position - attacker.position;
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
    request.initial.position = attacker.position;
    request.initial.position.x += static_cast<float>(config.meleeAttackEffectOffset) * direction.x;
    request.initial.position.y += static_cast<float>(config.meleeAttackEffectOffset) * direction.y;
    request.initial.position.z += static_cast<float>(config.meleeAttackEffectOffset) * direction.z;
    request.initial.velocity = target.position - request.initial.position;
    if (request.initial.velocity.norm() <= 0.01)
    {
        request.initial.velocity = direction;
    }
    request.initial.velocity.normTo(static_cast<float>(config.projectileSpeed));
    request.initial.totalFrame = std::max(
        config.minimumTotalFrames,
        static_cast<int>(std::ceil(distance2d(target.position, request.initial.position) / config.projectileSpeed))
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

    auto* pulled = &requireBy(state.rescue.units, result.teleport->unitId, rescueSnapshotUnitId);
    pulled->unit.cell = result.teleport->destinationCell;
    pulled->position = result.teleport->destinationPosition;
    syncRescuePosition(state, pulled->unit.id, pulled->position);

    if (result.counterDelta.unitId >= 0)
    {
        auto* puller = &requireBy(state.rescue.units, result.counterDelta.unitId, rescueSnapshotUnitId);
        puller->unit.forcePullProtectRemaining = std::max(
            0,
            puller->unit.forcePullProtectRemaining + result.counterDelta.protectRemainingDelta);
        puller->unit.forcePullExecuteRemaining = std::max(
            0,
            puller->unit.forcePullExecuteRemaining + result.counterDelta.executeRemainingDelta);
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
        assert(result.heal.targetUnitId == pulled->unit.id);
        pulled->unit.hp = std::min(pulled->unit.maxHp, pulled->unit.hp + result.heal.amount);
        visualEvents.push_back(roleEffectEvent(
            pulled->unit.id,
            KysChess::EFT_HEAL,
            CoreRoleStatusEffectFrames));
    }
    if (result.invincibility.frames > 0)
    {
        assert(result.invincibility.targetUnitId == pulled->unit.id);
        pulled->unit.invincible += result.invincibility.frames;
    }
    syncRescueDamageUnit(state, pulled->unit.id, pulled->unit.hp, pulled->unit.invincible);

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
    BattleRescuePullMode mode,
    int pulledUnitId,
    int pullerTeam,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    auto rescue = BattleRescueRepositionSystem().resolve(
        makeRescueInput(state, mode, pulledUnitId, pullerTeam));
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
    if (state.rescue.units.empty() || transaction.defender.vitals.maxHp <= 0 || !transaction.defender.alive)
    {
        return;
    }

    const auto* pulled = tryFindBy(state.rescue.units, transaction.defender.id, rescueSnapshotUnitId);
    const auto* attacker = tryFindBy(state.rescue.units, transaction.attacker.id, rescueSnapshotUnitId);
    if (!pulled)
    {
        return;
    }

    auto* mutablePulled = &requireBy(state.rescue.units, transaction.defender.id, rescueSnapshotUnitId);
    mutablePulled->unit.alive = transaction.defender.alive;
    mutablePulled->unit.hp = transaction.defender.vitals.hp;
    mutablePulled->unit.maxHp = transaction.defender.vitals.maxHp;
    mutablePulled->unit.invincible = transaction.defender.invincible;

    const int hpBefore = transaction.defender.vitals.hp - transaction.defenderDelta.hpDelta;
    if (attacker
        && attacker->unit.alive
        && attacker->unit.team != pulled->unit.team
        && hpBefore * 4 > transaction.defender.vitals.maxHp
        && transaction.defender.vitals.hp * 4 <= transaction.defender.vitals.maxHp)
    {
        tryApplyRescue(state,
                       result,
                       BattleRescuePullMode::Protect,
                       transaction.defender.id,
                       pulled->unit.team,
                       logEvents,
                       visualEvents);
    }

    const auto& currentPulled = requireBy(state.rescue.units, transaction.defender.id, rescueSnapshotUnitId);
    if (hpBefore * 100 > transaction.defender.vitals.maxHp * 15
        && currentPulled.unit.hp * 100 <= transaction.defender.vitals.maxHp * 15
        && state.rescue.executeUnattendedRadius > 0.0
        && rescueUnitUnattendedByTeam(state, transaction.defender.id, 1 - currentPulled.unit.team))
    {
        tryApplyRescue(state,
                       result,
                       BattleRescuePullMode::Execute,
                       transaction.defender.id,
                       1 - currentPulled.unit.team,
                       logEvents,
                       visualEvents);
    }
}

bool buildFrameDamageTransaction(
    const BattleRuntimeState& state,
    const BattleDamageRequest& request,
    BattleDamageTransactionInput& transaction)
{
    assert(request.defenderUnitId >= 0);

    const auto* defenderUnit = state.units.findUnit(request.defenderUnitId);
    const auto* defenderExtras = tryFindById(state.damage.unitExtras, request.defenderUnitId);
    if (!defenderUnit)
    {
        return false;
    }
    const auto defender = makeBattleDamageUnitState(*defenderUnit, defenderExtras);

    transaction.request = request;
    if (request.attackerUnitId >= 0)
    {
        const auto* attackerUnit = state.units.findUnit(request.attackerUnitId);
        if (!attackerUnit)
        {
            return false;
        }
        transaction.attacker = makeBattleDamageUnitState(
            *attackerUnit,
            tryFindById(state.damage.unitExtras, request.attackerUnitId));
    }
    else
    {
        transaction.attacker.id = request.attackerUnitId;
    }
    transaction.defender = defender;
    if (const auto* runtimeStatus = tryFindById(state.status.units, request.defenderUnitId))
    {
        transaction.defenderStatus = makeBattleStatusUnitState(
            *runtimeStatus,
            state.units.requireUnit(request.defenderUnitId));
    }
    else
    {
        transaction.defenderStatus = makeFallbackStatusUnit(defender);
    }
    transaction.defenderCooldown = makeBattleFrameCooldownState(state.units.requireUnit(request.defenderUnitId));
    return true;
}

void appendDamagePendingPresentationPlaceholder(BattleRuntimeState& state)
{
    if (!state.damage.pendingPresentation.empty())
    {
        state.damage.pendingPresentation.push_back({});
    }
}

void appendFrameDamageTransaction(
    BattleRuntimeState& state,
    BattleDamageTransactionInput transaction,
    std::optional<BattleDamagePresentationInput> presentation)
{
    state.damage.pendingTransactions.push_back(std::move(transaction));
    if (presentation)
    {
        while (state.damage.pendingPresentation.size() + 1 < state.damage.pendingTransactions.size())
        {
            state.damage.pendingPresentation.push_back({});
        }
        state.damage.pendingPresentation.push_back(std::move(*presentation));
        return;
    }
    appendDamagePendingPresentationPlaceholder(state);
}

std::optional<BattleDamagePresentationInput> makeFrameDamagePresentation(
    const BattleRuntimeState& state,
    const BattleHpDamageCommand& command)
{
    const auto styleIt = state.damage.presentationStylesByDefender.find(command.targetUnitId);
    if (styleIt == state.damage.presentationStylesByDefender.end())
    {
        return std::nullopt;
    }

    BattleDamagePresentationInput presentation;
    presentation.enabled = true;
    presentation.critical = command.critical;
    presentation.ultimate = command.ultimate;
    presentation.executed = command.executed;
    presentation.skillName = command.skillName;
    presentation.detailText = command.detailText;
    presentation.normalDamageColor = styleIt->second.normalDamageColor;
    presentation.emphasizedDamageColor = styleIt->second.emphasizedDamageColor;
    presentation.executeTextColor = styleIt->second.executeTextColor;
    presentation.normalDamageTextSize = styleIt->second.normalDamageTextSize;
    presentation.emphasizedDamageTextSize = styleIt->second.emphasizedDamageTextSize;
    presentation.executeTextSize = styleIt->second.executeTextSize;
    return presentation;
}

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    const BattleHpDamageCommand& command)
{
    const auto* defender = state.units.findUnit(command.targetUnitId);
    if (!defender)
    {
        return false;
    }

    BattleDamageRequest request;
    request.attackerUnitId = command.sourceUnitId;
    request.defenderUnitId = command.targetUnitId;
    request.baseDamage = command.executed
        ? std::max(command.damage, defender->vitals.hp)
        : command.damage;
    request.preResolvedDamage = true;
    request.frozenFrames = command.frozenFrames;

    BattleDamageTransactionInput transaction;
    if (!buildFrameDamageTransaction(state, request, transaction))
    {
        return false;
    }
    appendFrameDamageTransaction(
        state,
        std::move(transaction),
        makeFrameDamagePresentation(state, command));
    return true;
}

bool tryAppendFrameDamageTransaction(
    BattleRuntimeState& state,
    const BattleMpDamageCommand& command)
{
    auto request = command.damage;
    request.attackerUnitId = command.sourceUnitId;
    request.defenderUnitId = command.targetUnitId;

    BattleDamageTransactionInput transaction;
    if (!buildFrameDamageTransaction(state, request, transaction))
    {
        return false;
    }
    appendFrameDamageTransaction(state, std::move(transaction), std::nullopt);
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

    BattleDamageTransactionInput transaction;
    if (!buildFrameDamageTransaction(state, request, transaction))
    {
        return false;
    }
    appendFrameDamageTransaction(state, std::move(transaction), std::nullopt);
    return true;
}

void applyTeamEffectEventsToFrameState(
    BattleRuntimeState& state,
    const std::vector<BattleTeamEffectEvent>& events)
{
    for (const auto& event : events)
    {
        const auto& unit = state.units.requireUnit(event.targetUnitId);
        if (auto comboIt = state.combo.units.find(event.targetUnitId);
            comboIt != state.combo.units.end())
        {
            comboIt->second.shield = unit.shield;
        }
    }
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
    std::vector<BattleLogEvent>& logEvents)
{
    if (state.units.units.empty())
    {
        return false;
    }

    auto application = applyBattleTeamEffectCommand(state.units, command);
    logEvents.insert(
        logEvents.end(),
        application.logEvents.begin(),
        application.logEvents.end());
    applyTeamEffectEventsToFrameState(state, application.events);
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
    auto* unit = state.units.findUnit(command.unitId);
    if (!unit)
    {
        return false;
    }

    const int restored = std::min(command.amount, std::max(0, unit->vitals.maxMp - unit->vitals.mp));
    if (restored <= 0)
    {
        return true;
    }

    unit->vitals.mp += restored;
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
    auto* unit = state.units.findUnit(command.targetUnitId);
    if (!unit)
    {
        return false;
    }

    const int before = unit->vitals.hp;
    unit->vitals.hp = std::min(unit->vitals.maxHp, unit->vitals.hp + command.amount);
    const int healed = unit->vitals.hp - before;
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
    auto comboIt = state.combo.units.find(command.targetUnitId);
    auto* runtimeUnit = state.units.findUnit(command.targetUnitId);
    if (comboIt == state.combo.units.end() && !runtimeUnit)
    {
        return false;
    }

    if (comboIt != state.combo.units.end())
    {
        comboIt->second.shield += command.amount;
    }
    if (runtimeUnit)
    {
        runtimeUnit->shield += command.amount;
    }
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
    auto* unit = state.units.findUnit(command.unitId);
    auto comboIt = state.combo.units.find(command.unitId);
    if (!unit && comboIt == state.combo.units.end())
    {
        return false;
    }

    if (unit)
    {
        unit->stats.attack += command.attackBonus;
        unit->stats.defence += command.defenceBonus;
    }
    if (!command.permanent && comboIt != state.combo.units.end())
    {
        if (command.attackBonus != 0 && command.durationFrames > 0)
        {
            comboIt->second.tempAttackBuffs.push_back({ command.attackBonus, command.durationFrames });
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
        return applyFrameTeamEffectCommand(state, command, teamEffectEvents, logEvents);
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
            state.units);
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
            knockback->grantHurtFrame,
        });
        if (auto* unit = tryFindById(state.world.units, knockback->targetUnitId))
        {
            unit->velocity += knockback->velocityDelta;
            if (knockback->velocityCap > 0.0 && unit->velocity.norm() > knockback->velocityCap)
            {
                unit->velocity.normTo(static_cast<float>(knockback->velocityCap));
            }
        }
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

void appendDamageApplicationUnit(
    std::vector<BattleDamageApplicationUnitSnapshot>& units,
    std::map<int, std::size_t>& indexByUnitId,
    int unitId,
    int team,
    bool alive)
{
    if (unitId < 0)
    {
        return;
    }
    if (indexByUnitId.contains(unitId))
    {
        return;
    }
    indexByUnitId[unitId] = units.size();
    units.push_back({ unitId, team, alive });
}

BattleDamageApplicationInput makeFrameDamageApplicationInput(BattleRuntimeState& state)
{
    BattleDamageApplicationInput input;
    input.frame = state.world.frame;
    input.aggregatePendingTransactionsByDefender = state.damage.aggregatePendingTransactionsByDefender;
    input.pendingTransactions = &state.damage.pendingTransactions;
    input.pendingPresentation = &state.damage.pendingPresentation;
    input.unitEffects = &state.damage.unitEffects;
    input.deathEffects = &state.deathEffects.store;
    input.deathEffectUnits = &state.units;
    input.projectileFollowUps = &state.projectileFollowUps;
    input.projectileFollowUpUnits = &state.units;

    std::map<int, std::size_t> indexByUnitId;
    for (const auto& unit : state.world.units)
    {
        appendDamageApplicationUnit(input.units, indexByUnitId, unit.id, unit.team, unit.alive);
    }
    for (const auto& transaction : state.damage.pendingTransactions)
    {
        appendDamageApplicationUnit(
            input.units,
            indexByUnitId,
            transaction.attacker.id,
            0,
            transaction.attacker.alive);
        appendDamageApplicationUnit(
            input.units,
            indexByUnitId,
            transaction.defender.id,
            1,
            transaction.defender.alive);
    }
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
    for (const auto& unit : state.world.units)
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
    state.result.endedFrame = state.world.frame;
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
    state.status.config.frame = state.world.frame;
    auto statusTick = BattleStatusSystem(state.status.config).tick(state.units, state.status.units);
    for (const auto& event : statusTick.events)
    {
        if (event.type != BattleStatusEventType::PoisonDamage
            && event.type != BattleStatusEventType::BleedDamage)
        {
            continue;
        }

        auto* attackerUnit = state.units.findUnit(event.sourceUnitId);
        auto* defenderUnit = state.units.findUnit(event.unitId);
        auto* defenderStatus = tryFindById(state.status.units, event.unitId);
        if (!attackerUnit || !defenderUnit || !defenderStatus)
        {
            continue;
        }

        BattleDamageTransactionInput transaction;
        transaction.request.attackerUnitId = event.sourceUnitId;
        transaction.request.defenderUnitId = event.unitId;
        transaction.request.baseDamage = event.value;
        transaction.request.preResolvedDamage = true;
        transaction.attacker = makeBattleDamageUnitState(
            *attackerUnit,
            tryFindById(state.damage.unitExtras, event.sourceUnitId));
        transaction.defender = makeBattleDamageUnitState(
            *defenderUnit,
            tryFindById(state.damage.unitExtras, event.unitId));
        transaction.defenderStatus = makeBattleStatusUnitState(
            *defenderStatus,
            state.units.requireUnit(event.unitId));
        transaction.defenderCooldown = makeBattleFrameCooldownState(state.units.requireUnit(event.unitId));
        state.damage.pendingTransactions.push_back(std::move(transaction));
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
        log.text = command.label;
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
    std::vector<BattleLogEvent>& logEvents)
{
    if (state.units.units.empty())
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
    applyTeamEffectEventsToFrameState(state, events);
    teamEffectEvents.insert(
        teamEffectEvents.end(),
        events.begin(),
        events.end());
}

void applyBroadcastTriggerTimer(BattleRuntimeState& state, int sourceUnitId, const BattleComboFrameRuntimeEvent& event)
{
    assert(event.durationFrames > 0);
    const auto* source = state.units.findUnit(sourceUnitId);
    if (!source)
    {
        return;
    }
    for (const auto& unit : state.units.units)
    {
        if (!unit.alive || unit.id == sourceUnitId || unit.team != source->team)
        {
            continue;
        }
        auto comboIt = state.combo.units.find(unit.id);
        assert(comboIt != state.combo.units.end());
        comboIt->second.triggerTimers[event.trigger] = event.durationFrames;
    }
}

void applyPostSkillInvincibility(
    BattleRuntimeState& state,
    int sourceUnitId,
    const BattleComboFrameRuntimeEvent& event,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleLogEvent>& logEvents)
{
    assert(event.value > 0);
    if (state.units.units.empty())
    {
        return;
    }

    BattleEffectRegistry registry;
    BattleEffectDispatcher dispatcher(registry);
    dispatcher.addEffect({
        event.effectIndex,
        "技能後無敵",
        BattleHook::SkillFinished,
        BattleEffectTarget::Source,
        "技能後無敵",
        event.value,
    });
    auto commands = dispatcher.dispatch(
        state.units,
        state.effects.activationCounts,
        { BattleHook::SkillFinished, sourceUnitId, -1 });
    appendEffectCommandLogEvents(logEvents, commands);
    effectCommands.insert(
        effectCommands.end(),
        commands.begin(),
        commands.end());
}

void applyRuntimeComboEvents(
    BattleRuntimeState& state,
    const std::vector<BattleRuntimeUnitFrameCommit>& runtimeCommits,
    std::vector<BattleGameplayCommand>& deferredCommands,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleLogEvent>& logEvents)
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
                applyRuntimeTeamEvents(state, result.unitId, event, teamEffectEvents, logEvents);
                break;
            case BattleComboFrameRuntimeEventType::BroadcastTriggerTimer:
                applyBroadcastTriggerTimer(state, result.unitId, event);
                break;
            case BattleComboFrameRuntimeEventType::PostSkillInvincibility:
                applyPostSkillInvincibility(state, result.unitId, event, effectCommands, logEvents);
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
    std::vector<BattleLogEvent>& logEvents)
{
    if (state.units.units.empty())
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
            if (!state.units.findUnit(sourceUnitId))
            {
                unappliedCommands.push_back(command);
                continue;
            }
            auto application = applyBattleTeamEffectCommand(state.units, command);
            logEvents.insert(
                logEvents.end(),
                application.logEvents.begin(),
                application.logEvents.end());
            applyTeamEffectEventsToFrameState(state, application.events);
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

std::vector<BattleFrameMovementPhysicsUnitResult> advanceMovementPhysics(BattleRuntimeState& state)
{
    std::vector<BattleFrameMovementPhysicsUnitResult> physicsResults;
    if (state.units.units.empty())
    {
        return physicsResults;
    }
    if (state.movementPhysics.collision.walkableByCell.empty())
    {
        return physicsResults;
    }

    assert(state.movementPhysics.collision.tileWidth > 0.0);
    assert(state.movementPhysics.collision.coordCount > 0);
    assert(state.movementPhysics.collision.defaultSeparationDistance > 0.0);

    state.movementPhysics.collision.units.clear();
    state.movementPhysics.collision.units.reserve(state.units.units.size());
    for (const auto& unit : state.units.units)
    {
        state.movementPhysics.collision.units.push_back({
            unit.id,
            unit.alive,
            unit.motion.position,
        });
    }

    for (auto& unit : state.units.units)
    {
        assert(unit.id >= 0);
        auto movementIt = state.movementRuntime.find(unit.id);
        if (movementIt == state.movementRuntime.end())
        {
            movementIt = state.movementRuntime.emplace(
                unit.id,
                BattleMovementPhysicsState{
                    unit.motion.position,
                    unit.motion.velocity,
                    unit.motion.acceleration,
                }).first;
        }
        auto& movementRuntime = movementIt->second;

        BattleFrameMovementPhysicsUnitResult result;
        result.unitId = unit.id;
        result.state = movementRuntime;
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
        physicsInput.collisionWorld = &state.movementPhysics.collision;
        physicsInput.unitId = unit.id;
        physicsInput.currentPosition = unit.motion.position;
        physicsInput.actionDashActive = actionDashActive;
        result.state = BattleMovementPhysicsSystem().advance(physicsInput);
        result.physicsAdvanced = true;
        movementRuntime = result.state;

        if (auto* worldUnit = tryFindById(state.world.units, unit.id))
        {
            worldUnit->position = result.state.position;
            worldUnit->velocity = result.state.velocity;
        }
        state.units.setMotion(
            unit.id,
            result.state.position,
            result.state.velocity,
            result.state.acceleration);

        if (auto* collisionUnit = tryFindById(state.movementPhysics.collision.units, result.unitId))
        {
            collisionUnit->position = result.state.position;
        }
        physicsResults.push_back(std::move(result));
    }
    return physicsResults;
}

void advanceMovement(BattleRuntimeState& state, BattleFrameResult& result)
{
    result.movement = BattleCore(state.world).tickMovement();
}

void publishMovementPresentationResults(BattleFrameResult& result)
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
        if (presentation.velocity.norm() > 0.01)
        {
            presentation.facing = presentation.velocity;
            presentation.facing.normTo(1);
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
            result.movementPresentationResults.push_back({ .unitId = physics.unitId });
        }
        auto& presentation = result.movementPresentationResults[indexIt->second];
        presentation.position = physics.state.position;
        presentation.velocity = physics.state.velocity;
        presentation.acceleration = physics.state.acceleration;
        presentation.frozenFrames = physics.frozenFrames;
        if (presentation.velocity.norm() > 0.01)
        {
            presentation.facing = presentation.velocity;
            presentation.facing.normTo(1);
        }
    }
}

void writeActionStateToUnitStore(BattleRuntimeState& state, const BattleFrameActionUnitResult& result)
{
    if (auto* unit = state.units.findUnit(result.unitId))
    {
        unit->animation.cooldown = result.state.cooldown;
        unit->animation.actFrame = result.state.actFrame;
        unit->animation.actType = result.state.actType;
        unit->operationType = result.state.operationType;
        unit->haveAction = result.state.haveAction;
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
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    for (auto& unit : state.units.units)
    {
        assert(unit.id >= 0);
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
        auto pendingCommitIt = state.action.pendingCommitInputs.find(unit.id);
        BattleFrameActionUnitResult result;
        result.unitId = unit.id;
        result.state = makeActionRuntimeState(unit);
        const bool wasActionActive = result.state.haveAction;

        if (!result.state.haveAction && castPlanInput)
        {
            auto castInput = refreshedCastInput(state, movement, *castPlanInput);
            if (usingRuntimeCastPlan)
            {
                castInput.unit.canStartAttack = castInput.unit.canStartAttack
                    && unit.animation.cooldown == 0;
                refreshRuntimeCastSkillBonuses(state, castInput);
            }
            const bool plannedUltimate = castInput.ultimateSkill.id >= 0
                && castInput.unit.mp == castInput.unit.maxMp;
            refreshRuntimeDashHitCount(
                state,
                unit,
                castInput,
                plannedUltimate,
                castInputWouldUseDash(castInput, selectedCastSkill(castInput, plannedUltimate)));
            auto cast = BattleCastPlanner().plan(castInput);
            gameplayEvents.insert(gameplayEvents.end(), cast.gameplayEvents.begin(), cast.gameplayEvents.end());
            visualEvents.insert(visualEvents.end(), cast.visualEvents.begin(), cast.visualEvents.end());
            if (cast.decision.canCast)
            {
                result.castStarted = true;
                result.state.haveAction = true;
                result.state.actFrame = 0;
                result.state.actType = cast.decision.ultimate
                    ? castInput.ultimateSkill.magicType
                    : castInput.normalSkill.magicType;
                result.state.operationType = cast.decision.operationType;
                result.state.cooldown = cast.animation.cooldownFrames;
                state.action.pendingCommitInputs[unit.id] =
                    makePendingCastActionInput(state, unit, castInput, cast);
                state.movementRuntime[unit.id].movementDashSpreadFrames = 0;
            }
            result.castResult = std::move(cast);
        }
        else if (result.state.haveAction && pendingCommitIt != state.action.pendingCommitInputs.end())
        {
            const int castFrame = actionCastFrame(state, result.state.operationType);
            if (result.state.actFrame == castFrame)
            {
                result.actionCommitted = true;
                result.castCommitted = pendingCommitIt->second.hasCast;
                result.actionInput = pendingCommitIt->second;
                state.action.pendingCommitInputs.erase(pendingCommitIt);
                result.actionResult = BattleActionCommitSystem().commit(result.actionInput);
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

        if (wasActionActive)
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
                state.action.pendingCommitInputs.erase(unit.id);
            }
        }

        if (result.actionCommitted)
        {
            unit.operationCount = result.actionResult.operationCount;
            if (result.actionInput.hasCast)
            {
                auto comboIt = state.combo.units.find(unit.id);
                if (comboIt != state.combo.units.end())
                {
                    comboIt->second = result.actionResult.combo;
                }
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
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    syncAttackUnitsFromWorld(state);
    state.attacks.frame = state.world.frame;
    BattleAttackSystem attackSystem;
    for (const auto& request : state.pendingAttackSpawns)
    {
        result.attackEvents.push_back(attackSystem.spawn(state.attacks, request));
    }
    state.pendingAttackSpawns.clear();
    auto tickEvents = attackSystem.tick(state.attacks);
    result.attackEvents.insert(
        result.attackEvents.end(),
        std::make_move_iterator(tickEvents.begin()),
        std::make_move_iterator(tickEvents.end()));
    applyProjectileCancelDamageResults(
        state,
        result.attackEvents,
        result.projectileCancelDamageCommands,
        frameCommands);
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
    if (state.result.ended && state.damage.pendingTransactions.empty())
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
        applyDamageTakenMpGain(transaction);
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
        appendEnemyTopDebuffUpdates(state, logEvents);
    }

    if (application.battleEnded)
    {
        state.result.ended = true;
        state.result.winningTeam = application.winningTeam;
        state.result.endedFrame = state.world.frame;
        state.result.eventEmitted = true;
    }
    state.damage.pendingTransactions.clear();
    state.damage.pendingPresentation.clear();
}

void emitPresentationFrame(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    BattlePresentationRecorder recorder;
    recorder.beginFrame(state.world.frame);
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
    for (const auto& event : result.movement.events)
    {
        recorder.recordLog(toLogEvent(event));
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
    int unitId,
    const KysChess::RoleComboState& state)
{
    BattleFrameComboRenderApplication application;
    application.unitId = unitId;
    application.shield = state.shield;
    application.blockFirstHitsRemaining = state.blockFirstHitsRemaining;
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
    result.stateApplications.comboRenderUnits.reserve(state.units.units.size());
    for (const auto& unit : state.units.units)
    {
        const auto comboIt = state.combo.units.find(unit.id);
        if (comboIt == state.combo.units.end())
        {
            continue;
        }
        result.stateApplications.comboRenderUnits.push_back(
            makeBattleFrameComboRenderApplication(unit.id, comboIt->second));
    }
    result.stateApplications.statusRenderUnits.clear();
    result.stateApplications.statusRenderUnits.reserve(state.status.units.size());
    for (const auto& status : state.status.units)
    {
        if (const auto* unit = state.units.findUnit(status.id))
        {
            result.stateApplications.statusRenderUnits.push_back(makeBattleFrameRenderStatusUnit(status, *unit));
        }
    }
    result.deathEffectTrackers.reserve(state.deathEffects.store.units.size());
    for (const auto& extras : state.deathEffects.store.units)
    {
        result.deathEffectTrackers.push_back({
            extras.id,
            extras.shieldOnAllyDeathTracker,
        });
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

BattleCore::BattleCore(BattleWorldState& world)
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
    assertFrameMovementConfigConfigured(state.world.config);
    assertFrameAttackWorldConfigured(state.attacks);

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
        logEvents);
    // Apply queued team effects whose source exists, e.g. delayed skill team heal.
    applyPendingTeamEffects(state, result.teamEffectEvents, logEvents);
    // Reduce early gameplay commands into concrete queues/state; currently mostly a pre-movement drain point.
    reduceFrameGameplayCommands(
        state,
        frameCommands,
        result.applications,
        result.teamEffectEvents,
        gameplayEvents,
        logEvents,
        visualEvents);
    if (hasCanonicalUnitStore(state))
    {
        // Mirror canonical runtime units into the movement world before movement planning.
        refreshMovementWorldFromRuntimeUnits(state);
    }
    // Advance velocity/acceleration physics, e.g. dash momentum, gravity, friction, frozen gating.
    result.movementPhysicsResults = advanceMovementPhysics(state);
    // Run tactical movement planning on the world snapshot, e.g. approach enemies or hold ranged distance.
    advanceMovement(state, result);
    // Publish one scene-facing movement payload with physics output overriding planner motion when present.
    publishMovementPresentationResults(result);
    // Start or commit unit actions, e.g. cast startup, attack spawn requests, blink teleports, action sounds.
    advanceActionFrameUnits(
        state,
        result.movement,
        result.applications,
        result.actionResults,
        gameplayEvents,
        logEvents,
        visualEvents);
    // Spawn/tick attacks and resolve hits; hit commands are reduced immediately into damage/effect queues.
    advanceAttacksAndResolveHits(state, result, frameCommands, gameplayEvents, logEvents, visualEvents);
    // Refresh rescue/protect snapshots only when damage lifecycle can consume them.
    if (!state.damage.pendingTransactions.empty())
    {
        syncRescueStateFromRuntimeUnits(state);
    }
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
