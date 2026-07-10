#include "PaperCameraControlMath.h"

#include <cassert>
#include <cmath>
#include <numbers>

namespace
{
Pointf normalized(Pointf value)
{
    assert(value.norm() > 0.0f);
    value.normTo(1.0f);
    return value;
}
}

PaperCameraGroundBasis paperCameraGroundBasis(float angle)
{
    PaperCameraGroundBasis basis;
    basis.forward = normalized({-std::cos(angle), -std::sin(angle), 0.0f});
    basis.screenRight = normalized({basis.forward.y, -basis.forward.x, 0.0f});
    return basis;
}

Pointf paperKeyboardPanVector(float angle, bool forward, bool backward, bool left, bool right)
{
    const auto basis = paperCameraGroundBasis(angle);
    Pointf value;
    if (forward) value += basis.forward;
    if (backward) value += -basis.forward;
    if (left) value += basis.screenRight;
    if (right) value += -basis.screenRight;
    return value;
}

Pointf paperTouchPanDelta(float angle, float cameraDistance, float cameraHeight, float fovDegrees, float uiHeight, float dx, float dy)
{
    assert(uiHeight > 0.0f);
    const auto basis = paperCameraGroundBasis(angle);
    const float fovRadians = fovDegrees * std::numbers::pi_v<float> / 180.0f;
    const float worldPerUiPixel = 2.0f * std::hypot(cameraDistance, cameraHeight)
        * std::tan(fovRadians * 0.5f) / uiHeight;
    const Pointf gestureDelta{
        -basis.screenRight.x * dx + basis.forward.x * dy,
        -basis.screenRight.y * dx + basis.forward.y * dy,
        0.0f,
    };
    const float scale = worldPerUiPixel * PaperTouchPanScale;
    return {gestureDelta.x * scale, gestureDelta.y * scale, gestureDelta.z * scale};
}

float paperTouchPinchDistance(float oldDistance, float previousSpan, float currentSpan)
{
    assert(oldDistance > 0.0f && previousSpan > 0.0f && currentSpan > 0.0f);
    return oldDistance * std::pow(previousSpan / currentSpan, PaperTouchPinchScale);
}

float paperTouchRotationDelta(float centroidDx, float uiWidth)
{
    assert(uiWidth > 0.0f);
    return centroidDx / uiWidth * std::numbers::pi_v<float> * PaperTouchRotateScale;
}

float paperTouchHeightDelta(float centroidDy, float uiHeight, float minHeight, float maxHeight)
{
    assert(uiHeight > 0.0f && maxHeight > minHeight);
    return -centroidDy / uiHeight * (maxHeight - minHeight) * PaperTouchHeightScale;
}
