#pragma once
#include "GameUtil.h"
#include "Save.h"
#include <glaze/glaze.hpp>
#include <cstring>
#include <map>

// ===== glz::meta specializations: struct fields <-> SQL columns =====

template <>
struct glz::meta<Save::BaseInfo>
{
    using T = Save::BaseInfo;
    static constexpr auto value = object(
        "乘船", &T::InShip,
        "子场景内", &T::InSubMap,
        "主地图X", &T::MainMapX,
        "主地图Y", &T::MainMapY,
        "子场景X", &T::SubMapX,
        "子场景Y", &T::SubMapY,
        "面朝方向", &T::FaceTowards,
        "船X", &T::ShipX,
        "船Y", &T::ShipY,
        "船X1", &T::ShipX1,
        "船Y1", &T::ShipY1,
        "内部编码", &T::Encode,
        "队友1", [](auto&& s) -> auto& { return s.Team[0]; },
        "队友2", [](auto&& s) -> auto& { return s.Team[1]; },
        "队友3", [](auto&& s) -> auto& { return s.Team[2]; },
        "队友4", [](auto&& s) -> auto& { return s.Team[3]; },
        "队友5", [](auto&& s) -> auto& { return s.Team[4]; },
        "队友6", [](auto&& s) -> auto& { return s.Team[5]; }
    );
};

template <>
struct glz::meta<ItemList>
{
    using T = ItemList;
    static constexpr auto value = object(
        "物品编号", &T::item_id,
        "物品数量", &T::count
    );
};

template <>
struct glz::meta<Role>
{
    using T = Role;
    static constexpr auto value = object(
        "编号", &T::ID,
        "头像", &T::HeadID,
        "生命成长", &T::IncLife,
        "无用", &T::UnUse,
        "名字", &T::Name,
        "外号", &T::Nick,
        "性别", &T::Sexual,
        "等级", &T::Level,
        "经验", &T::Exp,
        "生命", &T::HP,
        "生命最大值", &T::MaxHP,
        "内伤", &T::Hurt,
        "中毒", &T::Poison,
        "体力", &T::PhysicalPower,
        "物品修炼点数", &T::ExpForMakeItem,
        "武器", &T::Equip0,
        "防具", &T::Equip1,
        "装备武功1", [](auto&& s) -> auto& { return s.EquipMagic[0]; },
        "装备武功2", [](auto&& s) -> auto& { return s.EquipMagic[1]; },
        "装备武功3", [](auto&& s) -> auto& { return s.EquipMagic[2]; },
        "装备武功4", [](auto&& s) -> auto& { return s.EquipMagic[3]; },
        "二组装备武功1", [](auto&& s) -> auto& { return s.EquipMagic2[0]; },
        "二组装备武功2", [](auto&& s) -> auto& { return s.EquipMagic2[1]; },
        "二组装备武功3", [](auto&& s) -> auto& { return s.EquipMagic2[2]; },
        "二组装备武功4", [](auto&& s) -> auto& { return s.EquipMagic2[3]; },
        "装备物品", &T::EquipItem,
        "内力性质", &T::MPType,
        "内力", &T::MP,
        "内力最大值", &T::MaxMP,
        "攻击力", &T::Attack,
        "轻功", &T::Speed,
        "防御力", &T::Defence,
        "医疗", &T::Medicine,
        "用毒", &T::UsePoison,
        "解毒", &T::Detoxification,
        "抗毒", &T::AntiPoison,
        "拳掌", &T::Fist,
        "御剑", &T::Sword,
        "耍刀", &T::Knife,
        "特殊", &T::Unusual,
        "暗器", &T::HiddenWeapon,
        "武学常识", &T::Knowledge,
        "品德", &T::Morality,
        "攻击带毒", &T::AttackWithPoison,
        "左右互搏", &T::AttackTwice,
        "声望", &T::Fame,
        "资质", &T::IQ,
        "修炼物品", &T::PracticeItem,
        "修炼点数", &T::ExpForItem,
        "所会武功1", [](auto&& s) -> auto& { return s.MagicID[0]; },
        "所会武功2", [](auto&& s) -> auto& { return s.MagicID[1]; },
        "所会武功3", [](auto&& s) -> auto& { return s.MagicID[2]; },
        "所会武功4", [](auto&& s) -> auto& { return s.MagicID[3]; },
        "所会武功5", [](auto&& s) -> auto& { return s.MagicID[4]; },
        "所会武功6", [](auto&& s) -> auto& { return s.MagicID[5]; },
        "所会武功7", [](auto&& s) -> auto& { return s.MagicID[6]; },
        "所会武功8", [](auto&& s) -> auto& { return s.MagicID[7]; },
        "所会武功9", [](auto&& s) -> auto& { return s.MagicID[8]; },
        "所会武功10", [](auto&& s) -> auto& { return s.MagicID[9]; },
        "武功等级1", [](auto&& s) -> auto& { return s.MagicLevel[0]; },
        "武功等级2", [](auto&& s) -> auto& { return s.MagicLevel[1]; },
        "武功等级3", [](auto&& s) -> auto& { return s.MagicLevel[2]; },
        "武功等级4", [](auto&& s) -> auto& { return s.MagicLevel[3]; },
        "武功等级5", [](auto&& s) -> auto& { return s.MagicLevel[4]; },
        "武功等级6", [](auto&& s) -> auto& { return s.MagicLevel[5]; },
        "武功等级7", [](auto&& s) -> auto& { return s.MagicLevel[6]; },
        "武功等级8", [](auto&& s) -> auto& { return s.MagicLevel[7]; },
        "武功等级9", [](auto&& s) -> auto& { return s.MagicLevel[8]; },
        "武功等级10", [](auto&& s) -> auto& { return s.MagicLevel[9]; },
        "携带物品1", [](auto&& s) -> auto& { return s.TakingItem[0]; },
        "携带物品2", [](auto&& s) -> auto& { return s.TakingItem[1]; },
        "携带物品3", [](auto&& s) -> auto& { return s.TakingItem[2]; },
        "携带物品4", [](auto&& s) -> auto& { return s.TakingItem[3]; },
        "携带物品数量1", [](auto&& s) -> auto& { return s.TakingItemCount[0]; },
        "携带物品数量2", [](auto&& s) -> auto& { return s.TakingItemCount[1]; },
        "携带物品数量3", [](auto&& s) -> auto& { return s.TakingItemCount[2]; },
        "携带物品数量4", [](auto&& s) -> auto& { return s.TakingItemCount[3]; }
    );
};

template <>
struct glz::meta<Item>
{
    using T = Item;
    static constexpr auto value = object(
        "编号", &T::ID,
        "物品名", &T::Name,
        "物品说明", &T::Introduction,
        "练出武功", &T::MagicID,
        "暗器动画编号", &T::HiddenWeaponEffectID,
        "使用人", &T::User,
        "装备类型", &T::EquipType,
        "显示物品说明", &T::ShowIntroduction,
        "物品类型", &T::ItemType,
        "未知5", &T::UnKnown5,
        "未知6", &T::UnKnown6,
        "未知7", &T::UnKnown7,
        "加生命", &T::AddHP,
        "加生命最大值", &T::AddMaxHP,
        "加中毒解毒", &T::AddPoison,
        "加体力", &T::AddPhysicalPower,
        "改变内力性质", &T::ChangeMPType,
        "加内力", &T::AddMP,
        "加内力最大值", &T::AddMaxMP,
        "加攻击力", &T::AddAttack,
        "加轻功", &T::AddSpeed,
        "加防御力", &T::AddDefence,
        "加医疗", &T::AddMedicine,
        "加使毒", &T::AddUsePoison,
        "加解毒", &T::AddDetoxification,
        "加抗毒", &T::AddAntiPoison,
        "加拳掌", &T::AddFist,
        "加御剑", &T::AddSword,
        "加耍刀", &T::AddKnife,
        "加特殊兵器", &T::AddUnusual,
        "加暗器技巧", &T::AddHiddenWeapon,
        "加武学常识", &T::AddKnowledge,
        "加品德", &T::AddMorality,
        "加左右互搏", &T::AddAttackTwice,
        "加攻击带毒", &T::AddAttackWithPoison,
        "仅修炼人物", &T::OnlySuitableRole,
        "需内力性质", &T::NeedMPType,
        "需内力", &T::NeedMP,
        "需攻击力", &T::NeedAttack,
        "需轻功", &T::NeedSpeed,
        "需用毒", &T::NeedUsePoison,
        "需医疗", &T::NeedMedicine,
        "需解毒", &T::NeedDetoxification,
        "需拳掌", &T::NeedFist,
        "需御剑", &T::NeedSword,
        "需耍刀", &T::NeedKnife,
        "需特殊兵器", &T::NeedUnusual,
        "需暗器", &T::NeedHiddenWeapon,
        "需资质", &T::NeedIQ,
        "需经验", &T::NeedExp,
        "练出物品需经验", &T::NeedExpForMakeItem,
        "需材料", &T::NeedMaterial,
        "练出物品1", [](auto&& s) -> auto& { return s.MakeItem[0]; },
        "练出物品2", [](auto&& s) -> auto& { return s.MakeItem[1]; },
        "练出物品3", [](auto&& s) -> auto& { return s.MakeItem[2]; },
        "练出物品4", [](auto&& s) -> auto& { return s.MakeItem[3]; },
        "练出物品5", [](auto&& s) -> auto& { return s.MakeItem[4]; },
        "练出物品数量1", [](auto&& s) -> auto& { return s.MakeItemCount[0]; },
        "练出物品数量2", [](auto&& s) -> auto& { return s.MakeItemCount[1]; },
        "练出物品数量3", [](auto&& s) -> auto& { return s.MakeItemCount[2]; },
        "练出物品数量4", [](auto&& s) -> auto& { return s.MakeItemCount[3]; },
        "练出物品数量5", [](auto&& s) -> auto& { return s.MakeItemCount[4]; }
    );
};

template <>
struct glz::meta<SubMapInfo>
{
    using T = SubMapInfo;
    static constexpr auto value = object(
        "编号", &T::ID,
        "名称", &T::Name,
        "出门音乐", &T::ExitMusic,
        "进门音乐", &T::EntranceMusic,
        "跳转场景", &T::JumpSubMap,
        "进入条件", &T::EntranceCondition,
        "外景入口X1", &T::MainEntranceX1,
        "外景入口Y1", &T::MainEntranceY1,
        "外景入口X2", &T::MainEntranceX2,
        "外景入口Y2", &T::MainEntranceY2,
        "入口X", &T::EntranceX,
        "入口Y", &T::EntranceY,
        "出口X1", [](auto&& s) -> auto& { return s.ExitX[0]; },
        "出口X2", [](auto&& s) -> auto& { return s.ExitX[1]; },
        "出口X3", [](auto&& s) -> auto& { return s.ExitX[2]; },
        "出口Y1", [](auto&& s) -> auto& { return s.ExitY[0]; },
        "出口Y2", [](auto&& s) -> auto& { return s.ExitY[1]; },
        "出口Y3", [](auto&& s) -> auto& { return s.ExitY[2]; },
        "跳转X", &T::JumpX,
        "跳转Y", &T::JumpY,
        "跳转返还X", &T::JumpReturnX,
        "跳转返还Y", &T::JumpReturnY
    );
};

template <>
struct glz::meta<Magic>
{
    using T = Magic;
    static constexpr auto value = object(
        "编号", &T::ID,
        "名称", &T::Name,
        "未知1", [](auto&& s) -> auto& { return s.Unknown[0]; },
        "未知2", [](auto&& s) -> auto& { return s.Unknown[1]; },
        "未知3", [](auto&& s) -> auto& { return s.Unknown[2]; },
        "未知4", [](auto&& s) -> auto& { return s.Unknown[3]; },
        "未知5", [](auto&& s) -> auto& { return s.Unknown[4]; },
        "出招音效", &T::SoundID,
        "武功类型", &T::MagicType,
        "武功动画", &T::EffectID,
        "伤害类型", &T::HurtType,
        "攻击范围类型", &T::AttackAreaType,
        "消耗内力", &T::NeedMP,
        "敌人中毒", &T::WithPoison,
        "威力1", [](auto&& s) -> auto& { return s.Attack[0]; },
        "威力2", [](auto&& s) -> auto& { return s.Attack[1]; },
        "威力3", [](auto&& s) -> auto& { return s.Attack[2]; },
        "威力4", [](auto&& s) -> auto& { return s.Attack[3]; },
        "威力5", [](auto&& s) -> auto& { return s.Attack[4]; },
        "威力6", [](auto&& s) -> auto& { return s.Attack[5]; },
        "威力7", [](auto&& s) -> auto& { return s.Attack[6]; },
        "威力8", [](auto&& s) -> auto& { return s.Attack[7]; },
        "威力9", [](auto&& s) -> auto& { return s.Attack[8]; },
        "威力10", [](auto&& s) -> auto& { return s.Attack[9]; },
        "移动范围1", [](auto&& s) -> auto& { return s.SelectDistance[0]; },
        "移动范围2", [](auto&& s) -> auto& { return s.SelectDistance[1]; },
        "移动范围3", [](auto&& s) -> auto& { return s.SelectDistance[2]; },
        "移动范围4", [](auto&& s) -> auto& { return s.SelectDistance[3]; },
        "移动范围5", [](auto&& s) -> auto& { return s.SelectDistance[4]; },
        "移动范围6", [](auto&& s) -> auto& { return s.SelectDistance[5]; },
        "移动范围7", [](auto&& s) -> auto& { return s.SelectDistance[6]; },
        "移动范围8", [](auto&& s) -> auto& { return s.SelectDistance[7]; },
        "移动范围9", [](auto&& s) -> auto& { return s.SelectDistance[8]; },
        "移动范围10", [](auto&& s) -> auto& { return s.SelectDistance[9]; },
        "杀伤范围1", [](auto&& s) -> auto& { return s.AttackDistance[0]; },
        "杀伤范围2", [](auto&& s) -> auto& { return s.AttackDistance[1]; },
        "杀伤范围3", [](auto&& s) -> auto& { return s.AttackDistance[2]; },
        "杀伤范围4", [](auto&& s) -> auto& { return s.AttackDistance[3]; },
        "杀伤范围5", [](auto&& s) -> auto& { return s.AttackDistance[4]; },
        "杀伤范围6", [](auto&& s) -> auto& { return s.AttackDistance[5]; },
        "杀伤范围7", [](auto&& s) -> auto& { return s.AttackDistance[6]; },
        "杀伤范围8", [](auto&& s) -> auto& { return s.AttackDistance[7]; },
        "杀伤范围9", [](auto&& s) -> auto& { return s.AttackDistance[8]; },
        "杀伤范围10", [](auto&& s) -> auto& { return s.AttackDistance[9]; },
        "加内力1", [](auto&& s) -> auto& { return s.AddMP[0]; },
        "加内力2", [](auto&& s) -> auto& { return s.AddMP[1]; },
        "加内力3", [](auto&& s) -> auto& { return s.AddMP[2]; },
        "加内力4", [](auto&& s) -> auto& { return s.AddMP[3]; },
        "加内力5", [](auto&& s) -> auto& { return s.AddMP[4]; },
        "加内力6", [](auto&& s) -> auto& { return s.AddMP[5]; },
        "加内力7", [](auto&& s) -> auto& { return s.AddMP[6]; },
        "加内力8", [](auto&& s) -> auto& { return s.AddMP[7]; },
        "加内力9", [](auto&& s) -> auto& { return s.AddMP[8]; },
        "加内力10", [](auto&& s) -> auto& { return s.AddMP[9]; },
        "杀伤内力1", [](auto&& s) -> auto& { return s.HurtMP[0]; },
        "杀伤内力2", [](auto&& s) -> auto& { return s.HurtMP[1]; },
        "杀伤内力3", [](auto&& s) -> auto& { return s.HurtMP[2]; },
        "杀伤内力4", [](auto&& s) -> auto& { return s.HurtMP[3]; },
        "杀伤内力5", [](auto&& s) -> auto& { return s.HurtMP[4]; },
        "杀伤内力6", [](auto&& s) -> auto& { return s.HurtMP[5]; },
        "杀伤内力7", [](auto&& s) -> auto& { return s.HurtMP[6]; },
        "杀伤内力8", [](auto&& s) -> auto& { return s.HurtMP[7]; },
        "杀伤内力9", [](auto&& s) -> auto& { return s.HurtMP[8]; },
        "杀伤内力10", [](auto&& s) -> auto& { return s.HurtMP[9]; }
    );
};

template <>
struct glz::meta<Shop>
{
    using T = Shop;
    static constexpr auto value = object(
        "物品编号1", [](auto&& s) -> auto& { return s.ItemID[0]; },
        "物品编号2", [](auto&& s) -> auto& { return s.ItemID[1]; },
        "物品编号3", [](auto&& s) -> auto& { return s.ItemID[2]; },
        "物品编号4", [](auto&& s) -> auto& { return s.ItemID[3]; },
        "物品编号5", [](auto&& s) -> auto& { return s.ItemID[4]; },
        "物品总量1", [](auto&& s) -> auto& { return s.Total[0]; },
        "物品总量2", [](auto&& s) -> auto& { return s.Total[1]; },
        "物品总量3", [](auto&& s) -> auto& { return s.Total[2]; },
        "物品总量4", [](auto&& s) -> auto& { return s.Total[3]; },
        "物品总量5", [](auto&& s) -> auto& { return s.Total[4]; },
        "物品价格1", [](auto&& s) -> auto& { return s.Price[0]; },
        "物品价格2", [](auto&& s) -> auto& { return s.Price[1]; },
        "物品价格3", [](auto&& s) -> auto& { return s.Price[2]; },
        "物品价格4", [](auto&& s) -> auto& { return s.Price[3]; },
        "物品价格5", [](auto&& s) -> auto& { return s.Price[4]; }
    );
};

// ===== NewSave class =====

class NewSave
{
public:
    // FieldInfo保留用于Lua脚本兼容 (Script.cpp)
    struct FieldInfo
    {
        std::string name;
        int type;    //0-int, 2-string
        int offset;
        size_t length;
        int col = -1;
        std::string name0;
        FieldInfo() {}

        FieldInfo(const std::string& n, int t, int o, size_t l, int c = -1, const std::string& n0 = "") :
            name(n), type(t), offset(o), length(l), col(c), name0(n0)
        {
            std::transform(name0.begin(), name0.end(), name0.begin(), ::tolower);
        }
    };

private:
    std::vector<FieldInfo> base_, item_list_, role_, item_, submapinfo_, magic_, shop_;

    static NewSave* getInstance()
    {
        static NewSave ns;
        return &ns;
    }

    // 从glz::meta自动生成FieldInfo（用于Lua兼容）
    template <class T>
    static std::vector<FieldInfo> generateFieldInfo()
    {
        std::vector<FieldInfo> result;
        constexpr auto& tup = glz::meta<T>::value.value;
        constexpr auto N = glz::tuple_size_v<std::remove_cvref_t<decltype(tup)>> / 2;
        glz::for_each<N>([&]<size_t I>() {
            T dummy{};
            decltype(auto) member = glz::get_member(dummy, glz::get<2 * I + 1>(tup));
            using MemberType = std::remove_cvref_t<decltype(member)>;
            std::string name{ glz::get<2 * I>(tup) };
            int type = std::is_integral_v<MemberType> ? 0 : 2;
            int offset = (int)((char*)&member - (char*)&dummy);
            size_t length = sizeof(MemberType);
            result.emplace_back(name, type, offset, length);
        });
        return result;
    }

public:
    NewSave()
    {
        base_ = generateFieldInfo<Save::BaseInfo>();
        item_list_ = generateFieldInfo<ItemList>();
        role_ = generateFieldInfo<Role>();
        item_ = generateFieldInfo<Item>();
        submapinfo_ = generateFieldInfo<SubMapInfo>();
        magic_ = generateFieldInfo<Magic>();
        shop_ = generateFieldInfo<Shop>();
    }

    static const std::vector<FieldInfo>& getFieldInfo(const std::string& name);    //这些索引其他地方可能用到

    static void SaveDBBaseInfo(SQLite3Wrapper& db, Save::BaseInfo* data, int length);
    static void LoadDBBaseInfo(SQLite3Wrapper& db, Save::BaseInfo* data, int length);
    // 背包
    static void SaveDBItemList(SQLite3Wrapper& db, ItemList* data, int length);
    static void LoadDBItemList(SQLite3Wrapper& db, ItemList* data, int length);
    // 人物
    static void SaveDBRoleSave(SQLite3Wrapper& db, const std::vector<Role>& data);
    static void LoadDBRoleSave(SQLite3Wrapper& db, std::vector<Role>& data);
    // 物品
    static void SaveDBItemSave(SQLite3Wrapper& db, const std::vector<Item>& data);
    static void LoadDBItemSave(SQLite3Wrapper& db, std::vector<Item>& data);
    // 场景
    static void SaveDBSubMapInfoSave(SQLite3Wrapper& db, const std::vector<SubMapInfo>& data);
    static void LoadDBSubMapInfoSave(SQLite3Wrapper& db, std::vector<SubMapInfo>& data);
    // 武功
    static void SaveDBMagicSave(SQLite3Wrapper& db, const std::vector<Magic>& data);
    static void LoadDBMagicSave(SQLite3Wrapper& db, std::vector<Magic>& data);
    // 商店
    static void SaveDBShopSave(SQLite3Wrapper& db, const std::vector<Shop>& data);
    static void LoadDBShopSave(SQLite3Wrapper& db, std::vector<Shop>& data);

public:
    static int runSql(SQLite3Wrapper& db, const std::string& cmd);

private:
    // 利用glaze编译期反射，类型安全地生成SQL和读写数据
    template <class T>
    static void writeValues(SQLite3Wrapper& db, const std::string& table_name, const std::vector<T>& data)
    {
        constexpr auto& tup = glz::meta<T>::value.value;
        constexpr auto N = glz::tuple_size_v<std::remove_cvref_t<decltype(tup)>> / 2;

        db.execute("drop table " + table_name);

        // CREATE TABLE
        std::string cmd = "create table " + table_name + "(";
        glz::for_each<N>([&]<size_t I>() {
            T dummy{};
            using MemberType = std::remove_cvref_t<decltype(glz::get_member(dummy, glz::get<2 * I + 1>(tup)))>;
            cmd += std::string(glz::get<2 * I>(tup));
            if constexpr (std::is_integral_v<MemberType>)
                cmd += " integer,";
            else
                cmd += " text,";
        });
        cmd.pop_back();
        cmd += ")";
        db.execute(cmd);

        // INSERT
        for (auto& data1 : data)
        {
            std::string cmd1 = "insert into " + table_name + " values(";
            glz::for_each<N>([&]<size_t I>() {
                decltype(auto) val = glz::get_member(data1, glz::get<2 * I + 1>(tup));
                using MemberType = std::remove_cvref_t<decltype(val)>;
                if constexpr (std::is_integral_v<MemberType>)
                    cmd1 += std::to_string(val) + ",";
                else
                    cmd1 += "'" + std::string(val) + "',";
            });
            cmd1.pop_back();
            cmd1 += ")";
            auto ret1 = db.execute(cmd1);
            if (!ret1)
            {
                LOG("{}\n", db.getErrorMessage());
            }
        }
    }

    template <class T>
    static void readValues(SQLite3Wrapper& db, const std::string& table_name, std::vector<T>& data)
    {
        constexpr auto& tup = glz::meta<T>::value.value;
        constexpr auto N = glz::tuple_size_v<std::remove_cvref_t<decltype(tup)>> / 2;

        auto stmt = db.query("select * from " + table_name);
        if (!stmt.isValid())
        {
            LOG("{}\n", db.getErrorMessage());
            return;
        }

        // 建立列名 -> 列索引映射
        std::map<std::string, int> col_map;
        for (int i = 0; i < stmt.getColumnCount(); i++)
        {
            col_map[stmt.getColumnName(i)] = i;
        }

        int count = 0;
        while (stmt.step())
        {
            if (count + 1 >= (int)data.size())
            {
                data.emplace_back();
            }
            T& data1 = data[count];
            glz::for_each<N>([&]<size_t I>() {
                std::string name{ glz::get<2 * I>(tup) };
                auto it = col_map.find(name);
                if (it != col_map.end())
                {
                    int col = it->second;
                    decltype(auto) val = glz::get_member(data1, glz::get<2 * I + 1>(tup));
                    using MemberType = std::remove_cvref_t<decltype(val)>;
                    if constexpr (std::is_integral_v<MemberType>)
                        val = stmt.getColumnInt(col);
                    else
                        val = (const char*)stmt.getColumnText(col);
                }
            });
            count++;
        }
    }
};
