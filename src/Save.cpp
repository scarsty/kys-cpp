#include "Save.h"
#include <string>
#include <iostream>
#include <fstream>      // std::ifstream

const int MD5len = 32;

using namespace std;

Save Save::save;

Save::Save()
{
}


Save::~Save()
{

}

bool Save::LoadR(int num)
{
// 	unsigned char *Data;
// 	string filename, filename1;
// 	int i;
// 	filename = "save/R" + to_string(num);
// 	if (num == 0)
// 	{
// 		filename = "save/Ranger";
// 	}
// 	filename1 = filename + ".idx";
// 	cocos2d::Data Ridx = FileUtils::getInstance()->getDataFromFile(filename1);
// 
// 	if (!Ridx.isNull())
// 	{
// 		Offset = new int[Ridxlen / 4 + 1];
// 		Offset[0] = 0;
// 		Offset[1] = 32;
// 		memcpy(Offset + 2, Ridx.getBytes(), Ridxlen);
// 		for (i = 2; i < Ridxlen / 4 + 1; i++)
// 		{
// 			Offset[i] += 32;
// 		}
// 		Ridx.clear();
// 	}
// 	else
// 	{
// 		return false;
// 	}
// 
// 	filename1 = filename + ".grp";
// 	cocos2d::Data Rgrp = FileUtils::getInstance()->getDataFromFile(filename1);
// 
// 	if (!Rgrp.isNull())
// 	{
// 		Rgrplen = Rgrp.getSize();
// 		//Rgrp.seekg(0, ifstream::beg);
// 		Data = new unsigned char[Rgrplen];
// 		memcpy(Data, Rgrp.getBytes(), Rgrplen);
// 		jiemi(Data, key, Rgrplen);
// 		i = 0;
// 		GRPMD5_load = new unsigned char[32];
// 		memcpy(GRPMD5_load, Data + Offset[i], Offset[i + 1] - Offset[i]);
// 		
// 		//载入基本数据
// 		i = 1;
// 		int a = sizeof(BasicData);
// 		int b = (Offset[i + 1] - Offset[i]);
// 		B_Count =  1;
// 		m_BasicData.resize(B_Count);
// 		BasicData* pBasicData = &m_BasicData.at(0);
// 		memcpy(pBasicData, Data + Offset[i], Offset[i + 1] - Offset[i]);
// 
// 		//载入人物数据
// 		i = 2;
// 		R_Count = (Offset[i + 1] - Offset[i]) / sizeof(Character);
// 		m_Character.resize(R_Count);
// 		for (int j = 0; j < R_Count; j++){
// 			Character* pCharacter = &m_Character.at(j);
// 			memcpy(pCharacter, Data + Offset[i] + j*sizeof(Character), sizeof(Character));
// 		}
// // 		Character* pCharacter = &m_Character.at(0);
// // 		memcpy(pCharacter, Data + Offset[i], Offset[i + 1] - Offset[i]);
// 
// 
// 		//载入物品数据
// 		i = 3;
// 		I_Count = (Offset[i + 1] - Offset[i]) / sizeof(Item);
// 		m_Item.resize(I_Count);
// 		for (int j = 0; j < I_Count; j++){
// 			Item* pItem = &m_Item.at(j);
// 			memcpy(pItem, Data + Offset[i] + j*sizeof(Item), sizeof(Item));
// 		}
// // 		Item* pItem = &m_Item.at(0);
// // 		memcpy(pItem, Data + Offset[i], Offset[i + 1] - Offset[i]);
// 
// 		//载入场景数据
// 		i = 4;
// 		S_Count = (Offset[i + 1] - Offset[i]) / sizeof(SceneData);
// 		m_SceneData.resize(S_Count);
// 		for (int j = 0; j < S_Count; j++){
// 			SceneData* pSceneData = &m_SceneData.at(j);
// 			memcpy(pSceneData, Data + Offset[i] + j*sizeof(SceneData), sizeof(SceneData));
// 		}
// // 		SceneData* pSceneData = &m_SceneData.at(0);
// // 		memcpy(pSceneData, Data + Offset[i], Offset[i + 1] - Offset[i]);
// 
// 		//载入武功数据
// 		i = 5;
// 		M_Count = (Offset[i + 1] - Offset[i]) / sizeof(Magic);
// 		m_Magic.resize(M_Count);
// 		for (int j = 0; j < M_Count; j++){
// 			Magic* pMagic = &m_Magic.at(j);
// 			memcpy(pMagic, Data + Offset[i] + j*sizeof(Magic), sizeof(Magic));
// 		}
// // 		Magic* pMagic = &m_Magic.at(0);
// // 		memcpy(pMagic, Data + Offset[i], Offset[i + 1] - Offset[i]);
// 
// 		//载入小宝商店数据
// 		i = 6;
// 		SP_Count = (Offset[i + 1] - Offset[i]) / sizeof(BaoShop);
// 		m_Baoshop.resize(SP_Count);
// 		for (int j = 0; j < SP_Count; j++){
// 			BaoShop* pBaoShop = &m_Baoshop.at(j);
// 			memcpy(pBaoShop, Data + Offset[i] + j*sizeof(BaoShop), sizeof(BaoShop));
// 		}
// 
// 		//载入
// 		i = 7;
// 		//Calendar::m_Calendar.resize(1);
// 		Calendar* pCalendar = &m_Calendar;
// 		memcpy(pCalendar, Data + Offset[i], Offset[i + 1] - Offset[i]);
// 
// 
// 		i = 8;
// 		Z_Count = (Offset[i + 1] - Offset[i]) / sizeof(ZhaoShi);
// 		m_ZhaoShi.resize(Z_Count);
// 		for (int j = 0; j < Z_Count; j++){
// 			ZhaoShi* pZhaoShi = &m_ZhaoShi.at(j);
// 			memcpy(pZhaoShi, Data + Offset[i] + j*sizeof(ZhaoShi), sizeof(ZhaoShi));
// 		}
// // 		ZhaoShi* pZhaoShi = &m_ZhaoShi.at(0);
// // 		memcpy(pZhaoShi, Data + Offset[i], Offset[i + 1] - Offset[i]);
// 
// 		i = 9;
// 		M_Count = (Offset[i + 1] - Offset[i]) / sizeof(Faction);
//  		m_Faction.resize(M_Count);
//  		for (int j = 0; j < M_Count; j++){
//  			Faction* pFaction = &m_Faction.at(j);
//  			memcpy(pFaction, Data + Offset[i] + j*sizeof(Faction), sizeof(Faction));
//  		}
// 		Rgrp.clear();
// 
// 
// //		GRPMD5_cal = new unsigned char[MD5len];
// // 		strcpy((char *)GRPMD5_cal, (char *)(MD5Encode(Data + MD5len, Rgrplen - MD5len)));
// // 		//对比md5
// // 		memcmp(GRPMD5_cal, Data, MD5len);
// 		delete(Data);
// 	}
// 	else
// 	{
// 		return  false;
// 	}
// 
// 	unsigned char *Data1;
// 	m_SceneMapData.resize(S_Count);
// 	filename = "save/S" + to_string(num);
// 	if (num == 0)
// 	{
// 		filename = "save/allsin";
// 	}
// 	filename = filename + ".grp";
// 	cocos2d::Data ifs = FileUtils::getInstance()->getDataFromFile(filename);
// 	if (!ifs.isNull())
// 	{
// 		int len = ifs.getSize();
// 		int length = (len - 32) / (64 * 64 * 2 * 6);
// 		Data1 = new unsigned char[len];
// 		memcpy(Data1, ifs.getBytes(), len);
// 		jiemi(Data1, key, len);
// 		memcpy(GRPMD5_load, Data1, MD5len);
// 		SceneMapData* pSData = &m_SceneMapData.at(0);
// 		memcpy(pSData, Data1 + 32, length * 64 * 64 * 6 * 2);
// 		ifs.clear();
// 		delete(Data1);
// 	}
// 	else
// 	{
// 		return false;
// 	}
// 
// 
// 
// 	unsigned char *Data2;
// 	m_SceneEventData.resize(S_Count);
// 	filename = "save/D" + to_string(num);
// 	if (num == 0)
// 	{
// 		filename = "save/alldef";
// 	}
// 	filename = filename + ".grp";
// 	//readSD(filename1, Data, (unsigned char *)DData, S_Count *sizeof(TSceneDData));
// 	cocos2d::Data ios = FileUtils::getInstance()->getDataFromFile(filename);
// 	if (!ios.isNull())
// 	{
// 		int len = ios.getSize();
// 		int length = (len - 32) / (64 * 64 * 2 * 6);
// 		Data2 = new unsigned char[len];
// 		memcpy(Data2, ios.getBytes(), len);
// 		jiemi(Data2, key, len);
// 		memcpy(GRPMD5_load, Data2, MD5len);
// 		SceneEventData* pDData = &m_SceneEventData.at(0);
// 		memcpy(pDData, Data2 + 32, length * 400 * 18 * 2);
// 		ios.clear();
// 		delete(Data2);
// 	}
// 	else
// 	{
// 		return false;
// 	}
// 
// 	return  true;
}


void Save::jiemi(unsigned char *Data, unsigned char key, int len)
{
	int i;
	unsigned char password;
	for (i = 0; i < len; i++)
	{
		password = key << (i % 5);
		*(Data + i) ^= password;
	}

}

bool Save::SaveR(int num)
{
// 	BasicData* pBasicData = &m_BasicData.at(0);
// 	Character* pCharacter = &m_Character.at(0);
// 	Item* pItem = &m_Item.at(0);
// 	Magic* pMagic = &m_Magic.at(0);
// 	//Calendar* pCalendar = &Calendar::m_Calendar.at(0);
// 	SceneData* pSceneData = &m_SceneData.at(0);
// 	BaoShop* pBaoShop = &m_Baoshop.at(0);
// 	ZhaoShi* pZhaoShi = &m_ZhaoShi.at(0);
// 	Faction* pFaction = &m_Faction.at(0);
// 	SceneEventData* pDData = &m_SceneEventData.at(0);
// 	SceneMapData* pSData = &m_SceneMapData.at(0);
// 
// 	unsigned char *Data;
// 	string filenames, filename1;
// 	int i;
// 	filenames = "save/R" + to_string(num);
// 	if (num == 0)
// 	{
// 		filenames = "save/Ranger";
// 	}
// 	filename1 = filenames + ".idx";
// 	cocos2d::Data Ridx = FileUtils::getInstance()->getDataFromFile(filename1);
// 
// 	if (!Ridx.isNull())
// 	{
// 		Offset = new int[Ridxlen / 4 + 1];
// 		Offset[0] = 0;
// 		Offset[1] = 32;
// 		memcpy(Offset + 2, Ridx.getBytes(), Ridxlen);
// 		for (i = 2; i < Ridxlen / 4 + 1; i++)
// 		{
// 			Offset[i] += 32;
// 		}
// 		Ridx.clear();
// 	}
// 	else
// 	{
// 		return false;
// 	}
// 
// 	string str1 = "115bd419f58173eeca05594356eb4013";
// 	strcpy(num1.str, str1.c_str());
// 	string filename = "save/Ranger.grp";
// 	ofstream os(filename, ios_base::out | ios_base::binary);
// 	os.write(reinterpret_cast<char *>(&num1), sizeof(num1));
// 	os.seekp(Offset[1], ios::beg);
// 	os.write(reinterpret_cast<char *>(pBasicData), sizeof(BasicData));
// 	os.seekp(Offset[2], ios::beg);
// 	os.write(reinterpret_cast<char *>(pCharacter), sizeof(Character));
// 	os.seekp(Offset[3], ios::beg);
// 	os.write(reinterpret_cast<char *>(pItem), sizeof(Item));
// 	os.seekp(Offset[4], ios::beg);
// 	os.write(reinterpret_cast<char *>(pSceneData), sizeof(SceneData));
// 	os.seekp(Offset[5], ios::beg);
// 	os.write(reinterpret_cast<char *>(pMagic), sizeof(Magic));
// 	os.seekp(Offset[6], ios::beg);
// 	os.write(reinterpret_cast<char *>(pBaoShop), sizeof(BaoShop));
// 	os.seekp(Offset[7], ios::beg);
// //	os.write(reinterpret_cast<char *>(pCalendar), sizeof(Calendar));
// 	os.seekp(Offset[8], ios::beg);
// 	os.write(reinterpret_cast<char *>(pZhaoShi), sizeof(ZhaoShi));
// 	os.seekp(Offset[9], ios::beg);
// 	os.write(reinterpret_cast<char *>(pFaction), sizeof(Faction));
// 	os.seekp(Offset[10], ios::beg);
// 	os << " ";
// 	os.close();
// 	encryption(filename, key);
// 	return true;
}

void Save::encryption(string str, unsigned char key)
{
// 	int i;
// 	unsigned char *Data;
// 	unsigned char password;
// 	cocos2d::Data Rgrp = FileUtils::getInstance()->getDataFromFile(str);
// 	if (!Rgrp.isNull())
// 	{
// 		auto Rgrplen = Rgrp.getSize();
// 		Data = new unsigned char[Rgrplen];
// 		memcpy(Data, Rgrp.getBytes(), Rgrplen);
// 		for (i = 0; i < Rgrplen; i++)
// 		{
// 			password = key << (i % 5);
// 			*(Data + i) ^= password;
// 		}
// 	}
// 	string filename = "save/Ranger.grp";
// 	ofstream os(filename, ios_base::out | ios_base::binary);
// 	os.seekp(ios::beg);
// 	os << Data;
// 	os.close();
}

