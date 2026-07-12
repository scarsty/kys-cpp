#include "ChessContentLoader.h"

#include "BattleMap.h"
#include "ChessBattleEffects.h"
#include "ChessCombo.h"
#include "ChessEquipment.h"
#include "ChessNeigong.h"
#include "GameVersion.h"
#include "GrpIdxFile.h"
#include "SQLite3Wrapper.h"
#include "SimpleCC.h"
#include "filefunc.h"
#include "yaml-cpp/yaml.h"

#include <cstring>
#include <algorithm>
#include <format>
#include <map>

namespace KysChess
{
namespace
{

std::string pathText(const std::filesystem::path& path)
{
    return path.generic_string();
}

bool loadDatabaseRecords(
    const std::filesystem::path& databasePath,
    ChessGameContentData& data,
    const ChessDiagnosticSink& diagnostics)
{
    SQLite3Wrapper database;
    if (!database.open(pathText(databasePath)))
    {
        emitChessDiagnostic(
            diagnostics,
            ChessDiagnosticSeverity::Error,
            "遊戲資料",
            std::format("無法開啟資料庫 {}", pathText(databasePath)));
        return false;
    }
    const auto text = [](SQLite3Stmt& statement, int column) {
        const auto* value = statement.getColumnText(column);
        return value ? std::string(reinterpret_cast<const char*>(value)) : std::string{};
    };
    auto roles = database.query("select * from role");
    if (!roles.isValid())
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "遊戲資料", "角色資料表無效");
        return false;
    }
    while (roles.step())
    {
        ChessRoleDefinition definition;
        definition.ID = roles.getColumnInt(0);
        definition.HeadID = roles.getColumnInt(1);
        definition.Name = text(roles, 2);
        definition.Nick = text(roles, 3);
        definition.Cost = roles.getColumnInt(4);
        definition.Sexual = roles.getColumnInt(5);
        definition.MaxHP = roles.getColumnInt(6);
        definition.MPType = roles.getColumnInt(7);
        definition.MaxMP = roles.getColumnInt(8);
        definition.Attack = roles.getColumnInt(9);
        definition.Speed = roles.getColumnInt(10);
        definition.Defence = roles.getColumnInt(11);
        definition.Medicine = roles.getColumnInt(12);
        definition.UsePoison = roles.getColumnInt(13);
        definition.Detoxification = roles.getColumnInt(14);
        definition.AntiPoison = roles.getColumnInt(15);
        definition.Fist = roles.getColumnInt(16);
        definition.Sword = roles.getColumnInt(17);
        definition.Knife = roles.getColumnInt(18);
        definition.Unusual = roles.getColumnInt(19);
        definition.HiddenWeapon = roles.getColumnInt(20);
        definition.Knowledge = roles.getColumnInt(21);
        definition.Morality = roles.getColumnInt(22);
        definition.AttackWithPoison = roles.getColumnInt(23);
        definition.AttackTwice = roles.getColumnInt(24);
        definition.Fame = roles.getColumnInt(25);
        definition.IQ = roles.getColumnInt(26);
        for (int slot = 0; slot < ROLE_MAGIC_COUNT; ++slot)
        {
            definition.MagicID[slot] = roles.getColumnInt(27 + slot * 2);
            definition.MagicPower[slot] = roles.getColumnInt(28 + slot * 2);
        }
        data.roles.emplace(definition.ID, std::move(definition));
    }

    auto magics = database.query("select * from magic");
    if (!magics.isValid())
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "遊戲資料", "武學資料表無效");
        return false;
    }
    while (magics.step())
    {
        ChessMagicDefinition definition;
        definition.ID = magics.getColumnInt(0);
        definition.Name = text(magics, 1);
        definition.SoundID = magics.getColumnInt(2);
        definition.MagicType = magics.getColumnInt(3);
        definition.EffectID = magics.getColumnInt(4);
        definition.HurtType = magics.getColumnInt(5);
        definition.AttackAreaType = magics.getColumnInt(6);
        definition.NeedMP = magics.getColumnInt(7);
        definition.WithPoison = magics.getColumnInt(8);
        definition.SelectDistance = magics.getColumnInt(9);
        definition.AttackDistance = magics.getColumnInt(10);
        definition.AddMP = magics.getColumnInt(11);
        definition.HurtMP = magics.getColumnInt(12);
        data.magics.emplace(definition.ID, std::move(definition));
    }

    auto items = database.query("select * from item");
    if (!items.isValid())
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "遊戲資料", "物品資料表無效");
        return false;
    }
    while (items.step())
    {
        ChessItemDefinition definition;
        definition.id = items.getColumnInt(0);
        definition.name = text(items, 1);
        definition.magicId = items.getColumnInt(3);
        definition.equipType = items.getColumnInt(6);
        definition.itemType = items.getColumnInt(8);
        definition.addMaxHP = items.getColumnInt(13);
        definition.addAttack = items.getColumnInt(19);
        definition.addSpeed = items.getColumnInt(20);
        definition.addDefence = items.getColumnInt(21);
        definition.addFist = items.getColumnInt(26);
        definition.addSword = items.getColumnInt(27);
        definition.addKnife = items.getColumnInt(28);
        definition.addUnusual = items.getColumnInt(29);
        definition.addHiddenWeapon = items.getColumnInt(30);
        data.items.emplace(definition.id, std::move(definition));
    }
    database.close();
    return true;
}

void loadBattleMaps(const std::filesystem::path& root, ChessGameContentData& data)
{
    std::vector<BattleInfo> records;
    filefunc::readFileToVector(pathText(root / "resource" / "war.sta"), records);
    for (const auto& record : records)
    {
        ChessBattleMapDefinition definition;
        definition.id = record.ID;
        definition.battlefieldId = record.BattleFieldID;
        definition.musicId = record.Music;
        for (int index = 0; index < TEAMMATE_COUNT; ++index)
        {
            definition.teammateRoleIds.push_back(record.TeamMate[index]);
            definition.teammateX.push_back(record.TeamMateX[index]);
            definition.teammateY.push_back(record.TeamMateY[index]);
        }
        for (int index = 0; index < BATTLE_ENEMY_COUNT; ++index)
        {
            definition.enemyRoleIds.push_back(record.Enemy[index]);
            definition.enemyX.push_back(record.EnemyX[index]);
            definition.enemyY.push_back(record.EnemyY[index]);
        }
        data.battleMaps.emplace(definition.id, std::move(definition));
    }

    std::vector<int> offsets;
    std::vector<int> lengths;
    const auto bytes = GrpIdxFile::getIdxContent(
        pathText(root / "resource" / "warfld.idx"),
        pathText(root / "resource" / "warfld.grp"),
        &offsets,
        &lengths);
    for (std::size_t index = 0; index < lengths.size(); ++index)
    {
        ChessBattlefieldDefinition definition;
        definition.id = static_cast<int>(index);
        definition.layers.resize(BATTLEMAP_SAVE_LAYER_COUNT * BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT);
        const std::size_t requestedBytes = definition.layers.size() * sizeof(std::int16_t);
        if (static_cast<std::size_t>(lengths[index]) >= requestedBytes)
        {
            std::memcpy(definition.layers.data(), bytes.data() + offsets[index], requestedBytes);
            data.battlefields.emplace(definition.id, std::move(definition));
        }
    }
}

bool loadPoolRoleIds(
    const std::filesystem::path& path,
    std::vector<int>& roleIds,
    const ChessDiagnosticSink& diagnostics)
{
    roleIds.clear();
    try
    {
        const auto root = YAML::LoadFile(pathText(path));
        const auto roles = root["角色"];
        if (!roles || !roles.IsSequence())
        {
            emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "棋池配置", "缺少「角色」列表");
            return false;
        }
        for (const auto& role : roles)
        {
            const int roleId = role.as<int>();
            if (std::ranges::contains(roleIds, roleId))
            {
                emitChessDiagnostic(
                    diagnostics,
                    ChessDiagnosticSeverity::Error,
                    "棋池配置",
                    std::format("角色 ID {} 重複", roleId));
                return false;
            }
            roleIds.push_back(roleId);
        }
    }
    catch (const YAML::Exception& ex)
    {
        emitChessDiagnostic(diagnostics, ChessDiagnosticSeverity::Error, "棋池配置", std::format("無法讀取檔案 {}: {}", pathText(path), ex.what()));
        return false;
    }
    return true;
}

}

bool loadChessPoolRoleIds(
    const std::filesystem::path& path,
    std::vector<int>& roleIds,
    const ChessDiagnosticSink& diagnostics)
{
    return loadPoolRoleIds(path, roleIds, diagnostics);
}

std::optional<ChessGameContent> ChessContentLoader::load(const ChessContentLoadOptions& options)
{
    const auto root = std::filesystem::weakly_canonical(options.dataRoot);
    SimpleCC converter;
    if (converter.init({
            pathText(root / "cc" / "STPhrases.txt"),
            pathText(root / "cc" / "STCharacters.txt")}) != 0)
    {
        emitChessDiagnostic(options.diagnostics, ChessDiagnosticSeverity::Error, "文字轉換", "無法載入簡繁轉換資料");
        return std::nullopt;
    }
    const ChessTextConverter toTraditional = [&converter](std::string_view text) {
        return converter.conv(std::string(text));
    };

    ChessGameContentData data;
    data.difficulty = options.difficulty;
    if (!loadDatabaseRecords(root / "save" / "game.db", data, options.diagnostics))
    {
        return std::nullopt;
    }

    const auto configRoot = options.configRoot.empty()
        ? root / "config"
        : std::filesystem::weakly_canonical(options.configRoot);
    const auto poolPath = configRoot /
        (options.difficulty == Difficulty::Easy ? "chess_pool_easy.yaml" : "chess_pool.yaml");
    if (!loadChessPoolRoleIds(poolPath, data.poolRoleIds, options.diagnostics))
    {
        return std::nullopt;
    }
    const auto balancePath = configRoot /
        std::format("chess_balance_{}.yaml", ChessBalance::difficultyConfigSuffix(options.difficulty));
    if (!loadBalanceConfig(
            pathText(balancePath),
            pathText(configRoot / "chess_challenge.yaml"),
            toTraditional,
            options.diagnostics,
            data.balance))
    {
        return std::nullopt;
    }

    data.combos = loadChessCombos(pathText(configRoot / "chess_combos.yaml"), toTraditional, options.diagnostics);
    if (data.combos.empty())
    {
        return std::nullopt;
    }
    if (!loadChessEquipment(
            pathText(configRoot / "chess_equipment.yaml"),
            toTraditional,
            options.diagnostics,
            data.equipment,
            data.equipmentSynergies))
    {
        return std::nullopt;
    }

    std::vector<Item> legacyItems;
    legacyItems.reserve(data.items.size());
    for (const auto& [id, source] : data.items)
    {
        Item item;
        item.ID = source.id;
        item.Name = source.name;
        item.MagicID = source.magicId;
        item.EquipType = source.equipType;
        item.ItemType = source.itemType;
        item.AddMaxHP = source.addMaxHP;
        item.AddAttack = source.addAttack;
        item.AddSpeed = source.addSpeed;
        item.AddDefence = source.addDefence;
        item.AddFist = source.addFist;
        item.AddSword = source.addSword;
        item.AddKnife = source.addKnife;
        item.AddUnusual = source.addUnusual;
        item.AddHiddenWeapon = source.addHiddenWeapon;
        legacyItems.push_back(std::move(item));
    }
    std::vector<Magic> legacyMagics;
    legacyMagics.reserve(data.magics.size());
    for (const auto& [id, source] : data.magics)
    {
        Magic magic;
        static_cast<MagicSave&>(magic) = source;
        legacyMagics.push_back(std::move(magic));
    }
    std::vector<Item*> itemPointers;
    for (auto& item : legacyItems)
    {
        itemPointers.push_back(&item);
    }
    std::map<int, const Magic*> magicById;
    for (const auto& magic : legacyMagics)
    {
        magicById.emplace(magic.ID, &magic);
    }
    if (!loadChessNeigong(
            pathText(configRoot / "chess_neigong.yaml"),
            itemPointers,
            [&magicById](int id) {
                const auto found = magicById.find(id);
                return found == magicById.end() ? nullptr : found->second;
            },
            options.diagnostics,
            data.neigongConfig,
            data.neigong))
    {
        return std::nullopt;
    }
    if (!ChessBattleEffects::loadMagicEffectsFile(
            pathText(configRoot / "chess_magic_effects.yaml"),
            data.magicEffects,
            options.diagnostics))
    {
        return std::nullopt;
    }
    loadBattleMaps(root, data);
    emitChessDiagnostic(options.diagnostics, ChessDiagnosticSeverity::Info, "遊戲內容", "不可變規則內容載入完成");
    return ChessGameContent(std::move(data), loadGameVersion(root));
}

}
