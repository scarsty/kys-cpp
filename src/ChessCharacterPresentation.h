#pragma once

#include "ChessGameSession.h"
#include "ChessUiCommon.h"
#include "ChessPresentationHelpers.h"
#include "ChessMenuFormatting.h"
#include "battle/BattleCombatIntent.h"

#include <array>
#include <format>

namespace KysChess
{
enum class ChessCharacterContext { Catalogue, Shop, Owned };

struct ChessCharacterEntry
{
    int id = -1;
    std::string title;
    std::string value;
    std::string imageGroup;
    int imageId = -1;
    bool ultimate{};
    bool active{};
    std::string combatStyle;
    std::string tooltip;
    std::vector<PanelTextSourceRow> details;
};

struct ChessCharacterInspectorState
{
    bool expanded{};
    int section{};
    int page{};
    int entry = -1;

    void showSection(int value, int selectedEntry = -1)
    {
        expanded = true;
        section = value;
        page = 0;
        entry = selectedEntry;
    }
};

struct ChessCharacterPresentation
{
    int roleId{};
    int headId{};
    int cost{};
    int star{};
    std::string name;
    std::string context;
    std::string growth;
    std::string owned;
    ChessCalculatedStats stats;
    std::vector<ChessCharacterEntry> abilities;
    std::array<ChessCharacterEntry, 2> equipment;
    std::vector<ChessCharacterEntry> synergies;
};

inline std::array<std::pair<std::string, int>, 10> chessCharacterStats(const ChessCalculatedStats& stats)
{
    return {{{"生命", stats.maxHp}, {"攻擊", stats.attack}, {"防禦", stats.defence},
        {"輕功", stats.speed}, {"拳掌", stats.fist}, {"御劍", stats.sword},
        {"耍刀", stats.knife}, {"奇門", stats.unusual}, {"內力", stats.maxMp}, {"暗器", stats.hiddenWeapon}}};
}

inline ChessCharacterPresentation buildChessCharacterPresentation(const ChessGameContent& content,
    const ChessSessionState& state, int roleId, int star = 1, int instanceId = -1,
    ChessCharacterContext context = ChessCharacterContext::Catalogue)
{
    const auto* role = content.role(roleId);
    assert(role);
    const auto* piece = rosterPiece(state, instanceId);
    assert(instanceId < 0 || (piece && piece->roleId == roleId));
    auto preview = piece ? *piece : ChessSessionPiece{};
    preview.star = star;
    const int growthPercent = content.balance().talent(state.talent).amplifiedGrowthPercent;
    ChessCharacterPresentation result{
        .roleId = roleId, .headId = role->HeadID, .cost = role->Cost, .star = star, .name = role->Name,
        .context = piece ? (piece->star != star ? "升星預覽" : piece->deployed ? "出戰中" : "候補")
            : context == ChessCharacterContext::Shop ? "商店預覽" : "圖鑑預覽",
        .growth = piece ? std::format("勝場 {} · 含裝備基礎加成", piece->fightsWon) : "無勝場成長 · 未裝備",
        .stats = piece ? chessPieceStats(content, preview, growthPercent)
            : chessRoleStats(*role, content.balance(), star, 0, growthPercent),
    };
    if (piece && piece->luckStacks > 0)
        result.growth += std::format(" · 賭運 {} 層 / {}%", piece->luckStacks,
            content.balance().talent(state.talent).luckChance(piece->luckStacks));
    std::map<int, int> owned;
    for (const auto& [id, member] : state.roster)
        if (member.roleId == roleId) ++owned[member.star];
    for (const auto& [ownedStar, count] : owned)
        result.owned += std::format("{}星 ×{}  ", ownedStar, count);
    if (result.owned.empty()) result.owned = "尚未擁有";

    const auto abilities = chessAbilitiesForRoleStar(content, *role, star, EffectDescriptionStyle::Full);
    for (const auto& ability : abilities)
    {
        ChessCharacterEntry entry{.id = ability.magicId, .title = ability.name,
            .value = std::format("威力 {}", ability.powerByStar.front().power),
            .ultimate = &ability == &abilities.back()};
        const auto* magic = content.magic(ability.magicId);
        assert(magic);
        const auto operation = Battle::BattleCombatIntentPlanner().operationTypeForAttackArea(magic->AttackAreaType);
        switch (operation)
        {
        case Battle::BattleOperationType::Melee:
            entry.combatStyle = "輕擊";
            entry.tooltip = "輕擊：接近目標後近身攻擊。絕招可連帶命中附近敵人。";
            break;
        case Battle::BattleOperationType::TrackingProjectile:
            entry.combatStyle = "重擊";
            entry.tooltip = "重擊：在重擊距離內施放追蹤攻擊。";
            break;
        case Battle::BattleOperationType::RangedProjectile:
            entry.combatStyle = "遠程";
            entry.tooltip = "遠程：向目標發射飛行攻擊，依射程保持交戰距離。";
            break;
        default: assert(false); break;
        }
        entry.tooltip += "羈絆、裝備與內功可改變實際攻擊方式。";
        entry.details.push_back({.text = entry.tooltip, .tone = PanelTextTone::Context});
        if (entry.ultimate)
        {
            const auto effects = panelTextRowsForEffectDescription(ability.effects);
            entry.details.insert(entry.details.end(), effects.begin(), effects.end());
        }
        result.abilities.push_back(std::move(entry));
    }
    for (int slot = 0; slot < 2; ++slot)
    {
        auto& entry = result.equipment[slot];
        entry.value = slot == 0 ? "武器" : "防具";
        const int equipmentId = piece ? (slot == 0 ? piece->weaponInstanceId : piece->armorInstanceId) : -1;
        entry.id = equipmentId;
        const auto* instance = equipmentInstance(state, equipmentId);
        if (!instance) { entry.title = "未裝備"; continue; }
        const auto* item = content.item(instance->itemId);
        assert(item);
        entry.title = item->name;
        entry.imageGroup = "item";
        entry.imageId = item->id;
        const auto metadata = chessEquipmentMetadata(content, item->id);
        const ChessCalculatedStats additions{.maxHp = item->addMaxHP, .attack = item->addAttack,
            .defence = item->addDefence, .speed = item->addSpeed, .fist = item->addFist,
            .sword = item->addSword, .knife = item->addKnife, .unusual = item->addUnusual,
            .hiddenWeapon = item->addHiddenWeapon};
        for (const auto& [name, value] : chessCharacterStats(additions))
            if (value != 0) entry.details.push_back({.text = std::format("{} {:+}", name, value)});
        const auto effects = panelTextRowsForEquipment(metadata, roleId);
        entry.details.insert(entry.details.end(), effects.begin(), effects.end());
    }
    for (const auto& combo : content.combos())
    {
        if (combo.isNpcOnly || !std::ranges::contains(combo.memberRoleIds, roleId)) continue;
        const auto progress = evaluateChessComboProgress(state, content, combo);
        const auto count = formatChessComboProgressCount(progress);
        ChessCharacterEntry entry{.id = combo.id, .title = combo.name, .value = count,
            .imageGroup = "chess-synergies", .imageId = combo.iconId, .active = progress.active};
        entry.tooltip = combo.name + " · " + count;
        result.synergies.push_back(std::move(entry));
    }
    return result;
}
} // namespace KysChess
