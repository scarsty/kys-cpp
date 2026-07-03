#pragma once

#include "Chess.h"
#include "ChessBattleEffects.h"

#include <format>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace KysChess
{

struct ComboThreshold
{
    int count;
    std::string name;
    std::vector<ComboEffect> effects;
};

struct ComboDef
{
    int id;
    std::string name;
    std::vector<int> memberRoleIds;
    std::vector<ComboThreshold> thresholds;
    bool isAntiCombo = false;
    bool starSynergyBonus = false;
};

struct ActiveCombo
{
    int id;
    int memberCount = 0;          // effective count (star-augmented when starSynergyBonus)
    int physicalMemberCount = 0;  // raw count of distinct heroes selected
    int activeThresholdIdx = -1;
    bool isAntiCombo = false;
    std::set<int> memberRoleIds;
};

struct ComboProgress
{
    int comboId{};
    int physicalCount{};
    int effectiveCount{};
    int activeThresholdIdx = -1;
    int nextThresholdIdx = -1;
    int displayTargetCount{};
    bool active = false;
    bool isAntiCombo = false;
    bool starSynergyBonus = false;
};

// Returned by computeOwnership — physical owned count and star-augmented effective count.
struct ComboOwnership
{
    int owned = 0;      // distinct heroes from this combo present on the field
    int effective = 0;  // star-augmented value (== owned when starSynergyBonus is false)
};

// Compute how many heroes from `combo` are owned (present in starByRole),
// and the effective (star-augmented) count used for threshold evaluation.
// An entry in starByRole means the hero is on the field; its value is the star level.
inline ComboOwnership computeOwnership(
    const ComboDef& combo, const std::map<int, int>& starByRole)
{
    ComboOwnership result;
    for (int rid : combo.memberRoleIds)
    {
        auto it = starByRole.find(rid);
        if (it == starByRole.end()) continue;
        result.owned++;
        result.effective += combo.starSynergyBonus ? it->second : 1;
    }
    return result;
}

// Low-level combo count formatter used by combo progress display.
// owned        = physical heroes on field
// effective    = star-augmented count (== owned when no starSynergyBonus)
// denominator  = target to compare against (total pool or threshold count)
// Returns e.g. "3+2/7" (star synergy), "2/7" (regular), "獨/1" (anti-combo)
inline std::string formatComboCount(
    int owned, int effective, int denominator,
    bool starSynergyBonus, bool isAntiCombo)
{
    if (isAntiCombo)
        return std::format("獨/{}", denominator);
    int extra = effective - owned;
    if (starSynergyBonus && extra > 0)
        return std::format("{}+{}/{}", owned, extra, denominator);
    return std::format("{}/{}", owned, denominator);
}

namespace detail
{

inline std::map<std::string, std::set<int>> buildComboActAsMap(const std::vector<Chess>& selected)
{
    std::map<std::string, std::set<int>> result;
    for (const auto& chess : selected)
    {
        if (!chess.role) continue;
        for (const auto& comboName : chess.actAsComboNames)
        {
            result[comboName].insert(chess.role->ID);
        }
    }
    return result;
}

inline std::set<int> collectPresentComboMembers(
    const ComboDef& combo,
    const std::map<int, int>& starByRole,
    const std::map<std::string, std::set<int>>& actAsByComboName)
{
    std::set<int> memberRoleIds;
    auto addMember = [&](int roleId)
    {
        if (!starByRole.contains(roleId)) return;
        memberRoleIds.insert(roleId);
    };

    for (int roleId : combo.memberRoleIds)
        addMember(roleId);
    if (auto it = actAsByComboName.find(combo.name); it != actAsByComboName.end())
    {
        for (int roleId : it->second)
            addMember(roleId);
    }
    return memberRoleIds;
}

}    // namespace detail

inline ComboProgress evaluateComboProgress(
    const ComboDef& combo,
    const std::vector<Chess>& selected)
{
    ComboProgress progress;
    progress.comboId = combo.id;
    progress.isAntiCombo = combo.isAntiCombo;
    progress.starSynergyBonus = combo.starSynergyBonus;

    std::map<int, int> starByRole;
    for (const auto& chess : selected)
    {
        if (chess.role) starByRole[chess.role->ID] = chess.star;
    }

    auto memberRoleIds = detail::collectPresentComboMembers(combo, starByRole, detail::buildComboActAsMap(selected));
    progress.physicalCount = static_cast<int>(memberRoleIds.size());
    for (int roleId : memberRoleIds)
    {
        progress.effectiveCount += combo.starSynergyBonus ? starByRole[roleId] : 1;
    }

    if (combo.thresholds.empty())
    {
        return progress;
    }

    if (combo.isAntiCombo)
    {
        progress.physicalCount = progress.physicalCount > 0 ? 1 : 0;
        progress.effectiveCount = progress.physicalCount;
        progress.active = progress.physicalCount > 0;
        progress.activeThresholdIdx = progress.active ? 0 : -1;
        progress.displayTargetCount = combo.thresholds.front().count;
        return progress;
    }

    for (int i = 0; i < static_cast<int>(combo.thresholds.size()); ++i)
    {
        if (progress.effectiveCount >= combo.thresholds[i].count)
        {
            progress.activeThresholdIdx = i;
            progress.active = true;
            continue;
        }
        progress.nextThresholdIdx = i;
        break;
    }

    if (progress.nextThresholdIdx >= 0)
    {
        progress.displayTargetCount = combo.thresholds[progress.nextThresholdIdx].count;
    }
    else
    {
        progress.displayTargetCount = combo.thresholds.back().count;
    }
    return progress;
}

inline std::string formatComboProgressCount(const ComboProgress& progress)
{
    if (progress.isAntiCombo)
    {
        return progress.active ? std::format("獨/{} ✓", progress.displayTargetCount)
                               : std::format("獨/{}", progress.displayTargetCount);
    }

    std::string countText = formatComboCount(
        progress.physicalCount,
        progress.effectiveCount,
        progress.displayTargetCount,
        progress.starSynergyBonus,
        progress.isAntiCombo);
    if (progress.active)
    {
        countText += " ✓";
    }
    return countText;
}

class ChessCombo
{
public:
    static const std::vector<ComboDef>& getAllCombos();
    static std::vector<ActiveCombo> detectCombos(const std::vector<Chess>& selected);
    static std::map<int, RoleComboState> buildComboStates(const std::vector<ActiveCombo>& active);
    static std::vector<ComboEffect> collectGlobalEffects(const std::vector<ActiveCombo>& active);
    static std::vector<int> getCombosForRole(int roleId);
    // Build { roleId -> starLevel } map from a selection of Chess pieces.
    // An entry in the returned map means the hero is on the field.
    static std::map<int, int> buildStarMap(const std::vector<Chess>& selected);
    // Calculate gold bonus from active combos with goldCoefficient
    static int calculateGoldBonus(const std::vector<ActiveCombo>& active, const std::vector<Chess>& survivors);
    static bool hasActiveEffect(const std::vector<ActiveCombo>& active, EffectType effectType);
};

}  // namespace KysChess
