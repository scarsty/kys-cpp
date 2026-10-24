#pragma once

#include "ChessGameSession.h"

#include <cstddef>
#include <functional>
#include <memory>

namespace KysChess
{

enum class ChessReplayMismatch
{
    None,
    Header,
    IllegalAction,
    Evidence,
    Footer,
};

struct ChessReplayVerificationResult
{
    bool valid = false;
    ChessReplayMismatch mismatch = ChessReplayMismatch::None;
    std::uint64_t sequence{};
    std::string message;
};

struct ChessReplayAuditResult
{
    ChessReplayVerificationResult verification;
    std::unique_ptr<ChessGameSession> reconstructedSession;
};

// 序號 0 為初始局面，其餘回呼只發生在操作證據通過驗證之後。
// 完整重播及存檔快照是否有效，仍須以最終驗證結果為準。
using ChessReplayObserver = std::function<void(std::size_t, const ChessGameSession&)>;

class ChessReplayAudit
{
public:
    ChessReplayAudit(
        std::shared_ptr<const ChessGameContent> content,
        ChessReplay replay,
        ChessReplayObserver observer = {});

    void step(std::size_t decisionBudget, int battleFrameBudget);
    bool finished() const { return finished_; }
    std::size_t completedDecisionCount() const { return nextDecision_; }
    std::size_t totalDecisionCount() const { return replay_.decisions.size(); }
    ChessReplayAuditResult takeResult();

private:
    friend class ChessReplayVerifier;
    ChessReplayAuditResult takePrefixResult();
    void fail(
        ChessReplayMismatch category,
        std::uint64_t sequence,
        std::string message);
    bool completeDecision(const ChessActionResult& actual);
    void verifyFooter();

    ChessReplay replay_;
    ChessReplayObserver observer_;
    std::unique_ptr<ChessGameSession> session_;
    ChessReplayVerificationResult verification_;
    std::size_t nextDecision_{};
    std::string currentActionDescription_{};
    bool finished_ = false;
};

class ChessReplayVerifier
{
public:
    static ChessReplayAuditResult audit(
        std::shared_ptr<const ChessGameContent> content,
        const ChessReplay& replay);
    static ChessReplayAuditResult reconstructPrefix(
        std::shared_ptr<const ChessGameContent> content,
        const ChessReplay& replay,
        std::size_t decisionCount);
    static ChessReplayVerificationResult verify(
        std::shared_ptr<const ChessGameContent> content,
        const ChessReplay& replay);
};

}
