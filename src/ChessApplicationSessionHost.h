#pragma once

#include <array>
#include <memory>

namespace KysChess
{

class ChessGameSession;
class ChessGameContent;
struct ChessSessionCheckpoint;
enum class ChessCheckpointError;
enum class Difficulty;

class ChessApplicationSessionHost
{
public:
    static ChessApplicationSessionHost& instance();
    ChessGameSession& session();
    void reset(Difficulty difficulty);
    ChessCheckpointError prepareRestore(
        const ChessSessionCheckpoint& checkpoint,
        std::unique_ptr<ChessGameSession>& replacement);
    void commitRestore(std::unique_ptr<ChessGameSession> replacement);

private:
    ChessApplicationSessionHost();
    ~ChessApplicationSessionHost();

    ChessApplicationSessionHost(const ChessApplicationSessionHost&) = delete;
    ChessApplicationSessionHost& operator=(const ChessApplicationSessionHost&) = delete;

    std::shared_ptr<const ChessGameContent> contentFor(Difficulty difficulty);

    ChessGameSession* session_ = nullptr;
    std::array<std::shared_ptr<const ChessGameContent>, 3> contentByDifficulty_;
};

ChessGameSession& applicationChessSession();
void resetApplicationChessSession(Difficulty difficulty);

}
