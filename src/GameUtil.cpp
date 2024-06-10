#include "GameUtil.h"

GameUtil::GameUtil()
{
    loadFile(GameUtil::PATH() + "config/kysmod.ini");
}

GameUtil::~GameUtil()
{
    saveFile(GameUtil::PATH() + "config/kysmod.ini");
}
