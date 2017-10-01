#pragma once
#include "BattleSave.h"
#include "File.h"
#include "PotConv.h"

BattleSave BattleSave::battle_data_;

BattleSave::BattleSave()
{
    File::readFileToVector("../game/resource/war.sta", battle_infos_);
    File::readFileToVector("../game/resource/warfld.grp", battle_map_save_layer_data_);
    for (auto& i : battle_infos_)
    {
        PotConv::fromCP950ToCP936(i.Name);
        std::string s = i.Name;
    }
}


BattleSave::~BattleSave()
{
}



