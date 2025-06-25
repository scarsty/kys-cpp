#include "Save.h"
#include "GameUtil.h"
#include "GrpIdxFile.h"
#include "NewSave.h"
#include "PotConv.h"
#include "filefunc.h"
#include "lz4.h"

Save::Save()
{
}

Save::~Save()
{
}

std::string Save::getFilename(int i, char c)
{
    std::string filename;
    if (i > 0)
    {
        if (c == 'r')
        {
            filename = std::format("{}save/{}.db", GameUtil::PATH(), i);
        }
        else
        {
            filename = std::format("{}save/{}{}.grp", GameUtil::PATH(), c, i);
        }
    }
    else
    {
        if (c == 'r')
        {
            filename = GameUtil::PATH() + "save/0.db";
        }
        else if (c == 's')
        {
            filename = GameUtil::PATH() + "save/allsin.grp";
        }
        else if (c == 'd')
        {
            filename = GameUtil::PATH() + "save/alldef.grp";
        }
    }
    return filename;
}

bool Save::checkSaveFileExist(int num)
{
    return filefunc::fileExist(getFilename(num, 'r'))
        && filefunc::fileExist(getFilename(num, 's'))
        && filefunc::fileExist(getFilename(num, 'd'));
}

void Save::updateAllPtrVector()
{
    toPtrVector(roles_mem_, roles_);
    toPtrVector(items_mem_, items_);
    toPtrVector(submap_infos_mem_, submap_infos_);
    toPtrVector(magics_mem_, magics_);
    toPtrVector(shops_mem_, shops_);
}

bool Save::load(int num)
{
    if (!checkSaveFileExist(num))
    {
        return false;
    }

    //loadR(num);
    loadRFromDB(num);
    loadSD(num);

    //saveRToDB(0);    //调试用
    makeMapsAndRepairID();

    //saveRToDB(num);    //临时转换
    return true;
}

bool Save::save(int num)
{
    //saveR(num);
    auto filename = GameUtil::PATH() + "save/" + std::to_string(num) + ".db";
    filefunc::removeFile(filename);
    saveRToDB(num);
    saveSD(num);
    return true;
}

void Save::loadSD(int num)
{
    auto submap_count = submap_infos_.size();

    bool db_failed = false;

    if (DB_SD)
    {
        auto filename = GameUtil::PATH() + "save/" + std::to_string(num) + ".db";
        SQLite3Wrapper db(filename);
        for (int i = 0; i < submap_count; i++)
        {
            auto ps = &(submap_infos_mem_[i].LayerData(0, 0, 0));
            auto pd = submap_infos_mem_[i].Event(0);

            SQLite3Blob sblob(db, "bindata", "sdata", i + 1);
            SQLite3Blob dblob(db, "bindata", "ddata", i + 1);
            if (!sblob.isValid() || !dblob.isValid())
            {
                db_failed = true;
                LOG("Failed to read SD data for submap {} from database: {}\n", i, db.getErrorMessage());
                break;
            }
            int size_lz4s = sblob.getSize();
            int size_lz4d = dblob.getSize();
            std::string sdata_str(size_lz4s, 0);
            std::string ddata_str(size_lz4d, 0);
            sblob.read(sdata_str.data(), size_lz4s);
            dblob.read(ddata_str.data(), size_lz4d);

            int size_s = LZ4_decompress_safe(sdata_str.data(), (char*)ps, size_lz4s, sdata_length_);
            int size_d = LZ4_decompress_safe(ddata_str.data(), (char*)pd, size_lz4d, ddata_length_);

            if (size_s != sdata_length_ || size_d != ddata_length_)
            {
                LOG("LZ4 decompression failed for submap {}: expected sizes {}, {}, got {}, {}\n",
                    i, sdata_length_, ddata_length_, size_s, size_d);
                //db_failed = true;
                //break;
            }
        }
    }
    if (!DB_SD || db_failed)
    {
        std::vector<char> sdata(submap_count * sdata_length_);
        std::vector<char> ddata(submap_count * ddata_length_);
        std::string filenames = getFilename(num, 's');
        std::string filenamed = getFilename(num, 'd');
        GrpIdxFile::readFile(filenames, sdata.data(), submap_count * sdata_length_);
        GrpIdxFile::readFile(filenamed, ddata.data(), submap_count * ddata_length_);
        for (int i = 0; i < submap_count; i++)
        {
            auto ps = &(submap_infos_mem_[i].LayerData(0, 0, 0));
            auto pd = submap_infos_mem_[i].Event(0);

            memcpy(ps, sdata.data() + sdata_length_ * i, sdata_length_);
            memcpy(pd, ddata.data() + ddata_length_ * i, ddata_length_);
        }
    }
}

void Save::saveSD(int num)
{
    auto submap_count = submap_infos_mem_.size();

    std::vector<char> sdata(submap_count * sdata_length_);
    std::vector<char> ddata(submap_count * ddata_length_);

    if (DB_SD)
    {
        std::vector<char> sdata1(sdata.size());
        std::vector<char> ddata1(ddata.size());

        auto filename = GameUtil::PATH() + "save/" + std::to_string(num) + ".db";
        SQLite3Wrapper db(filename);
        db.execute("drop table bindata");
        std::string cmd = "create table bindata (id integer, sdata blob, ddata blob)";
        db.execute(cmd);
        db.BeginTransaction();
        for (int i = 0; i < submap_count; i++)
        {
            cmd = std::format("insert into bindata values(?, ?, ?)");
            auto stmt = db.prepare(cmd);
            stmt.bind(1, i);

            auto ps = &(submap_infos_mem_[i].LayerData(0, 0, 0));
            auto pd = submap_infos_mem_[i].Event(0);
            int size_lz4s = LZ4_compress_default((const char*)ps, sdata1.data(), sdata_length_, sdata1.size());
            int size_lz4d = LZ4_compress_default((const char*)pd, ddata1.data(), ddata_length_, ddata1.size());
            stmt.bind(2, sdata1.data(), size_lz4s);
            stmt.bind(3, ddata1.data(), size_lz4d);
            stmt.step();
        }
        db.CommitTransaction();
    }
    else
    {
        for (int i = 0; i < submap_count; i++)
        {
            auto ps = &(submap_infos_mem_[i].LayerData(0, 0, 0));
            auto pd = submap_infos_mem_[i].Event(0);
            memcpy(sdata.data() + sdata_length_ * i, ps, sdata_length_);
            memcpy(ddata.data() + ddata_length_ * i, pd, ddata_length_);
        }
        //保存s和d数据到文件中
        std::string filenames = getFilename(num, 's');
        std::string filenamed = getFilename(num, 'd');
        GrpIdxFile::writeFile(filenames, sdata.data(), submap_count * sdata_length_);
        GrpIdxFile::writeFile(filenamed, ddata.data(), submap_count * ddata_length_);
    }
}

void Save::resetRData(const std::vector<RoleSave>& newData)
{
    roles_mem_.clear();
    for (int i = 0; i < newData.size(); i++)
    {
        roles_mem_.emplace_back();
        auto& r = roles_mem_.back();
        memcpy(&r, &newData[i], sizeof(RoleSave));
        r.RealID = r.ID;
        r.ID = i;
    }
    updateAllPtrVector();
}

Role* Save::getTeamMate(int i)
{
    if (i < 0 || i >= TEAMMATE_COUNT)
    {
        return nullptr;
    }
    int r = Team[i];
    if (r < 0 || r >= roles_mem_.size())
    {
        return nullptr;
    }
    return roles_[r];
}

Item* Save::getItemByBagIndex(int i)
{
    if (i < 0 || i >= ITEM_IN_BAG_COUNT)
    {
        return nullptr;
    }
    int r = Items[i].item_id;
    if (r < 0 || r >= items_mem_.size())
    {
        return nullptr;
    }
    return items_[r];
}

int Save::getItemCountByBagIndex(int i)
{
    return Items[i].count;
}

int Save::getItemCountInBag(Item* item)
{
    return getItemCountInBag(item->ID);
}

std::vector<std::tuple<Item*, int>> Save::getAvailableEquipItems()
{
    std::vector<std::tuple<Item*, int>> ret = {};
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        auto id = Items[i].item_id;
        auto count = Items[i].count;
        if (id < 0)
        {
            break;
        }
        if (count <= 0)
        {
            continue;
        }
        auto item = getItem(id);
        if (item && (item->ItemType == 4))
        // if (item && (item->ItemType == 3 || item->ItemType == 4))
        {
            ret.emplace_back(item, count);
        }
    }
    return ret;
}

int Save::getItemCountInBag(int item_id)
{
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        auto id = Items[i].item_id;
        if (id < 0)
        {
            break;
        }
        if (id == item_id)
        {
            return Items[i].count;
        }
    }
    return 0;
}

int Save::getMoneyCountInBag()
{
    return getItemCountInBag(Item::MoneyItemID);
}

void Save::makeMapsAndRepairID()
{
    roles_by_name_.clear();
    magics_by_name_.clear();
    items_by_name_.clear();
    submap_infos_by_name_.clear();

    //有重名的，斟酌使用
    int count = 0;
    for (auto& i : roles_)
    {
        roles_by_name_[i->Name] = i;
        i->ID = count++;
    }
    count = 0;
    for (auto& i : magics_)
    {
        magics_by_name_[i->Name] = i;
        i->ID = count++;
    }
    count = 0;
    for (auto& i : items_)
    {
        items_by_name_[i->Name] = i;
        i->ID = count++;
    }
    count = 0;
    for (auto& i : submap_infos_)
    {
        submap_infos_by_name_[i->Name] = i;
        i->ID = count++;
    }
}

Magic* Save::getRoleLearnedMagic(Role* r, int i)
{
    if (i < 0 || i >= ROLE_MAGIC_COUNT)
    {
        return nullptr;
    }
    return getMagic(r->MagicID[i]);
}

int Save::getRoleLearnedMagicLevelIndex(Role* r, Magic* m)
{
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        if (r->MagicID[i] == m->ID)
        {
            return r->getRoleMagicLevelIndex(i);
        }
    }
    return -1;
}

void Save::saveRToDB(int num)
{
    //此处最好复制一个，先搞搞再说
    std::string filename = GameUtil::PATH() + "save/" + std::to_string(num) + ".db";
    //convert::writeStringToFile(convert::readStringFromFile(filename0), filename);
    SQLite3Wrapper db(filename);
    saveRToDB(db);
}

void Save::loadRFromDB(int num)
{
    auto filename = GameUtil::PATH() + "save/" + std::to_string(num) + ".db";
    if (!filefunc::fileExist(filename))
    {
        return;
    }
    SQLite3Wrapper db(filename);
    loadRFromDB(db);
    updateAllPtrVector();
    Encode = 65001;
}

void Save::saveRToDB(SQLite3Wrapper& db)
{
    db.BeginTransaction();
    NewSave::SaveDBBaseInfo(db, (BaseInfo*)this, 1);
    NewSave::SaveDBItemList(db, Items, ITEM_IN_BAG_COUNT);
    NewSave::SaveDBRoleSave(db, roles_mem_);
    NewSave::SaveDBItemSave(db, items_mem_);
    NewSave::SaveDBSubMapInfoSave(db, submap_infos_mem_);
    NewSave::SaveDBMagicSave(db, magics_mem_);
    NewSave::SaveDBShopSave(db, shops_mem_);
    db.CommitTransaction();
}

void Save::loadRFromDB(SQLite3Wrapper& db)
{
    NewSave::LoadDBBaseInfo(db, (BaseInfo*)this, 1);
    NewSave::LoadDBItemList(db, Items, ITEM_IN_BAG_COUNT);
    NewSave::LoadDBRoleSave(db, roles_mem_);
    NewSave::LoadDBItemSave(db, items_mem_);
    NewSave::LoadDBSubMapInfoSave(db, submap_infos_mem_);
    NewSave::LoadDBMagicSave(db, magics_mem_);
    NewSave::LoadDBShopSave(db, shops_mem_);
}

void Save::saveSDToDB(int num)
{
    //    auto filename = GameUtil::PATH() + "save/" + std::to_string(num) + ".db";
    //    SQLite3Wrapper db(filename);
    //    db.execute("drop table ddata");
    ////    std::string cmd = R"(
    ////create table ddata (场景编号 integer, 场景事件编号 integer, 不可行走 integer,
    ////事件编号 integer, 普通触发 integer, 对话触发 integer, 路过触发 integer,
    ////贴图 integer, 开始贴图 integer, 结束贴图 integer, 延迟 integer)";
    //    std::string cmd = R"(create table bindata (sdata blob, data blob)";
    //    db.execute(cmd);
    //    SQLite3Blob data_blob(db, "bindata", "ddata", 0);
}

void Save::runSql(const std::string& cmd)
{
    if (cmd.find("update ") != 0) { return; }
    SQLite3Wrapper db("");
    //if (db.)
    {
        if (cmd.find("base ") != std::string::npos)
        {
            NewSave::SaveDBBaseInfo(db, (BaseInfo*)this, 1);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBBaseInfo(db, (BaseInfo*)this, 1);
        }
        else if (cmd.find("bag ") != std::string::npos)
        {
            NewSave::SaveDBItemList(db, Items, ITEM_IN_BAG_COUNT);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBItemList(db, Items, ITEM_IN_BAG_COUNT);
        }
        else if (cmd.find("role ") != std::string::npos)
        {
            NewSave::SaveDBRoleSave(db, roles_mem_);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBRoleSave(db, roles_mem_);
        }
        else if (cmd.find("item ") != std::string::npos)
        {
            NewSave::SaveDBItemSave(db, items_mem_);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBItemSave(db, items_mem_);
        }
        else if (cmd.find("submap ") != std::string::npos)
        {
            NewSave::SaveDBSubMapInfoSave(db, submap_infos_mem_);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBSubMapInfoSave(db, submap_infos_mem_);
        }
        else if (cmd.find("magic ") != std::string::npos)
        {
            NewSave::SaveDBMagicSave(db, magics_mem_);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBMagicSave(db, magics_mem_);
        }
        else if (cmd.find("shop ") != std::string::npos)
        {
            NewSave::SaveDBShopSave(db, shops_mem_);
            NewSave::runSql(db, cmd);
            NewSave::LoadDBShopSave(db, shops_mem_);
        }
    }
}
