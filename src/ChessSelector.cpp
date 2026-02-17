#include "ChessSelector.h"

#include "BattleRoleManager.h"
#include "ChessPool.h"
#include "DynamicChessMap.h"
#include "InputBox.h"
#include "Menu.h"
#include "Random.h"
#include "Save.h"
#include "TempStore.h"
#include <Engine.h>
#include <UIRoleStatusMenu.h>
#include <string>
#include <vector>

namespace KysChess
{

int ChessSelector::calculateCost(int tier, int star, int count)
{
    return (tier + 1) * (std::pow(3, star)) * count;
}

namespace
{

// Calculate display width of a UTF-8 string
// Chinese characters and wide characters take 2 columns, ASCII takes 1
static int getDisplayWidth(const std::string& str)
{
    int width = 0;
    for (size_t i = 0; i < str.size();)
    {
        unsigned char c = str[i];
        if (c < 0x80)
        {
            // ASCII character - 1 column
            width += 1;
            i += 1;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            // 2-byte UTF-8 - typically 2 columns
            width += 2;
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            // 3-byte UTF-8 (includes Chinese characters and ★) - 2 columns
            width += 2;
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            // 4-byte UTF-8 - 2 columns
            width += 2;
            i += 4;
        }
        else
        {
            // Invalid UTF-8, skip
            i += 1;
        }
    }
    return width;
}

static std::pair<std::string, Color> formatChessName(Role* role, int tier, std::optional<int> starOpt, std::optional<int> countOpt, const std::string& prefix = "")
{
    std::string result;
    int displayWidth = 0;

    // Add prefix if present
    if (!prefix.empty())
    {
        result += prefix;
        displayWidth += getDisplayWidth(prefix);
    }

    // Add name and pad to fixed width (16 columns total including prefix)
    result += role->Name;
    displayWidth += getDisplayWidth(role->Name);
    int nameColumnWidth = 16;
    while (displayWidth < nameColumnWidth)
    {
        result += " ";
        displayWidth += 1;
    }

    // Add stars in fixed width column (8 columns for up to 4 stars)
    int starColumnWidth = 8;
    int starWidth = 0;
    if (auto star = starOpt)
    {
        for (int i = 0; i < *star + 1; ++i)
        {
            result += "★";
            starWidth += 2;
        }
    }
    while (starWidth < starColumnWidth)
    {
        result += " ";
        starWidth += 1;
    }

    // Add quantity in fixed width column (6 columns)
    int quantityColumnWidth = 6;
    int quantityWidth = 0;
    if (auto count = countOpt)
    {
        std::string countStr = "x" + std::to_string(*count);
        result += countStr;
        quantityWidth += countStr.size();
    }
    while (quantityWidth < quantityColumnWidth)
    {
        result += " ";
        quantityWidth += 1;
    }

    // Add cost right-aligned in fixed width column (6 columns for $XXX)
    int cost = ChessSelector::calculateCost(tier, starOpt.value_or(0), countOpt.value_or(1));
    std::string costStr = "$" + std::to_string(cost);
    int costColumnWidth = 6;
    int costWidth = costStr.size();

    // Add leading spaces for right alignment
    while (costWidth < costColumnWidth)
    {
        result += " ";
        costWidth += 1;
    }
    result += costStr;

    Color colors[] = {
        { 175, 238, 238 },
        { 0, 255, 0 },
        { 30, 144, 255 },
        { 75, 0, 130 },
        { 255, 0, 0 }  // Red for tier 5
    };
    return { result, colors[tier] };
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
            menu->setInputPosition(30, 60);    // Reasonable for UIStatus panel
            menu->getStatusDrawable().getUIStatus().setPosition(640, 60);
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

    // Expand each chess by its count so each piece is listed individually
    for (const auto& [chess, count] : chessMap)
    {
        for (int i = 0; i < count; ++i)
        {
            auto [name, color] = formatChessName(chess.role, ChessPool::GetChessTier(chess.role->ID), chess.star, {});
            rolePairs.emplace_back(chess.role->ID, name);
            roleColors.push_back(color);
            chessList.push_back(chess);
        }
    }

    auto menu = std::make_shared<UIRoleStatusMenu>("出售棋子", rolePairs, roleColors, 10, 32);
    menu->setInputPosition(30, 60);
    menu->getStatusDrawable().getUIStatus().setPosition(640, 60);
    menu->run();

    int selectedId = menu->getResult();
    if (selectedId >= 0)
    {
        auto chess = chessList[selectedId];
        gameData.collection.removeChess(chess);

        // Remove one instance from battle selection if it was selected
        auto& selected = gameData.getSelectedForBattle();
        std::vector<Chess> newSelection;
        bool removedOne = false;
        for (const auto& s : selected)
        {
            if (!removedOne && s.role == chess.role && s.star == chess.star)
            {
                removedOne = true;  // Skip this one
            }
            else
            {
                newSelection.push_back(s);
            }
        }
        gameData.setSelectedForBattle(newSelection);

        auto text = std::make_shared<TextBox>();
        text->setText(std::format("售出棋子{}", chess.role->Name));
        text->setFontSize(32);
        text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 100, Engine::getInstance()->getUIHeight() / 2);
    }
}

void ChessSelector::selectForBattle()
{
    auto& gameData = GameData::get();
    auto& chessMap = gameData.collection.getChess();

    if (chessMap.empty())
    {
        return;
    }

    int maxSelection = std::max(gameData.getLevel(), 2);
    std::vector<Chess> currentSelection = gameData.getSelectedForBattle();

    for (;;)
    {
        std::vector<std::pair<int, std::string>> rolePairs;
        std::vector<Color> roleColors;
        std::vector<Chess> chessList;

        // Add currently selected chess with [已選] prefix
        for (const auto& chess : currentSelection)
        {
            auto [name, color] = formatChessName(chess.role, ChessPool::GetChessTier(chess.role->ID), chess.star, {}, "[已選]");
            rolePairs.emplace_back(chess.role->ID, name);
            roleColors.push_back({ 255, 215, 0, 255 });  // Gold for selected
            chessList.push_back(chess);
        }

        // Add available chess from collection - expand by count so each piece is listed individually
        for (const auto& [chess, count] : chessMap)
        {
            // Count how many of this chess are already selected
            int selectedCount = 0;
            for (const auto& selected : currentSelection)
            {
                if (selected.role == chess.role && selected.star == chess.star)
                {
                    selectedCount++;
                }
            }

            // Add remaining unselected pieces
            int remainingCount = count - selectedCount;
            for (int i = 0; i < remainingCount; ++i)
            {
                auto [name, color] = formatChessName(chess.role, ChessPool::GetChessTier(chess.role->ID), chess.star, {});
                rolePairs.emplace_back(chess.role->ID, name);
                roleColors.push_back(color);
                chessList.push_back(chess);
            }
        }

        std::string menuTitle = std::format("選擇出戰棋子 {}/{}", currentSelection.size(), maxSelection);
        auto menu = std::make_shared<UIRoleStatusMenu>(menuTitle, rolePairs, roleColors, 10, 32, false);
        menu->setInputPosition(30, 60);
        menu->getStatusDrawable().getUIStatus().setPosition(640, 60);
        menu->run();

        int selectedId = menu->getResult();
        if (selectedId < 0)
        {
            // Save and exit
            gameData.setSelectedForBattle(currentSelection);
            return;
        }
        else
        {
            auto chess = chessList[selectedId];

            // Check if this is a selected chess (deselect one instance)
            if (selectedId < currentSelection.size())
            {
                // This is a selected chess, remove it
                currentSelection.erase(currentSelection.begin() + selectedId);
            }
            else
            {
                // Add to selection if not at max
                if (currentSelection.size() < maxSelection)
                {
                    currentSelection.push_back(chess);
                }
                else
                {
                    auto text = std::make_shared<TextBox>();
                    text->setText(std::format("最多只能選擇{}個棋子", maxSelection));
                    text->setFontSize(32);
                    text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 100, Engine::getInstance()->getUIHeight() / 2);
                }
            }
        }
    }
}

void ChessSelector::enterBattle()
{
    auto& gameData = GameData::get();
    auto& progress = gameData.battleProgress;

    // Check if game is complete
    if (progress.isGameComplete())
    {
        auto text = std::make_shared<TextBox>();
        text->setText("恭喜通關！");
        text->setFontSize(32);
        text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 100, Engine::getInstance()->getUIHeight() / 2);
        return;
    }

    // Check if we have selected chess
    auto& selectedChess = gameData.getSelectedForBattle();
    if (selectedChess.empty())
    {
        auto text = std::make_shared<TextBox>();
        text->setText("請先選擇出戰棋子");
        text->setFontSize(32);
        text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 100, Engine::getInstance()->getUIHeight() / 2);
        return;
    }

    // Generate enemies based on progress
    std::vector<int> enemyIds;
    int stage = progress.getStage();
    int subStage = progress.getSubStage();
    bool isBoss = progress.isBossFight();

    // Determine enemy tier and count
    int baseTier = stage;  // Stage 0 = tier 0, stage 1 = tier 1, etc.
    int enemyCount = std::min(10, 3 + subStage * 2);  // 3, 5, 7, 9, 10 enemies

    // Boss fight: use next tier or add stars
    if (isBoss)
    {
        int bossTier = std::min(4, baseTier + 1);  // Next tier, max tier 4
        int bossStars = (baseTier >= 4) ? std::min(3, stage - 3) : 0;  // Add stars if already at max tier

        // Generate boss enemies
        for (int i = 0; i < enemyCount; ++i)
        {
            auto role = ChessPool::selectEnemyFromPool(bossTier);
            enemyIds.push_back(role->ID);

            // Apply star enhancement to boss enemies
            if (bossStars > 0)
            {
                role->MaxHP += BattleRoleManager::HP_PER_STAR * bossStars;
                role->HP = role->MaxHP;
                role->Attack += BattleRoleManager::ATK_PER_STAR * bossStars;
                role->Defence += BattleRoleManager::DEF_PER_STAR * bossStars;
            }
        }
    }
    else
    {
        // Regular fight: use current tier with some variation
        static Random<> rand;
        for (int i = 0; i < enemyCount; ++i)
        {
            // 70% current tier, 30% previous tier (if available)
            int tier = baseTier;
            if (baseTier > 0 && rand.rand_int(100) < 30)
            {
                tier = baseTier - 1;
            }

            auto role = ChessPool::selectEnemyFromPool(tier);
            enemyIds.push_back(role->ID);

            // Small chance for starred enemies in later substages
            if (subStage >= 2 && rand.rand_int(100) < 20)
            {
                int stars = 1;
                role->MaxHP += BattleRoleManager::HP_PER_STAR * stars;
                role->HP = role->MaxHP;
                role->Attack += BattleRoleManager::ATK_PER_STAR * stars;
                role->Defence += BattleRoleManager::DEF_PER_STAR * stars;
            }
        }
    }

    // Apply enhancements to selected chess
    auto teammateIds = BattleRoleManager::applyEnhancements(selectedChess);

    // Create battle
    DynamicBattleRoles roles;
    roles.teammate_ids = teammateIds;
    roles.enemy_ids = enemyIds;

    auto battle = DynamicChessMap::createBattle(roles);

    // Run battle
    battle->run();

    // Restore roles after battle
    BattleRoleManager::restoreRoles();

    // Check battle result and advance progress
    // TODO: Check if battle was won before advancing
    progress.advance();
}

}    //namespace KysChess
