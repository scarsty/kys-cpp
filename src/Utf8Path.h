#pragma once

#include <filesystem>
#include <string>

namespace KysChess
{

inline std::string pathToUtf8(const std::filesystem::path& path)
{
    const auto text = path.generic_u8string();
    return {
        reinterpret_cast<const char*>(text.data()),
        text.size(),
    };
}

}
