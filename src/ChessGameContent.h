#pragma once

#include "ChessBalance.h"
#include "ChessBattleEffects.h"
#include "ChessCombo.h"
#include "ChessEquipment.h"
#include "ChessNeigong.h"
#include "Types.h"

#include <map>
#include <memory>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace KysChess
{

struct ChessRoleDefinition : RoleSave
{
};

struct ChessMagicDefinition : MagicSave
{
};

struct ChessItemDefinition
{
    int id = -1;
    int magicId = -1;
    int equipType = -1;
    int itemType = -1;
    int addMaxHP = 0;
    int addAttack = 0;
    int addSpeed = 0;
    int addDefence = 0;
    int addFist = 0;
    int addSword = 0;
    int addKnife = 0;
    int addUnusual = 0;
    int addHiddenWeapon = 0;
    std::string name;

    auto operator<=>(const ChessItemDefinition&) const = default;
};

struct ChessBattlefieldDefinition
{
    int id = -1;
    std::vector<std::int16_t> layers;

    auto operator<=>(const ChessBattlefieldDefinition&) const = default;
};

struct ChessBattleMapDefinition
{
    int id = -1;
    int battlefieldId = -1;
    int musicId = -1;
    std::string name;
    std::vector<int> teammateRoleIds;
    std::vector<int> teammateX;
    std::vector<int> teammateY;
    std::vector<int> enemyRoleIds;
    std::vector<int> enemyX;
    std::vector<int> enemyY;

    auto operator<=>(const ChessBattleMapDefinition&) const = default;
};

struct ChessGameContentData
{
    Difficulty difficulty = Difficulty::Easy;
    BalanceConfig balance;
    std::map<int, ChessRoleDefinition> roles;
    std::map<int, ChessMagicDefinition> magics;
    std::map<int, ChessItemDefinition> items;
    std::vector<int> poolRoleIds;
    std::vector<ComboDef> combos;
    std::vector<EquipmentDef> equipment;
    std::vector<EquipmentSynergyDef> equipmentSynergies;
    NeigongConfig neigongConfig;
    std::vector<NeigongDef> neigong;
    std::vector<ChessMagicEffectDefinition> magicEffects;
    std::map<int, ChessBattleMapDefinition> battleMaps;
    std::map<int, ChessBattlefieldDefinition> battlefields;
};

class ChessGameContent
{
public:
    explicit ChessGameContent(ChessGameContentData data, std::string gameVersion = "dev");

    Difficulty difficulty() const { return data_->difficulty; }
    const BalanceConfig& balance() const { return data_->balance; }
    const std::map<int, ChessRoleDefinition>& roles() const { return data_->roles; }
    const std::map<int, ChessMagicDefinition>& magics() const { return data_->magics; }
    const std::map<int, ChessItemDefinition>& items() const { return data_->items; }
    const std::vector<int>& poolRoleIds() const { return data_->poolRoleIds; }
    const std::vector<ComboDef>& combos() const { return data_->combos; }
    const std::vector<EquipmentDef>& equipment() const { return data_->equipment; }
    const std::vector<EquipmentSynergyDef>& equipmentSynergies() const { return data_->equipmentSynergies; }
    const NeigongConfig& neigongConfig() const { return data_->neigongConfig; }
    const std::vector<NeigongDef>& neigong() const { return data_->neigong; }
    const std::vector<ChessMagicEffectDefinition>& magicEffects() const { return data_->magicEffects; }
    const std::map<int, ChessBattleMapDefinition>& battleMaps() const { return data_->battleMaps; }
    const std::map<int, ChessBattlefieldDefinition>& battlefields() const { return data_->battlefields; }
    const std::string& gameVersion() const { return gameVersion_; }

    const ChessRoleDefinition* role(int roleId) const;
    const ChessMagicDefinition* magic(int magicId) const;
    const ChessItemDefinition* item(int itemId) const;

private:
    std::shared_ptr<const ChessGameContentData> data_;
    std::string gameVersion_;
};

std::vector<std::pair<const ChessMagicDefinition*, int>> chessRoleMagicsForStar(
    const ChessGameContent& content,
    const ChessRoleDefinition& role,
    int star);

}
