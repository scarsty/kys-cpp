#pragma once

#include "ChessJsonProtocol.h"

#include <iosfwd>

namespace KysChess
{

enum class ChessCliOutputMode
{
    Human,
    Compact,
    Json,
};

class ChessCliController
{
public:
    explicit ChessCliController(
        ChessJsonProtocol::ContentProvider contentProvider,
        ChessJsonProtocolOptions options = {});

    int runJsonl(std::istream& input, std::ostream& output);
    int runMcp(std::istream& input, std::ostream& output);
    int runInteractive(
        std::istream& input,
        std::ostream& output,
        ChessCliOutputMode mode = ChessCliOutputMode::Human);
    std::string executeInteractive(
        std::string_view command,
        ChessCliOutputMode mode = ChessCliOutputMode::Human);
    std::string newSession(
        Difficulty difficulty,
        std::uint64_t seed,
        ChessCliOutputMode mode = ChessCliOutputMode::Human);
    static std::string helpText();

    ChessJsonProtocol& protocol() { return protocol_; }

private:
    std::string submitRequest(std::string method, std::string paramsJson);
    std::string submitAction(const ChessAction& action, ChessCliOutputMode mode);
    std::string renderCurrent(ChessCliOutputMode mode) const;
    std::string renderBattle(ChessCliOutputMode mode) const;

    ChessJsonProtocol protocol_;
    std::uint64_t nextRequestId_ = 1;
};

}
