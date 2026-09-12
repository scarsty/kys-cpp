#include "ChessMagicEffectDisplay.h"
#include "DisplayText.h"

#include <algorithm>
#include <array>
#include <cassert>

namespace KysChess
{
namespace
{

constexpr int kEffectInset = 10;
constexpr int kValueMinimumX = 92;
constexpr int kValuePreferredRightReserve = 110;
constexpr int kValueMaximumX = 130;
constexpr int kValueDisplayUnits = 9;

struct MagicDisplayFontMetrics
{
    int skillFontSize{};
    int effectFontSize{};
    int skillLineHeight{};
    int effectLineHeight{};
};

constexpr std::array<MagicDisplayFontMetrics, 9> kFontMetrics{{
    {20, 18, 24, 20},
    {19, 16, 22, 18},
    {18, 16, 20, 18},
    {19, 14, 21, 16},
    {18, 14, 20, 16},
    {18, 13, 20, 13},
    {17, 12, 19, 12},
    {16, 11, 18, 11},
    {15, 10, 17, 10},
}};

const ChessMagicEffectDefinition* findDefinition(
    const std::vector<ChessMagicEffectDefinition>& definitions,
    int magicId)
{
    auto it = std::ranges::find_if(
        definitions,
        [magicId](const ChessMagicEffectDefinition& definition)
        {
            return definition.magicId == magicId;
        });
    return it == definitions.end() ? nullptr : &*it;
}

}  // namespace

std::vector<ChessMagicEffectDisplayLine> buildChessMagicEffectDisplayRows(
    const std::vector<const MagicSave*>& magics,
    const std::vector<ChessMagicEffectDefinition>& definitions,
    int ultimateMagicId)
{
    assert(magics.size() <= 2);
    std::vector<ChessMagicEffectDisplayLine> rows;
    assert(ultimateMagicId < 0
        || std::ranges::contains(magics, ultimateMagicId, &MagicSave::ID));

    for (auto* magic : magics)
    {
        const bool ultimate = magic && magic->ID == ultimateMagicId;
        rows.push_back({
            ChessMagicEffectDisplayLineKind::Skill,
            magic,
            magic ? magic->Name : std::string{},
            ultimate,
        });

        if (!ultimate || !magic)
        {
            continue;
        }

        const auto* definition = findDefinition(definitions, magic->ID);
        if (!definition)
        {
            continue;
        }

        const auto rendered = describeGameplayEffects(definition->effects, EffectDescriptionStyle::Compact);
        for (const auto& section : rendered.sections)
        {
            for (const auto& block : section.blocks)
            {
                for (const auto& row : block.rows)
                {
                    rows.push_back({
                        .kind = ChessMagicEffectDisplayLineKind::Effect,
                        .magic = magic,
                        .text = row.text,
                        .ultimate = true,
                        .semanticIndent = row.indent,
                        .breakBefore = row.breakBefore,
                        .wrapping = row.wrapping,
                    });
                }
            }
        }
    }

    return rows;
}

ChessMagicEffectDisplayLayout layoutChessMagicEffectDisplay(
    const std::vector<ChessMagicEffectDisplayLine>& rows,
    int viewportWidth,
    int viewportHeight)
{
    assert(viewportWidth > kEffectInset);
    assert(viewportHeight > 0);
    assert(std::ranges::count(rows, ChessMagicEffectDisplayLineKind::Skill,
               &ChessMagicEffectDisplayLine::kind)
        <= 2);

    const auto makeLayout = [&](MagicDisplayFontMetrics metrics)
    {
        ChessMagicEffectDisplayLayout result{
            .viewportWidth = viewportWidth,
            .viewportHeight = viewportHeight,
            .skillFontSize = metrics.skillFontSize,
            .effectFontSize = metrics.effectFontSize,
            .skillValueX = std::min(
                kValueMaximumX,
                std::max(kValueMinimumX, viewportWidth - kValuePreferredRightReserve)),
        };
        int y = 0;
        for (const auto& row : rows)
        {
            const int fontSize = row.kind == ChessMagicEffectDisplayLineKind::Skill
                ? metrics.skillFontSize
                : metrics.effectFontSize;
            const int lineHeight = row.kind == ChessMagicEffectDisplayLineKind::Skill
                ? metrics.skillLineHeight
                : metrics.effectLineHeight;
            const int x = row.kind == ChessMagicEffectDisplayLineKind::Skill
                ? 0
                : kEffectInset + row.semanticIndent * metrics.effectFontSize;
            const int effectDisplayWidth = std::max(
                1,
                (viewportWidth - x) * 2 / metrics.effectFontSize);
            const auto wrapped = row.kind == ChessMagicEffectDisplayLineKind::Skill
                ? std::vector<std::string>{row.text}
                : wrapDisplayText(row.text, effectDisplayWidth, true, row.wrapping);
            bool firstPhysicalLine = true;
            for (const auto& text : wrapped)
            {
                auto content = row;
                content.text = text;
                if (!firstPhysicalLine)
                {
                    content.breakBefore = EffectDescriptionSemanticBreak::None;
                }
                result.lines.push_back({
                    .content = std::move(content),
                    .x = x,
                    .y = y,
                    .width = displayTextWidth(text) * fontSize / 2,
                    .height = lineHeight,
                    .fontSize = fontSize,
                });
                y += lineHeight;
                firstPhysicalLine = false;
            }
        }
        result.requiredHeight = y;
        return result;
    };

    const int minimumEffectFontSize = viewportWidth >= 244 ? 12 : 10;
    std::optional<ChessMagicEffectDisplayLayout> horizontallyValid;
    for (const auto metrics : kFontMetrics)
    {
        if (metrics.effectFontSize < minimumEffectFontSize) continue;
        auto result = makeLayout(metrics);
        const bool horizontalContentFits = std::ranges::all_of(
            result.lines,
            [&](const auto& line) {
                return line.x >= 0
                    && line.x + line.width <= viewportWidth
                    && (line.content.kind != ChessMagicEffectDisplayLineKind::Skill
                        || line.x + line.width <= result.skillValueX);
            });
        const bool skillValueFits = result.skillValueX
            + kValueDisplayUnits * result.skillFontSize / 2 <= viewportWidth;
        if (result.requiredHeight <= viewportHeight
            && horizontalContentFits
            && skillValueFits)
        {
            assert(std::ranges::all_of(result.lines, [&](const auto& line) {
                return line.x >= 0
                    && line.x + line.width <= viewportWidth
                    && line.y >= 0
                    && line.y + line.height <= viewportHeight;
            }));
            return result;
        }
        if (horizontalContentFits && skillValueFits)
            horizontallyValid = std::move(result);
    }
    assert(horizontallyValid && "武學 Compact 文案無法在最小可讀字級內水平排版");
    horizontallyValid->scrollable = true;
    horizontallyValid->scrollIndicatorHeight = horizontallyValid->effectFontSize;
    const int contentHeight = horizontallyValid->viewportHeight
        - horizontallyValid->scrollIndicatorHeight;
    assert(contentHeight > 0);
    horizontallyValid->maximumScrollOffset = std::max(
        0,
        horizontallyValid->requiredHeight - contentHeight);
    horizontallyValid->scrollStops.push_back(0);

    const auto& lines = horizontallyValid->lines;
    for (std::size_t groupStart = 0; groupStart < lines.size();)
    {
        std::size_t nextGroup = groupStart + 1;
        while (nextGroup < lines.size()
            && lines[nextGroup].content.kind != ChessMagicEffectDisplayLineKind::Skill
            && lines[nextGroup].content.breakBefore == EffectDescriptionSemanticBreak::None)
        {
            ++nextGroup;
        }

        const auto appendStop = [&](int offset)
        {
            if (offset > 0 && offset < horizontallyValid->maximumScrollOffset
                && !std::ranges::contains(horizontallyValid->scrollStops, offset))
            {
                horizontallyValid->scrollStops.push_back(offset);
            }
        };
        appendStop(lines[groupStart].y);

        const int groupBottom = lines[nextGroup - 1].y + lines[nextGroup - 1].height;
        if (groupBottom - lines[groupStart].y > contentHeight)
        {
            for (std::size_t lineIndex = groupStart + 1;
                 lineIndex < nextGroup;
                 ++lineIndex)
            {
                appendStop(lines[lineIndex].y);
            }
        }
        groupStart = nextGroup;
    }
    if (horizontallyValid->maximumScrollOffset > 0
        && !std::ranges::contains(
            horizontallyValid->scrollStops,
            horizontallyValid->maximumScrollOffset))
    {
        horizontallyValid->scrollStops.push_back(
            horizontallyValid->maximumScrollOffset);
    }
    return std::move(*horizontallyValid);
}

int clampChessMagicEffectDisplayScrollOffset(
    const ChessMagicEffectDisplayLayout& layout,
    int scrollOffset)
{
    return std::clamp(scrollOffset, 0, layout.maximumScrollOffset);
}

int stepChessMagicEffectDisplayScrollOffset(
    const ChessMagicEffectDisplayLayout& layout,
    int scrollOffset,
    int direction)
{
    assert(direction == -1 || direction == 1);
    if (!layout.scrollable || layout.scrollStops.empty()) return 0;
    scrollOffset = clampChessMagicEffectDisplayScrollOffset(layout, scrollOffset);
    if (direction > 0)
    {
        const auto next = std::ranges::find_if(
            layout.scrollStops,
            [scrollOffset](int stop) { return stop > scrollOffset; });
        return next == layout.scrollStops.end()
            ? layout.maximumScrollOffset
            : *next;
    }
    for (auto stop = layout.scrollStops.rbegin();
         stop != layout.scrollStops.rend();
         ++stop)
    {
        if (*stop < scrollOffset) return *stop;
    }
    return 0;
}

std::vector<PositionedChessMagicEffectDisplayLine> visibleChessMagicEffectDisplayLines(
    const ChessMagicEffectDisplayLayout& layout,
    int scrollOffset)
{
    scrollOffset = clampChessMagicEffectDisplayScrollOffset(layout, scrollOffset);
    const int contentHeight = layout.viewportHeight - layout.scrollIndicatorHeight;
    std::vector<PositionedChessMagicEffectDisplayLine> result;
    for (const auto& source : layout.lines)
    {
        auto line = source;
        line.y -= scrollOffset;
        if (line.y < 0 || line.y + line.height > contentHeight) continue;
        result.push_back(std::move(line));
    }
    return result;
}

}  // namespace KysChess
