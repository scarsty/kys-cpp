#pragma once

#include "ChessSessionTypes.h"
#include "Point.h"

#include <cassert>
#include <cstdint>
#include <optional>
#include <utility>

namespace KysChess
{

enum class ChessGuiFlowResult : std::uint8_t
{
    Continue,
    Aborted,
};

inline ChessGuiFlowResult chessGuiFlowResult(bool runOwnerExitRequested)
{
    return runOwnerExitRequested
        ? ChessGuiFlowResult::Aborted
        : ChessGuiFlowResult::Continue;
}

struct ChessGuiInitialBattleCameraCenter
{
    Point grid;
    Pointf world;
};

template <typename Units, typename GridToWorld>
ChessGuiInitialBattleCameraCenter chessGuiInitialBattleCameraCenter(
    const Units& units,
    GridToWorld&& gridToWorld)
{
    Point total;
    int allyCount{};
    for (const auto& unit : units)
    {
        if (unit.team == 0)
        {
            total += unit.grid;
            ++allyCount;
        }
    }
    assert(allyCount > 0);
    const Point grid{total.x / allyCount, total.y / allyCount};
    return {
        grid,
        std::forward<GridToWorld>(gridToWorld)(grid.x, grid.y),
    };
}

// 載入期間不開放操作；最後一批完成後還需呈現一幀，避免累積輸入穿透。
class ChessGuiPreBattleFlow
{
public:
    void markFramePresented()
    {
        if (phase_ == Phase::AwaitingFirstFrame) phase_ = Phase::Loading;
        else if (phase_ == Phase::AwaitingReadyFrame) phase_ = Phase::Ready;
    }

    template <class LoadBatch>
    bool runLoadBatch(LoadBatch&& batch)
    {
        if (phase_ != Phase::Loading) return false;
        if (std::forward<LoadBatch>(batch)()) phase_ = Phase::AwaitingReadyFrame;
        return true;
    }

    bool assetsLoaded() const { return phase_ == Phase::AwaitingReadyFrame || phase_ == Phase::Ready; }
    bool interactive() const { return phase_ == Phase::Ready; }

private:
    enum class Phase { AwaitingFirstFrame, Loading, AwaitingReadyFrame, Ready };
    Phase phase_ = Phase::AwaitingFirstFrame;
};

inline bool captureChessGuiBattleCompletion(
    std::optional<ChessActionResult>& destination,
    std::optional<ChessActionResult>& source,
    Battle::BattleOutcome outcome,
    int& presentationResult)
{
    if (!source)
    {
        return false;
    }

    assert(!destination);
    assert(outcome != Battle::BattleOutcome::InProgress);
    const int canonicalResult = outcome == Battle::BattleOutcome::PlayerVictory ? 0 : 1;
    assert(presentationResult == -1 || presentationResult == canonicalResult);

    destination.emplace(std::move(*source));
    source.reset();
    presentationResult = canonicalResult;
    return true;
}

inline bool chessGuiBattleReadyForPostBattle(
    bool completedSceneRun,
    const std::optional<ChessActionResult>& completedAction)
{
    if (!completedSceneRun)
    {
        return false;
    }
    assert(completedAction);
    return completedAction.has_value();
}

}
