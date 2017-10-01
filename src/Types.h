#pragma once
#include <cstdint>

enum
{
    SUBMAP_COORD_COUNT = 64,
    MAINMAP_COORD_COUNT = 480,
    SUBMAP_EVENT_COUNT = 200,                         //单场景最大事件数
    ITEM_IN_BAG_COUNT = 200,                           //最大物品数
    TEAMMATE_COUNT = 6,                         //最大队伍人员数
    ROLE_MAGIC_COUNT = 10,                         //最大队伍人员数
    ROLE_TAKING_ITEM_COUNT = 4,

};

enum
{
    MAX_LEVEL = 30,
    MAX_MP = 999,
    MAX_HP = 999,
    MAX_PHYSICAL_POWER = 100,

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
    MAX_APTITUDE = 100,
};


struct Role;
struct Item;
struct Magic;
struct SubMapRecord;
struct Shop;

//成员函数若是开头大写，并且无下划线，则可以直接访问并修改

//Role的说明：性别 0-男 1 女 2 其他
struct Role
{
    int16_t ID;
    int16_t HeadNum, IncLife, UnUse;
    char Name[10], Nick[10];
    int16_t Sexual, Level;
    uint16_t Exp;
    int16_t HP, MaxHP, Hurt, Poison, PhysicalPower;
    uint16_t ExpForItem;
    int16_t Equip1, Equip2;
    int16_t Frame[15];
    int16_t MPType, MP, MaxMP;
    int16_t Attack, Speed, Defence, Medcine, UsePoison, Detoxification, AntiPoison, Fist, Sword, Knife, Unusual, HiddenWeapon;
    int16_t Knowledge, Morality, AttackWithPoison, AttackTwice, Fame, IQ;
    int16_t PracticeBook;
    uint16_t ExpForBook;
    int16_t MagicID[ROLE_MAGIC_COUNT], MagicLevel[ROLE_MAGIC_COUNT];
    int16_t TakingItem[ROLE_TAKING_ITEM_COUNT], TakingItemCount[ROLE_TAKING_ITEM_COUNT];
    Magic* getLearnedMagic(int i);
    int getLearnedMagicLevel(int i);
    void limit();
};

struct Item
{
    int16_t ID;
    char Name[20], Name1[20];
    char Introduction[30];
    int16_t MagicID, AmiNum, User, EquipType, ShowIntro, ItemType, UnKnown5, UnKnown6, UnKnown7;
    int16_t AddHP, AddMaxHP, AddPoison, AddPhysicalPower, ChangeMPType, AddMP, AddMaxMP, AddAttack, AddSpeed;
    int16_t AddDefence, AddMedcine, AddUsePoi, AddMedPoison, AddAntiPoison;
    int16_t AddFist, AddSword, AddKnife, AddUnusual, AddHidWeapon, AddKnowledge, AddMorality, AddAttackTwice, AddAttackWithPoison;
    int16_t OnlyPracRole, NeedMPType, NeedMP, NeedAttack, NeedSpeed, NeedUsePoi, NeedMedcine, NeedMedPoi;
    int16_t NeedFist, NeedSword, NeedKnife, NeedUnusual, NeedHiddenWeapon, NeedIQ;
    int16_t NeedExp, NeedExpForItem, NeedMaterial;
    int16_t NeedItem[5], NeedItemAmount[5];
};

struct Magic
{
    int16_t ID;
    char Name[10];
    int16_t Unknown[5];
    int16_t SoundNum, MagicType, AmiNum, HurtType, AttackAreaType, NeedMP, WithPoison;
    int16_t Attack[10], MoveDistance[10], AttackDistance[10], AddMP[10], HurtMP[10];
};

struct SubMapLayerData
{
    int16_t Data[6][SUBMAP_COORD_COUNT * SUBMAP_COORD_COUNT];
};

//event1为主动触发，event2为物品触发，event3为经过触发
struct SubMapEvent
{
    int16_t CannotWalk, Index, Event1, Event2, Event3, CurrentPic, EndPic, BeginPic, PicDelay;
private:
    int16_t X_, Y_;
public:
    int16_t X() { return X_; }
    int16_t Y() { return Y_; }
    void setPosition(int x, int y, SubMapRecord* submap_record);
};

struct SubMapRecord
{
    int16_t ID;
    char Name[10];
    int16_t ExitMusic, EntranceMusic;
    int16_t JumpScence, EntranceCondition;
    int16_t MainEntranceX1, MainEntranceY1, MainEntranceX2, MainEntranceY2;
    int16_t EntranceX, EntranceY;
    int16_t ExitX[3], ExitY[3];
    int16_t JumpX1, JumpY1, JumpX2, JumpY2;

    int16_t& Earth(int x, int y);
    int16_t& Building(int x, int y);
    int16_t& Decoration(int x, int y);
    int16_t& EventIndex(int x, int y);
    int16_t& BuildingHeight(int x, int y);
    int16_t& DecorationHeight(int x, int y);
    SubMapEvent* Event(int x, int y);
    SubMapEvent* Event(int i);
    int16_t& LayerData(int layer, int x, int y);
};

struct Shop
{
    int16_t Item[5], Amount[5], Price[5];
};
