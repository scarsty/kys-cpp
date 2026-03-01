#pragma once
#include "SuperMenuText.h"
#include "UIStatusDrawable.h"
#include <vector>
#include <memory>

class UIRoleStatusMenu : public SuperMenuText {
public:
    UIRoleStatusMenu(const std::string& title, const std::vector<std::pair<int, std::string>>& rolePairs,
                     const std::vector<Color>& roleColors, int itemsPerPage, int fontSize = 24, bool needsConfirmation = true, bool exitable = true,
                     const std::vector<Color>& outlineColors = {}, const std::vector<bool>& animateOutlines = {},
                     const std::vector<int>& outlineThicknesses = {});
    UIStatusDrawable& getStatusDrawable() { return *statusDrawable_; }

private:
    std::shared_ptr<UIStatusDrawable> statusDrawable_;
};