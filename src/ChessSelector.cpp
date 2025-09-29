#include "ChessSelector.h"

#include <vector>
#include <string>
#include "Save.h"
#include <Engine.h>
#include <UIRoleStatusMenu.h>
#include "TempStore.h"
#include "ChessPool.h"

namespace KysChess
{

static GameData gameData;

void ChessSelector::getChess()
{
    // always get 5 pieces for now
    auto rollOfChess = getChessFromPool(gameData.getLevel(), 5);
    

    std::vector<std::pair<int, std::string>> rolePairs;
    std::vector<Color> roleColors;
    auto& roles = Save::getInstance()->getRoles();

    // To be moved later. Need to add color as well.
    // The level may need to be changed as well.
    // Need back and cancel button.
    auto starAnnotator = [&roles](Role* role, int star)
    {
        std::string name = role->Name;
        int size = name.size() / 3;
        while (size <= 5)
        {
            name += "  ";
            size += 1;
        }
        //for (int i = 0; i < star + 1; ++i)
        //{
        //    name += "★";
        //    size += 1;
        //}
        while (size < 10)
        {
            name += "  ";
            size += 1;
        }
        name += "-";
        name += std::to_string(star + 1);
        Color colors[] = {
            { 175, 238, 238 },
            { 0, 255, 0 },
            { 30, 144, 255 },
            { 75, 0, 130 },
            { 255, 165, 0 }
        };
        return std::pair{ name, colors[star] };
    };

    auto addRoleWithStar = [&](Role* role, int star)
    {
        auto [name, color] = starAnnotator(role, star);
        rolePairs.emplace_back(role->ID, name);
        roleColors.push_back(color);
    };

    for (auto [role, star] : rollOfChess)
    {
        addRoleWithStar(role, star);
    }

    auto menu = std::make_shared<UIRoleStatusMenu>(rolePairs, roleColors, 5);
    menu->setInputPosition(100, 60);    // Reasonable for UIStatus panel
    menu->getStatusDrawable().getUIStatus().setPosition(600, 60);
    menu->setShowNavigationButtons(false);
    menu->run();
}

void ChessSelector::viewMyChess()
{
}

}


