#pragma once

#include "ChessBattleEffects.h"
#include "Types.h"

#include <string>
#include <vector>

namespace KysChess
{

enum class ChessMagicEffectDisplayLineKind
{
    Skill,
    Effect,
};

struct ChessMagicEffectDisplayLine
{
    ChessMagicEffectDisplayLineKind kind = ChessMagicEffectDisplayLineKind::Skill;
    Magic* magic = nullptr;
    std::string text;
    bool ultimate = false;
};

std::vector<ChessMagicEffectDisplayLine> buildChessMagicEffectDisplayRows(
    const Role& role,
    int star,
    const std::vector<Magic*>& magics,
    const std::vector<ChessMagicEffectDefinition>& definitions);

const std::vector<ChessMagicEffectDefinition>& chessMagicEffectDefinitionsForDisplay();

}  // namespace KysChess
