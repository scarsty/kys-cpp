#pragma once

#include <functional>
#include <string>

namespace ScenePreloader
{

void preloadSubSceneAssets(int submapId);
void preloadBattlefieldAssets(int battlefieldId);
void showPromptAndPreload(const std::string& message, const std::function<void()>& preload);

}
