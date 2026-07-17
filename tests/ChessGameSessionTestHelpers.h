#pragma once

#include "ChessContentLoader.h"
#include "ChessGameSession.h"

#include <filesystem>
#include <memory>

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

inline std::shared_ptr<const ChessGameContent> managementContent(int initialMoney = 100)
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;
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
    return std::make_shared<const ChessGameContent>(std::move(data));
}

inline std::shared_ptr<const ChessGameContent> actualContent(Difficulty difficulty = Difficulty::Normal)
{
    ChessContentLoadOptions options;
    options.dataRoot = std::filesystem::current_path() / "work" / "game-dev";
    options.configRoot = std::filesystem::current_path() / "config";
    options.difficulty = difficulty;
    auto loaded = ChessContentLoader::load(options);
    return loaded
        ? std::make_shared<const ChessGameContent>(std::move(*loaded))
        : nullptr;
}

inline std::shared_ptr<const ChessGameContent> configuredMapChoiceContent()
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
        {{EffectType::BattleMapChoice, 1}},
    });
    data.combos.push_back(std::move(combo));
    data.items.emplace(500, ChessItemDefinition{
        500, -1, 0, 1, 0, 10, 0, 0, 0, 0, 0, 0, 0, "配置選圖劍"});
    data.equipment.push_back({500, 1, 0, {}, {"配置選圖羈絆"}});

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

}
