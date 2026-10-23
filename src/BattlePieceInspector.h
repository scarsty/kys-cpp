#pragma once

#include "ChessBattleEffectSemantics.h"
#include "battle/BattleRuntimeSession.h"
#include "battle/BattleRuntimeEffects.h"

#include <algorithm>
#include <format>

namespace KysChess
{
// 與 chess-status 圖示包的固定編號一致。
enum class BattleEffectIcon
{
    Poison, Bleed, Stun, MpBlocked, ColdPoison, WitheredBone,
    SevenStarMark, NeutralizeForce, Blinded, NextAttackMiss, DamageBlock, SingleHitCap,
    BattleSpirit, TrueQi, PoisonExplosion, Shadowless, SwordGuard, Critical,
    Berserk, Shield, Health, Attack, Defence, Speed,
};

// 顯示優先序：有時限的效果、可消耗的護盾／次數、持續修正。
enum class BattleEffectLifetime { Timed, Consumable, Ongoing };

struct BattlePieceEffectRow
{
    std::string name;
    std::string value;
    std::string quantity;
    std::string duration;
    std::string source;
    std::string detail;
    BattleEffectIcon icon{};
    bool negative{};
    BattleEffectLifetime lifetime{};
};

inline BattleEffectIcon battleInspectorStatusIcon(BattleStatusKind status)
{
    switch (status)
    {
    case BattleStatusKind::Poison: return BattleEffectIcon::Poison;
    case BattleStatusKind::Bleed: return BattleEffectIcon::Bleed;
    case BattleStatusKind::Stun: return BattleEffectIcon::Stun;
    case BattleStatusKind::MpBlocked: return BattleEffectIcon::MpBlocked;
    case BattleStatusKind::ColdPoison: return BattleEffectIcon::ColdPoison;
    case BattleStatusKind::WitheredBone: return BattleEffectIcon::WitheredBone;
    case BattleStatusKind::SevenStarMark: return BattleEffectIcon::SevenStarMark;
    case BattleStatusKind::NeutralizeForce: return BattleEffectIcon::NeutralizeForce;
    case BattleStatusKind::Blinded: return BattleEffectIcon::Blinded;
    case BattleStatusKind::NextAttackMiss: return BattleEffectIcon::NextAttackMiss;
    case BattleStatusKind::DamageBlockLayer: return BattleEffectIcon::DamageBlock;
    case BattleStatusKind::SingleHitCapLayer: return BattleEffectIcon::SingleHitCap;
    case BattleStatusKind::BattleSpirit: return BattleEffectIcon::BattleSpirit;
    case BattleStatusKind::TrueQi: return BattleEffectIcon::TrueQi;
    case BattleStatusKind::PoisonExplosion: return BattleEffectIcon::PoisonExplosion;
    case BattleStatusKind::Shadowless: return BattleEffectIcon::Shadowless;
    case BattleStatusKind::SwordGuard: return BattleEffectIcon::SwordGuard;
    case BattleStatusKind::NextAttackCritical: return BattleEffectIcon::Critical;
    case BattleStatusKind::Huagong: return BattleEffectIcon::Poison;
    case BattleStatusKind::Berserk: return BattleEffectIcon::Berserk;
    case BattleStatusKind::Count: break;
    }
    std::unreachable();
}

inline BattleEffectIcon battleInspectorAttributeIcon(BattleAttribute attribute)
{
    switch (attribute)
    {
    case BattleAttribute::MaxHp: return BattleEffectIcon::Health;
    case BattleAttribute::Attack: return BattleEffectIcon::Attack;
    case BattleAttribute::Defence: return BattleEffectIcon::Defence;
    case BattleAttribute::CriticalChance:
    case BattleAttribute::CriticalDamage:
    case BattleAttribute::CriticalAfterDodge: return BattleEffectIcon::Critical;
    case BattleAttribute::DodgeChance: return BattleEffectIcon::Shadowless;
    case BattleAttribute::BlockChance:
    case BattleAttribute::DamageReduction:
    case BattleAttribute::StaggerResistance: return BattleEffectIcon::Shield;
    case BattleAttribute::SkillDamage: return BattleEffectIcon::BattleSpirit;
    case BattleAttribute::MpRecoveryBonus: return BattleEffectIcon::TrueQi;
    case BattleAttribute::ProjectileReflectChance:
    case BattleAttribute::SkillReflectPercent:
    case BattleAttribute::CounterUltimateBlockChance: return BattleEffectIcon::SwordGuard;
    case BattleAttribute::GuaranteedHit: return BattleEffectIcon::SevenStarMark;
    case BattleAttribute::Speed:
    case BattleAttribute::DashChance:
    case BattleAttribute::CooldownReduction:
    case BattleAttribute::OutgoingCooldownExtensionChance:
    case BattleAttribute::OutgoingCooldownExtensionPercent:
    case BattleAttribute::IncomingCooldownExtensionChance:
    case BattleAttribute::IncomingCooldownExtensionPercent: return BattleEffectIcon::Speed;
    }
    std::unreachable();
}

inline std::string_view battleInspectorAttributeLabel(BattleAttribute attribute)
{
    switch (attribute)
    {
    case BattleAttribute::MaxHp: return "生命上限";
    case BattleAttribute::Attack: return "攻擊";
    case BattleAttribute::Defence: return "防禦";
    case BattleAttribute::Speed: return "輕功";
    case BattleAttribute::CriticalChance: return "暴擊率";
    case BattleAttribute::CriticalDamage: return "暴擊傷害";
    case BattleAttribute::DodgeChance: return "閃避率";
    case BattleAttribute::BlockChance: return "格擋率";
    case BattleAttribute::DamageReduction: return "減傷";
    case BattleAttribute::SkillDamage: return "武功傷害";
    case BattleAttribute::CooldownReduction: return "冷卻縮減";
    case BattleAttribute::MpRecoveryBonus: return "回內加成";
    case BattleAttribute::StaggerResistance: return "硬直抗性";
    case BattleAttribute::ProjectileReflectChance: return "彈體反射率";
    case BattleAttribute::SkillReflectPercent: return "武功反傷";
    case BattleAttribute::CounterUltimateBlockChance: return "反制絕招率";
    case BattleAttribute::CriticalAfterDodge: return "閃避後暴擊";
    case BattleAttribute::DashChance: return "突進率";
    case BattleAttribute::OutgoingCooldownExtensionChance: return "施加冷卻延長率";
    case BattleAttribute::OutgoingCooldownExtensionPercent: return "施加冷卻延長";
    case BattleAttribute::IncomingCooldownExtensionChance: return "承受冷卻延長率";
    case BattleAttribute::IncomingCooldownExtensionPercent: return "承受冷卻延長";
    case BattleAttribute::GuaranteedHit: return "必中";
    }
    std::unreachable();
}

inline std::string battleInspectorAttributeAmount(BattleAttribute attribute, AttributeOperation operation, int amount)
{
    const bool rate = battleAttributeUsesPercentagePoints(attribute);
    const auto unit = rate ? "%" : "";
    switch (operation)
    {
    case AttributeOperation::FlatAdd: return std::format("{:+}{}", amount, rate ? " 個百分點" : "");
    case AttributeOperation::PercentAdd: return std::format("{:+}%", amount);
    case AttributeOperation::PercentagePointAdd: return std::format("{:+} 個百分點", amount);
    case AttributeOperation::Override: return std::format("設為 {}{}", amount, unit);
    case AttributeOperation::Multiply: return std::format("×{}%", amount);
    case AttributeOperation::AtLeast: return std::format("至少 {}{}", amount, unit);
    }
    std::unreachable();
}

// 幀數直接使用戰鬥時鐘，暫停與倍速不會改變顯示的持續時間語義。
inline std::string battleInspectorDuration(std::optional<std::int64_t> frames)
{
    return frames ? std::format("剩 {} 幀", *frames) : "持續生效";
}

inline std::vector<BattlePieceEffectRow> battlePieceEffectRows(
    const Battle::BattleRuntimeState& runtime, int unitId)
{
    using namespace Battle;
    const auto& record = runtime.units.require(unitId);
    const auto& unit = record.core;
    std::vector<BattlePieceEffectRow> rows;
    const auto source = [&](int id) -> std::string
    {
        if (id < 0) return {};
        return runtime.units.requireCore(id).name;
    };
    const auto protection = [&](std::string_view name, int amount, bool timed, BattleEffectIcon icon)
    {
        if (amount > 0) rows.push_back({.name = std::string(name),
            .value = timed ? "" : std::to_string(amount),
            .duration = timed ? battleInspectorDuration(amount) : "", .icon = icon,
            .lifetime = timed ? BattleEffectLifetime::Timed : BattleEffectLifetime::Consumable});
    };
    protection("護盾", unit.shield, false, BattleEffectIcon::Shield);
    protection("無敵", unit.invincible, true, BattleEffectIcon::DamageBlock);
    protection("假死復甦", unit.feignedDeathFrames, true, BattleEffectIcon::DamageBlock);
    protection("狀態護盾", record.status.effects.statusShield, false, BattleEffectIcon::DamageBlock);
    protection("硬直護盾", record.status.effects.staggerShield, false, BattleEffectIcon::SingleHitCap);
    protection("控制免疫", record.status.effects.controlImmunityFrames, true, BattleEffectIcon::SwordGuard);
    protection("雙持格擋次數", record.damage.dualWieldBlocksRemaining, false, BattleEffectIcon::SwordGuard);
    for (const auto& status : record.status.effects.statuses)
    {
        const auto& catalog = statusCatalogEntry(status.kind);
        const bool charges = catalog.quantity == StatusQuantityModel::TriggerCharges
            || catalog.quantity == StatusQuantityModel::DamageBlockCharges;
        const auto quantity = catalog.quantity == StatusQuantityModel::None || catalog.quantity == StatusQuantityModel::Internal
            ? std::string{} : std::format("{}{}", status.stacks, charges ? "次" : "層");
        rows.push_back({.name = std::string(catalog.label), .quantity = quantity,
            .duration = battleInspectorDuration(status.remainingFrames > 0
                ? std::optional<std::int64_t>(status.remainingFrames) : std::nullopt),
            .source = source(status.sourceUnitId), .icon = battleInspectorStatusIcon(status.kind),
            .negative = catalog.negative,
            .lifetime = status.remainingFrames > 0 ? BattleEffectLifetime::Timed
                : charges ? BattleEffectLifetime::Consumable : BattleEffectLifetime::Ongoing});
    }
    const auto remaining = [&](const auto& modifier) -> std::optional<std::int64_t>
    {
        return modifier.expiresFrameExclusive
            ? std::optional<std::int64_t>(*modifier.expiresFrameExclusive - runtime.movement.frame)
            : std::nullopt;
    };
    const auto active = [&](const auto& modifier)
    {
        return modifier.targetUnitId == unitId
            && (!modifier.expiresFrameExclusive || *modifier.expiresFrameExclusive > runtime.movement.frame);
    };
    for (const auto& modifier : runtime.effectCommands.attributeModifiers)
    {
        if (!active(modifier)) continue;
        rows.push_back({.name = std::string(battleInspectorAttributeLabel(modifier.attribute)),
            .value = battleInspectorAttributeAmount(modifier.attribute, modifier.operation, modifier.amount),
            .quantity = std::format("{}層", modifier.stackCount),
            .duration = battleInspectorDuration(remaining(modifier)), .source = source(modifier.binding.ownerUnitId),
            .detail = modifier.eventSourceUnitId >= 0 ? "限定對象：" + runtime.units.requireCore(modifier.eventSourceUnitId).name : "",
            .icon = battleInspectorAttributeIcon(modifier.attribute), .negative = modifier.negative,
            .lifetime = modifier.expiresFrameExclusive ? BattleEffectLifetime::Timed : BattleEffectLifetime::Ongoing});
    }
    for (const auto& modifier : runtime.effectCommands.damageModifiers)
    {
        if (!active(modifier)) continue;
        std::string amount;
        switch (modifier.operation)
        {
        case DamageModifierOperation::FlatAdd: amount = std::format("{:+}", modifier.amount); break;
        case DamageModifierOperation::PercentAdd: amount = std::format("{:+}%", modifier.amount); break;
        case DamageModifierOperation::Multiply: amount = std::format("×{}%", modifier.amount); break;
        case DamageModifierOperation::IgnoreDefensePercent: amount = std::format("無視防禦 {}%", modifier.amount); break;
        case DamageModifierOperation::CapSingleHitAtMaxHpPercent: amount = std::format("單次上限：生命 {}%", modifier.amount); break;
        case DamageModifierOperation::CapSingleHitAtValue: amount = std::format("單次上限 {}", modifier.amount); break;
        case DamageModifierOperation::ExecuteBelowMaxHpPercent: amount = std::format("斬殺線 {}%", modifier.amount); break;
        }
        const auto channel = modifier.channel == DamageChannel::Skill ? "武功"
            : modifier.channel == DamageChannel::Dot ? "持續"
            : modifier.channel == DamageChannel::Effect ? "效果" : "全部";
        const auto stage = modifier.stage == DamageModifierStage::BeforeDefense ? "防禦前"
            : modifier.stage == DamageModifierStage::AfterDefense ? "防禦後" : "最終";
        rows.push_back({.name = std::format("{}{}傷害",
            modifier.perspective == DamageModifierPerspective::Outgoing ? "造成" : "承受",
            channel), .value = amount, .quantity = std::format("{}層", modifier.stackCount),
            .duration = battleInspectorDuration(remaining(modifier)), .source = source(modifier.binding.ownerUnitId),
            .detail = stage, .icon = modifier.perspective == DamageModifierPerspective::Outgoing
                ? BattleEffectIcon::Attack : BattleEffectIcon::Defence, .negative = modifier.negative,
            .lifetime = modifier.expiresFrameExclusive ? BattleEffectLifetime::Timed : BattleEffectLifetime::Ongoing});
    }
    for (const auto& absorption : runtime.effectCommands.damageAbsorptions)
    {
        if (absorption.targetUnitId != unitId || absorption.expiresFrameExclusive <= runtime.movement.frame) continue;
        rows.push_back({.name = "傷害吸收", .value = std::format("{}%", absorption.absorbedPct),
            .duration = battleInspectorDuration(absorption.expiresFrameExclusive - runtime.movement.frame),
            .source = source(absorption.binding.ownerUnitId), .detail = std::format("已吸收 {}", absorption.accumulatedDamage),
            .icon = BattleEffectIcon::Shield, .lifetime = BattleEffectLifetime::Timed});
    }
    std::stable_sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        if (a.lifetime != b.lifetime) return a.lifetime < b.lifetime;
        return a.negative < b.negative;
    });
    return rows;
}
}
