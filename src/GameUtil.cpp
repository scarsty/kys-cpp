#include "GameUtil.h"
#include "Random.h"
#include "Save.h"
#include "strfunc.h"
#include "fmt1.h"

GameUtil::GameUtil()
{
    loadFile(GameUtil::PATH() + "config/kysmod.ini");
}

GameUtil::~GameUtil()
{
    saveFile(GameUtil::PATH() + "config/kysmod.ini");
}


