#pragma once

#include <string>
#include <fstream>      // std::ifstream
#include "battle.h"
#include "Common.h"


using namespace std;

battle battle::bBattle;

battle::battle()
{
	//initData();
}


battle::~battle()
{
}

bool battle::loadSta()
{
	string fileName;
	fileName = "War.sta";
	cocos2d::Data warData = FileUtils::getInstance()->getDataFromFile(fileName);

	if (!warData.isNull())
	{
		int i = sizeof(TWarSta);
		size_t B_Count = warData.getSize() / i;
		//float B_Count1 = warData.getSize() / i;
		warSta.resize(B_Count);
		for (int j = 0; j < B_Count; j++){
			TWarSta* pWData = &warSta[j];
			memcpy(pWData, warData.getBytes() + i*j, i);
		}
		warData.clear();
	}
	else
	{
		return false;
	}

	fileName = "warfld.grp";
	cocos2d::Data warSData = FileUtils::getInstance()->getDataFromFile(fileName);

	if (!warSData.isNull())
	{
		int i = sizeof(SceneBData)/4;
		size_t B_Count = warSData.getSize() / i;
		//bData = new Data[B_Count];
		bData.resize(B_Count);
		for (int j = 0; j < B_Count; j++){
			SceneBData* pData = &bData[j];
			memcpy(pData, warSData.getBytes() + i*j, i);
		}
		warSData.clear();
		return true;
	}
	else
	{
		return false;
	}

}
bool battle::initSta(int currentBattle)
{
	return true;
}



