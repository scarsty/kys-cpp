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

std::shared_ptr<SuperMenuText> makeIndexedMenu(
    const std::string& title,
    const IndexedMenuData& data,
    IndexedMenuConfig config,
    const std::vector<std::shared_ptr<DrawableOnCall>>& drawables,
    const std::vector<Chess>& itemPreviewData)
{
    auto menu = std::make_shared<SuperMenuText>(title, config.fontSize, data.labels, config.perPage, makeMenuOptions(data, config));
    menu->setInputPosition(config.x, config.y);

    if (!itemPreviewData.empty())
    {
        auto statusDrawable = std::make_shared<UIStatusDrawable>(itemPreviewData);
        auto frame = ChessScreenLayout::shopStatusPanel();
        statusDrawable->getUIStatus().setPosition(frame.x, frame.y);
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