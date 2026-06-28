#include "BattleScenePauseControl.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleScenePauseControl_AllowsPauseOnlyDuringActiveNonCloseUpBattle", "[battle][pause_control]")
{
    CHECK(battleSceneCanTogglePause(false, -1));
    CHECK_FALSE(battleSceneCanTogglePause(true, -1));
    CHECK_FALSE(battleSceneCanTogglePause(false, 0));
    CHECK_FALSE(battleSceneCanTogglePause(false, 1));
}

TEST_CASE("BattleScenePauseControl_CyclesSpeedHalfNormalDouble", "[battle][pause_control]")
{
    CHECK(nextBattleSpeedSetting(2) == 1);
    CHECK(nextBattleSpeedSetting(1) == 0);
    CHECK(nextBattleSpeedSetting(0) == 2);

    CHECK(battleSpeedDisplayText(2) == "半速");
    CHECK(battleSpeedDisplayText(1) == "常速");
    CHECK(battleSpeedDisplayText(0) == "倍速");
}
