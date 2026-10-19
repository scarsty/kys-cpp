#pragma once

namespace KysChess
{

enum class ChessSelectionLayout { Catalogue, ShortChoice, Talent, Overview, Tabbed, Command, Dialog };

inline int chessSelectionColumns(ChessSelectionLayout layout)
{
    if (layout == ChessSelectionLayout::Talent || layout == ChessSelectionLayout::Overview || layout == ChessSelectionLayout::Tabbed) return 1;
    if (layout == ChessSelectionLayout::ShortChoice) return 3;
    if (layout == ChessSelectionLayout::Command || layout == ChessSelectionLayout::Dialog) return 2;
    return 4;
}

inline int chessSelectionPageSize(ChessSelectionLayout layout)
{
    if (layout == ChessSelectionLayout::Overview) return 9;
    if (layout == ChessSelectionLayout::Catalogue) return 12;
    if (layout == ChessSelectionLayout::Command) return 8;
    if (layout == ChessSelectionLayout::Talent) return 5;
    if (layout == ChessSelectionLayout::ShortChoice) return 3;
    return 6;
}

inline int nextChessSelectionFocus(int current, int controlCount, bool reverse = false)
{
    if (controlCount == 0) return 0;
    return (current + (reverse ? controlCount - 1 : 1)) % controlCount;
}

} // namespace KysChess
