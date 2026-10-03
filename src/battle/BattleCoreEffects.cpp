#include "BattleCoreDetail.h"
#include "../ChessBattleEffectSemantics.h"
#include "../ChessEftIds.h"
#include "BattleAreaEffectSystem.h"
#include "BattleEffectAttackCastSystem.h"
#include "BattleEffectEventBridge.h"
#include "BattleFrameContext.h"
#include "BattleLogSegments.h"
#include "BattlePresentationVisuals.h"
#include "BattleProjectileEvents.h"
#include "BattleResourceRules.h"
#include "BattleRuntimeEffects.h"
#include "BattleStatusSystem.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <memory_resource>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>



namespace KysChess::Battle
{

namespace
{

bool isMpBlocked(BattleRuntimeState& state, int unitId)
{
    return state.units.require(unitId).mpBlocked();
}

template<class... Visitors>
struct Overloaded : Visitors...
{
    using Visitors::operator()...;
};

struct BattleCommandSinks
{
    std::vector<BattleAttackSpawnRequest>& attackSpawns;
    std::vector<BattlePendingDamageIntent>& pendingDamage;
    std::vector<BattleGameplayEvent>& gameplayEvents;
    std::vector<BattleLogEvent>& logEvents;
    std::vector<BattleVisualEvent>& visualEvents;
};

int adjustedRuntimeMpRestore(BattleRuntimeState& state, int unitId, int amount)
{
    return adjustedMpRestore(
        isMpBlocked(state, unitId),
        CoreDetail::mpRecoveryBonusPct(state, unitId),
        amount);
}

std::string_view effectSourceKindName(EffectSourceKind kind)
{
    switch (kind)
    {
    case EffectSourceKind::Combo: return "combo";
    case EffectSourceKind::Equipment: return "equipment";
    case EffectSourceKind::EquipmentSynergy: return "equipment_synergy";
    case EffectSourceKind::Neigong: return "neigong";
    case EffectSourceKind::Magic: return "magic";
    }
    assert(false);
    return {};
}

BattleLogEvent makeEffectLogEvent(
    const BattleRuntimeState& state,
    const EffectCommandMetadata& metadata,
    int targetUnitId,
    int frame)
{
    return CoreDetail::makeEffectLogEvent(state, metadata.binding, targetUnitId, frame);
}

BattleStatusSemanticId statusSemanticId(BattleStatusKind status)
{
    switch (status)
    {
    case BattleStatusKind::Stun: return BattleStatusSemanticId::Stun;
    case BattleStatusKind::Poison: return BattleStatusSemanticId::Poison;
    case BattleStatusKind::Bleed: return BattleStatusSemanticId::Bleed;
    case BattleStatusKind::MpBlocked: return BattleStatusSemanticId::MpBlocked;
    case BattleStatusKind::TrueQi: return BattleStatusSemanticId::TrueQi;
    case BattleStatusKind::BattleSpirit: return BattleStatusSemanticId::BattleSpirit;
    case BattleStatusKind::ColdPoison:
    case BattleStatusKind::WitheredBone:
    case BattleStatusKind::SevenStarMark:
    case BattleStatusKind::NeutralizeForce:
    case BattleStatusKind::Blinded:
    case BattleStatusKind::NextAttackMiss:
    case BattleStatusKind::DamageBlockLayer:
    case BattleStatusKind::SingleHitCapLayer:
    case BattleStatusKind::PoisonExplosion:
    case BattleStatusKind::Shadowless:
    case BattleStatusKind::SwordGuard:
    case BattleStatusKind::NextAttackCritical:
    case BattleStatusKind::Berserk:
        return BattleStatusSemanticId::None;
    case BattleStatusKind::Count:
        break;
    }
    assert(false);
    return BattleStatusSemanticId::None;
}

BattleResourceSemanticId resourceSemanticId(BattleResource resource)
{
    switch (resource)
    {
    case BattleResource::Shield: return BattleResourceSemanticId::Shield;
    case BattleResource::StatusShield: return BattleResourceSemanticId::StatusShield;
    case BattleResource::StaggerShield: return BattleResourceSemanticId::StaggerShield;
    case BattleResource::ActiveCooldown: return BattleResourceSemanticId::Cooldown;
    case BattleResource::ControlImmunityFrames:
        return BattleResourceSemanticId::ControlImmunity;
    case BattleResource::InvincibilityFrames:
        return BattleResourceSemanticId::Invincibility;
    case BattleResource::Hp:
    case BattleResource::Mp:
        return BattleResourceSemanticId::None;
    }
    assert(false);
    return BattleResourceSemanticId::None;
}

std::string_view resourceLabel(BattleResource resource)
{
    switch (resource)
    {
    case BattleResource::Shield: return "護盾";
    case BattleResource::StatusShield: return "狀態護盾";
    case BattleResource::StaggerShield: return "硬直護盾";
    case BattleResource::ActiveCooldown: return "冷卻";
    case BattleResource::ControlImmunityFrames: return "控場免疫";
    case BattleResource::InvincibilityFrames: return "無敵";
    case BattleResource::Hp:
    case BattleResource::Mp:
        return {};
    }
    assert(false);
    return {};
}

std::string_view attributeLabel(BattleAttribute attribute)
{
    switch (attribute)
    {
    case BattleAttribute::MaxHp: return "最大生命";
    case BattleAttribute::Attack: return "攻擊";
    case BattleAttribute::Defence: return "防禦";
    case BattleAttribute::Speed: return "速度";
    case BattleAttribute::CriticalChance: return "暴擊率";
    case BattleAttribute::CriticalDamage: return "暴擊傷害";
    case BattleAttribute::DodgeChance: return "閃避率";
    case BattleAttribute::BlockChance: return "格擋率";
    case BattleAttribute::DamageReduction: return "減傷";
    case BattleAttribute::SkillDamage: return "技能傷害";
    case BattleAttribute::CooldownReduction: return "冷卻減少";
    case BattleAttribute::MpRecoveryBonus: return "回內加成";
    case BattleAttribute::StaggerResistance: return "硬直抗性";
    case BattleAttribute::ProjectileReflectChance: return "彈道反彈率";
    case BattleAttribute::SkillReflectPercent: return "技能反彈率";
    case BattleAttribute::CounterUltimateBlockChance: return "反絕招格擋率";
    case BattleAttribute::CriticalAfterDodge: return "閃避後暴擊率";
    case BattleAttribute::DashChance: return "突進率";
    case BattleAttribute::OutgoingCooldownExtensionChance: return "出招冷卻延長率";
    case BattleAttribute::OutgoingCooldownExtensionPercent: return "出招冷卻延長";
    case BattleAttribute::IncomingCooldownExtensionChance: return "受擊冷卻延長率";
    case BattleAttribute::IncomingCooldownExtensionPercent: return "受擊冷卻延長";
    case BattleAttribute::GuaranteedHit: return "必中";
    }
    assert(false);
    return {};
}

int stackedModifierAmount(int amount, int stackCount)
{
    return static_cast<int>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(amount) * stackCount,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()));
}

std::string attributeModifierText(
    BattleAttribute attribute,
    AttributeOperation operation,
    int amount)
{
    switch (operation)
    {
    case AttributeOperation::FlatAdd:
        return std::format("{}數值{:+}", attributeLabel(attribute), amount);
    case AttributeOperation::PercentAdd:
        return std::format("{}百分比{:+}%", attributeLabel(attribute), amount);
    case AttributeOperation::PercentagePointAdd:
        return std::format("{}百分點{:+}", attributeLabel(attribute), amount);
    case AttributeOperation::Override:
        return std::format("{}設為{}", attributeLabel(attribute), amount);
    case AttributeOperation::Multiply:
        return std::format("{}倍率{}%", attributeLabel(attribute), amount);
    case AttributeOperation::AtLeast:
        return std::format("{}至少{}", attributeLabel(attribute), amount);
    }
    assert(false);
    return {};
}

std::string damageModifierText(
    DamageModifierPerspective perspective,
    DamageModifierOperation operation,
    int amount)
{
    const std::string_view subject = perspective == DamageModifierPerspective::Outgoing
        ? "輸出傷害"
        : "承受傷害";
    switch (operation)
    {
    case DamageModifierOperation::FlatAdd:
        return std::format("{}{:+}", subject, amount);
    case DamageModifierOperation::PercentAdd:
        return std::format("{}百分比{:+}%", subject, amount);
    case DamageModifierOperation::Multiply:
        return std::format("{}倍率{}%", subject, amount);
    case DamageModifierOperation::IgnoreDefensePercent:
        return std::format("{}忽略防禦{:+}%", subject, amount);
    case DamageModifierOperation::CapSingleHitAtMaxHpPercent:
        return std::format("{}上限{}%最大生命", subject, amount);
    case DamageModifierOperation::CapSingleHitAtValue:
        return std::format("{}上限{}", subject, amount);
    case DamageModifierOperation::ExecuteBelowMaxHpPercent:
        return std::format("{}低於{}%最大生命時處決", subject, amount);
    }
    assert(false);
    return {};
}

bool damageModifierIsNegative(
    DamageModifierPerspective perspective,
    DamageModifierOperation operation,
    int amount)
{
    const bool outgoing = perspective == DamageModifierPerspective::Outgoing;
    switch (operation)
    {
    case DamageModifierOperation::FlatAdd:
    case DamageModifierOperation::PercentAdd:
    case DamageModifierOperation::IgnoreDefensePercent:
        return outgoing ? amount < 0 : amount > 0;
    case DamageModifierOperation::Multiply:
        return outgoing ? amount < 100 : amount > 100;
    case DamageModifierOperation::CapSingleHitAtMaxHpPercent:
        return outgoing;
    case DamageModifierOperation::ExecuteBelowMaxHpPercent:
        return !outgoing;
    case DamageModifierOperation::CapSingleHitAtValue:
        return outgoing;
    }
    assert(false);
    return false;
}

std::vector<BattleLogTextSegment> statusLogSegments(
    BattleStatusKind status,
    int durationFrames,
    int stackCount)
{
    if (durationFrames > 0)
    {
        return logSegments<BattleLogTextTone::SkillName>(
            battleStatusLabel(status),
            "（",
            std::pair{ BattleLogTextTone::DurationValue, durationFrames },
            std::pair{ BattleLogTextTone::DurationValue, "幀" },
            "）");
    }

    const auto counter = statusQuantityCounter(statusCatalogEntry(status).quantity);
    if (stackCount > 0 && !counter.empty())
    {
        return logSegments<BattleLogTextTone::SkillName>(
            battleStatusLabel(status),
            "（",
            std::pair{ BattleLogTextTone::ResourceValue, stackCount },
            std::pair{ BattleLogTextTone::ResourceValue, counter },
            "）");
    }
    return battleLogText(std::string(battleStatusLabel(status)), BattleLogTextTone::SkillName);
}

BattleVisualEvent semanticCueEvent(const BattleSemanticCueRequest& cue)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::RoleEffect;
    event.roleEffectType = BattleRoleEffectType::StatusCue;
    event.targetUnitId = cue.targetUnitId;
    event.durationFrames = 15;
    switch (cue.family)
    {
    case BattleSemanticCueFamily::SwordIntent:
        event.visualPath = BattleCueSwordVisualPath;
        event.color = {160, 225, 255, 255};
        event.durationFrames = 12;
        break;
    case BattleSemanticCueFamily::Positive:
        event.visualPath = BattleCuePositiveVisualPath;
        event.color = { 255, 204, 96, 220 };
        break;
    case BattleSemanticCueFamily::Protection:
        event.visualPath = BattleCuePositiveVisualPath;
        event.color = { 112, 224, 255, 210 };
        break;
    case BattleSemanticCueFamily::Poison:
        event.visualPath = BattleCueNegativeVisualPath;
        event.color = { 136, 220, 96, 170 };
        break;
    case BattleSemanticCueFamily::Bleed:
        event.visualPath = BattleCueBleedVisualPath;
        event.color = { 255, 94, 86, 220 };
        break;
    case BattleSemanticCueFamily::Control:
        event.visualPath = BattleCueControlVisualPath;
        event.color = { 104, 160, 255, 190 };
        break;
    case BattleSemanticCueFamily::Curse:
        event.visualPath = BattleCueNegativeVisualPath;
        event.color = { 190, 112, 255, 170 };
        break;
    case BattleSemanticCueFamily::Cleanse:
        event.visualPath = BattleCueCleanseVisualPath;
        event.color = { 184, 255, 246, 205 };
        break;
    }
    return event;
}

void appendMpResourceEffectLogEvents(
    const BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    const ChangeResourceEffectCommand& command,
    const BattleResourceEffectResult& result,
    int frame)
{
    if (command.resource != BattleResource::Mp)
    {
        return;
    }

    const auto reason = [&]
    {
        switch (command.kind)
        {
        case ResourceChangeKind::Restore: return std::string_view{ "回復內力" };
        case ResourceChangeKind::Grant: return std::string_view{ "獲得內力" };
        case ResourceChangeKind::Drain: return std::string_view{ "吸取內力" };
        case ResourceChangeKind::Transfer: return std::string_view{ "轉移內力" };
        case ResourceChangeKind::Remove:
        case ResourceChangeKind::RefreshToAtLeast:
            return std::string_view{};
        }
        assert(false);
        return std::string_view{};
    }();

    int removed{};
    for (const auto& delta : result.deltas)
    {
        if (delta.unitId == metadata.targetUnitId && delta.after < delta.before)
        {
            removed += delta.before - delta.after;
        }
    }
    if (removed > 0
        && (command.kind == ResourceChangeKind::Drain
            || command.kind == ResourceChangeKind::Transfer))
    {
        auto event = makeEffectLogEvent(state, metadata, metadata.targetUnitId, frame);
        event.amount = removed;
        event.statusId = BattleStatusSemanticId::MagicPointsDrained;
        event.resourceId = BattleResourceSemanticId::MagicPoints;
        event.segments = battleLogText(std::string(reason), BattleLogTextTone::SkillName);
        logEvents.push_back(std::move(event));
    }

    for (const auto& delta : result.deltas)
    {
        const int restored = delta.after - delta.before;
        if (restored <= 0)
        {
            continue;
        }
        auto event = makeEffectLogEvent(state, metadata, delta.unitId, frame);
        event.type = BattleLogEventType::Heal;
        event.amount = restored;
        event.resourceId = BattleResourceSemanticId::MagicPoints;
        event.segments = battleLogText(std::string(reason), BattleLogTextTone::SkillName);
        logEvents.push_back(std::move(event));
    }
}

void appendEffectResourceLogEvents(
    const BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    const ChangeResourceEffectCommand& command,
    const BattleResourceEffectResult& result,
    int frame)
{
    if (command.resource == BattleResource::Mp)
    {
        appendMpResourceEffectLogEvents(state, logEvents, metadata, command, result, frame);
        return;
    }

    const auto semanticResource = resourceSemanticId(command.resource);
    if (semanticResource == BattleResourceSemanticId::None)
    {
        return;
    }

    for (const auto& delta : result.deltas)
    {
        if (delta.before == delta.after)
        {
            continue;
        }
        auto event = makeEffectLogEvent(state, metadata, delta.unitId, frame);
        event.amount = delta.after - delta.before;
        event.previousAmount = delta.before;
        event.newAmount = delta.after;
        event.statusId = BattleStatusSemanticId::ResourceChanged;
        event.resourceId = semanticResource;
        event.skillName = command.activationLog;
        event.segments = battleLogText(
            std::format(
                "{}{:+}（{}→{}）",
                resourceLabel(command.resource),
                event.amount,
                delta.before,
                delta.after),
            event.amount > 0
                ? BattleLogTextTone::Positive
                : BattleLogTextTone::Negative);
        if (!command.activationLog.empty())
            event.segments.insert(event.segments.begin(),
                {command.activationLog + "：", BattleLogTextTone::SkillName});
        logEvents.push_back(std::move(event));
    }
}

int modifierRemainingFrames(const std::optional<std::int64_t>& expiresFrameExclusive, int frame)
{
    return expiresFrameExclusive ? static_cast<int>(*expiresFrameExclusive - frame) : 0;
}

void appendAttributeModifierLog(
    const BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    const BattleAttributeModifierInstance& modifier,
    int frame)
{
    auto event = makeEffectLogEvent(state, metadata, modifier.targetUnitId, frame);
    event.amount = stackedModifierAmount(modifier.amount, modifier.stackCount);
    event.stackCount = modifier.stackCount;
    event.statusId = BattleStatusSemanticId::AttributeModifier;
    const auto tone = modifier.negative ? BattleLogTextTone::Negative : BattleLogTextTone::Positive;
    event.segments = battleLogText(
        attributeModifierText(
            modifier.attribute,
            modifier.operation,
            event.amount),
        tone);
    appendDurationFramesSuffix(
        event.segments,
        modifierRemainingFrames(modifier.expiresFrameExclusive, frame),
        tone);
    logEvents.push_back(std::move(event));
}

void appendDamageModifierLog(
    const BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    const BattleDamageModifierInstance& modifier,
    int frame)
{
    auto event = makeEffectLogEvent(state, metadata, modifier.targetUnitId, frame);
    event.amount = stackedModifierAmount(modifier.amount, modifier.stackCount);
    event.stackCount = modifier.stackCount;
    event.statusId = BattleStatusSemanticId::DamageModifier;
    const auto tone = modifier.negative ? BattleLogTextTone::Negative : BattleLogTextTone::Positive;
    event.segments = battleLogText(
        damageModifierText(
            modifier.perspective,
            modifier.operation,
            event.amount),
        tone);
    appendDurationFramesSuffix(
        event.segments,
        modifierRemainingFrames(modifier.expiresFrameExclusive, frame),
        tone);
    logEvents.push_back(std::move(event));
}

const BattleStatusContribution* appliedStatusContribution(
    const BattleStatusApplyResult& result,
    const EffectCommandMetadata& metadata,
    const ApplyStatusEffectCommand& command)
{
    const auto provenance = BattleEffectCommandSystem::statusProducerProvenance(metadata);
    const auto found = std::ranges::find_if(
        result.target.effects.statuses,
        [&](const auto& status)
        {
            return status.kind == command.status
                && status.producer
                && *status.producer == provenance.producer
                && status.origin
                && *status.origin == provenance.origin;
        });
    assert(found != result.target.effects.statuses.end());
    return &*found;
}

void appendPersistentBehaviorModifierLogs(
    const BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    BattleStatusKind status,
    const BattleStatusContribution& contribution,
    const ApplyStatusEffectCommand& command,
    int frame)
{
    if (!command.behavior)
    {
        return;
    }

    for (const auto& rule : command.behavior->rules)
    {
        if (rule.event != EffectEvent::StatusPersistent)
        {
            continue;
        }
        for (const auto& action : rule.actions)
        {
            std::visit(Overloaded{
                [&](const ModifyAttributeAction& modifier)
                {
                    const auto amount = effectiveConstantEffectNumberValue(
                        modifier.amount,
                        contribution.stacks);
                    if (!amount)
                    {
                        return;
                    }
                    auto event = makeEffectLogEvent(
                        state,
                        metadata,
                        metadata.targetUnitId,
                        frame);
                    event.amount = *amount;
                    event.stackCount = contribution.stacks;
                    event.statusId = BattleStatusSemanticId::AttributeModifier;
                    event.segments = battleLogText(
                        std::format(
                            "{}：{}",
                            battleStatusLabel(status),
                            attributeModifierText(
                                modifier.attribute,
                                modifier.operation,
                                *amount)),
                        attributeModifierIsNegative(modifier.operation, *amount)
                            ? BattleLogTextTone::Negative
                            : BattleLogTextTone::Positive);
                    logEvents.push_back(std::move(event));
                },
                [&](const ModifyDamageAction& modifier)
                {
                    const auto amount = effectiveConstantEffectNumberValue(
                        modifier.amount,
                        contribution.stacks);
                    if (!amount)
                    {
                        return;
                    }
                    auto event = makeEffectLogEvent(
                        state,
                        metadata,
                        metadata.targetUnitId,
                        frame);
                    event.amount = *amount;
                    event.stackCount = contribution.stacks;
                    event.statusId = BattleStatusSemanticId::DamageModifier;
                    event.segments = battleLogText(
                        std::format(
                            "{}：{}",
                            battleStatusLabel(status),
                            damageModifierText(
                                modifier.perspective,
                                modifier.operation,
                                *amount)),
                        damageModifierIsNegative(
                            modifier.perspective,
                            modifier.operation,
                            *amount)
                            ? BattleLogTextTone::Negative
                            : BattleLogTextTone::Positive);
                    logEvents.push_back(std::move(event));
                },
                [&](const ModifyHealTransactionAction& modifier)
                {
                    const std::string description = modifier.operation == HealModifierOperation::Block
                        ? "禁止受療"
                        : std::format("受療{}%", modifier.percent);
                    auto event = makeEffectLogEvent(
                        state,
                        metadata,
                        metadata.targetUnitId,
                        frame);
                    event.statusId = BattleStatusSemanticId::HealModifier;
                    event.stackCount = contribution.stacks;
                    event.amount = modifier.operation == HealModifierOperation::Block
                        ? 0
                        : modifier.percent;
                    event.segments = battleLogText(
                        std::format(
                            "{}：{}",
                            battleStatusLabel(status),
                            description),
                        BattleLogTextTone::Negative);
                    logEvents.push_back(std::move(event));
                },
                [&](const auto&)
                {
                },
            }, action.value);
        }
    }
}

void appendEffectStatusLogEvents(
    const BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    const ApplyStatusEffectCommand& command,
    const BattleStatusApplyResult& result,
    int frame)
{
    if (command.status == BattleStatusKind::Poison || !result.applied)
    {
        return;
    }

    const auto* contribution = appliedStatusContribution(result, metadata, command);
    const int stackCount = statusCatalogEntry(command.status).quantity != StatusQuantityModel::None
        && statusCatalogEntry(command.status).quantity != StatusQuantityModel::Internal
        ? contribution->stacks
        : 0;
    const int durationFrames = result.appliedDurationFrames;

    auto event = makeEffectLogEvent(state, metadata, metadata.targetUnitId, frame);
    event.statusId = statusSemanticId(command.status);
    event.amount = durationFrames > 0
        ? durationFrames
        : stackCount > 0 ? stackCount : result.value;
    event.stackCount = stackCount;
    event.segments = statusLogSegments(command.status, durationFrames, stackCount);
    logEvents.push_back(std::move(event));

    appendPersistentBehaviorModifierLogs(
        state,
        logEvents,
        metadata,
        command.status,
        *contribution,
        command,
        frame);
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
    CoreDetail::appendStatusEventLog(logEvents, unitId, unitId, reason);
    return true;
}

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
    BattleFrameContext& frame,
    BattleGameplayCommand& command,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    BattleFrameVector<BattleGameplayCommand>& pending,
    BattleCommandSinks sinks)
{
    if (const auto* hp = std::get_if<BattleHpDamageCommand>(&command))
    {
        return CoreDetail::tryAppendFrameDamageTransaction(state, sinks.pendingDamage, *hp);
    }
    if (const auto* mp = std::get_if<BattleMpDamageCommand>(&command))
    {
        return CoreDetail::tryAppendFrameDamageTransaction(state, sinks.pendingDamage, *mp);
    }
    if (const auto* sideEffect = std::get_if<BattleAcceptedHitSideEffectCommand>(&command))
    {
        return CoreDetail::tryAppendFrameDamageTransaction(state, sinks.pendingDamage, *sideEffect);
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
        CoreDetail::reserveTrackedProjectileFollowUps(state, followUps);
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
        bool submitted{};
        const bool handled = CoreDetail::tryCommitAutoUltimate(
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
            sinks.visualEvents, &submitted);
        if (autoUltimate->reportSubmission)
        {
            CoreDetail::appendStatusEventLog(sinks.logEvents, autoUltimate->unitId, autoUltimate->unitId,
                submitted ? "賭運自動絕招已提交" : "賭運自動絕招未提交：沒有可用絕招或合法目標");
            sinks.logEvents.back().statusId = submitted
                ? BattleStatusSemanticId::RecoveryUltimateCommitted : BattleStatusSemanticId::RecoveryUltimateSkipped;
            sinks.logEvents.back().semanticSourceKind = "talent";
            sinks.logEvents.back().semanticSourceName = "賭運";
        }
        return handled;
    }
    if (const auto* knockback = std::get_if<BattleKnockbackCommand>(&command))
    {
        if (CoreDetail::applyKnockbackImpulse(state, *knockback) && knockback->effectSource)
        {
            auto log = CoreDetail::makeEffectLogEvent(state, *knockback->effectSource,
                knockback->targetUnitId, state.movement.frame);
            log.statusId = BattleStatusSemanticId::Knockback;
            log.amount = static_cast<int>(knockback->distance);
            log.secondaryAmount = knockback->lockFrames;
            log.segments = battleLogText(std::format("{}推力（{}距離·{}幀）",
                knockback->semanticDirection == ForceMoveDirection::TowardSource ? "牽引" : "擊退",
                log.amount, log.secondaryAmount));
            sinks.logEvents.push_back(std::move(log));
        }
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
        && lhs.eventSourceUnitId == rhs.eventSourceUnitId
        && lhs.executionLane == rhs.executionLane
        && lhs.producerActionOrder == rhs.producerActionOrder
        && lhs.behaviorRuleOrder == rhs.behaviorRuleOrder
        && ((!lhs.statusContribution && !rhs.statusContribution)
            || (lhs.statusContribution && rhs.statusContribution
                && lhs.statusContribution->holderUnitId
                    == rhs.statusContribution->holderUnitId
                && lhs.statusContribution->kind
                    == rhs.statusContribution->kind
                && lhs.statusContribution->appliedSequence
                    == rhs.statusContribution->appliedSequence));
}

bool modifierApplicationShouldCue(
    BattleModifierApplyOutcome outcome,
    int stackCount)
{
    switch (outcome)
    {
    case BattleModifierApplyOutcome::Applied:
    case BattleModifierApplyOutcome::Replaced:
        return true;
    case BattleModifierApplyOutcome::StackChanged:
        return stackCount == 1;
    case BattleModifierApplyOutcome::Refreshed:
    case BattleModifierApplyOutcome::KeptStronger:
    case BattleModifierApplyOutcome::BlockedByStatusShield:
        return false;
    }
    assert(false);
    return false;
}

bool isProtectionAttribute(BattleAttribute attribute)
{
    switch (attribute)
    {
    case BattleAttribute::Defence:
    case BattleAttribute::DodgeChance:
    case BattleAttribute::BlockChance:
    case BattleAttribute::DamageReduction:
    case BattleAttribute::StaggerResistance:
    case BattleAttribute::ProjectileReflectChance:
    case BattleAttribute::SkillReflectPercent:
    case BattleAttribute::CounterUltimateBlockChance:
        return true;
    default:
        return false;
    }
}

BattleSemanticCueFamily statusCueFamily(BattleStatusKind status)
{
    switch (status)
    {
    case BattleStatusKind::Poison:
    case BattleStatusKind::ColdPoison:
        return BattleSemanticCueFamily::Poison;
    case BattleStatusKind::Bleed:
        return BattleSemanticCueFamily::Bleed;
    case BattleStatusKind::Stun:
    case BattleStatusKind::MpBlocked:
    case BattleStatusKind::Blinded:
        return BattleSemanticCueFamily::Control;
    case BattleStatusKind::WitheredBone:
    case BattleStatusKind::SevenStarMark:
    case BattleStatusKind::NeutralizeForce:
        return BattleSemanticCueFamily::Curse;
    case BattleStatusKind::NextAttackMiss:
    case BattleStatusKind::DamageBlockLayer:
    case BattleStatusKind::SingleHitCapLayer:
    case BattleStatusKind::Shadowless:
    case BattleStatusKind::SwordGuard:
        return BattleSemanticCueFamily::Protection;
    case BattleStatusKind::BattleSpirit:
    case BattleStatusKind::TrueQi:
    case BattleStatusKind::PoisonExplosion:
    case BattleStatusKind::NextAttackCritical:
    case BattleStatusKind::Berserk:
        return BattleSemanticCueFamily::Positive;
    }
    assert(false);
    return BattleSemanticCueFamily::Curse;
}

void queueSemanticCue(
    BattleFrameContext& frame,
    const EffectCommandMetadata& metadata,
    int targetUnitId,
    BattleSemanticCueFamily family)
{
    if (metadata.event != EffectEvent::BattleInitialized)
    {
        frame.queueSemanticCue(targetUnitId, family);
    }
}

void queueStatusApplyCue(
    BattleFrameContext& frame,
    const EffectCommandMetadata& metadata,
    BattleStatusKind kind,
    int requestedStacks,
    const BattleStatusApplyResult& result)
{
    switch (result.outcome)
    {
    case BattleStatusApplyOutcome::BlockedByStatusShield:
    case BattleStatusApplyOutcome::BlockedByStaggerShield:
    case BattleStatusApplyOutcome::BlockedByControlImmunity:
        queueSemanticCue(
            frame,
            metadata,
            metadata.targetUnitId,
            BattleSemanticCueFamily::Protection);
        return;
    case BattleStatusApplyOutcome::Applied:
    case BattleStatusApplyOutcome::Replaced:
        break;
    case BattleStatusApplyOutcome::StackChanged:
        // Cue only the primary application; later stack or frame growth stays silent.
        if (!result.applied || result.value > requestedStacks)
        {
            return;
        }
        break;
    case BattleStatusApplyOutcome::Refreshed:
    case BattleStatusApplyOutcome::KeptStronger:
    case BattleStatusApplyOutcome::TargetDead:
        return;
    }
    queueSemanticCue(frame, metadata, metadata.targetUnitId, statusCueFamily(kind));
}

void reduceEffectCommandImpl(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const EffectCommand& command,
    BattleEffectCommandReduction* reductionReceipt)
{
    const auto& context = command.execution;
    auto reduction = BattleEffectCommandSystem().reduce(
        state,
        command);
    assert(reduction.entries.size() == 1);
    const auto& entry = reduction.entries.front();
    if (const auto* attribute = std::get_if<BattleAttributeEffectResult>(&entry.value))
    {
        const bool maintainedEveryFrame = entry.metadata.event == EffectEvent::FrameAdvanced
            && attribute->modifier.expiresFrameExclusive
            && *attribute->modifier.expiresFrameExclusive == context.frame + 1;
        if (attribute->applied && !maintainedEveryFrame)
        {
            appendAttributeModifierLog(
                state,
                frame.logEvents,
                entry.metadata,
                attribute->modifier,
                context.frame);
        }
        if (attribute->outcome == BattleModifierApplyOutcome::BlockedByStatusShield)
        {
            queueSemanticCue(
                frame,
                entry.metadata,
                entry.metadata.targetUnitId,
                BattleSemanticCueFamily::Protection);
        }
        // 逐幀維持的一幀屬性不播放通知；到期重建不代表重新獲得增益。
        else if (modifierApplicationShouldCue(
                     attribute->outcome,
                     attribute->modifier.stackCount)
            && !maintainedEveryFrame)
        {
            const auto family = attribute->modifier.negative
                ? BattleSemanticCueFamily::Curse
                : attribute->modifier.attribute == BattleAttribute::GuaranteedHit
                    ? BattleSemanticCueFamily::SwordIntent
                : (isProtectionAttribute(attribute->modifier.attribute)
                    ? BattleSemanticCueFamily::Protection
                    : BattleSemanticCueFamily::Positive);
            queueSemanticCue(frame, entry.metadata, entry.metadata.targetUnitId, family);
        }
    }
    else if (const auto* modifier = std::get_if<BattleDamageModifierEffectResult>(&entry.value))
    {
        if (modifier->applied)
        {
            appendDamageModifierLog(
                state,
                frame.logEvents,
                entry.metadata,
                modifier->modifier,
                context.frame);
        }
        if (modifier->outcome == BattleModifierApplyOutcome::BlockedByStatusShield)
        {
            queueSemanticCue(
                frame,
                entry.metadata,
                entry.metadata.targetUnitId,
                BattleSemanticCueFamily::Protection);
        }
        else if (modifierApplicationShouldCue(
                     modifier->outcome,
                     modifier->modifier.stackCount))
        {
            const auto family = modifier->modifier.negative
                ? BattleSemanticCueFamily::Curse
                : (modifier->modifier.perspective == DamageModifierPerspective::Incoming
                    ? BattleSemanticCueFamily::Protection
                    : BattleSemanticCueFamily::Positive);
            queueSemanticCue(frame, entry.metadata, entry.metadata.targetUnitId, family);
        }
    }
    else if (const auto* absorption = std::get_if<BattleDamageAbsorptionEffectResult>(&entry.value))
    {
        auto log = makeEffectLogEvent(state, entry.metadata,
            absorption->absorption.targetUnitId, context.frame);
        log.statusId = BattleStatusSemanticId::DamageAbsorption;
        log.amount = absorption->absorption.absorbedPct;
        const int duration = static_cast<int>(
            absorption->absorption.expiresFrameExclusive - context.frame);
        log.segments = logSegments<BattleLogTextTone::Positive>(
            absorption->outcome == BattleModifierApplyOutcome::Refreshed ? "刷新傷害吸收" : "傷害吸收",
            std::format("{}%（", log.amount),
            std::pair{ BattleLogTextTone::DurationValue, duration }, "幀）");
        frame.logEvents.push_back(std::move(log));
        if (absorption->outcome == BattleModifierApplyOutcome::BlockedByStatusShield
            || modifierApplicationShouldCue(absorption->outcome, 1))
        {
            queueSemanticCue(
                frame,
                entry.metadata,
                entry.metadata.targetUnitId,
                BattleSemanticCueFamily::Protection);
        }
    }
    else if (const auto* damage = std::get_if<BattleEffectDamageRequestOutput>(&entry.value))
    {
        CoreDetail::appendEffectDamageOutput(state, frame, pendingDamage, *damage, context);
    }
    else if (const auto* area = std::get_if<BattleAreaEffectResult>(&entry.value))
    {
        CoreDetail::appendAreaLifecycleLogs(state, frame.logEvents, area->area.events, context.frame);
    }
    else if (const auto* attack = std::get_if<
                 BattleRoutedEffectCommand<ModifyAttackEffectCommand>>(&entry.value))
    {
        if (!attack->command.activationLog.empty()
            && std::ranges::none_of(frame.logEvents, [&](const BattleLogEvent& log)
            {
                return log.frame == context.frame
                    && log.category == BattleLogCategory::Cast
                    && log.targetUnitId == entry.metadata.targetUnitId
                    && log.semanticSourceTeam == entry.metadata.binding.sourceTeam
                    && log.semanticSourceKind == effectSourceKindName(entry.metadata.binding.kind)
                    && log.skillName == attack->command.activationLog;
            }))
        {
            auto log = makeEffectLogEvent(
                state,
                entry.metadata,
                entry.metadata.targetUnitId,
                context.frame);
            log.category = BattleLogCategory::Cast;
            log.skillName = attack->command.activationLog;
            log.segments = logSegments(
                "觸發",
                std::pair{BattleLogTextTone::SkillName, attack->command.activationLog});
            frame.logEvents.push_back(std::move(log));
        }
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
            .effectSource = entry.metadata.binding,
        });
    }
    else if (const auto* cast = std::get_if<
                 BattleRoutedEffectCommand<ModifyCastEffectCommand>>(&entry.value))
    {
        const auto& request = cast->command.autoUltimate;
        if (request)
        {
            frame.queueCommand(BattleAutoUltimateCommand{
                entry.metadata.targetUnitId,
                request->consumeMp,
                request->announce,
            });
        }
    }
    else if (const auto* heal = std::get_if<BattleResourceEffectResult>(&entry.value))
    {
        const auto* resource = std::get_if<ChangeResourceEffectCommand>(&command.value);
        assert(resource);
        appendEffectResourceLogEvents(
            state,
            frame.logEvents,
            entry.metadata,
            *resource,
            *heal,
            context.frame);
        if (heal->heal && heal->heal->appliedAmount > 0)
        {
            auto event = makeEffectLogEvent(
                state,
                entry.metadata,
                heal->heal->request.targetUnitId,
                context.frame);
            event.type = BattleLogEventType::Heal;
            event.amount = heal->heal->appliedAmount;
            event.resourceId = BattleResourceSemanticId::HitPoints;
            event.segments = battleLogText("效果治療", BattleLogTextTone::SkillName);
            frame.logEvents.push_back(std::move(event));
            frame.visualEvents.push_back(CoreDetail::roleEffectEvent(
                heal->heal->request.targetUnitId,
                KysChess::EFT_HEAL,
                CoreDetail::CoreRoleStatusEffectFrames));
        }
        if (resource->resource == BattleResource::Shield
            || resource->resource == BattleResource::StatusShield
            || resource->resource == BattleResource::StaggerShield
            || resource->resource == BattleResource::ControlImmunityFrames
            || resource->resource == BattleResource::InvincibilityFrames)
        {
            for (const auto& delta : heal->deltas)
            {
                if (delta.after > delta.before)
                {
                    queueSemanticCue(
                        frame,
                        entry.metadata,
                        delta.unitId,
                        BattleSemanticCueFamily::Protection);
                }
            }
        }
    }
    else if (const auto* status = std::get_if<BattleStatusApplyEffectResult>(&entry.value))
    {
        const auto* apply = std::get_if<ApplyStatusEffectCommand>(&command.value);
        assert(apply);
        CoreDetail::appendPoisonEffectLogEvents(
            frame.logEvents,
            entry.metadata,
            *apply,
            *status);
        appendEffectStatusLogEvents(
            state,
            frame.logEvents,
            entry.metadata,
            *apply,
            status->status,
            context.frame);
        queueStatusApplyCue(
            frame,
            entry.metadata,
            apply->status,
            apply->stacks,
            status->status);
    }
    else if (const auto* consume = std::get_if<BattleStatusConsumeEffectResult>(&entry.value))
    {
        CoreDetail::appendStatusConsumptionLog(state, frame.logEvents, consume->status, context.frame);
        const auto* commandConsume = std::get_if<ConsumeStatusEffectCommand>(&command.value);
        const auto* commandConsumeThis = std::get_if<ConsumeThisStatusEffectCommand>(
            &command.value);
        assert(commandConsume || commandConsumeThis);
        const auto* depletedCommand = commandConsume
            ? &commandConsume->whenDepleted
            : &commandConsumeThis->whenDepleted;
        if (consume->depletedStatus && *depletedCommand)
        {
            appendEffectStatusLogEvents(state, frame.logEvents, entry.metadata,
                **depletedCommand, *consume->depletedStatus, context.frame);
            queueStatusApplyCue(
                frame,
                entry.metadata,
                (*depletedCommand)->status,
                (*depletedCommand)->stacks,
                *consume->depletedStatus);
        }
    }
    else if (const auto* remove = std::get_if<BattleStatusRemoveEffectResult>(&entry.value))
    {
        const auto appendRemoval = [&](std::string description)
        {
            auto log = makeEffectLogEvent(state, entry.metadata, entry.metadata.targetUnitId, context.frame);
            log.statusId = BattleStatusSemanticId::StatusRemoved;
            log.amount = 1;
            log.segments = battleLogText("移除：" + description, BattleLogTextTone::Positive);
            frame.logEvents.push_back(std::move(log));
        };
        for (const auto kind : remove->status.removedStatuses)
            appendRemoval(std::string(battleStatusLabel(kind)));
        for (const auto& modifier : remove->removedAttributeModifiers)
            appendRemoval(attributeModifierText(modifier.attribute, modifier.operation,
                stackedModifierAmount(modifier.amount, modifier.stackCount)));
        for (const auto& modifier : remove->removedDamageModifiers)
            appendRemoval(damageModifierText(modifier.perspective, modifier.operation,
                stackedModifierAmount(modifier.amount, modifier.stackCount)));
        if (remove->status.currentActionStaggerCleared
            && !std::ranges::contains(remove->status.removedStatuses, BattleStatusKind::Stun))
            appendRemoval("當前硬直");
        if (remove->status.removedCount > 0
            || remove->status.currentActionStaggerCleared
            || !remove->removedAttributeModifiers.empty()
            || !remove->removedDamageModifiers.empty())
        {
            queueSemanticCue(
                frame,
                entry.metadata,
                entry.metadata.targetUnitId,
                BattleSemanticCueFamily::Cleanse);
        }
    }
    if (reductionReceipt)
    {
        reductionReceipt->entries.push_back(std::move(reduction.entries.front()));
    }
}

void registerEffectDamageContinuation(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::size_t firstDamageIndex,
    std::vector<EffectCommand> commands)
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
    decltype(state.effectIntegration.reportedEnemyTopDebuffs) current;
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
        assert(modifier.attribute == BattleAttribute::Attack || modifier.attribute == BattleAttribute::Defence);
        auto& total = current[{ modifier.targetUnitId, modifier.attribute }];
        if (total.sourceTeam < 0)
        {
            total.sourceTeam = modifier.binding.sourceTeam;
        }
        else
        {
            assert(total.sourceTeam == modifier.binding.sourceTeam);
        }
        total.value += modifier.amount * modifier.stackCount;
    }

    std::set<std::pair<int, BattleAttribute>> targets;
    for (const auto& [key, _] : current)
    {
        targets.insert(key);
    }
    for (const auto& [key, _] : state.effectIntegration.reportedEnemyTopDebuffs)
    {
        targets.insert(key);
    }

    decltype(current) nextReported;
    for (const auto& key : targets)
    {
        const auto [targetUnitId, attribute] = key;
        const auto active = current.find(key);
        const auto previous = state.effectIntegration.reportedEnemyTopDebuffs.find(key);
        const int previousValue = previous == state.effectIntegration.reportedEnemyTopDebuffs.end()
            ? 0
            : previous->second.value;
        int newValue{};
        int sourceTeam = previous == state.effectIntegration.reportedEnemyTopDebuffs.end()
            ? -1
            : previous->second.sourceTeam;
        if (active != current.end())
        {
            newValue = active->second.value;
            sourceTeam = active->second.sourceTeam;
            nextReported.emplace(key, BattleEnemyTopDebuffReportState{
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
        event.statusId = attribute == BattleAttribute::Attack
            ? BattleStatusSemanticId::EnemyTopAttackDebuff
            : BattleStatusSemanticId::EnemyTopDefenceDebuff;
        event.semanticSourceTeam = sourceTeam;
        event.semanticSourceKind = "combo";
        event.semanticSourceName = "陰險";
        event.segments = battleLogText(
            std::format("陰險：{}{:+}", attribute == BattleAttribute::Attack ? "攻擊" : "防禦", event.amount),
            BattleLogTextTone::Negative);
        logEvents.push_back(std::move(event));
    }
    state.effectIntegration.reportedEnemyTopDebuffs = std::move(nextReported);
}

void queueFreeChildCast(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
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
    pending.skillPlan = CoreDetail::makePendingCastSkillPlan(parentContext.skill);
    pending.effectCast = child;
    auto input = CoreDetail::tryMakeRuntimeCastInputForPendingCast(
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
    const auto& childSkill = CoreDetail::selectedCastSkill(*input, child.provenance.ultimate);
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
    CoreDetail::applyEffectAttackDirectives(cast.attackSpawnRequests, preparedAttackEffects);
    const auto childAttackEffects = BattleEffectAttackCastSystem().applyAttackCommands(
        *input,
        cast,
        childCommands,
        effectAttackState);
    CoreDetail::applyEffectAttackDirectives(cast.attackSpawnRequests, childAttackEffects);
    state.effectIntegration.nextSharedHitGroupId = effectAttackState.nextSharedHitGroupId;
    for (auto& request : cast.attackSpawnRequests)
    {
        request.provenance.propagation = freeCast.propagation;
        request.provenance.origin = BattleAttackOriginKind::CastDerived;
    }
    CoreDetail::reserveEffectRootCastAttacks(
        state.castLifecycle,
        child,
        cast.attackSpawnRequests);
    state.effectIntegration.casts[child.provenance.castId] = {
        .originalTargetUnitId = cast.decision.targetUnitId,
        .resourcesBeforeCast = CoreDetail::snapshotEffectResourcesBeforeCast(state),
        .skill = childSkill,
        .operationType = pending.operationType,
    };
    for (auto& request : cast.attackSpawnRequests)
    {
        state.nextFrame.queueAttack(std::move(request));
    }
    auto log = CoreDetail::makeEffectLogEvent(state, freeCast.metadata.binding,
        cast.decision.targetUnitId, state.movement.frame);
    log.statusId = BattleStatusSemanticId::FreeCast;
    log.skillId = childSkill.id;
    log.skillName = childSkill.name;
    log.segments = battleLogText("免費追加出招：" + childSkill.name, BattleLogTextTone::SkillName);
    frame.logEvents.push_back(std::move(log));
    state.castLifecycle.completeWork(child.commitBarrier);
}

std::optional<BattleAreaVisualStyle> areaVisualStyle(const BattleAreaEffect& area)
{
    if (area.source.kind != EffectSourceKind::Magic)
    {
        return std::nullopt;
    }
    return battleAreaVisualStyleForMagicId(area.source.sourceId);
}



}  // namespace

namespace CoreDetail
{
BattleLogEvent makeEffectLogEvent(
    const BattleRuntimeState& state,
    const EffectSourceBinding& binding,
    int targetUnitId,
    int frame)
{
    BattleLogEvent event;
    event.frame = frame;
    event.sourceUnitId = binding.ownerUnitId;
    event.targetUnitId = targetUnitId;
    event.semanticSourceTeam = binding.sourceTeam;
    event.semanticSourceKind = std::string(effectSourceKindName(binding.kind));
    if (const auto source = state.effectSourceNames.find({ binding.kind, binding.sourceId });
        source != state.effectSourceNames.end())
        event.semanticSourceName = source->second;
    return event;
}

void appendAreaLifecycleLogs(
    const BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logs,
    std::span<const BattleAreaLifecycleEvent> events,
    int frame)
{
    for (const auto& area : events)
    {
        auto log = makeEffectLogEvent(state, area.source, area.source.ownerUnitId, frame);
        log.effectId = area.areaId.value;
        log.perspective = BattleLogPerspective::SourceOnly;
        if (area.type == BattleAreaLifecycleEventType::Removed)
        {
            log.statusId = BattleStatusSemanticId::AreaRemoved;
            std::string_view reason;
            switch (area.removalReason)
            {
            case BattleAreaRemovalReason::Explicit: reason = "移除"; break;
            case BattleAreaRemovalReason::Expired: reason = "到期"; break;
            case BattleAreaRemovalReason::SourceDied: reason = "來源陣亡"; break;
            case BattleAreaRemovalReason::Replaced: reason = "被取代"; break;
            }
            log.segments = battleLogText(std::format("區域結束（{}）", reason));
        }
        else
        {
            log.statusId = area.type == BattleAreaLifecycleEventType::Created
                ? BattleStatusSemanticId::AreaCreated : BattleStatusSemanticId::AreaRefreshed;
            log.amount = area.expiresFrameExclusive - frame;
            log.segments = logStatusFrames<BattleLogTextTone::Positive>(
                area.type == BattleAreaLifecycleEventType::Created ? "建立區域" : "刷新區域", log.amount);
        }
        logs.push_back(std::move(log));
    }
}

void appendStatusConsumptionLog(
    const BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logs,
    const BattleStatusConsumeResult& result,
    int frame,
    const StatusConsumptionLogOverride& logOverride)
{
    if (result.consumed)
        appendStatusConsumptionLog(state, logs, BattleStatusConsumptionReceipt{
            result.target.id, result.consumedStatus, result.remainingStacks }, frame, logOverride);
}

void appendStatusConsumptionLog(
    const BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logs,
    const BattleStatusConsumptionReceipt& receipt,
    int frame,
    const StatusConsumptionLogOverride& logOverride)
{
    // 中毒每次扣次數已有毒傷事件，不再重複寫一筆消耗。
    if (receipt.contribution.kind == BattleStatusKind::Poison)
        return;
    const auto& status = receipt.contribution;
    BattleLogEvent log;
    if (status.origin)
        log = makeEffectLogEvent(state, status.origin->binding, receipt.targetUnitId, frame);
    else
    {
        log.frame = frame;
        log.sourceUnitId = status.sourceUnitId;
        log.targetUnitId = receipt.targetUnitId;
    }
    if (logOverride.sourceUnitId)
        log.sourceUnitId = *logOverride.sourceUnitId;
    if (logOverride.targetUnitId)
        log.targetUnitId = *logOverride.targetUnitId;
    log.statusId = BattleStatusSemanticId::StatusConsumed;
    log.amount = status.stacks;
    log.previousAmount = receipt.remainingStacks + status.stacks;
    log.newAmount = receipt.remainingStacks;
    log.segments = battleLogText(std::format("{}消耗{} {}（剩餘{}）",
        logOverride.actionPrefix, battleStatusLabel(status.kind), status.stacks,
        receipt.remainingStacks));
    logs.push_back(std::move(log));
}

void appendHitDamageModifierLog(
    const BattleRuntimeState& state,
    std::vector<BattleLogEvent>& logs,
    const EffectCommandMetadata& metadata,
    const ModifyDamageEffectCommand& modifier,
    int frame)
{
    auto log = makeEffectLogEvent(state, metadata.binding, metadata.targetUnitId, frame);
    log.statusId = BattleStatusSemanticId::DamageModifier;
    log.amount = modifier.amount;
    log.segments = battleLogText("本次命中：" + damageModifierText(
        modifier.perspective, modifier.operation, modifier.amount));
    logs.push_back(std::move(log));
}

void appendAreaDamagePulses(BattleRuntimeState& state, BattleFrameContext& frame)
{
    const int currentFrame = state.movement.frame;
    for (const auto& area : state.areas.areas)
    {
        if (!BattleAreaEffectSystem::activeAt(area, currentFrame)) continue;
        for (const auto& modifier : area.modifiers)
        {
            if (modifier.kind != AreaModifierKind::PeriodicDamage) continue;
            const int age = currentFrame - area.createdFrame;
            if (age <= 0 || age % modifier.intervalFrames != 0) continue;
            auto pulse = roleEffectEvent(area.source.ownerUnitId, -1, 12);
            pulse.visualPath = BattleCueFireVisualPath;
            pulse.roleEffectType = BattleRoleEffectType::FireCue;
            pulse.color = {255, 255, 255, 230};
            frame.visualEvents.push_back(std::move(pulse));
            for (const auto& unit : state.units.all())
            {
                if (!unit.core.alive || unit.core.team == area.sourceTeam
                    || !BattleAreaEffectSystem::containsUnit(area, state.gridTransform,
                        state.units, unit.id(), currentFrame)) continue;
                BattleDamageRequest damage;
                damage.attackerUnitId = area.source.ownerUnitId;
                damage.defenderUnitId = unit.id();
                damage.baseDamage = modifier.amount.flat;
                damage.damageKind = BattleDamageKind::Effect;
                damage.triggersDefenseEffects = false;
                appendFramePendingDamage(state, frame.currentFrameDamage(), damage,
                    std::nullopt, 0, false, {}, makeEffectDamageOrigin(area.source,
                        area.mergeKey.ruleId, 0, std::nullopt, std::nullopt, std::nullopt));
            }
        }
    }
}


void reduceEffectCommand(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const EffectCommand& command,
    BattleEffectCommandReduction* reductionReceipt)
{
    reduceEffectCommandImpl(
        state,
        frame,
        pendingDamage,
        command,
        reductionReceipt);
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
                        == CastPropagationPolicy::NoEffectRules
                    || event.provenance.propagation
                        == CastPropagationPolicy::SourceHitRulesOnly);
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
                        frame,
                        event,
                        castContext,
                        freeCast,
                        dispatched.commands,
                        frame.frameMemoryResource());
                }
            }

            frame.queueEffectCommands(std::move(dispatched.commands));
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

    for (const auto& cue : frame.drainSemanticCues())
    {
        visualEvents.push_back(semanticCueEvent(cue));
    }

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
    presentationFrame.areas.reserve(state.areas.areas.size());
    for (const auto& area : state.areas.areas)
    {
        const auto style = areaVisualStyle(area);
        if (!style || !BattleAreaEffectSystem::activeAt(area, state.movement.frame))
        {
            continue;
        }
        const auto pulse = std::ranges::find(area.modifiers,
            AreaModifierKind::PeriodicDamage, &AreaModifier::kind);
        presentationFrame.areas.push_back({
            .areaId = area.id.value,
            .sourceUnitId = area.source.ownerUnitId,
            .sourceTeam = area.sourceTeam,
            .center = BattleAreaEffectSystem::center(area, state.units),
            .radiusTiles = area.geometry.radiusTiles,
            .tileWidth = state.gridTransform.tileWidth,
            .style = *style,
            .createdFrame = area.createdFrame,
            .expiresFrameExclusive = area.expiresFrameExclusive,
            .pulseIntervalFrames = pulse == area.modifiers.end() ? 0 : pulse->intervalFrames,
        });
    }
    presentationFrame.gameplayEvents.reserve(
        presentationFrame.gameplayEvents.size() + frame.attackEvents.size());
    presentationFrame.visualEvents.reserve(
        presentationFrame.visualEvents.size() + frame.attackEvents.size() * 3);

    const auto appendGameplayEvent = [&](BattleGameplayEvent event)
    {
        resolveEventFrame(event);
        presentationFrame.gameplayEvents.push_back(std::move(event));
    };
    for (const auto& event : frame.attackEvents)
    {
        appendGameplayEvent(toGameplayEvent(event, state.attacks));
        appendVisualEvents(event, state.attacks, state.movement.frame, presentationFrame.visualEvents);
    }
    presentationFrame.attackSoundIds = std::move(frame.attackSoundIds);
    presentationFrame.rumbles = std::move(frame.rumbles);
    presentationFrame.blinkSoundCount = frame.blinkSoundCount;
    result = std::move(presentationFrame);
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

void reduceEffectCommandBatches(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::uint64_t reductionReceiptId,
    BattleEffectCommandReduction* reductionReceipt)
{
    assert((reductionReceiptId == 0) == (reductionReceipt == nullptr));
    while (true)
    {
        auto queued = std::exchange(
            state.effectIntegration.queuedCommandBatches,
            {});
        for (auto& batch : queued)
        {
            frame.queueEffectCommands(std::move(batch.commands));
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
                            batch.reductionReceiptId == reductionReceiptId
                                ? reductionReceipt
                                : nullptr);
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
                            std::move(continuationCommands));
                        break;
                    }
                    actionBegin = actionEnd;
                }
                ruleBegin = ruleEnd;
            }
        }
    }
}

std::vector<BattleFrameEffectCommandBatch> dispatchFrameAdvancedEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    int upcomingFrame)
{
    assert(upcomingFrame == state.movement.frame + 1);
    assert(!state.units.empty());
    std::vector<BattleFrameEffectCommandBatch> deferredAutoUltimateBatches;
    const auto& placeholderOwner = state.units.all().front().core;
    auto eventHeader = nextEffectEventHeader(state, placeholderOwner.id);
    eventHeader.executionFrame = upcomingFrame;
    const auto event = BattleEffectEventBridge().makeEvent(
        state,
        std::move(eventHeader),
        EffectEvent::FrameAdvanced,
        FrameTickEventData{
            .deltaFrames = 1,
            .periodOrdinal = static_cast<std::uint64_t>(upcomingFrame),
        });
    auto dispatched = BattleEffectEventBridge().dispatchFrameAdvanced(state, event);
    for (auto& command : dispatched.commands)
    {
        const auto* modifyCast = std::get_if<ModifyCastEffectCommand>(
            &command.value);
        if (modifyCast && modifyCast->autoUltimate)
        {
            std::vector<EffectCommand> commands;
            commands.push_back(std::move(command));
            deferredAutoUltimateBatches.push_back({
                std::move(commands),
            });
        }
        else
        {
            std::vector<EffectCommand> commands;
            commands.push_back(std::move(command));
            frame.queueEffectCommands(std::move(commands));
        }
    }
    reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    appendEnemyTopDebuffReportEvents(state, upcomingFrame, frame.logEvents);
    return deferredAutoUltimateBatches;
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

CastPropagationPolicy derivedAttackPropagation(
    const BattleAttackProvenance& sourceAttack)
{
    assert(sourceAttack.valid());
    return sourceAttack.propagation == CastPropagationPolicy::SourceRules
        ? CastPropagationPolicy::SourceHitRulesOnly
        : sourceAttack.propagation;
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
                });
            }
            unitRecord.clearSkillCooldownSource();
        }
    }
    return result;
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
    EffectFormulaInputs formulaInputs)
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

void reserveEffectAttack(
    BattleCastLifecycle& lifecycle,
    const BattleCastProvenance& parent,
    BattleAttackSpawnRequest& request)
{
    assert(parent.valid());
    assert(!request.provenance.valid());
    assert(!request.castWork.valid());
    assert(request.initial.attackSourceUnitId >= 0);

    std::optional<BattleCastStart> assisted;
    if (request.initial.attackSourceUnitId != parent.sourceUnitId)
    {
        const auto assistedPropagation = request.provenance.propagation
                == CastPropagationPolicy::NoEffectRules
            ? CastPropagationPolicy::NoEffectRules
            : CastPropagationPolicy::SourceHitRulesOnly;
        assisted = lifecycle.beginChildCast(parent.castId, {
            .sourceUnitId = request.initial.attackSourceUnitId,
            .magicId = request.initial.skillId,
            .origin = CastOriginKind::AssistedAttack,
            .propagation = assistedPropagation,
        });
    }
    const auto reservation = lifecycle.reserveAttack(
        assisted ? assisted->provenance.castId : parent.castId,
        {
            .parentAttackId = request.provenance.parentAttackId,
            .origin = request.provenance.origin,
            .rootAttack = request.provenance.rootAttack,
            .mainProjectile = request.provenance.mainProjectile,
            .sharedHitGroupId = request.provenance.sharedHitGroupId,
            .propagation = request.provenance.propagation,
        });
    request.provenance = reservation.provenance;
    request.castWork = reservation.work;
    if (assisted)
        lifecycle.completeWork(assisted->commitBarrier);
}

void reserveEffectRootCastAttacks(
    BattleCastLifecycle& lifecycle,
    const BattleCastStart& start,
    std::span<BattleAttackSpawnRequest> requests)
{
    for (auto& request : requests)
        reserveEffectAttack(lifecycle, start.provenance, request);
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
    frame.queueEffectCommands(std::move(commandBatch.commands));
    reduceEffectCommandBatches(state, frame, pendingDamage);
}

std::vector<BattleStatusEvent> advanceStatus(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage)
{
    static_cast<void>(pendingDamage);
    state.status.config.frame = state.movement.frame;
    auto statusTick = BattleStatusSystem(state.status.config).tick(state.units);
    return std::move(statusTick.events);
}

int mpRecoveryBonusPct(BattleRuntimeState& state, int unitId)
{
    return effectAdjustedAttribute(
        state,
        unitId,
        BattleAttribute::MpRecoveryBonus,
        0);
}

}  // namespace CoreDetail

}  // namespace KysChess::Battle
