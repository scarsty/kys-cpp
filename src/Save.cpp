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

    auto Rgrp = getIdxContent(filename_idx, filenamer, &offset_, &length_);

    int c = 0;
    memcpy(&InShip, Rgrp + offset_[c], length_[c]);

    File::readDataToVector(Rgrp + offset_[1], length_[1], roles_);
    File::readDataToVector(Rgrp + offset_[2], length_[2], items_);
    File::readDataToVector(Rgrp + offset_[3], length_[3], submap_records_);
    File::readDataToVector(Rgrp + offset_[4], length_[4], magics_);
    File::readDataToVector(Rgrp + offset_[5], length_[5], shops_);

    delete[] Rgrp;

    auto submap_count = submap_records_.size();

    submap_data_.resize(submap_count);
    File::readFile(filenames, &submap_data_[0], submap_count * sizeof(SubMapLayerData));

    submap_event_.resize(submap_count * SUBMAP_EVENT_COUNT);
    File::readFile(filenamed, &submap_event_[0], submap_count * SUBMAP_EVENT_COUNT * sizeof(SubMapEvent));

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
        for (auto& i : submap_records_)
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

    char* Rgrp = new char[offset_.back()];
    int c = 0;
    memcpy(Rgrp + offset_[c], this, length_[c]);
    c = 1;
    memcpy(Rgrp + offset_[c], &roles_[0], length_[c]);
    c = 2;
    memcpy(Rgrp + offset_[c], &items_[0], length_[c]);
    c = 3;
    memcpy(Rgrp + offset_[c], &submap_records_[0], length_[c]);
    c = 4;
    memcpy(Rgrp + offset_[c], &magics_[0], length_[c]);
    c = 5;
    memcpy(Rgrp + offset_[c], &shops_[0], length_[c]);
    File::writeFile(filenamer, Rgrp, offset_.back());
    delete[] Rgrp;

    auto submap_count = submap_records_.size();
    File::writeFile(filenames, &submap_data_[0], submap_count * sizeof(SubMapLayerData));
    File::writeFile(filenamed, &submap_event_[0], submap_count * SUBMAP_EVENT_COUNT * sizeof(SubMapEvent));

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

int16_t Save::getItemCountByBagIndex(int i)
{
    return Items[i].count;
}

int16_t Save::getItemCountInBag(Item* item)
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
    submap_records_by_name_.clear();

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
    for (auto& i : submap_records_)
    {
        submap_records_by_name_[i.Name] = &i;
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
