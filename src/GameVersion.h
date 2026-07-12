#pragma once

#include <filesystem>
#include <string>

namespace KysChess
{

std::string loadGameVersion(const std::filesystem::path& dataRoot);

}
