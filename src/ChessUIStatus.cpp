#include "ChessUIStatus.h"
#include "BattleRoleManager.h"
#include "BattleSceneHades.h"
#include "ChessCombo.h"
#include "ChessDetailPanels.h"
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
    Color color_red = { 255, 90, 60, 255 };
    Color color_magic = { 236, 200, 40, 255 };
    Color color_purple = { 208, 152, 208, 255 };
    Color color_equip = { 210, 230, 220, 255 };

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

    int font_size = 22;

    auto drawLabeledValue = [&](int baseX, int rowY, const char* label, const std::string& value, Color valueColor) {
        font->draw(label, font_size, baseX, rowY, color_ability1);
        font->draw(value, font_size, baseX + 44, rowY, valueColor);
    };

    // Stats right of avatar
    int sx = x_ + 195, sy = y_ + 20;
    Color c = color_white;
    if (chess_.role->MPType == 0) c = color_purple;
    else if (chess_.role->MPType == 1) c = color_magic;
    PanelTextCursor statCursor{font, sx, sy};
    drawLabeledValue(statCursor.x, statCursor.y, "生命", std::format("{:5}/{:5}", dispMaxHP, dispMaxHP), color_white);
    statCursor.skip(25);
    drawLabeledValue(statCursor.x, statCursor.y, "內力", std::format("{:5}/{:5}", chess_.role->MP, GameUtil::MAX_MP), c);
    statCursor.skip(30);
    drawLabeledValue(statCursor.x, statCursor.y, "攻擊", std::format("{:5}", dispAttack), select_color1(dispAttack, Role::getMaxValue()->Attack));
    statCursor.skip(25);
    drawLabeledValue(statCursor.x, statCursor.y, "防禦", std::format("{:5}", dispDefence), select_color1(dispDefence, Role::getMaxValue()->Defence));
    statCursor.skip(25);
    drawLabeledValue(statCursor.x, statCursor.y, "輕功", std::format("{:5}", dispSpeed), select_color1(dispSpeed, Role::getMaxValue()->Speed));

    // 技能 section
    int x = x_ + 20;
    int y = y_ + 155;

    int dispFist = bs.fist;
    int dispSword = bs.sword;
    int dispKnife = bs.knife;
    int dispUnusual = bs.unusual;
    int dispHidden = bs.hidden;

    PanelTextCursor skillCursor{font, x, y + 30};
    drawLabeledValue(skillCursor.x, skillCursor.y, "拳掌", std::format("{:5}", dispFist), select_color1(dispFist, Role::getMaxValue()->Fist));
    skillCursor.skip(25);
    drawLabeledValue(skillCursor.x, skillCursor.y, "御劍", std::format("{:5}", dispSword), select_color1(dispSword, Role::getMaxValue()->Sword));
    skillCursor.skip(25);
    drawLabeledValue(skillCursor.x, skillCursor.y, "耍刀", std::format("{:5}", dispKnife), select_color1(dispKnife, Role::getMaxValue()->Knife));
    skillCursor.skip(25);
    drawLabeledValue(skillCursor.x, skillCursor.y, "特殊", std::format("{:5}", dispUnusual), select_color1(dispUnusual, Role::getMaxValue()->Unusual));
    skillCursor.skip(25);
    drawLabeledValue(skillCursor.x, skillCursor.y, "暗器", std::format("{:5}", dispHidden), select_color1(dispHidden, Role::getMaxValue()->HiddenWeapon));

    // 武学 section - beside 技能, single column
    int mx = x_ + 220;
    font->draw("武學", 25, mx - 10, y, color_name);
    auto magics = chess_.role->getLearnedMagics(chess_.star);
    PanelTextCursor magicCursor{font, mx, y + 30};
    for (int i = 0; i < magics.size(); i++)
    {
        auto magic = magics[i];
        int rowY = magicCursor.y;
        int skillAtk = chess_.role->getMagicPower(magic, chess_.star);
        int opType = BattleSceneHades::getOperationType(magic->AttackAreaType);
        const char* opName = BattleSceneHades::getOperationTypeName(opType);
        font->draw(std::format("{}", magic->Name), font_size, magicCursor.x, rowY, color_ability1);
        font->draw(std::format("{:4} {}", skillAtk, opName), font_size, magicCursor.x + 120, rowY, color_white);
        magicCursor.skip(25);
    }

    // Owned pieces and combo affiliations stay on the lower-left.
    x = x_ + 10;
    y = y_ + 305;
    font->draw("擁有", font_size, x, y, color_name);
    std::map<int, int> starCounts;
    for (auto& [instanceId, chess] : gd.roster().items())
    {
        if (chess.role->ID == chess_.role->ID)
            starCounts[chess.star]++;
    }
    int ownedX = x + 50;
    for (auto& [star, count] : starCounts)
    {
        std::string stars;
        for (int i = 0; i < star; i++) stars += "★";
        font->draw(std::format("{} x{}", stars, count), font_size, ownedX, y, color_white);
        ownedX += 120;
    }

    // Combo affiliations
    y += 28;
    auto roleCombos = KysChess::ChessCombo::getCombosForRole(chess_.role->ID);
    if (!roleCombos.empty())
    {
        font->draw("羈絆", font_size, x, y, color_name);
        PanelTextCursor comboCursor{font, x + 10, y + 24};
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
            comboCursor.line(std::format("{}", combo.name), font_size - 2, col, 0);
            if (comboCursor.y > y_ + panelH - 10) break;
        }
    }

    // Equipment display uses the preview chess instance directly and stays in a fixed lower-right block.
    if (chess_.id.value >= 0)
    {
        int equipX = x_ + 235;
        int equipY = roleCombos.empty() ? y_ + 333 : y;
        PanelTextCursor equipCursor{font, equipX, equipY};
        equipCursor.line("裝備", font_size, color_name, 6);

        auto drawEquipLine = [&](const char* slotName, const InstancedItem& instance) {
            int rowY = equipCursor.y;
            auto* equipment = instance.itemId >= 0 ? KysChess::ChessEquipment::getById(instance.itemId) : nullptr;
            auto* item = equipment ? equipment->getItem() : nullptr;
            equipCursor.line(slotName, font_size - 2, color_ability1, 6);
            if (item)
            {
                TextureManager::getInstance()->renderTexture("item", instance.itemId, equipX + 64, rowY, color_white, 255, 0.24, 0.24);
                font->draw(item->Name, font_size - 2, equipX + 88, rowY + 2, color_equip);
            }
            else
            {
                font->draw("無", font_size - 2, equipX + 64, rowY + 2, {140, 140, 140, 255});
            }
        };

        drawEquipLine("武器", chess_.weaponInstance);
        drawEquipLine("護甲", chess_.armorInstance);
    }
}

}