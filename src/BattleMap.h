#pragma once
#include <vector>
#include "Types.h"

//这个仅保存战场前两层
struct BattleFieldData2
{
    MAP_INT data[BATTLEMAP_SAVE_LAYER_COUNT][BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT];
};

//这个类用来初始化记录，没别的用
class BattleMap
{
private:
    std::vector<BattleInfo> battle_infos_;
    std::vector<BattleFieldData2> battle_field_data2_;
public:
    BattleMap();
    ~BattleMap();

    static BattleMap* getInstance() 
    {
        static BattleMap bm;
        return &bm;
    }
    BattleInfo* getBattleInfo(int i);
    void copyLayerData(int battle_field_id, int layer, MapSquareInt* out);

};

