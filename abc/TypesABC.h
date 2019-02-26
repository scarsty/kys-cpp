#pragma once
#include <cstdint>
#include <string>

typedef int16_t MAP_INT;
typedef uint16_t SAVE_UINT;

struct MapSquare
{
    MapSquare() {}
    MapSquare(int size) : MapSquare() { resize(size); }
    ~MapSquare()
    {
        if (data_)
        {
            delete data_;
        }
    }
    //不会保留原始数据
    void resize(int x)
    {
        if (data_)
        {
            delete data_;
        }
        data_ = new MAP_INT[x * x];
        line_ = x;
    }

    MAP_INT& data(int x, int y) { return data_[x + line_ * y]; }
    MAP_INT& data(int x) { return data_[x]; }
    int size() { return line_; }
    int squareSize() { return line_ * line_; }
    void setAll(int v)
    {
        for (int i = 0; i < squareSize(); i++)
        {
            data_[i] = v;
        }
    }
    void copyTo(MapSquare* ms)
    {
        for (int i = 0; i < squareSize(); i++)
        {
            ms->data_[i] = data_[i];
        }
    }
    void copyFrom(MapSquare* ms)
    {
        for (int i = 0; i < squareSize(); i++)
        {
            data_[i] = ms->data_[i];
        }
    }

private:
    MAP_INT* data_ = nullptr;
    MAP_INT line_ = 0;
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
    SUBMAP_EVENT_COUNT = 200,    //单场景最大事件数
    ITEM_IN_BAG_COUNT = 200,     //最大物品数
    TEAMMATE_COUNT = 6,          //最大队伍人员数
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

    MAX_MEDICINE = 100,
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
struct RoleSave1
{
public:
    MAP_INT ID;
    MAP_INT HeadID, IncLife, UnUse;
    char Name[10], Nick[10];
    MAP_INT Sexual;    //性别 0-男 1 女 2 其他
    MAP_INT Level;
    SAVE_UINT Exp;
    MAP_INT HP, MaxHP, Hurt, Poison, PhysicalPower;
    SAVE_UINT ExpForMakeItem;
    MAP_INT Equip0, Equip1;
    MAP_INT Frame[15];    //动作帧数，改为不在此处保存，故实际无用，另外延迟帧数对效果几乎无影响，废弃
    MAP_INT MPType, MP, MaxMP;
    MAP_INT Attack, Speed, Defence, Medicine, UsePoison, Detoxification, AntiPoison, Fist, Sword, Knife, Unusual, HiddenWeapon;
    MAP_INT Knowledge, Morality, AttackWithPoison, AttackTwice, Fame, IQ;
    MAP_INT PracticeItem;
    SAVE_UINT ExpForItem;
    MAP_INT MagicID[ROLE_MAGIC_COUNT], MagicLevel[ROLE_MAGIC_COUNT];
    MAP_INT TakingItem[ROLE_TAKING_ITEM_COUNT], TakingItemCount[ROLE_TAKING_ITEM_COUNT];
};

//存档中的物品数据
struct ItemSave1
{
    MAP_INT ID;
    char Name[20];
    MAP_INT Name1[10];
    char Introduction[30];
    MAP_INT MagicID, HiddenWeaponEffectID, User, EquipType, ShowIntroduction;
    MAP_INT ItemType;    //0剧情，1装备，2秘笈，3药品，4暗器
    MAP_INT UnKnown5, UnKnown6, UnKnown7;
    MAP_INT AddHP, AddMaxHP, AddPoison, AddPhysicalPower, ChangeMPType, AddMP, AddMaxMP;
    MAP_INT AddAttack, AddSpeed, AddDefence, AddMedicine, AddUsePoison, AddDetoxification, AddAntiPoison;
    MAP_INT AddFist, AddSword, AddKnife, AddUnusual, AddHiddenWeapon, AddKnowledge, AddMorality, AddAttackTwice, AddAttackWithPoison;
    MAP_INT OnlySuitableRole, NeedMPType, NeedMP, NeedAttack, NeedSpeed, NeedUsePoison, NeedMedicine, NeedDetoxification;
    MAP_INT NeedFist, NeedSword, NeedKnife, NeedUnusual, NeedHiddenWeapon, NeedIQ;
    MAP_INT NeedExp, NeedExpForMakeItem, NeedMaterial;
    MAP_INT MakeItem[5], MakeItemCount[5];
};

//存档中的武学数据（无适合对应翻译，而且武侠小说中的武学近于魔法，暂且如此）
struct MagicSave1
{
    MAP_INT ID;
    char Name[10];
    MAP_INT Unknown[5];
    MAP_INT SoundID;
    MAP_INT MagicType;    //1-拳，2-剑，3-刀，4-特殊
    MAP_INT EffectID;
    MAP_INT HurtType;          //0-普通，1-吸取MP
    MAP_INT AttackAreaType;    //0-点，1-线，2-十字，3-面
    MAP_INT NeedMP, WithPoison;
    MAP_INT Attack[10], SelectDistance[10], AttackDistance[10], AddMP[10], HurtMP[10];
};

//存档中的子场景数据
//约定：Scene表示游戏中运行的某个Element实例，而Map表示存储的数据
struct SubMapInfoSave1
{
    MAP_INT ID;
    char Name[10];
    MAP_INT ExitMusic, EntranceMusic;
    MAP_INT JumpSubMap, EntranceCondition;
    MAP_INT MainEntranceX1, MainEntranceY1, MainEntranceX2, MainEntranceY2;
    MAP_INT EntranceX, EntranceY;
    MAP_INT ExitX[3], ExitY[3];
    MAP_INT JumpX, JumpY, JumpReturnX, JumpReturnY;
};

//场景事件数据
struct SubMapEvent1
{
    //event1为主动触发，event2为物品触发，event3为经过触发
    MAP_INT CannotWalk, Index, Event1, Event2, Event3, CurrentPic, EndPic, BeginPic, PicDelay;
};

//实际的场景数据
struct SubMapInfo1 : public SubMapInfoSave1
{
private:
    MAP_INT layer_data_[SUBMAP_LAYER_COUNT][SUBMAP_COORD_COUNT * SUBMAP_COORD_COUNT];
    SubMapEvent1 events_[SUBMAP_EVENT_COUNT];
};

//存档中的商店数据
struct ShopSave1
{
    MAP_INT ItemID[SHOP_ITEM_COUNT], Total[SHOP_ITEM_COUNT], Price[SHOP_ITEM_COUNT];
};

//存档中的角色数据
struct RoleSave
{
public:
    int ID;
    int HeadID, IncLife, UnUse;
    char Name[20], Nick[20];
    int Sexual;    //性别 0-男 1 女 2 其他
    int Level;
    int Exp;
    int HP, MaxHP, Hurt, Poison, PhysicalPower;
    int ExpForMakeItem;
    int Equip0, Equip1;
    int Frame[15];    //动作帧数，改为不在此处保存，故实际无用，另外延迟帧数对效果几乎无影响，废弃
    int MPType, MP, MaxMP;
    int Attack, Speed, Defence, Medicine, UsePoison, Detoxification, AntiPoison, Fist, Sword, Knife, Unusual, HiddenWeapon;
    int Knowledge, Morality, AttackWithPoison, AttackTwice, Fame, IQ;
    int PracticeItem;
    int ExpForItem;
    int MagicID[ROLE_MAGIC_COUNT], MagicLevel[ROLE_MAGIC_COUNT];
    int TakingItem[ROLE_TAKING_ITEM_COUNT], TakingItemCount[ROLE_TAKING_ITEM_COUNT];
};

//存档中的物品数据
struct ItemSave
{
    int ID;
    char Name[40];
    int Name1[10];
    char Introduction[60];
    int MagicID, HiddenWeaponEffectID, User, EquipType, ShowIntroduction;
    int ItemType;    //0剧情，1装备，2秘笈，3药品，4暗器
    int UnKnown5, UnKnown6, UnKnown7;
    int AddHP, AddMaxHP, AddPoison, AddPhysicalPower, ChangeMPType, AddMP, AddMaxMP;
    int AddAttack, AddSpeed, AddDefence, AddMedicine, AddUsePoison, AddDetoxification, AddAntiPoison;
    int AddFist, AddSword, AddKnife, AddUnusual, AddHiddenWeapon, AddKnowledge, AddMorality, AddAttackTwice, AddAttackWithPoison;
    int OnlySuitableRole, NeedMPType, NeedMP, NeedAttack, NeedSpeed, NeedUsePoison, NeedMedicine, NeedDetoxification;
    int NeedFist, NeedSword, NeedKnife, NeedUnusual, NeedHiddenWeapon, NeedIQ;
    int NeedExp, NeedExpForMakeItem, NeedMaterial;
    int MakeItem[5], MakeItemCount[5];
};

//存档中的武学数据（无适合对应翻译，而且武侠小说中的武学近于魔法，暂且如此）
struct MagicSave
{
    int ID;
    char Name[20];
    int Unknown[5];
    int SoundID;
    int MagicType;    //1-拳，2-剑，3-刀，4-特殊
    int EffectID;
    int HurtType;          //0-普通，1-吸取MP
    int AttackAreaType;    //0-点，1-线，2-十字，3-面
    int NeedMP, WithPoison;
    int Attack[10], SelectDistance[10], AttackDistance[10], AddMP[10], HurtMP[10];
};

//存档中的子场景数据
//约定：Scene表示游戏中运行的某个Element实例，而Map表示存储的数据
struct SubMapInfoSave
{
    int ID;
    char Name[20];
    int ExitMusic, EntranceMusic;
    int JumpSubMap, EntranceCondition;
    int MainEntranceX1, MainEntranceY1, MainEntranceX2, MainEntranceY2;
    int EntranceX, EntranceY;
    int ExitX[3], ExitY[3];
    int JumpX, JumpY, JumpReturnX, JumpReturnY;
};

//场景事件数据
struct SubMapEvent
{
    //event1为主动触发，event2为物品触发，event3为经过触发
    int CannotWalk, Index, Event1, Event2, Event3, CurrentPic, EndPic, BeginPic, PicDelay;
};

//实际的场景数据
struct SubMapInfo : public SubMapInfoSave
{
private:
    int layer_data_[SUBMAP_LAYER_COUNT][SUBMAP_COORD_COUNT * SUBMAP_COORD_COUNT];
    SubMapEvent events_[SUBMAP_EVENT_COUNT];
};

//存档中的商店数据
struct ShopSave
{
    int ItemID[SHOP_ITEM_COUNT], Total[SHOP_ITEM_COUNT], Price[SHOP_ITEM_COUNT];
};
