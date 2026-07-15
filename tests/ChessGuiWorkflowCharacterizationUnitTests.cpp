#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{

std::string readGuiAdapter()
{
    const auto path =
        std::filesystem::current_path() / "src" / "ChessGuiSessionAdapter.cpp";
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    return std::string(std::istreambuf_iterator<char>(input), {});
}

void checkNearby(
    std::string_view source,
    std::string_view anchor,
    std::string_view expected,
    std::size_t maximumDistance = 1200)
{
    const auto anchorPosition = source.find(anchor);
    REQUIRE(anchorPosition != std::string_view::npos);
    const auto expectedPosition = source.find(expected, anchorPosition);
    REQUIRE(expectedPosition != std::string_view::npos);
    CHECK(expectedPosition - anchorPosition < maximumDistance);
}

}  // namespace

TEST_CASE("graphical decision screens remain wired to the same session actions",
          "[chess][gui][workflow-characterization]")
{
    const auto source = readGuiAdapter();
    constexpr std::array mappings{
        "case ChessActionType::BuyShopSlot: showShop();",
        "case ChessActionType::SellChess: chooseChess(descriptor.type);",
        "case ChessActionType::SetDeployment: chooseDeployment();",
        "case ChessActionType::AddBan: chooseBan(descriptor);",
        "case ChessActionType::Equip: chooseEquipment(descriptor);",
        "case ChessActionType::BuyLegendaryEquipment: chooseLegendary(descriptor);",
        "case ChessActionType::StartChallenge: return chooseChallenge(descriptor);",
        "case ChessActionType::ChooseReward: return chooseReward(descriptor);",
        "case ChessActionType::ChooseMap: chooseMap(descriptor);",
        "case ChessActionType::SwapPositions: chooseSwap(descriptor);",
        "case ChessActionType::SetShopLocked: action.value = !session_.state().shopLocked;",
        "case ChessActionType::SetPositionSwapEnabled: action.value = !session_.state().options.positionSwapEnabled;",
    };
    for (const auto mapping : mappings)
    {
        CAPTURE(mapping);
        CHECK(source.contains(mapping));
    }

    checkNearby(
        source,
        "if (descriptor.type == ChessActionType::PrepareBattle)",
        "return drainPreparedBattle();");
    checkNearby(
        source,
        "case ChessContextMenuAction::EnterBattle:",
        "legalAction(session_, ChessActionType::PrepareBattle)");
}

TEST_CASE("graphical confirmations retain their labels and accepted row",
          "[chess][gui][workflow-characterization]")
{
    const auto source = readGuiAdapter();

    checkNearby(
        source,
        R"(std::vector<std::string>{"確認購買", "取消"})",
        "if (menu->getResult() == 0)");
    checkNearby(
        source,
        R"(std::vector<std::string>{"確認逆天改命", "取消"})",
        "if (menu->getResult() == 0)");
    checkNearby(
        source,
        R"(std::vector<std::string>{"繼續選擇", "跳過剩餘禁棋"})",
        "if (confirm->getResult() == 1)");
}

TEST_CASE("graphical battle and reward workflows retain their interaction order",
          "[chess][gui][workflow-characterization]")
{
    const auto source = readGuiAdapter();

    checkNearby(
        source,
        "while (session_.state().phase == ChessSessionPhase::BattlePreparation)",
        "chooseMap(*map)");
    checkNearby(
        source,
        "chooseMap(*map)",
        "continue;");
    checkNearby(
        source,
        "showBattlePreview();",
        "std::make_shared<BattleSceneHades>(session_)");
    checkNearby(
        source,
        "std::make_shared<BattleSceneHades>(session_)",
        "battle->run();");

    checkNearby(
        source,
        "while (session_.state().phase == ChessSessionPhase::RewardChoice)",
        "chooseBan(*ban)");
    checkNearby(
        source,
        "chooseBan(*ban)",
        "ChessActionType::SkipForcedBans");
    checkNearby(
        source,
        "skip.type = ChessActionType::SkipForcedBans;",
        "chooseReward(*choose)");
}
