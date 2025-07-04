#pragma once
#include "GameUtil.h"
#include "Save.h"
#include <cstring>

class NewSave
{
private:
    struct FieldInfo
    {
        std::string name;
        int type;    //0-int, 1-char, 2-string
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

    std::vector<FieldInfo> base_, item_list_, role_, item_, submapinfo_, magic_, shop_;

    static NewSave* getInstance()
    {
        static NewSave ns;
        return &ns;
    }

public:
    NewSave() { initDBFieldInfo(); }

    static const std::vector<FieldInfo>& getFieldInfo(const std::string& name);    //这些索引其他地方可能用到

    void initDBFieldInfo();
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
    template <class T>
    static void writeValues(SQLite3Wrapper& db, const std::string& table_name, std::vector<FieldInfo>& infos, const std::vector<T>& data)
    {
        db.execute("drop table " + table_name);
        std::string cmd = "create table " + table_name + "(";
        for (auto& info : infos)
        {
            cmd += info.name + " ";
            if (info.type == 0)
            {
                cmd += "integer,";
            }
            else if (info.type == 1)
            {
                cmd += "text,";
            }
            else
            {
                cmd += "text,";
            }
        }
        cmd.pop_back();
        cmd += ")";
        auto ret = db.execute(cmd);

        for (auto& data1 : data)
        {
            std::string cmd1 = "insert into " + table_name + " values(";
            for (auto& info : infos)
            {
                if (info.type == 0)
                {
                    cmd1 += std::to_string(*(int*)((char*)&data1 + info.offset)) + ",";
                }
                else if (info.type == 1)
                {
                    cmd1 += "'" + std::string((char*)((char*)&data1 + info.offset)) + "',";
                }
                else
                {
                    auto& str = *(std::string*)((char*)&data1 + info.offset);
                    cmd1 += "'" + str + "',";
                }
            }
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
    static void readValues(SQLite3Wrapper& db, const std::string& table_name, std::vector<FieldInfo>& infos, std::vector<T>& data)
    {
        std::string cmd = "select * from " + table_name;
        auto stmt = db.query(cmd);
        if (!stmt.isValid())
        {
            LOG("{}\n", db.getErrorMessage());
        }

        for (int i = 0; i < stmt.getColumnCount(); i++)
        {
            auto name = stmt.getColumnName(i);
            for (auto& info : infos)
            {
                if (name == info.name)
                {
                    info.col = i;
                    break;
                }
            }
        }
        //读取
        int count = 0;
        while (stmt.step())
        {
            if (count + 1 >= data.size())
            {
                data.emplace_back();
            }
            T& data1 = data[count];
            for (int i = 0; i < infos.size(); i++)
            {
                auto& info = infos[i];
                if (info.col >= 0)
                {
                    if (info.type == 0)
                    {
                        auto p = (int*)((char*)&data1 + info.offset);
                        *p = stmt.getColumnInt(info.col);
                    }
                    else if (info.type == 1)
                    {
                        auto p = (char*)((char*)&data1 + info.offset);
                        std::string str((char*)stmt.getColumnText(info.col));
                        //str = PotConv::utf8tocp936(str);
                        memset(p, 0, info.length);
                        memcpy(p, str.data(), std::min(info.length, str.length()));
                    }
                    else
                    {
                        auto& str = *(std::string*)((char*)&data1 + info.offset);
                        str = (char*)stmt.getColumnText(info.col);
                    }
                }
            }
            count++;
        }
    }
};
