#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include "ChessSessionTypes.h"

namespace KysChess
{

class ChessGameSession;
class ChessGameContent;
struct ChessSessionCheckpoint;
enum class ChessCheckpointError;
enum class Difficulty : std::uint8_t;

class ChessApplicationSessionHost
{
public:
    static ChessApplicationSessionHost& instance();
    ChessGameSession& session();
    void reset(Difficulty difficulty, std::optional<ChessTalentId> talent = std::nullopt);
    std::shared_ptr<const ChessGameContent> contentFor(Difficulty difficulty);
    ChessCheckpointError prepareRestore(
        const ChessSessionCheckpoint& checkpoint,
        std::unique_ptr<ChessGameSession>& replacement);
    void commitRestore(std::unique_ptr<ChessGameSession> replacement);

private:
    ChessApplicationSessionHost();
    ~ChessApplicationSessionHost();

    ChessApplicationSessionHost(const ChessApplicationSessionHost&) = delete;
    ChessApplicationSessionHost& operator=(const ChessApplicationSessionHost&) = delete;

    ChessGameSession* session_ = nullptr;
    std::array<std::shared_ptr<const ChessGameContent>, 3> contentByDifficulty_;
};

ChessGameSession& applicationChessSession();
void resetApplicationChessSession(Difficulty difficulty, std::optional<ChessTalentId> talent = std::nullopt);

}
