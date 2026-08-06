#include "GameUtil.h"
#include "strfunc.h"

GameUtil::GameUtil()
{
    loadFile(GameUtil::PATH() + "config/kysmod.ini");
}

GameUtil::~GameUtil()
{
}

void GameUtil::saveConfig()
{
    saveFile(GameUtil::PATH() + "config/kysmod.ini");
}

void GameUtil::selectAndroidGamePath()
{
    const std::string root = "/sdcard/kys-cpp/";
    const std::string default_path = root + "game/";
    const std::string games_ini = root + "games.ini";
    if (!filefunc::fileExist(games_ini))
    {
        PATH() = default_path;
        return;
    }

    INIReaderNormal ini;
    if (ini.loadFile(games_ini) != 0)
    {
        PATH() = default_path;
        return;
    }
    int index = ini.getInt("games", "current", 0);
    std::string entry = ini.getString("games", std::to_string(index), "");
    size_t separator = entry.find_last_of(':');
    if (separator == std::string::npos)
    {
        PATH() = default_path;
        return;
    }

    std::string directory = strfunc::trim(entry.substr(separator + 1));
    if (directory.empty() || directory == "." || directory == ".."
        || directory.find_first_of("/\\") != std::string::npos)
    {
        PATH() = default_path;
        return;
    }

    std::string path = root + directory + "/";
    PATH() = filefunc::fileExist(path + "config/kysmod.ini") ? path : default_path;
}
