#pragma once

#include <string_view>

struct BattleScenePaperCameraDefaults
{
    float distance{};
    float height{};
};

bool battleSceneCanTogglePause(bool closeUpActive, int battleResult);
int nextBattleSpeedSetting(int currentBattleSpeed);
std::string_view battleSpeedDisplayText(int battleSpeed);
bool battleSceneInitialPaperView(bool paperBattleViewSetting, bool positionSwapEnabled);
std::string_view battlePaperViewDisplayText(bool paperView);
std::string_view battlePaperCameraModeDisplayText(bool autoCenter);
bool battlePaperCameraAutoCenterAfterEntry();
BattleScenePaperCameraDefaults battleScenePaperCameraDefaults();
