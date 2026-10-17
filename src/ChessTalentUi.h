#pragma once

#include "ChessBalance.h"
#include "ChessUiCommon.h"
#include "ChessUiStyle.h"
#include "DisplayText.h"
#include "Engine.h"
#include "Font.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace KysChess
{

inline const char* chessTalentShortDescription(ChessTalentId id)
{
    switch (id)
    {
    case ChessTalentId::DivineArms: return "獲得更多高階裝備，終局可用神兵商店。";
    case ChessTalentId::LateBloomer: return "勝場成長受星級及百分比效果加成，培養長期主力。";
    case ChessTalentId::Gambler: return "開局額外禁棋；付費刷新為一枚棋子累積賭運，提高致命傷害免死機率。";
    case ChessTalentId::Backbone: return "非三費友軍的額外星級強化三費棋子的內力與攻防；三費升二星後，下次商店保證三費。";
    case ChessTalentId::Alliance: return "開局為一個羈絆永久加點，高階羈絆提前啟用。";
    }
    std::unreachable();
}

struct ChessTalentFactTableMetrics
{
    int fontSize{};
    int height{};
    int headerHeight{};
    int categoryWidth{};
    int labelWidth{};
    int valueWidth{};
};

inline Color chessTalentFactAccent(ChessTalentFactKind kind)
{
    switch (kind)
    {
    case ChessTalentFactKind::Equipment:
        return ChessUiStyle::gold;
    case ChessTalentFactKind::TalentEquipment:
        return {210, 182, 229, 255};
    case ChessTalentFactKind::Shop:
        return ChessUiStyle::context;
    case ChessTalentFactKind::Mechanic:
        return ChessUiStyle::positive;
    }
    std::unreachable();
}

inline bool chessTalentPresentationUsesEquipmentTable(
    const ChessTalentPresentation& presentation)
{
    return std::any_of(
        presentation.facts.begin(),
        presentation.facts.end(),
        [](const auto& fact) { return fact.kind != ChessTalentFactKind::Mechanic; });
}

inline std::vector<std::string> wrapChessTalentDescription(
    const std::string& text,
    int maximumWidth)
{
    std::vector<std::string> result;
    std::size_t lineStart = 0;
    while (true)
    {
        const std::size_t lineEnd = text.find('\n', lineStart);
        const auto paragraph = text.substr(
            lineStart,
            lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart);
        const auto wrapped = wrapDisplayText(paragraph, maximumWidth);
        if (wrapped.empty())
        {
            result.emplace_back();
        }
        else
        {
            result.insert(result.end(), wrapped.begin(), wrapped.end());
        }
        if (lineEnd == std::string::npos)
        {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return result;
}

inline int chessTalentFactColumnWidth(int pixelWidth, int column)
{
    assert(pixelWidth >= 320);
    if (column == 0)
    {
        return pixelWidth * (pixelWidth >= 800 ? 18 : 27) / 100;
    }
    if (column == 1)
    {
        return pixelWidth * (pixelWidth >= 800 ? 22 : 27) / 100;
    }
    return pixelWidth
        - chessTalentFactColumnWidth(pixelWidth, 0)
        - chessTalentFactColumnWidth(pixelWidth, 1);
}

inline int chessTalentFactLineCount(
    const std::string& text,
    int pixelWidth,
    int fontSize)
{
    const int units = displayTextUnitsForPixelWidth(fontSize, pixelWidth - 16);
    return std::max(1, static_cast<int>(wrapDisplayText(text, units).size()));
}

inline int chessTalentFactRowHeight(
    const ChessTalentFactRow& fact,
    int fontSize,
    int categoryWidth,
    int labelWidth,
    int valueWidth)
{
    const int lineCount = std::max({
        chessTalentFactLineCount(fact.category, categoryWidth, fontSize),
        chessTalentFactLineCount(fact.label, labelWidth, fontSize),
        chessTalentFactLineCount(fact.value, valueWidth, fontSize),
    });
    return lineCount * (fontSize + 4) + 8;
}

inline ChessTalentFactTableMetrics measureChessTalentFactTable(
    const ChessTalentPresentation& presentation,
    int pixelWidth,
    int pixelHeight,
    int preferredFontSize = 16,
    int minimumFontSize = 12)
{
    assert(!presentation.facts.empty());
    assert(pixelWidth >= 320);
    assert(pixelHeight > 0);
    assert(preferredFontSize >= minimumFontSize);
    const int categoryWidth = chessTalentFactColumnWidth(pixelWidth, 0);
    const int labelWidth = chessTalentFactColumnWidth(pixelWidth, 1);
    const int valueWidth = chessTalentFactColumnWidth(pixelWidth, 2);

    for (int fontSize = preferredFontSize; fontSize >= minimumFontSize; --fontSize)
    {
        const int headerHeight = fontSize + 14;
        int height = headerHeight + 2;
        for (const auto& fact : presentation.facts)
        {
            height += chessTalentFactRowHeight(
                fact,
                fontSize,
                categoryWidth,
                labelWidth,
                valueWidth);
            height += 1;
        }
        if (height <= pixelHeight)
        {
            return {
                fontSize,
                height,
                headerHeight,
                categoryWidth,
                labelWidth,
                valueWidth,
            };
        }
    }

    assert(false && "棋手天賦事實表無法在最小字級容納");
    const int fontSize = minimumFontSize;
    const int headerHeight = fontSize + 14;
    int height = headerHeight + 2;
    for (const auto& fact : presentation.facts)
    {
        height += chessTalentFactRowHeight(
            fact,
            fontSize,
            categoryWidth,
            labelWidth,
            valueWidth) + 1;
    }
    return {
        fontSize,
        height,
        headerHeight,
        categoryWidth,
        labelWidth,
        valueWidth,
    };
}

// 固定可讀字級，按完整資料列分頁；過長的單列按文字行延續。
inline std::vector<ChessTalentPresentation> paginateChessTalentFacts(
    const ChessTalentPresentation& presentation, int width, int height, int fontSize)
{
    const int header = fontSize + 16;
    const int available = height - header;
    const int maxLines = std::max(1, (available - 9) / (fontSize + 4));
    std::vector<ChessTalentPresentation> pages(1, presentation);
    pages.back().facts.clear();
    int used{};
    for (const auto& fact : presentation.facts)
    {
        const std::array cells{fact.category, fact.label, fact.value};
        std::array<std::vector<std::string>, 3> lines;
        int count = 1;
        for (int c = 0; c < 3; ++c)
        {
            lines[c] = wrapDisplayText(cells[c], displayTextUnitsForPixelWidth(fontSize, chessTalentFactColumnWidth(width, c) - 16));
            count = std::max(count, static_cast<int>(lines[c].size()));
        }
        for (int start = 0; start < count; start += maxLines)
        {
            auto part = fact;
            std::array<std::string*, 3> values{&part.category, &part.label, &part.value};
            for (int c = 0; c < 3; ++c)
            {
                values[c]->clear();
                for (int i = start; i < std::min(start + maxLines, static_cast<int>(lines[c].size())); ++i)
                    *values[c] += lines[c][i];
            }
            const int rowHeight = chessTalentFactRowHeight(part, fontSize,
                chessTalentFactColumnWidth(width, 0), chessTalentFactColumnWidth(width, 1), chessTalentFactColumnWidth(width, 2)) + 1;
            if (used + rowHeight > available && !pages.back().facts.empty())
            {
                pages.push_back(presentation);
                pages.back().facts.clear();
                used = 0;
            }
            pages.back().facts.push_back(std::move(part));
            used += rowHeight;
        }
    }
    return pages;
}

inline void drawChessTalentFactTable(
    const ChessTalentPresentation& presentation,
    int x,
    int y,
    int pixelWidth,
    int pixelHeight,
    int preferredFontSize = 16,
    int minimumFontSize = 12)
{
    const auto metrics = measureChessTalentFactTable(
        presentation,
        pixelWidth,
        pixelHeight,
        preferredFontSize,
        minimumFontSize);
    auto* engine = Engine::getInstance();
    auto* font = Font::getInstance();
    const bool equipmentTable = chessTalentPresentationUsesEquipmentTable(presentation);

    engine->fillRoundedRect(ChessUiStyle::quietFill, x, y, pixelWidth, metrics.height, 8);
    engine->fillColor(ChessUiStyle::activeFill, x + 1, y + 1, pixelWidth - 2, metrics.headerHeight);

    constexpr int padding = 8;
    if (equipmentTable)
    {
        font->draw("來源", metrics.fontSize, x + padding, y + 5, {175, 215, 235, 255});
        font->draw(
            "關卡",
            metrics.fontSize,
            x + metrics.categoryWidth + padding,
            y + 5,
            {225, 215, 175, 255});
        font->draw(
            "獎勵內容",
            metrics.fontSize,
            x + metrics.categoryWidth + metrics.labelWidth + padding,
            y + 5,
            {215, 190, 245, 255});
    }
    else
    {
        font->draw("機制", metrics.fontSize, x + padding, y + 5, {175, 235, 215, 255});
        font->draw(
            "條件",
            metrics.fontSize,
            x + metrics.categoryWidth + padding,
            y + 5,
            {225, 215, 175, 255});
        font->draw(
            "效果",
            metrics.fontSize,
            x + metrics.categoryWidth + metrics.labelWidth + padding,
            y + 5,
            {175, 215, 245, 255});
    }

    int rowY = y + metrics.headerHeight + 2;
    for (std::size_t index = 0; index < presentation.facts.size(); ++index)
    {
        const auto& fact = presentation.facts[index];
        const int rowHeight = chessTalentFactRowHeight(
            fact,
            metrics.fontSize,
            metrics.categoryWidth,
            metrics.labelWidth,
            metrics.valueWidth);
        const auto accent = chessTalentFactAccent(fact.kind);
        const Color rowFill = index % 2 == 0
            ? Color{115, 98, 67, 24}
            : Color{115, 98, 67, 8};
        engine->fillColor(rowFill, x + 1, rowY, pixelWidth - 2, rowHeight);
        engine->fillColor(accent, x + 1, rowY, 5, rowHeight);

        const auto categoryLines = wrapDisplayText(
            fact.category,
            displayTextUnitsForPixelWidth(metrics.fontSize, metrics.categoryWidth - 16));
        const auto labelLines = wrapDisplayText(
            fact.label,
            displayTextUnitsForPixelWidth(metrics.fontSize, metrics.labelWidth - 16));
        const auto valueLines = wrapDisplayText(
            fact.value,
            displayTextUnitsForPixelWidth(metrics.fontSize, metrics.valueWidth - 16));
        const auto drawCell = [&](
            const std::vector<std::string>& lines,
            int cellX,
            Color color) {
            int lineY = rowY + 4;
            for (const auto& line : lines)
            {
                font->draw(line, metrics.fontSize, cellX + padding, lineY, color);
                lineY += metrics.fontSize + 4;
            }
        };
        drawCell(categoryLines, x, accent);
        drawCell(
            labelLines,
            x + metrics.categoryWidth,
            {225, 225, 215, 255});
        drawCell(
            valueLines,
            x + metrics.categoryWidth + metrics.labelWidth,
            accent);

        engine->fillColor(
            ChessUiStyle::line,
            x + 1,
            rowY + rowHeight - 1,
            pixelWidth - 2,
            1);
        rowY += rowHeight + 1;
    }
}

}    // namespace KysChess
