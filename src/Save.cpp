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

char* Save::getIdxContent(std::string filename_idx, std::string filename_grp, std::vector<int>* offset, std::vector<int>* length)
{
    int* Ridx;
    int len = 0;
    File::readFile(filename_idx.c_str(), (char**)&Ridx, &len);

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
    File::readFile(filename_grp.c_str(), Rgrp, total_length);
    return Rgrp;
}

Save::Save()
{
}

Save::~Save()
{
}

bool Save::LoadR(int num)
{
    std::string filenamer, filenames, filenamed, filename_idx;
    int i;
    filenamer = "../game/save/r" + std::to_string(num) + ".grp";
    filenames = "../game/save/s" + std::to_string(num) + ".grp";
    filenamed = "../game/save/d" + std::to_string(num) + ".grp";
    if (num == 0)
    {
        filenamer = "../game/save/ranger.grp";
        filenames = "../game/save/s" + std::to_string(num) + ".grp";
        filenamed = "../game/save/d" + std::to_string(num) + ".grp";
    }
    filename_idx = "../game/save/ranger.idx";
    
    auto Rgrp = getIdxContent(filename_idx, filenamer, &offset_, &length_);
    memcpy(&global_data_, Rgrp + offset_[0], length_[0]);

    int c = 1;
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
    File::readFile(filenames.c_str(), &submap_data_[0], submap_count * sizeof(SubMapData));

    submap_array_.resize(submap_count);
    for (int i = 0; i < submap_count; i++)
    {
        submap_array_[i].Earth.setData(&submap_data_[i].data[0][0][0], SceneMaxX, SceneMaxY);
        submap_array_[i].Building.setData(&submap_data_[i].data[1][0][0], SceneMaxX, SceneMaxY);
        submap_array_[i].Decoration.setData(&submap_data_[i].data[2][0][0], SceneMaxX, SceneMaxY);
        submap_array_[i].EventID.setData(&submap_data_[i].data[3][0][0], SceneMaxX, SceneMaxY);
        submap_array_[i].BuildingHeight.setData(&submap_data_[i].data[4][0][0], SceneMaxX, SceneMaxY);
        submap_array_[i].EventHeight.setData(&submap_data_[i].data[5][0][0], SceneMaxX, SceneMaxY);
    }

    submap_event_.resize(submap_count * PerSceneMaxEvent);
    File::readFile(filenamed.c_str(), &submap_event_[0], submap_count * PerSceneMaxEvent * sizeof(SubMapEvent));

    return true;
}

bool Save::SaveR(int num)
{
    return true;
}


