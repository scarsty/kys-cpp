#pragma once
#include <cstdint>
#include <string>
#include "Engine.h"

typedef int16_t SAVE_INT;
typedef uint16_t SAVE_UINT;

struct MapSquare
{
    MapSquare() {}
    MapSquare(int size) : MapSquare() { resize(size); }
    ~MapSquare() { if (data_) { delete data_; } }
    //不会保留原始数据
    void resize(int x)
    {
        if (data_) { delete data_; }
        data_ = new SAVE_INT[x * x];
        line_ = x;
    }

    SAVE_INT& data(int x, int y) { return data_[x + line_ * y]; }
    SAVE_INT& data(int x) { return data_[x]; }
    int size() { return line_; }
    int squareSize() { return line_ * line_; }
    void setAll(int v) { for (int i = 0; i < squareSize(); i++) { data_[i] = v; } }
    void copyTo(MapSquare* ms) { for (int i = 0; i < squareSize(); i++) { ms->data_[i] = data_[i]; } }
    void copyFrom(MapSquare* ms) { for (int i = 0; i < squareSize(); i++) { data_[i] = ms->data_[i]; } }
private:
    SAVE_INT* data_ = nullptr;
    SAVE_INT line_ = 0;
};

//前置声明
struct Role;
struct Item;
struct Magic;
struct SubMapInfo;
struct Shop;
class Save;

enum
{
    SUBMAP_COORD_COUNT = 64,
    SUBMAP_LAYER_COUNT = 6,
    MAINMAP_COORD_COUNT = 480,
    SUBMAP_EVENT_COUNT = 200,                         //单场景最大事件数
    ITEM_IN_BAG_COUNT = 200,                           //最大物品数
    TEAMMATE_COUNT = 6,                         //最大队伍人员数
};

enum
{
    ROLE_MAGIC_COUNT = 10,
    ROLE_TAKING_ITEM_COUNT = 4,

    MAX_LEVEL = 30,
    MAX_MP = 999,
    MAX_HP = 999,
    MAX_PHYSICAL_POWER = 100,

    MAX_POISON = 100,

    MAX_ATTACK = 100,
    MAX_DEFENCE = 100,
    MAX_SPEED = 100,

    MAX_MEDCINE = 100,
    MAX_USE_POISON = 100,
    MAX_DETOXIFICATION = 100,
    MAX_ANTI_POISON = 100,

    MAX_FIST = 100,
    MAX_SWORD = 100,
    MAX_KNIFE = 100,
    MAX_UNUSUAL = 100,
    MAX_HIDDEN_WEAPON = 100,

    MAX_KNOWLEDGE = 100,
    MAX_MORALITY = 100,
    MAX_ATTACK_WITH_POISON = 100,
    MAX_FAME = 999,
    MAX_IQ = 100,

    MAX_MAGIC_LEVEL = 999,
    MAX_MAGIC_LEVEL_INDEX = 9,

    MAX_EXP = 65535,
};

enum
{
    MONEY_ITEM_ID = 174,
    COMPASS_ITEM_ID = 182,
};

enum
{
    SHOP_ITEM_COUNT = 5,
};


//成员函数若是开头大写，并且无下划线，则可以直接访问并修改

//存档中的角色数据
struct RoleSave
{
public:
    SAVE_INT ID;
    SAVE_INT HeadID, IncLife, UnUse;
    char Name[10], Nick[10];
    SAVE_INT Sexual;  //性别 0-男 1 女 2 其他
    SAVE_INT Level;
    SAVE_UINT Exp;
    SAVE_INT HP, MaxHP, Hurt, Poison, PhysicalPower;
    SAVE_UINT ExpForMakeItem;
    SAVE_INT Equip0, Equip1;
    SAVE_INT Frame[15];    //动作帧数，改为不在此处保存，故实际无用，另外延迟帧数对效果几乎无影响，废弃
    SAVE_INT MPType, MP, MaxMP;
    SAVE_INT Attack, Speed, Defence, Medcine, UsePoison, Detoxification, AntiPoison, Fist, Sword, Knife, Unusual, HiddenWeapon;
    SAVE_INT Knowledge, Morality, AttackWithPoison, AttackTwice, Fame, IQ;
    SAVE_INT PracticeItem;
    SAVE_UINT ExpForItem;
    SAVE_INT MagicID[ROLE_MAGIC_COUNT], MagicLevel[ROLE_MAGIC_COUNT];
    SAVE_INT TakingItem[ROLE_TAKING_ITEM_COUNT], TakingItemCount[ROLE_TAKING_ITEM_COUNT];

};

//实际的角色数据，基类之外的通常是战斗属性
struct Role : public RoleSave
{
public:
    int Team;
    int FaceTowards, Dead, Step;
    int Pic, BattleSpeed;
    int ExpGot, Auto;
    int FightFrame[5];
    int FightingFrame;
    int Moved, Acted;
    int ActTeam;  //选择行动阵营 0-我方，1-非我方，画效果层时有效

    std::string ShowString;
    BP_Color ShowColor;

private:
    int X_, Y_;
    int prevX_, prevY_;
public:
    MapSquare* position_layer_ = nullptr;
    void setPoitionLayer(MapSquare* l) { position_layer_ = l; }
    void setPosition(int x, int y);
    void setPrevPosition(int x, int y) { prevX_ = x; prevY_ = y; }
    void resetPosition() { setPosition(prevX_, prevY_); }
    int X() { return X_; }
    int Y() { return Y_; }

    //带role的，表示后面的参数是人物武功栏
    int getRoleShowLearnedMagicLevel(int i);
    int getRoleMagicLevelIndex(int i);

    int getLearnedMagicCount();
    int getMagicLevelIndex(Magic* magic);
    int getMagicLevelIndex(int magic_id);

    void limit();

    int learnMagic(Magic* magic);
    int learnMagic(int magic_id);

public:
    int AI_Action = 0;
    int AI_MoveX, AI_MoveY;
    int AI_ActionX, AI_ActionY;
    Magic* AI_Magic = nullptr;
};

//存档中的物品数据
struct ItemSave
{
    SAVE_INT ID;
    char Name[20], Name1[20];
    char Introduction[30];
    SAVE_INT MagicID, HiddenWeaponEffectID, User, EquipType, ShowIntroduction;
    SAVE_INT ItemType;   //0剧情，1装备，2秘笈，3药品，4暗器
    SAVE_INT UnKnown5, UnKnown6, UnKnown7;
    SAVE_INT AddHP, AddMaxHP, AddPoison, AddPhysicalPower, ChangeMPType, AddMP, AddMaxMP;
    SAVE_INT AddAttack, AddSpeed, AddDefence, AddMedcine, AddUsePoison, AddDetoxification, AddAntiPoison;
    SAVE_INT AddFist, AddSword, AddKnife, AddUnusual, AddHiddenWeapon, AddKnowledge, AddMorality, AddAttackTwice, AddAttackWithPoison;
    SAVE_INT OnlySuitableRole, NeedMPType, NeedMP, NeedAttack, NeedSpeed, NeedUsePoison, NeedMedcine, NeedDetoxification;
    SAVE_INT NeedFist, NeedSword, NeedKnife, NeedUnusual, NeedHiddenWeapon, NeedIQ;
    SAVE_INT NeedExp, NeedExpForMakeItem, NeedMaterial;
    SAVE_INT MakeItem[5], MakeItemCount[5];
};

//实际的物品数据
struct Item : ItemSave
{
    bool isCompass() { return ID == COMPASS_ITEM_ID; }
};

//存档中的武学数据（无适合对应翻译，而且武侠小说中的武学近于魔法，暂且如此）
struct MagicSave
{
    SAVE_INT ID;
    char Name[10];
    SAVE_INT Unknown[5];
    SAVE_INT SoundID;
    SAVE_INT MagicType;  //1-拳，2-剑，3-刀，4-特殊
    SAVE_INT EffectID;
    SAVE_INT HurtType;  //0-普通，1-吸取MP
    SAVE_INT AttackAreaType;  //0-点，1-线，2-十字，3-面
    SAVE_INT NeedMP, WithPoison;
    SAVE_INT Attack[10], SelectDistance[10], AttackDistance[10], AddMP[10], HurtMP[10];
};

struct Magic : MagicSave
{
    int calNeedMP(int level_index) { return NeedMP*(level_index + 2) / 2; }
    int calMaxLevelIndexByMP(int mp, int max_level);
};

//存档中的子场景数据
//约定：Scene表示游戏中运行的某个Element实例，而Map表示存储的数据
struct SubMapInfoSave
{
    SAVE_INT ID;
    char Name[10];
    SAVE_INT ExitMusic, EntranceMusic;
    SAVE_INT JumpScence, EntranceCondition;
    SAVE_INT MainEntranceX1, MainEntranceY1, MainEntranceX2, MainEntranceY2;
    SAVE_INT EntranceX, EntranceY;
    SAVE_INT ExitX[3], ExitY[3];
    SAVE_INT JumpX1, JumpY1, JumpX2, JumpY2;
};

//场景事件数据
struct SubMapEvent
{
    //event1为主动触发，event2为物品触发，event3为经过触发
    SAVE_INT CannotWalk, Index, Event1, Event2, Event3, CurrentPic, EndPic, BeginPic, PicDelay;
private:
    SAVE_INT X_, Y_;
public:
    SAVE_INT X() { return X_; }
    SAVE_INT Y() { return Y_; }
    void setPosition(int x, int y, SubMapInfo* submap_record);
};

//实际的场景数据
struct SubMapInfo : public SubMapInfoSave
{
public:
    SAVE_INT& LayerData(int layer, int x, int y)
    {
        return layer_data_[layer][x + y * SUBMAP_COORD_COUNT];
    }

    SAVE_INT& Earth(int x, int y)
    {
        return LayerData(0, x, y);
    }

    SAVE_INT& Building(int x, int y)
    {
        return LayerData(1, x, y);;
    }

    SAVE_INT& Decoration(int x, int y)
    {
        return LayerData(2, x, y);;
    }

    SAVE_INT& EventIndex(int x, int y)
    {
        return LayerData(3, x, y);;
    }

    SAVE_INT& BuildingHeight(int x, int y)
    {
        return LayerData(4, x, y);;
    }

    SAVE_INT& DecorationHeight(int x, int y)
    {
        return LayerData(5, x, y);;
    }

    SubMapEvent* Event(int x, int y)
    {
        int i = EventIndex(x, y);
        return Event(i);
    }

    SubMapEvent* Event(int i)
    {
        if (i < 0 || i >= SUBMAP_EVENT_COUNT)
        {
            return nullptr;
        }
        return &events_[i];
    }

private:
    SAVE_INT layer_data_[SUBMAP_LAYER_COUNT][SUBMAP_COORD_COUNT * SUBMAP_COORD_COUNT];
    SubMapEvent events_[SUBMAP_EVENT_COUNT];
};

//存档中的商店数据
struct ShopSave
{
    SAVE_INT ItemID[SHOP_ITEM_COUNT], Total[SHOP_ITEM_COUNT], Price[SHOP_ITEM_COUNT];
};

//实际商店数据
struct Shop : ShopSave
{

};