#pragma once

#include "BattlePresentation.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string_view>

namespace KysChess::Battle
{

inline constexpr char BattleCueVisualPathPrefix[] = "chess-effects/cue-";
inline constexpr char BattleCuePositiveVisualPath[] = "chess-effects/cue-positive";
inline constexpr char BattleCueNegativeVisualPath[] = "chess-effects/cue-negative";
inline constexpr char BattleCueBleedVisualPath[] = "chess-effects/cue-bleed";
inline constexpr char BattleCueControlVisualPath[] = "chess-effects/cue-control";
inline constexpr char BattleCueCleanseVisualPath[] = "chess-effects/cue-cleanse";
inline constexpr char BattleAreaSandVisualPath[] = "chess-effects/area-sand";
inline constexpr char BattleAreaWardVisualPath[] = "chess-effects/area-ward";

inline constexpr std::array<std::string_view, 7> BattleEffectVisualPaths = {
    BattleCuePositiveVisualPath,
    BattleCueNegativeVisualPath,
    BattleCueBleedVisualPath,
    BattleCueControlVisualPath,
    BattleCueCleanseVisualPath,
    BattleAreaSandVisualPath,
    BattleAreaWardVisualPath,
};

inline constexpr int YellowSandWhipMagicId = 78;       // 黃沙萬里鞭
inline constexpr int DemonSubduingStaffMagicId = 86;   // 伏魔杖法

inline constexpr std::optional<BattleAreaVisualStyle> battleAreaVisualStyleForMagicId(int magicId)
{
    switch (magicId)
    {
    case YellowSandWhipMagicId: return BattleAreaVisualStyle::Sand;
    case DemonSubduingStaffMagicId: return BattleAreaVisualStyle::ProtectiveWard;
    default: return std::nullopt;
    }
}

inline constexpr const char* battleAreaVisualPath(BattleAreaVisualStyle style)
{
    switch (style)
    {
    case BattleAreaVisualStyle::Sand: return BattleAreaSandVisualPath;
    case BattleAreaVisualStyle::ProtectiveWard: return BattleAreaWardVisualPath;
    }
    assert(false);
    return "";
}

inline constexpr std::uint8_t battleAreaVisualAlpha(
    const BattleAreaPresentation& area,
    int frame)
{
    constexpr int FadeFrames = 6;
    const int baseAlpha = area.style == BattleAreaVisualStyle::Sand ? 166 : 140;
    const int fadeIn = std::clamp(frame - area.createdFrame + 1, 0, FadeFrames);
    const int fadeOut = std::clamp(area.expiresFrameExclusive - frame, 0, FadeFrames);
    return static_cast<std::uint8_t>(baseAlpha * std::min(fadeIn, fadeOut) / FadeFrames);
}

}  // namespace KysChess::Battle
