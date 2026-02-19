#include "ChessUIStatus.h"
#include "BattleRoleManager.h"
#include "ChessCombo.h"
#include "ChessPool.h"
#include "Engine.h"
#include "GameUtil.h"
#include "TempStore.h"

void ChessUIStatus::draw()
{
    if (!role_) return;

    // Draw translucent black background
    Engine::getInstance()->fillColor({0, 0, 0, 128}, x_, y_, 460, 440);

    auto& gd = KysChess::GameData::get();
    auto font = Font::getInstance();
    Color color_white = { 255, 255, 255, 255 };
    Color color_name = { 255, 215, 0, 255 };

    // Compute boosted stats for display
    auto bs = KysChess::BattleRoleManager::computeStarStats(role_, star_);
    int dispMaxHP = bs.hp;
    int dispAttack = bs.atk;
    int dispDefence = bs.def;
    int dispSpeed = bs.spd;

    TextureManager::getInstance()->renderTexture("head", role_->HeadID, x_ + 10, y_ + 15);

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
    if (role_->MPType == 0) c = color_purple;
    else if (role_->MPType == 1) c = color_magic;
    font->draw("內力", font_size, sx, sy + 25, color_ability1);
    font->draw(std::format("{:5}/{:5}", role_->MP, GameUtil::MAX_MP), font_size, sx + 44, sy + 25, c);
    font->draw("攻擊", font_size, sx, sy + 55, color_ability1);
    font->draw(std::format("{:5}", dispAttack), font_size, sx + 44, sy + 55, select_color1(dispAttack, Role::getMaxValue()->Attack));
    font->draw("防禦", font_size, sx, sy + 80, color_ability1);
    font->draw(std::format("{:5}", dispDefence), font_size, sx + 44, sy + 80, select_color1(dispDefence, Role::getMaxValue()->Defence));
    font->draw("輕功", font_size, sx, sy + 105, color_ability1);
    font->draw(std::format("{:5}", dispSpeed), font_size, sx + 44, sy + 105, select_color1(dispSpeed, Role::getMaxValue()->Speed));

    // 技能 section
    int x = x_ + 20;
    int y = y_ + 155;
    font->draw("技能", 25, x - 10, y, color_name);

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
    for (int i = 0; i < 4; i++)
    {
        auto magic = Save::getInstance()->getRoleLearnedMagic(role_, i);
        if (magic)
        {
            int x1 = mx;
            int y1 = y + 30 + i * 25;
            int lvl = role_->getMagicLevelIndex(magic);
            int skillAtk = magic->Attack[lvl];
            font->draw(std::format("{}", magic->Name), font_size, x1, y1, color_ability1);
            font->draw(std::format("{:4}", skillAtk), font_size, x1 + 120, y1, color_white);
        }
    }

    // Owned pieces
    x = x_ + 10;
    y = y_ + 310;
    font->draw("擁有", font_size, x, y, color_name);
    int ox = x + 50;
    for (auto& [chess, count] : gd.collection.getChess())
    {
        if (chess.role->ID != role_->ID) continue;
        std::string stars;
        for (int i = 0; i <= chess.star; i++) stars += "★";
        font->draw(std::format("{} x{}", stars, count), font_size, ox, y, color_white);
        ox += 120;
    }

    // Combo affiliations
    y += 28;
    auto roleCombos = KysChess::ChessCombo::getCombosForRole(role_->ID);
    if (!roleCombos.empty())
    {
        font->draw("羈絆", font_size, x, y, color_name);
        y += 24;
        std::set<int> ownedIds;
        for (auto& ch : gd.getSelectedForBattle())
            if (ch.role) ownedIds.insert(ch.role->ID);
        auto& allCombos = KysChess::ChessCombo::getAllCombos();
        for (auto cid : roleCombos)
        {
            auto& combo = allCombos[(int)cid];
            int owned = 0;
            for (int rid : combo.memberRoleIds)
                if (ownedIds.count(rid)) owned++;
            Color col = owned >= 2 ? Color{0, 255, 100, 255} : Color{180, 180, 180, 255};
            font->draw(std::format("{}", combo.name), font_size - 2, x + 10, y, col);
            y += 20;
            if (y > y_ + 440) break;
        }
    }
}