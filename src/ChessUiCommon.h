#pragma once

#include "ChessEffectDescription.h"
#include "ChessCatalogQueries.h"
#include "ChessUiStyle.h"
#include "DisplayText.h"
#include "Font.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace KysChess
{
void showChessComboOverview(const ChessGameContent& content, const ChessSessionState& state, int comboId = -1);
void showChessComboOverview(const ChessGameContent& content, std::span<const ChessComboMetadata> combos,
    std::span<const int> deployedRoles, int comboId, const std::string& title);


inline const EquipmentDef* equipmentDefinition(const ChessGameContent& content, int itemId)
{
    const auto found = std::ranges::find(content.equipment(), itemId, &EquipmentDef::itemId);
    return found == content.equipment().end() ? nullptr : &*found;
}

inline const ChessEquipmentInstance* equipmentInstance(const ChessSessionState& state, int instanceId)
{
    const auto found = state.equipmentInventory.find(instanceId);
    return found == state.equipmentInventory.end() ? nullptr : &found->second;
}

inline const ChessSessionPiece* rosterPiece(const ChessSessionState& state, int instanceId)
{
    const auto found = state.roster.find(instanceId);
    return found == state.roster.end() ? nullptr : &found->second;
}

struct PanelTextCursor
{
    Font* font;
    int x;
    int y;

    void line(const std::string& text, int fontSize, Color color, int extraSpacing = 4, int indent = 0)
    {
        font->draw(text, fontSize, x + indent, y, color);
        y += fontSize + extraSpacing;
    }

    void skip(int spacing)
    {
        y += spacing;
    }
};

enum class PanelTextTone { Body, Heading, Effect, Exclusive, Inactive, Combo, Context };

inline Color panelTextColor(PanelTextTone tone)
{
    using namespace ChessUiStyle;
    switch (tone)
    {
    case PanelTextTone::Heading: return gold;
    case PanelTextTone::Effect: return positive;
    case PanelTextTone::Exclusive: return exclusive;
    case PanelTextTone::Inactive: return muted;
    case PanelTextTone::Combo:
    case PanelTextTone::Context: return context;
    default: return ink;
    }
}

struct PanelTextSourceRow
{
    std::string text;
    int fontSizeDelta{};
    int indentUnits{};
    int spacingBefore{};
    int spacingAfter{};
    DisplayTextWrapping wrapping{};
    PanelTextTone tone{};
};

struct PanelTextPhysicalLine
{
    std::size_t sourceRow{};
    std::string text;
    int fontSize{};
    int indentPixels{};
    int y{};
    PanelTextTone tone{};
};

struct PanelTextLayout
{
    int baseFontSize{};
    int height{};
    std::vector<PanelTextPhysicalLine> lines;
};

inline std::vector<PanelTextSourceRow> panelTextRowsForEffectDescription(
    const RenderedEffectDescription& rendered,
    int fontSizeDelta = 0,
    int extraSpacing = 3,
    int baseIndentUnits = 0)
{
    std::vector<PanelTextSourceRow> result;
    bool firstRow = true;
    for (const auto& section : rendered.sections)
    {
        if (section.heading)
        {
            result.push_back({
                .text = *section.heading,
                .fontSizeDelta = fontSizeDelta,
                .indentUnits = baseIndentUnits,
                .spacingBefore = firstRow ? 0 : extraSpacing,
                .spacingAfter = extraSpacing,
            });
            firstRow = false;
        }
        for (const auto& block : section.blocks)
        {
            for (const auto& row : block.rows)
            {
                result.push_back({
                    .text = row.text,
                    .fontSizeDelta = fontSizeDelta,
                    .indentUnits = baseIndentUnits + row.indent * 2,
                    .spacingBefore = !firstRow
                            && row.breakBefore != EffectDescriptionSemanticBreak::None
                        ? extraSpacing
                        : 0,
                    .spacingAfter = extraSpacing,
                    .wrapping = row.wrapping,
                });
                firstRow = false;
            }
        }
    }
    return result;
}

inline std::vector<PanelTextSourceRow> panelTextRowsForEquipment(
    const ChessEquipmentMetadata& equipment, std::optional<int> recipientRoleId = std::nullopt)
{
    std::vector<PanelTextSourceRow> rows;
    const auto appendText = [&](std::string text, PanelTextTone tone, bool heading = false) {
        rows.push_back({
            .text = std::move(text),
            .fontSizeDelta = heading ? 2 : 0,
            .spacingBefore = heading && !rows.empty() ? 6 : 0,
            .spacingAfter = 2,
            .tone = tone,
        });
    };
    const auto appendEffects = [&](const RenderedEffectDescription& effects, PanelTextTone tone, int indent = 0) {
        auto effectRows = panelTextRowsForEffectDescription(effects, 0, 2, indent);
        for (auto& row : effectRows) row.tone = tone;
        rows.insert(rows.end(),
            std::make_move_iterator(effectRows.begin()),
            std::make_move_iterator(effectRows.end()));
    };
    const auto appendCombos = [&](const std::vector<std::string>& names, PanelTextTone tone, int indent = 0) {
        for (const auto& name : names)
        {
            appendText(chessNonBattleRuleDescription(CountsAsComboRule{ name }, true), tone);
            rows.back().indentUnits = indent;
        }
    };
    if (!equipment.specialEffects.sections.empty())
    {
        appendText("特殊效果：", PanelTextTone::Heading, true);
        appendEffects(equipment.specialEffects, PanelTextTone::Effect);
    }
    appendCombos(equipment.countsAsCombos, PanelTextTone::Combo);
    for (const auto& bonus : equipment.characterBonuses)
    {
        std::string heading;
        for (const auto& role : bonus.roles)
        {
            if (!heading.empty()) heading += "、";
            heading += role;
        }
        heading += "專屬";
        const bool applicable = !recipientRoleId || std::ranges::contains(bonus.roleIds, *recipientRoleId);
        if (recipientRoleId) heading += applicable ? "（適用）" : "（不適用）";
        const auto tone = applicable ? PanelTextTone::Exclusive : PanelTextTone::Inactive;
        appendText(heading + "：", tone, true);
        appendCombos(bonus.countsAsCombos, applicable ? PanelTextTone::Combo : tone, 2);
        appendEffects(bonus.effects, tone, 2);
    }
    return rows;
}

inline PanelTextLayout layoutPanelText(
    std::span<const PanelTextSourceRow> rows,
    int pixelWidth,
    int baseFontSize);

void showChessMessage(const std::string& text, int fontSize = 32);
enum class ChessChoiceStyle { Options, Confirmation, DestructiveConfirmation };
int runChessChoiceMenu(const std::string& title, const std::vector<std::string>& choices,
    ChessChoiceStyle style = ChessChoiceStyle::Options);
bool confirmChessBan(const std::string& roleName);
void playChessUpgradeSound();
int getRandomChessMusic();
int getRandomBattleMusic();
bool isChessSceneMusic(int musicId);
inline int displayTextUnitsForPixelWidth(int fontSize, int pixelWidth, int indent = 0)
{
    assert(fontSize > 0);
    assert(pixelWidth - indent >= fontSize);
    return (pixelWidth - indent) * 2 / fontSize;
}

inline PanelTextLayout layoutPanelText(
    std::span<const PanelTextSourceRow> rows,
    int pixelWidth,
    int baseFontSize)
{
    assert(pixelWidth > 0);
    assert(baseFontSize > 0);
    PanelTextLayout result{
        .baseFontSize = baseFontSize,
    };
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const auto& row = rows[rowIndex];
        assert(!row.text.empty());
        const int fontSize = baseFontSize + row.fontSizeDelta;
        assert(fontSize > 0);
        const int indentPixels = row.indentUnits * fontSize / 2;
        const int displayWidth = displayTextUnitsForPixelWidth(
            fontSize,
            pixelWidth,
            indentPixels);
        const auto wrapped = wrapDisplayText(row.text, displayWidth, true, row.wrapping);
        assert(!wrapped.empty());
        result.height += row.spacingBefore;
        for (std::size_t lineIndex = 0; lineIndex < wrapped.size(); ++lineIndex)
        {
            result.lines.push_back({
                .sourceRow = rowIndex,
                .text = wrapped[lineIndex],
                .fontSize = fontSize,
                .indentPixels = indentPixels,
                .y = result.height,
                .tone = row.tone,
            });
            result.height += fontSize;
            if (rowIndex + 1 < rows.size() || lineIndex + 1 < wrapped.size())
                result.height += row.spacingAfter;
        }
    }
    return result;
}

inline std::vector<std::vector<PanelTextPhysicalLine>> paginatePanelText(
    const PanelTextLayout& layout, int pageHeight)
{
    assert(pageHeight >= layout.baseFontSize);
    std::vector<std::vector<PanelTextPhysicalLine>> pages(1);
    int origin{};
    for (auto line : layout.lines)
    {
        if (!pages.back().empty() && line.y + line.fontSize - origin > pageHeight)
        {
            pages.emplace_back();
            origin = line.y;
        }
        line.y -= origin;
        pages.back().push_back(std::move(line));
    }
    return pages;
}

inline std::vector<std::vector<PanelTextPhysicalLine>> paginatePanelText(
    std::span<const PanelTextSourceRow> rows, int width, int fontSize, int pageHeight)
{
    return paginatePanelText(layoutPanelText(rows, width, fontSize), pageHeight);
}

}    // namespace KysChess
