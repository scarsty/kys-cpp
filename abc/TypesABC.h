#pragma once
#include <cstdint>
#include <string>

typedef int16_t MAP_INT;
typedef uint16_t SAVE_UINT;

enum
{
    SUBMAP_COORD_COUNT = 64,
    SUBMAP_LAYER_COUNT = 6,
    MAINMAP_COORD_COUNT = 480,
    SUBMAP_EVENT_COUNT = 200,    //单场景最大事件数
    ITEM_IN_BAG_COUNT = 1000,    //最大物品数
    TEAMMATE_COUNT = 6,          //最大队伍人员数
};

enum
{
    ROLE_MAGIC_COUNT = 10,
    ROLE_TAKING_ITEM_COUNT = 4,

    MAX_MAGIC_LEVEL = 999,
    MAX_MAGIC_LEVEL_INDEX = 9,

    ROLE_INTERNAL_COUNT = 4,
};

enum
{
    SHOP_ITEM_COUNT = 5,
};

struct ItemList16
{
    MAP_INT item_id = -1, count = 0;
};

struct BaseInfo16
{
    //此处为全局数据，载入和保存使用，必须放在类开头，按照顺序，否则自己看着办
    MAP_INT InShip, InSubMap, MainMapX, MainMapY, SubMapX, SubMapY, FaceTowards, ShipX, ShipY, ShipX1, ShipY1, Encode;
    MAP_INT Team[TEAMMATE_COUNT];
    ItemList16 Items[ITEM_IN_BAG_COUNT];
};

struct ItemList
{
    int item_id = -1, count = 0;
};

struct BaseInfo
{
    //此处为全局数据，载入和保存使用，必须放在类开头，按照顺序，否则自己看着办
    int InShip, InSubMap, MainMapX, MainMapY, SubMapX, SubMapY, FaceTowards, ShipX, ShipY, ShipX1, ShipY1, Encode;
    int Team[TEAMMATE_COUNT];
    ItemList Items[ITEM_IN_BAG_COUNT];
};

//成员函数若是开头大写，并且无下划线，则可以直接访问并修改

//存档中的角色数据
struct RoleSave16
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
struct ItemSave16
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
struct MagicSave16
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
struct SubMapInfoSave16
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

//存档中的商店数据
struct ShopSave16
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
    //int Frame[15];    //动作帧数，改为不在此处保存，故实际无用，另外延迟帧数对效果几乎无影响，废弃
    int EquipMagic[4];     //装备武学
    int EquipMagic2[4];    //装备被动武学
    int EquipItem;         //装备物品
    int Frame[6];          //帧数，现仅用于占位
    int MPType, MP, MaxMP;
    int Attack, Speed, Defence, Medicine, UsePoison, Detoxification, AntiPoison, Fist, Sword, Knife, Unusual, HiddenWeapon;
    int Knowledge, Morality, AttackWithPoison, AttackTwice, Fame, IQ;
    int PracticeItem;
    int ExpForItem;
    int MagicID[ROLE_MAGIC_COUNT], MagicLevel[ROLE_MAGIC_COUNT];
    int TakingItem[ROLE_TAKING_ITEM_COUNT], TakingItemCount[ROLE_TAKING_ITEM_COUNT];
    int InternalID[ROLE_INTERNAL_COUNT], InternalLevel[ROLE_INTERNAL_COUNT];
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

//存档中的商店数据
struct ShopSave
{
    int ItemID[SHOP_ITEM_COUNT], Total[SHOP_ITEM_COUNT], Price[SHOP_ITEM_COUNT];
};