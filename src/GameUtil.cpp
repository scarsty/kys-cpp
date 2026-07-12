#include "GameUtil.h"

#include "GameVersion.h"

GameUtil::GameUtil()
{
    loadFile(GameUtil::PATH() + "config/kysmod.ini");
    versionStorage() = KysChess::loadGameVersion(GameUtil::PATH());
}

GameUtil::~GameUtil()
{
    saveFile(GameUtil::PATH() + "config/kysmod.ini");
}
