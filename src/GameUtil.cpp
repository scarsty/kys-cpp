#include "GameUtil.h"

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
