#include "BattlePresentation.h"

#include <cassert>

namespace KysChess::Battle
{

namespace
{
void assertValidEvent(const BattlePresentationEvent& event)
{
    assert(event.frame == BattlePresentationCurrentFrame || event.frame >= 0);
    switch (event.type)
    {
    case BattlePresentationEventType::DamageLog:
    case BattlePresentationEventType::HealLog:
        assert(event.amount >= 0);
        break;
    case BattlePresentationEventType::StatusLog:
        assert(!event.text.empty());
        break;
    case BattlePresentationEventType::FloatingText:
        assert(!event.text.empty());
        assert(event.textSize > 0);
        break;
    case BattlePresentationEventType::RoleEffect:
        assert(event.targetUnitId >= 0);
        assert(event.effectId >= 0);
        break;
    case BattlePresentationEventType::DamageNumber:
        assert(event.targetUnitId >= 0);
        assert(event.amount > 0);
        assert(event.textSize > 0);
        break;
    case BattlePresentationEventType::CameraFocus:
        assert(event.durationFrames >= 0);
        break;
    case BattlePresentationEventType::ProjectileMoved:
        assert(event.effectId >= 0);
        break;
    case BattlePresentationEventType::ProjectileHit:
        assert(event.effectId >= 0);
        assert(event.targetUnitId >= 0);
        break;
    case BattlePresentationEventType::ProjectileExpired:
    case BattlePresentationEventType::ProjectileTargetLost:
        assert(event.effectId >= 0);
        break;
    case BattlePresentationEventType::ProjectileCancelled:
        assert(event.effectId >= 0);
        assert(event.amount >= 0);
        break;
    case BattlePresentationEventType::ProjectileBounced:
        assert(event.effectId >= 0);
        assert(event.targetUnitId >= 0);
        assert(event.amount >= 0);
        break;
    }
}
}  // namespace

void BattlePresentationRecorder::beginFrame(BattlePresentationSnapshot snapshot)
{
    frame_.snapshot = std::move(snapshot);
    frame_.events.clear();
}

void BattlePresentationRecorder::record(BattlePresentationEvent event)
{
    assertValidEvent(event);
    if (event.frame == BattlePresentationCurrentFrame)
    {
        event.frame = frame_.snapshot.frame;
    }
    frame_.events.push_back(std::move(event));
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
