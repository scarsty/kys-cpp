#include "ChessPvp.h"

#include "BattlefieldData.h"
#include "ChessManagementRules.h"
#include "ChessReplayJournal.h"
#include "GameDataStore.h"

#include <glaze/json.hpp>

#include <algorithm>
#include <cassert>
#include <format>
#include <limits>
#include <set>

namespace KysChess
{
namespace PvpDetail
{

struct ExternalSlotData
{
    GameDataStore gameData;
};

}

namespace
{

using PvpDetail::ExternalSlotData;

std::optional<ChessSessionCheckpoint> decodeCheckpoint(
    std::string_view payload,
    ChessCheckpointError& error)
{
    ChessCheckpointError directError;
    if (auto direct = ChessSessionCheckpoint::parseJson(payload, directError))
    {
        return direct;
    }
    if (directError == ChessCheckpointError::UnrepresentableSnapshot)
    {
        error = directError;
        return std::nullopt;
    }

    ExternalSlotData slot;
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    if (glz::read<options>(slot, payload))
    {
        error = ChessCheckpointError::Malformed;
        return std::nullopt;
    }
    return ChessSessionCheckpoint::fromData(
        slot.gameData.chessSessionCheckpoint,
        error);
}

Point rotatePvpPoint(Point point)
{
    return {61 - point.x, 59 - point.y};
}

template <std::size_t Size>
std::array<Point, Size> rotated(const std::array<Point, Size>& source)
{
    std::array<Point, Size> result;
    std::ranges::transform(source, result.begin(), rotatePvpPoint);
    return result;
}

}

ChessPvpComposition extractChessPvpComposition(const ChessGameSession& session)
{
    const auto& state = session.state();
    assert(ChessManagementRules::formationIsValid(state, state.formationSlots));

    ChessPvpComposition result;
    result.formationSlots = state.formationSlots;
    result.obtainedNeigongIds = state.obtainedNeigongIds;
    const auto itemId = [&](int equipmentInstanceId) {
        if (equipmentInstanceId < 0)
        {
            return -1;
        }
        const auto found = state.equipmentInventory.find(equipmentInstanceId);
        assert(found != state.equipmentInventory.end());
        return found->second.itemId;
    };
    for (const int instanceId : state.formationSlots)
    {
        if (instanceId < 0)
        {
            continue;
        }
        const auto& piece = state.roster.at(instanceId);
        result.pieces.push_back({
            piece.instanceId,
            piece.roleId,
            piece.star,
            itemId(piece.weaponInstanceId),
            itemId(piece.armorInstanceId),
            piece.fightsWon,
        });
    }
    return result;
}

const std::array<Point, kChessFormationSlotCount>& ChessPvpMapLayout::localFormation()
{
    static const std::array<Point, kChessFormationSlotCount> positions{{
        {28, 24},
        {28, 28},
        {28, 32},
        {28, 35},
        {26, 26},
        {26, 30},
        {26, 34},
        {24, 24},
        {24, 29},
        {24, 35},
    }};
    return positions;
}

const std::array<Point, kChessFormationSlotCount>& ChessPvpMapLayout::opponentFormation()
{
    static const auto positions = rotated(localFormation());
    return positions;
}

const std::array<Point, 3>& ChessPvpMapLayout::localAdditionalSpawns()
{
    static const std::array<Point, 3> positions{{
        {22, 26},
        {22, 30},
        {22, 34},
    }};
    return positions;
}

const std::array<Point, 3>& ChessPvpMapLayout::opponentAdditionalSpawns()
{
    static const auto positions = rotated(localAdditionalSpawns());
    return positions;
}

bool ChessPvpMapLayout::validate(const ChessGameContent& content, std::string& error)
{
    error.clear();
    if (!content.battleMaps().contains(BattleId))
    {
        error = "缺少 PvP 戰鬥 133";
        return false;
    }
    const auto field = content.battlefields().find(BattlefieldId);
    if (field == content.battlefields().end())
    {
        error = "缺少 PvP 戰場 21";
        return false;
    }
    BattlefieldData terrain(&field->second);
    std::set<std::pair<int, int>> unique;
    const auto validatePair = [&](Point local, Point opponent) {
        const auto expectedOpponent = rotatePvpPoint(local);
        if (expectedOpponent.x != opponent.x || expectedOpponent.y != opponent.y)
        {
            error = "PvP 站位不是精確的 180 度旋轉";
            return false;
        }
        for (const Point point : {local, opponent})
        {
            if (!terrain.canWalk(point.x, point.y))
            {
                error = std::format("PvP 站位 ({},{}) 不可行走", point.x, point.y);
                return false;
            }
            if (!unique.emplace(point.x, point.y).second)
            {
                error = "PvP 站位座標重複";
                return false;
            }
        }
        return true;
    };
    for (std::size_t index = 0; index < localFormation().size(); ++index)
    {
        if (!validatePair(localFormation()[index], opponentFormation()[index]))
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < localAdditionalSpawns().size(); ++index)
    {
        if (!validatePair(localAdditionalSpawns()[index], opponentAdditionalSpawns()[index]))
        {
            return false;
        }
    }
    for (int x = 22; x <= 39; ++x)
    {
        for (int y = 24; y <= 35; ++y)
        {
            const auto rotatedPoint = rotatePvpPoint({x, y});
            if (terrain.canWalk(x, y)
                != terrain.canWalk(rotatedPoint.x, rotatedPoint.y))
            {
                error = "PvP 競技場區域的碰撞遮罩不對稱";
                return false;
            }
        }
    }
    return true;
}

ChessPvpSaveVerifier::ChessPvpSaveVerifier(
    std::shared_ptr<const ChessGameContent> content,
    std::string_view payload)
    : content_(std::move(content))
{
    assert(content_);
    ChessCheckpointError checkpointError;
    checkpoint_ = decodeCheckpoint(payload, checkpointError);
    if (!checkpoint_)
    {
        if (checkpointError == ChessCheckpointError::UnrepresentableSnapshot)
        {
            fail(ChessPvpSaveError::InvalidFormation, "匯入存檔的陣形資料無效");
        }
        else
        {
            fail(ChessPvpSaveError::Malformed, "匯入內容不是完整的自走棋存檔 JSON");
        }
        return;
    }
    result_.gameVersion = checkpoint_->gameVersion();
    if (checkpoint_->gameVersion() != content_->gameVersion())
    {
        fail(
            ChessPvpSaveError::VersionMismatch,
            std::format(
                "遊戲版本不相符：存檔 {}，目前 {}",
                checkpoint_->gameVersion(),
                content_->gameVersion()));
        return;
    }
    if (content_->difficulty() != Difficulty::Hard
        || checkpoint_->state.difficulty != Difficulty::Hard
        || checkpoint_->replay.header.difficulty != "hard")
    {
        fail(ChessPvpSaveError::HardModeRequired, "離線對戰只接受困難模式存檔");
        return;
    }
    if (checkpoint_->state.phase == ChessSessionPhase::BattleResolution)
    {
        fail(ChessPvpSaveError::UnrepresentableSnapshot, "存檔不在可驗證的穩定決策邊界");
        return;
    }
    audit_ = std::make_unique<ChessReplayAudit>(content_, checkpoint_->replay);
    if (audit_->finished())
    {
        finishAudit();
    }
}

void ChessPvpSaveVerifier::fail(
    ChessPvpSaveError error,
    std::string message,
    std::uint64_t sequence)
{
    result_.valid = false;
    result_.error = error;
    result_.sequence = sequence;
    result_.message = std::move(message);
    finished_ = true;
}

void ChessPvpSaveVerifier::step(
    std::size_t decisionBudget,
    int battleFrameBudget)
{
    if (finished_)
    {
        return;
    }
    audit_->step(decisionBudget, battleFrameBudget);
    if (audit_->finished())
    {
        finishAudit();
    }
}

std::size_t ChessPvpSaveVerifier::completedActionCount() const
{
    return audit_ ? audit_->completedDecisionCount() : 0;
}

std::size_t ChessPvpSaveVerifier::totalActionCount() const
{
    return audit_ ? audit_->totalDecisionCount() : 0;
}

void ChessPvpSaveVerifier::finishAudit()
{
    auto audited = audit_->takeResult();
    if (!audited.verification.valid)
    {
        fail(
            ChessPvpSaveError::ReplayVerificationFailed,
            std::format(
                "重播驗證在第 {} 個操作失敗：{}",
                audited.verification.sequence,
                audited.verification.message),
            audited.verification.sequence);
        return;
    }
    assert(audited.reconstructedSession);
    const auto& reconstructed = *audited.reconstructedSession;
    if (!reconstructed.isStableDecisionBoundary())
    {
        fail(ChessPvpSaveError::UnrepresentableSnapshot, "存檔不在可驗證的穩定決策邊界");
        return;
    }
    if (reconstructed.state() != checkpoint_->state)
    {
        fail(ChessPvpSaveError::SnapshotStateMismatch, "最終存檔快照與重播結果不相符");
        return;
    }
    if (reconstructed.random().state() != checkpoint_->random)
    {
        fail(ChessPvpSaveError::SnapshotRandomMismatch, "最終隨機狀態與重播結果不相符");
        return;
    }
    const auto freshHash = chessStateHash(reconstructed.state(), reconstructed.random());
    const auto checkpointHash = chessStateHash(checkpoint_->state, checkpoint_->random);
    if (freshHash != checkpointHash
        || freshHash != checkpoint_->snapshotHash
        || freshHash != checkpoint_->replay.footer.finalStateHash)
    {
        fail(ChessPvpSaveError::SnapshotHashMismatch, "存檔快照雜湊與重播最終狀態不相符");
        return;
    }
    if (!ChessManagementRules::formationIsValid(
            reconstructed.state(),
            reconstructed.state().formationSlots))
    {
        fail(ChessPvpSaveError::InvalidFormation, "匯入存檔的陣形資料無效");
        return;
    }

    result_.composition = extractChessPvpComposition(reconstructed);
    if (result_.composition.pieces.empty())
    {
        fail(ChessPvpSaveError::NoDeployedPieces, "匯入存檔沒有出戰棋子");
        return;
    }
    result_.valid = true;
    result_.error = ChessPvpSaveError::None;
    result_.sequence = checkpoint_->replay.decisions.size();
    result_.message.clear();
    finished_ = true;
}

ChessPvpSaveVerificationResult ChessPvpSaveVerifier::takeResult()
{
    assert(finished_);
    return std::move(result_);
}

ChessPvpSaveVerificationResult ChessPvpSaveVerifier::verify(
    std::shared_ptr<const ChessGameContent> content,
    std::string_view payload)
{
    ChessPvpSaveVerifier verifier(std::move(content), payload);
    while (!verifier.finished())
    {
        verifier.step(
            std::numeric_limits<std::size_t>::max(),
            std::numeric_limits<int>::max());
    }
    return verifier.takeResult();
}

}
