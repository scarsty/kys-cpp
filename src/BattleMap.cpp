#pragma once
#include "BattleMap.h"
#include "File.h"
#include "PotConv.h"

BattleMap BattleMap::battle_data_;

BattleMap::BattleMap()
{
    File::readFileToVector("../game/resource/war.sta", battle_infos_);
    File::readFileToVector("../game/resource/warfld.grp", battle_field_data2_);
    for (auto& i : battle_infos_)
    {
        PotConv::fromCP950ToCP936(i.Name);
        std::string s = i.Name;
    }
}

BattleMap::~BattleMap()
{
}

void BattleMap::copyLayerData(int battle_field_id, int layer, MapSquare* out)
{
    auto layer_data = battle_field_data2_[battle_field_id];
    memcpy(&out->data(0), &(layer_data.data[layer][0]), sizeof(SAVE_INT) * BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT);
}


