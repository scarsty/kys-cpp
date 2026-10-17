#pragma once

#include "Engine.h"
#include "Font.h"
#include "ChessUiStyle.h"

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

struct ShopPanelLayout
{
    PanelFrame owned;
    PanelFrame combo;
    PanelFrame status;
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
inline constexpr int kDefaultMenuItemUnits = kMenuNameUnits + kMenuStarUnits + kMenuCostUnits;
inline constexpr int kComboPanelMinimumFontSize = 10;

inline PanelFrame fullContentRegion()
{
    return {16, 80, 1248, 560};
}

inline PanelFrame browseDetailRegion()
{
    return {416, 80, 848, 560};
}

inline ShopPanelLayout characterDetails()
{
    return {{16, 490, 384, 150}, {416, 430, 848, 210}, {416, 80, 848, 342}};
}

inline PanelFrame largeModalPreviewPanel()
{
    return browseDetailRegion();
}

inline PanelFrame positionSwapPanel()
{
    return browseDetailRegion();
}

inline void drawPanel(
    const PanelFrame& frame,
    Color fillColor = ChessUiStyle::panelFill,
    int radius = 6)
{
    Engine::getInstance()->fillRoundedRect(fillColor, frame.x, frame.y, frame.w, frame.h, radius);
}

}    // namespace ChessScreenLayout

}    // namespace KysChess
