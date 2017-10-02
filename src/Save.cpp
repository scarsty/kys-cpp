#include "Save.h"
#include <string>
#include <iostream>
#include <fstream>
#include "File.h"
#include "PotConv.h"
#include "others/libconvert.h"

Save Save::save_;

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
    }
    else
    {
        if (c == 'r')
        {
            filename = "../game/save/ranger.grp";
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

bool Save::LoadR(int num)
{
    std::string filenamer = getFilename(num, 'r');
    std::string filenames = getFilename(num, 's');
    std::string filenamed = getFilename(num, 'd');
    std::string filename_idx = "../game/save/ranger.idx";

    auto rgrp = getIdxContent(filename_idx, filenamer, &offset_, &length_);

    int c = 0;
    memcpy(&InShip, rgrp + offset_[c], length_[c]);

    File::readDataToVector(rgrp + offset_[1], length_[1], roles_, sizeof(RoleSave));
    File::readDataToVector(rgrp + offset_[2], length_[2], items_, sizeof(ItemSave));
    File::readDataToVector(rgrp + offset_[3], length_[3], submap_infos_, sizeof(SubMapInfoSave));
    File::readDataToVector(rgrp + offset_[4], length_[4], magics_, sizeof(MagicSave));
    File::readDataToVector(rgrp + offset_[5], length_[5], shops_, sizeof(ShopSave));

    delete[] rgrp;

    auto submap_count = submap_infos_.size();

    auto sdata_length = sizeof(SAVE_INT) * SUBMAP_LAYER_COUNT * SUBMAP_COORD_COUNT * SUBMAP_COORD_COUNT;
    auto sdata = new char[submap_count * sdata_length];
    File::readFile(filenames, sdata, submap_count * sdata_length);
    auto ddata_length = sizeof(SubMapEvent) * SUBMAP_EVENT_COUNT;
    auto ddata = new char[submap_count * sdata_length];
    File::readFile(filenamed, ddata, submap_count * ddata_length);

    for (int i = 0; i < submap_count; i++)
    {
        memcpy(&(submap_infos_[i].LayerData(0, 0, 0)), sdata + sdata_length * i, sdata_length);
        memcpy(submap_infos_[i].Event(0), ddata + ddata_length * i, ddata_length);
    }
    delete[] sdata;
    delete[] ddata;

    //内部编码为cp936
    if (Encode != 936)
    {
        Encode = 936;
        for (auto& i : roles_)
        {
            PotConv::fromCP950ToCP936(i.Name);
            PotConv::fromCP950ToCP936(i.Nick);
        }
        for (auto& i : items_)
        {
            PotConv::fromCP950ToCP936(i.Name);
            PotConv::fromCP950ToCP936(i.Introduction);
        }
        for (auto& i : magics_)
        {
            PotConv::fromCP950ToCP936(i.Name);
        }
        for (auto& i : submap_infos_)
        {
            PotConv::fromCP950ToCP936(i.Name);
        }
    }

    makeMaps();

    return true;
}

bool Save::SaveR(int num)
{
    std::string filenamer = getFilename(num, 'r');
    std::string filenames = getFilename(num, 's');
    std::string filenamed = getFilename(num, 'd');

    char* rgrp = new char[offset_.back()];
    memcpy(rgrp + offset_[0], &InShip, length_[0]);
    File::writeVectorToData(rgrp + offset_[1], length_[1], roles_, sizeof(RoleSave));
    File::writeVectorToData(rgrp + offset_[2], length_[2], items_, sizeof(ItemSave));
    File::writeVectorToData(rgrp + offset_[3], length_[3], submap_infos_, sizeof(SubMapInfoSave));
    File::writeVectorToData(rgrp + offset_[4], length_[4], magics_, sizeof(MagicSave));
    File::writeVectorToData(rgrp + offset_[5], length_[5], shops_, sizeof(ShopSave));

    File::writeFile(filenamer, rgrp, offset_.back());
    delete[] rgrp;

    auto submap_count = submap_infos_.size();
    //File::writeFile(filenames, &submap_data_[0], submap_count * sizeof(SubMapLayerData));
    //File::writeFile(filenamed, &submap_event_[0], submap_count * SUBMAP_EVENT_COUNT * sizeof(SubMapEvent));

    return true;
}

Role* Save::getTeamMate(int i)
{
    if (i < 0 || i >= TEAMMATE_COUNT)
    {
        return nullptr;
    }
    int r = Team[i];
    if (r < 0 || r >= roles_.size())
    {
        return nullptr;
    }
    return &(roles_[r]);
}

Item* Save::getItemByBagIndex(int i)
{
    if (i < 0 || i >= ITEM_IN_BAG_COUNT)
    {
        return nullptr;
    }
    int r = Items[i].item_id;
    if (r < 0 || r >= items_.size())
    {
        return nullptr;
    }
    return &(items_[r]);
}

SAVE_INT Save::getItemCountByBagIndex(int i)
{
    return Items[i].count;
}

SAVE_INT Save::getItemCountInBag(Item* item)
{
    return getItemCountInBag(item->ID);
}

int Save::getItemCountInBag(int item_id)
{
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        auto id = Items[i].item_id;
        if (id < 0) { break; }
        if (id == item_id)
        {
            return Items[i].count;
        }
    }
    return 0;
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
        roles_by_name_[i.Name] = &i;
    }
    for (auto& i : magics_)
    {
        magics_by_name_[i.Name] = &i;
    }
    for (auto& i : items_)
    {
        items_by_name_[i.Name] = &i;
    }
    for (auto& i : submap_infos_)
    {
        submap_infos_by_name_[i.Name] = &i;
    }

}

char* Save::getIdxContent(std::string filename_idx, std::string filename_grp, std::vector<int>* offset, std::vector<int>* length)
{
    int* Ridx;
    int len = 0;
    File::readFile(filename_idx, (char**)&Ridx, &len);

    offset->resize(len / 4 + 1);
    length->resize(len / 4);
    offset->at(0) = 0;
    for (int i = 0; i < len / 4; i++)
    {
        (*offset)[i + 1] = Ridx[i];
        (*length)[i] = (*offset)[i + 1] - (*offset)[i];
    }
    int total_length = offset->back();
    delete[] Ridx;

    auto Rgrp = new char[total_length];
    File::readFile(filename_grp, Rgrp, total_length);
    return Rgrp;
}
