#include "NewSave.h"

const std::vector<NewSave::FieldInfo>& NewSave::getFieldInfo(const std::string& name)
{
    if (name == "Base") { return getInstance()->base_; }
    if (name == "ItemList") { return getInstance()->item_list_; }
    if (name == "Role") { return getInstance()->role_; }
    if (name == "Item") { return getInstance()->item_; }
    if (name == "Magic") { return getInstance()->magic_; }
    if (name == "SubMapInfo") { return getInstance()->submapinfo_; }
    if (name == "Shop") { return getInstance()->shop_; }
    static std::vector<FieldInfo> empty;
    return empty;
}

void NewSave::SaveDBBaseInfo(SQLite3Wrapper& db, Save::BaseInfo* data, int length)
{
    std::vector<Save::BaseInfo> v = { *data };
    writeValues(db, "base", v);
}

void NewSave::LoadDBBaseInfo(SQLite3Wrapper& db, Save::BaseInfo* data, int length)
{
    std::vector<Save::BaseInfo> datas;
    readValues(db, "base", datas);
    if (!datas.empty()) { *data = datas[0]; }
}

void NewSave::SaveDBItemList(SQLite3Wrapper& db, ItemList* data, int length)
{
    std::vector<ItemList> itemlist(data, data + length);
    writeValues(db, "bag", itemlist);
}

void NewSave::LoadDBItemList(SQLite3Wrapper& db, ItemList* data, int length)
{
    std::vector<ItemList> itemlist;
    readValues(db, "bag", itemlist);
    for (int i = 0; i < std::min(length, int(itemlist.size())); i++)
    {
        data[i] = itemlist[i];
    }
}

void NewSave::SaveDBRoleSave(SQLite3Wrapper& db, const std::vector<Role>& data)
{
    writeValues(db, "role", data);
}

void NewSave::LoadDBRoleSave(SQLite3Wrapper& db, std::vector<Role>& data)
{
    readValues(db, "role", data);
}

void NewSave::SaveDBItemSave(SQLite3Wrapper& db, const std::vector<Item>& data)
{
    writeValues(db, "item", data);
}

void NewSave::LoadDBItemSave(SQLite3Wrapper& db, std::vector<Item>& data)
{
    readValues(db, "item", data);
}

void NewSave::SaveDBSubMapInfoSave(SQLite3Wrapper& db, const std::vector<SubMapInfo>& data)
{
    writeValues(db, "submap", data);
}

void NewSave::LoadDBSubMapInfoSave(SQLite3Wrapper& db, std::vector<SubMapInfo>& data)
{
    readValues(db, "submap", data);
}

void NewSave::SaveDBMagicSave(SQLite3Wrapper& db, const std::vector<Magic>& data)
{
    writeValues(db, "magic", data);
}

void NewSave::LoadDBMagicSave(SQLite3Wrapper& db, std::vector<Magic>& data)
{
    readValues(db, "magic", data);
}

void NewSave::SaveDBShopSave(SQLite3Wrapper& db, const std::vector<Shop>& data)
{
    writeValues(db, "shop", data);
}

void NewSave::LoadDBShopSave(SQLite3Wrapper& db, std::vector<Shop>& data)
{
    readValues(db, "shop", data);
}

int NewSave::runSql(SQLite3Wrapper& db, const std::string& cmd)
{
    int r = db.execute(cmd);
    if (!r)
    {
        LOG("{}\n", db.getErrorMessage());
    }
    return r;
}
