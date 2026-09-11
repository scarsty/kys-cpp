#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace KysChess
{

enum class DisplayTextWrapping
{
    Semantic,
    Prose,
};

inline int utf8DisplayTextCharacterLength(unsigned char value)
{
    if (value < 0x80) return 1;
    if ((value & 0xE0) == 0xC0) return 2;
    if ((value & 0xF0) == 0xE0) return 3;
    if ((value & 0xF8) == 0xF0) return 4;
    return 1;
}

inline int utf8DisplayTextCharacterWidth(unsigned char value)
{
    // Font::renderText 對 ASCII 前進半格，其餘字元（包括「·」）皆前進一整格。
    return value < 0x80 ? 1 : 2;
}

inline int displayTextWidth(std::string_view text)
{
    int result = 0;
    for (std::size_t index = 0; index < text.size();)
    {
        const auto value = static_cast<unsigned char>(text[index]);
        const int characterLength = utf8DisplayTextCharacterLength(value);
        if (value >= 0x80 && characterLength == 1)
        {
            ++index;
            continue;
        }
        result += utf8DisplayTextCharacterWidth(value);
        index += characterLength;
    }
    return result;
}

inline void alignDisplayTextRows(std::vector<std::string>& rows)
{
    int maximumWidth{};
    for (const auto& row : rows)
        maximumWidth = std::max(maximumWidth, displayTextWidth(row));
    for (auto& row : rows)
        row.append(maximumWidth - displayTextWidth(row), ' ');
}

inline std::vector<std::string> wrapDisplayText(
    const std::string& text,
    int maximumWidth,
    bool allowGlyphBreaks = true,
    DisplayTextWrapping wrapping = DisplayTextWrapping::Semantic)
{
    if (text.empty() || maximumWidth <= 0)
    {
        return {};
    }

    struct Glyph
    {
        std::string text;
        int width{};
        bool preferredBreakAfter = false;
        bool cannotStartLine{};
    };
    std::vector<Glyph> glyphs;
    for (std::size_t index = 0; index < text.size();)
    {
        const auto value = static_cast<unsigned char>(text[index]);
        const int characterLength = utf8DisplayTextCharacterLength(value);
        auto glyph = text.substr(index, characterLength);
        index += characterLength;
        const bool preferredBreak = glyph == " " || glyph == ":" || glyph == "："
            || glyph == "(" || glyph == ")" || glyph == "（" || glyph == "）"
            || glyph == "," || glyph == ";" || glyph == "/"
            || glyph == "，" || glyph == "、" || glyph == "；" || glyph == "。";
        const bool cannotStartLine = glyph == "，" || glyph == "。" || glyph == "；"
            || glyph == "：" || glyph == "、" || glyph == "！" || glyph == "？"
            || glyph == "）" || glyph == "」" || glyph == "』"
            || glyph == "," || glyph == "." || glyph == ";" || glyph == ":"
            || glyph == "!" || glyph == "?" || glyph == ")" || glyph == "%";
        glyphs.push_back({
            .text = std::move(glyph),
            .width = utf8DisplayTextCharacterWidth(value),
            .preferredBreakAfter = preferredBreak,
            .cannotStartLine = cannotStartLine,
        });
    }

    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < glyphs.size())
    {
        int width = 0;
        std::size_t end = start;
        std::size_t preferredBreak = start;
        int preferredWidth{};
        while (end < glyphs.size() && width + glyphs[end].width <= maximumWidth)
        {
            width += glyphs[end].width;
            if (glyphs[end].preferredBreakAfter)
            {
                preferredBreak = end + 1;
                preferredWidth = width;
            }
            ++end;
        }

        std::size_t lineEnd = end;
        if (end < glyphs.size() && preferredBreak > start
            && (wrapping == DisplayTextWrapping::Semantic
                || preferredWidth * 3 >= maximumWidth * 2))
        {
            lineEnd = preferredBreak;
        }
        else if (end < glyphs.size() && !allowGlyphBreaks)
        {
            return {};
        }
        if (lineEnd == start)
        {
            if (!allowGlyphBreaks) return {};
            lineEnd = std::min(start + 1, glyphs.size());
        }
        if (wrapping == DisplayTextWrapping::Prose && lineEnd < glyphs.size())
        {
            // 換行只處理排版：標點與前字同行，不把短引句拆成標題。
            while (lineEnd > start + 1 && glyphs[lineEnd].cannotStartLine)
                --lineEnd;
            const auto numeric = [&](std::size_t index)
            {
                return glyphs[index].text.size() == 1
                    && std::string_view("0123456789.+-%").find(glyphs[index].text[0])
                        != std::string_view::npos;
            };
            if (numeric(lineEnd) && numeric(lineEnd - 1))
            {
                auto numberStart = lineEnd;
                while (numberStart > start && numeric(numberStart - 1)) --numberStart;
                if (numberStart > start) lineEnd = numberStart;
            }
        }

        std::string line;
        for (std::size_t index = start; index < lineEnd; ++index)
        {
            line += glyphs[index].text;
        }
        while (!line.empty() && line.back() == ' ')
        {
            line.pop_back();
        }
        lines.push_back(std::move(line));

        start = lineEnd;
        while (start < glyphs.size() && glyphs[start].text == " ")
        {
            ++start;
        }
    }
    return lines;
}

}  // namespace KysChess
