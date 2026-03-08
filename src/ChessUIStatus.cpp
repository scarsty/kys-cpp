#include "ChessUIStatus.h"
#include "BattleRoleManager.h"
#include "BattleSceneHades.h"
#include "ChessCombo.h"
#include "ChessManager.h"
#include "ChessPool.h"
#include "ChessScreenLayout.h"
#include "Engine.h"
#include "GameUtil.h"
#include "GameState.h"

namespace KysChess {

void ChessUIStatus::draw()
{
    if (!chess_.role) return;

    auto frame = ChessScreenLayout::shopStatusPanel();
    int panelW = frame.w;
    int panelH = frame.h;

    // Draw translucent black background
    Engine::getInstance()->fillRoundedRect({0, 0, 0, 128}, x_, y_, panelW, panelH, 8);
    Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, x_, y_, panelW, panelH, 8);

    auto& gd = KysChess::GameState::get();
    auto font = Font::getInstance();
    Color color_white = { 255, 255, 255, 255 };
    Color color_name = { 255, 215, 0, 255 };

    // Compute boosted stats for display
    auto bs = KysChess::BattleRoleManager::computeStarStats(chess_.role, chess_.star);
    int dispMaxHP = bs.hp;
    int dispAttack = bs.atk;
    int dispDefence = bs.def;
    int dispSpeed = bs.spd;

    TextureManager::getInstance()->renderTexture("head", chess_.role->HeadID, x_ + 10, y_ + 15);

    Color color_ability1 = { 255, 250, 205, 255 };
    Color color_ability2 = { 236, 200, 40, 255 };
    Color color_red = { 255, 90, 60, 255 };
    Color color_magic = { 236, 200, 40, 255 };
    Color color_magic_level1 = { 253, 101, 101, 255 };
    Color color_purple = { 208, 152, 208, 255 };
    Color color_magic_empty = { 236, 200, 40, 255 };
    Color color_equip = { 165, 28, 218, 255 };

    auto select_color1 = [&](int v, int max_v) -> Color
    {
        if (v >= max_v * 0.9)
        {
            return color_red;
        }
        else if (v >= max_v * 0.8)
        {
            return { 255, 165, 79, 255 };
        }
        else if (v >= max_v * 0.7)
        {
            return { 255, 255, 50, 255 };
        }
        else if (v < 0)
        {
            return color_purple;
        }
        return color_white;
    };

    auto select_color2 = [&](int v) -> Color
    {
        if (v > 0)
        {
            return color_red;
        }
        if (v < 0)
        {
            return color_purple;
        }
        return color_white;
    };

    int font_size = 22;

    // Stats right of avatar
    int sx = x_ + 195, sy = y_ + 20;
    font->draw("生命", font_size, sx, sy, color_ability1);
    font->draw(std::format("{:5}/{:5}", dispMaxHP, dispMaxHP), font_size, sx + 44, sy, color_white);
    Color c = color_white;
    if (chess_.role->MPType == 0) c = color_purple;
    else if (chess_.role->MPType == 1) c = color_magic;
    font->draw("內力", font_size, sx, sy + 25, color_ability1);
    font->draw(std::format("{:5}/{:5}", chess_.role->MP, GameUtil::MAX_MP), font_size, sx + 44, sy + 25, c);
    font->draw("攻擊", font_size, sx, sy + 55, color_ability1);
    font->draw(std::format("{:5}", dispAttack), font_size, sx + 44, sy + 55, select_color1(dispAttack, Role::getMaxValue()->Attack));
    font->draw("防禦", font_size, sx, sy + 80, color_ability1);
    font->draw(std::format("{:5}", dispDefence), font_size, sx + 44, sy + 80, select_color1(dispDefence, Role::getMaxValue()->Defence));
    font->draw("輕功", font_size, sx, sy + 105, color_ability1);
    font->draw(std::format("{:5}", dispSpeed), font_size, sx + 44, sy + 105, select_color1(dispSpeed, Role::getMaxValue()->Speed));

    // 技能 section
    int x = x_ + 20;
    int y = y_ + 155;
    // font->draw("技能", 25, x - 10, y, color_name);

    int dispFist = bs.fist;
    int dispSword = bs.sword;
    int dispKnife = bs.knife;
    int dispUnusual = bs.unusual;
    int dispHidden = bs.hidden;

    font->draw("拳掌", font_size, x, y + 30, color_ability1);
    font->draw(std::format("{:5}", dispFist), font_size, x + 44, y + 30, select_color1(dispFist, Role::getMaxValue()->Fist));
    font->draw("御劍", font_size, x, y + 55, color_ability1);
    font->draw(std::format("{:5}", dispSword), font_size, x + 44, y + 55, select_color1(dispSword, Role::getMaxValue()->Sword));
    font->draw("耍刀", font_size, x, y + 80, color_ability1);
    font->draw(std::format("{:5}", dispKnife), font_size, x + 44, y + 80, select_color1(dispKnife, Role::getMaxValue()->Knife));
    font->draw("特殊", font_size, x, y + 105, color_ability1);
    font->draw(std::format("{:5}", dispUnusual), font_size, x + 44, y + 105, select_color1(dispUnusual, Role::getMaxValue()->Unusual));
    font->draw("暗器", font_size, x, y + 130, color_ability1);
    font->draw(std::format("{:5}", dispHidden), font_size, x + 44, y + 130, select_color1(dispHidden, Role::getMaxValue()->HiddenWeapon));

    // 武学 section - beside 技能, single column
    int mx = x_ + 220;
    font->draw("武學", 25, mx - 10, y, color_name);
    auto magics = chess_.role->getLearnedMagics(chess_.star);
    for (int i = 0; i < magics.size(); i++)
    {
        auto magic = magics[i];
        int x1 = mx;
        int y1 = y + 30 + i * 25;
        int skillAtk = chess_.role->getMagicPower(magic, chess_.star);
        int opType = BattleSceneHades::getOperationType(magic->AttackAreaType);
        const char* opName = BattleSceneHades::getOperationTypeName(opType);
        font->draw(std::format("{}", magic->Name), font_size, x1, y1, color_ability1);
        font->draw(std::format("{:4} {}", skillAtk, opName), font_size, x1 + 120, y1, color_white);
    }

    // Owned pieces
    x = x_ + 10;
    y = y_ + 310;
    font->draw("擁有", font_size, x, y, color_name);
    int ox = x + 50;
    std::map<int, int> starCounts;
    for (auto& [instanceId, chess] : gd.roster().items())
    {
        if (chess.role->ID == chess_.role->ID)
            starCounts[chess.star]++;
    }
    for (auto& [star, count] : starCounts)
    {
        std::string stars;
        for (int i = 0; i < star; i++) stars += "★";
        font->draw(std::format("{} x{}", stars, count), font_size, ox, y, color_white);
        ox += 120;
    }

    // Combo affiliations
    y += 28;
    auto roleCombos = KysChess::ChessCombo::getCombosForRole(chess_.role->ID);
    if (!roleCombos.empty())
    {
        font->draw("羈絆", font_size, x, y, color_name);
        y += 24;
        auto& gameState = KysChess::GameState::get();
        auto starByRole = KysChess::ChessCombo::buildStarMap(KysChess::ChessManager(gameState.roster(), gameState.equipmentInventory(), gameState.economy()).getSelectedForBattle());
        auto& allCombos = KysChess::ChessCombo::getAllCombos();
        for (auto cid : roleCombos)
        {
            auto& combo = allCombos[(int)cid];
            auto [owned, effective] = KysChess::computeOwnership(combo, starByRole);
            // Active if any threshold is satisfied
            bool active = false;
            for (auto& t : combo.thresholds)
                if (effective >= t.count) { active = true; break; }
            Color col = active ? Color{0, 255, 100, 255} : Color{180, 180, 180, 255};
            font->draw(std::format("{}", combo.name), font_size - 2, x + 10, y, col);
            y += 20;
            if (y > y_ + panelH - 10) break;
        }
    }

    // Equipment display (only if instanceId is provided)
    if (chess_.id.value >= 0)
    {
        y += 8;
        if (y < y_ + panelH - 30)
        {
            font->draw("裝備", font_size, x, y, color_name);
            y += 24;
            int weaponId = chess_.weaponInstance.itemId;
            int armorId = chess_.armorInstance.itemId;
            if (weaponId >= 0)
            {
                auto* eq = KysChess::ChessEquipment::getById(weaponId);
                if (eq && eq->getItem())
                {
                    TextureManager::getInstance()->renderTexture("item", weaponId, x + 10, y, color_white, 255, 0.24, 0.24);
                    font->draw(eq->getItem()->Name, font_size - 2, x + 34, y + 2, color_equip);
                    y += 24;
                }
            }
            if (armorId >= 0)
            {
                auto* eq = KysChess::ChessEquipment::getById(armorId);
                if (eq && eq->getItem())
                {
                    TextureManager::getInstance()->renderTexture("item", armorId, x + 10, y, color_white, 255, 0.24, 0.24);
                    font->draw(eq->getItem()->Name, font_size - 2, x + 34, y + 2, color_equip);
                }
            }
        }
    }
}

}