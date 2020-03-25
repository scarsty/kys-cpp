#include "NewSave.h"
#include "PotConv.h"
#include "csv.h"
#include <fstream>
#include <iostream>

// 基本
void NewSave::SaveCSVBaseInfo(Save::BaseInfo* data, int length, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_基本.csv");
    fout << "乘船";
    fout << ",";
    fout << "子场景内";
    fout << ",";
    fout << "主地图X";
    fout << ",";
    fout << "主地图Y";
    fout << ",";
    fout << "子场景X";
    fout << ",";
    fout << "子场景Y";
    fout << ",";
    fout << "面朝方向";
    fout << ",";
    fout << "船X";
    fout << ",";
    fout << "船Y";
    fout << ",";
    fout << "船X1";
    fout << ",";
    fout << "船Y1";
    fout << ",";
    fout << "内部编码";
    fout << ",";
    fout << "队友1";
    fout << ",";
    fout << "队友2";
    fout << ",";
    fout << "队友3";
    fout << ",";
    fout << "队友4";
    fout << ",";
    fout << "队友5";
    fout << ",";
    fout << "队友6";
    fout << std::endl;
    for (int i = 0; i < length; i++)
    {
        fout << data[i].InShip;
        fout << ",";
        fout << data[i].InSubMap;
        fout << ",";
        fout << data[i].MainMapX;
        fout << ",";
        fout << data[i].MainMapY;
        fout << ",";
        fout << data[i].SubMapX;
        fout << ",";
        fout << data[i].SubMapY;
        fout << ",";
        fout << data[i].FaceTowards;
        fout << ",";
        fout << data[i].ShipX;
        fout << ",";
        fout << data[i].ShipY;
        fout << ",";
        fout << data[i].TimeCount;
        fout << ",";
        fout << data[i].TimeEvent;
        fout << ",";
        fout << data[i].RandomEvent;
        fout << ",";
        for (int j = 0; j < 6; j++)
        {
            fout << data[i].Team[j];
            if (j != 6 - 1)
            {
                fout << ",";
            }
        }
        fout << std::endl;
    }
}
// 背包
void NewSave::SaveCSVItemList(ItemList* data, int length, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_背包.csv");
    fout << "物品编号";
    fout << ",";
    fout << "物品数量";
    fout << std::endl;
    for (int i = 0; i < length; i++)
    {
        fout << data[i].item_id;
        fout << ",";
        fout << data[i].count;
        fout << std::endl;
    }
}
// 人物
void NewSave::SaveCSVRoleSave(const std::vector<Role>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_人物.csv");
    fout << "编号";
    fout << ",";
    fout << "头像";
    fout << ",";
    fout << "生命成长";
    fout << ",";
    fout << "无用";
    fout << ",";
    fout << "名字";
    fout << ",";
    fout << "外号";
    fout << ",";
    fout << "性别";
    fout << ",";
    fout << "等级";
    fout << ",";
    fout << "经验";
    fout << ",";
    fout << "生命";
    fout << ",";
    fout << "生命最大值";
    fout << ",";
    fout << "内伤";
    fout << ",";
    fout << "中毒";
    fout << ",";
    fout << "体力";
    fout << ",";
    fout << "物品修炼点数";
    fout << ",";
    fout << "武器";
    fout << ",";
    fout << "防具";
	fout << ",";
	fout << "鞋子";
	fout << ",";
	fout << "饰品";

    fout << ",";
    fout << "内力性质";
    fout << ",";
    fout << "内力";
    fout << ",";
    fout << "内力最大值";
    fout << ",";
    fout << "攻击力";
    fout << ",";
    fout << "轻功";
    fout << ",";
    fout << "防御力";
    fout << ",";
    fout << "医疗";
    fout << ",";
    fout << "用毒";
    fout << ",";
    fout << "解毒";
    fout << ",";
    fout << "抗毒";
    fout << ",";
    fout << "拳掌";
    fout << ",";
    fout << "御剑";
    fout << ",";
    fout << "耍刀";
    fout << ",";
    fout << "特殊";
    fout << ",";
    fout << "暗器";
    fout << ",";
    fout << "武学常识";
    fout << ",";
    fout << "品德";
    fout << ",";
    fout << "攻击带毒";
    fout << ",";
    fout << "左右互搏";
    fout << ",";
    fout << "声望";
    fout << ",";
    fout << "资质";
    fout << ",";
    fout << "修炼物品";
    fout << ",";
    fout << "修炼点数";
    fout << ",";
    fout << "所会武功1";
    fout << ",";
    fout << "所会武功2";
    fout << ",";
    fout << "所会武功3";
    fout << ",";
    fout << "所会武功4";
    fout << ",";
    fout << "所会武功5";
    fout << ",";
    fout << "所会武功6";
    fout << ",";
    fout << "所会武功7";
    fout << ",";
    fout << "所会武功8";
    fout << ",";
    fout << "所会武功9";
    fout << ",";
    fout << "所会武功10";
    fout << ",";
    fout << "武功等级1";
    fout << ",";
    fout << "武功等级2";
    fout << ",";
    fout << "武功等级3";
    fout << ",";
    fout << "武功等级4";
    fout << ",";
    fout << "武功等级5";
    fout << ",";
    fout << "武功等级6";
    fout << ",";
    fout << "武功等级7";
    fout << ",";
    fout << "武功等级8";
    fout << ",";
    fout << "武功等级9";
    fout << ",";
    fout << "武功等级10";
    fout << ",";
    fout << "携带物品1";
    fout << ",";
    fout << "携带物品2";
    fout << ",";
    fout << "携带物品3";
    fout << ",";
    fout << "携带物品4";
    fout << ",";
    fout << "携带物品数量1";
    fout << ",";
    fout << "携带物品数量2";
    fout << ",";
    fout << "携带物品数量3";
    fout << ",";
    fout << "携带物品数量4";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << data[i].HeadID;
        fout << ",";
        fout << data[i].IncLife;
        fout << ",";
        fout << data[i].Fuyuan;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
        fout << '"' << data[i].Nick << '"';
        fout << ",";
        fout << data[i].Sexual;
        fout << ",";
        fout << data[i].Level;
        fout << ",";
        fout << data[i].Exp;
        fout << ",";
        fout << data[i].HP;
        fout << ",";
        fout << data[i].MaxHP;
        fout << ",";
        fout << data[i].Hurt;
        fout << ",";
        fout << data[i].Poison;
        fout << ",";
        fout << data[i].PhysicalPower;
        fout << ",";

		fout << data[i].Equip[0];
        fout << ",";
        fout << data[i].Equip[1];
        fout << ",";
		fout << data[i].Equip[2];
		fout << ",";
		fout << data[i].Equip[3];
		fout << ",";

        fout << data[i].MPType;
        fout << ",";
        fout << data[i].MP;
        fout << ",";
        fout << data[i].MaxMP;
        fout << ",";
        fout << data[i].Attack;
        fout << ",";
        fout << data[i].Speed;
        fout << ",";
        fout << data[i].Defence;
        fout << ",";
        fout << data[i].Medicine;
        fout << ",";
        fout << data[i].UsePoison;
        fout << ",";
        fout << data[i].Detoxification;
        fout << ",";
        fout << data[i].AntiPoison;
        fout << ",";
        fout << data[i].Fist;
        fout << ",";
        fout << data[i].Sword;
        fout << ",";
        fout << data[i].Knife;
        fout << ",";
        fout << data[i].Unusual;
        fout << ",";
        fout << data[i].HiddenWeapon;
        fout << ",";
        fout << data[i].Knowledge;
        fout << ",";
        fout << data[i].Morality;
        fout << ",";
        fout << data[i].AttackWithPoison;
        fout << ",";
        fout << data[i].AttackTwice;
        fout << ",";
        fout << data[i].Fame;
        fout << ",";
        fout << data[i].IQ;
        fout << ",";
        fout << data[i].PracticeItem;
        fout << ",";
        fout << data[i].ExpForItem;
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].MagicID[j];
            if (j != 10 - 1)
            {
                fout << ",";
            }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].MagicLevel[j];
            if (j != 10 - 1)
            {
                fout << ",";
            }
        }
        fout << ",";
        for (int j = 0; j < 4; j++)
        {
            fout << data[i].TakingItem[j];
            if (j != 4 - 1)
            {
                fout << ",";
            }
        }
        fout << ",";
        for (int j = 0; j < 4; j++)
        {
            fout << data[i].TakingItemCount[j];
            if (j != 4 - 1)
            {
                fout << ",";
            }
        }
        fout << std::endl;
    }
}
// 物品
void NewSave::SaveCSVItemSave(const std::vector<Item>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_物品.csv");
    fout << "编号";
    fout << ",";
    fout << "物品名";
    fout << ",";
    fout << "物品名无用1";
    fout << ",";
    fout << "物品名无用2";
    fout << ",";
    fout << "物品名无用3";
    fout << ",";
    fout << "物品名无用4";
    fout << ",";
    fout << "物品名无用5";
    fout << ",";
    fout << "物品名无用6";
    fout << ",";
    fout << "物品名无用7";
    fout << ",";
    fout << "物品名无用8";
    fout << ",";
    fout << "物品名无用9";
    fout << ",";
    fout << "物品名无用10";
    fout << ",";
    fout << "物品说明";
    fout << ",";
    fout << "练出武功";
    fout << ",";
    fout << "暗器动画编号";
    fout << ",";
    fout << "使用人";
    fout << ",";
    fout << "装备类型";
    fout << ",";
    fout << "显示物品说明";
    fout << ",";
    fout << "物品类型";
    fout << ",";
    fout << "未知5";
    fout << ",";
    fout << "未知6";
    fout << ",";
    fout << "未知7";
    fout << ",";
    fout << "加生命";
    fout << ",";
    fout << "加生命最大值";
    fout << ",";
    fout << "加中毒解毒";
    fout << ",";
    fout << "加体力";
    fout << ",";
    fout << "改变内力性质";
    fout << ",";
    fout << "加内力";
    fout << ",";
    fout << "加内力最大值";
    fout << ",";
    fout << "加攻击力";
    fout << ",";
    fout << "加轻功";
    fout << ",";
    fout << "加防御力";
    fout << ",";
    fout << "加医疗";
    fout << ",";
    fout << "加使毒";
    fout << ",";
    fout << "加解毒";
    fout << ",";
    fout << "加抗毒";
    fout << ",";
    fout << "加拳掌";
    fout << ",";
    fout << "加御剑";
    fout << ",";
    fout << "加耍刀";
    fout << ",";
    fout << "加特殊兵器";
    fout << ",";
    fout << "加暗器技巧";
    fout << ",";
    fout << "加武学常识";
    fout << ",";
    fout << "加品德";
    fout << ",";
    fout << "加左右互搏";
    fout << ",";
    fout << "加攻击带毒";
    fout << ",";
    fout << "仅修炼人物";
    fout << ",";
    fout << "需内力性质";
    fout << ",";
    fout << "需内力";
    fout << ",";
    fout << "需攻击力";
    fout << ",";
    fout << "需轻功";
    fout << ",";
    fout << "需用毒";
    fout << ",";
    fout << "需医疗";
    fout << ",";
    fout << "需解毒";
    fout << ",";
    fout << "需拳掌";
    fout << ",";
    fout << "需御剑";
    fout << ",";
    fout << "需耍刀";
    fout << ",";
    fout << "需特殊兵器";
    fout << ",";
    fout << "需暗器";
    fout << ",";
    fout << "需资质";
    fout << ",";
    fout << "需经验";
    fout << ",";
    fout << "练出物品需经验";
    fout << ",";
    fout << "需材料";
    fout << ",";
    fout << "练出物品1";
    fout << ",";
    fout << "练出物品2";
    fout << ",";
    fout << "练出物品3";
    fout << ",";
    fout << "练出物品4";
    fout << ",";
    fout << "练出物品5";
    fout << ",";
    fout << "练出物品数量1";
    fout << ",";
    fout << "练出物品数量2";
    fout << ",";
    fout << "练出物品数量3";
    fout << ",";
    fout << "练出物品数量4";
    fout << ",";
    fout << "练出物品数量5";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
		fout << '"' << data[i].ExpofMagic << '"';
		fout << ",";
		fout << '"' << data[i].SetNum << '"';
		fout << ",";
		fout << '"' << data[i].BattleBattleEffect << '"';
		fout << ",";
		fout << '"' << data[i].AddSign << '"';
		fout << ",";
		fout << '"' << data[i].needSex << '"';
		fout << ",";
		fout << '"' << data[i].rehurt << '"';
		fout << ",";
		fout << '"' << data[i].NeedEthics << '"';
		fout << ",";
		fout << '"' << data[i].NeedRepute << '"';
		fout << ",";
		fout << '"' << data[i].AddQianli << '"';
		fout << ",";
		fout << '"' << data[i].BattleNum << '"';
		fout << ",";
        fout << '"' << data[i].Introduction << '"';
        fout << ",";
        fout << data[i].MagicID;
        fout << ",";
        fout << data[i].HiddenWeaponEffectID;
        fout << ",";
        fout << data[i].User;
        fout << ",";
        fout << data[i].EquipType;
        fout << ",";
        fout << data[i].ShowIntroduction;
        fout << ",";
        fout << data[i].ItemType;
        fout << ",";
        fout << data[i].Inventory;
        fout << ",";
        fout << data[i].Price;
        fout << ",";
        fout << data[i].EventNum;
        fout << ",";
        fout << data[i].AddHP;
        fout << ",";
        fout << data[i].AddMaxHP;
        fout << ",";
        fout << data[i].AddPoison;
        fout << ",";
        fout << data[i].AddPhysicalPower;
        fout << ",";
        fout << data[i].ChangeMPType;
        fout << ",";
        fout << data[i].AddMP;
        fout << ",";
        fout << data[i].AddMaxMP;
        fout << ",";
        fout << data[i].AddAttack;
        fout << ",";
        fout << data[i].AddSpeed;
        fout << ",";
        fout << data[i].AddDefence;
        fout << ",";
        fout << data[i].AddMedicine;
        fout << ",";
        fout << data[i].AddUsePoison;
        fout << ",";
        fout << data[i].AddDetoxification;
        fout << ",";
        fout << data[i].AddAntiPoison;
        fout << ",";
        fout << data[i].AddFist;
        fout << ",";
        fout << data[i].AddSword;
        fout << ",";
        fout << data[i].AddKnife;
        fout << ",";
        fout << data[i].AddUnusual;
        fout << ",";
        fout << data[i].AddHiddenWeapon;
        fout << ",";
        fout << data[i].AddKnowledge;
        fout << ",";
        fout << data[i].AddMorality;
        fout << ",";
        fout << data[i].AddAttackTwice;
        fout << ",";
        fout << data[i].AddAttackWithPoison;
        fout << ",";
        fout << data[i].OnlySuitableRole;
        fout << ",";
        fout << data[i].NeedMPType;
        fout << ",";
        fout << data[i].NeedMP;
        fout << ",";
        fout << data[i].NeedAttack;
        fout << ",";
        fout << data[i].NeedSpeed;
        fout << ",";
        fout << data[i].NeedUsePoison;
        fout << ",";
        fout << data[i].NeedMedicine;
        fout << ",";
        fout << data[i].NeedDetoxification;
        fout << ",";
        fout << data[i].NeedFist;
        fout << ",";
        fout << data[i].NeedSword;
        fout << ",";
        fout << data[i].NeedKnife;
        fout << ",";
        fout << data[i].NeedUnusual;
        fout << ",";
        fout << data[i].NeedHiddenWeapon;
        fout << ",";
        fout << data[i].NeedIQ;
        fout << ",";
        fout << data[i].NeedExp;
        fout << ",";
        fout << data[i].NeedExpForMakeItem;
        fout << ",";
        fout << data[i].NeedMaterial;
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].MakeItem[j];
            if (j != 5 - 1)
            {
                fout << ",";
            }
        }
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].MakeItemCount[j];
            if (j != 5 - 1)
            {
                fout << ",";
            }
        }
        fout << std::endl;
    }
}
// 场景
void NewSave::SaveCSVSubMapInfoSave(const std::vector<SubMapInfo>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_场景.csv");
    fout << "编号";
    fout << ",";
    fout << "名称";
    fout << ",";
    fout << "出门音乐";
    fout << ",";
    fout << "进门音乐";
    fout << ",";
    fout << "跳转场景";
    fout << ",";
    fout << "进入条件";
    fout << ",";
    fout << "外景入口X1";
    fout << ",";
    fout << "外景入口Y1";
    fout << ",";
    fout << "外景入口X2";
    fout << ",";
    fout << "外景入口Y2";
    fout << ",";
    fout << "入口X";
    fout << ",";
    fout << "入口Y";
    fout << ",";
    fout << "出口X1";
    fout << ",";
    fout << "出口X2";
    fout << ",";
    fout << "出口X3";
    fout << ",";
    fout << "出口Y1";
    fout << ",";
    fout << "出口Y2";
    fout << ",";
    fout << "出口Y3";
    fout << ",";
    fout << "跳转X";
    fout << ",";
    fout << "跳转Y";
    fout << ",";
    fout << "跳转返还X";
    fout << ",";
    fout << "跳转返还Y";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
        fout << data[i].ExitMusic;
        fout << ",";
        fout << data[i].EntranceMusic;
        fout << ",";
        fout << data[i].JumpSubMap;
        fout << ",";
        fout << data[i].EntranceCondition;
        fout << ",";
        fout << data[i].MainEntranceX1;
        fout << ",";
        fout << data[i].MainEntranceY1;
        fout << ",";
        fout << data[i].MainEntranceX2;
        fout << ",";
        fout << data[i].MainEntranceY2;
        fout << ",";
        fout << data[i].EntranceX;
        fout << ",";
        fout << data[i].EntranceY;
        fout << ",";
        for (int j = 0; j < 3; j++)
        {
            fout << data[i].ExitX[j];
            if (j != 3 - 1)
            {
                fout << ",";
            }
        }
        fout << ",";
        for (int j = 0; j < 3; j++)
        {
            fout << data[i].ExitY[j];
            if (j != 3 - 1)
            {
                fout << ",";
            }
        }


        fout << std::endl;
    }
}
// 武功
void NewSave::SaveCSVMagicSave(const std::vector<Magic>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_武功.csv");
    fout << "编号";
    fout << ",";
    fout << "名称";
    fout << ",";
    fout << "未知1";
    fout << ",";
    fout << "未知2";
    fout << ",";
    fout << "未知3";
    fout << ",";
    fout << "未知4";
    fout << ",";
    fout << "未知5";
    fout << ",";
    fout << "出招音效";
    fout << ",";
    fout << "武功类型";
    fout << ",";
    fout << "武功动画";
    fout << ",";
    fout << "伤害类型";
    fout << ",";
    fout << "攻击范围类型";
    fout << ",";
    fout << "消耗内力";
    fout << ",";
    fout << "敌人中毒";
    fout << ",";
    fout << "威力1";
    fout << ",";
    fout << "威力2";
    fout << ",";
    fout << "威力3";
    fout << ",";
    fout << "威力4";
    fout << ",";
    fout << "威力5";
    fout << ",";
    fout << "威力6";
    fout << ",";
    fout << "威力7";
    fout << ",";
    fout << "威力8";
    fout << ",";
    fout << "威力9";
    fout << ",";
    fout << "威力10";
    fout << ",";
    fout << "移动范围1";
    fout << ",";
    fout << "移动范围2";
    fout << ",";
    fout << "移动范围3";
    fout << ",";
    fout << "移动范围4";
    fout << ",";
    fout << "移动范围5";
    fout << ",";
    fout << "移动范围6";
    fout << ",";
    fout << "移动范围7";
    fout << ",";
    fout << "移动范围8";
    fout << ",";
    fout << "移动范围9";
    fout << ",";
    fout << "移动范围10";
    fout << ",";
    fout << "杀伤范围1";
    fout << ",";
    fout << "杀伤范围2";
    fout << ",";
    fout << "杀伤范围3";
    fout << ",";
    fout << "杀伤范围4";
    fout << ",";
    fout << "杀伤范围5";
    fout << ",";
    fout << "杀伤范围6";
    fout << ",";
    fout << "杀伤范围7";
    fout << ",";
    fout << "杀伤范围8";
    fout << ",";
    fout << "杀伤范围9";
    fout << ",";
    fout << "杀伤范围10";
    fout << ",";
    fout << "加内力1";
    fout << ",";
    fout << "加内力2";
    fout << ",";
    fout << "加内力3";
    fout << ",";
    fout << "加内力4";
    fout << ",";
    fout << "加内力5";
    fout << ",";
    fout << "加内力6";
    fout << ",";
    fout << "加内力7";
    fout << ",";
    fout << "加内力8";
    fout << ",";
    fout << "加内力9";
    fout << ",";
    fout << "加内力10";
    fout << ",";
    fout << "杀伤内力1";
    fout << ",";
    fout << "杀伤内力2";
    fout << ",";
    fout << "杀伤内力3";
    fout << ",";
    fout << "杀伤内力4";
    fout << ",";
    fout << "杀伤内力5";
    fout << ",";
    fout << "杀伤内力6";
    fout << ",";
    fout << "杀伤内力7";
    fout << ",";
    fout << "杀伤内力8";
    fout << ",";
    fout << "杀伤内力9";
    fout << ",";
    fout << "杀伤内力10";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";

        fout << ",";
        fout << data[i].SoundID;
        fout << ",";
        fout << data[i].MagicType;
        fout << ",";
        fout << data[i].EffectID;
        fout << ",";
        fout << data[i].HurtType;
        fout << ",";
        fout << data[i].AttackAreaType;
        fout << ",";
        fout << data[i].NeedMP;
        fout << ",";
        fout << data[i].WithPoison;
        fout << ",";

        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].SelectDistance[j];
            if (j != 10 - 1)
            {
                fout << ",";
            }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].AttackDistance[j];
            if (j != 10 - 1)
            {
                fout << ",";
            }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].AddMP[j];
            if (j != 10 - 1)
            {
                fout << ",";
            }
        }
        fout << ",";

        fout << std::endl;
    }
}
// 商店
void NewSave::SaveCSVShopSave(const std::vector<Shop>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_商店.csv");
    fout << "物品编号1";
    fout << ",";
    fout << "物品编号2";
    fout << ",";
    fout << "物品编号3";
    fout << ",";
    fout << "物品编号4";
    fout << ",";
    fout << "物品编号5";
    fout << ",";
    fout << "物品总量1";
    fout << ",";
    fout << "物品总量2";
    fout << ",";
    fout << "物品总量3";
    fout << ",";
    fout << "物品总量4";
    fout << ",";
    fout << "物品总量5";
    fout << ",";
    fout << "物品价格1";
    fout << ",";
    fout << "物品价格2";
    fout << ",";
    fout << "物品价格3";
    fout << ",";
    fout << "物品价格4";
    fout << ",";
    fout << "物品价格5";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].ItemID[j];
            if (j != 5 - 1)
            {
                fout << ",";
            }
        }
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].ItermCount[j];
            if (j != 5 - 1)
            {
                fout << ",";
            }
        }

        fout << std::endl;
    }
}
// 基本
void NewSave::LoadCSVBaseInfo(Save::BaseInfo* data, int length, int record)
{
    io::CSVReader<21, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_基本.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "乘船",
		"場景",
		"人X",
		"人Y",
		"內場景坐標X",
		"內場景坐標Y",
		"人面對方向",
		"船X",
		"船Y",
		"計時",
		"定時事件",
		"隨機事件",
		"內場景方向",
		"船面對方向",
		"隊伍人數",
		"隊友",
		"隊友1",
		"隊友2",
		"隊友3",
		"隊友4",
		"隊友5");
    auto getDefault = []()
    {
        Save::BaseInfo nextLineData;
        nextLineData.InShip = 0;
        nextLineData.InSubMap = 0;
        nextLineData.MainMapX = 0;
        nextLineData.MainMapY = 0;
        nextLineData.SubMapX = 0;
        nextLineData.SubMapY = 0;
        nextLineData.FaceTowards = 0;
        nextLineData.ShipX = 0;
        nextLineData.ShipY = 0;
        nextLineData.TimeCount = 0; 
        nextLineData.TimeEvent = 0;
        nextLineData.RandomEvent = 0;
		nextLineData.SubmapTowards = 0;
		nextLineData.ShipTowards = 0;
		nextLineData.TeamCount = 0;
        for (int j = 0; j < 6; j++)
        {
            nextLineData.Team[j] = -1;
        }
        return nextLineData;
    };
    int lines = 0;
    auto nextLineData = getDefault();
	while (in.read_row(
		nextLineData.InShip,
		nextLineData.InSubMap,
		nextLineData.MainMapX,
		nextLineData.MainMapY,
		nextLineData.SubMapX,
		nextLineData.SubMapY,
		nextLineData.FaceTowards,
		nextLineData.ShipX,
		nextLineData.ShipY,
		nextLineData.TimeCount,
		nextLineData.TimeEvent,
		nextLineData.RandomEvent,
		nextLineData.SubmapTowards,
		nextLineData.ShipTowards,
		nextLineData.TeamCount,
        nextLineData.Team[0],
        nextLineData.Team[1],
        nextLineData.Team[2],
        nextLineData.Team[3],
        nextLineData.Team[4],
        nextLineData.Team[5]))
    {
        data[lines] = nextLineData;
        if (lines + 1 == length)
        {
            break;
        }
        lines++;
        nextLineData = getDefault();
    }
}
// 背包
void NewSave::LoadCSVItemList(ItemList* data, int length, int record)
{
    io::CSVReader<2, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_背包.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "物品编号",
        "物品数量");
    auto getDefault = []()
    {
        ItemList nextLineData;
        nextLineData.item_id = -1;
        nextLineData.count = 0;
        return nextLineData;
    };
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.item_id,
        nextLineData.count))
    {
        data[lines] = nextLineData;
        if (lines + 1 == length)
        {
            break;
        }
        lines++;
        nextLineData = getDefault();
    }
}
// 人物
void NewSave::LoadCSVRoleSave(std::vector<Role>& data, int record)
{
	
    data.clear();
    io::CSVReader<305, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_人物.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
		"编号",
		"頭像/戰斗代號",
		"生命增長",
		"福緣",
		"姓名",
		"外號",
		"性別",
		"等級",
		"經驗值",
		"生命",
		"生命最大值",
		"受傷程度",
		"中毒程度",
		"體力",
		"戰斗圖編號",
		"武器",
		"防具",
		"頭戴",
		"腳穿",
		"飾品",
		"功體",
		"隊伍狀態",
		"憤怒值",
		"查找標記",
		"行動能力",
		"技能點",
		"戰略AI狀態",
		"外觀",
		"行動時間",
		"難度",
		"忠誠度",
		"藥物",
		"內力性質",
		"內力",
		"內力最大值",
		"攻擊力",
		"輕功",
		"防御力",
		"醫療",
		"暫缺",
		"技藝",
		"抗毒",
		"拳法",
		"御劍",
		"耍刀",
		"特殊兵器",
		"暗器技巧",
		"武學常識",
		"品德",
		"毒術",
		"左右互搏",
		"聲望",
		"資質",
		"修練物品",
		"修煉點數",
		"相性",
		"交情",
		"人物類別",
		"聲望傾向",
		"品德傾向",
		"相性傾向",
		"交情傾向",
		"所屬門派",
		"師父",
		"師承序位",
		"拜師順序",
		"仇視門派",
		"仇視門派1",
		"當前位置",
		"內部位置",
		"內部方向1",
		"人物狀態",
		"練武傾向",
		"冥思傾向",
		"勞動傾向",
		"其他傾向",
		"臨時位置",
		"內部臨時位置",
		"內部方向2",
		"場景X坐標",
		"場景Y坐標",
		"送書標記",
		"官府貢獻",
		"夫妻",
		"年齡",
		"未用1",
		"未用2",
		"未用3",
		"戰場ID",
		"所會武功",
		"所會武功1",
		"所會武功2",
		"所會武功3",
		"所會武功4",
		"所會武功5",
		"所會武功6",
		"所會武功7",
		"所會武功8",
		"所會武功9",
		"所會武功10",
		"所會武功11",
		"所會武功12",
		"所會武功13",
		"所會武功14",
		"所會武功15",
		"所會武功16",
		"所會武功17",
		"所會武功18",
		"所會武功19",
		"所會武功20",
		"所會武功21",
		"所會武功22",
		"所會武功23",
		"所會武功24",
		"所會武功25",
		"所會武功26",
		"所會武功27",
		"所會武功28",
		"所會武功29",
		"所會武功30",
		"所會武功31",
		"所會武功32",
		"所會武功33",
		"所會武功34",
		"所會武功35",
		"所會武功36",
		"所會武功37",
		"所會武功38",
		"所會武功39",
		"所會武功等級",
		"所會武功等級1",
		"所會武功等級2",
		"所會武功等級3",
		"所會武功等級4",
		"所會武功等級5",
		"所會武功等級6",
		"所會武功等級7",
		"所會武功等級8",
		"所會武功等級9",
		"所會武功等級10",
		"所會武功等級11",
		"所會武功等級12",
		"所會武功等級13",
		"所會武功等級14",
		"所會武功等級15",
		"所會武功等級16",
		"所會武功等級17",
		"所會武功等級18",
		"所會武功等級19",
		"所會武功等級20",
		"所會武功等級21",
		"所會武功等級22",
		"所會武功等級23",
		"所會武功等級24",
		"所會武功等級25",
		"所會武功等級26",
		"所會武功等級27",
		"所會武功等級28",
		"所會武功等級29",
		"所會武功等級30",
		"所會武功等級31",
		"所會武功等級32",
		"所會武功等級33",
		"所會武功等級34",
		"所會武功等級35",
		"所會武功等級36",
		"所會武功等級37",
		"所會武功等級38",
		"所會武功等級39",
		"攜帶物品1",
		"攜帶物品2",
		"攜帶物品3",
		"攜帶物品4",
		"攜帶物品1數量",
		"攜帶物品2數量",
		"攜帶物品3數量",
		"攜帶物品4數量",
		"激活武功",
		"激活武功1",
		"激活武功2",
		"激活武功3",
		"激活武功4",
		"激活武功5",
		"激活武功6",
		"激活武功7",
		"激活武功8",
		"激活武功9",
		"所會招式",
		"所會招式1",
		"所會招式2",
		"所會招式3",
		"所會招式4",
		"所會招式5",
		"所會招式6",
		"所會招式7",
		"所會招式8",
		"所會招式9",
		"所會招式10",
		"所會招式11",
		"所會招式12",
		"所會招式13",
		"所會招式14",
		"所會招式15",
		"所會招式16",
		"所會招式17",
		"所會招式18",
		"所會招式19",
		"所會招式20",
		"所會招式21",
		"所會招式22",
		"所會招式23",
		"所會招式24",
		"所會招式25",
		"所會招式26",
		"所會招式27",
		"所會招式28",
		"所會招式29",
		"所會招式30",
		"所會招式31",
		"所會招式32",
		"所會招式33",
		"所會招式34",
		"所會招式35",
		"所會招式36",
		"所會招式37",
		"所會招式38",
		"所會招式39",
		"對話菜單開關",
		"對話事件",
		"狀態事件",
		"入隊事件",
		"切磋事件",
		"學習事件",
		"事件池1",
		"事件池2",
		"事件池3",
		"離隊期限",
		"離隊事件",
		"隨機值1",
		"隨機值2",
		"攜帶物品5",
		"攜帶物品6",
		"攜帶物品7",
		"攜帶物品8",
		"攜帶物品5數量",
		"攜帶物品6數量",
		"攜帶物品7數量",
		"攜帶物品8數量",
		"暗箭",
		"醫師",
		"裝備特技",
		"回血",
		"回內",
		"回體",
		"暴躁",
		"配合",
		"武學",
		"突破",
		"冷靜",
		"百變",
		"破氣",
		"罩門",
		"變幻",
		"反攻",
		"氣功",
		"硬功",
		"靈活",
		"行氣",
		"身法",
		"攻擊潛力",
		"防禦潛力",
		"輕功潛力",
		"拳掌潛力",
		"御劍潛力",
		"耍刀潛力",
		"奇門潛力",
		"暗器潛力",
		"固守",
		"天命",
		"星宿",
		"專長",
		"專長1",
		"專長2",
		"專長3",
		"專長4",
		"特性",
		"特性1",
		"特性2",
		"特性3",
		"特性4",
		"特性5",
		"特性6",
		"特性7",
		"特性8",
		"特性9",
		"門派貢獻",
		"未使用",
		"未使用1",
		"未使用2",
		"未使用3",
		"未使用4",
		"未使用5",
		"未使用6",
		"未使用7",
		"未使用8");
    auto getDefault = []()
    {
        Role nextLineData;
        nextLineData.ID = 0;
        nextLineData.HeadID = 0;
        nextLineData.IncLife = 0;
        nextLineData.Fuyuan = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
        memset(nextLineData.Nick, '\0', sizeof(nextLineData.Nick));
        nextLineData.Sexual = 0;
        nextLineData.Level = 0;
        nextLineData.Exp = 0;
        nextLineData.HP = 0;
        nextLineData.MaxHP = 0;
        nextLineData.Hurt = 0;
        nextLineData.Poison = 0;
        nextLineData.PhysicalPower = 0;
		nextLineData.FightNum = 0;
		for (int j = 0; j < 5; j++)
		{
			nextLineData.Equip[j] = 0;
		}
		nextLineData.Gongti = 0;
		nextLineData.TeamState = 0;
		nextLineData.Angry = 0;
		nextLineData.isRandomed = 0;
		nextLineData.Moveable = 0;
		nextLineData.In_HeadNum = 0;
		nextLineData.ZhanLueAI = 0;
		nextLineData.Impression = 0; 
		nextLineData.dtime = 0; 
		nextLineData.difficulty = 0;
		nextLineData.Zhongcheng = 0;
		nextLineData.reHurt = 0;
		nextLineData.MPType = 0;
		nextLineData.MP = 0;
		nextLineData.MaxMP = 0;
		nextLineData.Attack = 0;
		nextLineData.Speed = 0;
		nextLineData.Defence = 0;
		nextLineData.Medicine = 0;
		nextLineData.UsePoison = 0;
		nextLineData.Detoxification = 0;
		nextLineData.AntiPoison = 0; 
		nextLineData.Fist = 0; 
		nextLineData.Sword = 0;
		nextLineData.Knife = 0;
		nextLineData.Unusual = 0;
		nextLineData.HiddenWeapon = 0;
		nextLineData.Knowledge = 0; 
		nextLineData.Morality = 0;
		nextLineData.AttackWithPoison = 0;
		nextLineData.AttackTwice = 0;
		nextLineData.Fame = 0;
		nextLineData.IQ = 0;
		nextLineData.PracticeItem = 0;
		nextLineData.ExpForItem = 0;
		nextLineData.xiangxing = 0;
		nextLineData.jiaoqing = 0;
		nextLineData.Rtype = 0;
		nextLineData.swq = 0;
		nextLineData.pdq = 0;
		nextLineData.xxq = 0;
		nextLineData.jqq = 0;
		nextLineData.MenPai = 0;
		nextLineData.shifu = 0;
		nextLineData.scsx = 0;
		nextLineData.bssx = 0;
		nextLineData.Choushi[0] = 0;
		nextLineData.Choushi[1] = 0;
		nextLineData.weizhi = 0;
		nextLineData.nweizhi = 0;
		nextLineData.nfangxiang = 0;
		nextLineData.OnStatus = 0;
		nextLineData.lwq = 0;
		nextLineData.msq = 0;
		nextLineData.ldq = 0;
		nextLineData.qtq = 0; 
		nextLineData.lsweizhi = 0;
		nextLineData.lsnweizhi = 0;
		nextLineData.lsfangxiang = 0;
		nextLineData.Sx = 0;
		nextLineData.Sy = 0;
		nextLineData.songshu = 0;
		nextLineData.gongxian = 0;
		nextLineData.unuse5 = 0;
		nextLineData.unuse6 = 0;
		nextLineData.unuse7 = 0;
		nextLineData.unuse8 = 0;
		nextLineData.unuse9 = 0;
		nextLineData.btnum = 0; //记录年龄,备份年龄 //98
		for (int j = 0; j < ROLE_MAGIC_COUNT; j++)
		{
			nextLineData.MagicID[j] = 0;
		}
		for (int j = 0; j < ROLE_MAGIC_COUNT; j++)
		{
			nextLineData.MagicLevel[j] = 0;
		}
		for (int j = 0; j < ROLE_TAKING_ITEM_COUNT; j++)
		{
			nextLineData.TakingItem[j] = 0;
		}
		for (int j = 0; j < ROLE_TAKING_ITEM_COUNT; j++)
		{
			nextLineData.TakingItemCount[j] = 0;
		}
		for (int j = 0; j < 10; j++)
		{
			nextLineData.JhMagic[j] = 0;
		}
		for (int j = 0; j < ROLE_MAGIC_COUNT; j++)
		{
			nextLineData.LZhaoshi[j] = 0;
		}
		nextLineData.MRevent = 0;
		for (int j = 0; j < 8; j++)
		{
			nextLineData.AllEvent[j] = 0;
		}
		nextLineData.LeaveTime = 0;
		nextLineData.LeaveEvent = 0;
		nextLineData.LaoLian = 0;
		nextLineData.QiangZhuang = 0;
		nextLineData.NeiJia = 0;
		nextLineData.QiangGong = 0;
		nextLineData.JianGU = 0;
		nextLineData.QingLing = 0;
		nextLineData.QuanShi = 0;
		nextLineData.JianKe = 0;
		nextLineData.Daoke = 0;
		nextLineData.YiBing = 0;
		nextLineData.AnJian = 0;
		nextLineData.YIShi = 0;
		nextLineData.DuRen = 0;
		nextLineData.HuiXue = 0;
		nextLineData.HuiNei = 0;
		nextLineData.HuiTI = 0;
		nextLineData.BaoZao = 0;
		nextLineData.PeiHe = 0;
		nextLineData.WuXue = 0;
		nextLineData.TuPo = 0;
		nextLineData.LengJing = 0;
		nextLineData.BaiBian = 0;
		nextLineData.PoQi = 0;
		nextLineData.ZhaoMen = 0;
		nextLineData.BianHuan = 0;
		nextLineData.FanGong = 0;
		nextLineData.QiGong = 0;
		nextLineData.YingGong = 0;
		nextLineData.LingHuo = 0;
		nextLineData.XingQi = 0;
		nextLineData.ShenFa = 0;
		nextLineData.FenFa = 0;
		nextLineData.ZhanYi = 0;
		nextLineData.JingZhun = 0;
		nextLineData.JiSu = 0;
		nextLineData.KuangBao = 0;
		nextLineData.ShouFa = 0; 
		nextLineData.LianHuan = 0;
		nextLineData.WaJie = 0;
		nextLineData.GuShou = 0; 
		nextLineData.TianMing = 0;
		nextLineData.XingXiu = 0;
		for (int j = 0; j < 5; j++)
		{
			nextLineData.ZhuanChang[j] = 0;
		}
		for (int j = 0; j < 10; j++)
		{
			nextLineData.TeXing[j] = 0;
		}
		for (int j = 0; j < 10; j++)
		{
			nextLineData.unused[j] = 0;
		}
        return nextLineData;
    };
    char* Name__;
    char* Nick__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        nextLineData.HeadID,
        nextLineData.IncLife,
        nextLineData.Fuyuan,
        Name__,
        Nick__,
        nextLineData.Sexual,
        nextLineData.Level,
        nextLineData.Exp,
        nextLineData.HP,
        nextLineData.MaxHP,
        nextLineData.Hurt,
        nextLineData.Poison,
        nextLineData.PhysicalPower,
		nextLineData.FightNum,
        nextLineData.Equip[0],
        nextLineData.Equip[1],
		nextLineData.Equip[2],
		nextLineData.Equip[3],
		nextLineData.Equip[4],
		nextLineData.Gongti,
		nextLineData.TeamState,
		nextLineData.Angry,
		nextLineData.isRandomed,
		nextLineData.Moveable,
		nextLineData.In_HeadNum,
		nextLineData.ZhanLueAI,
		nextLineData.Impression,
		nextLineData.dtime,
		nextLineData.difficulty,
		nextLineData.Zhongcheng,
		nextLineData.reHurt,
		nextLineData.MPType,
		nextLineData.MP,
		nextLineData.MaxMP,
		nextLineData.Attack,
		nextLineData.Speed,
		nextLineData.Defence,
		nextLineData.Medicine,
		nextLineData.UsePoison,
		nextLineData.Detoxification,
		nextLineData.AntiPoison,
		nextLineData.Fist,
		nextLineData.Sword,
		nextLineData.Knife,
		nextLineData.Unusual,
		nextLineData.HiddenWeapon,
		nextLineData.Knowledge,
		nextLineData.Morality,
		nextLineData.AttackWithPoison,
		nextLineData.AttackTwice,
		nextLineData.Fame,
		nextLineData.IQ,
		nextLineData.PracticeItem,
		nextLineData.ExpForItem,
		nextLineData.xiangxing,
		nextLineData.jiaoqing,
		nextLineData.Rtype,
		nextLineData.swq,
		nextLineData.pdq,
		nextLineData.xxq,
		nextLineData.jqq,
		nextLineData.MenPai,
		nextLineData.shifu,
		nextLineData.scsx,
		nextLineData.bssx,
		nextLineData.Choushi[0],
		nextLineData.Choushi[1],
		nextLineData.weizhi,
		nextLineData.nweizhi,
		nextLineData.nfangxiang,
		nextLineData.OnStatus,
		nextLineData.lwq,
		nextLineData.msq,
		nextLineData.ldq,
		nextLineData.qtq,
		nextLineData.lsweizhi,
		nextLineData.lsnweizhi,
		nextLineData.lsfangxiang,
		nextLineData.Sx,
		nextLineData.Sy,
		nextLineData.songshu,
		nextLineData.gongxian,
		nextLineData.unuse5,
		nextLineData.unuse6,
		nextLineData.unuse7,
		nextLineData.unuse8,
		nextLineData.unuse9,
		nextLineData.btnum, //记录年龄,备份年龄 //98
		nextLineData.MagicID[0],
		nextLineData.MagicID[1],
		nextLineData.MagicID[2],
		nextLineData.MagicID[3],
		nextLineData.MagicID[4],
		nextLineData.MagicID[5],
		nextLineData.MagicID[6],
		nextLineData.MagicID[7],
		nextLineData.MagicID[8],
		nextLineData.MagicID[9],
		nextLineData.MagicID[10],
		nextLineData.MagicID[11],
			nextLineData.MagicID[12],
			nextLineData.MagicID[13],
			nextLineData.MagicID[14],
			nextLineData.MagicID[15],
			nextLineData.MagicID[16],
			nextLineData.MagicID[17],
			nextLineData.MagicID[18],
			nextLineData.MagicID[19],
			nextLineData.MagicID[20],
			nextLineData.MagicID[21],
			nextLineData.MagicID[22],
			nextLineData.MagicID[23],
			nextLineData.MagicID[24],
			nextLineData.MagicID[25],
			nextLineData.MagicID[26],
			nextLineData.MagicID[27],
			nextLineData.MagicID[28],
			nextLineData.MagicID[29],
			nextLineData.MagicID[30],
			nextLineData.MagicID[31],
			nextLineData.MagicID[32],
			nextLineData.MagicID[33],
			nextLineData.MagicID[34],
			nextLineData.MagicID[35],
			nextLineData.MagicID[36],
			nextLineData.MagicID[37],
			nextLineData.MagicID[38],
			nextLineData.MagicID[39],
			nextLineData.MagicLevel[0],
			nextLineData.MagicLevel[1],
			nextLineData.MagicLevel[2],
			nextLineData.MagicLevel[3],
			nextLineData.MagicLevel[4],
			nextLineData.MagicLevel[5],
			nextLineData.MagicLevel[6],
			nextLineData.MagicLevel[7],
			nextLineData.MagicLevel[8],
			nextLineData.MagicLevel[9],
			nextLineData.MagicLevel[10],
			nextLineData.MagicLevel[11],
			nextLineData.MagicLevel[12],
			nextLineData.MagicLevel[13],
			nextLineData.MagicLevel[14],
			nextLineData.MagicLevel[15],
			nextLineData.MagicLevel[16],
			nextLineData.MagicLevel[17],
			nextLineData.MagicLevel[18],
			nextLineData.MagicLevel[19],
			nextLineData.MagicLevel[20],
			nextLineData.MagicLevel[21],
			nextLineData.MagicLevel[22],
			nextLineData.MagicLevel[23],
			nextLineData.MagicLevel[24],
			nextLineData.MagicLevel[25],
			nextLineData.MagicLevel[26],
			nextLineData.MagicLevel[27],
			nextLineData.MagicLevel[28],
			nextLineData.MagicLevel[29],
			nextLineData.MagicLevel[30],
			nextLineData.MagicLevel[31],
			nextLineData.MagicLevel[32],
			nextLineData.MagicLevel[33],
			nextLineData.MagicLevel[34],
			nextLineData.MagicLevel[35],
			nextLineData.MagicLevel[36],
			nextLineData.MagicLevel[37],
			nextLineData.MagicLevel[38],
			nextLineData.MagicLevel[39],
			nextLineData.TakingItem[0],
			nextLineData.TakingItem[1],
			nextLineData.TakingItem[2],
			nextLineData.TakingItem[3],
			nextLineData.TakingItemCount[0],
			nextLineData.TakingItemCount[1],
			nextLineData.TakingItemCount[2],
			nextLineData.TakingItemCount[3],
			nextLineData.JhMagic[0],
			nextLineData.JhMagic[1],
			nextLineData.JhMagic[2],
			nextLineData.JhMagic[3],
			nextLineData.JhMagic[4],
			nextLineData.JhMagic[5],
			nextLineData.JhMagic[6],
			nextLineData.JhMagic[7],
			nextLineData.JhMagic[8],
			nextLineData.JhMagic[9],
			nextLineData.LZhaoshi[0],
			nextLineData.LZhaoshi[1],
			nextLineData.LZhaoshi[2],
			nextLineData.LZhaoshi[3],
			nextLineData.LZhaoshi[4],
			nextLineData.LZhaoshi[5],
			nextLineData.LZhaoshi[6],
			nextLineData.LZhaoshi[7],
			nextLineData.LZhaoshi[8],
			nextLineData.LZhaoshi[9],
			nextLineData.LZhaoshi[10],
			nextLineData.LZhaoshi[11],
			nextLineData.LZhaoshi[12],
			nextLineData.LZhaoshi[13],
			nextLineData.LZhaoshi[14],
			nextLineData.LZhaoshi[15],
			nextLineData.LZhaoshi[16],
			nextLineData.LZhaoshi[17],
			nextLineData.LZhaoshi[18],
			nextLineData.LZhaoshi[19],
			nextLineData.LZhaoshi[20],
			nextLineData.LZhaoshi[21],
			nextLineData.LZhaoshi[22],
			nextLineData.LZhaoshi[23],
			nextLineData.LZhaoshi[24],
			nextLineData.LZhaoshi[25],
			nextLineData.LZhaoshi[26],
			nextLineData.LZhaoshi[27],
			nextLineData.LZhaoshi[28],
			nextLineData.LZhaoshi[29],
			nextLineData.LZhaoshi[30],
			nextLineData.LZhaoshi[31],
			nextLineData.LZhaoshi[32],
			nextLineData.LZhaoshi[33],
			nextLineData.LZhaoshi[34],
			nextLineData.LZhaoshi[35],
			nextLineData.LZhaoshi[36],
			nextLineData.LZhaoshi[37],
			nextLineData.LZhaoshi[38],
			nextLineData.LZhaoshi[39],
			nextLineData.MRevent,
			nextLineData.AllEvent[0],
			nextLineData.AllEvent[1],
			nextLineData.AllEvent[2],
			nextLineData.AllEvent[3],
			nextLineData.AllEvent[4],
			nextLineData.AllEvent[5],
			nextLineData.AllEvent[6],
			nextLineData.AllEvent[7],
		nextLineData.LeaveTime,
		nextLineData.LeaveEvent,
		nextLineData.LaoLian,
		nextLineData.QiangZhuang,
		nextLineData.NeiJia,
		nextLineData.QiangGong,
		nextLineData.JianGU,
		nextLineData.QingLing,
		nextLineData.QuanShi,
		nextLineData.JianKe,
		nextLineData.Daoke,
		nextLineData.YiBing,
		nextLineData.AnJian,
		nextLineData.YIShi,
		nextLineData.DuRen,
		nextLineData.HuiXue,
		nextLineData.HuiNei,
		nextLineData.HuiTI,
		nextLineData.BaoZao,
		nextLineData.PeiHe,
		nextLineData.WuXue,
		nextLineData.TuPo,
		nextLineData.LengJing,
		nextLineData.BaiBian,
		nextLineData.PoQi,
		nextLineData.ZhaoMen,
		nextLineData.BianHuan,
		nextLineData.FanGong,
		nextLineData.QiGong,
		nextLineData.YingGong,
		nextLineData.LingHuo,
		nextLineData.XingQi,
		nextLineData.ShenFa,
		nextLineData.FenFa,
		nextLineData.ZhanYi,
		nextLineData.JingZhun,
		nextLineData.JiSu,
		nextLineData.KuangBao,
		nextLineData.ShouFa,
		nextLineData.LianHuan,
		nextLineData.WaJie,
		nextLineData.GuShou,
		nextLineData.TianMing,
		nextLineData.XingXiu,
		nextLineData.ZhuanChang[0],
			nextLineData.ZhuanChang[1],
			nextLineData.ZhuanChang[2],
			nextLineData.ZhuanChang[3],
			nextLineData.ZhuanChang[4],
				nextLineData.TeXing[0],
				nextLineData.TeXing[1],
				nextLineData.TeXing[2],
				nextLineData.TeXing[3],
				nextLineData.TeXing[4],
				nextLineData.TeXing[5],
				nextLineData.TeXing[6],
				nextLineData.TeXing[7],
				nextLineData.TeXing[8],
				nextLineData.TeXing[9],
				nextLineData.unused[0],
				nextLineData.unused[1],
				nextLineData.unused[2],
				nextLineData.unused[3],
				nextLineData.unused[4],
				nextLineData.unused[5],
				nextLineData.unused[6],
				nextLineData.unused[7],
				nextLineData.unused[8],
				nextLineData.unused[9]))
    {
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        strncpy(nextLineData.Nick, Nick__, sizeof(nextLineData.Nick) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
	
}
// 物品
void NewSave::LoadCSVItemSave(std::vector<Item>& data, int record)
{
    data.clear();
    io::CSVReader<82, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_物品.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
		"编号",
		"物品名",
		"物品武功經驗",
		"套裝號",
		"戰斗特效",
		"增加標簽",
		"要求性別",
		"恢復傷勢",
		"需品德",
		"需聲望",
		"增加潛力值",
		"戰場號",
		"物品說明",
		"練出武功",
		"暗器動畫編號",
		"使用人",
		"裝備類型",
		"顯示物品說明",
		"類型",
		"商店庫存",
		"售價",
		"調用事件",
		"加生命",
		"加生命最大值",
		"加中毒解毒",
		"加體力",
		"改變內力性質",
		"加內力",
		"加內力最大值",
		"加攻擊力",
		"加輕功",
		"加防御力",
		"加醫療",
		"加使毒",
		"加技藝",
		"加抗毒",
		"加拳掌",
		"加御劍",
		"加耍刀",
		"加特殊兵器",
		"加暗器技巧",
		"加武學常識",
		"加品德",
		"需生命",
		"加功夫帶毒",
		"僅修煉人物",
		"需內力性質",
		"需內力",
		"需攻擊力",
		"需輕功",
		"需毒術",
		"需醫療",
		"需技藝",
		"需拳掌",
		"需御劍",
		"需耍刀",
		"需特殊兵器",
		"需暗器",
		"需資質",
		"需經驗",
		"數量",
		"稀有度",
		"所需物品1",
		"所需物品2",
		"所需物品3",
		"所需物品4",
		"所需物品5",
		"需要物品1數量",
		"需要物品2數量",
		"需要物品3數量",
		"需要物品4數量",
		"需要物品5數量",
		"加資質",
		"加富源",
		"未使用",
		"未使用1",
		"未使用2",
		"未使用3",
		"未使用4",
		"未使用5",
		"未使用6",
		"未使用7");
    auto getDefault = []()
    {
        Item nextLineData;
        nextLineData.ID = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
		nextLineData.ExpofMagic = 0;
		nextLineData.SetNum = 0;
		nextLineData.BattleBattleEffect = 0;
		nextLineData.AddSign = 0;
		nextLineData.needSex = 0;
		nextLineData.rehurt = 0;
		nextLineData.NeedEthics = 0;
		nextLineData.NeedRepute = 0;
		nextLineData.AddQianli = 0;
		nextLineData.BattleNum = 0;
        memset(nextLineData.Introduction, '\0', sizeof(nextLineData.Introduction));
        nextLineData.MagicID = -1;
        nextLineData.HiddenWeaponEffectID = -1;
        nextLineData.User = -1;
        nextLineData.EquipType = -1;
        nextLineData.ShowIntroduction = 0;
        nextLineData.ItemType = 0;
        nextLineData.Inventory = 0; 
        nextLineData.Price = 0;
        nextLineData.EventNum = 0;
        nextLineData.AddHP = 0;
        nextLineData.AddMaxHP = 0;
        nextLineData.AddPoison = 0;
        nextLineData.AddPhysicalPower = 0;
        nextLineData.ChangeMPType = -1;
        nextLineData.AddMP = 0;
        nextLineData.AddMaxMP = 0;
        nextLineData.AddAttack = 0;
        nextLineData.AddSpeed = 0;
        nextLineData.AddDefence = 0;
        nextLineData.AddMedicine = 0;
        nextLineData.AddUsePoison = 0;
        nextLineData.AddDetoxification = 0;
        nextLineData.AddAntiPoison = 0;
        nextLineData.AddFist = 0;
        nextLineData.AddSword = 0;
        nextLineData.AddKnife = 0;
        nextLineData.AddUnusual = 0;
        nextLineData.AddHiddenWeapon = 0;
        nextLineData.AddKnowledge = 0;
        nextLineData.AddMorality = 0;
        nextLineData.AddAttackTwice = 0;
        nextLineData.AddAttackWithPoison = 0;
        nextLineData.OnlySuitableRole = -1;
        nextLineData.NeedMPType = 0;
        nextLineData.NeedMP = 0;
        nextLineData.NeedAttack = 0;
        nextLineData.NeedSpeed = 0;
        nextLineData.NeedUsePoison = 0;
        nextLineData.NeedMedicine = 0;
        nextLineData.NeedDetoxification = 0;
        nextLineData.NeedFist = 0;
        nextLineData.NeedSword = 0;
        nextLineData.NeedKnife = 0;
        nextLineData.NeedUnusual = 0;
        nextLineData.NeedHiddenWeapon = 0;
        nextLineData.NeedIQ = 0;
        nextLineData.NeedExp = 0;
        nextLineData.NeedExpForMakeItem = 0;
        nextLineData.NeedMaterial = -1;
        for (int j = 0; j < 5; j++)
        {
            nextLineData.MakeItem[j] = -1;
        }
        for (int j = 0; j < 5; j++)
        {
            nextLineData.MakeItemCount[j] = 0;
        }
		nextLineData.AddIQ = 0;
		nextLineData.AddFuyuan = 0;
		for (int j = 0; j < 8; j++)
		{
			nextLineData.Unused[j] = 0;
		}
        return nextLineData;
    };
    char* Name__;
    char* Introduction__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        Name__,
		nextLineData.ExpofMagic ,
	nextLineData.SetNum ,
	nextLineData.BattleBattleEffect ,
	nextLineData.AddSign ,
	nextLineData.needSex ,
	nextLineData.rehurt ,
	nextLineData.NeedEthics ,
	nextLineData.NeedRepute ,
	nextLineData.AddQianli ,
	nextLineData.BattleNum ,
        Introduction__,
        nextLineData.MagicID,
        nextLineData.HiddenWeaponEffectID,
        nextLineData.User,
        nextLineData.EquipType,
        nextLineData.ShowIntroduction,
        nextLineData.ItemType,
		nextLineData.Inventory,
	nextLineData.Price,
	nextLineData.EventNum,
        nextLineData.AddHP,
        nextLineData.AddMaxHP,
        nextLineData.AddPoison,
        nextLineData.AddPhysicalPower,
        nextLineData.ChangeMPType,
        nextLineData.AddMP,
        nextLineData.AddMaxMP,
        nextLineData.AddAttack,
        nextLineData.AddSpeed,
        nextLineData.AddDefence,
        nextLineData.AddMedicine,
        nextLineData.AddUsePoison,
        nextLineData.AddDetoxification,
        nextLineData.AddAntiPoison,
        nextLineData.AddFist,
        nextLineData.AddSword,
        nextLineData.AddKnife,
        nextLineData.AddUnusual,
        nextLineData.AddHiddenWeapon,
        nextLineData.AddKnowledge,
        nextLineData.AddMorality,
        nextLineData.AddAttackTwice,
        nextLineData.AddAttackWithPoison,
        nextLineData.OnlySuitableRole,
        nextLineData.NeedMPType,
        nextLineData.NeedMP,
        nextLineData.NeedAttack,
        nextLineData.NeedSpeed,
        nextLineData.NeedUsePoison,
        nextLineData.NeedMedicine,
        nextLineData.NeedDetoxification,
        nextLineData.NeedFist,
        nextLineData.NeedSword,
        nextLineData.NeedKnife,
        nextLineData.NeedUnusual,
        nextLineData.NeedHiddenWeapon,
        nextLineData.NeedIQ,
        nextLineData.NeedExp,
        nextLineData.NeedExpForMakeItem,
        nextLineData.NeedMaterial,
        nextLineData.MakeItem[0],
        nextLineData.MakeItem[1],
        nextLineData.MakeItem[2],
        nextLineData.MakeItem[3],
        nextLineData.MakeItem[4],
        nextLineData.MakeItemCount[0],
        nextLineData.MakeItemCount[1],
        nextLineData.MakeItemCount[2],
        nextLineData.MakeItemCount[3],
        nextLineData.MakeItemCount[4],
		nextLineData.AddIQ,
		nextLineData.AddFuyuan,
		nextLineData.Unused[0],
		nextLineData.Unused[1],
		nextLineData.Unused[2],
		nextLineData.Unused[3],
		nextLineData.Unused[4],
		nextLineData.Unused[5],
		nextLineData.Unused[6],
		nextLineData.Unused[7]))
    {
        strncpy(nextLineData.Introduction, Introduction__, sizeof(nextLineData.Introduction) - 1);
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
	
}
// 场景
void NewSave::LoadCSVSubMapInfoSave(std::vector<SubMapInfo>& data, int record)
{
    data.clear();
    io::CSVReader<91, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_场景.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
		"编号",
		"名稱",
		"出門音樂",
		"進門音樂",
		"場景調色板",
		"進入條件",
		"外景入口X1",
		"外景入口Y1",
		"外景入口X2",
		"外景入口Y2",
		"入口X",
		"入口Y",
		"出口X1",
		"出口X2",
		"出口X3",
		"出口Y1",
		"出口Y2",
		"出口Y3",
		"環境",
		"未用",
		"所屬門派",
		"戰斗中",
		"總練武場數",
		"練武場數",
		"總藏經閣數",
		"藏經閣數",
		"練武場X",
		"練武場X1",
		"練武場X2",
		"練武場X3",
		"練武場X4",
		"練武場Y",
		"練武場Y1",
		"練武場Y2",
		"練武場Y3",
		"練武場Y4",
		"藏經閣X",
		"藏經閣X1",
		"藏經閣X2",
		"藏經閣X3",
		"藏經閣X4",
		"藏經閣Y",
		"藏經閣Y1",
		"藏經閣Y2",
		"藏經閣Y3",
		"藏經閣Y4",
		"閉關室開關",
		"閉關室X",
		"閉關室Y",
		"煉丹爐開關",
		"煉丹爐X",
		"煉丹爐Y",
		"兵器場開關",
		"兵器場X",
		"兵器場Y",
		"旗幟X",
		"旗幟Y",
		"煉丹進度",
		"鍛造進度",
		"防御加成",
		"防御設施",
		"鐵礦",
		"石料",
		"木材",
		"食物",
		"焦炭",
		"草藥",
		"烏木",
		"異草",
		"稀金",
		"玄鐵",
		"連接",
		"連接1",
		"連接2",
		"連接3",
		"連接4",
		"連接5",
		"連接6",
		"連接7",
		"連接8",
		"連接9",
		"未使用",
		"未使用1",
		"未使用2",
		"未使用3",
		"未使用4",
		"未使用5",
		"未使用6",
		"未使用7",
		"未使用8",
		"未使用9");
    auto getDefault = []()
    {
        SubMapInfo nextLineData;
        nextLineData.ID = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
        nextLineData.ExitMusic = 0;
        nextLineData.EntranceMusic = -1;
        nextLineData.JumpSubMap = -1;
        nextLineData.EntranceCondition = 0;
        nextLineData.MainEntranceX1 = 0;
        nextLineData.MainEntranceY1 = 0;
        nextLineData.MainEntranceX2 = 0;
        nextLineData.MainEntranceY2 = 0;
        nextLineData.EntranceX = 0;
        nextLineData.EntranceY = 0;
        for (int j = 0; j < 3; j++)
        {
            nextLineData.ExitX[j] = -1;
        }
        for (int j = 0; j < 3; j++)
        {
            nextLineData.ExitY[j] = -1;
        }
		nextLineData.Mapmode = 0;
			nextLineData.unuse = 0; 
			nextLineData.menpai = 0;
			nextLineData.inbattle = 0;
			nextLineData.zlwc = 0; 
			nextLineData.lwc = 0; 
			nextLineData.zcjg = 0; 
			nextLineData.cjg = 0;
			nextLineData.lwcx[0] = 0;
			nextLineData.lwcx[1] = 0;
			nextLineData.lwcx[2] = 0;
			nextLineData.lwcx[3] = 0;
			nextLineData.lwcx[4] = 0;
			nextLineData.lwcy[0] = 0;
			nextLineData.lwcy[1] = 0;
			nextLineData.lwcy[2] = 0;
			nextLineData.lwcy[3] = 0;
			nextLineData.lwcy[4] = 0;
			nextLineData.cjgx[0] = 0;
			nextLineData.cjgx[1] = 0;
			nextLineData.cjgx[2] = 0;
			nextLineData.cjgx[3] = 0;
			nextLineData.cjgx[4] = 0;
			nextLineData.cjgy[0] = 0;
			nextLineData.cjgy[1] = 0;
			nextLineData.cjgy[2] = 0;
			nextLineData.cjgy[3] = 0;
			nextLineData.cjgy[4] = 0;
			nextLineData.bgskg = 0;
			nextLineData.bgsx = 0;
			nextLineData.bgsy = 0;
			nextLineData.ldlkg = 0;
			nextLineData.ldlx = 0; 
			nextLineData.ldly = 0;
			nextLineData.bqckg = 0;
			nextLineData.bqcx = 0;
			nextLineData.bqcy = 0;
			nextLineData.qizhix = 0;
			nextLineData.qizhiy = 0;
			nextLineData.ldjd = 0;
			nextLineData.dzjd = 0;
			nextLineData.fyjc = 0;
			nextLineData.fyss = 0;
			nextLineData.addziyuan[0] = 0;
			nextLineData.addziyuan[1] = 0;
			nextLineData.addziyuan[2] = 0;
			nextLineData.addziyuan[3] = 0;
			nextLineData.addziyuan[4] = 0;
			nextLineData.addziyuan[5] = 0;
			nextLineData.addziyuan[6] = 0;
			nextLineData.addziyuan[7] = 0;
			nextLineData.addziyuan[8] = 0;
			nextLineData.addziyuan[9] = 0;
			nextLineData.lianjie[0] = 0;
			nextLineData.lianjie[1] = 0;
			nextLineData.lianjie[2] = 0;
			nextLineData.lianjie[3] = 0;
			nextLineData.lianjie[4] = 0;
			nextLineData.lianjie[5] = 0;
			nextLineData.lianjie[6] = 0;
			nextLineData.lianjie[7] = 0;
			nextLineData.lianjie[8] = 0;
			nextLineData.lianjie[9] = 0;
			nextLineData.unused[0] = 0;
			nextLineData.unused[1] = 0;
			nextLineData.unused[2] = 0;
			nextLineData.unused[3] = 0;
			nextLineData.unused[4] = 0;
			nextLineData.unused[5] = 0;
			nextLineData.unused[6] = 0;
			nextLineData.unused[7] = 0;
			nextLineData.unused[8] = 0;
			nextLineData.unused[9] = 0;
        return nextLineData;
    };
    char* Name__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        Name__,
        nextLineData.ExitMusic,
        nextLineData.EntranceMusic,
        nextLineData.JumpSubMap,
        nextLineData.EntranceCondition,
        nextLineData.MainEntranceX1,
        nextLineData.MainEntranceY1,
        nextLineData.MainEntranceX2,
        nextLineData.MainEntranceY2,
        nextLineData.EntranceX,
        nextLineData.EntranceY,
        nextLineData.ExitX[0],
        nextLineData.ExitX[1],
        nextLineData.ExitX[2],
        nextLineData.ExitY[0],
        nextLineData.ExitY[1],
        nextLineData.ExitY[2],
		nextLineData.Mapmode,
		nextLineData.unuse,
		nextLineData.menpai,
		nextLineData.inbattle,
		nextLineData.zlwc,
		nextLineData.lwc,
		nextLineData.zcjg,
		nextLineData.cjg,
		nextLineData.lwcx[0],
		nextLineData.lwcx[1],
		nextLineData.lwcx[2],
		nextLineData.lwcx[3],
		nextLineData.lwcx[4],
		nextLineData.lwcy[0],
		nextLineData.lwcy[1],
		nextLineData.lwcy[2],
		nextLineData.lwcy[3],
		nextLineData.lwcy[4],
		nextLineData.cjgx[0],
		nextLineData.cjgx[1],
		nextLineData.cjgx[2],
		nextLineData.cjgx[3],
		nextLineData.cjgx[4],
		nextLineData.cjgy[0],
		nextLineData.cjgy[1],
		nextLineData.cjgy[2],
		nextLineData.cjgy[3],
		nextLineData.cjgy[4],
		nextLineData.bgskg,
		nextLineData.bgsx,
		nextLineData.bgsy,
		nextLineData.ldlkg,
		nextLineData.ldlx,
		nextLineData.ldly,
		nextLineData.bqckg,
		nextLineData.bqcx,
		nextLineData.bqcy,
		nextLineData.qizhix,
		nextLineData.qizhiy,
		nextLineData.ldjd,
		nextLineData.dzjd,
		nextLineData.fyjc,
		nextLineData.fyss,
		nextLineData.addziyuan[0],
		nextLineData.addziyuan[1],
		nextLineData.addziyuan[2],
		nextLineData.addziyuan[3],
		nextLineData.addziyuan[4],
		nextLineData.addziyuan[5],
		nextLineData.addziyuan[6],
		nextLineData.addziyuan[7],
		nextLineData.addziyuan[8],
		nextLineData.addziyuan[9],
		nextLineData.lianjie[0],
		nextLineData.lianjie[1],
		nextLineData.lianjie[2],
		nextLineData.lianjie[3],
		nextLineData.lianjie[4],
		nextLineData.lianjie[5],
		nextLineData.lianjie[6],
		nextLineData.lianjie[7],
		nextLineData.lianjie[8],
		nextLineData.lianjie[9],
		nextLineData.unused[0],
		nextLineData.unused[1],
		nextLineData.unused[2],
		nextLineData.unused[3],
		nextLineData.unused[4],
		nextLineData.unused[5],
		nextLineData.unused[6],
		nextLineData.unused[7],
		nextLineData.unused[8],
		nextLineData.unused[9]))
    {
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
// 武功
void NewSave::LoadCSVMagicSave(std::vector<Magic>& data, int record)
{
    data.clear();
    io::CSVReader<114, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_武功.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
		"编号",
		"名稱",
		"無用",
		"秘笈",
		"需要生命",
		"最小距離",
		"是否單圖特效",
		"事件",
		"出招音效",
		"武功類型",
		"武功動畫&音效",
		"內力類型",
		"攻擊范圍",
		"消耗內力點數",
		"敵人中毒點數",
		"最小攻擊力",
		"最大攻擊力",
		"威力系數",
		"攻擊力比重",
		"內力比重",
		"輕功比重",
		"兵器值比重",
		"是否秘傳",
		"加內力比例",
		"加生命比例",
		"1級移動范圍",
		"2級移動范圍",
		"3級移動范圍",
		"4級移動范圍",
		"5級移動范圍",
		"6級移動范圍",
		"7級移動范圍",
		"8級移動范圍",
		"9級移動范圍",
		"10級移動范圍",
		"1級殺傷范圍",
		"2級殺傷范圍",
		"3級殺傷范圍",
		"4級殺傷范圍",
		"5級殺傷范圍",
		"6級殺傷范圍",
		"7級殺傷范圍",
		"8級殺傷范圍",
		"9級殺傷范圍",
		"10級殺傷范圍",
		"1級加生命",
		"2級加生命",
		"3級加生命",
		"1級加內力",
		"2級加內力",
		"3級加內力",
		"1級加攻擊",
		"2級加攻擊",
		"3級加攻擊",
		"1級加防御",
		"2級加防御",
		"3級加防御",
		"1級加輕功",
		"2級加輕功",
		"3級加輕功",
		"最小封穴幾率",
		"最大封穴幾率",
		"最小內傷幾率",
		"最大內傷幾率",
		"增加醫療",
		"增加毒術",
		"增加技藝",
		"增加抗毒",
		"增加拳掌",
		"增加御劍",
		"增加耍刀",
		"增加特殊",
		"增加暗器",
		"狀態",
		"1級需經驗",
		"2級需經驗",
		"3級需經驗",
		"最高等級",
		"說明",
		"招式",
		"招式1",
		"招式2",
		"招式3",
		"招式4",
		"特殊",
		"特殊1",
		"特殊2",
		"特殊3",
		"特殊4",
		"特殊5",
		"特殊6",
		"特殊7",
		"特殊8",
		"特殊9",
		"特殊類型",
		"特殊類型1",
		"特殊類型2",
		"特殊類型3",
		"特殊類型4",
		"特殊類型5",
		"特殊類型6",
		"特殊類型7",
		"特殊類型8",
		"特殊類型9",
		"需要貢獻",
		"未使用",
		"未使用1",
		"未使用2",
		"未使用3",
		"未使用4",
		"未使用5",
		"未使用6",
		"未使用7",
		"未使用8");
    auto getDefault = []()
    {
        Magic nextLineData;
        nextLineData.ID = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
		nextLineData.useness = 0;
			nextLineData.miji = 0;
			nextLineData.NeedHP = 0;
			nextLineData.MinStep = 0;
			nextLineData.bigami = 0;
			nextLineData.EventNum = 0;
        nextLineData.SoundID = 0;
        nextLineData.MagicType = 0;
        nextLineData.EffectID = 0;
        nextLineData.HurtType = 0;
        nextLineData.AttackAreaType = 0;
        nextLineData.NeedMP = 0;
        nextLineData.WithPoison = 0;
		nextLineData.MinHurt = 0;
			nextLineData.MaxHurt = 0;
			nextLineData.HurtModulus = 0;
			nextLineData.AttackModulus = 0;
			nextLineData.MPModulus = 0;
			nextLineData.SpeedModulus = 0;
			nextLineData.WeaponModulus = 0;
		nextLineData.Ismichuan = 0;
			nextLineData.AddMpScale = 0;
			nextLineData.AddHpScale = 0;

        for (int j = 0; j < 10; j++)
        {
            nextLineData.SelectDistance[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.AttackDistance[j] = 0;
        }
        for (int j = 0; j < 3; j++)
        {
            nextLineData.AddHP[j] = 0;
        }
        for (int j = 0; j < 3; j++)
        {
            nextLineData.AddMP[j] = 0;
        }
		for (int j = 0; j < 3; j++)
		{
			nextLineData.AddAtt[j] = 0;
		}
		for (int j = 0; j < 3; j++)
		{
			nextLineData.AddDef[j] = 0;
		}
		for (int j = 0; j < 3; j++)
		{
			nextLineData.AddSpd[j] = 0;
		}
		nextLineData.MinPeg = 0; 
			nextLineData.MaxPeg = 0; 
			nextLineData.MinInjury = 0; 
			nextLineData.MaxInjury = 0; 
			nextLineData.AddMedcine = 0; 
			nextLineData.AddUsePoi = 0; 
			nextLineData.AddMedPoi = 0; 
			nextLineData.AddDefPoi = 0;
		nextLineData.AddFist = 0; 
			nextLineData.AddSword = 0; 
			nextLineData.AddKnife = 0; 
			nextLineData.AddUnusual = 0; 
			nextLineData.AddHidWeapon = 0; 
			nextLineData.BattleState = 0;
			for (int j = 0; j < 3; j++)
			{
				nextLineData.NeedExp[j] = 0;
			}
		nextLineData.MaxLevel = 0;
		memset(nextLineData.Introduction, '\0', sizeof(nextLineData.Introduction));
		nextLineData.Zhaoshi[5] = 0;
		for (int j = 0; j < 10; j++)
		{
			nextLineData.Teshu[j] = 0;
		}
		for (int j = 0; j < 10; j++)
		{
			nextLineData.Teshumod[j] = 0;
		}
		for (int j = 0; j < 10; j++)
		{
			nextLineData.unused[j] = 0;
		}

        return nextLineData;
    };
    char* Name__;
	char* Introduction__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        Name__,
		nextLineData.useness,
		nextLineData.miji,
		nextLineData.NeedHP,
		nextLineData.MinStep,
		nextLineData.bigami,
		nextLineData.EventNum,
        nextLineData.SoundID,
        nextLineData.MagicType,
        nextLineData.EffectID,
        nextLineData.HurtType,
        nextLineData.AttackAreaType,
        nextLineData.NeedMP,
        nextLineData.WithPoison,
		nextLineData.MinHurt, 
		nextLineData.MaxHurt,
		nextLineData.HurtModulus,
		nextLineData.AttackModulus,
		nextLineData.MPModulus,
		nextLineData.SpeedModulus,
		nextLineData.WeaponModulus,
		nextLineData.Ismichuan,
		nextLineData.AddMpScale,
		nextLineData.AddHpScale,
        nextLineData.SelectDistance[0],
        nextLineData.SelectDistance[1],
        nextLineData.SelectDistance[2],
        nextLineData.SelectDistance[3],
        nextLineData.SelectDistance[4],
        nextLineData.SelectDistance[5],
        nextLineData.SelectDistance[6],
        nextLineData.SelectDistance[7],
        nextLineData.SelectDistance[8],
        nextLineData.SelectDistance[9],
        nextLineData.AttackDistance[0],
        nextLineData.AttackDistance[1],
        nextLineData.AttackDistance[2],
        nextLineData.AttackDistance[3],
        nextLineData.AttackDistance[4],
        nextLineData.AttackDistance[5],
        nextLineData.AttackDistance[6],
        nextLineData.AttackDistance[7],
        nextLineData.AttackDistance[8],
        nextLineData.AttackDistance[9],
        nextLineData.AddHP[0],
        nextLineData.AddHP[1],
        nextLineData.AddHP[2],
        nextLineData.AddMP[0],
        nextLineData.AddMP[1],
        nextLineData.AddMP[2],
		nextLineData.AddAtt[0],
		nextLineData.AddAtt[1],
		nextLineData.AddAtt[2],
		nextLineData.AddDef[0],
		nextLineData.AddDef[1],
		nextLineData.AddDef[2],
		nextLineData.AddSpd[0],
		nextLineData.AddSpd[1],
		nextLineData.AddSpd[2],
		nextLineData.MinPeg,
		nextLineData.MaxPeg,
		nextLineData.MinInjury,
		nextLineData.MaxInjury,
		nextLineData.AddMedcine,
		nextLineData.AddUsePoi,
		nextLineData.AddMedPoi,
		nextLineData.AddDefPoi,
		nextLineData.AddFist,
		nextLineData.AddSword,
		nextLineData.AddKnife,
		nextLineData.AddUnusual,
		nextLineData.AddHidWeapon,
		nextLineData.BattleState,
		nextLineData.NeedExp[0],
		nextLineData.NeedExp[1],
		nextLineData.NeedExp[2],
		nextLineData.MaxLevel,
		Introduction__,
		nextLineData.Zhaoshi[0],
		nextLineData.Zhaoshi[1],
		nextLineData.Zhaoshi[2],
		nextLineData.Zhaoshi[3],
		nextLineData.Zhaoshi[4],
		nextLineData.Teshu[0],
		nextLineData.Teshu[1],
		nextLineData.Teshu[2],
		nextLineData.Teshu[3],
		nextLineData.Teshu[4],
		nextLineData.Teshu[5],
		nextLineData.Teshu[6],
		nextLineData.Teshu[7],
		nextLineData.Teshu[8],
		nextLineData.Teshu[9],
		nextLineData.Teshumod[0],
		nextLineData.Teshumod[1],
		nextLineData.Teshumod[2],
		nextLineData.Teshumod[3],
		nextLineData.Teshumod[4],
		nextLineData.Teshumod[5],
		nextLineData.Teshumod[6],
		nextLineData.Teshumod[7],
		nextLineData.Teshumod[8],
		nextLineData.Teshumod[9],
		nextLineData.unused[0],
		nextLineData.unused[1],
		nextLineData.unused[2],
		nextLineData.unused[3],
		nextLineData.unused[4],
		nextLineData.unused[5],
		nextLineData.unused[6],
		nextLineData.unused[7],
		nextLineData.unused[8],
		nextLineData.unused[9]))
    {
		strncpy(nextLineData.Introduction, Introduction__, sizeof(nextLineData.Introduction) - 1);
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
// 商店
void NewSave::LoadCSVShopSave(std::vector<Shop>& data, int record)
{
    data.clear();
    io::CSVReader<72, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_商店.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
		"物品",
		"物品個數",
		"物品1",
		"物品個數1",
		"物品2",
		"物品個數2",
		"物品3",
		"物品個數3",
		"物品4",
		"物品個數4",
		"物品5",
		"物品個數5",
		"物品6",
		"物品個數6",
		"物品7",
		"物品個數7",
		"物品8",
		"物品個數8",
		"物品9",
		"物品個數9",
		"物品10",
		"物品個數10",
		"物品11",
		"物品個數11",
		"物品12",
		"物品個數12",
		"物品13",
		"物品個數13",
		"物品14",
		"物品個數14",
		"物品15",
		"物品個數15",
		"物品16",
		"物品個數16",
		"物品17",
		"物品個數17",
		"物品18",
		"物品個數18",
		"物品19",
		"物品個數19",
		"物品20",
		"物品個數20",
		"物品21",
		"物品個數21",
		"物品22",
		"物品個數22",
		"物品23",
		"物品個數23",
		"物品24",
		"物品個數24",
		"物品25",
		"物品個數25",
		"物品26",
		"物品個數26",
		"物品27",
		"物品個數27",
		"物品28",
		"物品個數28",
		"物品29",
		"物品個數29",
		"物品30",
		"物品個數30",
		"物品31",
		"物品個數31",
		"物品32",
		"物品個數32",
		"物品33",
		"物品個數33",
		"物品34",
		"物品個數34",
		"物品35",
		"物品個數35");
    auto getDefault = []()
    {
        Shop nextLineData;
        for (int j = 0; j < 36; j++)
        {
            nextLineData.ItemID[j] = -1;
			nextLineData.ItermCount[j] = -1;
        }
        return nextLineData;
    };
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ItemID[0],
		nextLineData.ItermCount[0],
		nextLineData.ItemID[1],
		nextLineData.ItermCount[1],
		nextLineData.ItemID[2],
		nextLineData.ItermCount[2],
		nextLineData.ItemID[3],
		nextLineData.ItermCount[3],
		nextLineData.ItemID[4],
		nextLineData.ItermCount[4],
		nextLineData.ItemID[5],
		nextLineData.ItermCount[5],
		nextLineData.ItemID[6],
		nextLineData.ItermCount[6],
		nextLineData.ItemID[7],
		nextLineData.ItermCount[7],
		nextLineData.ItemID[8],
		nextLineData.ItermCount[8],
		nextLineData.ItemID[9],
		nextLineData.ItermCount[9],
		nextLineData.ItemID[10],
		nextLineData.ItermCount[10],
		nextLineData.ItemID[11],
		nextLineData.ItermCount[11],
		nextLineData.ItemID[12],
		nextLineData.ItermCount[12],
		nextLineData.ItemID[13],
		nextLineData.ItermCount[13],
		nextLineData.ItemID[14],
		nextLineData.ItermCount[14],
		nextLineData.ItemID[15],
		nextLineData.ItermCount[15],
		nextLineData.ItemID[16],
		nextLineData.ItermCount[16],
		nextLineData.ItemID[17],
		nextLineData.ItermCount[17],
		nextLineData.ItemID[18],
		nextLineData.ItermCount[18],
		nextLineData.ItemID[19],
		nextLineData.ItermCount[19],
		nextLineData.ItemID[20],
		nextLineData.ItermCount[20],
		nextLineData.ItemID[21],
		nextLineData.ItermCount[21],
		nextLineData.ItemID[22],
		nextLineData.ItermCount[22],
		nextLineData.ItemID[23],
		nextLineData.ItermCount[23],
		nextLineData.ItemID[24],
		nextLineData.ItermCount[24],
		nextLineData.ItemID[25],
		nextLineData.ItermCount[25],
		nextLineData.ItemID[26],
		nextLineData.ItermCount[26],
		nextLineData.ItemID[27],
		nextLineData.ItermCount[27],
		nextLineData.ItemID[28],
		nextLineData.ItermCount[28],
		nextLineData.ItemID[29],
		nextLineData.ItermCount[29],
		nextLineData.ItemID[30],
		nextLineData.ItermCount[30],
		nextLineData.ItemID[31],
		nextLineData.ItermCount[31],
		nextLineData.ItemID[32],
		nextLineData.ItermCount[32],
		nextLineData.ItemID[33],
		nextLineData.ItermCount[33],
		nextLineData.ItemID[34],
		nextLineData.ItermCount[34],
		nextLineData.ItemID[35],
		nextLineData.ItermCount[35]))
    {
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}

void NewSave::LoadCSVTimeSave(std::vector<TimeInfoSave>& data, int record)
{
	data.clear();
	io::CSVReader<5, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_日期.csv");
	in.read_header(io::ignore_missing_column | io::ignore_extra_column,
		"甲子",
		"年",
		"月",
		"日",
		"时");
	auto getDefault = []()
	{
		TimeInfoSave nextLineData;
		nextLineData.Jiazi = -1;
		nextLineData.Year = -1;
		nextLineData.Month = -1;
		nextLineData.Day = -1;
		nextLineData.Hour = -1;
		return nextLineData;
	};
	int lines = 0;
	auto nextLineData = getDefault();
	while (in.read_row(
		nextLineData.Jiazi,
		nextLineData.Year,
		nextLineData.Month,
		nextLineData.Day,
		nextLineData.Hour))
	{
		data.push_back(nextLineData);
		lines++;
		nextLineData = getDefault();
	}
}

void NewSave::LoadCSVZhaoshiSave(std::vector<ZhaoshiInfoSave>& data, int record)
{
	data.clear();
	io::CSVReader<57, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_招式.csv");
	in.read_header(io::ignore_missing_column | io::ignore_extra_column,
		"编号",
		"從屬",
		"順位",
		"名稱",
		"是否攻擊",
		"攻擊加成",
		"是否防御",
		"防御加成",
		"說明",
		"特效類型",
		"特效數據",
		"特效類型1",
		"特效數據1",
		"特效類型2",
		"特效數據2",
		"特效類型3",
		"特效數據3",
		"特效類型4",
		"特效數據4",
		"特效類型5",
		"特效數據5",
		"特效類型6",
		"特效數據6",
		"特效類型7",
		"特效數據7",
		"特效類型8",
		"特效數據8",
		"特效類型9",
		"特效數據9",
		"特效類型10",
		"特效數據10",
		"特效類型11",
		"特效數據11",
		"特效類型12",
		"特效數據12",
		"特效類型13",
		"特效數據13",
		"特效類型14",
		"特效數據14",
		"特效類型15",
		"特效數據15",
		"特效類型16",
		"特效數據16",
		"特效類型17",
		"特效數據17",
		"特效類型18",
		"特效數據18",
		"特效類型19",
		"特效數據19",
		"特效類型20",
		"特效數據20",
		"特效類型21",
		"特效數據21",
		"特效類型22",
		"特效數據22",
		"特效類型23",
		"特效數據23");
	auto getDefault = []()
	{
		ZhaoshiInfoSave nextLineData;
		nextLineData.daihao = -1;
			nextLineData.congshu = -1;
			nextLineData.shunwei = -1;
		memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
		nextLineData.ygongji = -1;
		nextLineData.gongji = -1;
		nextLineData.yfangyu = -1;
			nextLineData.fangyu = -1;
		memset(nextLineData.Introduction, '\0', sizeof(nextLineData.Introduction));
		for (int j = 0; j < 24; j++) {
			nextLineData.texiao[j].Type = -1;
			nextLineData.texiao[j].Value = -1;
		}

		return nextLineData;
	};
	int lines = 0;
	char* Name__;
	char* Introduction__;
	auto nextLineData = getDefault();
	while (in.read_row(
		nextLineData.daihao,
		nextLineData.congshu,
	nextLineData.shunwei,
	Name__,
	nextLineData.ygongji,
	nextLineData.gongji,
	nextLineData.yfangyu,
	nextLineData.fangyu,
		Introduction__,
		nextLineData.texiao[0].Type,
		nextLineData.texiao[0].Value,
		nextLineData.texiao[1].Type,
		nextLineData.texiao[1].Value,
		nextLineData.texiao[2].Type,
		nextLineData.texiao[2].Value,
		nextLineData.texiao[3].Type,
		nextLineData.texiao[3].Value,
		nextLineData.texiao[4].Type,
		nextLineData.texiao[4].Value,
		nextLineData.texiao[5].Type,
		nextLineData.texiao[5].Value,
		nextLineData.texiao[6].Type,
		nextLineData.texiao[6].Value,
		nextLineData.texiao[7].Type,
		nextLineData.texiao[7].Value,
		nextLineData.texiao[8].Type,
		nextLineData.texiao[8].Value,
		nextLineData.texiao[9].Type,
		nextLineData.texiao[9].Value,
		nextLineData.texiao[10].Type,
		nextLineData.texiao[10].Value,
		nextLineData.texiao[11].Type,
		nextLineData.texiao[11].Value,
		nextLineData.texiao[12].Type,
		nextLineData.texiao[12].Value,
		nextLineData.texiao[13].Type,
		nextLineData.texiao[13].Value,
		nextLineData.texiao[14].Type,
		nextLineData.texiao[14].Value,
		nextLineData.texiao[15].Type,
		nextLineData.texiao[15].Value,
		nextLineData.texiao[16].Type,
		nextLineData.texiao[16].Value,
		nextLineData.texiao[17].Type,
		nextLineData.texiao[17].Value,
		nextLineData.texiao[18].Type,
		nextLineData.texiao[18].Value,
		nextLineData.texiao[19].Type,
		nextLineData.texiao[19].Value,
		nextLineData.texiao[20].Type,
		nextLineData.texiao[20].Value,
		nextLineData.texiao[21].Type,
		nextLineData.texiao[21].Value,
		nextLineData.texiao[22].Type,
		nextLineData.texiao[22].Value,
		nextLineData.texiao[23].Type,
		nextLineData.texiao[23].Value))
	{
		strncpy(nextLineData.Introduction, Introduction__, sizeof(nextLineData.Introduction) - 1);
		strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
		data.push_back(nextLineData);
		lines++;
		nextLineData = getDefault();
	}
}


void NewSave::LoadCSVMenpaiSave(std::vector<MenpaiInfoSave>& data, int record)
{
	data.clear();
	io::CSVReader<114, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_门派.csv");
	in.read_header(io::ignore_missing_column | io::ignore_extra_column,
		"编号",
		"名稱",
		"據點個數",
		"總舵",
		"掌門人",
		"弟子個數",
		"門派聲望",
		"門派善惡",
		"鐵礦",
		"石料",
		"木材",
		"食物",
		"焦炭",
		"草藥",
		"烏木",
		"異草",
		"稀金",
		"玄鐵",
		"鐵礦+",
		"石料+",
		"木材+",
		"食物+",
		"焦炭+",
		"草藥+",
		"烏木+",
		"異草+",
		"稀金+",
		"玄鐵+",
		"門派內功",
		"門派內功1",
		"門派內功2",
		"門派內功3",
		"門派內功4",
		"門派內功5",
		"門派內功6",
		"門派內功7",
		"門派內功8",
		"門派內功9",
		"門派內功10",
		"門派內功11",
		"門派內功12",
		"門派內功13",
		"門派內功14",
		"門派內功15",
		"門派內功16",
		"門派內功17",
		"門派內功18",
		"門派內功19",
		"無",
		"蓬萊",
		"青城",
		"武當",
		"少林",
		"全真",
		"峨眉",
		"昆侖",
		"崆峒",
		"嵩山",
		"華山",
		"衡山",
		"泰山",
		"恒山",
		"血刀",
		"鐵掌",
		"丐幫",
		"五毒",
		"明教",
		"日月",
		"雪山",
		"暫無",
		"暫無1",
		"暫無2",
		"暫無3",
		"暫無4",
		"逍遙",
		"星宿",
		"長樂",
		"馬家",
		"劍霞",
		"八極",
		"伏牛",
		"無量",
		"野狐",
		"野馬",
		"地躺",
		"慧劍",
		"惡豹",
		"劍宗",
		"門派職務",
		"門派職務1",
		"門派職務2",
		"門派職務3",
		"門派職務4",
		"門派職務5",
		"門派職務6",
		"門派職務7",
		"門派職務8",
		"門派職務9",
		"擴張傾向",
		"招弟子傾向",
		"弟子成長速度",
		"旗幟",
		"男弟子起圖",
		"男弟子頭像",
		"女弟子起圖",
		"女弟子頭像",
		"弟子性別",
		"弟子身份",
		"滅門事件",
		"同盟",
		"查找標記",
		"未定義2",
		"未定義3",
		"未定義4");
	auto getDefault = []()
	{
		MenpaiInfoSave nextLineData;
		nextLineData.num = -1;
		memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
		nextLineData.jvdian = -1;
			nextLineData.zongduo = -1;
			nextLineData.zmr = -1;
			nextLineData.dizi = -1;
			nextLineData.shengwang = -1;
			nextLineData.shane = -1;
			for (int j = 0; j < 10; j++) {
				nextLineData.ziyuan[j] = -1;
			};
		for (int j = 0; j < 10; j++) {
			nextLineData.aziyuan[j] = -1;
		};
		for (int j = 0; j < 20; j++) {
			nextLineData.neigong[j] = -1;
		};
		for (int j = 0; j < 40; j++) {
			nextLineData.guanxi[j] = -1;
		};
		for (int j = 0; j < 10; j++) {
			nextLineData.zhiwu[j] = -1;
		};
		nextLineData.kzq = -1; 
			nextLineData.dzq = -1;
			nextLineData.czsd = -1;
			nextLineData.qizhi = -1;
			nextLineData.mdizigrp = -1;
			nextLineData.mdizipic = -1;
			nextLineData.fdizigrp = -1;
			nextLineData.fdizipic = -1;
			nextLineData.sexy = -1;
			nextLineData.identity = -1;
			nextLineData.endevent = -1;
			nextLineData.tongmeng = -1;
			nextLineData.israndomed = -1;
			nextLineData.unuse2 = -1;
			nextLineData.unuse3 = -1;
			nextLineData.unuse4 = -1;
		return nextLineData;
	};
	int lines = 0;
	char* Name__;
	auto nextLineData = getDefault();
	while (in.read_row(
		nextLineData.num,
		Name__,
		nextLineData.jvdian,
		nextLineData.zongduo,
		nextLineData.zmr,
		nextLineData.dizi,
		nextLineData.shengwang,
		nextLineData.shane,
		nextLineData.ziyuan[0],
		nextLineData.ziyuan[1],
		nextLineData.ziyuan[2],
		nextLineData.ziyuan[3],
		nextLineData.ziyuan[4],
		nextLineData.ziyuan[5],
		nextLineData.ziyuan[6],
		nextLineData.ziyuan[7],
		nextLineData.ziyuan[8],
		nextLineData.ziyuan[9],
		nextLineData.aziyuan[0],
		nextLineData.aziyuan[1],
		nextLineData.aziyuan[2],
		nextLineData.aziyuan[3],
		nextLineData.aziyuan[4],
		nextLineData.aziyuan[5],
		nextLineData.aziyuan[6],
		nextLineData.aziyuan[7],
		nextLineData.aziyuan[8],
		nextLineData.aziyuan[9],
		nextLineData.neigong[0],
		nextLineData.neigong[1],
		nextLineData.neigong[2],
		nextLineData.neigong[3],
		nextLineData.neigong[4],
		nextLineData.neigong[5],
		nextLineData.neigong[6],
		nextLineData.neigong[7],
		nextLineData.neigong[8],
		nextLineData.neigong[9],
		nextLineData.neigong[10],
		nextLineData.neigong[11],
		nextLineData.neigong[12],
		nextLineData.neigong[13],
		nextLineData.neigong[14],
		nextLineData.neigong[15],
		nextLineData.neigong[16],
		nextLineData.neigong[17],
		nextLineData.neigong[18],
		nextLineData.neigong[19],
		nextLineData.guanxi[0],
		nextLineData.guanxi[1],
		nextLineData.guanxi[2],
		nextLineData.guanxi[3],
		nextLineData.guanxi[4],
		nextLineData.guanxi[5],
		nextLineData.guanxi[6],
		nextLineData.guanxi[7],
		nextLineData.guanxi[8],
		nextLineData.guanxi[9],
		nextLineData.guanxi[10],
		nextLineData.guanxi[11],
		nextLineData.guanxi[12],
		nextLineData.guanxi[13],
		nextLineData.guanxi[14],
		nextLineData.guanxi[15],
		nextLineData.guanxi[16],
		nextLineData.guanxi[17],
		nextLineData.guanxi[18],
		nextLineData.guanxi[19],
		nextLineData.guanxi[20],
		nextLineData.guanxi[21],
		nextLineData.guanxi[22],
		nextLineData.guanxi[23],
		nextLineData.guanxi[24],
		nextLineData.guanxi[25],
		nextLineData.guanxi[26],
		nextLineData.guanxi[27],
		nextLineData.guanxi[28],
		nextLineData.guanxi[29],
		nextLineData.guanxi[30],
		nextLineData.guanxi[31],
		nextLineData.guanxi[32],
		nextLineData.guanxi[33],
		nextLineData.guanxi[34],
		nextLineData.guanxi[35],
		nextLineData.guanxi[36],
		nextLineData.guanxi[37],
		nextLineData.guanxi[38],
		nextLineData.guanxi[39],
		nextLineData.zhiwu[0],
		nextLineData.zhiwu[1],
		nextLineData.zhiwu[2],
		nextLineData.zhiwu[3],
		nextLineData.zhiwu[4],
		nextLineData.zhiwu[5],
		nextLineData.zhiwu[6],
		nextLineData.zhiwu[7],
		nextLineData.zhiwu[8],
		nextLineData.zhiwu[9],
		nextLineData.kzq,
		nextLineData.dzq,
		nextLineData.czsd,
		nextLineData.qizhi,
		nextLineData.mdizigrp,
		nextLineData.mdizipic,
		nextLineData.fdizigrp,
		nextLineData.fdizipic,
		nextLineData.sexy,
		nextLineData.identity,
		nextLineData.endevent,
		nextLineData.tongmeng,
		nextLineData.israndomed,
		nextLineData.unuse2,
		nextLineData.unuse3,
		nextLineData.unuse4))
	{
		strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
		data.push_back(nextLineData);
		lines++;
		nextLineData = getDefault();
	}
}


void NewSave::LoadCSVRSignSave(std::vector<RSign>& data, int record)
{
	data.clear();
	io::CSVReader<55, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_招式.csv");
	in.read_header(io::ignore_missing_column | io::ignore_extra_column,
		"代號",
		"名稱",
		"效果",
		"類別",
		"說明",
		"說明",
		"是否隱藏",
		"特效類型",
		"特效數據",
		"特效類型1",
		"特效數據1",
		"特效類型2",
		"特效數據2",
		"特效類型3",
		"特效數據3",
		"特效類型4",
		"特效數據4",
		"特效類型5",
		"特效數據5",
		"特效類型6",
		"特效數據6",
		"特效類型7",
		"特效數據7",
		"特效類型8",
		"特效數據8",
		"特效類型9",
		"特效數據9",
		"特效類型10",
		"特效數據10",
		"特效類型11",
		"特效數據11",
		"特效類型12",
		"特效數據12",
		"特效類型13",
		"特效數據13",
		"特效類型14",
		"特效數據14",
		"特效類型15",
		"特效數據15",
		"特效類型16",
		"特效數據16",
		"特效類型17",
		"特效數據17",
		"特效類型18",
		"特效數據18",
		"特效類型19",
		"特效數據19",
		"特效類型20",
		"特效數據20",
		"特效類型21",
		"特效數據21",
		"特效類型22",
		"特效數據22",
		"特效類型23",
		"特效數據23");
	auto getDefault = []()
	{
		RSign nextLineData;
		nextLineData.num = -1;
		memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
		nextLineData.effert = -1;
		nextLineData.TypeNum = -1;
		memset(nextLineData.Introduction, '\0', sizeof(nextLineData.Introduction));
		nextLineData.beiyong = -1;
		nextLineData.isshow = -1;
		for (int j = 0; j < 24; j++) {
			nextLineData.texiao[j].Type = -1;
			nextLineData.texiao[j].Value = -1;
		}

		return nextLineData;
	};
	int lines = 0;
	char* Name__;
	char* Introduction__;
	auto nextLineData = getDefault();
	while (in.read_row(
		nextLineData.num,
		Name__,
		nextLineData.effert,
		nextLineData.TypeNum,
		Introduction__,
		nextLineData.beiyong,
			nextLineData.isshow, //44
		nextLineData.texiao[0].Type,
		nextLineData.texiao[0].Value,
		nextLineData.texiao[1].Type,
		nextLineData.texiao[1].Value,
		nextLineData.texiao[2].Type,
		nextLineData.texiao[2].Value,
		nextLineData.texiao[3].Type,
		nextLineData.texiao[3].Value,
		nextLineData.texiao[4].Type,
		nextLineData.texiao[4].Value,
		nextLineData.texiao[5].Type,
		nextLineData.texiao[5].Value,
		nextLineData.texiao[6].Type,
		nextLineData.texiao[6].Value,
		nextLineData.texiao[7].Type,
		nextLineData.texiao[7].Value,
		nextLineData.texiao[8].Type,
		nextLineData.texiao[8].Value,
		nextLineData.texiao[9].Type,
		nextLineData.texiao[9].Value,
		nextLineData.texiao[10].Type,
		nextLineData.texiao[10].Value,
		nextLineData.texiao[11].Type,
		nextLineData.texiao[11].Value,
		nextLineData.texiao[12].Type,
		nextLineData.texiao[12].Value,
		nextLineData.texiao[13].Type,
		nextLineData.texiao[13].Value,
		nextLineData.texiao[14].Type,
		nextLineData.texiao[14].Value,
		nextLineData.texiao[15].Type,
		nextLineData.texiao[15].Value,
		nextLineData.texiao[16].Type,
		nextLineData.texiao[16].Value,
		nextLineData.texiao[17].Type,
		nextLineData.texiao[17].Value,
		nextLineData.texiao[18].Type,
		nextLineData.texiao[18].Value,
		nextLineData.texiao[19].Type,
		nextLineData.texiao[19].Value,
		nextLineData.texiao[20].Type,
		nextLineData.texiao[20].Value,
		nextLineData.texiao[21].Type,
		nextLineData.texiao[21].Value,
		nextLineData.texiao[22].Type,
		nextLineData.texiao[22].Value,
		nextLineData.texiao[23].Type,
		nextLineData.texiao[23].Value))
	{
		strncpy(nextLineData.Introduction, Introduction__, sizeof(nextLineData.Introduction) - 1);
		strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
		data.push_back(nextLineData);
		lines++;
		nextLineData = getDefault();
	}
}


void NewSave::InsertRoleAt(std::vector<Role>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
    for (int i = 0; i < 1; i++)
    {
        auto p = Save::getInstance();
        if (p->Team[0] >= idx)
        {
            p->Team[0] += 1;
        }
        if (p->Team[1] >= idx)
        {
            p->Team[1] += 1;
        }
        if (p->Team[2] >= idx)
        {
            p->Team[2] += 1;
        }
        if (p->Team[3] >= idx)
        {
            p->Team[3] += 1;
        }
        if (p->Team[4] >= idx)
        {
            p->Team[4] += 1;
        }
        if (p->Team[5] >= idx)
        {
            p->Team[5] += 1;
        }
    }
    for (auto& p : Save::getInstance()->getItems())
    {
        if (p->User >= idx)
        {
            p->User += 1;
        }
        if (p->OnlySuitableRole >= idx)
        {
            p->OnlySuitableRole += 1;
        }
    }
}


void NewSave::InsertItemAt(std::vector<Item>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        auto* p = &(Save::getInstance()->Items[i]);
        if (p->item_id >= idx)
        {
            p->item_id += 1;
        }
    }
    for (auto& p : Save::getInstance()->getRoles())
    {
        if (p->Equip[0] >= idx)
        {
            p->Equip[0] += 1;
        }
        if (p->Equip[1] >= idx)
        {
            p->Equip[1] += 1;
        }
		if (p->Equip[2] >= idx)
		{
			p->Equip[2] += 1;
		}
		if (p->Equip[3] >= idx)
		{
			p->Equip[3] += 1;
		}
        if (p->PracticeItem >= idx)
        {
            p->PracticeItem += 1;
        }
        if (p->TakingItem[0] >= idx)
        {
            p->TakingItem[0] += 1;
        }
        if (p->TakingItem[1] >= idx)
        {
            p->TakingItem[1] += 1;
        }
        if (p->TakingItem[2] >= idx)
        {
            p->TakingItem[2] += 1;
        }
        if (p->TakingItem[3] >= idx)
        {
            p->TakingItem[3] += 1;
        }
    }
    for (auto& p : Save::getInstance()->getItems())
    {
        if (p->NeedMaterial >= idx)
        {
            p->NeedMaterial += 1;
        }
        if (p->MakeItem[0] >= idx)
        {
            p->MakeItem[0] += 1;
        }
        if (p->MakeItem[1] >= idx)
        {
            p->MakeItem[1] += 1;
        }
        if (p->MakeItem[2] >= idx)
        {
            p->MakeItem[2] += 1;
        }
        if (p->MakeItem[3] >= idx)
        {
            p->MakeItem[3] += 1;
        }
        if (p->MakeItem[4] >= idx)
        {
            p->MakeItem[4] += 1;
        }
    }
    for (auto& p : Save::getInstance()->getShops())
    {
        if (p->ItemID[0] >= idx)
        {
            p->ItemID[0] += 1;
        }
        if (p->ItemID[1] >= idx)
        {
            p->ItemID[1] += 1;
        }
        if (p->ItemID[2] >= idx)
        {
            p->ItemID[2] += 1;
        }
        if (p->ItemID[3] >= idx)
        {
            p->ItemID[3] += 1;
        }
        if (p->ItemID[4] >= idx)
        {
            p->ItemID[4] += 1;
        }
    }
}
void NewSave::InsertSubMapInfoAt(std::vector<SubMapInfo>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
}
void NewSave::InsertMagicAt(std::vector<Magic>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
    for (auto& p : Save::getInstance()->getRoles())
    {
        if (p->MagicID[0] >= idx)
        {
            p->MagicID[0] += 1;
        }
        if (p->MagicID[1] >= idx)
        {
            p->MagicID[1] += 1;
        }
        if (p->MagicID[2] >= idx)
        {
            p->MagicID[2] += 1;
        }
        if (p->MagicID[3] >= idx)
        {
            p->MagicID[3] += 1;
        }
        if (p->MagicID[4] >= idx)
        {
            p->MagicID[4] += 1;
        }
        if (p->MagicID[5] >= idx)
        {
            p->MagicID[5] += 1;
        }
        if (p->MagicID[6] >= idx)
        {
            p->MagicID[6] += 1;
        }
        if (p->MagicID[7] >= idx)
        {
            p->MagicID[7] += 1;
        }
        if (p->MagicID[8] >= idx)
        {
            p->MagicID[8] += 1;
        }
        if (p->MagicID[9] >= idx)
        {
            p->MagicID[9] += 1;
        }
    }
    for (auto& p : Save::getInstance()->getItems())
    {
        if (p->MagicID >= idx)
        {
            p->MagicID += 1;
        }
    }
}
void NewSave::InsertShopAt(std::vector<Shop>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        // data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
}

NewSave NewSave::new_save_;

#define GET_OFFSET(field) (int((char*)(&(a.field)) - (char*)(&a)))
#define BIND_FIELD_INT(key, field) FieldInfo(key, 0, GET_OFFSET(field), sizeof(a.field))
#define BIND_FIELD_TEXT(key, field) FieldInfo(key, 1, GET_OFFSET(field), sizeof(a.field))

void NewSave::initDBFieldInfo()
{
    if (new_save_.base_.size() == 0)
    {
        Save::BaseInfo a;
        new_save_.base_ =
        {
            BIND_FIELD_INT("乘船", InShip),
            BIND_FIELD_INT("子场景内", InSubMap),
            BIND_FIELD_INT("主地图X", MainMapX),
            BIND_FIELD_INT("主地图Y", MainMapY),
            BIND_FIELD_INT("子场景X", SubMapX),
            BIND_FIELD_INT("子场景Y", SubMapY),
            BIND_FIELD_INT("面朝方向", FaceTowards),
            BIND_FIELD_INT("船X", ShipX),
            BIND_FIELD_INT("船Y", ShipY),
			BIND_FIELD_INT("船X1",TimeCount),       
			BIND_FIELD_INT("船Y1", TimeEvent),
            BIND_FIELD_INT("内部编码", RandomEvent),
            BIND_FIELD_INT("队友1", Team[0]),
            BIND_FIELD_INT("队友2", Team[1]),
            BIND_FIELD_INT("队友3", Team[2]),
            BIND_FIELD_INT("队友4", Team[3]),
            BIND_FIELD_INT("队友5", Team[4]),
            BIND_FIELD_INT("队友6", Team[5]),
        };
    }
    if (new_save_.item_list_.size() == 0)
    {
        ItemList a;
        new_save_.item_list_ =
        {
            BIND_FIELD_INT("物品编号", item_id),
            BIND_FIELD_INT("物品数量", count),
        };
    }
    if (new_save_.role_.size() == 0)
    {
        Role a;
        new_save_.role_ =
        {
            BIND_FIELD_INT("编号", ID),
            BIND_FIELD_INT("头像", HeadID),
            BIND_FIELD_INT("生命成长", IncLife),
            BIND_FIELD_INT("富源", Fuyuan),
            BIND_FIELD_TEXT("名字", Name),
            BIND_FIELD_TEXT("外号", Nick),
            BIND_FIELD_INT("性别", Sexual),
            BIND_FIELD_INT("等级", Level),
            BIND_FIELD_INT("经验", Exp),
            BIND_FIELD_INT("生命", HP),
            BIND_FIELD_INT("生命最大值", MaxHP),
            BIND_FIELD_INT("内伤", Hurt),
            BIND_FIELD_INT("中毒", Poison),
            BIND_FIELD_INT("体力", PhysicalPower),
            BIND_FIELD_INT("武器", Equip[0]),
            BIND_FIELD_INT("防具", Equip[1]),
			BIND_FIELD_INT("鞋子", Equip[3]),
			BIND_FIELD_INT("饰品", Equip[3]),

            BIND_FIELD_INT("内力性质", MPType),
            BIND_FIELD_INT("内力", MP),
            BIND_FIELD_INT("内力最大值", MaxMP),
            BIND_FIELD_INT("攻击力", Attack),
            BIND_FIELD_INT("轻功", Speed),
            BIND_FIELD_INT("防御力", Defence),
            BIND_FIELD_INT("医疗", Medicine),
            BIND_FIELD_INT("用毒", UsePoison),
            BIND_FIELD_INT("解毒", Detoxification),
            BIND_FIELD_INT("抗毒", AntiPoison),
            BIND_FIELD_INT("拳掌", Fist),
            BIND_FIELD_INT("御剑", Sword),
            BIND_FIELD_INT("耍刀", Knife),
            BIND_FIELD_INT("特殊", Unusual),
            BIND_FIELD_INT("暗器", HiddenWeapon),
            BIND_FIELD_INT("武学常识", Knowledge),
            BIND_FIELD_INT("品德", Morality),
            BIND_FIELD_INT("攻击带毒", AttackWithPoison),
            BIND_FIELD_INT("左右互搏", AttackTwice),
            BIND_FIELD_INT("声望", Fame),
            BIND_FIELD_INT("资质", IQ),
            BIND_FIELD_INT("修炼物品", PracticeItem),
            BIND_FIELD_INT("修炼点数", ExpForItem),
            BIND_FIELD_INT("所会武功1", MagicID[0]),
            BIND_FIELD_INT("所会武功2", MagicID[1]),
            BIND_FIELD_INT("所会武功3", MagicID[2]),
            BIND_FIELD_INT("所会武功4", MagicID[3]),
            BIND_FIELD_INT("所会武功5", MagicID[4]),
            BIND_FIELD_INT("所会武功6", MagicID[5]),
            BIND_FIELD_INT("所会武功7", MagicID[6]),
            BIND_FIELD_INT("所会武功8", MagicID[7]),
            BIND_FIELD_INT("所会武功9", MagicID[8]),
            BIND_FIELD_INT("所会武功10", MagicID[9]),
            BIND_FIELD_INT("武功等级1", MagicLevel[0]),
            BIND_FIELD_INT("武功等级2", MagicLevel[1]),
            BIND_FIELD_INT("武功等级3", MagicLevel[2]),
            BIND_FIELD_INT("武功等级4", MagicLevel[3]),
            BIND_FIELD_INT("武功等级5", MagicLevel[4]),
            BIND_FIELD_INT("武功等级6", MagicLevel[5]),
            BIND_FIELD_INT("武功等级7", MagicLevel[6]),
            BIND_FIELD_INT("武功等级8", MagicLevel[7]),
            BIND_FIELD_INT("武功等级9", MagicLevel[8]),
            BIND_FIELD_INT("武功等级10", MagicLevel[9]),
            BIND_FIELD_INT("携带物品1", TakingItem[0]),
            BIND_FIELD_INT("携带物品2", TakingItem[1]),
            BIND_FIELD_INT("携带物品3", TakingItem[2]),
            BIND_FIELD_INT("携带物品4", TakingItem[3]),
            BIND_FIELD_INT("携带物品数量1", TakingItemCount[0]),
            BIND_FIELD_INT("携带物品数量2", TakingItemCount[1]),
            BIND_FIELD_INT("携带物品数量3", TakingItemCount[2]),
            BIND_FIELD_INT("携带物品数量4", TakingItemCount[3]),
        };
    }
    if (new_save_.item_.size() == 0)
    {
        Item a;
        new_save_.item_ =
        {
            BIND_FIELD_INT("编号", ID),
            BIND_FIELD_TEXT("物品名", Name),


            BIND_FIELD_TEXT("物品说明", Introduction),
            BIND_FIELD_INT("练出武功", MagicID),
            BIND_FIELD_INT("暗器动画编号", HiddenWeaponEffectID),
            BIND_FIELD_INT("使用人", User),
            BIND_FIELD_INT("装备类型", EquipType),
            BIND_FIELD_INT("显示物品说明", ShowIntroduction),
            BIND_FIELD_INT("物品类型", ItemType),

			
			BIND_FIELD_INT("加生命", AddHP),
            BIND_FIELD_INT("加生命最大值", AddMaxHP),
            BIND_FIELD_INT("加中毒解毒", AddPoison),
            BIND_FIELD_INT("加体力", AddPhysicalPower),
            BIND_FIELD_INT("改变内力性质", ChangeMPType),
            BIND_FIELD_INT("加内力", AddMP),
            BIND_FIELD_INT("加内力最大值", AddMaxMP),
            BIND_FIELD_INT("加攻击力", AddAttack),
            BIND_FIELD_INT("加轻功", AddSpeed),
            BIND_FIELD_INT("加防御力", AddDefence),
            BIND_FIELD_INT("加医疗", AddMedicine),
            BIND_FIELD_INT("加使毒", AddUsePoison),
            BIND_FIELD_INT("加解毒", AddDetoxification),
            BIND_FIELD_INT("加抗毒", AddAntiPoison),
            BIND_FIELD_INT("加拳掌", AddFist),
            BIND_FIELD_INT("加御剑", AddSword),
            BIND_FIELD_INT("加耍刀", AddKnife),
            BIND_FIELD_INT("加特殊兵器", AddUnusual),
            BIND_FIELD_INT("加暗器技巧", AddHiddenWeapon),
            BIND_FIELD_INT("加武学常识", AddKnowledge),
            BIND_FIELD_INT("加品德", AddMorality),
            BIND_FIELD_INT("加左右互搏", AddAttackTwice),
            BIND_FIELD_INT("加攻击带毒", AddAttackWithPoison),
            BIND_FIELD_INT("仅修炼人物", OnlySuitableRole),
            BIND_FIELD_INT("需内力性质", NeedMPType),
            BIND_FIELD_INT("需内力", NeedMP),
            BIND_FIELD_INT("需攻击力", NeedAttack),
            BIND_FIELD_INT("需轻功", NeedSpeed),
            BIND_FIELD_INT("需用毒", NeedUsePoison),
            BIND_FIELD_INT("需医疗", NeedMedicine),
            BIND_FIELD_INT("需解毒", NeedDetoxification),
            BIND_FIELD_INT("需拳掌", NeedFist),
            BIND_FIELD_INT("需御剑", NeedSword),
            BIND_FIELD_INT("需耍刀", NeedKnife),
            BIND_FIELD_INT("需特殊兵器", NeedUnusual),
            BIND_FIELD_INT("需暗器", NeedHiddenWeapon),
            BIND_FIELD_INT("需资质", NeedIQ),
            BIND_FIELD_INT("需经验", NeedExp),
            BIND_FIELD_INT("练出物品需经验", NeedExpForMakeItem),
            BIND_FIELD_INT("需材料", NeedMaterial),
            BIND_FIELD_INT("练出物品1", MakeItem[0]),
            BIND_FIELD_INT("练出物品2", MakeItem[1]),
            BIND_FIELD_INT("练出物品3", MakeItem[2]),
            BIND_FIELD_INT("练出物品4", MakeItem[3]),
            BIND_FIELD_INT("练出物品5", MakeItem[4]),
            BIND_FIELD_INT("练出物品数量1", MakeItemCount[0]),
            BIND_FIELD_INT("练出物品数量2", MakeItemCount[1]),
            BIND_FIELD_INT("练出物品数量3", MakeItemCount[2]),
            BIND_FIELD_INT("练出物品数量4", MakeItemCount[3]),
            BIND_FIELD_INT("练出物品数量5", MakeItemCount[4]),
        };
    }
    if (new_save_.submap_.size() == 0)
    {
        SubMapInfo a;
        new_save_.submap_ =
        {
            BIND_FIELD_INT("编号", ID),
            BIND_FIELD_TEXT("名称", Name),
            BIND_FIELD_INT("出门音乐", ExitMusic),
            BIND_FIELD_INT("进门音乐", EntranceMusic),
            BIND_FIELD_INT("跳转场景", JumpSubMap),
            BIND_FIELD_INT("进入条件", EntranceCondition),
            BIND_FIELD_INT("外景入口X1", MainEntranceX1),
            BIND_FIELD_INT("外景入口Y1", MainEntranceY1),
            BIND_FIELD_INT("外景入口X2", MainEntranceX2),
            BIND_FIELD_INT("外景入口Y2", MainEntranceY2),
            BIND_FIELD_INT("入口X", EntranceX),
            BIND_FIELD_INT("入口Y", EntranceY),
            BIND_FIELD_INT("出口X1", ExitX[0]),
            BIND_FIELD_INT("出口X2", ExitX[1]),
            BIND_FIELD_INT("出口X3", ExitX[2]),
            BIND_FIELD_INT("出口Y1", ExitY[0]),
            BIND_FIELD_INT("出口Y2", ExitY[1]),
            BIND_FIELD_INT("出口Y3", ExitY[2]),

        };
    }
    if (new_save_.magic_.size() == 0)
    {
        Magic a;
        new_save_.magic_ =
        {
            BIND_FIELD_INT("编号", ID),
            BIND_FIELD_TEXT("名称", Name),

            BIND_FIELD_INT("出招音效", SoundID),
            BIND_FIELD_INT("武功类型", MagicType),
            BIND_FIELD_INT("武功动画", EffectID),
            BIND_FIELD_INT("伤害类型", HurtType),
            BIND_FIELD_INT("攻击范围类型", AttackAreaType),
            BIND_FIELD_INT("消耗内力", NeedMP),
            BIND_FIELD_INT("敌人中毒", WithPoison),

            BIND_FIELD_INT("移动范围1", SelectDistance[0]),
            BIND_FIELD_INT("移动范围2", SelectDistance[1]),
            BIND_FIELD_INT("移动范围3", SelectDistance[2]),
            BIND_FIELD_INT("移动范围4", SelectDistance[3]),
            BIND_FIELD_INT("移动范围5", SelectDistance[4]),
            BIND_FIELD_INT("移动范围6", SelectDistance[5]),
            BIND_FIELD_INT("移动范围7", SelectDistance[6]),
            BIND_FIELD_INT("移动范围8", SelectDistance[7]),
            BIND_FIELD_INT("移动范围9", SelectDistance[8]),
            BIND_FIELD_INT("移动范围10", SelectDistance[9]),
            BIND_FIELD_INT("杀伤范围1", AttackDistance[0]),
            BIND_FIELD_INT("杀伤范围2", AttackDistance[1]),
            BIND_FIELD_INT("杀伤范围3", AttackDistance[2]),
            BIND_FIELD_INT("杀伤范围4", AttackDistance[3]),
            BIND_FIELD_INT("杀伤范围5", AttackDistance[4]),
            BIND_FIELD_INT("杀伤范围6", AttackDistance[5]),
            BIND_FIELD_INT("杀伤范围7", AttackDistance[6]),
            BIND_FIELD_INT("杀伤范围8", AttackDistance[7]),
            BIND_FIELD_INT("杀伤范围9", AttackDistance[8]),
            BIND_FIELD_INT("杀伤范围10", AttackDistance[9]),
            BIND_FIELD_INT("加内力1", AddMP[0]),
            BIND_FIELD_INT("加内力2", AddMP[1]),
            BIND_FIELD_INT("加内力3", AddMP[2]),
            BIND_FIELD_INT("加内力4", AddMP[3]),
            BIND_FIELD_INT("加内力5", AddMP[4]),
            BIND_FIELD_INT("加内力6", AddMP[5]),
            BIND_FIELD_INT("加内力7", AddMP[6]),
            BIND_FIELD_INT("加内力8", AddMP[7]),
            BIND_FIELD_INT("加内力9", AddMP[8]),
            BIND_FIELD_INT("加内力10", AddMP[9]),

        };
    }
    if (new_save_.shop_.size() == 0)
    {
        Shop a;
        new_save_.shop_ =
        {
            BIND_FIELD_INT("物品编号1", ItemID[0]),
            BIND_FIELD_INT("物品编号2", ItemID[1]),
            BIND_FIELD_INT("物品编号3", ItemID[2]),
            BIND_FIELD_INT("物品编号4", ItemID[3]),
            BIND_FIELD_INT("物品编号5", ItemID[4]),

        };
    }
}

void NewSave::SaveDBBaseInfo(sqlite3* db, Save::BaseInfo* data, int length)
{
    sqlite3_exec(db, "delete from base", nullptr, nullptr, nullptr);
    std::string cmd = "insert into base values(";
    for (auto& info : new_save_.base_)
    {
        cmd += std::to_string(*(int*)((char*)data + info.offset)) + ",";
    }
    cmd.pop_back();
    cmd += ")";
    sqlite3_exec(db, cmd.c_str(), nullptr, nullptr, nullptr);
}

void NewSave::LoadDBBaseInfo(sqlite3* db, Save::BaseInfo* data, int length)
{
    std::vector<Save::BaseInfo> datas;
    readValues(db, "base", new_save_.base_, datas);
    *data = datas[0];
}

void NewSave::SaveDBItemList(sqlite3* db, ItemList* data, int length)
{
    std::vector<ItemList> itemlist(length);
    for (int i = 0; i < length; i++)
    {
        itemlist[i] = data[i];
    }
    writeValues(db, "bag", new_save_.item_list_, itemlist);
}

void NewSave::LoadDBItemList(sqlite3* db, ItemList* data, int length)
{
    std::vector<ItemList> itemlist;
    readValues(db, "bag", new_save_.item_list_, itemlist);
    for (int i = 0; i < length; i++)
    {
        data[i] = itemlist[i];
    }
}

void NewSave::SaveDBRoleSave(sqlite3* db, const std::vector<Role>& data)
{
    writeValues(db, "role", new_save_.role_, data);
}

void NewSave::LoadDBRoleSave(sqlite3* db, std::vector<Role>& data)
{
    readValues(db, "role", new_save_.role_, data);
}

void NewSave::SaveDBItemSave(sqlite3* db, const std::vector<Item>& data)
{
    writeValues(db, "item", new_save_.item_, data);
}

void NewSave::LoadDBItemSave(sqlite3* db, std::vector<Item>& data)
{
    readValues(db, "item", new_save_.item_, data);
}

void NewSave::SaveDBSubMapInfoSave(sqlite3* db, const std::vector<SubMapInfo>& data)
{
    writeValues(db, "submap", new_save_.submap_, data);
}

void NewSave::LoadDBSubMapInfoSave(sqlite3* db, std::vector<SubMapInfo>& data)
{
    readValues(db, "submap", new_save_.submap_, data);
}

void NewSave::SaveDBMagicSave(sqlite3* db, const std::vector<Magic>& data)
{
    writeValues(db, "magic", new_save_.magic_, data);
}

void NewSave::LoadDBMagicSave(sqlite3* db, std::vector<Magic>& data)
{
	readValues(db, "magic", new_save_.magic_, data);
}

void NewSave::SaveDBShopSave(sqlite3* db, const std::vector<Shop>& data)
{
    writeValues(db, "shop", new_save_.shop_, data);
}

void NewSave::LoadDBShopSave(sqlite3* db, std::vector<Shop>& data)
{
    readValues(db, "shop", new_save_.shop_, data);
}
