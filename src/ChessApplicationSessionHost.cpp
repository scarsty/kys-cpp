#include "ChessApplicationSessionHost.h"

#include "ChessContentLoader.h"
#include "ChessGameSession.h"
#include "GameUtil.h"
#include "ChessSessionCheckpoint.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>

namespace KysChess
{
namespace
{

std::optional<Difficulty> checkpointDifficulty(const ChessSessionCheckpoint& checkpoint)
{
    switch (checkpoint.state.difficulty)
    {
    case Difficulty::Easy:
    case Difficulty::Normal:
    case Difficulty::Hard:
        return checkpoint.state.difficulty;
    }
    return std::nullopt;
}

std::size_t difficultyIndex(Difficulty difficulty)
{
    switch (difficulty)
    {
    case Difficulty::Easy: return 0;
    case Difficulty::Normal: return 1;
    case Difficulty::Hard: return 2;
    }
    throw std::invalid_argument("難度識別碼無效");
}

std::shared_ptr<const ChessGameContent> loadApplicationContent(Difficulty difficulty)
{
    const auto dataRoot = std::filesystem::weakly_canonical(GameUtil::PATH());
    auto configRoot = dataRoot / "config";
    const auto repositoryConfig = dataRoot.parent_path().parent_path() / "config";
    if (std::filesystem::exists(repositoryConfig / "chess_challenge.yaml"))
    {
        configRoot = repositoryConfig;
    }
    ChessContentLoadOptions options;
    options.dataRoot = dataRoot;
    options.configRoot = configRoot;
    options.difficulty = difficulty;
    options.diagnostics = [](const ChessDiagnostic& diagnostic) {
        LOG("[{}] {}\n", diagnostic.source, diagnostic.message);
    };
    auto content = ChessContentLoader::load(options);
    if (!content)
    {
        throw std::runtime_error("無法載入自走棋規則內容");
    }
    return std::make_shared<const ChessGameContent>(std::move(*content));
}

}

ChessApplicationSessionHost::ChessApplicationSessionHost() = default;

ChessApplicationSessionHost::~ChessApplicationSessionHost()
{
    delete session_;
}

ChessApplicationSessionHost& ChessApplicationSessionHost::instance()
{
    static ChessApplicationSessionHost host;
    return host;
}

ChessGameSession& ChessApplicationSessionHost::session()
{
    if (!session_)
    {
        reset(Difficulty::Easy);
    }
    return *session_;
}

void ChessApplicationSessionHost::reset(Difficulty difficulty)
{
    const auto rootSeed = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    auto replacement = std::make_unique<ChessGameSession>(
        contentFor(difficulty),
        rootSeed);
    commitRestore(std::move(replacement));
}

ChessCheckpointError ChessApplicationSessionHost::prepareRestore(
    const ChessSessionCheckpoint& checkpoint,
    std::unique_ptr<ChessGameSession>& replacement)
{
    replacement.reset();
    const auto difficulty = checkpointDifficulty(checkpoint);
    if (!difficulty)
    {
        return ChessCheckpointError::UnrepresentableSnapshot;
    }
    auto candidate = std::make_unique<ChessGameSession>(
        contentFor(*difficulty),
        checkpoint.random.rootSeed,
        checkpoint.state.options);
    const auto error = checkpoint.restore(*candidate);
    if (error != ChessCheckpointError::None)
    {
        return error;
    }
    replacement = std::move(candidate);
    return ChessCheckpointError::None;
}

void ChessApplicationSessionHost::commitRestore(
    std::unique_ptr<ChessGameSession> replacement)
{
    if (session_)
    {
        session_->replaceWith(std::move(*replacement));
    }
    else
    {
        session_ = replacement.release();
    }
}

std::shared_ptr<const ChessGameContent> ChessApplicationSessionHost::contentFor(
    Difficulty difficulty)
{
    auto& content = contentByDifficulty_[difficultyIndex(difficulty)];
    if (!content)
    {
        content = loadApplicationContent(difficulty);
    }
    return content;
}

ChessGameSession& applicationChessSession()
{
    return ChessApplicationSessionHost::instance().session();
}

void resetApplicationChessSession(Difficulty difficulty)
{
    ChessApplicationSessionHost::instance().reset(difficulty);
}

}
