#include "Save.h"
#include "File.h"
#include "GrpIdxFile.h"
#include "PotConv.h"
#include "csv.h"
#include "libconvert.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

Save::Save()
{
}

Save::~Save()
{
}

std::string Save::getFilename(int i, char c)
{
    std::string filename;
    if (i > 0)
    {
        filename = convert::formatString("../game/save/%c%d.grp", c, i);
        if (c == 'r')
        {
            filename += "32";
        }
    }
    else
    {
        if (c == 'r')
        {
            filename = "../game/save/ranger.grp32";
        }
        else if (c == 's')
        {
            filename = "../game/save/allsin.grp";
        }
        else if (c == 'd')
        {
            filename = "../game/save/alldef.grp";
        }
    }
    return filename;
}

bool Save::checkSaveFileExist(int num)
{
    return File::fileExist(getFilename(num, 'r'))
        && File::fileExist(getFilename(num, 's'))
        && File::fileExist(getFilename(num, 'd'));
}

void Save::updateAllPtrVector()
{
    toPtrVector(roles_mem_, roles_);
    toPtrVector(items_mem_, items_);
    toPtrVector(submap_infos_mem_, submap_infos_);
    toPtrVector(magics_mem_, magics_);
    toPtrVector(shops_mem_, shops_);
}

bool Save::load(int num)
{
    if (!checkSaveFileExist(num))
    {
        return false;
    }

    loadR(num);
    loadSD(num);

    //内部编码为cp936
    if (Encode != 936)
    {
        Encode = 936;
        for (auto i : roles_)
        {
            PotConv::fromCP950ToCP936(i->Name);
            PotConv::fromCP950ToCP936(i->Nick);
        }
        for (auto i : items_)
        {
            PotConv::fromCP950ToCP936(i->Name);
            PotConv::fromCP950ToCP936(i->Introduction);
        }
        for (auto i : magics_)
        {
            PotConv::fromCP950ToCP936(i->Name);
        }
        for (auto i : submap_infos_)
        {
            PotConv::fromCP950ToCP936(i->Name);
        }
    }

    makeMaps();

    return true;
}

void Save::loadR(int num)
{
    std::string filenamer = getFilename(num, 'r');
    std::string filename_idx = "../game/save/ranger.idx32";
    auto rgrp = GrpIdxFile::getIdxContent(filename_idx, filenamer, &offset_, &length_);

    memcpy(&InShip, rgrp + offset_[0], length_[0]);
    File::readDataToVector(rgrp + offset_[1], length_[1], roles_mem_, sizeof(RoleSave));
    File::readDataToVector(rgrp + offset_[2], length_[2], items_mem_, sizeof(ItemSave));
    File::readDataToVector(rgrp + offset_[3], length_[3], submap_infos_mem_, sizeof(SubMapInfoSave));
    File::readDataToVector(rgrp + offset_[4], length_[4], magics_mem_, sizeof(MagicSave));
    File::readDataToVector(rgrp + offset_[5], length_[5], shops_mem_, sizeof(ShopSave));

    updateAllPtrVector();

    delete[] rgrp;
}

void Save::loadSD(int num)
{
    std::string filenames = getFilename(num, 's');
    std::string filenamed = getFilename(num, 'd');

    auto submap_count = submap_infos_.size();

    auto sdata = new char[submap_count * sdata_length_];
    auto ddata = new char[submap_count * ddata_length_];
    File::readFile(filenames, sdata, submap_count * sdata_length_);
    File::readFile(filenamed, ddata, submap_count * ddata_length_);
    for (int i = 0; i < submap_count; i++)
    {
        memcpy(&(submap_infos_mem_[i].LayerData(0, 0, 0)), sdata + sdata_length_ * i, sdata_length_);
        memcpy(submap_infos_mem_[i].Event(0), ddata + ddata_length_ * i, ddata_length_);
    }
    delete[] sdata;
    delete[] ddata;

}

bool Save::save(int num)
{
    saveR(num);
    saveSD(num);
    return true;
}

void Save::saveR(int num)
{
    std::string filenamer = getFilename(num, 'r');

    char* rgrp = new char[offset_.back()];
    memcpy(rgrp + offset_[0], &InShip, length_[0]);
    File::writeVectorToData(rgrp + offset_[1], length_[1], roles_mem_, sizeof(RoleSave));
    File::writeVectorToData(rgrp + offset_[2], length_[2], items_mem_, sizeof(ItemSave));
    File::writeVectorToData(rgrp + offset_[3], length_[3], submap_infos_mem_, sizeof(SubMapInfoSave));
    File::writeVectorToData(rgrp + offset_[4], length_[4], magics_mem_, sizeof(MagicSave));
    File::writeVectorToData(rgrp + offset_[5], length_[5], shops_mem_, sizeof(ShopSave));

    File::writeFile(filenamer, rgrp, offset_.back());
    delete[] rgrp;
}

void Save::saveSD(int num)
{
    std::string filenames = getFilename(num, 's');
    std::string filenamed = getFilename(num, 'd');
    auto submap_count = submap_infos_mem_.size();

    auto sdata = new char[submap_count * sdata_length_];
    auto ddata = new char[submap_count * ddata_length_];
    for (int i = 0; i < submap_count; i++)
    {
        memcpy(sdata + sdata_length_ * i, &(submap_infos_mem_[i].LayerData(0, 0, 0)), sdata_length_);
        memcpy(ddata + ddata_length_ * i, submap_infos_mem_[i].Event(0), ddata_length_);
    }
    File::writeFile(filenames, sdata, submap_count * sdata_length_);
    File::writeFile(filenamed, ddata, submap_count * ddata_length_);
    delete[] sdata;
    delete[] ddata;
}

void Save::resetRData(const std::vector<RoleSave>& newData)
{
    roles_mem_.clear();
    for (int i = 0; i < newData.size(); i++) {
        roles_mem_.emplace_back();
        auto& r = roles_mem_.back();
        memcpy(&r, &newData[i], sizeof(RoleSave));
        r.RealID = r.ID;
        r.ID = i;
    }
    updateAllPtrVector();
}

Role* Save::getTeamMate(int i)
{
    if (i < 0 || i >= TEAMMATE_COUNT)
    {
        return nullptr;
    }
    int r = Team[i];
    if (r < 0 || r >= roles_mem_.size())
    {
        return nullptr;
    }
    return roles_[r];
}

Item* Save::getItemByBagIndex(int i)
{
    if (i < 0 || i >= ITEM_IN_BAG_COUNT)
    {
        return nullptr;
    }
    int r = Items[i].item_id;
    if (r < 0 || r >= items_mem_.size())
    {
        return nullptr;
    }
    return items_[r];
}

int Save::getItemCountByBagIndex(int i)
{
    return Items[i].count;
}

int Save::getItemCountInBag(Item* item)
{
    return getItemCountInBag(item->ID);
}

int Save::getItemCountInBag(int item_id)
{
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        auto id = Items[i].item_id;
        if (id < 0)
        {
            break;
        }
        if (id == item_id)
        {
            return Items[i].count;
        }
    }
    return 0;
}

int Save::getMoneyCountInBag()
{
    return getItemCountInBag(Item::MoneyItemID);
}

void Save::makeMaps()
{
    roles_by_name_.clear();
    magics_by_name_.clear();
    items_by_name_.clear();
    submap_infos_by_name_.clear();

    //有重名的，斟酌使用
    for (auto& i : roles_)
    {
        roles_by_name_[i->Name] = i;
    }
    for (auto& i : magics_)
    {
        magics_by_name_[i->Name] = i;
    }
    for (auto& i : items_)
    {
        items_by_name_[i->Name] = i;
    }
    for (auto& i : submap_infos_)
    {
        submap_infos_by_name_[i->Name] = i;
    }
}

Magic* Save::getRoleLearnedMagic(Role* r, int i)
{
    if (i < 0 || i >= ROLE_MAGIC_COUNT)
    {
        return nullptr;
    }
    return getMagic(r->MagicID[i]);
}

int Save::getRoleLearnedMagicLevelIndex(Role* r, Magic* m)
{
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        if (r->MagicID[i] == m->ID)
        {
            return r->getRoleMagicLevelIndex(i);
        }
    }
    return -1;
}

void Save::saveRToCSV(int num)
{
    NewSave::SaveToCSVBaseInfo((BaseInfo*)this, 1, num);
    // 背包
    NewSave::SaveToCSVItemList(Items, ITEM_IN_BAG_COUNT, num);
    // 人物
    NewSave::SaveToCSVRoleSave(roles_mem_, num);
    // 物品
    NewSave::SaveToCSVItemSave(items_mem_, num);
    // 场景
    NewSave::SaveToCSVSubMapInfoSave(submap_infos_mem_, num);
    // 武功
    NewSave::SaveToCSVMagicSave(magics_mem_, num);
    // 商店
    NewSave::SaveToCSVShopSave(shops_mem_, num);
}

void Save::loadRFromCSV(int num)
{
    NewSave::LoadFromCSVBaseInfo((BaseInfo*)this, 1, num);
    NewSave::LoadFromCSVItemList(Items, ITEM_IN_BAG_COUNT, num);
    NewSave::LoadFromCSVRoleSave(roles_mem_, num);
    NewSave::LoadFromCSVItemSave(items_mem_, num);
    NewSave::LoadFromCSVSubMapInfoSave(submap_infos_mem_, num);
    NewSave::LoadFromCSVMagicSave(magics_mem_, num);
    NewSave::LoadFromCSVShopSave(shops_mem_, num);
    updateAllPtrVector();
    makeMaps();
}

bool Save::insertAt(const std::string& type, int idx)
{
    if (type == u8"Role")
    {
        NewSave::InsertRoleAt(roles_mem_, idx);
        return true;
    }
    else if (type == u8"Item")
    {
        NewSave::InsertItemAt(items_mem_, idx);
        return true;
    }
    else if (type == u8"Magic")
    {
        NewSave::InsertMagicAt(magics_mem_, idx);
        return true;
    }
    else if (type == u8"SubMapInfo")
    {
        NewSave::InsertSubMapInfoAt(submap_infos_mem_, idx);
        return true;
    }
    else if (type == u8"Shop")
    {
        NewSave::InsertShopAt(shops_mem_, idx);
        return true;
    }
    return false;
}

// 基本
void Save::NewSave::SaveToCSVBaseInfo(BaseInfo* data, int length, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_基本.csv");
    fout << "乘船";
    fout << ",";
    fout << "子场景内";
    fout << ",";
    fout << "主地图X";
    fout << ",";
    fout << "主地图Y";
    fout << ",";
    fout << "子场景X";
    fout << ",";
    fout << "子场景Y";
    fout << ",";
    fout << "面朝方向";
    fout << ",";
    fout << "船X";
    fout << ",";
    fout << "船Y";
    fout << ",";
    fout << "船X1";
    fout << ",";
    fout << "船Y1";
    fout << ",";
    fout << "内部编码";
    fout << ",";
    fout << "队友1";
    fout << ",";
    fout << "队友2";
    fout << ",";
    fout << "队友3";
    fout << ",";
    fout << "队友4";
    fout << ",";
    fout << "队友5";
    fout << ",";
    fout << "队友6";
    fout << std::endl;
    for (int i = 0; i < length; i++)
    {
        fout << data[i].InShip;
        fout << ",";
        fout << data[i].InSubMap;
        fout << ",";
        fout << data[i].MainMapX;
        fout << ",";
        fout << data[i].MainMapY;
        fout << ",";
        fout << data[i].SubMapX;
        fout << ",";
        fout << data[i].SubMapY;
        fout << ",";
        fout << data[i].FaceTowards;
        fout << ",";
        fout << data[i].ShipX;
        fout << ",";
        fout << data[i].ShipY;
        fout << ",";
        fout << data[i].ShipX1;
        fout << ",";
        fout << data[i].ShipY1;
        fout << ",";
        fout << data[i].Encode;
        fout << ",";
        for (int j = 0; j < 6; j++)
        {
            fout << data[i].Team[j];
            if (j != 6 - 1)
            { fout << ","; }
        }
        fout << std::endl;
    }
}
// 背包
void Save::NewSave::SaveToCSVItemList(ItemList* data, int length, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_背包.csv");
    fout << "物品编号";
    fout << ",";
    fout << "物品数量";
    fout << std::endl;
    for (int i = 0; i < length; i++)
    {
        fout << data[i].item_id;
        fout << ",";
        fout << data[i].count;
        fout << std::endl;
    }
}
// 人物
void Save::NewSave::SaveToCSVRoleSave(const std::vector<Role>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_人物.csv");
    fout << "编号";
    fout << ",";
    fout << "头像";
    fout << ",";
    fout << "生命成长";
    fout << ",";
    fout << "无用";
    fout << ",";
    fout << "名字";
    fout << ",";
    fout << "外号";
    fout << ",";
    fout << "性别";
    fout << ",";
    fout << "等级";
    fout << ",";
    fout << "经验";
    fout << ",";
    fout << "生命";
    fout << ",";
    fout << "生命最大值";
    fout << ",";
    fout << "内伤";
    fout << ",";
    fout << "中毒";
    fout << ",";
    fout << "体力";
    fout << ",";
    fout << "物品修炼点数";
    fout << ",";
    fout << "武器";
    fout << ",";
    fout << "防具";
    fout << ",";
    fout << "动作帧数1";
    fout << ",";
    fout << "动作帧数2";
    fout << ",";
    fout << "动作帧数3";
    fout << ",";
    fout << "动作帧数4";
    fout << ",";
    fout << "动作帧数5";
    fout << ",";
    fout << "动作帧数6";
    fout << ",";
    fout << "动作帧数7";
    fout << ",";
    fout << "动作帧数8";
    fout << ",";
    fout << "动作帧数9";
    fout << ",";
    fout << "动作帧数10";
    fout << ",";
    fout << "动作帧数11";
    fout << ",";
    fout << "动作帧数12";
    fout << ",";
    fout << "动作帧数13";
    fout << ",";
    fout << "动作帧数14";
    fout << ",";
    fout << "动作帧数15";
    fout << ",";
    fout << "内力性质";
    fout << ",";
    fout << "内力";
    fout << ",";
    fout << "内力最大值";
    fout << ",";
    fout << "攻击力";
    fout << ",";
    fout << "轻功";
    fout << ",";
    fout << "防御力";
    fout << ",";
    fout << "医疗";
    fout << ",";
    fout << "用毒";
    fout << ",";
    fout << "解毒";
    fout << ",";
    fout << "抗毒";
    fout << ",";
    fout << "拳掌";
    fout << ",";
    fout << "御剑";
    fout << ",";
    fout << "耍刀";
    fout << ",";
    fout << "特殊";
    fout << ",";
    fout << "暗器";
    fout << ",";
    fout << "武学常识";
    fout << ",";
    fout << "品德";
    fout << ",";
    fout << "攻击带毒";
    fout << ",";
    fout << "左右互搏";
    fout << ",";
    fout << "声望";
    fout << ",";
    fout << "资质";
    fout << ",";
    fout << "修炼物品";
    fout << ",";
    fout << "修炼点数";
    fout << ",";
    fout << "所会武功1";
    fout << ",";
    fout << "所会武功2";
    fout << ",";
    fout << "所会武功3";
    fout << ",";
    fout << "所会武功4";
    fout << ",";
    fout << "所会武功5";
    fout << ",";
    fout << "所会武功6";
    fout << ",";
    fout << "所会武功7";
    fout << ",";
    fout << "所会武功8";
    fout << ",";
    fout << "所会武功9";
    fout << ",";
    fout << "所会武功10";
    fout << ",";
    fout << "武功等级1";
    fout << ",";
    fout << "武功等级2";
    fout << ",";
    fout << "武功等级3";
    fout << ",";
    fout << "武功等级4";
    fout << ",";
    fout << "武功等级5";
    fout << ",";
    fout << "武功等级6";
    fout << ",";
    fout << "武功等级7";
    fout << ",";
    fout << "武功等级8";
    fout << ",";
    fout << "武功等级9";
    fout << ",";
    fout << "武功等级10";
    fout << ",";
    fout << "携带物品1";
    fout << ",";
    fout << "携带物品2";
    fout << ",";
    fout << "携带物品3";
    fout << ",";
    fout << "携带物品4";
    fout << ",";
    fout << "携带物品数量1";
    fout << ",";
    fout << "携带物品数量2";
    fout << ",";
    fout << "携带物品数量3";
    fout << ",";
    fout << "携带物品数量4";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << data[i].HeadID;
        fout << ",";
        fout << data[i].IncLife;
        fout << ",";
        fout << data[i].UnUse;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
        fout << '"' << data[i].Nick << '"';
        fout << ",";
        fout << data[i].Sexual;
        fout << ",";
        fout << data[i].Level;
        fout << ",";
        fout << data[i].Exp;
        fout << ",";
        fout << data[i].HP;
        fout << ",";
        fout << data[i].MaxHP;
        fout << ",";
        fout << data[i].Hurt;
        fout << ",";
        fout << data[i].Poison;
        fout << ",";
        fout << data[i].PhysicalPower;
        fout << ",";
        fout << data[i].ExpForMakeItem;
        fout << ",";
        fout << data[i].Equip0;
        fout << ",";
        fout << data[i].Equip1;
        fout << ",";
        for (int j = 0; j < 15; j++)
        {
            fout << data[i].Frame[j];
            if (j != 15 - 1)
            { fout << ","; }
        }
        fout << ",";
        fout << data[i].MPType;
        fout << ",";
        fout << data[i].MP;
        fout << ",";
        fout << data[i].MaxMP;
        fout << ",";
        fout << data[i].Attack;
        fout << ",";
        fout << data[i].Speed;
        fout << ",";
        fout << data[i].Defence;
        fout << ",";
        fout << data[i].Medicine;
        fout << ",";
        fout << data[i].UsePoison;
        fout << ",";
        fout << data[i].Detoxification;
        fout << ",";
        fout << data[i].AntiPoison;
        fout << ",";
        fout << data[i].Fist;
        fout << ",";
        fout << data[i].Sword;
        fout << ",";
        fout << data[i].Knife;
        fout << ",";
        fout << data[i].Unusual;
        fout << ",";
        fout << data[i].HiddenWeapon;
        fout << ",";
        fout << data[i].Knowledge;
        fout << ",";
        fout << data[i].Morality;
        fout << ",";
        fout << data[i].AttackWithPoison;
        fout << ",";
        fout << data[i].AttackTwice;
        fout << ",";
        fout << data[i].Fame;
        fout << ",";
        fout << data[i].IQ;
        fout << ",";
        fout << data[i].PracticeItem;
        fout << ",";
        fout << data[i].ExpForItem;
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].MagicID[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].MagicLevel[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 4; j++)
        {
            fout << data[i].TakingItem[j];
            if (j != 4 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 4; j++)
        {
            fout << data[i].TakingItemCount[j];
            if (j != 4 - 1)
            { fout << ","; }
        }
        fout << std::endl;
    }
}
// 物品
void Save::NewSave::SaveToCSVItemSave(const std::vector<Item>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_物品.csv");
    fout << "编号";
    fout << ",";
    fout << "物品名";
    fout << ",";
    fout << "物品名无用1";
    fout << ",";
    fout << "物品名无用2";
    fout << ",";
    fout << "物品名无用3";
    fout << ",";
    fout << "物品名无用4";
    fout << ",";
    fout << "物品名无用5";
    fout << ",";
    fout << "物品名无用6";
    fout << ",";
    fout << "物品名无用7";
    fout << ",";
    fout << "物品名无用8";
    fout << ",";
    fout << "物品名无用9";
    fout << ",";
    fout << "物品名无用10";
    fout << ",";
    fout << "物品说明";
    fout << ",";
    fout << "练出武功";
    fout << ",";
    fout << "暗器动画编号";
    fout << ",";
    fout << "使用人";
    fout << ",";
    fout << "装备类型";
    fout << ",";
    fout << "显示物品说明";
    fout << ",";
    fout << "物品类型";
    fout << ",";
    fout << "未知5";
    fout << ",";
    fout << "未知6";
    fout << ",";
    fout << "未知7";
    fout << ",";
    fout << "加生命";
    fout << ",";
    fout << "加生命最大值";
    fout << ",";
    fout << "加中毒解毒";
    fout << ",";
    fout << "加体力";
    fout << ",";
    fout << "改变内力性质";
    fout << ",";
    fout << "加内力";
    fout << ",";
    fout << "加内力最大值";
    fout << ",";
    fout << "加攻击力";
    fout << ",";
    fout << "加轻功";
    fout << ",";
    fout << "加防御力";
    fout << ",";
    fout << "加医疗";
    fout << ",";
    fout << "加使毒";
    fout << ",";
    fout << "加解毒";
    fout << ",";
    fout << "加抗毒";
    fout << ",";
    fout << "加拳掌";
    fout << ",";
    fout << "加御剑";
    fout << ",";
    fout << "加耍刀";
    fout << ",";
    fout << "加特殊兵器";
    fout << ",";
    fout << "加暗器技巧";
    fout << ",";
    fout << "加武学常识";
    fout << ",";
    fout << "加品德";
    fout << ",";
    fout << "加左右互搏";
    fout << ",";
    fout << "加攻击带毒";
    fout << ",";
    fout << "仅修炼人物";
    fout << ",";
    fout << "需内力性质";
    fout << ",";
    fout << "需内力";
    fout << ",";
    fout << "需攻击力";
    fout << ",";
    fout << "需轻功";
    fout << ",";
    fout << "需用毒";
    fout << ",";
    fout << "需医疗";
    fout << ",";
    fout << "需解毒";
    fout << ",";
    fout << "需拳掌";
    fout << ",";
    fout << "需御剑";
    fout << ",";
    fout << "需耍刀";
    fout << ",";
    fout << "需特殊兵器";
    fout << ",";
    fout << "需暗器";
    fout << ",";
    fout << "需资质";
    fout << ",";
    fout << "需经验";
    fout << ",";
    fout << "练出物品需经验";
    fout << ",";
    fout << "需材料";
    fout << ",";
    fout << "练出物品1";
    fout << ",";
    fout << "练出物品2";
    fout << ",";
    fout << "练出物品3";
    fout << ",";
    fout << "练出物品4";
    fout << ",";
    fout << "练出物品5";
    fout << ",";
    fout << "练出物品数量1";
    fout << ",";
    fout << "练出物品数量2";
    fout << ",";
    fout << "练出物品数量3";
    fout << ",";
    fout << "练出物品数量4";
    fout << ",";
    fout << "练出物品数量5";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].Name1[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        fout << '"' << data[i].Introduction << '"';
        fout << ",";
        fout << data[i].MagicID;
        fout << ",";
        fout << data[i].HiddenWeaponEffectID;
        fout << ",";
        fout << data[i].User;
        fout << ",";
        fout << data[i].EquipType;
        fout << ",";
        fout << data[i].ShowIntroduction;
        fout << ",";
        fout << data[i].ItemType;
        fout << ",";
        fout << data[i].UnKnown5;
        fout << ",";
        fout << data[i].UnKnown6;
        fout << ",";
        fout << data[i].UnKnown7;
        fout << ",";
        fout << data[i].AddHP;
        fout << ",";
        fout << data[i].AddMaxHP;
        fout << ",";
        fout << data[i].AddPoison;
        fout << ",";
        fout << data[i].AddPhysicalPower;
        fout << ",";
        fout << data[i].ChangeMPType;
        fout << ",";
        fout << data[i].AddMP;
        fout << ",";
        fout << data[i].AddMaxMP;
        fout << ",";
        fout << data[i].AddAttack;
        fout << ",";
        fout << data[i].AddSpeed;
        fout << ",";
        fout << data[i].AddDefence;
        fout << ",";
        fout << data[i].AddMedicine;
        fout << ",";
        fout << data[i].AddUsePoison;
        fout << ",";
        fout << data[i].AddDetoxification;
        fout << ",";
        fout << data[i].AddAntiPoison;
        fout << ",";
        fout << data[i].AddFist;
        fout << ",";
        fout << data[i].AddSword;
        fout << ",";
        fout << data[i].AddKnife;
        fout << ",";
        fout << data[i].AddUnusual;
        fout << ",";
        fout << data[i].AddHiddenWeapon;
        fout << ",";
        fout << data[i].AddKnowledge;
        fout << ",";
        fout << data[i].AddMorality;
        fout << ",";
        fout << data[i].AddAttackTwice;
        fout << ",";
        fout << data[i].AddAttackWithPoison;
        fout << ",";
        fout << data[i].OnlySuitableRole;
        fout << ",";
        fout << data[i].NeedMPType;
        fout << ",";
        fout << data[i].NeedMP;
        fout << ",";
        fout << data[i].NeedAttack;
        fout << ",";
        fout << data[i].NeedSpeed;
        fout << ",";
        fout << data[i].NeedUsePoison;
        fout << ",";
        fout << data[i].NeedMedicine;
        fout << ",";
        fout << data[i].NeedDetoxification;
        fout << ",";
        fout << data[i].NeedFist;
        fout << ",";
        fout << data[i].NeedSword;
        fout << ",";
        fout << data[i].NeedKnife;
        fout << ",";
        fout << data[i].NeedUnusual;
        fout << ",";
        fout << data[i].NeedHiddenWeapon;
        fout << ",";
        fout << data[i].NeedIQ;
        fout << ",";
        fout << data[i].NeedExp;
        fout << ",";
        fout << data[i].NeedExpForMakeItem;
        fout << ",";
        fout << data[i].NeedMaterial;
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].MakeItem[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].MakeItemCount[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << std::endl;
    }
}
// 场景
void Save::NewSave::SaveToCSVSubMapInfoSave(const std::vector<SubMapInfo>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_场景.csv");
    fout << "编号";
    fout << ",";
    fout << "名称";
    fout << ",";
    fout << "出门音乐";
    fout << ",";
    fout << "进门音乐";
    fout << ",";
    fout << "跳转场景";
    fout << ",";
    fout << "进入条件";
    fout << ",";
    fout << "外景入口X1";
    fout << ",";
    fout << "外景入口Y1";
    fout << ",";
    fout << "外景入口X2";
    fout << ",";
    fout << "外景入口Y2";
    fout << ",";
    fout << "入口X";
    fout << ",";
    fout << "入口Y";
    fout << ",";
    fout << "出口X1";
    fout << ",";
    fout << "出口X2";
    fout << ",";
    fout << "出口X3";
    fout << ",";
    fout << "出口Y1";
    fout << ",";
    fout << "出口Y2";
    fout << ",";
    fout << "出口Y3";
    fout << ",";
    fout << "跳转X";
    fout << ",";
    fout << "跳转Y";
    fout << ",";
    fout << "跳转返还X";
    fout << ",";
    fout << "跳转返还Y";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
        fout << data[i].ExitMusic;
        fout << ",";
        fout << data[i].EntranceMusic;
        fout << ",";
        fout << data[i].JumpSubMap;
        fout << ",";
        fout << data[i].EntranceCondition;
        fout << ",";
        fout << data[i].MainEntranceX1;
        fout << ",";
        fout << data[i].MainEntranceY1;
        fout << ",";
        fout << data[i].MainEntranceX2;
        fout << ",";
        fout << data[i].MainEntranceY2;
        fout << ",";
        fout << data[i].EntranceX;
        fout << ",";
        fout << data[i].EntranceY;
        fout << ",";
        for (int j = 0; j < 3; j++)
        {
            fout << data[i].ExitX[j];
            if (j != 3 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 3; j++)
        {
            fout << data[i].ExitY[j];
            if (j != 3 - 1)
            { fout << ","; }
        }
        fout << ",";
        fout << data[i].JumpX;
        fout << ",";
        fout << data[i].JumpY;
        fout << ",";
        fout << data[i].JumpReturnX;
        fout << ",";
        fout << data[i].JumpReturnY;
        fout << std::endl;
    }
}
// 武功
void Save::NewSave::SaveToCSVMagicSave(const std::vector<Magic>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_武功.csv");
    fout << "编号";
    fout << ",";
    fout << "名称";
    fout << ",";
    fout << "未知1";
    fout << ",";
    fout << "未知2";
    fout << ",";
    fout << "未知3";
    fout << ",";
    fout << "未知4";
    fout << ",";
    fout << "未知5";
    fout << ",";
    fout << "出招音效";
    fout << ",";
    fout << "武功类型";
    fout << ",";
    fout << "武功动画";
    fout << ",";
    fout << "伤害类型";
    fout << ",";
    fout << "攻击范围类型";
    fout << ",";
    fout << "消耗内力";
    fout << ",";
    fout << "敌人中毒";
    fout << ",";
    fout << "威力1";
    fout << ",";
    fout << "威力2";
    fout << ",";
    fout << "威力3";
    fout << ",";
    fout << "威力4";
    fout << ",";
    fout << "威力5";
    fout << ",";
    fout << "威力6";
    fout << ",";
    fout << "威力7";
    fout << ",";
    fout << "威力8";
    fout << ",";
    fout << "威力9";
    fout << ",";
    fout << "威力10";
    fout << ",";
    fout << "移动范围1";
    fout << ",";
    fout << "移动范围2";
    fout << ",";
    fout << "移动范围3";
    fout << ",";
    fout << "移动范围4";
    fout << ",";
    fout << "移动范围5";
    fout << ",";
    fout << "移动范围6";
    fout << ",";
    fout << "移动范围7";
    fout << ",";
    fout << "移动范围8";
    fout << ",";
    fout << "移动范围9";
    fout << ",";
    fout << "移动范围10";
    fout << ",";
    fout << "杀伤范围1";
    fout << ",";
    fout << "杀伤范围2";
    fout << ",";
    fout << "杀伤范围3";
    fout << ",";
    fout << "杀伤范围4";
    fout << ",";
    fout << "杀伤范围5";
    fout << ",";
    fout << "杀伤范围6";
    fout << ",";
    fout << "杀伤范围7";
    fout << ",";
    fout << "杀伤范围8";
    fout << ",";
    fout << "杀伤范围9";
    fout << ",";
    fout << "杀伤范围10";
    fout << ",";
    fout << "加内力1";
    fout << ",";
    fout << "加内力2";
    fout << ",";
    fout << "加内力3";
    fout << ",";
    fout << "加内力4";
    fout << ",";
    fout << "加内力5";
    fout << ",";
    fout << "加内力6";
    fout << ",";
    fout << "加内力7";
    fout << ",";
    fout << "加内力8";
    fout << ",";
    fout << "加内力9";
    fout << ",";
    fout << "加内力10";
    fout << ",";
    fout << "杀伤内力1";
    fout << ",";
    fout << "杀伤内力2";
    fout << ",";
    fout << "杀伤内力3";
    fout << ",";
    fout << "杀伤内力4";
    fout << ",";
    fout << "杀伤内力5";
    fout << ",";
    fout << "杀伤内力6";
    fout << ",";
    fout << "杀伤内力7";
    fout << ",";
    fout << "杀伤内力8";
    fout << ",";
    fout << "杀伤内力9";
    fout << ",";
    fout << "杀伤内力10";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].Unknown[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << ",";
        fout << data[i].SoundID;
        fout << ",";
        fout << data[i].MagicType;
        fout << ",";
        fout << data[i].EffectID;
        fout << ",";
        fout << data[i].HurtType;
        fout << ",";
        fout << data[i].AttackAreaType;
        fout << ",";
        fout << data[i].NeedMP;
        fout << ",";
        fout << data[i].WithPoison;
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].Attack[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].SelectDistance[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].AttackDistance[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].AddMP[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].HurtMP[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << std::endl;
    }
}
// 商店
void Save::NewSave::SaveToCSVShopSave(const std::vector<Shop>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_商店.csv");
    fout << "物品编号1";
    fout << ",";
    fout << "物品编号2";
    fout << ",";
    fout << "物品编号3";
    fout << ",";
    fout << "物品编号4";
    fout << ",";
    fout << "物品编号5";
    fout << ",";
    fout << "物品总量1";
    fout << ",";
    fout << "物品总量2";
    fout << ",";
    fout << "物品总量3";
    fout << ",";
    fout << "物品总量4";
    fout << ",";
    fout << "物品总量5";
    fout << ",";
    fout << "物品价格1";
    fout << ",";
    fout << "物品价格2";
    fout << ",";
    fout << "物品价格3";
    fout << ",";
    fout << "物品价格4";
    fout << ",";
    fout << "物品价格5";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].ItemID[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].Total[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].Price[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << std::endl;
    }
}
// 基本
void Save::NewSave::LoadFromCSVBaseInfo(BaseInfo* data, int length, int record)
{
    io::CSVReader<18, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_基本.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "乘船",
        "子场景内",
        "主地图X",
        "主地图Y",
        "子场景X",
        "子场景Y",
        "面朝方向",
        "船X",
        "船Y",
        "船X1",
        "船Y1",
        "内部编码",
        "队友1",
        "队友2",
        "队友3",
        "队友4",
        "队友5",
        "队友6");
    auto getDefault = []()
    {
        BaseInfo nextLineData;
        nextLineData.InShip = 0;
        nextLineData.InSubMap = 0;
        nextLineData.MainMapX = 0;
        nextLineData.MainMapY = 0;
        nextLineData.SubMapX = 0;
        nextLineData.SubMapY = 0;
        nextLineData.FaceTowards = 0;
        nextLineData.ShipX = 0;
        nextLineData.ShipY = 0;
        nextLineData.ShipX1 = 0;
        nextLineData.ShipY1 = 0;
        nextLineData.Encode = 0;
        for (int j = 0; j < 6; j++)
        {
            nextLineData.Team[j] = -1;
        }
        return nextLineData;
    };
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.InShip,
        nextLineData.InSubMap,
        nextLineData.MainMapX,
        nextLineData.MainMapY,
        nextLineData.SubMapX,
        nextLineData.SubMapY,
        nextLineData.FaceTowards,
        nextLineData.ShipX,
        nextLineData.ShipY,
        nextLineData.ShipX1,
        nextLineData.ShipY1,
        nextLineData.Encode,
        nextLineData.Team[0],
        nextLineData.Team[1],
        nextLineData.Team[2],
        nextLineData.Team[3],
        nextLineData.Team[4],
        nextLineData.Team[5]))
    {
        data[lines] = nextLineData;
        if (lines + 1 == length)
        { break; }
        lines++;
        nextLineData = getDefault();
    }
}
// 背包
void Save::NewSave::LoadFromCSVItemList(ItemList* data, int length, int record)
{
    io::CSVReader<2, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_背包.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "物品编号",
        "物品数量");
    auto getDefault = []()
    {
        ItemList nextLineData;
        nextLineData.item_id = -1;
        nextLineData.count = 0;
        return nextLineData;
    };
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.item_id,
        nextLineData.count))
    {
        data[lines] = nextLineData;
        if (lines + 1 == length)
        { break; }
        lines++;
        nextLineData = getDefault();
    }
}
// 人物
void Save::NewSave::LoadFromCSVRoleSave(std::vector<Role>& data, int record)
{
    data.clear();
    io::CSVReader<83, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_人物.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "编号",
        "头像",
        "生命成长",
        "无用",
        "名字",
        "外号",
        "性别",
        "等级",
        "经验",
        "生命",
        "生命最大值",
        "内伤",
        "中毒",
        "体力",
        "物品修炼点数",
        "武器",
        "防具",
        "动作帧数1",
        "动作帧数2",
        "动作帧数3",
        "动作帧数4",
        "动作帧数5",
        "动作帧数6",
        "动作帧数7",
        "动作帧数8",
        "动作帧数9",
        "动作帧数10",
        "动作帧数11",
        "动作帧数12",
        "动作帧数13",
        "动作帧数14",
        "动作帧数15",
        "内力性质",
        "内力",
        "内力最大值",
        "攻击力",
        "轻功",
        "防御力",
        "医疗",
        "用毒",
        "解毒",
        "抗毒",
        "拳掌",
        "御剑",
        "耍刀",
        "特殊",
        "暗器",
        "武学常识",
        "品德",
        "攻击带毒",
        "左右互搏",
        "声望",
        "资质",
        "修炼物品",
        "修炼点数",
        "所会武功1",
        "所会武功2",
        "所会武功3",
        "所会武功4",
        "所会武功5",
        "所会武功6",
        "所会武功7",
        "所会武功8",
        "所会武功9",
        "所会武功10",
        "武功等级1",
        "武功等级2",
        "武功等级3",
        "武功等级4",
        "武功等级5",
        "武功等级6",
        "武功等级7",
        "武功等级8",
        "武功等级9",
        "武功等级10",
        "携带物品1",
        "携带物品2",
        "携带物品3",
        "携带物品4",
        "携带物品数量1",
        "携带物品数量2",
        "携带物品数量3",
        "携带物品数量4");
    auto getDefault = []()
    {
        Role nextLineData;
        nextLineData.ID = 0;
        nextLineData.HeadID = 0;
        nextLineData.IncLife = 0;
        nextLineData.UnUse = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
        memset(nextLineData.Nick, '\0', sizeof(nextLineData.Nick));
        nextLineData.Sexual = 0;
        nextLineData.Level = 0;
        nextLineData.Exp = 0;
        nextLineData.HP = 0;
        nextLineData.MaxHP = 0;
        nextLineData.Hurt = 0;
        nextLineData.Poison = 0;
        nextLineData.PhysicalPower = 0;
        nextLineData.ExpForMakeItem = 0;
        nextLineData.Equip0 = -1;
        nextLineData.Equip1 = -1;
        for (int j = 0; j < 15; j++)
        {
            nextLineData.Frame[j] = 0;
        }
        nextLineData.MPType = 0;
        nextLineData.MP = 0;
        nextLineData.MaxMP = 0;
        nextLineData.Attack = 0;
        nextLineData.Speed = 0;
        nextLineData.Defence = 0;
        nextLineData.Medicine = 0;
        nextLineData.UsePoison = 0;
        nextLineData.Detoxification = 0;
        nextLineData.AntiPoison = 0;
        nextLineData.Fist = 0;
        nextLineData.Sword = 0;
        nextLineData.Knife = 0;
        nextLineData.Unusual = 0;
        nextLineData.HiddenWeapon = 0;
        nextLineData.Knowledge = 0;
        nextLineData.Morality = 0;
        nextLineData.AttackWithPoison = 0;
        nextLineData.AttackTwice = 0;
        nextLineData.Fame = 0;
        nextLineData.IQ = 0;
        nextLineData.PracticeItem = -1;
        nextLineData.ExpForItem = 0;
        for (int j = 0; j < 10; j++)
        {
            nextLineData.MagicID[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.MagicLevel[j] = 0;
        }
        for (int j = 0; j < 4; j++)
        {
            nextLineData.TakingItem[j] = -1;
        }
        for (int j = 0; j < 4; j++)
        {
            nextLineData.TakingItemCount[j] = 0;
        }
        return nextLineData;
    };
    char* Name__;
    char* Nick__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        nextLineData.HeadID,
        nextLineData.IncLife,
        nextLineData.UnUse,
        Name__,
        Nick__,
        nextLineData.Sexual,
        nextLineData.Level,
        nextLineData.Exp,
        nextLineData.HP,
        nextLineData.MaxHP,
        nextLineData.Hurt,
        nextLineData.Poison,
        nextLineData.PhysicalPower,
        nextLineData.ExpForMakeItem,
        nextLineData.Equip0,
        nextLineData.Equip1,
        nextLineData.Frame[0],
        nextLineData.Frame[1],
        nextLineData.Frame[2],
        nextLineData.Frame[3],
        nextLineData.Frame[4],
        nextLineData.Frame[5],
        nextLineData.Frame[6],
        nextLineData.Frame[7],
        nextLineData.Frame[8],
        nextLineData.Frame[9],
        nextLineData.Frame[10],
        nextLineData.Frame[11],
        nextLineData.Frame[12],
        nextLineData.Frame[13],
        nextLineData.Frame[14],
        nextLineData.MPType,
        nextLineData.MP,
        nextLineData.MaxMP,
        nextLineData.Attack,
        nextLineData.Speed,
        nextLineData.Defence,
        nextLineData.Medicine,
        nextLineData.UsePoison,
        nextLineData.Detoxification,
        nextLineData.AntiPoison,
        nextLineData.Fist,
        nextLineData.Sword,
        nextLineData.Knife,
        nextLineData.Unusual,
        nextLineData.HiddenWeapon,
        nextLineData.Knowledge,
        nextLineData.Morality,
        nextLineData.AttackWithPoison,
        nextLineData.AttackTwice,
        nextLineData.Fame,
        nextLineData.IQ,
        nextLineData.PracticeItem,
        nextLineData.ExpForItem,
        nextLineData.MagicID[0],
        nextLineData.MagicID[1],
        nextLineData.MagicID[2],
        nextLineData.MagicID[3],
        nextLineData.MagicID[4],
        nextLineData.MagicID[5],
        nextLineData.MagicID[6],
        nextLineData.MagicID[7],
        nextLineData.MagicID[8],
        nextLineData.MagicID[9],
        nextLineData.MagicLevel[0],
        nextLineData.MagicLevel[1],
        nextLineData.MagicLevel[2],
        nextLineData.MagicLevel[3],
        nextLineData.MagicLevel[4],
        nextLineData.MagicLevel[5],
        nextLineData.MagicLevel[6],
        nextLineData.MagicLevel[7],
        nextLineData.MagicLevel[8],
        nextLineData.MagicLevel[9],
        nextLineData.TakingItem[0],
        nextLineData.TakingItem[1],
        nextLineData.TakingItem[2],
        nextLineData.TakingItem[3],
        nextLineData.TakingItemCount[0],
        nextLineData.TakingItemCount[1],
        nextLineData.TakingItemCount[2],
        nextLineData.TakingItemCount[3]))
    {
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        strncpy(nextLineData.Nick, Nick__, sizeof(nextLineData.Nick) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
// 物品
void Save::NewSave::LoadFromCSVItemSave(std::vector<Item>& data, int record)
{
    data.clear();
    io::CSVReader<72, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_物品.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "编号",
        "物品名",
        "物品名无用1",
        "物品名无用2",
        "物品名无用3",
        "物品名无用4",
        "物品名无用5",
        "物品名无用6",
        "物品名无用7",
        "物品名无用8",
        "物品名无用9",
        "物品名无用10",
        "物品说明",
        "练出武功",
        "暗器动画编号",
        "使用人",
        "装备类型",
        "显示物品说明",
        "物品类型",
        "未知5",
        "未知6",
        "未知7",
        "加生命",
        "加生命最大值",
        "加中毒解毒",
        "加体力",
        "改变内力性质",
        "加内力",
        "加内力最大值",
        "加攻击力",
        "加轻功",
        "加防御力",
        "加医疗",
        "加使毒",
        "加解毒",
        "加抗毒",
        "加拳掌",
        "加御剑",
        "加耍刀",
        "加特殊兵器",
        "加暗器技巧",
        "加武学常识",
        "加品德",
        "加左右互搏",
        "加攻击带毒",
        "仅修炼人物",
        "需内力性质",
        "需内力",
        "需攻击力",
        "需轻功",
        "需用毒",
        "需医疗",
        "需解毒",
        "需拳掌",
        "需御剑",
        "需耍刀",
        "需特殊兵器",
        "需暗器",
        "需资质",
        "需经验",
        "练出物品需经验",
        "需材料",
        "练出物品1",
        "练出物品2",
        "练出物品3",
        "练出物品4",
        "练出物品5",
        "练出物品数量1",
        "练出物品数量2",
        "练出物品数量3",
        "练出物品数量4",
        "练出物品数量5");
    auto getDefault = []()
    {
        Item nextLineData;
        nextLineData.ID = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
        for (int j = 0; j < 10; j++)
        {
            nextLineData.Name1[j] = 0;
        }
        memset(nextLineData.Introduction, '\0', sizeof(nextLineData.Introduction));
        nextLineData.MagicID = -1;
        nextLineData.HiddenWeaponEffectID = -1;
        nextLineData.User = -1;
        nextLineData.EquipType = -1;
        nextLineData.ShowIntroduction = 0;
        nextLineData.ItemType = 0;
        nextLineData.UnKnown5 = 0;
        nextLineData.UnKnown6 = 0;
        nextLineData.UnKnown7 = 0;
        nextLineData.AddHP = 0;
        nextLineData.AddMaxHP = 0;
        nextLineData.AddPoison = 0;
        nextLineData.AddPhysicalPower = 0;
        nextLineData.ChangeMPType = -1;
        nextLineData.AddMP = 0;
        nextLineData.AddMaxMP = 0;
        nextLineData.AddAttack = 0;
        nextLineData.AddSpeed = 0;
        nextLineData.AddDefence = 0;
        nextLineData.AddMedicine = 0;
        nextLineData.AddUsePoison = 0;
        nextLineData.AddDetoxification = 0;
        nextLineData.AddAntiPoison = 0;
        nextLineData.AddFist = 0;
        nextLineData.AddSword = 0;
        nextLineData.AddKnife = 0;
        nextLineData.AddUnusual = 0;
        nextLineData.AddHiddenWeapon = 0;
        nextLineData.AddKnowledge = 0;
        nextLineData.AddMorality = 0;
        nextLineData.AddAttackTwice = 0;
        nextLineData.AddAttackWithPoison = 0;
        nextLineData.OnlySuitableRole = -1;
        nextLineData.NeedMPType = 0;
        nextLineData.NeedMP = 0;
        nextLineData.NeedAttack = 0;
        nextLineData.NeedSpeed = 0;
        nextLineData.NeedUsePoison = 0;
        nextLineData.NeedMedicine = 0;
        nextLineData.NeedDetoxification = 0;
        nextLineData.NeedFist = 0;
        nextLineData.NeedSword = 0;
        nextLineData.NeedKnife = 0;
        nextLineData.NeedUnusual = 0;
        nextLineData.NeedHiddenWeapon = 0;
        nextLineData.NeedIQ = 0;
        nextLineData.NeedExp = 0;
        nextLineData.NeedExpForMakeItem = 0;
        nextLineData.NeedMaterial = -1;
        for (int j = 0; j < 5; j++)
        {
            nextLineData.MakeItem[j] = -1;
        }
        for (int j = 0; j < 5; j++)
        {
            nextLineData.MakeItemCount[j] = 0;
        }
        return nextLineData;
    };
    char* Name__;
    char* Introduction__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        Name__,
        nextLineData.Name1[0],
        nextLineData.Name1[1],
        nextLineData.Name1[2],
        nextLineData.Name1[3],
        nextLineData.Name1[4],
        nextLineData.Name1[5],
        nextLineData.Name1[6],
        nextLineData.Name1[7],
        nextLineData.Name1[8],
        nextLineData.Name1[9],
        Introduction__,
        nextLineData.MagicID,
        nextLineData.HiddenWeaponEffectID,
        nextLineData.User,
        nextLineData.EquipType,
        nextLineData.ShowIntroduction,
        nextLineData.ItemType,
        nextLineData.UnKnown5,
        nextLineData.UnKnown6,
        nextLineData.UnKnown7,
        nextLineData.AddHP,
        nextLineData.AddMaxHP,
        nextLineData.AddPoison,
        nextLineData.AddPhysicalPower,
        nextLineData.ChangeMPType,
        nextLineData.AddMP,
        nextLineData.AddMaxMP,
        nextLineData.AddAttack,
        nextLineData.AddSpeed,
        nextLineData.AddDefence,
        nextLineData.AddMedicine,
        nextLineData.AddUsePoison,
        nextLineData.AddDetoxification,
        nextLineData.AddAntiPoison,
        nextLineData.AddFist,
        nextLineData.AddSword,
        nextLineData.AddKnife,
        nextLineData.AddUnusual,
        nextLineData.AddHiddenWeapon,
        nextLineData.AddKnowledge,
        nextLineData.AddMorality,
        nextLineData.AddAttackTwice,
        nextLineData.AddAttackWithPoison,
        nextLineData.OnlySuitableRole,
        nextLineData.NeedMPType,
        nextLineData.NeedMP,
        nextLineData.NeedAttack,
        nextLineData.NeedSpeed,
        nextLineData.NeedUsePoison,
        nextLineData.NeedMedicine,
        nextLineData.NeedDetoxification,
        nextLineData.NeedFist,
        nextLineData.NeedSword,
        nextLineData.NeedKnife,
        nextLineData.NeedUnusual,
        nextLineData.NeedHiddenWeapon,
        nextLineData.NeedIQ,
        nextLineData.NeedExp,
        nextLineData.NeedExpForMakeItem,
        nextLineData.NeedMaterial,
        nextLineData.MakeItem[0],
        nextLineData.MakeItem[1],
        nextLineData.MakeItem[2],
        nextLineData.MakeItem[3],
        nextLineData.MakeItem[4],
        nextLineData.MakeItemCount[0],
        nextLineData.MakeItemCount[1],
        nextLineData.MakeItemCount[2],
        nextLineData.MakeItemCount[3],
        nextLineData.MakeItemCount[4]))
    {
        strncpy(nextLineData.Introduction, Introduction__, sizeof(nextLineData.Introduction) - 1);
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
// 场景
void Save::NewSave::LoadFromCSVSubMapInfoSave(std::vector<SubMapInfo>& data, int record)
{
    data.clear();
    io::CSVReader<22, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_场景.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "编号",
        "名称",
        "出门音乐",
        "进门音乐",
        "跳转场景",
        "进入条件",
        "外景入口X1",
        "外景入口Y1",
        "外景入口X2",
        "外景入口Y2",
        "入口X",
        "入口Y",
        "出口X1",
        "出口X2",
        "出口X3",
        "出口Y1",
        "出口Y2",
        "出口Y3",
        "跳转X",
        "跳转Y",
        "跳转返还X",
        "跳转返还Y");
    auto getDefault = []()
    {
        SubMapInfo nextLineData;
        nextLineData.ID = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
        nextLineData.ExitMusic = 0;
        nextLineData.EntranceMusic = -1;
        nextLineData.JumpSubMap = -1;
        nextLineData.EntranceCondition = 0;
        nextLineData.MainEntranceX1 = 0;
        nextLineData.MainEntranceY1 = 0;
        nextLineData.MainEntranceX2 = 0;
        nextLineData.MainEntranceY2 = 0;
        nextLineData.EntranceX = 0;
        nextLineData.EntranceY = 0;
        for (int j = 0; j < 3; j++)
        {
            nextLineData.ExitX[j] = -1;
        }
        for (int j = 0; j < 3; j++)
        {
            nextLineData.ExitY[j] = -1;
        }
        nextLineData.JumpX = 0;
        nextLineData.JumpY = 0;
        nextLineData.JumpReturnX = 0;
        nextLineData.JumpReturnY = 0;
        return nextLineData;
    };
    char* Name__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        Name__,
        nextLineData.ExitMusic,
        nextLineData.EntranceMusic,
        nextLineData.JumpSubMap,
        nextLineData.EntranceCondition,
        nextLineData.MainEntranceX1,
        nextLineData.MainEntranceY1,
        nextLineData.MainEntranceX2,
        nextLineData.MainEntranceY2,
        nextLineData.EntranceX,
        nextLineData.EntranceY,
        nextLineData.ExitX[0],
        nextLineData.ExitX[1],
        nextLineData.ExitX[2],
        nextLineData.ExitY[0],
        nextLineData.ExitY[1],
        nextLineData.ExitY[2],
        nextLineData.JumpX,
        nextLineData.JumpY,
        nextLineData.JumpReturnX,
        nextLineData.JumpReturnY))
    {
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
// 武功
void Save::NewSave::LoadFromCSVMagicSave(std::vector<Magic>& data, int record)
{
    data.clear();
    io::CSVReader<64, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_武功.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "编号",
        "名称",
        "未知1",
        "未知2",
        "未知3",
        "未知4",
        "未知5",
        "出招音效",
        "武功类型",
        "武功动画",
        "伤害类型",
        "攻击范围类型",
        "消耗内力",
        "敌人中毒",
        "威力1",
        "威力2",
        "威力3",
        "威力4",
        "威力5",
        "威力6",
        "威力7",
        "威力8",
        "威力9",
        "威力10",
        "移动范围1",
        "移动范围2",
        "移动范围3",
        "移动范围4",
        "移动范围5",
        "移动范围6",
        "移动范围7",
        "移动范围8",
        "移动范围9",
        "移动范围10",
        "杀伤范围1",
        "杀伤范围2",
        "杀伤范围3",
        "杀伤范围4",
        "杀伤范围5",
        "杀伤范围6",
        "杀伤范围7",
        "杀伤范围8",
        "杀伤范围9",
        "杀伤范围10",
        "加内力1",
        "加内力2",
        "加内力3",
        "加内力4",
        "加内力5",
        "加内力6",
        "加内力7",
        "加内力8",
        "加内力9",
        "加内力10",
        "杀伤内力1",
        "杀伤内力2",
        "杀伤内力3",
        "杀伤内力4",
        "杀伤内力5",
        "杀伤内力6",
        "杀伤内力7",
        "杀伤内力8",
        "杀伤内力9",
        "杀伤内力10");
    auto getDefault = []()
    {
        Magic nextLineData;
        nextLineData.ID = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
        for (int j = 0; j < 5; j++)
        {
            nextLineData.Unknown[j] = 0;
        }
        nextLineData.SoundID = 0;
        nextLineData.MagicType = 0;
        nextLineData.EffectID = 0;
        nextLineData.HurtType = 0;
        nextLineData.AttackAreaType = 0;
        nextLineData.NeedMP = 0;
        nextLineData.WithPoison = 0;
        for (int j = 0; j < 10; j++)
        {
            nextLineData.Attack[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.SelectDistance[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.AttackDistance[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.AddMP[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.HurtMP[j] = 0;
        }
        return nextLineData;
    };
    char* Name__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        Name__,
        nextLineData.Unknown[0],
        nextLineData.Unknown[1],
        nextLineData.Unknown[2],
        nextLineData.Unknown[3],
        nextLineData.Unknown[4],
        nextLineData.SoundID,
        nextLineData.MagicType,
        nextLineData.EffectID,
        nextLineData.HurtType,
        nextLineData.AttackAreaType,
        nextLineData.NeedMP,
        nextLineData.WithPoison,
        nextLineData.Attack[0],
        nextLineData.Attack[1],
        nextLineData.Attack[2],
        nextLineData.Attack[3],
        nextLineData.Attack[4],
        nextLineData.Attack[5],
        nextLineData.Attack[6],
        nextLineData.Attack[7],
        nextLineData.Attack[8],
        nextLineData.Attack[9],
        nextLineData.SelectDistance[0],
        nextLineData.SelectDistance[1],
        nextLineData.SelectDistance[2],
        nextLineData.SelectDistance[3],
        nextLineData.SelectDistance[4],
        nextLineData.SelectDistance[5],
        nextLineData.SelectDistance[6],
        nextLineData.SelectDistance[7],
        nextLineData.SelectDistance[8],
        nextLineData.SelectDistance[9],
        nextLineData.AttackDistance[0],
        nextLineData.AttackDistance[1],
        nextLineData.AttackDistance[2],
        nextLineData.AttackDistance[3],
        nextLineData.AttackDistance[4],
        nextLineData.AttackDistance[5],
        nextLineData.AttackDistance[6],
        nextLineData.AttackDistance[7],
        nextLineData.AttackDistance[8],
        nextLineData.AttackDistance[9],
        nextLineData.AddMP[0],
        nextLineData.AddMP[1],
        nextLineData.AddMP[2],
        nextLineData.AddMP[3],
        nextLineData.AddMP[4],
        nextLineData.AddMP[5],
        nextLineData.AddMP[6],
        nextLineData.AddMP[7],
        nextLineData.AddMP[8],
        nextLineData.AddMP[9],
        nextLineData.HurtMP[0],
        nextLineData.HurtMP[1],
        nextLineData.HurtMP[2],
        nextLineData.HurtMP[3],
        nextLineData.HurtMP[4],
        nextLineData.HurtMP[5],
        nextLineData.HurtMP[6],
        nextLineData.HurtMP[7],
        nextLineData.HurtMP[8],
        nextLineData.HurtMP[9]))
    {
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
// 商店
void Save::NewSave::LoadFromCSVShopSave(std::vector<Shop>& data, int record)
{
    data.clear();
    io::CSVReader<15, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_商店.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "物品编号1",
        "物品编号2",
        "物品编号3",
        "物品编号4",
        "物品编号5",
        "物品总量1",
        "物品总量2",
        "物品总量3",
        "物品总量4",
        "物品总量5",
        "物品价格1",
        "物品价格2",
        "物品价格3",
        "物品价格4",
        "物品价格5");
    auto getDefault = []()
    {
        Shop nextLineData;
        for (int j = 0; j < 5; j++)
        {
            nextLineData.ItemID[j] = -1;
        }
        for (int j = 0; j < 5; j++)
        {
            nextLineData.Total[j] = 0;
        }
        for (int j = 0; j < 5; j++)
        {
            nextLineData.Price[j] = 0;
        }
        return nextLineData;
    };
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ItemID[0],
        nextLineData.ItemID[1],
        nextLineData.ItemID[2],
        nextLineData.ItemID[3],
        nextLineData.ItemID[4],
        nextLineData.Total[0],
        nextLineData.Total[1],
        nextLineData.Total[2],
        nextLineData.Total[3],
        nextLineData.Total[4],
        nextLineData.Price[0],
        nextLineData.Price[1],
        nextLineData.Price[2],
        nextLineData.Price[3],
        nextLineData.Price[4]))
    {
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
void Save::NewSave::InsertRoleAt(std::vector<Role>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
    for (int i = 0; i < 1; i++)
    {
        auto p = Save::getInstance();
        if (p->Team[0] >= idx)
        { p->Team[0] += 1; }
        if (p->Team[1] >= idx)
        { p->Team[1] += 1; }
        if (p->Team[2] >= idx)
        { p->Team[2] += 1; }
        if (p->Team[3] >= idx)
        { p->Team[3] += 1; }
        if (p->Team[4] >= idx)
        { p->Team[4] += 1; }
        if (p->Team[5] >= idx)
        { p->Team[5] += 1; }
    }
    for (auto& p : Save::getInstance()->getItems())
    {
        if (p->User >= idx)
        { p->User += 1; }
        if (p->OnlySuitableRole >= idx)
        { p->OnlySuitableRole += 1; }
    }
}
void Save::NewSave::InsertItemAt(std::vector<Item>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        auto* p = &(Save::getInstance()->Items[i]);
        if (p->item_id >= idx)
        { p->item_id += 1; }
    }
    for (auto& p : Save::getInstance()->getRoles())
    {
        if (p->Equip0 >= idx)
        { p->Equip0 += 1; }
        if (p->Equip1 >= idx)
        { p->Equip1 += 1; }
        if (p->PracticeItem >= idx)
        { p->PracticeItem += 1; }
        if (p->TakingItem[0] >= idx)
        { p->TakingItem[0] += 1; }
        if (p->TakingItem[1] >= idx)
        { p->TakingItem[1] += 1; }
        if (p->TakingItem[2] >= idx)
        { p->TakingItem[2] += 1; }
        if (p->TakingItem[3] >= idx)
        { p->TakingItem[3] += 1; }
    }
    for (auto& p : Save::getInstance()->getItems())
    {
        if (p->NeedMaterial >= idx)
        { p->NeedMaterial += 1; }
        if (p->MakeItem[0] >= idx)
        { p->MakeItem[0] += 1; }
        if (p->MakeItem[1] >= idx)
        { p->MakeItem[1] += 1; }
        if (p->MakeItem[2] >= idx)
        { p->MakeItem[2] += 1; }
        if (p->MakeItem[3] >= idx)
        { p->MakeItem[3] += 1; }
        if (p->MakeItem[4] >= idx)
        { p->MakeItem[4] += 1; }
    }
    for (auto& p : Save::getInstance()->getShops())
    {
        if (p->ItemID[0] >= idx)
        { p->ItemID[0] += 1; }
        if (p->ItemID[1] >= idx)
        { p->ItemID[1] += 1; }
        if (p->ItemID[2] >= idx)
        { p->ItemID[2] += 1; }
        if (p->ItemID[3] >= idx)
        { p->ItemID[3] += 1; }
        if (p->ItemID[4] >= idx)
        { p->ItemID[4] += 1; }
    }
}
void Save::NewSave::InsertSubMapInfoAt(std::vector<SubMapInfo>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
}
void Save::NewSave::InsertMagicAt(std::vector<Magic>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
    for (auto& p : Save::getInstance()->getRoles())
    {
        if (p->MagicID[0] >= idx)
        { p->MagicID[0] += 1; }
        if (p->MagicID[1] >= idx)
        { p->MagicID[1] += 1; }
        if (p->MagicID[2] >= idx)
        { p->MagicID[2] += 1; }
        if (p->MagicID[3] >= idx)
        { p->MagicID[3] += 1; }
        if (p->MagicID[4] >= idx)
        { p->MagicID[4] += 1; }
        if (p->MagicID[5] >= idx)
        { p->MagicID[5] += 1; }
        if (p->MagicID[6] >= idx)
        { p->MagicID[6] += 1; }
        if (p->MagicID[7] >= idx)
        { p->MagicID[7] += 1; }
        if (p->MagicID[8] >= idx)
        { p->MagicID[8] += 1; }
        if (p->MagicID[9] >= idx)
        { p->MagicID[9] += 1; }
    }
    for (auto& p : Save::getInstance()->getItems())
    {
        if (p->MagicID >= idx)
        { p->MagicID += 1; }
    }
}
void Save::NewSave::InsertShopAt(std::vector<Shop>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        // data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
}