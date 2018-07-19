#include "File.h"
#include "GrpIdxFile.h"
#include "PotConv.h"
#include "Save.h"
#include "libconvert.h"
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

bool Save::load(int num)
{
    if (!checkSaveFileExist(num))
    {
        return false;
    }
    std::string filenamer = getFilename(num, 'r');
    std::string filenames = getFilename(num, 's');
    std::string filenamed = getFilename(num, 'd');
    std::string filename_idx = "../game/save/ranger.idx32";

    auto rgrp = GrpIdxFile::getIdxContent(filename_idx, filenamer, &offset_, &length_);

    memcpy(&InShip, rgrp.data() + offset_[0], length_[0]);
    File::readDataToVector(rgrp.data() + offset_[1], length_[1], roles_mem_, sizeof(RoleSave));
    File::readDataToVector(rgrp.data() + offset_[2], length_[2], items_mem_, sizeof(ItemSave));
    File::readDataToVector(rgrp.data() + offset_[3], length_[3], submap_infos_mem_, sizeof(SubMapInfoSave));
    File::readDataToVector(rgrp.data() + offset_[4], length_[4], magics_mem_, sizeof(MagicSave));
    File::readDataToVector(rgrp.data() + offset_[5], length_[5], shops_mem_, sizeof(ShopSave));

    toPtrVector(roles_mem_, roles_);
    toPtrVector(items_mem_, items_);
    toPtrVector(submap_infos_mem_, submap_infos_);
    toPtrVector(magics_mem_, magics_);
    toPtrVector(shops_mem_, shops_);

    auto submap_count = submap_infos_.size();

    auto sdata = std::vector<char>(submap_count * sdata_length_);
    auto ddata = std::vector<char>(submap_count * ddata_length_);
    File::readFile(filenames, sdata.data(), submap_count * sdata_length_);
    File::readFile(filenamed, ddata.data(), submap_count * ddata_length_);
    for (int i = 0; i < submap_count; i++)
    {
        memcpy(&(submap_infos_mem_[i].LayerData(0, 0, 0)), sdata.data() + sdata_length_ * i, sdata_length_);
        memcpy(submap_infos_mem_[i].Event(0), ddata.data() + ddata_length_ * i, ddata_length_);
    }

    //�ڲ�����Ϊcp936
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

bool Save::save(int num)
{
    std::string filenamer = getFilename(num, 'r');
    std::string filenames = getFilename(num, 's');
    std::string filenamed = getFilename(num, 'd');

    std::vector<char> rgrp(offset_.back());
    memcpy(rgrp.data() + offset_[0], &InShip, length_[0]);
    File::writeVectorToData(rgrp.data() + offset_[1], length_[1], roles_mem_, sizeof(RoleSave));
    File::writeVectorToData(rgrp.data() + offset_[2], length_[2], items_mem_, sizeof(ItemSave));
    File::writeVectorToData(rgrp.data() + offset_[3], length_[3], submap_infos_mem_, sizeof(SubMapInfoSave));
    File::writeVectorToData(rgrp.data() + offset_[4], length_[4], magics_mem_, sizeof(MagicSave));
    File::writeVectorToData(rgrp.data() + offset_[5], length_[5], shops_mem_, sizeof(ShopSave));

    File::writeFile(filenamer, rgrp.data(), offset_.back());

    auto submap_count = submap_infos_mem_.size();

    std::vector<char> sdata(submap_count * sdata_length_);
    std::vector<char> ddata(submap_count * ddata_length_);
    for (int i = 0; i < submap_count; i++)
    {
        memcpy(sdata.data() + sdata_length_ * i, &(submap_infos_mem_[i].LayerData(0, 0, 0)), sdata_length_);
        memcpy(ddata.data() + ddata_length_ * i, submap_infos_mem_[i].Event(0), ddata_length_);
    }
    File::writeFile(filenames, sdata.data(), submap_count * sdata_length_);
    File::writeFile(filenamed, ddata.data(), submap_count * ddata_length_);

    return true;
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

    //�������ģ�����ʹ��
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
