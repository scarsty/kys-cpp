#include "BattleCore.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <iterator>
#include <map>
#include <set>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace KysChess::Battle
{

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

BattleRuntimeUnit* BattleUnitStore::findUnit(int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleRuntimeUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

const BattleRuntimeUnit* BattleUnitStore::findUnit(int unitId) const
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleRuntimeUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

BattleRuntimeUnit& BattleUnitStore::requireUnit(int unitId)
{
    auto* unit = findUnit(unitId);
    assert(unit);
    return *unit;
}

const BattleRuntimeUnit& BattleUnitStore::requireUnit(int unitId) const
{
    const auto* unit = findUnit(unitId);
    assert(unit);
    return *unit;
}

void BattleUnitStore::writeDamageUnit(const BattleDamageUnitState& source)
{
    auto& unit = requireUnit(source.id);
    unit.alive = source.alive;
    unit.hp = source.hp;
    unit.maxHp = source.maxHp;
    unit.mp = source.mp;
    unit.maxMp = source.maxMp;
    unit.attack = source.attack;
    unit.invincible = source.invincible;
    unit.shield = source.shield;
    unit.mpBlocked = source.mpBlocked;
    unit.mpRecoveryBonusPct = source.mpRecoveryBonusPct;
}

void BattleUnitStore::setPosition(int unitId, Pointf position)
{
    auto& unit = requireUnit(unitId);
    unit.position = position;
    unit.grid = gridTransform.toGrid(position);
}

void BattleUnitStore::setMotion(int unitId, Pointf position, Pointf velocity, Pointf acceleration)
{
    auto& unit = requireUnit(unitId);
    unit.position = position;
    unit.velocity = velocity;
    unit.acceleration = acceleration;
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

        const double distance = (candidate.position - source.position).norm();
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

        const double distance = (candidate.position - source.position).norm();
        if (targetUnitId < 0 || distance > bestDistance)
        {
            targetUnitId = candidate.id;
            bestDistance = distance;
        }
    }
    return targetUnitId;
}

namespace
{
bool hasCanonicalUnitStore(const BattleRuntimeState& state);

BattlePresentationUnitSnapshot toPresentationUnit(const BattleUnitState& unit)
{
    return {
        unit.id,
        unit.realRoleId,
        unit.name,
        unit.team,
        unit.alive,
        0,
        0,
        0,
        0,
        0,
        0,
        unit.position,
        unit.velocity,
    };
}

void applyUnitStoreSnapshot(BattlePresentationUnitSnapshot& snapshot, const BattleUnitStore& units)
{
    const auto* unit = units.findUnit(snapshot.id);
    if (!unit)
    {
        return;
    }

    snapshot.alive = unit->alive;
    snapshot.hp = unit->hp;
    snapshot.maxHp = unit->maxHp;
    snapshot.invincible = unit->invincible;
}

BattlePresentationSnapshot makePresentationSnapshot(const BattleRuntimeState& state)
{
    BattlePresentationSnapshot snapshot;
    snapshot.frame = state.world.frame;
    snapshot.units.reserve(state.world.units.size());
    for (const auto& unit : state.world.units)
    {
        auto presentationUnit = toPresentationUnit(unit);
        applyUnitStoreSnapshot(presentationUnit, state.units);
        snapshot.units.push_back(std::move(presentationUnit));
    }
    return snapshot;
}

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

const BattleAttackInstance* findAttack(const BattleAttackWorld& world, int attackId)
{
    if (auto it = std::find_if(world.attacks.begin(), world.attacks.end(), [&](const BattleAttackInstance& attack)
        {
            return attack.id == attackId;
        }); it != world.attacks.end())
    {
        return &*it;
    }
    return nullptr;
}

const BattleProjectileCancelBaseDamage* findProjectileCancelBaseDamage(
    const BattleRuntimeState& state,
    int attackId,
    int otherAttackId)
{
    auto it = std::find_if(
        state.projectileCancelBaseDamages.begin(),
        state.projectileCancelBaseDamages.end(),
        [&](const BattleProjectileCancelBaseDamage& input)
        {
            return input.attackId == attackId
                && (input.otherAttackId < 0 || input.otherAttackId == otherAttackId);
        });
    return it != state.projectileCancelBaseDamages.end() ? &*it : nullptr;
}

const BattleFrameHitScalarInput* findHitScalar(
    const BattleRuntimeState& state,
    int attackId,
    int attackerUnitId,
    int defenderUnitId)
{
    auto it = std::find_if(
        state.hits.scalars.begin(),
        state.hits.scalars.end(),
        [&](const BattleFrameHitScalarInput& input)
        {
            return input.attackId == attackId
                && input.attackerUnitId == attackerUnitId
                && input.defenderUnitId == defenderUnitId;
        });
    return it != state.hits.scalars.end() ? &*it : nullptr;
}

const BattleHitUnitSnapshot* findHitUnit(const BattleRuntimeState& state, int unitId)
{
    auto it = std::find_if(
        state.hits.units.begin(),
        state.hits.units.end(),
        [&](const BattleHitUnitSnapshot& unit)
        {
            return unit.id == unitId;
        });
    return it != state.hits.units.end() ? &*it : nullptr;
}

const BattleFrameHitSkillInput* findHitSkill(
    const BattleRuntimeState& state,
    int attackId,
    int attackerUnitId,
    int defenderUnitId)
{
    auto it = std::find_if(
        state.hits.skills.begin(),
        state.hits.skills.end(),
        [&](const BattleFrameHitSkillInput& input)
        {
            return input.attackId == attackId
                && input.attackerUnitId == attackerUnitId
                && input.defenderUnitId == defenderUnitId;
        });
    return it != state.hits.skills.end() ? &*it : nullptr;
}

const BattleFrameHitItemInput* findHitItem(
    const BattleRuntimeState& state,
    int attackId,
    int attackerUnitId,
    int defenderUnitId)
{
    auto it = std::find_if(
        state.hits.items.begin(),
        state.hits.items.end(),
        [&](const BattleFrameHitItemInput& input)
        {
            return input.attackId == attackId
                && input.attackerUnitId == attackerUnitId
                && input.defenderUnitId == defenderUnitId;
        });
    return it != state.hits.items.end() ? &*it : nullptr;
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

double takeRuntimePercentRoll(BattleRuntimeState& state)
{
    if (state.runtime.nextPercentRoll < state.runtime.percentRolls.size())
    {
        const double roll = state.runtime.percentRolls[state.runtime.nextPercentRoll++];
        assert(roll >= 0.0 && roll < 100.0);
        return roll;
    }
    return 0.0;
}

void applyAttackContext(BattleVisualEvent& presentation, const BattleAttackWorld& world, int attackId)
{
    presentation.effectId = attackId;
    if (const auto* attack = findAttack(world, attackId))
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
    if (const auto* attack = findAttack(world, attackId))
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
        if (const auto* sourceAttack = findAttack(world, event.attackId))
        {
            gameplay.sourceUnitId = sourceAttack->state.attackerUnitId;
        }
        if (const auto* spawnedAttack = findAttack(world, event.otherAttackId))
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
        auto it = std::find_if(state.world.units.begin(), state.world.units.end(), [&](const BattleUnitState& unit)
            {
                return unit.id == attackUnit.id;
            });
        if (it != state.world.units.end())
        {
            attackUnit.alive = it->alive;
            attackUnit.team = it->team;
            attackUnit.position = it->position;
        }
    }
}

void advanceRuntimeUnits(BattleRuntimeState& state, std::vector<BattleGameplayCommand>& commands)
{
    BattleComboTriggerSystem comboSystem;
    state.runtime.committedResults.clear();
    for (const auto& input : state.runtime.units)
    {
        assert(input.unitId >= 0);

        BattleFrameRuntimeUnitResult committed;
        committed.unitId = input.unitId;
        committed.result = BattleFrameUnitRuntimeSystem().advance(input.input);

        if (hasCanonicalUnitStore(state))
        {
            auto& unit = state.units.requireUnit(input.unitId);
            unit.cooldown = committed.result.state.cooldown;
            unit.haveAction = committed.result.state.haveAction;
            unit.operationType = committed.result.state.operationType;
            unit.actType = committed.result.state.actType;
            unit.physicalPower = committed.result.state.physicalPower;
            if (committed.result.mpDelta > 0 && !unit.mpBlocked)
            {
                unit.mp += committed.result.mpDelta * (100 + unit.mpRecoveryBonusPct) / 100;
            }
            else if (committed.result.mpDelta < 0)
            {
                unit.mp += committed.result.mpDelta;
            }
            if (committed.result.resetDashVelocity)
            {
                unit.velocity = { 0, 0, 0 };
            }
        }

        auto comboIt = state.combo.units.find(input.unitId);
        if (comboIt != state.combo.units.end())
        {
            auto frameEvents = comboSystem.advanceFrameRuntime(
                comboIt->second,
                {
                    state.world.frame,
                    input.hp,
                    input.maxHp,
                    input.alive,
                    input.lastAlive,
                });
            committed.comboEvents.insert(
                committed.comboEvents.end(),
                frameEvents.begin(),
                frameEvents.end());
        }
        if (committed.result.skillFinished && comboIt != state.combo.units.end())
        {
            auto skillFinishedEvents = comboSystem.collectSkillFinishedRuntimeEvents(
                comboIt->second,
                input.alive);
            committed.comboEvents.insert(
                committed.comboEvents.end(),
                skillFinishedEvents.begin(),
                skillFinishedEvents.end());

            auto teamHeal = comboSystem.collectPendingSkillTeamHeal(
                comboIt->second,
                { BattleComboTriggerHook::AfterSkillCast, input.unitId, -1 },
                [&]() { return takeRuntimePercentRoll(state); });
            if (teamHeal.flatHeal > 0 || teamHeal.pctHeal > 0)
            {
                BattleTeamHealCommand command{
                    input.unitId,
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
        state.runtime.committedResults.push_back(std::move(committed));
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
        const auto* attack = findAttack(state.attacks, event.attackId);
        const auto* otherAttack = findAttack(state.attacks, event.otherAttackId);
        assert(attack);
        assert(otherAttack);

        if (const auto* damage = findProjectileCancelBaseDamage(state, event.attackId, event.otherAttackId))
        {
            event.projectileCancelDamage = scaleProjectileCancelDamage(
                damage->baseDamage,
                attack->state.operationType);
        }
        if (const auto* damage = findProjectileCancelBaseDamage(state, event.otherAttackId, event.attackId))
        {
            event.otherProjectileCancelDamage = scaleProjectileCancelDamage(
                damage->baseDamage,
                otherAttack->state.operationType);
        }

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

std::vector<double> hitResolverRolls(
    const BattleFrameHitScalarInput& scalar,
    bool consumedDodgeRoll)
{
    if (!consumedDodgeRoll || scalar.percentRolls.empty())
    {
        return scalar.percentRolls;
    }
    return { std::next(scalar.percentRolls.begin()), scalar.percentRolls.end() };
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
    const BattleRuntimeState& state,
    const BattleAttackEvent& event,
    const BattleFrameHitScalarInput& scalar,
    bool consumedDodgeRoll)
{
    const auto* attacker = findHitUnit(state, event.sourceUnitId);
    const auto* defender = findHitUnit(state, event.unitId);
    assert(attacker);
    assert(defender);

    BattleHitResolutionInput input;
    input.attackEvent = event;
    input.attacker = *attacker;
    input.defender = *defender;
    if ((input.attackEvent.position - input.defender.position).norm() == 0.0)
    {
        assert(input.defender.facing.norm() > 0.0);
        input.attackEvent.position = input.defender.position + input.defender.facing;
    }
    input.sharedBleedMaxStacks = scalar.sharedBleedMaxStacks;
    input.randomDamageVariance = scalar.randomDamageVariance;
    input.pendingDefenderHpDamage = scalar.pendingDefenderHpDamage;
    input.percentRolls = hitResolverRolls(scalar, consumedDodgeRoll);

    if (const auto* skill = findHitSkill(state, event.attackId, event.sourceUnitId, event.unitId))
    {
        input.skill = skill->skill;
        input.skill.resolvedBaseDamage = scalar.resolvedMagicBaseDamage;
    }
    if (const auto* item = findHitItem(state, event.attackId, event.sourceUnitId, event.unitId))
    {
        input.item = item->item;
        input.item.resolvedDamage = scalar.resolvedHiddenWeaponDamage;
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
    const BattleFrameHitScalarInput& scalar,
    std::vector<BattleHitResolutionResult>& hitResults,
    std::vector<BattleLogEvent>& logEvents)
{
    auto defenderComboIt = state.combo.units.find(event.unitId);
    if (defenderComboIt == state.combo.units.end())
    {
        return false;
    }

    const double roll = scalar.percentRolls.empty() ? 0.0 : scalar.percentRolls.front();
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

        const auto* scalar = findHitScalar(state, event.attackId, event.sourceUnitId, event.unitId);
        if (!scalar)
        {
            continue;
        }

        const bool consumedDodgeRoll = state.combo.units.find(event.unitId) != state.combo.units.end();
        if (tryResolveDodgeHit(state, event, *scalar, hitResults, logEvents))
        {
            continue;
        }

        auto input = makeHitResolutionInput(state, event, *scalar, consumedDodgeRoll);
        auto result = BattleHitResolver().resolve(input);
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

BattleStatusRuntimeUnit* findStatusUnit(std::vector<BattleStatusRuntimeUnit>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleStatusRuntimeUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

BattleUnitState* findWorldUnit(BattleWorldState& world, int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleUnitState& unit)
        {
            return unit.id == unitId;
        });
    return it != world.units.end() ? &*it : nullptr;
}

const BattleUnitState* findWorldUnit(const BattleWorldState& world, int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleUnitState& unit)
        {
            return unit.id == unitId;
        });
    return it != world.units.end() ? &*it : nullptr;
}

const BattleStatusRuntimeUnit* findStatusUnit(const std::vector<BattleStatusRuntimeUnit>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleStatusRuntimeUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

BattleDamageRuntimeUnit* findDamageRuntimeUnit(std::vector<BattleDamageRuntimeUnit>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleDamageRuntimeUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

const BattleDamageRuntimeUnit* findDamageRuntimeUnit(const std::vector<BattleDamageRuntimeUnit>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleDamageRuntimeUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

bool hasCanonicalUnitStore(const BattleRuntimeState& state)
{
    return !state.units.units.empty();
}

BattleFrameRescueUnitSnapshot* findRescueUnit(std::vector<BattleFrameRescueUnitSnapshot>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleFrameRescueUnitSnapshot& unit)
        {
            return unit.unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

const BattleFrameRescueUnitSnapshot* findRescueUnit(const std::vector<BattleFrameRescueUnitSnapshot>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleFrameRescueUnitSnapshot& unit)
        {
            return unit.unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

double distance2d(Pointf lhs, Pointf rhs)
{
    return EuclidDis(lhs.x - rhs.x, lhs.y - rhs.y);
}

BattleCastInput refreshedCastInput(const BattleRuntimeState& state,
                                   const BattleTickResult& movement,
                                   BattleCastInput input)
{
    if (const auto* source = findWorldUnit(state.world, input.unit.id))
    {
        input.unit.position = source->position;
        input.unit.alive = source->alive;
        input.unit.canStartAttack = source->canAttack;
        if (auto decision = movement.decisions.find(source->id); decision != movement.decisions.end())
        {
            input.unit.canStartAttack = decision->second.action == MovementAction::AttackReady;
        }
    }
    if (const auto* status = findStatusUnit(state.status.units, input.unit.id))
    {
        input.unit.frozen = status->frozenTimer > 0;
    }
    if (const auto* target = findWorldUnit(state.world, input.targetUnitId))
    {
        if (target->alive)
        {
            input.targetPosition = target->position;
            input.targetDistance = distance2d(input.unit.position, target->position);
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
        break;
    }
    return gameplay;
}

BattleStatusUnitState makeFallbackStatusUnit(const BattleDamageUnitState& unit)
{
    BattleStatusUnitState status;
    status.id = unit.id;
    status.alive = unit.alive;
    status.hp = unit.hp;
    status.maxHp = unit.maxHp;
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
    damage.hp = unit.hp;
    damage.maxHp = unit.maxHp;
    damage.mp = unit.mp;
    damage.maxMp = unit.maxMp;
    damage.attack = unit.attack;
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
    cooldown.cooldown = unit.cooldown;
    cooldown.cooldownMax = unit.cooldownMax;
    cooldown.haveAction = unit.haveAction;
    cooldown.operationType = unit.operationType;
    cooldown.actType = unit.actType;
    return cooldown;
}

void applyDamageResultToFrameState(BattleRuntimeState& state, const BattleDamageTransactionResult& transaction)
{
    if (hasCanonicalUnitStore(state))
    {
        state.units.writeDamageUnit(transaction.attacker);
        state.units.writeDamageUnit(transaction.defender);
        state.units.requireUnit(transaction.defender.id).cooldown = transaction.defenderCooldown.cooldown;
    }
    if (auto* attacker = findDamageRuntimeUnit(state.damage.unitExtras, transaction.attacker.id))
    {
        writeBattleDamageRuntimeUnit(*attacker, transaction.attacker);
    }
    if (auto* defender = findDamageRuntimeUnit(state.damage.unitExtras, transaction.defender.id))
    {
        writeBattleDamageRuntimeUnit(*defender, transaction.defender);
    }
    if (auto* attacker = findWorldUnit(state.world, transaction.attacker.id))
    {
        attacker->alive = transaction.attacker.alive;
    }
    if (auto* defender = findWorldUnit(state.world, transaction.defender.id))
    {
        defender->alive = transaction.defender.alive;
    }
    if (auto* status = findStatusUnit(state.status.units, transaction.defender.id))
    {
        writeBattleStatusRuntimeUnit(*status, transaction.defenderStatus);
    }
}

void syncRescueDamageUnit(BattleRuntimeState& state, int unitId, int hp, int invincible)
{
    if (hasCanonicalUnitStore(state))
    {
        auto& unit = state.units.requireUnit(unitId);
        unit.hp = hp;
        unit.invincible = invincible;
    }
}

void syncRescuePosition(BattleRuntimeState& state, int unitId, Pointf position)
{
    if (hasCanonicalUnitStore(state))
    {
        state.units.setPosition(unitId, position);
    }
    if (auto* worldUnit = findWorldUnit(state.world, unitId))
    {
        worldUnit->position = position;
    }
    auto attackUnitIt = std::find_if(
        state.attacks.units.begin(),
        state.attacks.units.end(),
        [&](const BattleAttackUnit& unit)
        {
            return unit.id == unitId;
        });
    if (attackUnitIt != state.attacks.units.end())
    {
        attackUnitIt->position = position;
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

Pointf rescueCellPosition(const BattleRuntimeState& state, Point cell)
{
    auto it = state.rescue.positionsByCell.find({ cell.x, cell.y });
    if (it != state.rescue.positionsByCell.end())
    {
        return it->second;
    }
    return {
        static_cast<float>(cell.x * state.world.config.tileWidth),
        static_cast<float>(cell.y * state.world.config.tileWidth),
        0.0f,
    };
}

bool rescueUnitUnattendedByTeam(const BattleRuntimeState& state, int targetUnitId, int team)
{
    assert(state.rescue.executeUnattendedRadius > 0.0);
    const auto* target = findRescueUnit(state.rescue.units, targetUnitId);
    assert(target);
    for (const auto& unit : state.rescue.units)
    {
        if (!unit.unit.alive || unit.unit.team != team)
        {
            continue;
        }
        if (distance2d(unit.position, target->position) <= state.rescue.executeUnattendedRadius)
        {
            return false;
        }
    }
    return true;
}

Point movementPhysicsCell(const BattleMovementPhysicsCollisionWorld& world, Pointf position)
{
    assert(world.tileWidth > 0.0);
    assert(world.coordCount > 0);
    double x = position.x - world.coordCount * world.tileWidth;
    Point cell;
    cell.x = static_cast<int>(std::round((x / world.tileWidth + position.y / world.tileWidth) / 2.0));
    cell.y = static_cast<int>(std::round((-x / world.tileWidth + position.y / world.tileWidth) / 2.0));
    return cell;
}

bool movementPhysicsCellWalkable(const BattleMovementPhysicsCollisionWorld& world, Point cell)
{
    auto it = std::find_if(world.cells.begin(), world.cells.end(), [&](const BattleMovementPhysicsCollisionCellSnapshot& snapshot)
        {
            return snapshot.x == cell.x && snapshot.y == cell.y;
        });
    return it != world.cells.end() && it->walkable;
}

bool canMoveInPhysicsSnapshot(
    const BattleMovementPhysicsCollisionWorld& world,
    int unitId,
    Pointf currentPosition,
    Pointf nextPosition,
    int separationDistance)
{
    if (currentPosition.z > 1.0f)
    {
        return true;
    }

    const double separation = separationDistance == -1
        ? world.defaultSeparationDistance
        : static_cast<double>(separationDistance);
    for (const auto& unit : world.units)
    {
        if (!unit.alive || unit.id == unitId)
        {
            continue;
        }
        const double nextDistance = distance2d(nextPosition, unit.position);
        if (nextDistance < separation)
        {
            const double currentDistance = distance2d(currentPosition, unit.position);
            if (currentDistance >= nextDistance)
            {
                return false;
            }
        }
    }

    return movementPhysicsCellWalkable(world, movementPhysicsCell(world, nextPosition));
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

    const auto* attacker = findRescueUnit(state.rescue.units, command.attackerUnitId);
    const auto* target = findRescueUnit(state.rescue.units, command.targetUnitId);
    assert(attacker);
    assert(target);

    auto direction = target->position - attacker->position;
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
    request.initial.position = attacker->position;
    request.initial.position.x += static_cast<float>(config.meleeAttackEffectOffset) * direction.x;
    request.initial.position.y += static_cast<float>(config.meleeAttackEffectOffset) * direction.y;
    request.initial.position.z += static_cast<float>(config.meleeAttackEffectOffset) * direction.z;
    request.initial.velocity = target->position - request.initial.position;
    if (request.initial.velocity.norm() <= 0.01)
    {
        request.initial.velocity = direction;
    }
    request.initial.velocity.normTo(static_cast<float>(config.projectileSpeed));
    request.initial.totalFrame = std::max(
        config.minimumTotalFrames,
        static_cast<int>(std::ceil(distance2d(target->position, request.initial.position) / config.projectileSpeed))
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

    auto* pulled = findRescueUnit(state.rescue.units, result.teleport->unitId);
    assert(pulled);
    pulled->unit.cell = result.teleport->destinationCell;
    pulled->position = rescueCellPosition(state, result.teleport->destinationCell);
    syncRescuePosition(state, pulled->unit.id, pulled->position);

    if (result.counterDelta.unitId >= 0)
    {
        auto* puller = findRescueUnit(state.rescue.units, result.counterDelta.unitId);
        assert(puller);
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
    state.rescue.committedResults.push_back(std::move(rescue));
    return true;
}

void applyRescueRepositionForDamage(
    BattleRuntimeState& state,
    const BattleDamageTransactionResult& transaction,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    if (state.rescue.units.empty() || transaction.defender.maxHp <= 0 || !transaction.defender.alive)
    {
        return;
    }

    const auto* pulled = findRescueUnit(state.rescue.units, transaction.defender.id);
    const auto* attacker = findRescueUnit(state.rescue.units, transaction.attacker.id);
    if (!pulled)
    {
        return;
    }

    auto* mutablePulled = findRescueUnit(state.rescue.units, transaction.defender.id);
    assert(mutablePulled);
    mutablePulled->unit.alive = transaction.defender.alive;
    mutablePulled->unit.hp = transaction.defender.hp;
    mutablePulled->unit.maxHp = transaction.defender.maxHp;
    mutablePulled->unit.invincible = transaction.defender.invincible;

    const int hpBefore = transaction.defender.hp - transaction.defenderDelta.hpDelta;
    if (attacker
        && attacker->unit.alive
        && attacker->unit.team != pulled->unit.team
        && hpBefore * 4 > transaction.defender.maxHp
        && transaction.defender.hp * 4 <= transaction.defender.maxHp)
    {
        tryApplyRescue(state,
                       BattleRescuePullMode::Protect,
                       transaction.defender.id,
                       pulled->unit.team,
                       logEvents,
                       visualEvents);
    }

    const auto* currentPulled = findRescueUnit(state.rescue.units, transaction.defender.id);
    assert(currentPulled);
    if (hpBefore * 100 > transaction.defender.maxHp * 15
        && currentPulled->unit.hp * 100 <= transaction.defender.maxHp * 15
        && state.rescue.executeUnattendedRadius > 0.0
        && rescueUnitUnattendedByTeam(state, transaction.defender.id, 1 - currentPulled->unit.team))
    {
        tryApplyRescue(state,
                       BattleRescuePullMode::Execute,
                       transaction.defender.id,
                       1 - currentPulled->unit.team,
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
    const auto* defenderExtras = findDamageRuntimeUnit(state.damage.unitExtras, request.defenderUnitId);
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
            findDamageRuntimeUnit(state.damage.unitExtras, request.attackerUnitId));
    }
    else
    {
        transaction.attacker.id = request.attackerUnitId;
    }
    transaction.defender = defender;
    if (const auto* runtimeStatus = findStatusUnit(state.status.units, request.defenderUnitId))
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
        ? std::max(command.damage, defender->hp)
        : command.damage;
    request.preResolvedDamage = true;

    BattleDamageTransactionInput transaction;
    if (!buildFrameDamageTransaction(state, request, transaction))
    {
        return false;
    }
    state.damage.pendingTransactions.push_back(std::move(transaction));

    const auto styleIt = state.damage.presentationStylesByDefender.find(command.targetUnitId);
    if (styleIt != state.damage.presentationStylesByDefender.end())
    {
        while (state.damage.pendingPresentation.size() + 1 < state.damage.pendingTransactions.size())
        {
            state.damage.pendingPresentation.push_back({});
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
        state.damage.pendingPresentation.push_back(std::move(presentation));
    }
    else if (!state.damage.pendingPresentation.empty())
    {
        state.damage.pendingPresentation.push_back({});
    }
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
    state.damage.pendingTransactions.push_back(std::move(transaction));
    if (!state.damage.pendingPresentation.empty())
    {
        state.damage.pendingPresentation.push_back({});
    }
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
    state.damage.pendingTransactions.push_back(std::move(transaction));
    if (!state.damage.pendingPresentation.empty())
    {
        state.damage.pendingPresentation.push_back({});
    }
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
    state.teamEffects.committedEvents.insert(
        state.teamEffects.committedEvents.end(),
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

    const int restored = std::min(command.amount, std::max(0, unit->maxMp - unit->mp));
    if (restored <= 0)
    {
        return true;
    }

    unit->mp += restored;
    applications.mpRestores.push_back({ command.unitId, restored });
    appendStatusEventLog(logEvents, command.unitId, command.unitId, command.reason);
    return true;
}

bool applyFrameUnitHealCommand(
    BattleRuntimeState& state,
    const BattleUnitHealCommand& command,
    BattleFrameApplications& applications,
    std::vector<BattleLogEvent>& logEvents)
{
    auto* unit = state.units.findUnit(command.targetUnitId);
    if (!unit)
    {
        return false;
    }

    const int before = unit->hp;
    unit->hp = std::min(unit->maxHp, unit->hp + command.amount);
    const int healed = unit->hp - before;
    if (healed <= 0)
    {
        return true;
    }

    applications.unitHeals.push_back({ command.sourceUnitId, command.targetUnitId, healed });
    appendHealEventLog(logEvents, command.sourceUnitId, command.targetUnitId, healed, command.reason);
    return true;
}

bool applyFrameUnitShieldCommand(
    BattleRuntimeState& state,
    const BattleUnitShieldCommand& command,
    BattleFrameApplications& applications,
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
    applications.unitShields.push_back({ command.sourceUnitId, command.targetUnitId, command.amount });
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
    BattleFrameApplications& applications,
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
        unit->attack += command.attackBonus;
        unit->defence += command.defenceBonus;
    }
    if (!command.permanent && comboIt != state.combo.units.end())
    {
        if (command.attackBonus != 0 && command.durationFrames > 0)
        {
            comboIt->second.tempAttackBuffs.push_back({ command.attackBonus, command.durationFrames });
        }
    }
    applications.tempAttackBuffs.push_back({
        command.unitId,
        command.attackBonus,
        command.defenceBonus,
        command.durationFrames,
        command.permanent,
    });
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
        return applyFrameTeamEffectCommand(state, command, logEvents);
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
        applications.autoUltimateRequests.push_back({ autoUltimate->unitId, autoUltimate->consumeMp });
        return true;
    }
    if (const auto* knockback = std::get_if<BattleKnockbackCommand>(&command))
    {
        applications.knockbacks.push_back({
            knockback->targetUnitId,
            knockback->velocityDelta,
            knockback->velocityCap,
            knockback->grantHurtFrame,
        });
        if (auto* unit = findWorldUnit(state.world, knockback->targetUnitId))
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
        return applyFrameTempAttackBuffCommand(state, *tempAttack, applications, logEvents);
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
        return applyFrameUnitHealCommand(state, *heal, applications, logEvents);
    }
    if (const auto* shield = std::get_if<BattleUnitShieldCommand>(&command))
    {
        return applyFrameUnitShieldCommand(state, *shield, applications, logEvents);
    }
    assert(false);
    return false;
}

void reduceFrameGameplayCommands(
    BattleRuntimeState& state,
    std::vector<BattleGameplayCommand>& commands,
    BattleFrameApplications& applications,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    std::vector<BattleGameplayCommand> pending = std::move(commands);
    std::vector<BattleGameplayCommand> unreduced;
    for (std::size_t i = 0; i < pending.size(); ++i)
    {
        if (!reduceFrameGameplayCommand(state, pending[i], applications, pending, logEvents, visualEvents))
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
    input.pendingTransactions = state.damage.pendingTransactions;
    input.pendingPresentation = state.damage.pendingPresentation;
    input.unitEffects = state.damage.unitEffects;
    input.pendingAliveByTeam = state.result.pendingAliveByTeam;
    input.deathEffects = &state.deathEffects.store;
    input.deathEffectUnits = &state.units;
    input.projectileFollowUps = state.projectileFollowUps;
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
    for (const auto& [team, count] : state.result.pendingAliveByTeam)
    {
        if (count > 0)
        {
            aliveTeams.insert(team);
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

void advanceStatus(BattleRuntimeState& state)
{
    state.status.config.frame = state.world.frame;
    auto statusTick = BattleStatusSystem(state.status.config).tick(state.units, state.status.units);
    state.status.events.insert(
        state.status.events.end(),
        statusTick.events.begin(),
        statusTick.events.end());
    for (const auto& event : statusTick.events)
    {
        if (event.type != BattleStatusEventType::PoisonDamage
            && event.type != BattleStatusEventType::BleedDamage)
        {
            continue;
        }

        auto* attackerUnit = state.units.findUnit(event.sourceUnitId);
        auto* defenderUnit = state.units.findUnit(event.unitId);
        auto* defenderStatus = findStatusUnit(state.status.units, event.unitId);
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
            findDamageRuntimeUnit(state.damage.unitExtras, event.sourceUnitId));
        transaction.defender = makeBattleDamageUnitState(
            *defenderUnit,
            findDamageRuntimeUnit(state.damage.unitExtras, event.unitId));
        transaction.defenderStatus = makeBattleStatusUnitState(
            *defenderStatus,
            state.units.requireUnit(event.unitId));
        transaction.defenderCooldown = makeBattleFrameCooldownState(state.units.requireUnit(event.unitId));
        state.damage.pendingTransactions.push_back(std::move(transaction));
    }
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
    state.teamEffects.committedEvents.insert(
        state.teamEffects.committedEvents.end(),
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
    state.effects.committedCommands.insert(
        state.effects.committedCommands.end(),
        commands.begin(),
        commands.end());

}

void applyRuntimeComboEvents(
    BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logEvents)
{
    for (const auto& result : state.runtime.committedResults)
    {
        for (const auto& event : result.comboEvents)
        {
            switch (event.type)
            {
            case BattleComboFrameRuntimeEventType::SelfHpRegen:
            case BattleComboFrameRuntimeEventType::HealAura:
            case BattleComboFrameRuntimeEventType::HealPercentSelf:
                applyRuntimeTeamEvents(state, result.unitId, event, logEvents);
                break;
            case BattleComboFrameRuntimeEventType::BroadcastTriggerTimer:
                applyBroadcastTriggerTimer(state, result.unitId, event);
                break;
            case BattleComboFrameRuntimeEventType::PostSkillInvincibility:
                applyPostSkillInvincibility(state, result.unitId, event, logEvents);
                break;
            case BattleComboFrameRuntimeEventType::AutoUltimateReady:
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
    }
}

void applyPendingTeamEffects(
    BattleRuntimeState& state,
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
            state.teamEffects.committedEvents.insert(
                state.teamEffects.committedEvents.end(),
                application.events.begin(),
                application.events.end());
            continue;
        }
        unappliedCommands.push_back(command);
    }
    state.teamEffects.pendingCommands = std::move(unappliedCommands);
}

void advanceMovementPhysics(BattleRuntimeState& state)
{
    state.movementPhysics.committedResults.clear();
    if (state.movementPhysics.units.empty())
    {
        return;
    }

    assert(state.movementPhysics.collision.tileWidth > 0.0);
    assert(state.movementPhysics.collision.coordCount > 0);
    assert(state.movementPhysics.collision.defaultSeparationDistance > 0.0);

    for (const auto& input : state.movementPhysics.units)
    {
        assert(input.unitId >= 0);
        BattleFrameMovementPhysicsUnitResult result;
        result.unitId = input.unitId;
        result.state = input.state;
        result.frozenFrames = input.frozenFrames;

        if (result.frozenFrames > 0)
        {
            --result.frozenFrames;
            state.movementPhysics.committedResults.push_back(std::move(result));
            continue;
        }

        BattleMovementPhysicsInput physicsInput;
        physicsInput.state = input.state;
        physicsInput.config = state.movementPhysics.config;
        physicsInput.actionDashActive = input.actionDashActive;
        physicsInput.canMove = [&](Pointf position, int separationDistance)
        {
            return canMoveInPhysicsSnapshot(
                state.movementPhysics.collision,
                input.unitId,
                input.state.position,
                position,
                separationDistance);
        };
        result.state = BattleMovementPhysicsSystem().advance(physicsInput);
        result.physicsAdvanced = true;

        if (auto* unit = findWorldUnit(state.world, input.unitId))
        {
            unit->position = result.state.position;
            unit->velocity = result.state.velocity;
        }
        if (hasCanonicalUnitStore(state))
        {
            state.units.setMotion(
                input.unitId,
                result.state.position,
                result.state.velocity,
                result.state.acceleration);
        }
        auto collisionIt = std::find_if(
            state.movementPhysics.collision.units.begin(),
            state.movementPhysics.collision.units.end(),
            [&](const BattleMovementPhysicsCollisionUnitSnapshot& unit)
            {
                return unit.id == input.unitId;
            });
        if (collisionIt != state.movementPhysics.collision.units.end())
        {
            collisionIt->position = result.state.position;
        }
        state.movementPhysics.committedResults.push_back(std::move(result));
    }
    state.movementPhysics.units.clear();
}

void advanceMovement(BattleRuntimeState& state, BattleFrameResult& result)
{
    result.movement = BattleCore(state.world).tickMovement();
}

void advanceActionFrameUnits(
    BattleRuntimeState& state,
    const BattleTickResult& movement,
    std::vector<BattleFrameActionUnitResult>& actionResults,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    for (const auto& input : state.actions.units)
    {
        assert(input.unitId >= 0);

        BattleFrameActionUnitResult result;
        result.unitId = input.unitId;
        result.state = input.state;

        if (input.hasSelectedCastInput)
        {
            assert(input.selectedOperationType != BattleOperationType::None);
            auto cast = BattleCastPlanner().commitSelectedCast(
                refreshedCastInput(state, movement, input.selectedCastInput),
                input.selectedCastUltimate,
                input.selectedOperationType);
            gameplayEvents.insert(gameplayEvents.end(), cast.gameplayEvents.begin(), cast.gameplayEvents.end());
            visualEvents.insert(visualEvents.end(), cast.visualEvents.begin(), cast.visualEvents.end());
            result.castResult = cast;

            if (cast.decision.canCast)
            {
                result.actionCommitted = true;
                result.castCommitted = true;
                result.actionInput = input.selectedActionInput;
                result.actionInput.hasCast = true;
                result.actionInput.cast = std::move(cast);
                result.actionResult = BattleActionCommitSystem().commit(result.actionInput);
                state.pendingAttackSpawns.insert(
                    state.pendingAttackSpawns.end(),
                    result.actionResult.attackSpawnRequests.begin(),
                    result.actionResult.attackSpawnRequests.end());
                visualEvents.insert(
                    visualEvents.end(),
                    result.actionResult.visualEvents.begin(),
                    result.actionResult.visualEvents.end());
            }
        }
        else if (!input.state.haveAction && input.canPlanCast)
        {
            auto cast = BattleCastPlanner().plan(refreshedCastInput(state, movement, input.castInput));
            gameplayEvents.insert(gameplayEvents.end(), cast.gameplayEvents.begin(), cast.gameplayEvents.end());
            visualEvents.insert(visualEvents.end(), cast.visualEvents.begin(), cast.visualEvents.end());
            if (cast.decision.canCast)
            {
                result.castStarted = true;
                result.state.haveAction = true;
                result.state.actFrame = 0;
                result.state.operationType = cast.decision.operationType;
                result.state.cooldownFrames = cast.animation.cooldownFrames;
                result.state.recoveryFrames = cast.animation.recoveryFrames;
            }
            result.castResult = std::move(cast);
        }
        else if (input.state.haveAction && input.hasPendingActionInput)
        {
            assert(input.state.castFrame >= 0);
            if (input.state.actFrame == input.state.castFrame)
            {
                result.actionCommitted = true;
                result.castCommitted = input.pendingActionInput.hasCast;
                result.actionInput = input.pendingActionInput;
                result.actionResult = BattleActionCommitSystem().commit(input.pendingActionInput);
                state.pendingAttackSpawns.insert(
                    state.pendingAttackSpawns.end(),
                    result.actionResult.attackSpawnRequests.begin(),
                    result.actionResult.attackSpawnRequests.end());
                visualEvents.insert(
                    visualEvents.end(),
                    result.actionResult.visualEvents.begin(),
                    result.actionResult.visualEvents.end());
            }
        }

        if (input.state.haveAction)
        {
            ++result.state.actFrame;
            if (result.state.cooldownFrames > 0
                && result.state.actType >= 0
                && result.state.operationType != BattleOperationType::None
                && result.state.actFrame > result.state.castFrame + result.state.recoveryFrames)
            {
                result.state.haveAction = false;
                result.state.operationType = BattleOperationType::None;
                result.state.actType = -1;
            }
        }

        actionResults.push_back(std::move(result));
    }
    state.actions.units.clear();
}

void advanceAttacksAndResolveHits(
    BattleRuntimeState& state,
    BattleFrameResult& result,
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
        result.commands);
    resolveHitEvents(
        state,
        result.attackEvents,
        result.hitResults,
        result.commands,
        logEvents,
        visualEvents);
    reduceFrameGameplayCommands(state, result.commands, result.applications, logEvents, visualEvents);
}

void applyDamageAndLifecycle(
    BattleRuntimeState& state,
    BattleFrameResult& result,
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

    state.damage.lifecycleEvents = application.lifecycleEvents;
    state.damage.logEvents = application.logEvents;
    state.damage.visualEvents = application.visualEvents;
    logEvents.insert(
        logEvents.end(),
        state.damage.logEvents.begin(),
        state.damage.logEvents.end());
    visualEvents.insert(
        visualEvents.end(),
        state.damage.visualEvents.begin(),
        state.damage.visualEvents.end());
    result.commands.insert(
        result.commands.end(),
        application.commands.begin(),
        application.commands.end());

    for (const auto& transaction : application.transactions)
    {
        applyDamageResultToFrameState(state, transaction);
        applyRescueRepositionForDamage(state, transaction, logEvents, visualEvents);
        for (const auto& event : transaction.events)
        {
            if (event.type == BattleDamageEventType::UnitDied)
            {
                continue;
            }
            gameplayEvents.push_back(toGameplayEvent(event));
        }
        state.damage.committedTransactions.push_back(transaction);
    }
    for (const auto& event : application.gameplayEvents)
    {
        if (event.type == BattleGameplayEventType::BattleEnded && battleEndAlreadyEmitted)
        {
            continue;
        }
        gameplayEvents.push_back(event);
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
    recorder.beginFrame(makePresentationSnapshot(state));
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

int collectFrameExtraProjectileCount(KysChess::RoleComboState& state, int unitId, int baseCount)
{
    return BattleComboTriggerSystem().collectExtraProjectileCount(
        state,
        { BattleComboTriggerHook::AfterSkillCast, unitId, -1 },
        baseCount,
        []() { return 0.0; });
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
    double rollPercent)
{
    return BattleComboTriggerSystem().resolveArmorPenetratedDefense(
        state,
        { attackerUnitId, targetUnitId, defense },
        [rollPercent]() { return rollPercent; }).defense;
}

BattleFrameUnitRuntimeResult BattleFrameUnitRuntimeSystem::advance(
    const BattleFrameUnitRuntimeInput& input) const
{
    assert(input.frame >= 0);
    assert(input.mpRegenIntervalFrames > 0);
    assert(input.physicalPowerRegenIntervalFrames > 0);
    assert(input.state.cooldown >= 0);
    assert(input.state.physicalPower >= 0);

    BattleFrameUnitRuntimeResult result;
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

void clearBattleDamageFrameScratch(BattleRuntimeState& state)
{
    state.damage.pendingTransactions.clear();
    state.damage.pendingPresentation.clear();
    state.damage.committedTransactions.clear();
    state.damage.lifecycleEvents.clear();
    state.damage.logEvents.clear();
    state.damage.visualEvents.clear();
}

BattleFrameResult BattleFrameRunner::runFrame(BattleRuntimeState& state) const
{
    assertFrameMovementConfigConfigured(state.world.config);
    assertFrameAttackWorldConfigured(state.attacks);

    BattleFrameResult result;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;

    state.teamEffects.committedEvents.clear();
    state.effects.committedCommands.clear();
    state.rescue.committedResults.clear();
    advanceStatus(state);
    advanceRuntimeUnits(state, result.commands);
    applyRuntimeComboEvents(state, logEvents);
    applyPendingTeamEffects(state, logEvents);
    reduceFrameGameplayCommands(state, result.commands, result.applications, logEvents, visualEvents);
    advanceMovementPhysics(state);
    advanceMovement(state, result);
    advanceActionFrameUnits(
        state,
        result.movement,
        result.actionResults,
        gameplayEvents,
        visualEvents);
    advanceAttacksAndResolveHits(state, result, logEvents, visualEvents);
    applyDamageAndLifecycle(state, result, gameplayEvents, logEvents, visualEvents);
    reduceFrameGameplayCommands(state, result.commands, result.applications, logEvents, visualEvents);
    emitPresentationFrame(state, result, gameplayEvents, logEvents, visualEvents);
    return result;
}

}  // namespace KysChess::Battle
