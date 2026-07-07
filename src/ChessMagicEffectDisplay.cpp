#include "ChessMagicEffectDisplay.h"

#include <algorithm>
#include <utility>

namespace KysChess
{
namespace
{

int magicPowerForStar(const Role& role, int magicId, int star)
{
    const int effectiveStar = star > 0 ? star : role.Star;
    const int start = RoleSave::getMagicSlotStart(effectiveStar);
    const int end = RoleSave::getMagicSlotEnd(effectiveStar);
    for (int i = start; i < end; ++i)
    {
        if (role.MagicID[i] == magicId)
        {
            return role.MagicPower[i];
        }
    }
    return 0;
}

Magic* selectedUltimateMagic(
    const Role& role,
    int star,
    const std::vector<Magic*>& magics)
{
    if (role.UsingMagic)
    {
        return role.UsingMagic;
    }

    Magic* selected = nullptr;
    int selectedPower = 0;
    for (auto* magic : magics)
    {
        const int power = magic ? magicPowerForStar(role, magic->ID, star) : 0;
        if (!selected || power > selectedPower)
        {
            selected = magic;
            selectedPower = power;
        }
    }
    return selected;
}

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
    const Role& role,
    int star,
    const std::vector<Magic*>& magics,
    const std::vector<ChessMagicEffectDefinition>& definitions)
{
    std::vector<ChessMagicEffectDisplayLine> rows;
    const auto* ultimateMagic = selectedUltimateMagic(role, star, magics);
    const int ultimateMagicId = ultimateMagic ? ultimateMagic->ID : -1;

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

const std::vector<ChessMagicEffectDefinition>& chessMagicEffectDefinitionsForDisplay()
{
    static const std::vector<ChessMagicEffectDefinition> definitions = []
    {
        std::vector<ChessMagicEffectDefinition> loaded;
        ChessBattleEffects::loadDefaultMagicEffectsFile(loaded);
        return loaded;
    }();
    return definitions;
}

}  // namespace KysChess
