#pragma once
#include <vector>
#include "Types.h"

enum
{
    BATTLE_ROLE_COUNT = 4096,                       //战场最大人数
    BATTLEMAP_SAVE_LAYER_COUNT = 2,                        //数据文件存储地图数据层数
    BATTLEMAP_LAYER_COUNT = 8,                     //战场需要地图层数
    BATTLEMAP_COORD_COUNT = 64,                        //战场最大坐标
};

struct BattleInfo
{
    int16_t ID;
    char Name[10];
    int16_t BattleFieldID, Exp, Music;
    int16_t TeamMate[6], AutoTeamMate[6], TeamMateX[6], TeamMateY[6];
    int16_t Enemy[20], EnemyX[20], EnemyY[20];
};

//这个仅保存战场前两层
struct BattleFieldData2
{
    int16_t data[BATTLEMAP_SAVE_LAYER_COUNT][BATTLEMAP_COORD_COUNT*BATTLEMAP_COORD_COUNT];
};

//这个类用来初始化记录，没别的用
class BattleSave
{
private:
    std::vector<BattleInfo> battle_infos_;
    std::vector<BattleFieldData2> battle_field_data2_;
    static BattleSave battle_data_;
public:
    BattleSave();
    ~BattleSave();

    static BattleSave* getInstance() { return &battle_data_; }
    BattleInfo* getBattleInfo(int i) { if (i < 0 || i >= battle_infos_.size()) { return nullptr; } return &battle_infos_[i]; }
    void copyLayerData(int battle_field_id, int layer, int16_t* out);

};

