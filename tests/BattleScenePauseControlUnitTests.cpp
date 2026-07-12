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

TEST_CASE("BattleScenePauseControl_PositionSwapStartsPaperBattleInClassicView", "[battle][pause_control]")
{
    CHECK(battleSceneInitialPaperView(true, false));
    CHECK_FALSE(battleSceneInitialPaperView(true, true));
    CHECK_FALSE(battleSceneInitialPaperView(false, false));
    CHECK_FALSE(battleSceneInitialPaperView(false, true));
}

TEST_CASE("BattleScenePauseControl_DisplaysPaperViewToggleState", "[battle][pause_control]")
{
    CHECK(battlePaperViewDisplayText(false) == "2D");
    CHECK(battlePaperViewDisplayText(true) == "3D");
}

TEST_CASE("BattleScenePauseControl_DisplaysPaperCameraModeState", "[battle][pause_control]")
{
    CHECK(battlePaperCameraModeDisplayText(false) == "自由");
    CHECK(battlePaperCameraModeDisplayText(true) == "跟隨");
}

TEST_CASE("BattleScenePauseControl_PaperCameraStartsFreeAfterInitialSnap", "[battle][pause_control]")
{
    CHECK_FALSE(battlePaperCameraAutoCenterAfterEntry());
}

TEST_CASE("BattleScenePauseControl_UsesClassicLikePaperCameraDefaults", "[battle][pause_control]")
{
    const auto defaults = battleScenePaperCameraDefaults();
    CHECK(defaults.distance == 440.0f);
    CHECK(defaults.height == 337.62387471f);
}
