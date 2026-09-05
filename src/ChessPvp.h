#pragma once

#include "ChessReplayVerifier.h"
#include "ChessSessionCheckpoint.h"
#include "Point.h"

#include <array>
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace KysChess
{

struct ChessPvpPiece
{
    int chessInstanceId = -1;
    int roleId = -1;
    int star = 1;
    int weaponItemId = -1;
    int armorItemId = -1;
    int fightsWon{};
    int amplifiedGrowthPercent{};
    int openingMp{};
    std::optional<Battle::BattleLethalRecovery> lethalRecovery;

    auto operator<=>(const ChessPvpPiece&) const = default;
};

struct ChessPvpComposition
{
    std::vector<ChessPvpPiece> pieces;
    std::vector<int> formationSlots;
    std::set<int> obtainedNeigongIds;

    auto operator<=>(const ChessPvpComposition&) const = default;
};

ChessPvpComposition extractChessPvpComposition(const ChessGameSession& session);

class ChessPvpMapLayout
{
public:
    static constexpr int BattleId = 133;
    static constexpr int BattlefieldId = 21;
    static constexpr std::string_view DisplayName = "PvP Arena";

    static const std::array<Point, kChessFormationSlotCount>& localFormation();
    static const std::array<Point, kChessFormationSlotCount>& opponentFormation();
    static const std::array<Point, 3>& localAdditionalSpawns();
    static const std::array<Point, 3>& opponentAdditionalSpawns();
    static bool validate(const ChessGameContent& content, std::string& error);
};

enum class ChessPvpSaveError
{
    None,
    Malformed,
    VersionMismatch,
    HardModeRequired,
    ReplayVerificationFailed,
    UnrepresentableSnapshot,
    SnapshotStateMismatch,
    SnapshotRandomMismatch,
    SnapshotHashMismatch,
    InvalidFormation,
    NoDeployedPieces,
};

struct ChessPvpSaveVerificationResult
{
    bool valid = false;
    ChessPvpSaveError error = ChessPvpSaveError::None;
    std::uint64_t sequence{};
    std::string message;
    std::string gameVersion;
    ChessPvpComposition composition;
};

class ChessPvpSaveVerifier
{
public:
    ChessPvpSaveVerifier(
        std::shared_ptr<const ChessGameContent> content,
        std::string_view payload);

    void step(std::size_t decisionBudget, int battleFrameBudget);
    bool finished() const { return finished_; }
    std::size_t completedActionCount() const;
    std::size_t totalActionCount() const;
    ChessPvpSaveVerificationResult takeResult();

    static ChessPvpSaveVerificationResult verify(
        std::shared_ptr<const ChessGameContent> content,
        std::string_view payload);

private:
    void fail(
        ChessPvpSaveError error,
        std::string message,
        std::uint64_t sequence = 0);
    void finishAudit();

    std::shared_ptr<const ChessGameContent> content_;
    std::optional<ChessSessionCheckpoint> checkpoint_;
    std::unique_ptr<ChessReplayAudit> audit_;
    ChessPvpSaveVerificationResult result_;
    bool finished_ = false;
};

}
