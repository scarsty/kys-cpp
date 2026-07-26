#pragma once

#include "ChessDiagnostics.h"
#include "ChessGameSession.h"
#include "ChessSaveStore.h"

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <functional>

namespace KysChess
{

struct ChessJsonProtocolOptions
{
    std::filesystem::path autosaveFile;
};

class ChessJsonProtocol
{
public:
    using ContentProvider = std::function<std::shared_ptr<const ChessGameContent>(Difficulty)>;

    explicit ChessJsonProtocol(
        ContentProvider contentProvider,
        ChessJsonProtocolOptions options = {});
    explicit ChessJsonProtocol(
        std::shared_ptr<const ChessGameContent> fixedContent,
        ChessJsonProtocolOptions options = {});

    std::string handleLine(std::string_view requestJson);
    std::string handleMcpLine(std::string_view requestJson);
    const ChessGameSession* session() const { return session_.get(); }

private:
    std::shared_ptr<const ChessGameContent> loadContent(Difficulty difficulty);
    void loadAutosave();
    std::expected<void, std::string> persistAutosave();
    void updateAutosave();

    ContentProvider contentProvider_;
    std::shared_ptr<const ChessGameContent> fixedContent_;
    std::unique_ptr<ChessGameSession> session_;
    ChessSaveStore saves_;
    std::filesystem::path autosaveFile_;
    std::string lastAutosaveError_;
};

}
