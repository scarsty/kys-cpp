#pragma once

#include "ChessGameSession.h"
#include "ChessPreparedBattleAnalysis.h"
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

inline std::string chessComboCountWithoutStatus(const ChessComboMetadata& synergy)
{
    return synergy.progressCount.substr(0, synergy.progressCount.find(" ✓"));
}

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
    bool showCost = true;
    bool equipmentEditable{};
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

inline ChessCharacterPresentation buildChessCharacterContent(const ChessGameContent& content,
    int roleId, int star, const std::array<int, 2>& equipmentItemIds)
{
    const auto* role = content.role(roleId);
    assert(role);
    ChessCharacterPresentation result{
        .roleId = roleId, .headId = role->HeadID, .cost = role->Cost, .star = star, .name = role->Name,
    };
    const auto abilities = chessAbilitiesForRoleStar(content, *role, star, EffectDescriptionStyle::Compact);
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
            entry.tooltip = entry.ultimate ? "輕擊：本次必定強化，並追加一記延遲的近身濺射。"
                : "輕擊：連續普通近戰打到第三下，會變成強化追擊。";
            break;
        case Battle::BattleOperationType::TrackingProjectile:
            entry.combatStyle = "重擊";
            entry.tooltip = entry.ultimate ? "重擊：基礎發射兩枚追蹤彈，可追擊目標或附近的備選敵人。"
                : "重擊：基礎發射一枚追蹤彈，可追擊目標或附近的備選敵人。";
            break;
        case Battle::BattleOperationType::RangedProjectile:
            entry.combatStyle = "遠程";
            entry.tooltip = entry.ultimate ? "遠程：直線穿透，另帶三枚35%威力的扇形側彈。"
                : "遠程：直線穿透，另帶兩枚20%威力的扇形側彈。";
            break;
        default: assert(false); break;
        }
        if (operation == Battle::BattleOperationType::RangedProjectile
            || operation == Battle::BattleOperationType::TrackingProjectile)
            entry.tooltip += "投射物飛行越久威力略減，側背命中有傷害加成；可被彈反，也可能與敵方投射物互相抵消或削弱。";
        if (entry.ultimate)
        {
            const auto effects = panelTextRowsForEffectDescription(ability.effects);
            entry.details.insert(entry.details.end(), effects.begin(), effects.end());
        }
        entry.details.push_back({.text = entry.tooltip, .spacingBefore = entry.details.empty() ? 0 : 12,
            .tone = PanelTextTone::Context});
        result.abilities.push_back(std::move(entry));
    }
    for (int slot = 0; slot < 2; ++slot)
    {
        auto& entry = result.equipment[slot];
        entry.value = slot == 0 ? "武器" : "防具";
        entry.id = equipmentItemIds[slot];
        if (entry.id < 0) { entry.title = "未裝備"; continue; }
        const auto* item = content.item(entry.id);
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
    return result;
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
    std::array<int, 2> equipmentItemIds{-1, -1};
    if (piece)
        for (int slot = 0; slot < 2; ++slot)
            if (const auto* equipment = equipmentInstance(state, slot == 0 ? piece->weaponInstanceId : piece->armorInstanceId))
                equipmentItemIds[slot] = equipment->itemId;
    auto result = buildChessCharacterContent(content, roleId, star, equipmentItemIds);
    result.equipmentEditable = piece && context == ChessCharacterContext::Owned;
    result.context = piece ? (piece->star != star ? "升星預覽" : piece->deployed ? "出戰中" : "候補")
        : context == ChessCharacterContext::Shop ? "商店預覽" : "圖鑑預覽";
    result.growth = piece ? std::format("勝場 {} · 含裝備基礎加成", piece->fightsWon) : "無勝場成長 · 未裝備";
    result.stats = piece ? chessPieceStats(content, preview, growthPercent)
        : chessRoleStats(*role, content.balance(), star, 0, growthPercent);
    if (piece && piece->luckStacks > 0)
        result.growth += std::format(" · 賭運 {} 層 / {}%", piece->luckStacks,
            content.balance().talent(state.talent).luckChance(piece->luckStacks));
    std::map<int, int> owned;
    for (const auto& [id, member] : state.roster)
        if (member.roleId == roleId) ++owned[member.star];
    for (const auto& [ownedStar, count] : owned)
        result.owned += std::format("{}星 ×{}  ", ownedStar, count);
    if (result.owned.empty()) result.owned = "尚未擁有";

    for (const auto& combo : content.combos())
    {
        if (combo.isNpcOnly || !std::ranges::contains(combo.memberRoleIds, roleId)) continue;
        const auto progress = evaluateChessComboProgress(state, content, combo);
        const auto count = formatChessComboProgressCount(progress);
        ChessCharacterEntry entry{.id = combo.id, .title = combo.name, .value = count,
            .imageGroup = "chess-synergies", .imageId = combo.iconId, .active = progress.active};
        result.synergies.push_back(std::move(entry));
    }
    return result;
}

// 戰鬥角色沿用整備／圖鑑的武學與裝備資料，數值與羈絆取自所屬陣容。
inline ChessCharacterPresentation buildChessBattleCharacterPresentation(const ChessGameContent& content,
    const ChessPreparedBattleUnitAnalysis& unit, std::span<const ChessComboMetadata> synergies)
{
    assert(unit.initializedCombatStats);
    auto result = buildChessCharacterContent(content, unit.roleId, unit.star, {unit.weaponItemId, unit.armorItemId});
    result.showCost = false;
    result.context = unit.team == 0 ? "我方陣容" : "敵方陣容";
    result.growth = "含星級、裝備及陣容羈絆的開戰數值；戰中觸發效果另計。";
    result.stats = *unit.initializedCombatStats;
    for (const auto& synergy : synergies)
    {
        const auto combo = std::ranges::find(content.combos(), synergy.comboId, &ComboDef::id);
        assert(combo != content.combos().end());
        const bool contributes = std::ranges::any_of(synergy.contributions, [&](const auto& contribution) {
            return std::ranges::contains(contribution.unitIds, unit.unitId);
        });
        if (!contributes && !std::ranges::contains(combo->memberRoleIds, unit.roleId)) continue;
        result.synergies.push_back({.id = combo->id, .title = combo->name, .value = chessComboCountWithoutStatus(synergy),
            .imageGroup = "chess-synergies", .imageId = combo->iconId, .active = synergy.active});
    }
    return result;
}
} // namespace KysChess
