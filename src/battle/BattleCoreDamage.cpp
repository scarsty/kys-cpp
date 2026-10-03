#include "BattleCoreDetail.h"
#include "../ChessBattleEffectSemantics.h"
#include "../ChessEftIds.h"
#include "../Find.h"
#include "BattleAreaEffectSystem.h"
#include "BattleEffectEventBridge.h"
#include "BattleFrameContext.h"
#include "BattleLogSegments.h"
#include "BattleMath.h"
#include "BattleMovementPhysics.h"
#include "BattleProjectileEvents.h"
#include "BattlePresentationVisuals.h"
#include "BattleResourceRules.h"
#include "BattleRuntimeEffects.h"
#include "BattleStatusSystem.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <memory_resource>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>



namespace KysChess::Battle
{

namespace
{

struct AreaProjectilePresentation
{
    int effectId{};
    std::string_view reason;
};

BattleHealModifierState statusHealModifiers(
    const BattleRuntimeUnitRecord& record,
    BattleHealKind kind)
{
    const auto status = BattleStatusSystem({}).persistentModifiers(record.status.effects);
    return battleStatusHealModifiers(status, kind);
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

std::string formatStatusValue(const std::string& label, int value, const char* unit)
{
    if (value <= 0)
    {
        return label;
    }
    return std::format("{}（{}{}）", label, value, unit);
}

int rescueSnapshotUnitId(const BattleFrameRescueUnitSnapshot& snapshot)
{
    return snapshot.unit.id;
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
        snapshot.unit.healModifiers = statusHealModifiers(unit, BattleHealKind::Rescue);
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
        if (battleDistance2d(unit.position, target.position) <= state.rescue.executeUnattendedRadius)
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

    BattleAttackSpawnRequest request{ BattleAttackPayload(
        BattleAttackDelivery::contact(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
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
            visualEvents.push_back(CoreDetail::roleEffectEvent(
                pulled.id,
                KysChess::EFT_HEAL,
                CoreDetail::CoreRoleStatusEffectFrames));
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

BattlePresentationColor statusTickDamageTextColor(BattleDamageKind kind)
{
    switch (kind)
    {
    case BattleDamageKind::Poison:
        return { 0, 200, 0, 255 };
    case BattleDamageKind::Bleed:
        return { 190, 120, 60, 255 };
    default:
        assert(false);
        return {};
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


std::vector<int> effectDamageTargetIds(
    const BattleRuntimeState& state,
    const BattleEffectDamageRequestOutput& output)
{
    if (output.delivery.targetUnitIds) return *output.delivery.targetUnitIds;
    const auto& center = state.units.requireCore(output.request.defenderUnitId);
    if (output.delivery.area.kind == DamageAreaKind::SingleTarget)
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
        switch (output.delivery.area.kind)
        {
        case DamageAreaKind::SingleTarget:
            inside = candidate.id == center.id;
            break;
        case DamageAreaKind::Circle:
        {
            const double radius = output.delivery.area.radiusTiles * state.gridTransform.tileWidth;
            inside = battlePointSegmentWithinRadius(
                candidate.motion.position,
                center.motion.position,
                center.motion.position,
                radius);
            break;
        }
        case DamageAreaKind::Square:
        {
            const int halfSide = output.delivery.area.squareSideTiles / 2;
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
    if (output.delivery.displayedSourceMaxHpPercent)
    {
        const int percent = *output.delivery.displayedSourceMaxHpPercent;
        return stunFrames > 0
            ? std::format("{}{}%（{}幀）", reason, percent, stunFrames)
            : std::format("{}{}%", reason, percent);
    }
    return stunFrames > 0
        ? std::format("{}（{}傷害，{}幀）", reason, output.request.baseDamage, stunFrames)
        : std::format("{}（{}傷害）", reason, output.request.baseDamage);
}


void appendAreaProjectileDamageOutput(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleEffectDamageRequestOutput& output,
    const EffectExecutionInputs& context)
{
    assert(output.delivery.areaProjectiles);
    assert(output.transactionCount > 0);
    if (output.request.baseDamage <= 0)
    {
        return;
    }

    const auto& delivery = *output.delivery.areaProjectiles;
    const auto presentation = areaProjectilePresentation(delivery.visual);
    for (int transaction = 0; transaction < output.transactionCount; ++transaction)
    {
        BattleAreaProjectileFollowUp followUp;
        if (context.cast && context.retainCastUntilDamageDescendants
            && context.cast->sourceUnitId == output.request.attackerUnitId)
        {
            assert(context.cast->valid());
            followUp.cast = *context.cast;
            followUp.sourceAttack = output.triggeringAttack;
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
        followUp.damagePct = output.delivery.projectileSourceMaxHpPercent;
        followUp.damageKind = output.request.damageKind;
        followUp.appliesDamageModifiers = !output.request.preResolvedDamage;
        followUp.triggersDefenseEffects = output.request.triggersDefenseEffects;
        followUp.stunFrames = delivery.stunFrames;
        followUp.reason = presentation.reason;
        followUp.logText = areaProjectileLogText(
            output,
            presentation.reason,
            delivery.stunFrames);
        frame.mutableAreaProjectileFollowUps().push_back(std::move(followUp));
    }
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

    const auto bleed = BattleStatusSystem({}).snapshot(transaction.defenderStatus);
    const int currentStacks = std::max(
        event.value,
        bleed.stacks(BattleStatusKind::Bleed));
    const int maxStacks = std::max(
        currentStacks,
        bleed.targetTotalCapacity(BattleStatusKind::Bleed).value_or(event.maxValue));
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
            frame.visualEvents.push_back(CoreDetail::roleEffectEvent(
                event.targetUnitId,
                KysChess::EFT_HEAL,
                CoreDetail::CoreRoleStatusEffectFrames));
            CoreDetail::appendHealEventLog(
                frame.logEvents,
                event.sourceUnitId,
                event.targetUnitId,
                event.value,
                "命中回血");
            break;
        case BattleDamageEventType::CooldownExtended:
            CoreDetail::appendStatusEventLog(
                frame.logEvents,
                event.sourceUnitId,
                event.targetUnitId,
                formatAppliedCooldownExtension(event),
                BattleStatusSemanticId::None,
                BattleResourceSemanticId::Cooldown,
                event.value);
            break;
        case BattleDamageEventType::MpRestored:
            CoreDetail::appendHealEventLog(
                frame.logEvents,
                event.sourceUnitId,
                event.targetUnitId,
                event.value,
                "回復內力",
                BattleResourceSemanticId::MagicPoints);
            break;
        case BattleDamageEventType::MpDrained:
            CoreDetail::appendStatusEventLog(
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
                     .channel = CoreDetail::effectDamageChannel(request.damageKind),
                     .stage = stage,
                     .frame = state.movement.frame,
                 }))
        {
            const int amount = battleSaturatedMultiply(
                modifier.amount,
                modifier.stackCount);
            switch (modifier.operation)
            {
            case DamageModifierOperation::FlatAdd:
                if (perspective == DamageModifierPerspective::Outgoing)
                {
                    result.flatDamageIncrease = battleSaturatedAdd(
                        result.flatDamageIncrease,
                        amount);
                }
                else
                {
                    result.flatDamageReduction = battleSaturatedInt(
                        static_cast<std::int64_t>(result.flatDamageReduction)
                            - amount);
                }
                break;
            case DamageModifierOperation::PercentAdd:
            case DamageModifierOperation::Multiply:
            {
                const int percentDelta = modifier.operation
                        == DamageModifierOperation::Multiply
                    ? battleSaturatedAdd(amount, -100)
                    : amount;
                if (perspective == DamageModifierPerspective::Outgoing)
                {
                    result.skillDamagePct = battleSaturatedAdd(
                        result.skillDamagePct,
                        percentDelta);
                }
                else if (percentDelta < 0)
                {
                    result.damageReductionPct = battleSaturatedInt(
                        static_cast<std::int64_t>(result.damageReductionPct)
                            - percentDelta);
                }
                else
                {
                    result.damageTakenIncreasePct = battleSaturatedAdd(
                        result.damageTakenIncreasePct,
                        percentDelta);
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
        CoreDetail::mpRecoveryBonusPct(state, defender.id()));
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
            CoreDetail::mpRecoveryBonusPct(state, attacker.id()));
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
        transaction.attackerHealModifiers = statusHealModifiers(
            attacker,
            BattleHealKind::OnHit);
    }
    else
    {
        transaction.attacker.id = OptionalDamageAttackerUnitId;
    }

    auto damageKind = request.damageKind;
    if (damageKind == BattleDamageKind::Physical)
    {
        if (request.usingSkill)
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
    const BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleDamagePresentationInput& presentation,
    const BattleDamageTransactionResult& transaction,
    const EffectDamageOrigin& origin)
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
    std::visit([&](const auto& effect)
    {
        if constexpr (requires { effect.binding; })
        {
            damageLog = CoreDetail::makeEffectLogEvent(state, effect.binding,
                transaction.defender.id, state.movement.frame);
            damageLog.skillName = damageLog.semanticSourceName;
            if (effect.binding.kind == EffectSourceKind::Magic
                && transaction.damageKind != BattleDamageKind::Poison
                && transaction.damageKind != BattleDamageKind::Bleed)
                damageLog.skillId = effect.binding.sourceId;
        }
    }, origin);
    damageLog.type = BattleLogEventType::Damage;
    damageLog.sourceUnitId = transaction.attacker.id;
    damageLog.targetUnitId = transaction.defender.id;
    damageLog.amount = hpDamage;
    if (!presentation.skillName.empty()) damageLog.skillName = presentation.skillName;
    if (presentation.skillId >= 0) damageLog.skillId = presentation.skillId;
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
        frame.visualEvents.push_back(CoreDetail::roleEffectEvent(
            transaction.defender.id,
            KysChess::EFT_BLOCK,
            CoreDetail::CoreRoleStatusEffectFrames));
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

        CoreDetail::appendAreaLifecycleLogs(state, frame.logEvents,
            BattleAreaEffectSystem::removeForSourceDeath(state.areas, event.targetUnitId), state.movement.frame);
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
    CoreDetail::reserveTrackedProjectileFollowUps(state, projectileExpansion);
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
            assert(request.initial.attackSourceUnitId == followUp.cast.sourceUnitId);

            BattleAttackReservationRequest reservationRequest;
            if (followUp.sourceAttack)
            {
                reservationRequest.parentAttackId = followUp.sourceAttack->attackId;
                reservationRequest.sharedHitGroupId =
                    followUp.sourceAttack->sharedHitGroupId;
                reservationRequest.propagation = CoreDetail::derivedAttackPropagation(
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
        if (!candidateRecord.comboFacts.isMember(comboId))
        {
            continue;
        }
        if (candidateRecord.comboFacts.hasApplied(comboId))
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
    for (int comboId : deadRecord.comboFacts.appliedComboIds())
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
                comboId,
                state.movement.frame,
                &targetRecord.status.effects.nextNegativeEffectSequence);
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
            case BattleAttribute::GuaranteedHit:
            case BattleAttribute::CriticalChance:
            case BattleAttribute::CriticalDamage:
            case BattleAttribute::DodgeChance:
            case BattleAttribute::BlockChance:
            case BattleAttribute::DamageReduction:
            case BattleAttribute::SkillDamage:
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
        frame.queueEffectCommands(std::move(antiComboTransfer.commands));
        state.effectRules.appendAntiComboTransferredRules(
            deadUnitId,
            target->id,
            target->team,
            comboId);
        targetRecord.comboFacts.addApplied(comboId);
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

std::string formatExecuteStatus(int thresholdPct)
{
    if (thresholdPct <= 0)
    {
        return "觸發處決";
    }
    return std::format("觸發處決（斬殺線{}%）", thresholdPct);
}

void appendExecuteStatusLog(
    BattleFrameContext& frame,
    int attackerUnitId,
    int defenderUnitId,
    int thresholdPct)
{
    CoreDetail::appendStatusEventLog(
        frame.logEvents,
        attackerUnitId,
        defenderUnitId,
        formatExecuteStatus(thresholdPct));
    frame.logEvents.back().statusId = BattleStatusSemanticId::ExecuteTriggered;
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
    appendExecuteStatusLog(
        frame,
        request.attackerUnitId,
        request.defenderUnitId,
        intent.executeThresholdPct);
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
        frame.visualEvents.push_back(CoreDetail::roleEffectEvent(
            request.defenderUnitId,
            KysChess::EFT_BLOCK,
            CoreDetail::CoreRoleStatusEffectFrames));
        CoreDetail::appendStatusEventLog(
            frame.logEvents,
            request.defenderUnitId,
            request.attackerUnitId,
            "格擋後釋放絕招");
        frame.queueCommand(BattleAutoUltimateCommand{ request.defenderUnitId, false });
    }
    if (block)
    {
        frame.visualEvents.push_back(CoreDetail::roleEffectEvent(
            request.defenderUnitId,
            KysChess::EFT_BLOCK,
            CoreDetail::CoreRoleStatusEffectFrames));
        CoreDetail::appendStatusEventLog(
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
        && (request.attackerUnitId == OptionalDamageAttackerUnitId
            || effectAdjustedAttribute(state, request.attackerUnitId, BattleAttribute::GuaranteedHit, 0, request.defenderUnitId) <= 0)
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
            CoreDetail::nextEffectEventHeader(state, ownerUnitId),
            transaction,
            intent.effectOrigin,
            input);

        frame.queueEffectCommands(std::move(dispatched.commands));
        CoreDetail::reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
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
            resolved.result.request.cast = attack->provenance.cast;
        }
        healSystem.recordResolved(state, std::move(resolved));
        CoreDetail::reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
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
            CoreDetail::nextEffectEventHeader(state, transaction.defender.id),
            EffectEvent::ShieldBroken,
            std::move(payload));
        frame.queueEffectCommands(std::move(dispatched.commands));
        CoreDetail::reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
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
        CoreDetail::nextEffectEventHeader(state, transaction.defender.id),
        EffectEvent::UnitDied,
        death);
    frame.queueEffectCommands(std::move(dispatched.commands));
    CoreDetail::reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());

    auto deathDamageAbsorptions =
        BattleEffectCommandSystem::removeDamageAbsorptionsForSourceDeath(
            state,
            transaction.defender.id);
    CoreDetail::appendDamageAbsorptionSettlements(
        state,
        frame,
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
            CoreDetail::nextEffectEventHeader(state, ally.id()),
            EffectEvent::AllyDied,
            std::move(allyDeath));
        frame.queueEffectCommands(std::move(allyDispatched.commands));
        CoreDetail::reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    }
}






}  // namespace

namespace CoreDetail
{

void appendFramePendingDamage(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    BattleDamageRequest request,
    std::optional<BattleDamagePresentationInput> presentation,
    int executeThresholdPct,
    bool canTriggerDefenderBlock,
    BattleAttackProvenance provenance,
    std::optional<EffectDamageOrigin> effectOrigin,
    CastWorkToken delayedCastWork)
{
    assert(request.defenderUnitId >= 0);
    assert(!provenance.valid() || request.attackerUnitId == provenance.cast.sourceUnitId);

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

void appendDamageAbsorptionSettlements(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
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
        auto log = makeEffectLogEvent(state, absorption.binding, absorption.targetUnitId, settlementFrame);
        log.statusId = BattleStatusSemanticId::DamageAbsorptionEnded;
        log.amount = battleSaturatedInt(absorption.accumulatedDamage);
        log.secondaryAmount = damage;
        log.segments = battleLogText(std::format("傷害吸收結束（{}；累積{}，待返還{}）",
            state.units.requireCore(absorption.binding.ownerUnitId).alive ? "到期" : "來源陣亡",
            absorption.accumulatedDamage, damage));
        frame.logEvents.push_back(std::move(log));
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
        context.scope.binding = absorption.binding;
        const auto targets = BattleEffectSystem::selectTargets(
            absorption.settlementTarget,
            context,
            state.random);
        EffectCommandMetadata metadata{
            .binding = absorption.binding,
            .ruleId = absorption.ruleId,
            .authoredActionOrder = absorption.authoredActionOrder,
            .targetUnitId = absorption.targetUnitId,
            .statusContribution = absorption.statusContribution,
        };
        DealDamageEffectCommand command;
        command.amount = damage;
        command.kind = absorption.settlementDamageKind;
        command.delivery.statusTickPresentation = false;
        command.delivery.targetUnitIds = targets;
        const EffectExecutionInputs inputs{
            .frame = settlementFrame,
            .cast = absorption.triggeringCast,
            .attack = absorption.triggeringAttack,
            .retainCastUntilDamageDescendants = false,
        };
        const auto output = BattleEffectCommandSystem::prepareDamageOutput(metadata, command, inputs);
        appendEffectDamageOutput(state, frame, pendingDamage, output, inputs);
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
        const auto redirect = request.redirected ? std::nullopt
            : BattleAreaEffectSystem::damageRedirect(state.areas, state.gridTransform,
                state.units, request.defenderUnitId, state.movement.frame);
        auto transactionInput = makeFrameDamageTransactionInput(state, request);
        transactionInput.redirectHpDamage = redirect.has_value();
        if (request.redirected) transactionInput.liveOutgoingDamagePctDelta = 0;
        auto transaction = BattleDamageSystem().resolveTransaction(transactionInput, &state.talentRandom);
        if (transaction.executed && !presentation.executed)
        {
            presentation.executed = true;
            appendDamagePresentationDetail(presentation, "處決");
            applyFrameDamagePresentationStyle(state, transaction.defender.id, presentation);
            appendExecuteStatusLog(
                frame,
                transaction.attacker.id,
                transaction.defender.id,
                0);
        }
        if (transaction.redirectedHpDamage > 0)
        {
            BattleDamageRequest redirected;
            redirected.attackerUnitId = request.attackerUnitId;
            redirected.defenderUnitId = redirect->guardianUnitId;
            redirected.baseDamage = static_cast<int>(
                static_cast<std::int64_t>(transaction.redirectedHpDamage)
                * (100 - redirect->reductionPct) / 100);
            redirected.damageKind = request.damageKind;
            redirected.preResolvedDamage = true;
            redirected.redirected = true;
            // 承傷沿用攻擊者記帳，但不是另一次攻擊，不重複觸發命中規則。
            appendFramePendingDamage(state, pendingDamage, redirected, presentation,
                0, false, {}, EffectEnvironmentDamageOrigin{});
            auto guardCue = roleEffectEvent(redirect->guardianUnitId, -1, 12);
            guardCue.visualPath = BattleCueGuardianVisualPath;
            guardCue.roleEffectType = BattleRoleEffectType::GuardianCue;
            frame.visualEvents.push_back(std::move(guardCue));
            appendStatusEventLog(frame.logEvents, redirect->guardianUnitId,
                request.defenderUnitId, "護衛承傷");
        }
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
        if (transaction.defenseStatusConsumed)
        {
            appendStatusConsumptionLog(state, frame.logEvents,
                *transaction.defenseStatusConsumed, state.movement.frame);
        }
        if (transaction.recoveryTested)
        {
            appendStatusEventLog(frame.logEvents, transaction.defender.id, transaction.defender.id,
                transaction.recoverySucceeded ? "賭運判定成功，免死並嘗試自動絕招" : "賭運判定失敗");
            frame.logEvents.back().statusId = transaction.recoverySucceeded
                ? BattleStatusSemanticId::LethalRecoverySucceeded : BattleStatusSemanticId::LethalRecoveryFailed;
            frame.logEvents.back().semanticSourceKind = "talent";
            frame.logEvents.back().semanticSourceName = "賭運";
            if (transaction.recoverySucceeded)
                frame.queueCommand(BattleAutoUltimateCommand{transaction.defender.id, false, true, true});
        }
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
        appendFrameDamageOutputEvents(state, frame, presentation, transaction, intent.effectOrigin);
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

void appendStatusEventLog(
    std::vector<BattleLogEvent>& logEvents,
    int sourceUnitId,
    int targetUnitId,
    std::string text,
    BattleStatusSemanticId statusId,
    BattleResourceSemanticId resourceId,
    int amount)
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
        assert(request.initial.attackSourceUnitId == sourceAttack.cast.sourceUnitId);
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
    request.preResolvedDamage = command.preResolvedDamage;
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

    // 受擊反制可作用於原攻擊者；保留觸發來源，但不把非傷害副作用計入命中傷害。
    assert(request.baseDamage == 0 && request.mpDamage == 0);
    appendFramePendingDamage(
        state,
        pendingDamage,
        std::move(request),
        std::nullopt,
        false,
        false,
        {},
        EffectAttackDamageOrigin{ command.provenance });
    return true;
}

void appendHealEventLog(
    std::vector<BattleLogEvent>& logEvents,
    int sourceUnitId,
    int targetUnitId,
    int amount,
    std::string text,
    BattleResourceSemanticId resourceId)
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

void appendEffectDamageOutput(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleEffectDamageRequestOutput& output,
    const EffectExecutionInputs& context)
{
    if (output.delivery.areaProjectiles)
    {
        appendAreaProjectileDamageOutput(state, frame, output, context);
        return;
    }
    for (int targetUnitId : effectDamageTargetIds(state, output))
    {
        if (context.cast && output.delivery.perCast.perTargetLimit > 0)
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
            const auto provenance = output.hitDamageCredit
                && output.hitDamageCredit->targetUnitId == targetUnitId
                ? output.hitDamageCredit->provenance
                : BattleAttackProvenance{};
            std::optional<BattleDamagePresentationInput> presentation;
            auto damageOrigin = BattleEffectCommandSystem::damageOrigin(output);
            if (output.statusContribution && output.delivery.statusTickPresentation)
            {
                if (output.request.damageKind == BattleDamageKind::Poison
                    || output.request.damageKind == BattleDamageKind::Bleed)
                {
                    BattleDamagePresentationInput statusPresentation;
                    statusPresentation.segments = battleLogText(
                        output.request.damageKind == BattleDamageKind::Poison
                            ? "中毒"
                            : "流血",
                        BattleLogTextTone::SkillName);
                    applyStatusTickDamagePresentation(
                        state,
                        output.request.damageKind,
                        targetUnitId,
                        statusPresentation);
                    presentation = std::move(statusPresentation);
                }
            }
            appendFramePendingDamage(
                state,
                pendingDamage,
                std::move(request),
                std::move(presentation),
                false,
                false,
                provenance,
                std::move(damageOrigin),
                reserveEffectDamageDescendantWork(state, context, provenance));
        }
    }
}

void appendPoisonEffectLogEvents(
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    const ApplyStatusEffectCommand& command,
    const BattleStatusApplyEffectResult& result)
{
    if (command.status != BattleStatusKind::Poison)
    {
        return;
    }

    BattleLogEvent payload;
    payload.type = BattleLogEventType::Status;
    payload.sourceUnitId = metadata.binding.ownerUnitId;
    payload.targetUnitId = metadata.targetUnitId;
    const int damagePercent = poisonDamagePercent(command.behavior);
    payload.amount = damagePercent;
    const int triggerCount = command.stacks;
    payload.secondaryAmount = triggerCount;
    payload.statusId = BattleStatusSemanticId::PoisonPayload;
    payload.segments = battleLogText(
        std::format("中毒負載{}%（預定{}次）", damagePercent, triggerCount),
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
    applied.amount = damagePercent;
    applied.statusId = BattleStatusSemanticId::Poison;
    applied.segments = battleLogText(
        std::format("中毒{}%", damagePercent),
        BattleLogTextTone::Negative);
    appendDurationFramesSuffix(
        applied.segments,
        result.status.appliedDurationFrames,
        BattleLogTextTone::Negative);
    logEvents.push_back(std::move(applied));
}

void applyStatusTickDamagePresentation(
    const BattleRuntimeState& state,
    BattleDamageKind kind,
    int targetUnitId,
    BattleDamagePresentationInput& presentation)
{
    applyFrameDamagePresentationStyle(state, targetUnitId, presentation);
    presentation.enabled = true;
    presentation.normalDamageColor = statusTickDamageTextColor(kind);
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
        damage.lethalRecovery = runtime->lethalRecovery;
    }
    return damage;
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

CastWorkToken reserveEffectDamageDescendantWork(
    BattleRuntimeState& state,
    const EffectExecutionInputs& context,
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

void writeBattleDamageRuntimeUnitImpl(BattleDamageRuntimeUnit& runtime, const BattleDamageUnitState& unit)
{
    runtime.hurtInvincFrames = unit.hurtInvincFrames;
    runtime.dualWieldBlocksRemaining = unit.dualWieldBlocksRemaining;
    runtime.deathPrevention = unit.deathPrevention;
    runtime.deathPreventionUsed = unit.deathPreventionUsed;
    runtime.deathPreventionFrames = unit.deathPreventionFrames;
    runtime.lethalRecovery = unit.lethalRecovery;
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
    case BattleDamageKind::Pure:
    case BattleDamageKind::Effect:
    case BattleDamageKind::Execute:
        return DamageChannel::Effect;
    }
    assert(false);
    return DamageChannel::All;
}

}  // namespace CoreDetail

}  // namespace KysChess::Battle
