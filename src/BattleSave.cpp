#pragma once
#include "BattleSave.h"
#include "File.h"
#include "PotConv.h"

BattleSave BattleSave::battle_data_;

BattleSave::BattleSave()
{
    File::readFileToVector("../game/resource/war.sta", battle_infos_);
    File::readFileToVector("../game/resource/warfld.grp", battle_field_data2_);
    for (auto& i : battle_infos_)
    {
        PotConv::fromCP950ToCP936(i.Name);
        std::string s = i.Name;
    }
}

BattleSave::~BattleSave()
{
}

void BattleSave::copyLayerData(int battle_field_id, int layer, int16_t* out)
{
    auto layer_data = battle_field_data2_[battle_field_id];
    memcpy(out, &(layer_data.data[layer][0]), sizeof(int16_t) * BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT);
}


