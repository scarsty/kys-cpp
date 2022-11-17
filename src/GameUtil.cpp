#include "GameUtil.h"
#include "Random.h"
#include "Save.h"
#include "fmt1.h"
#include "strfunc.h"

GameUtil::GameUtil()
{
    loadFile(GameUtil::PATH() + "config/kysmod.ini");
}

GameUtil::~GameUtil()
{
    saveFile(GameUtil::PATH() + "config/kysmod.ini");
}
