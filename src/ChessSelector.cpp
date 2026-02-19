#include "ChessSelector.h"

#include "BattleStatsView.h"
#include "ChessBalance.h"
#include "ChessCombo.h"
#include "ChessPool.h"
#include "DynamicChessMap.h"
#include "DrawableOnCall.h"
#include "Font.h"
#include "InputBox.h"
#include "Menu.h"
#include "Random.h"
#include "Save.h"
#include "TempStore.h"
#include <Engine.h>
#include <SuperMenuText.h>
#include <UIRoleStatusMenu.h>
#include <string>
#include <vector>

namespace KysChess
{

int ChessSelector::calculateCost(int tier, int star, int count)
{
    auto& cfg = ChessBalance::config();
    return cfg.tierPrices[tier] * static_cast<int>(std::pow(cfg.starCostMult, star)) * count;
}

namespace
{

static std::string comboEffectDesc(const ComboEffect& eff)
{
    auto triggerPrefix = [&]() -> std::string {
        if (eff.trigger == Trigger::WhileLowHP) return std::format("血量<{}%: ", eff.trigger_value);
        if (eff.trigger == Trigger::AllyLowHPBurst) return std::format("血量<{}%狂暴({}帧): ", eff.trigger_value, eff.value2);
        return "";
    };
    std::string d;
    switch (eff.type)
    {
    case EffectType::FlatHP: d = std::format("生命+{}", eff.value); break;
    case EffectType::FlatATK: d = std::format("攻击+{}", eff.value); break;
    case EffectType::FlatDEF: d = std::format("防御+{}", eff.value); break;
    case EffectType::FlatSPD: d = std::format("速度+{}", eff.value); break;
    case EffectType::PctHP: d = std::format("生命+{}%", eff.value); break;
    case EffectType::PctATK: d = std::format("攻击+{}%", eff.value); break;
    case EffectType::PctDEF: d = std::format("防御+{}%", eff.value); break;
    case EffectType::NegPctDEF: d = std::format("防御-{}%", eff.value); break;
    case EffectType::FlatDmgReduction: d = std::format("减伤{}", eff.value); break;
    case EffectType::BlockChance: d = std::format("{}%格挡", eff.value); break;
    case EffectType::DodgeChance: d = std::format("{}%闪避", eff.value); break;
    case EffectType::DodgeThenCrit: d = "闪避后暴击"; break;
    case EffectType::CritChance: d = std::format("{}%暴击", eff.value); break;
    case EffectType::CritMultiplier: d = std::format("暴击{}%伤害", eff.value); break;
    case EffectType::EveryNthDouble: d = std::format("每{}次双倍", eff.value); break;
    case EffectType::ArmorPenChance: d = std::format("{}%穿甲", eff.value); break;
    case EffectType::ArmorPenPct: d = std::format("无视{}%防御", eff.value); break;
    case EffectType::StunChance: d = eff.value2 ? std::format("{}%眩晕({}帧)", eff.value, eff.value2) : std::format("{}%眩晕", eff.value); break;
    case EffectType::KnockbackChance: d = std::format("{}%击退", eff.value); break;
    case EffectType::PoisonDOT: d = eff.value2 ? std::format("中毒{}%/帧({}帧)", eff.value, eff.value2) : std::format("中毒{}%/帧", eff.value); break;
    case EffectType::PoisonDmgAmp: d = std::format("中毒增伤{}%", eff.value); break;
    case EffectType::MPOnHit: d = std::format("命中回{}MP", eff.value); break;
    case EffectType::MPDrain: d = std::format("吸取{}MP", eff.value); break;
    case EffectType::MPRecoveryBonus: d = std::format("回蓝+{}%", eff.value); break;
    case EffectType::SkillDmgPct: d = std::format("技能伤害+{}%", eff.value); break;
    case EffectType::SkillReflectPct: d = std::format("反弹{}%", eff.value); break;
    case EffectType::CDR: d = std::format("冷却-{}%", eff.value); break;
    case EffectType::ShieldPctMaxHP: d = std::format("护盾{}%生命", eff.value); break;
    case EffectType::ShieldCDR: d = std::format("护盾冷却-{}%", eff.value); break;
    case EffectType::HealAuraPct: d = eff.value2 ? std::format("治疗光环{}%(每{}帧)", eff.value, eff.value2) : std::format("治疗光环{}%", eff.value); break;
    case EffectType::HealedATKSPDBoost: d = std::format("受治疗加速{}%", eff.value); break;
    case EffectType::HPRegenPct: d = eff.value2 ? std::format("回血{:.1f}%(每{}帧)", eff.value / 100.0, eff.value2) : std::format("回血{:.1f}%", eff.value / 100.0); break;
    case EffectType::FreezeReductionPct: d = std::format("冰冻-{}%", eff.value); break;
    case EffectType::ControlImmunityFrames: d = std::format("免控>{}帧", eff.value); break;
    case EffectType::KillHealPct: d = std::format("击杀回{}%血", eff.value); break;
    case EffectType::KillInvincFrames: d = std::format("击杀无敌{}帧", eff.value); break;
    case EffectType::PostSkillInvincFrames: d = std::format("技能后无敌{}帧", eff.value); break;
    case EffectType::DmgReductionPct: d = std::format("伤害减免{}%", eff.value); break;
    default: d = std::format("效果({})", eff.value); break;
    }
    return triggerPrefix() + d;
}

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

static std::shared_ptr<UIRoleStatusMenu> makeChessMenu(const std::string& title,
    std::vector<std::pair<int, std::string>>& items, std::vector<Color>& colors,
    int perPage, int fontSize, bool showNav = true)
{
    auto menu = std::make_shared<UIRoleStatusMenu>(title, items, colors, perPage, fontSize, false);
    menu->setInputPosition(80, 60);
    menu->getStatusDrawable().getUIStatus().setPosition(720, 30);
    if (!showNav) menu->setShowNavigationButtons(false);
    return menu;
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

            rolePairs.emplace_back(-1, std::format("刷新 ${}", ChessBalance::config().refreshCost));
            roleColors.push_back({ 128, 128, 128, 255 });

            rolePairs.emplace_back(-2, gameData.isShopLocked() ? "[已鎖定] 點擊解鎖" : "[未鎖定] 點擊鎖定");
            roleColors.push_back(gameData.isShopLocked() ? Color{ 255, 80, 80, 255 } : Color{ 128, 128, 128, 255 });

            // To be moved later. Need to add color as well.
            // The level may need to be changed as well.
            // Need back and cancel button.
            auto addRoleWithTier = [&](Role* role, int tier)
            {
                auto [name, color] = formatChessName(role, tier, {}, {});
                rolePairs.emplace_back(role->ID * 10, name);
                roleColors.push_back(color);
            };

            for (auto [role, star] : rollOfChess)
            {
                addRoleWithTier(role, star);
            }

            auto menu = makeChessMenu(std::format("購買棋子 等級{} ${} 背包{}/{}", gameData.getLevel() + 1, gameData.getMoney(), gameData.getBenchCount(), ChessBalance::config().benchSize), rolePairs, roleColors, rolePairs.size(), 32, false);

            // Combo info panel for hovered character
            auto comboInfoPanel = std::make_shared<DrawableOnCall>([](DrawableOnCall* self) {
                int rawId = self->getID();
                if (rawId < 0) return;
                int roleId = rawId / 10;

                auto roleCombos = ChessCombo::getCombosForRole(roleId);
                if (roleCombos.empty()) return;

                auto& allCombos = ChessCombo::getAllCombos();
                auto& gd = GameData::get();

                // Count selected pieces per role
                std::set<int> ownedIds;
                for (auto& ch : gd.getSelectedForBattle())
                    if (ch.role) ownedIds.insert(ch.role->ID);

                int px = 720, py = 475, fs = 20;
                Engine::getInstance()->fillColor({0, 0, 0, 160}, px, py, 460, 240);
                Font::getInstance()->draw("羈絆資訊", fs + 4, px + 10, py + 5, {255, 255, 100, 255});

                int ry = py + 32;
                for (auto cid : roleCombos)
                {
                    if (ry > py + 225) break;
                    auto& c = allCombos[(int)cid];
                    int owned = 0;
                    for (int rid : c.memberRoleIds)
                        if (ownedIds.count(rid)) owned++;

                    // Find next relevant threshold
                    const ComboThreshold* nextThr = nullptr;
                    for (auto& t : c.thresholds)
                    {
                        if (owned < t.count) { nextThr = &t; break; }
                    }
                    if (!nextThr && !c.thresholds.empty())
                        nextThr = &c.thresholds.back();

                    Color col = owned >= (nextThr ? nextThr->count : 999) ? Color{0, 255, 100, 255} : Color{200, 200, 200, 255};
                    std::string header = std::format("{} ({}/{})", c.name, owned, nextThr ? nextThr->count : (int)c.memberRoleIds.size());
                    Font::getInstance()->draw(header, fs, px + 10, ry, col);
                    ry += fs + 4;

                    if (nextThr)
                    {
                        for (auto& eff : nextThr->effects)
                        {
                            if (ry > py + 225) break;
                            Font::getInstance()->draw("  " + comboEffectDesc(eff), fs - 2, px + 10, ry, {180, 220, 255, 255});
                            ry += fs;
                        }
                    }
                    ry += 4;
                }
            });
            menu->addDrawableOnCall(comboInfoPanel);

            menu->run();

            int selectedId = menu->getResult();
            if (selectedId < 0)
            {
                return;
            }
            else if (selectedId == 0)
            {
                if (!gameData.spend(ChessBalance::config().refreshCost)) continue;
                gameData.chessPool.refresh();
                break;
            }
            else if (selectedId == 1)
            {
                gameData.setShopLocked(!gameData.isShopLocked());
                continue;
            }
            else if (selectedId >= 2)
            {
                auto actualIdx = selectedId - 2;
                auto [role, tier] = rollOfChess[actualIdx];
                int cost = ChessBalance::config().tierPrices[tier];
                if (gameData.isBenchFull() && !gameData.collection.wouldMerge({ role, 0 }))
                {
                    auto text = std::make_shared<TextBox>();
                    text->setText("背包已滿！請先出售棋子");
                    text->setFontSize(32);
                    text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 150, Engine::getInstance()->getUIHeight() / 2);
                    continue;
                }
                if (!gameData.spend(cost)) continue;
                gameData.chessPool.removeChessAt(actualIdx);
                gameData.addChessAndFixSelection({ role, 0 });
                auto text = std::make_shared<TextBox>();
                text->setText(std::format("消費{}，獲取{}", cost, role->Name));
                text->setFontSize(32);
                text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 100, Engine::getInstance()->getUIHeight() / 2);
                break;
            }
        }
    }
}

void ChessSelector::sellChess()
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
            rolePairs.emplace_back(chess.role->ID * 10 + chess.star, name);
            roleColors.push_back(color);
            chessList.push_back(chess);
        }
    }

    auto menu = makeChessMenu(std::format("出售棋子 背包{}/{}", gameData.getBenchCount(), ChessBalance::config().benchSize), rolePairs, roleColors, 10, 32);
    menu->run();

    int selectedId = menu->getResult();
    if (selectedId >= 0)
    {
        auto chess = chessList[selectedId];
        int tier = ChessPool::GetChessTier(chess.role->ID);
        int sellPrice = calculateCost(tier, chess.star, 1);
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
        gameData.make(sellPrice);

        auto text = std::make_shared<TextBox>();
        text->setText(std::format("售出棋子{}，獲得${}", chess.role->Name, sellPrice));
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

    int maxSelection = std::max(gameData.getLevel() + 1, ChessBalance::config().minBattleSize);
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
            rolePairs.emplace_back(chess.role->ID * 10 + chess.star, name);
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
                rolePairs.emplace_back(chess.role->ID * 10 + chess.star, name);
                roleColors.push_back(color);
                chessList.push_back(chess);
            }
        }

        std::string menuTitle = std::format("選擇出戰棋子 {}/{} 背包{}/{}", currentSelection.size(), maxSelection, gameData.getBenchCount(), ChessBalance::config().benchSize);
        auto menu = makeChessMenu(menuTitle, rolePairs, roleColors, 10, 32);

        // Combo info panel — same as shop
        auto comboPanel = std::make_shared<DrawableOnCall>([](DrawableOnCall* self) {
                int rawId = self->getID();
                if (rawId < 0) return;
                int roleId = rawId / 10;

                auto roleCombos = ChessCombo::getCombosForRole(roleId);
                if (roleCombos.empty()) return;

                auto& allCombos = ChessCombo::getAllCombos();
                auto& gd = GameData::get();

                std::set<int> ownedIds;
                for (auto& ch : gd.getSelectedForBattle())
                    if (ch.role) ownedIds.insert(ch.role->ID);

                int px = 720, py = 475, fs = 20;
                Engine::getInstance()->fillColor({0, 0, 0, 160}, px, py, 460, 240);
                Font::getInstance()->draw("羈絆資訊", fs + 4, px + 10, py + 5, {255, 255, 100, 255});

                int ry = py + 32;
                for (auto cid : roleCombos)
                {
                    if (ry > py + 225) break;
                    auto& c = allCombos[(int)cid];
                    int owned = 0;
                    for (int rid : c.memberRoleIds)
                        if (ownedIds.count(rid)) owned++;

                    const ComboThreshold* nextThr = nullptr;
                    for (auto& t : c.thresholds)
                    {
                        if (owned < t.count) { nextThr = &t; break; }
                    }
                    if (!nextThr && !c.thresholds.empty())
                        nextThr = &c.thresholds.back();

                    Color col = owned >= (nextThr ? nextThr->count : 999) ? Color{0, 255, 100, 255} : Color{200, 200, 200, 255};
                    std::string header = std::format("{} ({}/{})", c.name, owned, nextThr ? nextThr->count : (int)c.memberRoleIds.size());
                    Font::getInstance()->draw(header, fs, px + 10, ry, col);
                    ry += fs + 4;

                    if (nextThr)
                    {
                        for (auto& eff : nextThr->effects)
                        {
                            if (ry > py + 225) break;
                            Font::getInstance()->draw("  " + comboEffectDesc(eff), fs - 2, px + 10, ry, {180, 220, 255, 255});
                            ry += fs;
                        }
                    }
                    ry += 4;
                }
        });
        menu->addDrawableOnCall(comboPanel);

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

    // Save enemy random counter before generation for retry determinism
    auto savedEnemyCallCount = gameData.enemyCallCount_;

    // Generate enemies based on progress
    std::vector<int> enemyIds;
    std::vector<int> enemyStars;
    int stage = progress.getStage();
    int subStage = progress.getSubStage();
    bool isBoss = progress.isBossFight();

    // Determine enemy tier and count
    auto& ecfg = ChessBalance::config();
    int baseTier = std::min(stage, ecfg.bossMaxTier);
    int overflowStages = std::max(0, stage - ecfg.bossMaxTier);
    int enemyCount = std::min(ecfg.enemyCountMax, ecfg.enemyCountBase + stage + subStage);

    // Boss fight: use next tier or add stars
    if (isBoss)
    {
        int bossTier = std::min(ecfg.bossMaxTier, baseTier + ecfg.bossTierUp);
        int bossStars = (baseTier >= ecfg.bossMaxTier) ? std::min(3, stage - (ecfg.bossStarStartStage - 1)) : 0;

        // Generate boss enemies
        for (int i = 0; i < enemyCount; ++i)
        {
            auto role = ChessPool::selectEnemyFromPool(bossTier);
            enemyIds.push_back(role->ID);
            enemyStars.push_back(bossStars);
        }
    }
    else
    {
        // Regular fight: use current tier with some variation
        for (int i = 0; i < enemyCount; ++i)
        {
            // 70% current tier, 30% previous tier (if available)
            int tier = baseTier;
            if (baseTier > 0 && GameData::get().enemyRandInt(100) < ecfg.lowerTierMixPct)
            {
                tier = baseTier - 1;
            }

            auto role = ChessPool::selectEnemyFromPool(tier);
            enemyIds.push_back(role->ID);

            int star = overflowStages;
            if (subStage >= ecfg.starUpgradeStartSub && GameData::get().enemyRandInt(100) < ecfg.starUpgradePct)
                star += 1;
            if (star >= 1 && stage >= ecfg.star2UpgradeStartStage && GameData::get().enemyRandInt(100) < ecfg.star2UpgradePct)
                star += 1;
            enemyStars.push_back(std::min(star, 3));
        }
    }

    // Detect combos for both teams before enhancements modify stats
    auto allyCombos = ChessCombo::detectCombos(selectedChess);
    std::vector<Chess> enemyChessVec;
    for (int id : enemyIds)
    {
        auto r = Save::getInstance()->getRole(id);
        if (r) enemyChessVec.push_back({r, 0});
    }
    auto enemyCombos = ChessCombo::detectCombos(enemyChessVec);

    // Show pre-battle stats screen
    {
        auto preBattle = std::make_shared<BattleStatsView>();
        preBattle->setupPreBattle(selectedChess, enemyIds, allyCombos, enemyCombos);
        preBattle->run();
    }

    // Create battle - pass raw IDs and stars, enhancements applied on copies
    DynamicBattleRoles roles;
    for (auto& c : selectedChess)
    {
        roles.teammate_ids.push_back(c.role->ID);
        roles.teammate_stars.push_back(c.star);
    }
    roles.enemy_ids = enemyIds;
    roles.enemy_stars = enemyStars;

    auto battle = DynamicChessMap::createBattle(roles);

    // Seed battle RNG deterministically so retries produce same combat
    unsigned int battleSeed = static_cast<unsigned int>(gameData.enemyRandInt(INT_MAX));
    battle->rand_.set_seed(battleSeed);

    // Run battle
    battle->run();

    // Show post-battle summary
    {
        auto postBattle = std::make_shared<BattleStatsView>();
        postBattle->setupPostBattle(battle->getFriendsObj(), battle->getEnemiesObj(), battle->getTracker(), battle->getResult());
        postBattle->run();
    }

    // Recover all HP for selected chess
    for (const auto& chess : selectedChess)
    {
        chess.role->HP = chess.role->MaxHP;
        chess.role->MP = chess.role->MaxMP;
    }

    if (battle->getResult() == 0)
    {
        int reward = ecfg.rewardBase + stage * ecfg.rewardStageCoeff + subStage;
        gameData.make(reward);
        int expGain = isBoss ? ecfg.bossBattleExp : ecfg.battleExp;
        int oldLvl = gameData.getLevel();
        gameData.increaseExp(expGain);
        progress.advance();
        if (!gameData.isShopLocked())
        {
            gameData.chessPool.refresh();
        }
        auto text = std::make_shared<TextBox>();
        std::string lvlMsg = (gameData.getLevel() > oldLvl) ? std::format(" 升級！等級{}", gameData.getLevel() + 1) : "";
        text->setText(std::format("勝利！獲得${} 經驗+{}{} 當前第{}章第{}關", reward, expGain, lvlMsg, progress.getStage() + 1, progress.getSubStage() + 1));
        text->setFontSize(32);
        text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 200, Engine::getInstance()->getUIHeight() / 2);
    }
    else
    {
        // Reset enemy random counter so retry generates same enemies
        gameData.enemyCallCount_ = savedEnemyCallCount;
        gameData.restoreRand();
        auto text = std::make_shared<TextBox>();
        text->setText("戰鬥失敗！請調整陣容後再試");
        text->setFontSize(32);
        text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 200, Engine::getInstance()->getUIHeight() / 2);
    }
}

void ChessSelector::buyExp()
{
    auto& gd = GameData::get();
    auto& bcfg = ChessBalance::config();
    if (gd.getLevel() >= bcfg.maxLevel)
    {
        auto text = std::make_shared<TextBox>();
        text->setText("已達最高等級！");
        text->setFontSize(32);
        text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 100, Engine::getInstance()->getUIHeight() / 2);
        return;
    }

    int curLvl = gd.getLevel();
    int nextLvl = curLvl + 1;
    int curPieces = std::max(curLvl + 1, bcfg.minBattleSize);
    int nextPieces = std::max(nextLvl + 1, bcfg.minBattleSize);
    bool atNextMax = nextLvl >= bcfg.maxLevel;

    auto weightStr = [&](int lvl) -> std::string {
        if (lvl >= (int)bcfg.shopWeights.size()) return "已滿級";
        auto& w = bcfg.shopWeights[lvl];
        return std::format("1費{}% 2費{}% 3費{}% 4費{}% 5費{}%", w[0], w[1], w[2], w[3], w[4]);
    };

    // Pre-compute strings so lambda only captures values
    std::string hdr = std::format("等級 {}    經驗 {}/{}    金幣 ${}",
        curLvl + 1, gd.getExp(), gd.getExpForNextLevel(), gd.getMoney());
    std::string costLine = std::format("花費 ${}  獲得 {} 經驗", bcfg.buyExpCost, bcfg.buyExpAmount);
    std::string pieceLine = std::format("出戰人數: {}", curPieces);
    std::string pieceNext = (!atNextMax && nextPieces > curPieces) ? std::format("  → {}", nextPieces) : "";
    std::string curW = "  " + weightStr(curLvl);
    std::string nextW = "  " + (atNextMax ? std::string("已滿級") : weightStr(nextLvl));

    // Show confirmation menu with info panel
    auto menu = std::make_shared<MenuText>(std::vector<std::string>{"確認購買", "取消"});
    menu->setFontSize(24);
    menu->arrange(0, 0, 0, 32);

    auto infoPanel = std::make_shared<DrawableOnCall>([=](DrawableOnCall*) {
        int px = 350, py = 195, fs = 20, lh = fs + 6;
        Engine::getInstance()->fillColor({0, 0, 0, 180}, px, py, 520, 280);
        auto* font = Font::getInstance();
        int ly = py + 10, x = px + 10;

        font->draw(hdr, fs, x, ly, {255, 215, 0, 255}); ly += lh;
        font->draw(costLine, fs, x, ly, {255, 255, 255, 255}); ly += lh + 6;
        font->draw(pieceLine, fs, x, ly, {130, 220, 255, 255});
        if (!pieceNext.empty()) font->draw(pieceNext, fs, x + fs * 8, ly, {100, 255, 100, 255});
        ly += lh + 6;
        font->draw("當前商店權重:", fs, x, ly, {160, 160, 160, 255}); ly += lh;
        font->draw(curW, fs, x, ly, {255, 255, 255, 255}); ly += lh + 4;
        font->draw("下一級商店權重:", fs, x, ly, {160, 160, 160, 255}); ly += lh;
        font->draw(nextW, fs, x, ly, {100, 255, 100, 255});
    });
    menu->addChild(infoPanel);
    menu->runAtPosition(200, 200);

    if (menu->getResult() != 0) return;
    if (!gd.spend(bcfg.buyExpCost)) return;

    int oldLvl = gd.getLevel();
    gd.increaseExp(bcfg.buyExpAmount);
    auto text = std::make_shared<TextBox>();
    if (gd.getLevel() > oldLvl)
        text->setText(std::format("升級！等級{} 經驗{}/{}", gd.getLevel() + 1, gd.getExp(), gd.getExpForNextLevel()));
    else
        text->setText(std::format("經驗{}/{}", gd.getExp(), gd.getExpForNextLevel()));
    text->setFontSize(32);
    text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 150, Engine::getInstance()->getUIHeight() / 2);
}

void ChessSelector::showContextMenu()
{
    //ChessBalance::apply();
    auto menu = std::make_shared<MenuText>(std::vector<std::string>{ "購買棋子", "出售棋子", "選擇出戰", "進入戰鬥", std::format("購買經驗 ${}", ChessBalance::config().buyExpCost), "查看羈絆" });
    menu->setFontSize(24);
    menu->arrange(0, 0, 0, 32);
    menu->runAtPosition(200, 200);

    switch (menu->getResult())
    {
    case 0: getChess(); break;
    case 1: sellChess(); break;
    case 2: selectForBattle(); break;
    case 3: enterBattle(); break;
    case 4: buyExp(); break;
    case 5: viewCombos(); break;
    default: break;
    }
}

void ChessSelector::viewCombos()
{
    auto& combos = ChessCombo::getAllCombos();
    auto& gameData = GameData::get();

    // Build set of selected role IDs
    std::set<int> ownedIds;
    for (auto& chess : gameData.getSelectedForBattle())
        if (chess.role) ownedIds.insert(chess.role->ID);

    // Build menu items: one per combo
    std::vector<std::pair<int, std::string>> items;
    std::vector<Color> colors;
    for (int i = 0; i < (int)combos.size(); i++)
    {
        auto& c = combos[i];
        int owned = 0;
        for (int rid : c.memberRoleIds)
            if (ownedIds.count(rid)) owned++;
        // Pad name to 14 bytes (7 Chinese chars) for consistent menu width
        std::string padded = c.name;
        while (padded.size() < 14) padded += "　";  // fullwidth space
        std::string label = std::format("{}({:2}/{:2})", padded, owned, c.memberRoleIds.size());
        items.emplace_back(i, label);
        colors.push_back(owned > 0 ? Color{0, 255, 0, 255} : Color{180, 180, 180, 255});
    }

    // Detail panel drawable — two-column: members left, thresholds right
    auto detailDraw = std::make_shared<DrawableOnCall>([&combos, &ownedIds](DrawableOnCall* self) {
        int idx = self->getID();
        if (idx < 0 || idx >= (int)combos.size()) return;
        auto& c = combos[idx];
        auto save = Save::getInstance();
        auto font = Font::getInstance();

        int px = 460, py = 60, fs = 24;
        Engine::getInstance()->fillColor({0, 0, 0, 180}, px, py, 660, 560);

        font->draw(c.name, fs + 4, px + 10, py + 10, {255, 255, 100, 255});

        // Left column: members
        int y = py + 45;
        font->draw("成员:", fs, px + 10, y, {200, 200, 200, 255});
        y += fs + 4;
        for (int rid : c.memberRoleIds)
        {
            auto role = save->getRole(rid);
            if (!role) continue;
            bool owned = ownedIds.count(rid) > 0;
            Color col = owned ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255};
            font->draw(std::format("  {} ({}费){}", role->Name, ChessPool::GetChessTier(rid) + 1, owned ? " ✓" : ""), fs, px + 10, y, col);
            y += fs + 1;
        }

        // Right column: thresholds
        int rx = px + 260, ry = py + 45;
        font->draw(c.isAntiCombo ? "条件:" : "阈值:", fs, rx, ry, {200, 200, 200, 255});
        ry += fs + 4;
        for (auto& t : c.thresholds)
        {
            font->draw(std::format("{}人: {}", t.count, t.name), fs, rx, ry, {255, 200, 100, 255});
            ry += fs + 1;
            for (auto& eff : t.effects)
            {
                font->draw("  " + comboEffectDesc(eff), fs - 2, rx, ry, {220, 220, 220, 255});
                ry += fs - 2;
            }
            ry += 8;
        }
    });

    SuperMenuTextExtraOptions opts;
    opts.itemColors_ = colors;
    opts.needInputBox_ = false;
    opts.exitable_ = true;
    auto menu = std::make_shared<SuperMenuText>("羈絆一覽", 26, items, 12, opts);
    menu->setInputPosition(80, 60);
    menu->addDrawableOnCall(detailDraw);
    menu->run();
}

}    //namespace KysChess
