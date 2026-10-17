#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ScenePreloader
{

void preloadSubSceneAssets(int submapId);
std::vector<std::pair<std::string, int>> battlefieldAssets(int battlefieldId);
void showPromptAndPreload(const std::string& message, const std::function<void()>& preload);

}
