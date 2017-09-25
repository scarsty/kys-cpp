#pragma once
#include <vector>
#include "Types.h"
#include <map>

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

    static std::string getFilename(int i, char c);

    //注意在读取之后，offset比length尾部会多一个元素，该值即总长度
    std::vector<int> offset_, length_;

    //这里实际保存所有数据
    GlobalData global_data_;
    std::vector<Role> roles_;
    std::vector<Magic> magics_;
    std::vector<Item> items_;
    std::vector<SubMapRecord> submap_records_;
    std::vector<SubMapData> submap_data_;
    std::vector<Shop> shops_;
    std::vector<SubMapEvent> submap_event_;

    std::map<std::string, Role*> roles_by_name_;
    std::map<std::string, Item*> items_by_name_;
    std::map<std::string, Magic*> magics_by_name_;
    std::map<std::string, SubMapRecord*> submap_records_by_name_;

    GlobalData* getGlobalData() { return &global_data_; }

    Role* getRole(int i) { return &roles_[i]; }
    Magic* getMagic(int i) { return &magics_[i]; }
    Item* getItem(int i) { return &items_[i]; }
    SubMapRecord* getSubMapRecord(int i) { return &submap_records_[i]; }

    Role* getTeamMate(int i);
    Item* getItemFromBag(int i);
    int16_t getItemCountFromBag(int i);
    int16_t getItemCountFromBag(Item* item);

    void makeMaps();

    static void fromCP950ToCP936(char* s);
    static char* getIdxContent(std::string filename_idx, std::string filename_grp, std::vector<int>* offset, std::vector<int>* length);

};


