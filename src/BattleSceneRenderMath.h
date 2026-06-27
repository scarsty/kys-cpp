#pragma once

#include "Engine.h"
#include "Point.h"
#include "Types.h"

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
}  // namespace BattleSceneRenderMath
