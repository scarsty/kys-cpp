#pragma once
#include <vector>
#include "File.h"

enum
{
    _Rtypecount = 10,
    SLayerCount = 6,
    SceneMaxX = 64,
    SceneMaxY = 64,
    PerSceneMaxEvent = 400,                          //单场景最大事件数
    MAXScene = 200,                                  //最大场景数
    MAX_ITEM_AMOUNT = 400,                           //最大物品数
    MaxRole = 1000,                                  //最大人物数
    Maxfaction = 200,                                //最大门派数
    MaxZhaoShi = 1000,                               //最大招式数
    MaxMagic = 1000,                                 //最大武功数
    MaxTeamMember = 6,                               //最大队伍人员数
    MaxEquipNum = 5,                                 //最大装备数量
    MaxEventNum = 3000,                              //最大事件数
    MaxEventFactor = 50,                             //单事件最大参数数
    EventFolderNum = 1,                              //事件文件夹数
    EventFileNum = 1,                                //单事件文件夹事件文件数
    MaxMagicNum = 30
};


struct SceneMapData
{
	short Earth[SceneMaxX][SceneMaxY];
    short Earth[SceneMaxX][SceneMaxY];
    short Earth[SceneMaxX][SceneMaxY];
    short Earth[SceneMaxX][SceneMaxY];
    short Earth[SceneMaxX][SceneMaxY];
    short Earth[SceneMaxX][SceneMaxY];
};

struct TSceneEvent
{
	short CanWalk, Num, Event1, Event2, Event3, BeginPic1, EndPic, BeginPic2, PicDelay, XPos, YPos;
	short StartTime, Duration, Interval, DoneTime, IsActive, OutTime, OutEvent;
};

struct SceneEventData
{

	TSceneEvent Data[PerSceneMaxEvent];
};

class Save
{
public:
    Save();
    ~Save();

    int* Offset;
    unsigned char key = 156;
    unsigned char password;
    int Rgrplen;
    int Ridxlen = _Rtypecount * 4;
    unsigned char* GRPMD5_cal, *GRPMD5_load;
    int B_Count;                                        //基础数据数量
    int R_Count;                                        //角色数量
    int I_Count;                                        //物品数量
    int S_Count;                                        //场景数量
    int M_Count;                                        //武功数量
    int SP_Count;                                       //小宝商店数量
    int Z_Count;                                        //招式数量
    int F_Count;                                        //门派数量
    static const int R_length = 436;                    //角色数据长度

    bool LoadR(int num);
    bool SaveR(int num);

    static Save save;
    static Save* getInstance()
    {
        return &save;
    }

    void jiemi(unsigned char* Data, unsigned char key, int len);
    void encryption(std::string str, unsigned char key);

    //std::vector<BaoShop> m_Baoshop;
    //std::vector<BasicData> m_BasicData;
    //std::vector<Character> m_Character;
    //std::vector<Calendar> m_Calendar;
    //std::vector<Faction> m_Faction;
    //std::vector<Magic> m_Magic;
    //std::vector<SceneData> m_SceneData;
    //std::vector<ZhaoShi> m_ZhaoShi;
    //std::vector<Item> m_Item;
    std::vector<SceneMapData> m_SceneMapData;
    std::vector<SceneEventData> m_SceneEventData;
};

