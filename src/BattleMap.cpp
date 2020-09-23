#include "BattleMap.h"
#include "File.h"
#include "GrpIdxFile.h"
#include "PotConv.h"

BattleMap::BattleMap()
{
    File::readFileToVector("../game/resource/war.sta", battle_infos_);

    //地图的长度不一致，故换方法读取
    std::vector<int> offset, length;
    auto battle_map = GrpIdxFile::getIdxContent("../game/resource/warfld.idx", "../game/resource/warfld.grp", &offset, &length);
    battle_field_data2_.resize(length.size());
    for (int i = 0; i < battle_field_data2_.size(); i++)
    {
        memcpy(battle_field_data2_[i].data, battle_map.data() + offset[i], sizeof(BattleFieldData2));
    }
    //File::readFileToVector("../game/resource/warfld.grp", battle_field_data2_);

    for (auto& i : battle_infos_)
    {
        PotConv::fromCP950ToCP936(i.Name);
        std::string s = i.Name;
    }
}

BattleMap::~BattleMap()
{
}

void BattleMap::copyLayerData(int battle_field_id, int layer, MapSquareInt* out)
{
    auto layer_data = battle_field_data2_[battle_field_id];
    memcpy(&out->data(0), &(layer_data.data[layer][0]), sizeof(MAP_INT) * BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT);
}
