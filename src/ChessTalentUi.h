#pragma once

#include "ChessBalance.h"
#include "ChessUiCommon.h"
#include "DisplayText.h"
#include "Engine.h"
#include "Font.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace KysChess
{

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
        return {255, 200, 105, 255};
    case ChessTalentFactKind::TalentEquipment:
        return {205, 165, 255, 255};
    case ChessTalentFactKind::Shop:
        return {170, 210, 255, 255};
    case ChessTalentFactKind::Mechanic:
        return {115, 220, 190, 255};
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
        return std::clamp(pixelWidth / 5, 108, 132);
    }
    if (column == 1)
    {
        return std::clamp(pixelWidth / 4, 128, 168);
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

    engine->fillRoundedRect({9, 16, 24, 235}, x, y, pixelWidth, metrics.height, 8);
    engine->drawRoundedRect({90, 105, 125, 230}, x, y, pixelWidth, metrics.height, 8);
    engine->fillColor({35, 48, 63, 245}, x + 1, y + 1, pixelWidth - 2, metrics.headerHeight);

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
            ? Color{24, 28, 36, 235}
            : Color{30, 35, 45, 235};
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
            {75, 82, 94, 180},
            x + 1,
            rowY + rowHeight - 1,
            pixelWidth - 2,
            1);
        rowY += rowHeight + 1;
    }
}

}    // namespace KysChess
