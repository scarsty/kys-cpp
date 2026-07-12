#include "ChessReplayJson.h"
#include "ChessReplayVerifier.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace KysChess;

namespace
{

std::shared_ptr<const ChessGameContent> goldenContent(bool immediatelyComplete)
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;
    data.balance.initialMoney = 20;
    data.balance.shopSlotCount = 3;
    data.balance.totalFights = immediatelyComplete ? 0 : 3;
    data.balance.minBattleSize = 1;
    for (auto& weights : data.balance.shopWeights)
    {
        weights = {100, 0, 0, 0, 0};
    }
    ChessRoleDefinition role;
    role.ID = 10;
    role.Name = "黃金測試棋子";
    role.Cost = 1;
    role.MaxHP = 100;
    role.Attack = 10;
    data.roles.emplace(role.ID, role);
    data.poolRoleIds.push_back(role.ID);
    return std::make_shared<const ChessGameContent>(std::move(data));
}

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}

void writeText(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

bool updateGoldensRequested()
{
    char* value = nullptr;
    std::size_t size{};
    _dupenv_s(&value, &size, "KYS_UPDATE_REPLAY_GOLDENS");
    const bool requested = value != nullptr;
    std::free(value);
    return requested;
}

}

TEST_CASE("committed short and complete replay goldens verify", "[chess][replay][golden][determinism]")
{
    const auto root = std::filesystem::current_path() / "tests" / "data" / "replays";
    const auto shortPath = root / "short_v1.jsonl";
    const auto completePath = root / "complete_v1.jsonl";

    const auto shortContent = goldenContent(false);
    ChessGameSession shortSession(shortContent, 0x1234);
    ChessAction lock;
    lock.type = ChessActionType::SetShopLocked;
    lock.value = true;
    REQUIRE(shortSession.submitAndDrain(lock).accepted);
    ChessAction buy;
    buy.type = ChessActionType::BuyShopSlot;
    buy.shopSlot = 0;
    REQUIRE(shortSession.submitAndDrain(buy).accepted);
    const auto exportedShortReplay = shortSession.exportReplay();
    REQUIRE(exportedShortReplay);
    const auto shortJson = serializeChessReplayJsonl(*exportedShortReplay);

    const auto completeContent = goldenContent(true);
    ChessGameSession completeSession(completeContent, 0x5678);
    ChessAction finish;
    finish.type = ChessActionType::FinishRun;
    REQUIRE(completeSession.submitAndDrain(finish).accepted);
    const auto exportedCompleteReplay = completeSession.exportReplay();
    REQUIRE(exportedCompleteReplay);
    const auto completeJson = serializeChessReplayJsonl(*exportedCompleteReplay);

    if (updateGoldensRequested())
    {
        writeText(shortPath, shortJson);
        writeText(completePath, completeJson);
    }

    REQUIRE(std::filesystem::exists(shortPath));
    REQUIRE(std::filesystem::exists(completePath));
    CHECK(readText(shortPath) == shortJson);
    CHECK(readText(completePath) == completeJson);

    ChessReplayJsonError error;
    const auto shortReplay = parseChessReplayJsonl(readText(shortPath), error);
    REQUIRE(shortReplay);
    CHECK(ChessReplayVerifier::verify(shortContent, *shortReplay).valid);
    const auto completeReplay = parseChessReplayJsonl(readText(completePath), error);
    REQUIRE(completeReplay);
    CHECK(completeReplay->footer.complete);
    CHECK(ChessReplayVerifier::verify(completeContent, *completeReplay).valid);
}
