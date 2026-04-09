#include "GameUtil.h"

GameUtil::GameUtil()
{
    loadFile(GameUtil::PATH() + "config/kysmod.ini");
    versionStorage() = loadReleaseVersion();
}

GameUtil::~GameUtil()
{
    saveFile(GameUtil::PATH() + "config/kysmod.ini");
}

std::string GameUtil::loadReleaseVersion()
{
    INIReaderNormal releaseConfig;
    if (releaseConfig.loadFile(GameUtil::PATH() + "config/release.ini") != 0)
    {
        return "dev";
    }

    auto version = releaseConfig.getString("release", "version", "");
    if (version.empty())
    {
        return "dev";
    }
    return version;
}
