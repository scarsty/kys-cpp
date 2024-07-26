#include "Save.h"
#include "GameUtil.h"
#include "GrpIdxFile.h"
#include "NewSave.h"
#include "PotConv.h"
#include "filefunc.h"
#include "fmt1.h"

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
        filename = fmt1::format(GameUtil::PATH() + "save/{}{}.grp", c, i);
        if (c == 'r')
        {
            filename += "32";
        }
    }
    else
    {
        if (c == 'r')
        {
            filename = GameUtil::PATH() + "save/ranger.grp32";
        }
        else if (c == 's')
        {
            filename = GameUtil::PATH() + "save/allsin.grp";
        }
        else if (c == 'd')
        {
            filename = GameUtil::PATH() + "save/alldef.grp";
        }
    }
    return filename;
}

bool Save::checkSaveFileExist(int num)
{
    return filefunc::fileExist(getFilename(num, 'r'))
        && filefunc::fileExist(getFilename(num, 's'))
        && filefunc::fileExist(getFilename(num, 'd'));
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
    loadRFromDB(num);
    loadSD(num);

    //saveRToDB(0);    //调试用
    makeMapsAndRepairID();

    //saveRToDB(num);    //临时转换
    return true;
}

void Save::loadR(int num)
{
    std::string filenamer = getFilename(num, 'r');
    std::string filename_idx = GameUtil::PATH() + "save/ranger.idx32";
    auto rgrp = GrpIdxFile::getIdxContent(filename_idx, filenamer, &offset_, &length_);
    memcpy((void*)this, rgrp.data() + offset_[0], length_[0]);
    filefunc::readDataToVector(rgrp.data() + offset_[1], length_[1], roles_mem_, sizeof(RoleSave));
    filefunc::readDataToVector(rgrp.data() + offset_[2], length_[2], items_mem_, sizeof(ItemSave));
    filefunc::readDataToVector(rgrp.data() + offset_[3], length_[3], submap_infos_mem_, sizeof(SubMapInfoSave));
    filefunc::readDataToVector(rgrp.data() + offset_[4], length_[4], magics_mem_, sizeof(MagicSave));
    filefunc::readDataToVector(rgrp.data() + offset_[5], length_[5], shops_mem_, sizeof(ShopSave));
    updateAllPtrVector();

    //内部编码为65001
    if (Encode != 65001)
    {
        if (Encode == 936)
        {
            for (auto i : roles_)
            {
                PotConv::fromCP936ToUTF8(i->Name, i->Name);
                PotConv::fromCP936ToUTF8(i->Nick, i->Nick);
            }
            for (auto i : items_)
            {
                PotConv::fromCP936ToUTF8(i->Name, i->Name);
                PotConv::fromCP936ToUTF8(i->Introduction, i->Introduction);
            }
            for (auto i : magics_)
            {
                PotConv::fromCP936ToUTF8(i->Name, i->Name);
            }
            for (auto i : submap_infos_)
            {
                PotConv::fromCP936ToUTF8(i->Name, i->Name);
            }
        }
        else
        {
            for (auto i : roles_)
            {
                PotConv::fromCP950ToUTF8(i->Name, i->Name);
                PotConv::fromCP950ToUTF8(i->Nick, i->Nick);
            }
            for (auto i : items_)
            {
                PotConv::fromCP950ToUTF8(i->Name, i->Name);
                PotConv::fromCP950ToUTF8(i->Introduction, i->Introduction);
            }
            for (auto i : magics_)
            {
                PotConv::fromCP950ToUTF8(i->Name, i->Name);
            }
            for (auto i : submap_infos_)
            {
                PotConv::fromCP950ToUTF8(i->Name, i->Name);
            }
        }
        Encode = 65001;
    }
}

void Save::loadSD(int num)
{
    std::string filenames = getFilename(num, 's');
    std::string filenamed = getFilename(num, 'd');

    auto submap_count = submap_infos_.size();

    auto sdata = new char[submap_count * sdata_length_];
    auto ddata = new char[submap_count * ddata_length_];
    GrpIdxFile::readFile(filenames, sdata, submap_count * sdata_length_);
    GrpIdxFile::readFile(filenamed, ddata, submap_count * ddata_length_);
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
    saveRToDB(num);
    return true;
}

void Save::saveR(int num)
{
    std::string filenamer = getFilename(num, 'r');

    char* rgrp = new char[offset_.back()];
    memcpy(rgrp + offset_[0], &InShip, length_[0]);
    filefunc::writeVectorToData(rgrp + offset_[1], length_[1], roles_mem_, sizeof(RoleSave));
    filefunc::writeVectorToData(rgrp + offset_[2], length_[2], items_mem_, sizeof(ItemSave));
    filefunc::writeVectorToData(rgrp + offset_[3], length_[3], submap_infos_mem_, sizeof(SubMapInfoSave));
    filefunc::writeVectorToData(rgrp + offset_[4], length_[4], magics_mem_, sizeof(MagicSave));
    filefunc::writeVectorToData(rgrp + offset_[5], length_[5], shops_mem_, sizeof(ShopSave));

    GrpIdxFile::writeFile(filenamer, rgrp, offset_.back());
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
    GrpIdxFile::writeFile(filenames, sdata, submap_count * sdata_length_);
    GrpIdxFile::writeFile(filenamed, ddata, submap_count * ddata_length_);
    delete[] sdata;
    delete[] ddata;
}

void Save::resetRData(const std::vector<RoleSave>& newData)
{
    roles_mem_.clear();
    for (int i = 0; i < newData.size(); i++)
    {
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

std::vector<std::tuple<Item*, int>> Save::getAvailableEquipItems()
{
    std::vector<std::tuple<Item*, int>> ret = {};
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        auto id = Items[i].item_id;
        auto count = Items[i].count;
        if (id < 0)
        {
            break;
        }
        if (count <= 0)
        {
            continue;
        }
        auto item = getItem(id);
        if (item && (item->ItemType == 4))
        // if (item && (item->ItemType == 3 || item->ItemType == 4))
        {
            ret.emplace_back(item, count);
        }
    }
    return ret;
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

void Save::makeMapsAndRepairID()
{
    roles_by_name_.clear();
    magics_by_name_.clear();
    items_by_name_.clear();
    submap_infos_by_name_.clear();

    //有重名的，斟酌使用
    int count = 0;
    for (auto& i : roles_)
    {
        roles_by_name_[i->Name] = i;
        i->ID = count++;
    }
    count = 0;
    for (auto& i : magics_)
    {
        magics_by_name_[i->Name] = i;
        i->ID = count++;
    }
    count = 0;
    for (auto& i : items_)
    {
        items_by_name_[i->Name] = i;
        i->ID = count++;
    }
    count = 0;
    for (auto& i : submap_infos_)
    {
        submap_infos_by_name_[i->Name] = i;
        i->ID = count++;
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

/*
void Save::saveRToCSV(int num)
{
    NewSave::SaveCSVBaseInfo((BaseInfo*)this, 1, num);
    // 背包
    NewSave::SaveCSVItemList(Items, ITEM_IN_BAG_COUNT, num);
    // 人物
    NewSave::SaveCSVRoleSave(roles_mem_, num);
    // 物品
    NewSave::SaveCSVItemSave(items_mem_, num);
    // 场景
    NewSave::SaveCSVSubMapInfoSave(submap_infos_mem_, num);
    // 武功
    NewSave::SaveCSVMagicSave(magics_mem_, num);
    // 商店
    NewSave::SaveCSVShopSave(shops_mem_, num);
}

void Save::loadRFromCSV(int num)
{
    NewSave::LoadCSVBaseInfo((BaseInfo*)this, 1, num);
    NewSave::LoadCSVItemList(Items, ITEM_IN_BAG_COUNT, num);
    NewSave::LoadCSVRoleSave(roles_mem_, num);
    NewSave::LoadCSVItemSave(items_mem_, num);
    NewSave::LoadCSVSubMapInfoSave(submap_infos_mem_, num);
    NewSave::LoadCSVMagicSave(magics_mem_, num);
    NewSave::LoadCSVShopSave(shops_mem_, num);
    updateAllPtrVector();
    makeMaps();
}

bool Save::insertAt(const std::string& type, int idx)
{
    if (type == "Role")
    {
        NewSave::InsertRoleAt(roles_mem_, idx);
        return true;
    }
    else if (type == "Item")
    {
        NewSave::InsertItemAt(items_mem_, idx);
        return true;
    }
    else if (type == "Magic")
    {
        NewSave::InsertMagicAt(magics_mem_, idx);
        return true;
    }
    else if (type == "SubMapInfo")
    {
        NewSave::InsertSubMapInfoAt(submap_infos_mem_, idx);
        return true;
    }
    else if (type == "Shop")
    {
        NewSave::InsertShopAt(shops_mem_, idx);
        return true;
    }
    return false;
}
*/

void Save::saveRToDB(int num)
{
    sqlite3* db;
    //此处最好复制一个，先搞搞再说
    std::string filename = GameUtil::PATH() + "save/" + std::to_string(num) + ".db";
    //convert::writeStringToFile(convert::readStringFromFile(filename0), filename);
    sqlite3_open(filename.c_str(), &db);
    saveRToDB(db);
    sqlite3_close(db);
}

void Save::loadRFromDB(int num)
{
    auto filename = GameUtil::PATH() + "save/" + std::to_string(num) + ".db";
    if (!filefunc::fileExist(filename))
    {
        return;
    }
    sqlite3* db;
    sqlite3_open(filename.c_str(), &db);
    loadRFromDB(db);
    sqlite3_close(db);
    updateAllPtrVector();
    Encode = 65001;
}

void Save::saveRToDB(sqlite3* db)
{
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
    NewSave::SaveDBBaseInfo(db, (BaseInfo*)this, 1);
    NewSave::SaveDBItemList(db, Items, ITEM_IN_BAG_COUNT);
    NewSave::SaveDBRoleSave(db, roles_mem_);
    NewSave::SaveDBItemSave(db, items_mem_);
    NewSave::SaveDBSubMapInfoSave(db, submap_infos_mem_);
    NewSave::SaveDBMagicSave(db, magics_mem_);
    NewSave::SaveDBShopSave(db, shops_mem_);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}

void Save::loadRFromDB(sqlite3* db)
{
    NewSave::LoadDBBaseInfo(db, (BaseInfo*)this, 1);
    NewSave::LoadDBItemList(db, Items, ITEM_IN_BAG_COUNT);
    NewSave::LoadDBRoleSave(db, roles_mem_);
    NewSave::LoadDBItemSave(db, items_mem_);
    NewSave::LoadDBSubMapInfoSave(db, submap_infos_mem_);
    NewSave::LoadDBMagicSave(db, magics_mem_);
    NewSave::LoadDBShopSave(db, shops_mem_);
}

void Save::runSql(const std::string& cmd)
{
    if (cmd.find("update ") != 0) { return; }
    sqlite3* db = nullptr;
    sqlite3_open(nullptr, &db);
    if (db)
    {
        if (cmd.find("base ") != std::string::npos)
        {
            NewSave::SaveDBBaseInfo(db, (BaseInfo*)this, 1);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBBaseInfo(db, (BaseInfo*)this, 1);
        }
        else if (cmd.find("bag ") != std::string::npos)
        {
            NewSave::SaveDBItemList(db, Items, ITEM_IN_BAG_COUNT);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBItemList(db, Items, ITEM_IN_BAG_COUNT);
        }
        else if (cmd.find("role ") != std::string::npos)
        {
            NewSave::SaveDBRoleSave(db, roles_mem_);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBRoleSave(db, roles_mem_);
        }
        else if (cmd.find("item ") != std::string::npos)
        {
            NewSave::SaveDBItemSave(db, items_mem_);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBItemSave(db, items_mem_);
        }
        else if (cmd.find("submap ") != std::string::npos)
        {
            NewSave::SaveDBSubMapInfoSave(db, submap_infos_mem_);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBSubMapInfoSave(db, submap_infos_mem_);
        }
        else if (cmd.find("magic ") != std::string::npos)
        {
            NewSave::SaveDBMagicSave(db, magics_mem_);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBMagicSave(db, magics_mem_);
        }
        else if (cmd.find("shop ") != std::string::npos)
        {
            NewSave::SaveDBShopSave(db, shops_mem_);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBShopSave(db, shops_mem_);
        }
        sqlite3_close(db);
    }
}
