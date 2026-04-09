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
    releaseConfig.loadFile(GameUtil::PATH() + "config/release.ini");
    return releaseConfig.getString("release", "version", "");
}
