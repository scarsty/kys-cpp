#pragma once

#include "ChessContentLoader.h"
#include "ChessDiagnostics.h"
#include "ChessGameplayEffect.h"
#include "ChessGameSession.h"
#include "ChessPvp.h"

#include <yaml-cpp/yaml.h>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace KysChess::Test
{

inline void enableFastTestBattle(
    ChessGameContentData& data,
    ChessRoleDefinition& role)
{
    constexpr int magicId = 1;
    role.Speed = 100;
    role.Fist = 100;
    for (int index = 0; index < ROLE_MAGIC_COUNT; ++index)
    {
        role.MagicID[index] = magicId;
        role.MagicPower[index] = 500;
    }

    ChessMagicDefinition magic;
    magic.ID = magicId;
    magic.Name = "快速測試武學";
    magic.MagicType = 1;
    magic.AttackAreaType = 0;
    magic.SelectDistance = 4;
    data.magics.try_emplace(magic.ID, std::move(magic));
}

inline std::shared_ptr<const ChessGameContent> managementContent(
    int initialMoney = 100,
    Difficulty difficulty = Difficulty::Normal,
    std::string gameVersion = "dev")
{
    ChessGameContentData data;
    data.difficulty = difficulty;
    data.balance.initialMoney = initialMoney;
    data.balance.shopSlotCount = 5;
    data.balance.refreshCost = 2;
    data.balance.buyExpCost = 5;
    data.balance.buyExpAmount = 5;
    data.balance.benchSize = 10;
    data.balance.minBattleSize = 2;
    data.balance.expTable = {4, 6, 10};
    data.balance.maxLevel = 3;
    for (auto& row : data.balance.shopWeights)
    {
        row = {100, 0, 0, 0, 0};
    }

    ChessRoleDefinition role;
    role.ID = 10;
    role.Name = "測試棋子";
    role.Cost = 1;
    role.MaxHP = 500;
    role.Attack = 50;
    enableFastTestBattle(data, role);
    data.roles.emplace(role.ID, role);
    data.poolRoleIds.push_back(role.ID);
    return std::make_shared<const ChessGameContent>(
        std::move(data),
        std::move(gameVersion));
}

inline std::shared_ptr<const ChessGameContent> configuredMapChoiceContent(
    std::vector<EffectRule> equipmentRules = {})
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;
    data.balance.initialMoney = 100;
    data.balance.shopSlotCount = 3;
    data.balance.minBattleSize = 1;
    data.balance.enemyTable = {{{2, 1}}};
    for (auto& row : data.balance.shopWeights)
    {
        row = {100, 0, 0, 0, 0};
    }

    ChessRoleDefinition ally;
    ally.ID = 10;
    ally.Name = "選圖測試棋子";
    ally.Cost = 1;
    ally.MaxHP = 500;
    ally.Attack = 50;
    enableFastTestBattle(data, ally);
    data.roles.emplace(ally.ID, ally);
    ChessRoleDefinition enemy = ally;
    enemy.ID = 20;
    enemy.Name = "選圖測試敵人";
    enemy.Cost = 2;
    data.roles.emplace(enemy.ID, enemy);
    ChessRoleDefinition proxy = ally;
    proxy.ID = 30;
    proxy.Name = "裝備代入棋子";
    data.roles.emplace(proxy.ID, proxy);
    data.poolRoleIds = {ally.ID, enemy.ID};

    ComboDef combo;
    combo.id = 0;
    combo.name = "配置選圖羈絆";
    combo.memberRoleIds = {ally.ID};
    combo.starSynergyBonus = true;
    combo.thresholds.push_back({
        2,
        "配置門檻",
        {},
        {BattleMapChoiceRule{}},
    });
    data.combos.push_back(std::move(combo));
    data.items.emplace(500, ChessItemDefinition{
        500, -1, 0, 1, 0, 10, 0, 0, 0, 0, 0, 0, 0, "配置選圖劍"});
    data.equipment.push_back({
        500,
        1,
        0,
        std::move(equipmentRules),
        {CountsAsComboRule{"配置選圖羈絆"}},
    });

    for (const int mapId : {7, 8})
    {
        ChessBattleMapDefinition map;
        map.id = mapId;
        map.battlefieldId = mapId;
        map.teammateRoleIds = std::vector<int>(10, -1);
        map.enemyRoleIds = std::vector<int>(20, -1);
        for (int index = 0; index < 10; ++index)
        {
            map.teammateX.push_back(20 + index);
            map.teammateY.push_back(20);
        }
        for (int index = 0; index < 20; ++index)
        {
            map.enemyX.push_back(40 - index);
            map.enemyY.push_back(40);
        }
        data.battleMaps.emplace(map.id, std::move(map));
        ChessBattlefieldDefinition field;
        field.id = mapId;
        field.layers.assign(2 * 64 * 64, 0);
        data.battlefields.emplace(field.id, std::move(field));
    }
    return std::make_shared<const ChessGameContent>(std::move(data));
}

inline std::shared_ptr<const ChessGameContent> singlePieceChallengeContent(int totalFights = 28)
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;
    data.balance.initialMoney = 10;
    data.balance.shopSlotCount = 1;
    data.balance.minBattleSize = 2;
    data.balance.totalFights = totalFights;
    data.balance.enemyTable = {{{1, 1}}};
    for (auto& row : data.balance.shopWeights)
    {
        row = {100, 0, 0, 0, 0};
    }

    ChessRoleDefinition role;
    role.ID = 10;
    role.Name = "單騎棋子";
    role.Cost = 1;
    role.MaxHP = 100;
    data.roles.emplace(role.ID, role);
    data.poolRoleIds = {role.ID};

    BalanceConfig::ChallengeDef challenge;
    challenge.name = "單騎遠征";
    challenge.enemies.push_back({role.ID, 1});
    data.balance.challenges.push_back(std::move(challenge));
    return std::make_shared<const ChessGameContent>(std::move(data));
}

inline ChessAction buySlot(int slot)
{
    ChessAction action;
    action.type = ChessActionType::BuyShopSlot;
    action.shopSlot = slot;
    return action;
}

namespace Detail
{
inline void appendEffectDefinition(
    ChessGameContentData& data,
    int magicId,
    std::string_view effectsYaml)
{
    ChessMagicEffectDefinition definition;
    definition.magicId = magicId;
    std::vector<GameplayEffect> effects;
    std::uint64_t id{};
    ChessDiagnosticCollector diagnostics;
    REQUIRE(parseGameplayEffects(
        YAML::Load(std::string(effectsYaml)),
        effects,
        definition.rules,
        id,
        "合成內容效果",
        diagnostics.sink()));
    REQUIRE_FALSE(diagnostics.hasErrors());
    definition.effects = std::move(effects);
    data.magicEffects.push_back(std::move(definition));
}

inline void appendEquipment(
    ChessGameContentData& data,
    int itemId,
    int equipType,
    int tier,
    std::string_view name,
    std::string_view effectsYaml)
{
    data.items.emplace(
        itemId,
        ChessItemDefinition{itemId, -1, equipType, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, std::string(name)});
    ChessGameContentData scratch;
    appendEffectDefinition(scratch, itemId, effectsYaml);
    EquipmentDef def;
    def.itemId = itemId;
    def.tier = tier;
    def.equipType = equipType;
    def.rules = scratch.magicEffects.front().rules;
    def.effects = scratch.magicEffects.front().effects;
    data.equipment.push_back(std::move(def));
}
}  // namespace Detail

// 合成內容：協定、編解碼、錦標賽與戰鬥測試自有的完整棋局內容。
// 測試不讀取頂層設定檔；需要新欄位時在此補上測試自有數值，
// 讓斷言鎖定的數字都屬於本函式，而非會隨平衡調整的外部檔案。
inline std::shared_ptr<const ChessGameContent> syntheticContent(
    Difficulty difficulty = Difficulty::Normal,
    std::string gameVersion = "dev")
{
    ChessGameContentData data;
    data.difficulty = difficulty;
    auto& balance = data.balance;
    balance.initialMoney = 100;
    balance.shopSlotCount = 5;
    balance.refreshCost = 2;
    balance.buyExpCost = 5;
    balance.buyExpAmount = 5;
    balance.benchSize = 10;
    balance.minBattleSize = 2;
    balance.expTable = {4, 6, 10};
    balance.maxLevel = 3;
    balance.totalFights = 28;
    balance.legendaryShop = {.unlockFight = 5, .price = 30};
    balance.playerEquipmentRewards = {{3, 2, 2, 4, 1}, {7, 3, 2, 4, 1}};
    if (difficulty != Difficulty::Easy)
    {
        balance.banUnlocks.push_back({.afterFight = 3, .slots = 2, .maxTier = 3});
    }
    for (auto& row : balance.shopWeights)
    {
        row = {100, 0, 0, 0, 0};
    }

    const auto makeRole = [&](int id, int cost, int maxHp, int attack)
    {
        ChessRoleDefinition role;
        role.ID = id;
        role.Name = std::format("合成棋子{}", id);
        role.Cost = cost;
        role.MaxHP = maxHp;
        role.Attack = attack;
        enableFastTestBattle(data, role);
        data.roles.emplace(role.ID, role);
        data.poolRoleIds.push_back(role.ID);
        return role;
    };
    makeRole(10, 1, 500, 50);
    auto stacking = makeRole(69, 2, 800, 60);
    stacking.MagicID[0] = 901;
    stacking.MagicPower[0] = 500;
    data.roles.at(69) = stacking;
    for (const int id : {4, 11, 12, 13, 14, 15, 16, 160})
    {
        makeRole(id, 1, 400, 45);
    }

    // 一般武功不掛效果定義，讓面板呈現「沒有額外配置」說明；
    // 武功 901 為疊層型，完整說明含執行細節字樣。
    Detail::appendEffectDefinition(
        data,
        901,
        R"([{類型: 出招疊加增傷減傷, 每次層數: 1, 層數上限: 10, 每層增傷百分比: 5, 每層承傷百分比: -1}])");

    ChessMagicDefinition stackingMagic;
    stackingMagic.ID = 901;
    stackingMagic.Name = "合成疊層武學";
    stackingMagic.MagicType = 1;
    stackingMagic.AttackAreaType = 0;
    stackingMagic.SelectDistance = 4;
    data.magics.try_emplace(stackingMagic.ID, std::move(stackingMagic));

    Detail::appendEquipment(
        data,
        800,
        0,
        3,
        "屠龍刀",
        R"([{類型: 命中忽略防禦, 忽略防禦百分比: 40}])");
    Detail::appendEquipment(
        data,
        801,
        1,
        4,
        "倚天劍",
        R"([{類型: 命中忽略防禦, 忽略防禦百分比: 60}])");

    BalanceConfig::ChallengeDef challenge;
    challenge.name = "倚天屠龍";
    challenge.description = "合成遠征挑戰";
    for (int index = 0; index < 12; ++index)
    {
        challenge.enemies.push_back({69, 3, 800, 801});
    }
    data.balance.challenges.push_back(std::move(challenge));

    // PvP 用戰場：戰鬥地圖 133 搭配戰場 21 的全可走空場地，
    // 供 ChessPvpMapLayout 座標直接擺子。
    ChessBattleMapDefinition pvpMap;
    pvpMap.id = ChessPvpMapLayout::BattleId;
    pvpMap.battlefieldId = ChessPvpMapLayout::BattlefieldId;
    data.battleMaps.emplace(pvpMap.id, std::move(pvpMap));
    ChessBattlefieldDefinition pvpField;
    pvpField.id = ChessPvpMapLayout::BattlefieldId;
    pvpField.layers.assign(2 * 64 * 64, 0);
    data.battlefields.emplace(pvpField.id, std::move(pvpField));

    return std::make_shared<const ChessGameContent>(
        std::move(data),
        std::move(gameVersion));
}

}
