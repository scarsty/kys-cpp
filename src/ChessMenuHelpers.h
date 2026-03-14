#pragma once

#include "ChessSelectorPresenter.h"
#include "DrawableOnCall.h"
#include "ChessScreenLayout.h"
#include "SuperMenuText.h"

#include <memory>
#include <optional>
#include <vector>

namespace KysChess
{

struct IndexedMenuConfig
{
    int perPage = 10;
    int fontSize = 36;
    int x = 80;
    int y = 70;
    bool showNav = true;
    bool needInputBox = false;
    bool confirmation = false;
    bool exitable = true;
    std::optional<PanelFrame> previewFrame;
    std::vector<Color> outlineColors;
    std::vector<bool> animateOutlines;
    std::vector<int> outlineThicknesses;
};

SuperMenuTextExtraOptions makeMenuOptions(const IndexedMenuData& data, const IndexedMenuConfig& config);
std::shared_ptr<SuperMenuText> makeIndexedMenu(
    const std::string& title,
    const IndexedMenuData& data,
    IndexedMenuConfig config = {},
    const std::vector<std::shared_ptr<DrawableOnCall>>& drawables = {},
    const std::vector<Chess>& itemPreviewData = {});

}    // namespace KysChess