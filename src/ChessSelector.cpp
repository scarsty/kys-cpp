#include "ChessSelector.h"

#include "ChessPool.h"
#include "InputBox.h"
#include "Menu.h"
#include "Save.h"
#include "TempStore.h"
#include <Engine.h>
#include <UIRoleStatusMenu.h>
#include <string>
#include <vector>

namespace KysChess
{

namespace
{

int calculateCost(int tier, int star, int count)
{
    return (tier + 1) * (std::pow(3, star)) * count;
}

static std::pair<std::string, Color> formatChessName(Role* role, int tier, std::optional<int> starOpt, std::optional<int> countOpt)
{
    std::string name = role->Name;
    int size = name.size() / 3;
    while (size <= 5)
    {
        name += "  ";
        size += 1;
    }

    if (auto star = starOpt)
    {
        for (int i = 0; i < *star + 1; ++i)
        {
            name += "★";
            size += 1;
        }
    }

    if (auto count = countOpt)
    {
        name += " x" + std::to_string(*count);
    }

    int cost = calculateCost(tier, starOpt.value_or(0), countOpt.value_or(1));
    auto costStr = std::to_string(cost);

    int totalSize = 12 - costStr.size();

    while (size < totalSize)
    {
        name += "  ";
        size += 1;
    }

    name += "$";
    name += costStr;

    Color colors[] = {
        { 175, 238, 238 },
        { 0, 255, 0 },
        { 30, 144, 255 },
        { 75, 0, 130 },
        { 255, 165, 0 }
    };
    return { name, colors[tier] };
}

}    //namespace

void ChessSelector::getChess()
{
    auto& gameData = GameData::get();
    for (;;)
    {
        auto rollOfChess = gameData.chessPool.getChessFromPool(gameData.getLevel());

        for (;;)
        {
            std::vector<std::pair<int, std::string>> rolePairs;
            std::vector<Color> roleColors;
            auto& roles = Save::getInstance()->getRoles();

            rolePairs.emplace_back(-1, "刷新 $2");
            roleColors.push_back({ 128, 128, 128, 255 });

            // To be moved later. Need to add color as well.
            // The level may need to be changed as well.
            // Need back and cancel button.
            auto addRoleWithTier = [&](Role* role, int tier)
            {
                auto [name, color] = formatChessName(role, tier, {}, {});
                rolePairs.emplace_back(role->ID, name);
                roleColors.push_back(color);
            };

            for (auto [role, star] : rollOfChess)
            {
                addRoleWithTier(role, star);
            }

            auto menu = std::make_shared<UIRoleStatusMenu>(std::format("購買棋子 当前等级{}", gameData.getLevel() + 1), rolePairs, roleColors, rolePairs.size(), 32);
            menu->setInputPosition(100, 60);    // Reasonable for UIStatus panel
            menu->getStatusDrawable().getUIStatus().setPosition(600, 60);
            menu->setShowNavigationButtons(false);
            menu->run();

            int selectedId = menu->getResult();
            if (selectedId < 0)
            {
                return;
            }
            else if (selectedId == 0)
            {
                // TODO: Add cost here
                gameData.chessPool.refresh();
                break;
            }
            else if (selectedId >= 1)
            {
                auto actualIdx = selectedId - 1;
                gameData.chessPool.removeChessAt(actualIdx);
                // Find the selected chess
                auto [role, tier] = rollOfChess[actualIdx];
                gameData.collection.addChess({ role, 0 });
                auto text = std::make_shared<TextBox>();
                text->setText(std::format("消費{}，獲取{}", tier + 1, role->Name));
                text->setFontSize(32);
                text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 100, Engine::getInstance()->getUIHeight() / 2);
                break;
            }
        }
    }
}

void ChessSelector::viewMyChess()
{
    auto& gameData = GameData::get();

    auto& chessMap = gameData.collection.getChess();

    if (chessMap.empty())
    {
        // No chess in collection
        return;
    }

    std::vector<std::pair<int, std::string>> rolePairs;
    std::vector<Color> roleColors;
    std::vector<Chess> chessList;    // Keep track of chess for removal
    std::vector<Chess> snapshot;

    for (const auto& [chess, count] : chessMap)
    {
        snapshot.push_back(chess);
        auto [name, color] = formatChessName(chess.role, ChessPool::GetChessTier(chess.role->ID), chess.star, count);
        rolePairs.emplace_back(chess.role->ID, name);
        roleColors.push_back(color);
        chessList.push_back(chess);
    }

    auto menu = std::make_shared<UIRoleStatusMenu>("出售棋子", rolePairs, roleColors, 10, 32);
    menu->setInputPosition(100, 60);
    menu->getStatusDrawable().getUIStatus().setPosition(600, 60);
    menu->run();

    int selectedId = menu->getResult();
    if (selectedId >= 0)
    {
        auto chess = snapshot[selectedId];
        gameData.collection.removeChess(chess);
        auto text = std::make_shared<TextBox>();
        text->setText(std::format("售出棋子{}", chess.role->Name));
        text->setFontSize(32);
        text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 100, Engine::getInstance()->getUIHeight() / 2);
    }
}

void ChessSelector::enterBattle()
{
    
}

}    //namespace KysChess
