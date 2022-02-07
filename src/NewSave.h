#pragma once
#include "PotConv.h"
#include "Save.h"
#include <algorithm>

class NewSave
{
private:
    struct FieldInfo
    {
        std::string name;
        int type;    //0-int, 1-char
        int offset;
        size_t length;
        int col = -1;
        FieldInfo() {}
        FieldInfo(const std::string& n, int t, int o, size_t l, int c = -1) : name(n), type(t), offset(o), length(l), col(c) {}
    };
    std::vector<FieldInfo> base_, item_list_, role_, item_, submap_, magic_, shop_;
    //std::map<std::string, FieldInfo> base_map_;
    static NewSave new_save_;

public:
    static void initDBFieldInfo();
    static void SaveDBBaseInfo(sqlite3* db, Save::BaseInfo* data, int length);
    static void LoadDBBaseInfo(sqlite3* db, Save::BaseInfo* data, int length);
    // 背包
    static void SaveDBItemList(sqlite3* db, ItemList* data, int length);
    static void LoadDBItemList(sqlite3* db, ItemList* data, int length);
    // 人物
    static void SaveDBRoleSave(sqlite3* db, const std::vector<Role>& data);
    static void LoadDBRoleSave(sqlite3* db, std::vector<Role>& data);
    // 物品
    static void SaveDBItemSave(sqlite3* db, const std::vector<Item>& data);
    static void LoadDBItemSave(sqlite3* db, std::vector<Item>& data);
    // 场景
    static void SaveDBSubMapInfoSave(sqlite3* db, const std::vector<SubMapInfo>& data);
    static void LoadDBSubMapInfoSave(sqlite3* db, std::vector<SubMapInfo>& data);
    // 武功
    static void SaveDBMagicSave(sqlite3* db, const std::vector<Magic>& data);
    static void LoadDBMagicSave(sqlite3* db, std::vector<Magic>& data);
    // 商店
    static void SaveDBShopSave(sqlite3* db, const std::vector<Shop>& data);
    static void LoadDBShopSave(sqlite3* db, std::vector<Shop>& data);

private:
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

    template <class T>
    static void readValues(sqlite3* db, const std::string& table_name, std::vector<FieldInfo>& infos, std::vector<T>& data)
    {
        sqlite3_stmt* statement;
        std::string strSql = "select * from " + table_name;
        sqlite3_prepare(db, strSql.c_str(), -1, &statement, NULL);

        //重整列序
        //在游戏最初必须运行一次读取
        if (infos[0].col == -1)
        {
            for (int i = 0; i < sqlite3_column_count(statement); i++)
            {
                auto name = sqlite3_column_name(statement, i);
                for (auto& info : infos)
                {
                    if (name == info.name)
                    {
                        info.col = i;
                        break;
                    }
                }
            }
            //std::sort(infos.begin(), infos.end(), [](FieldInfo & f1, FieldInfo & f2) { return f1.col < f2.col; });
        }
        //读取
        data.clear();
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            T data1;
            for (auto& info : infos)
            {
                if (info.type == 0)
                {
                    auto p = (int*)((char*)&data1 + info.offset);
                    *p = sqlite3_column_int(statement, info.col);
                }
                else
                {
                    auto p = (char*)((char*)&data1 + info.offset);
                    std::string str((char*)sqlite3_column_text(statement, info.col));
                    //str = PotConv::utf8tocp936(str);
                    memset(p, 0, info.length);
                    memcpy(p, str.data(), std::min(info.length, str.length()));
                }
            }
            data.push_back(data1);
        }
        sqlite3_finalize(statement);
    }
};
