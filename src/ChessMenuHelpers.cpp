#include "ChessMenuHelpers.h"

#include "ChessDrawableOnCall.h"
#include "ChessScreenLayout.h"
#include "GameUtil.h"
#include "UIStatusDrawable.h"

namespace KysChess
{

SuperMenuTextExtraOptions makeMenuOptions(const IndexedMenuData& data, const IndexedMenuConfig& config)
{
    SuperMenuTextExtraOptions opts;
    opts.itemColors_ = data.colors;
    opts.outlineColors_ = config.outlineColors;
    opts.animateOutlines_ = config.animateOutlines;
    opts.outlineThicknesses_ = config.outlineThicknesses;
    opts.needInputBox_ = config.needInputBox;
    opts.confirmation_ = config.confirmation;
    opts.exitable_ = config.exitable;
    return opts;
}

ShopIndexedMenuSetup makeShopIndexedMenuSetup(const IndexedMenuData& data, IndexedMenuConfig config, int visibleRows)
{
    auto menuAnchor = ChessScreenLayout::shopMenuAnchor();
    config.x = menuAnchor.x;
    config.y = menuAnchor.y;

    int resolvedVisibleRows = std::max(1, visibleRows);
    auto panels = ChessScreenLayout::shopPanelsForMenu(menuAnchor, data.labels, config.fontSize, resolvedVisibleRows);
    config.previewFrame = panels.status;
    return {config, panels};
}

std::shared_ptr<SuperMenuText> makeChessMenu(
    const std::string& title,
    const ChessMenuData& data,
    IndexedMenuConfig config,
    const std::vector<std::shared_ptr<DrawableOnCall>>& drawables)
{
    return makeIndexedMenu(title, data, config, drawables, data.previewData);
}

std::shared_ptr<SuperMenuText> makeIndexedMenu(
    const std::string& title,
    const IndexedMenuData& data,
    IndexedMenuConfig config,
    const std::vector<std::shared_ptr<DrawableOnCall>>& drawables,
    const std::vector<Chess>& itemPreviewData)
{
    auto menu = std::make_shared<SuperMenuText>(title, config.fontSize, data.labels, config.perPage, makeMenuOptions(data, config));
    menu->setInputPosition(config.x, config.y);

    if (config.showPreviewStatus && !itemPreviewData.empty())
    {
        auto statusDrawable = std::make_shared<UIStatusDrawable>(itemPreviewData);
        PanelAnchor menuAnchor{config.x, config.y};
        auto previewFrame = config.previewFrame.value_or(ChessScreenLayout::browseDetailRegionForMenu(menuAnchor, data.labels, config.fontSize));
        statusDrawable->getUIStatus().setPosition(previewFrame.x, previewFrame.y);
        statusDrawable->getUIStatus().setSize(previewFrame.w, previewFrame.h);
        menu->addDrawableOnCall(statusDrawable);
    }

    for (auto& drawable : drawables)
    {
        if (!itemPreviewData.empty())
        {
            if (auto chessDrawable = std::dynamic_pointer_cast<ChessDrawableOnCall>(drawable))
            {
                chessDrawable->setPreviewData(itemPreviewData);
            }
        }
        menu->addDrawableOnCall(drawable);
    }

    if (!config.showNav)
    {
        menu->setShowNavigationButtons(false);
    }
    menu->setDoubleTapMode(GameUtil::isMobileDevice());
    return menu;
}

}    // namespace KysChess