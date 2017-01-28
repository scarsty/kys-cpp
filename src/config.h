#pragma once

struct config
{
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
        EventFileNum = 1                                 //单事件文件夹事件文件数
    };
};

struct SceneMapData
{
    short Data[config::SLayerCount][config::SceneMaxX][config::SceneMaxY];
};

struct SceneData
{
    short ListNum;
    char Name[10];
    short Placeholder1, ExitMusic, EntranceMusic, Pallet, EnterCondition; //10
    short MainEntranceY1, MainEntranceX1, MainEntranceY2, MainEntranceX2, EntranceY, EntranceX, ExitY[3], ExitX[3]; //22
    short MapMode, Unuse, Faction, IsBattle, MaxPraFieldAmount, PraFieldAmount, MaxStudyAmount, StudyAmount; //30
    short PraFieldX[5], PraFieldY[5], StudyX[5], StudyY[5]; //50
    short IsMeditation, MeditationX, MeditationY, IsAlchemy, AlchemyX, AlchemyY, IsFactory, FactoryX, FactoryY, FlagX, FlagY, AlchemyPro, MeditationPro, DefBuff, DefBuild; //65
    short AddResourse[10], Connection[10];
};

struct SceneEventData
{
    struct TSceneEvent
    {
        short CanWalk, Num, Event1, Event2, Event3, BeginPic1, EndPic, BeginPic2, PicDelay, XPos, YPos, StartTime, Duration, Interval, DoneTime,
              IsActive, OutTime, OutEvent;
    };
    TSceneEvent Data[config::PerSceneMaxEvent];
};

