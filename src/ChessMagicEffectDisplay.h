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

struct PositionedChessMagicEffectDisplayLine
{
    ChessMagicEffectDisplayLine content;
    int x{};
    int y{};
    int width{};
    int height{};
    int fontSize{};
};

struct ChessMagicEffectDisplayLayout
{
    std::vector<PositionedChessMagicEffectDisplayLine> lines;
    int viewportWidth{};
    int viewportHeight{};
    int requiredHeight{};
    int skillFontSize{};
    int effectFontSize{};
    int skillValueX{};
};

ChessMagicEffectDisplayLayout layoutChessMagicEffectDisplay(
    const std::vector<ChessMagicEffectDisplayLine>& rows,
    int viewportWidth,
    int viewportHeight);

}  // namespace KysChess
