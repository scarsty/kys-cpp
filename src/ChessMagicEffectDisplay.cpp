#include "ChessMagicEffectDisplay.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace KysChess
{
namespace
{

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

bool hasOffensiveCharmPair(
    const ChessMagicEffectDefinition& definition,
    const ComboEffect& effect)
{
    return std::ranges::any_of(
        definition.effects,
        [&](const ComboEffect& candidate)
        {
            return candidate.type == EffectType::OffensiveCharm
                && candidate.trigger == effect.trigger
                && candidate.triggerValue == effect.triggerValue
                && candidate.value == effect.value
                && candidate.value2 == effect.value2;
        });
}

std::string displayTextForMagicEffect(
    const ChessMagicEffectDefinition& definition,
    const ComboEffect& effect)
{
    if (effect.type == EffectType::CharmCDRDebuff && hasOffensiveCharmPair(definition, effect))
    {
        return {};
    }

    if (effect.type == EffectType::OffensiveCharm && effect.value2 > 0)
    {
        ComboEffect display = effect;
        display.type = EffectType::CharmCDRDebuff;
        return comboEffectCompactDesc(display);
    }

    return comboEffectCompactDesc(effect);
}

}  // namespace

std::vector<ChessMagicEffectDisplayLine> buildChessMagicEffectDisplayRows(
    const std::vector<const MagicSave*>& magics,
    const std::vector<ChessMagicEffectDefinition>& definitions,
    int ultimateMagicId)
{
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

        for (const auto& effect : definition->effects)
        {
            auto text = displayTextForMagicEffect(*definition, effect);
            if (!text.empty())
            {
                rows.push_back({
                    ChessMagicEffectDisplayLineKind::Effect,
                    magic,
                    std::move(text),
                    true,
                });
            }
        }
    }

    return rows;
}

}  // namespace KysChess
