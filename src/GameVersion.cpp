#include "GameVersion.h"

#include <fstream>
#include <string_view>

namespace KysChess
{
namespace
{

std::string_view trim(std::string_view text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
    {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

}

std::string loadGameVersion(const std::filesystem::path& dataRoot)
{
    std::ifstream input(dataRoot / "config" / "release.ini", std::ios::binary);
    std::string line;
    bool releaseSection = false;
    while (std::getline(input, line))
    {
        const auto value = trim(line);
        if (value.empty() || value.starts_with(';') || value.starts_with('#'))
        {
            continue;
        }
        if (value.front() == '[' && value.back() == ']')
        {
            releaseSection = trim(value.substr(1, value.size() - 2)) == "release";
            continue;
        }
        if (!releaseSection)
        {
            continue;
        }
        const auto separator = value.find('=');
        if (separator == std::string_view::npos || trim(value.substr(0, separator)) != "version")
        {
            continue;
        }
        const auto version = trim(value.substr(separator + 1));
        return version.empty() ? "dev" : std::string(version);
    }
    return "dev";
}

}
