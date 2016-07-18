#pragma once
 #include "BasicData.h"
// #include "Character.h"
// #include "Item.h"
// #include "Magic.h"
// #include "BaoShop.h"
// #include "ZhaoShi.h"
// #include "Faction.h"
// #include "Calendar.h"
#include "SceneData.h"
#include "SceneMapData.h"
// #include "SceneEventData.h"
#include "md5.h"
#include "config.h"
#include <vector>
#include "File.h"

class Save
{
public:
    Save();
    ~Save();
    struct md5num
    {
        char str[32];
    };

    int* Offset;
    unsigned char key = 156;
    unsigned char password;
    int Rgrplen;
    int Ridxlen = config::_Rtypecount * 4;
    unsigned char* GRPMD5_cal, *GRPMD5_load;
    md5num num1;
    int B_Count;										//基础数据数量
    int R_Count;										//角色数量
    int I_Count;										//物品数量
    int S_Count;										//场景数量
    int M_Count;										//武功数量
    int SP_Count;										//小宝商店数量
    int Z_Count;										//招式数量
    int F_Count;										//门派数量
    static const int R_length = 436;						//角色数据长度

    bool LoadR(int num);
    bool SaveR(int num);

    static Save save;
    static Save* getInstance()
    {
        return &save;
    }

    void jiemi(unsigned char *Data, unsigned char key, int len);
    void encryption(std::string str, unsigned char key);

    // 	 std::vector<BaoShop> m_Baoshop;
     	 std::vector<BasicData*> m_BasicData;
    // 	 std::vector<Character> m_Character;
    // 	 Calendar m_Calendar;
    // 	 std::vector<Faction> m_Faction;
    // 	 std::vector<Magic> m_Magic;
     	 std::vector<SceneData*> m_SceneData;
    // 	 std::vector<ZhaoShi> m_ZhaoShi;
    // 	 std::vector<Item> m_Item;
    std::vector<SceneMapData*> m_SceneMapData;
    // 	 std::vector<SceneEventData> m_SceneEventData;
};

