#include "ChessReplayVerifier.h"

#include "ChessReplayJournal.h"
#include "ChessRuntimeConstants.h"
#include <algorithm>
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

ChessReplayVerificationResult ChessReplayVerifier::verify(
    std::shared_ptr<const ChessGameContent> content,
    const ChessReplay& replay)
{
    if (replay.header.gameVersion != content->gameVersion()
        || replay.header.options.battleFrameLimit != kChessBattleFrameLimit)
    {
        return mismatch(ChessReplayMismatch::Header, 0, "重播版本不相容");
    }
    const auto expectedDifficulty = content->difficulty() == Difficulty::Easy
        ? "easy"
        : content->difficulty() == Difficulty::Normal ? "normal" : "hard";
    if (replay.header.difficulty != expectedDifficulty)
    {
        return mismatch(ChessReplayMismatch::Header, 0, "重播難度與規則內容不相符");
    }

    ChessGameSession session(content, replay.header.rootSeed, replay.header.options);
    for (std::size_t index = 0; index < replay.decisions.size(); ++index)
    {
        const auto& expected = replay.decisions[index];
        const std::uint64_t sequence = index + 1;
        if (expected.sequence != sequence)
            return mismatch(ChessReplayMismatch::Sequence, sequence, "決策序號不連續");
        if (expected.phase != session.state().phase)
            return mismatch(ChessReplayMismatch::Phase, sequence, "決策階段不相符");
        if (expected.preStateHash != session.observe().stateHash)
            return mismatch(ChessReplayMismatch::PreState, sequence, "前置狀態雜湊不相符");
        const auto legal = session.legalActions();
        if (std::ranges::none_of(legal, [&](const ChessActionOffer& offer) {
                return offer.type == expected.action.type;
            }))
        {
            return mismatch(ChessReplayMismatch::IllegalAction, sequence, "記錄的操作不在合法操作集合內");
        }
        auto actual = session.beginAction(expected.action);
        if (!actual.accepted)
            return mismatch(ChessReplayMismatch::IllegalAction, sequence, "記錄的操作不合法");
        while (actual.transitionPending)
        {
            auto advance = session.advanceAutomatic(std::numeric_limits<int>::max());
            if (advance.completedAction)
            {
                actual = std::move(*advance.completedAction);
            }
        }
        if (actual.eventHash != expected.eventHash)
            return mismatch(ChessReplayMismatch::Event, sequence, "事件雜湊不相符");
        if (actual.rngDigest != expected.rngDigest)
            return mismatch(ChessReplayMismatch::Rng, sequence, "亂數摘要不相符");
        if (actual.postStateHash != expected.postStateHash)
            return mismatch(ChessReplayMismatch::PostState, sequence, "後置狀態雜湊不相符");
        const auto& actualRecord = session.journal().decisions().back();
        if (actualRecord.previousChainHash != expected.previousChainHash
            || actualRecord.chainHash != expected.chainHash)
            return mismatch(ChessReplayMismatch::Chain, sequence, "鏈式雜湊不相符");
    }
    const auto actualReplay = session.exportReplay();
    if (!actualReplay
        || actualReplay->footer.terminalChainHash != replay.footer.terminalChainHash
        || actualReplay->footer.finalStateHash != replay.footer.finalStateHash
        || actualReplay->footer.complete != replay.footer.complete
        || actualReplay->footer.fightReached != replay.footer.fightReached)
    {
        return mismatch(ChessReplayMismatch::Footer, replay.decisions.size(), "重播頁尾不相符");
    }
    return {true, ChessReplayMismatch::None, replay.decisions.size(), {}};
}

}
