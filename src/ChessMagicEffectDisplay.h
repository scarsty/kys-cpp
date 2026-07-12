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
    const MagicSave* magic = nullptr;
    std::string text;
    bool ultimate = false;
};

std::vector<ChessMagicEffectDisplayLine> buildChessMagicEffectDisplayRows(
    const std::vector<const MagicSave*>& magics,
    const std::vector<ChessMagicEffectDefinition>& definitions,
    int ultimateMagicId);

}  // namespace KysChess
