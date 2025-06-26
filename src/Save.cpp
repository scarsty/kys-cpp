#include "Save.h"
#include "GameUtil.h"
#include "GrpIdxFile.h"
#include "NewSave.h"
#include "PotConv.h"
#include "ZipFile.h"
#include "filefunc.h"

Save::Save()
{
}

Save::~Save()
{
}

std::string Save::getFilename(int i, char c)
{
    std::string filename;

    //自动决定
    if (c == 0)
    {
        c = ZIP_SAVE ? 'z' : 'r';
    }

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
    if (c == 'z')
    {
        filename = std::format("{}save/{}.zip", GameUtil::PATH(), i);
    }
    return filename;
}

bool Save::checkSaveFileExist(int num)
{
    return (ZIP_SAVE == 1 && filefunc::fileExist(getFilename(num, 'z')))
        || filefunc::fileExist(getFilename(num, 'r')) && filefunc::fileExist(getFilename(num, 's')) && filefunc::fileExist(getFilename(num, 'd'));
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

    ZipFile zip;
    if (ZIP_SAVE)
    {
        zip.openRead(getFilename(num, 'z'));
    }

    auto filenamedb = getFilename(num, 'r');
    if (zip.opened())
    {
        //临时使用99号文件名，避免覆盖原来的文件
        filenamedb = getFilename(99, 'r');
        filefunc::writeStringToFile(zip.readFile("1.db"), filenamedb);
    }

    SQLite3Wrapper db(filenamedb);
    loadRFromDB(db);
    updateAllPtrVector();

    //读取SD数据
    //注意比较多的回退情况，即找不到zip时，又单独找其他文件
    auto submap_count = submap_infos_.size();

    bool db_failed = false;

    if (DB_SD)
    {
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
            sblob.read(ps, size_lz4s);
            dblob.read(pd, size_lz4d);
        }
    }
    if (!DB_SD || db_failed)
    {
        std::vector<char> sdata(submap_count * sdata_length_);
        std::vector<char> ddata(submap_count * ddata_length_);
        if (zip.opened())
        {
            GrpIdxFile::readFile(getFilename(num, 's'), sdata.data(), submap_count * sdata_length_);
            GrpIdxFile::readFile(getFilename(num, 'd'), ddata.data(), submap_count * ddata_length_);
        }
        else
        {
            zip.readFileToBuffer("s1.grp", sdata);
            zip.readFileToBuffer("d1.grp", ddata);
        }
        for (int i = 0; i < submap_count; i++)
        {
            auto ps = &(submap_infos_mem_[i].LayerData(0, 0, 0));
            auto pd = submap_infos_mem_[i].Event(0);

            memcpy(ps, sdata.data() + sdata_length_ * i, sdata_length_);
            memcpy(pd, ddata.data() + ddata_length_ * i, ddata_length_);
        }
    }
    //saveRToDB(0);    //调试用
    makeMapsAndRepairID();
    //saveRToDB(num);    //临时转换
    db.close();
    filefunc::removeFile(getFilename(99, 'r'));    //删除临时文件
    return true;
}

bool Save::save(int num)
{
    ZipFile zip;
    if (ZIP_SAVE)
    {
        zip.openWrite(getFilename(num, 'z'));
    }

    auto filenamedb = getFilename(num, 'r');

    if (ZIP_SAVE)
    {
        filenamedb = getFilename(99, 'r');
    }

    SQLite3Wrapper db(filenamedb);
    saveRToDB(db);

    //保存SD数据
    auto submap_count = submap_infos_mem_.size();

    if (DB_SD)
    {
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
            stmt.bind(2, ps, sdata_length_);
            stmt.bind(3, pd, ddata_length_);
            stmt.step();
        }
        db.CommitTransaction();
    }
    else
    {
        std::vector<char> sdata(submap_count * sdata_length_);
        std::vector<char> ddata(submap_count * ddata_length_);
        for (int i = 0; i < submap_count; i++)
        {
            auto ps = &(submap_infos_mem_[i].LayerData(0, 0, 0));
            auto pd = submap_infos_mem_[i].Event(0);
            memcpy(sdata.data() + sdata_length_ * i, ps, sdata_length_);
            memcpy(ddata.data() + ddata_length_ * i, pd, ddata_length_);
        }

        if (ZIP_SAVE)
        {
            zip.addData("s1.grp", sdata.data(), sdata.size());
            zip.addData("d1.grp", ddata.data(), ddata.size());
        }
        else
        {
            //保存到文件中
            GrpIdxFile::writeFile(getFilename(num, 's'), sdata.data(), sdata.size());
            GrpIdxFile::writeFile(getFilename(num, 'd'), ddata.data(), ddata.size());
        }
    }
    //最后处理db文件
    if (ZIP_SAVE)
    {
        db.close();
        zip.addFile("1.db", filenamedb);
        filefunc::removeFile(filenamedb);    //删除临时文件
    }

    return true;
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
