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
    std::optional<Battle::BattleOperationType> combatOperation;
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

struct ChessCharacterGrowth
{
    int fightsWon{};
    int luckStacks{};
    int luckChance{};
    ChessCalculatedStats stats;
    bool showLuck{};
};

struct ChessCharacterPresentation
{
    int roleId{};
    int headId{};
    int cost{};
    int star{};
    bool showCost = true;
    bool equipmentEditable{};
    bool banned{};
    std::string name;
    std::string context;
    std::string statsNote;
    std::optional<ChessCharacterGrowth> growthSummary;
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

inline ChessCharacterEntry chessCharacterSynergyEntry(const ComboDef& combo, const ChessComboMetadata& metadata)
{
    ChessCharacterEntry entry{.id = combo.id, .title = combo.name,
        .value = chessComboCountWithoutStatus(metadata) + (metadata.active ? " ✓" : ""),
        .imageGroup = "chess-synergies", .imageId = combo.iconId, .active = metadata.active};
    if (!metadata.thresholds.empty())
    {
        const auto& threshold = metadata.thresholds[metadata.active ? metadata.activeThresholdIndex : 0];
        entry.details.push_back({.text = std::format("{}點 · {} {}", threshold.requiredCount, threshold.name,
                metadata.active ? "✓" : "未啟動"),
            .spacingAfter = 8, .tone = metadata.active ? PanelTextTone::Effect : PanelTextTone::Inactive});
        auto effects = panelTextRowsForEffectDescription(threshold.effects);
        if (!metadata.active)
            for (auto& row : effects) row.tone = PanelTextTone::Inactive;
        entry.details.insert(entry.details.end(), effects.begin(), effects.end());
    }
    return entry;
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
            .value = std::to_string(ability.powerByStar.front().power),
            .ultimate = &ability == &abilities.back()};
        const auto* magic = content.magic(ability.magicId);
        assert(magic);
        const auto operation = Battle::BattleCombatIntentPlanner().operationTypeForAttackArea(magic->AttackAreaType);
        entry.combatOperation = operation;
        switch (operation)
        {
        case Battle::BattleOperationType::Melee:
            entry.tooltip = entry.ultimate ? "輕擊：強化攻擊，追加近身濺射。"
                : "輕擊：每第三下近戰強化追擊。";
            break;
        case Battle::BattleOperationType::TrackingProjectile:
            entry.tooltip = entry.ultimate ? "重擊：發射兩枚追蹤彈。"
                : "重擊：發射一枚追蹤彈。";
            break;
        case Battle::BattleOperationType::RangedProjectile:
            entry.tooltip = entry.ultimate ? "遠程：穿透，附三枚35%側彈。"
                : "遠程：穿透，附兩枚20%側彈。";
            break;
        default: assert(false); break;
        }
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
        if (entry.id < 0) { entry.title = entry.value; entry.value.clear(); continue; }
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
    std::array<int, 2> equipmentItemIds{-1, -1};
    if (piece)
        for (int slot = 0; slot < 2; ++slot)
            if (const auto* equipment = equipmentInstance(state, slot == 0 ? piece->weaponInstanceId : piece->armorInstanceId))
                equipmentItemIds[slot] = equipment->itemId;
    auto result = buildChessCharacterContent(content, roleId, star, equipmentItemIds);
    result.equipmentEditable = piece && context == ChessCharacterContext::Owned;
    result.banned = state.bannedRoleIds.contains(roleId);
    result.context = piece ? (piece->star != star ? "升星預覽" : piece->deployed ? "出戰中" : "候補")
        : context == ChessCharacterContext::Shop ? "商店預覽" : "圖鑑預覽";
    if (piece)
    {
        const auto breakdown = chessPieceStatBreakdown(content, state, preview);
        result.growthSummary = {piece->fightsWon, piece->luckStacks,
            content.balance().talent(state.talent).luckChance(piece->luckStacks), breakdown.fightWinGrowth,
            state.talent == ChessTalentId::Gambler};
        result.stats = breakdown.total;
    }
    else result.stats = chessRoleStats(*role, content.balance(), star, 0);

    for (const auto& combo : content.combos())
    {
        if (combo.isNpcOnly) continue;
        const auto progress = evaluateChessComboProgress(state, content, combo);
        const bool contributes = piece && std::ranges::any_of(progress.contributions, [&](const auto& contribution) {
            return std::ranges::contains(contribution.unitIds, piece->instanceId);
        });
        if (!contributes && !std::ranges::contains(combo.memberRoleIds, roleId)) continue;
        const auto metadata = chessComboMetadata(content, combo, progress.physicalCount, progress.effectiveCount,
            progress.activeThresholdIndex, progress.nextThresholdIndex, progress.contributions,
            EffectDescriptionStyle::Compact, progress.requiredCounts);
        result.synergies.push_back(chessCharacterSynergyEntry(combo, metadata));
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
    result.statsNote = "開戰屬性，含星級、裝備與陣容加成";
    result.stats = *unit.initializedCombatStats;
    for (const auto& synergy : synergies)
    {
        const auto combo = std::ranges::find(content.combos(), synergy.comboId, &ComboDef::id);
        assert(combo != content.combos().end());
        const bool contributes = std::ranges::any_of(synergy.contributions, [&](const auto& contribution) {
            return std::ranges::contains(contribution.unitIds, unit.unitId);
        });
        if (!contributes && !std::ranges::contains(combo->memberRoleIds, unit.roleId)) continue;
        result.synergies.push_back(chessCharacterSynergyEntry(*combo, synergy));
    }
    return result;
}
} // namespace KysChess
