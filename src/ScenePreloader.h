#pragma once

#include <functional>
#include <string>

namespace ScenePreloader
{

void preloadSubSceneAssets(int submapId);
void preloadBattleAssets(int battleId);
void showPromptAndPreload(const std::string& message, const std::function<void()>& preload);

}
