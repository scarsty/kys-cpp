#include "ChessCliController.h"
#include "ChessGameSessionTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>

using namespace KysChess;
using namespace KysChess::Test;

namespace
{

ChessCliController managementController(
    const std::shared_ptr<const ChessGameContent>& content)
{
    return ChessCliController([content](Difficulty difficulty) {
        return difficulty == Difficulty::Normal ? content : nullptr;
    });
}

}

TEST_CASE("interactive commands submit the same typed actions as direct play", "[chess][cli][protocol]")
{
    const auto content = managementContent();
    auto controller = managementController(content);
    controller.newSession(Difficulty::Normal, 7, ChessCliOutputMode::Compact);
    ChessGameSession direct(content, 7);

    controller.executeInteractive("lock on", ChessCliOutputMode::Compact);
    ChessAction action;
    action.type = ChessActionType::SetShopLocked;
    action.value = true;
    const auto expected = direct.submitAndDrain(action);

    REQUIRE(controller.protocol().session());
    CHECK(controller.protocol().session()->observe().stateHash == direct.observe().stateHash);
    CHECK(controller.protocol().session()->journal().evidenceHash() == expected.evidenceHash);
}

TEST_CASE("JSONL controller writes exactly one protocol response per line", "[chess][cli][protocol]")
{
    const auto content = managementContent();
    auto controller = managementController(content);
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

TEST_CASE("interactive controller rejects JSON output mode", "[chess][cli]")
{
    const auto content = managementContent();
    auto controller = managementController(content);
    std::istringstream input("quit\n");
    std::ostringstream output;

    CHECK(controller.runInteractive(input, output, ChessCliOutputMode::Json) == 2);
    CHECK(output.str().contains("請使用 JSONL 模式"));
}

TEST_CASE("interactive CLI exposes the gameplay position swap option", "[chess][cli][protocol]")
{
    const auto content = managementContent();
    auto controller = managementController(content);
    controller.newSession(Difficulty::Normal, 11, ChessCliOutputMode::Compact);

    controller.executeInteractive("position_swap off", ChessCliOutputMode::Compact);

    REQUIRE(controller.protocol().session());
    CHECK_FALSE(controller.protocol().session()->observe().options.positionSwapEnabled);
}

TEST_CASE("interactive CLI rejects missing operands without mutating state", "[chess][cli]")
{
    const auto content = managementContent();
    auto controller = managementController(content);
    controller.newSession(Difficulty::Normal, 7, ChessCliOutputMode::Compact);
    REQUIRE(controller.protocol().session());
    const auto before = controller.protocol().session()->observe();

    const auto output = controller.executeInteractive("buy", ChessCliOutputMode::Compact);

    const auto after = controller.protocol().session()->observe();
    CHECK(output == "用法：buy <商店欄位>\n");
    CHECK(after.money == before.money);
    CHECK(after.stateHash == before.stateHash);
}

TEST_CASE("interactive CLI surfaces rejected actions", "[chess][cli][protocol]")
{
    const auto content = managementContent();
    auto controller = managementController(content);
    controller.newSession(Difficulty::Normal, 7, ChessCliOutputMode::Compact);

    const auto output = controller.executeInteractive("buy 99", ChessCliOutputMode::Compact);

    CHECK(output.contains("操作失敗（invalid_shop_slot）"));
    CHECK(output.contains("金幣$"));
}

TEST_CASE("interactive CLI legal output uses accepted human commands", "[chess][cli]")
{
    const auto content = managementContent();
    auto controller = managementController(content);
    controller.newSession(Difficulty::Normal, 7, ChessCliOutputMode::Compact);

    const auto output = controller.executeInteractive("legal", ChessCliOutputMode::Human);

    CHECK(output.contains("buy <商店欄位>"));
    CHECK(output.contains("refresh"));
    CHECK_FALSE(output.contains("buy_shop_slot"));
    CHECK_FALSE(output.starts_with('{'));
}

TEST_CASE("interactive CLI formats save and load responses for humans", "[chess][cli][save]")
{
    const auto content = managementContent();
    auto controller = managementController(content);
    controller.newSession(Difficulty::Normal, 7, ChessCliOutputMode::Compact);

    const auto saved = controller.executeInteractive("save test", ChessCliOutputMode::Compact);
    controller.executeInteractive("lock on", ChessCliOutputMode::Compact);
    const auto loaded = controller.executeInteractive("load test", ChessCliOutputMode::Compact);

    CHECK(saved == "已建立存檔「test」（修訂 1）。\n");
    CHECK(loaded.contains("已載入存檔「test」"));
    REQUIRE(controller.protocol().session());
    CHECK_FALSE(controller.protocol().session()->observe().shopLocked);
}

TEST_CASE("interactive new validates difficulty and seed", "[chess][cli]")
{
    const auto content = managementContent();
    auto controller = managementController(content);
    controller.newSession(Difficulty::Normal, 7, ChessCliOutputMode::Compact);
    REQUIRE(controller.protocol().session());
    const auto before = controller.protocol().session()->observe().stateHash;

    const auto output = controller.executeInteractive(
        "new impossible not-a-seed",
        ChessCliOutputMode::Compact);

    CHECK(output == "用法：new <easy|normal|hard> <種子>\n");
    CHECK(controller.protocol().session()->observe().stateHash == before);
}
