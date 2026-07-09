#include "BattleScenePauseControl.h"

#include <cassert>

bool battleSceneCanTogglePause(bool closeUpActive, int battleResult)
{
    return !closeUpActive && battleResult < 0;
}

int nextBattleSpeedSetting(int currentBattleSpeed)
{
    switch (currentBattleSpeed)
    {
    case 2:
        return 1;
    case 1:
        return 0;
    case 0:
        return 2;
    }
    assert(false);
    return 1;
}

std::string_view battleSpeedDisplayText(int battleSpeed)
{
    switch (battleSpeed)
    {
    case 2:
        return "半速";
    case 1:
        return "常速";
    case 0:
        return "倍速";
    }
    assert(false);
    return "常速";
}

bool battleSceneInitialPaperView(bool paperBattleViewSetting, bool positionSwapEnabled)
{
    return paperBattleViewSetting && !positionSwapEnabled;
}

std::string_view battlePaperViewDisplayText(bool paperView)
{
    return paperView ? "3D" : "2D";
}

std::string_view battlePaperCameraModeDisplayText(bool autoCenter)
{
    return autoCenter ? "跟隨" : "自由";
}

bool battlePaperCameraAutoCenterAfterEntry()
{
    return false;
}

BattleScenePaperCameraDefaults battleScenePaperCameraDefaults()
{
    return { 580.0f, 240.0f };
}
