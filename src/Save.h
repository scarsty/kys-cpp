#pragma once
#include <vector>
#include "Types.h"
#include <map>

struct ItemList { int item_id, count; };

class Save
{
public:
    //�˴�Ϊȫ�����ݣ�����ͱ���ʹ�ã���������࿪ͷ������˳�򣬷����Լ����Ű�
    int InShip, InSubMap, MainMapX, MainMapY, SubMapX, SubMapY, FaceTowards, ShipX, ShipY, ShipX1, ShipY1, Encode;
    int Team[TEAMMATE_COUNT];
    ItemList Items[ITEM_IN_BAG_COUNT];
private:
    //��������������
    int buffer_[100];

    int sdata_length_ = sizeof(MAP_INT) * SUBMAP_LAYER_COUNT * SUBMAP_COORD_COUNT * SUBMAP_COORD_COUNT;
    int ddata_length_ = sizeof(SubMapEvent) * SUBMAP_EVENT_COUNT;

public:
    Save();
    ~Save();

    bool load(int num);
    void loadR(int num);
    void loadSD(int num);
    bool save(int num);
    void saveR(int num);
    void saveSD(int num);

    // �������罻��
    void resetRData(const std::vector<RoleSave>& newData);

    static Save* getInstance()
    {
        static Save s;
        return &s;
    }

    static std::string getFilename(int i, char c);
    static bool checkSaveFileExist(int num);
private:
    //ע���ڶ�ȡ֮��offset��lengthβ�����һ��Ԫ�أ���ֵ���ܳ���
    std::vector<int> offset_, length_;

    //����ʵ�ʱ�����������
    std::vector<Role> roles_mem_;
    std::vector<Magic> magics_mem_;
    std::vector<Item> items_mem_;
    std::vector<SubMapInfo> submap_infos_mem_;
    std::vector<Shop> shops_mem_;

    //���汣�����ָ�룬�󲿷�ʱ��ʹ��
    std::vector<Role*> roles_;
    std::vector<Magic*> magics_;
    std::vector<Item*> items_;
    std::vector<SubMapInfo*> submap_infos_;
    std::vector<Shop*> shops_;

    std::map<std::string, Role*> roles_by_name_;
    std::map<std::string, Item*> items_by_name_;
    std::map<std::string, Magic*> magics_by_name_;
    std::map<std::string, SubMapInfo*> submap_infos_by_name_;

    template <class T> void setSavePointer(std::vector<T>& v, int size)
    {
        for (auto& i : v)
        {
            i.save_ = this;
        }
    }

    template <class T> void toPtrVector(std::vector<T>& v, std::vector<T*>& v_ptr)
    {
        v_ptr.clear();
        for (auto& i : v)
        {
            v_ptr.push_back(&i);
        }
    }

    void updateAllPtrVector();

public:
    Role* getRole(int i) { if (i < 0 || i >= roles_.size()) { return nullptr; } return roles_[i]; }
    Magic* getMagic(int i) { if (i <= 0 || i >= magics_.size()) { return nullptr; } return magics_[i]; }  //0���书��Ч
    Item* getItem(int i) { if (i < 0 || i >= items_.size()) { return nullptr; } return items_[i]; }
    SubMapInfo* getSubMapInfo(int i) { if (i < 0 || i >= submap_infos_.size()) { return nullptr; } return submap_infos_[i]; }
    Shop* getShop(int i) { if (i < 0 || i >= shops_.size()) { return nullptr; } return shops_[i]; }

    Role* getTeamMate(int i);
    int getTeamMateID(int i) { return Team[i]; }

    Item* getItemByBagIndex(int i);
    int getItemCountByBagIndex(int i);
    int getItemCountInBag(Item* item);

    int getItemCountInBag(int item_id);
    int getMoneyCountInBag();

    void makeMaps();

    Role* getRoleByName(std::string name) { return roles_by_name_[name]; }
    Magic* getMagicByName(std::string name) { return magics_by_name_[name]; }
    Item* getItemByName(std::string name) { return items_by_name_[name]; }
    SubMapInfo* getSubMapRecordByName(std::string name) { return submap_infos_by_name_[name]; }

    Magic* getRoleLearnedMagic(Role* r, int i);
    int getRoleLearnedMagicLevelIndex(Role* r, Magic* m);

    const std::vector<Role*>& getRoles() { return roles_; }
    const std::vector<Magic*>& getMagics() { return magics_; }
    const std::vector<Item*>& getItems() { return items_; }
    const std::vector<SubMapInfo*>& getSubMapInfos() { return submap_infos_; }
    const std::vector<Shop*>& getShops() { return shops_; }

public:
    int MaxLevel = 30;
    int MaxHP = 999;
    int MaxMP = 999;
    int MaxPhysicalPower = 100;

    int MaxPosion = 100;

    int MaxAttack = 100;
    int MaxDefence = 100;
    int MaxSpeed = 100;

    int MaxMedicine = 100;
    int MaxUsePoison = 100;
    int MaxDetoxification = 100;
    int MaxAntiPoison = 100;

    int MaxFist = 100;
    int MaxSword = 100;
    int MaxKnife = 100;
    int MaxUnusual = 100;
    int MaxHiddenWeapon = 100;

    int MaxKnowledge = 100;
    int MaxMorality = 100;
    int MaxAttackWithPoison = 100;
    int MaxFame = 999;
    int MaxIQ = 100;

    int MaxExp = 99999;



    void loadSaveValues();


    // �±���ϵ��
private:
    // �������������Ȼ��һ�������ܷ�����������
    struct BaseInfo {
        int InShip, InSubMap, MainMapX, MainMapY, SubMapX, SubMapY, FaceTowards, ShipX, ShipY, ShipX1, ShipY1, Encode;
        int Team[TEAMMATE_COUNT];
    };

    class NewSave {
    public:
        static void Save::NewSave::SaveToCSVBaseInfo(BaseInfo* data, int length, int record);
        static void Save::NewSave::LoadFromCSVBaseInfo(BaseInfo* data, int length, int record);
        // ����
        static void Save::NewSave::SaveToCSVItemList(ItemList* data, int length, int record);
        static void Save::NewSave::LoadFromCSVItemList(ItemList* data, int length, int record);
        // ����
        static void Save::NewSave::SaveToCSVRoleSave(const std::vector<Role>& data, int record);
        static void Save::NewSave::LoadFromCSVRoleSave(std::vector<Role>& data, int record);
        static void Save::NewSave::InsertRoleAt(std::vector<Role>& data, int idx);
        // ��Ʒ
        static void Save::NewSave::SaveToCSVItemSave(const std::vector<Item>& data, int record);
        static void Save::NewSave::LoadFromCSVItemSave(std::vector<Item>& data, int record);
        static void Save::NewSave::InsertItemAt(std::vector<Item>& data, int idx);
        // ����
        static void Save::NewSave::SaveToCSVSubMapInfoSave(const std::vector<SubMapInfo>& data, int record);
        static void Save::NewSave::LoadFromCSVSubMapInfoSave(std::vector<SubMapInfo>& data, int record);
        static void Save::NewSave::InsertSubMapInfoAt(std::vector<SubMapInfo>& data, int idx);
        // �书
        static void Save::NewSave::SaveToCSVMagicSave(const std::vector<Magic>& data, int record);
        static void Save::NewSave::LoadFromCSVMagicSave(std::vector<Magic>& data, int record);
        static void Save::NewSave::InsertMagicAt(std::vector<Magic>& data, int idx);
        // �̵�
        static void Save::NewSave::SaveToCSVShopSave(const std::vector<Shop>& data, int record);
        static void Save::NewSave::LoadFromCSVShopSave(std::vector<Shop>& data, int record);
        static void Save::NewSave::InsertShopAt(std::vector<Shop>& data, int idx);
    };
public:
    void saveRToCSV(int num);
    void loadRFromCSV(int num);
    bool insertAt(const std::string& type, int idx);
};