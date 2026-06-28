#include "ChessChallengeMenu.h"
#include "ChessProgress.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

TEST_CASE("challenge menu paginates expedition challenges nine at a time", "[chess][challenge]")
{
    BalanceConfig config;
    config.challenges.resize(10);
    config.challenges[0].name = "喬峰";
    config.challenges[1].name = "慕容";

    ChessProgress progress;
    progress.completeChallenge(1);

    const auto setup = buildChallengeMenuSetup(config.challenges, progress, PanelAnchor{120, 80});

    CHECK(setup.config.perPage == 9);
    REQUIRE(setup.data.labels.size() == 10);
    CHECK(setup.data.labels[0] == "喬峰");
    CHECK(setup.data.labels[1] == "[已通關] 慕容");
    CHECK(setup.data.colors[0].r == 255);
    CHECK(setup.data.colors[0].g == 200);
    CHECK(setup.data.colors[0].b == 100);
    CHECK(setup.data.colors[0].a == 255);
    CHECK(setup.data.colors[1].r == 120);
    CHECK(setup.data.colors[1].g == 120);
    CHECK(setup.data.colors[1].b == 120);
    CHECK(setup.data.colors[1].a == 255);
}
