#pragma once

#include "BattlePresentation.h"
#include "../ChessEftIds.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <utility>

namespace KysChess::Battle
{

inline constexpr int YellowSandWhipMagicId = 78;       // 黃沙萬里鞭
inline constexpr int DemonSubduingStaffMagicId = 86;   // 伏魔杖法

inline constexpr std::optional<BattleAreaVisualStyle> battleAreaVisualStyleForMagicId(int magicId)
{
    switch (magicId)
    {
    case YellowSandWhipMagicId: return BattleAreaVisualStyle::Sand;
    case DemonSubduingStaffMagicId: return BattleAreaVisualStyle::ProtectiveWard;
    case 96: return BattleAreaVisualStyle::ProtectiveWard; // 羅漢伏魔功
    case 66: return BattleAreaVisualStyle::Fire; // 火焰刀法
    default: return std::nullopt;
    }
}

inline constexpr int battleAreaEffectId(BattleAreaVisualStyle style)
{
    switch (style)
    {
    case BattleAreaVisualStyle::Sand: return EFT_AREA_SAND;
    case BattleAreaVisualStyle::ProtectiveWard: return EFT_AREA_WARD;
    case BattleAreaVisualStyle::Fire: return EFT_AREA_FIRE;
    }
    assert(false);
    std::unreachable();
}

inline constexpr std::uint8_t battleAreaVisualAlpha(
    const BattleAreaPresentation& area,
    int frame)
{
    constexpr int FadeFrames = 6;
    int baseAlpha = area.style == BattleAreaVisualStyle::Sand ? 166 : 140;
    if (area.pulseIntervalFrames > 0)
    {
        const int phase = std::max(0, frame - area.createdFrame) % area.pulseIntervalFrames;
        const int pulseFrames = std::min(6, area.pulseIntervalFrames);
        baseAlpha = 110 + 70 * std::max(0, pulseFrames - phase) / pulseFrames;
    }
    const int fadeIn = std::clamp(frame - area.createdFrame + 1, 0, FadeFrames);
    const int fadeOut = std::clamp(area.expiresFrameExclusive - frame, 0, FadeFrames);
    return static_cast<std::uint8_t>(baseAlpha * std::min(fadeIn, fadeOut) / FadeFrames);
}

}  // namespace KysChess::Battle
