#include "Save.h"
#include "ChessModHook.h"
#include "ChessSessionCheckpoint.h"
#include "GameUtil.h"
#include "GrpIdxFile.h"
#include "NewSave.h"
#include "SQLite3Wrapper.h"
#include "filefunc.h"

#include <glaze/json.hpp>

#include <string_view>

namespace SavePersistence
{

constexpr auto kWriteOptions = glz::opts{.prettify = true};
constexpr auto kReadOptions = glz::opts{.error_on_unknown_keys = false};

using SceneStateData = KysChess::ChessSaveSceneState;
using SlotData = KysChess::ChessSaveSlotData;

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

SlotData captureSlotData(const Save& save)
{
    SlotData data;
    data.scene.inSubMap = save.InSubMap;
    data.scene.subMapX = save.SubMapX;
    data.scene.subMapY = save.SubMapY;
    data.scene.faceTowards = save.FaceTowards;
    data.checkpoint = KysChess::ChessModHook::exportCheckpoint().toData();
    return data;
}

std::optional<KysChess::ChessSessionCheckpoint> parseSlotCheckpoint(
    const SlotData& data,
    std::string& error)
{
    KysChess::ChessCheckpointError checkpointError;
    auto checkpoint = KysChess::ChessSessionCheckpoint::fromData(
        data.checkpoint,
        checkpointError);
    if (!checkpoint)
    {
        error = KysChess::chessCheckpointErrorDescription(checkpointError);
    }
    return checkpoint;
}

void applySceneState(const SceneStateData& scene, Save& save)
{
    save.InSubMap = scene.inSubMap;
    save.SubMapX = scene.subMapX;
    save.SubMapY = scene.subMapY;
    save.FaceTowards = scene.faceTowards;
}

bool parseSlotJson(std::string_view payload, SlotData& data, std::string& error)
{
    error.clear();
    if (const auto result = glz::read<kReadOptions>(data, payload); result)
    {
        error = glz::format_error(result, payload);
        return false;
    }
    return true;
}

bool serializeSlotJson(const SlotData& data, std::string& payload, std::string& error)
{
    payload.clear();
    error.clear();
    if (const auto result = glz::write<kWriteOptions>(data, payload); result)
    {
        error = glz::format_error(result);
        payload.clear();
        return false;
    }
    return true;
}

bool readSlotJson(const std::string& path, SlotData& data, std::string& error)
{
    if (!filefunc::fileExist(path))
    {
        error = "找不到存檔檔案";
        return false;
    }

    auto payload = filefunc::readFileToString(path);
    if (payload.empty())
    {
        error = "存檔檔案是空的或無法讀取";
        return false;
    }

    return parseSlotJson(payload, data, error);
}

bool writeSlotJson(const std::string& path, const SlotData& data, std::string& error)
{
    std::string payload;
    if (!serializeSlotJson(data, payload, error))
    {
        return false;
    }

    filefunc::makePath(filefunc::getParentPath(path));
    if (filefunc::writeStringToFile(payload, path) <= 0)
    {
        error = "無法寫入存檔檔案";
        return false;
    }
    return true;
}

std::string slotJsonLoadFilename(int slot)
{
    auto persistedPath = slotJsonFilename(slot);
    if (filefunc::fileExist(persistedPath))
    {
        return persistedPath;
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

bool Save::loadSharedGameData()
{
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

    // 读取SD数据，始终从默认文件读取。
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

bool Save::prepareChessMode()
{
    if (!loadSharedGameData())
    {
        return false;
    }

    KysChess::ChessModHook::initializeSaveState(*this);
    return true;
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
    auto jsonPath = SavePersistence::slotJsonLoadFilename(num);
    if (jsonPath.empty())
    {
        LOG("[存檔讀取] 槽位 {} 失敗（步驟 1/2：尋找存檔）：找不到存檔檔案\n", num);
        return false;
    }

    SavePersistence::SlotData slotData;
    std::string error;
    if (!SavePersistence::readSlotJson(jsonPath, slotData, error))
    {
        LOG(
            "[存檔讀取] 槽位 {} 失敗（步驟 1/2：讀取並解析 JSON）：{}；檔案 '{}'\n",
            num,
            error,
            jsonPath);
        return false;
    }

    auto checkpoint = SavePersistence::parseSlotCheckpoint(slotData, error);
    if (!checkpoint)
    {
        LOG(
            "[存檔讀取] 槽位 {} 失敗（步驟 2/2：解析自走棋檢查點）：{}；檔案 '{}'\n",
            num,
            error,
            jsonPath);
        return false;
    }
    if (!KysChess::ChessModHook::importCheckpoint(*checkpoint, *this))
    {
        LOG(
            "[存檔讀取] 槽位 {} 失敗（步驟 2/2：驗證並還原自走棋資料）；檔案 '{}'\n",
            num,
            jsonPath);
        return false;
    }
    SavePersistence::applySceneState(slotData.scene, *this);

    return true;
}

bool Save::save(int num)
{
    if (!KysChess::ChessModHook::canSaveCheckpoint())
    {
        LOG("[存檔寫入] 槽位 {} 失敗（步驟 1/2：檢查可存檔狀態）\n", num);
        return false;
    }
    auto slotPath = SavePersistence::slotJsonFilename(num);
    auto slotData = SavePersistence::captureSlotData(*this);
    std::string error;
    if (!SavePersistence::writeSlotJson(slotPath, slotData, error))
    {
        LOG(
            "[存檔寫入] 槽位 {} 失敗（步驟 2/2：序列化並寫入 JSON）：{}；檔案 '{}'\n",
            num,
            error,
            slotPath);
        return false;
    }

    GameUtil::syncPersistentStorage();
    return true;
}

bool Save::exportSlotJson(int num, std::string& payload)
{
    payload.clear();

    SavePersistence::SlotData slotData;
    auto slotPath = SavePersistence::slotJsonLoadFilename(num);
    if (slotPath.empty())
    {
        LOG("[外部存檔匯出] 槽位 {} 失敗（步驟 1/3：尋找存檔）：找不到存檔檔案\n", num);
        return false;
    }

    std::string error;
    if (!SavePersistence::readSlotJson(slotPath, slotData, error))
    {
        LOG(
            "[外部存檔匯出] 槽位 {} 失敗（步驟 1/3：讀取並解析 JSON）：{}；檔案 '{}'\n",
            num,
            error,
            slotPath);
        return false;
    }
    auto checkpoint = SavePersistence::parseSlotCheckpoint(slotData, error);
    if (!checkpoint
        || !KysChess::ChessModHook::isCheckpointReadable(*checkpoint, error))
    {
        LOG(
            "[外部存檔匯出] 槽位 {} 驗證失敗（步驟 2/3：檢查自走棋檢查點）：{}\n",
            num,
            error);
        return false;
    }
    if (!SavePersistence::serializeSlotJson(slotData, payload, error))
    {
        LOG(
            "[外部存檔匯出] 槽位 {} 失敗（步驟 3/3：序列化匯出 JSON）：{}\n",
            num,
            error);
        return false;
    }

    return true;
}

bool Save::importSlotJson(int num, const std::string& payload)
{
    KysChess::ChessCheckpointError checkpointError;
    auto parsed = KysChess::parseChessSavePayload(payload, checkpointError);
    if (!parsed)
    {
        LOG(
            "[外部存檔匯入] 槽位 {} 失敗（步驟 1/3：解析存檔 JSON）：{}\n",
            num,
            KysChess::chessCheckpointErrorDescription(checkpointError));
        return false;
    }

    SavePersistence::SlotData slotData;
    slotData.scene = parsed->scene.value_or(
        KysChess::ChessModHook::initialSaveSceneState());
    slotData.checkpoint = parsed->checkpoint.toData();
    std::string error;
    if (!KysChess::ChessModHook::isCheckpointReadable(parsed->checkpoint, error))
    {
        LOG(
            "[外部存檔匯入] 槽位 {} 驗證失敗（步驟 2/3：檢查自走棋檢查點）：{}\n",
            num,
            error);
        return false;
    }

    auto slotPath = SavePersistence::slotJsonFilename(num);
    if (!SavePersistence::writeSlotJson(slotPath, slotData, error))
    {
        LOG(
            "[外部存檔匯入] 槽位 {} 失敗（步驟 3/3：寫入存檔）：{}；檔案 '{}'\n",
            num,
            error,
            slotPath);
        return false;
    }

    GameUtil::syncPersistentStorage();
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
    updateAllPtrVector();

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
