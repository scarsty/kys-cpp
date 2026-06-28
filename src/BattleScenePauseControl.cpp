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
