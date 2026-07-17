#pragma once

#include "ChessReplayJson.h"
#include "ChessReplayTypes.h"
#include "ChessRunRandom.h"

#include <optional>
#include <string>
#include <string_view>

namespace KysChess
{

class ChessGameContent;
class ChessGameSession;

enum class ChessCheckpointError
{
    None,
    Malformed,
    IncompatibleGameVersion,
    UnrepresentableSnapshot,
    UnstableBoundary,
};

struct ChessSessionCheckpointData
{
    std::string game_version;
    ChessReplayData replay;
    ChessSessionState state;
    ChessRunRandomState random;
    std::string snapshot_hash;
    std::uint64_t save_revision{};
    std::string label;
};

struct ChessSessionCheckpoint
{
    std::string gameVersion;
    ChessReplay replay;
    ChessSessionState state;
    ChessRunRandomState random;
    ChessSha256 snapshotHash{};
    std::uint64_t saveRevision{};
    std::string label;

    static ChessSessionCheckpoint capture(
        const ChessGameSession& session,
        std::uint64_t saveRevision,
        std::string label = {});
    ChessCheckpointError restore(ChessGameSession& session) const;

    ChessSessionCheckpointData toData() const;
    static std::optional<ChessSessionCheckpoint> fromData(
        const ChessSessionCheckpointData& data,
        ChessCheckpointError& error);
    std::string serializeJson() const;
    static std::optional<ChessSessionCheckpoint> parseJson(
        std::string_view json,
        ChessCheckpointError& error);
};

}
