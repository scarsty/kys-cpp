#include "ChessEquipment.h"
#include "ChessBattleEffects.h"
#include "Chess.h"
#include "Font.h"
#include "GameUtil.h"
#include "Save.h"
#include "yaml-cpp/yaml.h"
#include <algorithm>
#include <format>

namespace KysChess
{

static std::vector<EquipmentDef> pool_;
static std::vector<EquipmentSynergyDef> synergies_;
static bool loaded_ = false;

namespace
{

void appendSynergyDef(const YAML::Node& entry, int equipmentId)
{
    EquipmentSynergyDef def;
    def.equipmentId = equipmentId;

    auto roleIdsNode = entry["角色ID"];
    if (!roleIdsNode)
    {
        return;
    }
    if (roleIdsNode.IsSequence())
    {
        for (const auto& roleNode : roleIdsNode)
        {
            def.roleIds.push_back(roleNode.as<int>());
        }
    }
    else
    {
        def.roleIds.push_back(roleIdsNode.as<int>());
    }
    if (def.roleIds.empty())
    {
        return;
    }

    if (entry["效果"] && entry["效果"].IsSequence())
    {
        int effectIndex = 0;
        for (const auto& eNode : entry["效果"])
        {
            ++effectIndex;
            ComboEffect eff;
            auto effectContext = std::format("裝備羈絆裝備{}效果#{}", def.equipmentId, effectIndex);
            if (!ChessBattleEffects::parseEffect(eNode, eff, effectContext))
            {
                continue;
            }
            if (eff.type == EffectType::ActAsCombo)
            {
                def.actAsComboNames.push_back(Font::getInstance()->S2T(eff.text));
                continue;
            }
            def.effects.push_back(eff);
        }
    }

    synergies_.push_back(def);
}

}    // namespace

const Item* EquipmentDef::getItem() const
{
    return Save::getInstance()->getItem(itemId);
}

void ChessEquipment::loadConfig()
{
    if (loaded_) return;
    loaded_ = true;

    YAML::Node config;
    try { config = YAML::LoadFile(GameUtil::PATH() + "config/chess_equipment.yaml"); }
    catch (...) { return; }

    if (config["装备列表"])
    {
        for (const auto& entry : config["装备列表"])
        {
            EquipmentDef def;
            def.itemId = entry["装备ID"].as<int>();
            def.tier = entry["层级"].as<int>();
            def.equipType = entry["装备类型"].as<int>();

            if (entry["效果"] && entry["效果"].IsSequence())
            {
                for (const auto& eNode : entry["效果"])
                {
                    ComboEffect eff;
                    auto effectContext = std::format("裝備{}效果#{}", def.itemId, def.effects.size() + 1);
                    if (ChessBattleEffects::parseEffect(eNode, eff, effectContext))
                    {
                        if (eff.type == EffectType::ActAsCombo)
                        {
                            def.actAsComboNames.push_back(Font::getInstance()->S2T(eff.text));
                        }
                        def.effects.push_back(eff);
                    }
                }
            }

            if (entry["装备羁绊"] && entry["装备羁绊"].IsSequence())
            {
                for (const auto& synergyNode : entry["装备羁绊"])
                {
                    appendSynergyDef(synergyNode, def.itemId);
                }
            }

            pool_.push_back(def);
        }
    }
}

const std::vector<EquipmentDef>& ChessEquipment::getAll()
{
    loadConfig();
    return pool_;
}

const EquipmentDef* ChessEquipment::getById(int itemId)
{
    loadConfig();
    for (auto& eq : pool_)
        if (eq.itemId == itemId) return &eq;
    return nullptr;
}

std::vector<const EquipmentDef*> ChessEquipment::getByTier(int tier)
{
    loadConfig();
    std::vector<const EquipmentDef*> result;
    for (auto& eq : pool_)
        if (eq.tier == tier) result.push_back(&eq);
    return result;
}

const std::vector<EquipmentSynergyDef>& ChessEquipment::getAllSynergies()
{
    loadConfig();
    return synergies_;
}

std::vector<const EquipmentSynergyDef*> ChessEquipment::getSynergiesFor(int roleId, int weaponId, int armorId)
{
    loadConfig();
    std::vector<const EquipmentSynergyDef*> result;
    for (auto& synergy : synergies_)
    {
        if (std::find(synergy.roleIds.begin(), synergy.roleIds.end(), roleId) == synergy.roleIds.end())
        {
            continue;
        }
        if (synergy.equipmentId == weaponId || synergy.equipmentId == armorId)
        {
            result.push_back(&synergy);
        }
    }
    return result;
}

void ChessEquipment::applyActiveSynergies(Chess& chess)
{
    chess.actAsComboNames.clear();

    auto appendEquipmentActAs = [&](int itemId)
    {
        auto* equipment = getById(itemId);
        if (!equipment)
        {
            return;
        }
        chess.actAsComboNames.insert(
            chess.actAsComboNames.end(),
            equipment->actAsComboNames.begin(),
            equipment->actAsComboNames.end());
    };

    appendEquipmentActAs(chess.weaponInstance.itemId);
    appendEquipmentActAs(chess.armorInstance.itemId);

    if (!chess.role)
    {
        return;
    }

    auto synergies = getSynergiesFor(chess.role->ID, chess.weaponInstance.itemId, chess.armorInstance.itemId);
    for (auto* synergy : synergies)
    {
        chess.actAsComboNames.insert(
            chess.actAsComboNames.end(),
            synergy->actAsComboNames.begin(),
            synergy->actAsComboNames.end());
    }
}

std::vector<Chess> ChessEquipment::withActiveSynergies(std::vector<Chess> chesses)
{
    for (auto& chess : chesses)
    {
        applyActiveSynergies(chess);
    }
    return chesses;
}

}    // namespace KysChess
