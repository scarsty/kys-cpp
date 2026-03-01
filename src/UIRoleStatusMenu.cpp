#include "UIRoleStatusMenu.h"
#include "UIStatusDrawable.h"

UIRoleStatusMenu::UIRoleStatusMenu(const std::string& title, const std::vector<std::pair<int, std::string>>& rolePairs,
    const std::vector<Color>& roleColors, int itemsPerPage, int fontSize, bool needsConfirmation, bool exitable,
    const std::vector<Color>& outlineColors, const std::vector<bool>& animateOutlines,
    const std::vector<int>& outlineThicknesses) :
    SuperMenuText(title, fontSize, rolePairs, itemsPerPage,
        SuperMenuTextExtraOptions{ .itemColors_ = roleColors, .outlineColors_ = outlineColors, .animateOutlines_ = animateOutlines, .outlineThicknesses_ = outlineThicknesses, .needInputBox_ = false, .confirmation_ = needsConfirmation, .exitable_ = exitable, .returnIdxOnly = true })
{
    statusDrawable_ = std::make_shared<UIStatusDrawable>();
    addDrawableOnCall(statusDrawable_);
}