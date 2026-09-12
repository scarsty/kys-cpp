#pragma once

#include "ChessScreenLayout.h"
#include "ChessUiCommon.h"

#include <algorithm>
#include <cassert>

namespace KysChess
{

struct SessionStatusLayout
{
    int fontSize = 22;
    int smallFontSize = 20;
    int titleFontSize = 24;
    int pad = 12;
    int gap = 18;
    int lineHeight = fontSize + 8;
    int topY{};
    int bottomY{};
    int sectionTitleY{};
    int sectionContentY{};
    int skillTopY{};
    int magicStartY{};
    int magicAvailableHeight = 1;
    int comboRows = 2;
    int comboCols = 1;
    int comboColWidth{};
    PanelFrame panel{};
    PanelFrame avatar{};
    PanelFrame magic{};
    PanelFrame owned{};
    PanelFrame combo{};
    PanelFrame equip{};
    LabelValueColumn statsColumn{};
    LabelValueColumn skillCol1{};
    LabelValueColumn skillCol2{};

    static SessionStatusLayout build(Font* font, const PanelFrame& panel, int avatarWidth, int equipmentWidth)
    {
        constexpr int kTopTextY = 18;
        constexpr int kAvatarTop = 14;
        constexpr int kStatValueOffset = 32;
        constexpr int kSkillValueOffset = 32;
        constexpr int kSkillSecondColumnX = 106;
        constexpr int kMagicStartOffsetX = 138;
        constexpr int kMagicHeaderHeight = 36;
        constexpr int kBottomSectionMinHeight = 96;
        constexpr int kBottomSectionMaxHeight = 128;
        constexpr int kBottomSectionTitleTop = 6;
        constexpr int kBottomSectionHeaderHeight = 28;
        constexpr int kOwnedSectionWidth = 150;

        SessionStatusLayout layout;
        layout.panel = panel;
        layout.topY = panel.y + kTopTextY;
        layout.avatar = {panel.x + layout.pad, panel.y + kAvatarTop, avatarWidth, 128};

        const int statsBottom = layout.topY + 4 * layout.lineHeight + layout.fontSize;
        layout.skillTopY = std::max(layout.avatar.y + layout.avatar.h, statsBottom) + 14;
        const int proficiencyBottom = layout.skillTopY + layout.lineHeight + layout.fontSize;
        const int preferredBottomHeight = std::clamp(panel.h / 3, kBottomSectionMinHeight, kBottomSectionMaxHeight);
        layout.bottomY = std::max(panel.y + panel.h - layout.pad - preferredBottomHeight, proficiencyBottom + 8);
        const int bottomHeight = panel.y + panel.h - layout.pad - layout.bottomY;
        layout.sectionTitleY = layout.bottomY + kBottomSectionTitleTop;
        layout.sectionContentY = layout.sectionTitleY + kBottomSectionHeaderHeight;

        const int statsX = layout.avatar.x + layout.avatar.w + layout.gap;
        layout.statsColumn = {font, layout.fontSize, statsX, statsX + kStatValueOffset, {255, 250, 205, 255}};
        layout.skillCol1 = {font, layout.fontSize, layout.avatar.x, layout.avatar.x + kSkillValueOffset, {255, 250, 205, 255}};
        layout.skillCol2 = {font, layout.fontSize, layout.avatar.x + kSkillSecondColumnX, layout.avatar.x + kSkillSecondColumnX + kSkillValueOffset, {255, 250, 205, 255}};

        const int magicX = statsX + kMagicStartOffsetX;
        layout.magic = {magicX, layout.topY, panel.x + panel.w - layout.pad - magicX, layout.bottomY - layout.topY};
        layout.magicStartY = layout.magic.y + kMagicHeaderHeight;
        layout.magicAvailableHeight = std::max(
            1,
            layout.bottomY - 10 - layout.magicStartY);

        const int innerW = panel.w - layout.pad * 2;
        const int comboW = innerW - kOwnedSectionWidth - equipmentWidth - layout.gap * 2;
        assert(comboW > 0);
        layout.owned = {panel.x + layout.pad, layout.sectionTitleY, kOwnedSectionWidth, bottomHeight};
        layout.combo = {layout.owned.x + layout.owned.w + layout.gap, layout.sectionTitleY, comboW, bottomHeight};
        layout.equip = {panel.x + panel.w - layout.pad - equipmentWidth, layout.sectionTitleY, equipmentWidth, bottomHeight};
        return layout;
    }

    void finalizeComboColumns(int comboCount)
    {
        comboRows = std::max(2, (combo.h - 32) / (smallFontSize + 2));
        comboCols = comboCount > comboRows ? 2 : 1;
        constexpr int kComboMinColumnWidth = 90;
        comboColWidth = comboCols == 1 ? combo.w : std::max(kComboMinColumnWidth, (combo.w - gap) / 2);
    }
};


} // namespace KysChess
