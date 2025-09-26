#include "UIStatusDrawable.h"
#include "UIRoleStatusMenu.h"

UIRoleStatusMenu::UIRoleStatusMenu(const std::vector<std::pair<int, std::string>>& rolePairs,
                                   const std::vector<Color>& roleColors)
    : SuperMenuText("╫ги╚ап╠М", 24, rolePairs, static_cast<int>(rolePairs.size()), roleColors)
{
    statusDrawable_ = std::make_shared<UIStatusDrawable>();
    addDrawableOnCall(statusDrawable_);
}