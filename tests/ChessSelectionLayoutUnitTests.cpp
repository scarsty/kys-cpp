#include "ChessSelectionLayout.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

TEST_CASE("ChessSelectionLayout_CatalogueUsesFixedCanvasCapacity", "[chess][ui]")
{
    CHECK(chessSelectionColumns(ChessSelectionLayout::Catalogue) == 4);
    CHECK(chessSelectionPageSize(ChessSelectionLayout::Catalogue) == 12);
}

TEST_CASE("ChessSelectionLayout_TalentAndShortChoicesUseTheirOwnColumns", "[chess][ui]")
{
    CHECK(chessSelectionColumns(ChessSelectionLayout::Talent) == 1);
    CHECK(chessSelectionPageSize(ChessSelectionLayout::Talent) == 5);
    CHECK(chessSelectionColumns(ChessSelectionLayout::ShortChoice) == 3);
    CHECK(chessSelectionPageSize(ChessSelectionLayout::ShortChoice) == 3);
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
