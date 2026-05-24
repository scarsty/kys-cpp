#include "BattleSceneCamera.h"

#include "Engine.h"
#include "SystemSettings.h"

std::optional<Pointf> BattleSceneCamera::handleManualInput(
    const EngineEvent& event,
    const Pointf& center,
    BattleSceneCameraBounds bounds)
{
    if (!SystemSettings::getInstance()->data().manualCamera)
    {
        manualDragging_ = false;
        return std::nullopt;
    }

    auto* engine = Engine::getInstance();
    switch (event.type)
    {
    case EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == BUTTON_LEFT)
        {
            int presentX = 0;
            int presentY = 0;
            int presentW = 0;
            int presentH = 0;
            engine->getPresentRect(presentX, presentY, presentW, presentH);
            manualDragging_ = presentW > 0
                && presentH > 0
                && event.button.x >= presentX
                && event.button.x < presentX + presentW
                && event.button.y >= presentY
                && event.button.y < presentY + presentH;
        }
        break;
    case EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == BUTTON_LEFT)
        {
            manualDragging_ = false;
        }
        break;
    case EVENT_MOUSE_MOTION:
        if (manualDragging_)
        {
            int presentX = 0;
            int presentY = 0;
            int presentW = 0;
            int presentH = 0;
            engine->getPresentRect(presentX, presentY, presentW, presentH);
            if (presentW > 0 && presentH > 0)
            {
                double worldDeltaX = static_cast<double>(event.motion.xrel) * (bounds.renderCenterX * 2.0 / presentW);
                double worldDeltaY = static_cast<double>(event.motion.yrel) * (bounds.renderCenterY * 2.0 / presentH);
                Pointf nextCenter = center;
                nextCenter.x -= static_cast<float>(worldDeltaX);
                nextCenter.y -= static_cast<float>(worldDeltaY * 2.0);
                return clampCenter(nextCenter, bounds);
            }
        }
        break;
    }
    return std::nullopt;
}
