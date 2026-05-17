#include "BattleSceneCamera.h"

#include "BattleSceneRenderMath.h"
#include "Engine.h"
#include "SystemSettings.h"

#include <algorithm>
#include <cassert>

namespace
{
constexpr float CameraAutoFollowLerp = 1.0f;
constexpr float CameraDeathFocusLerp = 1.0f;
constexpr float CameraDeathRetargetBlend = 0.0f;

Pointf lerpPoint(const Pointf& from, const Pointf& to, float t)
{
    t = (std::max)(0.0f, (std::min)(t, 1.0f));
    return {
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.z + (to.z - from.z) * t,
    };
}
}  // namespace

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

Pointf BattleSceneCamera::updateAuto(
    const Pointf& center,
    const BattleSceneUnitStore& units,
    BattleSceneCameraBounds bounds)
{
    Pointf desired = target_;
    bool haveDesired = false;

    if (closeUp_ <= 0)
    {
        Pointf allyCenter{ 0, 0, 0 };
        int count = 0;
        for (const auto& unit : units.runtimeUnits())
        {
            if (unit.team == 0 && unit.alive)
            {
                allyCenter += unit.motion.position;
                count++;
            }
        }

        if (count > 0)
        {
            allyCenter.x /= count;
            allyCenter.y /= count;
            allyCenter.z /= count;
            desired = allyCenter;
            haveDesired = true;
        }
    }
    else
    {
        haveDesired = true;
    }

    if (haveDesired)
    {
        target_ = desired;
    }

    float lerp = closeUp_ > 0 ? CameraDeathFocusLerp : CameraAutoFollowLerp;
    return clampCenter(lerpPoint(center, target_, lerp), bounds);
}

void BattleSceneCamera::focusOn(const Pointf& focusPoint, int zoomFrames)
{
    if (zoomFrames <= 0)
    {
        return;
    }

    if (closeUp_ <= 0)
    {
        target_ = focusPoint;
    }
    else
    {
        target_ = lerpPoint(target_, focusPoint, CameraDeathRetargetBlend);
    }

    closeUp_ = std::max(closeUp_, zoomFrames);
    closeUpTotal_ = std::max(closeUpTotal_, closeUp_);
}

void BattleSceneCamera::reset(Pointf center)
{
    manualDragging_ = false;
    target_ = center;
    closeUpTotal_ = 0;
}

void BattleSceneCamera::decreaseCloseUp()
{
    BattleSceneRenderMath::decreaseToZero(closeUp_);
}

Pointf BattleSceneCamera::clampCenter(Pointf center, BattleSceneCameraBounds bounds)
{
    assert(bounds.sceneWidth > 0.0f);
    assert(bounds.sceneHeight > 0.0f);

    float minX = bounds.renderCenterX;
    float maxX = bounds.sceneWidth - bounds.renderCenterX;
    if (maxX <= minX)
    {
        center.x = bounds.sceneWidth * 0.5f;
    }
    else
    {
        center.x = (std::max)(minX, (std::min)(center.x, maxX));
    }

    float centerY = center.y * 0.5f;
    float minY = bounds.renderCenterY;
    float maxY = bounds.sceneHeight - bounds.renderCenterY;
    if (maxY <= minY)
    {
        centerY = bounds.sceneHeight * 0.5f;
    }
    else
    {
        centerY = (std::max)(minY, (std::min)(centerY, maxY));
    }
    center.y = centerY * 2.0f;
    return center;
}

bool BattleSceneCamera::closeUpActive() const
{
    return closeUp_ > 0;
}

int BattleSceneCamera::closeUpFrames() const
{
    return closeUp_;
}

int BattleSceneCamera::closeUpTotalFrames() const
{
    return closeUpTotal_;
}
