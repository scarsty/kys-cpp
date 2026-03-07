#include "ChessEquipment.h"
#include "ChessBattleEffects.h"
#include "GameUtil.h"
#include "Save.h"
#include "yaml-cpp/yaml.h"
#include <format>

namespace KysChess
{

static std::vector<EquipmentDef> pool_;
static bool loaded_ = false;

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

    if (!config["装备列表"]) return;

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
                if (ChessBattleEffects::parseEffect(eNode, eff, std::format("装备{}", def.itemId)))
                    def.effects.push_back(eff);
            }
        }

        pool_.push_back(def);
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

}    // namespace KysChess
