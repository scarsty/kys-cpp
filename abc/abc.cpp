
//一些辅助的功能
//一些常数的设置比较不合理，建议以调试模式手动执行

#include "GrpIdxFile.h"
#include "OpenCCConverter.h"
#include "PotConv.h"
#include "TypesABC.h"
#include "filefunc.h"
#include "fmt1.h"
#include "strfunc.h"
#include <sqlite3.h>

BaseInfo16 baseinfo16_;
std::vector<RoleSave16> roles16_;
std::vector<MagicSave16> magics16_;
std::vector<ItemSave16> items16_;
std::vector<SubMapInfoSave16> submapinfo16_;
std::vector<ShopSave16> shops16_;

BaseInfo baseinfo_;
std::vector<RoleSave> roles_;
std::vector<MagicSave> magics_;
std::vector<ItemSave> items_;
std::vector<SubMapInfoSave> submapinfos_;
std::vector<ShopSave> shops_;

std::vector<int> offset16, length16, offset32, length32;

struct FieldInfo
{
    std::string name;
    int type;    //0-int, 1-char, 2-string
    int offset;
    size_t length;
    int col = -1;

    FieldInfo() {}

    FieldInfo(const std::string& n, int t, int o, size_t l, int c = -1) :
        name(n), type(t), offset(o), length(l), col(c) {}
};

std::vector<FieldInfo> db_base_, db_item_list_, db_role_, db_item_, db_submapinfo_, db_magic_, db_shop_;

#define GET_OFFSET(field) (int((char*)(&(a.field)) - (char*)(&a)))
#define BIND_FIELD_INT(key, field) FieldInfo(key, 0, GET_OFFSET(field), sizeof(a.field))
#define BIND_FIELD_TEXT(key, field) FieldInfo(key, 1, GET_OFFSET(field), sizeof(a.field))
#define BIND_FIELD_STRING BIND_FIELD_TEXT

void initDBFieldInfo()
{
    if (db_base_.size() == 0)
    {
        BaseInfo a;
        db_base_ =
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
                BIND_FIELD_INT("船X1", ShipX1),
                BIND_FIELD_INT("船Y1", ShipY1),
                BIND_FIELD_INT("内部编码", Encode),
                BIND_FIELD_INT("队友1", Team[0]),
                BIND_FIELD_INT("队友2", Team[1]),
                BIND_FIELD_INT("队友3", Team[2]),
                BIND_FIELD_INT("队友4", Team[3]),
                BIND_FIELD_INT("队友5", Team[4]),
                BIND_FIELD_INT("队友6", Team[5]),
            };
    }
    if (db_item_list_.size() == 0)
    {
        ItemList a;
        db_item_list_ =
            {
                BIND_FIELD_INT("物品编号", item_id),
                BIND_FIELD_INT("物品数量", count),
            };
    }
    if (db_role_.size() == 0)
    {
        RoleSave a;
        db_role_ =
            {
                BIND_FIELD_INT("编号", ID),
                BIND_FIELD_INT("头像", HeadID),
                BIND_FIELD_INT("生命成长", IncLife),
                BIND_FIELD_INT("无用", UnUse),
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
                BIND_FIELD_INT("物品修炼点数", ExpForMakeItem),
                BIND_FIELD_INT("武器", Equip0),
                BIND_FIELD_INT("防具", Equip1),
                BIND_FIELD_INT("装备武功1", EquipMagic[0]),
                BIND_FIELD_INT("装备武功2", EquipMagic[1]),
                BIND_FIELD_INT("装备武功3", EquipMagic[2]),
                BIND_FIELD_INT("装备武功4", EquipMagic[3]),
                BIND_FIELD_INT("二组装备武功1", EquipMagic2[0]),
                BIND_FIELD_INT("二组装备武功2", EquipMagic2[1]),
                BIND_FIELD_INT("二组装备武功3", EquipMagic2[2]),
                BIND_FIELD_INT("二组装备武功4", EquipMagic2[3]),
                BIND_FIELD_INT("装备物品", EquipItem),
                //BIND_FIELD_INT("动作帧数1", Frame[0]),
                //BIND_FIELD_INT("动作帧数2", Frame[1]),
                //BIND_FIELD_INT("动作帧数3", Frame[2]),
                //BIND_FIELD_INT("动作帧数4", Frame[3]),
                //BIND_FIELD_INT("动作帧数5", Frame[4]),
                //BIND_FIELD_INT("动作帧数6", Frame[5]),
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
    if (db_item_.size() == 0)
    {
        ItemSave a;
        db_item_ =
            {
                BIND_FIELD_INT("编号", ID),
                BIND_FIELD_TEXT("物品名", Name),
                BIND_FIELD_INT("物品名无用1", Name1[0]),
                BIND_FIELD_INT("物品名无用2", Name1[1]),
                BIND_FIELD_INT("物品名无用3", Name1[2]),
                BIND_FIELD_INT("物品名无用4", Name1[3]),
                BIND_FIELD_INT("物品名无用5", Name1[4]),
                BIND_FIELD_INT("物品名无用6", Name1[5]),
                BIND_FIELD_INT("物品名无用7", Name1[6]),
                BIND_FIELD_INT("物品名无用8", Name1[7]),
                BIND_FIELD_INT("物品名无用9", Name1[8]),
                BIND_FIELD_INT("物品名无用10", Name1[9]),
                BIND_FIELD_TEXT("物品说明", Introduction),
                BIND_FIELD_INT("练出武功", MagicID),
                BIND_FIELD_INT("暗器动画编号", HiddenWeaponEffectID),
                BIND_FIELD_INT("使用人", User),
                BIND_FIELD_INT("装备类型", EquipType),
                BIND_FIELD_INT("显示物品说明", ShowIntroduction),
                BIND_FIELD_INT("物品类型", ItemType),
                BIND_FIELD_INT("未知5", UnKnown5),
                BIND_FIELD_INT("未知6", UnKnown6),
                BIND_FIELD_INT("未知7", UnKnown7),
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
    if (db_submapinfo_.size() == 0)
    {
        SubMapInfoSave a;
        db_submapinfo_ =
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
                BIND_FIELD_INT("跳转X", JumpX),
                BIND_FIELD_INT("跳转Y", JumpY),
                BIND_FIELD_INT("跳转返还X", JumpReturnX),
                BIND_FIELD_INT("跳转返还Y", JumpReturnY),
            };
    }
    if (db_magic_.size() == 0)
    {
        MagicSave a;
        db_magic_ =
            {
                BIND_FIELD_INT("编号", ID),
                BIND_FIELD_TEXT("名称", Name),
                BIND_FIELD_INT("未知1", Unknown[0]),
                BIND_FIELD_INT("未知2", Unknown[1]),
                BIND_FIELD_INT("未知3", Unknown[2]),
                BIND_FIELD_INT("未知4", Unknown[3]),
                BIND_FIELD_INT("未知5", Unknown[4]),
                BIND_FIELD_INT("出招音效", SoundID),
                BIND_FIELD_INT("武功类型", MagicType),
                BIND_FIELD_INT("武功动画", EffectID),
                BIND_FIELD_INT("伤害类型", HurtType),
                BIND_FIELD_INT("攻击范围类型", AttackAreaType),
                BIND_FIELD_INT("消耗内力", NeedMP),
                BIND_FIELD_INT("敌人中毒", WithPoison),
                BIND_FIELD_INT("威力1", Attack[0]),
                BIND_FIELD_INT("威力2", Attack[1]),
                BIND_FIELD_INT("威力3", Attack[2]),
                BIND_FIELD_INT("威力4", Attack[3]),
                BIND_FIELD_INT("威力5", Attack[4]),
                BIND_FIELD_INT("威力6", Attack[5]),
                BIND_FIELD_INT("威力7", Attack[6]),
                BIND_FIELD_INT("威力8", Attack[7]),
                BIND_FIELD_INT("威力9", Attack[8]),
                BIND_FIELD_INT("威力10", Attack[9]),
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
                BIND_FIELD_INT("杀伤内力1", HurtMP[0]),
                BIND_FIELD_INT("杀伤内力2", HurtMP[1]),
                BIND_FIELD_INT("杀伤内力3", HurtMP[2]),
                BIND_FIELD_INT("杀伤内力4", HurtMP[3]),
                BIND_FIELD_INT("杀伤内力5", HurtMP[4]),
                BIND_FIELD_INT("杀伤内力6", HurtMP[5]),
                BIND_FIELD_INT("杀伤内力7", HurtMP[6]),
                BIND_FIELD_INT("杀伤内力8", HurtMP[7]),
                BIND_FIELD_INT("杀伤内力9", HurtMP[8]),
                BIND_FIELD_INT("杀伤内力10", HurtMP[9]),
            };
    }
    if (db_shop_.size() == 0)
    {
        ShopSave a;
        db_shop_ =
            {
                BIND_FIELD_INT("物品编号1", ItemID[0]),
                BIND_FIELD_INT("物品编号2", ItemID[1]),
                BIND_FIELD_INT("物品编号3", ItemID[2]),
                BIND_FIELD_INT("物品编号4", ItemID[3]),
                BIND_FIELD_INT("物品编号5", ItemID[4]),
                BIND_FIELD_INT("物品总量1", Total[0]),
                BIND_FIELD_INT("物品总量2", Total[1]),
                BIND_FIELD_INT("物品总量3", Total[2]),
                BIND_FIELD_INT("物品总量4", Total[3]),
                BIND_FIELD_INT("物品总量5", Total[4]),
                BIND_FIELD_INT("物品价格1", Price[0]),
                BIND_FIELD_INT("物品价格2", Price[1]),
                BIND_FIELD_INT("物品价格3", Price[2]),
                BIND_FIELD_INT("物品价格4", Price[3]),
                BIND_FIELD_INT("物品价格5", Price[4]),
            };
    }
}

//转换二进制文件为文本
void trans_bin_list(std::string in, std::string out)
{
    std::vector<int16_t> leave_list;
    filefunc::readFileToVector(in, leave_list);

    std::string s;
    for (auto a : leave_list)
    {
        s += fmt1::format("{}, ", a);
    }
    filefunc::writeStringToFile(s, out);
}

//导出战斗帧数为文本
void trans_fight_frame(std::string path0)
{
    for (int i = 0; i <= 300; i++)
    {
        std::string path = fmt1::format("{}/fight{:03}", path0, i);
        std::vector<int16_t> frame;
        std::string filename = path + "/fightframe.ka";
        if (filefunc::fileExist(filename))
        {
            filefunc::readFileToVector(path + "/fightframe.ka", frame);
            std::string content;
            fmt1::print("role {}\n", i);
            for (int j = 0; j < 5; j++)
            {
                if (frame[j] > 0)
                {
                    fmt1::print("{}, {}\n", j, frame[j]);
                    content += fmt1::format("{}, {}\n", j, frame[j]);
                }
            }
            filefunc::writeStringToFile(content, path + "/fightframe.txt");
        }
    }
}

template <class T>
static void writeValues(sqlite3* db, const std::string& table_name, std::vector<FieldInfo>& infos, const std::vector<T>& data)
{
    sqlite3_exec(db, ("drop table " + table_name).c_str(), nullptr, nullptr, nullptr);
    std::string cmd = "create table " + table_name + "(";
    for (auto& info : infos)
    {
        cmd += info.name + " ";
        if (info.type == 0)
        {
            cmd += "int,";
        }
        else if (info.type == 1)
        {
            cmd += "text,";
        }
    }
    cmd.pop_back();
    cmd += ")";
    sqlite3_exec(db, cmd.c_str(), nullptr, nullptr, nullptr);

    for (auto& data1 : data)
    {
        std::string cmd = "insert into " + table_name + " values(";
        for (auto& info : infos)
        {
            if (info.type == 0)
            {
                cmd += std::to_string(*(int*)((char*)&data1 + info.offset)) + ",";
            }
            else
            {
                cmd += "'" + std::string((char*)((char*)&data1 + info.offset)) + "',";
            }
        }
        cmd.pop_back();
        cmd += ")";
        sqlite3_exec(db, cmd.c_str(), nullptr, nullptr, nullptr);
    }
}

void saveR0ToDB(int num)
{
    sqlite3* db;
    //此处最好复制一个，先搞搞再说
    std::string filename = "save/" + std::to_string(num) + ".db";
    //convert::writeStringToFile(convert::readStringFromFile(filename0), filename);
    sqlite3_open(filename.c_str(), &db);

    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);

    //基本信息
    sqlite3_exec(db, "drop table base", nullptr, nullptr, nullptr);

    std::string cmd = "create table base(";
    for (auto& info : db_base_)
    {
        cmd += info.name + " ";
        if (info.type == 0)
        {
            cmd += "int,";
        }
        else if (info.type == 1)
        {
            cmd += "text,";
        }
    }
    cmd.pop_back();
    cmd += ")";
    sqlite3_exec(db, cmd.c_str(), nullptr, nullptr, nullptr);

    cmd = "insert into base values(";
    for (auto& info : db_base_)
    {
        cmd += std::to_string(*(int*)((char*)&baseinfo_ + info.offset)) + ",";
    }
    cmd.pop_back();
    cmd += ")";
    sqlite3_exec(db, cmd.c_str(), nullptr, nullptr, nullptr);

    //背包
    int length = ITEM_IN_BAG_COUNT;
    std::vector<ItemList> itemlist(length);
    for (int i = 0; i < length; i++)
    {
        itemlist[i] = baseinfo_.Items[i];
    }
    writeValues(db, "bag", db_item_list_, itemlist);
    //其他
    writeValues(db, "role", db_role_, roles_);
    writeValues(db, "item", db_item_, items_);
    writeValues(db, "submap", db_submapinfo_, submapinfos_);
    writeValues(db, "magic", db_magic_, magics_);
    writeValues(db, "shop", db_shop_, shops_);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

    sqlite3_close(db);
}

//扩展存档，将短整数扩展为int32
//最后一个参数：帧数需从之前存档格式获取
int expandR(std::string idx, std::string grp, int index, bool ranger = true, bool make_fightframe = false)
{
    if (!filefunc::fileExist(grp) || !filefunc::fileExist(idx))
    {
        return -1;
    }

    auto rgrp1_str = GrpIdxFile::getIdxContent(idx, grp, &offset16, &length16);
    auto rgrp1 = rgrp1_str.c_str();
    offset32 = offset16;
    length32 = length16;
    for (auto& i : offset32)
    {
        i *= 2;
    }
    for (auto& i : length32)
    {
        i *= 2;
    }

    int len = offset16.back();
    auto rgrp2 = new char[len * 2];

    auto s16 = (int16_t*)rgrp1;
    auto s32 = (int*)rgrp2;
    for (int i = 0; i < len / 2; i++)
    {
        s32[i] = s16[i];
    }

    if (ranger || make_fightframe)
    {
        memcpy(&baseinfo16_, rgrp1, sizeof(baseinfo16_));
        filefunc::readDataToVector(rgrp1 + offset16[1], length16[1], roles16_);
        filefunc::readDataToVector(rgrp1 + offset16[2], length16[2], items16_);
        filefunc::readDataToVector(rgrp1 + offset16[3], length16[3], submapinfo16_);
        filefunc::readDataToVector(rgrp1 + offset16[4], length16[4], magics16_);
        filefunc::readDataToVector(rgrp1 + offset16[5], length16[5], shops16_);
        if (make_fightframe)
        {
            for (auto it = roles16_.rbegin(); it != roles16_.rend(); it++)
            {
                auto r = *it;
                std::string content;
                fmt1::print("role {}\n", r.HeadID);
                for (int j = 0; j < 5; j++)
                {
                    if (r.Frame[j] > 0)
                    {
                        fmt1::print("{}, {}\n", j, r.Frame[j]);
                        content += fmt1::format("{}, {}\n", j, r.Frame[j]);
                    }
                }
                if (!content.empty())
                {
                    auto p = filefunc::getParentPath(idx);
                    p = filefunc::getParentPath(p);
                    auto f = fmt1::format("{}/resource/fight/fight{:03}/fightframe.txt", p, r.HeadID);
                    filefunc::writeStringToFile(content, f);
                }
            }
            if (!ranger) { return 1; }
        }

        memcpy(&baseinfo_, rgrp2, sizeof(baseinfo16_));
        filefunc::readDataToVector(rgrp2 + offset32[1], length32[1], roles_);
        filefunc::readDataToVector(rgrp2 + offset32[2], length32[2], items_);
        filefunc::readDataToVector(rgrp2 + offset32[3], length32[3], submapinfos_);
        filefunc::readDataToVector(rgrp2 + offset32[4], length32[4], magics_);
        filefunc::readDataToVector(rgrp2 + offset32[5], length32[5], shops_);
        //仅有包含名字的才需要特殊转换
        for (int i = 0; i < roles_.size(); i++)
        {
            memset(roles_[i].Name, 0, sizeof(roles_[i].Name));
            memset(roles_[i].Nick, 0, sizeof(roles_[i].Nick));
            PotConv::fromCP950ToUTF8(roles16_[i].Name, roles_[i].Name);
            PotConv::fromCP950ToUTF8(roles16_[i].Nick, roles_[i].Nick);
        }
        for (int i = 0; i < items_.size(); i++)
        {
            memset(items_[i].Name, 0, sizeof(items_[i].Name));
            memset(items_[i].Introduction, 0, sizeof(items_[i].Introduction));
            PotConv::fromCP950ToUTF8(items16_[i].Name, items_[i].Name);
            PotConv::fromCP950ToUTF8(items16_[i].Introduction, items_[i].Introduction);
        }
        for (int i = 0; i < magics_.size(); i++)
        {
            memset(magics_[i].Name, 0, sizeof(magics_[i].Name));
            PotConv::fromCP950ToUTF8(magics16_[i].Name, magics_[i].Name);
        }
        for (int i = 0; i < submapinfos_.size(); i++)
        {
            memset(submapinfos_[i].Name, 0, sizeof(submapinfos_[i].Name));
            PotConv::fromCP950ToUTF8(submapinfo16_[i].Name, submapinfos_[i].Name);
        }
        baseinfo_.Encode = 65001;
        memcpy(rgrp2, &baseinfo_, sizeof(baseinfo_));
        filefunc::writeVectorToData(rgrp2 + offset32[1], length32[1], roles_, sizeof(RoleSave));
        filefunc::writeVectorToData(rgrp2 + offset32[2], length32[2], items_, sizeof(ItemSave));
        filefunc::writeVectorToData(rgrp2 + offset32[3], length32[3], submapinfos_, sizeof(SubMapInfoSave));
        filefunc::writeVectorToData(rgrp2 + offset32[4], length32[4], magics_, sizeof(MagicSave));
    }
    s32[1]--;    //submap scene id
    GrpIdxFile::writeFile(grp + "32", rgrp2, len * 2);
    GrpIdxFile::writeFile(idx + "32", &offset32[1], 4 * offset32.size() - 4);
    //delete rgrp1;
    delete rgrp2;

    saveR0ToDB(index);
    fmt1::print("trans {} end\n", grp.c_str());
    return 0;
}

//将in的非零数据转到out
void combine_ka(std::string in, std::string out)
{
    std::vector<int16_t> in1, out1;
    filefunc::readFileToVector(in, in1);
    filefunc::readFileToVector(out, out1);
    std::string s;
    int i = 0;
    for (int i = 0; i < out1.size(); i += 2)
    {
        if (i < in1.size() && out1[i] == 0 && out1[i + 1] == 0)
        {
            out1[i] = in1[i];
            out1[i + 1] = in1[i + 1];
            fmt1::print("{}, ", i / 2);
        }
    }
    GrpIdxFile::writeFile(out, out1.data(), out1.size() * 2);
    //filefunc::writeStringToFile(s, out);
}

//验证战斗帧数的正确性
void check_fight_frame(std::string path, int repair = 0)
{
    for (int i = 0; i < 500; i++)
    {
        auto path1 = fmt1::format("{}/fight{:03}", path, i);
        if (filefunc::pathExist(path1))
        {
            auto files = filefunc::getFilesInPath(path1, 0);
            int count = files.size() - 3;
            int sum = 0;
            auto filename = path1 + "/fightframe.txt";
            auto numbers = strfunc::findNumbers<int>(filefunc::readFileToString(filename));
            for (int i = 0; i < numbers.size(); i += 2)
            {
                sum += numbers[i + 1];
            }
            if (sum * 4 != count)
            {
                fmt1::print("{}\n", path1);
                if (repair)
                {
                    if (numbers.size() <= 2)
                    {
                        auto str = fmt1::format("{}, {}", numbers[0], count / 4);
                        filefunc::writeStringToFile(str, filename);
                    }
                }
            }
        }
    }
}

//检查3号指令的最后3个参数正确性
void check_script(std::string path)
{
    auto files = filefunc::getFilesInPath(path, 0);
    for (auto& f : files)
    {
        bool repair = false;
        auto lines = strfunc::splitString(filefunc::readFileToString(path + "/" + f), "\n", false);
        for (auto& line : lines)
        {
            auto num = strfunc::findNumbers<int>(line);
            if (num.size() >= 13 && line.find("--") != 0)
            {
                if (num[0] == 3)
                {
                    if (num[1] > 0 && num[12] >= 0 && num[13] >= 0)
                    {
                        fmt1::print("{}\n", line);
                        auto strs = strfunc::splitString(line, ",", false);
                        strs[10] = "-2";
                        strs[11] = "-2";
                        strs[12][0] = '2';
                        strs[12] = "-" + strs[12];
                        std::string new_str;
                        for (auto& s : strs)
                        {
                            new_str += s + ",";
                        }
                        new_str.pop_back();
                        fmt1::print("{}\n", new_str);
                        line = "--repair" + line + "\n" + new_str;
                        repair = true;
                    }
                    if (num[12] < 0 && num[13] < 0)
                    {
                        //fmt1::print("{}\n", line);
                    }
                }
            }
        }
        if (repair)
        {
            std::string new_str;
            for (auto& s : lines)
            {
                new_str += s + "\n";
            }
            filefunc::writeStringToFile(new_str, path + "/" + f);
        }
    }
}

void loadR0()
{
    std::string filenamer = "save/ranger.grp32";
    std::string filename_idx = "save/ranger.idx32";
    auto rgrp = GrpIdxFile::getIdxContent(filename_idx, filenamer, &offset32, &length32);
    memcpy((void*)(&baseinfo_), rgrp.data() + offset32[0], length32[0]);
    filefunc::readDataToVector(rgrp.data() + offset32[1], length32[1], roles_, sizeof(RoleSave));
    filefunc::readDataToVector(rgrp.data() + offset32[2], length32[2], items_, sizeof(ItemSave));
    filefunc::readDataToVector(rgrp.data() + offset32[3], length32[3], submapinfos_, sizeof(SubMapInfoSave));
    filefunc::readDataToVector(rgrp.data() + offset32[4], length32[4], magics_, sizeof(MagicSave));
    filefunc::readDataToVector(rgrp.data() + offset32[5], length32[5], shops_, sizeof(ShopSave));
}

//重新产生头像
void make_heads(std::string path)
{
    auto h_lib = filefunc::getFilesInPath(path);
    loadR0();
    OpenCCConverter::getInstance()->set("cc/t2s.json");
    for (auto& r : roles_)
    {
        std::string name = r.Name;
        name = OpenCCConverter::getInstance()->UTF8s2t(name);
        name = PotConv::utf8tocp936(name);
        for (auto& h : h_lib)
        {
            if (h.find(name) == 0)
            {
                fmt1::print("{} {}\n", r.HeadID, h);
                auto file_src = path + "\\" + h;
                auto file_dst = fmt1::format("..\\game\\resource\\head1\\{}.png", r.HeadID);
                auto cmd = "copy " + file_src + " " + file_dst;
                system(cmd.c_str());
                break;
            }
        }
    }
}

int main()
{
#ifdef _WIN32
    //system("chcp 65001");
#endif
    std::string path = "./";
    //check_script(path +"script/oldevent");
    //check_fight_frame(path +"resource/fight", 1);
    //trans_fight_frame();

    //trans_bin_list(path + "list/levelup.bin", path + "list/levelup.txt");
    //trans_bin_list(path + "list/leave.bin", path + "list/leave.txt");
    initDBFieldInfo();
    //expandR(path + "save/ranger.idx", path + "save/ranger.grp", 0);
    expandR(path + "save/ranger.idx", path + "save/r1.grp", 1);
    expandR(path + "save/ranger.idx", path + "save/r2.grp", 2);
    expandR(path + "save/ranger.idx", path + "save/r3.grp", 3);
    expandR(path + "save/ranger.idx", path + "save/r4.grp", 4);
    expandR(path + "save/ranger.idx", path + "save/r5.grp", 5);

    //expandR(path + "save/ranger.idx", path + "save/ranger.grp", 0, false, true);

    //make_heads(R"(D:\_sty_bak\kys-all\转换至kys-cpp工具\头像)");

    //combine_ka(path + "resource/wmap/index.ka", path + "resource/smap/index.ka");

    return 0;
}
