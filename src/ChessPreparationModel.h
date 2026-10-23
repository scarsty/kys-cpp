#pragma once

#include "ChessCombo.h"
#include "ChessManagementRules.h"
#include "ChessPointerGestures.h"
#include "ChessScreenLayout.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <string>

namespace KysChess
{

inline std::string chessLateBloomerSelectionDescription(const ChessGameContent& content, const ChessSessionPiece& piece)
{
    const auto& talent = content.balance().talent(ChessTalentId::LateBloomer);
    const auto* role = content.role(piece.roleId);
    assert(role);
    std::string text = std::format(
        "選定這枚{}作為本局唯一培養對象，選定後不可更換。\n"
        "目前{}星、{}勝；達到三星與{}勝後自動突破四星，沿用一般升星加成。\n"
        "勝場包含已有勝場、出戰的主線勝利及遠征首次通關；合成取最高勝場並保留培養資格。\n"
        "第二枚同名棋子不會獲得資格；出售不退還選擇機會。",
        role->Name, piece.star, piece.fightsWon, talent.evolutionWins);
    const int shield = talent.evolutionStatusShields.at(role->Cost - 1);
    if (shield > 0)
        text += std::format("\n突破後每次開戰獲得{}幀狀態護盾，可抵銷負面狀態的持續時間。", shield);
    return text;
}

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

// 第一次點選鎖定操作，第二次才執行。觸控花費與出售共用。
// 不設時間限制，讓玩家能讀完資訊；切換操作、取消或開啟模態時解除。
struct ChessPreparationConfirmation
{
    std::string armedKey;

    bool confirm(const std::string& key)
    {
        if (armedKey == key) { armedKey.clear(); return true; }
        armedKey = key;
        return false;
    }
    void reset() { armedKey.clear(); }
    void cancelUnless(const std::string& key) { if (armedKey != key) reset(); }
};

enum class ChessPreparationView { Piece, Equipment, Information, Neigongs, Neigong };

// 整備的顯示順序保留原有位置，新上／下陣的棋子排在目的地末尾。
// 戰場站位仍由 formationSlots 管理，不因整備動畫而改動。
struct ChessPreparationRosterOrder
{
    std::vector<int> deployed;
    std::vector<int> bench;

    void reconcile(const ChessSessionState& state)
    {
        const auto keep = [&](std::vector<int>& ids, bool isDeployed) {
            std::erase_if(ids, [&](int id) {
                const auto found = state.roster.find(id);
                return found == state.roster.end() || found->second.deployed != isDeployed;
            });
        };
        keep(deployed, true);
        keep(bench, false);
        for (int id : state.formationSlots)
            if (id >= 0 && !std::ranges::contains(deployed, id)) deployed.push_back(id);
        for (const auto& [id, piece] : state.roster)
            if (!piece.deployed && !std::ranges::contains(bench, id)) bench.push_back(id);
    }
};

struct ChessPreparationCardMotion
{
    PanelFrame from{};
    PanelFrame to{};
    double startedAt{};
    static constexpr double duration = 220;

    bool active(double now) const { return now < startedAt + duration; }
    PanelFrame frame(double now) const
    {
        const double t = std::clamp((now - startedAt) / duration, 0.0, 1.0);
        const double eased = 1 - (1 - t) * (1 - t) * (1 - t);
        const auto interpolate = [eased](int start, int end) {
            return static_cast<int>(start + (end - start) * eased + 0.5);
        };
        return {interpolate(from.x, to.x), interpolate(from.y, to.y),
            interpolate(from.w, to.w), interpolate(from.h, to.h)};
    }
};

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

struct ChessPreparationNavigationContext
{
    ChessPreparationView view{};
    int listPage{};
    int textPage{};
    std::string focusKey;
    ChessPreparationSelection selection;
    int equipmentId = -1;
    int equipmentType{};
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

inline ChessAction chessPreparationDeployment(const ChessSessionState& state, int instanceId)
{
    ChessAction action;
    action.type = ChessActionType::SetDeployment;
    for (const auto& [id, piece] : state.roster)
        if (piece.deployed && id != instanceId) action.chessInstanceIds.push_back(id);
    if (!state.roster.at(instanceId).deployed) action.chessInstanceIds.push_back(instanceId);
    return action;
}

} // namespace KysChess
