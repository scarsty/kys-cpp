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

const BattleUnitIdentity* resolveIdentity(const BattleScenePresentationPlayer::Bindings& bindings, int unitId)
{
    assert(bindings.resolveIdentity);
    if (unitId < 0)
    {
        return nullptr;
    }
    return bindings.resolveIdentity(unitId);
}

std::optional<BattleScenePresentationPlayer::UnitView> resolveUnitView(
    const BattleScenePresentationPlayer::Bindings& bindings,
    int unitId)
{
    assert(bindings.resolveUnitView);
    if (unitId < 0)
    {
        return std::nullopt;
    }
    return bindings.resolveUnitView(unitId);
}

Pointf floatingTextPositionFor(const BattleScenePresentationPlayer::UnitView& unit, const std::string& text)
{
    Pointf position = unit.position;
    position.x -= 7.5 * Font::getTextDrawSize(text);
    position.y -= 50;
    return position;
}

int resolveVisualTeam(const BattleScenePresentationPlayer::Bindings& bindings, int unitId)
{
    auto unit = resolveUnitView(bindings, unitId);
    return unit ? unit->team : -1;
}

BattleAttackEffect* findProjectile(
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

BattleAttackEffect& createProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const BattleScenePresentationPlayer::Bindings& bindings)
{
    assert(command.projectileAttackId >= 0);

    BattleAttackEffect effect;
    effect.VisualAttackId = command.projectileAttackId;
    effect.VisualOnly = 1;
    effect.Frame = 0;
    bindings.attackEffects->push_back(std::move(effect));
    return bindings.attackEffects->back();
}

void applyProjectilePayload(
    BattleAttackEffect& effect,
    const KysChess::Battle::BattlePresentationCommand& command,
    const BattleScenePresentationPlayer::Bindings& bindings)
{
    effect.VisualOnly = 1;
    effect.VisualTeam = resolveVisualTeam(bindings, command.projectileSourceUnitId);
    effect.Pos = command.projectilePosition;
    effect.Velocity = command.projectileVelocity;
    effect.TotalFrame = std::max(1, command.projectileDurationFrames);
    if (command.visualEffectId >= 0)
    {
        if (effect.VisualEffectId != command.visualEffectId)
        {
            effect.setEft(command.visualEffectId);
        }
        effect.TotalFrame = std::max(effect.TotalFrame, effect.TotalEffectFrame);
    }
}

BattleAttackEffect& upsertProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const BattleScenePresentationPlayer::Bindings& bindings)
{
    if (auto* effect = findProjectile(bindings, command.projectileAttackId))
    {
        return *effect;
    }
    return createProjectile(command, bindings);
}

void finishProjectile(BattleAttackEffect& effect, int fadeFrames)
{
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
    assert(bindings.resolveIdentity);
    assert(bindings.resolveUnitView);

    playLogs(frame.logEvents, bindings);
    playVisuals(frame.visualEvents, bindings);
}

void BattleScenePresentationPlayer::playLogs(
    const std::vector<KysChess::Battle::BattleLogEvent>& logEvents,
    const Bindings& bindings) const
{
    assert(bindings.tracker);
    assert(bindings.resolveIdentity);

    for (const auto& event : logEvents)
    {
        recordLog(event, bindings);
    }
}

void BattleScenePresentationPlayer::playVisuals(
    const std::vector<KysChess::Battle::BattleVisualEvent>& visualEvents,
    const Bindings& bindings) const
{
    assert(bindings.textEffects);
    assert(bindings.attackEffects);
    assert(bindings.resolveUnitView);

    KysChess::Battle::BattlePresentationFrame frame;
    frame.visualEvents = visualEvents;
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

void BattleScenePresentationPlayer::recordLog(
    const KysChess::Battle::BattleLogEvent& event,
    const Bindings& bindings) const
{
    using KysChess::Battle::BattleLogEventType;
    switch (event.type)
    {
    case BattleLogEventType::Damage:
        recordDamage(event, bindings);
        break;
    case BattleLogEventType::Heal:
        recordHeal(event, bindings);
        break;
    case BattleLogEventType::Status:
        recordStatus(event, bindings);
        break;
    case BattleLogEventType::UnitDied:
    case BattleLogEventType::BattleEnded:
        break;
    }
}

void BattleScenePresentationPlayer::recordDamage(
    const KysChess::Battle::BattleLogEvent& event,
    const Bindings& bindings) const
{
    bindings.tracker->recordDamage(
        resolveIdentity(bindings, event.sourceUnitId),
        resolveIdentity(bindings, event.targetUnitId),
        event.amount,
        event.skillName,
        event.frame,
        event.detailText);
}

void BattleScenePresentationPlayer::recordHeal(
    const KysChess::Battle::BattleLogEvent& event,
    const Bindings& bindings) const
{
    bindings.tracker->recordHeal(
        resolveIdentity(bindings, event.sourceUnitId),
        resolveIdentity(bindings, event.targetUnitId),
        event.amount,
        event.text,
        event.frame);
}

void BattleScenePresentationPlayer::recordStatus(
    const KysChess::Battle::BattleLogEvent& event,
    const Bindings& bindings) const
{
    bindings.tracker->recordStatus(
        resolveIdentity(bindings, event.sourceUnitId),
        resolveIdentity(bindings, event.targetUnitId),
        event.text,
        event.frame);
}

void BattleScenePresentationPlayer::spawnFloatingText(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    BattleTextEffect effect;
    effect.Text = command.text;
    effect.color = toSceneColor(command.color);
    if (auto unit = resolveUnitView(bindings, command.targetUnitId))
    {
        effect.Pos = floatingTextPositionFor(*unit, command.text);
    }
    else
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
    auto unit = resolveUnitView(bindings, command.targetUnitId);
    assert(unit);

    BattleAttackEffect effect;
    effect.FollowUnitId = command.targetUnitId;
    effect.Pos = { 0.0f, 0.0f, ROLE_STATUS_EFT_Z_OFFSET };
    effect.setEft(command.effectId);
    effect.TotalFrame = command.durationFrames > 0
        ? std::max(command.durationFrames, effect.TotalEffectFrame)
        : std::max(1, effect.TotalEffectFrame);
    effect.Frame = 0;
    effect.VisualOnly = 1;
    effect.VisualTeam = unit->team;
    bindings.attackEffects->push_back(std::move(effect));
}

void BattleScenePresentationPlayer::spawnDamageNumber(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto unit = resolveUnitView(bindings, command.targetUnitId);
    assert(unit);
    assert(command.amount > 0);

    BattleTextEffect effect;
    effect.Text = std::to_string(-command.amount);
    effect.color = toSceneColor(command.color);
    effect.Pos = floatingTextPositionFor(*unit, effect.Text);
    effect.Size = damageTextSize(command.textSize, command.amount, unit->maxHp);
    effect.color.a = damageTextAlpha(command.amount, unit->maxHp);
    bindings.textEffects->push_back(std::move(effect));
}

void BattleScenePresentationPlayer::spawnProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto& effect = upsertProjectile(command, bindings);
    applyProjectilePayload(effect, command, bindings);
}

void BattleScenePresentationPlayer::moveProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto& effect = upsertProjectile(command, bindings);
    applyProjectilePayload(effect, command, bindings);
}

void BattleScenePresentationPlayer::impactProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto& effect = upsertProjectile(command, bindings);
    applyProjectilePayload(effect, command, bindings);
    finishProjectile(effect, 15);
}

void BattleScenePresentationPlayer::expireProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto& effect = upsertProjectile(command, bindings);
    applyProjectilePayload(effect, command, bindings);
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
    if (command.projectileRelatedAttackId >= 0)
    {
        if (auto* other = findProjectile(bindings, command.projectileRelatedAttackId))
        {
            finishProjectile(*other, 5);
        }
    }
}

void BattleScenePresentationPlayer::bounceProjectile(
    const KysChess::Battle::BattlePresentationCommand& command,
    const Bindings& bindings) const
{
    auto& effect = upsertProjectile(command, bindings);
    applyProjectilePayload(effect, command, bindings);
    finishProjectile(effect, 15);
}
