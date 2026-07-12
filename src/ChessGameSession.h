#pragma once

#include "ChessManagementRules.h"
#include "ChessReplayJournal.h"
#include "HeadlessBattleRunner.h"

#include <memory>
#include <optional>
#include <set>

namespace KysChess
{

struct ChessSessionCheckpoint;

class ChessGameSession
{
public:
    ChessGameSession(
        std::shared_ptr<const ChessGameContent> content,
        std::uint64_t rootSeed,
        ChessSessionOptions options = {});
    static std::unique_ptr<ChessGameSession> createStandaloneBattle(
        std::shared_ptr<const ChessGameContent> content,
        std::uint64_t rootSeed,
        PreparedChessBattle preparedBattle,
        std::set<int> obtainedNeigongIds = {},
        ChessSessionOptions options = {});
    ~ChessGameSession();

    ChessGameplayObservation observe() const;
    std::vector<ChessLegalActionDescriptor> legalActions() const;
    ChessActionResult beginAction(const ChessAction& action);
    ChessAutomaticAdvanceResult advanceAutomatic(int frameBudget);
    ChessActionResult submitAndDrain(const ChessAction& action);
    std::optional<ChessReplay> exportReplay() const;

    const ChessSessionState& state() const { return state_; }
    const ChessRunRandom& random() const { return random_; }
    const ChessReplayJournal& journal() const { return journal_; }
    const ChessGameContent& content() const { return *content_; }
    std::shared_ptr<const ChessGameContent> sharedContent() const { return content_; }
    const HeadlessBattleResult* lastBattleResult() const { return lastBattleResult_.get(); }
    const std::optional<PreparedChessBattle>& lastBattlePrepared() const { return lastBattlePrepared_; }
    bool transitionPending() const { return pendingTransition_ != nullptr; }
    bool isStableDecisionBoundary() const;
    const Battle::BattleInitializationResult* pendingBattleInitialization() const;
    const Battle::BattleRuntimeSession* pendingBattleRuntime() const;
    const Battle::BattleRuntimeSession* presentationBattleRuntime() const;

private:
    friend struct ChessSessionCheckpoint;
    friend class ChessApplicationSessionHost;
    static ChessReplayHeader makeReplayHeader(
        const ChessGameContent& content,
        std::uint64_t rootSeed,
        const ChessSessionOptions& options);
    static std::string errorDescription(ChessRuleErrorCode error);
    ChessRuleErrorCode validateAction(const ChessAction& action) const;
    void applyAction(const ChessAction& action, std::vector<ChessSemanticEvent>& events);
    ChessActionResult finalizeAction(
        ChessSessionPhase phase,
        const ChessAction& action,
        const ChessSha256& preStateHash,
        std::vector<ChessSemanticEvent> events);
    void discardPendingTransition();
    void replaceWith(ChessGameSession&& other);

    struct PendingTransition;

    std::shared_ptr<const ChessGameContent> content_;
    ChessSessionState state_;
    ChessRunRandom random_;
    ChessReplayJournal journal_;
    std::shared_ptr<HeadlessBattleResult> lastBattleResult_;
    std::shared_ptr<Battle::BattleRuntimeSession> lastBattleRuntime_;
    std::optional<PreparedChessBattle> lastBattlePrepared_;
    std::unique_ptr<PendingTransition> pendingTransition_;
    bool replayExportEnabled_ = true;
};

}
