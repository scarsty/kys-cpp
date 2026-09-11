#pragma once

#include "ChessEffectDescription.h"
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
    int semanticIndent{};
    EffectDescriptionSemanticBreak breakBefore{};
    DisplayTextWrapping wrapping{};
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
    int scrollIndicatorHeight{};
    int maximumScrollOffset{};
    std::vector<int> scrollStops;
    bool scrollable = false;
};

ChessMagicEffectDisplayLayout layoutChessMagicEffectDisplay(
    const std::vector<ChessMagicEffectDisplayLine>& rows,
    int viewportWidth,
    int viewportHeight);

int clampChessMagicEffectDisplayScrollOffset(
    const ChessMagicEffectDisplayLayout& layout,
    int scrollOffset);

int stepChessMagicEffectDisplayScrollOffset(
    const ChessMagicEffectDisplayLayout& layout,
    int scrollOffset,
    int direction);

std::vector<PositionedChessMagicEffectDisplayLine> visibleChessMagicEffectDisplayLines(
    const ChessMagicEffectDisplayLayout& layout,
    int scrollOffset);

}  // namespace KysChess
