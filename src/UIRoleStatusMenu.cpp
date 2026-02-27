#include "UIRoleStatusMenu.h"
#include "UIStatusDrawable.h"

UIRoleStatusMenu::UIRoleStatusMenu(const std::string& title, const std::vector<std::pair<int, std::string>>& rolePairs,
    const std::vector<Color>& roleColors, int itemsPerPage, int fontSize, bool needsConfirmation, bool exitable) :
    SuperMenuText(title, fontSize, rolePairs, itemsPerPage,
        SuperMenuTextExtraOptions{ .itemColors_ = roleColors, .needInputBox_ = false, .confirmation_ = needsConfirmation, .exitable_ = exitable, .returnIdxOnly = true })
{
    statusDrawable_ = std::make_shared<UIStatusDrawable>();
    addDrawableOnCall(statusDrawable_);
}