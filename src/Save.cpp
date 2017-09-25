#include "Save.h"
#include <string>
#include <iostream>
#include <fstream>
#include "File.h"
#include "PotConv.h"

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
        filename = "../game/save/" + c + std::to_string(i) + ".grp";
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

    //submap_array_.resize(submap_count);
    //for (int i = 0; i < submap_count; i++)
    //{
    //    submap_array_[i].Earth.setData(&submap_data_[i].data[0][0], SUBMAP_MAX_X, SUBMAP_MAX_Y);
    //    submap_array_[i].Building.setData(&submap_data_[i].data[1][0], SUBMAP_MAX_X, SUBMAP_MAX_Y);
    //    submap_array_[i].Decoration.setData(&submap_data_[i].data[2][0], SUBMAP_MAX_X, SUBMAP_MAX_Y);
    //    submap_array_[i].EventID.setData(&submap_data_[i].data[3][0], SUBMAP_MAX_X, SUBMAP_MAX_Y);
    //    submap_array_[i].BuildingHeight.setData(&submap_data_[i].data[4][0], SUBMAP_MAX_X, SUBMAP_MAX_Y);
    //    submap_array_[i].EventHeight.setData(&submap_data_[i].data[5][0], SUBMAP_MAX_X, SUBMAP_MAX_Y);
    //}

    submap_event_.resize(submap_count * SUBMAP_MAX_EVENT);
    File::readFile(filenamed, &submap_event_[0], submap_count * SUBMAP_MAX_EVENT * sizeof(SubMapEvent));

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
