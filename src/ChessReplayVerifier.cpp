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
        const auto legal = session.legalActions();
        if (std::ranges::none_of(legal, [&](const ChessLegalActionDescriptor& descriptor) {
                return descriptor.type == expected.action.type;
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
        if (actual.evidenceHash != expected.evidenceHash)
            return mismatch(ChessReplayMismatch::Evidence, sequence, "驗證證據雜湊不相符");
    }
    const auto actualReplay = session.exportReplay();
    if (!actualReplay
        || actualReplay->footer.terminalEvidenceHash != replay.footer.terminalEvidenceHash
        || actualReplay->footer.finalStateHash != replay.footer.finalStateHash
        || actualReplay->footer.complete != replay.footer.complete
        || actualReplay->footer.fightReached != replay.footer.fightReached)
    {
        return mismatch(ChessReplayMismatch::Footer, replay.decisions.size(), "重播頁尾不相符");
    }
    return {true, ChessReplayMismatch::None, replay.decisions.size(), {}};
}

}
