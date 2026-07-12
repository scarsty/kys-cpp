#include "ChessAsciiBoard.h"
#include "ChessBattleText.h"
#include "ChessObservationText.h"
#include "ChessGameSessionTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("management text is structured Traditional Chinese", "[chess][text]")
{
    const auto content = managementContent();
    ChessGameSession session(content, 1);
    const auto text = ChessObservationText::format(
        session.observe(),
        *content,
        session.legalActions());

    CHECK(text.contains("階段：整備"));
    CHECK(text.contains("商店："));
    CHECK(text.contains("測試棋子"));
    CHECK(text.contains("可用操作："));
    CHECK(text.contains("buy_shop_slot"));
}

TEST_CASE("configured free refresh presentation is combo-generic", "[chess][text][config]")
{
    ChessGameContentData data;
    ComboDef combo;
    combo.id = 4;
    combo.name = "配置智略";
    combo.thresholds.push_back({2, "配置門檻", {{EffectType::FreeRefresh, 1}}});
    data.combos.push_back(std::move(combo));
    const ChessGameContent content(std::move(data));
    ChessGameplayObservation observation;
    observation.freeShopRefreshAvailable = true;
    observation.combos.push_back({4, 2, 2, 0, -1});

    const auto text = ChessObservationText::format(observation, content, {});

    CHECK(text.contains("配置智略：下一次刷新商店免費。"));
    CHECK_FALSE(text.contains("智將"));
}

TEST_CASE("ASCII board preserves coordinates and stable unit tokens", "[chess][text][board]")
{
    const auto content = managementContent();
    PreparedChessBattle prepared;
    prepared.stableBattleId = "test";
    prepared.battleSeed = 12;
    prepared.units.push_back({1, 1, 10, 0, 1});

    const auto board = ChessAsciiBoard::render(prepared, *content);

    CHECK(board.contains(" 18 19 20 21 22"));
    CHECK(board.contains(" 30   .  . A1  .  ."));
    CHECK(board.contains("A1 測試棋子 我方 ★1 (20,30)"));
}

TEST_CASE("battle text uses semantic IDs and omits movement traces", "[chess][text][battle]")
{
    PreparedChessBattle prepared;
    prepared.units.push_back({1, 1, 10, 0});
    prepared.units.push_back({2, -1, 20, 1});
    const std::vector<Battle::BattleDigestEvent> events{
        {Battle::BattleGameplayEventType::DamageApplied, 42, 1, 2, 186, 7},
        {Battle::BattleGameplayEventType::BattleEnded, 248, -1, -1, 0, -1},
    };

    const auto text = ChessBattleText::formatEvents(prepared, events);

    CHECK(text ==
        "[   42F] A1 對 E1 造成 186 點傷害（效果=7）\n"
        "[  248F] 戰鬥勝利\n");
}
