#pragma once

#include "Engine.h"
#include "Point.h"
#include "Types.h"

#include <algorithm>
#include <array>
#include <cassert>

namespace BattleSceneRenderMath
{
inline constexpr int FightStyleCount = 5;
inline constexpr int HurtFlashPeriod = 3;

inline constexpr int FightStyleFallbacks[FightStyleCount][FightStyleCount - 1] = {
    { 1, 2, 3, 4 },
    { 4, 2, 3, 0 },
    { 3, 1, 4, 0 },
    { 2, 1, 4, 0 },
    { 1, 2, 3, 0 },
};

inline Pointf realTowardsFromFaceTowards(int faceTowards)
{
    switch (faceTowards)
    {
    case Towards_RightDown:
        return { 1, 1, 0 };
    case Towards_RightUp:
        return { 1, -1, 0 };
    case Towards_LeftDown:
        return { -1, 1, 0 };
    case Towards_LeftUp:
        return { -1, -1, 0 };
    default:
        assert(false);
        return {};
    }
}

inline int firstAvailableRenderFightStyle(const std::array<int, FightStyleCount>& fightFrames)
{
    for (int style = 0; style < FightStyleCount; ++style)
    {
        if (fightFrames[style] > 0)
        {
            return style;
        }
    }
    return -1;
}

inline int resolveRenderFightStyle(const std::array<int, FightStyleCount>& fightFrames, int style)
{
    if (style >= 0 && style < FightStyleCount && fightFrames[style] > 0)
    {
        return style;
    }
    if (style >= 0 && style < FightStyleCount)
    {
        for (int fallback : FightStyleFallbacks[style])
        {
            if (fightFrames[fallback] > 0)
            {
                return fallback;
            }
        }
    }
    return firstAvailableRenderFightStyle(fightFrames);
}

inline int calRenderUnitPic(
    const std::array<int, FightStyleCount>& fightFrames,
    const Pointf& realTowards,
    int style,
    int frame)
{
    const int faceTowards = realTowardsToFaceTowards(realTowards);
    if (style < 0 || style >= FightStyleCount)
    {
        style = resolveRenderFightStyle(fightFrames, -1);
        return style >= 0 ? fightFrames[style] * faceTowards : faceTowards;
    }

    style = resolveRenderFightStyle(fightFrames, style);
    if (style < 0)
    {
        return faceTowards;
    }

    int total = 0;
    for (int i = 0; i < FightStyleCount; ++i)
    {
        if (i == style)
        {
            const int frameCount = fightFrames[style];
            const int lastFrame = frameCount - 1;
            const int clampedFrame = frame < lastFrame ? frame : lastFrame;
            return total + frameCount * faceTowards + clampedFrame;
        }
        total += fightFrames[i] * 4;
    }
    return faceTowards;
}

inline void decreaseToZero(int& value)
{
    if (value > 0)
    {
        --value;
    }
}

template <typename T>
void decreaseToZero(T& value, T step)
{
    if (value > 0)
    {
        value -= step;
    }
    if (value < 0)
    {
        value = 0;
    }
}

inline Color hurtFlashColor(int timer, const Color& baseColor)
{
    if (timer <= 0)
    {
        return baseColor;
    }

    const int phase = timer % HurtFlashPeriod;
    if (phase < 2)
    {
        return { 255, 100, 100, baseColor.a };
    }
    return baseColor;
}

struct BattleCursorFloorDraw
{
    bool shouldDraw{};
    Color color{};
};

inline BattleCursorFloorDraw battleCursorFloorDraw(
    bool cursorActive,
    bool actionMode,
    bool selectable,
    bool hasEffect,
    bool selected,
    bool cachedGroundTexture)
{
    BattleCursorFloorDraw draw;
    draw.shouldDraw = !cachedGroundTexture;
    draw.color = { 255, 255, 255, 255 };

    if (!cursorActive)
    {
        return draw;
    }

    draw.shouldDraw = selectable || !cachedGroundTexture;
    draw.color = selectable
        ? Color{ 128, 128, 128, 255 }
        : Color{ 64, 64, 64, 255 };

    if (actionMode && hasEffect)
    {
        draw.color = selectable
            ? Color{ 192, 192, 192, 255 }
            : Color{ 160, 160, 160, 255 };
    }

    if (selected)
    {
        draw.shouldDraw = true;
        draw.color = { 255, 255, 255, 255 };
    }
    return draw;
}

inline Point windowPointToUiPoint(const Point& windowPoint, const Rect& presentRect, int uiWidth, int uiHeight)
{
    assert(presentRect.w > 0);
    assert(presentRect.h > 0);
    assert(uiWidth > 0);
    assert(uiHeight > 0);
    return {
        (windowPoint.x - presentRect.x) * uiWidth / presentRect.w,
        (windowPoint.y - presentRect.y) * uiHeight / presentRect.h,
    };
}

inline Pointf clampPaperCameraCenter(Pointf center, float groundWidth, float groundHeight)
{
    assert(groundWidth > 0.0f);
    assert(groundHeight > 0.0f);
    center.x = std::clamp(center.x, 0.0f, groundWidth);
    center.y = std::clamp(center.y, 0.0f, groundHeight);
    center.z = 0.0f;
    return center;
}

inline float paperRoleInfoAnchorZ(int textureDy)
{
    constexpr float RoleInfoMinAnchorZ = 42.0f;
    constexpr float RoleInfoMaxAnchorZ = 64.0f;
    constexpr float RoleInfoAnchorScale = 0.45f;
    constexpr float RoleInfoAnchorPadding = 8.0f;
    return std::clamp(
        std::max(0.0f, static_cast<float>(textureDy)) * RoleInfoAnchorScale + RoleInfoAnchorPadding,
        RoleInfoMinAnchorZ,
        RoleInfoMaxAnchorZ);
}

inline float paperFloatingTextAnchorZ()
{
    return 80.0f;
}

inline std::array<Pointf, 4> paperFloorQuad(
    Pointf p00,
    Pointf p10,
    Pointf p11,
    Pointf p01,
    float z)
{
    p00.z = z;
    p10.z = z;
    p11.z = z;
    p01.z = z;
    return { p00, p10, p11, p01 };
}
}  // namespace BattleSceneRenderMath
