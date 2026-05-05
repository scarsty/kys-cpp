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
namespace
{
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

void applyStatusSnapshot(BattlePresentationUnitSnapshot& snapshot, const std::vector<BattleStatusUnitState>& statusUnits)
{
    auto it = std::find_if(statusUnits.begin(), statusUnits.end(), [&](const BattleStatusUnitState& unit)
        {
            return unit.id == snapshot.id;
        });
    if (it == statusUnits.end())
    {
        return;
    }

    snapshot.alive = it->alive;
    snapshot.hp = it->hp;
    snapshot.maxHp = it->maxHp;
    snapshot.invincible = it->invincible;
}

BattlePresentationSnapshot makePresentationSnapshot(const BattleFrameState& state)
{
    BattlePresentationSnapshot snapshot;
    snapshot.frame = state.world.frame;
    snapshot.units.reserve(state.world.units.size());
    for (const auto& unit : state.world.units)
    {
        auto presentationUnit = toPresentationUnit(unit);
        applyStatusSnapshot(presentationUnit, state.status.units);
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

BattlePresentationEvent toPresentationEvent(const BattleEvent& event)
{
    BattlePresentationEvent presentation;
    presentation.type = BattlePresentationEventType::StatusLog;
    presentation.sourceUnitId = event.unitId;
    presentation.targetUnitId = event.targetId;
    presentation.text = textForMovementEvent(event.type);
    presentation.position = event.to;
    presentation.amount = static_cast<int>(event.value);
    return presentation;
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
    const BattleFrameState& state,
    int attackId,
    int otherAttackId)
{
    auto it = std::find_if(
        state.projectileCancel.baseDamages.begin(),
        state.projectileCancel.baseDamages.end(),
        [&](const BattleProjectileCancelBaseDamage& input)
        {
            return input.attackId == attackId
                && (input.otherAttackId < 0 || input.otherAttackId == otherAttackId);
        });
    return it != state.projectileCancel.baseDamages.end() ? &*it : nullptr;
}

const BattleFrameHitScalarInput* findHitScalar(
    const BattleFrameState& state,
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

const BattleHitUnitSnapshot* findHitUnit(const BattleFrameState& state, int unitId)
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
    const BattleFrameState& state,
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
    const BattleFrameState& state,
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

BattlePresentationEvent dodgeStatusEvent(int defenderUnitId, int attackerUnitId)
{
    BattlePresentationEvent event;
    event.type = BattlePresentationEventType::StatusLog;
    event.sourceUnitId = defenderUnitId;
    event.targetUnitId = attackerUnitId;
    event.text = "閃避了來襲攻擊";
    return event;
}

double takeRuntimePercentRoll(BattleFrameState& state)
{
    if (state.runtime.nextPercentRoll < state.runtime.percentRolls.size())
    {
        const double roll = state.runtime.percentRolls[state.runtime.nextPercentRoll++];
        assert(roll >= 0.0 && roll < 100.0);
        return roll;
    }
    return 0.0;
}

void applyAttackContext(BattlePresentationEvent& presentation, const BattleAttackWorld& world, int attackId)
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

BattlePresentationEvent toProjectileSpawnPresentationEvent(
    const BattleAttackWorld& world,
    int attackId)
{
    BattlePresentationEvent presentation;
    presentation.type = BattlePresentationEventType::ProjectileSpawned;
    applyAttackContext(presentation, world, attackId);
    return presentation;
}

std::vector<BattlePresentationEvent> toPresentationEvents(
    const BattleAttackEvent& event,
    const BattleAttackWorld& world)
{
    BattlePresentationEvent presentation;
    applyAttackContext(presentation, world, event.attackId);

    switch (event.type)
    {
    case BattleAttackEventType::AttackSpawned:
        presentation.type = BattlePresentationEventType::ProjectileSpawned;
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
        presentation.type = BattlePresentationEventType::ProjectileMoved;
        break;
    case BattleAttackEventType::Hit:
        presentation.type = BattlePresentationEventType::ProjectileHit;
        presentation.targetUnitId = event.unitId;
        break;
    case BattleAttackEventType::Expired:
        presentation.type = BattlePresentationEventType::ProjectileExpired;
        break;
    case BattleAttackEventType::TargetLost:
        presentation.type = BattlePresentationEventType::ProjectileTargetLost;
        presentation.targetUnitId = event.unitId;
        presentation.amount = -1;
        break;
    case BattleAttackEventType::ProjectileCancel:
        presentation.type = BattlePresentationEventType::ProjectileCancelled;
        presentation.amount = event.otherAttackId;
        break;
    case BattleAttackEventType::Bounce:
        presentation.type = BattlePresentationEventType::ProjectileBounced;
        presentation.targetUnitId = event.unitId;
        presentation.amount = event.otherAttackId;
        break;
    }
    std::vector<BattlePresentationEvent> presentations;
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

void syncAttackUnitsFromWorld(BattleFrameState& state)
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

void advanceRuntimeUnits(BattleFrameState& state, std::vector<BattleGameplayCommand>& commands)
{
    BattleComboTriggerSystem comboSystem;
    state.runtime.committedResults.clear();
    for (const auto& input : state.runtime.units)
    {
        assert(input.unitId >= 0);

        BattleFrameRuntimeUnitResult committed;
        committed.unitId = input.unitId;
        committed.result = BattleFrameUnitRuntimeSystem().advance(input.input);

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
    BattleFrameState& state,
    std::vector<BattleAttackEvent>& events,
    std::vector<BattleGameplayCommand>& commands)
{
    BattleAttackSystem attackSystem;
    state.projectileCancel.committedCommands.clear();
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
        state.projectileCancel.committedCommands.push_back(command);
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

void updateCommittedHitCombos(BattleFrameState& state, const BattleHitResolutionResult& result)
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
    const BattleFrameState& state,
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
    BattleFrameState& state,
    const BattleAttackEvent& event,
    const BattleFrameHitScalarInput& scalar,
    std::vector<BattlePresentationEvent>& presentationEvents)
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
    result.presentationEvents.push_back(dodgeStatusEvent(event.unitId, event.sourceUnitId));
    presentationEvents.insert(
        presentationEvents.end(),
        result.presentationEvents.begin(),
        result.presentationEvents.end());
    state.hits.committedResults.push_back(std::move(result));
    return true;
}

void resolveHitEvents(
    BattleFrameState& state,
    const std::vector<BattleAttackEvent>& events,
    std::vector<BattleGameplayCommand>& commands,
    std::vector<BattlePresentationEvent>& presentationEvents)
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
        if (tryResolveDodgeHit(state, event, *scalar, presentationEvents))
        {
            continue;
        }

        auto input = makeHitResolutionInput(state, event, *scalar, consumedDodgeRoll);
        auto result = BattleHitResolver().resolve(input);
        auto followUps = expandBattleProjectileFollowUpCommands(result.commands, state.projectileFollowUps);
        result.commands = std::move(followUps.commands);
        result.presentationEvents.insert(
            result.presentationEvents.end(),
            followUps.presentationEvents.begin(),
            followUps.presentationEvents.end());
        updateCommittedHitCombos(state, result);
        commands.insert(commands.end(), result.commands.begin(), result.commands.end());
        presentationEvents.insert(
            presentationEvents.end(),
            result.presentationEvents.begin(),
            result.presentationEvents.end());
        state.hits.committedResults.push_back(std::move(result));
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

BattleStatusUnitState* findStatusUnit(std::vector<BattleStatusUnitState>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleStatusUnitState& unit)
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

const BattleStatusUnitState* findStatusUnit(const std::vector<BattleStatusUnitState>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleStatusUnitState& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

BattleDamageUnitState* findDamageUnit(std::vector<BattleDamageUnitState>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleDamageUnitState& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

const BattleDamageUnitState* findDamageUnit(const std::vector<BattleDamageUnitState>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleDamageUnitState& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

BattleTeamEffectUnit* findTeamEffectUnit(BattleTeamEffectWorld& world, int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleTeamEffectUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != world.units.end() ? &*it : nullptr;
}

const BattleTeamEffectUnit* findTeamEffectUnit(const BattleTeamEffectWorld& world, int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleTeamEffectUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != world.units.end() ? &*it : nullptr;
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

BattleEffectUnit* findEffectUnit(BattleEffectWorld& world, int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleEffectUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != world.units.end() ? &*it : nullptr;
}

BattleDeathEffectUnit* findDeathEffectUnit(BattleDeathEffectWorld& world, int unitId)
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleDeathEffectUnit& unit)
        {
            return unit.id == unitId;
        });
    return it != world.units.end() ? &*it : nullptr;
}

void applyDamageUnitToDeathEffectUnit(BattleDeathEffectUnit& deathUnit, const BattleDamageUnitState& damageUnit)
{
    deathUnit.alive = damageUnit.alive;
    deathUnit.hp = damageUnit.hp;
    deathUnit.maxHp = damageUnit.maxHp;
    deathUnit.attack = damageUnit.attack;
    deathUnit.shield = damageUnit.shield;
}

double distance2d(Pointf lhs, Pointf rhs)
{
    return EuclidDis(lhs.x - rhs.x, lhs.y - rhs.y);
}

BattleCastInput refreshedCastInput(const BattleFrameState& state,
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
        input.unit.alive = status->alive;
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

void applyDamageResultToFrameState(BattleFrameState& state, const BattleDamageTransactionResult& transaction)
{
    if (auto* attacker = findDamageUnit(state.damage.units, transaction.attacker.id))
    {
        *attacker = transaction.attacker;
    }
    if (auto* defender = findDamageUnit(state.damage.units, transaction.defender.id))
    {
        *defender = transaction.defender;
    }
    if (auto cooldownIt = state.damage.cooldowns.find(transaction.defender.id);
        cooldownIt != state.damage.cooldowns.end())
    {
        cooldownIt->second = transaction.defenderCooldown;
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
        *status = transaction.defenderStatus;
        status->alive = transaction.defender.alive;
        status->hp = transaction.defender.hp;
        status->maxHp = transaction.defender.maxHp;
        status->invincible = transaction.defender.invincible;
    }
    if (auto* deathUnit = findDeathEffectUnit(state.deathEffects.world, transaction.defender.id))
    {
        applyDamageUnitToDeathEffectUnit(*deathUnit, transaction.defender);
    }
    if (auto* deathUnit = findDeathEffectUnit(state.deathEffects.world, transaction.attacker.id))
    {
        applyDamageUnitToDeathEffectUnit(*deathUnit, transaction.attacker);
    }
}

void syncRescueDamageUnit(BattleFrameState& state, int unitId, int hp, int invincible)
{
    if (auto* damageUnit = findDamageUnit(state.damage.units, unitId))
    {
        damageUnit->hp = hp;
        damageUnit->invincible = invincible;
    }
    if (auto* status = findStatusUnit(state.status.units, unitId))
    {
        status->hp = hp;
        status->invincible = invincible;
    }
    if (auto* teamUnit = findTeamEffectUnit(state.teamEffects.world, unitId))
    {
        teamUnit->hp = hp;
    }
    if (auto* deathUnit = findDeathEffectUnit(state.deathEffects.world, unitId))
    {
        deathUnit->hp = hp;
    }
}

void syncRescuePosition(BattleFrameState& state, int unitId, Pointf position)
{
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
    const BattleFrameState& state,
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

Pointf rescueCellPosition(const BattleFrameState& state, Point cell)
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

bool rescueUnitUnattendedByTeam(const BattleFrameState& state, int targetUnitId, int team)
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
    const BattleFrameState& state,
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
    BattleFrameState& state,
    const BattleRescueRepositionResult& result,
    std::vector<BattlePresentationEvent>& presentationEvents)
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
    presentationEvents.insert(
        presentationEvents.end(),
        result.presentationEvents.begin(),
        result.presentationEvents.end());
}

bool tryApplyRescue(
    BattleFrameState& state,
    BattleRescuePullMode mode,
    int pulledUnitId,
    int pullerTeam,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    auto rescue = BattleRescueRepositionSystem().resolve(
        makeRescueInput(state, mode, pulledUnitId, pullerTeam));
    if (!rescue.teleport)
    {
        return false;
    }
    applyRescueResultToFrameState(state, rescue, presentationEvents);
    state.rescue.committedResults.push_back(std::move(rescue));
    return true;
}

void applyRescueRepositionForDamage(
    BattleFrameState& state,
    const BattleDamageTransactionResult& transaction,
    std::vector<BattlePresentationEvent>& presentationEvents)
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
                       presentationEvents);
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
                       presentationEvents);
    }
}

bool buildFrameDamageTransaction(
    const BattleFrameState& state,
    const BattleDamageRequest& request,
    BattleDamageTransactionInput& transaction)
{
    assert(request.defenderUnitId >= 0);

    const auto* defender = findDamageUnit(state.damage.units, request.defenderUnitId);
    if (!defender)
    {
        return false;
    }

    transaction.request = request;
    if (request.attackerUnitId >= 0)
    {
        const auto* attacker = findDamageUnit(state.damage.units, request.attackerUnitId);
        if (!attacker)
        {
            return false;
        }
        transaction.attacker = *attacker;
    }
    else
    {
        transaction.attacker.id = request.attackerUnitId;
    }
    transaction.defender = *defender;
    const auto* status = findStatusUnit(state.damage.statusUnits, request.defenderUnitId);
    if (!status)
    {
        status = findStatusUnit(state.status.units, request.defenderUnitId);
    }
    if (status)
    {
        transaction.defenderStatus = *status;
    }
    else
    {
        transaction.defenderStatus = makeFallbackStatusUnit(*defender);
    }
    if (auto cooldownIt = state.damage.cooldowns.find(request.defenderUnitId);
        cooldownIt != state.damage.cooldowns.end())
    {
        transaction.defenderCooldown = cooldownIt->second;
    }
    return true;
}

bool tryAppendFrameDamageTransaction(
    BattleFrameState& state,
    const BattleHpDamageCommand& command)
{
    const auto* defender = findDamageUnit(state.damage.units, command.targetUnitId);
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
    BattleFrameState& state,
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
    BattleFrameState& state,
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

BattleStatusUnitState* findFrameStatusUnit(BattleFrameState& state, int unitId)
{
    if (auto* status = findStatusUnit(state.damage.statusUnits, unitId))
    {
        return status;
    }
    return findStatusUnit(state.status.units, unitId);
}

void syncTeamEffectEventsToFrameState(
    BattleFrameState& state,
    const std::vector<BattleTeamEffectEvent>& events)
{
    for (const auto& event : events)
    {
        const auto* unit = findTeamEffectUnit(state.teamEffects.world, event.targetUnitId);
        assert(unit);
        if (auto* damageUnit = findDamageUnit(state.damage.units, event.targetUnitId))
        {
            damageUnit->hp = unit->hp;
            damageUnit->mp = unit->mp;
            damageUnit->shield = unit->shield;
        }
        if (auto* status = findFrameStatusUnit(state, event.targetUnitId))
        {
            status->hp = unit->hp;
        }
        if (auto cooldownIt = state.damage.cooldowns.find(event.targetUnitId);
            cooldownIt != state.damage.cooldowns.end())
        {
            cooldownIt->second.cooldown = unit->cooldown;
        }
        if (auto comboIt = state.combo.units.find(event.targetUnitId);
            comboIt != state.combo.units.end())
        {
            comboIt->second.shield = unit->shield;
        }
        if (auto* deathUnit = findDeathEffectUnit(state.deathEffects.world, event.targetUnitId))
        {
            deathUnit->hp = unit->hp;
            deathUnit->shield = unit->shield;
        }
    }
}

void appendStatusPresentation(
    std::vector<BattlePresentationEvent>& presentationEvents,
    int sourceUnitId,
    int targetUnitId,
    std::string text)
{
    BattlePresentationEvent presentation;
    presentation.type = BattlePresentationEventType::StatusLog;
    presentation.sourceUnitId = sourceUnitId;
    presentation.targetUnitId = targetUnitId;
    presentation.text = std::move(text);
    presentationEvents.push_back(std::move(presentation));
}

void appendHealPresentation(
    std::vector<BattlePresentationEvent>& presentationEvents,
    int sourceUnitId,
    int targetUnitId,
    int amount,
    std::string text)
{
    BattlePresentationEvent presentation;
    presentation.type = BattlePresentationEventType::HealLog;
    presentation.sourceUnitId = sourceUnitId;
    presentation.targetUnitId = targetUnitId;
    presentation.amount = amount;
    presentation.text = std::move(text);
    presentationEvents.push_back(std::move(presentation));
}

bool applyFrameTeamEffectCommand(
    BattleFrameState& state,
    const BattleGameplayCommand& command,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    if (state.teamEffects.world.units.empty())
    {
        return false;
    }

    auto application = applyBattleTeamEffectCommand(state.teamEffects.world, command);
    presentationEvents.insert(
        presentationEvents.end(),
        application.presentationEvents.begin(),
        application.presentationEvents.end());
    syncTeamEffectEventsToFrameState(state, application.events);
    state.teamEffects.committedEvents.insert(
        state.teamEffects.committedEvents.end(),
        application.events.begin(),
        application.events.end());
    return true;
}

bool applyFrameMpRestoreCommand(
    BattleFrameState& state,
    const BattleMpRestoreCommand& command,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    auto* unit = findDamageUnit(state.damage.units, command.unitId);
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
    if (auto* teamUnit = findTeamEffectUnit(state.teamEffects.world, command.unitId))
    {
        teamUnit->mp = unit->mp;
    }
    state.applications.mpRestores.push_back({ command.unitId, restored });
    appendStatusPresentation(presentationEvents, command.unitId, command.unitId, command.reason);
    return true;
}

bool applyFrameUnitHealCommand(
    BattleFrameState& state,
    const BattleUnitHealCommand& command,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    auto* unit = findDamageUnit(state.damage.units, command.targetUnitId);
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

    if (auto* status = findFrameStatusUnit(state, command.targetUnitId))
    {
        status->hp = unit->hp;
    }
    if (auto* teamUnit = findTeamEffectUnit(state.teamEffects.world, command.targetUnitId))
    {
        teamUnit->hp = unit->hp;
    }
    if (auto* deathUnit = findDeathEffectUnit(state.deathEffects.world, command.targetUnitId))
    {
        deathUnit->hp = unit->hp;
    }
    state.applications.unitHeals.push_back({ command.sourceUnitId, command.targetUnitId, healed });
    appendHealPresentation(presentationEvents, command.sourceUnitId, command.targetUnitId, healed, command.reason);
    return true;
}

bool applyFrameUnitShieldCommand(
    BattleFrameState& state,
    const BattleUnitShieldCommand& command,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    auto comboIt = state.combo.units.find(command.targetUnitId);
    if (comboIt == state.combo.units.end() && !findDamageUnit(state.damage.units, command.targetUnitId))
    {
        return false;
    }

    if (comboIt != state.combo.units.end())
    {
        comboIt->second.shield += command.amount;
    }
    if (auto* damageUnit = findDamageUnit(state.damage.units, command.targetUnitId))
    {
        damageUnit->shield += command.amount;
    }
    if (auto* teamUnit = findTeamEffectUnit(state.teamEffects.world, command.targetUnitId))
    {
        teamUnit->shield += command.amount;
    }
    if (auto* deathUnit = findDeathEffectUnit(state.deathEffects.world, command.targetUnitId))
    {
        deathUnit->shield += command.amount;
    }
    state.applications.unitShields.push_back({ command.sourceUnitId, command.targetUnitId, command.amount });
    appendStatusPresentation(
        presentationEvents,
        command.sourceUnitId,
        command.targetUnitId,
        formatStatusValue(command.reason, command.amount, "護盾"));
    return true;
}

bool applyFrameTempAttackBuffCommand(
    BattleFrameState& state,
    const BattleTempAttackBuffCommand& command,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    auto* unit = findDamageUnit(state.damage.units, command.unitId);
    auto comboIt = state.combo.units.find(command.unitId);
    if (!unit && comboIt == state.combo.units.end())
    {
        return false;
    }

    if (unit)
    {
        unit->attack += command.attackBonus;
    }
    if (!command.permanent && comboIt != state.combo.units.end())
    {
        if (command.attackBonus != 0 && command.durationFrames > 0)
        {
            comboIt->second.tempAttackBuffs.push_back({ command.attackBonus, command.durationFrames });
        }
    }
    if (auto* deathUnit = findDeathEffectUnit(state.deathEffects.world, command.unitId))
    {
        deathUnit->attack += command.attackBonus;
        deathUnit->defence += command.defenceBonus;
    }

    state.applications.tempAttackBuffs.push_back({
        command.unitId,
        command.attackBonus,
        command.defenceBonus,
        command.durationFrames,
        command.permanent,
    });
    if (!command.reason.empty())
    {
        appendStatusPresentation(
            presentationEvents,
            command.unitId,
            command.unitId,
            command.permanent
                ? std::format("{}（攻防+{}）", command.reason, command.attackBonus)
                : command.reason);
    }
    return true;
}

bool reduceFrameGameplayCommand(
    BattleFrameState& state,
    const BattleGameplayCommand& command,
    std::vector<BattleGameplayCommand>& pending,
    std::vector<BattlePresentationEvent>& presentationEvents)
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
        return applyFrameTeamEffectCommand(state, command, presentationEvents);
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
        auto followUps = expandBattleProjectileFollowUpCommands({ command }, state.projectileFollowUps);
        pending.insert(
            pending.end(),
            std::make_move_iterator(followUps.commands.begin()),
            std::make_move_iterator(followUps.commands.end()));
        presentationEvents.insert(
            presentationEvents.end(),
            std::make_move_iterator(followUps.presentationEvents.begin()),
            std::make_move_iterator(followUps.presentationEvents.end()));
        return true;
    }
    if (const auto* mpRestore = std::get_if<BattleMpRestoreCommand>(&command))
    {
        return applyFrameMpRestoreCommand(state, *mpRestore, presentationEvents);
    }
    if (const auto* autoUltimate = std::get_if<BattleAutoUltimateCommand>(&command))
    {
        state.applications.autoUltimateRequests.push_back({ autoUltimate->unitId, autoUltimate->consumeMp });
        return true;
    }
    if (const auto* knockback = std::get_if<BattleKnockbackCommand>(&command))
    {
        state.applications.knockbacks.push_back({
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
        return applyFrameTempAttackBuffCommand(state, *tempAttack, presentationEvents);
    }
    if (const auto* lastAttacker = std::get_if<BattleLastAttackerCommand>(&command))
    {
        state.applications.lastAttackers.push_back({ lastAttacker->targetUnitId, lastAttacker->attackerUnitId });
        return true;
    }
    if (const auto* rumble = std::get_if<BattleRumbleCommand>(&command))
    {
        state.applications.rumbles.push_back({
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
        return applyFrameUnitHealCommand(state, *heal, presentationEvents);
    }
    if (const auto* shield = std::get_if<BattleUnitShieldCommand>(&command))
    {
        return applyFrameUnitShieldCommand(state, *shield, presentationEvents);
    }
    assert(false);
    return false;
}

void reduceFrameGameplayCommands(
    BattleFrameState& state,
    std::vector<BattleGameplayCommand>& commands,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    std::vector<BattleGameplayCommand> pending = std::move(commands);
    std::vector<BattleGameplayCommand> unreduced;
    for (std::size_t i = 0; i < pending.size(); ++i)
    {
        if (!reduceFrameGameplayCommand(state, pending[i], pending, presentationEvents))
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

BattleDamageApplicationInput makeFrameDamageApplicationInput(const BattleFrameState& state)
{
    BattleDamageApplicationInput input;
    input.frame = state.world.frame;
    input.aggregatePendingTransactionsByDefender = state.damage.aggregatePendingTransactionsByDefender;
    input.pendingTransactions = state.damage.pendingTransactions;
    input.pendingPresentation = state.damage.pendingPresentation;
    input.unitEffects = state.damage.unitEffects;
    input.pendingAliveByTeam = state.result.pendingAliveByTeam;
    input.deathEffects = state.deathEffects.world;
    input.projectileFollowUps = state.projectileFollowUps;

    std::map<int, std::size_t> indexByUnitId;
    for (const auto& unit : state.world.units)
    {
        appendDamageApplicationUnit(input.units, indexByUnitId, unit.id, unit.team, unit.alive);
    }
    for (const auto& unit : state.deathEffects.world.units)
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

void updateBattleResult(BattleFrameState& state, std::vector<BattleGameplayEvent>& gameplayEvents)
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

void advanceStatus(BattleFrameState& state)
{
    state.status.config.frame = state.world.frame;
    auto statusTick = BattleStatusSystem(state.status.config).tick(state.status.units);
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

        auto* attacker = findDamageUnit(state.damage.units, event.sourceUnitId);
        auto* defender = findDamageUnit(state.damage.units, event.unitId);
        auto* defenderStatus = findStatusUnit(state.status.units, event.unitId);
        auto cooldownIt = state.damage.cooldowns.find(event.unitId);
        if (!attacker || !defender || !defenderStatus || cooldownIt == state.damage.cooldowns.end())
        {
            continue;
        }

        BattleDamageTransactionInput transaction;
        transaction.request.attackerUnitId = event.sourceUnitId;
        transaction.request.defenderUnitId = event.unitId;
        transaction.request.baseDamage = event.value;
        transaction.request.preResolvedDamage = true;
        transaction.attacker = *attacker;
        transaction.defender = *defender;
        transaction.defenderStatus = *defenderStatus;
        transaction.defenderCooldown = cooldownIt->second;
        state.damage.pendingTransactions.push_back(std::move(transaction));
    }
}

void appendTeamEffectPresentationEvents(
    std::vector<BattlePresentationEvent>& presentationEvents,
    const std::vector<BattleTeamEffectEvent>& events,
    const std::string& reason)
{
    for (const auto& event : events)
    {
        BattlePresentationEvent presentation;
        presentation.sourceUnitId = event.sourceUnitId;
        presentation.targetUnitId = event.targetUnitId;
        presentation.amount = event.value;
        presentation.text = reason;
        switch (event.type)
        {
        case BattleTeamEffectEventType::Heal:
            presentation.type = BattlePresentationEventType::HealLog;
            presentation.text = reason;
            break;
        case BattleTeamEffectEventType::MpRestore:
            presentation.type = BattlePresentationEventType::StatusLog;
            presentation.text = std::format("{}+{}MP", reason, event.value);
            break;
        case BattleTeamEffectEventType::ShieldGain:
            presentation.type = BattlePresentationEventType::StatusLog;
            presentation.text = formatStatusValue(reason, event.value, "護盾");
            break;
        case BattleTeamEffectEventType::CooldownReduced:
            presentation.type = BattlePresentationEventType::StatusLog;
            presentation.text = formatStatusValue(reason, event.value, "冷卻");
            break;
        }
        presentationEvents.push_back(std::move(presentation));
    }
}

void appendEffectCommandPresentationEvents(
    std::vector<BattlePresentationEvent>& presentationEvents,
    const std::vector<BattleEffectCommand>& commands)
{
    for (const auto& command : commands)
    {
        BattlePresentationEvent presentation;
        presentation.sourceUnitId = command.sourceUnitId;
        presentation.targetUnitId = command.targetUnitId;
        presentation.amount = command.value;
        presentation.text = command.label;
        switch (command.type)
        {
        case BattleEffectCommandType::Heal:
            presentation.type = BattlePresentationEventType::HealLog;
            break;
        case BattleEffectCommandType::AddShield:
        case BattleEffectCommandType::AddInvincibility:
        case BattleEffectCommandType::ModifyResource:
        case BattleEffectCommandType::ModifyCooldown:
        case BattleEffectCommandType::DedicatedEffect:
            presentation.type = BattlePresentationEventType::StatusLog;
            break;
        }
        presentationEvents.push_back(std::move(presentation));
    }
}

void applyRuntimeTeamEvents(
    BattleFrameState& state,
    int sourceUnitId,
    const BattleComboFrameRuntimeEvent& event,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    if (state.teamEffects.world.units.empty())
    {
        return;
    }

    BattleTeamEffectSystem system;
    std::vector<BattleTeamEffectEvent> events;
    std::string reason;
    switch (event.type)
    {
    case BattleComboFrameRuntimeEventType::SelfHpRegen:
        events = system.applySelfHeal(state.teamEffects.world, sourceUnitId, event.value);
        reason = "生命回復";
        break;
    case BattleComboFrameRuntimeEventType::HealAura:
        assert(state.teamEffects.healAuraRadius > 0.0);
        events = system.applyHealAura(
            state.teamEffects.world,
            sourceUnitId,
            event.value,
            event.value2,
            state.teamEffects.healAuraRadius,
            event.durationFrames);
        reason = "治療光環";
        break;
    case BattleComboFrameRuntimeEventType::HealPercentSelf:
        events = system.applySelfHeal(state.teamEffects.world, sourceUnitId, event.value);
        reason = "爆發治療";
        break;
    default:
        assert(false);
        break;
    }

    appendTeamEffectPresentationEvents(presentationEvents, events, reason);
    state.teamEffects.committedEvents.insert(
        state.teamEffects.committedEvents.end(),
        events.begin(),
        events.end());
}

void applyBroadcastTriggerTimer(BattleFrameState& state, int sourceUnitId, const BattleComboFrameRuntimeEvent& event)
{
    assert(event.durationFrames > 0);
    const auto* source = findTeamEffectUnit(state.teamEffects.world, sourceUnitId);
    if (!source)
    {
        return;
    }
    for (const auto& unit : state.teamEffects.world.units)
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
    BattleFrameState& state,
    int sourceUnitId,
    const BattleComboFrameRuntimeEvent& event,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    assert(event.value > 0);
    if (state.effects.world.units.empty())
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
        state.effects.world,
        { BattleHook::SkillFinished, sourceUnitId, -1 });
    appendEffectCommandPresentationEvents(presentationEvents, commands);
    state.effects.committedCommands.insert(
        state.effects.committedCommands.end(),
        commands.begin(),
        commands.end());

    if (auto* effectUnit = findEffectUnit(state.effects.world, sourceUnitId))
    {
        if (auto* statusUnit = findStatusUnit(state.status.units, sourceUnitId))
        {
            statusUnit->invincible = effectUnit->invincible;
        }
    }
}

void applyRuntimeComboEvents(BattleFrameState& state, std::vector<BattlePresentationEvent>& presentationEvents)
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
                applyRuntimeTeamEvents(state, result.unitId, event, presentationEvents);
                break;
            case BattleComboFrameRuntimeEventType::BroadcastTriggerTimer:
                applyBroadcastTriggerTimer(state, result.unitId, event);
                break;
            case BattleComboFrameRuntimeEventType::PostSkillInvincibility:
                applyPostSkillInvincibility(state, result.unitId, event, presentationEvents);
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
    BattleFrameState& state,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    if (state.teamEffects.world.units.empty())
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
            if (!findTeamEffectUnit(state.teamEffects.world, sourceUnitId))
            {
                unappliedCommands.push_back(command);
                continue;
            }
            auto application = applyBattleTeamEffectCommand(state.teamEffects.world, command);
            presentationEvents.insert(
                presentationEvents.end(),
                application.presentationEvents.begin(),
                application.presentationEvents.end());
            syncTeamEffectEventsToFrameState(state, application.events);
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

void advanceMovementPhysics(BattleFrameState& state)
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

void advanceMovement(BattleFrameState& state, BattleFrameResult& result)
{
    result.movement = BattleCore(state.world).tickMovement();
}

void advanceActionFrameUnits(
    BattleFrameState& state,
    const BattleTickResult& movement,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    state.actions.unitResults.clear();
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
            presentationEvents.insert(presentationEvents.end(), cast.presentationEvents.begin(), cast.presentationEvents.end());
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
                presentationEvents.insert(
                    presentationEvents.end(),
                    result.actionResult.presentationEvents.begin(),
                    result.actionResult.presentationEvents.end());
            }
        }
        else if (!input.state.haveAction && input.canPlanCast)
        {
            auto cast = BattleCastPlanner().plan(refreshedCastInput(state, movement, input.castInput));
            gameplayEvents.insert(gameplayEvents.end(), cast.gameplayEvents.begin(), cast.gameplayEvents.end());
            presentationEvents.insert(presentationEvents.end(), cast.presentationEvents.begin(), cast.presentationEvents.end());
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
                presentationEvents.insert(
                    presentationEvents.end(),
                    result.actionResult.presentationEvents.begin(),
                    result.actionResult.presentationEvents.end());
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

        state.actions.unitResults.push_back(std::move(result));
    }
    state.actions.units.clear();
}

void advanceAttacksAndResolveHits(
    BattleFrameState& state,
    BattleFrameResult& result,
    std::vector<BattlePresentationEvent>& presentationEvents)
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
    applyProjectileCancelDamageResults(state, result.attackEvents, result.commands);
    resolveHitEvents(state, result.attackEvents, result.commands, presentationEvents);
    reduceFrameGameplayCommands(state, result.commands, presentationEvents);
}

void applyDamageAndLifecycle(
    BattleFrameState& state,
    BattleFrameResult& result,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    if (state.result.ended && state.damage.pendingTransactions.empty())
    {
        return;
    }

    const bool battleEndAlreadyEmitted = state.result.eventEmitted;
    auto application = BattleDamageApplicationSystem().apply(makeFrameDamageApplicationInput(state));

    state.damage.lifecycleEvents = application.lifecycleEvents;
    state.damage.presentationEvents = application.presentationEvents;
    presentationEvents.insert(
        presentationEvents.end(),
        state.damage.presentationEvents.begin(),
        state.damage.presentationEvents.end());
    result.commands.insert(
        result.commands.end(),
        application.commands.begin(),
        application.commands.end());

    for (const auto& transaction : application.transactions)
    {
        applyDamageResultToFrameState(state, transaction);
        applyRescueRepositionForDamage(state, transaction, presentationEvents);
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
    state.deathEffects.world = std::move(application.deathEffects);

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
    BattleFrameState& state,
    BattleFrameResult& result,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattlePresentationEvent>& presentationEvents)
{
    BattlePresentationRecorder recorder;
    recorder.beginFrame(makePresentationSnapshot(state));
    for (auto event : gameplayEvents)
    {
        recorder.recordGameplay(std::move(event));
    }
    for (auto event : presentationEvents)
    {
        recorder.recordPresentation(std::move(event));
    }
    for (const auto& event : result.movement.events)
    {
        recorder.recordPresentation(toPresentationEvent(event));
    }
    for (const auto& event : result.attackEvents)
    {
        recorder.recordGameplay(toGameplayEvent(event, state.attacks));
        for (auto presentation : toPresentationEvents(event, state.attacks))
        {
            recorder.recordPresentation(std::move(presentation));
        }
    }
    result.frame = recorder.consumeFrame();
}
}  // namespace

BattleTeamEffectCommandApplication applyBattleTeamEffectCommand(
    BattleTeamEffectWorld& world,
    const BattleGameplayCommand& command)
{
    BattleTeamEffectCommandApplication result;
    BattleTeamEffectSystem system;
    if (const auto* heal = std::get_if<BattleTeamHealCommand>(&command))
    {
        assert(heal->sourceUnitId >= 0);
        result.events = system.applyTeamHeal(
            world,
            heal->sourceUnitId,
            heal->flatHeal,
            heal->pctHeal);
        appendTeamEffectPresentationEvents(result.presentationEvents, result.events, heal->reason);
        return result;
    }
    if (const auto* mp = std::get_if<BattleTeamMpRestoreCommand>(&command))
    {
        assert(mp->sourceUnitId >= 0);
        result.events = system.applyTeamMp(
            world,
            mp->sourceUnitId,
            mp->amount);
        appendTeamEffectPresentationEvents(result.presentationEvents, result.events, mp->reason);
        return result;
    }
    if (const auto* shield = std::get_if<BattleTeamShieldCommand>(&command))
    {
        assert(shield->sourceUnitId >= 0);
        result.events = system.applyTeamShield(
            world,
            shield->sourceUnitId,
            shield->amount,
            shield->refreshOnly);
        appendTeamEffectPresentationEvents(result.presentationEvents, result.events, shield->reason);
        return result;
    }
    assert(false);
    return result;
}

BattleCore::BattleCore(BattleWorldState& world)
    : world_(world)
{
}

BattleTickResult BattleCore::tickMovement()
{
    return BattleMovementPlanner(world_).tick();
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

BattleFrameResult BattleFrameRunner::advanceFrame(BattleFrameState& state) const
{
    assertFrameMovementConfigConfigured(state.world.config);
    assertFrameAttackWorldConfigured(state.attacks);

    BattleFrameResult result;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattlePresentationEvent> presentationEvents;

    state.teamEffects.committedEvents.clear();
    state.effects.committedCommands.clear();
    state.hits.committedResults.clear();
    state.rescue.committedResults.clear();
    state.applications = {};
    advanceStatus(state);
    advanceRuntimeUnits(state, result.commands);
    applyRuntimeComboEvents(state, presentationEvents);
    applyPendingTeamEffects(state, presentationEvents);
    reduceFrameGameplayCommands(state, result.commands, presentationEvents);
    advanceMovementPhysics(state);
    advanceMovement(state, result);
    advanceActionFrameUnits(state, result.movement, gameplayEvents, presentationEvents);
    advanceAttacksAndResolveHits(state, result, presentationEvents);
    applyDamageAndLifecycle(state, result, gameplayEvents, presentationEvents);
    reduceFrameGameplayCommands(state, result.commands, presentationEvents);
    emitPresentationFrame(state, result, gameplayEvents, presentationEvents);
    return result;
}

}  // namespace KysChess::Battle
