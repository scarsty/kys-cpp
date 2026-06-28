#pragma once

#include <string_view>

bool battleSceneCanTogglePause(bool closeUpActive, int battleResult);
int nextBattleSpeedSetting(int currentBattleSpeed);
std::string_view battleSpeedDisplayText(int battleSpeed);
