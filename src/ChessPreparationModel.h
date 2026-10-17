#pragma once

#include "ChessCombo.h"
#include "ChessManagementRules.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <string>
#include <span>

namespace KysChess
{

struct ChessPreparationCombo
{
    const ComboDef* definition{};
    ChessComboProgress progress;
};

// 摘要與完整清單共用排序；未啟動的高計數羈絆不擠走已啟動項目。
inline std::vector<ChessPreparationCombo> chessPreparationCombos(
    const ChessSessionState& state, const ChessGameContent& content)
{
    auto progresses = evaluateChessComboProgresses(state, content);
    std::vector<ChessPreparationCombo> result;
    for (std::size_t i = 0; i < progresses.size(); ++i)
        if (!content.combos()[i].isNpcOnly)
            result.push_back({&content.combos()[i], std::move(progresses[i])});
    std::stable_sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.progress.active != right.progress.active) return left.progress.active;
        return left.progress.effectiveCount > right.progress.effectiveCount;
    });
    return result;
}

// 整備只保存穩定識別碼；出售、合成或刷新後，不把舊選取轉給同一格的新棋子。
struct ChessPreparationSelection
{
    int instanceId = -1;
    int shopSlot = -1;
    int shopRoleId = -1;

    void selectPiece(int id) { *this = {id, -1, -1}; }
    void selectShop(int slot, int roleId) { *this = {-1, slot, roleId}; }
    void reconcile(const ChessSessionState& state)
    {
        if (instanceId >= 0 && !state.roster.contains(instanceId)) *this = {};
        if (shopSlot >= 0 && (shopSlot >= static_cast<int>(state.shop.size())
            || state.shop[shopSlot].roleId != shopRoleId)) *this = {};
    }
};

// 與舊行動版選單相同：第一次點選提供預覽，第二次才執行花費。
// 不設時間限制，讓玩家能讀完資訊；切換操作、取消或開啟模態時解除。
struct ChessPreparationTouchConfirmation
{
    std::string armedKey;

    bool confirm(const std::string& key)
    {
        if (armedKey == key) { armedKey.clear(); return true; }
        armedKey = key;
        return false;
    }
    void reset() { armedKey.clear(); }
};

// SDL 的滑鼠雙擊計數搭配穩定控制項身分；觸控沿用相同的 500ms 點擊間隔。
// 成功派送後吞掉同一串連點，避免移位後誤操作鄰格或把剛上陣的棋子撤回。
struct ChessPreparationPortraitClicks
{
    std::string key;
    uint64_t lastTimestamp{};
    bool consumed{};

    bool release(const std::string& target, uint8_t clicks, uint64_t timestamp, bool touch)
    {
        const bool close = timestamp >= lastTimestamp && timestamp - lastTimestamp <= 500000000;
        if (consumed && close) { lastTimestamp = timestamp; return false; }
        const bool doubleClick = key == target && close && (touch || clicks == 2);
        key = target;
        lastTimestamp = timestamp;
        consumed = doubleClick;
        return doubleClick;
    }
    void reset() { *this = {}; }
};

enum class ChessPreparationView { Piece, Equipment, Information, Replacement, Sell, Attributes, Ability, Neigongs, Neigong };

// 滑過只預覽；釘選後移動游標不會換物品，捲動亦不影響裝備選取。
struct ChessPreparationEquipmentPopover
{
    int equipmentId = -1;
    bool pinned{};
    int scroll{};

    void preview(int id)
    {
        if (!pinned && equipmentId != id) { equipmentId = id; scroll = 0; }
    }
    void pin(int id) { equipmentId = id; pinned = true; scroll = 0; }
    void dismiss() { *this = {}; }
    void scrollBy(int delta, int maximum) { scroll = std::clamp(scroll + delta, 0, maximum); }
};

inline void reconcileChessPreparationView(ChessPreparationSelection& selection,
    ChessPreparationView& view, const ChessSessionState& state)
{
    selection.reconcile(state);
    if (selection.instanceId < 0 && selection.shopRoleId < 0
        && (view == ChessPreparationView::Attributes || view == ChessPreparationView::Ability
            || view == ChessPreparationView::Replacement || view == ChessPreparationView::Sell))
        view = ChessPreparationView::Piece;
}

inline std::vector<int> chessPreparationVisibleFormation(
    std::span<const int> formation, int capacity)
{
    assert(formation.size() == kChessFormationSlotCount);
    std::vector<int> slots;
    for (int id : formation) if (id >= 0) slots.push_back(id);
    assert(static_cast<int>(slots.size()) <= capacity);
    slots.resize(capacity, -1);
    return slots;
}

struct ChessPreparationNavigationContext
{
    ChessPreparationView view{};
    int listPage{};
    int textPage{};
    std::string focusKey;
    ChessPreparationSelection selection;
    int equipmentId = -1;
    int equipmentType{};
    int abilityId = -1;
    int neigongId = -1;
    std::string information;
};

class ChessPreparationHistory
{
public:
    void push(ChessPreparationNavigationContext context) { entries_.push_back(std::move(context)); }
    void clear() { entries_.clear(); }
    std::optional<ChessPreparationNavigationContext> pop()
    {
        if (entries_.empty()) return std::nullopt;
        auto context = std::move(entries_.back());
        entries_.pop_back();
        return context;
    }
private:
    std::vector<ChessPreparationNavigationContext> entries_;
};

inline std::optional<int> chessPreparationPurchaseDestination(
    const ChessSessionState& state, const ChessActionResult& result)
{
    for (auto event = result.events.rbegin(); event != result.events.rend(); ++event)
    {
        if ((event->type == ChessSemanticEventType::ChessPurchased
                || event->type == ChessSemanticEventType::ChessMerged)
            && state.roster.contains(event->primaryId))
            return event->primaryId;
    }
    return std::nullopt;
}

inline ChessAction chessPreparationDeployment(
    const ChessSessionState& state, int incomingId, int outgoingId = -1)
{
    ChessAction action;
    action.type = ChessActionType::SetDeployment;
    for (const auto& [id, piece] : state.roster)
        if (piece.deployed && id != outgoingId) action.chessInstanceIds.push_back(id);
    if (incomingId >= 0)
    {
        assert(state.roster.contains(incomingId) && !state.roster.at(incomingId).deployed);
        action.chessInstanceIds.push_back(incomingId);
    }
    return action;
}

struct ChessPreparationComboChange
{
    std::string name;
    ChessComboProgress before;
    ChessComboProgress after;
};

inline std::vector<ChessPreparationComboChange> chessPreparationReplacementPreview(
    const ChessSessionState& state, const ChessGameContent& content, int incomingId, int outgoingId)
{
    assert(state.roster.contains(incomingId) && !state.roster.at(incomingId).deployed);
    assert(state.roster.contains(outgoingId) && state.roster.at(outgoingId).deployed);
    auto projected = state;
    projected.roster.at(incomingId).deployed = true;
    projected.roster.at(outgoingId).deployed = false;
    std::vector<ChessPreparationComboChange> changes;
    for (const auto& combo : content.combos())
    {
        auto before = evaluateChessComboProgress(state, content, combo);
        auto after = evaluateChessComboProgress(projected, content, combo);
        if (before.effectiveCount != after.effectiveCount
            || before.activeThresholdIndex != after.activeThresholdIndex
            || before.requiredCounts != after.requiredCounts)
            changes.push_back({combo.name, std::move(before), std::move(after)});
    }
    return changes;
}

} // namespace KysChess
