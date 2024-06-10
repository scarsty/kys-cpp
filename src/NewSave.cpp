#include "NewSave.h"
#include "fmt1.h"

#define GET_OFFSET(field) (int((char*)(&(a.field)) - (char*)(&a)))
#define BIND_FIELD_INT(key, field) FieldInfo(key, 0, GET_OFFSET(field), sizeof(a.field))
#define BIND_FIELD_TEXT(key, field) FieldInfo(key, 1, GET_OFFSET(field), sizeof(a.field))

const std::vector<NewSave::FieldInfo>& NewSave::getFieldInfo(const std::string& name)
{
    if (name == "Base") { return getInstance()->base_; }
    if (name == "ItemList") { return getInstance()->item_list_; }
    if (name == "Role") { return getInstance()->role_; }
    if (name == "Item") { return getInstance()->item_; }
    if (name == "Magic") { return getInstance()->magic_; }
    if (name == "SubMapInfo") { return getInstance()->submapinfo_; }
    if (name == "Shop") { return getInstance()->shop_; }
}

void NewSave::initDBFieldInfo()
{
    if (base_.size() == 0)
    {
        Save::BaseInfo a;
        base_ =
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
    if (item_list_.size() == 0)
    {
        ItemList a;
        item_list_ =
        {
            BIND_FIELD_INT("物品编号", item_id),
            BIND_FIELD_INT("物品数量", count),
        };
    }
    if (role_.size() == 0)
    {
        Role a;
        role_ =
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
    if (item_.size() == 0)
    {
        Item a;
        item_ =
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
    if (submapinfo_.size() == 0)
    {
        SubMapInfo a;
        submapinfo_ =
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
    if (magic_.size() == 0)
    {
        Magic a;
        magic_ =
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
    if (shop_.size() == 0)
    {
        Shop a;
        shop_ =
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

void NewSave::SaveDBBaseInfo(sqlite3* db, Save::BaseInfo* data, int length)
{
    sqlite3_exec(db, "drop table base", nullptr, nullptr, nullptr);

    std::string cmd = "create table base(";
    for (auto& info : getInstance()->base_)
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
    for (auto& info : getInstance()->base_)
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
    readValues(db, "base", getInstance()->base_, datas);
    *data = datas[0];
}

void NewSave::SaveDBItemList(sqlite3* db, ItemList* data, int length)
{
    std::vector<ItemList> itemlist(length);
    for (int i = 0; i < length; i++)
    {
        itemlist[i] = data[i];
    }
    writeValues(db, "bag", getInstance()->item_list_, itemlist);
}

void NewSave::LoadDBItemList(sqlite3* db, ItemList* data, int length)
{
    std::vector<ItemList> itemlist;
    readValues(db, "bag", getInstance()->item_list_, itemlist);
    for (int i = 0; i < std::min(length, int(itemlist.size())); i++)
    {
        data[i] = itemlist[i];
    }
}

void NewSave::SaveDBRoleSave(sqlite3* db, const std::vector<Role>& data)
{
    writeValues(db, "role", getInstance()->role_, data);
}

void NewSave::LoadDBRoleSave(sqlite3* db, std::vector<Role>& data)
{
    readValues(db, "role", getInstance()->role_, data);
}

void NewSave::SaveDBItemSave(sqlite3* db, const std::vector<Item>& data)
{
    writeValues(db, "item", getInstance()->item_, data);
}

void NewSave::LoadDBItemSave(sqlite3* db, std::vector<Item>& data)
{
    readValues(db, "item", getInstance()->item_, data);
}

void NewSave::SaveDBSubMapInfoSave(sqlite3* db, const std::vector<SubMapInfo>& data)
{
    writeValues(db, "submap", getInstance()->submapinfo_, data);
}

void NewSave::LoadDBSubMapInfoSave(sqlite3* db, std::vector<SubMapInfo>& data)
{
    readValues(db, "submap", getInstance()->submapinfo_, data);
}

void NewSave::SaveDBMagicSave(sqlite3* db, const std::vector<Magic>& data)
{
    writeValues(db, "magic", getInstance()->magic_, data);
}

void NewSave::LoadDBMagicSave(sqlite3* db, std::vector<Magic>& data)
{
    readValues(db, "magic", getInstance()->magic_, data);
}

void NewSave::SaveDBShopSave(sqlite3* db, const std::vector<Shop>& data)
{
    writeValues(db, "shop", getInstance()->shop_, data);
}

void NewSave::LoadDBShopSave(sqlite3* db, std::vector<Shop>& data)
{
    readValues(db, "shop", getInstance()->shop_, data);
}

int NewSave::runSql(sqlite3* db, const std::string& cmd)
{
    int r = sqlite3_exec(db, cmd.c_str(), nullptr, nullptr, nullptr);
    if (r)
    {
        fmt1::print("{}\n", sqlite3_errmsg(db));
    }
    return r;
}
