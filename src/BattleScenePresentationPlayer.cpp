#include "BattleScenePresentationPlayer.h"

#include "BattleScenePresentationConstants.h"
#include "Font.h"

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace
{
Color toSceneColor(KysChess::Battle::BattlePresentationColor color)
{
    return { color.r, color.g, color.b, color.a };
}

float damageTextImpactScale(int damage, int maxHp)
{
    if (damage <= 0 || maxHp <= 0)
    {
        return 1.0f;
    }

    float ratio = static_cast<float>(damage) / static_cast<float>(maxHp);
    return (std::max)(0.0f, (std::min)(ratio / 0.2f, 1.0f));
}

int damageTextSize(int baseSize, int damage, int maxHp)
{
    float impactScale = damageTextImpactScale(damage, maxHp);
    float sizeScale = 0.82f + 0.18f * impactScale;
    return (std::max)(1, int(baseSize * sizeScale + 0.5f));
}

uint8_t damageTextAlpha(int damage, int maxHp)
{
    float impactScale = damageTextImpactScale(damage, maxHp);
    return static_cast<uint8_t>(192 + (255 - 192) * impactScale);
}

Role* resolveRole(const BattleScenePresentationPlayer::Bindings& bindings, int unitId)
{
    assert(bindings.resolveRole);
    if (unitId < 0)
    {
        return nullptr;
    }
    return bindings.resolveRole(unitId);
}

BattleSceneAct::AttackEffect* findProjectile(
    const BattleScenePresentationPlayer::Bindings& bindings,
    int projectileAttackId)
{
    assert(bindings.attackEffects);
    auto it = std::find_if(bindings.attackEffects->begin(), bindings.attackEffects->end(), [&](const auto& effect)
        {
            return effect.VisualAttackId == projectileAttackId;
        });
    return it == bindings.attackEffects->end() ? nullptr : &*it;
}

BattleSceneAct::AttackEffect& createProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const BattleScenePresentationPlayer::Bindings& bindings)
{
    assert(command.projectileAttackId >= 0);

    BattleSceneAct::AttackEffect effect;
    effect.VisualAttackId = command.projectileAttackId;
    effect.Attacker = resolveRole(bindings, command.projectileSourceUnitId);
    effect.PreferredTarget = resolveRole(bindings, command.projectileTargetUnitId);
    effect.Pos = command.projectilePosition;
    effect.Velocity = command.projectileVelocity;
    effect.TotalFrame = std::max(1, command.projectileDurationFrames);
    effect.Frame = 0;
    effect.OperationType = command.projectileOperationKind;
    effect.NoHurt = 1;
    effect.IsMain = 0;
    effect.IgnoreProjectileCancel = 1;
    if (command.visualEffectId >= 0)
    {
        effect.setEft(command.visualEffectId);
        effect.TotalFrame = std::max(effect.TotalFrame, effect.TotalEffectFrame);
    }
    bindings.attackEffects->push_back(std::move(effect));
    return bindings.attackEffects->back();
}

BattleSceneAct::AttackEffect& upsertProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const BattleScenePresentationPlayer::Bindings& bindings)
{
    if (auto* effect = findProjectile(bindings, command.projectileAttackId))
    {
        return *effect;
    }
    return createProjectile(command, bindings);
}

void finishProjectile(BattleSceneAct::AttackEffect& effect, int fadeFrames)
{
    effect.NoHurt = 1;
    effect.Frame = std::max(effect.Frame, effect.TotalFrame - fadeFrames);
}
}  // namespace

void BattleScenePresentationPlayer::play(
    const KysChess::Battle::BattlePresentationFrame& frame,
    const Bindings& bindings) const
{
    assert(bindings.tracker);
    assert(bindings.textEffects);
    assert(bindings.attackEffects);
    assert(bindings.resolveRole);

    const auto plan = planner_.build(frame);
    for (const auto& command : plan.commands)
    {
        playCommand(command, bindings);
    }
}

void BattleScenePresentationPlayer::playCommand(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    using KysChess::Battle::BattlePresentationCommandType;
    switch (command.type)
    {
    case BattlePresentationCommandType::RecordDamage:
        recordDamage(command, bindings);
        break;
    case BattlePresentationCommandType::RecordHeal:
        recordHeal(command, bindings);
        break;
    case BattlePresentationCommandType::RecordStatus:
        recordStatus(command, bindings);
        break;
    case BattlePresentationCommandType::SpawnFloatingText:
        spawnFloatingText(command, bindings);
        break;
    case BattlePresentationCommandType::SpawnRoleEffect:
        spawnRoleEffect(command, bindings);
        break;
    case BattlePresentationCommandType::SpawnDamageNumber:
        spawnDamageNumber(command, bindings);
        break;
    case BattlePresentationCommandType::FocusCamera:
        break;
    case BattlePresentationCommandType::SpawnProjectile:
        spawnProjectile(command, bindings);
        break;
    case BattlePresentationCommandType::MoveProjectile:
        moveProjectile(command, bindings);
        break;
    case BattlePresentationCommandType::ImpactProjectile:
        impactProjectile(command, bindings);
        break;
    case BattlePresentationCommandType::ExpireProjectile:
        expireProjectile(command, bindings);
        break;
    case BattlePresentationCommandType::CancelProjectile:
        cancelProjectile(command, bindings);
        break;
    case BattlePresentationCommandType::BounceProjectile:
        bounceProjectile(command, bindings);
        break;
    }
}

void BattleScenePresentationPlayer::recordDamage(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    bindings.tracker->recordDamage(
        resolveRole(bindings, command.sourceUnitId),
        resolveRole(bindings, command.targetUnitId),
        command.amount,
        command.skillName,
        command.frame,
        command.detailText);
}

void BattleScenePresentationPlayer::recordHeal(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    bindings.tracker->recordHeal(
        resolveRole(bindings, command.sourceUnitId),
        resolveRole(bindings, command.targetUnitId),
        command.amount,
        command.text,
        command.frame);
}

void BattleScenePresentationPlayer::recordStatus(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    bindings.tracker->recordStatus(
        resolveRole(bindings, command.sourceUnitId),
        resolveRole(bindings, command.targetUnitId),
        command.text,
        command.frame);
}

void BattleScenePresentationPlayer::spawnFloatingText(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    BattleSceneAct::TextEffect effect;
    auto* role = resolveRole(bindings, command.targetUnitId);
    effect.set(command.text, toSceneColor(command.color), role);
    if (!role)
    {
        effect.Pos = command.position;
    }
    effect.Size = command.textSize;
    effect.Type = command.textMotionType;
    bindings.textEffects->push_back(std::move(effect));
}

void BattleScenePresentationPlayer::spawnRoleEffect(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto* role = resolveRole(bindings, command.targetUnitId);
    assert(role);

    BattleSceneAct::AttackEffect effect;
    effect.FollowRole = role;
    effect.Pos = { 0.0f, 0.0f, ROLE_STATUS_EFT_Z_OFFSET };
    effect.setEft(command.effectId);
    effect.TotalFrame = command.durationFrames > 0
        ? std::max(command.durationFrames, effect.TotalEffectFrame)
        : std::max(1, effect.TotalEffectFrame);
    effect.Frame = 0;
    effect.NoHurt = 1;
    effect.IsMain = 0;
    effect.IgnoreProjectileCancel = 1;
    bindings.attackEffects->push_back(std::move(effect));
}

void BattleScenePresentationPlayer::spawnDamageNumber(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto* role = resolveRole(bindings, command.targetUnitId);
    assert(role);
    assert(command.amount > 0);

    BattleSceneAct::TextEffect effect;
    effect.set(std::to_string(-command.amount), toSceneColor(command.color), role);
    effect.Size = damageTextSize(command.textSize, command.amount, role->MaxHP);
    effect.color.a = damageTextAlpha(command.amount, role->MaxHP);
    bindings.textEffects->push_back(std::move(effect));
}

void BattleScenePresentationPlayer::spawnProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    createProjectile(command, bindings);
}

void BattleScenePresentationPlayer::moveProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto& effect = upsertProjectile(command, bindings);
    effect.Pos = command.projectilePosition;
    effect.Velocity = command.projectileVelocity;
    effect.PreferredTarget = resolveRole(bindings, command.projectileTargetUnitId);
}

void BattleScenePresentationPlayer::impactProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto& effect = upsertProjectile(command, bindings);
    effect.Pos = command.projectilePosition;
    effect.PreferredTarget = resolveRole(bindings, command.projectileTargetUnitId);
    finishProjectile(effect, 15);
}

void BattleScenePresentationPlayer::expireProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto& effect = upsertProjectile(command, bindings);
    effect.Pos = command.projectilePosition;
    finishProjectile(effect, 5);
}

void BattleScenePresentationPlayer::cancelProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    if (auto* effect = findProjectile(bindings, command.projectileAttackId))
    {
        finishProjectile(*effect, 5);
    }
    if (auto* other = findProjectile(bindings, command.projectileRelatedAttackId))
    {
        finishProjectile(*other, 5);
    }
}

void BattleScenePresentationPlayer::bounceProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto& effect = upsertProjectile(command, bindings);
    effect.Pos = command.projectilePosition;
    effect.PreferredTarget = resolveRole(bindings, command.projectileTargetUnitId);
    finishProjectile(effect, 15);
}
