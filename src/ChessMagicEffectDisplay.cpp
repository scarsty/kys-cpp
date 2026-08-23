#include "ChessMagicEffectDisplay.h"

#include <algorithm>
#include <cassert>

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

        for (const auto& rule : definition->rules)
        {
            rows.push_back({
                ChessMagicEffectDisplayLineKind::Effect,
                magic,
                effectDescription(rule, EffectDescriptionStyle::Compact),
                true,
            });
        }
    }

    return rows;
}

}  // namespace KysChess
