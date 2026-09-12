#include "ChessEffectDescription.h"

#include <cassert>

namespace KysChess
{

RenderedEffectDescription describeGameplayEffects(std::span<const GameplayEffect> effects, EffectDescriptionStyle style)
{
    RenderedEffectDescription result;
    if (effects.empty()) { return result; }
    RenderedEffectDescriptionSection section;
    for (const auto& effect : effects)
    {
        assert(effect);
        auto text = effect->describe(style);
        assert(!text.empty());
        RenderedEffectDescriptionBlock block;
        block.rows.push_back({
            .kind = EffectDescriptionRowKind::Prose,
            .text = std::move(text),
            .breakBefore
            = section.blocks.empty() ? EffectDescriptionSemanticBreak::None : EffectDescriptionSemanticBreak::Block,
            .wrapping = DisplayTextWrapping::Prose,
        });
        section.blocks.push_back(std::move(block));
    }
    result.sections.push_back(std::move(section));
    return result;
}

std::vector<std::string> effectDescriptionTextRows(const RenderedEffectDescription& rendered)
{
    std::vector<std::string> result;
    for (const auto& section : rendered.sections)
    {
        if (section.heading) { result.push_back(*section.heading); }
        for (const auto& block : section.blocks)
        {
            for (const auto& row : block.rows) { result.push_back(row.text); }
        }
    }
    return result;
}

std::string joinEffectDescriptionRows(const RenderedEffectDescription& rendered, std::string_view separator)
{
    std::string result;
    for (const auto& row : effectDescriptionTextRows(rendered))
    {
        if (!result.empty()) { result += separator; }
        result += row;
    }
    return result;
}

}    // namespace KysChess
