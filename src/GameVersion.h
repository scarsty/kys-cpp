#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace KysChess
{

std::string loadGameVersion(const std::filesystem::path& dataRoot);
bool chessGameVersionsCompatible(std::string_view savedVersion, std::string_view currentVersion);

}
