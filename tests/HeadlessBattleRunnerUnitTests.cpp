#include "BattleRuntimeScenarioTestHelpers.h"
#include "BattleLogTestHelpers.h"
#include "HeadlessBattleRunner.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;

namespace
{

BattleRuntimeSessionCreationInput timeoutInput()
{
    BattleRuntimeSessionCreationInput input;
    input.rules = scenarioRules();
    input.rules.maximumFrames = 1;
    input.units.push_back(scenarioSetupUnit(1, 0, 100, {100, 100, 0}));
    input.units.push_back(scenarioSetupUnit(2, 1, 100, {500, 500, 0}));
    return input;
}

}

TEST_CASE("shared runtime emits one exact timeout event", "[battle][headless][determinism]")
{
    const auto result = HeadlessBattleRunner::run(timeoutInput());

    CHECK(result.summary.outcome == BattleOutcome::Timeout);
    CHECK(result.summary.endFrame == 1);
    REQUIRE(result.digestEvents.size() == 1);
    CHECK(result.digestEvents.front().type == BattleGameplayEventType::BattleEnded);
    CHECK(result.digestEvents.front().frame == 1);
    CHECK(chessSha256Hex(result.digest) == "f33b93309040c774cb9fdd11273c109f7452827b2626a0c09ee0ee2e2de2720a");
}

TEST_CASE("simultaneous wipe is player defeat", "[battle][headless][determinism]")
{
    BattleRuntimeState runtime;
    seedScenarioRuntimeUnits(runtime, {
        scenarioRuntimeUnit(1, 0, 1, {100, 100, 0}),
        scenarioRuntimeUnit(2, 1, 1, {120, 100, 0}),
    });
    runtime.nextFrame.queueDamage(scenarioPreResolvedDamage(2, 1, 1));
    runtime.nextFrame.queueDamage(scenarioPreResolvedDamage(1, 2, 1));
    BattleRuntimeSession session(std::move(runtime));

    const auto frame = session.runFrame();

    CHECK(session.runtime().result.ended);
    CHECK(session.runtime().result.outcome == BattleOutcome::PlayerDefeat);
    CHECK(session.runtime().result.winningTeam == 1);
    CHECK(std::ranges::count_if(frame.gameplayEvents, [](const auto& event) {
        return event.type == BattleGameplayEventType::BattleEnded;
    }) == 1);
}

TEST_CASE("battle digest excludes localized report names", "[battle][headless][determinism]")
{
    auto firstAttacker = BattleLogTest::reportUnit(1, 10, 0, 1, "角色甲");
    auto secondAttacker = BattleLogTest::reportUnit(1, 10, 0, 1, "角色乙");
    auto defender = BattleLogTest::reportUnit(2, 20, 1, 2, "敵人");
    BattleReportBuilder firstReport;
    BattleReportBuilder secondReport;
    firstReport.recordDamage(&firstAttacker, &defender, 50, "武學甲", 10, {}, 77);
    secondReport.recordDamage(&secondAttacker, &defender, 50, "武學乙", 10, {}, 77);

    HeadlessBattleResult first;
    first.summary.outcome = BattleOutcome::PlayerVictory;
    first.summary.endFrame = 10;
    first.report = firstReport.report();
    HeadlessBattleResult second = first;
    second.report = secondReport.report();

    CHECK(HeadlessBattleRunner::digest(first) == HeadlessBattleRunner::digest(second));
}
