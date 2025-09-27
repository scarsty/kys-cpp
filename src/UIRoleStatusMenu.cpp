#include "UIStatusDrawable.h"
#include "UIRoleStatusMenu.h"

UIRoleStatusMenu::UIRoleStatusMenu(const std::vector<std::pair<int, std::string>>& rolePairs,
                                   const std::vector<Color>& roleColors, int itemsPerPage)
    : SuperMenuText("角色列表", 24, rolePairs, itemsPerPage, roleColors)
{
    statusDrawable_ = std::make_shared<UIStatusDrawable>();
    addDrawableOnCall(statusDrawable_);
}