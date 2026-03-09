#pragma once

#include "Engine.h"
#include "Font.h"

#include <algorithm>
#include <string>
#include <vector>

namespace KysChess
{

struct PanelFrame
{
    int x;
    int y;
    int w;
    int h;
};

struct PanelAnchor
{
    int x;
    int y;
};

namespace ChessScreenLayout
{

inline PanelFrame fullContentRegion()
{
    return {55, 45, 1170, 630};
}

inline PanelFrame shopStatusPanel()
{
    auto region = fullContentRegion();
    return {region.x + 705, region.y + 10, 460, 430};
}

inline PanelFrame browseDetailRegion()
{
    auto region = fullContentRegion();
    return {region.x + 445, region.y + 10, 725, 610};
}

inline int estimateMenuWidth(const std::vector<std::string>& labels, int fontSize, int extraPadding = 48)
{
    int maxUnits = 0;
    for (const auto& label : labels)
    {
        maxUnits = std::max(maxUnits, Font::getTextDrawSize(label));
    }
    return maxUnits * fontSize / 2 + extraPadding;
}

inline PanelFrame browseDetailRegionForMenu(const PanelAnchor& menuAnchor, const std::vector<std::string>& labels, int fontSize)
{
    auto region = fullContentRegion();
    auto fallback = browseDetailRegion();
    int menuRight = menuAnchor.x + estimateMenuWidth(labels, fontSize);
    int left = std::max(fallback.x, menuRight + 40);
    int right = region.x + region.w - 10;
    int minWidth = 560;
    if (right - left < minWidth)
    {
        left = std::max(fallback.x, right - minWidth);
    }
    return {left, fallback.y, right - left, fallback.h};
}

inline PanelAnchor shopMenuAnchor()
{
    auto region = fullContentRegion();
    return {region.x + 5, region.y + 10};
}

inline PanelAnchor shopStatusAnchor()
{
    auto frame = shopStatusPanel();
    return {frame.x, frame.y};
}

inline PanelAnchor browseMenuAnchor()
{
    auto region = fullContentRegion();
    return {region.x + 5, region.y + 10};
}

inline PanelAnchor buyExpMenuAnchor()
{
    return {170, 255};
}

inline PanelAnchor positionSwapMenuAnchor()
{
    return {170, 255};
}

inline PanelFrame shopOwnedPanel()
{
    auto region = fullContentRegion();
    return {region.x, region.y + 385, 540, 235};
}

inline PanelFrame comboInfoPanel()
{
    auto region = fullContentRegion();
    auto owned = shopOwnedPanel();
    int x = owned.x + owned.w + 20;
    return {x, region.y + 455, region.x + region.w - x - 10, 175};
}

inline PanelFrame comboCatalogDetailPanel()
{
    return browseDetailRegion();
}

inline PanelFrame neigongDetailPanel()
{
    auto detail = browseDetailRegion();
    return {detail.x + 120, detail.y + 70, 540, 430};
}

inline PanelFrame equipmentDetailPanel()
{
    auto detail = browseDetailRegion();
    return {detail.x + 120, detail.y + 70, 540, 430};
}

inline PanelFrame challengeDetailPanel()
{
    auto region = fullContentRegion();
    return {region.x + 515, region.y + 10, 650, 610};
}

inline PanelFrame buyExpPreviewPanel()
{
    return {400, 180, 540, 300};
}

inline PanelFrame positionSwapPanel()
{
    return {420, 180, 540, 270};
}

inline PanelFrame guidePanel()
{
    return fullContentRegion();
}

inline void drawPanel(
    const PanelFrame& frame,
    Color fillColor = {0, 0, 0, 180},
    Color borderColor = {180, 170, 140, 200},
    int radius = 8)
{
    Engine::getInstance()->fillRoundedRect(fillColor, frame.x, frame.y, frame.w, frame.h, radius);
    Engine::getInstance()->drawRoundedRect(borderColor, frame.x, frame.y, frame.w, frame.h, radius);
}

}    // namespace ChessScreenLayout

}    // namespace KysChess