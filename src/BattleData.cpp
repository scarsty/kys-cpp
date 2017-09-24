#pragma once

#include <string>
#include <fstream>      // std::ifstream
#include "BattleData.h"
#include "File.h"


using namespace std;

BattleData BattleData::m_bBattle;

BattleData::BattleData()
{
}


BattleData::~BattleData()
{
}

bool BattleData::isLoad()
{
    string fileName;
    fileName = "War.sta";
    char* buffer;
    int len;
    File::readFile(fileName.c_str(), &buffer, &len);
    size_t B_Count = len / sizeof(BattleData);
    m_vcBattleInfo.resize(B_Count);
    memcpy(&m_vcBattleInfo[0], buffer, sizeof(BattleData)*B_Count);
    delete buffer;

    fileName = "warfld.grp";
    File::readFile(fileName.c_str(), &buffer, &len);
    int lenBattleFiled2 = 4096 * 2 * 2;
    B_Count = len / lenBattleFiled2;
    m_vcBattleSceneData.resize(B_Count);
    for (int i = 0; i < B_Count; i++)
    {
        memcpy(&m_vcBattleSceneData[i], buffer + i * lenBattleFiled2, lenBattleFiled2);
    }
    delete buffer;
    return true;
}

bool BattleData::isInitSta(int currentBattle)
{
    return true;
}



