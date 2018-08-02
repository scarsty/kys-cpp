#pragma once
#include "Engine.h"
#include <cstdint>
#include <string>

typedef int16_t MAP_INT;

template <typename T>
struct MapSquare
{
    MapSquare() {}
    MapSquare(int size)
        : MapSquare()
    {
        resize(size);
    }
    ~MapSquare()
    {
        if (data_)
        {
            delete data_;
        }
    }
    //���ᱣ��ԭʼ����
    void resize(int x)
    {
        if (data_)
        {
            delete data_;
        }
        data_ = new T[x * x];
        line_ = x;
    }

    T& data(int x, int y) { return data_[x + line_ * y]; }
    T& data(int x) { return data_[x]; }
    int size() { return line_; }
    int squareSize() { return line_ * line_; }
    void setAll(T v)
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
    T* data_ = nullptr;
    int line_ = 0;
};

typedef MapSquare<MAP_INT> MapSquareInt;

//ǰ������
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
    SUBMAP_EVENT_COUNT = 200,    //����������¼���
    ITEM_IN_BAG_COUNT = 200,     //�����Ʒ��
    TEAMMATE_COUNT = 6,          //��������Ա��
};

enum
{
    ROLE_MAGIC_COUNT = 10,
    ROLE_TAKING_ITEM_COUNT = 4,

    MAX_MAGIC_LEVEL = 999,
    MAX_MAGIC_LEVEL_INDEX = 9,
};

enum
{
    SHOP_ITEM_COUNT = 5,
};

//��Ա�������ǿ�ͷ��д���������»��ߣ������ֱ�ӷ��ʲ��޸�

//�浵�еĽ�ɫ����
struct RoleSave
{
public:
    int ID;
    int HeadID, IncLife, UnUse;
    char Name[20], Nick[20];
    int Sexual;    //�Ա� 0-�� 1 Ů 2 ����
    int Level;
    int Exp;
    int HP, MaxHP, Hurt, Poison, PhysicalPower;
    int ExpForMakeItem;
    int Equip0, Equip1;
    int Frame[15];    //����֡������Ϊ���ڴ˴����棬��ʵ�����ã������ӳ�֡����Ч��������Ӱ�죬����
    int MPType, MP, MaxMP;
    int Attack, Speed, Defence, Medicine, UsePoison, Detoxification, AntiPoison, Fist, Sword, Knife, Unusual, HiddenWeapon;
    int Knowledge, Morality, AttackWithPoison, AttackTwice, Fame, IQ;
    int PracticeItem;
    int ExpForItem;
    int MagicID[ROLE_MAGIC_COUNT], MagicLevel[ROLE_MAGIC_COUNT];
    int TakingItem[ROLE_TAKING_ITEM_COUNT], TakingItemCount[ROLE_TAKING_ITEM_COUNT];
};

//ʵ�ʵĽ�ɫ���ݣ�����֮���ͨ����ս������
struct Role : public RoleSave
{
public:
    int Team;
    int FaceTowards, Dead, Step;
    int Pic, BattleSpeed;
    int ExpGot, Auto;
    int FightFrame[5] = { -1 };
    int FightingFrame;
    int Moved, Acted;
    int ActTeam;    //ѡ���ж���Ӫ 0-�ҷ���1-���ҷ�����Ч����ʱ��Ч

    struct ShowString
    {
        std::string Text;
        BP_Color Color;
        int Size;
    };
    std::vector<ShowString> ShowStrings;

    int SelectedMagic;

    int Progress;

    int HPChange;
    int MPChange;
    int ProgressChange;

    int BattleHurt;
    int Eft;

private:
    int X_, Y_;
    int prevX_, prevY_;

public:
    MapSquare<Role*>* position_layer_ = nullptr;
    void setRolePositionLayer(MapSquare<Role*>* l) { position_layer_ = l; }
    void setPosition(int x, int y);
    void setPositionOnly(int x, int y)
    {
        X_ = x;
        Y_ = y;
    }
    void setPrevPosition(int x, int y)
    {
        prevX_ = x;
        prevY_ = y;
    }
    void resetPosition() { setPosition(prevX_, prevY_); }
    int X() { return X_; }
    int Y() { return Y_; }

    //��role�ģ���ʾ����Ĳ����������书��
    int getRoleShowLearnedMagicLevel(int i);
    int getRoleMagicLevelIndex(int i);

    int getLearnedMagicCount();
    int getMagicLevelIndex(Magic* magic);
    int getMagicLevelIndex(int magic_id);
    int getMagicOfRoleIndex(Magic* magic);

    void limit();

    int learnMagic(Magic* magic);
    int learnMagic(int magic_id);

    bool isAuto() { return Auto != 0 || Team != 0; }

    void addShowString(std::string text, BP_Color color = { 255,255,255,255 }, int size = 28) { ShowStrings.push_back({ text,color, size }); }
    void clearShowStrings() { ShowStrings.clear(); }

public:
    int AI_Action = 0;
    int AI_MoveX, AI_MoveY;
    int AI_ActionX, AI_ActionY;
    Magic* AI_Magic = nullptr;
    Item* AI_Item = nullptr;

public:
    static Role* getMaxValue() { return &max_role_value_; }

private:
    static Role max_role_value_;
};

//�浵�е���Ʒ����
struct ItemSave
{
    int ID;
    char Name[40];
    int Name1[10];
    char Introduction[60];
    int MagicID, HiddenWeaponEffectID, User, EquipType, ShowIntroduction;
    int ItemType;    //0���飬1װ����2���ţ�3ҩƷ��4����
    int UnKnown5, UnKnown6, UnKnown7;
    int AddHP, AddMaxHP, AddPoison, AddPhysicalPower, ChangeMPType, AddMP, AddMaxMP;
    int AddAttack, AddSpeed, AddDefence, AddMedicine, AddUsePoison, AddDetoxification, AddAntiPoison;
    int AddFist, AddSword, AddKnife, AddUnusual, AddHiddenWeapon, AddKnowledge, AddMorality, AddAttackTwice, AddAttackWithPoison;
    int OnlySuitableRole, NeedMPType, NeedMP, NeedAttack, NeedSpeed, NeedUsePoison, NeedMedicine, NeedDetoxification;
    int NeedFist, NeedSword, NeedKnife, NeedUnusual, NeedHiddenWeapon, NeedIQ;
    int NeedExp, NeedExpForMakeItem, NeedMaterial;
    int MakeItem[5], MakeItemCount[5];
};

//ʵ�ʵ���Ʒ����
struct Item : ItemSave
{
public:
    static int MoneyItemID;
    static int CompassItemID;

public:
    bool isCompass();
};

//�浵�е���ѧ���ݣ����ʺ϶�Ӧ���룬��������С˵�е���ѧ����ħ����������ˣ�
struct MagicSave
{
    int ID;
    char Name[20];
    int Unknown[5];
    int SoundID;
    int MagicType;    //1-ȭ��2-����3-����4-����
    int EffectID;
    int HurtType;          //0-��ͨ��1-��ȡMP
    int AttackAreaType;    //0-�㣬1-�ߣ�2-ʮ�֣�3-��
    int NeedMP, WithPoison;
    int Attack[10], SelectDistance[10], AttackDistance[10], AddMP[10], HurtMP[10];
};

struct Magic : MagicSave
{
    int calNeedMP(int level_index) { return NeedMP * ((level_index + 2) / 2); }
    int calMaxLevelIndexByMP(int mp, int max_level);
};

//�浵�е��ӳ�������
//Լ����Scene��ʾ��Ϸ�����е�ĳ��Elementʵ������Map��ʾ�洢������
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

//�����¼�����
struct SubMapEvent
{
    //event1Ϊ����������event2Ϊ��Ʒ������event3Ϊ��������
    MAP_INT CannotWalk, Index, Event1, Event2, Event3, CurrentPic, EndPic, BeginPic, PicDelay;

private:
    MAP_INT X_, Y_;

public:
    MAP_INT X() { return X_; }
    MAP_INT Y() { return Y_; }
    void setPosition(int x, int y, SubMapInfo* submap_record);
    void setPic(int pic)
    {
        BeginPic = pic;
        CurrentPic = pic;
        EndPic = pic;
    }
};

//ʵ�ʵĳ�������
struct SubMapInfo : public SubMapInfoSave
{
public:
    MAP_INT& LayerData(int layer, int x, int y)
    {
        return layer_data_[layer][x + y * SUBMAP_COORD_COUNT];
    }

    MAP_INT& Earth(int x, int y)
    {
        return LayerData(0, x, y);
    }

    MAP_INT& Building(int x, int y)
    {
        return LayerData(1, x, y);
    }

    MAP_INT& Decoration(int x, int y)
    {
        return LayerData(2, x, y);
    }

    MAP_INT& EventIndex(int x, int y)
    {
        return LayerData(3, x, y);
    }

    MAP_INT& BuildingHeight(int x, int y)
    {
        return LayerData(4, x, y);
    }

    MAP_INT& DecorationHeight(int x, int y)
    {
        return LayerData(5, x, y);
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
    MAP_INT layer_data_[SUBMAP_LAYER_COUNT][SUBMAP_COORD_COUNT * SUBMAP_COORD_COUNT];
    SubMapEvent events_[SUBMAP_EVENT_COUNT];
};

//�浵�е��̵�����
struct ShopSave
{
    int ItemID[SHOP_ITEM_COUNT], Total[SHOP_ITEM_COUNT], Price[SHOP_ITEM_COUNT];
};

//ʵ���̵�����
struct Shop : ShopSave
{
};