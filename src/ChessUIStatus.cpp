#include "ChessUIStatus.h"
#include "BattleRoleManager.h"
#include "BattleSceneHades.h"
#include "ChessCombo.h"
#include "ChessEquipment.h"
#include "ChessManager.h"
#include "ChessPool.h"
#include "ChessScreenLayout.h"
#include "ChessUiCommon.h"
#include "Engine.h"
#include "GameUtil.h"
#include "GameState.h"

#include <algorithm>

namespace KysChess {

namespace
{

constexpr int kAvatarMinWidth = 96;
constexpr int kAvatarHeight = 128;
constexpr int kStatsDetailOffsetY = 4;
constexpr int kSkillTopGap = 14;
constexpr int kOwnedTextInset = 8;
constexpr int kMagicBottomReserve = 10;
constexpr int kMagicValueMinOffset = 92;
constexpr int kMagicValuePreferredOffset = 110;
constexpr int kMagicValueMaxOffset = 130;
constexpr int kComboRowGap = 2;
constexpr int kEquipIconOffsetX = 46;
constexpr int kEquipIconSize = 28;
constexpr int kEquipIconTopAdjust = 3;
constexpr int kEquipFallbackTopAdjust = 2;
constexpr int kEquipNameGap = 8;

struct StatusBlock
{
    int x;
    int y;
    int w;
    int h;
};

struct StatusLayout
{
    int fontSize = 22;
    int smallFontSize = 20;
    int titleFontSize = 24;
    int pad = 12;
    int gap = 14;
    int lineHeight = fontSize + 4;
    int topY = 0;
    int bottomY = 0;
    int sectionTitleY = 0;
    int sectionContentY = 0;
    int magicStartY = 0;
    int magicAvailableRows = 1;
    int magicCols = 1;
    int magicColWidth = 0;
    int comboRows = 2;
    int comboCols = 1;
    int comboColWidth = 0;
    StatusBlock panel{};
    StatusBlock avatar{};
    StatusBlock magic{};
    StatusBlock owned{};
    StatusBlock combo{};
    StatusBlock equip{};
    LabelValueColumn statsColumn{};
    LabelValueColumn skillCol1{};
    LabelValueColumn skillCol2{};

    static StatusLayout build(Font* font, const StatusBlock& panel, int avatarWidth)
    {
        StatusLayout layout;
        constexpr int kTopTextY = 18;
        constexpr int kAvatarTop = 14;
        constexpr int kStatValueOffset = 62;
        constexpr int kSkillValueOffset = 44;
        constexpr int kSkillSecondColumnX = 118;
        constexpr int kMagicStartOffsetX = 210;
        constexpr int kMagicHeaderHeight = 30;
        constexpr int kBottomSectionMinHeight = 96;
        constexpr int kBottomSectionMaxHeight = 128;
        constexpr int kBottomSectionTitleTop = 6;
        constexpr int kBottomSectionHeaderHeight = 28;
        constexpr int kOwnedSectionWidth = 150;
        constexpr int kEquipSectionWidth = 200;
        constexpr int kComboSectionMinWidth = 160;

        layout.panel = panel;
        layout.topY = panel.y + kTopTextY;
        layout.avatar = {panel.x + layout.pad, panel.y + kAvatarTop, avatarWidth, kAvatarHeight};

        int bottomHeight = std::clamp(panel.h / 3, kBottomSectionMinHeight, kBottomSectionMaxHeight);
        layout.bottomY = panel.y + panel.h - layout.pad - bottomHeight;
        layout.sectionTitleY = layout.bottomY + kBottomSectionTitleTop;
        layout.sectionContentY = layout.sectionTitleY + kBottomSectionHeaderHeight;

        int statsX = layout.avatar.x + layout.avatar.w + layout.gap;
        layout.statsColumn = {font, layout.fontSize, statsX, statsX + kStatValueOffset, {255, 250, 205, 255}};
        layout.skillCol1 = {font, layout.fontSize, layout.avatar.x, layout.avatar.x + kSkillValueOffset, {255, 250, 205, 255}};
        layout.skillCol2 = {font, layout.fontSize, layout.avatar.x + kSkillSecondColumnX, layout.avatar.x + kSkillSecondColumnX + kSkillValueOffset, {255, 250, 205, 255}};

        int magicX = statsX + kMagicStartOffsetX;
        layout.magic = {magicX, layout.topY, panel.x + panel.w - layout.pad - magicX, layout.bottomY - layout.topY};
        layout.magicStartY = layout.magic.y + kMagicHeaderHeight;
        layout.magicAvailableRows = std::max(1, (layout.bottomY - kMagicBottomReserve - layout.magicStartY) / layout.lineHeight);

        int innerW = panel.w - layout.pad * 2;
        int comboW = std::max(kComboSectionMinWidth, innerW - kOwnedSectionWidth - kEquipSectionWidth - layout.gap * 2);
        layout.owned = {panel.x + layout.pad, layout.sectionTitleY, kOwnedSectionWidth, bottomHeight};
        layout.combo = {layout.owned.x + layout.owned.w + layout.gap, layout.sectionTitleY, comboW, bottomHeight};
        layout.equip = {layout.combo.x + layout.combo.w + layout.gap, layout.sectionTitleY, kEquipSectionWidth, bottomHeight};

        // Sections keep a stable horizontal layout regardless of whether the
        // current preview chess has equipment to show.
        return layout;
    }

    void finalizeMagicColumns(int magicCount)
    {
        magicCols = magicCount > magicAvailableRows ? 2 : 1;
        constexpr int kMagicMinColumnWidth = 170;
        magicColWidth = magicCols == 1 ? magic.w : std::max(kMagicMinColumnWidth, (magic.w - gap) / 2);
    }

    void finalizeComboColumns(int comboCount)
    {
        comboRows = std::max(2, (combo.h - 32) / (smallFontSize + 2));
        comboCols = comboCount > comboRows ? 2 : 1;
        constexpr int kComboMinColumnWidth = 90;
        comboColWidth = comboCols == 1 ? combo.w : std::max(kComboMinColumnWidth, (combo.w - gap) / 2);
    }
};

Rect scaledTextureRect(TextureWarpper* texture, int x, int y, int targetH, int minW = 0)
{
    texture->load();
    int renderH = std::max(1, targetH);
    int renderW = texture->h > 0 ? texture->w * renderH / std::max(1, texture->h) : renderH;
    renderW = std::max(renderW, minW);
    return {x + texture->dx, y + texture->dy, renderW, renderH};
}

}

void ChessUIStatus::draw()
{
    if (!chess_.role) return;

    auto defaultFrame = ChessScreenLayout::shopStatusPanel();
    int panelW = (w_ > 0) ? w_ : defaultFrame.w;
    int panelH = (h_ > 0) ? h_ : defaultFrame.h;

    // Draw translucent black background
    Engine::getInstance()->fillRoundedRect({0, 0, 0, 128}, x_, y_, panelW, panelH, 8);
    Engine::getInstance()->drawRoundedRect({180, 170, 140, 200}, x_, y_, panelW, panelH, 8);

    auto& gd = KysChess::GameState::get();
    auto font = Font::getInstance();
    Color color_white = { 255, 255, 255, 255 };
    Color color_name = { 255, 215, 0, 255 };

    // Compute boosted stats for display
    auto bs = KysChess::BattleRoleManager::computeStarStats(chess_.role, chess_.star, chess_.fightsWon);
    int dispMaxHP = bs.hp;
    int dispAttack = bs.atk;
    int dispDefence = bs.def;
    int dispSpeed = bs.spd;

    Color color_ability1 = { 255, 250, 205, 255 };
    Color color_red = { 255, 90, 60, 255 };
    Color color_magic = { 236, 200, 40, 255 };
    Color color_purple = { 208, 152, 208, 255 };
    Color color_equip = { 210, 230, 220, 255 };
    Color color_inactive = { 180, 180, 180, 255 };

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

    int avatarW = 128;
    bool showEquip = chess_.id.value >= 0;

    StatusBlock panel{x_, y_, panelW, panelH};

    if (auto* headTexture = TextureManager::getInstance()->getTexture("head", chess_.role->HeadID))
    {
        headTexture->load();
        if (headTexture->h > 0)
        {
            avatarW = std::max(kAvatarMinWidth, headTexture->w * kAvatarHeight / std::max(1, headTexture->h));
        }
    }

    auto layout = StatusLayout::build(font, panel, avatarW);

    if (auto* headTexture = TextureManager::getInstance()->getTexture("head", chess_.role->HeadID))
    {
        auto rect = scaledTextureRect(headTexture, layout.avatar.x, layout.avatar.y, layout.avatar.h, 96);
        TextureManager::getInstance()->renderTexture(headTexture, rect.x, rect.y, TextureManager::RenderInfo{ color_white, 255 }, rect.w, rect.h);
    }
    else
    {
        TextureManager::getInstance()->renderTexture("head", chess_.role->HeadID, layout.avatar.x, layout.avatar.y);
    }

    int statsY = layout.topY;
    Color c = color_white;
    if (chess_.role->MPType == 0) c = color_purple;
    else if (chess_.role->MPType == 1) c = color_magic;
    layout.statsColumn.line(statsY + layout.lineHeight * 0, "生命", std::format("{:5}/{:5}", dispMaxHP, dispMaxHP), color_white);
    layout.statsColumn.line(statsY + layout.lineHeight * 1, "內力", std::format("{:5}/{:5}", chess_.role->MP, GameUtil::MAX_MP), c);
    layout.statsColumn.line(statsY + layout.lineHeight * 2 + kStatsDetailOffsetY, "攻擊", std::format("{:5}", dispAttack), select_color1(dispAttack, Role::getMaxValue()->Attack));
    layout.statsColumn.line(statsY + layout.lineHeight * 3 + kStatsDetailOffsetY, "防禦", std::format("{:5}", dispDefence), select_color1(dispDefence, Role::getMaxValue()->Defence));
    layout.statsColumn.line(statsY + layout.lineHeight * 4 + kStatsDetailOffsetY, "輕功", std::format("{:5}", dispSpeed), select_color1(dispSpeed, Role::getMaxValue()->Speed));

    int dispFist = bs.fist;
    int dispSword = bs.sword;
    int dispKnife = bs.knife;
    int dispUnusual = bs.unusual;
    int skillY = layout.avatar.y + layout.avatar.h + kSkillTopGap;
    layout.skillCol1.line(skillY + layout.lineHeight * 0, "拳掌", std::format("{:5}", dispFist), select_color1(dispFist, Role::getMaxValue()->Fist));
    layout.skillCol2.line(skillY + layout.lineHeight * 0, "御劍", std::format("{:5}", dispSword), select_color1(dispSword, Role::getMaxValue()->Sword));
    layout.skillCol1.line(skillY + layout.lineHeight * 1, "耍刀", std::format("{:5}", dispKnife), select_color1(dispKnife, Role::getMaxValue()->Knife));
    layout.skillCol2.line(skillY + layout.lineHeight * 1, "特殊", std::format("{:5}", dispUnusual), select_color1(dispUnusual, Role::getMaxValue()->Unusual));

    font->draw("武學", layout.titleFontSize, layout.magic.x, layout.magic.y, color_name);
    auto magics = chess_.role->getLearnedMagics(chess_.star);
    layout.finalizeMagicColumns(static_cast<int>(magics.size()));
    for (int i = 0; i < magics.size(); i++)
    {
        int colIndex = i / layout.magicAvailableRows;
        int row = i % layout.magicAvailableRows;
        if (colIndex >= layout.magicCols)
        {
            break;
        }
        auto magic = magics[i];
        int colX = layout.magic.x + colIndex * (layout.magicColWidth + layout.gap);
        int rowY = layout.magicStartY + row * layout.lineHeight;
        int skillAtk = chess_.role->getMagicPower(magic, chess_.star);
        int opType = BattleSceneHades::getOperationType(magic->AttackAreaType);
        const char* opName = BattleSceneHades::getOperationTypeName(opType);
        int magicValueX = colX + std::min(kMagicValueMaxOffset, std::max(kMagicValueMinOffset, layout.magicColWidth - kMagicValuePreferredOffset));
        font->draw(std::format("{}", magic->Name), layout.fontSize, colX, rowY, color_ability1);
        font->draw(std::format("{:4} {}", skillAtk, opName), layout.fontSize, magicValueX, rowY, color_white);
    }

    font->draw("擁有", layout.titleFontSize, layout.owned.x, layout.sectionTitleY, color_name);
    std::map<int, int> starCounts;
    for (auto& [instanceId, chess] : gd.roster().items())
    {
        if (chess.role->ID == chess_.role->ID)
            starCounts[chess.star]++;
    }
    if (starCounts.empty())
    {
        font->draw("無", layout.smallFontSize, layout.owned.x + kOwnedTextInset, layout.sectionContentY, color_inactive);
    }
    else
    {
        PanelTextCursor ownedCursor{font, layout.owned.x + kOwnedTextInset, layout.sectionContentY};
        for (auto& [star, count] : starCounts)
        {
            std::string stars;
            for (int i = 0; i < star; i++) stars += "★";
            ownedCursor.line(std::format("{} x{}", stars, count), layout.smallFontSize, color_white, 4);
        }

        if (showEquip)
        {
            ownedCursor.line(std::format("勝場 {}", chess_.fightsWon), layout.smallFontSize, color_ability1, 4);
        }
    }

    if (starCounts.empty() && showEquip)
    {
        font->draw(std::format("勝場 {}", chess_.fightsWon), layout.smallFontSize,
            layout.owned.x + kOwnedTextInset, layout.sectionContentY + layout.smallFontSize + 8, color_ability1);
    }

    auto roleCombos = KysChess::ChessCombo::getCombosForRole(chess_.role->ID);
    font->draw("羈絆", layout.titleFontSize, layout.combo.x, layout.sectionTitleY, color_name);
    if (!roleCombos.empty())
    {
        auto& gameState = KysChess::GameState::get();
        auto starByRole = KysChess::ChessCombo::buildStarMap(KysChess::ChessManager(gameState.roster(), gameState.equipmentInventory(), gameState.economy()).getSelectedForBattle());
        auto& allCombos = KysChess::ChessCombo::getAllCombos();
        layout.finalizeComboColumns(static_cast<int>(roleCombos.size()));
        for (int index = 0; index < roleCombos.size(); ++index)
        {
            int colIndex = index / layout.comboRows;
            int row = index % layout.comboRows;
            if (colIndex >= layout.comboCols)
            {
                break;
            }
            auto cid = roleCombos[index];
            auto& combo = allCombos[(int)cid];
            auto [owned, effective] = KysChess::computeOwnership(combo, starByRole);
            bool active = false;
            for (auto& t : combo.thresholds)
                if (effective >= t.count) { active = true; break; }
            Color labelColor = active ? Color{0, 255, 100, 255} : Color{180, 180, 180, 255};
            font->draw(combo.name, layout.smallFontSize, layout.combo.x + colIndex * layout.comboColWidth, layout.sectionContentY + row * (layout.smallFontSize + kComboRowGap), labelColor);
        }
    }
    else
    {
        font->draw("無", layout.smallFontSize, layout.combo.x + kOwnedTextInset, layout.sectionContentY, color_inactive);
    }

    if (showEquip)
    {
        font->draw("裝備", layout.titleFontSize, layout.equip.x, layout.sectionTitleY, color_name);
        PanelTextCursor equipCursor{font, layout.equip.x, layout.sectionContentY};

        auto drawEquipLine = [&](const char* slotName, const InstancedItem& instance) {
            int rowY = equipCursor.y;
            auto* equipment = instance.itemId >= 0 ? KysChess::ChessEquipment::getById(instance.itemId) : nullptr;
            auto* item = equipment ? equipment->getItem() : nullptr;
            int iconX = equipCursor.x + kEquipIconOffsetX;
            font->draw(slotName, layout.smallFontSize, equipCursor.x, rowY, color_ability1);
            if (item)
            {
                if (auto* itemTexture = TextureManager::getInstance()->getTexture("item", instance.itemId))
                {
                    auto rect = scaledTextureRect(itemTexture, iconX, rowY - kEquipIconTopAdjust, kEquipIconSize);
                    TextureManager::getInstance()->renderTexture(itemTexture, rect.x, rect.y, TextureManager::RenderInfo{ color_white, 255 }, rect.w, rect.h);
                }
                else
                {
                    TextureManager::getInstance()->renderTexture("item", instance.itemId, iconX, rowY - kEquipFallbackTopAdjust,
                        TextureManager::RenderInfo{ color_white, 255, 0.26, 0.26 });
                }
                font->draw(item->Name, layout.smallFontSize, iconX + kEquipIconSize + kEquipNameGap, rowY, color_equip);
            }
            else
            {
                font->draw("無", layout.smallFontSize, iconX, rowY, color_inactive);
            }
            equipCursor.skip(layout.smallFontSize + 8);
        };

        drawEquipLine("武器", chess_.weaponInstance);
        drawEquipLine("護甲", chess_.armorInstance);
    }
}

}