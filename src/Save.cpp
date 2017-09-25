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
    memcpy(&global_data_, Rgrp + offset_[c], length_[c]);

    c = 1;
    roles_.resize(length_[c] / sizeof(Role));
    memcpy(&roles_[0], Rgrp + offset_[c], length_[c]);
    for (auto& r : roles_)
    {
        fromCP950ToCP936(r.Name);
        fromCP950ToCP936(r.Nick);
    }

    c = 2;
    items_.resize(length_[c] / sizeof(Item));
    memcpy(&items_[0], Rgrp + offset_[c], length_[c]);
    for (auto& i : items_)
    {
        fromCP950ToCP936(i.Name);
        fromCP950ToCP936(i.Introduction);
    }

    c = 3;
    submap_records_.resize(length_[c] / sizeof(SubMapRecord));
    memcpy(&submap_records_[0], Rgrp + offset_[c], length_[c]);
    for (auto& s : submap_records_)
    {
        fromCP950ToCP936(s.Name);
    }

    c = 4;
    magics_.resize(length_[c] / sizeof(Magic));
    memcpy(&magics_[0], Rgrp + offset_[c], length_[c]);
    for (auto& m : magics_)
    {
        fromCP950ToCP936(m.Name);
    }

    c = 5;
    shops_.resize(length_[c] / sizeof(Shop));
    memcpy(&shops_[0], Rgrp + offset_[c], length_[c]);
    delete[] Rgrp;

    auto submap_count = submap_records_.size();

    submap_data_.resize(submap_count);
    File::readFile(filenames, &submap_data_[0], submap_count * sizeof(SubMapData));

    submap_event_.resize(submap_count * SUBMAP_MAX_EVENT);
    File::readFile(filenamed, &submap_event_[0], submap_count * SUBMAP_MAX_EVENT * sizeof(SubMapEvent));

    //makeMaps();

    return true;
}

bool Save::SaveR(int num)
{
    std::string filenamer = getFilename(num, 'r');
    std::string filenames = getFilename(num, 's');
    std::string filenamed = getFilename(num, 'd');

    char* Rgrp = new char[offset_.back()];
    int c = 0;
    memcpy(Rgrp + offset_[c], &global_data_, length_[c]);
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
    File::writeFile(filenames, &submap_data_[0], submap_count * sizeof(SubMapData));
    File::writeFile(filenamed, &submap_event_[0], submap_count * SUBMAP_MAX_EVENT * sizeof(SubMapEvent));

    return true;
}

Role* Save::getTeamMate(int i)
{
    if (i < 0 || i >= MAX_TEAMMATE_COUNT)
    {
        return nullptr;
    }
    int r = global_data_.Team[i];
    if (r < 0 || r >= roles_.size())
    {
        return nullptr;
    }
    return &(roles_[r]);
}

Item* Save::getItemFromBag(int i)
{
    if (i < 0 || i >= MAX_ITEM_COUNT)
    {
        return nullptr;
    }
    int r = global_data_.ItemList[i].item;
    if (r < 0 || r >= items_.size())
    {
        return nullptr;
    }
    return &(items_[r]);
}

int16_t Save::getItemCountFromBag(int i)
{
    return global_data_.ItemList[i].count;
}

int16_t Save::getItemCountFromBag(Item* item)
{
    for (int i = 0; i < MAX_ITEM_COUNT; i++)
    {
        auto id = global_data_.ItemList[i].item;
        if (id < 0) { break; }
        if (id == item->ID)
        {
            return global_data_.ItemList[i].count;
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

    //有重名的
    for (auto& r : roles_)
    {
        roles_by_name_[r.Name] = &r;
    }

}

void Save::fromCP950ToCP936(char* s)
{
    auto str = PotConv::cp950tocp936(s);
    memcpy(s, str.data(), str.length());
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
