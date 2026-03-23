#include "Save.h"
#include "ChessModHook.h"
#include "GameDataStore.h"
#include "GameUtil.h"
#include "GrpIdxFile.h"
#include "NewSave.h"
#include "PotConv.h"
#include "SQLite3Wrapper.h"
#include "filefunc.h"

#include <array>
#include <glaze/json.hpp>

namespace SavePersistence
{

constexpr auto kWriteOptions = glz::opts{.prettify = true};
constexpr auto kReadOptions = glz::opts{.error_on_unknown_keys = false};

struct BaseInfoData
{
    int inShip = 0;
    int inSubMap = 0;
    int mainMapX = 0;
    int mainMapY = 0;
    int subMapX = 0;
    int subMapY = 0;
    int faceTowards = 0;
    int shipX = 0;
    int shipY = 0;
    int shipX1 = 0;
    int shipY1 = 0;
    int encode = 0;
    std::array<int, TEAMMATE_COUNT> team{};
};

struct BagEntry
{
    int itemId = -1;
    int count = 0;
};

struct SlotData
{
    BaseInfoData base;
    std::vector<BagEntry> bag;
    KysChess::GameDataStore gameData;
};

std::string sharedGameDbFilename()
{
    return std::format("{}save/game.db", GameUtil::PATH());
}

std::string slotJsonFilename(int slot)
{
#ifdef __EMSCRIPTEN__
    return std::format("/persist/{}.json", slot);
#else
    return std::format("{}save/{}.json", GameUtil::PATH(), slot);
#endif
}

std::string packagedSlotZeroJsonFilename()
{
    return GameUtil::PATH() + "save/0.json";
}

BaseInfoData captureBaseInfo(const Save& save)
{
    BaseInfoData base;
    base.inShip = save.InShip;
    base.inSubMap = save.InSubMap;
    base.mainMapX = save.MainMapX;
    base.mainMapY = save.MainMapY;
    base.subMapX = save.SubMapX;
    base.subMapY = save.SubMapY;
    base.faceTowards = save.FaceTowards;
    base.shipX = save.ShipX;
    base.shipY = save.ShipY;
    base.shipX1 = save.ShipX1;
    base.shipY1 = save.ShipY1;
    base.encode = save.Encode;
    for (int i = 0; i < TEAMMATE_COUNT; ++i)
    {
        base.team[i] = save.Team[i];
    }
    return base;
}

void applyBaseInfo(const BaseInfoData& base, Save& save)
{
    save.InShip = base.inShip;
    save.InSubMap = base.inSubMap;
    save.MainMapX = base.mainMapX;
    save.MainMapY = base.mainMapY;
    save.SubMapX = base.subMapX;
    save.SubMapY = base.subMapY;
    save.FaceTowards = base.faceTowards;
    save.ShipX = base.shipX;
    save.ShipY = base.shipY;
    save.ShipX1 = base.shipX1;
    save.ShipY1 = base.shipY1;
    save.Encode = base.encode;
    for (int i = 0; i < TEAMMATE_COUNT; ++i)
    {
        save.Team[i] = base.team[i];
    }
}

std::vector<BagEntry> captureBag(const Save& save)
{
    std::vector<BagEntry> bag;
    bag.reserve(ITEM_IN_BAG_COUNT);
    for (int i = 0; i < ITEM_IN_BAG_COUNT; ++i)
    {
        if (save.Items[i].item_id < 0)
        {
            break;
        }
        bag.push_back({save.Items[i].item_id, save.Items[i].count});
    }
    return bag;
}

void applyBag(const std::vector<BagEntry>& bag, Save& save)
{
    for (int i = 0; i < ITEM_IN_BAG_COUNT; ++i)
    {
        save.Items[i] = {};
    }

    const size_t maxCount = static_cast<size_t>(ITEM_IN_BAG_COUNT);
    const size_t count = bag.size() < maxCount ? bag.size() : maxCount;
    for (size_t i = 0; i < count; ++i)
    {
        save.Items[i].item_id = bag[i].itemId;
        save.Items[i].count = bag[i].count;
    }
}

SlotData captureSlotData(const Save& save)
{
    SlotData data;
    data.base = captureBaseInfo(save);
    data.bag = captureBag(save);
    data.gameData = KysChess::ChessModHook::exportGameData();
    return data;
}

void applySlotData(const SlotData& data, Save& save)
{
    applyBaseInfo(data.base, save);
    applyBag(data.bag, save);
    KysChess::ChessModHook::importGameData(data.gameData);
}

bool readSlotJson(const std::string& path, SlotData& data)
{
    if (!filefunc::fileExist(path))
    {
        return false;
    }

    auto payload = filefunc::readFileToString(path);
    if (payload.empty())
    {
        return false;
    }

    if (const auto result = glz::read<kReadOptions>(data, payload); result)
    {
        return false;
    }
    return true;
}

bool writeSlotJson(const std::string& path, const SlotData& data)
{
    std::string payload;
    if (const auto result = glz::write<kWriteOptions>(data, payload); result)
    {
        return false;
    }

    filefunc::makePath(filefunc::getParentPath(path));
    return filefunc::writeStringToFile(payload, path) > 0;
}

std::string slotJsonLoadFilename(int slot)
{
    auto persistedPath = slotJsonFilename(slot);
    if (filefunc::fileExist(persistedPath))
    {
        return persistedPath;
    }

    if (slot == 0)
    {
        auto packagedPath = packagedSlotZeroJsonFilename();
        if (packagedPath != persistedPath && filefunc::fileExist(packagedPath))
        {
            return packagedPath;
        }
    }

    return {};
}

std::string preferredTimestampFilename(int slot)
{
    if (auto jsonPath = slotJsonLoadFilename(slot); !jsonPath.empty())
    {
        return jsonPath;
    }

    return slotJsonFilename(slot);
}

}    // namespace SavePersistence

template <>
struct glz::meta<SavePersistence::BaseInfoData>
{
    static constexpr auto value = glz::object(
        "inShip", &SavePersistence::BaseInfoData::inShip,
        "inSubMap", &SavePersistence::BaseInfoData::inSubMap,
        "mainMapX", &SavePersistence::BaseInfoData::mainMapX,
        "mainMapY", &SavePersistence::BaseInfoData::mainMapY,
        "subMapX", &SavePersistence::BaseInfoData::subMapX,
        "subMapY", &SavePersistence::BaseInfoData::subMapY,
        "faceTowards", &SavePersistence::BaseInfoData::faceTowards,
        "shipX", &SavePersistence::BaseInfoData::shipX,
        "shipY", &SavePersistence::BaseInfoData::shipY,
        "shipX1", &SavePersistence::BaseInfoData::shipX1,
        "shipY1", &SavePersistence::BaseInfoData::shipY1,
        "encode", &SavePersistence::BaseInfoData::encode,
        "team", &SavePersistence::BaseInfoData::team);
};

template <>
struct glz::meta<SavePersistence::BagEntry>
{
    static constexpr auto value = glz::object(
        "itemId", &SavePersistence::BagEntry::itemId,
        "count", &SavePersistence::BagEntry::count);
};

template <>
struct glz::meta<SavePersistence::SlotData>
{
    static constexpr auto value = glz::object(
        "base", &SavePersistence::SlotData::base,
        "bag", &SavePersistence::SlotData::bag,
        "gameData", &SavePersistence::SlotData::gameData);
};

Save::Save()
{
}

Save::~Save()
{
}

std::string Save::getFilename(int i, char c)
{
    std::string filename;

    if (c == 0)
    {
        return SavePersistence::preferredTimestampFilename(i);
    }

    if (c == 'r')
    {
        return SavePersistence::slotJsonFilename(i);
    }

    if (i > 0)
    {
        if (c == 's')
        {
            return GameUtil::PATH() + "save/allsin.grp";
        }
        if (c == 'd')
        {
            return GameUtil::PATH() + "save/alldef.grp";
        }
        return std::format("{}save/{}{}.grp", GameUtil::PATH(), c, i);
    }
    if (i == 0 && c == 's')
    {
        return GameUtil::PATH() + "save/allsin.grp";
    }
    if (i == 0 && c == 'd')
    {
        return GameUtil::PATH() + "save/alldef.grp";
    }
    return filename;
}

bool Save::checkSaveFileExist(int num)
{
    return !SavePersistence::slotJsonLoadFilename(num).empty();
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

    auto jsonPath = SavePersistence::slotJsonLoadFilename(num);
    auto sharedDbPath = SavePersistence::sharedGameDbFilename();
    if (!filefunc::fileExist(sharedDbPath))
    {
        return false;
    }

    SQLite3Wrapper db;
    if (!db.open(sharedDbPath))
    {
        return false;
    }

    loadStaticDataFromDb(db);
    updateAllPtrVector();

    SavePersistence::SlotData slotData;
    if (!SavePersistence::readSlotJson(jsonPath, slotData))
    {
        return false;
    }

    SavePersistence::applySlotData(slotData, *this);

    //读取SD数据，始终从默认文件读取
    const int submap_count = static_cast<int>(submap_infos_.size());
    {
        std::vector<char> sdata(submap_count * sdata_length_);
        std::vector<char> ddata(submap_count * ddata_length_);
        GrpIdxFile::readFile(getFilename(0, 's'), sdata.data(), submap_count * sdata_length_);
        GrpIdxFile::readFile(getFilename(0, 'd'), ddata.data(), submap_count * ddata_length_);
        for (int i = 0; i < submap_count; i++)
        {
            auto ps = &(submap_infos_mem_[i].LayerData(0, 0, 0));
            auto pd = submap_infos_mem_[i].Event(0);

            memcpy(ps, sdata.data() + sdata_length_ * i, sdata_length_);
            memcpy(pd, ddata.data() + ddata_length_ * i, ddata_length_);
        }
    }
    makeMapsAndRepairID();
    db.close();

    return true;
}

bool Save::save(int num)
{
    auto slotPath = SavePersistence::slotJsonFilename(num);
    auto slotData = SavePersistence::captureSlotData(*this);
    if (!SavePersistence::writeSlotJson(slotPath, slotData))
    {
        return false;
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

void Save::loadStaticDataFromDb(SQLite3Wrapper& db)
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
