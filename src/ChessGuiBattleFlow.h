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

enum class ChessGuiPreBattleDismissInput : std::uint8_t
{
    None,
    KeyboardRelease,
    GamepadRelease,
    PointerRelease,
};

class ChessGuiPreBattleFlow
{
public:
    void markFirstFramePresented()
    {
        if (phase_ == Phase::AwaitingFirstFrame)
        {
            phase_ = Phase::PreloadPending;
        }
    }

    template <class Preload>
    bool runPreloadAfterFirstFrame(Preload&& preload)
    {
        if (!beginPreload())
        {
            return false;
        }
        std::forward<Preload>(preload)();
        completePreload();
        return true;
    }

    bool loadingVisible() const
    {
        return phase_ != Phase::Ready;
    }

    bool canDismiss() const
    {
        return phase_ == Phase::Ready;
    }

    bool shouldDismiss(ChessGuiPreBattleDismissInput input) const
    {
        return canDismiss() && input != ChessGuiPreBattleDismissInput::None;
    }

private:
    enum class Phase : std::uint8_t
    {
        AwaitingFirstFrame,
        PreloadPending,
        Preloading,
        Ready,
    };

    bool beginPreload()
    {
        if (phase_ != Phase::PreloadPending)
        {
            return false;
        }
        phase_ = Phase::Preloading;
        return true;
    }

    void completePreload()
    {
        assert(phase_ == Phase::Preloading);
        phase_ = Phase::Ready;
    }

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
