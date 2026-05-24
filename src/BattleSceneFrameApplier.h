#pragma once

#include "BattlePresentationEffects.h"
#include "BattleReport.h"
#include "BattleSceneCamera.h"
#include "BattleScenePresentationConstants.h"
#include "BattleSceneUnitStore.h"
#include "Point.h"
#include "Random.h"
#include "battle/BattlePresentation.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <format>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class BattleSceneFrameApplier
{
public:
    struct Bindings
    {
        BattleReportBuilder& report;
        BattleSceneUnitStore& units;
        std::deque<BattleAttackEffect>& attackEffects;
        std::deque<BattleTextEffect>& textEffects;
        std::unordered_map<int, int>& hurtFlashTimers;
        RandomDouble& random;
        Pointf& cameraPosition;
        int& battleResult;
        int& frozen;
        int& slow;
        int& shake;
        int& jitterX;
        int& jitterY;
        BattleSceneCamera& camera;
        BattleSceneCameraBounds& cameraBounds;
        bool& manualCameraEnabled;
    };

    explicit BattleSceneFrameApplier(Bindings bindings);

    template <class Effects>
    void apply(const KysChess::Battle::BattlePresentationFrame& frame, Effects& effects) const;

private:
    struct UnitView
    {
        Pointf position;
        int team = -1;
        int maxHp = 0;
    };

    template <class Effects>
    void applyDamageAndLifecycleEffects(const KysChess::Battle::BattlePresentationFrame& frame, Effects& effects) const;
    template <class Effects>
    void applyVisualEvent(const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;
    template <class Effects>
    void applyImpactEvent(const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;
    template <class Effects>
    void spawnFloatingText(const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;
    template <class Effects>
    void spawnRoleEffect(const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;
    template <class Effects>
    void spawnDamageNumber(const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;
    template <class Effects>
    void spawnProjectile(const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;
    template <class Effects>
    void moveProjectile(const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;
    template <class Effects>
    void impactProjectile(const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;
    template <class Effects>
    void expireProjectile(const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;
    void cancelProjectile(const KysChess::Battle::BattleVisualEvent& event) const;
    template <class Effects>
    void bounceProjectile(const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;
    void recordLog(const KysChess::Battle::BattleLogEvent& event) const;
    template <class Effects>
    void applyFrameEffects(const KysChess::Battle::BattlePresentationFrame& frame, Effects& effects) const;

    const KysChess::Battle::BattleRuntimeUnit* resolveRuntimeUnit(int unitId) const;
    std::optional<UnitView> resolveUnitView(int unitId) const;
    int resolveVisualTeam(int unitId) const;
    BattleAttackEffect* findProjectile(int projectileAttackId) const;
    BattleAttackEffect& createProjectile(const KysChess::Battle::BattleVisualEvent& event) const;
    template <class Effects>
    void applyProjectilePayload(BattleAttackEffect& effect, const KysChess::Battle::BattleVisualEvent& event, Effects& effects) const;

    Bindings bindings_;
};

namespace BattleSceneFrameApplierDetail
{
inline constexpr int HURT_FLASH_DURATION = 15;
inline constexpr int BLINK_SOUND_EFFECT_ID = 11;
inline constexpr int CAMERA_DEATH_ZOOM_FRAMES = 30;
inline constexpr int CAMERA_BATTLE_END_ZOOM_FRAMES = 30;
inline constexpr int CAMERA_DEATH_SLOW_FRAMES = 10;
inline constexpr int CAMERA_BATTLE_END_SLOW_FRAMES = 30;

inline Color toSceneColor(KysChess::Battle::BattlePresentationColor color)
{
    return { color.r, color.g, color.b, color.a };
}

inline float damageTextImpactScale(int damage, int maxHp)
{
    if (damage <= 0 || maxHp <= 0)
    {
        return 1.0f;
    }

    float ratio = static_cast<float>(damage) / static_cast<float>(maxHp);
    return (std::max)(0.0f, (std::min)(ratio / 0.2f, 1.0f));
}

inline int damageTextSize(int baseSize, int damage, int maxHp)
{
    float impactScale = damageTextImpactScale(damage, maxHp);
    float sizeScale = 0.82f + 0.18f * impactScale;
    return (std::max)(1, int(baseSize * sizeScale + 0.5f));
}

inline std::uint8_t damageTextAlpha(int damage, int maxHp)
{
    float impactScale = damageTextImpactScale(damage, maxHp);
    return static_cast<std::uint8_t>(192 + (255 - 192) * impactScale);
}

inline std::set<int> damagedUnitIdsFor(const std::vector<KysChess::Battle::BattleLogEvent>& logs)
{
    std::set<int> unitIds;
    for (const auto& log : logs)
    {
        if (log.type == KysChess::Battle::BattleLogEventType::Damage
            && log.amount > 0
            && log.targetUnitId >= 0)
        {
            unitIds.insert(log.targetUnitId);
        }
    }
    return unitIds;
}

inline std::vector<int> diedUnitIdsFor(const std::vector<KysChess::Battle::BattleGameplayEvent>& events)
{
    std::vector<int> unitIds;
    for (const auto& event : events)
    {
        if (event.type == KysChess::Battle::BattleGameplayEventType::UnitDied)
        {
            unitIds.push_back(event.targetUnitId);
        }
    }
    return unitIds;
}

inline Pointf floatingTextPositionFor(
    Pointf position,
    const std::string& text,
    auto& effects)
{
    position.x -= static_cast<float>(7.5 * effects.textDrawSize(text));
    position.y -= 50;
    return position;
}

inline void finishProjectile(BattleAttackEffect& effect, int fadeFrames)
{
    effect.Frame = std::max(effect.Frame, effect.TotalFrame - fadeFrames);
}

inline int projectileRelatedAttackIdFor(const KysChess::Battle::BattleVisualEvent& event)
{
    switch (event.type)
    {
    case KysChess::Battle::BattleVisualEventType::ProjectileCancelled:
    case KysChess::Battle::BattleVisualEventType::ProjectileBounced:
        return event.amount;
    default:
        return -1;
    }
}
}  // namespace BattleSceneFrameApplierDetail

template <class Effects>
void BattleSceneFrameApplier::apply(
    const KysChess::Battle::BattlePresentationFrame& frame,
    Effects& effects) const
{
    applyDamageAndLifecycleEffects(frame, effects);
    for (const auto& event : frame.visualEvents)
    {
        applyImpactEvent(event, effects);
    }
    for (const auto& event : frame.logEvents)
    {
        recordLog(event);
    }
    for (const auto& event : frame.visualEvents)
    {
        applyVisualEvent(event, effects);
    }
    applyFrameEffects(frame, effects);
}

template <class Effects>
void BattleSceneFrameApplier::applyDamageAndLifecycleEffects(
    const KysChess::Battle::BattlePresentationFrame& frame,
    Effects& effects) const
{
    using namespace BattleSceneFrameApplierDetail;

    for (const int unitId : damagedUnitIdsFor(frame.logEvents))
    {
        BattleAttackEffect effect;
        effect.FollowUnitId = unitId;
        effect.Path = std::format("eft/bld{:03}", int(bindings_.random.rand() * 5));
        effect.TotalEffectFrame = effects.effectFrameCount(effect.Path);
        effect.TotalFrame = effect.TotalEffectFrame;
        effect.Frame = 0;
        effect.VisualOnly = 1;
        bindings_.attackEffects.push_back(std::move(effect));
        bindings_.hurtFlashTimers[unitId] = HURT_FLASH_DURATION;
    }

    for (const int unitId : diedUnitIdsFor(frame.gameplayEvents))
    {
        const auto& unit = bindings_.units.requireRuntimeUnit(unitId);
        bindings_.jitterX = bindings_.random.rand_int(2) - bindings_.random.rand_int(2);
        bindings_.jitterY = bindings_.random.rand_int(2) - bindings_.random.rand_int(2);
        if (!bindings_.manualCameraEnabled)
        {
            auto cameraFocus = unit.motion.position;
            cameraFocus.y -= unit.motion.position.z * 2.0f;
            bindings_.cameraPosition = BattleSceneCamera::clampCenter(cameraFocus, bindings_.cameraBounds);
            bindings_.camera.focusOn(bindings_.cameraPosition, CAMERA_DEATH_ZOOM_FRAMES);
        }
        bindings_.frozen = 5;
        bindings_.shake = 10;
        bindings_.slow = CAMERA_DEATH_SLOW_FRAMES;
    }

    for (const auto& event : frame.gameplayEvents)
    {
        if (event.type != KysChess::Battle::BattleGameplayEventType::BattleEnded
            || bindings_.battleResult != -1)
        {
            continue;
        }
        bindings_.battleResult = event.amount;
        if (!bindings_.manualCameraEnabled)
        {
            bindings_.camera.focusOn(bindings_.cameraPosition, CAMERA_BATTLE_END_ZOOM_FRAMES);
        }
        bindings_.frozen = 60;
        bindings_.slow = CAMERA_BATTLE_END_SLOW_FRAMES;
        bindings_.shake = 60;
    }
}

template <class Effects>
void BattleSceneFrameApplier::applyVisualEvent(
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    using KysChess::Battle::BattleVisualEventType;
    switch (event.type)
    {
    case BattleVisualEventType::FloatingText:
        spawnFloatingText(event, effects);
        break;
    case BattleVisualEventType::RoleEffect:
        spawnRoleEffect(event, effects);
        break;
    case BattleVisualEventType::DamageNumber:
        spawnDamageNumber(event, effects);
        break;
    case BattleVisualEventType::CameraFocus:
        break;
    case BattleVisualEventType::ProjectileSpawned:
        spawnProjectile(event, effects);
        break;
    case BattleVisualEventType::ProjectileMoved:
        moveProjectile(event, effects);
        break;
    case BattleVisualEventType::ProjectileHit:
        impactProjectile(event, effects);
        break;
    case BattleVisualEventType::ProjectileExpired:
        expireProjectile(event, effects);
        break;
    case BattleVisualEventType::ProjectileTargetLost:
    case BattleVisualEventType::ProjectileCancelled:
        cancelProjectile(event);
        break;
    case BattleVisualEventType::ProjectileBounced:
        bounceProjectile(event, effects);
        break;
    }
}

template <class Effects>
void BattleSceneFrameApplier::applyImpactEvent(
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    if (event.type != KysChess::Battle::BattleVisualEventType::ProjectileHit)
    {
        return;
    }

    bindings_.units.setUnitShake(event.targetUnitId, event.impactUnitShake);
    if (event.impactEffectSoundId >= 0)
    {
        effects.playEffectSound(event.impactEffectSoundId);
    }
    if (event.impactSceneShake > 0)
    {
        bindings_.shake = event.impactSceneShake;
    }
    if (event.impactRumble)
    {
        effects.rumble(100, 100, 50);
    }
}

template <class Effects>
void BattleSceneFrameApplier::spawnFloatingText(
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    BattleTextEffect effect;
    effect.Text = event.text;
    effect.color = BattleSceneFrameApplierDetail::toSceneColor(event.color);
    if (auto unit = resolveUnitView(event.targetUnitId))
    {
        effect.Pos = BattleSceneFrameApplierDetail::floatingTextPositionFor(unit->position, event.text, effects);
    }
    else
    {
        effect.Pos = event.position;
    }
    effect.Size = event.textSize;
    effect.Type = event.textMotionType;
    bindings_.textEffects.push_back(std::move(effect));
}

template <class Effects>
void BattleSceneFrameApplier::spawnRoleEffect(
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    auto unit = resolveUnitView(event.targetUnitId);
    assert(unit);

    BattleAttackEffect effect;
    effect.FollowUnitId = event.targetUnitId;
    effect.Pos = { 0.0f, 0.0f, ROLE_STATUS_EFT_Z_OFFSET };
    effect.VisualEffectId = event.effectId;
    effect.Path = std::format("eft/eft{:03}", event.effectId);
    effect.TotalEffectFrame = effects.effectFrameCount(effect.Path);
    effect.TotalFrame = event.durationFrames > 0
        ? std::max(event.durationFrames, effect.TotalEffectFrame)
        : std::max(1, effect.TotalEffectFrame);
    effect.Frame = 0;
    effect.VisualOnly = 1;
    effect.VisualTeam = unit->team;
    bindings_.attackEffects.push_back(std::move(effect));
}

template <class Effects>
void BattleSceneFrameApplier::spawnDamageNumber(
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    auto unit = resolveUnitView(event.targetUnitId);
    assert(unit);
    assert(event.amount > 0);

    BattleTextEffect effect;
    effect.Text = std::to_string(-event.amount);
    effect.color = BattleSceneFrameApplierDetail::toSceneColor(event.color);
    effect.Pos = BattleSceneFrameApplierDetail::floatingTextPositionFor(unit->position, effect.Text, effects);
    effect.Size = BattleSceneFrameApplierDetail::damageTextSize(event.textSize, event.amount, unit->maxHp);
    effect.color.a = BattleSceneFrameApplierDetail::damageTextAlpha(event.amount, unit->maxHp);
    bindings_.textEffects.push_back(std::move(effect));
}

template <class Effects>
void BattleSceneFrameApplier::spawnProjectile(
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    auto* existing = findProjectile(event.effectId);
    auto& effect = existing ? *existing : createProjectile(event);
    applyProjectilePayload(effect, event, effects);
}

template <class Effects>
void BattleSceneFrameApplier::moveProjectile(
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    auto* existing = findProjectile(event.effectId);
    auto& effect = existing ? *existing : createProjectile(event);
    applyProjectilePayload(effect, event, effects);
}

template <class Effects>
void BattleSceneFrameApplier::impactProjectile(
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    auto* existing = findProjectile(event.effectId);
    auto& effect = existing ? *existing : createProjectile(event);
    applyProjectilePayload(effect, event, effects);
    if (!effect.Through)
    {
        BattleSceneFrameApplierDetail::finishProjectile(effect, 15);
    }
}

template <class Effects>
void BattleSceneFrameApplier::expireProjectile(
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    auto* existing = findProjectile(event.effectId);
    auto& effect = existing ? *existing : createProjectile(event);
    applyProjectilePayload(effect, event, effects);
    BattleSceneFrameApplierDetail::finishProjectile(effect, 5);
}

template <class Effects>
void BattleSceneFrameApplier::bounceProjectile(
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    auto* existing = findProjectile(event.effectId);
    auto& effect = existing ? *existing : createProjectile(event);
    applyProjectilePayload(effect, event, effects);
    BattleSceneFrameApplierDetail::finishProjectile(effect, 15);
}

template <class Effects>
void BattleSceneFrameApplier::applyFrameEffects(
    const KysChess::Battle::BattlePresentationFrame& frame,
    Effects& effects) const
{
    for (int soundId : frame.attackSoundIds)
    {
        effects.playAttackSound(soundId);
    }
    for (const auto& rumble : frame.rumbles)
    {
        effects.rumble(rumble.lowFrequency, rumble.highFrequency, rumble.durationMs);
    }
    for (int i = 0; i < frame.blinkSoundCount; ++i)
    {
        effects.playEffectSound(BattleSceneFrameApplierDetail::BLINK_SOUND_EFFECT_ID);
    }
}

template <class Effects>
void BattleSceneFrameApplier::applyProjectilePayload(
    BattleAttackEffect& effect,
    const KysChess::Battle::BattleVisualEvent& event,
    Effects& effects) const
{
    effect.VisualOnly = 1;
    effect.VisualTeam = resolveVisualTeam(event.sourceUnitId);
    effect.Through = event.through ? 1 : 0;
    effect.Pos = event.position;
    effect.Velocity = event.velocity;
    effect.TotalFrame = std::max(1, event.durationFrames);
    if (event.visualEffectId >= 0)
    {
        if (effect.VisualEffectId != event.visualEffectId)
        {
            effect.VisualEffectId = event.visualEffectId;
            effect.Path = std::format("eft/eft{:03}", event.visualEffectId);
            effect.TotalEffectFrame = effects.effectFrameCount(effect.Path);
        }
        effect.TotalFrame = std::max(effect.TotalFrame, effect.TotalEffectFrame);
    }
}
