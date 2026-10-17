#include "ChessSelectionLayout.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

TEST_CASE("ChessSelectionLayout_CompactCanvasChangesCataloguePagination", "[chess][ui]")
{
    CHECK_FALSE(chessSelectionCompact(1280, 720));
    CHECK_FALSE(chessSelectionCompact(1024, 576));
    CHECK(chessSelectionCompact(844, 390));
    CHECK(chessSelectionColumns(ChessSelectionLayout::Catalogue, false) == 4);
    CHECK(chessSelectionPageSize(ChessSelectionLayout::Catalogue, false) == 12);
    CHECK(chessSelectionColumns(ChessSelectionLayout::Catalogue, true) == 1);
    CHECK(chessSelectionPageSize(ChessSelectionLayout::Catalogue, true) == 3);
}

TEST_CASE("ChessSelectionLayout_TalentAndShortChoicesUseTheirOwnColumns", "[chess][ui]")
{
    CHECK(chessSelectionColumns(ChessSelectionLayout::Talent, false) == 1);
    CHECK(chessSelectionColumns(ChessSelectionLayout::Talent, true) == 1);
    CHECK(chessSelectionPageSize(ChessSelectionLayout::Talent, true) == 5);
    CHECK(chessSelectionPageSize(ChessSelectionLayout::Talent, false) == 5);
    CHECK(chessSelectionColumns(ChessSelectionLayout::ShortChoice, false) == 3);
    CHECK(chessSelectionPageSize(ChessSelectionLayout::ShortChoice, false) == 3);
    CHECK(chessSelectionColumns(ChessSelectionLayout::ShortChoice, true) == 1);
    CHECK(chessSelectionPageSize(ChessSelectionLayout::ShortChoice, true) == 3);
}

TEST_CASE("ChessSelectionLayout_KeyboardFocusWrapsWithoutChangingSelection", "[chess][ui]")
{
    const int selected = 0;
    int focused = 0;
    focused = nextChessSelectionFocus(focused, 3);
    CHECK(focused == 1);
    CHECK(selected == 0);
    focused = nextChessSelectionFocus(focused, 3);
    CHECK(focused == 2);
    CHECK(nextChessSelectionFocus(focused, 3) == 0);
    CHECK(nextChessSelectionFocus(0, 3, true) == 2);
}
