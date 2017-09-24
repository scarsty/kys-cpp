#pragma once
#include <vector>
#include "File.h"

enum
{
    _Rtypecount = 10,
    SceneMaxX = 64,
    SceneMaxY = 64,
    PerSceneMaxEvent = 200,                          //单场景最大事件数
    MAXScene = 200,                                  //最大场景数
    MAX_ITEM_COUNT = 400,                           //最大物品数
    MaxRole = 1000,                                  //最大人物数
    Maxfaction = 200,                                //最大门派数
    MaxZhaoShi = 1000,                               //最大招式数
    MaxMagic = 1000,                                 //最大武功数
    MAX_TEAM_COUNT = 6,                               //最大队伍人员数
    MaxEquipNum = 5,                                 //最大装备数量
    MaxEventNum = 3000,                              //最大事件数
    MaxEventFactor = 50,                             //单事件最大参数数
    EventFolderNum = 1,                              //事件文件夹数
    EventFileNum = 1,                                //单事件文件夹事件文件数
    MaxMagicNum = 30
};

struct ItemList { int16_t item, count; };
//此处成员函数均大写，可以直接访问
struct GlobalData
{
    int16_t InShip, unused0, MainMapX, MainMapY, SubMapX, SubMapY, FaceTowards, ShipX, ShipY, ShipX1, ShipY1, ShipTowards;
    int16_t Team[MAX_TEAM_COUNT];
    ItemList ItemList[MAX_ITEM_COUNT];
    //short m_sDifficulty, m_sCheating, m_sBeiyong[4];
};

struct Role;
struct Item;
struct Magic;
struct SubMapRecord;
struct SubMapData;
struct SubMapArray;
struct Shop;
struct SubMapEvent;

class Save
{
public:
    Save();
    ~Save();

    bool LoadR(int num);
    bool SaveR(int num);

    static Save save_;
    static Save* getInstance()
    {
        return &save_;
    }

    std::vector<int> offset_, length_;

    GlobalData global_data_;
    std::vector<Role> roles_;
    std::vector<Magic> magics_;
    std::vector<Item> items_;
    std::vector<SubMapRecord> submap_records_;
    std::vector<SubMapData> submap_data_;
    std::vector<SubMapArray> submap_array_;
    std::vector<Shop> shops_;
    std::vector<SubMapEvent> submap_event_;

    GlobalData* getGlobalData() { return &global_data_; }

    Role* getRole(int i) { return &roles_[i]; }
    Magic* getMagic(int i) { return &magics_[i]; }
    Item* getItem(int i) { return &items_[i]; }
    SubMapRecord* getSubMapRecord(int i) { return &submap_records_[i]; }

    static void fromCP950ToCP936(char* s);
    static char* getIdxContent(std::string filename_idx, std::string filename_grp, std::vector<int>* offset, std::vector<int>* length);

};

struct Role
{
    int16_t ID;
    int16_t HeadNum, IncLife, UnUse;
    char Name[10], Nick[10];
    int16_t Sexual, Level;
    uint16_t Exp;
    int16_t CurrentHP, MaxHP, Hurt, Poison, PhyPower;
    uint16_t ExpForItem;
    int16_t Equip1, Equip2;
    int16_t Frame[15];
    int16_t MPType, CurrentMP, MaxMP;
    int16_t Attack, Speed, Defence, Medcine, UsePoi, MedPoi, DefPoi, Fist, Sword, Knife, Unusual, HidWeapon;
    int16_t  Knowledge, Ethics, AttPoi, AttTwice, Repute, Aptitude, PracticeBook;
    uint16_t ExpForBook;
    int16_t MagicID[10], MagicLevel[10];
    int16_t TakingItem[4], TakingItemAmount[4];
    Magic* getLearnedMagic(int i) { return Save::getInstance()->getMagic(MagicID[i]); }
};

struct Item
{
    int16_t ID;
    char Name[20], Name1[20];
    char Introduction[30];
    int16_t MagicID, AmiNum, User, EquipType, ShowIntro, ItemType, UnKnown5, UnKnown6, UnKnown7;
    int16_t AddCurrentHP, AddMaxHP, AddPoi, AddPhyPower, ChangeMPType, AddCurrentMP, AddMaxMP, AddAttack, AddSpeed;
    int16_t AddDefence, AddMedcine, AddUsePoi, AddMedPoison, AddDefPoi;
    int16_t AddFist, AddSword, AddKnife, AddUnusual, AddHidWeapon, AddKnowledge, AddEthics, AddAttTwice, AddAttPoi;
    int16_t OnlyPracRole, NeedMPType, NeedMP, NeedAttack, NeedSpeed, NeedUsePoi, NeedMedcine, NeedMedPoi;
    int16_t NeedFist, NeedSword, NeedKnife, NeedUnusual, NeedHidWeapon, NeedAptitude;
    int16_t NeedExp, NeedExpForItem, NeedMaterial;
    int16_t NeedItem[5], NeedItemAmount[5];
};

struct Magic
{
    int16_t ID;
    char Name[10];
    int16_t Unknown[5];
    int16_t SoundNum, MagicType, AmiNum, HurtType, AttAreaType, NeedMP, Poison;
    int16_t Attack[10], MoveDistance[10], AttDistance[10], AddMP[10], HurtMP[10];
};

struct MapArray
{
private:
    int16_t* data_ = nullptr;
    int x_max_ = 0, y_max_ = 0;
    int self_ = 0;
public:
    void divide2() { for (int i = 0; i < x_max_ * y_max_; i++) { data_[i] /= 2; } }
    int16_t& data(int x = 0, int y = 0) { return data_[x + x_max_ * y]; }
    void setData(int16_t* d, int x, int y) { data_ = d; x_max_ = x; y_max_ = y; }
public:
    MapArray() {}
    MapArray(int x, int y) { data_ = new int16_t[x * y]; x_max_ = x; y_max_ = y; self_ = 1; }
    MapArray(int16_t* d, int x, int y) { setData(d, x, y); }
    ~MapArray() { if (self_ && data_) { delete[] data_; } }
};

struct SubMapArray
{
    MapArray Earth;
    MapArray Building;
    MapArray Decoration;
    MapArray EventID;
    MapArray BuildingHeight;
    MapArray EventHeight;
};

struct SubMapRecord
{
    int16_t ID;
    char Name[10];
    int16_t ExitMusic, EntranceMusic;
    int16_t JumpScence, EnCondition;
    int16_t MainEntranceX1, MainEntranceY1, MainEntranceX2, MainEntranceY2;
    int16_t EntranceX, EntranceY;
    int16_t ExitX[3], ExitY[3];
    int16_t JumpX1, JumpY1, JumpX2, JumpY2;
    MapArray* Earth()
    {
        return &(Save::getInstance()->submap_array_[ID].Earth);
    }
    MapArray* Building()
    {
        return &(Save::getInstance()->submap_array_[ID].Building);
    }
    MapArray* Decoration()
    {
        return &(Save::getInstance()->submap_array_[ID].Decoration);
    }
    MapArray* EventID()
    {
        return &(Save::getInstance()->submap_array_[ID].EventID);
    }
    MapArray* BuildingHeight()
    {
        return &(Save::getInstance()->submap_array_[ID].BuildingHeight);
    }
    MapArray* EventHeight()
    {
        return &(Save::getInstance()->submap_array_[ID].EventHeight);
    }

    int16_t& Earth(int x, int y)
    {
        return Save::getInstance()->submap_array_[ID].Earth.data(x, y);
    }
    int16_t& Building(int x, int y)
    {
        return Save::getInstance()->submap_array_[ID].Building.data(x, y);
    }
    int16_t& Decoration(int x, int y)
    {
        return Save::getInstance()->submap_array_[ID].Decoration.data(x, y);
    }
    int16_t& EventID(int x, int y)
    {
        return Save::getInstance()->submap_array_[ID].EventID.data(x, y);
    }
    int16_t& BuildingHeight(int x, int y)
    {
        return Save::getInstance()->submap_array_[ID].BuildingHeight.data(x, y);
    }
    int16_t& EventHeight(int x, int y)
    {
        return Save::getInstance()->submap_array_[ID].EventHeight.data(x, y);
    }

    SubMapEvent* Event(int x, int y)
    {
        int i = EventID(x,y);       
        return Event(i);
    }

    SubMapEvent* Event(int i)
    {
        if (i < 0 || i >= PerSceneMaxEvent)
        {
            return nullptr;
        }
        return &(Save::getInstance()->submap_event_[PerSceneMaxEvent * ID + i]);
    }
};

struct SubMapData
{
    int16_t data[6][SceneMaxX][SceneMaxY];
};

struct SubMapEvent
{
    int16_t CannotWalk, ID, Event1, Event2, Event3, BeginPic1, EndPic, BeginPic2, PicDelay, XPos, YPos;
};

struct Shop
{
    int16_t Item[5], Amount[5], Price[5];
};

