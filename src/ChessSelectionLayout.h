#pragma once

#include <algorithm>

namespace KysChess
{

enum class ChessSelectionLayout { Catalogue, ShortChoice, Talent, Overview, Command, Dialog };

inline bool chessSelectionCompact(int presentedWidth, int presentedHeight)
{
    return std::min(presentedWidth / 1280.0, presentedHeight / 720.0) < 0.72;
}

inline int chessSelectionColumns(ChessSelectionLayout layout, bool compact)
{
    if (layout == ChessSelectionLayout::Talent || layout == ChessSelectionLayout::Overview) return 1;
    if (layout == ChessSelectionLayout::ShortChoice) return compact ? 1 : 3;
    if (layout == ChessSelectionLayout::Command || layout == ChessSelectionLayout::Dialog) return compact ? 1 : 2;
    return compact ? 1 : 4;
}

inline int chessSelectionPageSize(ChessSelectionLayout layout, bool compact)
{
    if (layout == ChessSelectionLayout::Overview) return 9;
    if (layout == ChessSelectionLayout::Catalogue) return compact ? 3 : 12;
    if (layout == ChessSelectionLayout::Command) return compact ? 5 : 8;
    if (layout == ChessSelectionLayout::Talent) return 5;
    if (layout == ChessSelectionLayout::ShortChoice) return 3;
    return compact ? 3 : 6;
}

inline int nextChessSelectionFocus(int current, int controlCount, bool reverse = false)
{
    if (controlCount == 0) return 0;
    return (current + (reverse ? controlCount - 1 : 1)) % controlCount;
}

} // namespace KysChess
