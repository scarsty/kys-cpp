#pragma once

#include "Engine.h"

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
    return {210, 215};
}

inline PanelAnchor positionSwapMenuAnchor()
{
    return {170, 245};
}

inline PanelFrame shopOwnedPanel()
{
    auto region = fullContentRegion();
    return {region.x, region.y + 385, 670, 235};
}

inline PanelFrame comboInfoPanel()
{
    auto status = shopStatusPanel();
    return {status.x, status.y + status.h + 15, status.w, 185};
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
    return {355, 165, 570, 300};
}

inline PanelFrame positionSwapPanel()
{
    return {300, 185, 680, 270};
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