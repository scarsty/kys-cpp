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

struct ShopPanelLayout
{
    PanelFrame owned;
    PanelFrame combo;
    PanelFrame status;
    int menuWidth;
};

namespace ChessScreenLayout
{

// Menu formatting budget (in Font display-units). These define the
// conservative minimum allocation for menu item regions so anchors and
// formatters agree on layout. If a name or cost exceeds these values the
// formatter will expand the regions to fit the actual text.
inline constexpr int kMenuNameUnits = 16;
inline constexpr int kMenuStarUnits = 8;
inline constexpr int kMenuCostUnits = 6;
inline int getDefaultMenuItemUnits() { return kMenuNameUnits + kMenuStarUnits + kMenuCostUnits; }

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
    // Ensure we honor the minimum menu-item budget so formatters and anchors
    // agree on menu widths even when labels are padded to a fixed budget.
    maxUnits = std::max(maxUnits, getDefaultMenuItemUnits());
    return maxUnits * fontSize / 2 + extraPadding;
}

inline int estimateMenuBoxWidth(const std::vector<std::string>& labels, int fontSize)
{
    int maxUnits = 0;
    for (const auto& label : labels)
    {
        maxUnits = std::max(maxUnits, Font::getTextDrawSize(label));
    }
    maxUnits = std::max(maxUnits, getDefaultMenuItemUnits());
    return Font::getBoxSize(maxUnits, fontSize, 0, 0).w;
}

inline int menuRowHeight(int fontSize)
{
    return Engine::uiStyle() == 1 ? fontSize + 9 : fontSize + 1;
}

inline ShopPanelLayout shopPanelsForMenu(const PanelAnchor& menuAnchor, const std::vector<std::string>& labels, int fontSize, int visibleRows = 8)
{
    auto region = fullContentRegion();
    const int outerPadding = 10;
    const int panelGap = 20;
    const int panelTopGap = 12;
    const int statusTop = region.y + 10;

    int menuWidth = estimateMenuBoxWidth(labels, fontSize);
    int selectionsY = menuAnchor.y + fontSize + 16;
    int panelsY = selectionsY + menuRowHeight(fontSize) * visibleRows + panelTopGap;
    int bottom = region.y + region.h - outerPadding;

    PanelFrame owned{region.x, panelsY, menuWidth, bottom - panelsY};
    int comboX = owned.x + owned.w + panelGap;
    int comboW = region.x + region.w - outerPadding - comboX;
    PanelFrame combo{comboX, panelsY, comboW, owned.h};
    PanelFrame status{comboX, statusTop, comboW, panelsY - panelGap - statusTop};
    return {owned, combo, status, menuWidth};
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

inline PanelAnchor battleSeedRerollMenuAnchor()
{
    return buyExpMenuAnchor();
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
    // Align combo/info panel vertically with the owned panel so their tops
    // and heights match for a tidy visual alignment.
    int y = owned.y;
    int h = owned.h;
    int w = region.x + region.w - x - 10;
    return {x, y, w, h};
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

inline PanelFrame battleSeedRerollPreviewPanel()
{
    return buyExpPreviewPanel();
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