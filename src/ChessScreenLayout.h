#pragma once

namespace KysChess
{

struct PanelFrame
{
    int x;
    int y;
    int w;
    int h;
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
}    // namespace ChessScreenLayout

}    // namespace KysChess
