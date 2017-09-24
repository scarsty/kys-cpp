#include "Save.h"
#include <string>
#include <iostream>
#include <fstream>
#include "File.h"
#include "PotConv.h"

Save Save::save_;

void Save::fromCP950ToCP936(char* s)
{
    auto str = PotConv::cp950tocp936(s);
    memcpy(s, str.data(), str.length());
}

Save::Save()
{
}


Save::~Save()
{
}

bool Save::LoadR(int num)
{
    std::string filename, filename_idx;
    int i;
    filename = "../game/save/r" + std::to_string(num)+".grp";
    if (num == 0)
    {
        filename = "../game/save/ranger.grp";
    }
    filename_idx = "../game/save/ranger.idx";
    int* Ridx;
    int len;
    File::readFile(filename_idx.c_str(), (char**)&Ridx, &len);

    offset.resize(len / 4 + 1);
    length.resize(len / 4);
    offset[0] = 0;
    for (int i = 0; i < len / 4; i++)
    {
        offset[i + 1] = Ridx[i];
        length[i] = offset[i + 1] - offset[i];
    }
    int total_length = offset.back();
    delete Ridx;

    auto Rgrp = new char[total_length];
    File::readFile(filename.c_str(), Rgrp, total_length);

    memcpy(&global_data_, Rgrp + offset[0], length[0]);

    int c = 1;
    roles_.resize(length[c]/sizeof(Role));
    memcpy(&roles_[0], Rgrp+offset[c], length[c]);
    for (auto& r : roles_)
    {
        fromCP950ToCP936(r.Name);
        fromCP950ToCP936(r.Nick);
    }

    c = 2;
    items_.resize(length[c] / sizeof(Item));
    memcpy(&items_[0], Rgrp + offset[c], length[c]);
    for (auto& i : items_)
    {
        fromCP950ToCP936(i.Name);
        fromCP950ToCP936(i.Introduction);
    }

    c = 3;
    submap_records_.resize(length[c] / sizeof(SubMapRecord));
    memcpy(&submap_records_[0], Rgrp + offset[c], length[c]);

    c = 4;
    int s = sizeof(Magic);
    magics_.resize(length[c] / sizeof(Magic));
    memcpy(&magics_[0], Rgrp + offset[c], length[c]);

    c = 5;
    shops_.resize(length[c] / sizeof(Shop));
    memcpy(&shops_[0], Rgrp + offset[c], length[c]);

    return true;

//    Offset = new int[Ridxlen / 4 + 1];
//    Offset = new int[Ridxlen / 4 + 1];
//    Offset[0] = 0;
//    std::fprintf(stderr, "load file %s\n", filename_idx.c_str());
//    File::readFile(filename_idx.c_str(), Ridx, Ridxlen);
//    memcpy(Offset+1, Ridx, Ridxlen);
//
//    int GrpLenth;
//    filename_idx = filename + ".grp";
//    unsigned char* Rgrp;
//	std::fprintf(stderr, "load file %s\n", filename_idx.c_str());
//    File::readFile(filename_idx.c_str(), &Rgrp, &GrpLenth);
//    //jiemi(Rgrp, key, Rgrplen);
////     i = 0;
////     GRPMD5_load = new unsigned char[32];
////     memcpy(GRPMD5_load, Rgrp + Offset[i], Offset[i + 1] - Offset[i]);
//
//    //载入基本数据
//    i = 0;
//    int a = sizeof(GlobalData);
//    int b = (Offset[i + 1] - Offset[i]);
//    B_Count =  1;
//    global_data_.resize(B_Count);
//    //BasicData* pBasicData = &m_BasicData.at(0);
//    memcpy(&global_data_.at(0), Rgrp + Offset[i], Offset[i + 1] - Offset[i]);
//    
//    //载入人物数据
//    i = 1;
//    R_Count = (Offset[i + 1] - Offset[i]) / sizeof(Character);
//    roles_.resize(R_Count);
//    for (int j = 0; j < R_Count; j++){
//        Character* pCharacter = &roles_.at(j);
//        memcpy(pCharacter, Rgrp + Offset[i] + j*sizeof(Character), sizeof(Character));
//    }
//    
//    
//    //载入物品数据
//    i = 2;
//    I_Count = (Offset[i + 1] - Offset[i]) / sizeof(Item);
//    items_.resize(I_Count);
//    for (int j = 0; j < I_Count; j++){
//        Item* pItem = &items_.at(j);
//        memcpy(pItem, Rgrp + Offset[i] + j*sizeof(Item), sizeof(Item));
//    }
//    
//    //载入场景数据
//    i = 3;
//    S_Count = (Offset[i + 1] - Offset[i]) / sizeof(SubMapRecord);
//    submap_records_.resize(S_Count);
//	//memcpy(&m_SceneData.at(0), Rgrp + Offset[i], Offset[i + 1] - Offset[i]);
//     for (int j = 0; j < S_Count; j++)
//     {
//         memcpy(&submap_records_.at(j), Rgrp + Offset[i] + j * sizeof(SubMapRecord), sizeof(SubMapRecord));
//     }
//    
//    //载入武功数据
//    i = 4;
//    M_Count = (Offset[i + 1] - Offset[i]) / sizeof(Magic);
//    magics_.resize(M_Count);
//    for (int j = 0; j < M_Count; j++){
//        Magic* pMagic = &magics_.at(j);
//        memcpy(pMagic, Rgrp + Offset[i] + j*sizeof(Magic), sizeof(Magic));
//    }
//
//    
//    //载入小宝商店数据
//    i = 5;
//    SP_Count = (Offset[i + 1] - Offset[i]) / sizeof(BaoShop);
//    shops_.resize(SP_Count);
//    for (int j = 0; j < SP_Count; j++){
//        BaoShop* pBaoShop = &shops_.at(j);
//        memcpy(pBaoShop, Rgrp + Offset[i] + j*sizeof(BaoShop), sizeof(BaoShop));
//    }
//    
//    // Rgrp.clear();
//    
//    
//     //       GRPMD5_cal = new unsigned char[MD5len];
//     //       strcpy((char *)GRPMD5_cal, (char *)(MD5Encode(Data + MD5len, Rgrplen - MD5len)));
//     //       //对比md5
//     //       memcmp(GRPMD5_cal, Data, MD5len);
//    delete(Rgrp);
//    
//    submap_data_.resize(S_Count);
//    filename = "save/S" + to_string(num);
//    if (num == 0)
//    {
//        filename = "save/allsin";
//    }
//    filename = filename + ".grp";
//    fprintf(stderr, "load file %s\n", filename.c_str());
//    File::readFile(filename.c_str(), (void*)(&submap_data_[0].Data[0][0][0]), S_Count * 64 * 64 * 6 * 2);
//   
//     unsigned char *Data2;
//     m_SceneEventData.resize(S_Count);
//     filename = "save/D" + to_string(num);
//     if (num == 0)
//     {
//		 filename = "save/alldef";
//     }
//     filename = filename + ".grp";
//	 fprintf(stderr, "load file %s\n", filename.c_str());
//	 File::readFile(filename.c_str(), (void*)(&m_SceneEventData[0].Data[0]), S_Count * 400 * 18 * 2);
//
//       //readSD(filename1, Data, (unsigned char *)DData, S_Count *sizeof(TSceneDData));
//    //  cocos2d::Data ios = FileUtils::getInstance()->getDataFromFile(filename);
//    //  if (!ios.isNull())
//    //  {
//    //      int len = ios.getSize();
//    //      int length = (len - 32) / (64 * 64 * 2 * 6);
//    //      Data2 = new unsigned char[len];
//    //      memcpy(Data2, ios.getBytes(), len);
//    //      jiemi(Data2, key, len);
//    //      memcpy(GRPMD5_load, Data2, MD5len);
//    //      SceneEventData* pDData = &m_SceneEventData.at(0);
//    //      memcpy(pDData, Data2 + 32, length * 400 * 18 * 2);
//    //      ios.clear();
//    //      delete(Data2);
//    //  }
//    //  else
//    //  {
//    //      return false;
//    //  }
//    //


}

bool Save::SaveR(int num)
{
    //  BasicData* pBasicData = &m_BasicData.at(0);
    //  Character* pCharacter = &m_Character.at(0);
    //  Item* pItem = &m_Item.at(0);
    //  Magic* pMagic = &m_Magic.at(0);
    //  //Calendar* pCalendar = &Calendar::m_Calendar.at(0);
    //  SceneData* pSceneData = &m_SceneData.at(0);
    //  BaoShop* pBaoShop = &m_Baoshop.at(0);
    //  ZhaoShi* pZhaoShi = &m_ZhaoShi.at(0);
    //  Faction* pFaction = &m_Faction.at(0);
    //  SceneEventData* pDData = &m_SceneEventData.at(0);
    //  SceneMapData* pSData = &m_SceneMapData.at(0);
    //
    //  unsigned char *Data;
    //  string filenames, filename1;
    //  int i;
    //  filenames = "save/R" + to_string(num);
    //  if (num == 0)
    //  {
    //      filenames = "save/Ranger";
    //  }
    //  filename1 = filenames + ".idx";
    //  cocos2d::Data Ridx = FileUtils::getInstance()->getDataFromFile(filename1);
    //
    //  if (!Ridx.isNull())
    //  {
    //      Offset = new int[Ridxlen / 4 + 1];
    //      Offset[0] = 0;
    //      Offset[1] = 32;
    //      memcpy(Offset + 2, Ridx.getBytes(), Ridxlen);
    //      for (i = 2; i < Ridxlen / 4 + 1; i++)
    //      {
    //          Offset[i] += 32;
    //      }
    //      Ridx.clear();
    //  }
    //  else
    //  {
    //      return false;
    //  }
    //
    //  string str1 = "115bd419f58173eeca05594356eb4013";
    //  strcpy(num1.str, str1.c_str());
    //  string filename = "save/Ranger.grp";
    //  ofstream os(filename, ios_base::out | ios_base::binary);
    //  os.write(reinterpret_cast<char *>(&num1), sizeof(num1));
    //  os.seekp(Offset[1], ios::beg);
    //  os.write(reinterpret_cast<char *>(pBasicData), sizeof(BasicData));
    //  os.seekp(Offset[2], ios::beg);
    //  os.write(reinterpret_cast<char *>(pCharacter), sizeof(Character));
    //  os.seekp(Offset[3], ios::beg);
    //  os.write(reinterpret_cast<char *>(pItem), sizeof(Item));
    //  os.seekp(Offset[4], ios::beg);
    //  os.write(reinterpret_cast<char *>(pSceneData), sizeof(SceneData));
    //  os.seekp(Offset[5], ios::beg);
    //  os.write(reinterpret_cast<char *>(pMagic), sizeof(Magic));
    //  os.seekp(Offset[6], ios::beg);
    //  os.write(reinterpret_cast<char *>(pBaoShop), sizeof(BaoShop));
    //  os.seekp(Offset[7], ios::beg);
    // //   os.write(reinterpret_cast<char *>(pCalendar), sizeof(Calendar));
    //  os.seekp(Offset[8], ios::beg);
    //  os.write(reinterpret_cast<char *>(pZhaoShi), sizeof(ZhaoShi));
    //  os.seekp(Offset[9], ios::beg);
    //  os.write(reinterpret_cast<char *>(pFaction), sizeof(Faction));
    //  os.seekp(Offset[10], ios::beg);
    //  os << " ";
    //  os.close();
    //  encryption(filename, key);
    return true;
}


