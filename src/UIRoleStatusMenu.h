#pragma once
#include "SuperMenuText.h"
#include "UIStatusDrawable.h"
#include <vector>
#include <memory>

class UIRoleStatusMenu : public SuperMenuText {
public:
    UIRoleStatusMenu(const std::vector<std::pair<int, std::string>>& rolePairs,
                     const std::vector<Color>& roleColors, int itemsPerPage);
    UIStatusDrawable& getStatusDrawable() { return *statusDrawable_; }

private:
    std::shared_ptr<UIStatusDrawable> statusDrawable_;
};