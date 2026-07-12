#pragma once

#include "ChessDiagnostics.h"
#include "ChessGameSession.h"
#include "ChessSaveStore.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <functional>

namespace KysChess
{

class ChessJsonProtocol
{
public:
    using ContentProvider = std::function<std::shared_ptr<const ChessGameContent>(Difficulty)>;

    explicit ChessJsonProtocol(ContentProvider contentProvider);
    explicit ChessJsonProtocol(std::shared_ptr<const ChessGameContent> fixedContent);

    std::string handleLine(std::string_view requestJson);
    const ChessGameSession* session() const { return session_.get(); }

private:
    std::shared_ptr<const ChessGameContent> loadContent(Difficulty difficulty);

    ContentProvider contentProvider_;
    std::shared_ptr<const ChessGameContent> fixedContent_;
    std::unique_ptr<ChessGameSession> session_;
    ChessSaveStore saves_;
};

}
