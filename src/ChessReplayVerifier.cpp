#include "ChessReplayVerifier.h"

#include "ChessReplayJournal.h"
#include "ChessRuntimeConstants.h"
#include <algorithm>
#include <cassert>
#include <limits>

namespace KysChess
{
namespace
{

ChessReplayVerificationResult mismatch(
    ChessReplayMismatch category,
    std::uint64_t sequence,
    std::string message)
{
    return {false, category, sequence, std::move(message)};
}

}

ChessReplayAudit::ChessReplayAudit(
    std::shared_ptr<const ChessGameContent> content,
    ChessReplay replay)
    : replay_(std::move(replay))
{
    if (replay_.header.gameVersion != content->gameVersion()
        || replay_.header.options.battleFrameLimit != kChessBattleFrameLimit)
    {
        fail(ChessReplayMismatch::Header, 0, "重播版本不相容");
        return;
    }
    const auto expectedDifficulty = content->difficulty() == Difficulty::Easy
        ? "easy"
        : content->difficulty() == Difficulty::Normal ? "normal" : "hard";
    if (replay_.header.difficulty != expectedDifficulty)
    {
        fail(ChessReplayMismatch::Header, 0, "重播難度與規則內容不相符");
        return;
    }

    session_ = std::make_unique<ChessGameSession>(
        std::move(content),
        replay_.header.rootSeed,
        replay_.header.options);
}

void ChessReplayAudit::fail(
    ChessReplayMismatch category,
    std::uint64_t sequence,
    std::string message)
{
    verification_ = mismatch(category, sequence, std::move(message));
    finished_ = true;
}

bool ChessReplayAudit::completeDecision(const ChessActionResult& actual)
{
    const auto& expected = replay_.decisions[nextDecision_];
    const std::uint64_t sequence = nextDecision_ + 1;
    if (actual.evidenceHash != expected.evidenceHash)
    {
        fail(ChessReplayMismatch::Evidence, sequence, "驗證證據雜湊不相符");
        return false;
    }
    ++nextDecision_;
    return true;
}

void ChessReplayAudit::verifyFooter()
{
    const auto actualReplay = session_->exportReplay();
    if (!actualReplay
        || actualReplay->footer.terminalEvidenceHash != replay_.footer.terminalEvidenceHash
        || actualReplay->footer.finalStateHash != replay_.footer.finalStateHash
        || actualReplay->footer.complete != replay_.footer.complete
        || actualReplay->footer.fightReached != replay_.footer.fightReached)
    {
        fail(ChessReplayMismatch::Footer, replay_.decisions.size(), "重播頁尾不相符");
        return;
    }
    verification_ = {true, ChessReplayMismatch::None, replay_.decisions.size(), {}};
    finished_ = true;
}

void ChessReplayAudit::step(std::size_t decisionBudget, int battleFrameBudget)
{
    assert(decisionBudget > 0);
    assert(battleFrameBudget > 0);
    if (finished_)
    {
        return;
    }

    std::size_t decisionsCompleted{};
    int framesRemaining = battleFrameBudget;
    while (!finished_)
    {
        if (session_->transitionPending())
        {
            if (framesRemaining == 0)
            {
                return;
            }
            auto advance = session_->advanceAutomatic(framesRemaining);
            framesRemaining -= advance.framesAdvanced;
            if (!advance.completedAction)
            {
                return;
            }
            if (!completeDecision(*advance.completedAction))
            {
                return;
            }
            ++decisionsCompleted;
            continue;
        }

        if (nextDecision_ == replay_.decisions.size())
        {
            verifyFooter();
            return;
        }
        if (decisionsCompleted == decisionBudget)
        {
            return;
        }

        const auto& expected = replay_.decisions[nextDecision_];
        const std::uint64_t sequence = nextDecision_ + 1;
        const auto legal = session_->legalActions();
        if (std::ranges::none_of(legal, [&](const ChessLegalActionDescriptor& descriptor) {
                return descriptor.type == expected.action.type;
            }))
        {
            fail(ChessReplayMismatch::IllegalAction, sequence, "記錄的操作不在合法操作集合內");
            return;
        }
        const auto actual = session_->beginAction(expected.action);
        if (!actual.accepted)
        {
            fail(ChessReplayMismatch::IllegalAction, sequence, "記錄的操作不合法");
            return;
        }
        if (!actual.transitionPending)
        {
            if (!completeDecision(actual))
            {
                return;
            }
            ++decisionsCompleted;
        }
    }
}

ChessReplayAuditResult ChessReplayAudit::takeResult()
{
    assert(finished_);
    ChessReplayAuditResult result;
    result.verification = std::move(verification_);
    if (result.verification.valid)
    {
        result.reconstructedSession = std::move(session_);
    }
    return result;
}

ChessReplayAuditResult ChessReplayVerifier::audit(
    std::shared_ptr<const ChessGameContent> content,
    const ChessReplay& replay)
{
    ChessReplayAudit audit(std::move(content), replay);
    while (!audit.finished())
    {
        audit.step(std::numeric_limits<std::size_t>::max(), std::numeric_limits<int>::max());
    }
    return audit.takeResult();
}

ChessReplayVerificationResult ChessReplayVerifier::verify(
    std::shared_ptr<const ChessGameContent> content,
    const ChessReplay& replay)
{
    return audit(std::move(content), replay).verification;
}

}
