#include "ChessCliController.h"
#include "ChessGameSessionTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("interactive commands submit the same typed actions as direct play", "[chess][cli][protocol]")
{
    const auto content = managementContent();
    ChessCliController controller([content](Difficulty difficulty) {
        return difficulty == Difficulty::Normal ? content : nullptr;
    });
    controller.newSession(Difficulty::Normal, 7, ChessCliOutputMode::Compact);
    ChessGameSession direct(content, 7);

    controller.executeInteractive("lock on", ChessCliOutputMode::Compact);
    ChessAction action;
    action.type = ChessActionType::SetShopLocked;
    action.value = true;
    const auto expected = direct.submitAndDrain(action);

    REQUIRE(controller.protocol().session());
    CHECK(controller.protocol().session()->observe().stateHash == expected.postStateHash);
    CHECK(controller.protocol().session()->journal().chainHash() == expected.chainHash);
}

TEST_CASE("JSONL controller writes exactly one protocol response per line", "[chess][cli][protocol]")
{
    const auto content = managementContent();
    ChessCliController controller([content](Difficulty difficulty) {
        return difficulty == Difficulty::Normal ? content : nullptr;
    });
    std::istringstream input(
        "{\"id\":\"a\",\"method\":\"new\",\"params\":{\"difficulty\":\"normal\",\"seed\":\"0x0000000000000001\"}}\n"
        "{\"id\":\"b\",\"method\":\"observe\",\"params\":{}}\n");
    std::ostringstream output;

    CHECK(controller.runJsonl(input, output) == 0);
    const auto text = output.str();
    CHECK(std::ranges::count(text, '\n') == 2);
    CHECK(text.contains("\"id\":\"a\""));
    CHECK(text.contains("\"id\":\"b\""));
}

TEST_CASE("interactive CLI exposes the gameplay position swap option", "[chess][cli][protocol]")
{
    const auto content = managementContent();
    ChessCliController controller([content](Difficulty difficulty) {
        return difficulty == Difficulty::Normal ? content : nullptr;
    });
    controller.newSession(Difficulty::Normal, 11, ChessCliOutputMode::Compact);

    controller.executeInteractive("position_swap off", ChessCliOutputMode::Compact);

    REQUIRE(controller.protocol().session());
    CHECK_FALSE(controller.protocol().session()->observe().options.positionSwapEnabled);
}
