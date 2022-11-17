#include "BattleMap.h"
#include "GameUtil.h"
#include "GrpIdxFile.h"
#include "PotConv.h"
#include "filefunc.h"

BattleMap::BattleMap()
{
    filefunc::readFileToVector(GameUtil::PATH() + "resource/war.sta", battle_infos_);

    //地图的长度不一致，故换方法读取
    std::vector<int> offset, length;
    auto battle_map = GrpIdxFile::getIdxContent(GameUtil::PATH() + "resource/warfld.idx", GameUtil::PATH() + "resource/warfld.grp", &offset, &length);
    battle_field_data2_.resize(length.size());
    for (int i = 0; i < battle_field_data2_.size(); i++)
    {
        memcpy(battle_field_data2_[i].data, battle_map.data() + offset[i], sizeof(BattleFieldData2));
    }
    //File::readFileToVector(GameUtil::PATH()+"resource/warfld.grp", battle_field_data2_);

    //std::string str;
    for (auto& i : battle_infos_)
    {
        std::string s = PotConv::cp950toutf8(i.Name);
        //str += s + "\n";
    }
    //File::writeFile("1.txt", (void*)str.c_str(), str.size());
}

BattleMap::~BattleMap()
{
}

void BattleMap::copyLayerData(int battle_field_id, int layer, MapSquareInt* out)
{
    auto layer_data = battle_field_data2_[battle_field_id];
    memcpy(&out->data(0), &(layer_data.data[layer][0]), sizeof(MAP_INT) * BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT);
}
