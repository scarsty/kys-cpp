#include "BattleSceneCamera.h"

#include "Engine.h"

std::optional<Pointf> BattleSceneCamera::handleManualInput(
    const PointerEvent& event,
    const Pointf& center,
    BattleSceneCameraBounds bounds,
    bool enabled)
{
    if (!enabled)
    {
        manualDragging_ = false;
        return std::nullopt;
    }

    switch (event.phase)
    {
    case PointerPhase::ButtonDown:
        if (event.button == BUTTON_LEFT)
        {
            manualDragging_ = event.insidePresent;
        }
        break;
    case PointerPhase::ButtonUp:
    case PointerPhase::Cancel:
        if (event.button == BUTTON_LEFT)
        {
            manualDragging_ = false;
        }
        break;
    case PointerPhase::Motion:
        if (manualDragging_)
        {
            const auto& geometry = PointerInput::instance().presentGeometry();
            const double worldDeltaX = event.uiDelta.x * (bounds.renderCenterX * 2.0 / geometry.uiWidth);
            const double worldDeltaY = event.uiDelta.y * (bounds.renderCenterY * 2.0 / geometry.uiHeight);
            Pointf nextCenter = center;
            nextCenter.x -= static_cast<float>(worldDeltaX);
            nextCenter.y -= static_cast<float>(worldDeltaY * 2.0);
            return clampCenter(nextCenter, bounds);
        }
        break;
    case PointerPhase::Wheel:
        break;
    }
    return std::nullopt;
}
