#pragma once

#include <string>
#include <fstream>      // std::ifstream
#include "BattleData.h"
#include "Common.h"
#include "File.h"


using namespace std;

BattleData BattleData::bBattle;

BattleData::BattleData()
{
}


BattleData::~BattleData()
{
}

bool BattleData::load()
{
    string fileName;
    fileName = "War.sta";
    unsigned char* buffer;
    int len;
    File::readFile(fileName.c_str(), &buffer, &len);
    size_t B_Count = len / sizeof(BattleData);
    m_BattleData.resize(B_Count);
    memcpy(&m_BattleData[0], buffer, sizeof(BattleData)*B_Count);
    delete buffer;

    fileName = "warfld.grp";
    File::readFile(fileName.c_str(), &buffer, &len);
    B_Count = len / (sizeof(BattleSceneData) / 4);
    m_BattleSceneData.resize(B_Count);
    memcpy(&m_BattleSceneData[0], buffer, sizeof(BattleSceneData)*B_Count);
    delete buffer;
}

bool BattleData::initSta(int currentBattle)
{
    return true;
}



