#pragma once

#include "ChessEffectDescription.h"
#include "ChessCatalogQueries.h"
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

struct PanelTextColumnLayout
{
    std::size_t firstBlock{};
    std::size_t lastBlock{};
    PanelTextLayout layout;
};

struct PanelTextColumnsLayout
{
    int baseFontSize{};
    int columnWidth{};
    std::vector<PanelTextColumnLayout> columns;
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
    const auto appendEffects = [&](const RenderedEffectDescription& effects, PanelTextTone tone) {
        auto effectRows = panelTextRowsForEffectDescription(effects, 0, 2);
        for (auto& row : effectRows) row.tone = tone;
        rows.insert(rows.end(),
            std::make_move_iterator(effectRows.begin()),
            std::make_move_iterator(effectRows.end()));
    };
    const auto appendCombos = [&](const std::vector<std::string>& names, PanelTextTone tone) {
        for (const auto& name : names)
            appendText(chessNonBattleRuleDescription(CountsAsComboRule{ name }, true), tone);
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
        appendCombos(bonus.countsAsCombos, applicable ? PanelTextTone::Combo : tone);
        appendEffects(bonus.effects, tone);
    }
    return rows;
}

inline PanelTextLayout layoutPanelText(
    std::span<const PanelTextSourceRow> rows,
    int pixelWidth,
    int baseFontSize);

inline PanelTextLayout fitPanelText(
    std::span<const PanelTextSourceRow> rows,
    int pixelWidth,
    int pixelHeight,
    int preferredFontSize,
    int minimumFontSize);

inline std::optional<PanelTextColumnsLayout> fitPanelTextBlocks(
    const std::vector<std::vector<PanelTextSourceRow>>& blocks,
    int pixelWidth,
    int pixelHeight,
    int preferredFontSize,
    int minimumFontSize,
    int blockGap = 0);

struct LabelValueColumn
{
    Font* font;
    int fontSize;
    int labelX;
    int valueX;
    Color labelColor;

    void line(int rowY, const char* label, const std::string& value, Color valueColor) const
    {
        font->draw(label, fontSize, labelX, rowY, labelColor);
        font->draw(value, fontSize, valueX, rowY, valueColor);
    }
};

void showChessMessage(const std::string& text, int fontSize = 32);
enum class ChessChoiceStyle { Options, Confirmation, DestructiveConfirmation };
int runChessChoiceMenu(const std::string& title, const std::vector<std::string>& choices,
    ChessChoiceStyle style = ChessChoiceStyle::Options);
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

inline PanelTextLayout fitPanelText(
    std::span<const PanelTextSourceRow> rows,
    int pixelWidth,
    int pixelHeight,
    int preferredFontSize,
    int minimumFontSize)
{
    assert(pixelHeight > 0);
    assert(preferredFontSize >= minimumFontSize);
    assert(minimumFontSize > 0);
    for (int fontSize = preferredFontSize; fontSize >= minimumFontSize; --fontSize)
    {
        auto layout = layoutPanelText(rows, pixelWidth, fontSize);
        if (layout.height <= pixelHeight)
        {
            return layout;
        }
    }
    assert(false && "panel text does not fit at the minimum readable font size");
    return layoutPanelText(rows, pixelWidth, minimumFontSize);
}

inline std::optional<PanelTextColumnsLayout> fitPanelTextBlocks(
    const std::vector<std::vector<PanelTextSourceRow>>& blocks,
    int pixelWidth,
    int pixelHeight,
    int preferredFontSize,
    int minimumFontSize,
    int blockGap)
{
    assert(!blocks.empty());
    assert(pixelWidth > 0);
    assert(pixelHeight > 0);
    assert(preferredFontSize >= minimumFontSize);
    assert(minimumFontSize > 0);
    const auto combineBlocks = [&](std::size_t first, std::size_t last)
    {
        std::vector<PanelTextSourceRow> result;
        for (std::size_t index = first; index < last; ++index)
        {
            const auto firstRow = result.size();
            result.insert(result.end(), blocks[index].begin(), blocks[index].end());
            if (index > first)
                result[firstRow].spacingBefore += blockGap;
        }
        return result;
    };

    std::optional<PanelTextColumnsLayout> chosen;
    int chosenTallestColumn{};
    const int columnLimit = std::min(2, static_cast<int>(blocks.size()));
    for (int columns = 1; columns <= columnLimit; ++columns)
    {
        const int columnWidth = pixelWidth / columns;
        const std::size_t firstSplit = columns == 1 ? blocks.size() : 1;
        const std::size_t lastSplit = columns == 1 ? blocks.size() : blocks.size() - 1;
        for (std::size_t split = firstSplit; split <= lastSplit; ++split)
        {
            std::vector<std::pair<std::size_t, std::size_t>> ranges{
                {0, split},
            };
            if (columns == 2) ranges.push_back({split, blocks.size()});
            for (int fontSize = preferredFontSize; fontSize >= minimumFontSize; --fontSize)
            {
                std::vector<PanelTextColumnLayout> columnLayouts;
                int tallest{};
                bool fits = true;
                for (const auto [first, last] : ranges)
                {
                    const auto rows = combineBlocks(first, last);
                    auto layout = layoutPanelText(rows, columnWidth, fontSize);
                    tallest = std::max(tallest, layout.height);
                    fits = fits && layout.height <= pixelHeight;
                    columnLayouts.push_back({
                        .firstBlock = first,
                        .lastBlock = last,
                        .layout = std::move(layout),
                    });
                }
                if (!fits) continue;
                if (!chosen
                    || fontSize > chosen->baseFontSize
                    || (fontSize == chosen->baseFontSize
                        && columns < static_cast<int>(chosen->columns.size()))
                    || (fontSize == chosen->baseFontSize
                        && columns == static_cast<int>(chosen->columns.size())
                        && tallest < chosenTallestColumn))
                {
                    chosen = PanelTextColumnsLayout{
                        .baseFontSize = fontSize,
                        .columnWidth = columnWidth,
                        .columns = std::move(columnLayouts),
                    };
                    chosenTallestColumn = tallest;
                }
                break;
            }
        }
    }
    return chosen;
}

}    // namespace KysChess
