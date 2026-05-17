#include "BattlePresentation.h"

#include <cassert>

namespace KysChess::Battle
{

namespace
{
void stampFrame(int& eventFrame, int snapshotFrame)
{
    assert(eventFrame == BattlePresentationCurrentFrame || eventFrame >= 0);
    if (eventFrame == BattlePresentationCurrentFrame)
    {
        eventFrame = snapshotFrame;
    }
}

void assertValidEvent(const BattleGameplayEvent& event)
{
    assert(event.frame == BattlePresentationCurrentFrame || event.frame >= 0);
    switch (event.type)
    {
    case BattleGameplayEventType::CastStarted:
        assert(event.sourceUnitId >= 0);
        break;
    case BattleGameplayEventType::AttackSpawned:
        assert(event.effectId >= 0);
        assert(event.sourceUnitId >= 0);
        break;
    case BattleGameplayEventType::ProjectileMoved:
    case BattleGameplayEventType::ProjectileExpired:
    case BattleGameplayEventType::ProjectileCancelled:
        assert(event.effectId >= 0);
        break;
    case BattleGameplayEventType::ProjectileHit:
        assert(event.effectId >= 0);
        assert(event.targetUnitId >= 0);
        break;
    case BattleGameplayEventType::DamageApplied:
        assert(event.targetUnitId >= 0);
        assert(event.amount >= 0);
        break;
    case BattleGameplayEventType::StatusApplied:
        assert(event.targetUnitId >= 0);
        assert(!event.text.empty());
        break;
    case BattleGameplayEventType::ResourceChanged:
        assert(event.targetUnitId >= 0);
        break;
    case BattleGameplayEventType::UnitDied:
        assert(event.targetUnitId >= 0);
        break;
    case BattleGameplayEventType::BattleEnded:
        break;
    }
}

void assertValidEvent(const BattleLogEvent& event)
{
    assert(event.frame == BattlePresentationCurrentFrame || event.frame >= 0);
    switch (event.type)
    {
    case BattleLogEventType::Damage:
    case BattleLogEventType::Heal:
        assert(event.amount >= 0);
        break;
    case BattleLogEventType::Status:
        assert(!event.segments.empty());
        break;
    case BattleLogEventType::UnitDied:
        assert(event.targetUnitId >= 0);
        break;
    case BattleLogEventType::BattleEnded:
        break;
    }
}

void assertValidEvent(const BattleVisualEvent& event)
{
    assert(event.frame == BattlePresentationCurrentFrame || event.frame >= 0);
    switch (event.type)
    {
    case BattleVisualEventType::FloatingText:
        assert(!event.text.empty());
        assert(event.textSize > 0);
        break;
    case BattleVisualEventType::RoleEffect:
        assert(event.targetUnitId >= 0);
        assert(event.effectId >= 0);
        break;
    case BattleVisualEventType::DamageNumber:
        assert(event.targetUnitId >= 0);
        assert(event.amount > 0);
        assert(event.textSize > 0);
        break;
    case BattleVisualEventType::CameraFocus:
        assert(event.durationFrames >= 0);
        break;
    case BattleVisualEventType::ProjectileSpawned:
    case BattleVisualEventType::ProjectileMoved:
        assert(event.effectId >= 0);
        assert(event.durationFrames >= 0);
        break;
    case BattleVisualEventType::ProjectileHit:
        assert(event.effectId >= 0);
        assert(event.targetUnitId >= 0);
        break;
    case BattleVisualEventType::ProjectileExpired:
    case BattleVisualEventType::ProjectileTargetLost:
        assert(event.effectId >= 0);
        break;
    case BattleVisualEventType::ProjectileCancelled:
        assert(event.effectId >= 0);
        assert(event.amount >= 0);
        break;
    case BattleVisualEventType::ProjectileBounced:
        assert(event.effectId >= 0);
        assert(event.targetUnitId >= 0);
        assert(event.amount >= 0);
        break;
    }
}
}  // namespace

void BattlePresentationRecorder::beginFrame(int frame)
{
    frame_ = {};
    frame_.frame = frame;
}

void BattlePresentationRecorder::recordGameplay(BattleGameplayEvent event)
{
    assertValidEvent(event);
    stampFrame(event.frame, frame_.frame);
    frame_.gameplayEvents.push_back(std::move(event));
}

void BattlePresentationRecorder::recordLog(BattleLogEvent event)
{
    assertValidEvent(event);
    stampFrame(event.frame, frame_.frame);
    frame_.logEvents.push_back(std::move(event));
}

void BattlePresentationRecorder::recordVisual(BattleVisualEvent event)
{
    assertValidEvent(event);
    stampFrame(event.frame, frame_.frame);
    frame_.visualEvents.push_back(std::move(event));
}

const BattlePresentationFrame& BattlePresentationRecorder::frame() const
{
    return frame_;
}

BattlePresentationFrame BattlePresentationRecorder::consumeFrame()
{
    auto consumed = std::move(frame_);
    frame_ = {};
    return consumed;
}

}  // namespace KysChess::Battle
