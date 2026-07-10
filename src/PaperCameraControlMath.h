#pragma once

#include "Point.h"

struct PaperCameraGroundBasis
{
    Pointf forward{};
    Pointf screenRight{};
};

inline constexpr float PaperTouchPanScale = 1.0f;
inline constexpr float PaperTouchPinchScale = 1.0f;
inline constexpr float PaperTouchRotateScale = 1.0f;
inline constexpr float PaperTouchHeightScale = 1.0f;

PaperCameraGroundBasis paperCameraGroundBasis(float angle);
Pointf paperKeyboardPanVector(float angle, bool forward, bool backward, bool left, bool right);
Pointf paperTouchPanDelta(float angle, float cameraDistance, float cameraHeight, float fovDegrees, float uiHeight, float dx, float dy);
float paperTouchPinchDistance(float oldDistance, float previousSpan, float currentSpan);
float paperTouchRotationDelta(float centroidDx, float uiWidth);
float paperTouchHeightDelta(float centroidDy, float uiHeight, float minHeight, float maxHeight);
