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

constexpr std::array<MagicDisplayFontMetrics, 7> kFontMetrics{{
    {20, 18, 24, 20},
    {19, 16, 22, 18},
    {19, 14, 21, 16},
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

        for (const auto& rule : definition->rules)
        {
            rows.push_back({
                ChessMagicEffectDisplayLineKind::Effect,
                magic,
                effectDescription(
                    rule,
                    EffectDescriptionStyle::Compact,
                    EffectDescriptionContext{
                        .enclosingDefaultEvent = EffectEvent::UltimateCommitted,
                    }),
                true,
            });
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
        const int effectDisplayWidth = (viewportWidth - kEffectInset) * 2
            / metrics.effectFontSize;
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
                : kEffectInset;
            const auto wrapped = row.kind == ChessMagicEffectDisplayLineKind::Skill
                ? std::vector<std::string>{row.text}
                : wrapDisplayText(row.text, effectDisplayWidth);
            for (const auto& text : wrapped)
            {
                auto content = row;
                content.text = text;
                result.lines.push_back({
                    .content = std::move(content),
                    .x = x,
                    .y = y,
                    .width = displayTextWidth(text) * fontSize / 2,
                    .height = lineHeight,
                    .fontSize = fontSize,
                });
                y += lineHeight;
            }
        }
        result.requiredHeight = y;
        return result;
    };

    for (const auto metrics : kFontMetrics)
    {
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
    }
    assert(false && "武學 Compact 文案超出最小可讀字級容量");
    return makeLayout(kFontMetrics.back());
}

}  // namespace KysChess
